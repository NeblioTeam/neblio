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

bool CheckHardened(int nHeight, const uint256& hash, const MapCheckpoints& checkpoints)
{
    auto it = checkpoints.find(nHeight);
    if (it == checkpoints.cend())
        return true;
    return hash == it->second;
}

int GetTotalBlocksEstimate(const MapCheckpoints& checkpoints)
{
    if (!checkpoints.empty()) {
        return checkpoints.rbegin()->first;
    } else {
        return 0;
    }
}

boost::optional<CBlockIndex> GetLastCheckpoint(const ITxDB& txdb, const MapCheckpoints& checkpoints)
{
    if (checkpoints.empty()) {
        return boost::make_optional(*pindexGenesisBlock);
    }

    const boost::optional<int> bestHeight = txdb.GetBestChainHeight();
    if (!bestHeight) {
        NLog.write(b_sev::err, "Unable to retrieve best block height to retrieve the latest checkpoint");
        return boost::none;
    }
    auto it = checkpoints.lower_bound(*bestHeight);
    if (it == checkpoints.cend()) {
        assert(!checkpoints.empty() && "This function was supposed to assert the array is not empty");
        --it;
    } else if (it != checkpoints.cbegin() && it->first != *bestHeight) {
        // get the right checkpoint since lower_bound doesn't do what we need
        --it;
    }
    const uint256&                     blockHash  = it->second;
    const boost::optional<CBlockIndex> blockIndex = txdb.ReadBlockIndex(blockHash);
    std::string                        h          = blockHash.ToString();
    if (!blockIndex) {
        NLog.write(b_sev::err, "Failed to retrieve the block index at best block height {} and hash {}",
                   *bestHeight, blockHash.ToString());
        return boost::none;
    }
    return blockIndex;
}

// Check against synchronized checkpoint
bool CheckSync(const ITxDB& txdb, const uint256& blockHash, const CBlockIndex* pindexPrev,
               bool enableCaching, const MapCheckpoints& checkpoints,
               BlockToCheckpointCache& checkpointsCache)
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
        CBlockIndex index = *pindexPrev;
        while (true) {
            if (index.GetBlockHash() == Params().GenesisBlockHash()) {
                // this is the special case when nHeight == 1
                return true;
            }
            if (enableCaching) {
                const auto cp = checkpointsCache.get(index.GetBlockHash());
                if (cp.is_initialized()) {
                    checkpointsCache.add(cp->cachedCheckpoint, blockHash);
                    return cp->cachedCheckpoint == checkpointIt->second;
                }
            }
            if (index.nHeight == highestRelevantCheckpoint) {
                checkpointsCache.add(checkpointIt->second, blockHash);
                return index.GetBlockHash() == checkpointIt->second;
            }
            boost::optional<CBlockIndex> bi = index.getPrev(txdb);
            if (bi) {
                index = std::move(*bi);
            } else {
                NLog.write(b_sev::critical,
                           "CRITICAL ERROR: failed to get prev block for check point even though it's "
                           "not genesis. THIS SHOULD NEVER HAPPEN. Database broken?");
            }
        }
    }
    return false;
}

int64_t GetLastCheckpointBlockHeight()
{
    const MapCheckpoints& checkpoints = Params().Checkpoints();

    if (!checkpoints.empty()) {
        return checkpoints.rbegin()->first;
    } else {
        return 0;
    }
}

} // namespace Checkpoints
