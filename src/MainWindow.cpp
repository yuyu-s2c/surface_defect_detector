#include "MainWindow.h"

#include "ImageViewWidget.h"
#include "ResultExporter.h"

#include <QTreeWidget>
#include <QTableWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QStackedWidget>
#include <QScrollArea>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QHeaderView>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QSettings>
#include <QDir>
#include <QProgressBar>
#include <QStatusBar>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace {

QDoubleSpinBox* makeDoubleSpin(double min, double max, double step, int decimals)
{
    auto* s = new QDoubleSpinBox;
    s->setRange(min, max);
    s->setSingleStep(step);
    s->setDecimals(decimals);
    s->setKeyboardTracking(false);
    return s;
}

QSpinBox* makeIntSpin(int min, int max, int step)
{
    auto* s = new QSpinBox;
    s->setRange(min, max);
    s->setSingleStep(step);
    s->setKeyboardTracking(false);
    return s;
}

QString fmt3(double v)
{
    return QString::number(v, 'f', 3);
}

QString imageAccText(const ImageMetrics& im)
{
    return QStringLiteral("%1 (%2/%3)").arg(fmt3(im.accuracy())).arg(im.correct).arg(im.total);
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    buildUi();
    resize(1400, 900);
    setWindowTitle(QStringLiteral("表面缺陷检测工具"));
}

MainWindow::~MainWindow()
{
    if (m_ctrl.isBusy())
        QApplication::restoreOverrideCursor();
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

    // 右：结果面板（可滚动，P3 参数/导出/对比会撑高）
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

    QHBoxLayout* engineLayout = new QHBoxLayout;
    engineLayout->addWidget(new QLabel(QStringLiteral("检测引擎：")));
    m_engineCombo = new QComboBox;
    m_engineCombo->addItem(QStringLiteral("传统 CV（v0.1 基线）"));
    m_engineCombo->addItem(QStringLiteral("深度学习（EfficientAD）"));
    engineLayout->addWidget(m_engineCombo);
    engineLayout->addStretch();
    rightLayout->addLayout(engineLayout);

    auto* paramBox = new QGroupBox(QStringLiteral("检测参数"));
    auto* paramLayout = new QVBoxLayout(paramBox);
    m_paramStack = new QStackedWidget;

    auto* cvPage = new QWidget;
    auto* cvForm = new QFormLayout(cvPage);
    m_cvZThresh = makeDoubleSpin(0.1, 10.0, 0.1, 2);
    m_cvMorph = makeIntSpin(1, 51, 2);
    m_cvMinArea = makeIntSpin(0, 100000, 50);
    m_cvImageArea = makeIntSpin(0, 1000000, 100);
    cvForm->addRow(QStringLiteral("聚合阈值"), m_cvZThresh);
    cvForm->addRow(QStringLiteral("闭运算核"), m_cvMorph);
    cvForm->addRow(QStringLiteral("最小面积"), m_cvMinArea);
    cvForm->addRow(QStringLiteral("图像级门"), m_cvImageArea);
    m_paramStack->addWidget(cvPage);

    auto* dlPage = new QWidget;
    auto* dlForm = new QFormLayout(dlPage);
    m_dlSigma = makeDoubleSpin(0.1, 8.0, 0.1, 2);
    m_dlMorph = makeIntSpin(1, 51, 2);
    m_dlMinArea = makeIntSpin(0, 100000, 50);
    m_dlImageArea = makeIntSpin(0, 1000000, 50);
    dlForm->addRow(QStringLiteral("阈值 kσ"), m_dlSigma);
    dlForm->addRow(QStringLiteral("闭运算核"), m_dlMorph);
    dlForm->addRow(QStringLiteral("最小面积"), m_dlMinArea);
    dlForm->addRow(QStringLiteral("图像级门"), m_dlImageArea);
    m_paramStack->addWidget(dlPage);

    paramLayout->addWidget(m_paramStack);
    auto* paramBtnLayout = new QHBoxLayout;
    m_applyParamsButton = new QPushButton(QStringLiteral("应用"));
    m_restoreParamsButton = new QPushButton(QStringLiteral("恢复默认"));
    paramBtnLayout->addWidget(m_applyParamsButton);
    paramBtnLayout->addWidget(m_restoreParamsButton);
    paramLayout->addLayout(paramBtnLayout);
    rightLayout->addWidget(paramBox);

    rightLayout->addWidget(new QLabel(QStringLiteral("缺陷框：")));
    m_boxTable = new QTableWidget(0, 5);
    m_boxTable->setHorizontalHeaderLabels(
        {QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("宽"),
         QStringLiteral("高"), QStringLiteral("面积")});
    m_boxTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_boxTable->setMaximumHeight(140);
    rightLayout->addWidget(m_boxTable);

    m_batchButton = new QPushButton(QStringLiteral("批量运行当前类别"));
    m_batchButton->setEnabled(false);
    rightLayout->addWidget(m_batchButton);

    auto* exportLayout = new QHBoxLayout;
    m_exportCurrentButton = new QPushButton(QStringLiteral("导出当前图"));
    m_exportCurrentButton->setEnabled(false);
    m_exportBatchButton = new QPushButton(QStringLiteral("导出批量结果"));
    m_exportBatchButton->setEnabled(false);
    exportLayout->addWidget(m_exportCurrentButton);
    exportLayout->addWidget(m_exportBatchButton);
    rightLayout->addLayout(exportLayout);

    rightLayout->addWidget(new QLabel(QStringLiteral("各类指标：")));
    m_metricsTable = new QTableWidget(0, 6);
    m_metricsTable->setHorizontalHeaderLabels(
        {QStringLiteral("缺陷类型"), QStringLiteral("P"), QStringLiteral("R"),
         QStringLiteral("F1"), QStringLiteral("IoU"), QStringLiteral("图像级")});
    m_metricsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_metricsTable->setMaximumHeight(180);
    rightLayout->addWidget(m_metricsTable);

    m_compareButton = new QPushButton(QStringLiteral("对比双引擎"));
    m_compareButton->setEnabled(false);
    rightLayout->addWidget(m_compareButton);

    rightLayout->addWidget(new QLabel(QStringLiteral("双引擎对比：")));
    m_compareTable = new QTableWidget(0, 10);
    m_compareTable->setHorizontalHeaderLabels(
        {QStringLiteral("缺陷类型"),
         QStringLiteral("CV P"), QStringLiteral("CV R"), QStringLiteral("CV F1"),
         QStringLiteral("CV 图像级"),
         QStringLiteral("DL P"), QStringLiteral("DL R"), QStringLiteral("DL F1"),
         QStringLiteral("DL 图像级"), QStringLiteral("ΔF1")});
    m_compareTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_compareTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_compareTable->setMaximumHeight(200);
    rightLayout->addWidget(m_compareTable);

    m_statusLabel = new QLabel;
    m_statusLabel->setWordWrap(true);
    rightLayout->addWidget(m_statusLabel);

    auto* scroll = new QScrollArea;
    scroll->setWidget(rightPanel);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setMinimumWidth(360);
    scroll->setMaximumWidth(520);
    m_splitter->addWidget(scroll);

    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setStretchFactor(2, 0);
    setCentralWidget(m_splitter);

    connect(m_tree, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem*, QTreeWidgetItem*) { onTreeSelectionChanged(); });
    connect(m_batchButton, &QPushButton::clicked, this, &MainWindow::onRunBatch);
    connect(m_gtOverlayCheck, &QCheckBox::toggled, this, &MainWindow::onOverlayToggled);
    connect(m_detOverlayCheck, &QCheckBox::toggled, this, &MainWindow::onOverlayToggled);
    connect(m_applyParamsButton, &QPushButton::clicked, this, &MainWindow::onApplyParams);
    connect(m_restoreParamsButton, &QPushButton::clicked, this, &MainWindow::onRestoreParams);
    connect(m_exportCurrentButton, &QPushButton::clicked, this, &MainWindow::onExportCurrent);
    connect(m_exportBatchButton, &QPushButton::clicked, this, &MainWindow::onExportBatch);
    connect(m_compareButton, &QPushButton::clicked, this, &MainWindow::onCompareEngines);
    connect(m_engineCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        m_ctrl.setEngineKind(idx == 1 ? EngineKind::DL : EngineKind::Traditional);
        m_paramStack->setCurrentIndex(idx == 1 ? 1 : 0);
        syncParamsFromSettings();
        runDetectionForCurrent();
        updateExportButtons();
        if (m_ctrl.lastBatch(EngineKind::Traditional, m_currentCategory)
            && m_ctrl.lastBatch(EngineKind::DL, m_currentCategory))
            updateCompareTable();
    });

    connect(&m_ctrl, &DetectionController::progressChanged,
            this, &MainWindow::onProgress);
    connect(&m_ctrl, &DetectionController::busyChanged,
            this, &MainWindow::onBusyChanged);
    connect(&m_ctrl, &DetectionController::currentDetectFinished,
            this, &MainWindow::onDetectFinished);
    connect(&m_ctrl, &DetectionController::batchFinished,
            this, &MainWindow::onBatchFinished);
    connect(&m_ctrl, &DetectionController::compareFinished,
            this, &MainWindow::onCompareFinished);

    m_progressBar = new QProgressBar;
    m_progressBar->setMaximumWidth(240);
    m_progressBar->setMaximumHeight(16);
    m_progressBar->setTextVisible(true);
    m_progressBar->setVisible(false);
    statusBar()->addPermanentWidget(m_progressBar);
    statusBar()->showMessage(QStringLiteral("就绪"));

    fillTraditionalWidgets(TraditionalParams::defaults());
    fillDLWidgets(DLParams::defaultsFor(QString()));
}

bool MainWindow::loadDataset(const QString& rootPath)
{
    if (!m_ctrl.loadDataset(rootPath))
        return false;
    populateTree();
    setStatusText(QStringLiteral("数据集：%1").arg(m_ctrl.dataset().rootPath()));
    return true;
}

void MainWindow::populateTree()
{
    m_tree->clear();
    const DatasetManager& dataset = m_ctrl.dataset();
    const QStringList cats = dataset.categories();
    for (const QString& cat : cats) {
        QTreeWidgetItem* catItem = new QTreeWidgetItem(m_tree, {cat});
        catItem->setData(0, Qt::UserRole, QStringLiteral("category"));
        const QStringList defects = dataset.defectTypes(cat);
        for (const QString& defect : defects) {
            QTreeWidgetItem* defectItem = new QTreeWidgetItem(catItem, {defect});
            defectItem->setData(0, Qt::UserRole, QStringLiteral("defect"));
            const QStringList images = dataset.testImages(cat, defect);
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
    QString cat;
    if (role == QStringLiteral("image")) {
        QTreeWidgetItem* defectItem = item->parent();
        QTreeWidgetItem* catItem = defectItem ? defectItem->parent() : nullptr;
        if (!catItem)
            return;
        cat = catItem->text(0);
        m_currentDefectType = defectItem->text(0);
        m_currentImagePath = item->data(0, Qt::UserRole + 1).toString();
        const bool categoryChanged = (cat != m_currentCategory);
        m_currentCategory = cat;
        if (categoryChanged)
            syncParamsFromSettings();
        showImage(m_currentCategory, m_currentDefectType, m_currentImagePath);
        setWorkEnabled(!m_ctrl.isBusy());
        updateExportButtons();
    } else if (role == QStringLiteral("defect") || role == QStringLiteral("category")) {
        QTreeWidgetItem* catItem = (role == QStringLiteral("category")) ? item : item->parent();
        if (catItem) {
            cat = catItem->text(0);
            const bool categoryChanged = (cat != m_currentCategory);
            m_currentCategory = cat;
            if (categoryChanged)
                syncParamsFromSettings();
            setWorkEnabled(!m_ctrl.isBusy());
            updateExportButtons();
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

    m_currentGt.release();
    const QString gtPath = m_ctrl.dataset().groundTruthMask(category, defectType, imagePath);
    if (!gtPath.isEmpty()) {
        cv::Mat gt = cv::imread(gtPath.toLocal8Bit().constData(), cv::IMREAD_GRAYSCALE);
        if (!gt.empty() && gt.size() != m_currentBgr.size())
            cv::resize(gt, gt, m_currentBgr.size(), 0, 0, cv::INTER_NEAREST);
        m_currentGt = gt;
        m_view->setGroundTruthMask(gt);
    }

    m_imageInfoLabel->setText(QStringLiteral("%1 / %2 / %3")
                                  .arg(category, defectType, QFileInfo(imagePath).fileName()));

    m_boxTable->setRowCount(0);
    runDetectionForCurrent();
    onOverlayToggled();
}

void MainWindow::runDetectionForCurrent()
{
    if (m_currentBgr.empty() || m_currentCategory.isEmpty())
        return;
    m_ctrl.prepareAndDetectAsync(m_currentCategory, m_currentBgr);
}

void MainWindow::applyDetectionResult(const DetectionResult& result)
{
    m_currentResult = result;
    m_view->setDetectionOverlay(result.defectMask, result.boxes);

    m_boxTable->setRowCount(static_cast<int>(result.boxes.size()));
    for (size_t i = 0; i < result.boxes.size(); ++i) {
        const cv::Rect& r = result.boxes[i];
        const int row = static_cast<int>(i);
        m_boxTable->setItem(row, 0, new QTableWidgetItem(QString::number(r.x)));
        m_boxTable->setItem(row, 1, new QTableWidgetItem(QString::number(r.y)));
        m_boxTable->setItem(row, 2, new QTableWidgetItem(QString::number(r.width)));
        m_boxTable->setItem(row, 3, new QTableWidgetItem(QString::number(r.height)));
        m_boxTable->setItem(row, 4,
            new QTableWidgetItem(QString::number(result.areas[i])));
    }
    updateExportButtons();
}

void MainWindow::setStatusText(const QString& text)
{
    m_statusLabel->setText(text);
    statusBar()->showMessage(text);
}

void MainWindow::setWorkEnabled(bool enabled)
{
    const bool hasCat = !m_currentCategory.isEmpty();
    m_engineCombo->setEnabled(enabled);
    m_applyParamsButton->setEnabled(enabled);
    m_restoreParamsButton->setEnabled(enabled);
    m_batchButton->setEnabled(enabled && hasCat);
    m_compareButton->setEnabled(enabled && hasCat);
}

void MainWindow::onProgress(int current, int total, const QString& text)
{
    setStatusText(text);
    if (!m_ctrl.isBusy())
        return;
    m_progressBar->setVisible(true);
    if (total <= 0) {
        m_progressBar->setRange(0, 0); // 滚动忙碌条：加载 ONNX / 单张推理
    } else {
        m_progressBar->setRange(0, total);
        m_progressBar->setValue(current);
    }
}

void MainWindow::onBusyChanged(bool busy)
{
    setWorkEnabled(!busy);
    if (busy) {
        m_progressBar->setVisible(true);
        QApplication::setOverrideCursor(Qt::BusyCursor);
    } else {
        m_progressBar->setRange(0, 1);
        m_progressBar->setValue(1);
        m_progressBar->setVisible(false);
        QApplication::restoreOverrideCursor();
    }
}

void MainWindow::onDetectFinished(bool ok, const DetectionResult& result)
{
    if (!ok) {
        m_currentResult = {};
        m_boxTable->setRowCount(0);
        m_view->setDetectionOverlay({}, {});
        updateExportButtons();
        setStatusText(QStringLiteral("无法加载 %1 的检测引擎（ONNX 缺失或良品图不可读）")
                          .arg(m_currentCategory));
        return;
    }
    applyDetectionResult(result);
}

void MainWindow::onBatchFinished(bool ok, const QString& category)
{
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法完成 %1 批量检测（train/good 为空或 ONNX 缺失）")
                                 .arg(category));
        return;
    }
    if (const BatchMetrics* metrics = m_ctrl.lastBatch(currentKind(), category))
        updateMetricsTable(metrics->pixel, metrics->image);
    updateExportButtons();
    if (m_ctrl.lastBatch(EngineKind::Traditional, category)
        && m_ctrl.lastBatch(EngineKind::DL, category))
        updateCompareTable();
    setStatusText(QStringLiteral("已完成 %1 批量检测（%2）")
                      .arg(category, currentEngineName()));
}

void MainWindow::onCompareFinished(bool ok, const QString& category)
{
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("双引擎对比失败（某一侧模型或良品图不可用）"));
        return;
    }
    if (const BatchMetrics* cur = m_ctrl.lastBatch(currentKind(), category))
        updateMetricsTable(cur->pixel, cur->image);
    updateCompareTable();
    updateExportButtons();
    setStatusText(QStringLiteral("已完成 %1 双引擎对比").arg(category));
}

void MainWindow::onOverlayToggled()
{
    m_view->setGtOverlayVisible(m_gtOverlayCheck->isChecked());
    m_view->setDetectionOverlayVisible(m_detOverlayCheck->isChecked());
}

void MainWindow::onRunBatch()
{
    if (m_currentCategory.isEmpty() || m_ctrl.isBusy())
        return;
    m_ctrl.runBatchAsync(m_currentCategory);
}

void MainWindow::updateMetricsTable(const QMap<QString, PixelMetrics>& pixel,
                                    const QMap<QString, ImageMetrics>& image)
{
    const QStringList defects = pixel.keys();
    m_metricsTable->setRowCount(defects.size());
    int row = 0;
    for (const QString& defect : defects) {
        const PixelMetrics& p = pixel[defect];
        const ImageMetrics& im = image[defect];
        m_metricsTable->setItem(row, 0, new QTableWidgetItem(defect));
        m_metricsTable->setItem(row, 1, new QTableWidgetItem(fmt3(p.precision())));
        m_metricsTable->setItem(row, 2, new QTableWidgetItem(fmt3(p.recall())));
        m_metricsTable->setItem(row, 3, new QTableWidgetItem(fmt3(p.f1())));
        m_metricsTable->setItem(row, 4, new QTableWidgetItem(fmt3(p.iou())));
        m_metricsTable->setItem(row, 5, new QTableWidgetItem(imageAccText(im)));
        ++row;
    }
}

EngineKind MainWindow::currentKind() const
{
    return m_engineCombo->currentIndex() == 1 ? EngineKind::DL : EngineKind::Traditional;
}

QString MainWindow::currentEngineName() const
{
    return currentKind() == EngineKind::DL ? QStringLiteral("dl") : QStringLiteral("cv");
}

void MainWindow::updateExportButtons()
{
    m_exportCurrentButton->setEnabled(!m_currentBgr.empty());
    const bool hasBatch = m_ctrl.lastBatch(currentKind(), m_currentCategory) != nullptr;
    m_exportBatchButton->setEnabled(hasBatch);
}

TraditionalParams MainWindow::traditionalParamsFromWidgets() const
{
    TraditionalParams p;
    p.zAggThreshold = m_cvZThresh->value();
    p.morphCloseKernel = m_cvMorph->value();
    p.minDefectArea = m_cvMinArea->value();
    p.imageLevelMinArea = m_cvImageArea->value();
    return p;
}

DLParams MainWindow::dlParamsFromWidgets() const
{
    DLParams p;
    p.thresholdSigma = m_dlSigma->value();
    p.morphCloseKernel = m_dlMorph->value();
    p.minDefectArea = m_dlMinArea->value();
    p.imageLevelMinArea = m_dlImageArea->value();
    return p;
}

void MainWindow::fillTraditionalWidgets(const TraditionalParams& p)
{
    m_cvZThresh->setValue(p.zAggThreshold);
    m_cvMorph->setValue(p.morphCloseKernel);
    m_cvMinArea->setValue(p.minDefectArea);
    m_cvImageArea->setValue(p.imageLevelMinArea);
}

void MainWindow::fillDLWidgets(const DLParams& p)
{
    m_dlSigma->setValue(p.thresholdSigma);
    m_dlMorph->setValue(p.morphCloseKernel);
    m_dlMinArea->setValue(p.minDefectArea);
    m_dlImageArea->setValue(p.imageLevelMinArea);
}

TraditionalParams MainWindow::loadTraditionalSettings(const QString& category) const
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

DLParams MainWindow::loadDLSettings(const QString& category) const
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

void MainWindow::saveParamsToSettings()
{
    if (m_currentCategory.isEmpty())
        return;
    QSettings s;
    if (currentKind() == EngineKind::DL) {
        const DLParams p = dlParamsFromWidgets();
        s.beginGroup(QStringLiteral("params/dl/%1").arg(m_currentCategory));
        s.setValue(QStringLiteral("thresholdSigma"), p.thresholdSigma);
        s.setValue(QStringLiteral("morphCloseKernel"), p.morphCloseKernel);
        s.setValue(QStringLiteral("minDefectArea"), p.minDefectArea);
        s.setValue(QStringLiteral("imageLevelMinArea"), p.imageLevelMinArea);
    } else {
        const TraditionalParams p = traditionalParamsFromWidgets();
        s.beginGroup(QStringLiteral("params/cv/%1").arg(m_currentCategory));
        s.setValue(QStringLiteral("zAggThreshold"), p.zAggThreshold);
        s.setValue(QStringLiteral("morphCloseKernel"), p.morphCloseKernel);
        s.setValue(QStringLiteral("minDefectArea"), p.minDefectArea);
        s.setValue(QStringLiteral("imageLevelMinArea"), p.imageLevelMinArea);
    }
}

void MainWindow::syncParamsFromSettings()
{
    if (m_currentCategory.isEmpty())
        return;
    const TraditionalParams cv = loadTraditionalSettings(m_currentCategory);
    fillTraditionalWidgets(cv);
    m_ctrl.setTraditionalParams(m_currentCategory, cv);
    const DLParams dl = loadDLSettings(m_currentCategory);
    fillDLWidgets(dl);
    m_ctrl.setDLParams(m_currentCategory, dl);
    m_paramStack->setCurrentIndex(currentKind() == EngineKind::DL ? 1 : 0);
}

void MainWindow::onApplyParams()
{
    if (m_currentCategory.isEmpty())
        return;
    if (currentKind() == EngineKind::DL)
        m_ctrl.setDLParams(m_currentCategory, dlParamsFromWidgets());
    else
        m_ctrl.setTraditionalParams(m_currentCategory, traditionalParamsFromWidgets());
    saveParamsToSettings();
    runDetectionForCurrent();
    setStatusText(QStringLiteral("已应用 %1 / %2 参数")
                      .arg(m_currentCategory, currentEngineName()));
}

void MainWindow::onRestoreParams()
{
    if (m_currentCategory.isEmpty())
        return;
    if (currentKind() == EngineKind::DL) {
        const DLParams p = DLParams::defaultsFor(m_currentCategory);
        fillDLWidgets(p);
        m_ctrl.setDLParams(m_currentCategory, p);
    } else {
        const TraditionalParams p = TraditionalParams::defaults();
        fillTraditionalWidgets(p);
        m_ctrl.setTraditionalParams(m_currentCategory, p);
    }
    saveParamsToSettings();
    runDetectionForCurrent();
    setStatusText(QStringLiteral("已恢复 %1 / %2 的 P2 默认工作点")
                      .arg(m_currentCategory, currentEngineName()));
}

void MainWindow::onExportCurrent()
{
    if (m_currentBgr.empty())
        return;
    const QString defName = QStringLiteral("%1_%2_%3_%4.png")
                                .arg(m_currentCategory, m_currentDefectType,
                                     QFileInfo(m_currentImagePath).completeBaseName(),
                                     currentEngineName());
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("导出当前标注图"), defName,
        QStringLiteral("PNG (*.png)"));
    if (path.isEmpty())
        return;
    const cv::Mat annotated = ResultExporter::composeAnnotated(
        m_currentBgr, m_currentResult.defectMask, m_currentResult.boxes, m_currentGt);
    if (!ResultExporter::saveImage(path, annotated)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("写出失败：%1").arg(path));
        return;
    }
    setStatusText(QStringLiteral("已导出 %1").arg(path));
}

void MainWindow::onExportBatch()
{
    const BatchMetrics* metrics = m_ctrl.lastBatch(currentKind(), m_currentCategory);
    if (!metrics) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请先对当前引擎跑完该类别的批量检测"));
        return;
    }
    const QString defDir = QDir::current().filePath(
        QStringLiteral("export_%1_%2").arg(m_currentCategory, currentEngineName()));
    QDir().mkpath(defDir); // 对话框需要已存在的目录；先建好建议路径
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择导出目录"), defDir);
    if (dir.isEmpty())
        return;
    if (!ResultExporter::exportBatch(dir, m_currentCategory, currentEngineName(),
                                     m_ctrl.dataset(), *metrics)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("导出失败：%1").arg(dir));
        return;
    }
    setStatusText(QStringLiteral("已导出批量结果到 %1").arg(dir));
}

void MainWindow::onCompareEngines()
{
    if (m_currentCategory.isEmpty() || m_ctrl.isBusy())
        return;
    m_ctrl.compareAsync(m_currentCategory);
}

void MainWindow::updateCompareTable()
{
    const BatchMetrics* cv = m_ctrl.lastBatch(EngineKind::Traditional, m_currentCategory);
    const BatchMetrics* dl = m_ctrl.lastBatch(EngineKind::DL, m_currentCategory);
    if (!cv || !dl) {
        m_compareTable->setRowCount(0);
        return;
    }

    QStringList keys = cv->pixel.keys();
    for (const QString& k : dl->pixel.keys()) {
        if (!keys.contains(k))
            keys.append(k);
    }
    keys.sort();

    auto putRow = [this](int row, const QString& name,
                         const PixelMetrics& cp, const ImageMetrics& ci,
                         const PixelMetrics& dp, const ImageMetrics& di) {
        m_compareTable->setItem(row, 0, new QTableWidgetItem(name));
        m_compareTable->setItem(row, 1, new QTableWidgetItem(fmt3(cp.precision())));
        m_compareTable->setItem(row, 2, new QTableWidgetItem(fmt3(cp.recall())));
        m_compareTable->setItem(row, 3, new QTableWidgetItem(fmt3(cp.f1())));
        m_compareTable->setItem(row, 4, new QTableWidgetItem(imageAccText(ci)));
        m_compareTable->setItem(row, 5, new QTableWidgetItem(fmt3(dp.precision())));
        m_compareTable->setItem(row, 6, new QTableWidgetItem(fmt3(dp.recall())));
        m_compareTable->setItem(row, 7, new QTableWidgetItem(fmt3(dp.f1())));
        m_compareTable->setItem(row, 8, new QTableWidgetItem(imageAccText(di)));
        m_compareTable->setItem(row, 9, new QTableWidgetItem(fmt3(dp.f1() - cp.f1())));
    };

    PixelMetrics cvTotal, dlTotal;
    ImageMetrics cvImg, dlImg;
    m_compareTable->setRowCount(keys.size() + 2);
    int row = 0;
    for (const QString& defect : keys) {
        const PixelMetrics cp = cv->pixel.value(defect);
        const ImageMetrics ci = cv->image.value(defect);
        const PixelMetrics dp = dl->pixel.value(defect);
        const ImageMetrics di = dl->image.value(defect);
        cvTotal += cp;
        dlTotal += dp;
        cvImg += ci;
        dlImg += di;
        putRow(row, defect, cp, ci, dp, di);
        ++row;
    }
    putRow(row, QStringLiteral("汇总"), cvTotal, cvImg, dlTotal, dlImg);
    ++row;

    const ImageMetrics cvGood = cv->image.value(QStringLiteral("good"));
    const ImageMetrics dlGood = dl->image.value(QStringLiteral("good"));
    m_compareTable->setItem(row, 0, new QTableWidgetItem(QStringLiteral("good 误报率")));
    for (int c = 1; c <= 8; ++c)
        m_compareTable->setItem(row, c, new QTableWidgetItem(QStringLiteral("—")));
    if (cvGood.total > 0)
        m_compareTable->setItem(row, 4, new QTableWidgetItem(
            QStringLiteral("%1 (%2/%3)")
                .arg(fmt3(1.0 - cvGood.accuracy()))
                .arg(cvGood.total - cvGood.correct)
                .arg(cvGood.total)));
    if (dlGood.total > 0)
        m_compareTable->setItem(row, 8, new QTableWidgetItem(
            QStringLiteral("%1 (%2/%3)")
                .arg(fmt3(1.0 - dlGood.accuracy()))
                .arg(dlGood.total - dlGood.correct)
                .arg(dlGood.total)));
    m_compareTable->setItem(row, 9, new QTableWidgetItem(QStringLiteral("")));
}
