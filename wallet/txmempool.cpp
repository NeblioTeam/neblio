#include "txmempool.h"

#include "globals.h"

#include "ntp1/ntp1transaction.h"

bool CTxMemPool::addUnchecked(const uint256& hash, const CTransaction& tx)
{
    // Add to memory pool without checking anything.  Don't call this directly,
    // call AcceptToMemoryPool to properly check the transaction first.
    {
        // add token symbol
        if (const boost::optional<std::string> symbol = GetTokenSymbolIfIssuance(tx)) {
            const std::string processedSymbol             = ConvertSymbolToComparableString(*symbol);
            txidToissuedNTP1TokenSymbols[hash]            = processedSymbol;
            issuedNTP1TokenSymbolsToTxid[processedSymbol] = hash;
        }

        // add the tx
        mapTx[hash] = tx;
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            mapNextTx[tx.vin[i].prevout] = CInPoint(&mapTx[hash], i);
        }

        nTransactionsUpdated++;
    }
    return true;
}

bool CTxMemPool::remove(const CTransaction& tx, bool fRecursive)
{
    // Remove transaction from memory pool
    LOCK(cs);
    {
        const uint256 hash = tx.GetHash();
        if (mapTx.count(hash)) {
            if (fRecursive) {
                for (unsigned int i = 0; i < tx.vout.size(); i++) {
                    std::map<COutPoint, CInPoint>::iterator it = mapNextTx.find(COutPoint(hash, i));
                    if (it != mapNextTx.end())
                        remove(*it->second.ptx, true);
                }
            }
            for (const CTxIn& txin : tx.vin)
                mapNextTx.erase(txin.prevout);
            mapTx.erase(hash);
            nTransactionsUpdated++;
        }
        {
            auto it = txidToissuedNTP1TokenSymbols.find(hash);
            if (it != txidToissuedNTP1TokenSymbols.end()) {
                const std::string& symbol = it->second;
                txidToissuedNTP1TokenSymbols.erase(it);
                issuedNTP1TokenSymbolsToTxid.erase(symbol);
            }
        }
    }
    return true;
}

bool CTxMemPool::removeConflicts(const CTransaction& tx)
{
    // Remove transactions which depend on inputs of tx, recursively
    LOCK(cs);
    for (const CTxIn& txin : tx.vin) {
        std::map<COutPoint, CInPoint>::iterator it = mapNextTx.find(txin.prevout);
        if (it != mapNextTx.end()) {
            const CTransaction& txConflict = *it->second.ptx;
            if (txConflict != tx)
                remove(txConflict, true);
        }
    }
    return true;
}

void CTxMemPool::clear()
{
    LOCK(cs);
    mapTx.clear();
    mapNextTx.clear();
    ++nTransactionsUpdated;
}

void CTxMemPool::queryHashes(std::vector<uint256>& vtxid)
{
    vtxid.clear();

    LOCK(cs);
    vtxid.reserve(mapTx.size());
    for (std::map<uint256, CTransaction>::iterator mi = mapTx.begin(); mi != mapTx.end(); ++mi)
        vtxid.push_back((*mi).first);
}

unsigned long CTxMemPool::size() const
{
    LOCK(cs);
    return mapTx.size();
}

bool CTxMemPool::exists(uint256 hash) const
{
    LOCK(cs);
    return (mapTx.count(hash) != 0);
}

bool CTxMemPool::lookup(uint256 hash, CTransaction& result) const
{
    LOCK(cs);
    std::map<uint256, CTransaction>::const_iterator i = mapTx.find(hash);
    if (i == mapTx.end())
        return false;
    result = i->second;
    return true;
}

bool CTxMemPool::isSpent(const COutPoint& outpoint) const
{
    LOCK(cs);
    return isSpent_unsafe(outpoint);
}

bool CTxMemPool::isSpent_unsafe(const COutPoint& outpoint) const { return mapNextTx.count(outpoint); }

bool CTxMemPool::isIssaunceTokenSymbolAlreadyInMempool(const CTransaction& tx) const
{
    LOCK(cs);
    return isIssaunceTokenSymbolAlreadyInMempool_unsafe(tx);
}

bool CTxMemPool::isIssaunceTokenSymbolAlreadyInMempool_unsafe(const CTransaction& tx) const
{
    if (const boost::optional<std::string> symbol = GetTokenSymbolIfIssuance(tx)) {
        return isIssaunceTokenSymbolAlreadyInMempool_unsafe(*symbol);
    }
    return false;
}

bool CTxMemPool::isIssaunceTokenSymbolAlreadyInMempool_unsafe(const std::string& symbol) const
{
    const std::string& s = ConvertSymbolToComparableString(symbol);

    auto it = issuedNTP1TokenSymbolsToTxid.find(s);
    return it != issuedNTP1TokenSymbolsToTxid.cend();
}

const CTransaction* CTxMemPool::lookup_unsafe(const uint256& hash) const
{
    auto it = mapTx.find(hash);
    if (it != mapTx.cend())
        return &it->second;
    else
        return nullptr;
}

std::string CTxMemPool::ConvertSymbolToComparableString(std::string symbol)
{
    std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::tolower);
    return symbol;
}

boost::optional<std::string> CTxMemPool::GetTokenSymbolIfIssuance(const CTransaction& tx)
{
    try {
        std::string opRet;
        if (NTP1Transaction::IsTxNTP1(&tx, &opRet)) {
            auto scriptPtr = NTP1Script::ParseScriptHex(opRet);
            if (scriptPtr->getTxType() == NTP1Script::TxType_Issuance) {
                std::shared_ptr<const NTP1Script_Issuance> scriptPtrD =
                    std::dynamic_pointer_cast<const NTP1Script_Issuance>(scriptPtr);
                if (!scriptPtrD) {
                    NLog.write(b_sev::err,
                               "ERROR: even though the type of the NTP1Script is issuance, dynamic cast "
                               "failed. This SHOULD NEVER HAPPEN.");
                }
                return boost::make_optional(scriptPtrD->getTokenSymbol());
            }
        }
    } catch (std::exception& ex) {
        NLog.write(b_sev::err, "ERROR: Exception thrown while parsing submitted script: {}", ex.what());
    } catch (...) {
        NLog.write(b_sev::err, "ERROR: Unknown exception thrown while parsing submitted script");
    }
    return boost::none;
}
