#include "DatasetTreeModel.h"

#include "DatasetManager.h"

#include <QFileInfo>

DatasetTreeModel::DatasetTreeModel(QObject* parent)
    : QAbstractItemModel(parent)
    , m_root(new Node)
{
}

DatasetTreeModel::~DatasetTreeModel()
{
    deleteChildren(m_root);
    delete m_root;
}

void DatasetTreeModel::deleteChildren(Node* node)
{
    if (!node)
        return;
    for (Node* child : node->children) {
        deleteChildren(child);
        delete child;
    }
    node->children.clear();
}

QString DatasetTreeModel::nodeType(const QModelIndex& index) const
{
    return data(index, NodeTypeRole).toString();
}

QString DatasetTreeModel::displayName(const QModelIndex& index) const
{
    return data(index, Qt::DisplayRole).toString();
}

int DatasetTreeModel::nodeCount(const QModelIndex& index) const
{
    return data(index, CountRole).toInt();
}

void DatasetTreeModel::clear()
{
    beginResetModel();
    deleteChildren(m_root);
    endResetModel();
}

void DatasetTreeModel::rebuild(const DatasetManager& dataset)
{
    beginResetModel();
    deleteChildren(m_root);

    const QStringList cats = dataset.categories();
    for (const QString& cat : cats) {
        auto* catNode = new Node;
        catNode->parent = m_root;
        catNode->display = cat;
        catNode->nodeType = QStringLiteral("category");
        catNode->category = cat;

        int catCount = 0;
        const QStringList defects = dataset.defectTypes(cat);
        for (const QString& defect : defects) {
            const QStringList images = dataset.testImages(cat, defect);
            auto* defNode = new Node;
            defNode->parent = catNode;
            defNode->display = defect;
            defNode->nodeType = QStringLiteral("defect");
            defNode->category = cat;
            defNode->defectType = defect;
            defNode->count = images.size();
            catCount += images.size();

            for (const QString& img : images) {
                auto* imgNode = new Node;
                imgNode->parent = defNode;
                imgNode->display = QFileInfo(img).fileName();
                imgNode->nodeType = QStringLiteral("image");
                imgNode->category = cat;
                imgNode->defectType = defect;
                imgNode->imagePath = img;
                defNode->children.push_back(imgNode);
            }
            catNode->children.push_back(defNode);
        }
        catNode->count = catCount;
        m_root->children.push_back(catNode);
    }
    endResetModel();
}

DatasetTreeModel::Node* DatasetTreeModel::nodeFromIndex(const QModelIndex& index) const
{
    if (!index.isValid())
        return m_root;
    return static_cast<Node*>(index.internalPointer());
}

QModelIndex DatasetTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column != 0 || row < 0)
        return {};
    Node* parentNode = nodeFromIndex(parent);
    if (!parentNode || row >= parentNode->children.size())
        return {};
    return createIndex(row, column, parentNode->children.at(row));
}

QModelIndex DatasetTreeModel::parent(const QModelIndex& child) const
{
    if (!child.isValid())
        return {};
    Node* node = static_cast<Node*>(child.internalPointer());
    if (!node || !node->parent || node->parent == m_root)
        return {};
    Node* parentNode = node->parent;
    Node* grand = parentNode->parent;
    const int row = grand ? grand->children.indexOf(parentNode) : -1;
    if (row < 0)
        return {};
    return createIndex(row, 0, parentNode);
}

int DatasetTreeModel::rowCount(const QModelIndex& parent) const
{
    Node* node = nodeFromIndex(parent);
    return node ? node->children.size() : 0;
}

int DatasetTreeModel::columnCount(const QModelIndex&) const
{
    return 1;
}

Qt::ItemFlags DatasetTreeModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant DatasetTreeModel::data(const QModelIndex& index, int role) const
{
    Node* node = nodeFromIndex(index);
    if (!node || node == m_root)
        return {};
    switch (role) {
    case Qt::DisplayRole:
        return node->display;
    case NodeTypeRole:
        return node->nodeType;
    case CategoryRole:
        return node->category;
    case DefectTypeRole:
        return node->defectType;
    case ImagePathRole:
        return node->imagePath;
    case CountRole:
        return node->count;
    default:
        return {};
    }
}

QHash<int, QByteArray> DatasetTreeModel::roleNames() const
{
    return {
        {Qt::DisplayRole, "display"},
        {NodeTypeRole, "nodeType"},
        {CategoryRole, "category"},
        {DefectTypeRole, "defectType"},
        {ImagePathRole, "imagePath"},
        {CountRole, "count"},
    };
}
