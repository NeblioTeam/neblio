#ifndef NTP1TOKENLISTMODEL_H
#define NTP1TOKENLISTMODEL_H

#include <QAbstractTableModel>
#include <QTimer>

#include "ntp1/ntp1wallet.h"
#include "wallet.h"
#include "init.h"

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
    boost::recursive_mutex walletUpdateBeginLock;

    QTimer* walletUpdateEnderTimer;

    boost::promise<boost::shared_ptr<NTP1Wallet> > updateWalletPromise;
    boost::unique_future<boost::shared_ptr<NTP1Wallet> > updateWalletFuture;


    static void UpdateWalletBalances(boost::shared_ptr<NTP1Wallet> wallet, boost::promise<boost::shared_ptr<NTP1Wallet> >& promise);

    class NTP1WalletTxUpdater : public WalletNewTxUpdateFunctor
    {
        NTP1TokenListModel* model;
        int currentBlockHeight;
    public:
        NTP1WalletTxUpdater(NTP1TokenListModel* Model) : model(Model), currentBlockHeight(-1)
        {}

        void run(uint256,int currentHeight) Q_DECL_OVERRIDE
        {
            if(currentBlockHeight < 0) {
                setReferenceBlockHeight();
            }
            if(currentHeight <= currentBlockHeight + HEIGHT_OFFSET_TOLERANCE) {
                model->reloadBalances();
            }
        }

        // WalletNewTxUpdateFunctor interface
    public:
        void setReferenceBlockHeight() Q_DECL_OVERRIDE
        {
            currentBlockHeight = nBestHeight;
        }
    };

    boost::shared_ptr<NTP1WalletTxUpdater> ntp1WalletTxUpdater;
    void SetupNTP1WalletTxUpdaterToWallet()
    {
        while(!pwalletMain) {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
        }
        pwalletMain->setFunctorOnTxInsert(ntp1WalletTxUpdater);
    }

public:
    NTP1TokenListModel();
    ~NTP1TokenListModel();
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

    void saveWalletToFile();
    void loadWalletFromFile();
    static const std::string WalletFileName;
signals:
    void signal_walletUpdateRunning(bool running);

private slots:
    void beginWalletUpdate();
    void endWalletUpdate();
};

#endif // NTP1TOKENLISTMODEL_H
