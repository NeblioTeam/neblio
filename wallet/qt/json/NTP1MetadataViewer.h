#ifndef NTP1METADATAVIEWER_H
#define NTP1METADATAVIEWER_H

#include "qt/json/JsonTreeModel.h"
#include "qt/json/JsonTreeView.h"
#include <QDialog>
#include <QGridLayout>
#include <QPlainTextEdit>
#include <QPushButton>

class NTP1MetadataViewer : public QDialog
{
    Q_OBJECT

    QGridLayout* mainLayout;
    QPushButton* switchTreeTextViewButton;

    JsonTreeView*   treeView;
    QPlainTextEdit* textView;
    JsonTreeModel   treeModel;

    void setRoot(std::shared_ptr<JsonTreeNode> root);

public:
    NTP1MetadataViewer(QWidget* parent = 0);

    void setJsonStr(const std::string& jsonStr);

private slots:
    void slot_switchTreeTextView();
};

#endif // NTP1METADATAVIEWER_H
