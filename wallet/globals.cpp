#include "globals.h"

#include "txmempool.h"

CTxMemPool              mempool;
boost::atomic<uint32_t> nTransactionsUpdated{0};

BlockIndexMapType   mapBlockIndex;
CBlockIndexSmartPtr pindexBest{nullptr};
CBlockIndexSmartPtr pindexGenesisBlock = nullptr;

bool               fUseFastIndex;
boost::atomic<int> nBestHeight{-1};

CScheduler scheduler;

CBlockIndexSmartPtr pblockindexFBBHLast;

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
