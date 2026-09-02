#include "MainViewModel.h"

#include "Format.h"
#include "ImageConvert.h"
#include "OverlayColors.h"
#include "ResultExporter.h"
#include "sources/FolderSource.h"

#include <QDir>
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
{
    m_appliedCv = m_cv;
    m_appliedDl = m_dl;

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
}

MainViewModel::~MainViewModel()
{
    m_liveStarting = false;
    m_session.stop();
}

QString MainViewModel::imageInfo() const
{
    if (m_currentImagePath.isEmpty())
        return QStringLiteral("未选择图片");
    return QStringLiteral("%1 / %2 / %3")
        .arg(m_currentCategory, m_currentDefectType, QFileInfo(m_currentImagePath).fileName());
}

QString MainViewModel::scoreRuleText() const
{
    return QStringLiteral("OK / NG = 图像分 ≥ 判定阈值（分数过线）。绿叠加面积只影响框，不驱动剔除。");
}

QString MainViewModel::aboutBody() const
{
    return QStringLiteral(
        "离线质检工作站：文件夹图源、模拟取流、批量评估、PNG/CSV 导出、传统 CV 与 EfficientAD 双引擎。\n\n"
        "深度学习用本地 ONNX（默认 DirectML，失败回 CPU），不会联网、不会训练。"
        "权重约定：models/<类别>/weights/onnx/<类别>.onnx。"
        "首次对该执行器标定会扫描 train/good 并写入同目录 .calib.json（v3，按 EP 分键），之后复用缓存。\n\n"
        "图像级判定是分数过线，不是掩码面积。面积门只影响绿叠加和 --batch 对照列。\n\n"
        "本机未接相机 / PLC（Phase 4）。取流是 FolderSource 按 FPS 吐当前类 test/。");
}

QString MainViewModel::shortcutsHelp() const
{
    return QStringLiteral(
        "空格 开始/停止取流    Esc 停止取流    B 批量    Shift+C 对比引擎\n"
        "1 传统 CV    2 EfficientAD    G GT 叠加    D 检测叠加    F 适应画面\n"
        "Ctrl+O 打开数据集    Ctrl+E 导出当前图    Ctrl+Shift+E 导出批量    F1 关于");
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
    return true;
}

bool MainViewModel::canExportBatch() const
{
    return m_ctrl.lastBatch(currentKind(), m_currentCategory) != nullptr;
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
    m_currentBgr.release();
    m_currentGt.release();
    m_sourceImage = {};
    m_gtOverlayImage = {};
    clearDetection();
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
    if (m_liveRunning || m_liveStarting)
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
    tab = qBound(0, tab, 3);
    if (m_inspectorTab == tab)
        return;
    m_inspectorTab = tab;
    emit inspectorTabChanged();
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
    m_ctrl.runBatchAsync(m_currentCategory);
}

void MainViewModel::compareEngines()
{
    if (m_currentCategory.isEmpty() || m_ctrl.isBusy() || m_liveRunning || m_liveStarting)
        return;
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
    m_liveRunning = running;
    emit liveRunningChanged();
    emit workEnabledChanged();
}

void MainViewModel::startLive()
{
    if (!canStartLive()) {
        if (m_currentCategory.isEmpty())
            raiseError(QStringLiteral("请先选择一个类别"));
        return;
    }
    if (currentKind() == EngineKind::DL && dlEngineBlocked(m_currentCategory)) {
        raiseError(missingDlReason(m_currentCategory));
        return;
    }
    m_liveOkCount = 0;
    m_liveNgCount = 0;
    m_liveLastNg = false;
    emit liveStatsChanged();
    m_liveStarting = true;
    emit workEnabledChanged();
    m_ctrl.prepareEngineAsync(m_currentCategory);
}

void MainViewModel::stopLive()
{
    const bool was = m_liveRunning || m_liveStarting || m_session.isRunning();
    m_liveStarting = false;
    m_session.stop();
    setLiveRunning(false);
    emit workEnabledChanged();
    if (was) {
        setStatusTone(QStringLiteral("normal"));
        setStatusText(QStringLiteral("已停止取流"));
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
    auto src = std::make_unique<FolderSource>();
    src->setFromDataset(m_ctrl.dataset(), category);
    src->setFps(m_liveTargetFps);
    if (!m_session.start(std::move(src), category)) {
        emit workEnabledChanged();
        raiseError(QStringLiteral("无法开始取流（该类别没有测试图）"));
        return;
    }
    m_liveLatencyMs = 0;
    m_liveQueueDepth = 0;
    m_liveActualFps = 0.0;
    emit liveStatsChanged();
    setLiveRunning(true);
    setStatusTone(QStringLiteral("ok"));
    setStatusText(QStringLiteral("取流中 %1 @ %2 fps…").arg(category).arg(m_liveTargetFps));
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
    emit liveStatsChanged();

    const QString verdict = frame.result.detected() ? QStringLiteral("NG") : QStringLiteral("OK");
    if (frame.result.detected())
        ++m_liveNgCount;
    else
        ++m_liveOkCount;
    m_liveLastNg = frame.result.detected();
    setStatusTone(m_liveLastNg ? QStringLiteral("ng") : QStringLiteral("ok"));
    setStatusText(QStringLiteral("取流 %1 fps | 延迟 %2 ms | 队列 %3/%4 | OK %5 · NG %6 | %7 | %8/%9/%10（%11/%12）")
                      .arg(frame.actualFps, 0, 'f', 1)
                      .arg(frame.latencyMs)
                      .arg(frame.queueDepth)
                      .arg(frame.queueMax)
                      .arg(m_liveOkCount)
                      .arg(m_liveNgCount)
                      .arg(verdict)
                      .arg(frame.category, frame.defectType, QFileInfo(frame.path).fileName())
                      .arg(frame.done)
                      .arg(frame.total));
}

void MainViewModel::onLiveFinished(int total, int ngCount)
{
    m_liveStarting = false;
    setLiveRunning(false);
    setStatusTone(ngCount > 0 ? QStringLiteral("ng") : QStringLiteral("ok"));
    setStatusText(QStringLiteral("取流结束：%1 张，OK %2，NG %3")
                      .arg(total)
                      .arg(qMax(0, total - ngCount))
                      .arg(ngCount));
    showToast(QStringLiteral("取流结束：NG %1 / %2").arg(ngCount).arg(total));
    refreshStationStatus();
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
    const QString verdict = result.detected() ? QStringLiteral("NG") : QStringLiteral("OK");
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
                      .arg(category, currentEngineName()));
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
    setStatusText(QStringLiteral("已完成 %1 双引擎对比").arg(category));
    showToast(QStringLiteral("对比完成，见右侧「对比」"));
    refreshStationStatus();
}
