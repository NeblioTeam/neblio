#ifndef NewStakeDelegationDialog_H
#define NewStakeDelegationDialog_H

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

class NewStakeDelegationDialog;
class CCoinControl;
class CoinControlDialog;
class WalletModel;

class NewStakeDelegationDialog : public QDialog
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


    CoinControlDialog* coinControlDialog;
    QPushButton*       coinControlButton;

    WalletModel* walletModel = nullptr;

    void createWidgets();

public:
    NewStakeDelegationDialog(QWidget* parent = 0);
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

#endif // NewStakeDelegationDialog_H
