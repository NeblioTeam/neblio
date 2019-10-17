// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "util.h"
#include "sync.h"
#include "ui_interface.h"
#include "version.h"
#include "globals.h"
#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp> // for startswith() and endswith()
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/zlib.hpp>
#include <boost/iostreams/filtering_stream.hpp>

// Work around clang compilation problem in Boost 1.46:
// /usr/include/boost/program_options/detail/config_file.hpp:163:17: error: call to function
// 'to_internal' that is neither visible in the template definition nor found by argument-dependent
// lookup See also:
// http://stackoverflow.com/questions/10020179/compilation-fail-in-boost-librairies-program-options
//           http://clang.debian.net/status.php?version=3.0&key=CANNOT_FIND_FUNCTION
namespace boost {
namespace program_options {
std::string to_internal(const std::string&);
}
} // namespace boost

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/foreach.hpp>
#include <boost/program_options/detail/config_file.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/thread.hpp>
#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <stdarg.h>

#ifdef WIN32
#ifdef _MSC_VER
#pragma warning(disable : 4786)
#pragma warning(disable : 4804)
#pragma warning(disable : 4805)
#pragma warning(disable : 4717)
#endif
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0501
#ifdef _WIN32_IE
#undef _WIN32_IE
#endif
#define _WIN32_IE 0x0501
#define WIN32_LEAN_AND_MEAN 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "shlobj.h"
#include <io.h> /* for _commit */
#elif defined(__linux__)
#include <sys/prctl.h>
#endif

using namespace std;

ThreadSafeHashMap<string, string>         mapArgs;
ThreadSafeHashMap<string, vector<string>> mapMultiArgs;
bool                                      fDebug           = false;
bool                                      fDebugNet        = false;
bool                                      fPrintToConsole  = false;
bool                                      fPrintToDebugger = false;
boost::atomic<bool>                       fRequestShutdown{false};
bool                                      fDaemon      = false;
bool                                      fServer      = false;
bool                                      fCommandLine = false;
string                                    strMiscWarning;
bool                                      fTestNet       = false;
bool                                      fNoListen      = false;
bool                                      fLogTimestamps = true;
CMedianFilter<int64_t>                    vTimeOffsets(200, 0);
bool                                      fReopenDebugLog = false;
boost::atomic<bool>                       fShutdown{false};

boost::atomic_int MODEL_UPDATE_DELAY{500};

// Init OpenSSL library multithreading support
static CCriticalSection** ppmutexOpenSSL;
void                      locking_callback(int mode, int i, const char* /*file*/, int /*line*/)
{
    if (mode & CRYPTO_LOCK) {
        ENTER_CRITICAL_SECTION(*ppmutexOpenSSL[i]);
    } else {
        LEAVE_CRITICAL_SECTION(*ppmutexOpenSSL[i]);
    }
}

LockedPageManager LockedPageManager::instance;

// Init
class CInit
{
public:
    CInit()
    {
        // Init OpenSSL library multithreading support
        ppmutexOpenSSL =
            (CCriticalSection**)OPENSSL_malloc(CRYPTO_num_locks() * sizeof(CCriticalSection*));
        for (int i = 0; i < CRYPTO_num_locks(); i++)
            ppmutexOpenSSL[i] = new CCriticalSection();
        CRYPTO_set_locking_callback(locking_callback);

#ifdef WIN32
        // Seed random number generator with screen scrape and other hardware sources
        RAND_screen();
#endif

        // Seed random number generator with performance counter
        RandAddSeed();
    }
    ~CInit()
    {
        // Shutdown OpenSSL library multithreading support
        CRYPTO_set_locking_callback(NULL);
        for (int i = 0; i < CRYPTO_num_locks(); i++)
            delete ppmutexOpenSSL[i];
        OPENSSL_free(ppmutexOpenSSL);
    }
} instance_of_cinit;

void RandAddSeed()
{
    // Seed with CPU performance counter
    int64_t nCounter = GetPerformanceCounter();
    RAND_add(&nCounter, sizeof(nCounter), 1.5);
    memset(&nCounter, 0, sizeof(nCounter));
}

void RandAddSeedPerfmon()
{
    RandAddSeed();

    // This can take up to 2 seconds, so only do it every 10 minutes
    static int64_t nLastPerfmon;
    if (GetTime() < nLastPerfmon + 10 * 60)
        return;
    nLastPerfmon = GetTime();

#ifdef WIN32
    // Don't need this on Linux, OpenSSL automatically uses /dev/urandom
    // Seed with the entire set of perfmon data
    unsigned char pdata[250000];
    memset(pdata, 0, sizeof(pdata));
    unsigned long nSize = sizeof(pdata);
    long          ret   = RegQueryValueExA(HKEY_PERFORMANCE_DATA, "Global", NULL, NULL, pdata, &nSize);
    RegCloseKey(HKEY_PERFORMANCE_DATA);
    if (ret == ERROR_SUCCESS) {
        RAND_add(pdata, nSize, nSize / 100.0);
        memset(pdata, 0, nSize);
        printf("RandAddSeed() %lu bytes\n", nSize);
    }
#endif
}

uint64_t GetRand(uint64_t nMax)
{
    if (nMax == 0)
        return 0;

    // The range of the random source must be a multiple of the modulus
    // to give every possible output value an equal possibility
    uint64_t nRange = (std::numeric_limits<uint64_t>::max() / nMax) * nMax;
    uint64_t nRand  = 0;
    do
        RAND_bytes((unsigned char*)&nRand, sizeof(nRand));
    while (nRand >= nRange);
    return (nRand % nMax);
}

int GetRandInt(int nMax) { return GetRand(nMax); }

uint256 GetRandHash()
{
    uint256 hash;
    RAND_bytes((unsigned char*)&hash, sizeof(hash));
    return hash;
}

inline int OutputDebugStringF(const char* pszFormat, ...)
{
    int ret = 0;
    if (fPrintToConsole) {
        // print to console
        va_list arg_ptr;
        va_start(arg_ptr, pszFormat);
        ret = vprintf(pszFormat, arg_ptr);
        va_end(arg_ptr);
    } else if (!fPrintToDebugger) {
        // print to debug.log
        static FILE* fileout = NULL;

        if (!fileout) {
            boost::filesystem::path pathDebug = GetDataDir() / "debug.log";
            fileout                           = fopen(pathDebug.string().c_str(), "a");
            if (fileout)
                setbuf(fileout, NULL); // unbuffered
        }
        if (fileout) {
            static bool fStartedNewLine = true;

            // This routine may be called by global destructors during shutdown.
            // Since the order of destruction of static/global objects is undefined,
            // allocate mutexDebugLog on the heap the first time this routine
            // is called to avoid crashes during shutdown.
            static boost::mutex* mutexDebugLog = NULL;
            if (mutexDebugLog == NULL)
                mutexDebugLog = new boost::mutex();
            boost::mutex::scoped_lock scoped_lock(*mutexDebugLog);

            // reopen the log file, if requested
            if (fReopenDebugLog) {
                fReopenDebugLog                   = false;
                boost::filesystem::path pathDebug = GetDataDir() / "debug.log";
                if (freopen(pathDebug.string().c_str(), "a", fileout) != NULL)
                    setbuf(fileout, NULL); // unbuffered
            }

            // Debug print useful for profiling
            if (fLogTimestamps && fStartedNewLine)
                fprintf(fileout, "%s ", DateTimeStrFormat("%x %H:%M:%S", GetTime()).c_str());
            if (pszFormat[strlen(pszFormat) - 1] == '\n')
                fStartedNewLine = true;
            else
                fStartedNewLine = false;

            va_list arg_ptr;
            va_start(arg_ptr, pszFormat);
            ret = vfprintf(fileout, pszFormat, arg_ptr);
            va_end(arg_ptr);
        }
    }

#ifdef WIN32
    if (fPrintToDebugger) {
        static CCriticalSection cs_OutputDebugStringF;

        // accumulate and output a line at a time
        {
            LOCK(cs_OutputDebugStringF);
            static std::string buffer;

            va_list arg_ptr;
            va_start(arg_ptr, pszFormat);
            buffer += vstrprintf(pszFormat, arg_ptr);
            va_end(arg_ptr);

            int line_start = 0, line_end;
            while ((line_end = buffer.find('\n', line_start)) != -1) {
                OutputDebugStringA(buffer.substr(line_start, line_end - line_start).c_str());
                line_start = line_end + 1;
            }
            buffer.erase(0, line_start);
        }
    }
#endif
    return ret;
}

string vstrprintf(const char* format, va_list ap)
{
    char  buffer[50000];
    char* p     = buffer;
    int   limit = sizeof(buffer);
    int   ret;
    while (true) {
        va_list arg_ptr;
        va_copy(arg_ptr, ap);
#ifdef WIN32
        ret = _vsnprintf(p, limit, format, arg_ptr);
#else
        ret = vsnprintf(p, limit, format, arg_ptr);
#endif
        va_end(arg_ptr);
        if (ret >= 0 && ret < limit)
            break;
        if (p != buffer)
            delete[] p;
        limit *= 2;
        p = new char[limit];
        if (p == NULL)
            throw std::bad_alloc();
    }
    string str(p, p + ret);
    if (p != buffer)
        delete[] p;
    return str;
}

string real_strprintf(const char* format, int dummy, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, dummy);
    string str = vstrprintf(format, arg_ptr);
    va_end(arg_ptr);
    return str;
}

string real_strprintf(const std::string& format, int dummy, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, dummy);
    string str = vstrprintf(format.c_str(), arg_ptr);
    va_end(arg_ptr);
    return str;
}

bool error(const char* format, ...)
{
    va_list arg_ptr;
    va_start(arg_ptr, format);
    std::string str = vstrprintf(format, arg_ptr);
    va_end(arg_ptr);
    printf("ERROR: %s\n", str.c_str());
    return false;
}

void ParseString(const string& str, char c, vector<string>& v)
{
    if (str.empty())
        return;
    string::size_type i1 = 0;
    string::size_type i2;
    while (true) {
        i2 = str.find(c, i1);
        if (i2 == str.npos) {
            v.push_back(str.substr(i1));
            return;
        }
        v.push_back(str.substr(i1, i2 - i1));
        i1 = i2 + 1;
    }
}

string FormatMoney(int64_t n, bool fPlus)
{
    // Note: not using straight sprintf here because we do NOT want
    // localized number formatting.
    int64_t n_abs     = (n > 0 ? n : -n);
    int64_t quotient  = n_abs / COIN;
    int64_t remainder = n_abs % COIN;
    string  str       = strprintf("%" PRId64 ".%08" PRId64, quotient, remainder);

    // Right-trim excess zeros before the decimal point:
    int nTrim = 0;
    for (int i = str.size() - 1; (str[i] == '0' && isdigit(str[i - 2])); --i)
        ++nTrim;
    if (nTrim)
        str.erase(str.size() - nTrim, nTrim);

    if (n < 0)
        str.insert((unsigned int)0, 1, '-');
    else if (fPlus && n > 0)
        str.insert((unsigned int)0, 1, '+');
    return str;
}

bool ParseMoney(const string& str, int64_t& nRet) { return ParseMoney(str.c_str(), nRet); }

bool ParseMoney(const char* pszIn, int64_t& nRet)
{
    string      strWhole;
    int64_t     nUnits = 0;
    const char* p      = pszIn;
    while (isspace(*p))
        p++;
    for (; *p; p++) {
        if (*p == '.') {
            p++;
            int64_t nMult = CENT * 10;
            while (isdigit(*p) && (nMult > 0)) {
                nUnits += nMult * (*p++ - '0');
                nMult /= 10;
            }
            break;
        }
        if (isspace(*p))
            break;
        if (!isdigit(*p))
            return false;
        strWhole.insert(strWhole.end(), *p);
    }
    for (; *p; p++)
        if (!isspace(*p))
            return false;
    if (strWhole.size() > 10) // guard against 63 bit overflow
        return false;
    if (nUnits < 0 || nUnits > COIN)
        return false;
    int64_t nWhole = atoi64(strWhole);
    int64_t nValue = nWhole * COIN + nUnits;

    nRet = nValue;
    return true;
}

static const signed char phexdigit[256] = {
    -1,  -1,  -1,  -1,  -1,  -1, -1, -1, -1, -1,  -1,  -1,  -1,  -1,  -1,  -1, -1, -1, -1, -1, -1, -1,
    -1,  -1,  -1,  -1,  -1,  -1, -1, -1, -1, -1,  -1,  -1,  -1,  -1,  -1,  -1, -1, -1, -1, -1, -1, -1,
    -1,  -1,  -1,  -1,  0,   1,  2,  3,  4,  5,   6,   7,   8,   9,   -1,  -1, -1, -1, -1, -1, -1, 0xa,
    0xb, 0xc, 0xd, 0xe, 0xf, -1, -1, -1, -1, -1,  -1,  -1,  -1,  -1,  -1,  -1, -1, -1, -1, -1, -1, -1,
    -1,  -1,  -1,  -1,  -1,  -1, -1, -1, -1, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, -1, -1, -1, -1, -1, -1, -1,
    -1,  -1,  -1,  -1,  -1,  -1, -1, -1, -1, -1,  -1,  -1,  -1,  -1,  -1,  -1, -1, -1, -1, -1, -1, -1,
    -1,  -1,  -1,  -1,  -1,  -1, -1, -1, -1, -1,  -1,  -1,  -1,  -1,  -1,  -1, -1, -1, -1, -1, -1, -1,
    -1,  -1,  -1,  -1,  -1,  -1, -1, -1, -1, -1,  -1,  -1,  -1,  -1,  -1,  -1, -1, -1, -1, -1, -1, -1,
    -1,  -1,  -1,  -1,  -1,  -1, -1, -1, -1, -1,  -1,  -1,  -1,  -1,  -1,  -1, -1, -1, -1, -1, -1, -1,
    -1,  -1,  -1,  -1,  -1,  -1, -1, -1, -1, -1,  -1,  -1,  -1,  -1,  -1,  -1, -1, -1, -1, -1, -1, -1,
    -1,  -1,  -1,  -1,  -1,  -1, -1, -1, -1, -1,  -1,  -1,  -1,  -1,  -1,  -1, -1, -1, -1, -1, -1, -1,
    -1,  -1,  -1,  -1,  -1,  -1, -1, -1, -1, -1,  -1,  -1,  -1,  -1,
};

bool IsHex(const string& str)
{
    BOOST_FOREACH (unsigned char c, str) {
        if (phexdigit[c] < 0)
            return false;
    }
    return (str.size() > 0) && (str.size() % 2 == 0);
}

vector<unsigned char> ParseHex(const char* psz)
{
    // convert hex dump to vector
    vector<unsigned char> vch;
    while (true) {
        while (isspace(*psz))
            psz++;
        signed char c = phexdigit[(unsigned char)*psz++];
        if (c == (signed char)-1)
            break;
        unsigned char n = (c << 4);
        c               = phexdigit[(unsigned char)*psz++];
        if (c == (signed char)-1)
            break;
        n |= c;
        vch.push_back(n);
    }
    return vch;
}

vector<unsigned char> ParseHex(const string& str) { return ParseHex(str.c_str()); }

static void InterpretNegativeSetting(string name, ThreadSafeHashMap<string, string>& mapSettingsRet)
{
    // interpret -nofoo as -foo=0 (and -nofoo=0 as -foo=1) as long as -foo not set
    if (name.find("-no") == 0) {
        std::string positive("-");
        positive.append(name.begin() + 3, name.end());
        if (mapSettingsRet.exists(positive) == 0) {
            bool value = !GetBoolArg(name);
            mapSettingsRet.set(positive, (value ? "1" : "0"));
        }
    }
}

void ParseParameters(int argc, const char* const argv[])
{
    mapArgs.clear();
    mapMultiArgs.clear();
    for (int i = 1; i < argc; i++) {
        std::string str(argv[i]);
        std::string strValue;
        size_t      is_index = str.find('=');
        if (is_index != std::string::npos) {
            strValue = str.substr(is_index + 1);
            str      = str.substr(0, is_index);
        }
#ifdef WIN32
        boost::to_lower(str);
        if (boost::algorithm::starts_with(str, "/"))
            str = "-" + str.substr(1);
#endif
        if (str[0] != '-')
            break;

        mapArgs.set(str, strValue);
        std::vector<std::string> vals;
        mapMultiArgs.get(str, vals);
        vals.push_back(strValue);
        mapMultiArgs.set(str, vals);
    }

    {
        // add default neblio nodes
        mapArgs.set("-addnode", "nebliodseed2.nebl.io");
        std::vector<std::string> nodes;
        mapMultiArgs.get("-addnode", nodes);
        nodes.push_back("nebliodseed1.nebl.io");
        nodes.push_back("nebliodseed2.nebl.io");
        mapMultiArgs.set("-addnode", nodes);
    }

    std::unordered_map<std::string, std::string> mapArgsD = mapArgs.getInternalMap();
    // New 0.6 features:
    BOOST_FOREACH (const PAIRTYPE(string, string) & entry, mapArgsD) {
        string name = entry.first;

        //  interpret --foo as -foo (as long as both are not set)
        if (name.find("--") == 0) {
            std::string singleDash(name.begin() + 1, name.end());
            if (mapArgsD.count(singleDash) == 0)
                mapArgs.set(singleDash, entry.second);
            name = singleDash;
        }

        // interpret -nofoo as -foo=0 (and -nofoo=0 as -foo=1) as long as -foo not set
        InterpretNegativeSetting(name, mapArgs);
    }
}

std::string GetArg(const std::string& strArg, const std::string& strDefault)
{
    std::string strVal;
    bool        valExists = mapArgs.get(strArg, strVal);
    if (valExists) {
        return strVal;
    }
    return strDefault;
}

int64_t GetArg(const std::string& strArg, int64_t nDefault)
{
    std::string strVal;
    bool        valExists = mapArgs.get(strArg, strVal);
    if (valExists) {
        return atoi64(strVal);
    }
    return nDefault;
}

bool GetBoolArg(const std::string& strArg, bool fDefault)
{
    std::string strVal;
    bool        valExists = mapArgs.get(strArg, strVal);
    if (valExists) {
        if (strVal.empty()) {
            return true;
        }
        return (atoi(strVal) != 0);
    }
    return fDefault;
}

bool SoftSetArg(const std::string& strArg, const std::string& strValue)
{
    if (mapArgs.exists(strArg)) {
        return false;
    }
    mapArgs.set(strArg, strValue);
    return true;
}

bool SoftSetBoolArg(const std::string& strArg, bool fValue)
{
    if (fValue)
        return SoftSetArg(strArg, std::string("1"));
    else
        return SoftSetArg(strArg, std::string("0"));
}

string EncodeBase64(const unsigned char* pch, size_t len)
{
    static const char* pbase64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    string strRet = "";
    strRet.reserve((len + 2) / 3 * 4);

    int                  mode = 0, left = 0;
    const unsigned char* pchEnd = pch + len;

    while (pch < pchEnd) {
        int enc = *(pch++);
        switch (mode) {
        case 0: // we have no bits
            strRet += pbase64[enc >> 2];
            left = (enc & 3) << 4;
            mode = 1;
            break;

        case 1: // we have two bits
            strRet += pbase64[left | (enc >> 4)];
            left = (enc & 15) << 2;
            mode = 2;
            break;

        case 2: // we have four bits
            strRet += pbase64[left | (enc >> 6)];
            strRet += pbase64[enc & 63];
            mode = 0;
            break;
        }
    }

    if (mode) {
        strRet += pbase64[left];
        strRet += '=';
        if (mode == 1)
            strRet += '=';
    }

    return strRet;
}

string EncodeBase64(const string& str)
{
    return EncodeBase64((const unsigned char*)str.c_str(), str.size());
}

vector<unsigned char> DecodeBase64(const char* p, bool* pfInvalid)
{
    static const int decode64_table[256] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,
        7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
        -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
        49, 50, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

    if (pfInvalid)
        *pfInvalid = false;

    vector<unsigned char> vchRet;
    vchRet.reserve(strlen(p) * 3 / 4);

    int mode = 0;
    int left = 0;

    while (1) {
        int dec = decode64_table[(unsigned char)*p];
        if (dec == -1)
            break;
        p++;
        switch (mode) {
        case 0: // we have no bits and get 6
            left = dec;
            mode = 1;
            break;

        case 1: // we have 6 bits and keep 4
            vchRet.push_back((left << 2) | (dec >> 4));
            left = dec & 15;
            mode = 2;
            break;

        case 2: // we have 4 bits and get 6, we keep 2
            vchRet.push_back((left << 4) | (dec >> 2));
            left = dec & 3;
            mode = 3;
            break;

        case 3: // we have 2 bits and get 6
            vchRet.push_back((left << 6) | dec);
            mode = 0;
            break;
        }
    }

    if (pfInvalid)
        switch (mode) {
        case 0: // 4n base64 characters processed: ok
            break;

        case 1: // 4n+1 base64 character processed: impossible
            *pfInvalid = true;
            break;

        case 2: // 4n+2 base64 characters processed: require '=='
            if (left || p[0] != '=' || p[1] != '=' || decode64_table[(unsigned char)p[2]] != -1)
                *pfInvalid = true;
            break;

        case 3: // 4n+3 base64 characters processed: require '='
            if (left || p[0] != '=' || decode64_table[(unsigned char)p[1]] != -1)
                *pfInvalid = true;
            break;
        }

    return vchRet;
}

string DecodeBase64(const string& str)
{
    vector<unsigned char> vchRet = DecodeBase64(str.c_str());
    return string((const char*)&vchRet[0], vchRet.size());
}

string EncodeBase32(const unsigned char* pch, size_t len)
{
    static const char* pbase32 = "abcdefghijklmnopqrstuvwxyz234567";

    string strRet = "";
    strRet.reserve((len + 4) / 5 * 8);

    int                  mode = 0, left = 0;
    const unsigned char* pchEnd = pch + len;

    while (pch < pchEnd) {
        int enc = *(pch++);
        switch (mode) {
        case 0: // we have no bits
            strRet += pbase32[enc >> 3];
            left = (enc & 7) << 2;
            mode = 1;
            break;

        case 1: // we have three bits
            strRet += pbase32[left | (enc >> 6)];
            strRet += pbase32[(enc >> 1) & 31];
            left = (enc & 1) << 4;
            mode = 2;
            break;

        case 2: // we have one bit
            strRet += pbase32[left | (enc >> 4)];
            left = (enc & 15) << 1;
            mode = 3;
            break;

        case 3: // we have four bits
            strRet += pbase32[left | (enc >> 7)];
            strRet += pbase32[(enc >> 2) & 31];
            left = (enc & 3) << 3;
            mode = 4;
            break;

        case 4: // we have two bits
            strRet += pbase32[left | (enc >> 5)];
            strRet += pbase32[enc & 31];
            mode = 0;
        }
    }

    static const int nPadding[5] = {0, 6, 4, 3, 1};
    if (mode) {
        strRet += pbase32[left];
        for (int n = 0; n < nPadding[mode]; n++)
            strRet += '=';
    }

    return strRet;
}

string EncodeBase32(const string& str)
{
    return EncodeBase32((const unsigned char*)str.c_str(), str.size());
}

vector<unsigned char> DecodeBase32(const char* p, bool* pfInvalid)
{
    static const int decode32_table[256] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, 26, 27, 28, 29, 30, 31, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,
        7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
        -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
        23, 24, 25, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

    if (pfInvalid)
        *pfInvalid = false;

    vector<unsigned char> vchRet;
    vchRet.reserve((strlen(p)) * 5 / 8);

    int mode = 0;
    int left = 0;

    while (1) {
        int dec = decode32_table[(unsigned char)*p];
        if (dec == -1)
            break;
        p++;
        switch (mode) {
        case 0: // we have no bits and get 5
            left = dec;
            mode = 1;
            break;

        case 1: // we have 5 bits and keep 2
            vchRet.push_back((left << 3) | (dec >> 2));
            left = dec & 3;
            mode = 2;
            break;

        case 2: // we have 2 bits and keep 7
            left = left << 5 | dec;
            mode = 3;
            break;

        case 3: // we have 7 bits and keep 4
            vchRet.push_back((left << 1) | (dec >> 4));
            left = dec & 15;
            mode = 4;
            break;

        case 4: // we have 4 bits, and keep 1
            vchRet.push_back((left << 4) | (dec >> 1));
            left = dec & 1;
            mode = 5;
            break;

        case 5: // we have 1 bit, and keep 6
            left = left << 5 | dec;
            mode = 6;
            break;

        case 6: // we have 6 bits, and keep 3
            vchRet.push_back((left << 2) | (dec >> 3));
            left = dec & 7;
            mode = 7;
            break;

        case 7: // we have 3 bits, and keep 0
            vchRet.push_back((left << 5) | dec);
            mode = 0;
            break;
        }
    }

    if (pfInvalid)
        switch (mode) {
        case 0: // 8n base32 characters processed: ok
            break;

        case 1: // 8n+1 base32 characters processed: impossible
        case 3: //   +3
        case 6: //   +6
            *pfInvalid = true;
            break;

        case 2: // 8n+2 base32 characters processed: require '======'
            if (left || p[0] != '=' || p[1] != '=' || p[2] != '=' || p[3] != '=' || p[4] != '=' ||
                p[5] != '=' || decode32_table[(unsigned char)p[6]] != -1)
                *pfInvalid = true;
            break;

        case 4: // 8n+4 base32 characters processed: require '===='
            if (left || p[0] != '=' || p[1] != '=' || p[2] != '=' || p[3] != '=' ||
                decode32_table[(unsigned char)p[4]] != -1)
                *pfInvalid = true;
            break;

        case 5: // 8n+5 base32 characters processed: require '==='
            if (left || p[0] != '=' || p[1] != '=' || p[2] != '=' ||
                decode32_table[(unsigned char)p[3]] != -1)
                *pfInvalid = true;
            break;

        case 7: // 8n+7 base32 characters processed: require '='
            if (left || p[0] != '=' || decode32_table[(unsigned char)p[1]] != -1)
                *pfInvalid = true;
            break;
        }

    return vchRet;
}

string DecodeBase32(const string& str)
{
    vector<unsigned char> vchRet = DecodeBase32(str.c_str());
    return string((const char*)&vchRet[0], vchRet.size());
}

bool WildcardMatch(const char* psz, const char* mask)
{
    while (true) {
        switch (*mask) {
        case '\0':
            return (*psz == '\0');
        case '*':
            return WildcardMatch(psz, mask + 1) || (*psz && WildcardMatch(psz + 1, mask));
        case '?':
            if (*psz == '\0')
                return false;
            break;
        default:
            if (*psz != *mask)
                return false;
            break;
        }
        psz++;
        mask++;
    }
}

bool WildcardMatch(const string& str, const string& mask)
{
    return WildcardMatch(str.c_str(), mask.c_str());
}

static std::string FormatException(std::exception* pex, const char* pszThread)
{
#ifdef WIN32
    char pszModule[MAX_PATH] = "";
    GetModuleFileNameA(NULL, pszModule, sizeof(pszModule));
#else
    const char* pszModule = "neblio";
#endif
    if (pex)
        return strprintf("EXCEPTION: %s       \n%s       \n%s in %s       \n", typeid(*pex).name(),
                         pex->what(), pszModule, pszThread);
    else
        return strprintf("UNKNOWN EXCEPTION       \n%s in %s       \n", pszModule, pszThread);
}

void PrintException(std::exception* pex, const char* pszThread)
{
    std::string message = FormatException(pex, pszThread);
    printf("\n\n************************\n%s\n", message.c_str());
    fprintf(stderr, "\n\n************************\n%s\n", message.c_str());
    strMiscWarning = message;
    throw;
}

void PrintExceptionContinue(std::exception* pex, const char* pszThread)
{
    std::string message = FormatException(pex, pszThread);
    printf("\n\n************************\n%s\n", message.c_str());
    fprintf(stderr, "\n\n************************\n%s\n", message.c_str());
    strMiscWarning = message;
}

boost::filesystem::path GetDefaultDataDir()
{
    namespace fs = boost::filesystem;
    // Windows < Vista: C:\Documents and Settings\Username\Application Data\neblio
    // Windows >= Vista: C:\Users\Username\AppData\Roaming\neblio
    // Mac: ~/Library/Application Support/neblio
    // Unix: ~/.neblio
#ifdef WIN32
    // Windows
    return GetSpecialFolderPath(CSIDL_APPDATA) / "neblio";
#else
    fs::path pathRet;
    char* pszHome = getenv("HOME");
    if (pszHome == NULL || strlen(pszHome) == 0)
        pathRet = fs::path("/");
    else
        pathRet = fs::path(pszHome);
#ifdef MAC_OSX
    // Mac
    pathRet /= "Library/Application Support";
    fs::create_directory(pathRet);
    return pathRet / "neblio";
#else
    // Unix
    return pathRet / ".neblio";
#endif
#endif
}

const boost::filesystem::path& GetDataDir(bool fNetSpecific)
{
    namespace fs = boost::filesystem;

    static fs::path         pathCached[2];
    static CCriticalSection csPathCached;
    static bool             cachedPath[2] = {false, false};

    fs::path& path = pathCached[fNetSpecific];

    // This can be called during exceptions by printf, so we cache the
    // value so we don't have to do memory allocations after that.
    if (cachedPath[fNetSpecific])
        return path;

    LOCK(csPathCached);

    std::string datadirVal;
    bool        datadirExists = mapArgs.get("-datadir", datadirVal);
    if (datadirExists) {
        path = fs::system_complete(datadirVal);
        if (!fs::is_directory(path)) {
            path = "";
            return path;
        }
    } else {
        path = GetDefaultDataDir();
    }
    if (fNetSpecific && GetBoolArg("-testnet", false))
        path /= "testnet";

    fs::create_directories(path);

    cachedPath[fNetSpecific] = true;
    return path;
}

boost::filesystem::path GetConfigFile()
{
    boost::filesystem::path pathConfigFile(GetArg("-conf", "neblio.conf"));
    if (!pathConfigFile.is_complete())
        pathConfigFile = GetDataDir(false) / pathConfigFile;
    return pathConfigFile;
}

void ReadConfigFile(ThreadSafeHashMap<string, string>&         mapSettingsRet,
                    ThreadSafeHashMap<string, vector<string>>& mapMultiSettingsRet)
{
    boost::filesystem::ifstream streamConfig(GetConfigFile());
    if (!streamConfig.good())
        return; // No bitcoin.conf file is OK

    set<string> setOptions;
    setOptions.insert("*");

    for (boost::program_options::detail::config_file_iterator it(streamConfig, setOptions), end;
         it != end; ++it) {
        // Don't overwrite existing settings so command line settings override bitcoin.conf
        string strKey = string("-") + it->string_key;
        if (mapSettingsRet.exists(strKey) == 0) {
            mapSettingsRet.set(strKey, it->value[0]);
            // interpret nofoo=1 as foo=0 (and nofoo=0 as foo=1) as long as foo not set)
            InterpretNegativeSetting(strKey, mapSettingsRet);
        }
        // set the new values for the key strKey by loading the current value, modify it, and write it
        // back
        std::vector<std::string> multimapValVec;
        mapMultiSettingsRet.get(strKey, multimapValVec);
        multimapValVec.push_back(it->value[0]);
        mapMultiSettingsRet.set(strKey, multimapValVec);
    }
}

boost::filesystem::path GetPidFile()
{
    boost::filesystem::path pathPidFile(GetArg("-pid", "nebliod.pid"));
    if (!pathPidFile.is_complete())
        pathPidFile = GetDataDir() / pathPidFile;
    return pathPidFile;
}

#ifndef WIN32
void CreatePidFile(const boost::filesystem::path& path, pid_t pid)
{
    FILE* file = fopen(path.string().c_str(), "w");
    if (file) {
        fprintf(file, "%d\n", pid);
        fclose(file);
    }
}
#endif

bool RenameOver(boost::filesystem::path src, boost::filesystem::path dest)
{
#ifdef WIN32
    return MoveFileExA(src.string().c_str(), dest.string().c_str(), MOVEFILE_REPLACE_EXISTING);
#else
    int rc = std::rename(src.string().c_str(), dest.string().c_str());
    return (rc == 0);
#endif /* WIN32 */
}

void FileCommit(FILE* fileout)
{
    fflush(fileout); // harmless if redundantly called
#ifdef WIN32
    _commit(_fileno(fileout));
#else
#if defined(__linux__) || defined(__NetBSD__)
    fdatasync(fileno(fileout));
#else
    fsync(fileno(fileout));
#endif
#endif
}

void ShrinkDebugFile()
{
    // Scroll debug.log if it's getting too big
    boost::filesystem::path pathLog = GetDataDir() / "debug.log";
    FILE*                   file    = fopen(pathLog.string().c_str(), "r");
    if (file && boost::filesystem::file_size(pathLog) > 10 * 1000000) {
        // Restart the file with some of the end
        char pch[200000];
        fseek(file, -sizeof(pch), SEEK_END);
        int nBytes = fread(pch, 1, sizeof(pch), file);
        fclose(file);

        file = fopen(pathLog.string().c_str(), "w");
        if (file) {
            fwrite(pch, 1, nBytes, file);
            fclose(file);
        }
    }
}

//
// "Never go to sea with two chronometers; take one or three."
// Our three time sources are:
//  - System clock
//  - Median of other nodes clocks
//  - The user (asking the user to fix the system clock if the first two disagree)
//
static int64_t nMockTime = 0; // For unit testing

int64_t GetTime()
{
    if (nMockTime)
        return nMockTime;

    return time(NULL);
}

void SetMockTime(int64_t nMockTimeIn) { nMockTime = nMockTimeIn; }

static boost::atomic<int64_t> nTimeOffset{0};

int64_t GetTimeOffset() { return nTimeOffset; }

int64_t GetAdjustedTime() { return GetTime() + GetTimeOffset(); }

void AddTimeData(const CNetAddr& ip, int64_t nTime)
{
    int64_t nOffsetSample = nTime - GetTime();

    // Ignore duplicates
    static set<CNetAddr> setKnown;
    if (!setKnown.insert(ip).second)
        return;

    // Add data
    vTimeOffsets.input(nOffsetSample);
    printf("Added time data, samples %d, offset %+" PRId64 " (%+" PRId64 " minutes)\n",
           vTimeOffsets.size(), nOffsetSample, nOffsetSample / 60);
    if (vTimeOffsets.size() >= 5 && vTimeOffsets.size() % 2 == 1) {
        int64_t              nMedian = vTimeOffsets.median();
        std::vector<int64_t> vSorted = vTimeOffsets.sorted();
        // Only let other nodes change our time by so much
        if (abs64(nMedian) < 70 * 60) {
            nTimeOffset = nMedian;
        } else {
            nTimeOffset = 0;

            static bool fDone;
            if (!fDone) {
                // If nobody has a time different than ours but within 5 minutes of ours, give a warning
                bool fMatch = false;
                BOOST_FOREACH (int64_t nOffset, vSorted)
                    if (nOffset != 0 && abs64(nOffset) < 5 * 60)
                        fMatch = true;

                if (!fMatch) {
                    fDone = true;
                    string strMessage =
                        _("Warning: Please check that your computer's date and time are correct! If "
                          "your clock is wrong neblio will not work properly.");
                    strMiscWarning = strMessage;
                    printf("*** %s\n", strMessage.c_str());
                    uiInterface.ThreadSafeMessageBox(strMessage + " ", string("neblio"),
                                                     CClientUIInterface::OK |
                                                         CClientUIInterface::ICON_EXCLAMATION);
                }
            }
        }
        if (fDebug) {
            BOOST_FOREACH (int64_t n, vSorted)
                printf("%+" PRId64 "  ", n);
            printf("|  ");
        }
        printf("nTimeOffset = %+" PRId64 "  (%+" PRId64 " minutes)\n", nTimeOffset.load(),
               nTimeOffset.load() / 60);
    }
}

uint32_t insecure_rand_Rz = 11;
uint32_t insecure_rand_Rw = 11;
void     seed_insecure_rand(bool fDeterministic)
{
    // The seed values have some unlikely fixed points which we avoid.
    if (fDeterministic) {
        insecure_rand_Rz = insecure_rand_Rw = 11;
    } else {
        uint32_t tmp;
        do {
            RAND_bytes((unsigned char*)&tmp, 4);
        } while (tmp == 0 || tmp == 0x9068ffffU);
        insecure_rand_Rz = tmp;
        do {
            RAND_bytes((unsigned char*)&tmp, 4);
        } while (tmp == 0 || tmp == 0x464fffffU);
        insecure_rand_Rw = tmp;
    }
}

string FormatVersion(int nVersion)
{
    if (nVersion % 100 == 0)
        return strprintf("%d.%d.%d", nVersion / 1000000, (nVersion / 10000) % 100,
                         (nVersion / 100) % 100);
    else
        return strprintf("%d.%d.%d.%d", nVersion / 1000000, (nVersion / 10000) % 100,
                         (nVersion / 100) % 100, nVersion % 100);
}

string FormatFullVersion() { return CLIENT_BUILD; }

// Format the subversion field according to BIP 14 spec (https://en.bitcoin.it/wiki/BIP_0014)
std::string FormatSubVersion(const std::string& name, int nClientVersion,
                             const std::vector<std::string>& comments)
{
    std::ostringstream ss;
    ss << "/";
    ss << name << ":" << FormatVersion(nClientVersion);
    if (!comments.empty())
        ss << "(" << boost::algorithm::join(comments, "; ") << ")";
    ss << "/";
    return ss.str();
}

#ifdef WIN32
boost::filesystem::path GetSpecialFolderPath(int nFolder, bool fCreate)
{
    namespace fs = boost::filesystem;

    char pszPath[MAX_PATH] = "";

    if (SHGetSpecialFolderPathA(NULL, pszPath, nFolder, fCreate)) {
        return fs::path(pszPath);
    }

    printf("SHGetSpecialFolderPathA() failed, could not obtain requested path.\n");
    return fs::path("");
}
#endif

void runCommand(std::string strCommand)
{
    int nErr = ::system(strCommand.c_str());
    if (nErr)
        printf("runCommand error: system(%s) returned %d\n", strCommand.c_str(), nErr);
}

void RenameThread(const char* name)
{
#if defined(PR_SET_NAME)
    // Only the first 15 characters are used (16 - NUL terminator)
    ::prctl(PR_SET_NAME, name, 0, 0, 0);
#elif 0 && (defined(__FreeBSD__) || defined(__OpenBSD__))
    // TODO: This is currently disabled because it needs to be verified to work
    //       on FreeBSD or OpenBSD first. When verified the '0 &&' part can be
    //       removed.
    pthread_set_name_np(pthread_self(), name);

    // This is XCode 10.6-and-later; bring back if we drop 10.5 support:
    // #elif defined(MAC_OSX)
    //    pthread_setname_np(name);

#else
    // Prevent warnings for unused parameters...
    (void)name;
#endif
}

bool NewThread(void (*pfn)(void*), void* parg)
{
    try {
        boost::thread(pfn, parg); // thread detaches when out of scope
    } catch (boost::thread_resource_error& e) {
        printf("Error creating thread: %s\n", e.what());
        return false;
    }
    return true;
}

string GeneratePseudoRandomString(const int len)
{
    static const char alphanum[] = "0123456789"
                                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   "abcdefghijklmnopqrstuvwxyz";

    std::string s;
    s.resize(len);

    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    return s;
}

string GeneratePseudoRandomHex(const int len)
{
    static const char alphanum[] = "0123456789ABCDEF";

    std::string s;
    s.resize(len);

    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    return s;
}

std::string ZlibCompress(const std::string& data)
{
    boost::iostreams::filtering_istream in;
    in.push(boost::iostreams::zlib_compressor());
    in.push(boost::iostreams::array_source(&*data.begin(), &*data.end()));
    std::string res;
    boost::iostreams::copy(in, std::back_inserter(res));
    return res;
}

std::string ZlibDecompress(const std::string& compressedString)
{
    boost::iostreams::filtering_istream in;
    in.push(boost::iostreams::zlib_decompressor());
    in.push(boost::iostreams::array_source(&*compressedString.begin(), &*compressedString.end()));
    std::string res;
    boost::iostreams::copy(in, std::back_inserter(res));
    return res;
}

std::uintmax_t GetFreeDiskSpace(const boost::filesystem::path& path)
{
    static_assert(sizeof(std::uintmax_t) >= 8,
                  "Max integers size must be at least 8 bytes to store diskspace size correctly");
    return boost::filesystem::space(path).free;
}

static const std::string RESTART_SCHEDULED_PREFIX = ".scheduled.";

/**
 * A scheduled operation on restart is an operation that should be done when the program is restarted.
 * The scheduling is done by putting a file in the data directory, and looking for it when the program
 * starts. The benefit of this is to simplify the work for beginners and not have them bother with
 * command line arguments
 */
bool SC_CreateScheduledOperationOnRestart(const std::string& OpName)
{
    using PathType      = boost::filesystem::path;
    PathType opFilePath = SC_GetScheduledOperationFileName(OpName);
    // check if the operation file already exist
    if (boost::filesystem::exists(opFilePath)) {
        printf("Operation %s is already scheduled\n", OpName.c_str());
        return true;
    }
    boost::filesystem::ofstream of(opFilePath, std::ios::binary);
    of.write("1", 1); // avoid empty file, so write "1" to the file
    of.close();
    if (boost::filesystem::exists(opFilePath)) {
        printf("Operation %s has been successfully scheduled", OpName.c_str());
        return true;
    } else {
        throw std::runtime_error("Failed to schedule operation: " + OpName +
                                 "; it looks like the data directory is not writable");
    }
}

std::unordered_set<string> SC_GetScheduledOperationsOnRestart()
{
    using PathType = boost::filesystem::path;
    std::unordered_set<std::string> result;
    PathType                        dir = GetDataDir();
    if (boost::filesystem::is_directory(dir.c_str())) {
        result.clear();
        boost::filesystem::path               someDir(dir);
        boost::filesystem::directory_iterator end_iter;

        for (boost::filesystem::directory_iterator dir_iter(someDir); dir_iter != end_iter; ++dir_iter) {
            if (boost::filesystem::is_regular_file(dir_iter->status())) {
                std::string filename = dir_iter->path().filename().string();
                if (boost::algorithm::starts_with(filename, RESTART_SCHEDULED_PREFIX)) {
                    std::string opName(filename.cbegin() + RESTART_SCHEDULED_PREFIX.size(),
                                       filename.cend());
                    if (opName.size() > 0) {
                        result.insert(opName);
                    }
                }
            }
        }
    }
    return result;
}

bool SC_IsOperationOnRestartScheduled(const std::string& OpName)
{
    std::unordered_set<std::string> ops = SC_GetScheduledOperationsOnRestart();
    return ops.find(OpName) != ops.cend();
}

bool SC_DeleteOperationScheduledOnRestart(const std::string& OpName)
{
    using PathType                       = boost::filesystem::path;
    PathType                  opFilePath = SC_GetScheduledOperationFileName(OpName);
    boost::system::error_code ec1;
    if (boost::filesystem::exists(opFilePath, ec1)) {
        boost::system::error_code ec2;
        if (boost::filesystem::remove(opFilePath, ec2)) {
            return true;
        } else {
            printf("Error while removing scheduled operation on restart. OpFile: %s; Error: %s\n",
                   opFilePath.string().c_str(), ec2.message().c_str());
            return false;
        }
    } else {
        printf("Requested to remove operation \"%s\", which is not scheduled\n",
               opFilePath.string().c_str());
        return false;
    }
}

boost::filesystem::path SC_GetScheduledOperationFileName(const string& OpName)
{
    return GetDataDir() / (RESTART_SCHEDULED_PREFIX + OpName);
}

bool SC_CheckOperationOnRestartScheduleThenDeleteIt(const string& OpName)
{
    bool opExists = SC_IsOperationOnRestartScheduled(OpName);
    if (opExists) {
        bool deleteSuccess = SC_DeleteOperationScheduledOnRestart(OpName);
        if (!deleteSuccess) {
            printf("Failed to delete operation \"%s\"", OpName.c_str());
        }
        return true;
    } else {
        return false;
    }
}

string GetMimeTypeFromPath(const string& path)
{

    using boost::algorithm::iequals;
    const std::string ext = [&path]() -> std::string {
        auto const pos = path.rfind(".");
        if (pos == std::string::npos)
            return "application/unknown";
        return path.substr(pos);
    }();

    if (iequals(ext, ".htm"))
        return "text/html";
    if (iequals(ext, ".html"))
        return "text/html";
    if (iequals(ext, ".php"))
        return "text/html";
    if (iequals(ext, ".css"))
        return "text/css";
    if (iequals(ext, ".txt"))
        return "text/plain";
    if (iequals(ext, ".js"))
        return "application/javascript";
    if (iequals(ext, ".json"))
        return "application/json";
    if (iequals(ext, ".xml"))
        return "application/xml";
    if (iequals(ext, ".swf"))
        return "application/x-shockwave-flash";
    if (iequals(ext, ".flv"))
        return "video/x-flv";
    if (iequals(ext, ".png"))
        return "image/png";
    if (iequals(ext, ".jpe"))
        return "image/jpeg";
    if (iequals(ext, ".jpeg"))
        return "image/jpeg";
    if (iequals(ext, ".jpg"))
        return "image/jpeg";
    if (iequals(ext, ".gif"))
        return "image/gif";
    if (iequals(ext, ".bmp"))
        return "image/bmp";
    if (iequals(ext, ".ico"))
        return "image/vnd.microsoft.icon";
    if (iequals(ext, ".tiff"))
        return "image/tiff";
    if (iequals(ext, ".tif"))
        return "image/tiff";
    if (iequals(ext, ".svg"))
        return "image/svg+xml";
    if (iequals(ext, ".svgz"))
        return "image/svg+xml";
    return "application/text";
}
