#ifndef NTP1LISTELEMENTTOKENDATA_H
#define NTP1LISTELEMENTTOKENDATA_H

#include "qt/ntp1/ntp1tokenlistmodel.h"
#include <QIcon>
#include <QString>

struct NTP1ListElementTokenData
{
    QString name;
    QString tokenId;
    qint64  amount;
    QIcon   icon;
    quint32 divisibility;
    void    fill(int index, boost::shared_ptr<NTP1Wallet> wallet)
    {
        name = NTP1TokenListModel::__getTokenName(index, wallet);
        icon = NTP1TokenListModel::__getTokenIcon(index, wallet);
        if (icon.isNull()) {
            icon = QIcon(":/images/orion");
        }
        tokenId = NTP1TokenListModel::__getTokenId(index, wallet);
        amount  = FromString<qint64>(NTP1TokenListModel::__getTokenBalance(index, wallet).toStdString());
        divisibility = FromString<unsigned>(NTP1TokenListModel::__getTokenDivisibility(index, wallet).toStdString());
    }
};

#endif // NTP1LISTELEMENTTOKENDATA_H
