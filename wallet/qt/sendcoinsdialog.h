#ifndef SENDCOINSDIALOG_H
#define SENDCOINSDIALOG_H

#include <boost/shared_ptr.hpp>
#include <QDialog>
#include <QList>
#include <QObject>
#include <QSharedPointer>
#include <QString>

#include "coincontroldialog.h"
#include "ntp1/ntp1wallet.h"
#include "ntp1/ntp1transaction.h"
#include "walletmodel.h"

namespace Ui {
class SendCoinsDialog;
}
class SendCoinsEntry;
class SendCoinsRecipient;
class BalancesWorker;

QT_BEGIN_NAMESPACE
class QUrl;
QT_END_NAMESPACE

/* Object for signing a transaction on a Ledger device in a separate thread.
*/
class LedgerSignTxWorker : public QObject
{
    Q_OBJECT

public slots:
    // we use the shared pointer argument to ensure that workerPtr will be deleted after doing the
    // retrieval
    void signTx(
        WalletModel*                       model,
        QList<SendCoinsRecipient>          recipients,
        boost::shared_ptr<NTP1Wallet>      ntp1wallet,
        const RawNTP1MetadataBeforeSend&   ntp1metadata,
        bool                               fSpendDelegated,
        const CCoinControl*                coinControl,
        const std::string&                 strFromAccount,
        QSharedPointer<LedgerSignTxWorker> workerPtr
    );

signals:
    void resultReady(WalletModel::SendCoinsReturn);
};

/** Dialog for sending bitcoins */
class SendCoinsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SendCoinsDialog(QWidget* parent = 0);
    ~SendCoinsDialog();

    void setModel(WalletModel* modelIn);

    /** Set up the tab chain manually, as Qt messes up the tab chain by default in some cases (issue
     * https://bugreports.qt-project.org/browse/QTBUG-10907).
     */
    QWidget* setupTabChain(QWidget* prev);

    void pasteEntry(const SendCoinsRecipient& rv);
    bool handleURI(const QString& uri);

public slots:
    void            clearEntries();
    void            reject();
    void            accept();
    SendCoinsEntry* addEntry();
    void            updateRemoveEnabled();
    void setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance);
    void setUnknownBalance();
    void triggerUpdateBalance();
    void showEditMetadataDialog();

private:
    Ui::SendCoinsDialog*         ui;
    WalletModel*                 model;
    bool                         fNewRecipientAllowed;
    WalletModel::SendCoinsReturn sendStatus;

    bool isAnyNTP1TokenSelected() const;

private slots:
    void on_ledgerCheckBox_toggled(bool checked);
    void on_ledgerAddressBookButton_clicked();
    void on_sendButton_clicked();
    void setSendStatus(WalletModel::SendCoinsReturn status) { sendStatus = status; }
    void removeEntry(SendCoinsEntry* entry);
    void updateDisplayUnit();
    void coinControlFeatureChanged(bool);
    void coinControlButtonClicked();
    void coinControlChangeChecked(int);
    void coinControlChangeEdited(const QString&);
    void coinControlUpdateLabels();
    void coinControlClipboardQuantity();
    void coinControlClipboardAmount();
    void coinControlClipboardFee();
    void coinControlClipboardAfterFee();
    void coinControlClipboardBytes();
    void coinControlClipboardPriority();
    void coinControlClipboardLowOutput();
    void coinControlClipboardChange();
    void updateAllTokenLists();
    void tokenSelectionChanged();
};

#endif // SENDCOINSDIALOG_H
