#include "sendcoinsdialog.h"
#include "ui_sendcoinsdialog.h"

#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "guiutil.h"
#include "init.h"
#include "walletmodel.h"

#include "addressbookpage.h"
#include "askpassphrasedialog.h"
#include "bitcoinunits.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "sendcoinsentry.h"

#include "coincontrol.h"
#include "coincontroldialog.h"
#include "guiconstants.h"

#include <QClipboard>
#include <QLocale>
#include <QScrollBar>
#include <QTextDocument>

#include "ntp1/ntp1tokenlistmodel.h"
#include "ntp1/ntp1tools.h"

SendCoinsDialog::SendCoinsDialog(QWidget* parent)
    : QDialog(parent), ui(new Ui::SendCoinsDialog), model(0)
{
    ui->setupUi(this);

#ifdef Q_OS_MAC // Icons on push buttons are very uncommon on Mac
    ui->addButton->setIcon(QIcon());
    ui->clearButton->setIcon(QIcon());
    ui->sendButton->setIcon(QIcon());
#endif

#if QT_VERSION >= 0x040700
    /* Do not move this to the XML file, Qt before 4.7 will choke on it */
    ui->lineEditCoinControlChange->setPlaceholderText(
        tr("Enter a neblio address (e.g. 8dpZqgY4r2RoEdqYk3QsAqFckyf9pRHN6i)"));
#endif

    addEntry();

    ui->editMetadataDialog->setWindowModality(Qt::WindowModal);

    connect(ui->addButton, &QPushButton::clicked, this, &SendCoinsDialog::addEntry);
    connect(ui->editMetadataButton, &QPushButton::clicked, this,
            &SendCoinsDialog::showEditMetadataDialog);
    connect(ui->clearButton, &QPushButton::clicked, this, &SendCoinsDialog::clear);

    // Coin Control
    ui->lineEditCoinControlChange->setFont(GUIUtil::bitcoinAddressFont());
    connect(ui->pushButtonCoinControl, SIGNAL(clicked()), this, SLOT(coinControlButtonClicked()));
    connect(ui->checkBoxCoinControlChange, SIGNAL(stateChanged(int)), this,
            SLOT(coinControlChangeChecked(int)));
    connect(ui->lineEditCoinControlChange, SIGNAL(textEdited(const QString&)), this,
            SLOT(coinControlChangeEdited(const QString&)));

    // Coin Control: clipboard actions
    QAction* clipboardQuantityAction  = new QAction(tr("Copy quantity"), this);
    QAction* clipboardAmountAction    = new QAction(tr("Copy amount"), this);
    QAction* clipboardFeeAction       = new QAction(tr("Copy fee"), this);
    QAction* clipboardAfterFeeAction  = new QAction(tr("Copy after fee"), this);
    QAction* clipboardBytesAction     = new QAction(tr("Copy bytes"), this);
    QAction* clipboardPriorityAction  = new QAction(tr("Copy priority"), this);
    QAction* clipboardLowOutputAction = new QAction(tr("Copy low output"), this);
    QAction* clipboardChangeAction    = new QAction(tr("Copy change"), this);
    connect(clipboardQuantityAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardQuantity()));
    connect(clipboardAmountAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardAmount()));
    connect(clipboardFeeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardFee()));
    connect(clipboardAfterFeeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardAfterFee()));
    connect(clipboardBytesAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardBytes()));
    connect(clipboardPriorityAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardPriority()));
    connect(clipboardLowOutputAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardLowOutput()));
    connect(clipboardChangeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardChange()));
    ui->labelCoinControlQuantity->addAction(clipboardQuantityAction);
    ui->labelCoinControlAmount->addAction(clipboardAmountAction);
    ui->labelCoinControlFee->addAction(clipboardFeeAction);
    ui->labelCoinControlAfterFee->addAction(clipboardAfterFeeAction);
    ui->labelCoinControlBytes->addAction(clipboardBytesAction);
    ui->labelCoinControlPriority->addAction(clipboardPriorityAction);
    ui->labelCoinControlLowOutput->addAction(clipboardLowOutputAction);
    ui->labelCoinControlChange->addAction(clipboardChangeAction);

    fNewRecipientAllowed = true;
}

void SendCoinsDialog::setModel(WalletModel* model)
{
    this->model = model;

    for (int i = 0; i < ui->entries->count(); ++i) {
        SendCoinsEntry* entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if (entry) {
            entry->setModel(model);
        }
    }
    if (model && model->getOptionsModel()) {
        setUnknownBalance();
        triggerUpdateBalance();
        connect(model, SIGNAL(balanceChanged(qint64, qint64, qint64, qint64)), this,
                SLOT(setBalance(qint64, qint64, qint64, qint64)));
        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this,
                SLOT(updateDisplayUnit()));

        // Coin Control
        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this,
                SLOT(coinControlUpdateLabels()));
        connect(model->getOptionsModel(), SIGNAL(coinControlFeaturesChanged(bool)), this,
                SLOT(coinControlFeatureChanged(bool)));
        connect(model->getOptionsModel(), SIGNAL(transactionFeeChanged(qint64)), this,
                SLOT(coinControlUpdateLabels()));
        ui->frameCoinControl->setVisible(model->getOptionsModel()->getCoinControlFeatures());
        coinControlUpdateLabels();
    }
}

SendCoinsDialog::~SendCoinsDialog() { delete ui; }

void SendCoinsDialog::on_sendButton_clicked()
{
    QList<SendCoinsRecipient> recipients;
    bool                      valid                  = true;
    const bool                fSpendDelegatedOutputs = ui->allowSpendingDelegatedCoins->isChecked();
    ui->allowSpendingDelegatedCoins->setChecked(false);

    if (!model)
        return;

    for (int i = 0; i < ui->entries->count(); ++i) {
        SendCoinsEntry* entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if (entry) {
            if (entry->validate()) {
                recipients.append(entry->getValue());
            } else {
                valid = false;
            }
        }
    }

    if (!valid || recipients.isEmpty()) {
        return;
    }

    NTP1TokenListModel*           ntp1TokenListModel = ntp1TokenListModelInstance.load();
    boost::shared_ptr<NTP1Wallet> ntp1wallet;
    if (ntp1TokenListModel) {
        ntp1wallet = ntp1TokenListModel->getCurrentWallet();
    }

    // Format confirmation message
    QStringList formatted;
    foreach (const SendCoinsRecipient& rcp, recipients) {
        formatted.append(tr("<b>%1</b> to %2 (%3)")
                             .arg(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, rcp.amount),
                                  rcp.label.toHtmlEscaped(), rcp.address));
    }

    fNewRecipientAllowed = false;

    // allow metadata only with NTP1 transactions
    RawNTP1MetadataBeforeSend ntp1metadata;
    {
        bool allTokensNebl = true;
        for (const auto& r : recipients) {
            bool isNebl   = (r.tokenId.toStdString() == NTP1SendTxData::NEBL_TOKEN_ID);
            allTokensNebl = allTokensNebl && isNebl;
        }

        if (allTokensNebl && ui->editMetadataDialog->jsonDataExists()) {
            QMessageBox::warning(
                this, "Metadata in non-NTP1 transaction",
                "We found metadata set. NTP1 metadata requires an NTP1 transaction. Please "
                "include one or more NTP1 tokens in your transaction.");
            ui->editMetadataButton->setStyleSheet(STYLE_INVALID);
            return;
        }

        if (ui->editMetadataDialog->jsonDataExists()) {
            if (ui->editMetadataDialog->jsonDataValid()) {
                json_spirit::Object obj = ui->editMetadataDialog->getJsonData();
                ntp1metadata.metadata   = json_spirit::write(obj);
                if (ui->editMetadataDialog->encryptData()) {
                    assert(!recipients.empty());
                    std::string recipientAddress = recipients.at(0).address.toStdString();
                    for (int i = 1; i < recipients.size(); i++) {
                        if (recipients[i].address.toStdString() != recipientAddress) {
                            QMessageBox::warning(this, "Data encryption on multiple recipient",
                                                 "There is metadata to be sent encrypted in this "
                                                 "transaction. This can only "
                                                 "be done for one recipient, because components of the "
                                                 "encryption should be "
                                                 "stored in the message. If you wish to send to "
                                                 "multiple recipients, please "
                                                 "send them in separate transactions.");
                            return;
                        }
                    }
                    ntp1metadata.encrypt = true;
                }
            } else {
                QMessageBox::warning(this, "Invalid json data",
                                     "Invalid NTP1 metadata found. Either clear it or fix it");
                ui->editMetadataButton->setStyleSheet(STYLE_INVALID);
                return;
            }
        }
    }

    QMessageBox::StandardButton retval = QMessageBox::question(
        this, tr("Confirm send coins"), tr("Are you sure you want to send these tokens?"),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

    if (retval != QMessageBox::Yes) {
        fNewRecipientAllowed = true;
        return;
    }

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if (!ctx.isValid()) {
        // Unlock wallet was cancelled
        fNewRecipientAllowed = true;
        return;
    }

    WalletModel::SendCoinsReturn sendstatus;

    if (!model->getOptionsModel() || !model->getOptionsModel()->getCoinControlFeatures())
        sendstatus = model->sendCoins(recipients, ntp1wallet, ntp1metadata, fSpendDelegatedOutputs);
    else
        sendstatus = model->sendCoins(recipients, ntp1wallet, ntp1metadata, fSpendDelegatedOutputs,
                                      CoinControlDialog::coinControl);

    switch (sendstatus.status) {
    case WalletModel::InvalidAddress:
        QMessageBox::warning(this, tr("Send Coins"),
                             tr("The recipient address is not valid, please recheck."), QMessageBox::Ok,
                             QMessageBox::Ok);
        break;
    case WalletModel::InvalidAmount:
        QMessageBox::warning(this, tr("Send Coins"), tr("The amount to pay must be larger than 0."),
                             QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::AmountExceedsBalance:
        QMessageBox::warning(this, tr("Send Coins"), tr("The amount exceeds your balance."),
                             QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::AmountWithFeeExceedsBalance:
        QMessageBox::warning(
            this, tr("Send Coins"),
            tr("The total exceeds your balance when the %1 transaction fee is included.")
                .arg(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, sendstatus.fee)),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::DuplicateAddress:
        QMessageBox::warning(
            this, tr("Send Coins"),
            tr("Duplicate address found, can only send to each address once per send operation."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::TransactionCreationFailed:
        QMessageBox::warning(this, tr("Send Coins"),
                             tr("Error: Transaction creation failed. ") + sendstatus.msg,
                             QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::TransactionCommitFailed:
        QMessageBox::warning(
            this, tr("Send Coins"),
            tr("Error: The transaction was rejected. This might happen if some of the coins in your "
               "wallet were already spent, such as if you used a copy of wallet.dat and coins were "
               "spent in the copy but not marked as spent here."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::Aborted: // User aborted, nothing to do
        break;
    case WalletModel::AddressContainsNTP1Tokens:
        QMessageBox::warning(
            this, tr("Send Coins - NTP1 tokens problem"),
            "Error: One of the addresses chosen as an input for this transaction contains NTP1 tokens. "
            "You should NOT send NEBL from these addresses or the NTP1 tokens could be permanently "
            "burned. "
            "This address contains NTP1 tokens: " +
                sendstatus.address +
                "\n\n"
                "You have the following options:\n"
                "1. Use coin control and choose addresses that do not contain NTP1 tokens.\n"
                "2. Go to the Orion wallet and sweep the NTP1 tokens to Orion.\n"
                "3. Go to options, and disable this check. Please be aware that your NTP1 tokens WILL "
                "BE DESTROYED.\n",
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::AddressNTP1TokensCheckFailed:
        QMessageBox::warning(
            this, tr("Send Coins - NTP1 tokens problem"),
            "Error: Unable to check whether your addresses contain NTP1 tokens (for address: " +
                sendstatus.address +
                ")\n"
                "Sending NEBL from an address that contains NTP1 tokens could result in those tokens "
                "being permanently burned. "
                "Sweep your NTP1 tokens back to an Orion address using the Orion wallet. "
                "If you would like to proceed with this at your own risk, "
                "please go to options and disable this NTP1 token check.",
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::AddressNTP1TokensCheckFailedFailedToDecodeScriptPubKey:
        QMessageBox::warning(this, tr("Send Coins - NTP1 tokens problem"),
                             "Error: Unable to check whether your addresses contain NTP1 tokens "
                             "(Decoding scriptPubKey failed) "
                             "Sending NEBL from an address that contains NTP1 tokens could result in "
                             "those tokens being permanently burned. "
                             "Sweep your NTP1 tokens back to an Orion address using the Orion wallet. "
                             "If you would like to proceed with this at your own risk, "
                             "please go to options and disable this NTP1 token check.",
                             QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::AddressNTP1TokensCheckFailedTxNotFound:
        QMessageBox::warning(this, tr("Send Coins - NTP1 tokens problem"),
                             "Error: Unable to check whether your addresses contain NTP1 tokens "
                             "(Reference input transaction not found in the wallet) "
                             "Sending NEBL from an address that contains NTP1 tokens could result in "
                             "those tokens being permanently burned. "
                             "Sweep your NTP1 tokens back to an Orion address using the Orion wallet. "
                             "If you would like to proceed with this at your own risk, "
                             "please go to options and disable this NTP1 token check.",
                             QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::AddressNTP1TokensCheckFailedWrongNumberOfOutputs:
        QMessageBox::warning(this, tr("Send Coins - NTP1 tokens problem"),
                             "Error: Unable to check whether your addresses contain NTP1 tokens (number "
                             "of outputs in the transaction used for input is wrong) "
                             "Sending NEBL from an address that contains NTP1 tokens could result in "
                             "those tokens being permanently burned. "
                             "Sweep your NTP1 tokens back to an Orion address using the Orion wallet. "
                             "If you would like to proceed with this at your own risk, "
                             "please go to options and disable this NTP1 token check.",
                             QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::EmptyNTP1TokenID:
        QMessageBox::warning(this, tr("Send Coins - Empty token ID"),
                             "Error: a token in the inputs was found to have an empty token ID.",
                             QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::NTP1TokenCalculationsFailed:
        QMessageBox::warning(this, tr("Send Coins - NTP1 calculations failed"),
                             "Unable to calculate reserve tokens to be spent in this transaction. " +
                                 sendstatus.msg,
                             QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::OK:
        accept();
        CoinControlDialog::coinControl->UnSelectAll();
        coinControlUpdateLabels();
        ui->editMetadataDialog->clearData();
        break;
    }
    fNewRecipientAllowed = true;
}

void SendCoinsDialog::clear()
{
    // Remove entries until only one left
    while (ui->entries->count()) {
        delete ui->entries->takeAt(0)->widget();
    }
    addEntry();

    updateRemoveEnabled();

    ui->sendButton->setDefault(true);
}

void SendCoinsDialog::reject() { clear(); }

void SendCoinsDialog::accept() { clear(); }

SendCoinsEntry* SendCoinsDialog::addEntry()
{
    // metadata can be fixed if more recipients are added, so remove red color from the button
    ui->editMetadataButton->setStyleSheet("");

    SendCoinsEntry* entry = new SendCoinsEntry(this);
    entry->setModel(model);
    ui->entries->addWidget(entry);
    connect(entry, SIGNAL(removeEntry(SendCoinsEntry*)), this, SLOT(removeEntry(SendCoinsEntry*)));
    connect(entry, SIGNAL(payAmountChanged()), this, SLOT(coinControlUpdateLabels()));

    updateRemoveEnabled();

    // Focus the field, so that entry can start immediately
    entry->clear();
    entry->setFocus();
    ui->scrollAreaWidgetContents->resize(ui->scrollAreaWidgetContents->sizeHint());
    QCoreApplication::instance()->processEvents();
    QScrollBar* bar = ui->scrollArea->verticalScrollBar();
    if (bar)
        bar->setSliderPosition(bar->maximum());
    return entry;
}

void SendCoinsDialog::updateRemoveEnabled()
{
    // Remove buttons are enabled as soon as there is more than one send-entry
    bool enabled = (ui->entries->count() > 1);
    for (int i = 0; i < ui->entries->count(); ++i) {
        SendCoinsEntry* entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if (entry) {
            entry->setRemoveEnabled(enabled);
        }
    }
    setupTabChain(0);
    coinControlUpdateLabels();
}

void SendCoinsDialog::removeEntry(SendCoinsEntry* entry)
{
    delete entry;
    updateRemoveEnabled();
}

QWidget* SendCoinsDialog::setupTabChain(QWidget* prev)
{
    for (int i = 0; i < ui->entries->count(); ++i) {
        SendCoinsEntry* entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if (entry) {
            prev = entry->setupTabChain(prev);
        }
    }
    QWidget::setTabOrder(prev, ui->addButton);
    QWidget::setTabOrder(ui->addButton, ui->editMetadataButton);
    QWidget::setTabOrder(ui->editMetadataButton, ui->sendButton);
    return ui->sendButton;
}

void SendCoinsDialog::pasteEntry(const SendCoinsRecipient& rv)
{
    if (!fNewRecipientAllowed)
        return;

    SendCoinsEntry* entry = 0;
    // Replace the first entry if it is still unused
    if (ui->entries->count() == 1) {
        SendCoinsEntry* first = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(0)->widget());
        if (first->isClear()) {
            entry = first;
        }
    }
    if (!entry) {
        entry = addEntry();
    }

    entry->setValue(rv);
}

bool SendCoinsDialog::handleURI(const QString& uri)
{
    SendCoinsRecipient rv;
    // URI has to be valid
    if (GUIUtil::parseBitcoinURI(uri, &rv)) {
        CBitcoinAddress address(rv.address.toStdString());
        if (!address.IsValid())
            return false;
        pasteEntry(rv);
        return true;
    }

    return false;
}

void SendCoinsDialog::setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance,
                                 qint64 immatureBalance)
{
    Q_UNUSED(stake);
    Q_UNUSED(unconfirmedBalance);
    Q_UNUSED(immatureBalance);
    if (!model || !model->getOptionsModel())
        return;

    int unit = model->getOptionsModel()->getDisplayUnit();
    ui->labelBalance->setText(BitcoinUnits::formatWithUnit(unit, balance));
}

void SendCoinsDialog::setUnknownBalance() { ui->labelBalance->setText("Updating..."); }

void SendCoinsDialog::triggerUpdateBalance()
{
    QSharedPointer<BalancesWorker> worker = QSharedPointer<BalancesWorker>::create();
    worker->moveToThread(model->getBalancesThread());
    connect(worker.data(), &BalancesWorker::resultReady, this, &SendCoinsDialog::setBalance,
            Qt::QueuedConnection);
    GUIUtil::AsyncQtCall(worker.data(), [this, worker]() { worker->getBalances(model, worker); });
}

void SendCoinsDialog::showEditMetadataDialog()
{
    ui->editMetadataButton->setStyleSheet("");
    ui->editMetadataDialog->show();
}

void SendCoinsDialog::updateDisplayUnit()
{
    if (model && model->getOptionsModel()) {
        // Update labelBalance with the current balance and the current unit
        ui->labelBalance->setText(BitcoinUnits::formatWithUnit(
            model->getOptionsModel()->getDisplayUnit(), model->getBalance()));
    }
}

// Coin Control: copy label "Quantity" to clipboard
void SendCoinsDialog::coinControlClipboardQuantity()
{
    QApplication::clipboard()->setText(ui->labelCoinControlQuantity->text());
}

// Coin Control: copy label "Amount" to clipboard
void SendCoinsDialog::coinControlClipboardAmount()
{
    QApplication::clipboard()->setText(
        ui->labelCoinControlAmount->text().left(ui->labelCoinControlAmount->text().indexOf(" ")));
}

// Coin Control: copy label "Fee" to clipboard
void SendCoinsDialog::coinControlClipboardFee()
{
    QApplication::clipboard()->setText(
        ui->labelCoinControlFee->text().left(ui->labelCoinControlFee->text().indexOf(" ")));
}

// Coin Control: copy label "After fee" to clipboard
void SendCoinsDialog::coinControlClipboardAfterFee()
{
    QApplication::clipboard()->setText(
        ui->labelCoinControlAfterFee->text().left(ui->labelCoinControlAfterFee->text().indexOf(" ")));
}

// Coin Control: copy label "Bytes" to clipboard
void SendCoinsDialog::coinControlClipboardBytes()
{
    QApplication::clipboard()->setText(ui->labelCoinControlBytes->text());
}

// Coin Control: copy label "Priority" to clipboard
void SendCoinsDialog::coinControlClipboardPriority()
{
    QApplication::clipboard()->setText(ui->labelCoinControlPriority->text());
}

// Coin Control: copy label "Low output" to clipboard
void SendCoinsDialog::coinControlClipboardLowOutput()
{
    QApplication::clipboard()->setText(ui->labelCoinControlLowOutput->text());
}

// Coin Control: copy label "Change" to clipboard
void SendCoinsDialog::coinControlClipboardChange()
{
    QApplication::clipboard()->setText(
        ui->labelCoinControlChange->text().left(ui->labelCoinControlChange->text().indexOf(" ")));
}

void SendCoinsDialog::updateAllTokenLists()
{
    for (int i = 0; i < ui->entries->count(); ++i) {
        SendCoinsEntry* entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if (entry)
            entry->updateNTP1TokensList();
    }
}

// Coin Control: settings menu - coin control enabled/disabled by user
void SendCoinsDialog::coinControlFeatureChanged(bool checked)
{
    ui->frameCoinControl->setVisible(checked);

    if (!checked && model) // coin control features disabled
        CoinControlDialog::coinControl->SetNull();
}

// Coin Control: button inputs -> show actual coin control dialog
void SendCoinsDialog::coinControlButtonClicked()
{
    CoinControlDialog dlg;
    dlg.setModel(model);
    dlg.exec();
    coinControlUpdateLabels();
}

// Coin Control: checkbox custom change address
void SendCoinsDialog::coinControlChangeChecked(int state)
{
    if (model) {
        if (state == Qt::Checked)
            CoinControlDialog::coinControl->destChange =
                CBitcoinAddress(ui->lineEditCoinControlChange->text().toStdString()).Get();
        else
            CoinControlDialog::coinControl->destChange = CNoDestination();
    }

    ui->lineEditCoinControlChange->setEnabled((state == Qt::Checked));
    ui->labelCoinControlChangeLabel->setEnabled((state == Qt::Checked));
}

// Coin Control: custom change address changed
void SendCoinsDialog::coinControlChangeEdited(const QString& text)
{
    if (model) {
        CoinControlDialog::coinControl->destChange = CBitcoinAddress(text.toStdString()).Get();

        // label for the change address
        ui->labelCoinControlChangeLabel->setStyleSheet("QLabel{color:black;}");
        if (text.isEmpty())
            ui->labelCoinControlChangeLabel->setText("");
        else if (!CBitcoinAddress(text.toStdString()).IsValid()) {
            ui->labelCoinControlChangeLabel->setStyleSheet("QLabel{color:red;}");
            ui->labelCoinControlChangeLabel->setText(tr("WARNING: Invalid neblio address"));
        } else {
            QString associatedLabel = model->getAddressTableModel()->labelForAddress(text);
            if (!associatedLabel.isEmpty())
                ui->labelCoinControlChangeLabel->setText(associatedLabel);
            else {
                CPubKey pubkey;
                CKeyID  keyid;
                CBitcoinAddress(text.toStdString()).GetKeyID(keyid);
                if (model->getPubKey(keyid, pubkey))
                    ui->labelCoinControlChangeLabel->setText(tr("(no label)"));
                else {
                    ui->labelCoinControlChangeLabel->setStyleSheet("QLabel{color:red;}");
                    ui->labelCoinControlChangeLabel->setText(tr("WARNING: unknown change address"));
                }
            }
        }
    }
}

// Coin Control: update labels
void SendCoinsDialog::coinControlUpdateLabels()
{
    if (!model || !model->getOptionsModel() || !model->getOptionsModel()->getCoinControlFeatures())
        return;

    // set pay amounts
    CoinControlDialog::payAmounts.clear();
    for (int i = 0; i < ui->entries->count(); ++i) {
        SendCoinsEntry* entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if (entry)
            CoinControlDialog::payAmounts.append(entry->getValue().amount);
    }

    if (CoinControlDialog::coinControl->HasSelected()) {
        // actual coin control calculation
        CoinControlDialog::updateLabels(model, this);

        // show coin control stats
        ui->labelCoinControlAutomaticallySelected->hide();
        ui->widgetCoinControl->show();
    } else {
        // hide coin control stats
        ui->labelCoinControlAutomaticallySelected->show();
        ui->widgetCoinControl->hide();
        ui->labelCoinControlInsuffFunds->hide();
    }
}
