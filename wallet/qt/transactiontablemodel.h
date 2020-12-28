#ifndef TRANSACTIONTABLEMODEL_H
#define TRANSACTIONTABLEMODEL_H

#include <QAbstractTableModel>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <deque>
#include <uint256.h>

class CWallet;
class TransactionTablePriv;
class TransactionRecord;
class WalletModel;

class TxsRetrieverWorker : public QObject
{
    Q_OBJECT

public slots:
    // we use the shared pointer argument to ensure that workerPtr will be deleted after doing the
    // retrieval
    void getTxs(CWallet* wallet, QSharedPointer<TxsRetrieverWorker> workerPtr);

signals:
    // Signal that balance in wallet changed
    void resultReady(QSharedPointer<QList<TransactionRecord>> records);
};

/** UI model for the transaction table of a wallet.
 */
class TransactionTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit TransactionTableModel(CWallet* wallet, WalletModel* parent = 0);
    ~TransactionTableModel();

    enum ColumnIndex
    {
        Status    = 0,
        Date      = 1,
        Type      = 2,
        ToAddress = 3,
        Amount    = 4
    };

    /** Roles to get specific information from a transaction row.
        These are independent of column.
    */
    enum RoleIndex
    {
        /** Type of transaction */
        TypeRole = Qt::UserRole,
        /** Date and time this transaction was created */
        DateRole,
        /** Long description (HTML format) */
        LongDescriptionRole,
        /** Address of transaction */
        AddressRole,
        /** Label of address related to transaction */
        LabelRole,
        /** Net amount of transaction */
        AmountRole,
        /** Unique identifier */
        TxIDRole,
        /** Is transaction confirmed? */
        ConfirmedRole,
        /** Formatted amount, without brackets when unconfirmed */
        FormattedAmountRole,
        IsNTP1Role,
        NTP1MetadataHexRole,
        NTP1MetadataRole,
        OpReturnHexRole,
        /** Transaction status (TransactionRecord::Status) */
        StatusRole
    };

    int         rowCount(const QModelIndex& parent) const;
    int         columnCount(const QModelIndex& parent) const;
    QVariant    data(const QModelIndex& index, int role) const;
    QVariant    headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const;
    bool        isTxsRetrieverThreadRunning() const;

private:
    CWallet*              wallet;
    WalletModel*          walletModel;
    QStringList           columns;
    TransactionTablePriv* priv;

    std::deque<std::pair<uint256, int>> walletUpdatesQueue;
    QTimer                              walletUpdatesQueueConsumer;

    QThread txsRetrieverThread;
    bool    txsRetrieverWorkerRunning = false;

    QString  lookupAddress(const std::string& address, bool tooltip) const;
    QVariant addressColor(const TransactionRecord* wtx) const;
    QString  formatTxStatus(const TransactionRecord* wtx) const;
    QString  formatTxDate(const TransactionRecord* wtx) const;
    QString  formatTxType(const TransactionRecord* wtx) const;
    QString  formatTxToAddress(const TransactionRecord* wtx, bool tooltip) const;
    QString  formatTxAmount(const TransactionRecord* wtx, bool showUnconfirmed = true) const;
    QString  formatTooltip(const TransactionRecord* rec) const;
    QVariant txStatusDecoration(const TransactionRecord* wtx) const;
    QVariant txAddressDecoration(const TransactionRecord* wtx) const;
    void     pushToWalletUpdate(uint256 hash, int status);

public slots:
    void updateTransaction(const QString& hash, int status);
    void updateConfirmations();
    void updateDisplayUnit();

    void refreshWallet();
    void finishRefreshWallet(QSharedPointer<QList<TransactionRecord>> records);

    friend class TransactionTablePriv;

private slots:
    void consumeWalletUpdatesQueue();

signals:
    void txArrived(const QString& hash);
    void triggerRefeshTxs(CWallet* wallet, QSharedPointer<TxsRetrieverWorker> workerPtr);
};

#endif
