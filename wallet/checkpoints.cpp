// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/assign/list_of.hpp> // for 'map_list_of()'
#include <boost/foreach.hpp>

#include "chainparams.h"
#include "checkpoints.h"

#include "main.h"
#include "txdb.h"
#include "uint256.h"

namespace Checkpoints {

boost::atomic_int64_t BlockToCheckpointCache::counter{0};

BlockToCheckpointCache g_CheckpointsCache;
bool                   CheckHardened(int nHeight, const uint256& hash)
{
    const MapCheckpoints& checkpoints = Params().Checkpoints();

    auto it = checkpoints.find(nHeight);
    if (it == checkpoints.cend())
        return true;
    return hash == it->second;
}

int GetTotalBlocksEstimate()
{
    const MapCheckpoints& checkpoints = Params().Checkpoints();

    if (checkpoints.empty()) {
        return checkpoints.rbegin()->first;
    } else {
        return 0;
    }
}

CBlockIndex* GetLastCheckpoint(const std::map<uint256, CBlockIndexSmartPtr>& mapBlockIndex)
{
    const MapCheckpoints& checkpoints = Params().Checkpoints();

    BOOST_REVERSE_FOREACH(const MapCheckpoints::value_type& i, checkpoints)
    {
        const uint256&                                         hash = i.second;
        std::map<uint256, CBlockIndexSmartPtr>::const_iterator it   = mapBlockIndex.find(hash);
        if (it != mapBlockIndex.end())
            return boost::atomic_load(&it->second).get();
    }
    return nullptr;
}

// Check against synchronized checkpoint
bool CheckSync(const uint256& blockHash, const CBlockIndex* pindexPrev, bool enableCaching,
               const MapCheckpoints& checkpoints, BlockToCheckpointCache& checkpointsCache)
{
    // if checkpoints are empty, then everything is valid
    if (checkpoints.empty()) {
        return true;
    }

    const int nBlockHeight = pindexPrev->nHeight + 1;

    // get the last relevant checkpoint
    auto checkpointIt = checkpoints.find(nBlockHeight);
    if (checkpointIt == checkpoints.cend()) {
        checkpointIt = checkpoints.lower_bound(nBlockHeight);
        if (checkpointIt != checkpoints.cbegin()) {
            --checkpointIt;
        }
    }

    const int highestRelevantCheckpoint = checkpointIt->first;

    if (checkpointIt->first == nBlockHeight) {
        return checkpointIt->second == blockHash;
    } else {
        const CBlockIndex* pindex = pindexPrev;
        while (pindex) {
            if (enableCaching) {
                const auto cp = checkpointsCache.get(pindex->GetBlockHash());
                if (cp.is_initialized()) {
                    checkpointsCache.add(cp->cachedCheckpoint, blockHash);
                    return cp->cachedCheckpoint == checkpointIt->second;
                }
            }
            if (pindex->nHeight == highestRelevantCheckpoint) {
                checkpointsCache.add(checkpointIt->second, blockHash);
                return pindex->GetBlockHash() == checkpointIt->second;
            }
            pindex = boost::atomic_load(&pindex->pprev).get();
        }
    }
    return false;
}

int64_t GetLastCheckpointBlockHeight()
{
    const MapCheckpoints& checkpoints = Params().Checkpoints();

    if (checkpoints.empty()) {
        return checkpoints.rbegin()->first;
    } else {
        return 0;
    }
}

} // namespace Checkpoints
