#ifndef NTP1TOKENLISTMODEL_H
#define NTP1TOKENLISTMODEL_H

#include <QAbstractTableModel>
#include <QTimer>
#include <atomic>

#include "init.h"
#include "ntp1/ntp1wallet.h"
#include "wallet.h"

class NTP1TokenListModel : public QAbstractTableModel
{
    Q_OBJECT

    boost::shared_ptr<NTP1Wallet> ntp1wallet;
    bool                          walletLocked;
    boost::atomic_bool            walletUpdateRunning;
    boost::atomic_flag            walletUpdateLockFlag = BOOST_ATOMIC_FLAG_INIT;
    boost::atomic_bool            updateWalletAgain;
    // this mutex should not be necessary, but future<shared_ptr> is indicating an issue with
    // thread-sanitizer
    boost::mutex walletFutureCheckMutex;

    QTimer* walletUpdateEnderTimer;

    boost::promise<boost::shared_ptr<NTP1Wallet>>       updateWalletPromise;
    boost::unique_future<boost::shared_ptr<NTP1Wallet>> updateWalletFuture;

    static void UpdateWalletBalances(boost::shared_ptr<NTP1Wallet>                  wallet,
                                     boost::promise<boost::shared_ptr<NTP1Wallet>>& promise);

    class NTP1WalletTxUpdater : public WalletNewTxUpdateFunctor
    {
        NTP1TokenListModel* model;
        int                 currentBlockHeight;

    public:
        NTP1WalletTxUpdater(NTP1TokenListModel* Model) : model(Model), currentBlockHeight(-1) {}

        void run(uint256, int currentHeight) Q_DECL_OVERRIDE
        {
            if (currentBlockHeight < 0) {
                setReferenceBlockHeight();
            }
            if (currentHeight <= currentBlockHeight + HEIGHT_OFFSET_TOLERANCE) {
                model->reloadBalances();
            }
        }

        virtual ~NTP1WalletTxUpdater() {}

        // WalletNewTxUpdateFunctor interface
    public:
        void setReferenceBlockHeight() Q_DECL_OVERRIDE { currentBlockHeight = nBestHeight; }
    };

    boost::shared_ptr<NTP1WalletTxUpdater> ntp1WalletTxUpdater;
    void                                   SetupNTP1WalletTxUpdaterToWallet()
    {
        while (!std::atomic_load(&pwalletMain).get()) {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
        }
        std::atomic_load(&pwalletMain)->setFunctorOnTxInsert(ntp1WalletTxUpdater);
    }

public:
    static QString __getTokenName(int index, boost::shared_ptr<NTP1Wallet> theWallet);
    static QString __getTokenId(int index, boost::shared_ptr<NTP1Wallet> theWallet);
    static QString __getTokenDescription(int index, boost::shared_ptr<NTP1Wallet> theWallet);
    static QString __getTokenBalance(int index, boost::shared_ptr<NTP1Wallet> theWallet);
    static QString __getIssuanceTxid(int index, boost::shared_ptr<NTP1Wallet> theWallet);
    static QIcon   __getTokenIcon(int index, boost::shared_ptr<NTP1Wallet> theWallet);

    void clearNTP1Wallet();
    void refreshNTP1Wallet();

    NTP1TokenListModel();
    virtual ~NTP1TokenListModel();
    void reloadBalances();

    int      rowCount(const QModelIndex& parent) const Q_DECL_OVERRIDE;
    int      columnCount(const QModelIndex& parent) const Q_DECL_OVERRIDE;
    QVariant data(const QModelIndex& index, int role) const Q_DECL_OVERRIDE;

    /** Roles to get specific information from a token row.
        These are independent of column.
    */
    enum RoleIndex
    {
        TokenNameRole = Qt::UserRole,
        TokenDescriptionRole,
        TokenIdRole,
        AmountRole,
        IssuanceTxidRole
    };

    void                          saveWalletToFile();
    void                          loadWalletFromFile();
    boost::shared_ptr<NTP1Wallet> getCurrentWallet() const;
signals:
    void signal_walletUpdateRunning(bool running);

private slots:
    void beginWalletUpdate();
    void endWalletUpdate();
};

extern std::atomic<NTP1TokenListModel*> ntp1TokenListModelInstance;

#endif // NTP1TOKENLISTMODEL_H
