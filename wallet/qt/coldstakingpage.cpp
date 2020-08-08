#include "coldstakingpage.h"

#include "bitcoinunits.h"
#include "coinstakinglistitemdelegate.h"
#include "coldstakingmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "main.h"
#include "newstakedelegationdialog.h"
#include "optionsmodel.h"
#include "qt/coldstakingmodel.h"
#include "transactiontablemodel.h"
#include "wallet.h"
#include "walletmodel.h"

#include <QAction>
#include <QClipboard>
#include <QDesktopServices>
#include <QKeyEvent>
#include <QMenu>
#include <QUrl>

#include "txdb.h"

#define LOAD_MIN_TIME_INTERVAL 15

const QString ColdStakingPage::copyOwnerAddressText  = "Copy owner address";
const QString ColdStakingPage::copyStakerAddressText = "Copy staker address";
const QString ColdStakingPage::copyAmountText        = "Copy amount";
const QString ColdStakingPage::enableStakingText     = "Enable staking for this address";
const QString ColdStakingPage::disableStakingText    = "Disable staking for this address";
const QString ColdStakingPage::cantStakeText         = "Cannot stake an address you delegated";

ColdStakingPage::ColdStakingPage(QWidget* parent)
    : QWidget(parent), ui(new Ui_ColdStaking), model(new ColdStakingModel),
      itemDelegate(new CoinStakingListItemDelegate)
{
    ui->setupUi(this);

    ui->listColdStakingView->setItemDelegate(itemDelegate);
    ui->listColdStakingView->setIconSize(QSize(CoinStakingListItemDelegate::DECORATION_SIZE,
                                               CoinStakingListItemDelegate::DECORATION_SIZE));
    ui->listColdStakingView->setMinimumHeight(CoinStakingListItemDelegate::NUM_ITEMS *
                                              (CoinStakingListItemDelegate::DECORATION_SIZE + 2));
    ui->listColdStakingView->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listColdStakingView, SIGNAL(clicked(QModelIndex)), this,
            SLOT(handleTokenClicked(QModelIndex)));

    setupContextMenu();

    setModel(model);

    refreshData();

    newStakeDelegationDialog = new NewStakeDelegationDialog(this);

    connect(ui->delegateStakeButton, &QPushButton::clicked, newStakeDelegationDialog,
            &NewStakeDelegationDialog::open);
}

void ColdStakingPage::handleTokenClicked(const QModelIndex& /*index*/) {}

void ColdStakingPage::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_F && (event->modifiers() & Qt::ControlModifier)) {
        //        ui->filter_lineEdit->setFocus();
    }
}

void ColdStakingPage::setupContextMenu()
{
    ui->listColdStakingView->setContextMenuPolicy(Qt::CustomContextMenu);
    contextMenu          = new QMenu(this);
    copyOwnerAddrAction  = new QAction(copyOwnerAddressText, this);
    copyStakerAddrAction = new QAction(copyStakerAddressText, this);
    copyAmountAction     = new QAction(copyAmountText, this);
    toggleStakingAction  = new QAction(enableStakingText, this);

    contextMenu->addAction(copyOwnerAddrAction);
    contextMenu->addAction(copyStakerAddrAction);
    contextMenu->addAction(copyAmountAction);
    contextMenu->addSeparator();
    contextMenu->addAction(toggleStakingAction);

    connect(ui->listColdStakingView, &ColdStakingListView::customContextMenuRequested, this,
            &ColdStakingPage::slot_contextMenuRequested);

    connect(copyOwnerAddrAction, &QAction::triggered, this, &ColdStakingPage::slot_copyOwnerAddr);
    connect(copyStakerAddrAction, &QAction::triggered, this, &ColdStakingPage::slot_copyStakerAddr);
    connect(copyAmountAction, &QAction::triggered, this, &ColdStakingPage::slot_copyAmount);
}

void ColdStakingPage::slot_copyOwnerAddr()
{
    copyField(ColdStakingModel::ColumnIndex::OWNER_ADDRESS, "owner address");
}

void ColdStakingPage::slot_copyStakerAddr()
{
    copyField(ColdStakingModel::ColumnIndex::STAKING_ADDRESS, "staker address");
}

void ColdStakingPage::slot_copyAmount()
{
    copyField(ColdStakingModel::ColumnIndex::TOTAL_STACKEABLE_AMOUNT_STR, "amount");
}

void ColdStakingPage::slot_enableStaking(const QModelIndex& idx) { model->whitelist(idx); }

void ColdStakingPage::slot_disableStaking(const QModelIndex& idx) { model->blacklist(idx); }

ColdStakingPage::~ColdStakingPage()
{
    notifyWalletConnection.disconnect();
    delete ui;
}

ColdStakingModel* ColdStakingPage::getTokenListModel() const { return model; }

void ColdStakingPage::setModel(ColdStakingModel* model)
{
    if (model) {
        ui->listColdStakingView->setModel(model);
    }
}

void ColdStakingPage::setWalletModel(WalletModel* wModel)
{
    model->setWalletModel(wModel);
    connect(model->getTransactionTableModel(), &TransactionTableModel::txArrived, this,
            &ColdStakingPage::onTxArrived);

    notifyWalletConnection = model->getWalletModel()->getWallet()->NotifyAddressBookChanged.connect(
        [this](CWallet* /*wallet*/, const CTxDestination& /*address*/, const std::string& /*label*/,
               bool /*isMine*/, const std::string& /*purpose*/,
               ChangeType /*status*/) { refreshData(); });
}

void ColdStakingPage::slot_contextMenuRequested(QPoint pos)
{
    QModelIndexList selected = ui->listColdStakingView->selectedIndexesP();
    if (selected.size() != 1) {
        copyOwnerAddrAction->setDisabled(true);
    } else {
        copyOwnerAddrAction->setDisabled(false);
        configureToggleStakingAction(selected.front());
    }
    contextMenu->popup(ui->listColdStakingView->viewport()->mapToGlobal(pos));
}

void ColdStakingPage::onTxArrived(const QString& /*hash*/) { tryRefreshData(); }

void ColdStakingPage::tryRefreshData()
{
    // Check for min update time to not reload the UI so often if the node is syncing.
    int64_t now = GetTime();
    if (lastRefreshTime + LOAD_MIN_TIME_INTERVAL < now) {
        lastRefreshTime = now;
        refreshData();
    }
}

void ColdStakingPage::refreshData() { model->updateCSList(); }

void ColdStakingPage::copyField(ColdStakingModel::ColumnIndex column, const QString& columnName)
{
    QModelIndexList selected = ui->listColdStakingView->selectedIndexesP();
    std::set<int>   rows;
    for (long i = 0; i < selected.size(); i++) {
        QModelIndex index = selected.at(i);
        int         row   = index.row();
        rows.insert(row);
    }
    if (rows.size() != 1) {
        QMessageBox::warning(this, "Failed to copy",
                             "Failed to copy " + columnName +
                                 "; selected items size is not equal to one");
        return;
    }
    QModelIndex idx       = ui->listColdStakingView->model()->index(*rows.begin(), 0);
    QString     resultStr = ui->listColdStakingView->model()->data(idx, column).toString();
    if (!resultStr.isEmpty()) {
        QClipboard* clipboard = QGuiApplication::clipboard();
        clipboard->setText(resultStr);
    } else {
        QMessageBox::warning(this, "Failed to copy", "No information to include in the clipboard");
    }
}

void ColdStakingPage::configureToggleStakingAction(const QModelIndex& idx)
{
    bool isWhitelisted = ui->listColdStakingView->model()
                             ->data(idx, ColdStakingModel::ColumnIndex::IS_WHITELISTED)
                             .toBool();
    bool isStaker = ui->listColdStakingView->model()
                        ->data(idx, ColdStakingModel::ColumnIndex::IS_RECEIVED_DELEGATION)
                        .toBool();

    // disconnect everything and reconnect it below
    toggleStakingAction->disconnect();

    if (!isStaker) {
        if (isWhitelisted) {
            toggleStakingAction->setText(disableStakingText);
            connect(toggleStakingAction, &QAction::triggered, this, [=]() { slot_disableStaking(idx); });
        } else {
            toggleStakingAction->setText(enableStakingText);
            connect(toggleStakingAction, &QAction::triggered, this, [=]() { slot_enableStaking(idx); });
        }
        toggleStakingAction->setEnabled(true);
    } else {
        toggleStakingAction->setText(cantStakeText);
        toggleStakingAction->setEnabled(false);
    }
}
