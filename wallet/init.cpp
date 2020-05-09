// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "bitcoinrpc.h"
#include "txdb.h"
#include "walletdb.h"
#ifdef NEBLIO_REST
#include "nebliorest.h"
#endif
#include "checkpoints.h"
#include "globals.h"
#include "init.h"
#include "main.h"
#include "net.h"
#include "ui_interface.h"
#include "util.h"
#include "zerocoin/ZeroTest.h"
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

std::shared_ptr<CWallet> pwalletMain;
CClientUIInterface       uiInterface;
bool                     fConfChange;
bool                     fEnforceCanonical;
unsigned int             nNodeLifespan;
unsigned int             nDerivationMethodIndex;
unsigned int             nMinerSleep;
enum Checkpoints::CPMode CheckpointsMode;

LockedVar<boost::signals2::signal<void()>> StopRPCRequests;

//////////////////////////////////////////////////////////////////////////////
//
// Shutdown
//

void ExitTimeout(void* /*parg*/)
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
    NewThread(Shutdown, nullptr);
#endif
}

void Shutdown(void* /*parg*/)
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
        UnregisterWallet(pwalletMain);
        std::weak_ptr<CWallet> weakWallet = pwalletMain;
        pwalletMain.reset();
        while (weakWallet.lock()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        NewThread(ExitTimeout, NULL);
        MilliSleep(50);
        printf("neblio exited\n\n");
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
        if (!boost::filesystem::is_directory(GetDataDir(false))) {
            fprintf(stderr, "Error: Specified directory does not exist\n");
            Shutdown(NULL);
        }
        ReadConfigFile(mapArgs, mapMultiArgs);

        if (mapArgs.exists("-?") || mapArgs.exists("--help")) {
            // First part of help message is specific to bitcoind / RPC client
            std::string strUsage =
                _("neblio version") + " " + FormatFullVersion() + "\n\n" + _("Usage:") + "\n" +
                "  nebliod [options]                     " + "\n" +
                "  nebliod [options] <command> [params]  " + _("Send command to -server or nebliod") +
                "\n" + "  nebliod [options] help                " + _("List commands") + "\n" +
                "  nebliod [options] help <command>      " + _("Get help for a command") + "\n";

            strUsage += "\n" + HelpMessage();

            fprintf(stdout, "%s", strUsage.c_str());
            return false;
        }

        try {
            // Check for -testnet or -regtest parameter (Params() calls are only valid after this clause)
            SelectParams(ChainTypeFromCommandLine());
        } catch (std::exception& e) {
            fprintf(stderr, "Error: %s\n", e.what());
            return EXIT_FAILURE;
        }

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
        Shutdown(NULL);
    return fRet;
}

extern void noui_connect();
int         main(int argc, char* argv[])
{
    bool fRet = false;

    // Connect bitcoind signal handlers
    noui_connect();

    fRet = AppInit(argc, argv);

    if (fRet && fDaemon)
        return 0;

    return 1;
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
#ifdef USE_UPNP
#if USE_UPNP
        "  -upnp                  " + _("Use UPnP to map the listening port (default: 1 when listening)") + "\n" +
#else
        "  -upnp                  " + _("Use UPnP to map the listening port (default: 0)") + "\n" +
#endif
#endif
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
        "  -shrinkdebugfile       " + _("Shrink debug.log file on client startup (default: 1 when no -debug)") + "\n" +
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
    fUseFastIndex = GetBoolArg("-fastindex", true);
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

    std::vector<std::string> connectVals;
    bool                     connectExists = mapMultiArgs.get("-connect", connectVals);
    if (connectExists && connectVals.size() > 0) {
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

    std::string payTxFeeVal;
    bool        payTxFeeExists = mapArgs.get("-paytxfee", payTxFeeVal);
    if (payTxFeeExists) {
        if (!ParseMoney(payTxFeeVal, nTransactionFee))
            return InitError(
                strprintf(_("Invalid amount for -paytxfee=<amount>: '%s'"), payTxFeeVal.c_str()));
        if (nTransactionFee > 0.25 * COIN)
            InitWarning(_("Warning: -paytxfee is set very high! This is the transaction fee you will "
                          "pay if you send a transaction."));
    }

    fConfChange       = GetBoolArg("-confchange", false);
    fEnforceCanonical = GetBoolArg("-enforcecanonical", true);

    std::string mininpVal;
    bool        mininpExists = mapArgs.get("-mininput", mininpVal);
    if (mininpExists) {
        if (!ParseMoney(mininpVal, nMinimumInputValue))
            return InitError(
                strprintf(_("Invalid amount for -mininput=<amount>: '%s'"), mininpVal.c_str()));
    }

    // sanitize comments per BIP-0014, format user agent and check total size
    std::vector<std::string> uacommentsOrig;
    mapMultiArgs.get("-uacomment", uacommentsOrig);
    std::vector<std::string> uacomments;
    for (const std::string& cmt : uacommentsOrig) {
        if (cmt != SanitizeString(cmt, SAFE_CHARS_UA_COMMENT))
            return InitError("User Agent comment (" + cmt + ") contains unsafe characters.");
        uacomments.push_back(cmt);
    }
    strSubVersion = FormatSubVersion(CLIENT_NAME, CLIENT_VERSION, uacomments);
    if (strSubVersion.size() > MAX_SUBVERSION_LENGTH) {
        return InitError(strprintf(_("Total length of network version string (%i) exceeds maximum "
                                     "length (%i). Reduce the number or size of uacomments."),
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
        return InitError(strprintf(_("Wallet %s resides outside data directory %s."),
                                   strWalletFileName.c_str(), strDataDir.c_str()));

    // Make sure only a single Bitcoin process is using the data directory.
    boost::filesystem::path pathLockFile = GetDataDir() / ".lock";
    FILE*                   file =
        fopen(pathLockFile.string().c_str(), "a"); // empty lock file; created if it doesn't exist.
    if (file)
        fclose(file);
    static boost::interprocess::file_lock lock(pathLockFile.string().c_str());
    if (!lock.try_lock())
        return InitError(strprintf(
            _("Cannot obtain a lock on data directory %s.  neblio is probably already running."),
            strDataDir.c_str()));

#if !defined(WIN32) && !defined(QT_GUI)
    if (fDaemon) {
        // Daemonize
        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "Error: fork() returned %d errno %d\n", pid, errno);
            return false;
        }
        if (pid > 0) {
            CreatePidFile(GetPidFile(), pid);
            return true;
        }

        pid_t sid = setsid();
        if (sid < 0)
            fprintf(stderr, "Error: setsid() returned %d errno %d\n", sid, errno);
    }
#endif

    if (GetBoolArg("-shrinkdebugfile", !fDebug))
        ShrinkDebugFile();
    printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    printf("neblio version %s (%s)\n", FormatFullVersion().c_str(), CLIENT_DATE.c_str());
    printf("Using OpenSSL version %s\n", SSLeay_version(SSLEAY_VERSION));
    if (!fLogTimestamps)
        printf("Startup time: %s\n", DateTimeStrFormat("%x %H:%M:%S", GetTime()).c_str());
    printf("Default data directory %s\n", GetDefaultDataDir().string().c_str());
    printf("Used data directory %s\n", strDataDir.c_str());
    std::ostringstream strErrors;

    if (fDaemon)
        fprintf(stdout, "neblio server starting\n");

    int64_t nStart;

    // ********************************************************* Step 5: verify database integrity

    uiInterface.InitMessage(_("Verifying database integrity..."));

    if (!OpenDBWalletTransient()) {
        string msg = strprintf(_("Error initializing database environment %s!"
                                 " To recover, BACKUP THAT DIRECTORY, then remove"
                                 " everything from it except for wallet.dat."),
                               strDataDir.c_str());
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
            string msg = strprintf(_("Warning: wallet.dat corrupt, data salvaged!"
                                     " Original wallet.dat saved as wallet.{timestamp}.bak in %s; if"
                                     " your balance or transactions are incorrect you should"
                                     " restore from a backup."),
                                   strDataDir.c_str());
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
        return InitError(strprintf(_("Unknown -socks proxy version requested: %i"), nSocksVersion));

    if (mapArgs.exists("-onlynet")) {
        std::set<enum Network>   nets;
        std::vector<std::string> onlyNetVals;
        mapMultiArgs.get("-onlynet", onlyNetVals);
        BOOST_FOREACH (std::string snet, onlyNetVals) {
            enum Network net = ParseNetwork(snet);
            if (net == NET_UNROUTABLE)
                return InitError(
                    strprintf(_("Unknown network specified in -onlynet: '%s'"), snet.c_str()));
            nets.insert(net);
        }
        for (int n = 0; n < NET_MAX; n++) {
            enum Network net = (enum Network)n;
            if (!nets.count(net))
                SetLimited(net);
        }
    }

    CService    addrProxy;
    bool        fProxy = false;
    std::string proxyVal;
    bool        proxyExists = mapArgs.get("-proxy", proxyVal);
    if (proxyExists) {
        addrProxy = CService(proxyVal, 9050);
        if (!addrProxy.IsValid())
            return InitError(strprintf(_("Invalid -proxy address: '%s'"), proxyVal.c_str()));

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
    std::string torVal;
    bool        torExists = mapArgs.get("-tor", torVal);
    if (!(torExists && torVal == "0") && (fProxy || torExists)) {
        CService addrOnion;
        if (!torExists)
            addrOnion = addrProxy;
        else
            addrOnion = CService(torVal, 9050);
        if (!addrOnion.IsValid())
            return InitError(strprintf(_("Invalid -tor address: '%s'"), torVal.c_str()));
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
        std::string              strError;
        std::vector<std::string> bindVals;
        bool                     bindExists = mapMultiArgs.get("-bind", bindVals);
        if (bindExists) {
            BOOST_FOREACH (std::string strBind, bindVals) {
                CService addrBind;
                if (!Lookup(strBind.c_str(), addrBind, GetListenPort(), false))
                    return InitError(
                        strprintf(_("Cannot resolve -bind address: '%s'"), strBind.c_str()));
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

    std::vector<std::string> externalIPVals;
    bool                     externalIPExists = mapMultiArgs.get("-externalip", externalIPVals);
    if (externalIPExists) {
        BOOST_FOREACH (string strAddr, externalIPVals) {
            CService addrLocal(strAddr, GetListenPort(), fNameLookup);
            if (!addrLocal.IsValid())
                return InitError(
                    strprintf(_("Cannot resolve -externalip address: '%s'"), strAddr.c_str()));
            AddLocal(CService(strAddr, GetListenPort(), fNameLookup), LOCAL_MANUAL);
        }
    }

    std::string reserveBalanceVal;
    bool        reserveBalanceExists = mapArgs.get("-reservebalance", reserveBalanceVal);
    if (reserveBalanceExists) // ppcoin: reserve balance amount
    {
        if (!ParseMoney(reserveBalanceVal, nReserveBalance)) {
            InitError(_("Invalid amount for -reservebalance=<amount>"));
            return false;
        }
    }

    if (mapArgs.exists("-checkpointkey")) // ppcoin: checkpoint master priv key
    {
        if (!Checkpoints::SetCheckpointPrivKey(GetArg("-checkpointkey", "")))
            InitError(_("Unable to sign checkpoint, wrong checkpointkey?\n"));
    }

    {
        std::vector<std::string> seednodesVals;
        mapMultiArgs.get("-seednode", seednodesVals);
        BOOST_FOREACH (string strDest, seednodesVals)
            AddOneShot(strDest);
    }

    // ********************************************************* Step 7: load blockchain

    if (!bitdb.Open(GetDataDir())) {
        string msg = strprintf(_("Error initializing database environment %s!"
                                 " To recover, BACKUP THAT DIRECTORY, then remove"
                                 " everything from it except for wallet.dat."),
                               strDataDir.c_str());
        return InitError(msg);
    }

    if (GetBoolArg("-loadblockindextest")) {
        CTxDB txdb("r");
        txdb.LoadBlockIndex();
        PrintBlockTree();
        return false;
    }

    uiInterface.InitMessage(_("Loading block index..."));
    printf("Loading block index...\n");
    nStart = GetTimeMillis();

    // load the block index and make a few attempts before deeming it impossible
    bool loadBlockIndexSuccess = false;
    try {
        loadBlockIndexSuccess = LoadBlockIndex();
    } catch (std::exception& ex) {
        printf("Failed to load the block index in the first attempt with error: %s", ex.what());
    }

    if (!loadBlockIndexSuccess) {
        static const int MAX_ATTEMPTS = 3;
        bool             success      = false;
        for (int i = 0; i < MAX_ATTEMPTS; i++) {
            // print the error message
            const std::string msg = "Loading block index failed. Assuming it is corrupt and attempting "
                                    "to resync (attempt: " +
                                    std::to_string(i + 1) + "/" + std::to_string(MAX_ATTEMPTS) + ")...";
            uiInterface.InitMessage(msg);
            printf("%s", msg.c_str());

            // after failure, wait 3 seconds to try again
            std::this_thread::sleep_for(std::chrono::seconds(3));

            // clear stuff that are loaded before, and reset the blockchain database
            {
                mapBlockIndex.clear();
                setStakeSeen.clear();
                CTxDB txdb("r");
                txdb.init_blockindex(true);
            }

            // attempt to recreate the blockindex again
            try {
                if (LoadBlockIndex()) {
                    // if loaded successfully, break, otherwise continue to try again
                    success = true;
                    break;
                }
            } catch (std::exception& ex) {
                printf("Failed to load the block index with error: %s", ex.what());
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
        printf("Shutdown requested. Exiting.\n");
        return false;
    }
    printf(" block index %15" PRId64 "ms\n", GetTimeMillis() - nStart);

    if (GetBoolArg("-printblockindex") || GetBoolArg("-printblocktree")) {
        PrintBlockTree();
        return false;
    }

    std::string printBlockVal;
    bool        printBlockExists = mapArgs.get("-printblock", printBlockVal);
    if (printBlockExists) {
        string strMatch = printBlockVal;
        int    nFound   = 0;
        for (BlockIndexMapType::iterator mi = mapBlockIndex.begin(); mi != mapBlockIndex.end(); ++mi) {
            uint256 hash = (*mi).first;
            if (strncmp(hash.ToString().c_str(), strMatch.c_str(), strMatch.size()) == 0) {
                CBlockIndexSmartPtr pindex = mi->second;
                CBlock              block;
                block.ReadFromDisk(pindex.get());
                block.print();
                printf("\n");
                nFound++;
            }
        }
        if (nFound == 0)
            printf("No blocks matching %s were found\n", strMatch.c_str());
        return false;
    }

    // ********************************************************* Testing Zerocoin

    if (GetBoolArg("-zerotest", false)) {
        printf("\n=== ZeroCoin tests start ===\n");
        Test_RunAllTests();
        printf("=== ZeroCoin tests end ===\n\n");
    }

    // ********************************************************* Step 8: load wallet

    uiInterface.InitMessage(_("Loading wallet..."));
    printf("Loading wallet...\n");
    nStart         = GetTimeMillis();
    bool fFirstRun = true;
    {
        std::shared_ptr<CWallet> wlt = std::make_shared<CWallet>(strWalletFileName);
        std::atomic_store(&pwalletMain, wlt);
    }
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
            printf("%s", strErrors.str().c_str());
            return InitError(strErrors.str());
        } else
            strErrors << _("Error loading wallet.dat") << "\n";
    }

    if (GetBoolArg("-upgradewallet", fFirstRun)) {
        int nMaxVersion = GetArg("-upgradewallet", 0);
        if (nMaxVersion == 0) // the -upgradewallet without argument case
        {
            printf("Performing wallet upgrade to %i\n", FEATURE_LATEST);
            nMaxVersion = CLIENT_VERSION;
            pwalletMain->SetMinVersion(FEATURE_LATEST); // permanently upgrade the wallet immediately
        } else
            printf("Allowing wallet upgrade up to %i\n", nMaxVersion);
        if (nMaxVersion < pwalletMain->GetVersion())
            strErrors << _("Cannot downgrade wallet") << "\n";
        pwalletMain->SetMaxVersion(nMaxVersion);
    }

    if (fFirstRun) {
        // Create new keyUser and set as default key
        RandAddSeedPerfmon();

        CPubKey newDefaultKey;
        if (pwalletMain->GetKeyFromPool(newDefaultKey, false)) {
            pwalletMain->SetDefaultKey(newDefaultKey);
            if (!pwalletMain->SetAddressBookName(pwalletMain->vchDefaultKey.GetID(), ""))
                strErrors << _("Cannot write default address") << "\n";
        }
    }

    printf("%s", strErrors.str().c_str());
    printf(" wallet      %15" PRId64 "ms\n", GetTimeMillis() - nStart);

    RegisterWallet(pwalletMain);

    CBlockIndexSmartPtr pindexRescan = pindexBest;
    if (GetBoolArg("-rescan") ||
        SC_CheckOperationOnRestartScheduleThenDeleteIt(SC_SCHEDULE_ON_RESTART_OPNAME__RESCAN))
        pindexRescan = boost::atomic_load(&pindexGenesisBlock);
    else {
        CWalletDB     walletdb(strWalletFileName);
        CBlockLocator locator;
        if (walletdb.ReadBestBlock(locator))
            pindexRescan = locator.GetBlockIndex();
        else
            pindexRescan = boost::atomic_load(&pindexGenesisBlock);
    }
    if (pindexBest != pindexRescan && pindexBest && pindexRescan &&
        pindexBest->nHeight > pindexRescan->nHeight) {
        uiInterface.InitMessage(_("Rescanning..."));
        printf("Rescanning last %i blocks (from block %i)...\n",
               pindexBest->nHeight - pindexRescan->nHeight, pindexRescan->nHeight);
        nStart = GetTimeMillis();
        pwalletMain->ScanForWalletTransactions(pindexRescan.get(), true);
        printf(" rescan      %15" PRId64 "ms\n", GetTimeMillis() - nStart);
    }

    // ********************************************************* Step 9: import blocks

    std::vector<boost::filesystem::path>* vPath = new std::vector<boost::filesystem::path>();
    std::vector<std::string>              loadBlockVals;
    bool loadBlockExists = mapMultiArgs.get("-loadblock", loadBlockVals);
    if (loadBlockExists) {
        BOOST_FOREACH (string strFile, loadBlockVals)
            vPath->push_back(strFile);
    }
    uiInterface.InitMessage(_("Importing blockchain data file."));
    NewThread(ThreadImport, vPath);

    // ********************************************************* Step 10: load peers

    uiInterface.InitMessage(_("Loading addresses..."));
    printf("Loading addresses...\n");
    nStart = GetTimeMillis();

    {
        CAddrDB adb;
        if (!adb.Read(addrman.get()))
            printf("Invalid or missing peers.dat; recreating\n");
    }

    printf("Loaded %i addresses from peers.dat  %" PRId64 "ms\n", addrman.get().size(),
           GetTimeMillis() - nStart);

    // ********************************************************* Step 11: start node

    if (!CheckDiskSpace())
        return false;

    RandAddSeedPerfmon();

    //// debug print
    printf("mapBlockIndex.size() = %" PRIszu "\n", mapBlockIndex.size());
    printf("nBestHeight = %d\n", nBestHeight.load());
    printf("setKeyPool.size() = %" PRIszu "\n", pwalletMain->setKeyPool.size());
    printf("mapWallet.size() = %" PRIszu "\n", pwalletMain->mapWallet.size());
    printf("mapAddressBook.size() = %" PRIszu "\n", pwalletMain->mapAddressBook.size());

    if (!NewThread(StartNode, NULL))
        InitError(_("Error: could not start node"));

    if (fServer) {
        NewThread(ThreadRPCServer, NULL);
    }
    DeleteAuthCookie(); // clear the cookie from the previous session, if it exists

    // ********************************************************* Step 12: start rest listenser

#ifdef NEBLIO_REST
    uiInterface.InitMessage(_("Starting RESTful API Listener"));
    printf("Starting RESTful API Listener\n");
    NewThread(ThreadRESTServer, NULL);
#endif

    // ********************************************************* Step 13: finished
    uiInterface.InitMessage(_("Done loading"));
    printf("Done loading\n");

    if (!strErrors.str().empty())
        return InitError(strErrors.str());

    // Add wallet transactions that aren't already in a block to mapTransactions
    pwalletMain->ReacceptWalletTransactions();

#if !defined(QT_GUI)
    // Loop until process is exit()ed from shutdown() function,
    // called from ThreadRPCServer thread when a "stop" command is received.
    while (1)
        MilliSleep(5000);
#endif

    return true;
}
