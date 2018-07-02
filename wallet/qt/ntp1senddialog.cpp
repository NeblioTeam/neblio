#include "ntp1senddialog.h"

#include "bitcoinrpc.h"
#include "ntp1sendsingletokenfields.h"
#include <QMessageBox>

NTP1SendDialog::NTP1SendDialog(NTP1TokenListModel* tokenListModel, QWidget* parent) : QWidget(parent)
{
    tokenListDataModel = tokenListModel;
    createWidgets();
}

NTP1SendDialog::~NTP1SendDialog() {}

void NTP1SendDialog::createWidgets()
{
    mainLayout             = new QGridLayout;
    addRecipientButton     = new QPushButton(this);
    sendButton             = new QPushButton(this);
    recipientWidgetsWidget = new QWidget;
    feeWidget              = new NTP1SendTokensFeeWidget;
    addRecipientButton->setText("Add &Recipient");
    addRecipientButton->setIcon(QIcon(":/icons/add"));
    sendButton->setText("Send");
    sendButton->setIcon(QIcon(":/icons/send"));
    this->setLayout(mainLayout);

    recipientWidgetsLayout     = new QVBoxLayout;
    recipientWidgetsScrollArea = new QScrollArea;
    recipientWidgetsWidget->setLayout(recipientWidgetsLayout);
    // The dot prevents propagating to children
    recipientWidgetsWidget->setStyleSheet(
        ".QWidget {background-color: white;border:none;font-family:'Open Sans,sans-serif'; }");
    recipientWidgetsScrollArea->setWidget(recipientWidgetsWidget);

    recipientWidgetsScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    recipientWidgetsScrollArea->setWidgetResizable(true);
    recipientWidgetsScrollArea->setFrameStyle(QFrame::NoFrame);
    recipientWidgetsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    recipientWidgetsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    recipientWidgetsLayout->addWidget(feeWidget, 0, Qt::AlignTop);

    mainLayout->addWidget(recipientWidgetsScrollArea, 0, 0, 1, 3);
    mainLayout->addWidget(addRecipientButton, 1, 0, 1, 1);
    mainLayout->addWidget(sendButton, 1, 2, 1, 1);

    connect(addRecipientButton, &QPushButton::clicked, this, &NTP1SendDialog::slot_addRecipientWidget);
    connect(sendButton, &QPushButton::clicked, this, &NTP1SendDialog::slot_sendClicked);

    // add one widget
    slot_addRecipientWidget();
}

boost::shared_ptr<NTP1Wallet> NTP1SendDialog::getLatestNTP1Wallet() const
{
    if (tokenListDataModel != Q_NULLPTR) {
        return tokenListDataModel->getCurrentWallet();
    } else {
        printf("Error while retrieving latest wallet. Token list data model is nullptr.");
        return Q_NULLPTR;
    }
}

NTP1SendTokensData NTP1SendDialog::createTransactionData() const
{
    NTP1SendTokensData result;
    try {
        EnsureWalletIsUnlocked();
        for (auto w : recipientWidgets) {
            result.addRecipient(w->createRecipientData());
        }
        bool calcFee = false;
        if (feeWidget->isAutoCalcFeeSelected()) {
            calcFee = true;
            result.setFee(0);
        } else {
            calcFee = false;
            result.setFee(static_cast<int64_t>(std::ceil(
                FromString<long double>(feeWidget->getEnteredFee()) * static_cast<long double>(1.e8))));
        }
        result.calculateSources(getLatestNTP1Wallet(), calcFee);
        //        std::cout << result.exportToAPIFormat() << std::endl;
    } catch (std::exception& ex) {
        printf("Error while creating recipient send data: %s", ex.what());
        throw;
    }
    return result;
}

void NTP1SendDialog::slot_addRecipientWidget()
{
    NTP1SendSingleTokenFields* widget =
        new NTP1SendSingleTokenFields(boost::bind(&NTP1SendDialog::getLatestNTP1Wallet, this));
    connect(widget, &NTP1SendSingleTokenFields::signal_closeThis, this,
            &NTP1SendDialog::slot_removeRecipientWidget);
    connect(this, &NTP1SendDialog::signal_updateAllRecipientDialogsTokens, widget,
            &NTP1SendSingleTokenFields::slot_updateTokenList);
    widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    recipientWidgets.push_back(widget);
    recipientWidgetsLayout->addWidget(widget, 0, Qt::AlignTop);
    slot_actToShowOrHideCloseButtons();
}

void NTP1SendDialog::slot_removeRecipientWidget(QWidget* widget)
{
    if (recipientWidgets.size() <= 1) {
        return;
    }
    recipientWidgetsLayout->removeWidget(widget);
    auto it = std::find(recipientWidgets.begin(), recipientWidgets.end(), widget);
    delete widget;
    if (it != recipientWidgets.end()) {
        recipientWidgets.erase(it);
    } else {
        printf("Unable to find recipient widget to erase.");
    }
    slot_actToShowOrHideCloseButtons();
}

void NTP1SendDialog::slot_actToShowOrHideCloseButtons()
{
    if (recipientWidgets.size() > 1) {
        showAllCloseButtons();
    } else {
        hideAllCloseButtons();
    }
}

void NTP1SendDialog::slot_updateAllRecipientDialogsTokens()
{
    emit signal_updateAllRecipientDialogsTokens();
}

void NTP1SendDialog::slot_sendClicked()
{
    try {
        NTP1SendTokensData txData = createTransactionData();
        std::string        apiMsg = txData.exportToAPIFormat();
        //        std::cout << apiMsg << std::endl;
        std::string response = cURLTools::PostDataToHTTPS(
            NTP1Tools::GetURL_SendTokens(fTestNet), NTP1APICalls::NTP1_CONNECTION_TIMEOUT, apiMsg, true);
        //        std::cout << response << std::endl;

        json_spirit::Value signedTxJsonVal;
        {
            json_spirit::Value parsedNTP1RawTx;
            json_spirit::read_or_throw(response, parsedNTP1RawTx);

            std::string unsignedRawTx = NTP1Tools::GetStrField(parsedNTP1RawTx.get_obj(), "txHex");
            json_spirit::Array tempArray;
            tempArray.push_back(unsignedRawTx);
            signedTxJsonVal = signrawtransaction(tempArray, false);
        }
        {
            bool signSuccess = NTP1Tools::GetBoolField(signedTxJsonVal.get_obj(), "complete");
            if (signSuccess) {
                std::string signedRawTx = NTP1Tools::GetStrField(signedTxJsonVal.get_obj(), "hex");
                //                std::cout << signedRawTx << std::endl;
                json_spirit::Array tempArray;
                tempArray.push_back(signedRawTx);
                QMessageBox::StandardButton reply = QMessageBox::question(
                    this, "Transaction confirmation",
                    "Transaction ready to be send. Are you sure you want to submit it to the network?",
                    QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::Yes) {
                    json_spirit::Value submitResult = sendrawtransaction(tempArray, false);

                    std::string txid = json_spirit::write_string(submitResult, false);

                    QMessageBox::StandardButton reply = QMessageBox::question(
                        this, "Transaction submitted",
                        "Transaction is submitted to the network. The transaction id provided is: " +
                            QString::fromStdString(txid) + ".\n\nWould you like to reset all fields?",
                        QMessageBox::Yes | QMessageBox::No);
                    if (reply == QMessageBox::Yes) {
                        resetAllFields();
                    }
                }
            } else {
                throw std::runtime_error("Failed to sign transaction");
            }
        }

    } catch (std::exception& ex) {
        printf("Error while constructing transaction %s\n", ex.what());
        QMessageBox::warning(this, "Error sending", QString(ex.what()));
    }
}

void NTP1SendDialog::resetAllFields()
{
    feeWidget->resetAllFields();
    while (recipientWidgets.size() > 1) {
        slot_removeRecipientWidget(*recipientWidgets.begin());
    }
    if (recipientWidgets.size() > 0) {
        (*recipientWidgets.begin())->resetAllFields();
    }
}

void NTP1SendDialog::showAllCloseButtons()
{
    for (NTP1SendSingleTokenFields* w : recipientWidgets) {
        w->slot_showClose();
    }
}

void NTP1SendDialog::hideAllCloseButtons()
{
    for (NTP1SendSingleTokenFields* w : recipientWidgets) {
        w->slot_hideClose();
    }
}
