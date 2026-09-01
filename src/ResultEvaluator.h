#pragma once

#include <opencv2/core.hpp>

#include <QString>

// 像素级指标累积器：直接累加 TP/FP/FN/TN，指标在查询时计算
struct PixelMetrics
{
    long long tp = 0;
    long long fp = 0;
    long long fn = 0;
    long long tn = 0;

    double precision() const;
    double recall() const;
    double f1() const;
    double iou() const;

    PixelMetrics& operator+=(const PixelMetrics& o);
};

// 图像级指标累积器：total 张图中 correct 张判定正确
// （缺陷类有检出 = 正确；good 类无检出 = 正确）
struct ImageMetrics
{
    long long total = 0;
    long long correct = 0;

    double accuracy() const;

    ImageMetrics& operator+=(const ImageMetrics& o);
};

class ResultEvaluator
{
public:
    // 检测掩码（8UC1，0/255，空 Mat 视为全零）vs GT 掩码文件（空路径视为全零，即 good 类）
    static PixelMetrics evaluatePixel(const cv::Mat& defectMask, const QString& gtMaskPath);

    // 图像级判定：是否检出 vs 是否缺陷类
    static ImageMetrics evaluateImage(bool detected, bool isDefectClass)
    {
        ImageMetrics m;
        m.total = 1;
        m.correct = (detected == isDefectClass) ? 1 : 0;
        return m;
    }
};
