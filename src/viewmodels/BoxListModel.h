#pragma once

#include <QAbstractTableModel>
#include <QVector>

// 表格模型：列走 DisplayRole，给 TableView / HorizontalHeaderView 共用列宽。
class BoxListModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        ColNo = 0,
        ColX,
        ColY,
        ColW,
        ColH,
        ColArea,
        ColumnCount
    };

    struct Row {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        double area = 0.0;
    };

    explicit BoxListModel(QObject* parent = nullptr);

    void setRows(const QVector<Row>& rows);
    void clear();

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    QVector<Row> m_rows;
};
