#include "MetricsListModel.h"

#include "DisplayNames.h"
#include "Format.h"

MetricsListModel::MetricsListModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

void MetricsListModel::clear()
{
    if (m_rows.isEmpty())
        return;
    beginResetModel();
    m_rows.clear();
    endResetModel();
}

void MetricsListModel::setMetrics(const QMap<QString, PixelMetrics>& pixel,
                                 const QMap<QString, ImageMetrics>& image)
{
    beginResetModel();
    m_rows.clear();

    PixelMetrics totalP;
    ImageMetrics totalI;
    const QStringList keys = pixel.keys();
    for (const QString& defect : keys) {
        const PixelMetrics& p = pixel[defect];
        const ImageMetrics im = image.value(defect);
        totalP += p;
        totalI += im;
        Row row;
        row.defect = folderDisplayName(defect);
        row.precision = fmt3(p.precision());
        row.recall = fmt3(p.recall());
        row.f1 = fmt3(p.f1());
        row.iou = fmt3(p.iou());
        row.imageAcc = imageAccText(im);
        m_rows.push_back(row);
    }

    if (!m_rows.isEmpty()) {
        Row sum;
        sum.defect = QStringLiteral("汇总");
        sum.precision = fmt3(totalP.precision());
        sum.recall = fmt3(totalP.recall());
        sum.f1 = fmt3(totalP.f1());
        sum.iou = fmt3(totalP.iou());
        sum.imageAcc = imageAccText(totalI);
        sum.isSummary = true;
        m_rows.push_back(sum);
    }
    endResetModel();
}

int MetricsListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

int MetricsListModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return ColumnCount;
}

QVariant MetricsListModel::data(const QModelIndex& index, int role) const
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
    case ColPrecision:
        return r.precision;
    case ColRecall:
        return r.recall;
    case ColF1:
        return r.f1;
    case ColIou:
        return r.iou;
    case ColImageAcc:
        return r.imageAcc;
    default:
        return {};
    }
}

QVariant MetricsListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    static const QStringList headers = {
        QStringLiteral("缺陷类型"),
        QStringLiteral("精确率"),
        QStringLiteral("召回率"),
        QStringLiteral("综合分"),
        QStringLiteral("交并比"),
        QStringLiteral("图像检出"),
    };
    if (section < 0 || section >= headers.size())
        return {};
    return headers.at(section);
}

QHash<int, QByteArray> MetricsListModel::roleNames() const
{
    auto names = QAbstractTableModel::roleNames();
    names.insert(IsSummaryRole, "isSummary");
    return names;
}
