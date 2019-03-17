#include "ntp1summary.h"
#include "ui_ntp1summary.h"

#include "bitcoinunits.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "ntp1/ntp1tokenlistitemdelegate.h"
#include "optionsmodel.h"
#include "qt/ntp1/ntp1tokenlistmodel.h"
#include "walletmodel.h"

#include <QAction>
#include <QClipboard>
#include <QDesktopServices>
#include <QKeyEvent>
#include <QMenu>
#include <QUrl>

const QString NTP1Summary::copyTokenIdText          = "Copy Token ID";
const QString NTP1Summary::copyTokenSymbolText      = "Copy Token Symbol";
const QString NTP1Summary::copyTokenNameText        = "Copy Token Name";
const QString NTP1Summary::viewInBlockExplorerText  = "Show in block explorer";
const QString NTP1Summary::viewIssuanceMetadataText = "Show issuance metadata";

NTP1Summary::NTP1Summary(QWidget* parent)
    : QWidget(parent), ui(new Ui_NTP1Summary), currentBalance(-1), currentStake(0),
      currentUnconfirmedBalance(-1), currentImmatureBalance(-1), model(0), filter(0),
      tokenDelegate(new NTP1TokenListItemDelegate)
{
    model          = new NTP1TokenListModel;
    metadataViewer = new NTP1MetadataViewer;
    metadataViewer->setModal(true);
    ui->setupUi(this);

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
    viewInBlockExplorerAction = new QAction(viewInBlockExplorerText, this);
    showMetadataAction        = new QAction(viewIssuanceMetadataText, this);
    contextMenu->addAction(copyTokenIdAction);
    contextMenu->addAction(copyTokenSymbolAction);
    contextMenu->addAction(copyTokenNameAction);
    contextMenu->addSeparator();
    contextMenu->addAction(viewInBlockExplorerAction);
    contextMenu->addSeparator();
    contextMenu->addAction(showMetadataAction);
    connect(ui->listTokens, &TokensListView::customContextMenuRequested, this,
            &NTP1Summary::slot_contextMenuRequested);

    connect(copyTokenIdAction, &QAction::triggered, this, &NTP1Summary::slot_copyTokenIdAction);
    connect(copyTokenSymbolAction, &QAction::triggered, this, &NTP1Summary::slot_copyTokenSymbolAction);
    connect(copyTokenNameAction, &QAction::triggered, this, &NTP1Summary::slot_copyTokenNameAction);
    connect(viewInBlockExplorerAction, &QAction::triggered, this,
            &NTP1Summary::slot_visitInBlockExplorerAction);
    connect(showMetadataAction, &QAction::triggered, this, &NTP1Summary::slot_showMetadataAction);
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
                json_spirit::Value v = NTP1Transaction::GetNTP1IssuanceMetadata(issuanceTxid);
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
