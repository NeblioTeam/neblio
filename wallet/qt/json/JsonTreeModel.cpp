#include "JsonTreeModel.h"

#include "JsonTreeNode.h"
#include <QApplication>
#include <QColor>
#include <QStyle>
#include <string>

JsonTreeModel::JsonTreeModel(std::shared_ptr<JsonTreeNode> RootItem, bool Editable, QObject* parent)
    : QAbstractItemModel(parent)
{
    setRoot(RootItem);
    editable = Editable;
}

JsonTreeModel::~JsonTreeModel() {}

QVariant JsonTreeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    JsonTreeNode* item = static_cast<JsonTreeNode*>(index.internalPointer());
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        if (index.column() == 0)
            return QString::fromStdString(item->getKey());

        if (index.column() == 1)
            return QString::fromStdString(item->getValue());

        if (index.column() == 2)
            return QString::fromStdString(JsonTreeNode::NodeTypeToString(item));
    } else if (Qt::EditRole == role) {
        if (index.column() == 1) {
            return QString::fromStdString(item->getValue());
        }
    }

    if (role == Qt::ForegroundRole) {
        if (item->isCompoundType() && index.column() == 1) {
            QColor c = qApp->style()->standardPalette().text().color();
            c.setAlpha(128);
            return c;
        }
    }

    return QVariant();
}

QVariant JsonTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        if (section == 0)
            return "Key";
        else if (section == 1)
            return "Value";
        else if (section == 2)
            return "Type";
    }

    return QVariant();
}

QModelIndex JsonTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (parent.isValid() && parent.column() != 0)
        return QModelIndex();

    JsonTreeNode* parentItem = getItem(parent);

    JsonTreeNode* childItem = nullptr;
    if (row < static_cast<int>(parentItem->countChildren())) {
        childItem = parentItem->at(row).get();
    }

    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

QModelIndex JsonTreeModel::parent(const QModelIndex& index) const
{
    if (!index.isValid())
        return QModelIndex();

    JsonTreeNode* childItem  = getItem(index);
    JsonTreeNode* parentItem = childItem->getParentObject();

    if (parentItem == rootItem.get())
        return QModelIndex();

    auto idx = parentItem->getThisObjNumberInParent();
    return createIndex(idx, 0, parentItem);
}

int JsonTreeModel::rowCount(const QModelIndex& parent) const
{
    JsonTreeNode* parentItem = getItem(parent);

    if (!parent.isValid())
        parentItem = rootItem.get();
    else
        parentItem = static_cast<JsonTreeNode*>(parent.internalPointer());

    return parentItem->countChildren();
}

int JsonTreeModel::columnCount(const QModelIndex& /*parent*/) const { return 3; }

Qt::ItemFlags JsonTreeModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return 0;

    QFlags<Qt::ItemFlag> res = Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDragEnabled |
                               QAbstractItemModel::flags(index);

    if (!editable || index.column() == 2) {
        // type column is not editable anyway
        return res & ~Qt::ItemIsEditable;
    }

    JsonTreeNode* n = getItem(index);
    if (index.column() == 1) {
        // value
        if (n->getType() == json_spirit::Value_type::obj_type ||
            n->getType() == json_spirit::Value_type::array_type) {
            // not editable
            return res & ~Qt::ItemIsEditable;
        }
    }

    if (index.column() == 0) {
        // key
        if (!n->isRealJsonNode()) {
            // not editable
            return res & ~Qt::ItemIsEditable;
        }
    }

    return res;
}

bool JsonTreeModel::setData(const QModelIndex& index, const QVariant& value, int /*role*/)
{
    if (!editable) {
        return false;
    }

    if (!index.isValid())
        return false;

    if (index.column() < 0 || index.column() >= columnCount())
        return false;

    JsonTreeNode* n = getItem(index);
    if (index.column() == 0) {
        // key
        if (value.toString().size() == 0) {
            return false; // keys cannot be empty
        }
        n->setKey(value.toString().toStdString());
        return true;
    }

    if (index.column() == 1) {
        // value
        if (n->getType() != json_spirit::Value_type::obj_type &&
            n->getType() != json_spirit::Value_type::array_type) {
            n->setValue(value.toString().toStdString());
            return true;
        } else {
            return false;
        }
    }

    return false;
}

bool JsonTreeModel::insertRows(int position, int /*rows*/, const QModelIndex& parent)
{
    if (position < 0) {
        return false;
    }
    if (!toInsert) {
        return false;
    }
    if (parent.isValid()) {
        JsonTreeNode* item = static_cast<JsonTreeNode*>(parent.internalPointer());
        if (!item) {
            return false;
        }
        beginInsertRows(parent, position, position);
        item->insertChild(toInsert, position);
        endInsertRows();
        return true;
    } else {
        emit layoutAboutToBeChanged();
        rootItem->insertChild(toInsert, position);
        emit layoutChanged();
        return true;
    }
    return false;
}

void JsonTreeModel::setRoot(std::shared_ptr<JsonTreeNode> RootItem)
{
    beginResetModel();
    if (RootItem == nullptr) {
        this->rootItem.reset();
        this->rootItem = DefaultRoot;
    } else {
        this->rootItem.reset();
        this->rootItem = RootItem;
    }
    endResetModel();
}

std::shared_ptr<JsonTreeNode> JsonTreeModel::getRoot() const { return rootItem; }

JsonTreeNode* JsonTreeModel::getItem(const QModelIndex& index) const
{
    if (index.isValid()) {
        JsonTreeNode* item = static_cast<JsonTreeNode*>(index.internalPointer());
        if (item)
            return item;
    }
    return rootItem.get();
}

void JsonTreeModel::setEditable(bool val) { editable = val; }

bool JsonTreeModel::isEditable() const { return editable; }

bool JsonTreeModel::addNode(int position, const std::string& key, const std::string& value,
                            json_spirit::Value_type type, const QModelIndex& parent)
{
    toInsert = std::make_shared<JsonTreeNode>();
    toInsert->setKey(key);
    toInsert->setValue(value);
    toInsert->setType(type);
    return insertRows(position, 1, parent);
}

bool JsonTreeModel::moveNodeUp(const QModelIndex& parent)
{
    if (parent.isValid()) {
        JsonTreeNode* item = static_cast<JsonTreeNode*>(parent.internalPointer());
        if (item) {
            int idx = item->getThisObjNumberInParent();
            if (idx == 0) {
                return false;
            }
            beginMoveRows(parent.parent(), idx, idx, parent.parent(), idx - 1);
            item->getParentObject()->swapChildren(idx, idx - 1);
            endMoveRows();
            return true;
        } else {
            return false;
        }
    }
    return false;
}

bool JsonTreeModel::moveNodeDown(const QModelIndex& parent)
{
    if (parent.isValid()) {
        JsonTreeNode* item = static_cast<JsonTreeNode*>(parent.internalPointer());
        if (item) {
            int idx = item->getThisObjNumberInParent();
            if (idx + 1 == (int)item->getParentObject()->countChildren()) {
                return false;
            }
            // +2 because it's target position; see: https://doc.qt.io/qt-5/qabstractitemmodel.html
            beginMoveRows(parent.parent(), idx, idx, parent.parent(), idx + 2);
            item->getParentObject()->swapChildren(idx, idx + 1);
            endMoveRows();
            return true;
        } else {
            return false;
        }
    }
    return false;
}
