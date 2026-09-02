#pragma once

#include <QAbstractListModel>
#include <QVector>

class BoxListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        XRole = Qt::UserRole + 1,
        YRole,
        WidthRole,
        HeightRole,
        AreaRole
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
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    QVector<Row> m_rows;
};
