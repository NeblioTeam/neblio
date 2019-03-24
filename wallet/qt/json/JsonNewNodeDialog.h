#ifndef JSONNEWNODEDIALOG_H
#define JSONNEWNODEDIALOG_H

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include "json_spirit.h"

class JsonNewNodeDialog : public QDialog
{
    Q_OBJECT

    QGridLayout* mainLayout;
    QLineEdit*   keyLineEdit;
    QLineEdit*   valLineEdit;
    QComboBox*   valueTypeComboBox;
    QCheckBox*   boolValCheckbox;
    QPushButton* cancelButton;

    // if true, then the fields are committed to the vars below
    json_spirit::Value_type parentType = json_spirit::Value_type::null_type;
    // if insertion is in root, then it doesn't have to be object or array
    bool                    canBeAnyType = false;
    bool                    nodeCreated  = false;
    std::string             nodeKey;
    std::string             nodeVal;
    json_spirit::Value_type nodeType;

public:
    QPushButton* okButton;

public:
    JsonNewNodeDialog(QDialog* parent = nullptr);
    void clearFields();

    static const int JsonTypeCount = 7;

    static int                     JsonTypeToInt(json_spirit::Value_type t);
    static json_spirit::Value_type IntToJsonType(int i);
    static void                    TestSwitchingIntToJsonType();

    bool                    isNodeCreated() const;
    std::string             getNodeKey() const;
    std::string             getNodeVal() const;
    json_spirit::Value_type getNodeType() const;
    void setParentType(bool CanBeAnyType, json_spirit::Value_type t = json_spirit::null_type);

private slots:
    void implementFieldDisabledState(int index);
    void exportValuesFromControls();
};

#endif // JSONNEWNODEDIALOG_H
