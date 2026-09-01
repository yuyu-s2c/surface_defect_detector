#pragma once

#include "DatasetManager.h"
#include "IDetectionEngine.h"
#include "ResultEvaluator.h"

#include <QObject>
#include <QMap>
#include <QString>

#include <opencv2/core.hpp>

// 一个类别的批量评估结果（按缺陷类型累积）
struct BatchMetrics
{
    QMap<QString, PixelMetrics> pixel;  // 缺陷类型 -> 像素级指标
    QMap<QString, ImageMetrics> image;  // 缺陷类型 -> 图像级指标
};

// 应用服务层：数据集 + 引擎生命周期 + 检测编排。
// GUI（MainWindow）与 CLI（main.cpp --batch）共用同一份编排逻辑，避免两处重复。
//
// 当前为同步实现；Phase 2 接入 DL 推理（耗时长）后，内部可改为 worker 线程，
// 对外 API 与信号签名保持不变，调用方（MainWindow / main.cpp）无需改动。
class DetectionController : public QObject
{
    Q_OBJECT

public:
    explicit DetectionController(QObject* parent = nullptr);
    ~DetectionController() override;

    // 加载数据集根目录；失败返回 false。成功后清空引擎缓存。
    bool loadDataset(const QString& rootPath);

    // 数据集只读访问（树填充、GT 掩码路径查询用）
    const DatasetManager& dataset() const { return m_dataset; }

    // 确保该类别的检测引擎已构建（参考模型构建需读全部良品图，较耗时，
    // 故惰性构建并缓存；构建失败不缓存）。供调用方做显式错误提示。
    bool prepareEngine(const QString& category);

    // 单张检测；引擎未构建时自动构建，构建失败返回空结果。
    DetectionResult detect(const QString& category, const cv::Mat& image);

    // 批量跑一个类别的全部测试图并累积指标；引擎构建失败返回 false。
    // 期间每处理完一张图发射 imageProcessed。
    bool runBatch(const QString& category, BatchMetrics& out);

signals:
    // 批量模式每处理完一张图发一次。
    // GUI 接它调 processEvents 保持界面响应；CLI 接它打印逐图日志。
    void imageProcessed(const QString& defectType, const QString& imagePath,
                        const DetectionResult& result, const PixelMetrics& pm);

private:
    // 每类一个引擎实例，惰性构建并缓存；失败返回 nullptr（不缓存）
    IDetectionEngine* engineFor(const QString& category);

    DatasetManager m_dataset;
    QMap<QString, IDetectionEngine*> m_engines; // 类别 -> 引擎
};
