#ifndef NTP1TOKENLISTMODEL_H
#define NTP1TOKENLISTMODEL_H

#include <QAbstractTableModel>
#include <QTimer>

#include "ntp1/ntp1wallet.h"

class NTP1TokenListModel : public QAbstractTableModel
{
    Q_OBJECT

    boost::shared_ptr<NTP1Wallet> ntp1wallet;
    QString __getTokenName(int index) const;
    QString __getTokenDescription(int index) const;
    QString __getTokenBalance(int index) const;
    QIcon __getTokenIcon(int index) const;
    bool walletLocked;
    bool walletUpdateRunning;

    QTimer* walletUpdateBeginnerTimer;
    QTimer* walletUpdateEnderTimer;

    boost::promise<boost::shared_ptr<NTP1Wallet> > updateWalletPromise;
    boost::unique_future<boost::shared_ptr<NTP1Wallet> > updateWalletFuture;


    static void UpdateWalletBalances(boost::shared_ptr<NTP1Wallet> wallet, boost::promise<boost::shared_ptr<NTP1Wallet> >& promise);

public:
    NTP1TokenListModel();
    void reloadBalances();

    int rowCount(const QModelIndex &parent) const Q_DECL_OVERRIDE;
    int columnCount(const QModelIndex &parent) const Q_DECL_OVERRIDE;
    QVariant data(const QModelIndex &index, int role) const Q_DECL_OVERRIDE;

    /** Roles to get specific information from a token row.
        These are independent of column.
    */
    enum RoleIndex {
        TokenNameRole = Qt::UserRole,
        TokenDescriptionRole,
        AmountRole
    };
signals:
    void signal_walletUpdateRunning(bool running);

private slots:
    void beginWalletUpdate();
    void endWalletUpdate();
};

#endif // NTP1TOKENLISTMODEL_H
