#include "coldstakingmodel.h"
#include "addresstablemodel.h"
#include "base58.h"
#include "boost/thread/future.hpp"
#include <boost/atomic/atomic.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

#include <QIcon>
#include <QImage>
#include <QMetaType>

std::pair<QList<ColdStakingCachedItem>, CAmount>
ColdStakingModel::ProcessColdStakingUTXOList(const std::vector<COutput>& utxoList)
{
    QList<ColdStakingCachedItem> cachedItemsResult;
    CAmount                      cachedAmountResult = 0;

    if (!utxoList.empty()) {
        // Loop over each COutput into a CSDelegation
        for (const auto& utxo : utxoList) {

            const CWalletTx* wtx  = utxo.tx;
            const QString    txId = QString::fromStdString(wtx->GetHash().GetHex());
            const CTxOut&    out  = wtx->vout[utxo.i];

            // First parse the cs delegation
            boost::optional<ColdStakingCachedItem> item = ParseColdStakingCachedItem(out, txId, utxo.i);
            if (!item)
                continue;

            // it's spendable only when this wallet has the keys to spend it, a.k.a is the owner
            item->isSpendable =
                (IsMine(*pwalletMain, out.scriptPubKey) & isminetype::ISMINE_SPENDABLE_ALL) != 0;
            item->cachedTotalAmount += out.nValue;
            item->delegatedUtxo.insert(txId, utxo.i);

            // Now verify if the delegation exists in the cached list
            int indexDel = cachedItemsResult.indexOf(*item);
            if (indexDel == -1) {
                // If it doesn't, let's append it.
                cachedItemsResult.append(*item);
            } else {
                ColdStakingCachedItem& del = cachedItemsResult[indexDel];
                del.delegatedUtxo.unite(item->delegatedUtxo);
                del.cachedTotalAmount += item->cachedTotalAmount;
            }

            // add amount to cachedAmount if either:
            // - this is a owned delegation
            // - this is a staked delegation, and the owner is whitelisted
            //            if (!delegation.isSpendable &&
            //            !addressTableModel->isWhitelisted(delegation.ownerAddress))
            //                continue;
            cachedAmountResult += item->cachedTotalAmount;
        }
    }

    return std::make_pair(cachedItemsResult, cachedAmountResult);
}

WalletModel* ColdStakingModel::getWalletModel() { return walletModel; }

TransactionTableModel* ColdStakingModel::getTransactionTableModel() { return tableModel; }

AddressTableModel* ColdStakingModel::getAddressTableModel() { return addressTableModel; }

ColdStakingModel::ColdStakingModel()
{
    qRegisterMetaType<QSharedPointer<std::vector<COutput>>>("QSharedPointer<std::vector<COutput>>");
    qRegisterMetaType<QSharedPointer<AvailableP2CSCoinsWorker>>(
        "QSharedPointer<AvailableP2CSCoinsWorker>");
    qRegisterMetaType<QSharedPointer<std::pair<QList<ColdStakingCachedItem>, CAmount>>>(
        "QSharedPointer<std::pair<QList<ColdStakingCachedItem>, CAmount>>");
    retrieveOutputsThread.setObjectName("neblio-CsUTXORetriever"); // thread name
    retrieveOutputsThread.start();
}

ColdStakingModel::~ColdStakingModel()
{
    retrieveOutputsThread.quit();
    retrieveOutputsThread.wait();
}

int ColdStakingModel::rowCount(const QModelIndex& /*parent*/) const { return cachedItems.size(); }

int ColdStakingModel::columnCount(const QModelIndex& /*parent*/) const { return COLUMN_COUNT; }

QVariant ColdStakingModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    int row = index.row();
    if (row >= static_cast<int>(cachedItems.size()))
        return QVariant();

    const ColdStakingCachedItem& rec = cachedItems[row];

    if (role == ColdStakingModel::ColumnIndex::OWNER_ADDRESS) {
        return QString::fromStdString(rec.ownerAddress);
    }
    if (role == ColdStakingModel::ColumnIndex::OWNER_ADDRESS_LABEL) {
        return addressTableModel->labelForAddress(QString::fromStdString(rec.ownerAddress));
    }
    if (role == ColdStakingModel::ColumnIndex::STAKING_ADDRESS) {
        return QString::fromStdString(rec.stakingAddress);
    }
    if (role == ColdStakingModel::ColumnIndex::STAKING_ADDRESS_LABEL) {
        return addressTableModel->labelForAddress(QString::fromStdString(rec.stakingAddress));
    }
    if (role == ColdStakingModel::ColumnIndex::IS_WHITELISTED) {
        return addressTableModel->purposeForAddress(rec.ownerAddress)
                   .compare(AddressBook::AddressBookPurpose::DELEGATOR) == 0;
    }
    if (role == ColdStakingModel::ColumnIndex::TOTAL_STACKEABLE_AMOUNT_STR) {
        return QString::fromStdString(FormatMoney(rec.cachedTotalAmount));
    }
    if (role == ColdStakingModel::ColumnIndex::TOTAL_STACKEABLE_AMOUNT) {
        return qint64(rec.cachedTotalAmount);
    }
    if (role == ColdStakingModel::ColumnIndex::IS_RECEIVED_DELEGATION) {
        return rec.isSpendable;
    }
    return QVariant();
}

void ColdStakingModel::setWalletModel(WalletModel* wModel)
{
    walletModel       = wModel;
    addressTableModel = wModel->getAddressTableModel();
    tableModel        = wModel->getTransactionTableModel();
}

void ColdStakingModel::refresh()
{
    if (!appInitiated) {
        QTimer::singleShot(5000, this, &ColdStakingModel::refresh);
        return;
    }

    if (!pwalletMain) {
        QTimer::singleShot(5000, this, &ColdStakingModel::refresh);
        return;
    }

    // we want only one instane of the worker running
    if (isWorkerRunning) {
        QTimer::singleShot(5000, this, &ColdStakingModel::refresh);
        return;
    }

    isWorkerRunning = true;

    QSharedPointer<AvailableP2CSCoinsWorker> worker = QSharedPointer<AvailableP2CSCoinsWorker>::create();
    worker->moveToThread(&retrieveOutputsThread);
    connect(worker.data(), &AvailableP2CSCoinsWorker::resultReady, this,
            &ColdStakingModel::finishRefresh, Qt::QueuedConnection);
    QTimer::singleShot(0, worker.data(), [worker]() { worker->retrieveOutputs(worker); });
}

void ColdStakingModel::finishRefresh(
    QSharedPointer<std::pair<QList<ColdStakingCachedItem>, CAmount>> itemsAndAmount)
{
    assert(itemsAndAmount);

    // force sync since we got a vector from another thread
    std::atomic_thread_fence(std::memory_order_seq_cst);

    beginResetModel();
    // this is an RAII hack to guarantee that the function will end the model reset
    // WalletModel has nothing to do with this. It's just a dummy variable
    auto modelResetEnderFunctor = [this](WalletModel*) { endResetModel(); };
    std::unique_ptr<WalletModel, decltype(modelResetEnderFunctor)> txEnder(walletModel,
                                                                           modelResetEnderFunctor);

    std::tie(cachedItems, cachedAmount) = *itemsAndAmount;

    QMetaObject::invokeMethod(this, "emitDataSetChanged", Qt::QueuedConnection);

    isWorkerRunning = false;
}

boost::optional<ColdStakingCachedItem> ColdStakingModel::ParseColdStakingCachedItem(const CTxOut&  out,
                                                                                    const QString& txId,
                                                                                    const int& utxoIndex)
{
    txnouttype                  type;
    std::vector<CTxDestination> addresses;
    int                         nRequired;

    if (!ExtractDestinations(out.scriptPubKey, type, addresses, nRequired) || addresses.size() != 2) {
        printf("%s : Error extracting P2CS destinations for utxo: %s-%d", __func__,
               txId.toStdString().c_str(), utxoIndex);
        return boost::none;
    }

    std::string stakingAddressStr = CBitcoinAddress(addresses[0]).ToString();

    std::string ownerAddressStr = CBitcoinAddress(addresses[1]).ToString();

    return boost::make_optional(ColdStakingCachedItem(stakingAddressStr, ownerAddressStr));
}

bool ColdStakingModel::whitelist(const QModelIndex& modelIndex)
{
    QString address = modelIndex.data(ColumnIndex::OWNER_ADDRESS).toString();
    if (addressTableModel->isWhitelisted(address.toStdString())) {
        return error("trying to whitelist already whitelisted address");
    }

    if (!walletModel->whitelistAddressFromColdStaking(address))
        return false;

    // address whitelisted - update cached amount and row data
    const int idx = modelIndex.row();
    cachedAmount += cachedItems[idx].cachedTotalAmount;
    removeRowAndEmitDataChanged(idx);

    return true;
}

bool ColdStakingModel::blacklist(const QModelIndex& modelIndex)
{
    QString address = modelIndex.data(ColumnIndex::OWNER_ADDRESS).toString();
    if (!addressTableModel->isWhitelisted(address.toStdString())) {
        return error("trying to blacklist already blacklisted address");
    }

    if (!walletModel->blacklistAddressFromColdStaking(address))
        return false;

    // address blacklisted - update cached amount and row data
    const int idx = modelIndex.row();
    cachedAmount -= cachedItems[idx].cachedTotalAmount;
    removeRowAndEmitDataChanged(idx);

    return true;
}

void ColdStakingModel::removeRowAndEmitDataChanged(const int idx)
{
    beginRemoveRows(QModelIndex(), idx, idx);
    endRemoveRows();
    emit dataChanged(index(idx, 0, QModelIndex()), index(idx, COLUMN_COUNT, QModelIndex()));
}

void ColdStakingModel::updateCSList()
{
    QMetaObject::invokeMethod(this, "refresh", Qt::QueuedConnection);
}

void ColdStakingModel::emitDataSetChanged()
{
    emit dataChanged(index(0, 0, QModelIndex()), index(cachedItems.size(), COLUMN_COUNT, QModelIndex()));
}

void AvailableP2CSCoinsWorker::retrieveOutputs(QSharedPointer<AvailableP2CSCoinsWorker> workerPtr)
{
    QSharedPointer<std::vector<COutput>> utxoList = QSharedPointer<std::vector<COutput>>::create();
    while (!fShutdown && !pwalletMain->GetAvailableP2CSCoins(*utxoList)) {
        QThread::msleep(100);
    }

    auto result = ColdStakingModel::ProcessColdStakingUTXOList(*utxoList);

    emit resultReady(
        QSharedPointer<std::pair<QList<ColdStakingCachedItem>, CAmount>>::create(std::move(result)));
    workerPtr.reset();
}
