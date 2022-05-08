#include "consensus.h"

#include "blockindex.h"
#include "ntp1/ntp1transaction.h"
#include "stdexcept"
#include "transaction.h"

bool fEnforceCanonical;

bool IsFinalTx(const CTransaction& tx, const ITxDB& txdb, int nBlockHeight, int64_t nBlockTime)
{
    // Time based nLockTime implemented in 0.1.6
    if (tx.nLockTime == 0)
        return true;
    if (nBlockHeight == 0)
        nBlockHeight = txdb.GetBestChainHeight();
    if (nBlockTime == 0)
        nBlockTime = GetAdjustedTime();
    if ((int64_t)tx.nLockTime <
        ((int64_t)tx.nLockTime < LOCKTIME_THRESHOLD ? (int64_t)nBlockHeight : nBlockTime))
        return true;
    for (const CTxIn& txin : tx.vin)
        if (!txin.IsFinal())
            return false;
    return true;
}

bool IsStandardTx(const int blockHeight, const CTransaction& tx, std::string& reason)
{
    if (tx.nVersion > CTransaction::CURRENT_VERSION) {
        reason = "version";
        return false;
    }

    // nTime has different purpose from nLockTime but can be used in similar attacks
    if (tx.nTime > FutureDrift(GetAdjustedTime())) {
        reason = "time-too-new";
        return false;
    }

    // Extremely large transactions with lots of inputs can cost the network
    // almost as much to process as they cost the sender in fees, because
    // computing signature hashes is O(ninputs*txsize). Limiting transactions
    // to MAX_STANDARD_TX_SIZE mitigates CPU exhaustion attacks.
    unsigned int sz = tx.GetSerializeSize(SER_NETWORK, CTransaction::CURRENT_VERSION);
    if (sz >= MAX_STANDARD_TX_SIZE) {
        reason = "tx-size";
        return false;
    }

    for (const CTxIn& txin : tx.vin) {
        // Biggest 'standard' txin is a 3-signature 3-of-3 CHECKMULTISIG
        // pay-to-script-hash, which is 3 ~80-byte signatures, 3
        // ~65-byte public keys, plus a few script ops.
        if (txin.scriptSig.size() > 500) {
            reason = "scriptsig-size";
            return false;
        }
        if (!txin.scriptSig.IsPushOnly()) {
            reason = "scriptsig-not-pushonly";
            return false;
        }
        if (fEnforceCanonical && !txin.scriptSig.HasCanonicalPushes()) {
            reason = "scriptsig-non-canonical-push";
            return false;
        }
    }

    unsigned int nDataOut = 0;
    txnouttype   whichType;
    for (const CTxOut& txout : tx.vout) {
        if (!::IsStandard(blockHeight, txout.scriptPubKey, whichType)) {
            reason = "scriptpubkey";
            return false;
        }
        if (whichType == TX_NULL_DATA) {
            nDataOut++;
        }
        if (txout.nValue == 0) {
            reason = "dust";
            return false;
        }
        if (fEnforceCanonical && !txout.scriptPubKey.HasCanonicalPushes()) {
            reason = "scriptpubkey-non-canonical-push";
            return false;
        }
    }

    // only one OP_RETURN txout is permitted
    if (nDataOut > 1) {
        reason = "multi-op-return";
        return false;
    }

    return true;
}

bool EnableEnforceUniqueTokenSymbols(const ITxDB& txdb)
{
    if (Params().GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON, txdb)) {
        return true;
    } else {
        return false;
    }
}

bool IsIssuedTokenBlacklisted(std::pair<CTransaction, NTP1Transaction>& txPair)
{
    const auto& prevout0      = txPair.first.vin[0].prevout;
    std::string storedTokenId = txPair.second.getTokenIdIfIssuance(prevout0.hash.ToString(), prevout0.n);
    return Params().IsNTP1TokenBlacklisted(storedTokenId);
}

bool IsTxInMainChain(const ITxDB& txdb, const uint256& txHash)
{
    CTransaction tx;
    uint256      hashBlock;
    if (GetTransaction(txHash, tx, hashBlock)) {
        const auto mi = txdb.ReadBlockIndex(hashBlock);
        if (mi) {
            return mi->IsInMainChain(txdb);
        } else {
            throw std::runtime_error("Unable to find the block that has the transaction " +
                                     txHash.ToString());
        }
    }
    throw std::runtime_error("Unable to retrieve the transaction " + txHash.ToString());
}

void AssertNTP1TokenNameIsNotAlreadyInMainChain(const std::string& sym, const uint256& txHash,
                                                const ITxDB& txdb)
{
    // make sure that case doesn't matter by converting to upper case
    std::vector<uint256> storedSymbolsTxHashes;
    if (txdb.ReadNTP1TxsWithTokenSymbol(sym, storedSymbolsTxHashes)) {
        for (const uint256& h : storedSymbolsTxHashes) {
            if (!IsTxInMainChain(txdb, h)) {
                continue;
            }
            auto pair = std::make_pair(CTransaction::FetchTxFromDisk(h), NTP1Transaction());
            FetchNTP1TxFromDisk(pair, txdb, false);
            std::string storedSymbol = pair.second.getTokenSymbolIfIssuance();
            // blacklisted tokens can be duplicated, since they won't be used ever again
            if (IsIssuedTokenBlacklisted(pair)) {
                continue;
            }
            // make sure that case doesn't matter by converting to upper case
            std::transform(storedSymbol.begin(), storedSymbol.end(), storedSymbol.begin(), ::toupper);
            if (AreTokenSymbolsEquivalent(sym, storedSymbol) && txHash != h) {
                throw std::runtime_error(
                    "Failed to accept issuance of token " + sym + " from transaction " +
                    txHash.ToString() +
                    "; this token symbol already exists in transaction: " + h.ToString());
            }
        }
    } else {
        throw std::runtime_error("Unable to verify whether a token with the symbol " + sym +
                                 " already exists. Reading the database failed.");
    }
}

void AssertNTP1TokenNameIsNotAlreadyInMainChain(const NTP1Transaction& ntp1tx, const ITxDB& txdb)
{
    if (ntp1tx.getTxType() == NTP1TxType_ISSUANCE) {
        std::string sym = ntp1tx.getTokenSymbolIfIssuance();
        AssertNTP1TokenNameIsNotAlreadyInMainChain(sym, ntp1tx.getTxHash(), txdb);
    } else if (ntp1tx.getTxType() == NTP1TxType_UNKNOWN) {
        throw std::runtime_error("Attempted to " + std::string(__func__) +
                                 " on an uninitialized NTP1 transaction");
    }
}

// ppcoin: find last block index up to pindex
CBlockIndex GetLastBlockIndex(const CBlockIndex& indexIn, bool fProofOfStake, const ITxDB& txdb)
{
    CBlockIndex index = indexIn;

    // if we're looking for a PoW block, we start at the latest one,
    // better than looping over the whole history
    if (!fProofOfStake && index.nHeight > Params().LastPoWBlock()) {
        const boost::optional<uint256> oHash = txdb.ReadBlockHashOfHeight(Params().LastPoWBlock());
        if (oHash) {
            if (auto bindex = txdb.ReadBlockIndex(*oHash)) {
                index = std::move(*bindex);
            } else {
                NLog.write(b_sev::critical, "Failed to read last PoW block's block index");
            }
        }
    }

    while (index.hashPrev != 0 && index.IsProofOfStake() != fProofOfStake) {
        const boost::optional<CBlockIndex> bindex = index.getPrev(txdb);
        if (bindex) {
            index = std::move(*bindex);
        } else if (index.blockHash == Params().GenesisBlockHash()) {
            return index;
        } else {
            NLog.write(b_sev::critical,
                       "Failed to get prev block index, even though it's not genesis block");
            break;
        }
    }
    return index;
}
