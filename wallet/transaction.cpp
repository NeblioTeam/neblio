#include "transaction.h"

#include "bignum.h"
#include "block.h"
#include "checkpoints.h"
#include "main.h"
#include "txindex.h"
#include "txmempool.h"
#include "util.h"
#include <boost/foreach.hpp>

void CTransaction::SetNull()
{
    nVersion = CTransaction::CURRENT_VERSION;
    nTime    = GetAdjustedTime();
    vin.clear();
    vout.clear();
    nLockTime = 0;
    nDoS      = 0; // Denial-of-service prevention
}

uint256 CTransaction::GetHash() const { return SerializeHash(*this); }

bool CTransaction::IsNewerThan(const CTransaction& old) const
{
    if (vin.size() != old.vin.size())
        return false;
    for (unsigned int i = 0; i < vin.size(); i++)
        if (vin[i].prevout != old.vin[i].prevout)
            return false;

    bool         fNewer  = false;
    unsigned int nLowest = std::numeric_limits<unsigned int>::max();
    for (unsigned int i = 0; i < vin.size(); i++) {
        if (vin[i].nSequence != old.vin[i].nSequence) {
            if (vin[i].nSequence <= nLowest) {
                fNewer  = false;
                nLowest = vin[i].nSequence;
            }
            if (old.vin[i].nSequence < nLowest) {
                fNewer  = true;
                nLowest = old.vin[i].nSequence;
            }
        }
    }
    return fNewer;
}

bool CTransaction::IsCoinStake() const
{
    // ppcoin: the coin stake transaction is marked with the first output empty
    return (vin.size() > 0 && (!vin[0].prevout.IsNull()) && vout.size() >= 2 && vout[0].IsEmpty());
}

int64_t CTransaction::GetValueOut() const
{
    int64_t nValueOut = 0;
    BOOST_FOREACH (const CTxOut& txout, vout) {
        nValueOut += txout.nValue;
        if (!MoneyRange(txout.nValue) || !MoneyRange(nValueOut))
            throw std::runtime_error("CTransaction::GetValueOut() : value out of range");
    }
    return nValueOut;
}

std::string CTransaction::ToStringShort() const
{
    std::string str;
    str += strprintf("%s %s", GetHash().ToString().c_str(),
                     IsCoinBase() ? "base" : (IsCoinStake() ? "stake" : "user"));
    return str;
}

std::string CTransaction::ToString() const
{
    std::string str;
    str += IsCoinBase() ? "Coinbase" : (IsCoinStake() ? "Coinstake" : "CTransaction");
    str += strprintf(
        "(hash=%s, nTime=%d, ver=%d, vin.size=%" PRIszu ", vout.size=%" PRIszu ", nLockTime=%d)\n",
        GetHash().ToString().substr(0, 10).c_str(), nTime, nVersion, vin.size(), vout.size(), nLockTime);
    for (unsigned int i = 0; i < vin.size(); i++)
        str += "    " + vin[i].ToString() + "\n";
    for (unsigned int i = 0; i < vout.size(); i++)
        str += "    " + vout[i].ToString() + "\n";
    return str;
}

void CTransaction::print() const { printf("%s", ToString().c_str()); }

bool CTransaction::ReadFromDisk(CTxDB& txdb, COutPoint prevout, CTxIndex& txindexRet)
{
    SetNull();
    if (!txdb.ReadTxIndex(prevout.hash, txindexRet))
        return false;
    if (!txdb.ReadTx(txindexRet.pos, *this))
        return false;
    if (prevout.n >= vout.size()) {
        SetNull();
        return false;
    }
    return true;
}

bool CTransaction::ReadFromDisk(CTxDB& txdb, COutPoint prevout)
{
    CTxIndex txindex;
    return ReadFromDisk(txdb, prevout, txindex);
}

//
// Check transaction inputs, and make sure any
// pay-to-script-hash transactions are evaluating IsStandard scripts
//
// Why bother? To avoid denial-of-service attacks; an attacker
// can submit a standard HASH... OP_EQUAL transaction,
// which will get accepted into blocks. The redemption
// script can be anything; an attacker could use a very
// expensive-to-check-upon-redemption script like:
//   DUP CHECKSIG DROP ... repeated 100 times... OP_1
//
bool CTransaction::AreInputsStandard(const MapPrevTx& mapInputs) const
{
    if (IsCoinBase())
        return true; // Coinbases don't use vin normally

    for (unsigned int i = 0; i < vin.size(); i++) {
        const CTxOut& prev = GetOutputFor(vin[i], mapInputs);

        std::vector<std::vector<unsigned char>> vSolutions;
        txnouttype                              whichType;
        // get the scriptPubKey corresponding to this input:
        const CScript& prevScript = prev.scriptPubKey;
        if (!Solver(prevScript, whichType, vSolutions))
            return false;

        // Transactions with extra stuff in their scriptSigs are
        // non-standard. Note that this EvalScript() call will
        // be quick, because if there are any operations
        // beside "push data" in the scriptSig the
        // IsStandard() call returns false
        std::vector<std::vector<unsigned char>> stack;
        if (!EvalScript(stack, vin[i].scriptSig, *this, i, false, 0))
            return false;

        if (whichType == TX_SCRIPTHASH) {
            if (stack.empty())
                return false;
            CScript subscript(stack.back().begin(), stack.back().end()); // Get the redeemScript
            // Removed the check to make sure the redeemScript subscript fits one of the four standard
            // transaction types Instead, make sure that the redeemScript doesn't have too many signature
            // check Ops
            if (subscript.GetSigOpCount(true) > MAX_P2SH_SIGOPS) {
                return false;
            }
        } else {
            // Not a TX_SCRIPTHASH scriptPubKey
            int nArgsExpected = ScriptSigArgsExpected(whichType, vSolutions);
            if (nArgsExpected < 0)
                return false;
            // If stack is different than expected, not standard
            if (stack.size() != (unsigned int)nArgsExpected)
                return false;
        }
    }

    return true;
}

unsigned int CTransaction::GetLegacySigOpCount() const
{
    unsigned int nSigOps = 0;
    for (const CTxIn& txin : vin) {
        nSigOps += txin.scriptSig.GetSigOpCount(false);
    }
    for (const CTxOut& txout : vout) {
        nSigOps += txout.scriptPubKey.GetSigOpCount(false);
    }
    return nSigOps;
}

bool CTransaction::CheckTransaction() const
{
    // Basic checks that don't depend on any context
    if (vin.empty())
        return DoS(10, error("CTransaction::CheckTransaction() : vin empty"));
    if (vout.empty())
        return DoS(10, error("CTransaction::CheckTransaction() : vout empty"));
    // Size limits
    unsigned int nSizeLimit = MaxBlockSize();
    if (::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION) > nSizeLimit)
        return DoS(100, error("CTransaction::CheckTransaction() : size limits failed"));

    // Check for negative or overflow output values
    int64_t nValueOut = 0;
    for (unsigned int i = 0; i < vout.size(); i++) {
        const CTxOut& txout = vout[i];
        if (txout.IsEmpty() && !IsCoinBase() && !IsCoinStake())
            return DoS(100,
                       error("CTransaction::CheckTransaction() : txout empty for user transaction"));
        if (txout.nValue < 0)
            return DoS(100, error("CTransaction::CheckTransaction() : txout.nValue negative"));
        if (txout.nValue > MAX_MONEY)
            return DoS(100, error("CTransaction::CheckTransaction() : txout.nValue too high"));
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return DoS(100, error("CTransaction::CheckTransaction() : txout total out of range"));
    }

    // Check for duplicate inputs
    std::set<COutPoint> vInOutPoints;
    for (const CTxIn& txin : vin) {
        if (vInOutPoints.count(txin.prevout))
            return false;
        vInOutPoints.insert(txin.prevout);
    }

    if (IsCoinBase()) {
        if (vin[0].scriptSig.size() < 2 || vin[0].scriptSig.size() > 100)
            return DoS(100, error("CTransaction::CheckTransaction() : coinbase script size is invalid"));
    } else {
        for (const CTxIn& txin : vin)
            if (txin.prevout.IsNull())
                return DoS(10, error("CTransaction::CheckTransaction() : prevout is null"));
    }

    return true;
}

int64_t CTransaction::GetMinFee(unsigned int nBlockSize, enum GetMinFee_mode mode,
                                unsigned int nBytes) const
{
    // Base fee is either MIN_TX_FEE or MIN_RELAY_TX_FEE
    int64_t nBaseFee = (mode == GMF_RELAY) ? MIN_RELAY_TX_FEE : MIN_TX_FEE;

    unsigned int nNewBlockSize = nBlockSize + nBytes;
    int64_t      nMinFee       = (1 + (int64_t)nBytes / 1000) * nBaseFee;

    // To limit dust spam, require MIN_TX_FEE/MIN_RELAY_TX_FEE if any output is less than 0.01
    if (nMinFee < nBaseFee) {
        for (const CTxOut& txout : vout)
            if (txout.nValue < CENT)
                nMinFee = nBaseFee;
    }

    // Raise the price as the block approaches full
    unsigned int nSizeLimit = MaxBlockSize();
    if (nBlockSize != 1 && nNewBlockSize >= nSizeLimit / 2) {
        if (nNewBlockSize >= nSizeLimit)
            return MAX_MONEY;
        nMinFee *= nSizeLimit / (nSizeLimit - nNewBlockSize);
    }

    if (!MoneyRange(nMinFee))
        nMinFee = MAX_MONEY;
    return nMinFee;
}

bool CTransaction::ReadFromDisk(CDiskTxPos pos, CTxDB& txdb) { return txdb.ReadTx(pos, *this); }

bool CTransaction::DisconnectInputs(CTxDB& txdb)
{
    // Relinquish previous transactions' spent pointers
    if (!IsCoinBase()) {
        for (const CTxIn& txin : vin) {
            COutPoint prevout = txin.prevout;

            // Get prev txindex from disk
            CTxIndex txindex;
            if (!txdb.ReadTxIndex(prevout.hash, txindex))
                return error("DisconnectInputs() : ReadTxIndex failed");

            if (prevout.n >= txindex.vSpent.size())
                return error("DisconnectInputs() : prevout.n out of range");

            // Mark outpoint as not spent
            txindex.vSpent[prevout.n].SetNull();

            // Write back
            if (!txdb.UpdateTxIndex(prevout.hash, txindex))
                return error("DisconnectInputs() : UpdateTxIndex failed");
        }
    }

    // Remove transaction from index
    // This can fail if a duplicate of this transaction was in a chain that got
    // reorganized away. This is only possible if this transaction was completely
    // spent, so erasing it would be a no-op anyway.
    txdb.EraseTxIndex(*this);

    return true;
}

bool CTransaction::FetchInputs(CTxDB& txdb, const std::map<uint256, CTxIndex>& mapTestPool, bool fBlock,
                               bool fMiner, MapPrevTx& inputsRet, bool& fInvalid)
{
    // FetchInputs can return false either because we just haven't seen some inputs
    // (in which case the transaction should be stored as an orphan)
    // or because the transaction is malformed (in which case the transaction should
    // be dropped).  If tx is definitely invalid, fInvalid will be set to true.
    fInvalid = false;

    if (IsCoinBase())
        return true; // Coinbase transactions have no inputs to fetch.

    for (unsigned int i = 0; i < vin.size(); i++) {
        COutPoint prevout = vin[i].prevout;
        if (inputsRet.count(prevout.hash))
            continue; // Got it already

        // Read txindex
        CTxIndex& txindex = inputsRet[prevout.hash].first;
        bool      fFound  = true;
        if ((fBlock || fMiner) && mapTestPool.count(prevout.hash)) {
            // Get txindex from current proposed changes
            txindex = mapTestPool.find(prevout.hash)->second;
        } else {
            // Read txindex from txdb
            fFound = txdb.ReadTxIndex(prevout.hash, txindex);
        }
        if (!fFound && (fBlock || fMiner))
            return fMiner ? false
                          : error("FetchInputs() : %s prev tx %s index entry not found",
                                  GetHash().ToString().c_str(), prevout.hash.ToString().c_str());

        // Read txPrev
        CTransaction& txPrev = inputsRet[prevout.hash].second;
        if (!fFound || txindex.pos == CDiskTxPos(1, 1)) {
            // Get prev tx from single transactions in memory
            if (!mempool.lookup(prevout.hash, txPrev))
                return error("FetchInputs() : %s mempool Tx prev not found %s",
                             GetHash().ToString().c_str(), prevout.hash.ToString().c_str());
            if (!fFound)
                txindex.vSpent.resize(txPrev.vout.size());
        } else {
            // Get prev tx from disk
            if (!txPrev.ReadFromDisk(txindex.pos, txdb))
                return error("FetchInputs() : %s ReadFromDisk prev tx %s failed",
                             GetHash().ToString().c_str(), prevout.hash.ToString().c_str());
        }
    }

    // Make sure all prevout.n indexes are valid:
    for (unsigned int i = 0; i < vin.size(); i++) {
        const COutPoint prevout = vin[i].prevout;
        assert(inputsRet.count(prevout.hash) != 0);
        const CTxIndex&     txindex = inputsRet[prevout.hash].first;
        const CTransaction& txPrev  = inputsRet[prevout.hash].second;
        if (prevout.n >= txPrev.vout.size() || prevout.n >= txindex.vSpent.size()) {
            // Revisit this if/when transaction replacement is implemented and allows
            // adding inputs:
            fInvalid = true;
            return DoS(100, error("FetchInputs() : %s prevout.n out of range %d %" PRIszu " %" PRIszu
                                  " prev tx %s\n%s",
                                  GetHash().ToString().c_str(), prevout.n, txPrev.vout.size(),
                                  txindex.vSpent.size(), prevout.hash.ToString().c_str(),
                                  txPrev.ToString().c_str()));
        }
    }

    return true;
}

const CTxOut& CTransaction::GetOutputFor(const CTxIn& input, const MapPrevTx& inputs) const
{
    MapPrevTx::const_iterator mi = inputs.find(input.prevout.hash);
    if (mi == inputs.end())
        throw std::runtime_error("CTransaction::GetOutputFor() : prevout.hash not found");

    const CTransaction& txPrev = (mi->second).second;
    if (input.prevout.n >= txPrev.vout.size())
        throw std::runtime_error("CTransaction::GetOutputFor() : prevout.n out of range");

    return txPrev.vout[input.prevout.n];
}

int64_t CTransaction::GetValueIn(const MapPrevTx& inputs) const
{
    if (IsCoinBase())
        return 0;

    int64_t nResult = 0;
    for (unsigned int i = 0; i < vin.size(); i++) {
        nResult += GetOutputFor(vin[i], inputs).nValue;
    }
    return nResult;
}

unsigned int CTransaction::GetP2SHSigOpCount(const MapPrevTx& inputs) const
{
    if (IsCoinBase())
        return 0;

    unsigned int nSigOps = 0;
    for (unsigned int i = 0; i < vin.size(); i++) {
        const CTxOut& prevout = GetOutputFor(vin[i], inputs);
        if (prevout.scriptPubKey.IsPayToScriptHash())
            nSigOps += prevout.scriptPubKey.GetSigOpCount(vin[i].scriptSig);
    }
    return nSigOps;
}

bool CTransaction::ConnectInputs(CTxDB& /*txdb*/, MapPrevTx inputs,
                                 std::map<uint256, CTxIndex>& mapTestPool, const CDiskTxPos& posThisTx,
                                 const ConstCBlockIndexSmartPtr& pindexBlock, bool fBlock, bool fMiner)
{
    // Take over previous transactions' spent pointers
    // fBlock is true when this is called from AcceptBlock when a new best-block is added to the
    // blockchain fMiner is true when called from the internal bitcoin miner
    // ... both are false when called from CTransaction::AcceptToMemoryPool
    if (!IsCoinBase()) {
        int64_t nValueIn = 0;
        int64_t nFees    = 0;
        for (unsigned int i = 0; i < vin.size(); i++) {
            COutPoint prevout = vin[i].prevout;
            assert(inputs.count(prevout.hash) > 0);
            CTxIndex&     txindex = inputs[prevout.hash].first;
            CTransaction& txPrev  = inputs[prevout.hash].second;

            if (prevout.n >= txPrev.vout.size() || prevout.n >= txindex.vSpent.size())
                return DoS(100, error("ConnectInputs() : %s prevout.n out of range %d %" PRIszu
                                      " %" PRIszu " prev tx %s\n%s",
                                      GetHash().ToString().c_str(), prevout.n, txPrev.vout.size(),
                                      txindex.vSpent.size(), prevout.hash.ToString().c_str(),
                                      txPrev.ToString().c_str()));

            // If prev is coinbase or coinstake, check that it's matured
            int nCbM = CoinbaseMaturity();
            if (txPrev.IsCoinBase() || txPrev.IsCoinStake())
                for (ConstCBlockIndexSmartPtr pindex = boost::atomic_load(&pindexBlock);
                     pindex && pindexBlock->nHeight - pindex->nHeight < nCbM;
                     pindex = boost::atomic_load(&pindex->pprev)) {
                    static_assert(std::is_same<decltype(pindex->blockKeyInDB),
                                               decltype(txindex.pos.nBlockPos)>::value,
                                  "Expected same types");
                    if (pindex->blockKeyInDB == txindex.pos.nBlockPos) {
                        return error("ConnectInputs() : tried to spend %s at depth %d",
                                     txPrev.IsCoinBase() ? "coinbase" : "coinstake",
                                     pindexBlock->nHeight - pindex->nHeight);
                    }
                }

            // ppcoin: check transaction timestamp
            if (txPrev.nTime > nTime)
                return DoS(
                    100,
                    error("ConnectInputs() : transaction timestamp earlier than input transaction"));

            // Check for negative or overflow input values
            nValueIn += txPrev.vout[prevout.n].nValue;
            if (!MoneyRange(txPrev.vout[prevout.n].nValue) || !MoneyRange(nValueIn))
                return DoS(100, error("ConnectInputs() : txin values out of range"));
        }
        // The first loop above does all the inexpensive checks.
        // Only if ALL inputs pass do we perform expensive ECDSA signature checks.
        // Helps prevent CPU exhaustion attacks.
        for (unsigned int i = 0; i < vin.size(); i++) {
            COutPoint prevout = vin[i].prevout;
            assert(inputs.count(prevout.hash) > 0);
            CTxIndex&     txindex = inputs[prevout.hash].first;
            CTransaction& txPrev  = inputs[prevout.hash].second;

            // Check for conflicts (double-spend)
            // This doesn't trigger the DoS code on purpose; if it did, it would make it easier
            // for an attacker to attempt to split the network.
            if (!txindex.vSpent[prevout.n].IsNull())
                return fMiner ? false
                              : error("ConnectInputs() : %s prev tx already used at %s",
                                      GetHash().ToString().c_str(),
                                      txindex.vSpent[prevout.n].ToString().c_str());

            // Skip ECDSA signature verification when connecting blocks (fBlock=true)
            // before the last blockchain checkpoint. This is safe because block merkle hashes are
            // still computed and checked, and any change will be caught at the next checkpoint.
            if (!(fBlock && (nBestHeight < Checkpoints::GetTotalBlocksEstimate()))) {
                // Verify signature
                bool fStrictPayToScriptHash = true;
                if (!VerifySignature(txPrev, *this, i, fStrictPayToScriptHash, false, 0)) {
                    // only during transition phase for P2SH: do not invoke anti-DoS code for
                    // potentially old clients relaying bad P2SH transactions
                    if (fStrictPayToScriptHash && VerifySignature(txPrev, *this, i, false, false, 0))
                        return error("ConnectInputs() : %s P2SH VerifySignature failed",
                                     GetHash().ToString().c_str());

                    return DoS(100, error("ConnectInputs() : %s VerifySignature failed",
                                          GetHash().ToString().c_str()));
                }
            }

            // Mark outpoints as spent
            txindex.vSpent[prevout.n] = posThisTx;

            // Write back
            if (fBlock || fMiner) {
                mapTestPool[prevout.hash] = txindex;
            }
        }

        if (!IsCoinStake()) {
            if (nValueIn < GetValueOut())
                return DoS(100, error("ConnectInputs() : %s value in < value out",
                                      GetHash().ToString().c_str()));

            // Tally transaction fees
            int64_t nTxFee = nValueIn - GetValueOut();
            if (nTxFee < 0)
                return DoS(100, error("ConnectInputs() : %s nTxFee < 0", GetHash().ToString().c_str()));

            // enforce transaction fees for every block
            if (nTxFee < GetMinFee())
                return fBlock ? DoS(100,
                                    error("ConnectInputs() : %s not paying required fee=%s, paid=%s",
                                          GetHash().ToString().c_str(), FormatMoney(GetMinFee()).c_str(),
                                          FormatMoney(nTxFee).c_str()))
                              : false;

            nFees += nTxFee;
            if (!MoneyRange(nFees))
                return DoS(100, error("ConnectInputs() : nFees out of range"));
        }
    }

    return true;
}

// ppcoin: total coin age spent in transaction, in the unit of coin-days.
// Only those coins meeting minimum age requirement counts. As those
// transactions not in main chain are not currently indexed so we
// might not find out about their coin age. Older transactions are
// guaranteed to be in main chain by sync-checkpoint. This rule is
// introduced to help nodes establish a consistent view of the coin
// age (trust score) of competing branches.
bool CTransaction::GetCoinAge(CTxDB& txdb, uint64_t& nCoinAge) const
{
    CBigNum      bnCentSecond = 0; // coin age in the unit of cent-seconds
    unsigned int nSMA         = StakeMinAge();
    nCoinAge                  = 0;

    if (IsCoinBase())
        return true;

    for (const CTxIn& txin : vin) {
        // First try finding the previous transaction in database
        CTransaction txPrev;
        CTxIndex     txindex;
        if (!txPrev.ReadFromDisk(txdb, txin.prevout, txindex))
            continue; // previous transaction not in main chain
        if (nTime < txPrev.nTime)
            return false; // Transaction timestamp violation

        // Read block header
        CBlock block;
        if (!block.ReadFromDisk(txindex.pos.nBlockPos, false))
            return false; // unable to read block of previous transaction
        if (block.GetBlockTime() + nSMA > nTime)
            continue; // only count coins meeting min age requirement

        int64_t nValueIn = txPrev.vout[txin.prevout.n].nValue;
        bnCentSecond += CBigNum(nValueIn) * (nTime - txPrev.nTime) / CENT;

        if (fDebug)
            printf("coin age nValueIn=%" PRId64 " nTimeDiff=%d bnCentSecond=%s\n", nValueIn,
                   nTime - txPrev.nTime, bnCentSecond.ToString().c_str());
    }

    CBigNum bnCoinDay = bnCentSecond * CENT / COIN / (24 * 60 * 60);
    if (fDebug)
        printf("coin age bnCoinDay=%s\n", bnCoinDay.ToString().c_str());
    nCoinAge = bnCoinDay.getuint64();
    return true;
}
