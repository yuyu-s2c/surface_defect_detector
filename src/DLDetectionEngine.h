#pragma once

#include "IDetectionEngine.h"

#include <memory>
#include <vector>

namespace Ort { struct Env; struct Session; }

// ONNX Runtime 执行器。默认 Auto：先 DirectML，失败回 CPU。
// CLI `--provider` 可强制；改 EP 必须重建会话（DetectionController 清 DL 缓存）。
enum class OrtEpKind { Auto, Cpu, Dml };

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
// 且随数据集自适应。mean/std 与每图 max 落到模型同目录的 <模型名>.calib.json（v3），
// 下次启动若 ONNX、train/good 指纹与 EP（cpu/dml）未变则跳过良品推理
// （改 k 仍不触发重标定）。
class DLDetectionEngine : public IDetectionEngine
{
public:
    // modelPath：EfficientAD ONNX 模型路径（不存在时 buildReference 失败）
    explicit DLDetectionEngine(const QString& modelPath, OrtEpKind epKind = OrtEpKind::Auto);
    ~DLDetectionEngine() override; // Ort::Session 为前置声明，析构在 .cpp

    // 实际用上的 EP，如 "DML (NVIDIA GeForce RTX 3050 Ti Laptop GPU, 4096 MB)" / "CPU"
    QString activeProvider() const { return m_activeProvider; }
    QString modelPath() const { return m_modelPath; }
    // 模型旁 <名>.calib.json；GUI 用来显示「有没有缓存」，不表示 EP 一定匹配
    static QString calibCachePathFor(const QString& modelPath);
    bool loadedCalibFromCache() const { return m_loadedCalibFromCache; }

    // 加载 ONNX 会话，并用良品训练图标定像素阈值（见类注释）
    bool buildReference(const QStringList& goodImagePaths) override;

    bool hasReference() const override;

    DetectionResult detect(const cv::Mat& image) const override;

    // 可调参数
    int inputSize = 256;             // ONNX 模型输入边长（导出时固定 256×256）
    double thresholdSigma = 3.0;     // 像素阈值 = 良品热图逐图最大值的均值 + kσ（只切掩码）
    int morphCloseKernel = 21;       // 闭运算核（与传统引擎一致）
    int minDefectArea = 100;         // 连通域最小面积（像素）
    int imageLevelMinArea = 1000;    // 叠加/框面积门（对照列；不再驱动 detected()）

private:
    // 推理得到异常热图并上采样到原图尺寸（CV_32F）；失败返回空 Mat。
    // nativeMax：256 热图最大值，作图像级分数（上采样前，与 EfficientAD 图像分一致）
    cv::Mat anomalyMap(const cv::Mat& image, double* nativeMax = nullptr) const;

    // 图像级阈值 = mean + kσ（与像素阈同一 k）。无标定则 +inf（空结果不算检出）
    double currentImageThreshold() const;

    // useDml 失败抛 Ort::Exception，由 buildReference 决定是否回退 CPU
    void createSession(bool useDml);

    QString m_modelPath;
    OrtEpKind m_epKind = OrtEpKind::Auto;
    std::unique_ptr<Ort::Session> m_session; // ORT 会话（Env 为进程级静态共享）
    QString m_activeProvider;     // 给人看
    QString m_calibProviderKey;   // 标定缓存键："dml" / "cpu"
    bool m_loadedCalibFromCache = false;
    // 良品热图最大值的均值/标准差。detect() 用 mean + kσ 切掩码，改 k 不必重跑标定
    double m_calibMean = 0.0;
    double m_calibStd = 0.0;
    std::vector<double> m_imageScores; // 每张 train/good 的 256 热图 max，写入 calib v3 便于对照
};
