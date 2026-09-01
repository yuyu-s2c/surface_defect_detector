#include "ImageViewWidget.h"

#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QPen>
#include <QScrollBar>

#include <opencv2/imgproc.hpp>

ImageViewWidget::ImageViewWidget(QWidget* parent)
    : QGraphicsView(parent)
    , m_scene(new QGraphicsScene(this))
{
    setScene(m_scene);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::NoDrag); // 自己实现左键平移
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setBackgroundBrush(QColor(40, 40, 40));
}

void ImageViewWidget::setImage(const cv::Mat& bgrImage)
{
    m_scene->clear();
    m_baseItem = m_gtItem = m_detMaskItem = nullptr;
    m_boxItems.clear();
    m_panning = false;

    if (bgrImage.empty())
        return;

    // BGR -> RGBA 拷贝给 QImage（垫一层 own 数据，避免悬空）
    cv::Mat rgba;
    cv::cvtColor(bgrImage, rgba, cv::COLOR_BGR2RGBA);
    QImage img(rgba.data, rgba.cols, rgba.rows, static_cast<int>(rgba.step),
               QImage::Format_RGBA8888);
    m_baseItem = m_scene->addPixmap(QPixmap::fromImage(img.copy()));
    m_scene->setSceneRect(m_baseItem->boundingRect());
    resetView();
}

QImage ImageViewWidget::maskToOverlay(const cv::Mat& mask, const QColor& color, int alpha)
{
    QImage overlay(mask.cols, mask.rows, QImage::Format_ARGB32);
    overlay.fill(Qt::transparent);
    for (int y = 0; y < mask.rows; ++y) {
        const uchar* row = mask.ptr<uchar>(y);
        QRgb* out = reinterpret_cast<QRgb*>(overlay.scanLine(y));
        for (int x = 0; x < mask.cols; ++x) {
            if (row[x] != 0)
                out[x] = qRgba(color.red(), color.green(), color.blue(), alpha);
        }
    }
    return overlay;
}

void ImageViewWidget::setGroundTruthMask(const cv::Mat& mask)
{
    if (m_gtItem) {
        m_scene->removeItem(m_gtItem);
        delete m_gtItem;
        m_gtItem = nullptr;
    }
    if (mask.empty() || mask.type() != CV_8UC1)
        return;
    m_gtItem = m_scene->addPixmap(
        QPixmap::fromImage(maskToOverlay(mask, QColor(255, 0, 0), 110)));
    m_gtItem->setZValue(1);
}

void ImageViewWidget::setDetectionOverlay(const cv::Mat& mask, const std::vector<cv::Rect>& boxes)
{
    if (m_detMaskItem) {
        m_scene->removeItem(m_detMaskItem);
        delete m_detMaskItem;
        m_detMaskItem = nullptr;
    }
    for (QGraphicsRectItem* item : m_boxItems) {
        m_scene->removeItem(item);
        delete item;
    }
    m_boxItems.clear();

    if (!mask.empty() && mask.type() == CV_8UC1) {
        m_detMaskItem = m_scene->addPixmap(
            QPixmap::fromImage(maskToOverlay(mask, QColor(0, 220, 0), 90)));
        m_detMaskItem->setZValue(2);
    }
    QPen pen(QColor(0, 255, 0));
    pen.setWidth(2);
    pen.setCosmetic(true);
    for (const cv::Rect& r : boxes) {
        QGraphicsRectItem* item = m_scene->addRect(r.x, r.y, r.width, r.height, pen);
        item->setZValue(3);
        m_boxItems.push_back(item);
    }
}

void ImageViewWidget::setGtOverlayVisible(bool visible)
{
    if (m_gtItem)
        m_gtItem->setVisible(visible);
}

void ImageViewWidget::setDetectionOverlayVisible(bool visible)
{
    if (m_detMaskItem)
        m_detMaskItem->setVisible(visible);
    for (QGraphicsRectItem* item : m_boxItems)
        item->setVisible(visible);
}

bool ImageViewWidget::gtOverlayVisible() const
{
    return m_gtItem && m_gtItem->isVisible();
}

bool ImageViewWidget::detectionOverlayVisible() const
{
    return m_detMaskItem && m_detMaskItem->isVisible();
}

void ImageViewWidget::resetView()
{
    if (m_scene->sceneRect().isValid())
        fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
}

void ImageViewWidget::wheelEvent(QWheelEvent* event)
{
    const double factor = (event->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
    scale(factor, factor);
    event->accept();
}

void ImageViewWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_panning = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void ImageViewWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_panning) {
        const QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void ImageViewWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_panning) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}
