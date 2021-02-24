#ifndef CLIENTMODEL_H
#define CLIENTMODEL_H

#include <QObject>

#include "globals.h"

class OptionsModel;
class AddressTableModel;
class TransactionTableModel;
class CWallet;

QT_BEGIN_NAMESPACE
class QDateTime;
class QTimer;
QT_END_NAMESPACE

struct ChainTipData
{
    boost::atomic_int      height{0};
    boost::atomic_int64_t  time{0};
    boost::atomic<uint256> hash{0};
    boost::atomic_bool     isInitialSync{true};
};

extern int64_t nLastBlockTipUpdateNotification;

/** Model for Bitcoin network client. */
class ClientModel : public QObject
{
    Q_OBJECT
public:
    explicit ClientModel(OptionsModel* optionsModel, QObject* parent = 0);
    ~ClientModel();

    OptionsModel* getOptionsModel();

    int getNumConnections() const;
    int getNumBlocks() const;
    int getNumBlocksAtStartup();

    QDateTime getLastBlockDate() const;

    //! Return true if client connected to testnet
    bool isTestNet() const;
    //! Return true if core is doing initial block download
    bool inInitialBlockDownload() const;
    //! Return true if core is importing blocks
    bool isImporting() const;
    //! Return conservative estimate of total number of blocks, or 0 if unknown
    int getNumBlocksOfPeers() const;
    //! Return warnings to be displayed in status bar
    QString getStatusBarWarnings() const;

    QString formatFullVersion() const;
    QString formatBuildDate() const;
    QString clientName() const;
    QString formatClientStartupTime() const;

    void setTipBlock(const CBlockIndex& pindex, bool initialSync);

private:
    OptionsModel* optionsModel;

    int cachedNumBlocks;
    int cachedNumBlocksOfPeers;

    int numBlocksAtStartup;

    QTimer* pollTimer;

    ChainTipData cachedTip;

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();
signals:
    void numConnectionsChanged(int count);
    void numBlocksChanged(int count, int countOfPeers);

    //! Asynchronous error notification
    void error(const QString& title, const QString& message, bool modal);

public slots:
    void updateTimer();
    void updateNumConnections(int numConnections);
    void updateAlert(const QString& hash, int status);
};

#endif // CLIENTMODEL_H
