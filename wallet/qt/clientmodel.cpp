#include "clientmodel.h"
#include "addresstablemodel.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "transactiontablemodel.h"

#include "main.h"
#include "ui_interface.h"

#include <QDateTime>
#include <QTimer>

static const int64_t nClientStartupTime = GetTime();

int64_t nLastBlockTipUpdateNotification = 0;

ClientModel::ClientModel(OptionsModel* optionsModelIn, QObject* parent)
    : QObject(parent), optionsModel(optionsModelIn), cachedNumBlocks(0), cachedNumBlocksOfPeers(0),
      pollTimer(0)
{
    numBlocksAtStartup = -1;

    pollTimer = new QTimer(this);
    pollTimer->setInterval(MODEL_UPDATE_DELAY);
    pollTimer->start();
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(updateTimer()));

    subscribeToCoreSignals();

    const boost::optional<CBlockIndex> bi = CTxDB().GetBestBlockIndex();
    if (bi) {
        setTipBlock(*bi, true);
    } else {
        setTipBlock(*pindexGenesisBlock, true);
    }
}

ClientModel::~ClientModel() { unsubscribeFromCoreSignals(); }

int ClientModel::getNumConnections() const
{
    LOCK(cs_vNodes);
    return vNodes.size();
}

int ClientModel::getNumBlocks() const
{
    // The lock was removed after changing nBestHeight to atomic
    //    LOCK(cs_main);
    return CTxDB().GetBestChainHeight().value_or(0);
}

int ClientModel::getNumBlocksAtStartup()
{
    if (numBlocksAtStartup == -1)
        numBlocksAtStartup = getNumBlocks();
    return numBlocksAtStartup;
}

QDateTime ClientModel::getLastBlockDate() const
{
    const boost::optional<CBlockIndex> bi = CTxDB().GetBestBlockIndex();
    if (bi)
        return QDateTime::fromTime_t(cachedTip.time);
    else
        return QDateTime::fromTime_t(Params().GenesisBlock().GetBlockTime()); // Genesis block's time
}

void ClientModel::updateTimer()
{
    // Get required lock upfront. This avoids the GUI from getting stuck on
    // periodical polls if the core is holding the locks for a longer time -
    // for example, during a wallet rescan.

    // This is disabled because both getNumBlocks() and getNumBlocksOfPeers() are now thread-safe
    //    TRY_LOCK(cs_main, lockMain);
    //    if (!lockMain)
    //        return;

    // Some quantities (such as number of blocks) change so fast that we don't want to be notified for
    // each change. Periodically check and update with a timer.
    int newNumBlocks        = getNumBlocks();
    int newNumBlocksOfPeers = getNumBlocksOfPeers();

    if (cachedNumBlocks != newNumBlocks || cachedNumBlocksOfPeers != newNumBlocksOfPeers) {
        cachedNumBlocks        = newNumBlocks;
        cachedNumBlocksOfPeers = newNumBlocksOfPeers;

        emit numBlocksChanged(newNumBlocks, newNumBlocksOfPeers);
    }
}

void ClientModel::updateNumConnections(int numConnections)
{
    emit numConnectionsChanged(numConnections);
}

bool ClientModel::isTestNet() const { return Params().NetType() != NetworkType::Mainnet; }

bool ClientModel::inInitialBlockDownload() const { return IsInitialBlockDownload(CTxDB()); }

bool ClientModel::isImporting() const { return fImporting.load(); }

int ClientModel::getNumBlocksOfPeers() const { return GetNumBlocksOfPeers(); }

QString ClientModel::getStatusBarWarnings() const
{
    return QString::fromStdString(GetWarnings("statusbar"));
}

OptionsModel* ClientModel::getOptionsModel() { return optionsModel; }

QString ClientModel::formatFullVersion() const { return QString::fromStdString(FormatFullVersion()); }

QString ClientModel::formatBuildDate() const { return QString::fromStdString(CLIENT_DATE); }

QString ClientModel::clientName() const { return QString::fromStdString(CLIENT_NAME); }

QString ClientModel::formatClientStartupTime() const
{
    return QDateTime::fromTime_t(nClientStartupTime).toString();
}

void ClientModel::setTipBlock(const CBlockIndex& pindex, bool initialSync)
{
    cachedTip.hash          = pindex.GetBlockHash();
    cachedTip.time          = pindex.GetBlockTime();
    cachedTip.height        = pindex.nHeight;
    cachedTip.isInitialSync = initialSync;
}

static void BlockTipChanged(ClientModel* clientmodel, bool initialSync, const CBlockIndex& pIndex)
{
    // lock free async UI updates in case we have a new block tip
    // during initial sync, only update the UI if the last update
    // was > 1000ms (MODEL_UPDATE_DELAY) ago
    int64_t now = 0;
    if (initialSync)
        now = GetTimeMillis();

    // if we are in-sync, update the UI regardless of last update time
    if (!initialSync || now - nLastBlockTipUpdateNotification > MODEL_UPDATE_DELAY) {
        // pass a async signal to the UI thread
        clientmodel->setTipBlock(pIndex, initialSync);
        Q_EMIT clientmodel->numBlocksChanged(pIndex.nHeight, GetNumBlocksOfPeers());
        nLastBlockTipUpdateNotification = now;
    }
}

static void NotifyNumConnectionsChanged(ClientModel* clientmodel, int newNumConnections)
{
    NLog.write(b_sev::trace, "NotifyNumConnectionsChanged {}", newNumConnections);
    QMetaObject::invokeMethod(clientmodel, "updateNumConnections", Qt::QueuedConnection,
                              Q_ARG(int, newNumConnections));
}

void ClientModel::subscribeToCoreSignals()
{
    using namespace boost::placeholders;

    // Connect signals to client
    uiInterface.NotifyBlockTip.connect(boost::bind(BlockTipChanged, this, _1, _2));
    uiInterface.NotifyNumConnectionsChanged.connect(boost::bind(NotifyNumConnectionsChanged, this, _1));
}

void ClientModel::unsubscribeFromCoreSignals()
{
    using namespace boost::placeholders;

    // Disconnect signals from client
    uiInterface.NotifyBlockTip.disconnect(boost::bind(BlockTipChanged, this, _1, _2));
    uiInterface.NotifyNumConnectionsChanged.disconnect(
        boost::bind(NotifyNumConnectionsChanged, this, _1));
}
