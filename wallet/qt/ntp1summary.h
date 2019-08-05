#ifndef NTP1SUMMARY_H
#define NTP1SUMMARY_H

#include "ntp1/ntp1tokenlistfilterproxy.h"
#include "ntp1/ntp1tokenlistitemdelegate.h"
#include "ntp1/ntp1tokenlistmodel.h"
#include "ui_ntp1summary.h"
#include "json/NTP1MetadataViewer.h"
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

    // since loading the list of NTP1 tokens is async, this is will change accordingly
    bool    isNTP1TokensLoadRunning = false;
    QTimer* ntp1LoaderConcluderTimer;
    int     ntp1LoaderConcluderTimerTimeout = 100;

    boost::promise<std::unordered_set<std::string>>       alreadyIssuedNTP1SymbolsPromise;
    boost::unique_future<std::unordered_set<std::string>> alreadyIssuedNTP1SymbolsFuture;

    static void GetAlreadyIssuedNTP1Tokens(boost::promise<std::unordered_set<std::string>>& promise);

public:
    explicit NTP1Summary(QWidget* parent = 0);
    ~NTP1Summary();

    void setModel(NTP1TokenListModel* model);
    void showOutOfSyncWarning(bool fShow);

public slots:

signals:
    void tokenClicked(const QModelIndex& index);

public:
    Ui_NTP1Summary*     ui;
    NTP1TokenListModel* getTokenListModel() const;

private:
    static const QString copyTokenIdText;
    static const QString copyTokenSymbolText;
    static const QString copyTokenNameText;
    static const QString copyIssuanceTxid;
    static const QString viewInBlockExplorerText;
    static const QString viewIssuanceMetadataText;

    qint64                    currentBalance;
    qint64                    currentStake;
    qint64                    currentUnconfirmedBalance;
    qint64                    currentImmatureBalance;
    NTP1TokenListModel*       model;
    NTP1TokenListFilterProxy* filter;

    NTP1TokenListItemDelegate* tokenDelegate;

    QMenu*   contextMenu               = Q_NULLPTR;
    QAction* copyTokenIdAction         = Q_NULLPTR;
    QAction* copyTokenSymbolAction     = Q_NULLPTR;
    QAction* copyTokenNameAction       = Q_NULLPTR;
    QAction* copyIssuanceTxidAction    = Q_NULLPTR;
    QAction* viewInBlockExplorerAction = Q_NULLPTR;
    QAction* showMetadataAction        = Q_NULLPTR;

    NTP1MetadataViewer* metadataViewer;

    // caching mechanism for the metadata to avoid accessing the db repeatedly4
    std::unordered_map<uint256, json_spirit::Value> issuanceTxidVsMetadata;

private slots:
    void handleTokenClicked(const QModelIndex& index);
    void slot_contextMenuRequested(QPoint pos);
    void slot_copyTokenIdAction();
    void slot_copyTokenSymbolAction();
    void slot_copyTokenNameAction();
    void slot_copyIssuanceTxidAction();
    void slot_visitInBlockExplorerAction();
    void slot_showMetadataAction();
    void slot_showIssueNewTokenDialog();
    void slot_concludeLoadNTP1Tokens();

    // QWidget interface
protected:
    void keyPressEvent(QKeyEvent* event);
    void setupContextMenu();
};

#endif // NTP1SUMMARY_H
