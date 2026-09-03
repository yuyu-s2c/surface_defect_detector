#include "CompareListModel.h"

#include "DisplayNames.h"
#include "Format.h"

CompareListModel::CompareListModel(QObject* parent)
    : QAbstractTableModel(parent)
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
        m_rows.push_back(makeRow(folderDisplayName(defect), cp, ci, dp, di));
    }

    if (!keys.isEmpty()) {
        Row sum = makeRow(QStringLiteral("汇总"), cvTotal, cvImg, dlTotal, dlImg);
        sum.isSummary = true;
        m_rows.push_back(sum);
    }
    endResetModel();
}

int CompareListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

int CompareListModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return ColumnCount;
}

QVariant CompareListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row& r = m_rows.at(index.row());
    if (role == IsSummaryRole)
        return r.isSummary;
    if (role != Qt::DisplayRole || index.column() < 0 || index.column() >= ColumnCount)
        return {};
    switch (index.column()) {
    case ColDefect:
        return r.defect;
    case ColCvF1:
        return r.cvF1;
    case ColDlF1:
        return r.dlF1;
    case ColDelta:
        return r.deltaF1;
    default:
        return {};
    }
}

QVariant CompareListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    static const QStringList headers = {
        QStringLiteral("类型"),
        QStringLiteral("传统综合分"),
        QStringLiteral("深度综合分"),
        QStringLiteral("差值"),
    };
    if (section < 0 || section >= headers.size())
        return {};
    return headers.at(section);
}

QHash<int, QByteArray> CompareListModel::roleNames() const
{
    auto names = QAbstractTableModel::roleNames();
    names.insert(IsSummaryRole, "isSummary");
    return names;
}

QVariantMap CompareListModel::extraAt(int row) const
{
    if (row < 0 || row >= m_rows.size())
        return {};
    const Row& r = m_rows.at(row);
    return {
        {QStringLiteral("defect"), r.defect},
        {QStringLiteral("cvPrecision"), r.cvPrecision},
        {QStringLiteral("cvRecall"), r.cvRecall},
        {QStringLiteral("cvIou"), r.cvIou},
        {QStringLiteral("cvImageAcc"), r.cvImageAcc},
        {QStringLiteral("dlPrecision"), r.dlPrecision},
        {QStringLiteral("dlRecall"), r.dlRecall},
        {QStringLiteral("dlIou"), r.dlIou},
        {QStringLiteral("dlImageAcc"), r.dlImageAcc},
        {QStringLiteral("isSummary"), r.isSummary},
    };
}
