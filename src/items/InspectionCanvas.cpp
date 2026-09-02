#include "InspectionCanvas.h"

#include "OverlayColors.h"

#include <QCursor>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QWheelEvent>
#include <QtMath>

InspectionCanvas::InspectionCanvas(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
    setFillColor(QColor(QStringLiteral("#0B0F14")));
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
    setAntialiasing(true);
    setOpaquePainting(true);
}

void InspectionCanvas::setImage(const QImage& image)
{
    if (m_image == image)
        return;
    m_image = image;
    m_needFit = true;
    if (width() > 1.0 && height() > 1.0)
        fitView();
    emit imageChanged();
    update();
}

void InspectionCanvas::setGtOverlay(const QImage& image)
{
    if (m_gt == image)
        return;
    m_gt = image;
    emit gtOverlayChanged();
    update();
}

void InspectionCanvas::setDetOverlay(const QImage& image)
{
    if (m_det == image)
        return;
    m_det = image;
    emit detOverlayChanged();
    update();
}

void InspectionCanvas::setBoxes(const QVariantList& boxes)
{
    if (m_boxes == boxes)
        return;
    m_boxes = boxes;
    emit boxesChanged();
    update();
}

void InspectionCanvas::setGtVisible(bool visible)
{
    if (m_gtVisible == visible)
        return;
    m_gtVisible = visible;
    emit gtVisibleChanged();
    update();
}

void InspectionCanvas::setDetVisible(bool visible)
{
    if (m_detVisible == visible)
        return;
    m_detVisible = visible;
    emit detVisibleChanged();
    update();
}

void InspectionCanvas::setZoom(qreal zoom)
{
    zoom = qBound(0.05, zoom, 20.0);
    if (qFuzzyCompare(m_zoom, zoom))
        return;
    m_zoom = zoom;
    emit zoomChanged();
}

void InspectionCanvas::fitView()
{
    if (m_image.isNull() || width() <= 1.0 || height() <= 1.0)
        return;
    const qreal zx = width() / qreal(m_image.width());
    const qreal zy = height() / qreal(m_image.height());
    setZoom(qMin(zx, zy));
    m_offset = QPointF((width() - m_image.width() * m_zoom) * 0.5,
                       (height() - m_image.height() * m_zoom) * 0.5);
    m_needFit = false;
    update();
}

QPointF InspectionCanvas::imagePoint(const QPointF& itemPos) const
{
    if (m_zoom <= 0.0)
        return {};
    return (itemPos - m_offset) / m_zoom;
}

void InspectionCanvas::paint(QPainter* painter)
{
    painter->fillRect(boundingRect(), QColor(QStringLiteral("#0B0F14")));
    if (m_image.isNull())
        return;

    painter->save();
    painter->translate(m_offset);
    painter->scale(m_zoom, m_zoom);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, m_zoom < 1.5);
    painter->drawImage(0, 0, m_image);
    if (m_gtVisible && !m_gt.isNull())
        painter->drawImage(0, 0, m_gt);
    if (m_detVisible) {
        if (!m_det.isNull())
            painter->drawImage(0, 0, m_det);
        QPen pen(OverlayColors::detBox);
        pen.setWidthF(qMax(1.0, 2.0 / m_zoom));
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        for (const QVariant& v : m_boxes) {
            const QVariantMap m = v.toMap();
            painter->drawRect(QRectF(m.value(QStringLiteral("x")).toDouble(),
                                     m.value(QStringLiteral("y")).toDouble(),
                                     m.value(QStringLiteral("width")).toDouble(),
                                     m.value(QStringLiteral("height")).toDouble()));
        }
    }
    painter->restore();
}

void InspectionCanvas::wheelEvent(QWheelEvent* event)
{
    if (m_image.isNull()) {
        event->ignore();
        return;
    }
    const qreal factor = (event->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
    const QPointF mouse = event->position();
    const QPointF img = imagePoint(mouse);
    setZoom(m_zoom * factor);
    m_offset = mouse - img * m_zoom;
    m_needFit = false;
    update();
    event->accept();
}

void InspectionCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_panning = true;
        m_lastPos = event->position();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    event->ignore();
}

void InspectionCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (m_panning) {
        m_offset += event->position() - m_lastPos;
        m_lastPos = event->position();
        m_needFit = false;
        update();
        event->accept();
        return;
    }
    event->ignore();
}

void InspectionCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_panning) {
        m_panning = false;
        unsetCursor();
        event->accept();
        return;
    }
    event->ignore();
}

void InspectionCanvas::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    if (m_needFit)
        fitView();
}
