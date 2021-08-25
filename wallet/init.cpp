// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "init.h"
#include "bitcoinrpc.h"
#include "checkpoints.h"
#include "consensus.h"
#include "globals.h"
#include "logging/defaultlogger.h"
#include "main.h"
#include "net.h"
#include "stringmanip.h"
#include "txdb.h"
#include "ui_interface.h"
#include "util.h"
#include "wallet_interface.h"
#include "walletdb.h"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <openssl/crypto.h>

#ifndef WIN32
#include <signal.h>
#endif

using namespace std;
using namespace boost;

CClientUIInterface       uiInterface;
bool                     fConfChange;
unsigned int             nDerivationMethodIndex;
unsigned int             nMinerSleep;
enum Checkpoints::CPMode CheckpointsMode;
boost::atomic<bool>      appInitiated{false};

LockedVar<boost::signals2::signal<void()>> StopRPCRequests;
boost::atomic_flag                         StopRPCRequestsFlag = BOOST_ATOMIC_FLAG_INIT;

//////////////////////////////////////////////////////////////////////////////
//
// Shutdown
//

void ExitTimeout()
{
#ifdef WIN32
    MilliSleep(5000);
    ExitProcess(0);
#endif
}

//////////////////////////////////////////////////////////
/**
 * @brief VerifyDBWallet_transient; this is just an aux function that helps in blacklisting a
 *        false positive on an lock-inversion issue
 * @param strWalletFileName
 * @return Verification result
 */
CDBEnv::VerifyResult VerifyDBWalletTransient(const std::string& strWalletFileName)
{
    return bitdb.Verify(strWalletFileName, CWalletDB::Recover);
}

bool OpenDBWalletTransient() { return bitdb.Open(GetDataDir()); }

void FlushDBWalletTransient(bool shutdown) { bitdb.Flush(shutdown); }

DBErrors LoadDBWalletTransient(bool& fFirstRun)
{
    return std::atomic_load(&pwalletMain)->LoadWallet(fFirstRun);
}

//////////////////////////////////////////////////////////

void StartShutdown()
{
    // this shutdown flag is necessary because the rpc stops reading only when this flag is enabled. If
    // this isn't done, and since reading the socket stream is synchronous, it'll infinitely block,
    // preventing the program from shutting down indefinitely.
    fShutdown.store(true, boost::memory_order_seq_cst);

#ifdef QT_GUI
    // ensure we leave the Qt main loop for a clean GUI exit (Shutdown() is called in bitcoin.cpp
    // afterwards)
    uiInterface.QueueShutdown();
#else
    // Without UI, Shutdown() can simply be started in a new thread
    NewThread(Shutdown);
#endif
}

void Shutdown()
{
    static CCriticalSection cs_Shutdown;
    static bool             fTaken;

    // Make this thread recognisable as the shutdown thread
    RenameThread("neblio-shutoff");

    bool fFirstThread = false;
    {
        TRY_LOCK(cs_Shutdown, lockShutdown);
        if (lockShutdown) {
            fFirstThread = !fTaken;
            fTaken       = true;
        }
    }
    static bool fExit;
    if (fFirstThread) {
        fShutdown.store(true, boost::memory_order_seq_cst);
        nTransactionsUpdated++;
        //        CTxDB().Close();
        FlushDBWalletTransient(false);
        StopNode();
        FlushDBWalletTransient(true);
        boost::filesystem::remove(GetPidFile());
        std::weak_ptr<CWallet> weakWallet = pwalletMain;
        pwalletMain.reset();
        while (weakWallet.lock()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        // on certain platforms, signal2's destructor without disconnecting is causing a crash, this
        // fixes it
        StopRPCRequests.get().disconnect_all_slots();

        NewThread(ExitTimeout);
        MilliSleep(50);
        NLog.write(b_sev::info, "neblio exited\n\n\n\n\n\n\n\n\n");
        NLog.flush();
        fExit = true;
#ifndef QT_GUI
        // ensure non-UI client gets exited here, but let Bitcoin-Qt reach 'return 0;' in bitcoin.cpp
        exit(0);
#endif
    } else {
        while (!fExit)
            MilliSleep(500);
        MilliSleep(100);
        ExitThread(0);
    }
}

void HandleSIGTERM(int) { fRequestShutdown = true; }

void HandleSIGHUP(int) { fReopenDebugLog = true; }

void InitLogging()
{
    const boost::filesystem::path LogFilesDir = GetLoggingDir(GetDataDir());
    if (!boost::filesystem::is_directory(LogFilesDir)) {
        boost::filesystem::create_directories(GetLoggingDir(GetDataDir()));
    }
    const boost::filesystem::path LogFilePath = GetLogFileFullPath(GetDataDir());

    const bool isDebugMode = GetBoolArg("-debug");
    const bool rotateFile  = GetBoolArg("-rotatelogfile");

    // max file size
    const std::size_t maxSize = [&]() {
        const int64_t paramMaxLogFileSize = GetArg("-maxlogfilesize", -1);
        if (paramMaxLogFileSize <= 0) {
            // default values
            return static_cast<std::size_t>(isDebugMode ? 1 << 30 : 1 << 30);
        } else {
            return static_cast<std::size_t>(paramMaxLogFileSize);
        }
    }();

    // max rotated files
    const std::size_t maxFiles = [&]() {
        const int64_t paramMaxLogFiles = GetArg("-maxlogfiles", -1);
        if (paramMaxLogFiles <= 0) {
            // default values
            return static_cast<std::size_t>(isDebugMode ? 10 : 2);
        } else {
            return static_cast<std::size_t>(paramMaxLogFiles);
        }
    }();

    const b_sev minSeverity = isDebugMode ? b_sev::debug : b_sev::info;

    NLog.set_level(minSeverity);

    const bool logFileAddingResult = NLog.add_rotating_file(
        PossiblyWideStringToString(LogFilePath.native()), maxSize, maxFiles, rotateFile, minSeverity);
    if (!logFileAddingResult) {
        throw std::runtime_error("Failed to open log file for writing: " +
                                 PossiblyWideStringToString(LogFilePath.native()));
    }

    NLog.write(b_sev::info, "\n\n\n\n\n\n\n\n\n\n---------------------------------");

    NLog.write(b_sev::info, "Initialized logging successfully!");
}

//////////////////////////////////////////////////////////////////////////////
//
// Start
//
#if !defined(QT_GUI) && !defined(NEBLIO_UNITTESTS)
bool AppInit(int argc, char* argv[])
{
    bool fRet = false;
    try {
        //
        // Parameters
        //
        // If Qt is used, parameters/bitcoin.conf are parsed in qt/bitcoin.cpp's main()
        ParseParameters(argc, argv);
        if (!CheckDataDirOption()) {
            std::cerr << "Error: Specified data directory does not exist" << std::endl;
            Shutdown();
        }
        ReadConfigFile(mapArgs, mapMultiArgs);

        if (mapArgs.exists("-?") || mapArgs.exists("-h") || mapArgs.exists("--help")) {
            // First part of help message is specific to bitcoind / RPC client
            std::string strUsage =
                _("neblio version") + " " + FormatFullVersion() + "\n\n" + _("Usage:") + "\n" +
                "  nebliod [options]                     " + "\n" +
                "  nebliod [options] <command> [params]  " + _("Send command to -server or nebliod") +
                "\n" + "  nebliod [options] help                " + _("List commands") + "\n" +
                "  nebliod [options] help <command>      " + _("Get help for a command") + "\n";

            strUsage += "\n" + HelpMessage();

            std::cout << strUsage << std::endl;
            return true;
        }

        if (mapArgs.exists("-version")) {
            std::string strUsage = "version: " + FormatFullVersion() + "\n";
            std::cout << strUsage << std::endl;
            return true;
        }

        try {
            // Check for -testnet or -regtest parameter (Params() calls are only valid after this clause)
            SelectParams(ChainTypeFromCommandLine());
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return EXIT_FAILURE;
        }

        InitLogging();

        // Command-line RPC
        for (int i = 1; i < argc; i++)
            if (!IsSwitchChar(argv[i][0]) && !boost::algorithm::istarts_with(argv[i], "neblio:"))
                fCommandLine = true;

        if (fCommandLine) {
            int ret = CommandLineRPC(argc, argv);
            exit(ret);
        }

        fRet = AppInit2();
    } catch (std::exception& e) {
        PrintException(&e, "AppInit()");
    } catch (...) {
        PrintException(NULL, "AppInit()");
    }
    if (!fRet)
        Shutdown();
    return fRet;
}

extern void noui_connect();
int         main(int argc, char* argv[])
{
    // Connect bitcoind signal handlers
    noui_connect();

    const bool fRet = AppInit(argc, argv);

    //    if (fRet && fDaemon)
    //        return EXIT_SUCCESS;

    if (fRet)
        return EXIT_SUCCESS;

    return EXIT_FAILURE;
}
#endif

bool static InitError(const std::string& str)
{
    uiInterface.ThreadSafeMessageBox(str, _("neblio"),
                                     CClientUIInterface::OK | CClientUIInterface::MODAL);
    return false;
}

bool static InitWarning(const std::string& str)
{
    uiInterface.ThreadSafeMessageBox(str, _("neblio"),
                                     CClientUIInterface::OK | CClientUIInterface::ICON_EXCLAMATION |
                                         CClientUIInterface::MODAL);
    return true;
}

bool static Bind(const CService& addr, bool fError = true)
{
    if (IsLimited(addr))
        return false;
    std::string strError;
    if (!BindListenPort(addr, strError)) {
        if (fError)
            return InitError(strError);
        return false;
    }
    return true;
}

// Core-specific options shared between UI and daemon
std::string HelpMessage()
{
    // clang-format off
    string strUsage = _("Options:") + "\n" +
        "  -?                     " + _("This help message") + "\n" +
        "  -conf=<file>           " + _("Specify configuration file (default: neblio.conf)") + "\n" +
        "  -pid=<file>            " + _("Specify pid file (default: nebliod.pid)") + "\n" +
        "  -datadir=<dir>         " + _("Specify data directory") + "\n" +
        "  -wallet=<dir>          " + _("Specify wallet file (within data directory)") + "\n" +
        "  -dbcache=<n>           " + _("Set database cache size in megabytes (default: 25)") + "\n" +
        "  -maxorphanblocks=<n>   " + _("Keep at most <n> unconnectable blocks in memory (default: 750)") + "\n" +
        "  -maxorphantx=<n>       " + _("Keep at most <n> unconnectable transactions in memory (default: 100)") + "\n" +
        "  -dblogsize=<n>         " + _("Set database disk log size in megabytes (default: 100)") + "\n" +
        "  -timeout=<n>           " + _("Specify connection timeout in milliseconds (default: 5000)") + "\n" +
        "  -proxy=<ip:port>       " + _("Connect through socks proxy") + "\n" +
        "  -socks=<n>             " + _("Select the version of socks proxy to use (4-5, default: 5)") + "\n" +
        "  -tor=<ip:port>         " + _("Use proxy to reach tor hidden services (default: same as -proxy)") + "\n"
        "  -dns                   " + _("Allow DNS lookups for -addnode, -seednode and -connect") + "\n" +
        "  -port=<port>           " + _("Listen for connections on <port> (default: 6325 or testnet: 16325)") + "\n" +
        "  -maxconnections=<n>    " + _("Maintain at most <n> connections to peers (default: 125)") + "\n" +
        "  -addnode=<ip>          " + _("Add a node to connect to and attempt to keep the connection open") + "\n" +
        "  -connect=<ip>          " + _("Connect only to the specified node(s)") + "\n" +
        "  -seednode=<ip>         " + _("Connect to a node to retrieve peer addresses, and disconnect") + "\n" +
        "  -externalip=<ip>       " + _("Specify your own public address") + "\n" +
        "  -onlynet=<net>         " + _("Only connect to nodes in network <net> (IPv4, IPv6 or Tor)") + "\n" +
        "  -discover              " + _("Discover own IP address (default: 1 when listening and no -externalip)") + "\n" +
        "  -listen                " + _("Accept connections from outside (default: 1 if no -proxy or -connect)") + "\n" +
        "  -bind=<addr>           " + _("Bind to given address. Use [host]:port notation for IPv6") + "\n" +
        "  -dnsseed               " + _("Find peers using DNS lookup (default: 1)") + "\n" +
        "  -staking               " + _("Stake your coins to support network and gain reward (default: 1)") + "\n" +
        "  -synctime              " + _("Sync time with other nodes. Disable if time on your system is precise e.g. syncing with NTP (default: 1)") + "\n" +
        "  -cppolicy              " + _("Sync checkpoints policy (default: strict)") + "\n" +
        "  -banscore=<n>          " + _("Threshold for disconnecting misbehaving peers (default: 100)") + "\n" +
        "  -bantime=<n>           " + _("Number of seconds to keep misbehaving peers from reconnecting (default: 86400)") + "\n" +
        "  -maxreceivebuffer=<n>  " + _("Maximum per-connection receive buffer, <n>*1000 bytes (default: 5000)") + "\n" +
        "  -maxsendbuffer=<n>     " + _("Maximum per-connection send buffer, <n>*1000 bytes (default: 1000)") + "\n" +
        "  -noquicksync           " + _("Whether QuickSync should be used to quickly sync with the network") + "\n" +
        "  -coldstaking           " + _("Enable cold-staking for this node (default: true)") + "\n" +
#ifdef USE_UPNP
#if USE_UPNP
        "  -upnp                  " + _("Use UPnP to map the listening port (default: 1 when listening)") + "\n" +
#else
        "  -upnp                  " + _("Use UPnP to map the listening port (default: 0)") + "\n" +
#endif
#endif
        "  -nomempoolwalletresync " + _("(only for regtest) prevent the wallet from re-accepting transactions on start") + "\n" +
        "  -paytxfee=<amt>        " + _("Fee per KB to add to transactions you send") + "\n" +
        "  -mininput=<amt>        " + _("When creating transactions, ignore inputs with value less than this (default: 0.01)") + "\n" +
#ifdef QT_GUI
        "  -server                " + _("Accept command line and JSON-RPC commands") + "\n" +
#endif
#if !defined(WIN32) && !defined(QT_GUI)
        "  -daemon                " + _("Run in the background as a daemon and accept commands") + "\n" +
#endif
        "  -rpccookiefile=<file>  " + _("Location of the auth cookie (default: data dir)") + "\n" +
        "  -testnet               " + _("Use the test network") + "\n" +
        "  -debug                 " + _("Output extra debugging information. Implies all other -debug* options") + "\n" +
        "  -debugnet              " + _("Output extra network debugging information") + "\n" +
        "  -logtimestamps         " + _("Prepend debug output with timestamp") + "\n" +
        "  -maxlogfiles           " + _("Max number of log files resulting from log files rotation; default: 2 for normal; 10 for debug mode") + "\n" +
        "  -maxlogfilesize        " + _("Max size of a single rotated log file; default: 1 GB") + "\n" +
        "  -rotatelogfile         " + _("Rotate the current log file on startup; default: false") + "\n" +
        "  -printtoconsole        " + _("Send trace/debug info to console instead of debug.log file") + "\n" +
        "  -uacomment=<cmt>       " + _("Append comment to the user agent string") + "\n" +
#ifdef WIN32
        "  -printtodebugger       " + _("Send trace/debug info to debugger") + "\n" +
#endif
        "  -blockversion=<n>"       + _("Override block version to test forking scenarios (regtest only)") +
        "  -rpcuser=<user>        " + _("Username for JSON-RPC connections") + "\n" +
        "  -rpcpassword=<pw>      " + _("Password for JSON-RPC connections") + "\n" +
        "  -rpcport=<port>        " + _("Listen for JSON-RPC connections on <port> (default: 6326 or testnet: 16326 or regtest: 26326)") + "\n" +
        "  -rpcallowip=<ip>       " + _("Allow JSON-RPC connections from specified IP address") + "\n" +
        "  -rpcconnect=<ip>       " + _("Send commands to node running on <ip> (default: 127.0.0.1)") + "\n" +
        "  -blocknotify=<cmd>     " + _("Execute command when the best block changes (%s in cmd is replaced by block hash)") + "\n" +
        "  -walletnotify=<cmd>    " + _("Execute command when a wallet transaction changes (%s in cmd is replaced by TxID)") + "\n" +
        "  -confchange            " + _("Require a confirmations for change (default: 0)") + "\n" +
        "  -enforcecanonical      " + _("Enforce transaction scripts to use canonical PUSH operators (default: 1)") + "\n" +
        "  -alertnotify=<cmd>     " + _("Execute command when a relevant alert is received (%s in cmd is replaced by message)") + "\n" +
        "  -upgradewallet         " + _("Upgrade wallet to latest format") + "\n" +
        "  -keypool=<n>           " + _("Set key pool size to <n> (default: 100)") + "\n" +
        "  -rescan                " + _("Rescan the block chain for missing wallet transactions") + "\n" +
        "  -salvagewallet         " + _("Attempt to recover private keys from a corrupt wallet.dat") + "\n" +
        "  -checkblocks=<n>       " + _("How many blocks to check at startup (default: 2500, 0 = all)") + "\n" +
        "  -checklevel=<n>        " + _("How thorough the block verification is (0-6, default: 1)") + "\n" +
        "  -loadblock=<file>      " + _("Imports blocks from external blk000?.dat file") + "\n" +

        "\n" + _("Block creation options:") + "\n" +
        "  -blockminsize=<n>      "   + _("Set minimum block size in bytes (default: 0)") + "\n" +
        "  -blockmaxsize=<n>      "   + _("Set maximum block size in bytes (default: 250000)") + "\n" +
        "  -blockprioritysize=<n> "   + _("Set maximum size of high-priority/low-fee transactions in bytes (default: 27000)") + "\n";
    // clang-format on
    return strUsage;
}

/** Sanity checks
 *  Ensure that Bitcoin is running in a usable environment with all
 *  necessary library support.
 */
bool InitSanityCheck(void)
{
    if (!ECC_InitSanityCheck()) {
        InitError("OpenSSL appears to lack support for elliptic curve cryptography. For more "
                  "information, visit https://en.bitcoin.it/wiki/OpenSSL_and_EC_Libraries");
        return false;
    }

    // TODO: remaining sanity checks, see #4081

    return true;
}

void CreateMainWallet(const std::string& strWalletFileName)
{
    const std::shared_ptr<CWallet> wlt = std::make_shared<CWallet>(strWalletFileName);
    if (!wlt) {
        throw std::runtime_error("Unable to create wallet object");
    }
    std::atomic_store(&pwalletMain, wlt);
}

/** Initialize bitcoin.
 *  @pre Parameters should be parsed and config file should be read.
 */
bool AppInit2()
{
// ********************************************************* Step 1: setup
#ifdef _MSC_VER
    // Turn off Microsoft heap dump noise
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, CreateFileA("NUL", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0));
#endif
#if _MSC_VER >= 1400
    // Disable confusing "helpful" text message on abort, Ctrl-C
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
#ifdef WIN32
    // Enable Data Execution Prevention (DEP)
    // Minimum supported OS versions: WinXP SP3, WinVista >= SP1, Win Server 2008
    // A failure is non-critical and needs no further attention!
#ifndef PROCESS_DEP_ENABLE
// We define this here, because GCCs winbase.h limits this to _WIN32_WINNT >= 0x0601 (Windows 7),
// which is not correct. Can be removed, when GCCs winbase.h is fixed!
#define PROCESS_DEP_ENABLE 0x00000001
#endif
    typedef BOOL(WINAPI * PSETPROCDEPPOL)(DWORD);
    PSETPROCDEPPOL setProcDEPPol =
        (PSETPROCDEPPOL)GetProcAddress(GetModuleHandleA("Kernel32.dll"), "SetProcessDEPPolicy");
    if (setProcDEPPol != NULL)
        setProcDEPPol(PROCESS_DEP_ENABLE);
#endif
#ifndef WIN32
    umask(077);

    // Clean shutdown on SIGTERM
    struct sigaction sa;
    sa.sa_handler = HandleSIGTERM;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    // Reopen debug.log on SIGHUP
    struct sigaction sa_hup;
    sa_hup.sa_handler = HandleSIGHUP;
    sigemptyset(&sa_hup.sa_mask);
    sa_hup.sa_flags = 0;
    sigaction(SIGHUP, &sa_hup, NULL);
#endif

    // ********************************************************* Step 2: parameter interactions

    nNodeLifespan = GetArg("-addrlifespan", 7);
    nMinerSleep   = GetArg("-minersleep", 500);

    CheckpointsMode       = Checkpoints::CPMode_STRICT;
    std::string strCpMode = GetArg("-cppolicy", "strict");

    if (strCpMode == "strict")
        CheckpointsMode = Checkpoints::CPMode_STRICT;

    if (strCpMode == "advisory")
        CheckpointsMode = Checkpoints::CPMode_ADVISORY;

    if (strCpMode == "permissive")
        CheckpointsMode = Checkpoints::CPMode_PERMISSIVE;

    nDerivationMethodIndex = 0;

    if (mapArgs.exists("-bind")) {
        // when specifying an explicit binding address, you want to listen on it
        // even when -connect or -proxy is specified
        SoftSetBoolArg("-listen", true);
    }

    std::vector<std::string> connectVals =
        mapMultiArgs.get("-connect").value_or(std::vector<std::string>());
    if (connectVals.size() > 0) {
        // when only connecting to trusted nodes, do not seed via DNS, or listen by default
        SoftSetBoolArg("-dnsseed", false);
        SoftSetBoolArg("-listen", false);
    }

    if (mapArgs.exists("-proxy")) {
        // to protect privacy, do not listen by default if a proxy server is specified
        SoftSetBoolArg("-listen", false);
    }

    if (!GetBoolArg("-listen", true)) {
        // do not map ports or try to retrieve public IP when not listening (pointless)
        SoftSetBoolArg("-upnp", false);
        SoftSetBoolArg("-discover", false);
    }

    if (mapArgs.exists("-externalip")) {
        // if an explicit public IP is specified, do not try to find others
        SoftSetBoolArg("-discover", false);
    }

    if (GetBoolArg("-salvagewallet")) {
        // Rewrite just private keys: rescan to find transactions
        SoftSetBoolArg("-rescan", true);
    }

    // ********************************************************* Step 3: parameter-to-internal-flags

    fDebug = GetBoolArg("-debug");

    // -debug implies fDebug*
    if (fDebug)
        fDebugNet = true;
    else
        fDebugNet = GetBoolArg("-debugnet");

#if !defined(WIN32) && !defined(QT_GUI)
    fDaemon = GetBoolArg("-daemon");
#else
    fDaemon = false;
#endif

    if (fDaemon)
        fServer = true;
    else
        fServer = GetBoolArg("-server");

        /* force fServer when running without GUI */
#if !defined(QT_GUI)
    fServer = true;
#endif
    fPrintToConsole  = GetBoolArg("-printtoconsole");
    fPrintToDebugger = GetBoolArg("-printtodebugger");
    fLogTimestamps   = GetBoolArg("-logtimestamps", true);

    if (mapArgs.exists("-timeout")) {
        int nNewTimeout = GetArg("-timeout", 5000);
        if (nNewTimeout > 0 && nNewTimeout < 600000)
            nConnectTimeout = nNewTimeout;
    }

    const boost::optional<std::string> payTxFee = mapArgs.get("-paytxfee");
    if (payTxFee) {
        if (!ParseMoney(*payTxFee, nTransactionFee))
            return InitError(fmt::format(_("Invalid amount for -paytxfee=<amount>: '{}'"), *payTxFee));
        if (nTransactionFee > 0.25 * COIN)
            InitWarning(_("Warning: -paytxfee is set very high! This is the transaction fee you will "
                          "pay if you send a transaction."));
    }

    fConfChange       = GetBoolArg("-confchange", false);
    fEnforceCanonical = GetBoolArg("-enforcecanonical", true);

    boost::optional<std::string> mininpVal = mapArgs.get("-mininput");
    if (mininpVal) {
        if (!ParseMoney(*mininpVal, nMinimumInputValue))
            return InitError(fmt::format(_("Invalid amount for -mininput=<amount>: '{}'"), *mininpVal));
    }

    // sanitize comments per BIP-0014, format user agent and check total size
    std::vector<std::string> uacommentsOrig =
        mapMultiArgs.get("-uacomment").value_or(std::vector<std::string>());
    std::vector<std::string> uacomments;
    for (const std::string& cmt : uacommentsOrig) {
        if (cmt != SanitizeString(cmt, SAFE_CHARS_UA_COMMENT))
            return InitError("User Agent comment (" + cmt + ") contains unsafe characters.");
        uacomments.push_back(cmt);
    }
    strSubVersion = FormatSubVersion(CLIENT_NAME, CLIENT_VERSION, uacomments);
    if (strSubVersion.size() > MAX_SUBVERSION_LENGTH) {
        return InitError(fmt::format(_("Total length of network version string ({}) exceeds maximum "
                                       "length ({}). Reduce the number or size of uacomments."),
                                     strSubVersion.size(), MAX_SUBVERSION_LENGTH));
    }

    // ********************************************************* Step 4: application initialization: dir
    // lock, daemonize, pidfile, debug log Sanity check
    if (!InitSanityCheck())
        return InitError(_("Initialization sanity check failed. neblio is shutting down."));

    std::string strDataDir        = GetDataDir().string();
    std::string strWalletFileName = GetArg("-wallet", "wallet.dat");

    // strWalletFileName must be a plain filename without a directory
    if (strWalletFileName !=
        boost::filesystem::basename(strWalletFileName) + boost::filesystem::extension(strWalletFileName))
        return InitError(fmt::format(_("Wallet {} resides outside data directory {}."),
                                     strWalletFileName, strDataDir));

    // Make sure only a single Bitcoin process is using the data directory.
    boost::filesystem::path pathLockFile = GetDataDir() / ".lock";
    FILE*                   file =
        fopen(pathLockFile.string().c_str(), "a"); // empty lock file; created if it doesn't exist.
    if (file)
        fclose(file);
    static boost::interprocess::file_lock lock(pathLockFile.string().c_str());
    if (!lock.try_lock())
        return InitError(fmt::format(
            _("Cannot obtain a lock on data directory {}.  neblio is probably already running."),
            strDataDir.c_str()));

#if !defined(WIN32) && !defined(QT_GUI)
    if (fDaemon) {
        // Daemonize
        pid_t pid = fork();
        if (pid < 0) {
            std::cerr << "Error: fork() returned " << pid << " errno " << errno << std::endl;
            return false;
        }
        if (pid > 0) {
            CreatePidFile(GetPidFile(), pid);
            return true;
        }

        pid_t sid = setsid();
        if (sid < 0) {
            std::cerr << "Error: setsid() returned " << sid << " errno " << errno << std::endl;
        }
    }
#endif

    NLog.write(b_sev::info, "neblio version {} ({})", FormatFullVersion(), CLIENT_DATE);
    NLog.write(b_sev::info, "Using OpenSSL version {}", SSLeay_version(SSLEAY_VERSION));
    if (!fLogTimestamps)
        NLog.write(b_sev::info, "Startup time: {}", DateTimeStrFormat("%x %H:%M:%S", GetTime()));
    NLog.write(b_sev::info, "Default data directory {}", GetDefaultDataDir().string());
    NLog.write(b_sev::info, "Used data directory {}", strDataDir);
    std::ostringstream strErrors;

    if (fDaemon)
        std::cout << "neblio server starting" << std::endl;

    int64_t nStart;

    // ********************************************************* Step 5: verify database integrity

    uiInterface.InitMessage(_("Verifying database integrity..."), 0);

    if (!OpenDBWalletTransient()) {
        string msg = fmt::format(_("Error initializing database environment {}!"
                                   " To recover, BACKUP THAT DIRECTORY, then remove"
                                   " everything from it except for wallet.dat."),
                                 strDataDir);
        return InitError(msg);
    }

    if (GetBoolArg("-salvagewallet")) {
        // Recover readable keypairs:
        if (!CWalletDB::Recover(bitdb, strWalletFileName, true))
            return false;
    }

    if (filesystem::exists(GetDataDir() / strWalletFileName)) {
        CDBEnv::VerifyResult r = VerifyDBWalletTransient(strWalletFileName);
        if (r == CDBEnv::RECOVER_OK) {
            const string msg =
                fmt::format(_("Warning: wallet.dat corrupt, data salvaged!"
                              " Original wallet.dat saved as wallet.{timestamp}.bak in {}; if"
                              " your balance or transactions are incorrect you should"
                              " restore from a backup."),
                            strDataDir);
            uiInterface.ThreadSafeMessageBox(msg, _("neblio"),
                                             CClientUIInterface::OK |
                                                 CClientUIInterface::ICON_EXCLAMATION |
                                                 CClientUIInterface::MODAL);
        }
        if (r == CDBEnv::RECOVER_FAIL)
            return InitError(_("wallet.dat corrupt, salvage failed"));
    }

    // ********************************************************* Step 6: network initialization

    int nSocksVersion = GetArg("-socks", 5);

    if (nSocksVersion != 4 && nSocksVersion != 5)
        return InitError(fmt::format(_("Unknown -socks proxy version requested: {}"), nSocksVersion));

    if (mapArgs.exists("-onlynet")) {
        std::set<enum Network>   nets;
        std::vector<std::string> onlyNetVals =
            mapMultiArgs.get("-onlynet").value_or(std::vector<std::string>());
        for (const std::string& snet : onlyNetVals) {
            enum Network net = ParseNetwork(snet);
            if (net == NET_UNROUTABLE)
                return InitError(fmt::format(_("Unknown network specified in -onlynet: '{}'"), snet));
            nets.insert(net);
        }
        for (int n = 0; n < NET_MAX; n++) {
            enum Network net = (enum Network)n;
            if (!nets.count(net))
                SetLimited(net);
        }
    }

    CService                     addrProxy;
    bool                         fProxy = false;
    boost::optional<std::string> proxy  = mapArgs.get("-proxy");
    if (proxy) {
        addrProxy = CService(*proxy, 9050);
        if (!addrProxy.IsValid())
            return InitError(fmt::format(_("Invalid -proxy address: '{}'"), *proxy));

        if (!IsLimited(NET_IPV4))
            SetProxy(NET_IPV4, addrProxy, nSocksVersion);
        if (nSocksVersion > 4) {
            if (!IsLimited(NET_IPV6))
                SetProxy(NET_IPV6, addrProxy, nSocksVersion);
            SetNameProxy(addrProxy, nSocksVersion);
        }
        fProxy = true;
    }

    // -tor can override normal proxy, -notor disables tor entirely
    boost::optional<std::string> tor = mapArgs.get("-tor");
    if (!(tor && *tor == "0") && (fProxy || tor)) {
        CService addrOnion;
        if (!tor)
            addrOnion = addrProxy;
        else
            addrOnion = CService(*tor, 9050);
        if (!addrOnion.IsValid())
            return InitError(fmt::format(_("Invalid -tor address: '{}'"), *tor));
        SetProxy(NET_TOR, addrOnion, 5);
        SetReachable(NET_TOR);
    }

    // see Step 2: parameter interactions for more information about these
    fNoListen   = !GetBoolArg("-listen", true);
    fDiscover   = GetBoolArg("-discover", true);
    fNameLookup = GetBoolArg("-dns", true);
#ifdef USE_UPNP
    fUseUPnP = GetBoolArg("-upnp", USE_UPNP);
#endif

    bool fBound = false;
    if (!fNoListen) {
        std::string                               strError;
        boost::optional<std::vector<std::string>> bindVals = mapMultiArgs.get("-bind");
        if (bindVals) {
            for (const std::string& strBind : *bindVals) {
                CService addrBind;
                if (!Lookup(strBind.c_str(), addrBind, GetListenPort(), false))
                    return InitError(fmt::format(_("Cannot resolve -bind address: '{}'"), strBind));
                fBound |= Bind(addrBind);
            }
        } else {
            struct in_addr inaddr_any;
            inaddr_any.s_addr = INADDR_ANY;
            if (!IsLimited(NET_IPV6))
                fBound |= Bind(CService(in6addr_any, GetListenPort()), false);
            if (!IsLimited(NET_IPV4))
                fBound |= Bind(CService(inaddr_any, GetListenPort()), !fBound);
        }
        if (!fBound)
            return InitError(_("Failed to listen on any port. Use -listen=0 if you want this."));
    }

    boost::optional<std::vector<std::string>> externalIPVals = mapMultiArgs.get("-externalip");
    if (externalIPVals) {
        for (const string& strAddr : *externalIPVals) {
            CService addrLocal(strAddr, GetListenPort(), fNameLookup);
            if (!addrLocal.IsValid())
                return InitError(fmt::format(_("Cannot resolve -externalip address: '{}'"), strAddr));
            AddLocal(CService(strAddr, GetListenPort(), fNameLookup), LOCAL_MANUAL);
        }
    }

    boost::optional<std::string> reserveBalance = mapArgs.get("-reservebalance");
    if (reserveBalance) // ppcoin: reserve balance amount
    {
        if (!ParseMoney(*reserveBalance, nReserveBalance)) {
            InitError(_("Invalid amount for -reservebalance=<amount>"));
            return false;
        }
    }

    {
        std::vector<std::string> seednodesVals =
            mapMultiArgs.get("-seednode").value_or(std::vector<std::string>());
        for (const string& strDest : seednodesVals)
            AddOneShot(strDest);
    }

    // ********************************************************* Step 7: load blockchain

    if (!bitdb.Open(GetDataDir())) {
        const string msg = fmt::format(_("Error initializing database environment {}!"
                                         " To recover, BACKUP THAT DIRECTORY, then remove"
                                         " everything from it except for wallet.dat."),
                                       strDataDir);
        return InitError(msg);
    }

    if (GetBoolArg("-loadblockindextest")) {
        CTxDB txdb;
        txdb.LoadBlockIndex();
        PrintBlockTree();
        return false;
    }

    uiInterface.InitMessage(_("Loading block index..."), 0);
    NLog.write(b_sev::info, "Loading block index...");
    nStart = GetTimeMillis();

    // load the block index and make a few attempts before deeming it impossible
    bool loadBlockIndexSuccess = false;
    try {
        loadBlockIndexSuccess = LoadBlockIndex();
    } catch (std::exception& ex) {
        NLog.write(b_sev::err, "Failed to load the block index in the first attempt with error: {}",
                   ex.what());
    }

    if (!loadBlockIndexSuccess) {
        static const int MAX_ATTEMPTS = 3;
        bool             success      = false;
        for (int i = 0; i < MAX_ATTEMPTS; i++) {
            // print the error message
            const std::string msg = "Loading block index failed. Assuming it is corrupt and attempting "
                                    "to resync (attempt: " +
                                    std::to_string(i + 1) + "/" + std::to_string(MAX_ATTEMPTS) + ")...";
            uiInterface.InitMessage(msg, 0);
            NLog.write(b_sev::err, "{}", msg);

            // after failure, wait 3 seconds to try again
            std::this_thread::sleep_for(std::chrono::seconds(3));

            // clear stuff that are loaded before, and reset the blockchain database
            {
                //                mapBlockIndex.clear();
                //                setStakeSeen.clear();
                CTxDB txdb;
                txdb.resyncIfNecessary(true);
            }

            // attempt to recreate the blockindex again
            try {
                if (LoadBlockIndex()) {
                    // if loaded successfully, break, otherwise continue to try again
                    success = true;
                    break;
                }
            } catch (std::exception& ex) {
                NLog.write(b_sev::err, "Failed to load the block index with error: {}", ex.what());
            }
        }
        if (!success) {
            return InitError(_("Error loading blockindex after many attempts; check the log"));
        }
    }

    // as LoadBlockIndex can take several minutes, it's possible the user
    // requested to kill bitcoin-qt during the last operation. If so, exit.
    // As the program has not fully started yet, Shutdown() is possibly overkill.
    if (fRequestShutdown) {
        NLog.write(b_sev::info, "Shutdown requested. Exiting.");
        return false;
    }
    NLog.write(b_sev::info, " block index {} ms", GetTimeMillis() - nStart);

    if (GetBoolArg("-printblockindex") || GetBoolArg("-printblocktree")) {
        PrintBlockTree();
        return false;
    }

    const boost::optional<std::string> printBlock = mapArgs.get("-printblock");
    if (printBlock) {
        const CTxDB  txdb;
        const string strMatch      = *printBlock;
        int          nFound        = 0;
        const auto   blockIndexMap = txdb.ReadAllBlockIndexEntries();
        if (!blockIndexMap) {
            return NLog.error("Failed to read the block index map from the database");
        }
        for (auto mi = blockIndexMap->cbegin(); mi != blockIndexMap->cend(); ++mi) {
            uint256 hash = (*mi).first;
            if (strncmp(hash.ToString().c_str(), strMatch.c_str(), strMatch.size()) == 0) {
                boost::optional<CBlockIndex> pindex = mi->second;
                CBlock                       block;
                block.ReadFromDisk(&*pindex, txdb);
                block.print();
                nFound++;
            }
        }
        if (nFound == 0)
            NLog.write(b_sev::info, "No blocks matching {} were found", strMatch);
        return false;
    }

    // ********************************************************* Step 8: load wallet

    uiInterface.InitMessage(_("Loading wallet..."), 0);
    NLog.write(b_sev::info, "Loading wallet...");
    nStart         = GetTimeMillis();
    bool fFirstRun = true;

    CreateMainWallet(strWalletFileName);

    DBErrors nLoadWalletRet = LoadDBWalletTransient(fFirstRun);
    if (nLoadWalletRet != DB_LOAD_OK) {
        if (nLoadWalletRet == DB_CORRUPT)
            strErrors << _("Error loading wallet.dat: Wallet corrupted") << "\n";
        else if (nLoadWalletRet == DB_NONCRITICAL_ERROR) {
            string msg(
                _("Warning: error reading wallet.dat! All keys read correctly, but transaction data"
                  " or address book entries might be missing or incorrect."));
            uiInterface.ThreadSafeMessageBox(msg, _("neblio"),
                                             CClientUIInterface::OK |
                                                 CClientUIInterface::ICON_EXCLAMATION |
                                                 CClientUIInterface::MODAL);
        } else if (nLoadWalletRet == DB_TOO_NEW)
            strErrors << _("Error loading wallet.dat: Wallet requires newer version of neblio") << "\n";
        else if (nLoadWalletRet == DB_NEED_REWRITE) {
            strErrors << _("Wallet needed to be rewritten: restart neblio to complete") << "\n";
            NLog.write(b_sev::info, "{}", strErrors.str());
            return InitError(strErrors.str());
        } else
            strErrors << _("Error loading wallet.dat") << "\n";
    }

    if (GetBoolArg("-upgradewallet", fFirstRun)) {
        int nMaxVersion = GetArg("-upgradewallet", 0);
        if (nMaxVersion == 0) // the -upgradewallet without argument case
        {
            NLog.write(b_sev::info, "Performing wallet upgrade to {}", FEATURE_LATEST);
            nMaxVersion = CLIENT_VERSION;
            pwalletMain->SetMinVersion(FEATURE_LATEST); // permanently upgrade the wallet immediately
        } else
            NLog.write(b_sev::info, "Allowing wallet upgrade up to {}", nMaxVersion);
        if (nMaxVersion < pwalletMain->GetVersion())
            strErrors << _("Cannot downgrade wallet") << "\n";
        pwalletMain->SetMaxVersion(nMaxVersion);
    }

    if (fFirstRun) {
        // Create new keyUser and set as default key
        RandAddSeedPerfmon();

        CPubKey newKey;
        // Top up the keypool
        if (!pwalletMain->TopUpKeyPool()) {
            // Error generating keys
            InitError(_("Unable to generate initial key") += "\n");
            return NLog.error("{} {}", FUNCTIONSIG, "Unable to generate initial key");
        }
        if (pwalletMain->GetKeyFromPool(newKey)) {
            if (!pwalletMain->SetAddressBookEntry(newKey.GetID(), ""))
                strErrors << _("Cannot write first address") << "\n";
        }
    }

    NLog.write(b_sev::info, "{}", strErrors.str());
    NLog.write(b_sev::info, " wallet      {} ms", GetTimeMillis() - nStart);

    const CTxDB txdb;

    boost::optional<CBlockIndex> pindexRescan = txdb.GetBestBlockIndex();
    if (GetBoolArg("-rescan") ||
        SC_CheckOperationOnRestartScheduleThenDeleteIt(SC_SCHEDULE_ON_RESTART_OPNAME__RESCAN))
        pindexRescan = *pindexGenesisBlock;
    else {
        CWalletDB     walletdb(strWalletFileName);
        CBlockLocator locator;
        if (walletdb.ReadBestBlock(locator))
            pindexRescan = locator.GetBlockIndex(txdb);
        else
            pindexRescan = *pindexGenesisBlock;
    }
    boost::optional<CBlockIndex> bestBlockIndex = txdb.GetBestBlockIndex();
    if (pindexRescan && bestBlockIndex->GetBlockHash() != pindexRescan->GetBlockHash() &&
        txdb.GetBestBlockIndex() && bestBlockIndex->nHeight > pindexRescan->nHeight) {
        uiInterface.InitMessage(_("Rescanning..."), 0);
        NLog.write(b_sev::info, "Rescanning last {} blocks (from block {})...",
                   bestBlockIndex->nHeight - pindexRescan->nHeight, pindexRescan->nHeight);
        nStart = GetTimeMillis();
        pwalletMain->ScanForWalletTransactions(pindexRescan ? &*pindexRescan : nullptr, true);
        NLog.write(b_sev::info, " rescan      {} ms", GetTimeMillis() - nStart);
    }

    // ********************************************************* Step 9: import blocks

    std::vector<boost::filesystem::path>      vPath;
    boost::optional<std::vector<std::string>> loadBlockVals = mapMultiArgs.get("-loadblock");
    if (loadBlockVals) {
        for (const string& strFile : *loadBlockVals)
            vPath.push_back(strFile);
    }
    uiInterface.InitMessage(_("Importing blockchain data file."), 0);
    NewThread(ThreadImport, vPath);

    // ********************************************************* Step 10: load peers

    uiInterface.InitMessage(_("Loading addresses..."), 0);
    NLog.write(b_sev::info, "Loading addresses...");
    nStart = GetTimeMillis();

    {
        CAddrDB adb;
        if (!adb.Read(addrman.get()))
            NLog.write(b_sev::err, "Invalid or missing peers.dat; recreating");
    }

    NLog.write(b_sev::info, "Loaded {} addresses from peers.dat  {} ms", addrman.get().size(),
               GetTimeMillis() - nStart);

    // ********************************************************* Step 11: start node

    if (!CheckDiskSpace())
        return false;

    RandAddSeedPerfmon();

    //// debug print
    //    NLog.write(b_sev::info, "mapBlockIndex.size() = {}", mapBlockIndex.size());
    NLog.write(b_sev::info, "BestHeight = {}", bestBlockIndex->nHeight);
    NLog.write(b_sev::info, "setKeyPool.size() = {}", pwalletMain->setKeyPool.size());
    NLog.write(b_sev::info, "mapWallet.size() = {}", pwalletMain->mapWallet.size());
    NLog.write(b_sev::info, "mapAddressBook.size() = {}", pwalletMain->mapAddressBook.size());

    if (!NewThread(StartNode))
        InitError(_("Error: could not start node"));

    if (fServer) {
        NewThread(ThreadRPCServer);
    }
    DeleteAuthCookie(); // clear the cookie from the previous session, if it exists

    // ********************************************************* Step 13: finished
    uiInterface.InitMessage(_("Done loading"), 1);
    NLog.write(b_sev::info, "Done loading");

    if (!strErrors.str().empty())
        return InitError(strErrors.str());

    // Add wallet transactions that aren't already in a block to mapTransactions
    if (!(Params().NetType() == NetworkType::Regtest && GetBoolArg("-nomempoolwalletresync", false))) {
        pwalletMain->ReacceptWalletTransactions(txdb, true);
    }

    appInitiated = true;

#if !defined(QT_GUI)
    // Loop until process is exit()ed from shutdown() function,
    // called from ThreadRPCServer thread when a "stop" command is received.
    while (1)
        MilliSleep(5000);
#endif

    return true;
}
