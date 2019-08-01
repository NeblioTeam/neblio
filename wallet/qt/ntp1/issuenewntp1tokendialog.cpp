#include "issuenewntp1tokendialog.h"

#include "base58.h"
#include "bitcoinaddressvalidator.h"
#include "guiconstants.h"
#include "ntp1/ntp1transaction.h"
#include <QMessageBox>
#include <QVariant>
#include <util.h>

void IssueNewNTP1TokenDialog::createWidgets()
{
    setWindowTitle("Issue a new NTP1 token");

    mainLayout = new QGridLayout(this);

    this->setLayout(mainLayout);

    tokenSymbolLabel      = new QLabel("Token symbol", this);
    tokenSymbolLineEdit   = new QLineEdit(this);
    tokenSymbolErrorLabel = new QLabel("", this);
    tokenNameLabel        = new QLabel("Token name", this);
    tokenNameLineEdit     = new QLineEdit(this);
    issuerLabel           = new QLabel("Issuer", this);
    issuerLineEdit        = new QLineEdit(this);
    iconUrlLabel          = new QLabel("Icon URL", this);
    iconUrlLineEdit       = new QLineEdit(this);
    metadataDialog        = new NTP1CreateMetadataDialog(this);
    editMetadataButton    = new QPushButton("Edit issuance metadata", this);
    issueButton           = new QPushButton("Create", this);
    cancelButton          = new QPushButton("Cancel", this);
    clearButton           = new QPushButton("Clear", this);
    changeAddressLineEdit = new QLineEdit(this);
    changeAddressCheckbox = new QCheckBox("Send the change from this transaction to a specific address");
    changeAddressLineEdit->setPlaceholderText("Change address");
    costLabel = new QLabel(this);
    costLabel->setText("Issuing a token will cost " +
                       QString::number(NTP1Transaction::IssuanceFee / COIN) + " NEBL");
    costLabel->setAlignment(Qt::AlignHCenter);
    paymentSeparator = new QFrame(this);
    paymentSeparator->setFrameShape(QFrame::HLine);
    paymentSeparator->setFrameShadow(QFrame::Sunken);

    iconUrlLabel->setText(
        R"****(Icon URL <span style="color:red;">(Warning: This can never be changed in the future)</span>)****");

    changeAddressLineEdit->setValidator(new BitcoinAddressValidator);

    editMetadataButton->setAutoDefault(false);
    issueButton->setAutoDefault(false);
    cancelButton->setAutoDefault(false);
    clearButton->setAutoDefault(false);

    iconUrlLineEdit->setPlaceholderText("https://www.example.com/somedir/icon.png");

    int row = 0;
    mainLayout->addWidget(costLabel, row++, 0, 1, 3);
    mainLayout->addWidget(tokenSymbolLabel, row++, 0, 1, 3);
    mainLayout->addWidget(tokenSymbolLineEdit, row++, 0, 1, 3);
    mainLayout->addWidget(tokenSymbolErrorLabel, row++, 0, 1, 3);
    mainLayout->addWidget(tokenNameLabel, row++, 0, 1, 3);
    mainLayout->addWidget(tokenNameLineEdit, row++, 0, 1, 3);
    mainLayout->addWidget(issuerLabel, row++, 0, 1, 3);
    mainLayout->addWidget(issuerLineEdit, row++, 0, 1, 3);
    mainLayout->addWidget(iconUrlLabel, row++, 0, 1, 3);
    mainLayout->addWidget(iconUrlLineEdit, row++, 0, 1, 3);
    mainLayout->addWidget(editMetadataButton, row++, 0, 1, 3);
    mainLayout->addWidget(paymentSeparator, row++, 0, 1, 3);
    mainLayout->addWidget(changeAddressCheckbox, row++, 0, 1, 3);
    mainLayout->addWidget(changeAddressLineEdit, row++, 0, 1, 3);
    mainLayout->addWidget(issueButton, row, 0, 1, 1);
    mainLayout->addWidget(clearButton, row, 1, 1, 1);
    mainLayout->addWidget(cancelButton, row, 2, 1, 1);

    tokenSymbolValidator = new NTP1TokenSymbolValidator(*this, this);

    tokenSymbolLineEdit->setValidator(tokenSymbolValidator);

    connect(this->clearButton, &QPushButton::clicked, this, &IssueNewNTP1TokenDialog::slot_clearData);
    connect(this->changeAddressLineEdit, &QLineEdit::textChanged, this,
            &IssueNewNTP1TokenDialog::slot_modifyChangeAddressColor);
    connect(this->changeAddressCheckbox, &QCheckBox::toggled, this,
            &IssueNewNTP1TokenDialog::slot_changeAddressCheckboxToggled);
    connect(this->cancelButton, &QPushButton::clicked, this, &IssueNewNTP1TokenDialog::hide);
    connect(this->editMetadataButton, &QPushButton::clicked, this->metadataDialog,
            &NTP1CreateMetadataDialog::show);

    slot_changeAddressCheckboxToggled(changeAddressCheckbox->isChecked());
}

IssueNewNTP1TokenDialog::IssueNewNTP1TokenDialog(QWidget* parent) : QDialog(parent) { createWidgets(); }

void IssueNewNTP1TokenDialog::clearData()
{
    tokenNameLineEdit->clear();
    tokenSymbolLineEdit->clear();
    issuerLineEdit->clear();
    iconUrlLineEdit->clear();
    metadataDialog->clearData();
    changeAddressLineEdit->clear();
}

void IssueNewNTP1TokenDialog::validateInput() const
{
    if (changeAddressCheckbox->isChecked() &&
        !CBitcoinAddress(changeAddressLineEdit->text().toStdString()).IsValid()) {
        throw std::runtime_error("Invalid change address provided");
    }
}

void IssueNewNTP1TokenDialog::setAlreadyIssuedTokensSymbols(
    const std::unordered_set<string>& tokenSymbols)
{
    tokenSymbolValidator->setAlreadyIssuedTokenSymbols(tokenSymbols);
}

void IssueNewNTP1TokenDialog::setTokenSymbolValidatorErrorString(const QString& str)
{
    tokenSymbolErrorLabel->setStyleSheet("color:red");
    tokenSymbolErrorLabel->setText(str);
    if (str.isEmpty()) {
        tokenSymbolErrorLabel->setVisible(false);
    } else {
        tokenSymbolErrorLabel->setVisible(true);
    }
}

void IssueNewNTP1TokenDialog::slot_clearData()
{
    if (QMessageBox::question(this, "Clear all?",
                              "Are you sure you want to clear all the data in this dialog?") ==
        QMessageBox::Yes) {
        clearData();
    }
}

void IssueNewNTP1TokenDialog::slot_modifyChangeAddressColor()
{
    if (CBitcoinAddress(changeAddressLineEdit->text().toStdString()).IsValid()) {
        changeAddressLineEdit->setStyleSheet("");
    } else {
        changeAddressLineEdit->setStyleSheet(STYLE_INVALID);
    }
}

void IssueNewNTP1TokenDialog::slot_changeAddressCheckboxToggled(bool checked)
{
    changeAddressLineEdit->setEnabled(checked);
    if (!checked) {
        changeAddressLineEdit->setStyleSheet("");
    }
}

QValidator::State NTP1TokenSymbolValidator::validate(QString& input, int&) const
{
    input = input.toUpper();
    if (input.isEmpty()) {
        return State::Intermediate;
    }
    if (input.length() > 5) {
        return State::Invalid;
    }
    if (alreadyIssuedTokenSymbols.find(input.toStdString()) != alreadyIssuedTokenSymbols.cend()) {
        dialog.setTokenSymbolValidatorErrorString("A token with symbol " + input + " already exists");
        return State::Intermediate;
    } else {
        dialog.setTokenSymbolValidatorErrorString("");
    }
    if (NTP1Script::IsNTP1TokenSymbolValid(input.toStdString())) {

        return State::Acceptable;
    } else {
        return State::Invalid;
    }
}

void NTP1TokenSymbolValidator::setAlreadyIssuedTokenSymbols(
    const std::unordered_set<string>& tokenSymbols)
{
    alreadyIssuedTokenSymbols = tokenSymbols;
}

NTP1TokenSymbolValidator::NTP1TokenSymbolValidator(IssueNewNTP1TokenDialog& isseNewNTP1Dialog,
                                                   QObject*                 parent)
    : QValidator(parent), dialog(isseNewNTP1Dialog)
{
}
