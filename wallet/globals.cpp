#include "globals.h"

#include "txmempool.h"

CTxMemPool mempool;

// BlockIndexMapType   mapBlockIndex;
boost::shared_ptr<CBlockIndex> pindexGenesisBlock = nullptr;

boost::atomic_int64_t nTimeLastBestBlockReceived{0};

boost::atomic<uint32_t> nTransactionsUpdated{0};

boost::atomic<uint256> nBestInvalidTrust{0};

boost::atomic<int64_t> NodeIDCounter{0};

boost::atomic<bool> fImporting{false};

AllStoredVotes blockVotes;

std::string strSubVersion;

unsigned int nNodeLifespan;

// Settings
CAmount nTransactionFee    = MIN_TX_FEE;
CAmount nReserveBalance    = 0;
CAmount nMinimumInputValue = 0;

unsigned int nDerivationMethodIndex;

std::string SanitizeString(const std::string& str, int rule)
{
    std::string strResult;
    for (std::string::size_type i = 0; i < str.size(); i++) {
        if (SAFE_CHARS[rule].find(str[i]) != std::string::npos)
            strResult.push_back(str[i]);
    }
    return strResult;
}

unsigned int MaxBlockSize(int blockHeight)
{
    if (Params().GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON, blockHeight)) {
        return MAX_BLOCK_SIZE;
    } else {
        return OLD_MAX_BLOCK_SIZE;
    }
}
