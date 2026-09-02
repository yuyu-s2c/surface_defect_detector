#include "ImageConvert.h"

#include <opencv2/imgproc.hpp>

QImage bgrToQImage(const cv::Mat& bgr)
{
    if (bgr.empty())
        return {};
    cv::Mat rgba;
    cv::cvtColor(bgr, rgba, cv::COLOR_BGR2RGBA);
    return QImage(rgba.data, rgba.cols, rgba.rows, static_cast<int>(rgba.step),
                  QImage::Format_RGBA8888)
        .copy();
}

QImage maskToOverlayImage(const cv::Mat& mask, const QColor& color, int alpha)
{
    if (mask.empty() || mask.type() != CV_8UC1)
        return {};
    QImage overlay(mask.cols, mask.rows, QImage::Format_ARGB32);
    overlay.fill(Qt::transparent);
    const QRgb pixel = qRgba(color.red(), color.green(), color.blue(), alpha);
    for (int y = 0; y < mask.rows; ++y) {
        const uchar* row = mask.ptr<uchar>(y);
        QRgb* out = reinterpret_cast<QRgb*>(overlay.scanLine(y));
        for (int x = 0; x < mask.cols; ++x) {
            if (row[x] != 0)
                out[x] = pixel;
        }
    }
    return overlay;
}
