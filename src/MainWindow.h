#pragma once

#include <QMainWindow>

#include "DatasetManager.h"
#include "DetectionEngine.h"
#include "ResultEvaluator.h"

#include <opencv2/core.hpp>

class QTreeWidget;
class QTreeWidgetItem;
class QTableWidget;
class QCheckBox;
class QPushButton;
class QLabel;
class QSplitter;
class ImageViewWidget;

// 主窗口：左侧数据集树 / 中间图像查看器 / 右侧结果面板
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

private:
    void buildUi();
    void populateTree();
    void showImage(const QString& category, const QString& defectType,
                   const QString& imagePath);
    void runDetectionForCurrent();
    // 每类一个引擎实例，惰性构建并缓存（参考模型构建需读全部良品图，较耗时）
    DetectionEngine* engineFor(const QString& category);
    void updateMetricsTable(const QString& category,
                            const QMap<QString, PixelMetrics>& pixel,
                            const QMap<QString, ImageMetrics>& image);

    DatasetManager m_dataset;
    QMap<QString, DetectionEngine*> m_engines; // 类别 -> 引擎（惰性构建）
    QString m_currentCategory;
    QString m_currentDefectType;
    QString m_currentImagePath;
    cv::Mat m_currentBgr;
    DetectionResult m_currentResult;
    bool m_hasDetection = false;

    QSplitter* m_splitter = nullptr;
    QTreeWidget* m_tree = nullptr;
    ImageViewWidget* m_view = nullptr;
    QLabel* m_imageInfoLabel = nullptr;
    QCheckBox* m_gtOverlayCheck = nullptr;
    QCheckBox* m_detOverlayCheck = nullptr;
    QTableWidget* m_boxTable = nullptr;
    QPushButton* m_batchButton = nullptr;
    QTableWidget* m_metricsTable = nullptr;
    QLabel* m_statusLabel = nullptr;
};
