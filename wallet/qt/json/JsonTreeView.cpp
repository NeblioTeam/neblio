#include "JsonTreeView.h"
#include "JsonTreeModel.h"
#include "QHeaderView"
#include <QClipboard>
#include <QGuiApplication>
#include <QMenu>
#include <QMessageBox>

JsonTreeView::JsonTreeView(QWidget* parent) : QTreeView(parent)
{
    header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    setContextMenuPolicy(Qt::CustomContextMenu);

    contextMenu = new QMenu(this);

    addAboveAction     = new QAction("Add node above", this);
    addBelowAction     = new QAction("Add node below", this);
    addChildAction     = new QAction("Add child node", this);
    moveNodeUpAction   = new QAction("Move node up", this);
    moveNodeDownAction = new QAction("Move node down", this);
    copyAction         = new QAction("Copy text", this);

    newNodeDialog = new JsonNewNodeDialog;

    //    contextMenu->addAction(addAboveAction);
    //    contextMenu->addAction(addBelowAction);
    //    contextMenu->addAction(addChildAction);
    //    contextMenu->addSeparator();
    //    contextMenu->addAction(moveNodeUpAction);
    //    contextMenu->addAction(moveNodeDownAction);
    //    contextMenu->addSeparator();
    contextMenu->addAction(copyAction);

    setSelectionBehavior(QTreeView::SelectItems);

    connect(this, &JsonTreeView::customContextMenuRequested, this,
            &JsonTreeView::slot_onCustomContextMenu);
    connect(addAboveAction, &QAction::triggered, this, &JsonTreeView::slot_addNodeAbove);
    connect(addBelowAction, &QAction::triggered, this, &JsonTreeView::slot_addNodeBelow);
    connect(addChildAction, &QAction::triggered, this, &JsonTreeView::slot_addChildNode);
    connect(moveNodeUpAction, &QAction::triggered, this, &JsonTreeView::slot_moveNodeUp);
    connect(moveNodeDownAction, &QAction::triggered, this, &JsonTreeView::slot_moveNodeDown);
    connect(copyAction, &QAction::triggered, this, &JsonTreeView::slot_copyText);

    connect(newNodeDialog->okButton, &QPushButton::clicked, this,
            &JsonTreeView::slot_nodeCreatedFromNewNodeMenu);
}

JsonTreeModel* JsonTreeView::VerifyAndGetModel()
{
    if (!model()) {
        throw std::runtime_error("JsonTreeModel not initialized");
    }
    auto           mo = model();
    JsonTreeModel* m  = dynamic_cast<JsonTreeModel*>(mo);
    if (m) {
        return m;
    }
    throw std::runtime_error("Casting of JsonTreeModel failed");
}

bool JsonTreeView::canAddBesideNode(JsonTreeNode* node)
{
    JsonTreeNode* parent = node->getParentObject();
    return parent->getType() == json_spirit::obj_type || parent->getType() == json_spirit::array_type ||
           !parent->isRealJsonNode();
}

bool JsonTreeView::canAddChildNode(JsonTreeNode* node)
{
    return node->getType() == json_spirit::obj_type || node->getType() == json_spirit::array_type ||
           !node->isRealJsonNode();
}

void JsonTreeView::slot_onCustomContextMenu(const QPoint& point)
{
    JsonTreeModel*  m        = VerifyAndGetModel();
    QModelIndexList selected = selectedIndexes();
    if (selected.empty()) {
        return;
    }

    JsonTreeNode* selectedItem = (JsonTreeNode*)selected[0].internalPointer();
    /// selected item can be:
    /// 1. Value node (str, int, real, null)
    /// 2. array
    /// 3. array member number (not a valid json node)
    /// 4. obj
    ///

    if (canAddChildNode(selectedItem)) {
        addChildAction->setEnabled(true);
    } else {
        addChildAction->setEnabled(false);
    }

    if (canAddBesideNode(selectedItem)) {
        addAboveAction->setEnabled(true);
        addBelowAction->setEnabled(true);
    } else {
        addAboveAction->setEnabled(false);
        addBelowAction->setEnabled(false);
    }

    copyAction->setEnabled(true);
    if (m->isEditable()) {
        currentContextMenuIndex = indexAt(point);
        if (currentContextMenuIndex.isValid()) {
            moveNodeUpAction->setEnabled(true);
            moveNodeDownAction->setEnabled(true);
        } else {
            moveNodeUpAction->setEnabled(false);
            moveNodeDownAction->setEnabled(false);
        }
    } else {
        addChildAction->setEnabled(false);
        addAboveAction->setEnabled(false);
        addBelowAction->setEnabled(false);
        moveNodeUpAction->setEnabled(false);
        moveNodeDownAction->setEnabled(false);
    }
    contextMenu->exec(viewport()->mapToGlobal(point));
}

void JsonTreeView::slot_nodeCreatedFromNewNodeMenu()
{
    JsonTreeModel* m = VerifyAndGetModel();
    if (newNodeDialog->isNodeCreated()) {
        std::string             key    = newNodeDialog->getNodeKey();
        std::string             val    = newNodeDialog->getNodeVal();
        json_spirit::Value_type type   = newNodeDialog->getNodeType();
        bool                    status = false;
        if (addNodeTarget == AddNode_Child) {
            JsonTreeNode* item = (JsonTreeNode*)currentContextMenuIndex.internalPointer();
            status = m->addNode(item->countChildren(), key, val, type, currentContextMenuIndex);
        } else if (addNodeTarget == AddNode_Above) {
            QModelIndexList selected = selectedIndexes();
            if (selected.empty()) {
                return;
            }
            int targetPos = selected[0].row();
            status        = m->addNode(targetPos, key, val, type, currentContextMenuIndex.parent());
        } else if (addNodeTarget == AddNode_Below) {
            QModelIndexList selected = selectedIndexes();
            if (selected.empty()) {
                return;
            }
            int targetPos = selected[0].row();
            status        = m->addNode(targetPos + 1, key, val, type, currentContextMenuIndex.parent());
        } else {
            throw std::runtime_error("Unknown add position in " + std::string(__PRETTY_FUNCTION__));
        }
        if (!status) {
            QMessageBox::warning(this, "Failed to add node", "failed to add node to the json tree");
        }
    }
}

void JsonTreeView::slot_addNodeAbove()
{
    addNodeTarget = AddNode_Above;
    newNodeDialog->show();
    newNodeDialog->raise();
    QModelIndexList selected = selectedIndexes();
    if (selected.empty()) {
        return;
    }
    JsonTreeNode* item       = ((JsonTreeNode*)selected[0].internalPointer());
    JsonTreeNode* parentItem = item->getParentObject();
    newNodeDialog->setParentType(parentItem->isRealJsonNode(), parentItem->getType());
    newNodeDialog->clearFields();
}

void JsonTreeView::slot_addNodeBelow()
{
    addNodeTarget = AddNode_Below;
    newNodeDialog->show();
    newNodeDialog->raise();
    QModelIndexList selected = selectedIndexes();
    if (selected.empty()) {
        return;
    }
    JsonTreeNode* item       = ((JsonTreeNode*)selected[0].internalPointer());
    JsonTreeNode* parentItem = item->getParentObject();
    newNodeDialog->setParentType(parentItem->isRealJsonNode(), parentItem->getType());
    newNodeDialog->clearFields();
}

void JsonTreeView::slot_addChildNode()
{
    // TODO: child is only allowed for objects and arrays
    addNodeTarget = AddNode_Child;
    newNodeDialog->show();
    newNodeDialog->raise();
    QModelIndexList selected = selectedIndexes();
    if (selected.empty()) {
        return;
    }
    JsonTreeNode* item       = (JsonTreeNode*)selected[0].internalPointer();
    JsonTreeNode* parentItem = item->getParentObject();
    newNodeDialog->setParentType(item->isRealJsonNode(), parentItem->getType());
    newNodeDialog->clearFields();
}

void JsonTreeView::slot_moveNodeUp()
{
    JsonTreeModel* m = VerifyAndGetModel();
    m->moveNodeUp(currentContextMenuIndex);
}

void JsonTreeView::slot_moveNodeDown()
{
    JsonTreeModel* m = VerifyAndGetModel();
    m->moveNodeDown(currentContextMenuIndex);
}

void JsonTreeView::slot_copyText()
{
    QModelIndexList selected = this->selectedIndexes();
    if (selected.size() <= 0 || selected.size() > 1) {
        return;
    }

    if (currentContextMenuIndex.isValid()) {
        QString textToCopy = currentContextMenuIndex.data(Qt::DisplayRole).toString();
        QGuiApplication::clipboard()->setText(textToCopy);
    }
}
