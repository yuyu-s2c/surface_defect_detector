#pragma once

#include "DatasetManager.h"
#include "DLDetectionEngine.h"
#include "EngineParams.h"
#include "IDetectionEngine.h"
#include "ResultEvaluator.h"

#include <QObject>
#include <QMap>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QVector>

#include <opencv2/core.hpp>

#include <atomic>
#include <functional>
#include <optional>

// 检测引擎类型：传统 CV 基线 / 深度学习（EfficientAD ONNX）
enum class EngineKind { Traditional, DL };

// 批量中的单张记录（导出标注图用；掩码为引用计数，整批量级可接受）
struct BatchImageRecord
{
    QString defectType;
    QString imagePath;
    DetectionResult result;
    PixelMetrics pixel;
};

// 一个类别的批量评估结果（按缺陷类型累积 + 逐图记录）
struct BatchMetrics
{
    QMap<QString, PixelMetrics> pixel;       // 缺陷类型 -> 像素级指标
    QMap<QString, ImageMetrics> image;       // 分数口径（默认，detected()）
    QMap<QString, ImageMetrics> imageByArea; // 面积门对照列
    QVector<BatchImageRecord> records;
};

// 应用服务层：数据集 + 引擎生命周期 + 检测编排。
// GUI（MainViewModel）与 CLI（main.cpp --batch）共用同一份编排逻辑，避免两处重复。
//
// 同步 API（prepareEngine / detect / runBatch）供 CLI 使用，跑完再退。
// GUI 走 *Async：工作线程做 ONNX 加载/标定/推理，进度信号回主线程，避免切 DL 卡死。
class DetectionController : public QObject
{
    Q_OBJECT

public:
    explicit DetectionController(QObject* parent = nullptr);
    ~DetectionController() override;

    // 加载数据集根目录；失败返回 false。成功后清空引擎缓存与批量结果。
    bool loadDataset(const QString& rootPath);

    // 数据集只读访问（树填充、GT 掩码路径查询用）
    const DatasetManager& dataset() const { return m_dataset; }

    // 切换检测引擎类型（默认 Traditional）。
    // 两类引擎分缓存，切换不清实例，避免 GUI 来回切 / 双引擎对比时重复加载 ONNX。
    void setEngineKind(EngineKind kind);
    EngineKind engineKind() const;

    // ONNX EP（仅 DL）。改 EP 会丢掉已缓存的 DL 会话与该引擎批量结果。
    void setOrtEpKind(OrtEpKind kind);
    OrtEpKind ortEpKind() const;
    // 该类 DL 引擎实际用上的 EP 文案；尚未构建则空串
    QString dlProviderLabel(const QString& category) const;
    // 已加载会话用的 ONNX 路径；尚未构建则按约定解析（文件不存在仍返回空）
    QString dlModelPath(const QString& category) const;
    // anomalib 导出点，其次 models/<类>/<类>.onnx（接入文档约定，不因缺文件而省略）
    QStringList onnxModelCandidates(const QString& category) const;
    // 约定路径上是否已有 ONNX（GUI 缺模型提示用，不构建会话）
    bool hasOnnxModel(const QString& category) const;
    // 模型旁是否已有 .calib.json 文件（不校验 EP/指纹；真伪在 buildReference）
    bool hasCalibCache(const QString& category) const;
    bool dlLoadedCalibFromCache(const QString& category) const;
    int trainGoodCount(const QString& category) const;
    // GUI 展示当前 EP 策略，尚未建会话时也能看
    QString ortEpPolicyLabel() const;

    // 按类别覆盖可调参数；已缓存的该引擎实例当场改字段（DL 改 k 不重建会话）。
    // CLI 不调用，保持代码内 P2 工作点。
    void setTraditionalParams(const QString& category, const TraditionalParams& p);
    TraditionalParams traditionalParams(const QString& category) const;
    void setDLParams(const QString& category, const DLParams& p);
    DLParams dlParams(const QString& category) const;

    // 确保该类别的检测引擎已构建（参考模型构建需读全部良品图，较耗时，
    // 故惰性构建并缓存；构建失败不缓存）。供调用方做显式错误提示。
    bool prepareEngine(const QString& category);

    // 单张检测；引擎未构建时自动构建，构建失败返回空结果。
    DetectionResult detect(const QString& category, const cv::Mat& image);

    // 批量跑一个类别的全部测试图并累积指标；引擎构建失败返回 false。
    // 期间每处理完一张图发射 imageProcessed。成功后写入该引擎×类别的 lastBatch。
    bool runBatch(const QString& category, BatchMetrics& out);

    // 临时切到 kind 跑批量（写对应 lastBatch），结束后恢复原引擎。供双引擎对比。
    bool runBatchWithEngine(const QString& category, EngineKind kind, BatchMetrics& out);

    // 最近一次该引擎×类别的批量；没有则 nullptr（不清引擎缓存时也不丢）
    const BatchMetrics* lastBatch(EngineKind kind, const QString& category) const;

    bool isBusy() const;

    // GUI：后台准备引擎并检测当前图。忙碌时只保留最新一次请求。
    void prepareAndDetectAsync(const QString& category, const cv::Mat& image);
    // GUI：只构建引擎（取流开始前标定）。忙碌时忽略。
    void prepareEngineAsync(const QString& category);
    void runBatchAsync(const QString& category);
    void compareAsync(const QString& category);

signals:
    // 批量模式每处理完一张图发一次。
    // CLI 接它打印逐图日志；GUI 用 progressChanged，不要在这槽里 processEvents。
    void imageProcessed(const QString& defectType, const QString& imagePath,
                        const DetectionResult& result, const PixelMetrics& pm);

    // total<=0 表示不确定进度（加载 ONNX / 单张推理）
    void progressChanged(int current, int total, const QString& text);
    void busyChanged(bool busy);
    void currentDetectFinished(bool ok, const DetectionResult& result);
    void enginePrepared(bool ok, const QString& category);
    void batchFinished(bool ok, const QString& category);
    void compareFinished(bool ok, const QString& category);

private:
    struct PendingDetect
    {
        QString category;
        cv::Mat image;
        quint64 gen = 0;
    };

    IDetectionEngine* engineFor(const QString& category);
    QMap<QString, IDetectionEngine*>& engineMap();
    const QMap<QString, IDetectionEngine*>& engineMap() const;
    QMap<QString, BatchMetrics>& lastBatchMap(EngineKind kind);
    const QMap<QString, BatchMetrics>& lastBatchMap(EngineKind kind) const;

    void attachProgress(IDetectionEngine* engine, EngineKind kind, const QString& category);
    void startJob(std::function<void()> fn);
    void runDetectJob(const QString& category, cv::Mat image, quint64 gen);
    void finishJobOnGui();

    DatasetManager m_dataset;
    QMap<QString, IDetectionEngine*> m_cvEngines;
    QMap<QString, IDetectionEngine*> m_dlEngines;
    QMap<QString, TraditionalParams> m_traditionalParams;
    QMap<QString, DLParams> m_dlParams;
    QMap<QString, BatchMetrics> m_lastCv;
    QMap<QString, BatchMetrics> m_lastDl;
    EngineKind m_engineKind = EngineKind::Traditional;
    OrtEpKind m_ortEpKind = OrtEpKind::Auto;

    mutable QMutex m_mutex;
    QThreadPool m_pool;
    std::atomic<bool> m_abort{false};
    bool m_busy = false;
    quint64 m_detectGen = 0;
    std::optional<PendingDetect> m_pendingDetect;
};
