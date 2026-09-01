#pragma once

#include <QGraphicsView>

#include <opencv2/core.hpp>

class QGraphicsPixmapItem;
class QGraphicsRectItem;
class QWheelEvent;
class QMouseEvent;

// 图像查看器：QGraphicsView + QGraphicsPixmapItem。
// 滚轮缩放、左键拖拽平移；支持叠加 GT 掩码（红色半透明）与
// 检测结果（绿色框 + 缺陷掩码热区），可分别开关。
class ImageViewWidget : public QGraphicsView
{
    Q_OBJECT

public:
    explicit ImageViewWidget(QWidget* parent = nullptr);

    // 显示一张原图（BGR cv::Mat；会复制，调用者可复用缓冲）。
    // 清空旧叠加层。
    void setImage(const cv::Mat& bgrImage);

    // 叠加 GT 掩码（0/255 8UC1，红色半透明）。空 Mat 清除。
    void setGroundTruthMask(const cv::Mat& mask);

    // 叠加检测结果：缺陷掩码热区 + 外接框（绿色）。
    void setDetectionOverlay(const cv::Mat& mask, const std::vector<cv::Rect>& boxes);

    void setGtOverlayVisible(bool visible);
    void setDetectionOverlayVisible(bool visible);
    bool gtOverlayVisible() const;
    bool detectionOverlayVisible() const;

    void resetView();

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    // 把 0/255 掩码转成 RGBA 半透明叠加层
    static QImage maskToOverlay(const cv::Mat& mask, const QColor& color, int alpha);

    QGraphicsScene* m_scene = nullptr;
    QGraphicsPixmapItem* m_baseItem = nullptr;
    QGraphicsPixmapItem* m_gtItem = nullptr;
    QGraphicsPixmapItem* m_detMaskItem = nullptr;
    std::vector<QGraphicsRectItem*> m_boxItems;

    bool m_panning = false;
    QPoint m_lastMousePos;
};
