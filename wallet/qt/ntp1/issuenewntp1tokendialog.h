#ifndef ISSUENEWNTP1TOKENDIALOG_H
#define ISSUENEWNTP1TOKENDIALOG_H

#include "ntp1createmetadatadialog.h"
#include <QCheckBox>
#include <QDialog>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <unordered_set>

#include "ntp1/ntp1script.h"
#include <QValidator>

class IssueNewNTP1TokenDialog;

class NTP1TokenSymbolValidator : public QValidator
{
    std::unordered_set<std::string> alreadyIssuedTokenSymbols;

    IssueNewNTP1TokenDialog& dialog;

public:
    State validate(QString& input, int& /*pos*/) const override;
    void  setAlreadyIssuedTokenSymbols(const std::unordered_set<std::string>& tokenSymbols);
    NTP1TokenSymbolValidator(IssueNewNTP1TokenDialog& isseNewNTP1Dialog, QObject* parent = Q_NULLPTR);
};

class IssueNewNTP1TokenDialog : public QDialog
{
    Q_OBJECT

    QGridLayout* mainLayout;

    QLabel*                   tokenSymbolLabel;
    QLineEdit*                tokenSymbolLineEdit;
    QLabel*                   tokenSymbolErrorLabel;
    QLabel*                   issuerLabel;
    QLineEdit*                issuerLineEdit;
    QLabel*                   tokenNameLabel;
    QLineEdit*                tokenNameLineEdit;
    QLabel*                   iconUrlLabel;
    QLineEdit*                iconUrlLineEdit;
    NTP1CreateMetadataDialog* metadataDialog;
    QPushButton*              editMetadataButton;
    QPushButton*              issueButton;
    QPushButton*              cancelButton;
    QPushButton*              clearButton;
    QCheckBox*                changeAddressCheckbox;
    QLineEdit*                changeAddressLineEdit;
    QLabel*                   costLabel;
    QFrame*                   paymentSeparator;

    NTP1TokenSymbolValidator* tokenSymbolValidator;

    void createWidgets();

public:
    IssueNewNTP1TokenDialog(QWidget* parent = 0);
    void clearData();
    void validateInput() const;
    void setAlreadyIssuedTokensSymbols(const std::unordered_set<std::string>& tokenSymbols);

    void setTokenSymbolValidatorErrorString(const QString& str);

private slots:
    void slot_clearData();
    void slot_modifyChangeAddressColor();
    void slot_changeAddressCheckboxToggled(bool checked);
};

#endif // ISSUENEWNTP1TOKENDIALOG_H
