#include "MainWindow.h"

#include "ImageViewWidget.h"

#include <QTreeWidget>
#include <QTableWidget>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileInfo>
#include <QMessageBox>
#include <QFileDialog>
#include <QApplication>
#include <QCoreApplication>
#include <QDir>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    buildUi();
    resize(1400, 900);
    setWindowTitle(QStringLiteral("表面缺陷检测工具"));
}

MainWindow::~MainWindow()
{
    qDeleteAll(m_engines);
}

void MainWindow::buildUi()
{
    m_splitter = new QSplitter(this);

    // 左：数据集树
    m_tree = new QTreeWidget;
    m_tree->setHeaderLabel(QStringLiteral("数据集"));
    m_tree->setMinimumWidth(220);
    m_splitter->addWidget(m_tree);

    // 中：图像查看器
    m_view = new ImageViewWidget;
    m_splitter->addWidget(m_view);

    // 右：结果面板
    QWidget* rightPanel = new QWidget;
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);

    m_imageInfoLabel = new QLabel(QStringLiteral("未选择图片"));
    m_imageInfoLabel->setWordWrap(true);
    rightLayout->addWidget(m_imageInfoLabel);

    QHBoxLayout* overlayLayout = new QHBoxLayout;
    m_gtOverlayCheck = new QCheckBox(QStringLiteral("GT掩码"));
    m_gtOverlayCheck->setChecked(true);
    m_detOverlayCheck = new QCheckBox(QStringLiteral("检测结果"));
    m_detOverlayCheck->setChecked(true);
    overlayLayout->addWidget(m_gtOverlayCheck);
    overlayLayout->addWidget(m_detOverlayCheck);
    overlayLayout->addStretch();
    rightLayout->addLayout(overlayLayout);

    rightLayout->addWidget(new QLabel(QStringLiteral("缺陷框：")));
    m_boxTable = new QTableWidget(0, 5);
    m_boxTable->setHorizontalHeaderLabels(
        {QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("宽"),
         QStringLiteral("高"), QStringLiteral("面积")});
    m_boxTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_boxTable->setMaximumHeight(180);
    rightLayout->addWidget(m_boxTable);

    m_batchButton = new QPushButton(QStringLiteral("批量运行当前类别"));
    m_batchButton->setEnabled(false);
    rightLayout->addWidget(m_batchButton);

    rightLayout->addWidget(new QLabel(QStringLiteral("各类指标：")));
    m_metricsTable = new QTableWidget(0, 6);
    m_metricsTable->setHorizontalHeaderLabels(
        {QStringLiteral("缺陷类型"), QStringLiteral("P"), QStringLiteral("R"),
         QStringLiteral("F1"), QStringLiteral("IoU"), QStringLiteral("图像级")});
    m_metricsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    rightLayout->addWidget(m_metricsTable);

    m_statusLabel = new QLabel;
    rightLayout->addWidget(m_statusLabel);

    rightPanel->setMinimumWidth(320);
    rightPanel->setMaximumWidth(420);
    m_splitter->addWidget(rightPanel);

    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setStretchFactor(2, 0);
    setCentralWidget(m_splitter);

    connect(m_tree, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem*, QTreeWidgetItem*) { onTreeSelectionChanged(); });
    connect(m_batchButton, &QPushButton::clicked, this, &MainWindow::onRunBatch);
    connect(m_gtOverlayCheck, &QCheckBox::toggled, this, &MainWindow::onOverlayToggled);
    connect(m_detOverlayCheck, &QCheckBox::toggled, this, &MainWindow::onOverlayToggled);
}

bool MainWindow::loadDataset(const QString& rootPath)
{
    if (m_dataset.scan(rootPath) == 0)
        return false;
    populateTree();
    m_statusLabel->setText(QStringLiteral("数据集：%1").arg(m_dataset.rootPath()));
    return true;
}

void MainWindow::populateTree()
{
    m_tree->clear();
    const QStringList cats = m_dataset.categories();
    for (const QString& cat : cats) {
        QTreeWidgetItem* catItem = new QTreeWidgetItem(m_tree, {cat});
        catItem->setData(0, Qt::UserRole, QStringLiteral("category"));
        const QStringList defects = m_dataset.defectTypes(cat);
        for (const QString& defect : defects) {
            QTreeWidgetItem* defectItem = new QTreeWidgetItem(catItem, {defect});
            defectItem->setData(0, Qt::UserRole, QStringLiteral("defect"));
            const QStringList images = m_dataset.testImages(cat, defect);
            for (const QString& img : images) {
                QTreeWidgetItem* imgItem =
                    new QTreeWidgetItem(defectItem, {QFileInfo(img).fileName()});
                imgItem->setData(0, Qt::UserRole, QStringLiteral("image"));
                imgItem->setData(0, Qt::UserRole + 1, img);
            }
        }
        catItem->setExpanded(true);
    }
}

void MainWindow::onTreeSelectionChanged()
{
    QTreeWidgetItem* item = m_tree->currentItem();
    if (!item)
        return;

    const QString role = item->data(0, Qt::UserRole).toString();
    if (role == QStringLiteral("image")) {
        QTreeWidgetItem* defectItem = item->parent();
        QTreeWidgetItem* catItem = defectItem ? defectItem->parent() : nullptr;
        if (!catItem)
            return;
        m_currentCategory = catItem->text(0);
        m_currentDefectType = defectItem->text(0);
        m_currentImagePath = item->data(0, Qt::UserRole + 1).toString();
        showImage(m_currentCategory, m_currentDefectType, m_currentImagePath);
        m_batchButton->setEnabled(true);
    } else if (role == QStringLiteral("defect") || role == QStringLiteral("category")) {
        // 选中类别/缺陷类型节点时，仅启用批量按钮（类别批量）
        QTreeWidgetItem* catItem = (role == QStringLiteral("category")) ? item : item->parent();
        if (catItem) {
            m_currentCategory = catItem->text(0);
            m_batchButton->setEnabled(true);
        }
    }
}

void MainWindow::showImage(const QString& category, const QString& defectType,
                           const QString& imagePath)
{
    m_currentBgr = cv::imread(imagePath.toLocal8Bit().constData(), cv::IMREAD_COLOR);
    if (m_currentBgr.empty()) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法读取图片：%1").arg(imagePath));
        return;
    }
    m_view->setImage(m_currentBgr);

    // GT 掩码叠加
    const QString gtPath = m_dataset.groundTruthMask(category, defectType, imagePath);
    if (!gtPath.isEmpty()) {
        cv::Mat gt = cv::imread(gtPath.toLocal8Bit().constData(), cv::IMREAD_GRAYSCALE);
        if (!gt.empty() && gt.size() != m_currentBgr.size())
            cv::resize(gt, gt, m_currentBgr.size(), 0, 0, cv::INTER_NEAREST);
        m_view->setGroundTruthMask(gt);
    }

    m_imageInfoLabel->setText(QStringLiteral("%1 / %2 / %3")
                                  .arg(category, defectType, QFileInfo(imagePath).fileName()));

    m_hasDetection = false;
    m_boxTable->setRowCount(0);
    runDetectionForCurrent();
    onOverlayToggled();
}

void MainWindow::runDetectionForCurrent()
{
    if (m_currentBgr.empty() || m_currentCategory.isEmpty())
        return;

    DetectionEngine* engine = engineFor(m_currentCategory);
    if (!engine)
        return;

    m_currentResult = engine->detect(m_currentBgr);
    m_hasDetection = true;
    m_view->setDetectionOverlay(m_currentResult.defectMask, m_currentResult.boxes);

    m_boxTable->setRowCount(static_cast<int>(m_currentResult.boxes.size()));
    for (size_t i = 0; i < m_currentResult.boxes.size(); ++i) {
        const cv::Rect& r = m_currentResult.boxes[i];
        const int row = static_cast<int>(i);
        m_boxTable->setItem(row, 0, new QTableWidgetItem(QString::number(r.x)));
        m_boxTable->setItem(row, 1, new QTableWidgetItem(QString::number(r.y)));
        m_boxTable->setItem(row, 2, new QTableWidgetItem(QString::number(r.width)));
        m_boxTable->setItem(row, 3, new QTableWidgetItem(QString::number(r.height)));
        m_boxTable->setItem(row, 4,
            new QTableWidgetItem(QString::number(m_currentResult.areas[i])));
    }
}

void MainWindow::onOverlayToggled()
{
    m_view->setGtOverlayVisible(m_gtOverlayCheck->isChecked());
    m_view->setDetectionOverlayVisible(m_detOverlayCheck->isChecked());
}

DetectionEngine* MainWindow::engineFor(const QString& category)
{
    DetectionEngine* engine = m_engines.value(category, nullptr);
    if (engine)
        return engine;
    engine = new DetectionEngine;
    if (!engine->buildReference(m_dataset.trainGoodImages(category))) {
        delete engine;
        return nullptr;
    }
    m_engines.insert(category, engine);
    return engine;
}

void MainWindow::onRunBatch()
{
    if (m_currentCategory.isEmpty())
        return;

    const QString cat = m_currentCategory;
    DetectionEngine* engine = engineFor(cat);
    if (!engine) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法构建 %1 的参考模型（train/good 为空）").arg(cat));
        return;
    }

    QMap<QString, PixelMetrics> pixel;
    QMap<QString, ImageMetrics> image;
    const QStringList defects = m_dataset.defectTypes(cat);
    for (const QString& defect : defects) {
        const bool isDefect = (defect != QStringLiteral("good"));
        const QStringList images = m_dataset.testImages(cat, defect);
        for (const QString& imgPath : images) {
            cv::Mat img = cv::imread(imgPath.toLocal8Bit().constData(), cv::IMREAD_COLOR);
            if (img.empty())
                continue;
            DetectionResult r = engine->detect(img);
            const QString gtPath = m_dataset.groundTruthMask(cat, defect, imgPath);
            pixel[defect] += ResultEvaluator::evaluatePixel(r.defectMask, gtPath);
            image[defect] += ResultEvaluator::evaluateImage(r.detected(), isDefect);
        }
        QCoreApplication::processEvents(); // 保持界面响应
    }

    updateMetricsTable(cat, pixel, image);
    m_statusLabel->setText(QStringLiteral("已完成 %1 批量检测").arg(cat));
}

void MainWindow::updateMetricsTable(const QString& category,
                                    const QMap<QString, PixelMetrics>& pixel,
                                    const QMap<QString, ImageMetrics>& image)
{
    Q_UNUSED(category);
    const QStringList defects = pixel.keys();
    m_metricsTable->setRowCount(defects.size());
    int row = 0;
    for (const QString& defect : defects) {
        const PixelMetrics& p = pixel[defect];
        const ImageMetrics& im = image[defect];
        auto num = [](double v) { return QString::number(v, 'f', 3); };
        m_metricsTable->setItem(row, 0, new QTableWidgetItem(defect));
        m_metricsTable->setItem(row, 1, new QTableWidgetItem(num(p.precision())));
        m_metricsTable->setItem(row, 2, new QTableWidgetItem(num(p.recall())));
        m_metricsTable->setItem(row, 3, new QTableWidgetItem(num(p.f1())));
        m_metricsTable->setItem(row, 4, new QTableWidgetItem(num(p.iou())));
        m_metricsTable->setItem(row, 5, new QTableWidgetItem(
            QStringLiteral("%1 (%2/%3)").arg(num(im.accuracy()))
                .arg(im.correct).arg(im.total)));
        ++row;
    }
}
