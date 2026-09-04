#pragma once

#include "BoxListModel.h"
#include "CompareListModel.h"
#include "DatasetTreeModel.h"
#include "DetectionController.h"
#include "DoPulseModel.h"
#include "EngineParams.h"
#include "IDetectionEngine.h"
#include "InspectionSession.h"
#include "MetricsListModel.h"
#include "NgListModel.h"
#include "sources/WebcamPreview.h"

#include <QElapsedTimer>
#include <QImage>
#include <QModelIndex>
#include <QObject>
#include <QUrl>
#include <QVariantList>

#include <opencv2/core.hpp>

// GUI 状态根：QML 只绑这里。DetectionController 仍是 Model，不进 QML。
class MainViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool hasDataset READ hasDataset NOTIFY hasDatasetChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(int progressCurrent READ progressCurrent NOTIFY progressChanged)
    Q_PROPERTY(int progressTotal READ progressTotal NOTIFY progressChanged)
    Q_PROPERTY(QString progressText READ progressText NOTIFY progressChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString datasetRoot READ datasetRoot NOTIFY hasDatasetChanged)
    Q_PROPERTY(QString currentCategory READ currentCategory NOTIFY selectionChanged)
    Q_PROPERTY(QString currentCategoryLabel READ currentCategoryLabel NOTIFY selectionChanged)
    Q_PROPERTY(QString currentDefectType READ currentDefectType NOTIFY selectionChanged)
    Q_PROPERTY(QString currentImagePath READ currentImagePath NOTIFY selectionChanged)
    Q_PROPERTY(QString imageInfo READ imageInfo NOTIFY selectionChanged)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString scoreRuleText READ scoreRuleText CONSTANT)
    Q_PROPERTY(QString aboutBody READ aboutBody CONSTANT)
    Q_PROPERTY(QString shortcutsHelp READ shortcutsHelp CONSTANT)
    Q_PROPERTY(QString engineStatusText READ engineStatusText NOTIFY stationStatusChanged)
    Q_PROPERTY(QString providerText READ providerText NOTIFY stationStatusChanged)
    Q_PROPERTY(QString calibStatusText READ calibStatusText NOTIFY stationStatusChanged)
    Q_PROPERTY(QString expectedOnnxPath READ expectedOnnxPath NOTIFY stationStatusChanged)
    Q_PROPERTY(bool modelAvailable READ modelAvailable NOTIFY stationStatusChanged)
    Q_PROPERTY(bool calibCached READ calibCached NOTIFY stationStatusChanged)
    Q_PROPERTY(QString stationAlert READ stationAlert NOTIFY stationStatusChanged)
    Q_PROPERTY(bool stationAlertIsError READ stationAlertIsError NOTIFY stationStatusChanged)
    Q_PROPERTY(QString toastMessage READ toastMessage NOTIFY toastMessageChanged)
    Q_PROPERTY(QString busyKind READ busyKind NOTIFY progressChanged)
    Q_PROPERTY(QString busySubtitle READ busySubtitle NOTIFY progressChanged)
    Q_PROPERTY(int liveOkCount READ liveOkCount NOTIFY liveStatsChanged)
    Q_PROPERTY(int liveNgCount READ liveNgCount NOTIFY liveStatsChanged)
    Q_PROPERTY(bool liveLastNg READ liveLastNg NOTIFY liveStatsChanged)
    Q_PROPERTY(QString statusTone READ statusTone NOTIFY statusTextChanged)
    Q_PROPERTY(int engineKind READ engineKind WRITE setEngineKind NOTIFY engineKindChanged)
    Q_PROPERTY(bool gtOverlayVisible READ gtOverlayVisible WRITE setGtOverlayVisible NOTIFY overlayChanged)
    Q_PROPERTY(bool detOverlayVisible READ detOverlayVisible WRITE setDetOverlayVisible NOTIFY overlayChanged)
    Q_PROPERTY(bool hasImage READ hasImage NOTIFY imageChanged)
    Q_PROPERTY(bool canRunBatch READ canRunBatch NOTIFY workEnabledChanged)
    Q_PROPERTY(bool canStartLive READ canStartLive NOTIFY workEnabledChanged)
    Q_PROPERTY(int liveSourceKind READ liveSourceKind WRITE setLiveSourceKind NOTIFY liveSourceKindChanged)
    Q_PROPERTY(QString liveStartButtonText READ liveStartButtonText NOTIFY liveSourceKindChanged)
    Q_PROPERTY(QString selfCheckIntroText READ selfCheckIntroText NOTIFY liveSourceKindChanged)
    Q_PROPERTY(bool liveRunning READ liveRunning NOTIFY liveRunningChanged)
    Q_PROPERTY(bool previewRunning READ previewRunning NOTIFY previewRunningChanged)
    Q_PROPERTY(int liveTargetFps READ liveTargetFps WRITE setLiveTargetFps NOTIFY liveTargetFpsChanged)
    Q_PROPERTY(int liveLatencyMs READ liveLatencyMs NOTIFY liveStatsChanged)
    Q_PROPERTY(int liveQueueDepth READ liveQueueDepth NOTIFY liveStatsChanged)
    Q_PROPERTY(int liveQueueMax READ liveQueueMax CONSTANT)
    Q_PROPERTY(double liveActualFps READ liveActualFps NOTIFY liveStatsChanged)
    Q_PROPERTY(bool canExportBatch READ canExportBatch NOTIFY workEnabledChanged)
    Q_PROPERTY(bool paramsDirty READ paramsDirty NOTIFY paramsDirtyChanged)
    Q_PROPERTY(bool detected READ detected NOTIFY detectionChanged)
    Q_PROPERTY(bool verdictOk READ verdictOk NOTIFY detectionChanged)
    Q_PROPERTY(int defectCount READ defectCount NOTIFY detectionChanged)
    Q_PROPERTY(double totalArea READ totalArea NOTIFY detectionChanged)
    Q_PROPERTY(int minImageArea READ minImageArea NOTIFY detectionChanged)
    Q_PROPERTY(double imageScore READ imageScore NOTIFY detectionChanged)
    Q_PROPERTY(double imageThreshold READ imageThreshold NOTIFY detectionChanged)
    Q_PROPERTY(QImage sourceImage READ sourceImage NOTIFY imageChanged)
    Q_PROPERTY(QImage gtOverlayImage READ gtOverlayImage NOTIFY imageChanged)
    Q_PROPERTY(QImage detOverlayImage READ detOverlayImage NOTIFY detectionChanged)
    Q_PROPERTY(QVariantList boxRects READ boxRects NOTIFY detectionChanged)
    Q_PROPERTY(int inspectorTab READ inspectorTab WRITE setInspectorTab NOTIFY inspectorTabChanged)
    Q_PROPERTY(int workMode READ workMode WRITE setWorkMode NOTIFY workModeChanged)
    Q_PROPERTY(bool hasLiveSession READ hasLiveSession NOTIFY liveSessionChanged)
    Q_PROPERTY(bool canExportLive READ canExportLive NOTIFY liveSessionChanged)
    Q_PROPERTY(int liveDroppedCount READ liveDroppedCount NOTIFY liveStatsChanged)
    Q_PROPERTY(int liveLateCount READ liveLateCount NOTIFY liveStatsChanged)
    Q_PROPERTY(int liveMaxQueue READ liveMaxQueue NOTIFY liveStatsChanged)
    Q_PROPERTY(int liveMaxLatencyMs READ liveMaxLatencyMs NOTIFY liveStatsChanged)
    Q_PROPERTY(QString liveSessionTitle READ liveSessionTitle NOTIFY liveSessionChanged)
    Q_PROPERTY(QString liveSessionDir READ liveSessionDir NOTIFY liveSessionChanged)
    Q_PROPERTY(bool hasMetrics READ hasMetrics NOTIFY hasMetricsChanged)
    Q_PROPERTY(bool hasCompare READ hasCompare NOTIFY hasCompareChanged)
    Q_PROPERTY(QString compareCvF1 READ compareCvF1 NOTIFY hasCompareChanged)
    Q_PROPERTY(QString compareDlF1 READ compareDlF1 NOTIFY hasCompareChanged)
    Q_PROPERTY(QString compareDeltaF1 READ compareDeltaF1 NOTIFY hasCompareChanged)
    Q_PROPERTY(QString compareCvImg READ compareCvImg NOTIFY hasCompareChanged)
    Q_PROPERTY(QString compareDlImg READ compareDlImg NOTIFY hasCompareChanged)
    Q_PROPERTY(QString compareCvFpr READ compareCvFpr NOTIFY hasCompareChanged)
    Q_PROPERTY(QString compareDlFpr READ compareDlFpr NOTIFY hasCompareChanged)

    Q_PROPERTY(double cvZAggThreshold READ cvZAggThreshold WRITE setCvZAggThreshold NOTIFY cvParamsChanged)
    Q_PROPERTY(int cvMorphCloseKernel READ cvMorphCloseKernel WRITE setCvMorphCloseKernel NOTIFY cvParamsChanged)
    Q_PROPERTY(int cvMinDefectArea READ cvMinDefectArea WRITE setCvMinDefectArea NOTIFY cvParamsChanged)
    Q_PROPERTY(int cvImageLevelMinArea READ cvImageLevelMinArea WRITE setCvImageLevelMinArea NOTIFY cvParamsChanged)
    Q_PROPERTY(double dlThresholdSigma READ dlThresholdSigma WRITE setDlThresholdSigma NOTIFY dlParamsChanged)
    Q_PROPERTY(int dlMorphCloseKernel READ dlMorphCloseKernel WRITE setDlMorphCloseKernel NOTIFY dlParamsChanged)
    Q_PROPERTY(int dlMinDefectArea READ dlMinDefectArea WRITE setDlMinDefectArea NOTIFY dlParamsChanged)
    Q_PROPERTY(int dlImageLevelMinArea READ dlImageLevelMinArea WRITE setDlImageLevelMinArea NOTIFY dlParamsChanged)

    Q_PROPERTY(DatasetTreeModel* datasetModel READ datasetModel CONSTANT)
    Q_PROPERTY(BoxListModel* boxModel READ boxModel CONSTANT)
    Q_PROPERTY(MetricsListModel* metricsModel READ metricsModel CONSTANT)
    Q_PROPERTY(CompareListModel* compareModel READ compareModel CONSTANT)
    Q_PROPERTY(NgListModel* ngModel READ ngModel CONSTANT)
    Q_PROPERTY(DoPulseModel* doPulseModel READ doPulseModel CONSTANT)

    Q_PROPERTY(QString cameraStatusText READ cameraStatusText NOTIFY liveSourceKindChanged)
    Q_PROPERTY(QString sourceStatusText READ sourceStatusText NOTIFY liveSourceKindChanged)
    Q_PROPERTY(QString plcStatusText READ plcStatusText CONSTANT)
    Q_PROPERTY(QString workOrder READ workOrder WRITE setWorkOrder NOTIFY recipeChanged)
    Q_PROPERTY(QString operatorName READ operatorName WRITE setOperatorName NOTIFY recipeChanged)
    Q_PROPERTY(int consecutiveNgLimit READ consecutiveNgLimit WRITE setConsecutiveNgLimit NOTIFY recipeChanged)
    Q_PROPERTY(QVariantList selfCheckItems READ selfCheckItems NOTIFY selfCheckChanged)
    Q_PROPERTY(bool selfCheckPassed READ selfCheckPassed NOTIFY selfCheckChanged)
    Q_PROPERTY(QString selfCheckHint READ selfCheckHint NOTIFY selfCheckChanged)
    Q_PROPERTY(double liveYieldPercent READ liveYieldPercent NOTIFY liveStatsChanged)
    Q_PROPERTY(int liveTaktMs READ liveTaktMs NOTIFY liveStatsChanged)
    Q_PROPERTY(int liveConsecutiveNg READ liveConsecutiveNg NOTIFY liveStatsChanged)
    Q_PROPERTY(QString liveStopReason READ liveStopReason NOTIFY liveSessionChanged)
    Q_PROPERTY(bool liveInterlocked READ liveInterlocked NOTIFY liveSessionChanged)
    Q_PROPERTY(int livePlannedCount READ livePlannedCount NOTIFY liveStatsChanged)

public:
    explicit MainViewModel(QObject* parent = nullptr);
    ~MainViewModel() override;

    bool hasDataset() const { return m_hasDataset; }
    bool busy() const { return m_busy; }
    int progressCurrent() const { return m_progressCurrent; }
    int progressTotal() const { return m_progressTotal; }
    QString progressText() const { return m_progressText; }
    QString statusText() const { return m_statusText; }
    QString errorMessage() const { return m_errorMessage; }
    QString datasetRoot() const { return m_datasetRoot; }
    QString currentCategory() const { return m_currentCategory; }
    QString currentCategoryLabel() const;
    QString currentDefectType() const { return m_currentDefectType; }
    QString currentImagePath() const { return m_currentImagePath; }
    QString imageInfo() const;
    QString appVersion() const { return QStringLiteral("0.1"); }
    QString scoreRuleText() const;
    QString aboutBody() const;
    QString shortcutsHelp() const;
    QString engineStatusText() const { return m_engineStatusText; }
    QString providerText() const { return m_providerText; }
    QString calibStatusText() const { return m_calibStatusText; }
    QString expectedOnnxPath() const { return m_expectedOnnxPath; }
    bool modelAvailable() const { return m_modelAvailable; }
    bool calibCached() const { return m_calibCached; }
    QString stationAlert() const { return m_stationAlert; }
    bool stationAlertIsError() const { return m_stationAlertIsError; }
    QString toastMessage() const { return m_toastMessage; }
    QString busyKind() const { return m_busyKind; }
    QString busySubtitle() const { return m_busySubtitle; }
    int liveOkCount() const { return m_liveOkCount; }
    int liveNgCount() const { return m_liveNgCount; }
    bool liveLastNg() const { return m_liveLastNg; }
    QString statusTone() const { return m_statusTone; }
    int engineKind() const { return m_engineKind; }
    bool gtOverlayVisible() const { return m_gtOverlayVisible; }
    bool detOverlayVisible() const { return m_detOverlayVisible; }
    bool hasImage() const { return !m_sourceImage.isNull(); }
    bool canRunBatch() const;
    bool canStartLive() const;
    int liveSourceKind() const { return m_liveSourceKind; }
    QString liveStartButtonText() const;
    QString selfCheckIntroText() const;
    bool usingWebcam() const { return m_liveSourceKind == 1; }
    bool liveRunning() const { return m_liveRunning; }
    bool previewRunning() const { return m_previewRunning; }
    int liveTargetFps() const { return m_liveTargetFps; }
    int liveLatencyMs() const { return m_liveLatencyMs; }
    int liveQueueDepth() const { return m_liveQueueDepth; }
    int liveQueueMax() const { return InspectionSession::kMaxQueue; }
    double liveActualFps() const { return m_liveActualFps; }
    bool canExportBatch() const;
    bool paramsDirty() const { return m_paramsDirty; }
    bool detected() const { return m_detected; }
    bool verdictOk() const { return m_verdictOk; }
    int defectCount() const { return m_defectCount; }
    double totalArea() const { return m_totalArea; }
    int minImageArea() const { return m_minImageArea; }
    double imageScore() const { return m_imageScore; }
    double imageThreshold() const { return m_imageThreshold; }
    QImage sourceImage() const { return m_sourceImage; }
    QImage gtOverlayImage() const { return m_gtOverlayImage; }
    QImage detOverlayImage() const { return m_detOverlayImage; }
    QVariantList boxRects() const { return m_boxRects; }
    int inspectorTab() const { return m_inspectorTab; }
    int workMode() const { return m_workMode; }
    bool hasLiveSession() const { return m_hasLiveSession; }
    bool canExportLive() const;
    int liveDroppedCount() const { return m_liveDroppedCount; }
    int liveLateCount() const { return m_liveLateCount; }
    int liveMaxQueue() const { return m_liveMaxQueue; }
    int liveMaxLatencyMs() const { return m_liveMaxLatencyMs; }
    QString liveSessionTitle() const { return m_liveSessionTitle; }
    QString liveSessionDir() const { return m_liveSessionDir; }
    bool hasMetrics() const { return m_hasMetrics; }
    bool hasCompare() const { return m_hasCompare; }
    QString compareCvF1() const { return m_compareCvF1; }
    QString compareDlF1() const { return m_compareDlF1; }
    QString compareDeltaF1() const { return m_compareDeltaF1; }
    QString compareCvImg() const { return m_compareCvImg; }
    QString compareDlImg() const { return m_compareDlImg; }
    QString compareCvFpr() const { return m_compareCvFpr; }
    QString compareDlFpr() const { return m_compareDlFpr; }

    double cvZAggThreshold() const { return m_cv.zAggThreshold; }
    int cvMorphCloseKernel() const { return m_cv.morphCloseKernel; }
    int cvMinDefectArea() const { return m_cv.minDefectArea; }
    int cvImageLevelMinArea() const { return m_cv.imageLevelMinArea; }
    double dlThresholdSigma() const { return m_dl.thresholdSigma; }
    int dlMorphCloseKernel() const { return m_dl.morphCloseKernel; }
    int dlMinDefectArea() const { return m_dl.minDefectArea; }
    int dlImageLevelMinArea() const { return m_dl.imageLevelMinArea; }

    DatasetTreeModel* datasetModel() const { return m_datasetModel; }
    BoxListModel* boxModel() const { return m_boxModel; }
    MetricsListModel* metricsModel() const { return m_metricsModel; }
    CompareListModel* compareModel() const { return m_compareModel; }
    NgListModel* ngModel() const { return m_ngModel; }
    DoPulseModel* doPulseModel() const { return m_doPulseModel; }

    QString cameraStatusText() const;
    QString sourceStatusText() const;
    QString plcStatusText() const;
    QString workOrder() const { return m_workOrder; }
    QString operatorName() const { return m_operatorName; }
    int consecutiveNgLimit() const { return m_consecutiveNgLimit; }
    QVariantList selfCheckItems() const { return m_selfCheckItems; }
    bool selfCheckPassed() const { return m_selfCheckPassed; }
    QString selfCheckHint() const { return m_selfCheckHint; }
    double liveYieldPercent() const;
    int liveTaktMs() const { return m_liveTaktMs; }
    int liveConsecutiveNg() const { return m_liveConsecutiveNg; }
    QString liveStopReason() const { return m_liveStopReason; }
    bool liveInterlocked() const { return m_liveInterlocked; }
    int livePlannedCount() const { return m_livePlannedCount; }

    Q_INVOKABLE bool loadDataset(const QUrl& folder);
    bool loadDatasetPath(const QString& path);

    Q_INVOKABLE void selectNode(const QString& nodeType, const QString& category,
                                const QString& defectType, const QString& imagePath);
    Q_INVOKABLE void selectFromModelIndex(const QModelIndex& index);
    void setEngineKind(int kind);
    void setGtOverlayVisible(bool visible);
    void setDetOverlayVisible(bool visible);
    void setInspectorTab(int tab);
    void setWorkMode(int mode);

    void setCvZAggThreshold(double v);
    void setCvMorphCloseKernel(int v);
    void setCvMinDefectArea(int v);
    void setCvImageLevelMinArea(int v);
    void setDlThresholdSigma(double v);
    void setDlMorphCloseKernel(int v);
    void setDlMinDefectArea(int v);
    void setDlImageLevelMinArea(int v);

    Q_INVOKABLE void applyParams();
    Q_INVOKABLE void restoreParams();
    Q_INVOKABLE void runBatch();
    Q_INVOKABLE void compareEngines();
    Q_INVOKABLE void startLive();
    Q_INVOKABLE void stopLive();
    Q_INVOKABLE void requestStartLive();
    Q_INVOKABLE void confirmStartLive();
    void setLiveTargetFps(int fps);
    void setLiveSourceKind(int kind);
    void setWorkOrder(const QString& v);
    void setOperatorName(const QString& v);
    void setConsecutiveNgLimit(int n);
    Q_INVOKABLE bool exportCurrent(const QUrl& url);
    Q_INVOKABLE bool exportBatch(const QUrl& folder);
    Q_INVOKABLE bool exportLiveSession(const QUrl& folder);
    Q_INVOKABLE void reviewNg(int row);
    Q_INVOKABLE QUrl suggestedExportFileUrl() const;
    Q_INVOKABLE QUrl suggestedExportFolderUrl() const;
    Q_INVOKABLE QUrl suggestedLiveExportFolderUrl() const;
    Q_INVOKABLE QUrl datasetRootUrl() const;
    Q_INVOKABLE void clearToast();

signals:
    void hasDatasetChanged();
    void busyChanged();
    void progressChanged();
    void statusTextChanged();
    void errorMessageChanged();
    void selectionChanged();
    void engineKindChanged();
    void overlayChanged();
    void imageChanged();
    void workEnabledChanged();
    void liveRunningChanged();
    void previewRunningChanged();
    void liveSourceKindChanged();
    void liveTargetFpsChanged();
    void liveStatsChanged();
    void paramsDirtyChanged();
    void detectionChanged();
    void inspectorTabChanged();
    void workModeChanged();
    void liveSessionChanged();
    void hasMetricsChanged();
    void hasCompareChanged();
    void cvParamsChanged();
    void dlParamsChanged();
    void stationStatusChanged();
    void toastMessageChanged();
    void recipeChanged();
    void selfCheckChanged();
    void selfCheckRequested();

private:
    EngineKind currentKind() const;
    QString currentEngineName() const;
    void setStatusText(const QString& text);
    void raiseError(const QString& msg);
    void loadCurrentImage();
    void runDetectionForCurrent();
    void applyDetectionResult(const DetectionResult& result);
    void clearDetection();
    void syncParamsFromSettings();
    void saveParamsToSettings();
    TraditionalParams loadTraditionalSettings(const QString& category) const;
    DLParams loadDLSettings(const QString& category) const;
    void fillCv(const TraditionalParams& p);
    void fillDl(const DLParams& p);
    void updateParamsDirty();
    void refreshBatchDependent();
    void refreshMetrics();
    void refreshCompare();
    void refreshStationStatus();
    void showToast(const QString& msg);
    void setStatusTone(const QString& tone);
    TraditionalParams clampCv(const TraditionalParams& p);
    DLParams clampDl(const DLParams& p);
    QString missingDlReason(const QString& category) const;
    bool dlEngineBlocked(const QString& category) const;

    void onProgress(int current, int total, const QString& text);
    void onBusyChanged(bool busy);
    void onDetectFinished(bool ok, const DetectionResult& result);
    void onEnginePrepared(bool ok, const QString& category);
    void onBatchFinished(bool ok, const QString& category);
    void onCompareFinished(bool ok, const QString& category);
    void onLiveFrame(const LiveInspectedFrame& frame);
    void onLiveFinished(int total, int ngCount);
    void onLiveError(const QString& msg);
    void setLiveRunning(bool running);
    void startPreview();
    void stopPreview();
    void setPreviewRunning(bool running);
    void onPreviewFrame(const QImage& image);
    void onPreviewError(const QString& msg);

    void applyLiveSummary(const LiveSessionSummary& s);
    void beginLiveSession(const QString& category);
    void loadStationSettings();
    void saveStationSettings();
    void rebuildSelfCheck();
    int testImageCount(const QString& category) const;
    void appendDoPulse(const LiveInspectedFrame& frame);

    DetectionController m_ctrl;
    InspectionSession m_session;
    WebcamPreview m_preview;
    DatasetTreeModel* m_datasetModel = nullptr;
    BoxListModel* m_boxModel = nullptr;
    MetricsListModel* m_metricsModel = nullptr;
    CompareListModel* m_compareModel = nullptr;
    NgListModel* m_ngModel = nullptr;
    DoPulseModel* m_doPulseModel = nullptr;

    bool m_hasDataset = false;
    bool m_busy = false;
    bool m_syncingParams = false;
    bool m_paramsDirty = false;
    bool m_gtOverlayVisible = false;
    bool m_gtOverlayBeforeLive = false;
    bool m_detOverlayVisible = true;
    bool m_detected = false;
    bool m_verdictOk = true;
    bool m_hasMetrics = false;
    bool m_hasCompare = false;
    bool m_liveRunning = false;
    bool m_liveStarting = false;
    bool m_previewRunning = false;
    int m_liveSourceKind = 0; // 0=FolderSource，1=WebcamSource；不启动 CameraSource
    int m_engineKind = 0;
    int m_liveTargetFps = InspectionSession::kDefaultFps;
    int m_liveLatencyMs = 0;
    int m_liveQueueDepth = 0;
    double m_liveActualFps = 0.0;
    int m_progressCurrent = 0;
    int m_progressTotal = 0;
    int m_inspectorTab = 0;
    int m_workMode = 0;
    int m_defectCount = 0;
    int m_minImageArea = 0;
    double m_totalArea = 0.0;
    double m_imageScore = 0.0;
    double m_imageThreshold = 0.0;

    QString m_datasetRoot;
    QString m_currentCategory;
    QString m_currentDefectType;
    QString m_currentImagePath;
    QString m_statusText = QStringLiteral("就绪");
    QString m_progressText;
    QString m_errorMessage;
    QString m_engineStatusText = QStringLiteral("传统 CV");
    QString m_providerText;
    QString m_calibStatusText;
    QString m_expectedOnnxPath;
    QString m_stationAlert;
    QString m_toastMessage;
    QString m_busyKind;
    QString m_busySubtitle;
    QString m_statusTone = QStringLiteral("normal");
    bool m_modelAvailable = false;
    bool m_calibCached = false;
    bool m_stationAlertIsError = false;
    bool m_missingModelDialogShown = false;
    int m_liveOkCount = 0;
    int m_liveNgCount = 0;
    int m_liveDroppedCount = 0;
    int m_liveLateCount = 0;
    int m_liveMaxQueue = 0;
    int m_liveMaxLatencyMs = 0;
    bool m_liveLastNg = false;
    bool m_hasLiveSession = false;
    QString m_liveSessionTitle;
    QString m_liveSessionDir;
    QString m_workOrder;
    QString m_operatorName;
    QString m_liveStopReason;
    QVariantList m_selfCheckItems;
    QString m_selfCheckHint;
    int m_consecutiveNgLimit = 8;
    int m_liveConsecutiveNg = 0;
    int m_liveTaktMs = 0;
    int m_livePlannedCount = 0;
    bool m_selfCheckPassed = false;
    bool m_liveInterlocked = false;
    QElapsedTimer m_liveClock;
    QString m_compareCvF1;
    QString m_compareDlF1;
    QString m_compareDeltaF1;
    QString m_compareCvImg;
    QString m_compareDlImg;
    QString m_compareCvFpr;
    QString m_compareDlFpr;

    QImage m_sourceImage;
    QImage m_gtOverlayImage;
    QImage m_detOverlayImage;
    QVariantList m_boxRects;
    cv::Mat m_currentBgr;
    cv::Mat m_currentGt;
    DetectionResult m_currentResult;

    TraditionalParams m_cv;
    DLParams m_dl;
    TraditionalParams m_appliedCv;
    DLParams m_appliedDl;
};
