#include "newstakedelegationdialog.h"

#include "base58.h"
#include "bitcoinaddressvalidator.h"
#include "bitcoinunits.h"
#include "coincontrol.h"
#include "coincontroldialog.h"
#include "coldstakedelegation.h"
#include "guiconstants.h"
#include "init.h"
#include "main.h"
#include "messageboxwithtimer.h"
#include "wallet.h"
#include <util.h>

void NewStakeDelegationDialog::createWidgets()
{
    setWindowTitle("Create A New Cold Stake Delegation");

    mainLayout = new QGridLayout(this);

    this->setLayout(mainLayout);

    titleLabel = new QLabel(this);
    titleLabel->setText("Delegate your NEBL to another Node to Cold Stake for you. This is an\n"
                        "advanced feature. The Staker Address is the address that can stake\n"
                        "but cannnot spend the delegated NEBL. Only the Owner Address can ever\n"
                        "spend NEBL from this Smart Contract. See Neblio University for details.");

    titleLabel->setAlignment(Qt::AlignHCenter);

    stakerAddressLabel          = new QLabel("Staker Address", this);
    stakerAddressLineEdit       = new QLineEdit(this);
    amountLabel                 = new QLabel("Amount to Delegate (in NEBLs)", this);
    amountLineEdit              = new QLineEdit(this);
    showAdvancedOptionsCheckbox = new QCheckBox("Show advanced options", this);
    ownerAddressCheckbox        = new QCheckBox("Manually Specify Owner Address", this);
    ownerAddressLineEdit        = new QLineEdit(this);
    useDelegatedCheckbox =
        new QCheckBox("Allow Spending Already Delegated NEBL to Fill This Transaction", this);
    coinControlButton = new QPushButton("Coin Control (Advanced)", this);
    changeAddressCheckbox =
        new QCheckBox("Send Change From This Transaction to a Specific Address", this);
    changeAddressLineEdit = new QLineEdit(this);
    changeAddressLineEdit->setPlaceholderText("Change Address");
    createDelegationButton = new QPushButton("Create", this);
    clearButton            = new QPushButton("Clear", this);
    cancelButton           = new QPushButton("Cancel", this);
    titleSeparator         = new QFrame(this);
    titleSeparator->setFrameShape(QFrame::HLine);
    titleSeparator->setFrameShadow(QFrame::Sunken);
    paymentSeparator = new QFrame(this);
    paymentSeparator->setFrameShape(QFrame::HLine);
    paymentSeparator->setFrameShadow(QFrame::Sunken);

    timedMessageBox = new MessageBoxWithTimer();

    createDelegationButton->setAutoDefault(false);
    cancelButton->setAutoDefault(false);
    clearButton->setAutoDefault(false);

    int row = 0;
    mainLayout->addWidget(titleLabel, row++, 0, 1, 3);
    mainLayout->addWidget(titleSeparator, row++, 0, 1, 3);
    mainLayout->addWidget(stakerAddressLabel, row++, 0, 1, 3);
    mainLayout->addWidget(stakerAddressLineEdit, row++, 0, 1, 3);
    mainLayout->addWidget(amountLabel, row++, 0, 1, 3);
    mainLayout->addWidget(amountLineEdit, row++, 0, 1, 3);
    mainLayout->addWidget(showAdvancedOptionsCheckbox, row++, 0, 1, 3);
    mainLayout->addWidget(ownerAddressCheckbox, row++, 0, 1, 3);
    mainLayout->addWidget(ownerAddressLineEdit, row++, 0, 1, 3);
    mainLayout->addWidget(useDelegatedCheckbox, row++, 0, 1, 3);
    mainLayout->addWidget(paymentSeparator, row++, 0, 1, 3);
    mainLayout->addWidget(coinControlButton, row++, 0, 1, 3);
    mainLayout->addWidget(changeAddressCheckbox, row++, 0, 1, 3);
    mainLayout->addWidget(changeAddressLineEdit, row++, 0, 1, 3);
    mainLayout->addWidget(createDelegationButton, row, 0, 1, 1);
    mainLayout->addWidget(clearButton, row, 1, 1, 1);
    mainLayout->addWidget(cancelButton, row, 2, 1, 1);

    stakerAddressLineEdit->setValidator(new BitcoinAddressValidator(this));
    changeAddressLineEdit->setValidator(new BitcoinAddressValidator(this));
    ownerAddressLineEdit->setValidator(new BitcoinAddressValidator(this));

    static const QRegularExpression amountRegex = QRegularExpression("\\d*\\.\\d{0,8}");
    amountLineEdit->setValidator(new QRegularExpressionValidator(amountRegex, this));

    connect(this->clearButton, &QPushButton::clicked, this, &NewStakeDelegationDialog::slot_clearData);
    connect(this->stakerAddressLineEdit, &QLineEdit::textChanged, this,
            &NewStakeDelegationDialog::slot_modifyStakerAddressColor);
    connect(this->changeAddressLineEdit, &QLineEdit::textChanged, this,
            &NewStakeDelegationDialog::slot_modifyChangeAddressColor);
    connect(this->ownerAddressLineEdit, &QLineEdit::textChanged, this,
            &NewStakeDelegationDialog::slot_modifyTargetAddressColor);
    connect(this->changeAddressCheckbox, &QCheckBox::toggled, this,
            &NewStakeDelegationDialog::slot_changeAddressCheckboxToggled);
    connect(this->cancelButton, &QPushButton::clicked, this, &NewStakeDelegationDialog::hide);
    connect(this->createDelegationButton, &QPushButton::clicked, this,
            &NewStakeDelegationDialog::slot_createColdStake);
    connect(this->coinControlButton, &QPushButton::clicked, this,
            &NewStakeDelegationDialog::slot_coinControlButtonClicked);
    connect(this->ownerAddressCheckbox, &QCheckBox::toggled, this,
            &NewStakeDelegationDialog::slot_toggledSettingManualOwner);
    connect(this->useDelegatedCheckbox, &QCheckBox::toggled, this,
            &NewStakeDelegationDialog::slot_toggledUseDelegated);
    connect(this->showAdvancedOptionsCheckbox, &QCheckBox::toggled, this,
            &NewStakeDelegationDialog::slot_toggledShowAdvancedOptions);

    slot_changeAddressCheckboxToggled(changeAddressCheckbox->isChecked());
    slot_toggledSettingManualOwner();
    slot_toggledShowAdvancedOptions(showAdvancedOptionsCheckbox->isChecked());

    initializeMessageWithTimer();

    layout()->setSizeConstraint(QLayout::SetFixedSize);
    // remove the "?" button in window title
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

NewStakeDelegationDialog::NewStakeDelegationDialog(QWidget* parent) : QDialog(parent)
{
    createWidgets();
}

void NewStakeDelegationDialog::clearData()
{
    stakerAddressLineEdit->clear();
    stakerAddressLineEdit->setStyleSheet("");
    amountLineEdit->clear();
    changeAddressLineEdit->clear();
    ownerAddressLineEdit->clear();
    ownerAddressCheckbox->setChecked(false);
    useDelegatedCheckbox->setChecked(false);
    changeAddressCheckbox->setChecked(false);
    CoinControlDialog::coinControl->UnSelectAll();
}

void NewStakeDelegationDialog::validateInput() const {}

void NewStakeDelegationDialog::initializeMessageWithTimer()
{
    timedMessageBox           = new MessageBoxWithTimer(this);
    timedMessageBox_yesButton = timedMessageBox->addButton(QMessageBox::Yes);
    timedMessageBox_noButton  = timedMessageBox->addButton(QMessageBox::No);
    timedMessageBox->addButtonToWaitOn(timedMessageBox_yesButton);
}

void NewStakeDelegationDialog::setWalletModel(WalletModel* WalletModelPtr)
{
    walletModel = WalletModelPtr;
}

void NewStakeDelegationDialog::makeError(const QString& msg)
{
    QMessageBox::warning(this, "Failed to Create Delegation Transaction",
                         "Error while processing cold-stake delegation transaction: " + msg);
}

void NewStakeDelegationDialog::slot_clearData()
{
    if (QMessageBox::question(this, "Clear all?",
                              "Are you sure you want to clear all the data in this dialog?") ==
        QMessageBox::Yes) {
        clearData();
    }
}

void NewStakeDelegationDialog::slot_modifyStakerAddressColor()
{
    if (CBitcoinAddress(stakerAddressLineEdit->text().toStdString()).IsValid()) {
        stakerAddressLineEdit->setStyleSheet("");
    } else {
        stakerAddressLineEdit->setStyleSheet(STYLE_INVALID);
    }
}

void NewStakeDelegationDialog::slot_modifyChangeAddressColor()
{
    if (CBitcoinAddress(changeAddressLineEdit->text().toStdString()).IsValid()) {
        changeAddressLineEdit->setStyleSheet("");
    } else {
        changeAddressLineEdit->setStyleSheet(STYLE_INVALID);
    }
}

void NewStakeDelegationDialog::slot_modifyTargetAddressColor()
{
    if (CBitcoinAddress(ownerAddressLineEdit->text().toStdString()).IsValid()) {
        ownerAddressLineEdit->setStyleSheet("");
    } else {
        ownerAddressLineEdit->setStyleSheet(STYLE_INVALID);
    }
}

void NewStakeDelegationDialog::slot_changeAddressCheckboxToggled(bool checked)
{
    changeAddressLineEdit->setEnabled(checked);
    if (!checked) {
        changeAddressLineEdit->setStyleSheet("");
    }
}

void NewStakeDelegationDialog::slot_createColdStake()
{
    const bool fUseDelegated = useDelegatedCheckbox->isChecked();

    const std::string stakerAddress = stakerAddressLineEdit->text().toStdString();

    const CTxDB txdb;

    // get the owner address
    const boost::optional<std::string> ownerAddress =
        ownerAddressCheckbox->isChecked()
            ? boost::make_optional(ownerAddressLineEdit->text().toStdString())
            : boost::none;

    // parse the amount from the text field
    qint64 amount = 0;
    if (!BitcoinUnits::parse(BitcoinUnits::BTC, amountLineEdit->text(), &amount)) {
        return makeError("Failed to parse amount to a valid amount of NEBL");
    }

    // ensure the wallet is unlocked
    if (pwalletMain->IsLocked() || fWalletUnlockStakingOnly)
        return makeError("You must fully unlock your wallet (not just for staking) before attempting to "
                         "delegate stakes");

    // ensure the owner address is in this wallet
    static const bool fForceExternalAddr = true;
    if (ownerAddress) {
        CKeyID ownerKey;
        if (!CBitcoinAddress(*ownerAddress).GetKeyID(ownerKey))
            return makeError("Failed to calculate public key hash from owner address");
        if (!pwalletMain->HaveKey(ownerKey)) {
            const QString msg =
                "WARNING: The Owner Address you provided is not in this wallet! You are giving full "
                "control of these NEBL to this Owner Address. It should be part of a Neblio-Qt "
                "or Neblio Orion wallet that you control! \n\nARE YOU SURE YOU WANT TO PROCEED???"
                "\n\nTHIS CANNOT BE REVERSED!";
            timedMessageBox->setText(msg);
            timedMessageBox->setWindowTitle("Confirm External Owner Address");
            timedMessageBox->exec();
            if (timedMessageBox->clickedButton() != timedMessageBox_yesButton) {
                return;
            }
        }
    }

    // create delegation scriptPubKey
    const auto delegRes = CreateColdStakeDelegation(stakerAddress, amount, ownerAddress,
                                                    fForceExternalAddr, fUseDelegated, false);

    if (delegRes.isErr()) {
        return makeError(
            QString::fromStdString(ColdStakeDelegationErrorStr(delegRes.unwrapErr(RESULT_PRE))));
    }

    const CoinStakeDelegationResult res = delegRes.unwrap(RESULT_PRE);

    const CAmount currBalance =
        pwalletMain->GetBalance(txdb) - (fUseDelegated ? 0 : pwalletMain->GetDelegatedBalance(txdb));

    {
        // calculate inputs from coin control, if necessary
        assert(CoinControlDialog::coinControl != nullptr);
        bool                   takeInputsFromCoinControl = CoinControlDialog::coinControl->HasSelected();
        std::vector<COutPoint> inputs =
            (takeInputsFromCoinControl ? CoinControlDialog::coinControl->GetSelected()
                                       : std::vector<COutPoint>());

        // Get NTP1 wallet
        boost::shared_ptr<NTP1Wallet> ntp1wallet = boost::make_shared<NTP1Wallet>();
        ntp1wallet->setRetrieveFullMetadata(false);
        ntp1wallet->update();

        NTP1SendTxData tokenSelector;
        try {
            tokenSelector.selectNTP1Tokens(ntp1wallet, inputs,
                                           std::vector<NTP1SendTokensOneRecipientData>(),
                                           !takeInputsFromCoinControl);
        } catch (std::exception& ex) {
            return makeError(QString("Could not reserve the outputs necessary for spending. Error: ") +
                             ex.what());
        }

        // Create the transaction
        CAmount     nFeeRequired;
        CWalletTx   wtxNew;
        std::string strError;
        CReserveKey reservekey(pwalletMain.get());

        CoinControlDialog::coinControl->destChange =
            CNoDestination(); // default is: No change address specified
        if (changeAddressCheckbox->isChecked()) {
            CoinControlDialog::coinControl->destChange =
                CBitcoinAddress(changeAddressLineEdit->text().toStdString()).Get();
        }

        if (!pwalletMain->CreateTransaction(txdb, res.scriptPubKey, amount, wtxNew, reservekey,
                                            nFeeRequired, tokenSelector, &strError,
                                            RawNTP1MetadataBeforeSend(), false,
                                            CoinControlDialog::coinControl, fUseDelegated)) {
            if (amount + nFeeRequired > currBalance)
                strError =
                    fmt::format("Error: This transaction requires a transaction fee of at least {}"
                                "because of its amount, complexity, or use of recently received "
                                "funds!",
                                FormatMoney(nFeeRequired));
            NLog.write(b_sev::err, "{}: Cold-stake delegation error: {}", FUNCTIONSIG, strError);
            return makeError(QString::fromStdString(strError));
        }

        {
            QMessageBox::StandardButton answer =
                QMessageBox::question(this, "Do You Want to Proceed?",
                                      "Creating this Delegation Smart-Contract will cost " +
                                          QString::fromStdString(FormatMoney(nFeeRequired)) +
                                          " NEBL in fees. Are you sure you want to proceed?");

            if (answer != QMessageBox::Yes) {
                return;
            }
        }

        if (!pwalletMain->CommitTransaction(wtxNew, CTxDB(), reservekey))
            return makeError(
                "Error: The transaction was rejected! This might happen if some of the coins "
                "in your wallet were already spent, such as if you used a copy of wallet.dat "
                "and coins were spent in the copy but not marked as spent here.");

        QMessageBox::information(
            this, "Success!",
            QString::fromStdString("The Delegation Smart-Contract was Created Successfully!\n"
                                   "Transaction ID: " +
                                   wtxNew.GetHash().GetHex() +
                                   "\n"
                                   "Owner: " +
                                   res.ownerAddress.ToString() +
                                   "\n"
                                   "Staker: " +
                                   res.stakerAddress.ToString() +
                                   "\n"
                                   "To revoke the Delegation, simply spend these NEBL from the\n"
                                   "Owner Address. You can find this delegation entry on the \n"
                                   "Cold Staking tab."));
        clearData();
    }
}

void NewStakeDelegationDialog::slot_coinControlButtonClicked()
{
    CoinControlDialog dlg(this, false, QString());
    dlg.setModel(walletModel);
    dlg.exec(); // this is synchornous, so it wont' return until finished
}

void NewStakeDelegationDialog::slot_toggledSettingManualOwner()
{
    if (ownerAddressCheckbox->isChecked()) {
        const QString msg =
            "Setting the Owner Address manually should ONLY be done if you own that address! \n"
            "WARNING: The Owner Address will be able to spend these NEBL at any time! \n"
            "\nAre you sure you want to manually specify the Owner Address instead of "
            "letting the wallet pick automatically?";
        timedMessageBox->setText(msg);
        timedMessageBox->setWindowTitle("Confirm Setting Owner Address Manually");
        timedMessageBox->exec();
        if (timedMessageBox->clickedButton() == timedMessageBox_yesButton) {
            ownerAddressCheckbox->setChecked(true);
            ownerAddressLineEdit->setEnabled(true);
        } else {
            ownerAddressCheckbox->setChecked(false);
            ownerAddressLineEdit->setEnabled(false);
        }
    } else {
        ownerAddressLineEdit->setStyleSheet("");
        ownerAddressLineEdit->setEnabled(false);
    }
}

void NewStakeDelegationDialog::slot_toggledUseDelegated()
{
    if (useDelegatedCheckbox->isChecked()) {
        const QString msg =
            "WARNING: This will allow the wallet to potentially revoke any existing "
            "delegations to create this new delegation (this can be controlled using coin-control). "
            "This is only recommended if you know what you are doing. Are you sure you want to proceed?";
        timedMessageBox->setText(msg);
        timedMessageBox->setWindowTitle("Confirm Potentially Revoking Existing Delegations");
        timedMessageBox->exec();
        if (timedMessageBox->clickedButton() == timedMessageBox_yesButton) {
            useDelegatedCheckbox->setChecked(true);
        } else {
            useDelegatedCheckbox->setChecked(false);
        }
    }
}

void NewStakeDelegationDialog::slot_toggledShowAdvancedOptions(bool checked)
{
    ownerAddressCheckbox->setVisible(checked);
    ownerAddressLineEdit->setVisible(checked);
    useDelegatedCheckbox->setVisible(checked);
}
