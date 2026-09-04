#include "AuthViewModel.h"
#include "DetectionController.h"
#include "InspectionSession.h"
#include "MainViewModel.h"
#include "ResultEvaluator.h"
#include "ResultExporter.h"
#include "log/AppLog.h"
#include "sources/CompositeRejectSink.h"
#include "sources/FileRejectSink.h"
#include "sources/FolderSource.h"
#include "sources/LogRejectSink.h"
#include "sources/SimulatedDoSink.h"
#include "sources/WebcamSource.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMargins>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QScreen>
#include <QTextStream>
#include <QTimer>
#include <QWindow>

#include <memory>

static bool installAppLog(QString* error)
{
    AppLog::Options opt;
    if (!AppLog::parseOptions(QCoreApplication::arguments(), &opt, error))
        return false;
    AppLog::install(opt);
    return true;
}

static const char* engineLabel(EngineKind kind)
{
    return kind == EngineKind::DL ? "dl" : "cv";
}

static const char* epLabel(OrtEpKind ep)
{
    switch (ep) {
    case OrtEpKind::Cpu:
        return "cpu";
    case OrtEpKind::Dml:
        return "dml";
    case OrtEpKind::Auto:
    default:
        return "auto";
    }
}

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
        qCWarning(lcApp) << "未找到数据集根目录";
        out << "ERROR: 未找到数据集根目录（上级/上上级目录中没有 metal_nut、screw 等类别目录）\n";
        return 2;
    }
    qCInfo(lcApp) << "模式 batch 类别" << category
                  << "引擎" << engineLabel(engineKind)
                  << "provider" << epLabel(epKind)
                  << "数据集" << datasetRoot;

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
static int runLiveSmoke(const QString& categoryArg, EngineKind engineKind, OrtEpKind epKind,
                        int fps, QueueOverflowPolicy overflow)
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
        qCWarning(lcApp) << "未找到数据集根目录";
        out << "ERROR: 未找到数据集根目录\n";
        return 2;
    }
    qCInfo(lcApp) << "模式 live-smoke 类别" << category
                  << "引擎" << engineLabel(engineKind)
                  << "provider" << epLabel(epKind)
                  << "fps" << fps
                  << "数据集" << datasetRoot;

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
    src->setOverflowPolicy(overflow);
    const int planned = src->plannedCount();
    const QString engineName = engineKind == EngineKind::DL
        ? QStringLiteral("dl") : QStringLiteral("cv");
    QString sessionId;
    const QString sessionDir = ResultExporter::makeSessionDir(
        datasetRoot, category, engineName, &sessionId);

    InspectionSession session(ctrl);
    auto composite = std::make_unique<CompositeRejectSink>();
    composite->add(std::make_unique<LogRejectSink>());
    composite->add(std::make_unique<FileRejectSink>(sessionDir));
    composite->add(std::make_unique<SimulatedDoSink>(sessionDir));
    session.setRejectSink(std::move(composite));
    session.setConsecutiveNgLimit(0);
    session.setSessionMeta(sessionId, sessionDir, engineName,
                           engineKind == EngineKind::DL
                               ? ctrl.ortEpPolicyLabel()
                               : QStringLiteral("OpenCV CPU"));

    int lastDone = 0;
    int lastQueue = 0;
    qint64 lastLatency = 0;
    double lastFps = 0.0;
    qint64 maxLatency = 0;
    int maxQueue = 0;
    int lastDropped = 0;
    int lastLate = 0;
    QObject::connect(&session, &InspectionSession::frameInspected, &session,
                     [&](const LiveInspectedFrame& f) {
        lastDone = f.done;
        lastQueue = f.queueDepth;
        lastLatency = f.latencyMs;
        lastFps = f.actualFps;
        lastDropped = f.dropped;
        lastLate = f.lateCount;
        maxLatency = qMax(maxLatency, f.latencyMs);
        maxQueue = qMax(maxQueue, f.queueDepth);
        if (f.done == 1 || f.done == planned || (f.done % 25) == 0) {
            out << QStringLiteral("  %1/%2 %3 fps queue=%4 latency=%5 ms %6%7\n")
                       .arg(f.done)
                       .arg(f.total)
                       .arg(f.actualFps, 0, 'f', 1)
                       .arg(f.queueDepth)
                       .arg(f.latencyMs)
                       .arg(f.result.detected() ? QStringLiteral("NG") : QStringLiteral("OK"))
                       .arg(f.lateEject ? QStringLiteral(" LATE") : QString());
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

    const QString overflowName = overflow == QueueOverflowPolicy::DropOldest
        ? QStringLiteral("drop") : QStringLiteral("block");
    out << "live-smoke " << category
        << " engine=" << engineName
        << " fps=" << fps
        << " overflow=" << overflowName
        << " frames=" << planned << "\n";
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
    const LiveSessionSummary sum = session.lastSummary();

    if (finishedTotal < 0) {
        out << "ERROR: 取流未正常结束\n";
        return 2;
    }
    out << QStringLiteral("done=%1 ng=%2 dropped=%3 late=%4 elapsed=%5 s effective=%6 fps\n")
               .arg(finishedTotal)
               .arg(finishedNg)
               .arg(sum.dropped > 0 ? sum.dropped : lastDropped)
               .arg(sum.lateEject > 0 ? sum.lateEject : lastLate)
               .arg(elapsed, 0, 'f', 2)
               .arg(finishedTotal > 0 && elapsed > 0 ? finishedTotal / elapsed : 0.0, 0, 'f', 2);
    out << QStringLiteral("last: actual=%1 fps queue=%2 latency=%3 ms; max queue=%4 max latency=%5 ms\n")
               .arg(lastFps, 0, 'f', 1)
               .arg(lastQueue)
               .arg(lastLatency)
               .arg(maxQueue)
               .arg(maxLatency);
    out << "session=" << sessionDir << "\n";
    const bool dropMode = overflow == QueueOverflowPolicy::DropOldest;
    if (!dropMode && finishedTotal != planned) {
        out << "ERROR: 未跑完（计划 " << planned << " 张，实际 " << finishedTotal << "）\n";
        return 2;
    }
    if (!QFileInfo::exists(sessionDir + QStringLiteral("/session.csv"))
        || !QFileInfo::exists(sessionDir + QStringLiteral("/rejects.csv"))
        || !QFileInfo::exists(sessionDir + QStringLiteral("/do_map.csv"))) {
        out << "ERROR: 未写出 session.csv / rejects.csv / do_map.csv\n";
        return 2;
    }
    if (finishedNg > 0 && !QFileInfo::exists(sessionDir + QStringLiteral("/do_pulses.csv"))) {
        out << "ERROR: 有不合格但未写出 do_pulses.csv\n";
        return 2;
    }
    out.flush();
    return 0;
}

// 无头冒烟：本机摄像头限时取流。无 EOF，必须靠 --seconds 停。不是产品 --live。
static int runWebcamSmoke(const QString& categoryArg, EngineKind engineKind, OrtEpKind epKind,
                          int fps, int device, int seconds)
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
        qCWarning(lcApp) << "未找到数据集根目录";
        out << "ERROR: 未找到数据集根目录\n";
        return 2;
    }
    qCInfo(lcApp) << "模式 webcam-smoke 类别" << category
                  << "引擎" << engineLabel(engineKind)
                  << "provider" << epLabel(epKind)
                  << "fps" << fps
                  << "device" << device
                  << "数据集" << datasetRoot;

    QString probeDetail;
    if (!WebcamSource::probe(device, &probeDetail)) {
        qCWarning(lcApp) << probeDetail;
        out << "ERROR: " << probeDetail << "\n";
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
    seconds = qBound(1, seconds, 60);
    device = qMax(0, device);
    auto src = std::make_unique<WebcamSource>();
    src->setDeviceIndex(device);
    src->setFps(fps);

    const QString engineName = engineKind == EngineKind::DL
        ? QStringLiteral("dl") : QStringLiteral("cv");
    QString sessionId;
    const QString sessionDir = ResultExporter::makeSessionDir(
        datasetRoot, category, engineName, &sessionId);

    InspectionSession session(ctrl);
    auto composite = std::make_unique<CompositeRejectSink>();
    composite->add(std::make_unique<LogRejectSink>());
    composite->add(std::make_unique<FileRejectSink>(sessionDir));
    composite->add(std::make_unique<SimulatedDoSink>(sessionDir));
    session.setRejectSink(std::move(composite));
    session.setConsecutiveNgLimit(0);
    session.setSessionMeta(sessionId, sessionDir, engineName,
                           engineKind == EngineKind::DL
                               ? ctrl.ortEpPolicyLabel()
                               : QStringLiteral("OpenCV CPU"));

    int lastDone = 0;
    int lastQueue = 0;
    qint64 lastLatency = 0;
    double lastFps = 0.0;
    qint64 maxLatency = 0;
    int maxQueue = 0;
    int lastDropped = 0;
    int lastLate = 0;
    QObject::connect(&session, &InspectionSession::frameInspected, &session,
                     [&](const LiveInspectedFrame& f) {
        lastDone = f.done;
        lastQueue = f.queueDepth;
        lastLatency = f.latencyMs;
        lastFps = f.actualFps;
        lastDropped = f.dropped;
        lastLate = f.lateCount;
        maxLatency = qMax(maxLatency, f.latencyMs);
        maxQueue = qMax(maxQueue, f.queueDepth);
        if (f.done == 1 || (f.done % 10) == 0) {
            out << QStringLiteral("  %1 %2 fps queue=%3 latency=%4 ms %5%6\n")
                       .arg(f.done)
                       .arg(f.actualFps, 0, 'f', 1)
                       .arg(f.queueDepth)
                       .arg(f.latencyMs)
                       .arg(f.result.detected() ? QStringLiteral("NG") : QStringLiteral("OK"))
                       .arg(f.lateEject ? QStringLiteral(" LATE") : QString());
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

    out << "webcam-smoke " << category
        << " engine=" << engineName
        << " fps=" << fps
        << " device=" << device
        << " seconds=" << seconds
        << " overflow=drop"
        << "  " << probeDetail << "\n";
    out.flush();

    QElapsedTimer wall;
    wall.start();
    if (!session.start(std::move(src), category)) {
        out << "ERROR: InspectionSession 启动失败（本机摄像头打不开）\n";
        return 2;
    }
    // 只退出事件循环；stop() 放在 loop 之后，避免在 GUI 线程里堵死 wait。
    QTimer::singleShot(seconds * 1000, &loop, &QEventLoop::quit);
    loop.exec();
    session.stop();
    const double elapsed = wall.elapsed() / 1000.0;
    const LiveSessionSummary sum = session.lastSummary();

    if (finishedTotal < 0 && lastDone <= 0) {
        out << "ERROR: 取流未正常结束\n";
        return 2;
    }
    if (finishedTotal < 0)
        finishedTotal = lastDone;
    if (finishedNg < 0)
        finishedNg = sum.ng;
    out << QStringLiteral("done=%1 ng=%2 dropped=%3 late=%4 elapsed=%5 s effective=%6 fps\n")
               .arg(finishedTotal)
               .arg(finishedNg)
               .arg(sum.dropped > 0 ? sum.dropped : lastDropped)
               .arg(sum.lateEject > 0 ? sum.lateEject : lastLate)
               .arg(elapsed, 0, 'f', 2)
               .arg(finishedTotal > 0 && elapsed > 0 ? finishedTotal / elapsed : 0.0, 0, 'f', 2);
    out << QStringLiteral("last: actual=%1 fps queue=%2 latency=%3 ms; max queue=%4 max latency=%5 ms\n")
               .arg(lastFps, 0, 'f', 1)
               .arg(lastQueue)
               .arg(lastLatency)
               .arg(maxQueue)
               .arg(maxLatency);
    out << "session=" << sessionDir << "\n";
    if (!QFileInfo::exists(sessionDir + QStringLiteral("/session.csv"))
        || !QFileInfo::exists(sessionDir + QStringLiteral("/do_map.csv"))) {
        out << "ERROR: 未写出 session.csv / do_map.csv\n";
        return 2;
    }
    if (finishedNg > 0 && !QFileInfo::exists(sessionDir + QStringLiteral("/do_pulses.csv"))) {
        out << "ERROR: 有不合格但未写出 do_pulses.csv\n";
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

static QWindow* findNamedWindow(QObject* root, const QString& name)
{
    if (!root)
        return nullptr;
    if (auto* self = qobject_cast<QWindow*>(root)) {
        if (self->objectName() == name)
            return self;
    }
    return root->findChild<QWindow*>(name);
}

static QWindow* findNamedWindowFallback(const QString& name)
{
    const auto windows = QGuiApplication::allWindows();
    for (QWindow* w : windows) {
        if (w && w->objectName() == name)
            return w;
    }
    return nullptr;
}

static void placeLoginWindow(QWindow* win)
{
    if (!win)
        return;
    QScreen* screen = win->screen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    const QRect avail = screen->availableGeometry();
    win->create();
    const QMargins fm = win->frameMargins();
    int extraW = qMax(0, fm.left() + fm.right());
    int extraH = fm.top() + fm.bottom();
    if (extraH <= 0)
        extraH = 32;

    const int innerW = 460;
    const int innerH = 560;
    win->resize(innerW, innerH);
    win->setFramePosition(QPoint(
        avail.x() + (avail.width() - (innerW + extraW)) / 2,
        avail.y() + (avail.height() - (innerH + extraH)) / 2));
}

static int runGui(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    // 登录窗藏掉的瞬间主窗可能还没 show；默认 true 会当成最后一扇窗关了直接 quit。
    app.setQuitOnLastWindowClosed(false);
    QCoreApplication::setOrganizationName(QStringLiteral("surface_defect_detector"));
    QCoreApplication::setApplicationName(QStringLiteral("surface_defect_detector"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QString logErr;
    if (!installAppLog(&logErr)) {
        QTextStream(stderr) << logErr << '\n';
        return 2;
    }
    qCInfo(lcApp) << "模式 gui";

    auto* vm = new MainViewModel(&app);
    auto* auth = new AuthViewModel(&app);
    const QString root = locateDatasetRoot();
    if (!root.isEmpty())
        vm->loadDatasetPath(root);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("app"), vm);
    engine.rootContext()->setContextProperty(QStringLiteral("auth"), auth);
    QObject::connect(&engine, &QQmlEngine::warnings,
                     [](const QList<QQmlError>& list) {
                         for (const QQmlError& e : list)
                             qCWarning(lcGui) << e.toString();
                     });
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    // 两个独立根窗口：Main 不能挂在 LoginWindow 里，否则藏登录窗时主窗当子窗口一起没了。
    engine.loadFromModule("SurfaceDefect", "LoginWindow");
    engine.loadFromModule("SurfaceDefect", "Main");
    if (engine.rootObjects().size() < 2) {
        qCWarning(lcGui) << "QML 加载失败（SurfaceDefect/LoginWindow 或 Main）";
        QTextStream(stderr) << "ERROR: QML 加载失败（SurfaceDefect/LoginWindow 或 Main）\n";
        return 1;
    }
    QWindow* loginWin = nullptr;
    QWindow* mainWin = nullptr;
    for (QObject* obj : engine.rootObjects()) {
        if (!loginWin)
            loginWin = findNamedWindow(obj, QStringLiteral("loginWindow"));
        if (!mainWin)
            mainWin = findNamedWindow(obj, QStringLiteral("mainWindow"));
    }
    if (!loginWin)
        loginWin = findNamedWindowFallback(QStringLiteral("loginWindow"));
    if (!mainWin)
        mainWin = findNamedWindowFallback(QStringLiteral("mainWindow"));
    if (!loginWin || !mainWin) {
        qCWarning(lcGui) << "找不到登录窗或主窗";
        QTextStream(stderr) << "ERROR: 找不到登录窗或主窗\n";
        return 1;
    }
    placeLoginWindow(loginWin);
    placeMainWindow(mainWin);
    loginWin->show();
    loginWin->requestActivate();
    return QGuiApplication::exec();
}

int main(int argc, char* argv[])
{
    // Q*Application 构造前只能从 argv 解析 --batch
    QStringList raw;
    for (int i = 0; i < argc; ++i)
        raw << QString::fromLocal8Bit(argv[i]);

    const int liveSmokeIdx = raw.indexOf(QStringLiteral("--live-smoke"));
    const int webcamSmokeIdx = raw.indexOf(QStringLiteral("--webcam-smoke"));
    const int batchIdx = raw.indexOf(QStringLiteral("--batch"));
    if (liveSmokeIdx >= 0 || webcamSmokeIdx >= 0 || batchIdx >= 0) {
        QCoreApplication core(argc, argv);
        QCoreApplication::setOrganizationName(QStringLiteral("surface_defect_detector"));
        QCoreApplication::setApplicationName(QStringLiteral("surface_defect_detector"));
        QString logErr;
        if (!installAppLog(&logErr)) {
            QTextStream(stderr) << logErr << '\n';
            return 2;
        }
        const int catIdx = liveSmokeIdx >= 0 ? liveSmokeIdx
                          : (webcamSmokeIdx >= 0 ? webcamSmokeIdx : batchIdx);
        if (catIdx + 1 >= raw.size()) {
            QTextStream(stderr) << "用法: surface_defect_detector --batch|--live-smoke|--webcam-smoke <类别>"
                                   " [--engine cv|dl] [--provider auto|cpu|dml] [--fps N]"
                                   " [--overflow block|drop] [--device N] [--seconds N]"
                                   " [--log-level debug|info|warning] [--log-file 路径]\n";
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
        QueueOverflowPolicy overflow = QueueOverflowPolicy::Block;
        const int overflowIdx = raw.indexOf(QStringLiteral("--overflow"));
        if (overflowIdx >= 0) {
            if (overflowIdx + 1 >= raw.size()) {
                QTextStream(stderr) << "用法: --overflow block|drop\n";
                return 2;
            }
            const QString v = raw.at(overflowIdx + 1);
            if (v == QStringLiteral("drop"))
                overflow = QueueOverflowPolicy::DropOldest;
            else if (v != QStringLiteral("block")) {
                QTextStream(stderr) << "未知 --overflow: " << v << "（可选 block|drop）\n";
                return 2;
            }
        }
        if (liveSmokeIdx >= 0)
            return runLiveSmoke(raw.at(catIdx + 1), engineKind, epKind, fps, overflow);
        if (webcamSmokeIdx >= 0) {
            int device = 0;
            const int deviceIdx = raw.indexOf(QStringLiteral("--device"));
            if (deviceIdx >= 0) {
                if (deviceIdx + 1 >= raw.size()) {
                    QTextStream(stderr) << "用法: --device N\n";
                    return 2;
                }
                bool ok = false;
                device = raw.at(deviceIdx + 1).toInt(&ok);
                if (!ok || device < 0) {
                    QTextStream(stderr) << "无效 --device\n";
                    return 2;
                }
            }
            int seconds = 8;
            const int secIdx = raw.indexOf(QStringLiteral("--seconds"));
            if (secIdx >= 0) {
                if (secIdx + 1 >= raw.size()) {
                    QTextStream(stderr) << "用法: --seconds N\n";
                    return 2;
                }
                bool ok = false;
                seconds = raw.at(secIdx + 1).toInt(&ok);
                if (!ok) {
                    QTextStream(stderr) << "无效 --seconds\n";
                    return 2;
                }
            }
            Q_UNUSED(overflow);
            return runWebcamSmoke(raw.at(catIdx + 1), engineKind, epKind, fps, device, seconds);
        }
        return runBatch(raw.at(catIdx + 1), engineKind, epKind);
    }

    return runGui(argc, argv);
}
