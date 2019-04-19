#ifndef JSONTREEVIEW_H
#define JSONTREEVIEW_H

#include "JsonNewNodeDialog.h"
#include <QTreeView>

class JsonTreeModel;
class JsonTreeNode;

class JsonTreeView : public QTreeView
{
    Q_OBJECT

    QMenu*   contextMenu;
    QAction* addAboveAction;
    QAction* addBelowAction;
    QAction* addChildAction;
    QAction* moveNodeUpAction;
    QAction* moveNodeDownAction;
    QAction* copyAction;

    QModelIndex currentContextMenuIndex;

    JsonTreeModel* VerifyAndGetModel();

    enum AddNodeTarget
    {
        AddNode_Above,
        AddNode_Below,
        AddNode_Child
    };

    JsonNewNodeDialog* newNodeDialog;
    AddNodeTarget      addNodeTarget;

    bool canAddBesideNode(JsonTreeNode* node);
    bool canAddChildNode(JsonTreeNode* node);

public:
    JsonTreeView(QWidget* parent = nullptr);

private slots:
    void slot_onCustomContextMenu(const QPoint& point);
    void slot_nodeCreatedFromNewNodeMenu();
    void slot_addNodeAbove();
    void slot_addNodeBelow();
    void slot_addChildNode();
    void slot_moveNodeUp();
    void slot_moveNodeDown();
    void slot_copyText();
};

#endif // JSONTREEVIEW_H
