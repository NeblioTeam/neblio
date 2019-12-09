#include "issuenewntp1tokendialog.h"

#include "base58.h"
#include "bitcoinaddressvalidator.h"
#include "coincontrol.h"
#include "coincontroldialog.h"
#include "guiconstants.h"
#include "init.h"
#include "main.h"
#include "ntp1/ntp1transaction.h"
#include "txdb.h"
#include "wallet.h"
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
    amountLabel           = new QLabel("Amount to issue", this);
    amountLineEdit        = new QLineEdit(this);
    issuerLabel           = new QLabel("Issuer", this);
    issuerLineEdit        = new QLineEdit(this);
    iconUrlLabel          = new QLabel("Icon URL", this);
    iconUrlMimeTypeLabel  = new QLabel("", this);
    iconUrlLineEdit       = new QLineEdit(this);
    metadataDialog        = new NTP1CreateMetadataDialog(this);
    editMetadataButton    = new QPushButton("Edit issuance metadata", this);
    issueButton           = new QPushButton("Create", this);
    cancelButton          = new QPushButton("Cancel", this);
    clearButton           = new QPushButton("Clear", this);
    targetAddressLabel    = new QLabel("Target address - where the tokens will go after issuance", this);
    targetAddressLineEdit = new QLineEdit(this);
    changeAddressLineEdit = new QLineEdit(this);
    changeAddressCheckbox = new QCheckBox("Send the change from this transaction to a specific address");
    changeAddressLineEdit->setPlaceholderText("Change address");
    costLabel = new QLabel(this);
    costLabel->setText("Issuing a token will cost " +
                       QString::number(NTP1Transaction::IssuanceFee / COIN) +
                       " NEBL. NTP1 outputs cannot be used.");
    costLabel->setAlignment(Qt::AlignHCenter);
    paymentSeparator = new QFrame(this);
    paymentSeparator->setFrameShape(QFrame::HLine);
    paymentSeparator->setFrameShadow(QFrame::Sunken);

    coinControlDialog = new CoinControlDialog(this);
    coinControlButton = new QPushButton("Coin control (Advanced)", this);

    iconUrlLabel->setText(
        R"****(Icon URL <span style="color:red;">(Warning: This can never be changed in the future)</span>)****");

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
    mainLayout->addWidget(amountLabel, row++, 0, 1, 3);
    mainLayout->addWidget(amountLineEdit, row++, 0, 1, 3);
    mainLayout->addWidget(issuerLabel, row++, 0, 1, 3);
    mainLayout->addWidget(issuerLineEdit, row++, 0, 1, 3);
    mainLayout->addWidget(iconUrlLabel, row++, 0, 1, 3);
    mainLayout->addWidget(iconUrlMimeTypeLabel, row++, 0, 1, 3);
    mainLayout->addWidget(iconUrlLineEdit, row++, 0, 1, 3);
    mainLayout->addWidget(editMetadataButton, row++, 0, 1, 3);
    mainLayout->addWidget(paymentSeparator, row++, 0, 1, 3);
    mainLayout->addWidget(targetAddressLabel, row++, 0, 1, 3);
    mainLayout->addWidget(targetAddressLineEdit, row++, 0, 1, 3);
    mainLayout->addWidget(coinControlButton, row++, 0, 1, 3);
    mainLayout->addWidget(changeAddressCheckbox, row++, 0, 1, 3);
    mainLayout->addWidget(changeAddressLineEdit, row++, 0, 1, 3);
    mainLayout->addWidget(issueButton, row, 0, 1, 1);
    mainLayout->addWidget(clearButton, row, 1, 1, 1);
    mainLayout->addWidget(cancelButton, row, 2, 1, 1);

    tokenSymbolValidator = new NTP1TokenSymbolValidator(*this, this);
    tokenSymbolLineEdit->setValidator(tokenSymbolValidator);

    changeAddressLineEdit->setValidator(new BitcoinAddressValidator(this));
    targetAddressLineEdit->setValidator(new BitcoinAddressValidator(this));
    amountLineEdit->setValidator(new NTP1TokenAmountValidator(this));

    connect(this->clearButton, &QPushButton::clicked, this, &IssueNewNTP1TokenDialog::slot_clearData);
    connect(this->changeAddressLineEdit, &QLineEdit::textChanged, this,
            &IssueNewNTP1TokenDialog::slot_modifyChangeAddressColor);
    connect(this->targetAddressLineEdit, &QLineEdit::textChanged, this,
            &IssueNewNTP1TokenDialog::slot_modifyTargetAddressColor);
    connect(this->changeAddressCheckbox, &QCheckBox::toggled, this,
            &IssueNewNTP1TokenDialog::slot_changeAddressCheckboxToggled);
    connect(this->cancelButton, &QPushButton::clicked, this, &IssueNewNTP1TokenDialog::hide);
    connect(this->editMetadataButton, &QPushButton::clicked, this->metadataDialog,
            &NTP1CreateMetadataDialog::show);
    connect(this->issueButton, &QPushButton::clicked, this, &IssueNewNTP1TokenDialog::slot_doIssueToken);
    connect(this->iconUrlLineEdit, &QLineEdit::textChanged, this,
            &IssueNewNTP1TokenDialog::slot_iconUrlChanged);
    connect(this->coinControlButton, &QPushButton::clicked, this,
            &IssueNewNTP1TokenDialog::slot_coinControlButtonClicked);

    slot_changeAddressCheckboxToggled(changeAddressCheckbox->isChecked());
    slot_iconUrlChanged(iconUrlLineEdit->text());
    tokenSymbolErrorLabel->setVisible(false);
}

IssueNewNTP1TokenDialog::IssueNewNTP1TokenDialog(QWidget* parent) : QDialog(parent) { createWidgets(); }

void IssueNewNTP1TokenDialog::clearData()
{
    tokenNameLineEdit->clear();
    tokenSymbolLineEdit->clear();
    issuerLineEdit->clear();
    iconUrlLineEdit->clear();
    metadataDialog->clearData();
    amountLineEdit->clear();
    changeAddressLineEdit->clear();
    targetAddressLineEdit->clear();
}

void IssueNewNTP1TokenDialog::validateInput() const
{
    if (changeAddressCheckbox->isChecked() &&
        !CBitcoinAddress(changeAddressLineEdit->text().toStdString()).IsValid()) {
        throw std::runtime_error("Invalid change address provided");
    }
    if (!CBitcoinAddress(targetAddressLineEdit->text().toStdString()).IsValid()) {
        throw std::runtime_error("Invalid change address provided");
    }
    std::string tokenSymbolGiven = tokenSymbolLineEdit->text().trimmed().toStdString();
    std::string tokenNameGiven   = tokenNameLabel->text().trimmed().toStdString();
    std::string tokenIssuerGiven = issuerLineEdit->text().trimmed().toStdString();
    std::string tokenAmountGiven = amountLineEdit->text().trimmed().toStdString();
    if (tokenSymbolGiven.empty()) {
        throw std::runtime_error("Token symbol cannot be empty");
    }
    if (tokenSymbolValidator->tokenWithSymbolAlreadyIssued(tokenSymbolGiven)) {
        throw std::runtime_error("A token with symbol " + tokenSymbolGiven + " already exists");
    }
    if (tokenNameGiven.empty()) {
        throw std::runtime_error("Token name cannot be empty");
    }
    if (tokenIssuerGiven.empty()) {
        throw std::runtime_error("Token issuer cannot be empty");
    }
    if (tokenAmountGiven.empty()) {
        throw std::runtime_error("Token amount cannot be empty");
    }
    NTP1Int amount(tokenAmountGiven);
    if (amount <= 0) {
        throw std::runtime_error("Token amount cannot be zero/negative");
    }
    if (amount > NTP1MaxAmount) {
        throw std::runtime_error("Token amount to issue is larger than the maximum possible");
    }
}

void IssueNewNTP1TokenDialog::setAlreadyIssuedTokensSymbols(
    const std::unordered_set<std::string>& tokenSymbols)
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

void IssueNewNTP1TokenDialog::setWalletModel(WalletModel* WalletModelPtr)
{
    walletModel = WalletModelPtr;
}

json_spirit::Value IssueNewNTP1TokenDialog::getIssuanceMetadata() const
{
    validateInput();
    json_spirit::Object dataNode;
    dataNode.push_back(json_spirit::Pair("tokenName", tokenSymbolLineEdit->text().toStdString()));
    dataNode.push_back(json_spirit::Pair("description", tokenNameLineEdit->text().toStdString()));
    dataNode.push_back(json_spirit::Pair("issuer", issuerLineEdit->text().toStdString()));
    if (!iconUrlLineEdit->text().trimmed().isEmpty()) {
        std::string         url = iconUrlLineEdit->text().trimmed().toStdString();
        json_spirit::Object iconObject;
        iconObject.push_back(json_spirit::Pair("name", "icon"));
        iconObject.push_back(json_spirit::Pair("url", url));
        iconObject.push_back(json_spirit::Pair("mimeType", GetMimeTypeFromPath(url)));

        json_spirit::Array urlsArray;
        urlsArray.push_back(iconObject);

        dataNode.push_back(json_spirit::Pair("urls", urlsArray));
    }

    if (metadataDialog->jsonDataExists()) {
        if (metadataDialog->jsonDataValid()) {
            json_spirit::Object userDataObj = metadataDialog->getJsonData();
            // double-check that this object has "userData" node in it, which is supposed to be the only
            // node
            auto it = std::find_if(userDataObj.cbegin(), userDataObj.cend(),
                                   [](const json_spirit::Pair& p) { return p.name_ == "userData"; });
            if (it == userDataObj.cend()) {
                throw std::runtime_error(
                    "Internal error while retrieving user metadata from the metadata dialog");
            }

            json_spirit::Pair userDataPair = *it;
            dataNode.push_back(userDataPair);
        } else {
            throw std::runtime_error("While data exists in the metadata fields, they don't seem to be "
                                     "valid. Please use valid json.");
        }
    }

    json_spirit::Pair  dataPair("data", dataNode);
    json_spirit::Value rootNode({dataPair});
    return rootNode;
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

void IssueNewNTP1TokenDialog::slot_modifyTargetAddressColor()
{
    if (CBitcoinAddress(targetAddressLineEdit->text().toStdString()).IsValid()) {
        targetAddressLineEdit->setStyleSheet("");
    } else {
        targetAddressLineEdit->setStyleSheet(STYLE_INVALID);
    }
}

void IssueNewNTP1TokenDialog::slot_changeAddressCheckboxToggled(bool checked)
{
    changeAddressLineEdit->setEnabled(checked);
    if (!checked) {
        changeAddressLineEdit->setStyleSheet("");
    }
}

void IssueNewNTP1TokenDialog::slot_doIssueToken()
{
    try {
        validateInput();

        if (pwalletMain->IsLocked())
            throw std::runtime_error(
                "Error: Please enter the wallet passphrase with walletpassphrase first.");
        if (fWalletUnlockStakingOnly)
            throw std::runtime_error("Error: Wallet is unlocked for staking only.");

        json_spirit::Value        metadataObj = getIssuanceMetadata();
        RawNTP1MetadataBeforeSend metadata(json_spirit::write(metadataObj));

        if (pwalletMain == nullptr) {
            throw std::runtime_error("The wallet pointer is null. Failed to create transaction.");
        }

        const int64_t minAmount = 2 * MIN_TX_FEE + NTP1Transaction::IssuanceFee;

        CWalletTx wtx;

        // Get NTP1 wallet
        boost::shared_ptr<NTP1Wallet> ntp1wallet = boost::make_shared<NTP1Wallet>();
        ntp1wallet->setRetrieveFullMetadata(false);
        ntp1wallet->update();

        // Check funds
        int64_t nBalance = pwalletMain->GetBalance();
        if (minAmount > nBalance)
            throw std::runtime_error(
                "You don't have enough NEBLs to issue this token. You need at least: " +
                FormatMoney(minAmount) +
                ". It may even be slightly more depending on the size of the metadata.");

        NTP1Int     amount(amountLineEdit->text().toStdString());
        std::string tokenSymbol = tokenSymbolLineEdit->text().toStdString();

        NTP1SendTokensOneRecipientData ntp1recipient;
        ntp1recipient.amount = amount;
        ntp1recipient.destination =
            CBitcoinAddress(targetAddressLineEdit->text().toStdString()).ToString();
        ntp1recipient.tokenId = NTP1SendTxData::TO_ISSUE_TOKEN_ID;

        std::vector<NTP1SendTokensOneRecipientData> ntp1recipients{ntp1recipient};

        // calculate inputs from coin control, if necessary
        assert(CoinControlDialog::coinControl != nullptr);
        bool                   takeInputsFromCoinControl = CoinControlDialog::coinControl->HasSelected();
        std::vector<COutPoint> inputs =
            (takeInputsFromCoinControl ? CoinControlDialog::coinControl->GetSelected()
                                       : std::vector<COutPoint>());

        // make sure the amount is correct AND make sure that all inputs are non-NTP1
        if (takeInputsFromCoinControl) {
            CTxDB    txdb;
            uint64_t totalInInputs = 0;
            for (const COutPoint o : inputs) {

                CTransaction tx = CTransaction::FetchTxFromDisk(o.hash, txdb);

                assert(o.n < tx.vout.size());
                totalInInputs += tx.vout[o.n].nValue;

                // get NTP1 information of this transaction
                bool txIsNTP1 = NTP1Transaction::IsTxNTP1(&tx);

                if (txIsNTP1) {
                    // if this output is an NTP1 output, skip it
                    try {
                        std::vector<std::pair<CTransaction, NTP1Transaction>> inputs =
                            NTP1Transaction::GetAllNTP1InputsOfTx(tx, false);
                        NTP1Transaction ntp1tx;
                        ntp1tx.readNTP1DataFromTx(tx, inputs);
                        // if this output contains tokens, skip it to avoid burning them
                        if (ntp1tx.getTxOut(o.n).tokenCount() > 0) {
                            QMessageBox::warning(
                                this, "Error",
                                "One or more of the inputs chosen in coin control is an NTP1 output. "
                                "\n\nFor "
                                "issuance, you cannot choose NTP1 outputs. Please choose outputs that "
                                "contain only NEBLs");
                            return;
                        }
                    } catch (std::exception& ex) {
                        QMessageBox::warning(
                            this, "Error",
                            "An error occurred while verifying outputs from coin "
                            "control. Please choose a different set of outputs. The error is: \n\n" +
                                QString(ex.what()));
                        return;
                    }
                }
            }

            if (totalInInputs < minAmount) {
                throw std::runtime_error(
                    "You have not selected enough NEBLs from coin control to issue "
                    "this token. You need at least: " +
                    FormatMoney(minAmount) +
                    ". It may even be slightly more depending on the size of the metadata.");
            }
        }

        // initial selection of NTP1 tokens
        NTP1SendTxData tokenSelector;
        tokenSelector.issueNTP1Token(IssueTokenData(amount, tokenSymbol, metadata.metadata));
        tokenSelector.selectNTP1Tokens(ntp1wallet, inputs, ntp1recipients, !takeInputsFromCoinControl);

        // Send
        CReserveKey keyChange(pwalletMain.get());
        int64_t     nFeeRequired = 0;

        CoinControlDialog::coinControl->destChange =
            CNoDestination(); // default is: No change address specified
        if (changeAddressCheckbox->isChecked()) {
            CoinControlDialog::coinControl->destChange =
                CBitcoinAddress(targetAddressLineEdit->text().toStdString()).Get();
        }

        std::string errorMessage;

        bool fCreated = pwalletMain->CreateTransaction(
            std::vector<std::pair<CScript, int64_t>>(), wtx, keyChange, nFeeRequired, tokenSelector,
            metadata, true, CoinControlDialog::coinControl, &errorMessage);
        if (!fCreated) {
            if (minAmount + nFeeRequired > pwalletMain->GetBalance()) {
                throw std::runtime_error(
                    "Insufficient funds to create the transaction. The required fee is: " +
                    FormatMoney(nFeeRequired));
            }
            throw std::runtime_error("Transaction creation failed. " + errorMessage);
        }

        QMessageBox::StandardButton answer = QMessageBox::question(
            this, "Do you want to proceed?",
            "Creating this token will cost " + QString::fromStdString(FormatMoney(nFeeRequired)) +
                " NEBL. Are you sure you want to proceed? \n\n This is irreversible, "
                "and none of the data chosen for the token can be changed in the "
                "future.");

        if (answer != QMessageBox::Yes) {
            return;
        }

        // verify the NTP1 transaction before commiting
        try {
            std::vector<std::pair<CTransaction, NTP1Transaction>> inputsTxs =
                NTP1Transaction::GetAllNTP1InputsOfTx(wtx, false);
            NTP1Transaction ntp1tx;
            ntp1tx.readNTP1DataFromTx(wtx, inputsTxs);
        } catch (std::exception& ex) {
            printf("An invalid NTP1 transaction was created; an exception was thrown: %s\n", ex.what());
            throw std::runtime_error(
                "Unable to create the transaction. The transaction created would result in an invalid "
                "transaction. Please report your transaction details to the Neblio team. The "
                "error is: " +
                std::string(ex.what()));
        }

        if (!pwalletMain->CommitTransaction(wtx, keyChange))
            throw std::runtime_error("Transaction commit for broadcast failed");

        QMessageBox::information(this, "Success!",
                                 "Congratulations. Your transaction has been submitted to the network. "
                                 "Your transaction hash is: " +
                                     QString::fromStdString(wtx.GetHash().GetHex()));

        this->clearData();

    } catch (std::exception& ex) {
        QMessageBox::warning(this, "Error while attempting issuance",
                             "While attempting issuance, the following error happened: " +
                                 QString(ex.what()));
    }
}

void IssueNewNTP1TokenDialog::slot_iconUrlChanged(const QString& url)
{
    if (url.trimmed().isEmpty()) {
        iconUrlMimeTypeLabel->setVisible(false);
    } else {
        iconUrlMimeTypeLabel->setText("Selected icon mime-type: " +
                                      QString::fromStdString(GetMimeTypeFromPath(url.toStdString())));
        if (!iconUrlMimeTypeLabel->isVisible()) {
            iconUrlMimeTypeLabel->setVisible(true);
        }
    }
}

void IssueNewNTP1TokenDialog::slot_coinControlButtonClicked()
{
    CoinControlDialog dlg;
    dlg.setModel(walletModel);
    dlg.exec(); // this is synchornous, so it wont' return until finished
}

QValidator::State NTP1TokenSymbolValidator::validate(QString& input, int&) const
{
    if (input.isEmpty()) {
        return State::Intermediate;
    }
    if (input.length() > 5) {
        return State::Invalid;
    }
    if (alreadyIssuedTokenSymbols.find(input.toUpper().toStdString()) !=
        alreadyIssuedTokenSymbols.cend()) {
        dialog.setTokenSymbolValidatorErrorString("A token with symbol " + input.toUpper() +
                                                  " already exists");
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
    const std::unordered_set<std::string>& tokenSymbols)
{
    alreadyIssuedTokenSymbols = tokenSymbols;
}

bool NTP1TokenSymbolValidator::tokenWithSymbolAlreadyIssued(const std::string& tokenSymbol)
{
    return alreadyIssuedTokenSymbols.find(tokenSymbol) != alreadyIssuedTokenSymbols.cend();
}

NTP1TokenSymbolValidator::NTP1TokenSymbolValidator(IssueNewNTP1TokenDialog& isseNewNTP1Dialog,
                                                   QObject*                 parent)
    : QValidator(parent), dialog(isseNewNTP1Dialog)
{
}

NTP1TokenAmountValidator::NTP1TokenAmountValidator(QObject* parent) : QValidator(parent) {}

QValidator::State NTP1TokenAmountValidator::validate(QString& input, int&) const
{
    if (input.isEmpty()) {
        return State::Intermediate;
    }
    if (std::any_of(input.cbegin(), input.cend(), [](QChar c) { return !c.isNumber(); })) {
        return State::Invalid;
    }
    NTP1Int amount(input.toStdString());
    if (amount <= 0) {
        return State::Invalid;
    }
    if (amount > NTP1MaxAmount) {
        return State::Invalid;
    }
    return State::Acceptable;
}
