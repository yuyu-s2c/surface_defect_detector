#pragma once

#include <QMainWindow>

#include "DetectionController.h"
#include "EngineParams.h"
#include "IDetectionEngine.h"
#include "ResultEvaluator.h"

#include <opencv2/core.hpp>

class QTreeWidget;
class QTreeWidgetItem;
class QTableWidget;
class QCheckBox;
class QComboBox;
class QPushButton;
class QLabel;
class QSplitter;
class QStackedWidget;
class QDoubleSpinBox;
class QSpinBox;
class QProgressBar;
class ImageViewWidget;

// 主窗口（纯视图层）：左侧数据集树 / 中间图像查看器 / 右侧结果面板。
// 只做三件事：摆控件、把用户操作转发给 DetectionController、把结果渲染到界面；
// 数据集、引擎生命周期、批量编排全部在 DetectionController。
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // 加载数据集根目录；失败返回 false
    bool loadDataset(const QString& rootPath);

private slots:
    void onTreeSelectionChanged();
    void onRunBatch();
    void onOverlayToggled();
    void onApplyParams();
    void onRestoreParams();
    void onExportCurrent();
    void onExportBatch();
    void onCompareEngines();
    void onProgress(int current, int total, const QString& text);
    void onBusyChanged(bool busy);
    void onDetectFinished(bool ok, const DetectionResult& result);
    void onBatchFinished(bool ok, const QString& category);
    void onCompareFinished(bool ok, const QString& category);

private:
    void buildUi();
    void populateTree();
    void showImage(const QString& category, const QString& defectType,
                   const QString& imagePath);
    void runDetectionForCurrent();
    void applyDetectionResult(const DetectionResult& result);
    void setWorkEnabled(bool enabled);
    void updateMetricsTable(const QMap<QString, PixelMetrics>& pixel,
                            const QMap<QString, ImageMetrics>& image);
    void updateExportButtons();
    void updateCompareTable();
    void setStatusText(const QString& text);

    EngineKind currentKind() const;
    QString currentEngineName() const;

    // 从 QSettings（无键则 P2 默认）填控件并下发 Controller，不写盘
    void syncParamsFromSettings();
    TraditionalParams traditionalParamsFromWidgets() const;
    DLParams dlParamsFromWidgets() const;
    void fillTraditionalWidgets(const TraditionalParams& p);
    void fillDLWidgets(const DLParams& p);
    TraditionalParams loadTraditionalSettings(const QString& category) const;
    DLParams loadDLSettings(const QString& category) const;
    void saveParamsToSettings();

    DetectionController m_ctrl;
    QString m_currentCategory;
    QString m_currentDefectType;
    QString m_currentImagePath;
    cv::Mat m_currentBgr;
    cv::Mat m_currentGt;
    DetectionResult m_currentResult;

    QSplitter* m_splitter = nullptr;
    QTreeWidget* m_tree = nullptr;
    ImageViewWidget* m_view = nullptr;
    QLabel* m_imageInfoLabel = nullptr;
    QCheckBox* m_gtOverlayCheck = nullptr;
    QCheckBox* m_detOverlayCheck = nullptr;
    QTableWidget* m_boxTable = nullptr;
    QComboBox* m_engineCombo = nullptr;
    QStackedWidget* m_paramStack = nullptr;
    QDoubleSpinBox* m_cvZThresh = nullptr;
    QSpinBox* m_cvMorph = nullptr;
    QSpinBox* m_cvMinArea = nullptr;
    QSpinBox* m_cvImageArea = nullptr;
    QDoubleSpinBox* m_dlSigma = nullptr;
    QSpinBox* m_dlMorph = nullptr;
    QSpinBox* m_dlMinArea = nullptr;
    QSpinBox* m_dlImageArea = nullptr;
    QPushButton* m_applyParamsButton = nullptr;
    QPushButton* m_restoreParamsButton = nullptr;
    QPushButton* m_batchButton = nullptr;
    QPushButton* m_exportCurrentButton = nullptr;
    QPushButton* m_exportBatchButton = nullptr;
    QPushButton* m_compareButton = nullptr;
    QTableWidget* m_metricsTable = nullptr;
    QTableWidget* m_compareTable = nullptr;
    QLabel* m_statusLabel = nullptr;
    QProgressBar* m_progressBar = nullptr;
};
