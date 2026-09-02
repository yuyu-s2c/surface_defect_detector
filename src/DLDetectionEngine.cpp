#include "DLDetectionEngine.h"

#include <onnxruntime_cxx_api.h>

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <QFileInfo>
#include <QtDebug>

#include <cstring>
#include <vector>

namespace {
// 进程级共享 ORT 环境（官方推荐单例；多引擎实例共用一个 Env）
Ort::Env& ortEnv()
{
    static Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "surface_defect_detector");
    return env;
}
} // namespace

DLDetectionEngine::DLDetectionEngine(const QString& modelPath)
    : m_modelPath(modelPath)
{
}

DLDetectionEngine::~DLDetectionEngine() = default;

bool DLDetectionEngine::hasReference() const
{
    return m_session != nullptr;
}

bool DLDetectionEngine::buildReference(const QStringList& goodImagePaths)
{
    m_session.reset();
    m_threshold = 0.0;

    if (!QFileInfo::exists(m_modelPath)) {
        qWarning() << "DL 模型不存在:" << m_modelPath
                   << "（先运行 tools/training/train_efficientad.py 训练导出）";
        return false;
    }
    if (goodImagePaths.isEmpty()) {
        qWarning() << "DL 引擎阈值标定需要良品图，收到空列表";
        return false;
    }

    try {
        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(4);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        // Windows 下 ORTCHAR_T 为 wchar_t
        m_session = std::make_unique<Ort::Session>(
            ortEnv(), m_modelPath.toStdWString().c_str(), opts);
    } catch (const Ort::Exception& e) {
        qWarning() << "ONNX 会话创建失败:" << e.what();
        m_session.reset();
        return false;
    }

    // 阈值标定：跑全部良品图，统计每张热图最大值的均值与标准差。
    // 良品热图最大值反映"正常波动的上限"，缺陷图的异常区域应显著高于它。
    cv::Mat maxes;
    for (const QString& p : goodImagePaths) {
        cv::Mat img = cv::imread(p.toLocal8Bit().constData(), cv::IMREAD_COLOR);
        if (img.empty())
            continue;
        cv::Mat heat = anomalyMap(img);
        if (heat.empty())
            continue;
        double m = 0.0;
        cv::minMaxLoc(heat, nullptr, &m);
        maxes.push_back(m);
    }
    if (maxes.rows < 3) {
        qWarning() << "DL 阈值标定：有效良品图不足（" << maxes.rows << "张）";
        m_session.reset();
        return false;
    }
    cv::Scalar mean, stddev;
    cv::meanStdDev(maxes, mean, stddev);
    m_threshold = mean[0] + thresholdSigma * stddev[0];
    qInfo() << "DL 阈值标定:" << m_modelPath
            << "良品热图最大值 mean=" << mean[0] << "std=" << stddev[0]
            << "-> threshold=" << m_threshold;
    return true;
}

cv::Mat DLDetectionEngine::anomalyMap(const cv::Mat& image) const
{
    if (!m_session || image.empty())
        return {};

    try {
        // 前处理：resize -> BGR→RGB -> float/255 -> NCHW。
        // ImageNet 归一化在 EfficientAD 模型 forward 内部，ONNX 输入即 0~1 浮点图。
        const cv::Size inSize(inputSize, inputSize);
        cv::Mat rgb;
        cv::resize(image, rgb, inSize);
        cv::cvtColor(rgb, rgb, cv::COLOR_BGR2RGB);
        cv::Mat f;
        rgb.convertTo(f, CV_32F, 1.0 / 255.0);

        // HWC -> NCHW（连续 buffer）
        std::vector<float> input(3 * inputSize * inputSize);
        const int planeSize = inputSize * inputSize;
        std::vector<cv::Mat> channels(3);
        for (int c = 0; c < 3; ++c)
            channels[c] = cv::Mat(inputSize, inputSize, CV_32F,
                                  input.data() + c * planeSize);
        cv::split(f, channels);

        const int64_t shape[] = {1, 3, inputSize, inputSize};
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
        Ort::Value inTensor = Ort::Value::CreateTensor<float>(
            mem, input.data(), input.size(), shape, 4);

        // 不依赖具体张量名：输入取第 0 个，输出取第一个 4D 张量（异常热图）。
        // EfficientAD ONNX 导出通常含 anomaly_map(1,1,H,W) 与 pred_score(标量)。
        Ort::AllocatorWithDefaultOptions alloc;
        const auto inName = m_session->GetInputNameAllocated(0, alloc);
        const size_t outCount = m_session->GetOutputCount();
        std::vector<std::string> outNameStorage;
        std::vector<const char*> outNames;
        outNameStorage.reserve(outCount);
        outNames.reserve(outCount);
        for (size_t i = 0; i < outCount; ++i) {
            auto n = m_session->GetOutputNameAllocated(i, alloc);
            outNameStorage.emplace_back(n.get());
            outNames.push_back(outNameStorage.back().c_str());
        }
        const char* inNames[] = {inName.get()};
        auto outputs = m_session->Run(Ort::RunOptions{}, inNames, &inTensor, 1,
                                      outNames.data(), outNames.size());

        for (auto& out : outputs) {
            const auto info = out.GetTensorTypeAndShapeInfo();
            const auto outShape = info.GetShape();
            if (outShape.size() != 4)
                continue; // 跳过 pred_score 等非标量图输出
            const int h = static_cast<int>(outShape[2]);
            const int w = static_cast<int>(outShape[3]);
            const float* data = out.GetTensorData<float>();
            cv::Mat heat(h, w, CV_32F);
            std::memcpy(heat.data, data, sizeof(float) * h * w);
            // 上采样回原图尺寸，保证掩码与 GT 同尺寸（评估口径与 v0.1 一致）
            cv::resize(heat, heat, image.size(), 0, 0, cv::INTER_LINEAR);
            return heat;
        }
        qWarning() << "DL 推理：输出中没有 4D 异常热图";
        return {};
    } catch (const Ort::Exception& e) {
        qWarning() << "DL 推理失败:" << e.what();
        return {};
    }
}

DetectionResult DLDetectionEngine::detect(const cv::Mat& image) const
{
    DetectionResult result;
    if (!hasReference() || image.empty())
        return result;

    cv::Mat heat = anomalyMap(image);
    if (heat.empty())
        return result;

    // 阈值 + 形态学 + 连通域过滤（与传统引擎同口径）
    cv::Mat bin;
    cv::threshold(heat, bin, m_threshold, 255, cv::THRESH_BINARY);
    bin.convertTo(bin, CV_8U);
    const int ck = morphCloseKernel | 1;
    const cv::Mat closeKernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(ck, ck));
    cv::morphologyEx(bin, bin, cv::MORPH_CLOSE, closeKernel);
    const cv::Mat openKernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(bin, bin, cv::MORPH_OPEN, openKernel);

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
