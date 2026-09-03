#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

// 模拟 DO 脉冲列表。检测线程不碰本模型；ViewModel 在 GUI 线程 append。
class DoPulseModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        SeqRole = Qt::UserRole + 1,
        PointRole,
        ActionRole,
        PulseMsRole,
        FileNameRole,
        DefectRole,
        LatencyRole,
        LateRole
    };

    struct Record {
        int seq = 0;
        QString point;
        QString action;
        int pulseMs = 100;
        QString fileName;
        QString defectLabel;
        qint64 latencyMs = 0;
        bool lateEject = false;
    };

    explicit DoPulseModel(QObject* parent = nullptr);

    void append(const Record& rec);
    void clear();
    int count() const { return m_rows.size(); }

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void countChanged();

private:
    QVector<Record> m_rows;
};
