/*
 * Qt4 bitcoin GUI.
 *
 * W.J. van der Laan 2011-2012
 * The Bitcoin Developers 2011-2012
 */
#include "bitcoingui.h"
#include "NetworkForks.h"
#include "aboutdialog.h"
#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "askpassphrasedialog.h"
#include "bitcoinunits.h"
#include "clientmodel.h"
#include "editaddressdialog.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "notificator.h"
#include "optionsdialog.h"
#include "main.h"
#include "optionsmodel.h"
#include "overviewpage.h"
#include "rpcconsole.h"
#include "sendcoinsdialog.h"
#include "signverifymessagedialog.h"
#include "transactiondescdialog.h"
#include "transactiontablemodel.h"
#include "transactionview.h"
#include "wallet.h"
#include "walletmodel.h"

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include <QApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QMovie>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

#include <boost/atomic.hpp>
#include <iostream>
#include <memory>

extern std::shared_ptr<CWallet> pwalletMain;
extern boost::atomic<int64_t>   nLastCoinStakeSearchInterval;
double                          GetPoSKernelPS();

BitcoinGUI::BitcoinGUI(QWidget* parent)
    : QMainWindow(parent), clientModel(0), walletModel(0), encryptWalletAction(0),
      changePassphraseAction(0), unlockWalletAction(0), lockWalletAction(0), aboutQtAction(0),
      trayIcon(0), notificator(0), rpcConsole(0), nWeight(0)
{
    setWindowTitle(tr("neblio") + " - " + tr("Wallet"));
    qApp->setStyleSheet("QMainWindow { background-color: white;border:none;font-family:'Open "
                        "Sans,sans-serif'; } QStatusBar::item { border: 0px;} QDialog { "
                        "background-color: white;border:none;font-family:'Open "
                        "Sans,sans-serif'; } QScrollArea { "
                        "background-color: white;border:none;font-family:'Open "
                        "Sans,sans-serif'; }");
#ifndef Q_OS_MAC
    qApp->setWindowIcon(QIcon(":icons/bitcoin"));
    setWindowIcon(QIcon(":icons/bitcoin"));
#else
    setUnifiedTitleAndToolBarOnMac(true);
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif

    toolbar = NULL;

    // initialization for safety
    overviewPage = NULL;

    // Accept D&D of URIs
    setAcceptDrops(true);

    // Create actions for the toolbar, menu bar and tray/dock icon
    createActions();

    // Create application menu bar
    createMenuBar();

    // Create the toolbars
    createToolBars();

    // Create system tray icon and notification
    createTrayIcon();

    blockchainExporterProg = new QProgressDialog(this);
    blockchainExporterProg->close();
    blockchainExporterProg->setWindowTitle("Blockchain export progress");

    // Create tabs
    overviewPage    = new OverviewPage();
    ntp1SummaryPage = new NTP1Summary();

    transactionsPage  = new QWidget(this);
    QVBoxLayout* vbox = new QVBoxLayout();
    transactionView   = new TransactionView(this);
    vbox->addWidget(transactionView);
    transactionsPage->setLayout(vbox);

    addressBookPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::SendingTab);

    receiveCoinsPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::ReceivingTab);

    sendCoinsPage = new SendCoinsDialog(this);

    signVerifyMessageDialog = new SignVerifyMessageDialog(this);

    centralWidget = new QStackedWidget(this);
    centralWidget->addWidget(overviewPage);
    centralWidget->addWidget(ntp1SummaryPage);
    centralWidget->addWidget(transactionsPage);
    centralWidget->addWidget(addressBookPage);
    centralWidget->addWidget(receiveCoinsPage);
    centralWidget->addWidget(sendCoinsPage);
    setCentralWidget(centralWidget);

    // updater stuff
    isUpdateRunning              = false;
    updateConcluderTimer         = new QTimer(this);
    updateConcluderTimeout       = 3000;
    updateCheckTimer             = new QTimer(this);
    updateCheckTimerTimeout      = 15 * 60 * 1000; // check for updates every 15 minutes
    animationStopperTimer        = new QTimer(this);
    animationStopperTimerTimeout = 2 * 60 * 1000; // stop animations in 2 minutes
    setupUpdateControls();
    updateCheckTimer->start(updateCheckTimerTimeout);
    checkForNeblioUpdates();

    // backup alert timers
    backupCheckerTimer       = new QTimer(this);
    backupCheckerTimerPeriod = 30000;
    backupCheckerTimer->start(backupCheckerTimerPeriod);
    backupBlinkerTimer       = new QTimer(this);
    backupBlinkerTimerPeriod = 500;

    // Status bar notification icons
    QFrame* frameBlocks = new QFrame();
    frameBlocks->setContentsMargins(0, 0, 0, 0);
    frameBlocks->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    QHBoxLayout* frameBlocksLayout = new QHBoxLayout(frameBlocks);
    frameBlocksLayout->setContentsMargins(3, 0, 3, 0);
    frameBlocksLayout->setSpacing(3);
    labelEncryptionIcon  = new QLabel();
    labelStakingIcon     = new QLabel();
    labelConnectionsIcon = new QLabel();
    labelBlocksIcon      = new QLabel();
    labelBackupAlertIcon = new QLabel();
    labelBackupAlertIcon->setPixmap(
        QIcon(":/images/no-backup-made").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
    labelBackupAlertIcon->setToolTip(
        "You have not made a backup of your wallet since it was last changed");
    stopBackupAlertBlinker();
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelBackupAlertIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelEncryptionIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelStakingIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelConnectionsIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelBlocksIcon);
    frameBlocksLayout->addStretch();

    // backup alert connections
    connect(backupCheckerTimer, &QTimer::timeout, this, &BitcoinGUI::checkWhetherBackupIsMade);
    connect(backupBlinkerTimer, &QTimer::timeout, this, &BitcoinGUI::blinkBackupAlertIcon);

    if (GetBoolArg("-staking", true)) {
        QTimer* timerStakingIcon = new QTimer(labelStakingIcon);
        connect(timerStakingIcon, SIGNAL(timeout()), this, SLOT(updateStakingIcon()));
        timerStakingIcon->start(30 * 1000);
        updateStakingIcon();
    }

    // Progress bar and label for blocks download
    progressBarLabel = new QLabel();
    progressBarLabel->setVisible(false);
    progressBar = new QProgressBar();
    progressBar->setAlignment(Qt::AlignCenter);
    progressBar->setVisible(false);

    // Override style sheet for progress bar for styles that have a segmented progress bar,
    // as they make the text unreadable (workaround for issue #1071)
    // See https://qt-project.org/doc/qt-4.8/gallery.html
    QString curStyle = qApp->style()->metaObject()->className();

    progressBar->setStyleSheet("QProgressBar { background-color: #e8e8e8; border: 1px solid grey; "
                                   "border-radius: 2px; padding: 1px; text-align: center; } "
                                   "QProgressBar::chunk { background-color: #0bdbd0; border-radius: 2px; "
                                   "margin: 0px; }");

    statusBar()->addWidget(updaterLabel);
    updaterLabel->setAlignment(Qt::AlignCenter);
    statusBar()->addWidget(progressBarLabel);
    statusBar()->addWidget(progressBar);
    statusBar()->addPermanentWidget(frameBlocks);

    statusBar()->setStyleSheet("QStatusBar { background-color: white; border: none; }");

    syncIconMovie = new QMovie(":images/update-spinner", QByteArray(), this);

    // Clicking on a transaction on the overview page simply sends you to transaction history page
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), this, SLOT(gotoHistoryPage()));
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), transactionView,
            SLOT(focusTransaction(QModelIndex)));

    // Double-clicking on a transaction on the transaction history page shows details
    connect(transactionView, SIGNAL(doubleClicked(QModelIndex)), transactionView, SLOT(showDetails()));

    rpcConsole = new RPCConsole(this);
    connect(openRPCConsoleAction, SIGNAL(triggered()), rpcConsole, SLOT(show()));

    // Clicking on "Verify Message" in the address book sends you to the verify message tab
    connect(addressBookPage, SIGNAL(verifyMessage(QString)), this, SLOT(gotoVerifyMessageTab(QString)));
    // Clicking on "Sign Message" in the receive coins page sends you to the sign message tab
    connect(receiveCoinsPage, SIGNAL(signMessage(QString)), this, SLOT(gotoSignMessageTab(QString)));

    gotoOverviewPage();
    QSize updaterIconSize(labelEncryptionIcon->height(), labelEncryptionIcon->height());
    updaterCheckMovie->setScaledSize(updaterIconSize);
    updaterUpdateExistsMovie->setScaledSize(updaterIconSize);
    updaterErrorMovie->setScaledSize(updaterIconSize);
    updaterSpinnerMovie->setScaledSize(updaterIconSize);
    syncIconMovie->setScaledSize(updaterIconSize);

    checkWhetherBackupIsMade();
}

BitcoinGUI::~BitcoinGUI()
{
    if (trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
#ifdef Q_OS_MAC
    delete appMenuBar;
    MacDockIconHandler::cleanup();
#endif
}

void BitcoinGUI::createActions()
{
    QActionGroup* tabGroup = new QActionGroup(this);

    overviewAction = new QAction(QIcon(":/icons/overview"), tr("&Overview"), this);
    overviewAction->setToolTip(tr("Show general overview of wallet"));
    overviewAction->setCheckable(true);
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
    tabGroup->addAction(overviewAction);

    ntp1tokensAction = new QAction(QIcon(":/icons/ntp1summary"), tr("&NTP1 summary"), this);
    ntp1tokensAction->setToolTip(tr("Show general overview of NTP1 tokens"));
    ntp1tokensAction->setCheckable(true);
    ntp1tokensAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
    tabGroup->addAction(ntp1tokensAction);

    sendCoinsAction = new QAction(QIcon(":/icons/send"), tr("&Send tokens"), this);
    sendCoinsAction->setToolTip(tr("Send tokens to a neblio address"));
    sendCoinsAction->setCheckable(true);
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
    tabGroup->addAction(sendCoinsAction);

    receiveCoinsAction = new QAction(QIcon(":/icons/receiving_addresses"), tr("&Receive tokens"), this);
    receiveCoinsAction->setToolTip(tr("Show the list of addresses for receiving payments"));
    receiveCoinsAction->setCheckable(true);
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));
    tabGroup->addAction(receiveCoinsAction);

    historyAction = new QAction(QIcon(":/icons/history"), tr("&Transactions"), this);
    historyAction->setToolTip(tr("Browse transaction history"));
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_5));
    tabGroup->addAction(historyAction);

    addressBookAction = new QAction(QIcon(":/icons/address-book"), tr("&Address Book"), this);
    addressBookAction->setToolTip(tr("Edit the list of stored addresses and labels"));
    addressBookAction->setCheckable(true);
    addressBookAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_6));
    tabGroup->addAction(addressBookAction);

    connect(overviewAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(gotoOverviewPage()));
    connect(ntp1tokensAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(ntp1tokensAction, SIGNAL(triggered()), this, SLOT(gotoNTP1SummaryPage()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(gotoHistoryPage()));
    connect(addressBookAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(addressBookAction, SIGNAL(triggered()), this, SLOT(gotoAddressBookPage()));

    quitAction = new QAction(QIcon(":/icons/quit"), tr("E&xit"), this);
    quitAction->setToolTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
    aboutAction = new QAction(QIcon(":/icons/bitcoin"), tr("&About neblio"), this);
    aboutAction->setToolTip(tr("Show information about neblio"));
    aboutAction->setMenuRole(QAction::AboutRole);
    aboutQtAction = new QAction(QIcon(":/icons/about_qt"), tr("About &Qt"), this);
    aboutQtAction->setToolTip(tr("Show information about Qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    optionsAction = new QAction(QIcon(":/icons/options"), tr("&Options..."), this);
    optionsAction->setToolTip(tr("Modify configuration options for neblio"));
    optionsAction->setMenuRole(QAction::PreferencesRole);
    toggleHideAction    = new QAction(QIcon(":/icons/bitcoin"), tr("&Show / Hide"), this);
    encryptWalletAction = new QAction(QIcon(":/icons/lock_closed"), tr("&Encrypt Wallet..."), this);
    encryptWalletAction->setToolTip(tr("Encrypt or decrypt wallet"));
    encryptWalletAction->setCheckable(true);
    backupWalletAction = new QAction(QIcon(":/icons/filesave"), tr("&Backup Wallet..."), this);
    backupWalletAction->setToolTip(tr("Backup wallet to another location"));
    importWalletAction = new QAction(QIcon(":/icons/open"), tr("&Import Wallet..."), this);
    importWalletAction->setToolTip(
        tr("Import a wallet from another location that was backed up before"));
    changePassphraseAction = new QAction(QIcon(":/icons/key"), tr("&Change Passphrase..."), this);
    changePassphraseAction->setToolTip(tr("Change the passphrase used for wallet encryption"));
    unlockWalletAction = new QAction(QIcon(":/icons/lock_open"), tr("&Unlock Wallet..."), this);
    unlockWalletAction->setToolTip(tr("Unlock wallet"));
    lockWalletAction = new QAction(QIcon(":/icons/lock_closed"), tr("&Lock Wallet"), this);
    lockWalletAction->setToolTip(tr("Lock wallet"));
    signMessageAction   = new QAction(QIcon(":/icons/edit"), tr("Sign &message..."), this);
    verifyMessageAction = new QAction(QIcon(":/icons/transaction_0"), tr("&Verify message..."), this);
    exportBlockchainBootstrapAction =
        new QAction(QIcon(":/icons/hdd"), tr("&Export blockchain bootstrap..."), this);

    exportAction = new QAction(QIcon(":/icons/export"), tr("&Export..."), this);
    exportAction->setToolTip(tr("Export the data in the current tab to a file"));
    openRPCConsoleAction = new QAction(QIcon(":/icons/debugwindow"), tr("&Debug window"), this);
    openRPCConsoleAction->setToolTip(tr("Open debugging and diagnostic console"));

    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(encryptWalletAction, SIGNAL(triggered(bool)), this, SLOT(encryptWallet(bool)));
    connect(backupWalletAction, SIGNAL(triggered()), this, SLOT(backupWallet()));
    connect(importWalletAction, SIGNAL(triggered()), this, SLOT(importWallet()));
    connect(changePassphraseAction, SIGNAL(triggered()), this, SLOT(changePassphrase()));
    connect(unlockWalletAction, SIGNAL(triggered()), this, SLOT(unlockWallet()));
    connect(lockWalletAction, SIGNAL(triggered()), this, SLOT(lockWallet()));
    connect(signMessageAction, SIGNAL(triggered()), this, SLOT(gotoSignMessageTab()));
    connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(gotoVerifyMessageTab()));
    connect(exportBlockchainBootstrapAction, &QAction::triggered, this,
            &BitcoinGUI::exportBlockchainBootstrap);
}

void BitcoinGUI::createMenuBar()
{
#ifdef Q_OS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
    appMenuBar = new QMenuBar();
#else
    // Get the main window's menu bar on other platforms
    appMenuBar = menuBar();
#endif

    // Configure the menus
    QMenu* file = appMenuBar->addMenu(tr("&File"));
    file->addAction(backupWalletAction);
    file->addAction(importWalletAction);
    file->addSeparator();
    file->addAction(exportAction);
    file->addAction(exportBlockchainBootstrapAction);
    file->addSeparator();
    file->addAction(signMessageAction);
    file->addAction(verifyMessageAction);
    file->addSeparator();
    file->addAction(quitAction);

    QMenu* settings = appMenuBar->addMenu(tr("&Settings"));
    settings->addAction(encryptWalletAction);
    settings->addAction(changePassphraseAction);
    settings->addAction(unlockWalletAction);
    settings->addAction(lockWalletAction);
    settings->addSeparator();
    settings->addAction(optionsAction);

    QMenu* help = appMenuBar->addMenu(tr("&Help"));
    help->addAction(openRPCConsoleAction);
    help->addSeparator();
    help->addAction(aboutAction);
    help->addAction(aboutQtAction);
}

void BitcoinGUI::createToolBars()
{
    toolbar = addToolBar(tr("Tabs toolbar"));
    toolbar->setMovable(false); // Not movable because the color bar would not be commpatible
    toolbar->setStyleSheet("QToolBar {background-color: rgba(255, 255, 255, 0);}");

    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->addAction(overviewAction);
    toolbar->addAction(ntp1tokensAction);
    toolbar->addAction(sendCoinsAction);
    toolbar->addAction(receiveCoinsAction);
    toolbar->addAction(historyAction);
    toolbar->addAction(addressBookAction);

    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->addAction(exportAction);
}

void BitcoinGUI::setClientModel(ClientModel* clientModel)
{
    this->clientModel = clientModel;
    if (clientModel) {
        // Replace some strings and icons, when using the testnet
        if (clientModel->isTestNet()) {
            setWindowTitle(windowTitle() + QString(" ") + tr("[testnet]"));
#ifndef Q_OS_MAC
            qApp->setWindowIcon(QIcon(":icons/bitcoin_testnet"));
            setWindowIcon(QIcon(":icons/bitcoin_testnet"));
#else
            MacDockIconHandler::instance()->setIcon(QIcon(":icons/bitcoin_testnet"));
#endif
            if (trayIcon) {
                trayIcon->setToolTip(tr("neblio client") + QString(" ") + tr("[testnet]"));
                trayIcon->setIcon(QIcon(":/icons/toolbar_testnet"));
            }

            toggleHideAction->setIcon(QIcon(":/icons/toolbar_testnet"));
            aboutAction->setIcon(QIcon(":/icons/toolbar_testnet"));
        }

        // Create system tray menu (or setup the dock menu) that late to prevent users from calling
        // actions, while the client has not yet fully loaded
        createTrayIconMenu();

        // Keep up to date with client
        setNumConnections(clientModel->getNumConnections());
        connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));

        setNumBlocks(clientModel->getNumBlocks(), clientModel->getNumBlocksOfPeers());
        connect(clientModel, SIGNAL(numBlocksChanged(int, int)), this, SLOT(setNumBlocks(int, int)));

        // Report errors from network/worker thread
        connect(clientModel, SIGNAL(error(QString, QString, bool)), this,
                SLOT(error(QString, QString, bool)));

        rpcConsole->setClientModel(clientModel);
        addressBookPage->setOptionsModel(clientModel->getOptionsModel());
        receiveCoinsPage->setOptionsModel(clientModel->getOptionsModel());
    }
}

void BitcoinGUI::setWalletModel(WalletModel* walletModel)
{
    this->walletModel = walletModel;
    if (walletModel) {
        // Report errors from wallet thread
        connect(walletModel, SIGNAL(error(QString, QString, bool)), this,
                SLOT(error(QString, QString, bool)));

        // Put transaction list in tabs
        transactionView->setModel(walletModel);

        overviewPage->setModel(walletModel);
        addressBookPage->setModel(walletModel->getAddressTableModel());
        receiveCoinsPage->setModel(walletModel->getAddressTableModel());
        sendCoinsPage->setModel(walletModel);
        signVerifyMessageDialog->setModel(walletModel);
        ntp1SummaryPage->ui->issueNewNTP1TokenDialog->setWalletModel(walletModel);

        setEncryptionStatus(walletModel->getEncryptionStatus());
        connect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SLOT(setEncryptionStatus(int)));

        // Balloon pop-up for new transaction
        connect(walletModel->getTransactionTableModel(), SIGNAL(rowsInserted(QModelIndex, int, int)),
                this, SLOT(incomingTransaction(QModelIndex, int, int)));

        // Ask for passphrase if needed
        connect(walletModel, SIGNAL(requireUnlock()), this, SLOT(unlockWallet()));
    }
}

void BitcoinGUI::createTrayIcon()
{
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);

    trayIcon->setToolTip(tr("neblio client"));
    trayIcon->setIcon(QIcon(":/icons/toolbar"));
    trayIcon->show();
#endif

    notificator = new Notificator(qApp->applicationName(), trayIcon);
}

void BitcoinGUI::createTrayIconMenu()
{
    QMenu* trayIconMenu;
#ifndef Q_OS_MAC
    // return if trayIcon is unset (only on non-Mac OSes)
    if (!trayIcon)
        return;

    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);

    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this,
            SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
#else
    // Note: On Mac, the dock icon is used to provide the tray's functionality.
    MacDockIconHandler* dockIconHandler = MacDockIconHandler::instance();
    dockIconHandler->setMainWindow((QMainWindow*)this);
    trayIconMenu = dockIconHandler->dockMenu();
#endif

    // Configuration of the tray icon (or dock icon) icon menu
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(sendCoinsAction);
    trayIconMenu->addAction(receiveCoinsAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(signMessageAction);
    trayIconMenu->addAction(verifyMessageAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(optionsAction);
    trayIconMenu->addAction(openRPCConsoleAction);
#ifndef Q_OS_MAC // This is built-in on Mac
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif
}

#ifndef Q_OS_MAC
void BitcoinGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        // Click on system tray icon triggers show/hide of the main window
        toggleHideAction->trigger();
    }
}
#endif

void BitcoinGUI::optionsClicked()
{
    if (!clientModel || !clientModel->getOptionsModel())
        return;
    OptionsDialog dlg;
    dlg.setModel(clientModel->getOptionsModel());
    dlg.exec();
}

void BitcoinGUI::aboutClicked()
{
    AboutDialog dlg;
    dlg.setModel(clientModel);
    dlg.exec();
}

void BitcoinGUI::setNumConnections(int count)
{
    QString icon;
    switch (count) {
    case 0:
        icon = ":/icons/connect_0";
        break;
    case 1:
    case 2:
    case 3:
        icon = ":/icons/connect_1";
        break;
    case 4:
    case 5:
    case 6:
        icon = ":/icons/connect_2";
        break;
    case 7:
    case 8:
    case 9:
        icon = ":/icons/connect_3";
        break;
    default:
        icon = ":/icons/connect_4";
        break;
    }
    labelConnectionsIcon->setPixmap(QIcon(icon).pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
    labelConnectionsIcon->setToolTip(tr("%n active connection(s) to neblio network", "", count));
}

void BitcoinGUI::setNumBlocks(int count, int nTotalBlocks)
{
    // don't show / hide progress bar and its label if we have no connection to the network
    if (!clientModel || (clientModel->getNumConnections() == 0 && !clientModel->isImporting())) {
        progressBarLabel->setVisible(false);
        progressBar->setVisible(false);

        return;
    }

    QString strStatusBarWarnings = clientModel->getStatusBarWarnings();
    QString tooltip;

    if (count < nTotalBlocks) {
        int   nRemainingBlocks = nTotalBlocks - count;
        float nPercentageDone  = count / (nTotalBlocks * 0.01f);

        if (strStatusBarWarnings.isEmpty()) {
            progressBarLabel->setText(tr(clientModel->isImporting() ? "Importing blocks..."
                                                                    : "Synchronizing with network..."));
            progressBarLabel->setVisible(true);
            progressBar->setFormat(tr("~%n block(s) remaining", "", nRemainingBlocks));
            progressBar->setMaximum(nTotalBlocks);
            progressBar->setValue(count);
            progressBar->setVisible(true);
        }

        tooltip = tr("Downloaded %1 of %2 blocks of transaction history (%3% done).")
                      .arg(count)
                      .arg(nTotalBlocks)
                      .arg(nPercentageDone, 0, 'f', 2);
    } else {
        if (strStatusBarWarnings.isEmpty())
            progressBarLabel->setVisible(false);

        progressBar->setVisible(false);
        tooltip = tr("Downloaded %1 blocks of transaction history.").arg(count);
    }

    // Override progressBarLabel text and hide progress bar, when we have warnings to display
    if (!strStatusBarWarnings.isEmpty()) {
        progressBarLabel->setText(strStatusBarWarnings);
        progressBarLabel->setVisible(true);
        progressBar->setVisible(false);
    }

    QDateTime lastBlockDate = clientModel->getLastBlockDate();
    int       secs          = lastBlockDate.secsTo(QDateTime::currentDateTime());
    QString   text;

    // Represent time from last generated block in human readable text
    if (secs <= 0) {
        // Fully up to date. Leave text empty.
    } else if (secs < 60) {
        text = tr("%n second(s) ago", "", secs);
    } else if (secs < 60 * 60) {
        text = tr("%n minute(s) ago", "", secs / 60);
    } else if (secs < 24 * 60 * 60) {
        text = tr("%n hour(s) ago", "", secs / (60 * 60));
    } else {
        text = tr("%n day(s) ago", "", secs / (60 * 60 * 24));
    }

    // Set icon state: spinning if catching up, tick otherwise
    if (secs < 90 * 60 && count >= nTotalBlocks) {
        tooltip = tr("Up to date") + QString(".<br>") + tooltip;
        labelBlocksIcon->setPixmap(
            QIcon(":/icons/synced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));

        overviewPage->showOutOfSyncWarning(false);
        ntp1SummaryPage->showOutOfSyncWarning(false);
    } else {
        tooltip = tr("Catching up...") + QString("<br>") + tooltip;
        labelBlocksIcon->setMovie(syncIconMovie);
        syncIconMovie->start();

        overviewPage->showOutOfSyncWarning(true);
        ntp1SummaryPage->showOutOfSyncWarning(true);
    }

    if (!text.isEmpty()) {
        tooltip += QString("<br>");
        tooltip += tr("Last received block was generated %1.").arg(text);
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");

    labelBlocksIcon->setToolTip(tooltip);
    progressBarLabel->setToolTip(tooltip);
    progressBar->setToolTip(tooltip);
}

void BitcoinGUI::error(const QString& title, const QString& message, bool modal)
{
    // Report errors from network/worker thread
    if (modal) {
        QMessageBox::critical(this, title, message, QMessageBox::Ok, QMessageBox::Ok);
    } else {
        notificator->notify(Notificator::Critical, title, message);
    }
}

void BitcoinGUI::changeEvent(QEvent* e)
{
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if (e->type() == QEvent::WindowStateChange) {
        if (clientModel && clientModel->getOptionsModel()->getMinimizeToTray()) {
            QWindowStateChangeEvent* wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if (!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized()) {
                QTimer::singleShot(0, this, SLOT(hide()));
                e->ignore();
            }
        }
    }
#endif
}

void BitcoinGUI::closeEvent(QCloseEvent* event)
{
    if (clientModel) {
#ifndef Q_OS_MAC // Ignored on Mac
        if (!clientModel->getOptionsModel()->getMinimizeToTray() &&
            !clientModel->getOptionsModel()->getMinimizeOnClose()) {
            qApp->quit();
        }
#endif
    }
    QMainWindow::closeEvent(event);
}

void BitcoinGUI::askFee(qint64 nFeeRequired, bool* payFee)
{
    QString strMessage =
        tr("This transaction is over the size limit.  You can still send it for a fee of %1, "
           "which goes to the nodes that process your transaction and helps to support the network.  "
           "Do you want to pay the fee?")
            .arg(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, nFeeRequired));
    QMessageBox::StandardButton retval =
        QMessageBox::question(this, tr("Confirm transaction fee"), strMessage,
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes);
    *payFee = (retval == QMessageBox::Yes);
}

void BitcoinGUI::incomingTransaction(const QModelIndex& parent, int start, int /*end*/)
{
    if (!walletModel || !clientModel)
        return;
    TransactionTableModel* ttm = walletModel->getTransactionTableModel();
    qint64                 amount =
        ttm->index(start, TransactionTableModel::Amount, parent).data(Qt::EditRole).toULongLong();
    if (!clientModel->inInitialBlockDownload()) {
        // On new transaction, make an info balloon
        // Unless the initial block download is in progress, to prevent balloon-spam
        QString date    = ttm->index(start, TransactionTableModel::Date, parent).data().toString();
        QString type    = ttm->index(start, TransactionTableModel::Type, parent).data().toString();
        QString address = ttm->index(start, TransactionTableModel::ToAddress, parent).data().toString();
        QIcon   icon    = qvariant_cast<QIcon>(
            ttm->index(start, TransactionTableModel::ToAddress, parent).data(Qt::DecorationRole));

        notificator->notify(Notificator::Information,
                            (amount) < 0 ? tr("Sent transaction") : tr("Incoming transaction"),
                            tr("Date: %1\n"
                               "Amount: %2\n"
                               "Type: %3\n"
                               "Address: %4\n")
                                .arg(date)
                                .arg(BitcoinUnits::formatWithUnit(
                                    walletModel->getOptionsModel()->getDisplayUnit(), amount, true))
                                .arg(type)
                                .arg(address),
                            icon);
    }
}

void BitcoinGUI::gotoOverviewPage()
{
    overviewAction->setChecked(true);
    centralWidget->setCurrentWidget(overviewPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoNTP1SummaryPage()
{
    ntp1tokensAction->setChecked(true);
    centralWidget->setCurrentWidget(ntp1SummaryPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoHistoryPage()
{
    historyAction->setChecked(true);
    centralWidget->setCurrentWidget(transactionsPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), transactionView, SLOT(exportClicked()));
}

void BitcoinGUI::gotoAddressBookPage()
{
    addressBookAction->setChecked(true);
    centralWidget->setCurrentWidget(addressBookPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), addressBookPage, SLOT(exportClicked()));
}

void BitcoinGUI::gotoReceiveCoinsPage()
{
    receiveCoinsAction->setChecked(true);
    centralWidget->setCurrentWidget(receiveCoinsPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), receiveCoinsPage, SLOT(exportClicked()));
}

void BitcoinGUI::gotoSendCoinsPage()
{
    sendCoinsAction->setChecked(true);
    centralWidget->setCurrentWidget(sendCoinsPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoSignMessageTab(QString addr)
{
    // call show() in showTab_SM()
    signVerifyMessageDialog->showTab_SM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_SM(addr);
}

void BitcoinGUI::exportBlockchainBootstrap()
{
    QString saveDir =
        QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation) + "/" + "bootstrap.dat";
    QString filename = QFileDialog::getSaveFileName(this, tr("Export blockchain"), saveDir,
                                                    tr("Blockchain Data (*.dat)"));

    if (!filename.isEmpty()) {
        QMessageBox::StandardButton includeOrphanResult = QMessageBox::question(
            this, "Include ophans?",
            "Would you like to include orphan blocks?\n\nIf you don't know what this means, select no.",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No);
        if (includeOrphanResult != QMessageBox::Yes && includeOrphanResult != QMessageBox::No) {
            // this means cancel/escape was chosen
            return;
        }

        GraphTraverseType graphTraverseType = GraphTraverseType::DepthFirst;

        if (includeOrphanResult == QMessageBox::Yes) {
            QMessageBox msgBox;
            msgBox.setWindowTitle("Blockchain graph order?");
            msgBox.setText("How would you like to order the blockchain graph in the "
                           "bootstrap file? Breadth-first or Depth-first?");
            QAbstractButton* pButtonDepth = msgBox.addButton(tr("Depth-First"), QMessageBox::NoRole);
            QAbstractButton* pButtonBreadth =
                msgBox.addButton(tr("Breadth-first"), QMessageBox::YesRole);
            msgBox.addButton(tr("Cancel"), QMessageBox::RejectRole);

            msgBox.exec();

            if (msgBox.clickedButton() == pButtonBreadth) {
                graphTraverseType = GraphTraverseType::BreadthFirst;
            } else if (msgBox.clickedButton() == pButtonDepth) {
                graphTraverseType = GraphTraverseType::DepthFirst;
            } else {
                return;
            }
        }

        blockchainExporterProg->setValue(0);
        blockchainExporterProg->reset();
        blockchainExporterProg->setLabelText("Exporting... please wait.");
        blockchainExporterProg->open();

        boost::promise<void>       finished;
        boost::unique_future<void> finished_future = finished.get_future();
        std::atomic<bool>          stopped{false};
        std::atomic<double>        progress{false};

        if (includeOrphanResult == QMessageBox::Yes) {
            // with orphans
            boost::thread exporterThread(boost::bind(
                &ExportBootstrapBlockchainWithOrphans, filename.toStdString(), boost::ref(stopped),
                boost::ref(progress), boost::ref(finished), graphTraverseType));
            exporterThread.detach();
        } else {
            // without orphans
            boost::thread exporterThread(boost::bind(&ExportBootstrapBlockchain, filename.toStdString(),
                                                     boost::ref(stopped), boost::ref(progress),
                                                     boost::ref(finished)));
            exporterThread.detach();
        }

        while (!finished_future.is_ready()) {
            QApplication::processEvents();
            if (blockchainExporterProg->wasCanceled()) {
                stopped.store(true);
            } else {
                blockchainExporterProg->setValue(static_cast<int>(progress * 100));
                boost::this_thread::sleep_for(boost::chrono::milliseconds(10));
            }
        }
        blockchainExporterProg->setValue(100);
        blockchainExporterProg->close();
        try {
            finished_future.get();
            QMessageBox::information(this, "Exporting blockchain done",
                                     "Exporting the blockchain is done successfully to: \n\n" +
                                         filename +
                                         "\n\n"
                                         "To load this blockchain to another client, rename this "
                                         "file to bootstrap.dat, and put it in the data directory of "
                                         "the client in question.");
        } catch (std::exception& ex) {
            QMessageBox::warning(this, "Error exporting blockchain",
                                 "Failed to export the blockchain. Error: " + QString(ex.what()));
        }
    }
}

void BitcoinGUI::gotoVerifyMessageTab(QString addr)
{
    // call show() in showTab_VM()
    signVerifyMessageDialog->showTab_VM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_VM(addr);
}

void BitcoinGUI::dragEnterEvent(QDragEnterEvent* event)
{
    // Accept only URIs
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void BitcoinGUI::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        int         nValidUrisFound = 0;
        QList<QUrl> uris            = event->mimeData()->urls();
        foreach (const QUrl& uri, uris) {
            if (sendCoinsPage->handleURI(uri.toString()))
                nValidUrisFound++;
        }

        // if valid URIs were found
        if (nValidUrisFound)
            gotoSendCoinsPage();
        else
            notificator->notify(Notificator::Warning, tr("URI handling"),
                                tr("URI can not be parsed! This can be caused by an invalid neblio "
                                   "address or malformed URI parameters."));
    }

    event->acceptProposedAction();
}

void BitcoinGUI::handleURI(QString strURI)
{
    // URI has to be valid
    if (sendCoinsPage->handleURI(strURI)) {
        showNormalIfMinimized();
        gotoSendCoinsPage();
    } else
        notificator->notify(Notificator::Warning, tr("URI handling"),
                            tr("URI can not be parsed! This can be caused by an invalid neblio address "
                               "or malformed URI parameters."));
}

void BitcoinGUI::setEncryptionStatus(int status)
{
    switch (status) {
    case WalletModel::Unencrypted:
        labelEncryptionIcon->hide();
        encryptWalletAction->setChecked(false);
        changePassphraseAction->setEnabled(false);
        unlockWalletAction->setVisible(false);
        lockWalletAction->setVisible(false);
        encryptWalletAction->setEnabled(true);
        break;
    case WalletModel::Unlocked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setPixmap(
            QIcon(":/icons/lock_open").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        unlockWalletAction->setVisible(false);
        lockWalletAction->setVisible(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    case WalletModel::Locked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setPixmap(
            QIcon(":/icons/lock_closed").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>locked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        unlockWalletAction->setVisible(true);
        lockWalletAction->setVisible(false);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    }
}

void BitcoinGUI::encryptWallet(bool status)
{
    if (!walletModel)
        return;
    AskPassphraseDialog dlg(status ? AskPassphraseDialog::Encrypt : AskPassphraseDialog::Decrypt, this);
    dlg.setModel(walletModel);
    dlg.exec();

    setEncryptionStatus(walletModel->getEncryptionStatus());
}

void BitcoinGUI::backupWallet()
{
    // check if the wallet pointer is accessible (safety check)
    if (pwalletMain == NULL) {
        QMessageBox::warning(this, tr("Backup Failed"), tr("Unable to read the local wallet."));
        return;
    }

    unlockWallet();

    bool userDoesNotWantToUnlock = false;

    // before running a backup, the wallet should be unlocked, so that a the wallet hash can be stored
    if (pwalletMain->IsLocked()) {
        QMessageBox::StandardButton answer;
        answer = QMessageBox::question(
            this, "Wallet is still locked!",
            tr("Unlocking your wallet will help Neblio Wallet notify you when the next backup is "
               "necessary. Are you sure you want to keep the wallet locked before the backup process?"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (answer == QMessageBox::No) {
            backupWallet();
            return;
        } else if (answer == QMessageBox::Cancel) {
            return;
        } else {
            userDoesNotWantToUnlock = true;
        }
    }

    QString saveDir = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
    QString filename =
        QFileDialog::getSaveFileName(this, tr("Backup Wallet"), saveDir, tr("Wallet Data (*.dat)"));
    if (!filename.isEmpty()) {
        if (!walletModel->backupWallet(filename)) {
            QMessageBox::warning(
                this, tr("Backup Failed"),
                tr("There was an error trying to save the wallet data to the new location."));
        } else {
            try {
                WriteWalletBackupHash();
            } catch (std::exception& ex) {
                if (!userDoesNotWantToUnlock) {
                    QMessageBox::warning(
                        this, tr("Registering backup event failed"),
                        QString("There was an error trying register that the backup was done. This "
                                "means that the Wallet Application would not be able to notify you when "
                                "another backup is necessary. The error is: ") +
                            QString(ex.what()));
                }
            }
        }
    }

    // after having backed up the wallet, we recheck if a backup should be made, which resets the status
    // of the alert
    checkWhetherBackupIsMade();
}

void BitcoinGUI::importWallet()
{
    QString openDir = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
    QString filename =
        QFileDialog::getOpenFileName(this, tr("Import Wallet"), openDir, tr("Wallet Data (*.dat)"));
    if (!filename.isEmpty()) {
        // make sure the local wallet is unlocked
        if (pwalletMain->IsLocked()) {
            QMessageBox::warning(this, tr("Error"), "Please unlock the wallet before importing.");
            return;
        }
        if (fWalletUnlockStakingOnly) {
            QMessageBox::warning(
                this, tr("Error"),
                "Please unlock the wallet before importing; unlocking should NOT be for staking only.");
            return;
        }

        // get passphrase, if required
        std::string passphrase;
        bool        okPressed;
        QString     passFromDialog;
        if (IsWalletEncrypted(filename.toStdString())) {
            passFromDialog =
                QInputDialog::getText(this, tr("Input passphrase"), tr("Backup wallet passphrase:"),
                                      QLineEdit::Password, "", &okPressed);
            if (!okPressed)
                return;
            passphrase = passFromDialog.toStdString();
        }
        QMessageBox::warning(
            this, tr("Attention!"),
            "Please be advised that the import process might take a while due to the necessity of "
            "rescanning the blockchain. "
            "The program might become non-responseive for some time. "
            "Please be patient and do not terminate the program until it is finished importing. "
            "Interrupting the program might result in corrupting your current wallet.");
        // add the keys from the wallet
        try {
            std::pair<long, long> keysAddedOutOfTotal =
                ImportBackupWallet(filename.toStdString(), passphrase, 0);
            QMessageBox::information(this, tr("Import status"),
                                     QString("The result of import is: ") +
                                         QString::fromStdString(ToString(keysAddedOutOfTotal.first)) +
                                         QString(" out of ") +
                                         QString::fromStdString(ToString(keysAddedOutOfTotal.second)) +
                                         QString(" added to your wallet. ") +
                                         QString(keysAddedOutOfTotal.first != keysAddedOutOfTotal.second
                                                     ? "\n\nKeys that are not added are usually skipped "
                                                       "because they are already in your wallet."
                                                     : ""));
        } catch (std::exception& ex) {
            QMessageBox::warning(this, "Error",
                                 "Error: " + QString::fromStdString(std::string(ex.what())));
        }
    }
}

void BitcoinGUI::changePassphrase()
{
    AskPassphraseDialog dlg(AskPassphraseDialog::ChangePass, this);
    dlg.setModel(walletModel);
    dlg.exec();
}

void BitcoinGUI::unlockWallet()
{
    if (!walletModel)
        return;
    // Unlock wallet when requested by wallet model
    if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
        AskPassphraseDialog::Mode mode = sender() == unlockWalletAction
                                             ? AskPassphraseDialog::UnlockStaking
                                             : AskPassphraseDialog::Unlock;
        AskPassphraseDialog dlg(mode, this);
        dlg.setModel(walletModel);
        dlg.exec();
    }
    checkWhetherBackupIsMade();
}

void BitcoinGUI::lockWallet()
{
    if (!walletModel)
        return;

    walletModel->setWalletLocked(true);
}

void BitcoinGUI::showNormalIfMinimized(bool fToggleHidden)
{
    // activateWindow() (sometimes) helps with keyboard focus on Windows
    if (isHidden()) {
        show();
        activateWindow();
    } else if (isMinimized()) {
        showNormal();
        activateWindow();
    } else if (GUIUtil::isObscured(this)) {
        raise();
        activateWindow();
    } else if (fToggleHidden)
        hide();
}

void BitcoinGUI::toggleHidden() { showNormalIfMinimized(true); }

void BitcoinGUI::updateWeight()
{
    if (!pwalletMain)
        return;

    TRY_LOCK(cs_main, lockMain);
    if (!lockMain)
        return;

    TRY_LOCK(pwalletMain->cs_wallet, lockWallet);
    if (!lockWallet)
        return;

    uint64_t nMinWeight = 0, nMaxWeight = 0;
    pwalletMain->GetStakeWeight(*pwalletMain, nMinWeight, nMaxWeight, nWeight);
}

void BitcoinGUI::updateStakingIcon()
{
    updateWeight();

    if (nLastCoinStakeSearchInterval && nWeight) {
        uint64_t     nNetworkWeight = GetPoSKernelPS();
        unsigned int nTS            = TargetSpacing();
        unsigned     nEstimateTime  = nTS * nNetworkWeight / nWeight;

        QString text;
        if (nEstimateTime < 60) {
            text = tr("%n second(s)", "", nEstimateTime);
        } else if (nEstimateTime < 60 * 60) {
            text = tr("%n minute(s)", "", nEstimateTime / 60);
        } else if (nEstimateTime < 24 * 60 * 60) {
            text = tr("%n hour(s)", "", nEstimateTime / (60 * 60));
        } else {
            text = tr("%n day(s)", "", nEstimateTime / (60 * 60 * 24));
        }

        labelStakingIcon->setPixmap(
            QIcon(":/icons/staking_on").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        labelStakingIcon->setToolTip(tr("Staking.<br>Your weight is %1<br>Network weight is "
                                        "%2<br>Expected time to earn reward is %3")
                                         .arg(nWeight)
                                         .arg(nNetworkWeight)
                                         .arg(text));
    } else {
        bool isvNodesEmpty = false;
        {
            LOCK(cs_vNodes);
            isvNodesEmpty = vNodes.empty();
        }
        labelStakingIcon->setPixmap(
            QIcon(":/icons/staking_off").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        if (pwalletMain && pwalletMain->IsLocked())
            labelStakingIcon->setToolTip(tr("Not staking because wallet is locked"));
        else if (isvNodesEmpty)
            labelStakingIcon->setToolTip(tr("Not staking because wallet is offline"));
        else if (IsInitialBlockDownload_tolerant()) {
            labelStakingIcon->setToolTip(tr("Not staking because wallet is syncing"));
        } else if (!nWeight)
            labelStakingIcon->setToolTip(tr("Not staking because you don't have mature tokens"));
        else
            labelStakingIcon->setToolTip(tr("Not staking"));
    }
}

void BitcoinGUI::updateCheckAnimation_frameChanged(int frameNumber)
{
    if (frameNumber == (updaterCheckMovie->frameCount() - 1)) {
        updaterCheckMovie->stop();
    }
}

void BitcoinGUI::checkForNeblioUpdates()
{
    if (!isUpdateRunning && appInitiated) {
        printf("Checking for updates...\n");
        updaterLabel->setToolTip("Checking for updates...");
        updaterLabel->setMovie(updaterSpinnerMovie);
        updaterSpinnerMovie->start();
        latestRelease.clear();
        updateAvailablePromise = boost::promise<bool>();
        updateAvailableFuture  = updateAvailablePromise.get_future();
        boost::thread updaterThread(boost::bind(&NeblioUpdater::checkIfUpdateIsAvailable, &neblioUpdater,
                                                boost::ref(updateAvailablePromise),
                                                boost::ref(latestRelease)));
        updaterThread.detach();
        updateConcluderTimer->start(updateConcluderTimeout);
        isUpdateRunning = true;
    }
}

void BitcoinGUI::finishCheckForNeblioUpdates()
{
    if (isUpdateRunning && updateAvailableFuture.is_ready()) {
        printf("Concluding update check...\n");
        try {
            bool updateAvailable = updateAvailableFuture.get();
            if (updateAvailable) {
                updaterLabel->setMovie(updaterUpdateExistsMovie);
                updaterUpdateExistsMovie->start();
                updaterLabel->setToolTip("A new neblio wallet version exists! Please click here for "
                                         "release notes and a download link");

                // change the action of clicking on the update icon to show the dialog
                disconnect(updaterLabel, &ClickableLabel::clicked, this,
                           &BitcoinGUI::checkForNeblioUpdates);
                connect(updaterLabel, &ClickableLabel::clicked, updateDialog, &NeblioUpdateDialog::show);

                updateDialog->setUpdateRelease(latestRelease);
                animationStopperTimer->start(animationStopperTimerTimeout);

                // stop periodic update checking
                updateCheckTimer->stop();
            } else {
                updaterLabel->setMovie(updaterCheckMovie);
                updaterCheckMovie->start();
                updaterLabel->setToolTip("Your Neblio wallet application is up-to-date.");
            }
        } catch (std::exception& ex) {
            updaterLabel->setMovie(updaterErrorMovie);
            updaterErrorMovie->start();
            animationStopperTimer->start(animationStopperTimerTimeout);
            updaterLabel->setToolTip(QString("Unable to retrieve update information: ") +
                                     QString(ex.what()));
        }
        updaterSpinnerMovie->stop();
        updateConcluderTimer->stop();
        printf("Done with updates check.\n");
        isUpdateRunning = false;
    }
}

void BitcoinGUI::stopAnimations()
{
    // start and stop the movies to go back to frame 1
    updaterErrorMovie->stop();
    updaterErrorMovie->start();
    updaterErrorMovie->stop();
    updaterUpdateExistsMovie->stop();
    updaterUpdateExistsMovie->start();
    updaterUpdateExistsMovie->stop();

    animationStopperTimer->stop();
}

void BitcoinGUI::checkWhetherBackupIsMade()
{
    try {
        if (ShouldWalletBeBackedUp()) {
            startBackupAlertBlinker();
        } else {
            stopBackupAlertBlinker();
        }
    } catch (std::exception& ex) {
        printf("%s", ex.what());
        std::cerr << ex.what() << std::endl;
    }
}

void BitcoinGUI::startBackupAlertBlinker()
{
    backupBlinkerTimer->start(backupBlinkerTimerPeriod);
    labelBackupAlertIcon->setVisible(true);
}

void BitcoinGUI::stopBackupAlertBlinker()
{
    backupBlinkerTimer->stop();
    backupBlinkerOn = false;
    labelBackupAlertIcon->setVisible(false);
}

void BitcoinGUI::blinkBackupAlertIcon()
{
    if (backupBlinkerOn) {
        labelBackupAlertIcon->setPixmap(
            QIcon(":/images/no-backup-made-empty").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        backupBlinkerOn = false;
    } else {
        labelBackupAlertIcon->setPixmap(
            QIcon(":/images/no-backup-made").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        backupBlinkerOn = true;
    }
}

void BitcoinGUI::setupUpdateControls()
{
    updaterLabel = new ClickableLabel(this->statusBar());

    // Updater animations

    updaterCheckMovie = new QMovie(":images/update-animated-check", QByteArray(), this->statusBar());

    updaterUpdateExistsMovie =
        new QMovie(":images/update-update-available", QByteArray(), this->statusBar());

    updaterErrorMovie = new QMovie(":images/update-error", QByteArray(), this->statusBar());

    updaterSpinnerMovie = new QMovie(":images/update-spinner", QByteArray(), this->statusBar());

    updateDialog = new NeblioUpdateDialog(this);

    connect(updaterCheckMovie, &QMovie::frameChanged, this,
            &BitcoinGUI::updateCheckAnimation_frameChanged);

    connect(updaterLabel, &ClickableLabel::clicked, this, &BitcoinGUI::checkForNeblioUpdates);

    connect(updateConcluderTimer, &QTimer::timeout, this, &BitcoinGUI::finishCheckForNeblioUpdates);

    connect(updateCheckTimer, &QTimer::timeout, this, &BitcoinGUI::checkForNeblioUpdates);

    connect(animationStopperTimer, &QTimer::timeout, this, &BitcoinGUI::stopAnimations);
}
