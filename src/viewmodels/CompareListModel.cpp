#include "CompareListModel.h"

#include "Format.h"

CompareListModel::CompareListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void CompareListModel::clear()
{
    if (m_rows.isEmpty())
        return;
    beginResetModel();
    m_rows.clear();
    endResetModel();
}

void CompareListModel::setCompare(const QMap<QString, PixelMetrics>& cvPixel,
                                 const QMap<QString, ImageMetrics>& cvImage,
                                 const QMap<QString, PixelMetrics>& dlPixel,
                                 const QMap<QString, ImageMetrics>& dlImage)
{
    beginResetModel();
    m_rows.clear();

    QStringList keys = cvPixel.keys();
    for (const QString& k : dlPixel.keys()) {
        if (!keys.contains(k))
            keys.append(k);
    }
    keys.sort();

    PixelMetrics cvTotal, dlTotal;
    ImageMetrics cvImg, dlImg;

    auto makeRow = [](const QString& name,
                      const PixelMetrics& cp, const ImageMetrics& ci,
                      const PixelMetrics& dp, const ImageMetrics& di) {
        Row row;
        row.defect = name;
        row.cvPrecision = fmt3(cp.precision());
        row.cvRecall = fmt3(cp.recall());
        row.cvF1 = fmt3(cp.f1());
        row.cvIou = fmt3(cp.iou());
        row.cvImageAcc = imageAccText(ci);
        row.dlPrecision = fmt3(dp.precision());
        row.dlRecall = fmt3(dp.recall());
        row.dlF1 = fmt3(dp.f1());
        row.dlIou = fmt3(dp.iou());
        row.dlImageAcc = imageAccText(di);
        row.deltaF1 = fmt3(dp.f1() - cp.f1());
        return row;
    };

    for (const QString& defect : keys) {
        const PixelMetrics cp = cvPixel.value(defect);
        const ImageMetrics ci = cvImage.value(defect);
        const PixelMetrics dp = dlPixel.value(defect);
        const ImageMetrics di = dlImage.value(defect);
        cvTotal += cp;
        dlTotal += dp;
        cvImg += ci;
        dlImg += di;
        m_rows.push_back(makeRow(defect, cp, ci, dp, di));
    }

    if (!keys.isEmpty()) {
        Row sum = makeRow(QStringLiteral("汇总"), cvTotal, cvImg, dlTotal, dlImg);
        sum.isSummary = true;
        m_rows.push_back(sum);

        const ImageMetrics cvGood = cvImage.value(QStringLiteral("good"));
        const ImageMetrics dlGood = dlImage.value(QStringLiteral("good"));
        Row fpr;
        fpr.defect = QStringLiteral("good 误报率");
        fpr.isGoodFpr = true;
        if (cvGood.total > 0) {
            fpr.cvImageAcc = QStringLiteral("%1 (%2/%3)")
                                 .arg(fmt3(1.0 - cvGood.accuracy()))
                                 .arg(cvGood.total - cvGood.correct)
                                 .arg(cvGood.total);
        } else {
            fpr.cvImageAcc = QStringLiteral("—");
        }
        if (dlGood.total > 0) {
            fpr.dlImageAcc = QStringLiteral("%1 (%2/%3)")
                                 .arg(fmt3(1.0 - dlGood.accuracy()))
                                 .arg(dlGood.total - dlGood.correct)
                                 .arg(dlGood.total);
        } else {
            fpr.dlImageAcc = QStringLiteral("—");
        }
        m_rows.push_back(fpr);
    }
    endResetModel();
}

int CompareListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

QVariant CompareListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row& r = m_rows.at(index.row());
    switch (role) {
    case DefectRole:
        return r.defect;
    case CvPrecisionRole:
        return r.cvPrecision;
    case CvRecallRole:
        return r.cvRecall;
    case CvF1Role:
        return r.cvF1;
    case CvIouRole:
        return r.cvIou;
    case CvImageAccRole:
        return r.cvImageAcc;
    case DlPrecisionRole:
        return r.dlPrecision;
    case DlRecallRole:
        return r.dlRecall;
    case DlF1Role:
        return r.dlF1;
    case DlIouRole:
        return r.dlIou;
    case DlImageAccRole:
        return r.dlImageAcc;
    case DeltaF1Role:
        return r.deltaF1;
    case IsSummaryRole:
        return r.isSummary;
    case IsGoodFprRole:
        return r.isGoodFpr;
    default:
        return {};
    }
}

QHash<int, QByteArray> CompareListModel::roleNames() const
{
    return {
        {DefectRole, "defect"},
        {CvPrecisionRole, "cvPrecision"},
        {CvRecallRole, "cvRecall"},
        {CvF1Role, "cvF1"},
        {CvIouRole, "cvIou"},
        {CvImageAccRole, "cvImageAcc"},
        {DlPrecisionRole, "dlPrecision"},
        {DlRecallRole, "dlRecall"},
        {DlF1Role, "dlF1"},
        {DlIouRole, "dlIou"},
        {DlImageAccRole, "dlImageAcc"},
        {DeltaF1Role, "deltaF1"},
        {IsSummaryRole, "isSummary"},
        {IsGoodFprRole, "isGoodFpr"},
    };
}
