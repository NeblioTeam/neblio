#include "ntp1tokenlistmodel.h"
#include "boost/thread/future.hpp"
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>

#include <QImage>
#include <QIcon>

QString NTP1TokenListModel::__getTokenName(int index) const
{
    return QString::fromStdString(ntp1wallet.getTokenName(index));
}

QString NTP1TokenListModel::__getTokenDescription(int index) const
{
    return QString::fromStdString(ntp1wallet.getTokenDescription(index));
}

QString NTP1TokenListModel::__getTokenBalance(int index) const
{
    return QString::number(ntp1wallet.getTokenBalance(index));
}

QIcon NTP1TokenListModel::__getTokenIcon(int index) const
{
    const std::string& iconData = ntp1wallet.getTokenIcon(index);
    if(iconData.empty()) {
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

NTP1TokenListModel::NTP1TokenListModel()
{
    walletLocked = false;
    walletUpdateRunning = false;
    walletUpdateBeginnerTimer = new QTimer(this);
    walletUpdateEnderTimer    = new QTimer(this);
    connect(walletUpdateBeginnerTimer, &QTimer::timeout,
            this, &NTP1TokenListModel::beginWalletUpdate);
    connect(walletUpdateEnderTimer, &QTimer::timeout,
            this, &NTP1TokenListModel::endWalletUpdate);
    walletUpdateBeginnerTimer->start(10e3);
    walletUpdateEnderTimer->start(500);
}

void NTP1TokenListModel::reloadBalances()
{
    beginWalletUpdate();
}

void NTP1TokenListModel::beginWalletUpdate()
{
    if(!walletUpdateRunning) {
        emit signal_walletUpdateRunning(true);
        walletUpdateRunning = true;
        boost::shared_ptr<NTP1Wallet> wallet = boost::make_shared<NTP1Wallet>(ntp1wallet);
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
            if(!(*wallet == ntp1wallet)) {
                beginResetModel();
                ntp1wallet = *wallet;
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
    return ntp1wallet.getNumberOfTokens();
}

int NTP1TokenListModel::columnCount(const QModelIndex &parent) const
{
    return 3;
}

QVariant NTP1TokenListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if(index.row() >= static_cast<int>(ntp1wallet.getNumberOfTokens()))
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
