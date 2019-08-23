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

#include "json/json_spirit.h"

class IssueNewNTP1TokenDialog;
class CCoinControl;
class CoinControlDialog;
class WalletModel;

class NTP1TokenSymbolValidator : public QValidator
{
    std::unordered_set<std::string> alreadyIssuedTokenSymbols;

    IssueNewNTP1TokenDialog& dialog;

public:
    State validate(QString& input, int& /*pos*/) const override;
    void  setAlreadyIssuedTokenSymbols(const std::unordered_set<std::string>& tokenSymbols);
    bool  tokenWithSymbolAlreadyIssued(const std::string& tokenSymbol);
    NTP1TokenSymbolValidator(IssueNewNTP1TokenDialog& isseNewNTP1Dialog, QObject* parent = Q_NULLPTR);
};

class NTP1TokenAmountValidator : public QValidator
{
public:
    NTP1TokenAmountValidator(QObject* parent = 0);
    State validate(QString& input, int& /*pos*/) const override;
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
    QLabel*                   amountLabel;
    QLineEdit*                amountLineEdit;
    QLabel*                   iconUrlLabel;
    QLabel*                   iconUrlMimeTypeLabel;
    QLineEdit*                iconUrlLineEdit;
    NTP1CreateMetadataDialog* metadataDialog;
    QPushButton*              editMetadataButton;
    QPushButton*              issueButton;
    QPushButton*              cancelButton;
    QPushButton*              clearButton;
    QLabel*                   targetAddressLabel;
    QLineEdit*                targetAddressLineEdit;
    QCheckBox*                changeAddressCheckbox;
    QLineEdit*                changeAddressLineEdit;
    QLabel*                   costLabel;
    QFrame*                   paymentSeparator;

    NTP1TokenSymbolValidator* tokenSymbolValidator;

    CoinControlDialog* coinControlDialog;
    QPushButton*       coinControlButton;

    WalletModel* walletModel = nullptr;

    void createWidgets();

public:
    IssueNewNTP1TokenDialog(QWidget* parent = 0);
    void clearData();
    void validateInput() const;
    void setAlreadyIssuedTokensSymbols(const std::unordered_set<std::string>& tokenSymbols);

    void setTokenSymbolValidatorErrorString(const QString& str);

    void setWalletModel(WalletModel* WalletModelPtr);

    json_spirit::Value getIssuanceMetadata() const;

private slots:
    void slot_clearData();
    void slot_modifyChangeAddressColor();
    void slot_modifyTargetAddressColor();
    void slot_changeAddressCheckboxToggled(bool checked);
    void slot_doIssueToken();
    void slot_iconUrlChanged(const QString& url);
    void slot_coinControlButtonClicked();
};

#endif // ISSUENEWNTP1TOKENDIALOG_H
