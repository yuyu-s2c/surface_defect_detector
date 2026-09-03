#pragma once

#include "IDetectionEngine.h"

#include <QAbstractListModel>
#include <QString>
#include <QVector>

// 取流不合格列表。点选后 ViewModel 把该张加载到画布。
class NgListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int selectedRow READ selectedRow NOTIFY selectedRowChanged)

public:
    enum Role {
        FileNameRole = Qt::UserRole + 1,
        DefectRole,
        ScoreRole,
        LatencyRole,
        LateRole,
        PathRole,
        CategoryRole,
        SelectedRole
    };

    struct Record {
        QString category;
        QString defectType;
        QString path;
        QString fileName;
        QString defectLabel;
        double imageScore = 0.0;
        double imageThreshold = 0.0;
        qint64 latencyMs = 0;
        bool lateEject = false;
        DetectionResult result;
    };

    explicit NgListModel(QObject* parent = nullptr);

    void append(const Record& rec);
    void clear();
    const Record* recordAt(int row) const;
    int count() const { return m_rows.size(); }
    int selectedRow() const { return m_selected; }
    void setSelectedRow(int row);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void countChanged();
    void selectedRowChanged();

private:
    QVector<Record> m_rows;
    int m_selected = -1;
};
