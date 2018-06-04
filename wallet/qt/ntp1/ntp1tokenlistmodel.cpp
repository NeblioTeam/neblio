#include "ntp1tokenlistmodel.h"
#include "boost/thread/future.hpp"
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

#include <QImage>
#include <QIcon>

const std::string NTP1TokenListModel::WalletFileName = "NTP1Wallet.json";

QString NTP1TokenListModel::__getTokenName(int index) const
{
    return QString::fromStdString(ntp1wallet->getTokenName(index));
}

QString NTP1TokenListModel::__getTokenDescription(int index) const
{
    return QString::fromStdString(ntp1wallet->getTokenDescription(index));
}

QString NTP1TokenListModel::__getTokenBalance(int index) const
{
    return QString::number(ntp1wallet->getTokenBalance(index));
}

QIcon NTP1TokenListModel::__getTokenIcon(int index) const
{
    const std::string& iconData = ntp1wallet->getTokenIcon(index);
    if(iconData.empty() || NTP1Wallet::IconHasErrorContent(iconData)) {
        return QIcon();
    }
    QImage iconImage;
    iconImage.loadFromData((const uchar*)iconData.c_str(),iconData.size());
    return QIcon(QPixmap::fromImage(iconImage));
}

void NTP1TokenListModel::UpdateWalletBalances(boost::shared_ptr<NTP1Wallet> wallet, boost::promise<boost::shared_ptr<NTP1Wallet> > &promise)
{
    try {
        wallet->update();
        promise.set_value(wallet);
    } catch(...) {
        promise.set_exception(boost::current_exception());
    }
}

NTP1TokenListModel::NTP1TokenListModel() : ntp1WalletTxUpdater(boost::make_shared<NTP1WalletTxUpdater>(this))
{
    ntp1wallet = boost::make_shared<NTP1Wallet>();
    walletLocked = false;
    walletUpdateRunning = false;
    walletUpdateEnderTimer    = new QTimer(this);
    loadWalletFromFile();
    connect(walletUpdateEnderTimer, &QTimer::timeout,
            this, &NTP1TokenListModel::endWalletUpdate);
    walletUpdateEnderTimer->start(1000);
    reloadBalances();
    boost::thread t(&NTP1TokenListModel::SetupNTP1WalletTxUpdaterToWallet, this);
    t.detach();
}

NTP1TokenListModel::~NTP1TokenListModel()
{
    ntp1WalletTxUpdater.reset();
}

void NTP1TokenListModel::reloadBalances()
{
    boost::lock_guard<boost::recursive_mutex> lg(walletUpdateBeginLock);
    beginWalletUpdate();
}

void NTP1TokenListModel::beginWalletUpdate()
{
    if(!walletUpdateRunning) {
        emit signal_walletUpdateRunning(true);
        walletUpdateRunning = true;
        boost::shared_ptr<NTP1Wallet> wallet = boost::make_shared<NTP1Wallet>(*ntp1wallet);
        updateWalletPromise = boost::promise<boost::shared_ptr<NTP1Wallet> >();
        updateWalletFuture = updateWalletPromise.get_future();
        boost::thread t(boost::bind(&NTP1TokenListModel::UpdateWalletBalances, wallet, boost::ref(updateWalletPromise)));
        t.detach();
    }
}

void NTP1TokenListModel::endWalletUpdate()
{
    if(walletUpdateRunning && updateWalletFuture.is_ready()) {
        try {
            boost::shared_ptr<NTP1Wallet> wallet = updateWalletFuture.get();
            if(!(*wallet == *ntp1wallet)) {
                beginResetModel();
                boost::atomic_store(&ntp1wallet, wallet);
                saveWalletToFile();
                endResetModel();
            }
        } catch(std::exception& ex) {
            printf("Error while updating NTP1 balances: %s", ex.what());
        }
        walletUpdateRunning = false;
        emit signal_walletUpdateRunning(false);
    }
}

int NTP1TokenListModel::rowCount(const QModelIndex &parent) const
{
    return ntp1wallet->getNumberOfTokens();
}

int NTP1TokenListModel::columnCount(const QModelIndex &parent) const
{
    return 3;
}

QVariant NTP1TokenListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if(index.row() >= static_cast<int>(ntp1wallet->getNumberOfTokens()))
        return QVariant();

    if(role == NTP1TokenListModel::AmountRole) {
        return __getTokenBalance(index.row());
    }
    if(role == NTP1TokenListModel::TokenDescriptionRole) {
        return __getTokenDescription(index.row());
    }
    if(role == NTP1TokenListModel::TokenNameRole) {
        return __getTokenName(index.row());
    }
    if(role == Qt::DecorationRole) {
        return QVariant(__getTokenIcon(index.row()));
    }
    return QVariant();
}

void NTP1TokenListModel::saveWalletToFile()
{
    try {
        if(!ntp1wallet) {
            throw std::runtime_error("Error: NTP1 wallet is a null pointer!");
        }

        // The temp file here ensures that the target file is not tampered with before a successful writing happens
        // This is helpful to avoid corrupt files in cases such as diskspace issues
        srand(time(NULL));
        boost::filesystem::path tempFile = GetDataDir() / WalletFileName;
        tempFile.replace_extension(".json." + ToString(rand()));
        boost::filesystem::path permFile = GetDataDir() / WalletFileName;
        ntp1wallet->exportToFile(tempFile);
        if(boost::filesystem::exists(permFile)) boost::filesystem::remove(permFile);
        boost::filesystem::rename(tempFile, permFile);
    } catch (std::exception& ex) {
        printf("Failed at exporting wallet data. Error says %s", ex.what());
    }
}

void NTP1TokenListModel::loadWalletFromFile()
{
    try {
        if(!ntp1wallet) {
            throw std::runtime_error("Error: NTP1 wallet is a null pointer!");
        }
        boost::filesystem::path file = GetDataDir() / WalletFileName;
        if(boost::filesystem::exists(file)) {
            ntp1wallet->importFromFile(file);
        } else {
            printf("NTP1 wallet not found. One will be created soon.");
        }
    } catch (std::exception& ex) {
        ntp1wallet->clear();
        printf("Failed at exporting wallet data. Error says %s", ex.what());
    }
}
