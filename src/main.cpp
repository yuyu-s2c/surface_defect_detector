#include "DetectionController.h"
#include "InspectionSession.h"
#include "MainViewModel.h"
#include "ResultEvaluator.h"
#include "sources/FolderSource.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMargins>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QScreen>
#include <QTextStream>
#include <QWindow>

#include <memory>

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
        if (engineKind == EngineKind::DL) {
            const QStringList tried = ctrl.onnxModelCandidates(category);
            out << "ERROR: 无法构建 DL 参考模型（" << category
                << "：ONNX 缺失、会话创建失败或 train/good 不足 3 张）。"
                << "约定路径: " << tried.value(0)
                << " ；回退: " << tried.value(1)
                << "。DML 失败可试 --provider cpu\n";
        } else {
            out << "ERROR: 无法构建参考模型（" << category
                << "/train/good 为空或不可读）\n";
        }
        return 2;
    }
    out << "参考模板已构建（train/good 共 "
        << dataset.trainGoodImages(category).size() << " 张）\n";
    if (engineKind == EngineKind::DL) {
        const QString onnx = ctrl.dlModelPath(category);
        const QString ep = ctrl.dlProviderLabel(category);
        out << "ONNX: " << (onnx.isEmpty() ? QStringLiteral("?") : onnx) << "\n";
        out << "ORT provider: " << (ep.isEmpty() ? QStringLiteral("?") : ep) << "\n";
    }
    out << "\n";

    QObject::connect(&ctrl, &DetectionController::imageProcessed, &ctrl,
                     [&out](const QString& defect, const QString& imgPath,
                            const DetectionResult& r, const PixelMetrics& pm) {
        const bool isDefect = (defect != QStringLiteral("good"));
        out << QStringLiteral("%1/%2: boxes=%3 area=%4 score=%5 thr=%6 F1=%7 %8\n")
                   .arg(defect, QFileInfo(imgPath).fileName())
                   .arg(r.boxes.size())
                   .arg(r.totalArea, 0, 'f', 0)
                   .arg(r.imageScore, 0, 'f', 4)
                   .arg(r.imageThreshold, 0, 'f', 4)
                   .arg(pm.f1(), 0, 'f', 3)
                   .arg(r.detected() == isDefect ? QStringLiteral("OK")
                                                 : QStringLiteral("MISJUDGE"));
    });

    BatchMetrics metrics;
    ctrl.runBatch(category, metrics);
    const QMap<QString, PixelMetrics>& pixelByDefect = metrics.pixel;
    const QMap<QString, ImageMetrics>& imageByDefect = metrics.image;
    const QMap<QString, ImageMetrics>& imageAreaByDefect = metrics.imageByArea;
    PixelMetrics pixelTotal;
    ImageMetrics imageTotal;
    ImageMetrics imageAreaTotal;
    for (auto it = pixelByDefect.constBegin(); it != pixelByDefect.constEnd(); ++it) {
        pixelTotal += it.value();
        imageTotal += imageByDefect[it.key()];
        imageAreaTotal += imageAreaByDefect[it.key()];
    }

    out << "\n===== 各缺陷类型指标（像素级；img_acc=分数过线，img_area=面积门对照） =====\n";
    out << QStringLiteral("%1  %2  %3  %4  %5  %6  %7\n")
               .arg(QStringLiteral("defect"), -15)
               .arg(QStringLiteral("P"), 8)
               .arg(QStringLiteral("R"), 8)
               .arg(QStringLiteral("F1"), 8)
               .arg(QStringLiteral("IoU"), 8)
               .arg(QStringLiteral("img_acc"), 8)
               .arg(QStringLiteral("img_area"), 8);
    const QStringList keys = pixelByDefect.keys();
    for (const QString& defect : keys) {
        const PixelMetrics& p = pixelByDefect[defect];
        const ImageMetrics& im = imageByDefect[defect];
        const ImageMetrics& ia = imageAreaByDefect[defect];
        out << QStringLiteral("%1  %2  %3  %4  %5  %6 (%7/%8)  ")
                   .arg(defect, -15)
                   .arg(p.precision(), 8, 'f', 4)
                   .arg(p.recall(), 8, 'f', 4)
                   .arg(p.f1(), 8, 'f', 4)
                   .arg(p.iou(), 8, 'f', 4)
                   .arg(im.accuracy(), 8, 'f', 4)
                   .arg(im.correct)
                   .arg(im.total)
            << QStringLiteral("%1 (%2/%3)\n")
                   .arg(ia.accuracy(), 8, 'f', 4)
                   .arg(ia.correct)
                   .arg(ia.total);
    }

    out << "\n===== 汇总（含 good） =====\n";
    out << QStringLiteral("pixel: P=%1 R=%2 F1=%3 IoU=%4\n")
               .arg(pixelTotal.precision(), 0, 'f', 4)
               .arg(pixelTotal.recall(), 0, 'f', 4)
               .arg(pixelTotal.f1(), 0, 'f', 4)
               .arg(pixelTotal.iou(), 0, 'f', 4);
    out << QStringLiteral("image-level (score): %1 (%2/%3)\n")
               .arg(imageTotal.accuracy(), 0, 'f', 4)
               .arg(imageTotal.correct)
               .arg(imageTotal.total);
    out << QStringLiteral("image-level (area, 对照): %1 (%2/%3)\n")
               .arg(imageAreaTotal.accuracy(), 0, 'f', 4)
               .arg(imageAreaTotal.correct)
               .arg(imageAreaTotal.total);
    const ImageMetrics& goodIm = imageByDefect[QStringLiteral("good")];
    if (goodIm.total > 0) {
        out << QStringLiteral("good 误报率 (score): %1 (%2/%3 张 good 图误报)\n")
                   .arg(1.0 - goodIm.accuracy(), 0, 'f', 4)
                   .arg(goodIm.total - goodIm.correct)
                   .arg(goodIm.total);
    }
    const ImageMetrics& goodArea = imageAreaByDefect[QStringLiteral("good")];
    if (goodArea.total > 0) {
        out << QStringLiteral("good 误报率 (area, 对照): %1 (%2/%3)\n")
                   .arg(1.0 - goodArea.accuracy(), 0, 'f', 4)
                   .arg(goodArea.total - goodArea.correct)
                   .arg(goodArea.total);
    }
    out << QStringLiteral("elapsed: %1 s\n").arg(timer.elapsed() / 1000.0, 0, 'f', 1);
    out.flush();
    return 0;
}

// 无头冒烟：FolderSource 跑完一类 test，打印帧率/延迟/队列后退出。不是产品 --live。
static int runLiveSmoke(const QString& categoryArg, EngineKind engineKind, OrtEpKind epKind, int fps)
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
        out << "ERROR: 未找到数据集根目录\n";
        return 2;
    }

    DetectionController ctrl;
    ctrl.setEngineKind(engineKind);
    ctrl.setOrtEpKind(epKind);
    if (!ctrl.loadDataset(datasetRoot)) {
        out << "ERROR: 数据集扫描失败：" << datasetRoot << "\n";
        return 2;
    }
    if (!ctrl.dataset().categories().contains(category)) {
        out << "ERROR: 没有类别 " << category << "\n";
        return 2;
    }
    if (!ctrl.prepareEngine(category)) {
        out << "ERROR: 无法准备引擎（" << category << "）\n";
        return 2;
    }

    fps = qBound(InspectionSession::kMinFps, fps, InspectionSession::kMaxFps);
    auto src = std::make_unique<FolderSource>();
    src->setFromDataset(ctrl.dataset(), category);
    src->setFps(fps);
    const int planned = src->plannedCount();

    InspectionSession session(ctrl);
    int lastDone = 0;
    int lastQueue = 0;
    qint64 lastLatency = 0;
    double lastFps = 0.0;
    qint64 maxLatency = 0;
    int maxQueue = 0;
    QObject::connect(&session, &InspectionSession::frameInspected, &session,
                     [&](const LiveInspectedFrame& f) {
        lastDone = f.done;
        lastQueue = f.queueDepth;
        lastLatency = f.latencyMs;
        lastFps = f.actualFps;
        maxLatency = qMax(maxLatency, f.latencyMs);
        maxQueue = qMax(maxQueue, f.queueDepth);
        if (f.done == 1 || f.done == planned || (f.done % 25) == 0) {
            out << QStringLiteral("  %1/%2 %3 fps queue=%4 latency=%5 ms %6\n")
                       .arg(f.done)
                       .arg(f.total)
                       .arg(f.actualFps, 0, 'f', 1)
                       .arg(f.queueDepth)
                       .arg(f.latencyMs)
                       .arg(f.result.detected() ? QStringLiteral("NG") : QStringLiteral("OK"));
            out.flush();
        }
    });

    int finishedTotal = -1;
    int finishedNg = -1;
    QEventLoop loop;
    QObject::connect(&session, &InspectionSession::finished, &session,
                     [&](int total, int ng) {
        finishedTotal = total;
        finishedNg = ng;
        loop.quit();
    });
    QObject::connect(&session, &InspectionSession::errorOccurred, &session,
                     [&](const QString& msg) {
        out << "ERROR: " << msg << "\n";
        loop.quit();
    });

    out << "live-smoke " << category
        << " engine=" << (engineKind == EngineKind::DL ? QStringLiteral("dl") : QStringLiteral("cv"))
        << " fps=" << fps << " frames=" << planned << "\n";
    out.flush();

    QElapsedTimer wall;
    wall.start();
    if (!session.start(std::move(src), category)) {
        out << "ERROR: InspectionSession 启动失败\n";
        return 2;
    }
    loop.exec();
    session.stop();
    const double elapsed = wall.elapsed() / 1000.0;

    if (finishedTotal < 0) {
        out << "ERROR: 取流未正常结束\n";
        return 2;
    }
    out << QStringLiteral("done=%1 ng=%2 elapsed=%3 s effective=%4 fps\n")
               .arg(finishedTotal)
               .arg(finishedNg)
               .arg(elapsed, 0, 'f', 2)
               .arg(finishedTotal > 0 && elapsed > 0 ? finishedTotal / elapsed : 0.0, 0, 'f', 2);
    out << QStringLiteral("last: actual=%1 fps queue=%2 latency=%3 ms; max queue=%4 max latency=%5 ms\n")
               .arg(lastFps, 0, 'f', 1)
               .arg(lastQueue)
               .arg(lastLatency)
               .arg(maxQueue)
               .arg(maxLatency);
    if (finishedTotal != planned) {
        out << "ERROR: 未跑完（计划 " << planned << " 张，实际 " << finishedTotal << "）\n";
        return 2;
    }
    out.flush();
    return 0;
}

// 按当前屏 availableGeometry 夹紧客户区并居中。固定 1400×900 在 125%/150%
// 缩放下会连同标题栏超出任务栏以上的可用高度。
static void placeMainWindow(QWindow* win)
{
    if (!win)
        return;
    QScreen* screen = win->screen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    const QRect avail = screen->availableGeometry();
    const int margin = 12;
    win->create();
    const QMargins fm = win->frameMargins();
    int extraW = fm.left() + fm.right();
    int extraH = fm.top() + fm.bottom();
    if (extraH <= 0)
        extraH = 32; // create() 后窗框尚未兑现时的标题栏估计
    extraW = qMax(0, extraW);

    int innerW = avail.width() - extraW - margin * 2;
    int innerH = avail.height() - extraH - margin * 2;
    innerW = qMax(640, innerW);
    innerH = qMax(480, innerH);
    innerW = qMin(innerW, 1600);

    if (win->minimumWidth() > innerW)
        win->setMinimumWidth(innerW);
    if (win->minimumHeight() > innerH)
        win->setMinimumHeight(innerH);

    win->resize(innerW, innerH);
    win->setFramePosition(QPoint(
        avail.x() + (avail.width() - (innerW + extraW)) / 2,
        avail.y() + (avail.height() - (innerH + extraH)) / 2));
}

static int runGui(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("surface_defect_detector"));
    QCoreApplication::setApplicationName(QStringLiteral("surface_defect_detector"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));
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
    if (auto* win = qobject_cast<QWindow*>(engine.rootObjects().constFirst())) {
        placeMainWindow(win);
        win->show();
    }
    return QGuiApplication::exec();
}

int main(int argc, char* argv[])
{
    // Q*Application 构造前只能从 argv 解析 --batch
    QStringList raw;
    for (int i = 0; i < argc; ++i)
        raw << QString::fromLocal8Bit(argv[i]);

    const int liveSmokeIdx = raw.indexOf(QStringLiteral("--live-smoke"));
    const int batchIdx = raw.indexOf(QStringLiteral("--batch"));
    if (liveSmokeIdx >= 0 || batchIdx >= 0) {
        QCoreApplication core(argc, argv);
        QCoreApplication::setOrganizationName(QStringLiteral("surface_defect_detector"));
        QCoreApplication::setApplicationName(QStringLiteral("surface_defect_detector"));
        const int catIdx = liveSmokeIdx >= 0 ? liveSmokeIdx : batchIdx;
        if (catIdx + 1 >= raw.size()) {
            QTextStream(stderr) << "用法: surface_defect_detector --batch|--live-smoke <类别>"
                                   " [--engine cv|dl] [--provider auto|cpu|dml] [--fps N]\n";
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
        int fps = InspectionSession::kDefaultFps;
        const int fpsIdx = raw.indexOf(QStringLiteral("--fps"));
        if (fpsIdx >= 0) {
            if (fpsIdx + 1 >= raw.size()) {
                QTextStream(stderr) << "用法: --fps 1..15\n";
                return 2;
            }
            bool ok = false;
            fps = raw.at(fpsIdx + 1).toInt(&ok);
            if (!ok) {
                QTextStream(stderr) << "无效 --fps\n";
                return 2;
            }
        }
        if (liveSmokeIdx >= 0)
            return runLiveSmoke(raw.at(catIdx + 1), engineKind, epKind, fps);
        return runBatch(raw.at(catIdx + 1), engineKind, epKind);
    }

    return runGui(argc, argv);
}
