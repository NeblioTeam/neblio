#ifndef GLOBALS_H
#define GLOBALS_H

#include "amount.h"
#include "chainparams.h"
#include "sync.h"
#include "uint256.h"
#include <boost/atomic.hpp>
#include <boost/shared_ptr.hpp>
#include <map>

class CTxMemPool;
class CBlockIndex;

using CBlockIndexSmartPtr      = boost::shared_ptr<CBlockIndex>;
using ConstCBlockIndexSmartPtr = boost::shared_ptr<const CBlockIndex>;
using BlockIndexMapType        = std::map<uint256, CBlockIndexSmartPtr>;

extern CTxMemPool              mempool;
extern boost::atomic<uint32_t> nTransactionsUpdated;

extern CCriticalSection    cs_main;
extern BlockIndexMapType   mapBlockIndex;
extern CBlockIndexSmartPtr pindexBest;
extern CBlockIndexSmartPtr pindexGenesisBlock;

extern bool               fUseFastIndex;
extern boost::atomic<int> nBestHeight;

extern boost::atomic<uint256> nBestChainTrust;
extern boost::atomic<uint256> nBestInvalidTrust;
extern boost::atomic<uint256> hashBestChain;
extern boost::atomic<int64_t> nTimeBestReceived;

/** The maximum allowed size for a serialized block, in bytes (network rule) */
static const unsigned int MAX_BLOCK_SIZE     = 8000000;
static const unsigned int OLD_MAX_BLOCK_SIZE = 1000000;
/** The maximum size for transactions we're willing to relay/mine **/
static const unsigned int MAX_STANDARD_TX_SIZE = OLD_MAX_BLOCK_SIZE / 5;
/** The maximum allowed number of signature check operations in a block (network rule) */
static const unsigned int MAX_BLOCK_SIGOPS = OLD_MAX_BLOCK_SIZE / 50;
/** Default for -maxorphantx, maximum number of orphan transactions kept in memory */
static const unsigned int DEFAULT_MAX_ORPHAN_TRANSACTIONS = 100;
/** The maximum number of entries in an 'inv' protocol message */
static const unsigned int MAX_INV_SZ = 50000;
/** Default for -maxorphanblocks, maximum number of orphan blocks kept in memory */
static const unsigned int DEFAULT_MAX_ORPHAN_BLOCKS = 750;
/** Fees smaller than this (in satoshi) are considered zero fee (for relaying) */
static const int64_t MIN_RELAY_TX_FEE = MIN_TX_FEE;
/** Threshold for nLockTime: below this value it is interpreted as block number, otherwise as UNIX
 * timestamp. */
static const unsigned int LOCKTIME_THRESHOLD = 500000000; // Tue Nov  5 00:53:20 1985 UTC
/** Maximum length of reject messages. */
static const unsigned int MAX_REJECT_MESSAGE_LENGTH = 111;
/** Maximum length of the user agent string in `version` message */
static const unsigned int MAX_SUBVERSION_LENGTH = 256;

static const int64_t COIN_YEAR_REWARD = 10 * CENT; // 10%

/** The maximum allowed Peer Protocol Version */
static const unsigned int MIN_PEER_PROTO_VERSION     = 60303; // v3.0.3+
static const unsigned int OLD_MIN_PEER_PROTO_VERSION = 60210; // v2.1+

extern boost::atomic<int64_t> NodeIDCounter;

/** Subversion as sent to the P2P network in `version` messages */
extern std::string strSubVersion;

namespace Checkpoints {
/** Checkpointing mode */
enum CPMode
{
    // Scrict checkpoints policy, perform conflicts verification and resolve conflicts
    CPMode_STRICT = 0,
    // Advisory checkpoints policy, perform conflicts verification but don't try to resolve them
    CPMode_ADVISORY = 1,
    // Permissive checkpoints policy, don't perform any checking
    CPMode_PERMISSIVE = 2
};
} // namespace Checkpoints

extern Checkpoints::CPMode CheckpointsMode;

/** Used by SanitizeString() */
enum SafeChars
{
    SAFE_CHARS_DEFAULT,    //!< The full set of allowed chars
    SAFE_CHARS_UA_COMMENT, //!< BIP-0014 subset
    SAFE_CHARS_FILENAME,   //!< Chars allowed in filenames
    SAFE_CHARS_URI,        //!< Chars allowed in URIs (RFC 3986)
};

static const std::string CHARS_ALPHA_NUM =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

static const std::string SAFE_CHARS[] = {
    CHARS_ALPHA_NUM + " .,;-_/:?@()",            // SAFE_CHARS_DEFAULT
    CHARS_ALPHA_NUM + " .,;-_?@",                // SAFE_CHARS_UA_COMMENT
    CHARS_ALPHA_NUM + ".-_",                     // SAFE_CHARS_FILENAME
    CHARS_ALPHA_NUM + "!*'();:@&=+$,/?#[]-_.~%", // SAFE_CHARS_URI
};

std::string SanitizeString(const std::string& str, int rule);

void SetGlobalBestChainParameters(const CBlockIndexSmartPtr pindex, bool updateCountersAndTimes = false);

#endif // GLOBALS_H
