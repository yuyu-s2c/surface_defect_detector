#pragma once

#include "ResultEvaluator.h"

#include <QAbstractTableModel>
#include <QMap>
#include <QString>
#include <QVector>

class MetricsListModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        ColDefect = 0,
        ColPrecision,
        ColRecall,
        ColF1,
        ColIou,
        ColImageAcc,
        ColumnCount
    };
    enum Role {
        IsSummaryRole = Qt::UserRole + 1
    };

    explicit MetricsListModel(QObject* parent = nullptr);

    void setMetrics(const QMap<QString, PixelMetrics>& pixel,
                    const QMap<QString, ImageMetrics>& image);
    void clear();

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    struct Row {
        QString defect;
        QString precision;
        QString recall;
        QString f1;
        QString iou;
        QString imageAcc;
        bool isSummary = false;
    };

    QVector<Row> m_rows;
};
