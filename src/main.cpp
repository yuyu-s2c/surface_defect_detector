#include "MainWindow.h"
#include "DetectionController.h"
#include "ResultEvaluator.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
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

// 批处理模式：--batch <类别目录或类别名>
// 不弹窗、不进入事件循环，跑完打印指标到 stdout 后退出。
static int runBatch(const QString& categoryArg)
{
    QTextStream out(stdout);

    // 解析类别：优先当作已有类别目录（其上级即数据集根），
    // 否则在自动发现的数据集根下按名字找
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

    if (!ctrl.prepareEngine(category)) {
        out << "ERROR: 无法构建参考模板（" << category << "/train/good 为空或不可读）\n";
        return 2;
    }
    out << "参考模板已构建（train/good 共 "
        << dataset.trainGoodImages(category).size() << " 张）\n\n";

    // 逐图日志由批量信号驱动，与编排逻辑解耦（GUI 侧用同一信号做界面响应）
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
    // good 类误报率 = good 中误报图片比例
    const ImageMetrics& goodIm = imageByDefect[QStringLiteral("good")];
    if (goodIm.total > 0) {
        out << QStringLiteral("good 误报率: %1 (%2/%3 张 good 图误报)\n")
                   .arg(1.0 - goodIm.accuracy(), 0, 'f', 4)
                   .arg(goodIm.total - goodIm.correct)
                   .arg(goodIm.total);
    }
    out.flush();
    return 0;
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("surface_defect_detector"));

    const QStringList args = QCoreApplication::arguments();
    const int batchIdx = args.indexOf(QStringLiteral("--batch"));
    if (batchIdx >= 0) {
        if (batchIdx + 1 >= args.size()) {
            QTextStream(stderr) << "用法: surface_defect_detector --batch <类别目录或类别名>\n";
            return 2;
        }
        // 批处理：跑完即退，不进入事件循环
        return runBatch(args.at(batchIdx + 1));
    }

    // GUI 模式
    QString root = locateDatasetRoot();
    if (root.isEmpty()) {
        root = QFileDialog::getExistingDirectory(
            nullptr, QStringLiteral("请选择数据集根目录（含 metal_nut、screw 等类别目录）"),
            QDir::currentPath());
        if (root.isEmpty())
            return 0;
    }

    MainWindow w;
    if (!w.loadDataset(root)) {
        QTextStream(stderr) << "数据集加载失败: " << root << "\n";
        return 1;
    }
    w.show();
    return QApplication::exec();
}
