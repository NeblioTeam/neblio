// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/assign/list_of.hpp> // for 'map_list_of()'
#include <boost/foreach.hpp>

#include "chainparams.h"
#include "checkpoints.h"

#include "main.h"
#include "txdb.h"
#include "ui_interface.h"
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

bool ValidateCheckpointsInDB(const ITxDB& txdb)
{
    const boost::optional<CBlockIndex>& bestBlockIndex  = txdb.GetBestBlockIndex();
    const uint256&                      bestBlockHash   = txdb.GetBestBlockHash();
    const boost::optional<int>&         bestChainHeight = txdb.GetBestChainHeight();

    if (!bestBlockIndex && bestBlockHash == 0 && !bestChainHeight) {
        // we're at genesis
        return true;
    }

    if (!bestBlockIndex) {
        NLog.write(b_sev::critical, "Block index is inconsistent; it doesn't exist in the DB, while "
                                    "block height and best block hash exist");
        return false;
    }

    if (bestBlockHash == 0) {
        NLog.write(b_sev::critical, "Best block hash is inconsistent; it doesn't exist in the DB, while "
                                    "best block index exists");
        return false;
    }

    if (!bestChainHeight) {
        NLog.write(
            b_sev::critical,
            "Block height is inconsistent; it doesn't exist in the DB, while best block index exists");
        return false;
    }

    if (bestBlockIndex->GetBlockHash() != bestBlockHash) {
        NLog.write(b_sev::critical, "Block index is inconsistent; best block hash stored in best block "
                                    "index doesn't match best block index in database");
        return false;
    }

    if (bestBlockIndex->nHeight != bestChainHeight) {
        NLog.write(b_sev::critical, "Block index is inconsistent; best block height doesn't match the "
                                    "value in the best block index");
        return false;
    }

    const std::size_t checkpointsMaxCount = Params().Checkpoints().size();
    std::size_t       currentIndex        = 0;
    for (const std::pair<int, uint256>& cp : Params().Checkpoints()) {
        const int      cpBlockHeight = cp.first;
        const uint256& cpBlockHash   = cp.second;

        uiInterface.InitMessage(
            fmt::format("Done Verifying latest blocks {}/{}", currentIndex, checkpointsMaxCount));
        NLog.write(b_sev::info, "Done Verifying latest blocks {}/{}", currentIndex, checkpointsMaxCount);
        currentIndex++;

        if (cpBlockHeight > bestChainHeight) {
            // we're still syncing, we don't have to validate this
            continue;
        }

        const boost::optional<CBlockIndex> blockIndex = txdb.ReadBlockIndex(cpBlockHash);
        if (!blockIndex) {
            NLog.write(b_sev::critical,
                       "Block index is inconsistent; Checkpoint of block height {} and block hash {} "
                       "was unreadable from the block index",
                       cpBlockHeight, cpBlockHash.ToString());
            return false;
        }

        if (blockIndex->nHeight != cpBlockHeight) {
            NLog.write(b_sev::critical,
                       "Block index is inconsistent; While checkpoint of block hash {} "
                       "was found, the checkpoint provided height {} doesn't match the stored height {}",
                       cpBlockHash.ToString(), cpBlockHeight, blockIndex->nHeight);
            return false;
        }

        const boost::optional<uint256> blockHashOfHeight = txdb.ReadBlockHashOfHeight(cpBlockHeight);
        if (!blockHashOfHeight) {
            NLog.write(b_sev::critical,
                       "Database is inconsistent; While checkpoint of block hash {} of height {}"
                       "was found, the block height was not found based on the block hash in the DB",
                       cpBlockHash.ToString(), cpBlockHeight);
            return false;
        }

        if (*blockHashOfHeight != cpBlockHash) {
            NLog.write(
                b_sev::critical,
                "Database is inconsistent; While checkpoint of block hash {} "
                "was found, the block hash based on height {} didn't match the value from block index",
                cpBlockHash.ToString(), blockHashOfHeight->ToString());
            return false;
        }
    }

    return true;
}

} // namespace Checkpoints
