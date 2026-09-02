#include "ResultExporter.h"
#include "OverlayColors.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringConverter>
#include <QTextStream>

namespace {

void blendMask(cv::Mat& bgr, const cv::Mat& mask, const cv::Scalar& color, double alpha)
{
    if (mask.empty() || bgr.empty())
        return;
    cv::Mat m = mask;
    if (m.size() != bgr.size())
        cv::resize(m, m, bgr.size(), 0, 0, cv::INTER_NEAREST);
    if (m.type() != CV_8UC1)
        return;
    cv::Mat overlay = bgr.clone();
    overlay.setTo(color, m);
    cv::addWeighted(overlay, alpha, bgr, 1.0 - alpha, 0, bgr);
}

QString csvNum(double v)
{
    return QString::number(v, 'f', 4);
}

void writeSummaryRow(QTextStream& out, const QString& name,
                     const PixelMetrics& p, const ImageMetrics& im,
                     const ImageMetrics& ia)
{
    out << name << ','
        << csvNum(p.precision()) << ','
        << csvNum(p.recall()) << ','
        << csvNum(p.f1()) << ','
        << csvNum(p.iou()) << ','
        << csvNum(im.accuracy()) << ','
        << im.correct << ','
        << im.total << ','
        << csvNum(ia.accuracy()) << ','
        << ia.correct << '\n';
}

} // namespace

cv::Mat ResultExporter::composeAnnotated(const cv::Mat& bgr,
                                         const cv::Mat& detMask,
                                         const std::vector<cv::Rect>& boxes,
                                         const cv::Mat& gtMask)
{
    if (bgr.empty())
        return {};
    cv::Mat out;
    if (bgr.channels() == 1)
        cv::cvtColor(bgr, out, cv::COLOR_GRAY2BGR);
    else
        out = bgr.clone();

    blendMask(out, gtMask, OverlayColors::gtBgr(), OverlayColors::gtBlend);
    blendMask(out, detMask, OverlayColors::detBgr(), OverlayColors::detBlend);
    for (const cv::Rect& r : boxes)
        cv::rectangle(out, r, OverlayColors::detBoxBgr(), 2);
    return out;
}

bool ResultExporter::saveImage(const QString& path, const cv::Mat& bgr)
{
    if (bgr.empty() || path.isEmpty())
        return false;
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    return cv::imwrite(path.toLocal8Bit().constData(), bgr);
}

bool ResultExporter::exportBatch(const QString& dir,
                                 const QString& category,
                                 const QString& engineName,
                                 const DatasetManager& dataset,
                                 const BatchMetrics& metrics)
{
    if (dir.isEmpty())
        return false;
    if (!QDir().mkpath(dir))
        return false;
    QDir outDir(dir);

    for (const BatchImageRecord& rec : metrics.records) {
        cv::Mat img = cv::imread(rec.imagePath.toLocal8Bit().constData(), cv::IMREAD_COLOR);
        if (img.empty())
            continue;
        cv::Mat gt;
        const QString gtPath = dataset.groundTruthMask(category, rec.defectType, rec.imagePath);
        if (!gtPath.isEmpty())
            gt = cv::imread(gtPath.toLocal8Bit().constData(), cv::IMREAD_GRAYSCALE);
        const cv::Mat annotated = composeAnnotated(img, rec.result.defectMask,
                                                   rec.result.boxes, gt);
        const QString rel = QStringLiteral("images/%1/%2")
                                .arg(rec.defectType, QFileInfo(rec.imagePath).fileName());
        if (!saveImage(outDir.filePath(rel), annotated))
            return false;
    }

    QFile perFile(outDir.filePath(QStringLiteral("per_image.csv")));
    if (!perFile.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream per(&perFile);
    per.setEncoding(QStringConverter::Utf8);
    per << QStringLiteral("category,engine,defect,file,detected,detected_area,n_boxes,total_area,"
                          "image_score,image_threshold,P,R,F1,IoU,image_ok\n");
    for (const BatchImageRecord& rec : metrics.records) {
        const bool isDefect = (rec.defectType != QStringLiteral("good"));
        const bool imageOk = (rec.result.detected() == isDefect);
        per << category << ','
            << engineName << ','
            << rec.defectType << ','
            << QFileInfo(rec.imagePath).fileName() << ','
            << (rec.result.detected() ? 1 : 0) << ','
            << (rec.result.detectedByArea() ? 1 : 0) << ','
            << rec.result.boxes.size() << ','
            << QString::number(rec.result.totalArea, 'f', 0) << ','
            << csvNum(rec.result.imageScore) << ','
            << csvNum(rec.result.imageThreshold) << ','
            << csvNum(rec.pixel.precision()) << ','
            << csvNum(rec.pixel.recall()) << ','
            << csvNum(rec.pixel.f1()) << ','
            << csvNum(rec.pixel.iou()) << ','
            << (imageOk ? 1 : 0) << '\n';
    }
    perFile.close();

    QFile sumFile(outDir.filePath(QStringLiteral("summary.csv")));
    if (!sumFile.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream sum(&sumFile);
    sum.setEncoding(QStringConverter::Utf8);
    sum << QStringLiteral("category,engine,defect,P,R,F1,IoU,img_acc,img_correct,img_total,"
                          "img_area_acc,img_area_correct\n");
    PixelMetrics pixelTotal;
    ImageMetrics imageTotal;
    ImageMetrics imageAreaTotal;
    const QStringList keys = metrics.pixel.keys();
    for (const QString& defect : keys) {
        const PixelMetrics& p = metrics.pixel[defect];
        const ImageMetrics& im = metrics.image[defect];
        const ImageMetrics& ia = metrics.imageByArea[defect];
        pixelTotal += p;
        imageTotal += im;
        imageAreaTotal += ia;
        sum << category << ',' << engineName << ',';
        writeSummaryRow(sum, defect, p, im, ia);
    }
    sum << category << ',' << engineName << ',';
    writeSummaryRow(sum, QStringLiteral("TOTAL"), pixelTotal, imageTotal, imageAreaTotal);
    const ImageMetrics& goodIm = metrics.image[QStringLiteral("good")];
    if (goodIm.total > 0) {
        // P 列放误报率（分数口径），img_correct 为误报张数
        const ImageMetrics& goodArea = metrics.imageByArea[QStringLiteral("good")];
        sum << category << ',' << engineName << ','
            << QStringLiteral("good_FPR") << ','
            << csvNum(1.0 - goodIm.accuracy()) << ",,,,,"
            << (goodIm.total - goodIm.correct) << ','
            << goodIm.total << ','
            << csvNum(1.0 - goodArea.accuracy()) << ','
            << (goodArea.total - goodArea.correct) << '\n';
    }
    return true;
}
