// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_CHECKPOINT_H
#define BITCOIN_CHECKPOINT_H

#include "globals.h"
#include "net.h"
#include "util.h"
#include <map>

#include <boost/multi_array/index_gen.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>

class uint256;
class CBlockIndex;
class CSyncCheckpoint;

namespace Checkpoints {

struct IDTag
{
};

struct HashTag
{
};

struct CheckpointCacheElement
{
    uint256 cachedCheckpoint;
    uint256 blockHash;
    int64_t id;
};

class BlockToCheckpointCache
{
    static boost::atomic_int64_t counter;
    using CheckpointCacheContainer = boost::multi_index_container<
        CheckpointCacheElement,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_unique<boost::multi_index::tag<IDTag>,
                                               BOOST_MULTI_INDEX_MEMBER(CheckpointCacheElement, int64_t,
                                                                        id)>,
            boost::multi_index::ordered_unique<boost::multi_index::tag<HashTag>,
                                               BOOST_MULTI_INDEX_MEMBER(CheckpointCacheElement, uint256,
                                                                        blockHash)>>>;

    mutable boost::shared_mutex mtx;
    CheckpointCacheContainer    cacheContainer;

    static const int MAX_SIZE = 100;

public:
    void pop_one_unsafe()
    {
        if (cacheContainer.size() <= MAX_SIZE) {
            return;
        }
        auto& byID = cacheContainer.get<IDTag>();
        byID.erase(byID.begin());
    }

    void add(const uint256& CheckpointHash, const uint256& BlockHash)
    {
        CheckpointCacheElement cp;
        cp.id               = counter++;
        cp.blockHash        = BlockHash;
        cp.cachedCheckpoint = CheckpointHash;

        boost::unique_lock<boost::shared_mutex> lg(mtx);
        cacheContainer.insert(cp);
        if (cacheContainer.size() > MAX_SIZE) {
            pop_one_unsafe();
        }
    }

    boost::optional<CheckpointCacheElement> get(const uint256& BlockHash) const
    {
        boost::unique_lock<boost::shared_mutex> lg(mtx);

        const auto& byHash = cacheContainer.get<HashTag>();
        auto        it     = byHash.find(BlockHash);
        if (it == byHash.cend()) {
            return boost::none;
        }
        return boost::make_optional(*it);
    }

    void clear()
    {
        boost::unique_lock<boost::shared_mutex> lg(mtx);

        clear_unsafe();
    }

    void clear_unsafe() { cacheContainer.clear(); }

    std::size_t size() const
    {
        boost::unique_lock<boost::shared_mutex> lg(mtx);

        return cacheContainer.size();
    }

    std::size_t size_unsafe() const { return cacheContainer.size(); }
};

/**
 * This cache helps in finding the closest a block to a checkpoint in the past to avoid looping back
 */
extern BlockToCheckpointCache g_CheckpointsCache;
} // namespace Checkpoints

/** Block-chain checkpoints are compiled-in sanity checks.
 * They are updated every release or three.
 */
namespace Checkpoints {
// Returns true if block passes checkpoint checks
bool CheckHardened(int nHeight, const uint256& hash,
                   const MapCheckpoints& checkpoints = Params().Checkpoints());

// Return conservative estimate of total number of blocks, 0 if unknown
int GetTotalBlocksEstimate(const MapCheckpoints& checkpoints = Params().Checkpoints());

// Returns last boost::opional<CBlockIndex> in mapBlockIndex that is a checkpoint
boost::optional<CBlockIndex>
GetLastCheckpoint(const ITxDB& txdb, const MapCheckpoints& checkpoints = Params().Checkpoints());

CBlockIndex* GetLastSyncCheckpoint();
bool         CheckSync(const ITxDB& txdb, const uint256& blockHash, const CBlockIndex* pindexPrev,
                       bool enableCaching = true, const MapCheckpoints& checkpoints = Params().Checkpoints(),
                       BlockToCheckpointCache& checkpointsCache = g_CheckpointsCache);
int64_t      GetLastCheckpointBlockHeight();
} // namespace Checkpoints

#endif
