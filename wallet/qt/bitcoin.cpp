/*
 * W.J. van der Laan 2011-2012
 */
#include "bitcoingui.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "walletmodel.h"

#include "init.h"
#include "main.h"
#include "nebliosplash.h"
#include "qtipcserver.h"
#include "ui_interface.h"

#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QSplashScreen>
#include <QTextCodec>
#include <QTranslator>

#if defined(BITCOIN_NEED_QT_PLUGINS) && !defined(_BITCOIN_QT_PLUGINS_INCLUDED)
#define _BITCOIN_QT_PLUGINS_INCLUDED
#define __INSURE__
#include <QtPlugin>
Q_IMPORT_PLUGIN(qcncodecs)
Q_IMPORT_PLUGIN(qjpcodecs)
Q_IMPORT_PLUGIN(qtwcodecs)
Q_IMPORT_PLUGIN(qkrcodecs)
Q_IMPORT_PLUGIN(qtaccessiblewidgets)
#endif

// Declare meta types used for QMetaObject::invokeMethod
Q_DECLARE_METATYPE(bool*)
Q_DECLARE_METATYPE(CAmount)

// Need a global reference for the notifications to find the GUI
static BitcoinGUI*   guiref;
static NeblioSplash* splashref;

static void ThreadSafeMessageBox(const std::string& message, const std::string& caption, int style)
{
    // Message from network thread
    if (guiref) {
        bool modal = (style & CClientUIInterface::MODAL);
        // in case of modal message, use blocking connection to wait for user to click OK
        QMetaObject::invokeMethod(guiref, "error",
                                  modal ? GUIUtil::blockingGUIThreadConnection() : Qt::QueuedConnection,
                                  Q_ARG(QString, QString::fromStdString(caption)),
                                  Q_ARG(QString, QString::fromStdString(message)), Q_ARG(bool, modal));
    } else {
        NLog.write(b_sev::err, "{}: {}", caption, message);
        std::cerr << fmt::format("{}: {}", caption, message) << std::endl;
    }
}

static bool ThreadSafeAskFee(int64_t nFeeRequired, const std::string& /*strCaption*/)
{
    if (!guiref)
        return false;
    if (nFeeRequired < MIN_TX_FEE || nFeeRequired <= nTransactionFee || fDaemon)
        return true;
    bool payFee = false;

    QMetaObject::invokeMethod(guiref, "askFee", GUIUtil::blockingGUIThreadConnection(),
                              Q_ARG(qint64, nFeeRequired), Q_ARG(bool*, &payFee));

    return payFee;
}

static void ThreadSafeHandleURI(const std::string& strURI)
{
    if (!guiref)
        return;

    QMetaObject::invokeMethod(guiref, "handleURI", GUIUtil::blockingGUIThreadConnection(),
                              Q_ARG(QString, QString::fromStdString(strURI)));
}

static void InitMessage(const std::string& message, double progressFromZeroToOne)
{
    if (splashref) {
        splashref->showMessage(QString::fromStdString(message), progressFromZeroToOne);
        QApplication::instance()->processEvents();
    }
}

static void QueueShutdown()
{
    QMetaObject::invokeMethod(QCoreApplication::instance(), "quit", Qt::QueuedConnection);
}

/*
   Translate string to current locale using Qt.
 */
static std::string Translate(const char* psz)
{
    return QCoreApplication::translate("bitcoin-core", psz).toStdString();
}

/* Handle runaway exceptions. Shows a message box with the problem and quits the program.
 */
static void handleRunawayException(std::exception* e)
{
    PrintExceptionContinue(e, "Runaway exception");
    QMessageBox::critical(
        0, "Runaway exception",
        BitcoinGUI::tr(
            "A fatal error occurred. neblio can no longer continue safely and will quit. Error: ") +
            QString(e->what()) + QString("\n\n") + QString::fromStdString(strMiscWarning));
    exit(EXIT_FAILURE);
}

#ifndef BITCOIN_QT_TEST
int main(int argc, char* argv[])
{
    // Do this early as we don't want to bother initializing if we are just calling IPC
    ipcScanRelay(argc, argv);

#ifdef Q_OS_WIN
#if QT_VERSION >= 0x050600
    QGuiApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
    qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "1");
#endif
#endif

#if QT_VERSION < 0x050000
    // Internal string conversion is all UTF-8
    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
    QTextCodec::setCodecForCStrings(QTextCodec::codecForTr());
#endif

    Q_INIT_RESOURCE(bitcoin);
    QApplication app(argc, argv);

    // Register meta types used for QMetaObject::invokeMethod
    qRegisterMetaType<bool*>();
    //   Need to pass name here as CAmount is a typedef (see
    //   http://qt-project.org/doc/qt-5/qmetatype.html#qRegisterMetaType) IMPORTANT if it is no longer a
    //   typedef use the normal variant above
    qRegisterMetaType<CAmount>("CAmount");
    qRegisterMetaType<std::function<void(void)>>("std::function<void(void)>");

    // Install global event filter that makes sure that long tooltips can be word-wrapped
    app.installEventFilter(new GUIUtil::ToolTipToRichTextFilter(TOOLTIP_WRAP_THRESHOLD, &app));

    // Command-line options take precedence:
    ParseParameters(argc, argv);

    // ... then neblio.conf:
    if (!boost::filesystem::is_directory(GetDataDir(false))) {
        // This message can not be translated, as translation is not initialized yet
        // (which not yet possible because lang=XX can be overridden in bitcoin.conf in the data
        // directory)
        const std::string datadir = mapArgs.get("-datadir").value_or("");
        QMessageBox::critical(0, "neblio",
                              QString("Error: Specified data directory \"%1\" does not exist.")
                                  .arg(QString::fromStdString(datadir)));
        return EXIT_FAILURE;
    }
    ReadConfigFile(mapArgs, mapMultiArgs);

    try {
        // Check for -testnet or -regtest parameter (Params() calls are only valid after this clause)
        SelectParams(ChainTypeFromCommandLine());
    } catch (std::exception& e) {
        QMessageBox::critical(0, "neblio", QObject::tr("Error: %1").arg(e.what()));
        return EXIT_FAILURE;
    }

    InitLogging();

    // Application identification (must be set before OptionsModel is initialized,
    // as it is used to locate QSettings)
    app.setOrganizationName("neblio");
    // XXX app.setOrganizationDomain("");
    if (GetBoolArg("-testnet")) // Separate UI settings for testnet
        app.setApplicationName("neblio-Qt-testnet");
    else
        app.setApplicationName("neblio-Qt");

    // ... then GUI settings:
    OptionsModel optionsModel;

    // Get desired locale (e.g. "de_DE") from command line or use system locale
    QString lang_territory =
        QString::fromStdString(GetArg("-lang", QLocale::system().name().toStdString()));
    QString lang = lang_territory;
    // Convert to "de" only by truncating "_DE"
    lang.truncate(lang_territory.lastIndexOf('_'));

    QTranslator qtTranslatorBase, qtTranslator, translatorBase, translator;
    // Load language files for configured locale:
    // - First load the translator for the base language, without territory
    // - Then load the more specific locale translator

    // Load e.g. qt_de.qm
    if (qtTranslatorBase.load("qt_" + lang, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTranslatorBase);

    // Load e.g. qt_de_DE.qm
    if (qtTranslator.load("qt_" + lang_territory,
                          QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&qtTranslator);

    // Load e.g. bitcoin_de.qm (shortcut "de" needs to be defined in bitcoin.qrc)
    if (translatorBase.load(lang, ":/translations/"))
        app.installTranslator(&translatorBase);

    // Load e.g. bitcoin_de_DE.qm (shortcut "de_DE" needs to be defined in bitcoin.qrc)
    if (translator.load(lang_territory, ":/translations/"))
        app.installTranslator(&translator);

    // Subscribe to global signals from core
    uiInterface.ThreadSafeMessageBox.connect(ThreadSafeMessageBox);
    uiInterface.ThreadSafeAskFee.connect(ThreadSafeAskFee);
    uiInterface.ThreadSafeHandleURI.connect(ThreadSafeHandleURI);
    uiInterface.InitMessage.connect(InitMessage);
    uiInterface.QueueShutdown.connect(QueueShutdown);
    uiInterface.Translate.connect(Translate);

    // Show help message immediately after parsing command-line options (for "-lang") and setting locale,
    // but before showing splash screen.
    if (mapArgs.exists("-?") || mapArgs.exists("--help")) {
        GUIUtil::HelpMessageBox help;
        help.showOrPrint();
        return EXIT_FAILURE;
    }

    NeblioSplash splash;
    if (GetBoolArg("-splash", true) && !GetBoolArg("-min")) {
        splash.show();
        splash.moveWidgetToScreenCenter();
        splashref = &splash;
    }

    app.processEvents();

    app.setQuitOnLastWindowClosed(false);

    try {
        // Regenerate startup link, to fix links to old versions
        if (GUIUtil::GetStartOnSystemStartup())
            GUIUtil::SetStartOnSystemStartup(true);

        BitcoinGUI window;
        guiref = &window;
        if (AppInit2()) {
            {
                // Put this in a block, so that the Model objects are cleaned up before
                // calling Shutdown().

                if (splashref) {
                    splashref->close();
                    splashref = nullptr;
                    std::atomic_thread_fence(std::memory_order_seq_cst);
                }

                ClientModel clientModel(&optionsModel);
                WalletModel walletModel(pwalletMain.get(), &optionsModel);

                window.setClientModel(&clientModel);
                window.setWalletModel(&walletModel);

                // If -min option passed, start window minimized.
                if (GetBoolArg("-min")) {
                    window.showMinimized();
                } else {
                    window.show();
                }

                // Place this here as guiref has to be defined if we don't want to lose URIs
                ipcInit(argc, argv);

                app.exec();

                window.hide();
                window.setClientModel(0);
                window.setWalletModel(0);
                guiref = nullptr;
            }
            // Shutdown the core and its threads, but don't exit Bitcoin-Qt here
            Shutdown();
        } else {
            return EXIT_FAILURE;
        }
    } catch (std::exception& e) {
        handleRunawayException(&e);
    } catch (...) {
        handleRunawayException(nullptr);
    }
    return EXIT_SUCCESS;
}
#endif // BITCOIN_QT_TEST
