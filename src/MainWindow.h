#pragma once

#include <QMainWindow>

#include "DetectionController.h"
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
class ImageViewWidget;

// 主窗口（纯视图层）：左侧数据集树 / 中间图像查看器 / 右侧结果面板。
// 只做三件事：摆控件、把用户操作转发给 DetectionController、把结果渲染到界面；
// 数据集、引擎生命周期、批量编排全部在 DetectionController。
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    // 加载数据集根目录；失败返回 false
    bool loadDataset(const QString& rootPath);

private slots:
    void onTreeSelectionChanged();
    void onRunBatch();
    void onOverlayToggled();

private:
    void buildUi();
    void populateTree();
    void showImage(const QString& category, const QString& defectType,
                   const QString& imagePath);
    void runDetectionForCurrent();
    void updateMetricsTable(const QMap<QString, PixelMetrics>& pixel,
                            const QMap<QString, ImageMetrics>& image);

    DetectionController m_ctrl;
    QString m_currentCategory;
    QString m_currentDefectType;
    QString m_currentImagePath;
    cv::Mat m_currentBgr;

    QSplitter* m_splitter = nullptr;
    QTreeWidget* m_tree = nullptr;
    ImageViewWidget* m_view = nullptr;
    QLabel* m_imageInfoLabel = nullptr;
    QCheckBox* m_gtOverlayCheck = nullptr;
    QCheckBox* m_detOverlayCheck = nullptr;
    QTableWidget* m_boxTable = nullptr;
    QComboBox* m_engineCombo = nullptr;
    QPushButton* m_batchButton = nullptr;
    QTableWidget* m_metricsTable = nullptr;
    QLabel* m_statusLabel = nullptr;
};
