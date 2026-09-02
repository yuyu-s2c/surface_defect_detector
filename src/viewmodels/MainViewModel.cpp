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

bool MainViewModel::canRunBatch() const
{
    return !m_busy && !m_liveRunning && !m_liveStarting && !m_currentCategory.isEmpty();
}

bool MainViewModel::canStartLive() const
{
    return !m_busy && !m_liveRunning && !m_liveStarting && !m_currentCategory.isEmpty();
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
    setStatusText(QStringLiteral("数据集：%1").arg(m_datasetRoot));
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
    if (nodeType == QLatin1String("image")) {
        m_currentDefectType = defectType;
        m_currentImagePath = imagePath;
        if (categoryChanged)
            syncParamsFromSettings();
        loadCurrentImage();
        emit selectionChanged();
        emit workEnabledChanged();
        return;
    }
    if (nodeType == QLatin1String("defect"))
        m_currentDefectType = defectType;
    if (categoryChanged)
        syncParamsFromSettings();
    refreshBatchDependent();
    emit selectionChanged();
    emit workEnabledChanged();
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
    emit engineKindChanged();
    syncParamsFromSettings();
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
    if (qFuzzyCompare(m_cv.zAggThreshold, v))
        return;
    m_cv.zAggThreshold = v;
    emit cvParamsChanged();
    if (!m_syncingParams)
        updateParamsDirty();
}

void MainViewModel::setCvMorphCloseKernel(int v)
{
    if (m_cv.morphCloseKernel == v)
        return;
    m_cv.morphCloseKernel = v;
    emit cvParamsChanged();
    if (!m_syncingParams)
        updateParamsDirty();
}

void MainViewModel::setCvMinDefectArea(int v)
{
    if (m_cv.minDefectArea == v)
        return;
    m_cv.minDefectArea = v;
    emit cvParamsChanged();
    if (!m_syncingParams)
        updateParamsDirty();
}

void MainViewModel::setCvImageLevelMinArea(int v)
{
    if (m_cv.imageLevelMinArea == v)
        return;
    m_cv.imageLevelMinArea = v;
    emit cvParamsChanged();
    if (!m_syncingParams)
        updateParamsDirty();
}

void MainViewModel::setDlThresholdSigma(double v)
{
    if (qFuzzyCompare(m_dl.thresholdSigma, v))
        return;
    m_dl.thresholdSigma = v;
    emit dlParamsChanged();
    if (!m_syncingParams)
        updateParamsDirty();
}

void MainViewModel::setDlMorphCloseKernel(int v)
{
    if (m_dl.morphCloseKernel == v)
        return;
    m_dl.morphCloseKernel = v;
    emit dlParamsChanged();
    if (!m_syncingParams)
        updateParamsDirty();
}

void MainViewModel::setDlMinDefectArea(int v)
{
    if (m_dl.minDefectArea == v)
        return;
    m_dl.minDefectArea = v;
    emit dlParamsChanged();
    if (!m_syncingParams)
        updateParamsDirty();
}

void MainViewModel::setDlImageLevelMinArea(int v)
{
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
    return p;
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
    return p;
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
        m_ctrl.setDLParams(m_currentCategory, m_dl);
        m_appliedDl = m_dl;
    } else {
        m_ctrl.setTraditionalParams(m_currentCategory, m_cv);
        m_appliedCv = m_cv;
    }
    saveParamsToSettings();
    updateParamsDirty();
    runDetectionForCurrent();
    setStatusText(QStringLiteral("已应用 %1 / %2 参数")
                      .arg(m_currentCategory, currentEngineName()));
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
    setStatusText(QStringLiteral("已恢复 %1 / %2 的 P2 默认工作点")
                      .arg(m_currentCategory, currentEngineName()));
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
    if (was)
        setStatusText(QStringLiteral("已停止取流"));
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
    setStatusText(QStringLiteral("取流中 %1 @ %2 fps…").arg(category).arg(m_liveTargetFps));
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
    setStatusText(QStringLiteral("取流 %1 fps | 延迟 %2 ms | 队列 %3/%4 | %5 | %6/%7/%8（%9/%10）")
                      .arg(frame.actualFps, 0, 'f', 1)
                      .arg(frame.latencyMs)
                      .arg(frame.queueDepth)
                      .arg(frame.queueMax)
                      .arg(verdict)
                      .arg(frame.category, frame.defectType, QFileInfo(frame.path).fileName())
                      .arg(frame.done)
                      .arg(frame.total));
}

void MainViewModel::onLiveFinished(int total, int ngCount)
{
    m_liveStarting = false;
    setLiveRunning(false);
    setStatusText(QStringLiteral("取流结束：%1 张，NG %2").arg(total).arg(ngCount));
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
    if (m_currentBgr.empty())
        return false;
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
    emit progressChanged();
    setStatusText(text);
}

void MainViewModel::onBusyChanged(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
    emit workEnabledChanged();
}

void MainViewModel::onDetectFinished(bool ok, const DetectionResult& result)
{
    if (!ok) {
        clearDetection();
        if (currentKind() == EngineKind::DL) {
            const QString expected = m_ctrl.onnxModelCandidates(m_currentCategory).value(0);
            setStatusText(QStringLiteral("无法加载 %1 的 DL 引擎（ONNX 缺失或 train/good 不足 3 张；约定 %2）")
                              .arg(m_currentCategory, expected));
        } else {
            setStatusText(QStringLiteral("无法加载 %1 的传统引擎（train/good 为空或不可读）")
                              .arg(m_currentCategory));
        }
        return;
    }
    applyDetectionResult(result);
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
    setStatusText(QStringLiteral("已完成 %1 批量检测（%2）")
                      .arg(category, currentEngineName()));
}

void MainViewModel::onCompareFinished(bool ok, const QString& category)
{
    if (!ok) {
        raiseError(QStringLiteral("双引擎对比失败（某一侧模型或良品图不可用）"));
        return;
    }
    refreshBatchDependent();
    setInspectorTab(3);
    setStatusText(QStringLiteral("已完成 %1 双引擎对比").arg(category));
}
