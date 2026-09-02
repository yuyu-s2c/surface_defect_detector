#pragma once

#include "ResultEvaluator.h"

#include <QAbstractListModel>
#include <QMap>
#include <QString>
#include <QVector>

class CompareListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        DefectRole = Qt::UserRole + 1,
        CvPrecisionRole,
        CvRecallRole,
        CvF1Role,
        CvIouRole,
        CvImageAccRole,
        DlPrecisionRole,
        DlRecallRole,
        DlF1Role,
        DlIouRole,
        DlImageAccRole,
        DeltaF1Role,
        IsSummaryRole,
        IsGoodFprRole
    };

    explicit CompareListModel(QObject* parent = nullptr);

    void setCompare(const QMap<QString, PixelMetrics>& cvPixel,
                    const QMap<QString, ImageMetrics>& cvImage,
                    const QMap<QString, PixelMetrics>& dlPixel,
                    const QMap<QString, ImageMetrics>& dlImage);
    void clear();

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

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
        bool isGoodFpr = false;
    };

    QVector<Row> m_rows;
};
