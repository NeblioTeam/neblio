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

    // bi-directional mapping of the txid and NTP1 token symbol
    std::map<uint256, std::string> txidToissuedNTP1TokenSymbols;
    std::map<std::string, uint256> issuedNTP1TokenSymbolsToTxid;

    bool addUnchecked(const uint256& hash, const CTransaction& tx);
    bool remove(const CTransaction& tx, bool fRecursive = false);
    bool removeConflicts(const CTransaction& tx);
    void clear();
    void queryHashes(std::vector<uint256>& vtxid);

    unsigned long size() const;

    bool exists(uint256 hash) const;

    bool lookup(uint256 hash, CTransaction& result) const;

    bool isSpent(const COutPoint& outpoint) const;

    bool isSpent_unsafe(const COutPoint& outpoint) const;

    bool isIssaunceTokenSymbolAlreadyInMempool(const CTransaction& tx) const;

    bool isIssaunceTokenSymbolAlreadyInMempool_unsafe(const CTransaction& tx) const;

    bool isIssaunceTokenSymbolAlreadyInMempool(const std::string& symbol) const;

    bool isIssaunceTokenSymbolAlreadyInMempool_unsafe(const std::string& symbol) const;

    /// the returned pointer isn't guaranteed to remain valid, ensure to lock before using this method
    const CTransaction* lookup_unsafe(const uint256& hash) const;

private:
    static std::string                  ConvertSymbolToComparableString(std::string symbol);
    static boost::optional<std::string> GetTokenSymbolIfIssuance(const CTransaction& tx);
};

#endif // TXMEMPOOL_H
