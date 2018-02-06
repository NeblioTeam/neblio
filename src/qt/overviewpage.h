#ifndef OVERVIEWPAGE_H
#define OVERVIEWPAGE_H

#include <QWidget>
#include <QTimer>
#include <ui_overviewpage.h>

#include <boost/thread.hpp>
#include <boost/thread/future.hpp>

#include "neblioupdater.h"

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

namespace Ui {
    class OverviewPage;
}
class WalletModel;
class TxViewDelegate;
class TransactionFilterProxy;

/** Overview ("home") page widget */
class OverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewPage(QWidget *parent = 0);
    ~OverviewPage();

    void setModel(WalletModel *model);
    void showOutOfSyncWarning(bool fShow);

public slots:
    void setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance);

signals:
    void transactionClicked(const QModelIndex &index);

public:
    Ui::OverviewPage *ui;

    NeblioUpdater neblioUpdater;
    NeblioVersion latestVersion;
    boost::promise<bool> updateAvailablePromise;
    boost::unique_future<bool> updateAvailableFuture;
    QTimer* updateConcluderTimer;
    int updateConcluderTimeout;
    QTimer* updateCheckTimer;
    int updateCheckTimerTimeout;
private:
    WalletModel *model;
    qint64 currentBalance;
    qint64 currentStake;
    qint64 currentUnconfirmedBalance;
    qint64 currentImmatureBalance;

    TxViewDelegate *txdelegate;
    TransactionFilterProxy *filter;

    bool isUpdateRunning; // since update check is asynchronous, this is true while checking is running
    //The following are the images that can show up in the updater
    QMovie *bottom_bar_updater_spinner_movie;
    QMovie *bottom_bar_updater_check_movie;
    QMovie *bottom_bar_updater_no_update_movie;
    QMovie *bottom_bar_updater_error_movie;

    void setupUpdateControls();
private slots:
    void updateDisplayUnit();
    void handleTransactionClicked(const QModelIndex &index);

    // Stop the animation after playing once
    void updateCheckAnimation_frameChanged(int frameNumber);

    // This function calls the update check asynchronously
    void checkForNeblioUpdates();
    // Called periodically to asynchronously check if the update process is finished
    void finishCheckForNeblioUpdates();
};

#endif // OVERVIEWPAGE_H
