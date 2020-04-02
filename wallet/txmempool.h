#ifndef TXMEMPOOL_H
#define TXMEMPOOL_H

#include "transaction.h"
#include "util.h"
#include <map>

static const uint32_t MEMPOOL_HEIGHT = 0x7FFFFFFF;

class CTxMemPool
{
public:
    mutable CCriticalSection        cs;
    std::map<uint256, CTransaction> mapTx;
    std::map<COutPoint, CInPoint>   mapNextTx;

    bool addUnchecked(const uint256& hash, CTransaction& tx);
    bool remove(const CTransaction& tx, bool fRecursive = false);
    bool removeConflicts(const CTransaction& tx);
    void clear();
    void queryHashes(std::vector<uint256>& vtxid);

    unsigned long size() const
    {
        LOCK(cs);
        return mapTx.size();
    }

    bool exists(uint256 hash) const
    {
        LOCK(cs);
        return (mapTx.count(hash) != 0);
    }

    bool lookup(uint256 hash, CTransaction& result) const
    {
        LOCK(cs);
        std::map<uint256, CTransaction>::const_iterator i = mapTx.find(hash);
        if (i == mapTx.end())
            return false;
        result = i->second;
        return true;
    }

    bool isSpent(const COutPoint& outpoint)
    {
        LOCK(cs);
        return mapNextTx.count(outpoint);
    }

    /// the returned pointer isn't guaranteed to remain valid, ensure to lock before using this method
    const CTransaction* lookup_unsafe(const uint256& hash) const
    {
        auto it = mapTx.find(hash);
        if (it != mapTx.cend())
            return &it->second;
        else
            return nullptr;
    }
};

#endif // TXMEMPOOL_H
