#include "MainViewModel.h"

#include "DisplayNames.h"
#include "Format.h"
#include "ImageConvert.h"
#include "OverlayColors.h"
#include "ResultExporter.h"
#include "sources/CompositeRejectSink.h"
#include "sources/FileRejectSink.h"
#include "sources/FolderSource.h"
#include "sources/IFrameSource.h"
#include "sources/LogRejectSink.h"
#include "sources/SimulatedDoSink.h"
#include "sources/WebcamSource.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QVariantMap>
#include <QtMath>

#include <memory>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

MainViewModel::MainViewModel(QObject* parent)
    : QObject(parent)
    , m_session(m_ctrl, this)
    , m_datasetModel(new DatasetTreeModel(this))
    , m_boxModel(new BoxListModel(this))
    , m_metricsModel(new MetricsListModel(this))
    , m_compareModel(new CompareListModel(this))
    , m_ngModel(new NgListModel(this))
    , m_doPulseModel(new DoPulseModel(this))
{
    m_appliedCv = m_cv;
    m_appliedDl = m_dl;
    loadStationSettings();

    connect(&m_ctrl, &DetectionController::progressChanged,
            this, &MainViewModel::onProgress);
    connect(&m_ctrl, &DetectionController::busyChanged,
            this, &MainViewModel::onBusyChanged);
    connect(&m_ctrl, &DetectionController::currentDetectFinished,
            this, &MainViewModel::onDetectFinished);
    connect(&m_ctrl, &DetectionController::enginePrepared,
            this, &MainViewModel::onEnginePrepared);
    connect(&m_ctrl, &DetectionController::batchFinished,
            this, &MainViewModel::onBatchFinished);
    connect(&m_ctrl, &DetectionController::compareFinished,
            this, &MainViewModel::onCompareFinished);
    connect(&m_session, &InspectionSession::frameInspected,
            this, &MainViewModel::onLiveFrame);
    connect(&m_session, &InspectionSession::finished,
            this, &MainViewModel::onLiveFinished);
    connect(&m_session, &InspectionSession::errorOccurred,
            this, &MainViewModel::onLiveError);
    connect(&m_preview, &WebcamPreview::frameReady,
            this, &MainViewModel::onPreviewFrame, Qt::QueuedConnection);
    connect(&m_preview, &WebcamPreview::errorOccurred,
            this, &MainViewModel::onPreviewError, Qt::QueuedConnection);

    // 摄像头预览等登录后再开（onAuthChanged），避免登录层底下占设备。
}

MainViewModel::~MainViewModel()
{
    m_liveStarting = false;
    stopPreview();
    m_session.stop();
}

QString MainViewModel::currentCategoryLabel() const
{
    return folderDisplayName(m_currentCategory);
}

QString MainViewModel::imageInfo() const
{
    if (m_previewRunning)
        return QStringLiteral("本机摄像头预览");
    if (m_currentImagePath.isEmpty())
        return QStringLiteral("未选择图片");
    return QStringLiteral("%1 / %2 / %3")
        .arg(folderDisplayName(m_currentCategory),
             folderDisplayName(m_currentDefectType),
             QFileInfo(m_currentImagePath).fileName());
}

QString MainViewModel::scoreRuleText() const
{
    return QStringLiteral("合格 / 不合格 = 图像分达到判定阈值。绿框只标位置，不单独决定剔除。");
}

QString MainViewModel::aboutBody() const
{
    return QStringLiteral(
        "离线质检工作站：本机登录、文件夹图源、模拟取流、班次剔除落盘、模拟 PLC/DO 点表、批量评估、PNG/CSV 导出、传统 CV 与 EfficientAD 双引擎。\n\n"
        "账号三角色：操作员只跑检测台；工艺员可进分析台改参数/引擎；管理员另管本机账号。"
        "口令存在本机 AppData/users.json，不联网。命令行 --batch / --live-smoke / --webcam-smoke 不登录。\n\n"
        "深度学习用本地 ONNX（默认 DirectML，失败回 CPU），不会联网、不会训练。"
        "权重约定：models/<类别>/weights/onnx/<类别>.onnx。"
        "首次对该执行器标定会扫描 train/good 并写入同目录 .calib.json（v3，按 EP 分键），之后复用缓存。\n\n"
        "图像级判定是分数过线，不是掩码面积。面积门只影响绿叠加和 --batch 对照列。\n\n"
        "海康相机离线 / 未接 PLC（完整 Phase 4 等实机）。顶栏可切 FolderSource（按帧率吐当前类 test/）或本机 USB 摄像头（WebcamSource）。"
        "不会把 CameraSource 当假直播。Webcam 帧相对 metal_nut/screw 是分布外，整班 NG 是预期；指标仍以 --batch / --live-smoke 为准。\n\n"
        "不合格走 CompositeRejectSink：日志 + _sessions/ 叠图 CSV + 模拟 DO0.0 脉冲（do_map.csv / do_pulses.csv）。"
        "连续不合格可联锁停线。实机到货只换 CameraSource 并再加一个真实 DO 的 IRejectSink。");
}

QString MainViewModel::shortcutsHelp() const
{
    return QStringLiteral(
        "空格 开自检/停止取流    Esc 停止取流    B 批量    Shift+C 对比引擎\n"
        "1 传统 CV    2 EfficientAD    G 真值叠加    D 检测叠加    F 适应画面\n"
        "I 检测台    A 分析台（工艺员/管理员）    Ctrl+O 打开数据集\n"
        "Ctrl+E 导出当前图    Ctrl+Shift+E 导出批量    F1 关于");
}

TraditionalParams MainViewModel::clampCv(const TraditionalParams& p)
{
    return p.sanitized();
}

DLParams MainViewModel::clampDl(const DLParams& p)
{
    return p.sanitized();
}

QString MainViewModel::missingDlReason(const QString& category) const
{
    const QString expected = m_ctrl.onnxModelCandidates(category).value(0);
    const QString fallback = m_ctrl.onnxModelCandidates(category).value(1);
    if (!m_ctrl.hasOnnxModel(category)) {
        return QStringLiteral("未找到 %1 的 ONNX。约定 %2（回退 %3）。"
                              "请把已训练权重放到该路径，不要改代码。切回传统 CV 仍可检。")
            .arg(category, expected, fallback);
    }
    if (m_ctrl.trainGoodCount(category) < 3) {
        return QStringLiteral("%1 的 train/good 不足 3 张，无法标定 DL 阈值（不是训练失败）。")
            .arg(category);
    }
    return QStringLiteral("无法准备 %1 的 DL 引擎（ONNX 会话失败或标定未完成；约定 %2）。"
                          "DirectML 失败可改命令行 --provider cpu。")
        .arg(category, expected);
}

bool MainViewModel::dlEngineBlocked(const QString& category) const
{
    if (category.isEmpty())
        return false;
    return !m_ctrl.hasOnnxModel(category) || m_ctrl.trainGoodCount(category) < 3;
}

void MainViewModel::showToast(const QString& msg)
{
    m_toastMessage = msg;
    emit toastMessageChanged();
}

void MainViewModel::clearToast()
{
    if (m_toastMessage.isEmpty())
        return;
    m_toastMessage.clear();
    emit toastMessageChanged();
}

void MainViewModel::setStatusTone(const QString& tone)
{
    if (m_statusTone == tone)
        return;
    m_statusTone = tone;
    emit statusTextChanged();
}

QUrl MainViewModel::datasetRootUrl() const
{
    if (m_datasetRoot.isEmpty())
        return {};
    return QUrl::fromLocalFile(m_datasetRoot);
}

void MainViewModel::refreshStationStatus()
{
    const bool dl = (currentKind() == EngineKind::DL);
    QString engineStatus = dl ? QStringLiteral("EfficientAD") : QStringLiteral("传统 CV");
    QString provider = dl ? m_ctrl.ortEpPolicyLabel() : QStringLiteral("OpenCV CPU");
    QString expected;
    bool modelOk = true;
    bool calibOk = false;
    QString calibText;
    QString alert;
    bool alertErr = false;

    if (!m_currentCategory.isEmpty()) {
        expected = m_ctrl.onnxModelCandidates(m_currentCategory).value(0);
        if (dl) {
            modelOk = m_ctrl.hasOnnxModel(m_currentCategory);
            calibOk = m_ctrl.hasCalibCache(m_currentCategory);
            const QString liveEp = m_ctrl.dlProviderLabel(m_currentCategory);
            if (!liveEp.isEmpty())
                provider = liveEp;
            if (!modelOk) {
                engineStatus = QStringLiteral("EfficientAD · 缺模型");
                calibText = QStringLiteral("无 ONNX，无法标定");
                alert = missingDlReason(m_currentCategory);
                alertErr = true;
            } else if (m_ctrl.trainGoodCount(m_currentCategory) < 3) {
                engineStatus = QStringLiteral("EfficientAD · 缺良品");
                calibText = QStringLiteral("train/good 不足 3 张");
                alert = missingDlReason(m_currentCategory);
                alertErr = true;
            } else if (m_ctrl.dlProviderLabel(m_currentCategory).isEmpty()) {
                calibText = calibOk
                    ? QStringLiteral("有标定缓存，加载时复用")
                    : QStringLiteral("首次将扫描 train/good 标定（不是训练）");
                if (!calibOk) {
                    alert = QStringLiteral("该类尚未标定：第一次推理会扫描 train/good 写 .calib.json"
                                          "（DirectML 约十几秒，不是训练）。");
                    alertErr = false;
                }
            } else {
                calibText = m_ctrl.dlLoadedCalibFromCache(m_currentCategory)
                    ? QStringLiteral("已复用标定缓存")
                    : QStringLiteral("本会话已完成阈值标定");
            }
        } else {
            modelOk = m_ctrl.hasOnnxModel(m_currentCategory);
            calibOk = m_ctrl.hasCalibCache(m_currentCategory);
            calibText = QStringLiteral("良品均值/标准差参考");
            const int n = m_ctrl.trainGoodCount(m_currentCategory);
            if (n <= 0) {
                alert = QStringLiteral("%1 的 train/good 为空，传统引擎无法构建参考。")
                            .arg(m_currentCategory);
                alertErr = true;
            }
        }
    } else {
        modelOk = false;
        calibText = QStringLiteral("未选类别");
        provider = dl ? m_ctrl.ortEpPolicyLabel() : QStringLiteral("OpenCV CPU");
    }

    const bool changed = (m_engineStatusText != engineStatus)
        || (m_providerText != provider)
        || (m_calibStatusText != calibText)
        || (m_expectedOnnxPath != expected)
        || (m_modelAvailable != modelOk)
        || (m_calibCached != calibOk)
        || (m_stationAlert != alert)
        || (m_stationAlertIsError != alertErr);
    m_engineStatusText = engineStatus;
    m_providerText = provider;
    m_calibStatusText = calibText;
    m_expectedOnnxPath = expected;
    m_modelAvailable = modelOk;
    m_calibCached = calibOk;
    m_stationAlert = alert;
    m_stationAlertIsError = alertErr;
    if (changed)
        emit stationStatusChanged();
}

bool MainViewModel::canRunBatch() const
{
    if (m_busy || m_liveRunning || m_liveStarting || m_currentCategory.isEmpty())
        return false;
    // 缺模型时按钮灰掉，原因写在画布横幅，避免点进去才失败
    if (currentKind() == EngineKind::DL && dlEngineBlocked(m_currentCategory))
        return false;
    return true;
}

bool MainViewModel::canStartLive() const
{
    if (m_busy || m_liveRunning || m_liveStarting || m_currentCategory.isEmpty())
        return false;
    if (currentKind() == EngineKind::DL && dlEngineBlocked(m_currentCategory))
        return false;
    if (currentKind() == EngineKind::Traditional && m_ctrl.trainGoodCount(m_currentCategory) <= 0)
        return false;
    if (!usingWebcam() && testImageCount(m_currentCategory) <= 0)
        return false;
    return true;
}

QString MainViewModel::liveStartButtonText() const
{
    return usingWebcam() ? QStringLiteral("开线") : QStringLiteral("模拟开线");
}

QString MainViewModel::selfCheckIntroText() const
{
    if (usingWebcam()) {
        return QStringLiteral(
            "确认后用本机摄像头（WebcamSource）开线，不合格打模拟 DO0.0。"
            "海康 CameraSource 仍离线。检出相对当前类别是分布外，整班 NG 是预期。");
    }
    return QStringLiteral(
        "海康保持离线。确认后用 FolderSource 模拟产线，不合格打模拟 DO0.0。");
}

bool MainViewModel::canExportBatch() const
{
    return m_ctrl.lastBatch(currentKind(), m_currentCategory) != nullptr;
}

bool MainViewModel::canExportLive() const
{
    return !m_liveRunning && !m_liveStarting && !m_liveSessionDir.isEmpty();
}

QString MainViewModel::cameraStatusText() const
{
    if (usingWebcam())
        return QStringLiteral("本机摄像头 · OpenCV");
    return QStringLiteral("相机离线");
}

QString MainViewModel::sourceStatusText() const
{
    if (usingWebcam()) {
        if (m_liveRunning)
            return QStringLiteral("取流中 · WebcamSource");
        if (m_previewRunning)
            return QStringLiteral("预览中 · WebcamSource");
        return QStringLiteral("本机摄像头 · WebcamSource");
    }
    if (m_liveRunning)
        return QStringLiteral("模拟取流中 · FolderSource");
    return QStringLiteral("模拟取流 · FolderSource");
}

QString MainViewModel::plcStatusText() const
{
    return QStringLiteral("模拟 PLC · DO0.0");
}

double MainViewModel::liveYieldPercent() const
{
    const int n = m_liveOkCount + m_liveNgCount;
    if (n <= 0)
        return 0.0;
    return 100.0 * double(m_liveOkCount) / double(n);
}

int MainViewModel::testImageCount(const QString& category) const
{
    if (category.isEmpty())
        return 0;
    int n = 0;
    const QStringList defects = m_ctrl.dataset().defectTypes(category);
    for (const QString& d : defects)
        n += m_ctrl.dataset().testImages(category, d).size();
    return n;
}

void MainViewModel::loadStationSettings()
{
    QSettings s;
    s.beginGroup(QStringLiteral("station"));
    m_workOrder = s.value(QStringLiteral("workOrder")).toString();
    m_operatorName = s.value(QStringLiteral("operatorName")).toString();
    const int limit = s.value(QStringLiteral("consecutiveNgLimit"), 8).toInt();
    m_consecutiveNgLimit = qBound(0, limit, 200);
    const int src = s.value(QStringLiteral("liveSourceKind"), 0).toInt();
    m_liveSourceKind = (src == 1) ? 1 : 0;
    if (m_workOrder.isEmpty())
        m_workOrder = QStringLiteral("WO-%1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd")));
}

void MainViewModel::saveStationSettings()
{
    QSettings s;
    s.beginGroup(QStringLiteral("station"));
    s.setValue(QStringLiteral("workOrder"), m_workOrder);
    s.setValue(QStringLiteral("operatorName"), m_operatorName);
    s.setValue(QStringLiteral("consecutiveNgLimit"), m_consecutiveNgLimit);
    s.setValue(QStringLiteral("liveSourceKind"), m_liveSourceKind);
}

void MainViewModel::setWorkOrder(const QString& v)
{
    const QString t = v.trimmed();
    if (m_workOrder == t)
        return;
    m_workOrder = t;
    saveStationSettings();
    emit recipeChanged();
}

void MainViewModel::setOperatorName(const QString& v)
{
    const QString t = v.trimmed();
    if (m_operatorName == t)
        return;
    m_operatorName = t;
    saveStationSettings();
    emit recipeChanged();
}

void MainViewModel::onAuthChanged(bool loggedIn)
{
    if (loggedIn) {
        if (usingWebcam() && !m_liveRunning && !m_liveStarting)
            startPreview();
        return;
    }
    // 不要走 stopLive()：它在 Webcam 下会立刻再 startPreview。
    m_liveStarting = false;
    if (m_liveRunning || m_session.isRunning()) {
        m_session.stop();
        applyLiveSummary(m_session.lastSummary());
        setLiveRunning(false);
        emit workEnabledChanged();
    }
    stopPreview();
    if (m_workMode != 0)
        setWorkMode(0);
}

void MainViewModel::setConsecutiveNgLimit(int n)
{
    n = qBound(0, n, 200);
    if (m_consecutiveNgLimit == n)
        return;
    m_consecutiveNgLimit = n;
    saveStationSettings();
    emit recipeChanged();
}

void MainViewModel::setLiveSourceKind(int kind)
{
    const int k = (kind == 1) ? 1 : 0;
    if (m_liveSourceKind == k)
        return;
    if (m_liveRunning || m_liveStarting)
        return;
    m_liveSourceKind = k;
    saveStationSettings();
    emit liveSourceKindChanged();
    emit workEnabledChanged();
    emit stationStatusChanged();
    if (k == 1) {
        startPreview();
    } else {
        stopPreview();
        if (!m_currentImagePath.isEmpty())
            loadCurrentImage();
        else {
            m_currentBgr.release();
            m_sourceImage = {};
            emit imageChanged();
        }
    }
}

void MainViewModel::rebuildSelfCheck()
{
    m_selfCheckItems.clear();
    m_selfCheckPassed = true;
    auto add = [this](const QString& title, const QString& detail, bool ok) {
        m_selfCheckItems.push_back(QVariantMap{
            {QStringLiteral("title"), title},
            {QStringLiteral("detail"), detail},
            {QStringLiteral("ok"), ok},
        });
        if (!ok)
            m_selfCheckPassed = false;
    };

    add(QStringLiteral("相机"),
        usingWebcam()
            ? QStringLiteral("海康离线。本班走 WebcamSource，不启动 CameraSource。")
            : QStringLiteral("离线。本工位不启动 CameraSource，不发假直播。完整 P4 只换该类。"),
        true);

    if (usingWebcam()) {
        if (m_currentCategory.isEmpty()) {
            add(QStringLiteral("图源"),
                QStringLiteral("未选类别，无法标定引擎（Webcam 仍要当前类的 train/good）。"),
                false);
        } else {
            // 预览已占设备：再 probe() 会抢不到相机。出过帧就当探活通过。
            if (m_preview.hasFrame()) {
                add(QStringLiteral("图源"),
                    QStringLiteral("本机摄像头 · OpenCV · 设备 %1 · %2×%3 · %4 帧/秒 · 类别 %5 · 预览中")
                        .arg(m_preview.deviceIndex())
                        .arg(m_preview.lastWidth())
                        .arg(m_preview.lastHeight())
                        .arg(m_liveTargetFps)
                        .arg(folderDisplayName(m_currentCategory)),
                    true);
            } else if (m_preview.isRunning()) {
                add(QStringLiteral("图源"),
                    QStringLiteral("本机摄像头预览启动中 · %1 帧/秒 · 类别 %2")
                        .arg(m_liveTargetFps)
                        .arg(folderDisplayName(m_currentCategory)),
                    true);
            } else if (!m_preview.lastError().isEmpty()) {
                add(QStringLiteral("图源"), m_preview.lastError(), false);
            } else {
                QString detail;
                const bool camOk = WebcamSource::probe(0, &detail);
                add(QStringLiteral("图源"),
                    camOk
                        ? QStringLiteral("%1 · %2 帧/秒 · 类别 %3")
                              .arg(detail)
                              .arg(m_liveTargetFps)
                              .arg(folderDisplayName(m_currentCategory))
                        : detail,
                    camOk);
            }
        }
    } else {
        const int planned = testImageCount(m_currentCategory);
        if (m_currentCategory.isEmpty()) {
            add(QStringLiteral("图源"),
                QStringLiteral("未选类别，无法用 FolderSource 模拟取流。"),
                false);
        } else if (planned <= 0) {
            add(QStringLiteral("图源"),
                QStringLiteral("%1 的 test/ 为空。").arg(folderDisplayName(m_currentCategory)),
                false);
        } else {
            add(QStringLiteral("图源"),
                QStringLiteral("FolderSource · %1 · test/ %2 张 · %3 帧/秒")
                    .arg(folderDisplayName(m_currentCategory))
                    .arg(planned)
                    .arg(m_liveTargetFps),
                true);
        }
    }

    if (currentKind() == EngineKind::DL) {
        if (dlEngineBlocked(m_currentCategory)) {
            add(QStringLiteral("模型"), missingDlReason(m_currentCategory), false);
        } else {
            const QString calib = m_ctrl.hasCalibCache(m_currentCategory)
                ? QStringLiteral("已有 .calib.json，加载时复用")
                : QStringLiteral("首次将扫描 train/good 标定（不是训练）");
            add(QStringLiteral("模型"),
                QStringLiteral("EfficientAD · %1 · %2")
                    .arg(m_ctrl.ortEpPolicyLabel(), calib),
                true);
        }
    } else {
        const int good = m_currentCategory.isEmpty() ? 0 : m_ctrl.trainGoodCount(m_currentCategory);
        add(QStringLiteral("模型"),
            good > 0
                ? QStringLiteral("传统 CV · train/good %1 张参考").arg(good)
                : QStringLiteral("train/good 为空，传统引擎无法构建参考。"),
            good > 0);
    }

    const QString outDir = m_datasetRoot.isEmpty()
        ? QStringLiteral("（先打开数据集根）")
        : (m_datasetRoot + QStringLiteral("/_sessions/"));
    add(QStringLiteral("剔除输出"),
        QStringLiteral("模拟 DO0.0 脉冲 + 日志 + %1（叠图 / CSV / do_map.csv）").arg(outDir),
        !m_datasetRoot.isEmpty());

    QString recipe = m_workOrder.isEmpty()
        ? QStringLiteral("工单将按日期自动生成")
        : QStringLiteral("工单 %1").arg(m_workOrder);
    if (!m_operatorName.isEmpty())
        recipe += QStringLiteral(" · 操作员 %1").arg(m_operatorName);
    add(QStringLiteral("配方"), recipe, true);

    add(QStringLiteral("联锁"),
        m_consecutiveNgLimit > 0
            ? QStringLiteral("连续不合格 %1 张停线（--live-smoke 默认关闭，保证跑完一类）")
                  .arg(m_consecutiveNgLimit)
            : (usingWebcam()
                   ? QStringLiteral("连续不合格联锁已关，将持续取流直到手动停止")
                   : QStringLiteral("连续不合格联锁已关，将跑完 FolderSource playlist")),
        true);

    m_selfCheckHint = m_selfCheckPassed
        ? (usingWebcam()
               ? QStringLiteral("确认后用本机摄像头开线。海康保持离线。")
               : QStringLiteral("确认后开始模拟产线。海康保持离线。"))
        : QStringLiteral("自检未过，先处理红色项。");
    emit selfCheckChanged();
}

void MainViewModel::requestStartLive()
{
    if (m_liveRunning || m_liveStarting) {
        stopLive();
        return;
    }
    rebuildSelfCheck();
    emit selfCheckRequested();
}

void MainViewModel::confirmStartLive()
{
    rebuildSelfCheck();
    if (!m_selfCheckPassed) {
        raiseError(m_selfCheckHint);
        return;
    }
    startLive();
}

void MainViewModel::appendDoPulse(const LiveInspectedFrame& frame)
{
    DoPulseModel::Record rec;
    rec.seq = m_doPulseModel->count() + 1;
    rec.point = QString::fromLatin1(SimulatedDoSink::kRejectPoint);
    rec.action = QStringLiteral("REJECT");
    rec.pulseMs = SimulatedDoSink::kPulseMs;
    rec.fileName = QFileInfo(frame.path).fileName();
    rec.defectLabel = folderDisplayName(frame.defectType);
    rec.latencyMs = frame.latencyMs;
    rec.lateEject = frame.lateEject;
    m_doPulseModel->append(rec);
}

EngineKind MainViewModel::currentKind() const
{
    return m_engineKind == 1 ? EngineKind::DL : EngineKind::Traditional;
}

QString MainViewModel::currentEngineName() const
{
    return currentKind() == EngineKind::DL ? QStringLiteral("dl") : QStringLiteral("cv");
}

void MainViewModel::setStatusText(const QString& text)
{
    if (m_statusText == text)
        return;
    m_statusText = text;
    emit statusTextChanged();
}

void MainViewModel::raiseError(const QString& msg)
{
    m_errorMessage = msg;
    emit errorMessageChanged();
}

bool MainViewModel::loadDataset(const QUrl& folder)
{
    return loadDatasetPath(folder.toLocalFile());
}

bool MainViewModel::loadDatasetPath(const QString& path)
{
    stopLive();
    if (!m_ctrl.loadDataset(path)) {
        raiseError(QStringLiteral("数据集加载失败：%1").arg(path));
        return false;
    }
    m_datasetModel->rebuild(m_ctrl.dataset());
    m_datasetRoot = m_ctrl.dataset().rootPath();
    m_hasDataset = true;
    m_currentCategory.clear();
    m_currentDefectType.clear();
    m_currentImagePath.clear();
    m_missingModelDialogShown = false;
    m_currentGt.release();
    m_gtOverlayImage = {};
    clearDetection();
    if (!m_previewRunning) {
        m_currentBgr.release();
        m_sourceImage = {};
    }
    refreshBatchDependent();
    emit hasDatasetChanged();
    emit selectionChanged();
    emit imageChanged();
    emit workEnabledChanged();
    refreshStationStatus();
    setStatusTone(QStringLiteral("normal"));
    setStatusText(QStringLiteral("数据集：%1").arg(m_datasetRoot));
    showToast(QStringLiteral("已加载数据集"));
    return true;
}

void MainViewModel::selectFromModelIndex(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    selectNode(index.data(DatasetTreeModel::NodeTypeRole).toString(),
               index.data(DatasetTreeModel::CategoryRole).toString(),
               index.data(DatasetTreeModel::DefectTypeRole).toString(),
               index.data(DatasetTreeModel::ImagePathRole).toString());
}

void MainViewModel::selectNode(const QString& nodeType, const QString& category,
                              const QString& defectType, const QString& imagePath)
{
    if (m_liveRunning || m_liveStarting)
        return;
    if (category.isEmpty())
        return;
    const bool categoryChanged = (category != m_currentCategory);
    m_currentCategory = category;
    if (categoryChanged)
        m_missingModelDialogShown = false;
    if (nodeType == QLatin1String("image")) {
        m_currentDefectType = defectType;
        m_currentImagePath = imagePath;
        if (categoryChanged)
            syncParamsFromSettings();
        // 预览占画布：只记下路径，切回文件夹再 loadCurrentImage。
        if (!m_previewRunning)
            loadCurrentImage();
        emit selectionChanged();
        emit workEnabledChanged();
        refreshStationStatus();
        return;
    }
    if (nodeType == QLatin1String("defect"))
        m_currentDefectType = defectType;
    if (categoryChanged)
        syncParamsFromSettings();
    refreshBatchDependent();
    emit selectionChanged();
    emit workEnabledChanged();
    refreshStationStatus();
}

void MainViewModel::loadCurrentImage()
{
    m_currentBgr = cv::imread(m_currentImagePath.toLocal8Bit().constData(), cv::IMREAD_COLOR);
    if (m_currentBgr.empty()) {
        raiseError(QStringLiteral("无法读取图片：%1").arg(m_currentImagePath));
        return;
    }
    m_sourceImage = bgrToQImage(m_currentBgr);

    m_currentGt.release();
    m_gtOverlayImage = {};
    const QString gtPath = m_ctrl.dataset().groundTruthMask(
        m_currentCategory, m_currentDefectType, m_currentImagePath);
    if (!gtPath.isEmpty()) {
        cv::Mat gt = cv::imread(gtPath.toLocal8Bit().constData(), cv::IMREAD_GRAYSCALE);
        if (!gt.empty() && gt.size() != m_currentBgr.size())
            cv::resize(gt, gt, m_currentBgr.size(), 0, 0, cv::INTER_NEAREST);
        m_currentGt = gt;
        m_gtOverlayImage = maskToOverlayImage(gt, OverlayColors::gtRed, OverlayColors::gtAlpha);
    }

    clearDetection();
    emit imageChanged();
    runDetectionForCurrent();
}

void MainViewModel::runDetectionForCurrent()
{
    if (m_liveRunning || m_liveStarting || m_previewRunning)
        return;
    if (m_currentBgr.empty() || m_currentCategory.isEmpty())
        return;
    // 缺 ONNX / 良品时不丢进工作线程：避免转圈后才弹失败，也不静默回退 CV
    if (currentKind() == EngineKind::DL && dlEngineBlocked(m_currentCategory)) {
        clearDetection();
        refreshStationStatus();
        setStatusTone(QStringLiteral("warn"));
        setStatusText(missingDlReason(m_currentCategory));
        if (!m_missingModelDialogShown) {
            m_missingModelDialogShown = true;
            raiseError(missingDlReason(m_currentCategory));
        }
        return;
    }
    m_ctrl.prepareAndDetectAsync(m_currentCategory, m_currentBgr);
}

void MainViewModel::clearDetection()
{
    m_currentResult = {};
    m_detOverlayImage = {};
    m_boxRects.clear();
    m_boxModel->clear();
    m_detected = false;
    m_verdictOk = true;
    m_defectCount = 0;
    m_totalArea = 0.0;
    m_minImageArea = 0;
    m_imageScore = 0.0;
    m_imageThreshold = 0.0;
    emit detectionChanged();
}

void MainViewModel::applyDetectionResult(const DetectionResult& result)
{
    m_currentResult = result;
    m_detOverlayImage = maskToOverlayImage(result.defectMask, OverlayColors::detGreen,
                                           OverlayColors::detAlpha);

    QVector<BoxListModel::Row> rows;
    m_boxRects.clear();
    rows.reserve(int(result.boxes.size()));
    for (size_t i = 0; i < result.boxes.size(); ++i) {
        const cv::Rect& r = result.boxes[i];
        BoxListModel::Row row;
        row.x = r.x;
        row.y = r.y;
        row.width = r.width;
        row.height = r.height;
        row.area = (i < result.areas.size()) ? result.areas[i] : 0.0;
        rows.push_back(row);
        m_boxRects.push_back(QVariantMap{
            {QStringLiteral("x"), r.x},
            {QStringLiteral("y"), r.y},
            {QStringLiteral("width"), r.width},
            {QStringLiteral("height"), r.height},
        });
    }
    m_boxModel->setRows(rows);

    m_detected = result.detected();
    const bool isDefect = (m_currentDefectType != QLatin1String("good"));
    m_verdictOk = (result.detected() == isDefect);
    m_defectCount = int(result.boxes.size());
    m_totalArea = result.totalArea;
    m_minImageArea = result.minImageArea;
    m_imageScore = result.imageScore;
    m_imageThreshold = qIsFinite(result.imageThreshold) ? result.imageThreshold : 0.0;
    emit detectionChanged();
    emit workEnabledChanged();
}

void MainViewModel::setEngineKind(int kind)
{
    if (m_liveRunning || m_liveStarting)
        return;
    kind = (kind == 1) ? 1 : 0;
    if (m_engineKind == kind)
        return;
    m_engineKind = kind;
    m_ctrl.setEngineKind(currentKind());
    m_missingModelDialogShown = false;
    emit engineKindChanged();
    syncParamsFromSettings();
    refreshStationStatus();
    runDetectionForCurrent();
    refreshBatchDependent();
    emit workEnabledChanged();
}

void MainViewModel::setGtOverlayVisible(bool visible)
{
    if (m_liveRunning)
        return;
    if (m_gtOverlayVisible == visible)
        return;
    m_gtOverlayVisible = visible;
    emit overlayChanged();
}

void MainViewModel::setDetOverlayVisible(bool visible)
{
    if (m_detOverlayVisible == visible)
        return;
    m_detOverlayVisible = visible;
    emit overlayChanged();
}

void MainViewModel::setInspectorTab(int tab)
{
    tab = qBound(0, tab, 2);
    if (m_inspectorTab == tab)
        return;
    m_inspectorTab = tab;
    emit inspectorTabChanged();
}

void MainViewModel::setWorkMode(int mode)
{
    if (m_liveRunning || m_liveStarting)
        mode = 0;
    mode = (mode == 1) ? 1 : 0;
    if (m_workMode == mode)
        return;
    m_workMode = mode;
    emit workModeChanged();
}

void MainViewModel::setCvZAggThreshold(double v)
{
    v = qBound(0.1, v, 10.0);
    if (qFuzzyCompare(m_cv.zAggThreshold, v))
        return;
    m_cv.zAggThreshold = v;
    emit cvParamsChanged();
    if (!m_syncingParams)
        updateParamsDirty();
}

void MainViewModel::setCvMorphCloseKernel(int v)
{
    v = sanitizedMorphKernel(v);
    if (m_cv.morphCloseKernel == v)
        return;
    m_cv.morphCloseKernel = v;
    emit cvParamsChanged();
    if (!m_syncingParams)
        updateParamsDirty();
}

void MainViewModel::setCvMinDefectArea(int v)
{
    v = qBound(0, v, 100000);
    if (m_cv.minDefectArea == v)
        return;
    m_cv.minDefectArea = v;
    emit cvParamsChanged();
    if (!m_syncingParams)
        updateParamsDirty();
}

void MainViewModel::setCvImageLevelMinArea(int v)
{
    v = qBound(0, v, 1000000);
    if (m_cv.imageLevelMinArea == v)
        return;
    m_cv.imageLevelMinArea = v;
    emit cvParamsChanged();
    if (!m_syncingParams)
        updateParamsDirty();
}

void MainViewModel::setDlThresholdSigma(double v)
{
    v = qBound(0.1, v, 8.0);
    if (qFuzzyCompare(m_dl.thresholdSigma, v))
        return;
    m_dl.thresholdSigma = v;
    emit dlParamsChanged();
    if (!m_syncingParams)
        updateParamsDirty();
}

void MainViewModel::setDlMorphCloseKernel(int v)
{
    v = sanitizedMorphKernel(v);
    if (m_dl.morphCloseKernel == v)
        return;
    m_dl.morphCloseKernel = v;
    emit dlParamsChanged();
    if (!m_syncingParams)
        updateParamsDirty();
}

void MainViewModel::setDlMinDefectArea(int v)
{
    v = qBound(0, v, 100000);
    if (m_dl.minDefectArea == v)
        return;
    m_dl.minDefectArea = v;
    emit dlParamsChanged();
    if (!m_syncingParams)
        updateParamsDirty();
}

void MainViewModel::setDlImageLevelMinArea(int v)
{
    v = qBound(0, v, 1000000);
    if (m_dl.imageLevelMinArea == v)
        return;
    m_dl.imageLevelMinArea = v;
    emit dlParamsChanged();
    if (!m_syncingParams)
        updateParamsDirty();
}

void MainViewModel::fillCv(const TraditionalParams& p)
{
    m_cv = p;
    emit cvParamsChanged();
}

void MainViewModel::fillDl(const DLParams& p)
{
    m_dl = p;
    emit dlParamsChanged();
}

void MainViewModel::updateParamsDirty()
{
    const bool dirty = (m_engineKind == 1)
        ? !(qFuzzyCompare(m_dl.thresholdSigma, m_appliedDl.thresholdSigma)
            && m_dl.morphCloseKernel == m_appliedDl.morphCloseKernel
            && m_dl.minDefectArea == m_appliedDl.minDefectArea
            && m_dl.imageLevelMinArea == m_appliedDl.imageLevelMinArea)
        : !(qFuzzyCompare(m_cv.zAggThreshold, m_appliedCv.zAggThreshold)
            && m_cv.morphCloseKernel == m_appliedCv.morphCloseKernel
            && m_cv.minDefectArea == m_appliedCv.minDefectArea
            && m_cv.imageLevelMinArea == m_appliedCv.imageLevelMinArea);
    if (m_paramsDirty == dirty)
        return;
    m_paramsDirty = dirty;
    emit paramsDirtyChanged();
}

TraditionalParams MainViewModel::loadTraditionalSettings(const QString& category) const
{
    TraditionalParams p = TraditionalParams::defaults();
    QSettings s;
    s.beginGroup(QStringLiteral("params/cv/%1").arg(category));
    if (s.contains(QStringLiteral("zAggThreshold")))
        p.zAggThreshold = s.value(QStringLiteral("zAggThreshold")).toDouble();
    if (s.contains(QStringLiteral("morphCloseKernel")))
        p.morphCloseKernel = s.value(QStringLiteral("morphCloseKernel")).toInt();
    if (s.contains(QStringLiteral("minDefectArea")))
        p.minDefectArea = s.value(QStringLiteral("minDefectArea")).toInt();
    if (s.contains(QStringLiteral("imageLevelMinArea")))
        p.imageLevelMinArea = s.value(QStringLiteral("imageLevelMinArea")).toInt();
    return p.sanitized();
}

DLParams MainViewModel::loadDLSettings(const QString& category) const
{
    DLParams p = DLParams::defaultsFor(category);
    QSettings s;
    s.beginGroup(QStringLiteral("params/dl/%1").arg(category));
    if (s.contains(QStringLiteral("thresholdSigma")))
        p.thresholdSigma = s.value(QStringLiteral("thresholdSigma")).toDouble();
    if (s.contains(QStringLiteral("morphCloseKernel")))
        p.morphCloseKernel = s.value(QStringLiteral("morphCloseKernel")).toInt();
    if (s.contains(QStringLiteral("minDefectArea")))
        p.minDefectArea = s.value(QStringLiteral("minDefectArea")).toInt();
    if (s.contains(QStringLiteral("imageLevelMinArea")))
        p.imageLevelMinArea = s.value(QStringLiteral("imageLevelMinArea")).toInt();
    return p.sanitized();
}

void MainViewModel::saveParamsToSettings()
{
    if (m_currentCategory.isEmpty())
        return;
    QSettings s;
    if (currentKind() == EngineKind::DL) {
        s.beginGroup(QStringLiteral("params/dl/%1").arg(m_currentCategory));
        s.setValue(QStringLiteral("thresholdSigma"), m_dl.thresholdSigma);
        s.setValue(QStringLiteral("morphCloseKernel"), m_dl.morphCloseKernel);
        s.setValue(QStringLiteral("minDefectArea"), m_dl.minDefectArea);
        s.setValue(QStringLiteral("imageLevelMinArea"), m_dl.imageLevelMinArea);
    } else {
        s.beginGroup(QStringLiteral("params/cv/%1").arg(m_currentCategory));
        s.setValue(QStringLiteral("zAggThreshold"), m_cv.zAggThreshold);
        s.setValue(QStringLiteral("morphCloseKernel"), m_cv.morphCloseKernel);
        s.setValue(QStringLiteral("minDefectArea"), m_cv.minDefectArea);
        s.setValue(QStringLiteral("imageLevelMinArea"), m_cv.imageLevelMinArea);
    }
}

void MainViewModel::syncParamsFromSettings()
{
    if (m_currentCategory.isEmpty())
        return;
    m_syncingParams = true;
    const TraditionalParams cv = loadTraditionalSettings(m_currentCategory);
    fillCv(cv);
    m_ctrl.setTraditionalParams(m_currentCategory, cv);
    m_appliedCv = cv;
    const DLParams dl = loadDLSettings(m_currentCategory);
    fillDl(dl);
    m_ctrl.setDLParams(m_currentCategory, dl);
    m_appliedDl = dl;
    m_syncingParams = false;
    updateParamsDirty();
}

void MainViewModel::applyParams()
{
    if (m_currentCategory.isEmpty() || m_liveRunning || m_liveStarting)
        return;
    if (currentKind() == EngineKind::DL) {
        m_dl = clampDl(m_dl);
        emit dlParamsChanged();
        m_ctrl.setDLParams(m_currentCategory, m_dl);
        m_appliedDl = m_dl;
    } else {
        m_cv = clampCv(m_cv);
        emit cvParamsChanged();
        m_ctrl.setTraditionalParams(m_currentCategory, m_cv);
        m_appliedCv = m_cv;
    }
    saveParamsToSettings();
    updateParamsDirty();
    runDetectionForCurrent();
    setStatusTone(QStringLiteral("normal"));
    setStatusText(QStringLiteral("已应用 %1 / %2 参数")
                      .arg(m_currentCategory, currentEngineName()));
    showToast(QStringLiteral("参数已应用"));
}

void MainViewModel::restoreParams()
{
    if (m_currentCategory.isEmpty() || m_liveRunning || m_liveStarting)
        return;
    m_syncingParams = true;
    if (currentKind() == EngineKind::DL) {
        const DLParams p = DLParams::defaultsFor(m_currentCategory);
        fillDl(p);
        m_ctrl.setDLParams(m_currentCategory, p);
        m_appliedDl = p;
    } else {
        const TraditionalParams p = TraditionalParams::defaults();
        fillCv(p);
        m_ctrl.setTraditionalParams(m_currentCategory, p);
        m_appliedCv = p;
    }
    m_syncingParams = false;
    saveParamsToSettings();
    updateParamsDirty();
    runDetectionForCurrent();
    setStatusTone(QStringLiteral("normal"));
    setStatusText(QStringLiteral("已恢复 %1 / %2 的 P2 默认工作点")
                      .arg(m_currentCategory, currentEngineName()));
    showToast(QStringLiteral("已恢复 P2 工作点"));
}

void MainViewModel::runBatch()
{
    if (m_currentCategory.isEmpty() || m_ctrl.isBusy() || m_liveRunning || m_liveStarting)
        return;
    setWorkMode(1);
    setInspectorTab(1);
    m_ctrl.runBatchAsync(m_currentCategory);
}

void MainViewModel::compareEngines()
{
    if (m_currentCategory.isEmpty() || m_ctrl.isBusy() || m_liveRunning || m_liveStarting)
        return;
    setWorkMode(1);
    setInspectorTab(2);
    m_ctrl.compareAsync(m_currentCategory);
}

void MainViewModel::setLiveTargetFps(int fps)
{
    fps = qBound(InspectionSession::kMinFps, fps, InspectionSession::kMaxFps);
    if (m_liveTargetFps == fps)
        return;
    m_liveTargetFps = fps;
    emit liveTargetFpsChanged();
}

void MainViewModel::setLiveRunning(bool running)
{
    if (m_liveRunning == running)
        return;
    if (running) {
        m_gtOverlayBeforeLive = m_gtOverlayVisible;
        if (m_gtOverlayVisible) {
            m_gtOverlayVisible = false;
            emit overlayChanged();
        }
    } else if (m_gtOverlayVisible != m_gtOverlayBeforeLive) {
        m_gtOverlayVisible = m_gtOverlayBeforeLive;
        emit overlayChanged();
    }
    m_liveRunning = running;
    emit liveRunningChanged();
    emit liveSourceKindChanged();
    emit workEnabledChanged();
    emit liveSessionChanged();
}

void MainViewModel::setPreviewRunning(bool running)
{
    if (m_previewRunning == running)
        return;
    m_previewRunning = running;
    emit previewRunningChanged();
    emit liveSourceKindChanged();
    emit selectionChanged();
}

void MainViewModel::startPreview()
{
    if (!usingWebcam() || m_liveRunning || m_liveStarting)
        return;
    if (m_preview.isRunning()) {
        setPreviewRunning(true);
        return;
    }
    if (!m_preview.start(0)) {
        setPreviewRunning(false);
        const QString msg = m_preview.lastError().isEmpty()
            ? QStringLiteral("无法打开本机摄像头（设备 0）。")
            : m_preview.lastError();
        setStatusTone(QStringLiteral("warn"));
        setStatusText(msg);
        showToast(msg);
        return;
    }
    clearDetection();
    setPreviewRunning(true);
    setStatusTone(QStringLiteral("ok"));
    setStatusText(QStringLiteral("本机摄像头预览中…"));
}

void MainViewModel::stopPreview()
{
    // 先落标志，丢掉已入队的预览帧，避免停线/切源后盖住画布。
    setPreviewRunning(false);
    m_preview.stop();
}

void MainViewModel::onPreviewFrame(const QImage& image)
{
    if (!m_previewRunning || m_liveRunning || m_liveStarting)
        return;
    m_sourceImage = image;
    emit imageChanged();
}

void MainViewModel::onPreviewError(const QString& msg)
{
    if (!m_previewRunning || m_liveRunning || m_liveStarting)
        return;
    setPreviewRunning(false);
    setStatusTone(QStringLiteral("warn"));
    setStatusText(msg);
    showToast(msg);
}

void MainViewModel::startLive()
{
    if (!canStartLive()) {
        if (m_currentCategory.isEmpty())
            raiseError(QStringLiteral("请先选择一个类别"));
        else if (currentKind() == EngineKind::DL && dlEngineBlocked(m_currentCategory))
            raiseError(missingDlReason(m_currentCategory));
        else
            raiseError(usingWebcam()
                           ? QStringLiteral("无法开线（缺类别或良品参考）")
                           : QStringLiteral("无法开始模拟取流（缺测试图或良品参考）"));
        return;
    }
    if (m_workOrder.trimmed().isEmpty()) {
        m_workOrder = QStringLiteral("WO-%1")
                          .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmm")));
        saveStationSettings();
        emit recipeChanged();
    }
    setWorkMode(0);
    beginLiveSession(m_currentCategory);
    m_liveStarting = true;
    emit workEnabledChanged();
    m_ctrl.prepareEngineAsync(m_currentCategory);
}

void MainViewModel::beginLiveSession(const QString& category)
{
    m_liveOkCount = 0;
    m_liveNgCount = 0;
    m_liveDroppedCount = 0;
    m_liveLateCount = 0;
    m_liveMaxQueue = 0;
    m_liveMaxLatencyMs = 0;
    m_liveLastNg = false;
    m_liveConsecutiveNg = 0;
    m_liveTaktMs = 0;
    m_livePlannedCount = usingWebcam() ? 0 : testImageCount(category);
    m_liveInterlocked = false;
    m_liveStopReason.clear();
    m_hasLiveSession = false;
    m_liveSessionTitle.clear();
    m_liveSessionDir.clear();
    m_ngModel->clear();
    m_doPulseModel->clear();
    m_liveClock.invalidate();
    emit liveStatsChanged();
    emit liveSessionChanged();
}

void MainViewModel::stopLive()
{
    const bool was = m_liveRunning || m_liveStarting || m_session.isRunning();
    m_liveStarting = false;
    m_session.stop();
    applyLiveSummary(m_session.lastSummary());
    setLiveRunning(false);
    emit workEnabledChanged();
    if (was) {
        setStatusTone(QStringLiteral("normal"));
        setStatusText(usingWebcam() ? QStringLiteral("已停止取流")
                                    : QStringLiteral("已停止模拟取流"));
        if (usingWebcam())
            startPreview();
    }
}

void MainViewModel::onEnginePrepared(bool ok, const QString& category)
{
    if (!m_liveStarting)
        return;
    m_liveStarting = false;
    if (!ok) {
        emit workEnabledChanged();
        if (currentKind() == EngineKind::DL) {
            const QString expected = m_ctrl.onnxModelCandidates(category).value(0);
            raiseError(QStringLiteral("无法准备 %1 的 DL 引擎（ONNX 缺失或 train/good 不足 3 张；约定 %2）")
                           .arg(category, expected));
        } else {
            raiseError(QStringLiteral("无法准备 %1 的传统引擎（train/good 为空或不可读）")
                           .arg(category));
        }
        return;
    }
    std::unique_ptr<IFrameSource> src;
    if (usingWebcam()) {
        auto cam = std::make_unique<WebcamSource>();
        cam->setDeviceIndex(0);
        cam->setFps(m_liveTargetFps);
        src = std::move(cam);
    } else {
        auto folder = std::make_unique<FolderSource>();
        folder->setFromDataset(m_ctrl.dataset(), category);
        folder->setFps(m_liveTargetFps);
        src = std::move(folder);
    }

    QString sessionId;
    const QString dir = ResultExporter::makeSessionDir(
        m_datasetRoot, category, currentEngineName(), &sessionId);
    auto composite = std::make_unique<CompositeRejectSink>();
    composite->add(std::make_unique<LogRejectSink>());
    composite->add(std::make_unique<FileRejectSink>(dir));
    composite->add(std::make_unique<SimulatedDoSink>(dir));
    m_session.setRejectSink(std::move(composite));
    m_session.setConsecutiveNgLimit(m_consecutiveNgLimit);
    m_session.setSessionMeta(sessionId, dir, currentEngineName(), m_providerText,
                             m_workOrder, m_operatorName);
    m_liveSessionDir = dir;
    m_liveSessionTitle = QStringLiteral("%1 · %2 · %3")
                             .arg(m_workOrder.isEmpty() ? QStringLiteral("未填工单") : m_workOrder,
                                  folderDisplayName(category),
                                  currentKind() == EngineKind::DL
                                      ? QStringLiteral("EfficientAD")
                                      : QStringLiteral("传统 CV"));
    m_hasLiveSession = true;
    emit liveSessionChanged();

    // 开线前释放预览占用的同一设备；失败则把预览拉回来。
    stopPreview();
    if (!m_session.start(std::move(src), category)) {
        emit workEnabledChanged();
        raiseError(usingWebcam()
                       ? QStringLiteral("无法打开本机摄像头（设备 0）。")
                       : QStringLiteral("无法开始模拟取流（该类别没有测试图）"));
        if (usingWebcam())
            startPreview();
        return;
    }
    m_liveLatencyMs = 0;
    m_liveQueueDepth = 0;
    m_liveActualFps = 0.0;
    m_liveClock.restart();
    emit liveStatsChanged();
    setLiveRunning(true);
    setStatusTone(QStringLiteral("ok"));
    if (usingWebcam()) {
        setStatusText(QStringLiteral("本机摄像头取流 %1 @ %2 帧/秒…")
                          .arg(folderDisplayName(category)).arg(m_liveTargetFps));
    } else {
        setStatusText(QStringLiteral("模拟取流 %1 @ %2 帧/秒（海康离线）…")
                          .arg(folderDisplayName(category)).arg(m_liveTargetFps));
    }
    refreshStationStatus();
}

void MainViewModel::onLiveFrame(const LiveInspectedFrame& frame)
{
    m_currentCategory = frame.category;
    m_currentDefectType = frame.defectType;
    m_currentImagePath = frame.path;
    m_currentBgr = frame.bgr;
    m_sourceImage = bgrToQImage(m_currentBgr);

    m_currentGt.release();
    m_gtOverlayImage = {};
    const QString gtPath = m_ctrl.dataset().groundTruthMask(
        m_currentCategory, m_currentDefectType, m_currentImagePath);
    if (!gtPath.isEmpty()) {
        cv::Mat gt = cv::imread(gtPath.toLocal8Bit().constData(), cv::IMREAD_GRAYSCALE);
        if (!gt.empty() && !m_currentBgr.empty() && gt.size() != m_currentBgr.size())
            cv::resize(gt, gt, m_currentBgr.size(), 0, 0, cv::INTER_NEAREST);
        m_currentGt = gt;
        m_gtOverlayImage = maskToOverlayImage(gt, OverlayColors::gtRed, OverlayColors::gtAlpha);
    }

    applyDetectionResult(frame.result);
    emit imageChanged();
    emit selectionChanged();

    m_liveLatencyMs = int(frame.latencyMs);
    m_liveQueueDepth = frame.queueDepth;
    m_liveActualFps = frame.actualFps;
    m_liveDroppedCount = frame.dropped;
    m_liveLateCount = frame.lateCount;
    m_liveMaxQueue = qMax(m_liveMaxQueue, frame.queueDepth);
    m_liveMaxLatencyMs = qMax(m_liveMaxLatencyMs, int(frame.latencyMs));
    m_liveConsecutiveNg = frame.consecutiveNg;
    if (m_liveClock.isValid() && frame.done > 0)
        m_liveTaktMs = int(m_liveClock.elapsed() / frame.done);
    m_livePlannedCount = frame.total > 0 ? frame.total : m_livePlannedCount;
    emit liveStatsChanged();

    const QString verdict = frame.result.detected() ? QStringLiteral("不合格") : QStringLiteral("合格");
    if (frame.result.detected()) {
        ++m_liveNgCount;
        NgListModel::Record rec;
        rec.category = frame.category;
        rec.defectType = frame.defectType;
        rec.path = frame.path;
        rec.fileName = QFileInfo(frame.path).fileName();
        rec.defectLabel = folderDisplayName(frame.defectType);
        rec.imageScore = frame.result.imageScore;
        rec.imageThreshold = frame.result.imageThreshold;
        rec.latencyMs = frame.latencyMs;
        rec.lateEject = frame.lateEject;
        rec.result = frame.result;
        m_ngModel->append(rec);
        appendDoPulse(frame);
    } else {
        ++m_liveOkCount;
    }
    m_liveLastNg = frame.result.detected();
    if (frame.interlockStop) {
        m_liveInterlocked = true;
        m_liveStopReason = usingWebcam()
            ? QStringLiteral("连续不合格联锁（%1 张），已停线。本班走 WebcamSource。")
                  .arg(frame.consecutiveNgLimit > 0 ? frame.consecutiveNgLimit
                                                   : m_consecutiveNgLimit)
            : QStringLiteral("连续不合格联锁（%1 张），已停线。海康仍离线，本班走 FolderSource。")
                  .arg(frame.consecutiveNgLimit > 0 ? frame.consecutiveNgLimit
                                                   : m_consecutiveNgLimit);
        emit liveSessionChanged();
    }
    setStatusTone(m_liveLastNg ? QStringLiteral("ng") : QStringLiteral("ok"));
    setStatusText(QStringLiteral("%1  |  %2/%3/%4（%5/%6）  |  直通 %7%  节拍 %8 ms  连续NG %9")
                      .arg(verdict)
                      .arg(folderDisplayName(frame.category))
                      .arg(folderDisplayName(frame.defectType))
                      .arg(QFileInfo(frame.path).fileName())
                      .arg(frame.done)
                      .arg(frame.total)
                      .arg(liveYieldPercent(), 0, 'f', 1)
                      .arg(m_liveTaktMs)
                      .arg(m_liveConsecutiveNg));
}

void MainViewModel::onLiveFinished(int total, int ngCount)
{
    m_liveStarting = false;
    applyLiveSummary(m_session.lastSummary());
    setLiveRunning(false);
    const LiveSessionSummary s = m_session.lastSummary();
    if (!s.stopReason.isEmpty()) {
        m_liveInterlocked = true;
        m_liveStopReason = s.stopReason + (usingWebcam()
            ? QStringLiteral("。本班走 WebcamSource。")
            : QStringLiteral("。海康仍离线，本班走 FolderSource。"));
        emit liveSessionChanged();
        setStatusTone(QStringLiteral("ng"));
        setStatusText(QStringLiteral("联锁停线：%1  |  合格 %2  不合格 %3  直通 %4%")
                          .arg(s.stopReason)
                          .arg(s.ok)
                          .arg(s.ng)
                          .arg(liveYieldPercent(), 0, 'f', 1));
        showToast(QStringLiteral("联锁停线：%1").arg(s.stopReason));
    } else {
        setStatusTone(ngCount > 0 ? QStringLiteral("ng") : QStringLiteral("ok"));
        setStatusText((usingWebcam()
                           ? QStringLiteral("取流结束：%1 张，合格 %2，不合格 %3，直通 %4%")
                           : QStringLiteral("模拟取流结束：%1 张，合格 %2，不合格 %3，直通 %4%"))
                          .arg(total)
                          .arg(qMax(0, total - ngCount))
                          .arg(ngCount)
                          .arg(liveYieldPercent(), 0, 'f', 1));
        showToast((usingWebcam()
                       ? QStringLiteral("取流结束：不合格 %1 / %2")
                       : QStringLiteral("模拟取流结束：不合格 %1 / %2"))
                      .arg(ngCount).arg(total));
    }
    refreshStationStatus();
}

void MainViewModel::applyLiveSummary(const LiveSessionSummary& s)
{
    if (s.done <= 0 && s.sessionDir.isEmpty())
        return;
    m_liveOkCount = s.ok;
    m_liveNgCount = s.ng;
    m_liveDroppedCount = s.dropped;
    m_liveLateCount = s.lateEject;
    m_liveMaxQueue = s.maxQueue;
    m_liveMaxLatencyMs = int(s.maxLatencyMs);
    m_liveConsecutiveNg = s.consecutiveNg;
    if (s.elapsedSec > 0 && s.done > 0)
        m_liveTaktMs = int((s.elapsedSec * 1000.0) / s.done);
    if (!s.stopReason.isEmpty()) {
        m_liveInterlocked = true;
        m_liveStopReason = s.stopReason + (usingWebcam()
            ? QStringLiteral("。本班走 WebcamSource。")
            : QStringLiteral("。海康仍离线，本班走 FolderSource。"));
    }
    if (!s.sessionDir.isEmpty())
        m_liveSessionDir = s.sessionDir;
    if (!s.category.isEmpty()) {
        m_liveSessionTitle = QStringLiteral("%1 · %2")
                                 .arg(folderDisplayName(s.category),
                                      s.engineName == QStringLiteral("dl")
                                          ? QStringLiteral("EfficientAD")
                                          : QStringLiteral("传统 CV"));
    }
    m_hasLiveSession = true;
    emit liveStatsChanged();
    emit liveSessionChanged();
}

void MainViewModel::onLiveError(const QString& msg)
{
    m_liveStarting = false;
    m_session.stop();
    setLiveRunning(false);
    raiseError(msg);
}

QUrl MainViewModel::suggestedExportFileUrl() const
{
    const QString name = QStringLiteral("%1_%2_%3_%4.png")
                             .arg(m_currentCategory, m_currentDefectType,
                                  QFileInfo(m_currentImagePath).completeBaseName(),
                                  currentEngineName());
    return QUrl::fromLocalFile(QDir::current().filePath(name));
}

QUrl MainViewModel::suggestedExportFolderUrl() const
{
    const QString dir = QDir::current().filePath(
        QStringLiteral("export_%1_%2").arg(m_currentCategory, currentEngineName()));
    QDir().mkpath(dir);
    return QUrl::fromLocalFile(dir);
}

QUrl MainViewModel::suggestedLiveExportFolderUrl() const
{
    if (!m_liveSessionDir.isEmpty())
        return QUrl::fromLocalFile(m_liveSessionDir);
    return QUrl::fromLocalFile(QDir::current().filePath(QStringLiteral("export_live")));
}

void MainViewModel::reviewNg(int row)
{
    if (m_liveRunning || m_liveStarting)
        return;
    const NgListModel::Record* rec = m_ngModel->recordAt(row);
    if (!rec)
        return;
    m_ngModel->setSelectedRow(row);
    m_currentCategory = rec->category;
    m_currentDefectType = rec->defectType;
    m_currentImagePath = rec->path;
    m_currentBgr = cv::imread(rec->path.toLocal8Bit().constData(), cv::IMREAD_COLOR);
    m_sourceImage = bgrToQImage(m_currentBgr);
    m_currentGt.release();
    m_gtOverlayImage = {};
    const QString gtPath = m_ctrl.dataset().groundTruthMask(
        m_currentCategory, m_currentDefectType, m_currentImagePath);
    if (!gtPath.isEmpty()) {
        cv::Mat gt = cv::imread(gtPath.toLocal8Bit().constData(), cv::IMREAD_GRAYSCALE);
        if (!gt.empty() && !m_currentBgr.empty() && gt.size() != m_currentBgr.size())
            cv::resize(gt, gt, m_currentBgr.size(), 0, 0, cv::INTER_NEAREST);
        m_currentGt = gt;
        m_gtOverlayImage = maskToOverlayImage(gt, OverlayColors::gtRed, OverlayColors::gtAlpha);
    }
    applyDetectionResult(rec->result);
    emit imageChanged();
    emit selectionChanged();
    setStatusTone(QStringLiteral("ng"));
    setStatusText(QStringLiteral("回看不合格 %1 / %2")
                      .arg(folderDisplayName(rec->defectType), rec->fileName));
}

bool MainViewModel::exportLiveSession(const QUrl& folder)
{
    if (m_liveSessionDir.isEmpty()) {
        raiseError(QStringLiteral("还没有班次记录。先跑一次取流。"));
        return false;
    }
    const QString dest = folder.toLocalFile();
    if (dest.isEmpty())
        return false;
    if (!QDir().mkpath(dest)) {
        raiseError(QStringLiteral("无法创建目录：%1").arg(dest));
        return false;
    }
    const QDir src(m_liveSessionDir);
    const QFileInfoList files = src.entryInfoList(QDir::Files);
    if (files.isEmpty()) {
        raiseError(QStringLiteral("班次目录是空的：%1").arg(m_liveSessionDir));
        return false;
    }
    int copied = 0;
    for (const QFileInfo& fi : files) {
        const QString to = QDir(dest).filePath(fi.fileName());
        QFile::remove(to);
        if (QFile::copy(fi.absoluteFilePath(), to))
            ++copied;
    }
    if (copied <= 0) {
        raiseError(QStringLiteral("复制班次文件失败：%1").arg(dest));
        return false;
    }
    setStatusText(QStringLiteral("已导出班次到 %1").arg(dest));
    showToast(QStringLiteral("已导出班次 CSV 与不合格叠图"));
    return true;
}

bool MainViewModel::exportCurrent(const QUrl& url)
{
    if (m_currentBgr.empty()) {
        raiseError(QStringLiteral("没有可导出的图，请先从左侧选一张测试图"));
        return false;
    }
    const QString path = url.toLocalFile();
    if (path.isEmpty())
        return false;
    const cv::Mat annotated = ResultExporter::composeAnnotated(
        m_currentBgr, m_currentResult.defectMask, m_currentResult.boxes, m_currentGt);
    if (!ResultExporter::saveImage(path, annotated)) {
        raiseError(QStringLiteral("写出失败：%1").arg(path));
        return false;
    }
    setStatusText(QStringLiteral("已导出 %1").arg(path));
    showToast(QStringLiteral("已导出当前图"));
    return true;
}

bool MainViewModel::exportBatch(const QUrl& folder)
{
    const BatchMetrics* metrics = m_ctrl.lastBatch(currentKind(), m_currentCategory);
    if (!metrics) {
        raiseError(QStringLiteral("请先对当前引擎跑完该类别的批量检测"));
        return false;
    }
    const QString dir = folder.toLocalFile();
    if (dir.isEmpty())
        return false;
    if (!ResultExporter::exportBatch(dir, m_currentCategory, currentEngineName(),
                                     m_ctrl.dataset(), *metrics)) {
        raiseError(QStringLiteral("导出失败：%1").arg(dir));
        return false;
    }
    setStatusText(QStringLiteral("已导出批量结果到 %1").arg(dir));
    showToast(QStringLiteral("已导出批量 PNG + CSV"));
    return true;
}

void MainViewModel::refreshBatchDependent()
{
    refreshMetrics();
    refreshCompare();
    emit workEnabledChanged();
}

void MainViewModel::refreshMetrics()
{
    const BatchMetrics* metrics = m_ctrl.lastBatch(currentKind(), m_currentCategory);
    if (!metrics) {
        m_metricsModel->clear();
        if (m_hasMetrics) {
            m_hasMetrics = false;
            emit hasMetricsChanged();
        }
        return;
    }
    m_metricsModel->setMetrics(metrics->pixel, metrics->image);
    if (!m_hasMetrics) {
        m_hasMetrics = true;
        emit hasMetricsChanged();
    } else {
        emit hasMetricsChanged();
    }
}

void MainViewModel::refreshCompare()
{
    const BatchMetrics* cv = m_ctrl.lastBatch(EngineKind::Traditional, m_currentCategory);
    const BatchMetrics* dl = m_ctrl.lastBatch(EngineKind::DL, m_currentCategory);
    if (!cv || !dl) {
        m_compareModel->clear();
        m_hasCompare = false;
        m_compareCvF1.clear();
        m_compareDlF1.clear();
        m_compareDeltaF1.clear();
        m_compareCvImg.clear();
        m_compareDlImg.clear();
        m_compareCvFpr.clear();
        m_compareDlFpr.clear();
        emit hasCompareChanged();
        return;
    }
    m_compareModel->setCompare(cv->pixel, cv->image, dl->pixel, dl->image);

    PixelMetrics cvTotal, dlTotal;
    ImageMetrics cvImg, dlImg;
    for (auto it = cv->pixel.constBegin(); it != cv->pixel.constEnd(); ++it) {
        cvTotal += it.value();
        cvImg += cv->image.value(it.key());
    }
    for (auto it = dl->pixel.constBegin(); it != dl->pixel.constEnd(); ++it) {
        dlTotal += it.value();
        dlImg += dl->image.value(it.key());
    }
    m_compareCvF1 = fmt3(cvTotal.f1());
    m_compareDlF1 = fmt3(dlTotal.f1());
    m_compareDeltaF1 = fmt3(dlTotal.f1() - cvTotal.f1());
    m_compareCvImg = imageAccText(cvImg);
    m_compareDlImg = imageAccText(dlImg);

    const ImageMetrics cvGood = cv->image.value(QStringLiteral("good"));
    const ImageMetrics dlGood = dl->image.value(QStringLiteral("good"));
    m_compareCvFpr = (cvGood.total > 0)
        ? QStringLiteral("%1 (%2/%3)")
              .arg(fmt3(1.0 - cvGood.accuracy()))
              .arg(cvGood.total - cvGood.correct)
              .arg(cvGood.total)
        : QStringLiteral("—");
    m_compareDlFpr = (dlGood.total > 0)
        ? QStringLiteral("%1 (%2/%3)")
              .arg(fmt3(1.0 - dlGood.accuracy()))
              .arg(dlGood.total - dlGood.correct)
              .arg(dlGood.total)
        : QStringLiteral("—");
    m_hasCompare = true;
    emit hasCompareChanged();
}

void MainViewModel::onProgress(int current, int total, const QString& text)
{
    m_progressCurrent = current;
    m_progressTotal = total;
    m_progressText = text;
    // 蒙层副标题：夜班要能一眼看出「标定 ≠ 训练」
    if (text.contains(QStringLiteral("标定"))) {
        m_busyKind = QStringLiteral("calib");
        m_busySubtitle = QStringLiteral("正在扫描 train/good 标定阈值，这不是训练。完成后写入 .calib.json。");
    } else if (text.contains(QStringLiteral("加载 ONNX")) || text.contains(QStringLiteral("已加载"))) {
        m_busyKind = QStringLiteral("load");
        m_busySubtitle = QStringLiteral("加载本地权重，不会联网、不会训练。");
    } else if (text.contains(QStringLiteral("取流"))) {
        m_busyKind = QStringLiteral("livePrep");
        m_busySubtitle = QStringLiteral("取流前准备引擎。若无标定缓存，会先扫描良品图。");
    } else if (text.contains(QStringLiteral("对比"))) {
        m_busyKind = QStringLiteral("compare");
        m_busySubtitle = QStringLiteral("缺哪侧批量就补跑哪侧，口径与 --batch 相同。");
    } else if (text.contains(QStringLiteral("检测")) && total > 0) {
        m_busyKind = QStringLiteral("batch");
        m_busySubtitle.clear();
    } else if (text.contains(QStringLiteral("推理")) || text.contains(QStringLiteral("准备检测"))) {
        m_busyKind = QStringLiteral("detect");
        m_busySubtitle.clear();
    } else if (text.contains(QStringLiteral("构建传统"))) {
        m_busyKind = QStringLiteral("calib");
        m_busySubtitle = QStringLiteral("用 train/good 建均值/标准差参考，不是训练网络。");
    } else {
        m_busyKind = m_busy ? QStringLiteral("busy") : QString();
        m_busySubtitle.clear();
    }
    emit progressChanged();
    if (m_busy)
        setStatusTone(QStringLiteral("busy"));
    setStatusText(text);
}

void MainViewModel::onBusyChanged(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
    emit workEnabledChanged();
    if (!busy) {
        m_busyKind.clear();
        m_busySubtitle.clear();
        emit progressChanged();
        refreshStationStatus();
        if (!m_liveRunning)
            setStatusTone(QStringLiteral("normal"));
    }
}

void MainViewModel::onDetectFinished(bool ok, const DetectionResult& result)
{
    if (m_previewRunning || m_liveRunning || m_liveStarting)
        return;
    if (!ok) {
        clearDetection();
        refreshStationStatus();
        const QString reason = (currentKind() == EngineKind::DL)
            ? missingDlReason(m_currentCategory)
            : QStringLiteral("无法加载 %1 的传统引擎（train/good 为空或不可读）")
                  .arg(m_currentCategory);
        setStatusTone(QStringLiteral("warn"));
        setStatusText(reason);
        if (!m_missingModelDialogShown) {
            m_missingModelDialogShown = true;
            raiseError(reason);
        }
        return;
    }
    m_missingModelDialogShown = false;
    applyDetectionResult(result);
    refreshStationStatus();
    setStatusTone(result.detected() ? QStringLiteral("ng") : QStringLiteral("ok"));
    const QString verdict = result.detected() ? QStringLiteral("不合格") : QStringLiteral("合格");
    setStatusText(QStringLiteral("%1  %2  分 %3 / 阈 %4")
                      .arg(verdict, imageInfo())
                      .arg(result.imageScore, 0, 'f', 4)
                      .arg(qIsFinite(result.imageThreshold) ? result.imageThreshold : 0.0, 0, 'f', 4));
}

void MainViewModel::onBatchFinished(bool ok, const QString& category)
{
    if (!ok) {
        if (currentKind() == EngineKind::DL) {
            const QString expected = m_ctrl.onnxModelCandidates(category).value(0);
            raiseError(QStringLiteral("无法完成 %1 DL 批量（ONNX 缺失或 train/good 不足 3 张；约定 %2）")
                           .arg(category, expected));
        } else {
            raiseError(QStringLiteral("无法完成 %1 批量检测（train/good 为空或不可读）")
                           .arg(category));
        }
        return;
    }
    refreshBatchDependent();
    setInspectorTab(2);
    setStatusTone(QStringLiteral("ok"));
    setStatusText(QStringLiteral("已完成 %1 批量检测（%2）")
                      .arg(folderDisplayName(category), currentEngineName()));
    showToast(QStringLiteral("批量完成，见右侧「指标」"));
    refreshStationStatus();
}

void MainViewModel::onCompareFinished(bool ok, const QString& category)
{
    if (!ok) {
        raiseError(QStringLiteral("双引擎对比失败（某一侧模型或良品图不可用）"));
        return;
    }
    refreshBatchDependent();
    setInspectorTab(3);
    setStatusTone(QStringLiteral("ok"));
    setStatusText(QStringLiteral("已完成 %1 双引擎对比").arg(folderDisplayName(category)));
    showToast(QStringLiteral("对比完成，见右侧「对比」"));
    refreshStationStatus();
}
