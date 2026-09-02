#pragma once

#include <QImage>
#include <QPointF>
#include <QQuickPaintedItem>
#include <QVariantList>
#include <QtQml>

// 检测画布：原图 + GT/检测叠加 + 绿框。滚轮缩放（锚鼠标）、左键拖拽平移。
class InspectionCanvas : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QImage image READ image WRITE setImage NOTIFY imageChanged)
    Q_PROPERTY(QImage gtOverlay READ gtOverlay WRITE setGtOverlay NOTIFY gtOverlayChanged)
    Q_PROPERTY(QImage detOverlay READ detOverlay WRITE setDetOverlay NOTIFY detOverlayChanged)
    Q_PROPERTY(QVariantList boxes READ boxes WRITE setBoxes NOTIFY boxesChanged)
    Q_PROPERTY(bool gtVisible READ gtVisible WRITE setGtVisible NOTIFY gtVisibleChanged)
    Q_PROPERTY(bool detVisible READ detVisible WRITE setDetVisible NOTIFY detVisibleChanged)
    Q_PROPERTY(qreal zoom READ zoom NOTIFY zoomChanged)

public:
    explicit InspectionCanvas(QQuickItem* parent = nullptr);

    QImage image() const { return m_image; }
    void setImage(const QImage& image);

    QImage gtOverlay() const { return m_gt; }
    void setGtOverlay(const QImage& image);

    QImage detOverlay() const { return m_det; }
    void setDetOverlay(const QImage& image);

    QVariantList boxes() const { return m_boxes; }
    void setBoxes(const QVariantList& boxes);

    bool gtVisible() const { return m_gtVisible; }
    void setGtVisible(bool visible);

    bool detVisible() const { return m_detVisible; }
    void setDetVisible(bool visible);

    qreal zoom() const { return m_zoom; }

    Q_INVOKABLE void fitView();

    void paint(QPainter* painter) override;

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

signals:
    void imageChanged();
    void gtOverlayChanged();
    void detOverlayChanged();
    void boxesChanged();
    void gtVisibleChanged();
    void detVisibleChanged();
    void zoomChanged();

private:
    void setZoom(qreal zoom);
    QPointF imagePoint(const QPointF& itemPos) const;

    QImage m_image;
    QImage m_gt;
    QImage m_det;
    QVariantList m_boxes;
    bool m_gtVisible = true;
    bool m_detVisible = true;
    qreal m_zoom = 1.0;
    QPointF m_offset;
    bool m_panning = false;
    QPointF m_lastPos;
    bool m_needFit = true;
};
