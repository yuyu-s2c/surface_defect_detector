#pragma once

#include "IDetectionEngine.h"

#include <memory>

namespace Ort { struct Env; struct Session; }

// 深度学习检测引擎（Phase 2）：加载 anomalib 导出的 EfficientAD ONNX 模型，
// 输出与 v0.1 传统引擎相同的 DetectionResult 契约。
//
// 管线：resize 到 256×256 -> BGR→RGB、/255、NCHW -> ONNX 推理得异常热图
//       -> 上采样回原图尺寸 -> 阈值化 -> 形态学 -> 连通域按面积过滤
//       （后处理与 DetectionEngine 一致，便于同口径对比）。
//
// 为什么阈值在 buildReference 里用良品图自适应标定，而不是用 anomalib 训练时
// 存的阈值：anomalib 的阈值/归一化参数存在其 checkpoint 的 metadata 里，ONNX
// 导出不含完整的后处理标定；用 train/good 良品图跑一遍模型，取每张良品热图
// 最大值的 均值 + kσ 作为像素阈值，与接口契约（buildReference 收良品图）天然吻合，
// 且随数据集自适应。
class DLDetectionEngine : public IDetectionEngine
{
public:
    // modelPath：EfficientAD ONNX 模型路径（不存在时 buildReference 失败）
    explicit DLDetectionEngine(const QString& modelPath);
    ~DLDetectionEngine() override; // Ort::Session 为前置声明，析构在 .cpp

    // 加载 ONNX 会话，并用良品训练图标定像素阈值（见类注释）
    bool buildReference(const QStringList& goodImagePaths) override;

    bool hasReference() const override;

    DetectionResult detect(const cv::Mat& image) const override;

    // 可调参数
    int inputSize = 256;             // ONNX 模型输入边长（导出时固定 256×256）
    double thresholdSigma = 3.0;     // 阈值 = 良品热图逐图最大值的均值 + kσ（默认 3；screw 由 Controller 改为 1.0）
    int morphCloseKernel = 21;       // 闭运算核（与传统引擎一致）
    int minDefectArea = 100;         // 连通域最小面积（像素）
    int imageLevelMinArea = 1000;    // 图像级检出面积门（screw 由 Controller 改为 300）

private:
    // 推理得到异常热图并上采样到原图尺寸（CV_32F）；失败返回空 Mat
    cv::Mat anomalyMap(const cv::Mat& image) const;

    QString m_modelPath;
    std::unique_ptr<Ort::Session> m_session; // ORT 会话（Env 为进程级静态共享）
    // 良品热图最大值的均值/标准差。detect() 用 mean + kσ，改 k 不必重跑标定
    double m_calibMean = 0.0;
    double m_calibStd = 0.0;
};
