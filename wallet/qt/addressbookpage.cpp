#include "addressbookpage.h"
#include "ui_addressbookpage.h"

#include "addresstablemodel.h"
#include "optionsmodel.h"
#include "bitcoingui.h"
#include "editaddressdialog.h"
#include "csvmodelwriter.h"
#include "guiutil.h"
#include "ledger/bip32.h"
#include "ledger/error.h"
#include "ledgerBridge.h"
#include "ledger_ui/ledgermessagebox.h"
#include "ledger_ui/ledgeruiutils.h"

#include <QSortFilterProxyModel>
#include <QClipboard>
#include <QMessageBox>
#include <QMenu>

#ifdef USE_QRCODE
#include "qrcodedialog.h"
#endif

void VerifyLedgerAddressWorker::verify(uint32_t account, uint32_t index, QSharedPointer<VerifyLedgerAddressWorker> workerPtr) {
    ledger::bytes paymentPubKeyBytes;
    QString errorMessage;
    try {
        ledgerbridge::LedgerBridge ledgerBridge;
        paymentPubKeyBytes = ledgerBridge.GetPublicKey(account, false, index, true);
    } catch (const ledger::LedgerException& e) {
        errorMessage = ledger_ui::GetQtErrorMessage(e);
    }

    emit resultReady(errorMessage);
    workerPtr.reset();
}

AddressBookPage::AddressBookPage(Mode modeIn, Tabs tabIn, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddressBookPage),
    model(0),
    optionsModel(0),
    mode(modeIn),
    tab(tabIn)
{
    ui->setupUi(this);

#ifdef Q_OS_MAC // Icons on push buttons are very uncommon on Mac
    ui->newAddressButton->setIcon(QIcon());
    ui->copyToClipboard->setIcon(QIcon());
    ui->showQRCode->setIcon(QIcon());
    ui->signMessage->setIcon(QIcon());
    ui->verifyMessage->setIcon(QIcon());
    ui->verifyAddress->setIcon(QIcon());
    ui->deleteButton->setIcon(QIcon());
#endif

#ifndef USE_QRCODE
    ui->showQRCode->setVisible(false);
#endif

    switch(modeIn)
    {
    case ForSending:
        connect(ui->tableView, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(accept()));
        ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->tableView->setFocus();
        break;
    case ForEditing:
        ui->buttonBox->setVisible(false);
        break;
    }
    switch(tabIn)
    {
    case SendingTab:
        ui->labelExplanation->setVisible(false);
        ui->deleteButton->setVisible(true);
        ui->signMessage->setVisible(false);
        ui->verifyMessage->setVisible(true);
        ui->verifyAddress->setVisible(false);
        break;
    case ReceivingTab:
        ui->deleteButton->setVisible(false);
        ui->signMessage->setVisible(true);
        ui->verifyMessage->setVisible(false);
        ui->verifyAddress->setVisible(true);
        break;
    case LedgerTab:
        ui->deleteButton->setVisible(false);
        ui->signMessage->setVisible(false);
        ui->verifyMessage->setVisible(false);
        ui->verifyAddress->setVisible(true);
        break;
    }

    // Context menu actions
    QAction *copyLabelAction = new QAction(tr("Copy &Label"), this);
    QAction *copyAddressAction = new QAction(ui->copyToClipboard->text(), this);
    QAction *editAction = new QAction(tr("&Edit"), this);
    QAction *showQRCodeAction = new QAction(ui->showQRCode->text(), this);
    signMessageAction = new QAction(ui->signMessage->text(), this);
    verifyMessageAction = new QAction(ui->verifyMessage->text(), this);
    verifyAddressAction = new QAction(ui->verifyAddress->text(), this);
    deleteAction = new QAction(ui->deleteButton->text(), this);

    // Build context menu
    contextMenu = new QMenu();
    contextMenu->addAction(copyAddressAction);
    contextMenu->addAction(copyLabelAction);
    contextMenu->addAction(editAction);
    if(tabIn == SendingTab)
        contextMenu->addAction(deleteAction);

    contextMenu->addSeparator();

#ifdef USE_QRCODE
    contextMenu->addAction(showQRCodeAction);
#endif
    switch(tabIn)
    {
    case SendingTab:
        contextMenu->addAction(verifyMessageAction);
        break;
    case ReceivingTab:
        contextMenu->addAction(signMessageAction);
        contextMenu->addAction(verifyAddressAction);
        break;
    case LedgerTab:
        contextMenu->addAction(verifyAddressAction);
        break;
    }

    // Connect signals for context menu actions
    connect(copyAddressAction, SIGNAL(triggered()), this, SLOT(on_copyToClipboard_clicked()));
    connect(copyLabelAction, SIGNAL(triggered()), this, SLOT(onCopyLabelAction()));
    connect(editAction, SIGNAL(triggered()), this, SLOT(onEditAction()));
    connect(deleteAction, SIGNAL(triggered()), this, SLOT(on_deleteButton_clicked()));
    connect(showQRCodeAction, SIGNAL(triggered()), this, SLOT(on_showQRCode_clicked()));
    connect(signMessageAction, SIGNAL(triggered()), this, SLOT(on_signMessage_clicked()));
    connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(on_verifyMessage_clicked()));
    connect(verifyAddressAction, SIGNAL(triggered()), this, SLOT(on_verifyAddress_clicked()));

    connect(ui->tableView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));

    // Pass through accept action from button box
    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
}

AddressBookPage::~AddressBookPage()
{
    delete ui;
}

void AddressBookPage::setModel(AddressTableModel *modelIn)
{
    this->model = modelIn;
    if(!modelIn)
        return;

    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(modelIn);
    proxyModel->setDynamicSortFilter(true);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    switch(tab)
    {
    case ReceivingTab:
        // Receive filter
        proxyModel->setFilterRole(AddressTableModel::TypeRole);
        proxyModel->setFilterFixedString(AddressTableModel::Receive);
        break;
    case SendingTab:
        // Send filter
        proxyModel->setFilterRole(AddressTableModel::TypeRole);
        proxyModel->setFilterFixedString(AddressTableModel::Send);
        break;
    case LedgerTab:
        // Ledger filter
        proxyModel->setFilterRole(AddressTableModel::TypeRole);
        proxyModel->setFilterFixedString(AddressTableModel::ReceiveLedger);
        break;
    }
    ui->tableView->setModel(proxyModel);
    ui->tableView->sortByColumn(0, Qt::AscendingOrder);

    // Hide unwanted columns
    ui->tableView->hideColumn(AddressTableModel::IsLedger);
    ui->tableView->hideColumn(AddressTableModel::LedgerAccount);
    ui->tableView->hideColumn(AddressTableModel::LedgerIndex);

    // Set column widths
    ui->tableView->horizontalHeader()->setSectionResizeMode(
            AddressTableModel::Label, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->resizeSection(
            AddressTableModel::Address, 320);
    ui->tableView->horizontalHeader()->setSectionResizeMode(
            AddressTableModel::LedgerPath, QHeaderView::ResizeToContents);

    connect(ui->tableView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SLOT(selectionChanged()));

    // Select row for newly created address
    connect(modelIn, SIGNAL(rowsInserted(QModelIndex,int,int)),
            this, SLOT(selectNewAddress(QModelIndex,int,int)));

    selectionChanged();
}

void AddressBookPage::setOptionsModel(OptionsModel *optionsModelIn)
{
    this->optionsModel = optionsModelIn;
}

void AddressBookPage::on_copyToClipboard_clicked()
{
    GUIUtil::copyEntryData(ui->tableView, AddressTableModel::Address);
}

void AddressBookPage::onCopyLabelAction()
{
    GUIUtil::copyEntryData(ui->tableView, AddressTableModel::Label);
}

void AddressBookPage::onEditAction()
{
    if(!ui->tableView->selectionModel())
        return;
    QModelIndexList indexes = ui->tableView->selectionModel()->selectedRows();
    if(indexes.isEmpty())
        return;

    EditAddressDialog dlg(
            tab == SendingTab ?
            EditAddressDialog::EditSendingAddress :
            EditAddressDialog::EditReceivingAddress);
    dlg.setModel(model);
    QModelIndex origIndex = proxyModel->mapToSource(indexes.at(0));
    dlg.loadRow(origIndex.row());
    dlg.exec();
}

void AddressBookPage::on_signMessage_clicked()
{
    QTableView *table = ui->tableView;
    QModelIndexList indexes = table->selectionModel()->selectedRows(AddressTableModel::Address);
    QString addr;

    foreach (QModelIndex index, indexes)
    {
        QVariant address = index.data();
        addr = address.toString();
    }

    emit signMessage(addr);
}

void AddressBookPage::on_verifyMessage_clicked()
{
    QTableView *table = ui->tableView;
    QModelIndexList indexes = table->selectionModel()->selectedRows(AddressTableModel::Address);
    QString addr;

    foreach (QModelIndex index, indexes)
    {
        QVariant address = index.data();
        addr = address.toString();
    }

    emit verifyMessage(addr);
}

void AddressBookPage::on_verifyAddress_clicked()
{
    QTableView *table = ui->tableView;
    QModelIndexList indexes = table->selectionModel()->selectedRows(AddressTableModel::Address);
    QString ledgerAddress;
    uint32_t ledgerAccount;
    uint32_t ledgerIndex;

    foreach (QModelIndex index, indexes)
    {
        ledgerAddress = index.data().toString();
        ledgerAccount = index.sibling(index.row(), AddressTableModel::LedgerAccount).data().toInt();
        ledgerIndex = index.sibling(index.row(), AddressTableModel::LedgerIndex).data().toInt();
    }

    CKeyID ledgerKeyId;
    if (!CBitcoinAddress(ledgerAddress.toStdString()).GetKeyID(ledgerKeyId)) {
        // Should be unreachable
        return;
    }

    QSharedPointer<VerifyLedgerAddressWorker> worker = QSharedPointer<VerifyLedgerAddressWorker>::create();
    ledger_ui::LedgerMessageBox msgBox(this, worker, tr("Verifying address: <tt>%1</tt>").arg(ledgerAddress));
    connect(worker.data(), SIGNAL(resultReady(QString)), this, SLOT(showVerifyAddressResult(QString)));
    connect(worker.data(), SIGNAL(resultReady(QString)), &msgBox, SLOT(quit()));
    QTimer::singleShot(0, worker.data(), [worker, ledgerAccount, ledgerIndex]() { worker->verify(ledgerAccount, ledgerIndex, worker); });
    msgBox.exec();
}

void AddressBookPage::showVerifyAddressResult(QString errorMessage)
{
    if (!errorMessage.isEmpty()) {
        QMessageBox::critical(this, windowTitle(), errorMessage, QMessageBox::Ok, QMessageBox::Ok);
    }
}

void AddressBookPage::on_newAddressButton_clicked()
{
    if(!model)
        return;
    EditAddressDialog dlg(
            tab == SendingTab ?
            EditAddressDialog::NewSendingAddress :
            EditAddressDialog::NewReceivingAddress, this);
    dlg.setModel(model);
    if(dlg.exec())
    {
        newAddressToSelect = dlg.getAddress();
    }
}

void AddressBookPage::on_deleteButton_clicked()
{
    QTableView *table = ui->tableView;
    if(!table->selectionModel())
        return;
    QModelIndexList indexes = table->selectionModel()->selectedRows();
    if(!indexes.isEmpty())
    {
        table->model()->removeRow(indexes.at(0).row());
    }
}

void AddressBookPage::selectionChanged()
{
    // Set button states based on selected tab and selection
    QTableView *table = ui->tableView;
    if(!table->selectionModel())
        return;

    if(table->selectionModel()->hasSelection())
    {
        QModelIndexList ledgerColumn = table->selectionModel()->selectedRows(AddressTableModel::IsLedger);
        bool isLedger = ledgerColumn.size() && ledgerColumn[0].data().toBool();
        switch(tab)
        {
        case SendingTab:
            // In sending tab, allow deletion of selection
            ui->deleteButton->setEnabled(true);
            deleteAction->setEnabled(true);
            ui->signMessage->setEnabled(false);
            signMessageAction->setEnabled(false);
            ui->verifyMessage->setEnabled(true);
            verifyMessageAction->setEnabled(true);
            ui->verifyAddress->setEnabled(false);
            verifyAddressAction->setEnabled(false);
            break;
        case ReceivingTab:
            // Deleting receiving addresses, however, is not allowed
            ui->deleteButton->setEnabled(false);
            deleteAction->setEnabled(false);
            ui->signMessage->setEnabled(!isLedger);
            signMessageAction->setEnabled(!isLedger);
            ui->verifyMessage->setEnabled(false);
            verifyMessageAction->setEnabled(false);
            ui->verifyAddress->setEnabled(isLedger);
            verifyAddressAction->setEnabled(isLedger);
            break;
        case LedgerTab:
            ui->deleteButton->setEnabled(false);
            deleteAction->setEnabled(false);
            ui->signMessage->setEnabled(false);
            signMessageAction->setEnabled(false);
            ui->verifyMessage->setEnabled(false);
            verifyMessageAction->setEnabled(false);
            ui->verifyAddress->setEnabled(isLedger);
            verifyAddressAction->setEnabled(isLedger);
            break;
        }
        ui->copyToClipboard->setEnabled(true);
        ui->showQRCode->setEnabled(true);
    }
    else
    {
        ui->deleteButton->setEnabled(false);
        ui->showQRCode->setEnabled(false);
        ui->copyToClipboard->setEnabled(false);
        ui->signMessage->setEnabled(false);
        ui->verifyMessage->setEnabled(false);
        ui->verifyAddress->setEnabled(false);
    }
}

void AddressBookPage::done(int retval)
{
    QTableView *table = ui->tableView;
    if(!table->selectionModel() || !table->model())
        return;
    // When this is a tab/widget and not a model dialog, ignore "done"
    if(mode == ForEditing)
        return;

    // Figure out which address was selected, and return it
    QModelIndexList addressIndexes = table->selectionModel()->selectedRows(AddressTableModel::Address);
    QModelIndexList labelIndexes = table->selectionModel()->selectedRows(AddressTableModel::Label);

    if (!addressIndexes.isEmpty())
    {
        returnAddress = addressIndexes.at(0).data().toString();
        returnLabel = labelIndexes.at(0).data().toString();
    }

    if(returnAddress.isEmpty())
    {
        // If no address entry selected, return rejected
        retval = Rejected;
    }

    QDialog::done(retval);
}

void AddressBookPage::exportClicked()
{
    // CSV is currently the only supported format
    QString filename = GUIUtil::getSaveFileName(
            this,
            tr("Export Address Book Data"), QString(),
            tr("Comma separated file (*.csv)"));

    if (filename.isNull()) return;

    CSVModelWriter writer(filename);

    // name, column, role
    writer.setModel(proxyModel);
    writer.addColumn("Label", AddressTableModel::Label, Qt::EditRole);
    writer.addColumn("Address", AddressTableModel::Address, Qt::EditRole);

    if(!writer.write())
    {
        QMessageBox::critical(this, tr("Error exporting"), tr("Could not write to file %1.").arg(filename),
                              QMessageBox::Abort, QMessageBox::Abort);
    }
}

void AddressBookPage::on_showQRCode_clicked()
{
#ifdef USE_QRCODE
    QTableView *table = ui->tableView;
    QModelIndexList indexes = table->selectionModel()->selectedRows(AddressTableModel::Address);

    foreach (QModelIndex index, indexes)
    {
        QString address = index.data().toString(), label = index.sibling(index.row(), 0).data(Qt::EditRole).toString();

        QRCodeDialog *dialog = new QRCodeDialog(address, label, tab == ReceivingTab, this);
        if(optionsModel)
            dialog->setModel(optionsModel);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    }
#endif
}

void AddressBookPage::contextualMenu(const QPoint &point)
{
    QModelIndex index = ui->tableView->indexAt(point);
    if(index.isValid())
    {
        contextMenu->exec(QCursor::pos());
    }
}

void AddressBookPage::selectNewAddress(const QModelIndex &parent, int begin, int /*end*/)
{
    QModelIndex idx = proxyModel->mapFromSource(model->index(begin, AddressTableModel::Address, parent));
    if(idx.isValid() && (idx.data(Qt::EditRole).toString() == newAddressToSelect))
    {
        // Select row of newly created address, once
        ui->tableView->setFocus();
        ui->tableView->selectRow(idx.row());
        newAddressToSelect.clear();
    }
}
