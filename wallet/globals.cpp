#include "globals.h"

#include "txmempool.h"

CTxMemPool mempool;

BlockIndexMapType   mapBlockIndex;
CBlockIndexSmartPtr pindexGenesisBlock = nullptr;

bool fUseFastIndex;

CBlockIndexSmartPtr     pindexBest{nullptr};
boost::atomic<int>      nBestHeight{-1};
boost::atomic<uint256>  nBestChainTrust{0};
boost::atomic<uint256>  hashBestChain{0};
boost::atomic_int64_t   nTimeBestReceived{0};
boost::atomic<uint32_t> nTransactionsUpdated{0};

boost::atomic<uint256> nBestInvalidTrust{0};

boost::atomic<int64_t> NodeIDCounter{0};

std::string strSubVersion;

std::string SanitizeString(const std::string& str, int rule)
{
    std::string strResult;
    for (std::string::size_type i = 0; i < str.size(); i++) {
        if (SAFE_CHARS[rule].find(str[i]) != std::string::npos)
            strResult.push_back(str[i]);
    }
    return strResult;
}

void SetGlobalBestChainParameters(const CBlockIndexSmartPtr pindex, bool updateCountersAndTimes)
{
    hashBestChain = pindex->GetBlockHash();
    boost::atomic_store(&pindexBest, pindex);
    nBestHeight     = pindex->nHeight;
    nBestChainTrust = pindex->nChainTrust;
    if (updateCountersAndTimes) {
        nTimeBestReceived = GetTime();
        nTransactionsUpdated++;
    }
}
