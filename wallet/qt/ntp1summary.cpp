#include "ntp1summary.h"
#include "ui_ntp1summary.h"

#include "bitcoinunits.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "ntp1/ntp1tokenlistitemdelegate.h"
#include "optionsmodel.h"
#include "qt/ntp1/ntp1tokenlistmodel.h"
#include "main.h"
#include "walletmodel.h"

#include <QAction>
#include <QClipboard>
#include <QDesktopServices>
#include <QKeyEvent>
#include <QMenu>
#include <QUrl>

#include "txdb.h"

const QString NTP1Summary::copyTokenIdText          = "Copy Token ID";
const QString NTP1Summary::copyTokenSymbolText      = "Copy Token Symbol";
const QString NTP1Summary::copyTokenNameText        = "Copy Token Name";
const QString NTP1Summary::copyIssuanceTxid         = "Copy issuance transaction ID";
const QString NTP1Summary::viewInBlockExplorerText  = "Show in block explorer";
const QString NTP1Summary::viewIssuanceMetadataText = "Show issuance metadata";

void NTP1Summary::GetAlreadyIssuedNTP1Tokens(boost::promise<std::unordered_set<std::string>>& promise)
{
    try {
        LOCK(cs_main);
        std::unordered_set<std::string> result;
        CTxDB                           txdb;
        std::vector<uint256>            txs;
        // retrieve all issuance transactions hashes from db
        if (txdb.ReadAllIssuanceTxs(txs)) {
            for (const uint256& hash : txs) {
                // get every tx from db
                NTP1Transaction ntp1tx;
                if (txdb.ReadNTP1Tx(hash, ntp1tx)) {
                    // make sure that the transaction is in the main chain
                    CTransaction tx;
                    uint256      blockHash;
                    if (!GetTransaction(hash, tx, blockHash)) {
                        throw std::runtime_error(
                            "Failed to find the block that belongs to transaction: " + hash.ToString());
                    }
                    auto it = mapBlockIndex.find(blockHash);
                    if (it != mapBlockIndex.cend() && it->second->IsInMainChain()) {
                        std::string tokenSymbol = ntp1tx.getTokenSymbolIfIssuance();
                        // symbols should be in upper case for the comparison to work
                        std::transform(tokenSymbol.begin(), tokenSymbol.end(), tokenSymbol.begin(),
                                       ::toupper);
                        result.insert(tokenSymbol);
                    }
                } else {
                    throw std::runtime_error("Failed to read transaction " + hash.ToString() +
                                             " from blockchain database");
                }
            }
            promise.set_value(result);
        } else {
            throw std::runtime_error(
                "Failed to retrieve the list of already issued token symbols. This is "
                "necessary to avoid having duplicate token names");
        }
    } catch (std::exception& ex) {
        promise.set_exception(boost::current_exception());
    }
}

NTP1Summary::NTP1Summary(QWidget* parent)
    : QWidget(parent), ui(new Ui_NTP1Summary), currentBalance(-1), currentStake(0),
      currentUnconfirmedBalance(-1), currentImmatureBalance(-1), model(0), filter(0),
      tokenDelegate(new NTP1TokenListItemDelegate)
{
    model          = new NTP1TokenListModel;
    metadataViewer = new NTP1MetadataViewer;
    metadataViewer->setModal(true);
    ui->setupUi(this);

    ntp1LoaderConcluderTimer = new QTimer(this);

    ui->listTokens->setItemDelegate(tokenDelegate);
    ui->listTokens->setIconSize(
        QSize(NTP1TokenListItemDelegate::DECORATION_SIZE, NTP1TokenListItemDelegate::DECORATION_SIZE));
    ui->listTokens->setMinimumHeight(NTP1TokenListItemDelegate::NUM_ITEMS *
                                     (NTP1TokenListItemDelegate::DECORATION_SIZE + 2));
    ui->listTokens->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTokens, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTokenClicked(QModelIndex)));

    //    ui->showSendDialogButton->setText(sendDialogHiddenStr);
    //    connect(ui->showSendDialogButton, &QPushButton::clicked, this,
    //            &NTP1Summary::slot_actToShowSendTokensView);

    // init "out of sync" warning labels
    ui->labelBlockchainSyncStatus->setText("(" + tr("out of sync") + ")");

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);

    setupContextMenu();

    filter = new NTP1TokenListFilterProxy(ui->filter_lineEdit);
    setModel(model);

    connect(model, &NTP1TokenListModel::signal_walletUpdateRunning, ui->upper_table_loading_label,
            &QLabel::setVisible);
    connect(ui->filter_lineEdit, &QLineEdit::textChanged, filter,
            &NTP1TokenListFilterProxy::setFilterWildcard);
    connect(ui->issueNewNTP1TokenButton, &QPushButton::clicked, this,
            &NTP1Summary::slot_showIssueNewTokenDialog);
}

void NTP1Summary::handleTokenClicked(const QModelIndex& index)
{
    if (filter)
        emit tokenClicked(filter->mapToSource(index));
}

void NTP1Summary::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_F && (event->modifiers() & Qt::ControlModifier)) {
        ui->filter_lineEdit->setFocus();
    }
}

void NTP1Summary::setupContextMenu()
{
    ui->listTokens->setContextMenuPolicy(Qt::CustomContextMenu);
    contextMenu               = new QMenu(this);
    copyTokenIdAction         = new QAction(copyTokenIdText, this);
    copyTokenSymbolAction     = new QAction(copyTokenSymbolText, this);
    copyTokenNameAction       = new QAction(copyTokenNameText, this);
    copyIssuanceTxidAction    = new QAction(copyIssuanceTxid, this);
    viewInBlockExplorerAction = new QAction(viewInBlockExplorerText, this);
    showMetadataAction        = new QAction(viewIssuanceMetadataText, this);
    contextMenu->addAction(copyTokenIdAction);
    contextMenu->addAction(copyTokenSymbolAction);
    contextMenu->addAction(copyTokenNameAction);
    contextMenu->addSeparator();
    contextMenu->addAction(copyIssuanceTxidAction);
    contextMenu->addAction(viewInBlockExplorerAction);
    contextMenu->addSeparator();
    contextMenu->addAction(showMetadataAction);
    connect(ui->listTokens, &TokensListView::customContextMenuRequested, this,
            &NTP1Summary::slot_contextMenuRequested);

    connect(copyTokenIdAction, &QAction::triggered, this, &NTP1Summary::slot_copyTokenIdAction);
    connect(copyTokenSymbolAction, &QAction::triggered, this, &NTP1Summary::slot_copyTokenSymbolAction);
    connect(copyTokenNameAction, &QAction::triggered, this, &NTP1Summary::slot_copyTokenNameAction);
    connect(copyIssuanceTxidAction, &QAction::triggered, this,
            &NTP1Summary::slot_copyIssuanceTxidAction);
    connect(viewInBlockExplorerAction, &QAction::triggered, this,
            &NTP1Summary::slot_visitInBlockExplorerAction);
    connect(showMetadataAction, &QAction::triggered, this, &NTP1Summary::slot_showMetadataAction);
    connect(ntp1LoaderConcluderTimer, &QTimer::timeout, this, &NTP1Summary::slot_concludeLoadNTP1Tokens);
}

void NTP1Summary::slot_copyTokenIdAction()
{
    QModelIndexList selected = ui->listTokens->selectedIndexesP();
    std::set<int>   rows;
    for (long i = 0; i < selected.size(); i++) {
        QModelIndex index = selected.at(i);
        int         row   = index.row();
        rows.insert(row);
    }
    if (rows.size() != 1) {
        QMessageBox::warning(this, "Failed to copy",
                             "Failed to copy Token ID; selected items size is not equal to one");
        return;
    }
    QModelIndex idx   = ui->listTokens->model()->index(*rows.begin(), 0);
    QString resultStr = ui->listTokens->model()->data(idx, NTP1TokenListModel::TokenIdRole).toString();
    if (!resultStr.isEmpty()) {
        QClipboard* clipboard = QGuiApplication::clipboard();
        clipboard->setText(resultStr);
    } else {
        QMessageBox::warning(this, "Failed to copy", "No information to include in the clipboard");
    }
}

void NTP1Summary::slot_copyTokenSymbolAction()
{
    QModelIndexList selected = ui->listTokens->selectedIndexesP();
    std::set<int>   rows;
    for (long i = 0; i < selected.size(); i++) {
        QModelIndex index = selected.at(i);
        int         row   = index.row();
        rows.insert(row);
    }
    if (rows.size() != 1) {
        QMessageBox::warning(this, "Failed to copy",
                             "Failed to copy Token Symbol; selected items size is not equal to one");
        return;
    }
    QModelIndex idx   = ui->listTokens->model()->index(*rows.begin(), 0);
    QString resultStr = ui->listTokens->model()->data(idx, NTP1TokenListModel::TokenNameRole).toString();
    if (!resultStr.isEmpty()) {
        QClipboard* clipboard = QGuiApplication::clipboard();
        clipboard->setText(resultStr);
    } else {
        QMessageBox::warning(this, "Failed to copy", "No information to include in the clipboard");
    }
}

void NTP1Summary::slot_copyTokenNameAction()
{
    QModelIndexList selected = ui->listTokens->selectedIndexesP();
    std::set<int>   rows;
    for (long i = 0; i < selected.size(); i++) {
        QModelIndex index = selected.at(i);
        int         row   = index.row();
        rows.insert(row);
    }
    if (rows.size() != 1) {
        QMessageBox::warning(this, "Failed to copy",
                             "Failed to copy Token Name; selected items size is not equal to one");
        return;
    }
    QString resultStr = ui->listTokens->model()
                            ->data(ui->listTokens->model()->index(*rows.begin(), 0),
                                   NTP1TokenListModel::TokenDescriptionRole)
                            .toString();
    if (!resultStr.isEmpty()) {
        QClipboard* clipboard = QGuiApplication::clipboard();
        clipboard->setText(resultStr);
    } else {
        QMessageBox::warning(this, "Failed to copy", "No information to include in the clipboard");
    }
}

void NTP1Summary::slot_copyIssuanceTxidAction()
{
    QModelIndexList selected = ui->listTokens->selectedIndexesP();
    std::set<int>   rows;
    for (long i = 0; i < selected.size(); i++) {
        QModelIndex index = selected.at(i);
        int         row   = index.row();
        rows.insert(row);
    }
    if (rows.size() != 1) {
        QMessageBox::warning(this, "Failed get URL",
                             "Failed to get Token ID; selected items size is not equal to one");
        return;
    }
    QModelIndex idx = ui->listTokens->model()->index(*rows.begin(), 0);
    QString     resultStr =
        ui->listTokens->model()->data(idx, NTP1TokenListModel::IssuanceTxidRole).toString();
    if (!resultStr.isEmpty()) {
        QClipboard* clipboard = QGuiApplication::clipboard();
        clipboard->setText(resultStr);
    } else {
        QMessageBox::warning(this, "Failed to copy", "No information to include in the clipboard");
    }
}

void NTP1Summary::slot_visitInBlockExplorerAction()
{
    QModelIndexList selected = ui->listTokens->selectedIndexesP();
    std::set<int>   rows;
    for (long i = 0; i < selected.size(); i++) {
        QModelIndex index = selected.at(i);
        int         row   = index.row();
        rows.insert(row);
    }
    if (rows.size() != 1) {
        QMessageBox::warning(this, "Failed get URL",
                             "Failed to get Token ID; selected items size is not equal to one");
        return;
    }
    QModelIndex idx   = ui->listTokens->model()->index(*rows.begin(), 0);
    QString resultStr = ui->listTokens->model()->data(idx, NTP1TokenListModel::TokenIdRole).toString();
    if (!resultStr.isEmpty()) {
        QString link = QString::fromStdString(
            NTP1Tools::GetURL_ExplorerTokenInfo(resultStr.toStdString(), fTestNet));
        if (!QDesktopServices::openUrl(QUrl(link))) {
            QMessageBox::warning(
                this, "Failed to open browser",
                "Could not open the web browser to view token information in block explorer");
        }
    } else {
        QMessageBox::warning(this, "Failed to get token ID", "No information retrieved for token ID");
    }
}

void NTP1Summary::slot_showMetadataAction()
{
    QModelIndexList selected = ui->listTokens->selectedIndexesP();
    std::set<int>   rows;
    for (long i = 0; i < selected.size(); i++) {
        QModelIndex index = selected.at(i);
        int         row   = index.row();
        rows.insert(row);
    }
    if (rows.size() != 1) {
        QMessageBox::warning(this, "Failed get URL",
                             "Failed to get Token ID; selected items size is not equal to one");
        return;
    }
    QModelIndex                   idx       = ui->listTokens->model()->index(*rows.begin(), 0);
    NTP1TokenListModel::RoleIndex role      = NTP1TokenListModel::IssuanceTxidRole;
    QString                       resultStr = ui->listTokens->model()->data(idx, role).toString();
    if (!resultStr.isEmpty()) {
        uint256 issuanceTxid(resultStr.toStdString());
        try {
            auto it = issuanceTxidVsMetadata.find(issuanceTxid);
            if (it == issuanceTxidVsMetadata.cend()) {
                json_spirit::Value v;
                try {
                    v = NTP1Transaction::GetNTP1IssuanceMetadata(issuanceTxid);
                } catch(std::exception& ex) {
                    QMessageBox::warning(this, "Failed to get issuance transaction",
                                         "Failed to retrieve issuance transaction with error: " +
                                             QString(ex.what()));
                    return;
                }
                issuanceTxidVsMetadata[issuanceTxid] = v;
                std::stringstream ss;
                json_spirit::write_formatted(v, ss);
                metadataViewer->setJsonStr(ss.str());
            } else {
                std::stringstream ss;
                json_spirit::write_formatted(it->second, ss);
                metadataViewer->setJsonStr(ss.str());
            }
            metadataViewer->show();
            metadataViewer->raise();
        } catch (std::exception& ex) {
            QMessageBox::warning(this, "Failed to get issuance transaction",
                                 "Failed to retrieve issuance transaction with error: " +
                                     QString(ex.what()));
        } catch (...) {
            QMessageBox::warning(this, "Failed to get issuance transaction",
                                 "Failed to retrieve issuance transaction with an unknown error.");
        }
    } else {
        QMessageBox::warning(
            this, "Failed to get issuance txid",
            "Failed to retrieve issuance txid, which is required to retrieve the metadata");
    }
}

void NTP1Summary::slot_showIssueNewTokenDialog()
{
    if (!isNTP1TokensLoadRunning) {
        printf("Loading NTP1 tokens list...\n");
        QSize iconSize(ui->issueNewNTP1TokenButton->height(), ui->issueNewNTP1TokenButton->height());
        ui->loadIssuedNTP1SpinnerMovie->setScaledSize(iconSize);

        ui->loadIssuedNTP1SpinnerLabel->setToolTip("Loading issued NTP1 tokens...");
        ui->loadIssuedNTP1SpinnerLabel->setMovie(ui->loadIssuedNTP1SpinnerMovie);
        ui->loadIssuedNTP1SpinnerMovie->start();

        ui->loadIssuedNTP1SpinnerLabel->setVisible(true);
        ui->issueNewNTP1TokenButton->setVisible(false);

        alreadyIssuedNTP1SymbolsPromise = boost::promise<std::unordered_set<std::string>>();
        alreadyIssuedNTP1SymbolsFuture  = alreadyIssuedNTP1SymbolsPromise.get_future();
        boost::thread loadNTP1IssuedTokensThread(boost::bind(
            &NTP1Summary::GetAlreadyIssuedNTP1Tokens, boost::ref(alreadyIssuedNTP1SymbolsPromise)));
        loadNTP1IssuedTokensThread.detach();
        ntp1LoaderConcluderTimer->start(ntp1LoaderConcluderTimerTimeout);
        isNTP1TokensLoadRunning = true;
    }
}

void NTP1Summary::slot_concludeLoadNTP1Tokens()
{
    try {
        if (isNTP1TokensLoadRunning && alreadyIssuedNTP1SymbolsFuture.is_ready()) {
            printf("Concluding loading issued NTP1 tokens...\n");

            ntp1LoaderConcluderTimer->stop();

            std::unordered_set<std::string> alreadyIssuedSymbols = alreadyIssuedNTP1SymbolsFuture.get();

            ui->loadIssuedNTP1SpinnerLabel->setVisible(false);
            ui->issueNewNTP1TokenButton->setVisible(true);
            ui->loadIssuedNTP1SpinnerMovie->stop();

            isNTP1TokensLoadRunning = false;

            ui->issueNewNTP1TokenDialog->setAlreadyIssuedTokensSymbols(alreadyIssuedSymbols);
            ui->issueNewNTP1TokenDialog->show();
        } else {
        }
    } catch (std::exception& ex) {
        QMessageBox::warning(this, "Failed to retrieve token names",
                             "Failed to retrieve the list of already issued token symbols. This is "
                             "necessary to avoid having duplicate token names. Error: " +
                                 QString(ex.what()));
    }
}

NTP1Summary::~NTP1Summary() { delete ui; }

NTP1TokenListModel* NTP1Summary::getTokenListModel() const { return model; }

void NTP1Summary::setModel(NTP1TokenListModel* model)
{
    if (model) {
        filter->setSourceModel(model);
        ui->listTokens->setModel(filter);
    }
}

void NTP1Summary::showOutOfSyncWarning(bool fShow) { ui->labelBlockchainSyncStatus->setVisible(fShow); }

void NTP1Summary::slot_contextMenuRequested(QPoint pos)
{
    QModelIndexList selected = ui->listTokens->selectedIndexesP();
    if (selected.size() != 1) {
        copyTokenIdAction->setDisabled(true);
        metadataViewer->setDisabled(true);
    } else {
        copyTokenIdAction->setDisabled(false);
        metadataViewer->setDisabled(false);
    }
    contextMenu->popup(ui->listTokens->viewport()->mapToGlobal(pos));
}
