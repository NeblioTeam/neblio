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

#include <QValidator>

class NewStakeDelegationDialog;
class CCoinControl;
class WalletModel;
class MessageBoxWithTimer;

class NewStakeDelegationDialog : public QDialog
{
    Q_OBJECT

    QGridLayout* mainLayout;

    QLabel*      titleLabel;
    QFrame*      titleSeparator;
    QLabel*      stakerAddressLabel;
    QLineEdit*   stakerAddressLineEdit;
    QLabel*      amountLabel;
    QLineEdit*   amountLineEdit;
    QPushButton* createDelegationButton;
    QPushButton* cancelButton;
    QPushButton* clearButton;
    QCheckBox*   showAdvancedOptionsCheckbox;
    QCheckBox*   ownerAddressCheckbox;
    QLineEdit*   ownerAddressLineEdit;
    QCheckBox*   changeAddressCheckbox;
    QLineEdit*   changeAddressLineEdit;
    QCheckBox*   useDelegatedCheckbox;
    QFrame*      paymentSeparator;

    QPushButton*       coinControlButton;

    MessageBoxWithTimer* timedMessageBox;
    QPushButton*         timedMessageBox_yesButton;
    QPushButton*         timedMessageBox_noButton;

    WalletModel* walletModel = nullptr;

    void createWidgets();

    void initializeMessageWithTimer();

public:
    NewStakeDelegationDialog(QWidget* parent = 0);
    void clearData();
    void validateInput() const;

    void setWalletModel(WalletModel* WalletModelPtr);

    void makeError(const QString& msg);

private slots:
    void slot_clearData();
    void slot_modifyStakerAddressColor();
    void slot_modifyChangeAddressColor();
    void slot_modifyTargetAddressColor();
    void slot_changeAddressCheckboxToggled(bool checked);
    void slot_createColdStake();
    void slot_coinControlButtonClicked();
    void slot_toggledSettingManualOwner();
    void slot_toggledUseDelegated();
    void slot_toggledShowAdvancedOptions(bool checked);
};

#endif // NewStakeDelegationDialog_H
