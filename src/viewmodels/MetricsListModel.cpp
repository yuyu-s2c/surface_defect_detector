#include "MetricsListModel.h"

#include "Format.h"

MetricsListModel::MetricsListModel(QObject* parent)
    : QAbstractListModel(parent)
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
        row.defect = defect;
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

QVariant MetricsListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row& r = m_rows.at(index.row());
    switch (role) {
    case DefectRole:
        return r.defect;
    case PrecisionRole:
        return r.precision;
    case RecallRole:
        return r.recall;
    case F1Role:
        return r.f1;
    case IouRole:
        return r.iou;
    case ImageAccRole:
        return r.imageAcc;
    case IsSummaryRole:
        return r.isSummary;
    default:
        return {};
    }
}

QHash<int, QByteArray> MetricsListModel::roleNames() const
{
    return {
        {DefectRole, "defect"},
        {PrecisionRole, "precision"},
        {RecallRole, "recall"},
        {F1Role, "f1"},
        {IouRole, "iou"},
        {ImageAccRole, "imageAcc"},
        {IsSummaryRole, "isSummary"},
    };
}
