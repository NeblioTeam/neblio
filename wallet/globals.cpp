#include "globals.h"

#include "txmempool.h"

CTxMemPool              mempool;
boost::atomic<uint32_t> nTransactionsUpdated{0};

BlockIndexMapType   mapBlockIndex;
CBlockIndexSmartPtr pindexBest{nullptr};
CBlockIndexSmartPtr pindexGenesisBlock = nullptr;

NetworkType networkType = NetworkType::Mainnet;

bool               fUseFastIndex;
boost::atomic<int> nBestHeight{-1};

CBlockIndexSmartPtr pblockindexFBBHLast;
