#pragma once

#include <QColor>
#include <QImage>

#include <opencv2/core.hpp>

// cv::Mat ↔ QImage，只给 ViewModel / Canvas 用，不进 QML。
QImage bgrToQImage(const cv::Mat& bgr);
QImage maskToOverlayImage(const cv::Mat& mask, const QColor& color, int alpha);
