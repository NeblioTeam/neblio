#include "coldstakingmodel.h"
#include "addresstablemodel.h"
#include "base58.h"
#include "boost/thread/future.hpp"
#include <boost/atomic/atomic.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

#include <QIcon>
#include <QImage>

WalletModel* ColdStakingModel::getWalletModel() { return walletModel; }

TransactionTableModel* ColdStakingModel::getTransactionTableModel() { return tableModel; }

AddressTableModel* ColdStakingModel::getAddressTableModel() { return addressTableModel; }

ColdStakingModel::ColdStakingModel() {}

ColdStakingModel::~ColdStakingModel() {}

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
    if (!pwalletMain) {
        QTimer::singleShot(5000, this, &ColdStakingModel::refresh);
        return;
    }
    cachedItems.clear();
    cachedAmount = 0;
    // First get all of the p2cs utxo inside the wallet
    std::vector<COutput> utxoList;
    pwalletMain->GetAvailableP2CSCoins(utxoList);

    if (!utxoList.empty()) {
        // Loop over each COutput into a CSDelegation
        for (const auto& utxo : utxoList) {

            const CWalletTx* wtx  = utxo.tx;
            const QString    txId = QString::fromStdString(wtx->GetHash().GetHex());
            const CTxOut&    out  = wtx->vout[utxo.i];

            // First parse the cs delegation
            boost::optional<ColdStakingCachedItem> item = parseColdStakingCachedItem(out, txId, utxo.i);
            if (!item)
                continue;

            // it's spendable only when this wallet has the keys to spend it, a.k.a is the owner
            item->isSpendable =
                (IsMine(*pwalletMain, out.scriptPubKey) & isminetype::ISMINE_SPENDABLE_ALL) != 0;
            item->cachedTotalAmount += out.nValue;
            item->delegatedUtxo.insert(txId, utxo.i);

            // Now verify if the delegation exists in the cached list
            int indexDel = cachedItems.indexOf(*item);
            if (indexDel == -1) {
                // If it doesn't, let's append it.
                cachedItems.append(*item);
            } else {
                ColdStakingCachedItem& del = cachedItems[indexDel];
                del.delegatedUtxo.unite(item->delegatedUtxo);
                del.cachedTotalAmount += item->cachedTotalAmount;
            }

            // add amount to cachedAmount if either:
            // - this is a owned delegation
            // - this is a staked delegation, and the owner is whitelisted
            //            if (!delegation.isSpendable &&
            //            !addressTableModel->isWhitelisted(delegation.ownerAddress))
            //                continue;
            cachedAmount += item->cachedTotalAmount;
        }
    }
    QMetaObject::invokeMethod(this, "emitDataSetChanged", Qt::QueuedConnection);
}

boost::optional<ColdStakingCachedItem> ColdStakingModel::parseColdStakingCachedItem(const CTxOut&  out,
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
