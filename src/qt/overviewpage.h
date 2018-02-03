#ifndef OVERVIEWPAGE_H
#define OVERVIEWPAGE_H

#include <QWidget>
#include <ui_overviewpage.h>

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

private:
    WalletModel *model;
    qint64 currentBalance;
    qint64 currentStake;
    qint64 currentUnconfirmedBalance;
    qint64 currentImmatureBalance;

    TxViewDelegate *txdelegate;
    TransactionFilterProxy *filter;

    QMovie *bottom_bar_updater_spinner_movie;
    QMovie *bottom_bar_updater_check_movie;

    void setupUpdateAnimations();
private slots:
    void updateDisplayUnit();
    void handleTransactionClicked(const QModelIndex &index);

    // Stop the animation after playing once
    void updateCheckAnimation_frameChanged(int frameNumber);
};

#endif // OVERVIEWPAGE_H
