#ifndef NTP1SUMMARY_H
#define NTP1SUMMARY_H

#include "ntp1/ntp1tokenlistfilterproxy.h"
#include "ntp1/ntp1tokenlistitemdelegate.h"
#include "ntp1/ntp1tokenlistmodel.h"
#include "ui_ntp1summary.h"
#include <QTimer>
#include <QWidget>

#include <boost/thread.hpp>
#include <boost/thread/future.hpp>

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

class Ui_NTP1Summary;

class WalletModel;

/** NTP1Summary page widget */
class NTP1Summary : public QWidget
{
    Q_OBJECT

public:
    explicit NTP1Summary(QWidget* parent = 0);
    ~NTP1Summary();

    void setModel(NTP1TokenListModel* model);
    void showOutOfSyncWarning(bool fShow);

public slots:
    void setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance);

signals:
    void tokenClicked(const QModelIndex& index);

public:
    Ui_NTP1Summary* ui;

private:
    qint64                    currentBalance;
    qint64                    currentStake;
    qint64                    currentUnconfirmedBalance;
    qint64                    currentImmatureBalance;
    NTP1TokenListModel*       model;
    NTP1TokenListFilterProxy* filter;

    NTP1TokenListItemDelegate* tokenDelegate;

    QMenu*   contextMenu      = Q_NULLPTR;
    QAction* sendTokensAction = Q_NULLPTR;

private slots:
    void handleTokenClicked(const QModelIndex& index);
    void slot_contextMenuRequested(QPoint pos);

    // QWidget interface
protected:
    void keyPressEvent(QKeyEvent* event);
    void setupContextMenu();
};

#endif // NTP1SUMMARY_H
