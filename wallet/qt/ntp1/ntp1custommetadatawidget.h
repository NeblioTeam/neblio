#ifndef NTP1CUSTOMMETADATAWIDGET_H
#define NTP1CUSTOMMETADATAWIDGET_H

#include "json_spirit.h"
#include "qt/json/JsonTreeModel.h"
#include "qt/json/JsonTreeNode.h"
#include "qt/json/JsonTreeView.h"
#include <QGridLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QWidget>

class NTP1CustomMetadataWidget : public QWidget
{
    Q_OBJECT

    QGridLayout* mainLayout;

    QPlainTextEdit* jsonDataLineEdit;
    QPushButton*    clearButton;
    QPushButton*    okButton;
    QPushButton*    viewAsTreeButton;
    JsonTreeView*   jsonTreeView;
    JsonTreeModel*  jsonTreeModel;
    bool            isTreeViewActive = false;

public:
    NTP1CustomMetadataWidget(QWidget* parent = Q_NULLPTR);
    json_spirit::Object getJsonObject() const;
    bool                isJsonEmpty() const;
    bool                isJsonValid() const;
    void                clearData();

signals:
    void sig_okPressed();

public slots:
    void slot_okPressed();
    void slot_clearPressed();
    void slot_viewAsTreePressed();
    void slot_setViewTextOrTree();
};

#endif // NTP1CUSTOMMETADATAWIDGET_H
