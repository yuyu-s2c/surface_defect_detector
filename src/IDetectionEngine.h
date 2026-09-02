#pragma once

#include <opencv2/core.hpp>

#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

// 单张图的检测结果：所有检测引擎（传统 CV / 深度学习）的统一输出契约。
// 改此结构需同步改 DetectionController、ResultEvaluator、main.cpp 批处理三处。
struct DetectionResult
{
    cv::Mat defectMask;                 // 8UC1，0/255 缺陷掩码
    std::vector<cv::Rect> boxes;        // 缺陷外接框
    std::vector<double> areas;          // 各缺陷像素面积
    double totalArea = 0.0;             // 缺陷总像素数（连通域过滤后）
    int minImageArea = 1000;            // 图像级检出面积门；引擎 detect() 写入
    // 图像级检出判定：缺陷总面积达到阈值才算检出（零散噪声不报警）
    bool detected() const { return totalArea >= minImageArea; }
};

// 检测引擎抽象接口。Phase 2 的深度学习引擎（EfficientAD）实现同一接口即可接入，
// UI（MainWindow）、编排（DetectionController）、评估（ResultEvaluator）均无需改动。
class IDetectionEngine
{
public:
    virtual ~IDetectionEngine() = default;

    // current/total；返回 false 则中止 buildReference（窗口关闭时）
    using ProgressFn = std::function<bool(int current, int total)>;
    void setProgressCallback(ProgressFn cb) { m_progress = std::move(cb); }

    // 用该产品类的良品训练图构建参考模型；goodImagePaths 为空则构建失败
    virtual bool buildReference(const QStringList& goodImagePaths) = 0;

    virtual bool hasReference() const = 0;

    // 对一张原图（BGR 或灰度）执行检测；尺寸不符由实现内部统一到模型输入尺寸
    virtual DetectionResult detect(const cv::Mat& image) const = 0;

protected:
    bool reportProgress(int current, int total) const
    {
        return m_progress ? m_progress(current, total) : true;
    }

private:
    ProgressFn m_progress;
};
