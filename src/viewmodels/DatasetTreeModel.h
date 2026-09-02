#pragma once

#include <QAbstractItemModel>
#include <QString>
#include <QVector>

class DatasetManager;

// 三层树：类别 → 缺陷类型 → 测试图。角色给 QML TreeView 用。
class DatasetTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Role {
        NodeTypeRole = Qt::UserRole + 1,
        CategoryRole,
        DefectTypeRole,
        ImagePathRole,
        CountRole
    };

    explicit DatasetTreeModel(QObject* parent = nullptr);
    ~DatasetTreeModel() override;

    void rebuild(const DatasetManager& dataset);
    void clear();

    Q_INVOKABLE QString nodeType(const QModelIndex& index) const;
    Q_INVOKABLE QString displayName(const QModelIndex& index) const;
    Q_INVOKABLE int nodeCount(const QModelIndex& index) const;

    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    struct Node {
        Node* parent = nullptr;
        QVector<Node*> children;
        QString display;
        QString nodeType;
        QString category;
        QString defectType;
        QString imagePath;
        int count = 0;
    };

    Node* nodeFromIndex(const QModelIndex& index) const;
    void deleteChildren(Node* node);

    Node* m_root = nullptr;
};
