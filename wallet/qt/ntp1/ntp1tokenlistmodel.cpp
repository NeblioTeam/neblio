#include "ntp1tokenlistmodel.h"
#include "boost/thread/future.hpp"
#include <boost/atomic/atomic.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>

#include <QIcon>
#include <QImage>

std::atomic<NTP1TokenListModel*> ntp1TokenListModelInstance{nullptr};

QString NTP1TokenListModel::__getTokenName(int index, boost::shared_ptr<NTP1Wallet> theWallet)
{
    return QString::fromStdString(theWallet->getTokenName(index));
}

QString NTP1TokenListModel::__getTokenId(int index, boost::shared_ptr<NTP1Wallet> theWallet)
{
    return QString::fromStdString(theWallet->getTokenId(index));
}

QString NTP1TokenListModel::__getTokenDescription(int index, boost::shared_ptr<NTP1Wallet> theWallet)
{
    return QString::fromStdString(theWallet->getTokenDescription(index));
}

QString NTP1TokenListModel::__getTokenBalance(int index, boost::shared_ptr<NTP1Wallet> theWallet)
{
    return QString::fromStdString(ToString(theWallet->getTokenBalance(index)));
}

QString NTP1TokenListModel::__getIssuanceTxid(int index, boost::shared_ptr<NTP1Wallet> theWallet)
{
    return QString::fromStdString(theWallet->getTokenIssuanceTxid(index));
}

QIcon NTP1TokenListModel::__getTokenIcon(int index, boost::shared_ptr<NTP1Wallet> theWallet)
{
    const std::string& iconData = theWallet->getTokenIcon(index);
    if (iconData.empty() || NTP1Wallet::IconHasErrorContent(iconData)) {
        return QIcon();
    }
    QImage iconImage;
    iconImage.loadFromData((const uchar*)iconData.c_str(), iconData.size());
    return QIcon(QPixmap::fromImage(iconImage));
}

void NTP1TokenListModel::clearNTP1Wallet()
{
    if (ntp1wallet) {
        beginResetModel();
        ntp1wallet->clear();
        endResetModel();
    }
}

void NTP1TokenListModel::refreshNTP1Wallet()
{
    if (ntp1wallet) {
        beginResetModel();
        ntp1wallet->update();
        endResetModel();
    }
}

void NTP1TokenListModel::UpdateWalletBalances(boost::shared_ptr<NTP1Wallet>                  wallet,
                                              boost::promise<boost::shared_ptr<NTP1Wallet>>& promise)
{
    try {
        boost::atomic_load(&wallet)->update();
        promise.set_value(wallet);
    } catch (...) {
        try {
            promise.set_exception(boost::current_exception());
        } catch (std::exception& ex) {
            printf("Error: Setting promise exception failed for NTP1TokenListModel wallet: %s",
                   ex.what());
        } catch (...) {
            printf(
                "Error: Setting promise exception failed for NTP1TokenListModel wallet (Unknown error)");
        }
    }
}

NTP1TokenListModel::NTP1TokenListModel()
    : ntp1WalletTxUpdater(boost::make_shared<NTP1WalletTxUpdater>(this))
{
    updateWalletAgain.store(false);
    walletUpdateLockFlag.clear();
    ntp1wallet             = boost::make_shared<NTP1Wallet>();
    walletLocked           = false;
    walletUpdateRunning    = false;
    walletUpdateEnderTimer = new QTimer(this);
    loadWalletFromFile();
    connect(walletUpdateEnderTimer, &QTimer::timeout, this, &NTP1TokenListModel::endWalletUpdate);
    walletUpdateEnderTimer->start(1000);
    reloadBalances();
    boost::thread t(&NTP1TokenListModel::SetupNTP1WalletTxUpdaterToWallet, this);
    t.detach();
    ntp1TokenListModelInstance.store(this);
}

NTP1TokenListModel::~NTP1TokenListModel()
{
    ntp1TokenListModelInstance.store(nullptr);
    ntp1WalletTxUpdater.reset();
}

void NTP1TokenListModel::reloadBalances()
{
    // the following mechanism is to ensure that if this function is called too often, it won't
    // indefinitely block, but will simply queue requests through the boolean `updateWalletAgain`

    bool dummy       = false;
    auto restoreFunc = [this](bool*) { walletUpdateLockFlag.clear(); };
    std::unique_ptr<bool, decltype(restoreFunc)> lg(&dummy, restoreFunc); // RAII, acts like a lock_guard
    do {
        // only one thread can do this, if another thread tries, it'll just schedule another update
        if (!walletUpdateLockFlag.test_and_set(boost::memory_order_seq_cst)) {
            beginWalletUpdate();
        } else {
            // if locking failed, that means something else triggered the scan, so we just schedule an
            // update and exit when this scan is done
            updateWalletAgain.store(true, boost::memory_order_seq_cst);
            break;
        }
        // continue if a rescan is scheduled
    } while (updateWalletAgain.exchange(false, boost::memory_order_seq_cst));
}

void NTP1TokenListModel::beginWalletUpdate()
{
    if (!walletUpdateRunning) {
        walletUpdateRunning = true;
        emit                          signal_walletUpdateRunning(true);
        boost::shared_ptr<NTP1Wallet> wallet = boost::make_shared<NTP1Wallet>(*ntp1wallet);
        updateWalletPromise                  = boost::promise<boost::shared_ptr<NTP1Wallet>>();
        {
            boost::lock_guard<boost::mutex> lg(walletFutureCheckMutex);
            updateWalletFuture = updateWalletPromise.get_future();
        }
        boost::thread t(boost::bind(&NTP1TokenListModel::UpdateWalletBalances, wallet,
                                    boost::ref(updateWalletPromise)));
        t.detach();
    }
}

void NTP1TokenListModel::endWalletUpdate()
{
    bool is_ready = false;
    {
        boost::lock_guard<boost::mutex> lg(walletFutureCheckMutex);
        is_ready = updateWalletFuture.is_ready();
    }
    if (walletUpdateRunning && is_ready) {
        try {
            boost::shared_ptr<NTP1Wallet> wallet = updateWalletFuture.get();
            // the nullptr check is done after having a nullptr once happen.
            // Although this should never happen, having it doesn't hurt and is safer
            if ((wallet.get() != nullptr) && !(*wallet == *ntp1wallet)) {
                beginResetModel();
                boost::atomic_store(&ntp1wallet, wallet);
                saveWalletToFile();
                endResetModel();
            }
        } catch (std::exception& ex) {
            printf("Error while updating NTP1 balances: %s", ex.what());
        }
        walletUpdateRunning = false;
        emit signal_walletUpdateRunning(false);
    }
}

int NTP1TokenListModel::rowCount(const QModelIndex& /*parent*/) const
{
    return ntp1wallet->getNumberOfTokens();
}

int NTP1TokenListModel::columnCount(const QModelIndex& /*parent*/) const { return 3; }

QVariant NTP1TokenListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (index.row() >= static_cast<int>(ntp1wallet->getNumberOfTokens()))
        return QVariant();

    if (role == NTP1TokenListModel::AmountRole) {
        return __getTokenBalance(index.row(), ntp1wallet);
    }
    if (role == NTP1TokenListModel::TokenDescriptionRole) {
        return __getTokenDescription(index.row(), ntp1wallet);
    }
    if (role == NTP1TokenListModel::TokenIdRole) {
        return __getTokenId(index.row(), ntp1wallet);
    }
    if (role == NTP1TokenListModel::TokenNameRole) {
        return __getTokenName(index.row(), ntp1wallet);
    }
    if (role == NTP1TokenListModel::IssuanceTxidRole) {
        return __getIssuanceTxid(index.row(), ntp1wallet);
    }
    if (role == Qt::DecorationRole) {
        return QVariant(__getTokenIcon(index.row(), ntp1wallet));
    }
    return QVariant();
}

void NTP1TokenListModel::saveWalletToFile()
{
    try {
        if (!ntp1wallet) {
            throw std::runtime_error("Error: NTP1 wallet is a null pointer!");
        }

        // The temp file here ensures that the target file is not tampered with before a successful
        // writing happens This is helpful to avoid corrupt files in cases such as diskspace issues
        srand(time(NULL));
        boost::filesystem::path tempFile = GetDataDir() / NTP1WalletCacheFileName;
        tempFile.replace_extension(".json." + ToString(rand()));
        boost::filesystem::path permFile = GetDataDir() / NTP1WalletCacheFileName;
        ntp1wallet->exportToFile(tempFile);
        if (boost::filesystem::exists(permFile))
            boost::filesystem::remove(permFile);
        boost::filesystem::rename(tempFile, permFile);
    } catch (std::exception& ex) {
        printf("Failed at exporting wallet data. Error says %s", ex.what());
    }
}

void NTP1TokenListModel::loadWalletFromFile()
{
    try {
        if (!ntp1wallet) {
            throw std::runtime_error("Error: NTP1 wallet is a null pointer!");
        }
        boost::filesystem::path file = GetDataDir() / NTP1WalletCacheFileName;
        if (boost::filesystem::exists(file)) {
            ntp1wallet->importFromFile(file);
        } else {
            printf("NTP1 wallet not found. One will be created soon.");
        }
    } catch (std::exception& ex) {
        ntp1wallet->clear();
        printf("Failed at exporting wallet data. Error says %s", ex.what());
    }
}

boost::shared_ptr<NTP1Wallet> NTP1TokenListModel::getCurrentWallet() const
{
    return boost::atomic_load(&ntp1wallet);
}
