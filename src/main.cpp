#include "DetectionController.h"
#include "MainViewModel.h"
#include "ResultEvaluator.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTextStream>

// 在可执行文件的上级/上上级目录中寻找数据集根（含 metal_nut、screw 等类别目录）
static QString locateDatasetRoot()
{
    const QDir appDir(QCoreApplication::applicationDirPath());
    QStringList candidates;
    candidates << appDir.absolutePath()
               << QDir(appDir.absoluteFilePath(QStringLiteral(".."))).absolutePath()
               << QDir(appDir.absoluteFilePath(QStringLiteral("../.."))).absolutePath()
               << QDir::currentPath();
    return DatasetManager::findDatasetRoot(candidates);
}

static OrtEpKind parseOrtEpKind(const QString& v, bool* ok)
{
    if (ok)
        *ok = true;
    if (v == QStringLiteral("auto"))
        return OrtEpKind::Auto;
    if (v == QStringLiteral("cpu"))
        return OrtEpKind::Cpu;
    if (v == QStringLiteral("dml"))
        return OrtEpKind::Dml;
    if (ok)
        *ok = false;
    return OrtEpKind::Auto;
}

// 批处理模式：--batch <类别目录或类别名> [--engine cv|dl] [--provider auto|cpu|dml]
// 不弹窗、不进入事件循环，跑完打印指标到 stdout 后退出。
static int runBatch(const QString& categoryArg, EngineKind engineKind, OrtEpKind epKind)
{
    QTextStream out(stdout);

    QString datasetRoot;
    QString category;
    if (QFileInfo::exists(categoryArg) && QFileInfo(categoryArg).isDir()
        && QDir(categoryArg + QStringLiteral("/test")).exists()) {
        const QFileInfo catInfo(categoryArg);
        datasetRoot = catInfo.absolutePath();
        category = catInfo.fileName();
    } else {
        datasetRoot = locateDatasetRoot();
        category = categoryArg;
    }

    if (datasetRoot.isEmpty()) {
        out << "ERROR: 未找到数据集根目录（上级/上上级目录中没有 metal_nut、screw 等类别目录）\n";
        return 2;
    }

    DetectionController ctrl;
    ctrl.setEngineKind(engineKind);
    ctrl.setOrtEpKind(epKind);
    if (!ctrl.loadDataset(datasetRoot)) {
        out << "ERROR: 数据集根目录扫描失败：" << datasetRoot << "\n";
        return 2;
    }
    const DatasetManager& dataset = ctrl.dataset();
    if (!dataset.categories().contains(category)) {
        out << "ERROR: 数据集根 " << datasetRoot << " 中没有类别 " << category
            << "（已有：" << dataset.categories().join(QStringLiteral(", ")) << "）\n";
        return 2;
    }

    out << "数据集根: " << dataset.rootPath() << "\n";
    out << "类别: " << category << "\n";
    out << "引擎: " << (engineKind == EngineKind::DL ? QStringLiteral("dl (EfficientAD)")
                                                    : QStringLiteral("cv (传统)")) << "\n";
    QElapsedTimer timer;
    timer.start();

    if (!ctrl.prepareEngine(category)) {
        out << "ERROR: 无法构建参考模型（" << category
            << (engineKind == EngineKind::DL
                    ? QStringLiteral(" 的 ONNX 模型缺失、会话创建失败或良品图不可读；"
                                     "先跑 tools/training/train_efficientad.py；"
                                     "DML 失败可试 --provider cpu")
                    : QStringLiteral("/train/good 为空或不可读）"))
            << "\n";
        return 2;
    }
    out << "参考模板已构建（train/good 共 "
        << dataset.trainGoodImages(category).size() << " 张）\n";
    if (engineKind == EngineKind::DL) {
        const QString ep = ctrl.dlProviderLabel(category);
        out << "ORT provider: " << (ep.isEmpty() ? QStringLiteral("?") : ep) << "\n";
    }
    out << "\n";

    QObject::connect(&ctrl, &DetectionController::imageProcessed, &ctrl,
                     [&out](const QString& defect, const QString& imgPath,
                            const DetectionResult& r, const PixelMetrics& pm) {
        const bool isDefect = (defect != QStringLiteral("good"));
        out << QStringLiteral("%1/%2: boxes=%3 area=%4 F1=%5 %6\n")
                   .arg(defect, QFileInfo(imgPath).fileName())
                   .arg(r.boxes.size())
                   .arg(r.totalArea, 0, 'f', 0)
                   .arg(pm.f1(), 0, 'f', 3)
                   .arg(r.detected() == isDefect ? QStringLiteral("OK")
                                                 : QStringLiteral("MISJUDGE"));
    });

    BatchMetrics metrics;
    ctrl.runBatch(category, metrics);
    const QMap<QString, PixelMetrics>& pixelByDefect = metrics.pixel;
    const QMap<QString, ImageMetrics>& imageByDefect = metrics.image;
    PixelMetrics pixelTotal;
    ImageMetrics imageTotal;
    for (auto it = pixelByDefect.constBegin(); it != pixelByDefect.constEnd(); ++it) {
        pixelTotal += it.value();
        imageTotal += imageByDefect[it.key()];
    }

    out << "\n===== 各缺陷类型指标（像素级） =====\n";
    out << QStringLiteral("%1  %2  %3  %4  %5  %6\n")
               .arg(QStringLiteral("defect"), -15)
               .arg(QStringLiteral("P"), 8)
               .arg(QStringLiteral("R"), 8)
               .arg(QStringLiteral("F1"), 8)
               .arg(QStringLiteral("IoU"), 8)
               .arg(QStringLiteral("img_acc"), 8);
    const QStringList keys = pixelByDefect.keys();
    for (const QString& defect : keys) {
        const PixelMetrics& p = pixelByDefect[defect];
        const ImageMetrics& im = imageByDefect[defect];
        out << QStringLiteral("%1  %2  %3  %4  %5  %6 (%7/%8)\n")
                   .arg(defect, -15)
                   .arg(p.precision(), 8, 'f', 4)
                   .arg(p.recall(), 8, 'f', 4)
                   .arg(p.f1(), 8, 'f', 4)
                   .arg(p.iou(), 8, 'f', 4)
                   .arg(im.accuracy(), 8, 'f', 4)
                   .arg(im.correct)
                   .arg(im.total);
    }

    out << "\n===== 汇总（含 good） =====\n";
    out << QStringLiteral("pixel: P=%1 R=%2 F1=%3 IoU=%4\n")
               .arg(pixelTotal.precision(), 0, 'f', 4)
               .arg(pixelTotal.recall(), 0, 'f', 4)
               .arg(pixelTotal.f1(), 0, 'f', 4)
               .arg(pixelTotal.iou(), 0, 'f', 4);
    out << QStringLiteral("image-level accuracy: %1 (%2/%3)\n")
               .arg(imageTotal.accuracy(), 0, 'f', 4)
               .arg(imageTotal.correct)
               .arg(imageTotal.total);
    const ImageMetrics& goodIm = imageByDefect[QStringLiteral("good")];
    if (goodIm.total > 0) {
        out << QStringLiteral("good 误报率: %1 (%2/%3 张 good 图误报)\n")
                   .arg(1.0 - goodIm.accuracy(), 0, 'f', 4)
                   .arg(goodIm.total - goodIm.correct)
                   .arg(goodIm.total);
    }
    out << QStringLiteral("elapsed: %1 s\n").arg(timer.elapsed() / 1000.0, 0, 'f', 1);
    out.flush();
    return 0;
}

static int runGui(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("surface_defect_detector"));
    QCoreApplication::setApplicationName(QStringLiteral("surface_defect_detector"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    auto* vm = new MainViewModel(&app);
    const QString root = locateDatasetRoot();
    if (!root.isEmpty())
        vm->loadDatasetPath(root);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("app"), vm);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    engine.loadFromModule("SurfaceDefect", "Main");
    if (engine.rootObjects().isEmpty()) {
        QTextStream(stderr) << "ERROR: QML 加载失败（SurfaceDefect/Main）\n";
        return 1;
    }
    return QGuiApplication::exec();
}

int main(int argc, char* argv[])
{
    // Q*Application 构造前只能从 argv 解析 --batch
    QStringList raw;
    for (int i = 0; i < argc; ++i)
        raw << QString::fromLocal8Bit(argv[i]);

    const int batchIdx = raw.indexOf(QStringLiteral("--batch"));
    if (batchIdx >= 0) {
        QCoreApplication core(argc, argv);
        QCoreApplication::setOrganizationName(QStringLiteral("surface_defect_detector"));
        QCoreApplication::setApplicationName(QStringLiteral("surface_defect_detector"));
        if (batchIdx + 1 >= raw.size()) {
            QTextStream(stderr) << "用法: surface_defect_detector --batch <类别目录或类别名>"
                                   " [--engine cv|dl] [--provider auto|cpu|dml]\n";
            return 2;
        }
        EngineKind engineKind = EngineKind::Traditional;
        const int engineIdx = raw.indexOf(QStringLiteral("--engine"));
        if (engineIdx >= 0 && engineIdx + 1 < raw.size()) {
            const QString v = raw.at(engineIdx + 1);
            if (v == QStringLiteral("dl"))
                engineKind = EngineKind::DL;
            else if (v != QStringLiteral("cv")) {
                QTextStream(stderr) << "未知引擎: " << v << "（可选 cv|dl）\n";
                return 2;
            }
        }
        OrtEpKind epKind = OrtEpKind::Auto;
        const int providerIdx = raw.indexOf(QStringLiteral("--provider"));
        if (providerIdx >= 0) {
            if (providerIdx + 1 >= raw.size()) {
                QTextStream(stderr) << "用法: --provider auto|cpu|dml\n";
                return 2;
            }
            bool ok = false;
            epKind = parseOrtEpKind(raw.at(providerIdx + 1), &ok);
            if (!ok) {
                QTextStream(stderr) << "未知 provider: " << raw.at(providerIdx + 1)
                                    << "（可选 auto|cpu|dml）\n";
                return 2;
            }
        }
        return runBatch(raw.at(batchIdx + 1), engineKind, epKind);
    }

    return runGui(argc, argv);
}
