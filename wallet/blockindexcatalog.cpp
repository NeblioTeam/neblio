#include "blockindexcatalog.h"

#include <utility>

CBlockIndexSmartPtr BlockIndexCatalog::InsertBlockIndex(uint256 hash)
{
    if (hash == 0)
        return nullptr;

    // Return existing
    BlockIndexMapType::iterator mi = blockIndexMap.find(hash);
    if (mi != blockIndexMap.end())
        return mi->second;

    // Create new
    CBlockIndexSmartPtr pindexNew = boost::make_shared<CBlockIndex>();
    if (!pindexNew)
        throw std::runtime_error("LoadBlockIndex() : new CBlockIndex failed");
    mi = blockIndexMap.insert(std::make_pair(hash, pindexNew)).first;

    pindexNew->phashBlock = &((*mi).first);

    return pindexNew;
}

BlockIndexCatalog::BlockIndexCatalog() {}
