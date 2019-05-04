#include "JsonNewNodeDialog.h"

#include "JsonTreeNode.h"
#include "json_spirit.h"

#include <algorithm>
#include <numeric>

JsonNewNodeDialog::JsonNewNodeDialog(QDialog* parent) : QDialog(parent)
{

    mainLayout = new QGridLayout(this);

    valueTypeComboBox = new QComboBox(this);
    keyLineEdit       = new QLineEdit(this);
    valLineEdit       = new QLineEdit(this);
    okButton          = new QPushButton("Create", this);
    cancelButton      = new QPushButton("Cancel", this);
    boolValCheckbox   = new QCheckBox("True", this);

    mainLayout->addWidget(valueTypeComboBox, 0, 0, 1, 2);
    mainLayout->addWidget(keyLineEdit, 1, 0, 1, 2);
    mainLayout->addWidget(valLineEdit, 2, 0, 1, 2);
    mainLayout->addWidget(boolValCheckbox, 2, 0, 1, 2);
    mainLayout->addWidget(okButton, 3, 0, 1, 1);
    mainLayout->addWidget(cancelButton, 3, 1, 1, 1);

    keyLineEdit->setPlaceholderText("Node name (key/label)");
    valLineEdit->setPlaceholderText("Value");

    for (int i = 0; i < JsonTypeCount; i++) {
        valueTypeComboBox->addItem(QString::fromStdString(JsonTreeNode::TypeToString(IntToJsonType(i))));
    }

    connect(valueTypeComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &JsonNewNodeDialog::implementFieldDisabledState);
    connect(okButton, &QPushButton::clicked, this, &JsonNewNodeDialog::exportValuesFromControls);
    connect(cancelButton, &QPushButton::clicked, this, &JsonNewNodeDialog::close);

    TestSwitchingIntToJsonType();
    clearFields();

    setModal(true);
}

void JsonNewNodeDialog::clearFields()
{
    implementFieldDisabledState(valueTypeComboBox->currentIndex());
    keyLineEdit->clear();
    valLineEdit->clear();
    valueTypeComboBox->setCurrentIndex(0);
    boolValCheckbox->setChecked(false);
    nodeCreated = false;
}

int JsonNewNodeDialog::JsonTypeToInt(json_spirit::Value_type t)
{
    switch (t) {
    case json_spirit::obj_type:
        return 0;
    case json_spirit::array_type:
        return 1;
    case json_spirit::str_type:
        return 2;
    case json_spirit::bool_type:
        return 3;
    case json_spirit::int_type:
        return 4;
    case json_spirit::real_type:
        return 5;
    case json_spirit::null_type:
        return 6;
    default:
        return 6;
    }
}

json_spirit::Value_type JsonNewNodeDialog::IntToJsonType(int i)
{
    switch (i) {
    case 0:
        return json_spirit::obj_type;
    case 1:
        return json_spirit::array_type;
    case 2:
        return json_spirit::str_type;
    case 3:
        return json_spirit::bool_type;
    case 4:
        return json_spirit::int_type;
    case 5:
        return json_spirit::real_type;
    case 6:
        return json_spirit::null_type;
    default:
        throw std::runtime_error("Unknown json integer type in " + std::string(__PRETTY_FUNCTION__));
    }
}

void JsonNewNodeDialog::TestSwitchingIntToJsonType()
{
    for (int i = 0; i < JsonTypeCount; i++) {
        int j = JsonTypeToInt(IntToJsonType(i));
        if (j != i) {
            throw std::runtime_error("Testing json index failed. Index " + std::to_string(i) +
                                     " returned " + std::to_string(j));
        }
    }
}

bool JsonNewNodeDialog::isNodeCreated() const { return nodeCreated; }

std::string JsonNewNodeDialog::getNodeKey() const { return nodeKey; }

std::string JsonNewNodeDialog::getNodeVal() const { return nodeVal; }

json_spirit::Value_type JsonNewNodeDialog::getNodeType() const { return nodeType; }

void JsonNewNodeDialog::setParentType(bool CanBeAnyType, json_spirit::Value_type t)
{
    canBeAnyType = CanBeAnyType;
    parentType   = t;
}

class Int64Validator : public QValidator
{

    // QValidator interface
public:
    State validate(QString& S, int&) const override
    {
        const std::string str = S.toStdString();
        try {
            if (std::any_of(str.cbegin(), str.cend(), [](char c) { return !isdigit(c); })) {
                return State::Invalid;
            }
            std::stoll(str);
            return State::Acceptable;
        } catch (...) {
            return Invalid;
        }
    }
};

void JsonNewNodeDialog::implementFieldDisabledState(int index)
{
    if (parentType == json_spirit::array_type) {
        keyLineEdit->setEnabled(false);
        //        valueTypeComboBox->setCurrentIndex(JsonTypeToInt(json_spirit::null_type));
    } else {
        keyLineEdit->setEnabled(true);
    }

    index = valueTypeComboBox->currentIndex(); // since it could be set above

    if (IntToJsonType(index) == json_spirit::bool_type) {
        valLineEdit->setVisible(false);
        boolValCheckbox->setVisible(true);
    } else {
        valLineEdit->setVisible(true);
        boolValCheckbox->setVisible(false);
    }

    // TODO: depending on parent type, we may decide whether there's value
    if (IntToJsonType(index) == json_spirit::obj_type) {
        valLineEdit->setEnabled(false);
    } else if (IntToJsonType(index) == json_spirit::array_type) {
        valLineEdit->setEnabled(false);
    } else if (IntToJsonType(index) == json_spirit::null_type) {
        valLineEdit->setEnabled(false);
    } else if (IntToJsonType(index) == json_spirit::int_type) {
        valLineEdit->setValidator(new Int64Validator());
        valLineEdit->setEnabled(true);
    } else if (IntToJsonType(index) == json_spirit::real_type) {
        valLineEdit->setValidator(new QDoubleValidator());
        valLineEdit->setEnabled(true);
    } else if (IntToJsonType(index) == json_spirit::bool_type) {
        valLineEdit->setValidator(nullptr);
        valLineEdit->setEnabled(true);
    } else {
        valLineEdit->setValidator(nullptr);
        valLineEdit->setEnabled(true);
    }
}

void JsonNewNodeDialog::exportValuesFromControls()
{
    nodeKey  = keyLineEdit->text().toStdString();
    nodeType = IntToJsonType(valueTypeComboBox->currentIndex());
    if (nodeType == json_spirit::Value_type::bool_type) {
        nodeVal = boolValCheckbox->isChecked() ? "true" : "false";
    } else {
        nodeVal = valLineEdit->text().toStdString();
    }
    nodeCreated = true;
    close();
}
