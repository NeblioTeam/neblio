#ifndef BITCOINGUI_H
#define BITCOINGUI_H

#include "coldstakingpage.h"
#include "ntp1summary.h"
#include "overviewpage.h"
#include <QLinearGradient>
#include <QMainWindow>
#include <QPainter>
#include <QProgressDialog>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QToolBar>

#include <stdint.h>

#include "neblioupdatedialog.h"
#include "neblioupdater.h"

class TransactionTableModel;
class ClientModel;
class WalletModel;
class TransactionView;
class OverviewPage;
class AddressBookPage;
class SendCoinsDialog;
class SignVerifyMessageDialog;
class Notificator;
class RPCConsole;

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QTableView;
class QAbstractItemModel;
class QModelIndex;
class QProgressBar;
class QStackedWidget;
class QUrl;
QT_END_NAMESPACE

/**
  Bitcoin GUI main class. This class represents the main window of the Bitcoin UI. It communicates with
  both the client and wallet models to give the user an up-to-date view of the current core state.
*/
class BitcoinGUI : public QMainWindow
{
    Q_OBJECT

    ClickableLabel*            updaterLabel;
    NeblioUpdater              neblioUpdater;
    NeblioReleaseInfo          latestRelease;
    boost::promise<bool>       updateAvailablePromise;
    boost::unique_future<bool> updateAvailableFuture;
    QTimer*                    updateConcluderTimer;
    int                        updateConcluderTimeout;
    QTimer*                    updateCheckTimer;
    int                        updateCheckTimerTimeout;
    QTimer*                    animationStopperTimer;
    int                        animationStopperTimerTimeout;
    NeblioUpdateDialog*        updateDialog;

    bool isUpdateRunning; // since update check is asynchronous, this is true while checking is running
    // The following are the images that can show up in the updater
    QMovie* updaterSpinnerMovie;
    QMovie* updaterCheckMovie;
    QMovie* updaterUpdateExistsMovie;
    QMovie* updaterErrorMovie;

    void setupUpdateControls();

    QTimer* backupCheckerTimer;
    int     backupCheckerTimerPeriod;
    QTimer* backupBlinkerTimer;
    int     backupBlinkerTimerPeriod;
    bool    backupBlinkerOn;

public:
    explicit BitcoinGUI(QWidget* parent = 0);
    ~BitcoinGUI();

    /** Set the client model.
        The client model represents the part of the core that communicates with the P2P network, and is
       wallet-agnostic.
    */
    void setClientModel(ClientModel* clientModel);
    /** Set the wallet model.
        The wallet model represents a bitcoin wallet, and offers access to the list of transactions,
       address book and sending functionality.
    */
    void setWalletModel(WalletModel* walletModel);

protected:
    void changeEvent(QEvent* e);
    void closeEvent(QCloseEvent* event);
    void dragEnterEvent(QDragEnterEvent* event);
    void dropEvent(QDropEvent* event);

private:
    ClientModel* clientModel;
    WalletModel* walletModel;

    QStackedWidget* centralWidget;

    OverviewPage*            overviewPage;
    NTP1Summary*             ntp1SummaryPage;
    ColdStakingPage*         coldStakingPage;
    QWidget*                 transactionsPage;
    AddressBookPage*         addressBookPage;
    AddressBookPage*         receiveCoinsPage;
    SendCoinsDialog*         sendCoinsPage;
    SignVerifyMessageDialog* signVerifyMessageDialog;

    QLabel*       labelEncryptionIcon;
    QLabel*       labelStakingIcon;
    QLabel*       labelConnectionsIcon;
    QLabel*       labelBlocksIcon;
    QLabel*       labelBackupAlertIcon;
    QLabel*       progressBarLabel;
    QProgressBar* progressBar;

    QProgressDialog* blockchainExporterProg;

    QMenuBar* appMenuBar;
    QAction*  overviewAction;
    QAction*  ntp1tokensAction;
    QAction*  coldStakingAction;
    QAction*  historyAction;
    QAction*  quitAction;
    QAction*  sendCoinsAction;
    QAction*  addressBookAction;
    QAction*  signMessageAction;
    QAction*  verifyMessageAction;
    QAction*  exportBlockchainBootstrapAction;
    QAction*  aboutAction;
    QAction*  receiveCoinsAction;
    QAction*  optionsAction;
    QAction*  toggleHideAction;
    QAction*  exportAction;
    QAction*  encryptWalletAction;
    QAction*  backupWalletAction;
    QAction*  importWalletAction;
    QAction*  changePassphraseAction;
    QAction*  unlockWalletAction;
    QAction*  lockWalletAction;
    QAction*  aboutQtAction;
    QAction*  openRPCConsoleAction;

    QSystemTrayIcon* trayIcon;
    Notificator*     notificator;
    TransactionView* transactionView;
    RPCConsole*      rpcConsole;

    QMovie* syncIconMovie;

    QToolBar* toolbar;

    uint64_t nWeight;

    /** Create the main UI actions. */
    void createActions();
    /** Create the menu bar and sub-menus. */
    void createMenuBar();
    /** Create the toolbars */
    void createToolBars();
    /** Create system tray icon and notification */
    void createTrayIcon();
    /** Create system tray menu (or setup the dock menu) */
    void createTrayIconMenu();

    void paintEvent(QPaintEvent*)
    {
        QPainter painter(this);

        // paint the upper bar
        if (toolbar != NULL) {
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setBrush(QBrush(QColor(220, 220, 220)));
            // painter.drawRect(0, toolbar->pos().y(), this->size().width(), toolbar->height());
            QRect           rect(0, toolbar->pos().y(), this->size().width(), toolbar->height());
            QLinearGradient gradient(rect.topLeft(),
                                     rect.topRight()); // diagonal gradient from top-left to bottom-right
            gradient.setColorAt(0, QColor(11, 223, 212, 128));
            gradient.setColorAt(1, QColor(127, 75, 200, 128));

            painter.fillRect(rect, gradient);
        }

        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QBrush(QColor(41, 31, 58)));

        if (overviewPage == NULL)
            return;

        // This draws the bottom gray bar after calculating its position
        if (overviewAction->isChecked()) {
            painter.drawRect(0,
                             overviewPage->ui->bottom_bar_widget
                                 ->mapTo(overviewPage->ui->bottom_bar_widget->window(), QPoint(0, 0))
                                 .y(),
                             this->size().width(), overviewPage->ui->bottom_bar_widget->height());
        }
        if (ntp1tokensAction->isChecked()) {
            painter.drawRect(0,
                             ntp1SummaryPage->ui->bottom_bar_widget
                                 ->mapTo(ntp1SummaryPage->ui->bottom_bar_widget->window(), QPoint(0, 0))
                                 .y(),
                             this->size().width(), ntp1SummaryPage->ui->bottom_bar_widget->height());
        }
        if (coldStakingAction->isChecked()) {
            painter.drawRect(0,
                             coldStakingPage->ui->bottom_bar_widget
                                 ->mapTo(coldStakingPage->ui->bottom_bar_widget->window(), QPoint(0, 0))
                                 .y(),
                             this->size().width(), coldStakingPage->ui->bottom_bar_widget->height());
        }
    }

public slots:
    /** Set number of connections shown in the UI */
    void setNumConnections(int count);
    /** Set number of blocks shown in the UI */
    void setNumBlocks(int count, int nTotalBlocks);
    /** Set the encryption status as shown in the UI.
       @param[in] status            current encryption status
       @see WalletModel::EncryptionStatus
    */
    void setEncryptionStatus(int status);

    /** Notify the user of an error in the network or transaction handling code. */
    void error(const QString& title, const QString& message, bool modal);
    /** Asks the user whether to pay the transaction fee or to cancel the transaction.
       It is currently not possible to pass a return value to another thread through
       BlockingQueuedConnection, so an indirected pointer is used.
       https://bugreports.qt-project.org/browse/QTBUG-10440

      @param[in] nFeeRequired       the required fee
      @param[out] payFee            true to pay the fee, false to not pay the fee
    */
    void askFee(qint64 nFeeRequired, bool* payFee);
    void handleURI(QString strURI);

private slots:
    /** Switch to overview (home) page */
    void gotoOverviewPage();
    /** Switch to NTP1 tokens summary page */
    void gotoNTP1SummaryPage();
    /** Switch to Coldstaking page */
    void gotoColdStakingPage();
    /** Switch to history (transactions) page */
    void gotoHistoryPage();
    /** Switch to address book page */
    void gotoAddressBookPage();
    /** Switch to receive coins page */
    void gotoReceiveCoinsPage();
    /** Switch to send coins page */
    void gotoSendCoinsPage();

    /** Show Sign/Verify Message dialog and switch to sign message tab */
    void gotoSignMessageTab(QString addr = "");
    /** Show Sign/Verify Message dialog and switch to verify message tab */
    void gotoVerifyMessageTab(QString addr = "");

    void exportBlockchainBootstrap();

    /** Show configuration dialog */
    void optionsClicked();
    /** Show about dialog */
    void aboutClicked();
#ifndef Q_OS_MAC
    /** Handle tray icon clicked */
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
#endif
    /** Show incoming transaction notification for new transactions.

        The new items are those between start and end inclusive, under the given parent item.
    */
    void incomingTransaction(const QModelIndex& parent, int start, int end);
    /** Encrypt the wallet */
    void encryptWallet(bool status);
    /** Backup the wallet */
    void backupWallet();
    /** Import a wallet */
    void importWallet();
    /** Change encrypted wallet passphrase */
    void changePassphrase();
    /** Ask for passphrase to unlock wallet temporarily */
    void unlockWallet();

    void lockWallet();

    /** Show window if hidden, unminimize when minimized, rise when obscured or show if hidden and
     * fToggleHidden is true */
    void showNormalIfMinimized(bool fToggleHidden = false);
    /** simply calls showNormalIfMinimized(true) for use in SLOT() macro */
    void toggleHidden();

    void updateWeight();
    void updateStakingIcon();

    // Stop the animation after playing once
    void updateCheckAnimation_frameChanged(int frameNumber);
    // This function calls the update check asynchronously
    void checkForNeblioUpdates();
    // Called periodically to asynchronously check if the update process is finished
    void finishCheckForNeblioUpdates();

    void stopAnimations();

    void checkWhetherBackupIsMade();
    void startBackupAlertBlinker();
    void stopBackupAlertBlinker();
    void blinkBackupAlertIcon();
};

#endif
