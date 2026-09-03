#pragma once

#include "DetectionController.h"
#include "LiveSessionTypes.h"

#include <QString>
#include <QVector>

#include <opencv2/core.hpp>

#include <vector>

// 检测结果导出（Phase 3）：叠图与 CSV，无 UI 依赖。
// 配色与 InspectionCanvas / OverlayColors 一致：GT 红半透明、检测绿半透明 + 绿框。
class ResultExporter
{
public:
    // 原图 + 可选 GT / 检测掩码 + 外接框。任一掩码为空则跳过该层。
    static cv::Mat composeAnnotated(const cv::Mat& bgr,
                                    const cv::Mat& detMask,
                                    const std::vector<cv::Rect>& boxes,
                                    const cv::Mat& gtMask = cv::Mat());

    static bool saveImage(const QString& path, const cv::Mat& bgr);

    // 写出 images/<defect>/<filename>.png + per_image.csv + summary.csv。
    // per_image 含 image_score / image_threshold / detected（分数）与 detected_area；
    // summary 含 img_acc 与 img_area_* 对照列。目录不存在则创建。失败返回 false。
    static bool exportBatch(const QString& dir,
                            const QString& category,
                            const QString& engineName,
                            const DatasetManager& dataset,
                            const BatchMetrics& metrics);

    // 取流班次：session.csv（每张含 OK）+ summary.csv。目录不存在则创建。
    // summary 末尾追加工单/联锁列，不改 --batch CSV。
    static bool exportLiveSession(const QString& dir,
                                  const LiveSessionSummary& summary,
                                  const QVector<LivePieceRecord>& pieces);

    static QString makeSessionDir(const QString& datasetRoot,
                                  const QString& category,
                                  const QString& engineName,
                                  QString* sessionIdOut = nullptr);
};
