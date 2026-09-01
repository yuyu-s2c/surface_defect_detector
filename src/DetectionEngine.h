#pragma once

#include <opencv2/core.hpp>

#include <QString>
#include <QStringList>
#include <vector>

// 单张图的检测结果
struct DetectionResult
{
    cv::Mat defectMask;                 // 8UC1，0/255 缺陷掩码
    std::vector<cv::Rect> boxes;        // 缺陷外接框
    std::vector<double> areas;          // 各缺陷像素面积
    double totalArea = 0.0;             // 缺陷总像素数（连通域过滤后）
    // 图像级检出判定：缺陷总面积达到阈值才算检出（零散噪声不报警）
    bool detected(int minTotalArea = 1000) const { return totalArea >= minTotalArea; }
};

// 传统 OpenCV 检测引擎。接口保持稳定，便于以后替换为深度学习实现。
//
// 管线：灰度 -> 与良品统计模型求差 -> z-score 归一 -> 空间聚合 -> 阈值
//       -> 形态学 -> 连通域按面积过滤。
//
// 为什么不是简单的"absdiff + Otsu"：
// metal_nut 良品之间的表面纹理随机且差异大（实测任意良品图与中值模板
// 约 20% 像素 |diff|>30，且空间上成片），Otsu 会强制二值化出大量像素，
// 良品误报率 100%。因此参考模型采用良品均值图 + 逐像素标准差图，
// 用 z = |x - mean| / (std + eps) 把"纹理区正常波动大"纳入模型，
// 再对 z 图做 51×51 窗口均值聚合（纹理噪声是颗粒状高频，聚合后回落；
// 划痕/变形/变色是成片区域异常，聚合后仍显著），最后对聚合分数阈值化。
//
// 已知局限：screw 类存在旋转差异，本管线无配准，效果差；后续可在差分前
// 加 ORB 特征 + 单应性配准。metal_nut 的 scratch 类对比度低，召回有限。
class DetectionEngine
{
public:
    // 用该产品类的良品训练图构建参考模型（均值图 + 逐像素标准差图）。
    // goodImagePaths 为空则构建失败。
    bool buildReference(const QStringList& goodImagePaths);

    bool hasReference() const { return !m_referenceMean.empty(); }

    // 对一张原图（BGR 或灰度，尺寸不符会 resize 到模板尺寸）执行检测
    DetectionResult detect(const cv::Mat& image) const;

    // 可调参数
    int gaussianKernel = 5;             // 输入高斯预滤波核（奇数）
    double stdEps = 3.0;                // z-score 分母epsilon，防除零并抑制平坦区噪声
    int aggWindow = 51;                 // z 图空间聚合窗口（奇数）
    double zAggThreshold = 1.4;         // 聚合分数阈值：高于它判为缺陷像素
    int morphCloseKernel = 21;          // 闭运算核（连接邻近缺陷像素成区）
    int minDefectArea = 100;            // 连通域最小面积（像素），小于则视为噪声
    int imageLevelMinArea = 1000;       // 图像级检出判定的缺陷总面积下限

private:
    static cv::Mat toGray(const cv::Mat& image);

    cv::Mat m_referenceMean;            // 良品均值图，CV_32F
    cv::Mat m_referenceStd;             // 良品逐像素标准差图，CV_32F
    cv::Size m_templateSize;            // 模板尺寸，检测前统一到该尺寸
};
