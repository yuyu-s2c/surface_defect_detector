#include "DLDetectionEngine.h"

#include <onnxruntime_cxx_api.h>

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtDebug>

#include <windows.h>
#include <dxgi.h>

#include <cstring>
#include <vector>

namespace {
// 进程级共享 ORT 环境（官方推荐单例；多引擎实例共用一个 Env）
Ort::Env& ortEnv()
{
    static Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "surface_defect_detector");
    return env;
}

// 缓存格式版本：统计方式或键变了就升，强制重跑 train/good。
// v2：加入 provider（cpu/dml），CPU 与 DML 热图数值不可混用同一阈值。
constexpr int kCalibCacheVersion = 2;

struct DmlAdapter {
    int deviceId = 0;
    QString name;
    quint64 vramBytes = 0;
};

// 笔记本双显卡：device_id 0 经常是核显。按 DXGI 枚举下标选独显（最大专用显存）。
DmlAdapter pickDmlAdapter()
{
    DmlAdapter best;
    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))
        || !factory) {
        qWarning() << "DXGI 工厂创建失败，DirectML 用 device_id=0";
        return best;
    }
    IDXGIAdapter1* adapter = nullptr;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);
        adapter->Release();
        adapter = nullptr;
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;
        const QString name = QString::fromWCharArray(desc.Description);
        if (name.contains(QStringLiteral("Microsoft Basic Render"), Qt::CaseInsensitive))
            continue;
        if (desc.DedicatedVideoMemory >= best.vramBytes) {
            best.vramBytes = desc.DedicatedVideoMemory;
            best.deviceId = static_cast<int>(i);
            best.name = name;
        }
    }
    factory->Release();
    if (best.name.isEmpty())
        qWarning() << "未找到独立 GPU 适配器，DirectML 用 device_id=0";
    else
        qInfo() << "DirectML 适配器:" << best.name
                << "device_id=" << best.deviceId
                << "VRAM_MB=" << (best.vramBytes / (1024 * 1024));
    return best;
}

// 不 include dml_provider_factory.h（会拉 DirectML.h / d3d12.h，MinGW 没有那套 SDK 头）。
// 只取 OrtDmlApi 第一个函数，与官方结构体前缀布局一致。
struct OrtDmlApiHead {
    OrtStatus*(ORT_API_CALL* SessionOptionsAppendExecutionProvider_DML)(
        OrtSessionOptions* options, int device_id);
};

QString calibCachePath(const QString& modelPath)
{
    const QFileInfo fi(modelPath);
    return fi.dir().filePath(fi.completeBaseName() + QStringLiteral(".calib.json"));
}

// 不读像素：文件名 + size + mtime，比跑一遍 ONNX 便宜几个数量级
QString fingerprintGoodImages(const QStringList& paths)
{
    QStringList sorted = paths;
    sorted.sort();
    QCryptographicHash hash(QCryptographicHash::Sha1);
    for (const QString& p : sorted) {
        const QFileInfo fi(p);
        const QByteArray line = QStringLiteral("%1|%2|%3\n")
                                    .arg(fi.fileName())
                                    .arg(fi.size())
                                    .arg(fi.lastModified().toMSecsSinceEpoch())
                                    .toUtf8();
        hash.addData(line);
    }
    return QString::fromLatin1(hash.result().toHex());
}

bool tryLoadCalibCache(const QString& path, qint64 modelSize, qint64 modelMtimeMs,
                       int inputSize, int goodCount, const QString& goodFp,
                       const QString& provider, double& mean, double& stddev)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "DL 标定缓存损坏，忽略:" << path << err.errorString();
        return false;
    }
    const QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("version")).toInt() != kCalibCacheVersion)
        return false;
    if (obj.value(QStringLiteral("inputSize")).toInt() != inputSize)
        return false;
    if (obj.value(QStringLiteral("modelSize")).toInteger() != modelSize)
        return false;
    if (obj.value(QStringLiteral("modelMtimeMs")).toInteger() != modelMtimeMs)
        return false;
    if (obj.value(QStringLiteral("goodCount")).toInt() != goodCount)
        return false;
    if (obj.value(QStringLiteral("goodFingerprint")).toString() != goodFp)
        return false;
    if (obj.value(QStringLiteral("provider")).toString() != provider)
        return false;
    if (!obj.contains(QStringLiteral("calibMean")) || !obj.contains(QStringLiteral("calibStd")))
        return false;
    mean = obj.value(QStringLiteral("calibMean")).toDouble();
    stddev = obj.value(QStringLiteral("calibStd")).toDouble();
    return true;
}

void saveCalibCache(const QString& path, qint64 modelSize, qint64 modelMtimeMs,
                    int inputSize, int goodCount, const QString& goodFp,
                    const QString& provider, double mean, double stddev)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("version"), kCalibCacheVersion);
    obj.insert(QStringLiteral("inputSize"), inputSize);
    obj.insert(QStringLiteral("modelSize"), modelSize);
    obj.insert(QStringLiteral("modelMtimeMs"), modelMtimeMs);
    obj.insert(QStringLiteral("goodCount"), goodCount);
    obj.insert(QStringLiteral("goodFingerprint"), goodFp);
    obj.insert(QStringLiteral("provider"), provider);
    obj.insert(QStringLiteral("calibMean"), mean);
    obj.insert(QStringLiteral("calibStd"), stddev);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "DL 标定缓存写入失败:" << path << f.errorString();
        return;
    }
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}
} // namespace

DLDetectionEngine::DLDetectionEngine(const QString& modelPath, OrtEpKind epKind)
    : m_modelPath(modelPath)
    , m_epKind(epKind)
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
    m_calibMean = 0.0;
    m_calibStd = 0.0;

    if (!QFileInfo::exists(m_modelPath)) {
        qWarning() << "DL 模型不存在:" << m_modelPath
                   << "（先运行 tools/training/train_efficientad.py 训练导出）";
        return false;
    }
    if (goodImagePaths.isEmpty()) {
        qWarning() << "DL 引擎阈值标定需要良品图，收到空列表";
        return false;
    }

    const bool wantDml = (m_epKind != OrtEpKind::Cpu);
    const bool requireDml = (m_epKind == OrtEpKind::Dml);
    try {
        if (wantDml) {
            try {
                createSession(true);
            } catch (const Ort::Exception& e) {
                qWarning() << "DirectML 会话失败:" << e.what();
                m_session.reset();
                m_activeProvider.clear();
                m_calibProviderKey.clear();
                if (requireDml)
                    return false;
                qWarning() << "回退 CPU ONNX";
                createSession(false);
            }
        } else {
            createSession(false);
        }
    } catch (const Ort::Exception& e) {
        qWarning() << "ONNX 会话创建失败:" << e.what();
        m_session.reset();
        m_activeProvider.clear();
        m_calibProviderKey.clear();
        return false;
    }

    const QFileInfo modelInfo(m_modelPath);
    const qint64 modelSize = modelInfo.size();
    const qint64 modelMtimeMs = modelInfo.lastModified().toMSecsSinceEpoch();
    const QString goodFp = fingerprintGoodImages(goodImagePaths);
    const QString cachePath = calibCachePath(m_modelPath);
    if (tryLoadCalibCache(cachePath, modelSize, modelMtimeMs, inputSize,
                          goodImagePaths.size(), goodFp, m_calibProviderKey,
                          m_calibMean, m_calibStd)) {
        qInfo() << "DL 阈值从缓存加载:" << cachePath
                << "mean=" << m_calibMean << "std=" << m_calibStd
                << "k=" << thresholdSigma
                << "-> threshold=" << (m_calibMean + thresholdSigma * m_calibStd);
        return true;
    }

    // 阈值标定：跑全部良品图，统计每张热图最大值的均值与标准差。
    // 良品热图最大值反映"正常波动的上限"，缺陷图的异常区域应显著高于它。
    cv::Mat maxes;
    const int total = goodImagePaths.size();
    int i = 0;
    for (const QString& p : goodImagePaths) {
        ++i;
        if (!reportProgress(i, total)) {
            m_session.reset();
            return false;
        }
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
    m_calibMean = mean[0];
    m_calibStd = stddev[0];
    // 日志用当前 k 算出阈值；detect() 再按当时的 thresholdSigma 现算，改 k 无需重标定
    qInfo() << "DL 阈值标定:" << m_modelPath
            << "良品热图最大值 mean=" << m_calibMean << "std=" << m_calibStd
            << "k=" << thresholdSigma
            << "-> threshold=" << (m_calibMean + thresholdSigma * m_calibStd);
    saveCalibCache(cachePath, modelSize, modelMtimeMs, inputSize,
                   goodImagePaths.size(), goodFp, m_calibProviderKey,
                   m_calibMean, m_calibStd);
    return true;
}

void DLDetectionEngine::createSession(bool useDml)
{
    Ort::SessionOptions opts;
    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    if (useDml) {
        // DirectML EP 官方约束：关 mem pattern、顺序执行，否则会话创建失败
        opts.DisableMemPattern();
        opts.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
        const DmlAdapter ad = pickDmlAdapter();
        const void* rawApi = nullptr;
        Ort::ThrowOnError(Ort::GetApi().GetExecutionProviderApi(
            "DML", ORT_API_VERSION, &rawApi));
        if (!rawApi)
            throw Ort::Exception("GetExecutionProviderApi(DML) 返回空", ORT_FAIL);
        const auto* dmlApi = static_cast<const OrtDmlApiHead*>(rawApi);
        if (!dmlApi->SessionOptionsAppendExecutionProvider_DML)
            throw Ort::Exception("OrtDmlApi 缺少 Append DML", ORT_FAIL);
        Ort::ThrowOnError(
            dmlApi->SessionOptionsAppendExecutionProvider_DML(opts, ad.deviceId));
        m_calibProviderKey = QStringLiteral("dml");
        const qint64 mb = static_cast<qint64>(ad.vramBytes / (1024 * 1024));
        m_activeProvider = ad.name.isEmpty()
            ? QStringLiteral("DML (device %1)").arg(ad.deviceId)
            : QStringLiteral("DML (%1, %2 MB)").arg(ad.name).arg(mb);
    } else {
        opts.SetIntraOpNumThreads(4);
        m_calibProviderKey = QStringLiteral("cpu");
        m_activeProvider = QStringLiteral("CPU");
    }
    // Windows 下 ORTCHAR_T 为 wchar_t
    m_session = std::make_unique<Ort::Session>(
        ortEnv(), m_modelPath.toStdWString().c_str(), opts);
    qInfo() << "ONNX 会话:" << m_activeProvider << m_modelPath;
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
    // k 在 detect 时现算，GUI 改 thresholdSigma 不必重跑 train/good 标定
    const double threshold = m_calibMean + thresholdSigma * m_calibStd;
    cv::Mat bin;
    cv::threshold(heat, bin, threshold, 255, cv::THRESH_BINARY);
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
