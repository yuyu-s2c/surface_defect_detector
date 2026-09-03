#pragma once

#include "ResultEvaluator.h"

#include <QAbstractTableModel>
#include <QMap>
#include <QString>
#include <QVariantMap>
#include <QVector>

class CompareListModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        ColDefect = 0,
        ColCvF1,
        ColDlF1,
        ColDelta,
        ColumnCount
    };
    enum Role {
        IsSummaryRole = Qt::UserRole + 1
    };

    explicit CompareListModel(QObject* parent = nullptr);

    void setCompare(const QMap<QString, PixelMetrics>& cvPixel,
                    const QMap<QString, ImageMetrics>& cvImage,
                    const QMap<QString, PixelMetrics>& dlPixel,
                    const QMap<QString, ImageMetrics>& dlImage);
    void clear();

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap extraAt(int row) const;

private:
    struct Row {
        QString defect;
        QString cvPrecision;
        QString cvRecall;
        QString cvF1;
        QString cvIou;
        QString cvImageAcc;
        QString dlPrecision;
        QString dlRecall;
        QString dlF1;
        QString dlIou;
        QString dlImageAcc;
        QString deltaF1;
        bool isSummary = false;
    };

    QVector<Row> m_rows;
};
