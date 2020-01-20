#ifndef GLOBALS_H
#define GLOBALS_H

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

static const int LAST_POW_BLOCK = 1000; // 1000 PoW Blocks to kickstart

static const int64_t COIN = 100000000;
static const int64_t CENT = 1000000;

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
/** Fees smaller than this (in satoshi) are considered zero fee (for transaction creation) */
static const int64_t MIN_TX_FEE = 10000;
/** Fees smaller than this (in satoshi) are considered zero fee (for relaying) */
static const int64_t MIN_RELAY_TX_FEE = MIN_TX_FEE;
/** Threshold for nLockTime: below this value it is interpreted as block number, otherwise as UNIX
 * timestamp. */
static const unsigned int LOCKTIME_THRESHOLD = 500000000; // Tue Nov  5 00:53:20 1985 UTC

static const int64_t COIN_YEAR_REWARD = 10 * CENT; // 10%

/** The maximum allowed OP_RETURN size in bytes (network rule) */
static const unsigned int MAX_DATA_SIZE     = 4096;
static const unsigned int OLD_MAX_DATA_SIZE = 80;

/** The maximum allowed Peer Protocol Version */
static const unsigned int MIN_PEER_PROTO_VERSION     = 60210; // v2.1+
static const unsigned int OLD_MIN_PEER_PROTO_VERSION = 60200; // v2.0+

static const uint256
    hashGenesisBlock("0x7286972be4dbc1463d256049b7471c252e6557e222cab9be73181d359cd28bcc");
static const uint256
    hashGenesisBlockTestNet("0x7286972be4dbc1463d256049b7471c252e6557e222cab9be73181d359cd28bcc");

/** No amount larger than this (in satoshi) is valid */
static const int64_t MAX_MONEY =
    std::numeric_limits<int64_t>::max(); // Total coin that will be released (~infinite)

inline bool MoneyRange(int64_t nValue) { return (nValue >= 0 && nValue <= MAX_MONEY); }

extern CBlockIndexSmartPtr pblockindexFBBHLast;

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

#endif // GLOBALS_H
