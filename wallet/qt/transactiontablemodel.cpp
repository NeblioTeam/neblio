#include "transactiontablemodel.h"
#include "addresstablemodel.h"
#include "bitcoinunits.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "transactiondesc.h"
#include "transactionrecord.h"
#include "walletmodel.h"

#include "ui_interface.h"
#include "wallet.h"

#include <QColor>
#include <QDateTime>
#include <QIcon>
#include <QList>
#include <QLocale>
#include <QTimer>
#include <QtAlgorithms>

// Amount column is right-aligned it contains numbers
static int column_alignments[] = {Qt::AlignLeft | Qt::AlignVCenter, Qt::AlignLeft | Qt::AlignVCenter,
                                  Qt::AlignLeft | Qt::AlignVCenter, Qt::AlignLeft | Qt::AlignVCenter,
                                  Qt::AlignRight | Qt::AlignVCenter};

// Comparison operator for sort/binary search of model tx list
struct TxLessThan
{
    bool operator()(const TransactionRecord& a, const TransactionRecord& b) const
    {
        return a.hash < b.hash;
    }
    bool operator()(const TransactionRecord& a, const uint256& b) const { return a.hash < b; }
    bool operator()(const uint256& a, const TransactionRecord& b) const { return a < b.hash; }
};

// Private implementation
class TransactionTablePriv
{
public slots:

public:
    TransactionTablePriv(CWallet* wallet, TransactionTableModel* parent) : wallet(wallet), parent(parent)
    {
    }

    CWallet*               wallet;
    TransactionTableModel* parent;

    /* Local cache of wallet.
     * As it is in the same order as the CWallet, by definition
     * this is sorted by sha256.
     */
    QList<TransactionRecord> cachedWallet;

    /* Update our model of the wallet incrementally, to synchronize our model of the wallet
       with that of the core.

       Call with transaction that was added, removed or changed.
     */
    [[nodiscard]] bool updateWallet(const uint256& hash, int status)
    {
        {
            if (parent->isTxsRetrieverThreadRunning()) {
                return false;
            }

            // These locks were MOVED to the caller
            // TRY_LOCK(cs_main, lockMain);
            // if (!lockMain) {
            //     return false;
            // }
            // TRY_LOCK(wallet->cs_wallet, lockWallet);
            // if (!lockWallet) {
            //     return false;
            // }

            // Find transaction in wallet
            std::map<uint256, CWalletTx>::const_iterator mi       = wallet->mapWallet.find(hash);
            const bool                                   inWallet = mi != wallet->mapWallet.end();

            // Find bounds of this transaction in model
            QList<TransactionRecord>::iterator lower =
                std::lower_bound(cachedWallet.begin(), cachedWallet.end(), hash, TxLessThan());
            QList<TransactionRecord>::iterator upper =
                std::upper_bound(cachedWallet.begin(), cachedWallet.end(), hash, TxLessThan());
            int  lowerIndex = (lower - cachedWallet.begin());
            int  upperIndex = (upper - cachedWallet.begin());
            bool inModel    = (lower != upper);

            // Determine whether to show transaction or not
            bool showTransaction = (inWallet && TransactionRecord::showTransaction(mi->second));

            if (status == CT_UPDATED) {
                if (showTransaction && !inModel)
                    status = CT_NEW; /* Not in model, but want to show, treat as new */
                if (!showTransaction && inModel)
                    status = CT_DELETED; /* In model, but want to hide, treat as deleted */
            }

            OutputDebugStringF(
                "   inWallet=%i inModel=%i Index=%i-%i showTransaction=%i derivedStatus=%i\n", inWallet,
                inModel, lowerIndex, upperIndex, showTransaction, status);

            switch (status) {
            case CT_NEW:
                if (inModel) {
                    OutputDebugStringF(
                        "Warning: updateWallet: Got CT_NEW, but transaction is already in model\n");
                    break;
                }
                if (!inWallet) {
                    OutputDebugStringF(
                        "Warning: updateWallet: Got CT_NEW, but transaction is not in wallet\n");
                    break;
                }
                if (showTransaction) {
                    // Added -- insert at the right position
                    QList<TransactionRecord> toInsert =
                        TransactionRecord::decomposeTransaction(wallet, mi->second);
                    if (!toInsert.isEmpty()) /* only if something to insert */
                    {
                        parent->beginInsertRows(QModelIndex(), lowerIndex,
                                                lowerIndex + toInsert.size() - 1);
                        int insert_idx = lowerIndex;
                        foreach (const TransactionRecord& rec, toInsert) {
                            cachedWallet.insert(insert_idx, rec);
                            insert_idx += 1;
                        }
                        parent->endInsertRows();
                    }
                }
                break;
            case CT_DELETED:
                if (!inModel) {
                    OutputDebugStringF(
                        "Warning: updateWallet: Got CT_DELETED, but transaction is not in model\n");
                    break;
                }
                // Removed -- remove entire transaction from table
                parent->beginRemoveRows(QModelIndex(), lowerIndex, upperIndex - 1);
                cachedWallet.erase(lower, upper);
                parent->endRemoveRows();
                break;
            case CT_UPDATED:
                // Miscellaneous updates -- nothing to do, status update will take care of this, and is
                // only computed for visible transactions.
                break;
            }
        }
        return true;
    }

    int size() { return cachedWallet.size(); }

    TransactionRecord* index(int idx)
    {
        if (idx >= 0 && idx < cachedWallet.size()) {
            TransactionRecord* rec = &cachedWallet[idx];

            // Get required locks upfront. This avoids the GUI from getting
            // stuck if the core is holding the locks for a longer time - for
            // example, during a wallet rescan.
            //
            // If a status update is needed (blocks came in since last check),
            //  update the status of this transaction from the wallet. Otherwise,
            // simply re-use the cached status.
            TRY_LOCK(cs_main, lockMain);
            if (lockMain) {
                TRY_LOCK(wallet->cs_wallet, lockWallet);
                if (lockWallet && rec->statusUpdateNeeded()) {
                    std::map<uint256, CWalletTx>::iterator mi = wallet->mapWallet.find(rec->hash);

                    if (mi != wallet->mapWallet.end()) {
                        rec->updateStatus(mi->second);
                    }
                }
            }
            return rec;
        } else {
            return 0;
        }
    }

    QString describe(const TransactionRecord* rec)
    {
        {
            LOCK2(cs_main, wallet->cs_wallet);
            std::map<uint256, CWalletTx>::iterator mi = wallet->mapWallet.find(rec->hash);
            if (mi != wallet->mapWallet.end()) {
                return TransactionDesc::toHTML(wallet, mi->second);
            }
        }
        return QString("");
    }
};

TransactionTableModel::TransactionTableModel(CWallet* wallet, WalletModel* parent)
    : QAbstractTableModel(parent), wallet(wallet), walletModel(parent),
      priv(new TransactionTablePriv(wallet, this))
{
    columns << QString() << tr("Date") << tr("Type") << tr("Address") << tr("Amount");

    qRegisterMetaType<QSharedPointer<TxsRetrieverWorker>>("QSharedPointer<TxsRetrieverWorker>");
    qRegisterMetaType<QSharedPointer<QList<TransactionRecord>>>(
        "QSharedPointer<QList<TransactionRecord>>");
    qRegisterMetaType<CWallet*>("CWallet*");

    txsRetrieverThread.setObjectName("neblio-txRetrieverWorker"); // thread name
    txsRetrieverThread.start();

    connect(walletModel->getOptionsModel(), &OptionsModel::displayUnitChanged, this,
            &TransactionTableModel::updateDisplayUnit);

    QMetaObject::invokeMethod(this, "refreshWallet", Qt::QueuedConnection);

    connect(&walletUpdatesQueueConsumer, &QTimer::timeout, this,
            &TransactionTableModel::consumeWalletUpdatesQueue);
    walletUpdatesQueueConsumer.start(1000);
}

TransactionTableModel::~TransactionTableModel()
{
    txsRetrieverThread.quit();
    txsRetrieverThread.wait();
    delete priv;
}

void TransactionTableModel::updateTransaction(const QString& hash, int status)
{
    uint256 updated;
    updated.SetHex(hash.toStdString());

    // we use singleShot because Qt's invokeMethod doesn't support functors before a late version
    GUIUtil::AsyncQtCall(this, [this, updated, status]() { this->pushToWalletUpdate(updated, status); });

    emit txArrived(hash);
}

void TransactionTableModel::updateConfirmations()
{
    // Blocks came in since last poll.
    // Invalidate status (number of confirmations) and (possibly) description
    //  for all rows. Qt is smart enough to only actually request the data for the
    //  visible rows.
    emit dataChanged(index(0, Status), index(priv->size() - 1, Status));
    emit dataChanged(index(0, ToAddress), index(priv->size() - 1, ToAddress));
}

int TransactionTableModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int TransactionTableModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QString TransactionTableModel::formatTxStatus(const TransactionRecord* wtx) const
{
    QString status;

    switch (wtx->status.status) {
    case TransactionStatus::OpenUntilBlock:
        status = tr("Open for %n more block(s)", "", wtx->status.open_for);
        break;
    case TransactionStatus::OpenUntilDate:
        status = tr("Open until %1").arg(GUIUtil::dateTimeStr(wtx->status.open_for));
        break;
    case TransactionStatus::Offline:
        status = tr("Offline");
        break;
    case TransactionStatus::Unconfirmed:
        status = tr("Unconfirmed");
        break;
    case TransactionStatus::Confirming:
        status = tr("Confirming (%1 of %2 recommended confirmations)")
                     .arg(wtx->status.depth)
                     .arg(TransactionRecord::RecommendedNumConfirmations);
        break;
    case TransactionStatus::Confirmed:
        status = tr("Confirmed (%1 confirmations)").arg(wtx->status.depth);
        break;
    case TransactionStatus::Conflicted:
        status = tr("Conflicted");
        break;
    case TransactionStatus::Immature:
        status = tr("Immature (%1 confirmations, will be available after %2)")
                     .arg(wtx->status.depth)
                     .arg(wtx->status.depth + wtx->status.matures_in);
        break;
    case TransactionStatus::MaturesWarning:
        status = tr("This block was not received by any other nodes and will probably not be accepted!");
        break;
    case TransactionStatus::NotAccepted:
        status = tr("Generated but not accepted");
        break;
    }

    return status;
}

QString TransactionTableModel::formatTxDate(const TransactionRecord* wtx) const
{
    if (wtx->time) {
        return GUIUtil::dateTimeStr(wtx->time);
    } else {
        return QString();
    }
}

/* Look up address in address book, if found return label (address)
   otherwise just return (address)
 */
QString TransactionTableModel::lookupAddress(const std::string& address, bool tooltip) const
{
    QString label =
        walletModel->getAddressTableModel()->labelForAddress(QString::fromStdString(address));
    QString description;
    if (!label.isEmpty()) {
        description += label + QString(" ");
    }
    if (label.isEmpty() || walletModel->getOptionsModel()->getDisplayAddresses() || tooltip) {
        description += QString("(") + QString::fromStdString(address) + QString(")");
    }
    return description;
}

QString TransactionTableModel::formatTxType(const TransactionRecord* wtx) const
{
    switch (wtx->type) {
    case TransactionRecord::RecvWithAddress:
        return tr("Received with");
    case TransactionRecord::RecvFromOther:
        return tr("Received from");
    case TransactionRecord::SendToAddress:
    case TransactionRecord::SendToOther:
        return tr("Sent to");
    case TransactionRecord::SendToSelf:
        return tr("Payment to yourself");
    case TransactionRecord::Generated:
        return tr("Mined");
    case TransactionRecord::ColdStaker:
        return tr("Received cold-stake reward");
    case TransactionRecord::ColdDelegator:
        return tr("Sent cold-stake delegation");
    default:
        return QString();
    }
}

QVariant TransactionTableModel::txAddressDecoration(const TransactionRecord* wtx) const
{
    switch (wtx->type) {
    case TransactionRecord::Generated:
        return QIcon(":/icons/tx_mined");
    case TransactionRecord::RecvWithAddress:
    case TransactionRecord::RecvFromOther:
        return QIcon(":/icons/tx_input");
    case TransactionRecord::SendToAddress:
    case TransactionRecord::SendToOther:
        return QIcon(":/icons/tx_output");
    case TransactionRecord::ColdStaker:
        return QIcon(":/icons/cold_delegate_1");
    case TransactionRecord::ColdDelegator:
        return QIcon(":/icons/cold_delegate_0");
    default:
        return QIcon(":/icons/tx_inout");
    }
    return QVariant();
}

void TransactionTableModel::pushToWalletUpdate(uint256 hash, int status)
{
    walletUpdatesQueue.push_back(std::make_pair(std::move(hash), status));
}

void TransactionTableModel::refreshWallet()
{
    OutputDebugStringF("refreshWallet\n");
    if (txsRetrieverWorkerRunning) {
        QTimer::singleShot(1000, this, &TransactionTableModel::refreshWallet);
        return;
    }

    txsRetrieverWorkerRunning = true;

    QSharedPointer<TxsRetrieverWorker> worker = QSharedPointer<TxsRetrieverWorker>::create();
    worker->moveToThread(&txsRetrieverThread);
    connect(worker.data(), &TxsRetrieverWorker::resultReady, this,
            &TransactionTableModel::finishRefreshWallet, Qt::QueuedConnection);
    GUIUtil::AsyncQtCall(worker.data(), [this, worker]() { worker->getTxs(wallet, worker); });
}

void TransactionTableModel::finishRefreshWallet(QSharedPointer<QList<TransactionRecord>> records)
{
    assert(records);

    // force sync since we got a vector from another thread
    std::atomic_thread_fence(std::memory_order_seq_cst);

    beginResetModel();
    // this is an RAII hack to guarantee that the function will end the model reset
    // WalletModel has nothing to do with this. It's just a dummy variable
    auto modelResetEnderFunctor = [this](WalletModel*) { endResetModel(); };
    std::unique_ptr<WalletModel, decltype(modelResetEnderFunctor)> txEnder(walletModel,
                                                                           modelResetEnderFunctor);

    priv->cachedWallet = *records;

    txsRetrieverWorkerRunning = false;
}

void TransactionTableModel::consumeWalletUpdatesQueue()
{
    if (walletUpdatesQueue.empty()) {
        return;
    }

    if (txsRetrieverWorkerRunning) {
        return;
    }

    TRY_LOCK(cs_main, lockMain);
    if (!lockMain) {
        return;
    }
    TRY_LOCK(wallet->cs_wallet, lockWallet);
    if (!lockWallet) {
        return;
    }

    static constexpr int MAX_TO_POP = 200;

    for (int i = 0; i < MAX_TO_POP; i++) {
        if (walletUpdatesQueue.empty()) {
            break;
        }
        const bool success =
            priv->updateWallet(walletUpdatesQueue.front().first, walletUpdatesQueue.front().second);

        if (success) {
            walletUpdatesQueue.pop_front();
        }
    }
}

QString TransactionTableModel::formatTxToAddress(const TransactionRecord* wtx, bool tooltip) const
{
    switch (wtx->type) {
    case TransactionRecord::RecvFromOther:
        return QString::fromStdString(wtx->address);
    case TransactionRecord::RecvWithAddress:
    case TransactionRecord::SendToAddress:
    case TransactionRecord::Generated:
        return lookupAddress(wtx->address, tooltip);
    case TransactionRecord::SendToOther:
        return QString::fromStdString(wtx->address);
    case TransactionRecord::ColdStaker:
        return QString::fromStdString(wtx->address);
    case TransactionRecord::ColdDelegator:
        return QString::fromStdString(wtx->address);
    case TransactionRecord::SendToSelf:
    default:
        return tr("(n/a)");
    }
}

QVariant TransactionTableModel::addressColor(const TransactionRecord* wtx) const
{
    // Show addresses without label in a less visible color
    switch (wtx->type) {
    case TransactionRecord::RecvWithAddress:
    case TransactionRecord::SendToAddress:
    case TransactionRecord::ColdDelegator:
    case TransactionRecord::ColdStaker:
    case TransactionRecord::Generated: {
        QString label =
            walletModel->getAddressTableModel()->labelForAddress(QString::fromStdString(wtx->address));
        if (label.isEmpty())
            return COLOR_BAREADDRESS;
    } break;
    case TransactionRecord::SendToSelf:
        return COLOR_BAREADDRESS;
    default:
        break;
    }
    return QVariant();
}

QString TransactionTableModel::formatTxAmount(const TransactionRecord* wtx, bool showUnconfirmed) const
{
    // count total NTP1 tokens
    NTP1Int totalTokens = 0;
    if (wtx->ntp1DataLoaded) {
        for (int i = 0; i < (int)wtx->ntp1tx.getTxOutCount(); i++) {
            for (int j = 0; j < (int)wtx->ntp1tx.getTxOut(i).tokenCount(); j++) {
                totalTokens += wtx->ntp1tx.getTxOut(i).getToken(j).getAmount();
            }
        }
    }
    QString str;
    if (totalTokens == 0) {
        str = BitcoinUnits::format(walletModel->getOptionsModel()->getDisplayUnit(),
                                   wtx->credit + wtx->debit);
    } else {
        str = "NTP1";
    }
    if (showUnconfirmed) {
        if (!wtx->status.countsForBalance) {
            str = QString("[") + str + QString("]");
        }
    }
    return QString(str);
}

QVariant TransactionTableModel::txStatusDecoration(const TransactionRecord* wtx) const
{
    switch (wtx->status.status) {
    case TransactionStatus::OpenUntilBlock:
    case TransactionStatus::OpenUntilDate:
        return QColor(64, 64, 255);
    case TransactionStatus::Offline:
        return QColor(192, 192, 192);
    case TransactionStatus::Unconfirmed:
        return QIcon(":/icons/transaction_0");
    case TransactionStatus::Confirming:
        switch (wtx->status.depth) {
        case 1:
            return QIcon(":/icons/transaction_1");
        case 2:
            return QIcon(":/icons/transaction_2");
        case 3:
            return QIcon(":/icons/transaction_3");
        case 4:
            return QIcon(":/icons/transaction_4");
        default:
            return QIcon(":/icons/transaction_5");
        };
    case TransactionStatus::Confirmed:
        return QIcon(":/icons/transaction_confirmed");
    case TransactionStatus::Conflicted:
        return QIcon(":/icons/transaction_conflicted");
    case TransactionStatus::Immature: {
        int total = wtx->status.depth + wtx->status.matures_in;
        int part  = (wtx->status.depth * 4 / total) + 1;
        return QIcon(QString(":/icons/transaction_%1").arg(part));
    }
    case TransactionStatus::MaturesWarning:
    case TransactionStatus::NotAccepted:
        return QIcon(":/icons/transaction_0");
    }
    return QColor(0, 0, 0);
}

QString TransactionTableModel::formatTooltip(const TransactionRecord* rec) const
{
    QString tooltip = formatTxStatus(rec) + QString("\n") + formatTxType(rec);
    if (rec->type == TransactionRecord::RecvFromOther || rec->type == TransactionRecord::SendToOther ||
        rec->type == TransactionRecord::SendToAddress ||
        rec->type == TransactionRecord::RecvWithAddress) {
        tooltip += QString(" ") + formatTxToAddress(rec, true);
    }
    return tooltip;
}

QVariant TransactionTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();
    const TransactionRecord* rec = static_cast<TransactionRecord*>(index.internalPointer());

    switch (role) {
    case Qt::DecorationRole:
        switch (index.column()) {
        case Status:
            return txStatusDecoration(rec);
        case ToAddress:
            return txAddressDecoration(rec);
        }
        break;
    case Qt::DisplayRole:
        switch (index.column()) {
        case Date:
            return formatTxDate(rec);
        case Type:
            return formatTxType(rec);
        case ToAddress:
            return formatTxToAddress(rec, false);
        case Amount:
            return formatTxAmount(rec);
        }
        break;
    case Qt::EditRole:
        // Edit role is used for sorting, so return the unformatted values
        switch (index.column()) {
        case Status:
            return QString::fromStdString(rec->status.sortKey);
        case Date:
            return rec->time;
        case Type:
            return formatTxType(rec);
        case ToAddress:
            return formatTxToAddress(rec, true);
        case Amount:
            return rec->credit + rec->debit;
        }
        break;
    case Qt::ToolTipRole:
        return formatTooltip(rec);
    case Qt::TextAlignmentRole:
        return column_alignments[index.column()];
    case Qt::ForegroundRole:
        // Non-confirmed (but not immature) as transactions are grey
        if (!rec->status.countsForBalance && rec->status.status != TransactionStatus::Immature) {
            return COLOR_UNCONFIRMED;
        }
        if (index.column() == Amount && (rec->credit + rec->debit) < 0) {
            return COLOR_NEGATIVE;
        }
        if (index.column() == ToAddress) {
            return addressColor(rec);
        }
        break;
    case TypeRole:
        return rec->type;
    case DateRole:
        return QDateTime::fromTime_t(static_cast<uint>(rec->time));
    case LongDescriptionRole:
        return priv->describe(rec);
    case AddressRole:
        return QString::fromStdString(rec->address);
    case LabelRole:
        return walletModel->getAddressTableModel()->labelForAddress(
            QString::fromStdString(rec->address));
    case AmountRole:
        return rec->credit + rec->debit;
    case TxIDRole:
        return QString::fromStdString(rec->getTxID());
    case ConfirmedRole:
        return rec->status.countsForBalance;
    case IsNTP1Role:
        return rec->ntp1DataLoaded;
    case FormattedAmountRole:
        return formatTxAmount(rec, false);
    case StatusRole:
        return rec->status.status;
    case NTP1MetadataRole: {
        try {
            std::string                 opRet = rec->ntp1tx.getNTP1OpReturnScriptHex();
            std::shared_ptr<NTP1Script> s     = NTP1Script::ParseScript(opRet);
            return QString::fromStdString(s->GetMetadataAsString(s.get(), rec->tx));
        } catch (...) {
        }
    }
    case NTP1MetadataHexRole: {
        try {
            std::string                 opRet = rec->ntp1tx.getNTP1OpReturnScriptHex();
            std::shared_ptr<NTP1Script> s     = NTP1Script::ParseScript(opRet);
            return QString::fromStdString(
                boost::algorithm::hex(s->GetMetadataAsString(s.get(), rec->tx)));
        } catch (...) {
        }
    }
    case OpReturnHexRole:
        try {
            return QString::fromStdString(rec->ntp1tx.getNTP1OpReturnScriptHex());
        } catch (...) {
        }
    }
    return QVariant();
}

QVariant TransactionTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole) {
            return columns[section];
        } else if (role == Qt::TextAlignmentRole) {
            return column_alignments[section];
        } else if (role == Qt::ToolTipRole) {
            switch (section) {
            case Status:
                return tr("Transaction status. Hover over this field to show number of confirmations.");
            case Date:
                return tr("Date and time that the transaction was received.");
            case Type:
                return tr("Type of transaction.");
            case ToAddress:
                return tr("Destination address of transaction.");
            case Amount:
                return tr("Amount removed from or added to balance.");
            }
        }
    }
    return QVariant();
}

QModelIndex TransactionTableModel::index(int row, int column, const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    TransactionRecord* data = priv->index(row);
    if (data) {
        return createIndex(row, column, data);
    } else {
        return QModelIndex();
    }
}

bool TransactionTableModel::isTxsRetrieverThreadRunning() const { return txsRetrieverWorkerRunning; }

void TransactionTableModel::updateDisplayUnit()
{
    // emit dataChanged to update Amount column with the current unit
    emit dataChanged(index(0, Amount), index(priv->size() - 1, Amount));
}

void TxsRetrieverWorker::getTxs(CWallet* wallet, QSharedPointer<TxsRetrieverWorker> workerPtr)
{
    QSharedPointer<QList<TransactionRecord>> cachedWallet =
        QSharedPointer<QList<TransactionRecord>>::create();

    const std::vector<CWalletTx> walletTxes = wallet->getWalletTxs();

    for (const CWalletTx& wtx : walletTxes) {
        if (TransactionRecord::showTransaction(wtx))
            cachedWallet->append(TransactionRecord::decomposeTransaction(wallet, wtx));
        if (fShutdown.load(boost::memory_order_relaxed))
            break;
    }

    resultReady(cachedWallet);
    workerPtr.reset();
}
