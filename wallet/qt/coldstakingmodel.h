#ifndef COLDSTAKINGMODEL_H
#define COLDSTAKINGMODEL_H

#include <QAbstractTableModel>
#include <QSharedPointer>
#include <QThread>
#include <QTimer>
#include <atomic>

#include "init.h"
#include "wallet.h"
#include "walletmodel.h"

class ColdStakingCachedItem
{
public:
    ColdStakingCachedItem() {}
    ColdStakingCachedItem(const std::string& _stakingAddress, const std::string& _ownerAddress)
        : stakingAddress(_stakingAddress), ownerAddress(_ownerAddress), cachedTotalAmount(0)
    {
    }

    std::string stakingAddress;
    std::string ownerAddress;
    /// Map of txId --> index num for stakeable utxo delegations
    QMap<QString, int> delegatedUtxo;
    // Sum of all delegations to this owner address
    CAmount cachedTotalAmount;

    // coin owner side, set to true if it can be spend
    bool isSpendable;

    bool operator==(const ColdStakingCachedItem& obj) { return obj.ownerAddress == ownerAddress; }
};

class AvailableP2CSCoinsWorker : public QObject
{
    Q_OBJECT

public slots:
    // we use the shared pointer argument to ensure that workerPtr will be deleted after doing the
    // retrieval
    void retrieveOutputs(QSharedPointer<AvailableP2CSCoinsWorker> workerPtr)
    {
        QSharedPointer<std::vector<COutput>> utxoList = QSharedPointer<std::vector<COutput>>::create();
        while (!fShutdown && !pwalletMain->GetAvailableP2CSCoins(*utxoList)) {
            QThread::msleep(100);
        }

        emit resultReady(utxoList);
        workerPtr.reset();
    }

signals:
    void resultReady(QSharedPointer<std::vector<COutput>> utxoListPtr);
};

class ColdStakingModel : public QAbstractTableModel
{
    Q_OBJECT

    QList<ColdStakingCachedItem> cachedItems;
    CAmount                      cachedAmount;

    WalletModel*           walletModel;
    AddressTableModel*     addressTableModel = nullptr;
    TransactionTableModel* tableModel        = nullptr;

    static std::pair<QList<ColdStakingCachedItem>, CAmount>
    ProcessColdStakingUTXOList(const std::vector<COutput>& utxoList);

    QThread retrieveOutputsThread;
    bool    isWorkerRunning = false;

public:
    WalletModel*           getWalletModel();
    TransactionTableModel* getTransactionTableModel();
    AddressTableModel*     getAddressTableModel();

    enum ColumnIndex
    {
        OWNER_ADDRESS = Qt::UserRole,
        OWNER_ADDRESS_LABEL,
        STAKING_ADDRESS,
        STAKING_ADDRESS_LABEL,
        IS_WHITELISTED,
        IS_WHITELISTED_STRING,
        DELEGATED_UTXO_IDS,
        TOTAL_STACKEABLE_AMOUNT_STR,
        TOTAL_STACKEABLE_AMOUNT,
        IS_RECEIVED_DELEGATION,
        COLUMN_COUNT
    };

    ColdStakingModel();
    virtual ~ColdStakingModel();

    int      rowCount(const QModelIndex& parent) const Q_DECL_OVERRIDE;
    int      columnCount(const QModelIndex& parent) const Q_DECL_OVERRIDE;
    QVariant data(const QModelIndex& index, int role) const Q_DECL_OVERRIDE;

    void setWalletModel(WalletModel* wModel);

    void updateCSList();

    boost::optional<ColdStakingCachedItem> static parseColdStakingCachedItem(const CTxOut&  out,
                                                                             const QString& txId,
                                                                             const int&     utxoIndex);

    bool whitelist(const QModelIndex& modelIndex);
    bool blacklist(const QModelIndex& index);

    void removeRowAndEmitDataChanged(const int idx);

signals:
    void triggerWorkerRetrieveOutputs(QSharedPointer<AvailableP2CSCoinsWorker> workerPtr);

public slots:
    void refresh();
    void finishRefresh(QSharedPointer<std::vector<COutput>> utxoListPtr);
    void emitDataSetChanged();
};

#endif // COLDSTAKINGMODEL_H
