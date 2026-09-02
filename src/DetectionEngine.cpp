#include "DetectionEngine.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

cv::Mat DetectionEngine::toGray(const cv::Mat& image)
{
    if (image.channels() == 1)
        return image;
    cv::Mat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    return gray;
}

bool DetectionEngine::buildReference(const QStringList& goodImagePaths)
{
    m_referenceMean.release();
    m_referenceStd.release();
    if (goodImagePaths.isEmpty())
        return false;

    // 两遍累加求逐像素均值与标准差（CV_64F 累加防溢出）
    cv::Mat sum, sumSq;
    cv::Size size;
    int n = 0;
    for (const QString& p : goodImagePaths) {
        cv::Mat img = cv::imread(p.toLocal8Bit().constData(), cv::IMREAD_GRAYSCALE);
        if (img.empty())
            continue;
        if (n == 0) {
            size = img.size();
            sum = cv::Mat::zeros(size, CV_64F);
            sumSq = cv::Mat::zeros(size, CV_64F);
        } else if (img.size() != size) {
            cv::resize(img, img, size);
        }
        cv::Mat f;
        img.convertTo(f, CV_64F);
        sum += f;
        cv::multiply(f, f, f);
        sumSq += f;
        ++n;
    }
    if (n == 0)
        return false;

    cv::Mat mean = sum / n;
    cv::Mat var = sumSq / n - mean.mul(mean);
    cv::max(var, 0.0, var); // 浮点误差可能产生微小负值
    cv::Mat stddev;
    cv::sqrt(var, stddev);

    const int k = gaussianKernel | 1; // 保证奇数
    cv::GaussianBlur(mean, mean, cv::Size(k, k), 0);

    mean.convertTo(m_referenceMean, CV_32F);
    stddev.convertTo(m_referenceStd, CV_32F);
    m_templateSize = m_referenceMean.size();
    return true;
}

DetectionResult DetectionEngine::detect(const cv::Mat& image) const
{
    DetectionResult result;
    if (!hasReference() || image.empty())
        return result;

    // 1. 灰度 + 统一尺寸 + 高斯预滤波
    cv::Mat gray = toGray(image);
    if (gray.size() != m_templateSize)
        cv::resize(gray, gray, m_templateSize);
    const int k = gaussianKernel | 1;
    cv::GaussianBlur(gray, gray, cv::Size(k, k), 0);

    // 2. z-score：|x - mean| / (std + eps)。纹理区良品波动大（std 大），
    //    z 自动回落；平坦区 std 小，轻微异常也会得到高 z。
    //    注意：本管线假设测试图与参考模型视角基本对齐（metal_nut 成立）。
    //    screw 类存在旋转差异，差分会沿边缘产生大量误报，首版接受该效果，
    //    后续可在此加入特征配准（ORB + 单应性估计）再差分。
    cv::Mat f;
    gray.convertTo(f, CV_32F);
    cv::Mat z = cv::abs(f - m_referenceMean) / (m_referenceStd + stdEps);

    // 3. 空间聚合：51×51 窗口均值。纹理噪声是颗粒状高频，聚合后回落；
    //    划痕/变形/变色是成片异常，聚合后仍显著。
    cv::Mat agg;
    const int w = aggWindow | 1;
    cv::boxFilter(z, agg, CV_32F, cv::Size(w, w), cv::Point(-1, -1), true,
                  cv::BORDER_REPLICATE);

    // 4. 阈值 + 形态学：闭运算把邻近缺陷像素连成片，开运算去零星噪点
    cv::Mat bin;
    cv::threshold(agg, bin, zAggThreshold, 255, cv::THRESH_BINARY);
    bin.convertTo(bin, CV_8U);
    const int ck = morphCloseKernel | 1;
    const cv::Mat closeKernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(ck, ck));
    cv::morphologyEx(bin, bin, cv::MORPH_CLOSE, closeKernel);
    const cv::Mat openKernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(bin, bin, cv::MORPH_OPEN, openKernel);

    // 5. 连通域分析，按面积过滤
    cv::Mat labels, stats, centroids;
    const int count = cv::connectedComponentsWithStats(bin, labels, stats, centroids, 8, CV_32S);
    for (int i = 1; i < count; ++i) { // 0 是背景
        const int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area < minDefectArea)
            continue;
        result.boxes.emplace_back(stats.at<int>(i, cv::CC_STAT_LEFT),
                                  stats.at<int>(i, cv::CC_STAT_TOP),
                                  stats.at<int>(i, cv::CC_STAT_WIDTH),
                                  stats.at<int>(i, cv::CC_STAT_HEIGHT));
        result.areas.push_back(static_cast<double>(area));
        result.totalArea += area;
    }

    result.minImageArea = imageLevelMinArea;
    result.defectMask = bin;
    return result;
}
