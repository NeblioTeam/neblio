// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_H
#define BITCOIN_UTIL_H

#include "uint256.h"

#ifndef WIN32
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#endif

#include <boost/atomic.hpp>
#include <boost/regex.hpp>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/make_shared.hpp>
#include <boost/thread.hpp>
#include <openssl/md5.h>
#include <openssl/ripemd.h>
#include <openssl/sha.h>

#include "ThreadSafeHashMap.h"
#include "netbase.h" // for AddTimeData

// to obtain PRId64 on some old systems
#define __STDC_FORMAT_MACROS 1

#include <inttypes.h>
#include <stdint.h>

static const std::uintmax_t ONE_KB = (static_cast<uint64_t>(1) << 10);
static const std::uintmax_t ONE_MB = (static_cast<uint64_t>(1) << 20);
static const std::uintmax_t ONE_GB = (static_cast<uint64_t>(1) << 30);

// option to erase the blockchain and resync
const std::string SC_SCHEDULE_ON_RESTART_OPNAME__RESYNC = "resync";
// option to rescan the wallet
const std::string SC_SCHEDULE_ON_RESTART_OPNAME__RESCAN = "rescan";

/* Milliseconds between model updates */
extern boost::atomic_int MODEL_UPDATE_DELAY;

#define BEGIN(a) ((char*)&(a))
#define END(a) ((char*)&((&(a))[1]))
#define UBEGIN(a) ((unsigned char*)&(a))
#define UEND(a) ((unsigned char*)&((&(a))[1]))
#define ARRAYLEN(array) (sizeof(array) / sizeof((array)[0]))

#define UVOIDBEGIN(a) ((void*)&(a))
#define CVOIDBEGIN(a) ((const void*)&(a))
#define UINTBEGIN(a) ((uint32_t*)&(a))
#define CUINTBEGIN(a) ((const uint32_t*)&(a))

#ifndef PRId64
#if defined(_MSC_VER) || defined(__MSVCRT__)
#define PRId64 "I64d"
#define PRIu64 "I64u"
#define PRIx64 "I64x"
#else
#define PRId64 "lld"
#define PRIu64 "llu"
#define PRIx64 "llx"
#endif
#endif

#ifndef THROW_WITH_STACKTRACE
#define THROW_WITH_STACKTRACE(exception)                                                                \
    {                                                                                                   \
        LogStackTrace();                                                                                \
        throw(exception);                                                                               \
    }
void LogStackTrace();
#endif

/* Format characters for (s)size_t and ptrdiff_t */
#if defined(_MSC_VER) || defined(__MSVCRT__)
/* (s)size_t and ptrdiff_t have the same size specifier in MSVC:
   http://msdn.microsoft.com/en-us/library/tcxf1dw6%28v=vs.100%29.aspx
 */
#define PRIszx "Ix"
#define PRIszu "Iu"
#define PRIszd "Id"
#define PRIpdx "Ix"
#define PRIpdu "Iu"
#define PRIpdd "Id"
#else /* C99 standard */
#define PRIszx "zx"
#define PRIszu "zu"
#define PRIszd "zd"
#define PRIpdx "tx"
#define PRIpdu "tu"
#define PRIpdd "td"
#endif

// This is needed because the foreach macro can't get over the comma in pair<t1, t2>
#define PAIRTYPE(t1, t2) std::pair<t1, t2>

// Align by increasing pointer, must have extra space at end of buffer
template <size_t nBytes, typename T>
T* alignup(T* p)
{
    union {
        T*     ptr;
        size_t n;
    } u;
    u.ptr = p;
    u.n   = (u.n + (nBytes - 1)) & ~(nBytes - 1);
    return u.ptr;
}

#ifdef WIN32
#define MSG_NOSIGNAL 0
#define MSG_DONTWAIT 0

#ifndef S_IRUSR
#define S_IRUSR 0400
#define S_IWUSR 0200
#endif
#else
#define MAX_PATH 1024
#endif

inline void MilliSleep(int64_t n)
{
#if BOOST_VERSION >= 105000
    boost::this_thread::sleep_for(boost::chrono::milliseconds(n));
#else
    boost::this_thread::sleep(boost::posix_time::milliseconds(n));
#endif
}

/* This GNU C extension enables the compiler to check the format string against the parameters provided.
 * X is the number of the "format string" parameter, and Y is the number of the first variadic parameter.
 * Parameters count from 1.
 */
#ifdef __GNUC__
#define ATTR_WARN_PRINTF(X, Y) __attribute__((format(printf, X, Y)))
#else
#define ATTR_WARN_PRINTF(X, Y)
#endif

extern ThreadSafeHashMap<std::string, std::string>              mapArgs;
extern ThreadSafeHashMap<std::string, std::vector<std::string>> mapMultiArgs;
extern bool                                                     fDebug;
extern bool                                                     fDebugNet;
extern bool                                                     fPrintToConsole;
extern bool                                                     fPrintToDebugger;
extern boost::atomic<bool>                                      fRequestShutdown;
extern bool                                                     fDaemon;
extern bool                                                     fServer;
extern bool                                                     fCommandLine;
extern std::string                                              strMiscWarning;
extern bool                                                     fTestNet;
extern bool                                                     fNoListen;
extern bool                                                     fLogTimestamps;
extern bool                                                     fReopenDebugLog;
extern boost::atomic<bool>                                      fShutdown;

const std::string NTP1WalletCacheFileName = "NTP1DataCacheV2.json";

void RandAddSeed();
void RandAddSeedPerfmon();
int  ATTR_WARN_PRINTF(1, 2) OutputDebugStringF(const char* pszFormat, ...);

/*
  Rationale for the real_strprintf / strprintf construction:
    It is not allowed to use va_start with a pass-by-reference argument.
    (C++ standard, 18.7, paragraph 3). Use a dummy argument to work around this, and use a
    macro to keep similar semantics.
*/

/** Overload strprintf for char*, so that GCC format type warnings can be given */
std::string ATTR_WARN_PRINTF(1, 3) real_strprintf(const char* format, int dummy, ...);
/** Overload strprintf for std::string, to be able to use it with _ (translation).
 * This will not support GCC format type warnings (-Wformat) so be careful.
 */
std::string real_strprintf(const std::string& format, int dummy, ...);
#define strprintf(format, ...) real_strprintf(format, 0, __VA_ARGS__)
std::string vstrprintf(const char* format, va_list ap);

bool ATTR_WARN_PRINTF(1, 2) error(const char* format, ...);

/* Redefine printf so that it directs output to debug.log
 *
 * Do this *after* defining the other printf-like functions, because otherwise the
 * __attribute__((format(printf,X,Y))) gets expanded to __attribute__((format(OutputDebugStringF,X,Y)))
 * which confuses gcc.
 */
#define printf OutputDebugStringF

void                           PrintException(std::exception* pex, const char* pszThread);
void                           PrintExceptionContinue(std::exception* pex, const char* pszThread);
void                           ParseString(const std::string& str, char c, std::vector<std::string>& v);
std::string                    FormatMoney(int64_t n, bool fPlus = false);
bool                           ParseMoney(const std::string& str, int64_t& nRet);
bool                           ParseMoney(const char* pszIn, int64_t& nRet);
std::vector<unsigned char>     ParseHex(const char* psz);
std::vector<unsigned char>     ParseHex(const std::string& str);
bool                           IsHex(const std::string& str);
std::vector<unsigned char>     DecodeBase64(const char* p, bool* pfInvalid = NULL);
std::string                    DecodeBase64(const std::string& str);
std::string                    EncodeBase64(const unsigned char* pch, size_t len);
std::string                    EncodeBase64(const std::string& str);
std::vector<unsigned char>     DecodeBase32(const char* p, bool* pfInvalid = NULL);
std::string                    DecodeBase32(const std::string& str);
std::string                    EncodeBase32(const unsigned char* pch, size_t len);
std::string                    EncodeBase32(const std::string& str);
void                           ParseParameters(int argc, const char* const argv[]);
bool                           WildcardMatch(const char* psz, const char* mask);
bool                           WildcardMatch(const std::string& str, const std::string& mask);
void                           FileCommit(FILE* fileout);
bool                           RenameOver(boost::filesystem::path src, boost::filesystem::path dest);
boost::filesystem::path        GetDefaultDataDir();
const boost::filesystem::path& GetDataDir(bool fNetSpecific = true);
boost::filesystem::path        GetConfigFile();
boost::filesystem::path        GetPidFile();
#ifndef WIN32
void CreatePidFile(const boost::filesystem::path& path, pid_t pid);
#endif
void ReadConfigFile(ThreadSafeHashMap<std::string, std::string>&              mapSettingsRet,
                    ThreadSafeHashMap<std::string, std::vector<std::string>>& mapMultiSettingsRet);
#ifdef WIN32
boost::filesystem::path GetSpecialFolderPath(int nFolder, bool fCreate = true);
#endif
void        ShrinkDebugFile();
int         GetRandInt(int nMax);
uint64_t    GetRand(uint64_t nMax);
uint256     GetRandHash();
int64_t     GetTime();
void        SetMockTime(int64_t nMockTimeIn);
int64_t     GetAdjustedTime();
int64_t     GetTimeOffset();
std::string FormatFullVersion();
std::string FormatSubVersion(const std::string& name, int nClientVersion,
                             const std::vector<std::string>& comments);
void        AddTimeData(const CNetAddr& ip, int64_t nTime);
void        runCommand(std::string strCommand);

std::string GetMimeTypeFromPath(const std::string& path);

template <typename T>
std::string ToHexString(T&& value, bool prepend_0x = true)
{
    std::ostringstream ss;
    ss << (prepend_0x ? "0x" : "") << std::hex << std::uppercase << value;
    return ss.str();
}

template <typename T, typename U>
typename std::enable_if<std::is_convertible<U, std::string>::value, T>::type FromHexString(U&& str)
{
    std::stringstream ss;
    ss << std::hex << str;
    T res;
    ss >> res;
    return res;
}

template <typename T>
std::string ToString(T&& value)
{
    std::stringstream sstr;

    sstr << value;
    return sstr.str();
}

template <typename T, typename U>
typename std::enable_if<std::is_convertible<U, std::string>::value, T>::type FromString(U&& str)
{
    std::stringstream ss;
    ss << str;
    T ret;
    ss >> ret;
    return ret;
}

template <typename T, typename U>
typename std::enable_if<!std::is_convertible<U, std::string>::value && std::is_same<T, NTP1Int>::value,
                        T>::type
FromString(U&& str)
{
    std::stringstream ss;
    ss << str;
    T ret;
    ss >> ret;
    return ret;
}

inline std::string i64tostr(int64_t n) { return strprintf("%" PRId64, n); }

inline std::string itostr(int n) { return strprintf("%d", n); }

inline int64_t atoi64(const char* psz)
{
#ifdef _MSC_VER
    return _atoi64(psz);
#else
    return strtoll(psz, NULL, 10);
#endif
}

inline int64_t atoi64(const std::string& str)
{
#ifdef _MSC_VER
    return _atoi64(str.c_str());
#else
    return strtoll(str.c_str(), NULL, 10);
#endif
}

inline int atoi(const std::string& str) { return atoi(str.c_str()); }

inline int roundint(double d) { return (int)(d > 0 ? d + 0.5 : d - 0.5); }

inline int64_t roundint64(double d) { return (int64_t)(d > 0 ? d + 0.5 : d - 0.5); }

inline int64_t abs64(int64_t n) { return (n >= 0 ? n : -n); }

inline std::string leftTrim(std::string src, char chr)
{
    std::string::size_type pos = src.find_first_not_of(chr, 0);

    if (pos > 0)
        src.erase(0, pos);

    return src;
}

template <typename T>
std::string HexStr(const T itbegin, const T itend, bool fSpaces = false)
{
    std::string       rv;
    static const char hexmap[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    rv.reserve((itend - itbegin) * 3);
    for (T it = itbegin; it < itend; ++it) {
        unsigned char val = (unsigned char)(*it);
        if (fSpaces && it != itbegin)
            rv.push_back(' ');
        rv.push_back(hexmap[val >> 4]);
        rv.push_back(hexmap[val & 15]);
    }

    return rv;
}

inline std::string HexStr(const std::vector<unsigned char>& vch, bool fSpaces = false)
{
    return HexStr(vch.begin(), vch.end(), fSpaces);
}

inline int64_t GetPerformanceCounter()
{
    int64_t nCounter = 0;
#ifdef WIN32
    QueryPerformanceCounter((LARGE_INTEGER*)&nCounter);
#else
    timeval t;
    gettimeofday(&t, NULL);
    nCounter = (int64_t)t.tv_sec * 1000000 + t.tv_usec;
#endif
    return nCounter;
}

inline int64_t GetTimeMillis()
{
    return (boost::posix_time::ptime(boost::posix_time::microsec_clock::universal_time()) -
            boost::posix_time::ptime(boost::gregorian::date(1970, 1, 1)))
        .total_milliseconds();
}

inline std::string DateTimeStrFormat(const char* pszFormat, int64_t nTime)
{
    time_t     n       = nTime;
    struct tm* ptmTime = gmtime(&n);
    char       pszTime[200];
    strftime(pszTime, sizeof(pszTime), pszFormat, ptmTime);
    return pszTime;
}

static const std::string strTimestampFormat = "%Y-%m-%d %H:%M:%S UTC";
inline std::string       DateTimeStrFormat(int64_t nTime)
{
    return DateTimeStrFormat(strTimestampFormat.c_str(), nTime);
}

template <typename T>
void skipspaces(T& it)
{
    while (isspace(*it))
        ++it;
}

inline bool IsSwitchChar(char c)
{
#ifdef WIN32
    return c == '-' || c == '/';
#else
    return c == '-';
#endif
}

/**
 * Return string argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (e.g. "1")
 * @return command-line argument or default value
 */
std::string GetArg(const std::string& strArg, const std::string& strDefault);

/**
 * Return integer argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (e.g. 1)
 * @return command-line argument (0 if invalid number) or default value
 */
int64_t GetArg(const std::string& strArg, int64_t nDefault);

/**
 * Return boolean argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (true or false)
 * @return command-line argument or default value
 */
bool GetBoolArg(const std::string& strArg, bool fDefault = false);

/**
 * Set an argument if it doesn't already have a value
 *
 * @param strArg Argument to set (e.g. "-foo")
 * @param strValue Value (e.g. "1")
 * @return true if argument gets set, false if it already had a value
 */
bool SoftSetArg(const std::string& strArg, const std::string& strValue);

/**
 * Set a boolean argument if it doesn't already have a value
 *
 * @param strArg Argument to set (e.g. "-foo")
 * @param fValue Value (e.g. false)
 * @return true if argument gets set, false if it already had a value
 */
bool SoftSetBoolArg(const std::string& strArg, bool fValue);

/**
 * MWC RNG of George Marsaglia
 * This is intended to be fast. It has a period of 2^59.3, though the
 * least significant 16 bits only have a period of about 2^30.1.
 *
 * @return random value
 */
extern uint32_t insecure_rand_Rz;
extern uint32_t insecure_rand_Rw;
inline uint32_t insecure_rand(void)
{
    insecure_rand_Rz = 36969 * (insecure_rand_Rz & 65535) + (insecure_rand_Rz >> 16);
    insecure_rand_Rw = 18000 * (insecure_rand_Rw & 65535) + (insecure_rand_Rw >> 16);
    return (insecure_rand_Rw << 16) + insecure_rand_Rz;
}

/**
 * Seed insecure_rand using the random pool.
 * @param Deterministic Use a determinstic seed
 */
void seed_insecure_rand(bool fDeterministic = false);

/**
 * Timing-attack-resistant comparison.
 * Takes time proportional to length
 * of first argument.
 */
template <typename T>
bool TimingResistantEqual(const T& a, const T& b)
{
    if (b.size() == 0)
        return a.size() == 0;
    size_t accumulator = a.size() ^ b.size();
    for (size_t i = 0; i < a.size(); i++)
        accumulator |= a[i] ^ b[i % b.size()];
    return accumulator == 0;
}

/** Median filter over a stream of values.
 * Returns the median of the last N numbers
 */
template <typename T>
class CMedianFilter
{
private:
    std::vector<T>       vValues;
    std::vector<T>       vSorted;
    unsigned int         nSize;
    mutable boost::mutex mtx;

public:
    CMedianFilter(unsigned int size, T initial_value)
    {
        boost::lock_guard<boost::mutex> lg(mtx);
        nSize = size;
        vValues.reserve(size);
        vValues.push_back(initial_value);
        vSorted = vValues;
    }

    void input(T value)
    {
        boost::lock_guard<boost::mutex> lg(mtx);
        if (vValues.size() == nSize) {
            vValues.erase(vValues.begin());
        }
        vValues.push_back(value);

        vSorted.resize(vValues.size());
        std::copy(vValues.begin(), vValues.end(), vSorted.begin());
        std::sort(vSorted.begin(), vSorted.end());
    }

    T median() const
    {
        boost::lock_guard<boost::mutex> lg(mtx);
        int                             size = vSorted.size();
        assert(size > 0);
        if (size & 1) // Odd number of elements
        {
            return vSorted[size / 2];
        } else // Even number of elements
        {
            return (vSorted[size / 2 - 1] + vSorted[size / 2]) / 2;
        }
    }

    int size() const
    {
        boost::lock_guard<boost::mutex> lg(mtx);
        return vValues.size();
    }

    std::vector<T> sorted() const
    {
        boost::lock_guard<boost::mutex> lg(mtx);
        return vSorted;
    }
};

bool NewThread(void (*pfn)(void*), void* parg);

#ifdef WIN32
inline void SetThreadPriority(int nPriority) { SetThreadPriority(GetCurrentThread(), nPriority); }
#else

#define THREAD_PRIORITY_LOWEST PRIO_MAX
#define THREAD_PRIORITY_BELOW_NORMAL 2
#define THREAD_PRIORITY_NORMAL 0
#define THREAD_PRIORITY_ABOVE_NORMAL 0

inline void SetThreadPriority(int nPriority)
{
// It's unclear if it's even possible to change thread priorities on Linux,
// but we really and truly need it for the generation threads.
#ifdef PRIO_THREAD
    setpriority(PRIO_THREAD, 0, nPriority);
#else
    setpriority(PRIO_PROCESS, 0, nPriority);
#endif
}

inline void ExitThread(size_t nExitCode) { pthread_exit((void*)nExitCode); }
#endif

void RenameThread(const char* name);

inline uint32_t ByteReverse(uint32_t value)
{
    value = ((value & 0xFF00FF00) >> 8) | ((value & 0x00FF00FF) << 8);
    return (value << 16) | (value >> 16);
}

std::string GeneratePseudoRandomString(const int len);

std::string GeneratePseudoRandomHex(const int len);

template <typename T>
void SwapEndianness(T& var)
{
    static_assert(std::is_pod<T>::value, "Type must be POD type for safety");
    std::array<char, sizeof(T)> varArray;
    std::memcpy(varArray.data(), &var, sizeof(T));
    for (int i = 0; i < static_cast<int>(sizeof(var) / 2); i++)
        std::swap(varArray[sizeof(var) - 1 - i], varArray[i]);
    std::memcpy(&var, varArray.data(), sizeof(T));
}

inline bool IsBigEndian()
{
#ifdef __BYTE_ORDER__
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return false;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return true;
#else
    static_assert(0, "Unsupported endianness");
#endif
#else
    static_assert(0, "Could not detect endianness");
#endif
}

inline bool IsLittleEndian()
{
#ifdef __BYTE_ORDER__
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return true;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return false;
#else
    static_assert(0, "Unsupported endianness");
#endif
#else
    static_assert(0, "Could not detect endianness");
#endif
}

template <typename T>
inline void MakeBigEndian(T& v)
{
    if (IsLittleEndian()) {
        SwapEndianness(v);
    } else if (IsBigEndian()) {
    } else {
        throw std::runtime_error("Unknown endianness");
    }
}

template <typename T>
inline void MakeLittleEndian(T& v)
{
    if (IsLittleEndian()) {
    } else if (IsBigEndian()) {
        SwapEndianness(v);
    } else {
        throw std::runtime_error("Unknown endianness");
    }
}

// Convert the endianness of v from Big Endian to this computer's endianness
template <typename T>
inline void FromBigEndianToThisEndianness(T& v)
{
    if (IsLittleEndian()) {
        SwapEndianness(v);
    } else if (IsBigEndian()) {
    } else {
        throw std::runtime_error("Unknown endianness");
    }
}

// Convert the endianness of v from Little Endian to this computer's endianness
template <typename T>
inline void FromLittleEndianToThisEndianness(T& v)
{
    if (IsLittleEndian()) {
    } else if (IsBigEndian()) {
        SwapEndianness(v);
    } else {
        throw std::runtime_error("Unknown endianness");
    }
}

std::string ZlibCompress(const std::string& data);
std::string ZlibDecompress(const std::string& compressedString);

template <typename T>
std::string ConvertToBitString(T num)
{
    static_assert(std::numeric_limits<T>::is_integer, "This function is only for integers");
    std::string res;
    bool        negative = num < 0;
    while (num != 0) {
        res.push_back((num & 1) == 1 ? '1' : '0');
        num = num >> 1;
    }
    if (negative) {
        res.push_back('-');
    }
    std::reverse(res.begin(), res.end());
    return res;
}

uintmax_t GetFreeDiskSpace(const boost::filesystem::path& path);

bool                    SC_DeleteOperationScheduledOnRestart(const std::string& OpName);
boost::filesystem::path SC_GetScheduledOperationFileName(const std::string& OpName);
bool                    SC_IsOperationOnRestartScheduled(const std::string& OpName);
bool                    SC_CheckOperationOnRestartScheduleThenDeleteIt(const std::string& OpName);
std::unordered_set<std::string> SC_GetScheduledOperationsOnRestart();
bool                            SC_CreateScheduledOperationOnRestart(const std::string& OpName);

template <typename... Ts>
void ignore_unused(Ts const&...)
{
}

template <typename... Ts>
void ignore_unused()
{
}

template <typename T, typename MutexType = boost::recursive_mutex>
class LockedVar
{
    T                 var;
    mutable MutexType mtx;

public:
    LockedVar(T&& Var)
    {
        boost::lock_guard<MutexType> lg(mtx);
        var = std::move(Var);
    }

    LockedVar() {}

    T& get()
    {
        boost::lock_guard<MutexType> lg(mtx);
        return var;
    }

    T& get_unsafe() { return var; }

    [[nodiscard]] boost::shared_ptr<boost::lock_guard<MutexType>> get_lock() const
    {
        return boost::make_shared<boost::lock_guard<MutexType>>(mtx);
    }

    [[nodiscard]] boost::shared_ptr<boost::unique_lock<MutexType>> get_try_lock() const
    {
        auto lock = boost::make_shared<boost::unique_lock<MutexType>>(mtx, boost::defer_lock);
        if (mtx.try_lock()) {
            return lock;
        } else {
            return nullptr;
        }
    }
};

#endif
