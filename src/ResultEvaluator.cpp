#include "ResultEvaluator.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

namespace {

// 读掩码并二值化为 0/1；读不到或尺寸不符时返回空 Mat
cv::Mat loadBinaryMask(const QString& path, const cv::Size& size)
{
    if (path.isEmpty())
        return cv::Mat();
    cv::Mat m = cv::imread(path.toLocal8Bit().constData(), cv::IMREAD_GRAYSCALE);
    if (m.empty())
        return cv::Mat();
    if (m.size() != size)
        cv::resize(m, m, size, 0, 0, cv::INTER_NEAREST);
    cv::threshold(m, m, 127, 1.0, cv::THRESH_BINARY);
    m.convertTo(m, CV_8U);
    return m;
}

} // namespace

PixelMetrics ResultEvaluator::evaluatePixel(const cv::Mat& defectMask, const QString& gtMaskPath)
{
    PixelMetrics r;

    cv::Mat predBin;
    if (!defectMask.empty()) {
        cv::threshold(defectMask, predBin, 127, 1.0, cv::THRESH_BINARY);
        predBin.convertTo(predBin, CV_8U);
    } else {
        predBin = cv::Mat::zeros(1, 1, CV_8U);
    }

    cv::Mat gtBin = loadBinaryMask(gtMaskPath, predBin.size());
    if (gtBin.empty())
        gtBin = cv::Mat::zeros(predBin.size(), CV_8U); // good 类：GT 全零

    cv::Mat tp, fp, fn;
    cv::bitwise_and(predBin, gtBin, tp);
    cv::bitwise_and(predBin, ~gtBin, fp);
    cv::bitwise_and(~predBin, gtBin, fn);

    r.tp = cv::countNonZero(tp);
    r.fp = cv::countNonZero(fp);
    r.fn = cv::countNonZero(fn);
    r.tn = predBin.total() - static_cast<size_t>(r.tp + r.fp + r.fn);
    return r;
}

PixelMetrics& PixelMetrics::operator+=(const PixelMetrics& o)
{
    tp += o.tp;
    fp += o.fp;
    fn += o.fn;
    tn += o.tn;
    return *this;
}

double PixelMetrics::precision() const
{
    const long long d = tp + fp;
    return d > 0 ? static_cast<double>(tp) / d : 0.0;
}

double PixelMetrics::recall() const
{
    const long long d = tp + fn;
    return d > 0 ? static_cast<double>(tp) / d : 0.0;
}

double PixelMetrics::f1() const
{
    const double p = precision(), r = recall();
    return (p + r) > 0.0 ? 2.0 * p * r / (p + r) : 0.0;
}

double PixelMetrics::iou() const
{
    const long long d = tp + fp + fn;
    return d > 0 ? static_cast<double>(tp) / d : 0.0;
}

double ImageMetrics::accuracy() const
{
    return total > 0 ? static_cast<double>(correct) / total : 0.0;
}

ImageMetrics& ImageMetrics::operator+=(const ImageMetrics& o)
{
    total += o.total;
    correct += o.correct;
    return *this;
}
