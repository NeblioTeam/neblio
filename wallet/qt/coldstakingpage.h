#ifndef COLDSTAKING_H
#define COLDSTAKING_H

#include "coldstakingmodel.h"
#include "ui_coldstakingpage.h"
#include <QTimer>
#include <QWidget>

#include <boost/thread.hpp>
#include <boost/thread/future.hpp>

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

class Ui_ColdStaking;

class WalletModel;

class ColdStakingListItemDelegate;

class NewStakeDelegationDialog;

/** ColdStaking page widget */
class ColdStakingPage : public QWidget
{
    Q_OBJECT

    boost::signals2::connection notifyWalletConnection;

public:
    explicit ColdStakingPage(QWidget* parent = 0);
    virtual ~ColdStakingPage();

    void setModel(ColdStakingModel* model);
    void setWalletModel(WalletModel* wModel);

public slots:
    void onTxArrived(const QString& hash);

signals:
    void tokenClicked(const QModelIndex& index);

public:
    Ui_ColdStaking*   ui;
    ColdStakingModel* getTokenListModel() const;

private:
    ColdStakingModel* model;

    ColdStakingListItemDelegate* itemDelegate;

    NewStakeDelegationDialog* newStakeDelegationDialog = nullptr;

    static const QString copyOwnerAddressText;
    static const QString copyStakerAddressText;
    static const QString copyAmountText;
    static const QString enableStakingText;
    static const QString disableStakingText;
    static const QString cantStakeText;

    QMenu*   contextMenu          = Q_NULLPTR;
    QAction* copyOwnerAddrAction  = Q_NULLPTR;
    QAction* copyStakerAddrAction = Q_NULLPTR;
    QAction* copyAmountAction     = Q_NULLPTR;
    QAction* toggleStakingAction  = Q_NULLPTR;

    // caching mechanism for the metadata to avoid accessing the db repeatedly4
    std::unordered_map<uint256, json_spirit::Value> issuanceTxidVsMetadata;

    int64_t lastRefreshTime = 0;

    void tryRefreshData();
    void refreshData();

    void copyField(ColdStakingModel::ColumnIndex column, const QString& columnName);

    void configureToggleStakingAction(const QModelIndex& idx);

private slots:
    void handleTokenClicked(const QModelIndex& index);
    void slot_contextMenuRequested(QPoint pos);
    void slot_copyOwnerAddr();
    void slot_copyStakerAddr();
    void slot_copyAmount();
    void slot_enableStaking(const QModelIndex& idx);
    void slot_disableStaking(const QModelIndex& idx);

    // QWidget interface
protected:
    void keyPressEvent(QKeyEvent* event);
    void setupContextMenu();
};

#endif // COLDSTAKING_H
