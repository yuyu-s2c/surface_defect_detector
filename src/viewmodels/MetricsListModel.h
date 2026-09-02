#pragma once

#include "ResultEvaluator.h"

#include <QAbstractListModel>
#include <QMap>
#include <QString>
#include <QVector>

class MetricsListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        DefectRole = Qt::UserRole + 1,
        PrecisionRole,
        RecallRole,
        F1Role,
        IouRole,
        ImageAccRole,
        IsSummaryRole
    };

    explicit MetricsListModel(QObject* parent = nullptr);

    void setMetrics(const QMap<QString, PixelMetrics>& pixel,
                    const QMap<QString, ImageMetrics>& image);
    void clear();

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
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
