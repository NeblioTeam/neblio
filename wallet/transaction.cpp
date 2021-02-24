#include "transaction.h"

#include "base58.h"
#include "bignum.h"
#include "block.h"
#include "checkpoints.h"
#include "init.h"
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

bool CTransaction::CheckColdStake(const CScript& script) const
{
    // tx is a coinstake tx
    if (!IsCoinStake())
        return false;

    if (vin.empty())
        return false;

    const boost::optional<std::vector<uint8_t>> firstPubKey =
        vin[0].scriptSig.GetPubKeyOfP2CSScriptSig();
    if (!firstPubKey)
        return false; // this is not P2CS

    // all inputs must be P2CS and must be paying to the same pubkey
    for (unsigned int i = 1; i < vin.size(); i++) {
        if (vin[i].scriptSig.GetPubKeyOfP2CSScriptSig() != firstPubKey)
            return false;
    }

    // all outputs except first (coinstake marker)
    // have the same pubKeyScript and it matches the script we are spending
    for (unsigned int i = 1; i < vout.size(); i++)
        if (vout[i].scriptPubKey != script)
            return false;

    return true;
}

bool CTransaction::HasP2CSOutputs() const
{
    for (const CTxOut& txout : vout) {
        if (txout.scriptPubKey.IsPayToColdStaking())
            return true;
    }
    return false;
}

CAmount CTransaction::GetValueOut() const
{
    CAmount nValueOut = 0;
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
    str += fmt::format("{} {}", GetHash().ToString().c_str(),
                       IsCoinBase() ? "base" : (IsCoinStake() ? "stake" : "user"));
    return str;
}

std::string CTransaction::ToString() const
{
    std::string str;
    str += IsCoinBase() ? "Coinbase" : (IsCoinStake() ? "Coinstake" : "CTransaction");
    str += fmt::format("(hash={}, nTime={}, ver={}, vin.size={}, vout.size={}, nLockTime={})\n",
                       GetHash().ToString().substr(0, 10), nTime, nVersion, vin.size(), vout.size(),
                       nLockTime);
    for (unsigned int i = 0; i < vin.size(); i++)
        str += "    " + vin[i].ToString() + "\n";
    for (unsigned int i = 0; i < vout.size(); i++)
        str += "    " + vout[i].ToString() + "\n";
    return str;
}

void CTransaction::print() const { NLog.write(b_sev::info, "{}", ToString()); }

bool CTransaction::ReadFromDisk(const ITxDB& txdb, COutPoint prevout, CTxIndex& txindexRet)
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
        if (!Solver(CTxDB(), prevScript, whichType, vSolutions))
            return false;

        // Transactions with extra stuff in their scriptSigs are
        // non-standard. Note that this EvalScript() call will
        // be quick, because if there are any operations
        // beside "push data" in the scriptSig the
        // IsStandard() call returns false
        std::vector<std::vector<unsigned char>> stack;
        if (EvalScript(stack, vin[i].scriptSig, *this, i, false, 0).isErr())
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

Result<void, TxValidationState> CTransaction::CheckTransaction(const ITxDB& txdb,
                                                               CBlock*      sourceBlockPtr) const
{
    // Basic checks that don't depend on any context
    if (vin.empty()) {
        DoS(10, false);
        return Err(MakeInvalidTxState(TxValidationResult::TX_CONSENSUS, "bad-txns-vin-empty"));
    }
    if (vout.empty()) {
        DoS(10, false);
        return Err(MakeInvalidTxState(TxValidationResult::TX_CONSENSUS, "bad-txns-vout-empty"));
    }

    // Size limits
    unsigned int nSizeLimit = MaxBlockSize(txdb);
    if (::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION) > nSizeLimit) {
        DoS(100, false);
        return Err(MakeInvalidTxState(TxValidationResult::TX_CONSENSUS, "bad-txns-oversize"));
    }

    // Check for negative or overflow output values
    CAmount nValueOut = 0;
    for (unsigned int i = 0; i < vout.size(); i++) {
        const CTxOut& txout = vout[i];
        if (txout.IsEmpty() && !IsCoinBase() && !IsCoinStake()) {
            DoS(100, false);
            return Err(MakeInvalidTxState(TxValidationResult::TX_CONSENSUS, "txout-empty-for-tx"));
        }

        if (txout.nValue < 0) {
            DoS(100, false);
            return Err(MakeInvalidTxState(TxValidationResult::TX_CONSENSUS, "bad-txns-vout-negative"));
        }

        if (txout.nValue > MAX_MONEY) {
            DoS(100, false);
            return Err(MakeInvalidTxState(TxValidationResult::TX_CONSENSUS, "bad-txns-vout-toolarge"));
        }

        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut)) {
            DoS(100, false);
            return Err(
                MakeInvalidTxState(TxValidationResult::TX_CONSENSUS, "bad-txns-txouttotal-toolarge"));
        }

        // check cold staking enforcement (for delegations) and value out
        if (txout.scriptPubKey.IsPayToColdStaking()) {
            if (txout.nValue < Params().MinColdStakingAmount()) {
                DoS(100, false);
                return Err(MakeInvalidTxState(TxValidationResult::TX_CONSENSUS,
                                              "bad-txns-coldstake-low-amount"));
            }
        }
    }

    // Check for duplicate inputs
    {
        std::set<COutPoint> vInOutPoints;
        for (const CTxIn& txin : vin) {
            if (vInOutPoints.find(txin.prevout) != vInOutPoints.cend()) {
                if (sourceBlockPtr) {
                    sourceBlockPtr->reject = CBlockReject(REJECT_INVALID, "bad-txns-inputs-duplicate",
                                                          sourceBlockPtr->GetHash());
                }
                return Err(
                    MakeInvalidTxState(TxValidationResult::TX_CONSENSUS, "bad-txns-inputs-duplicate"));
            }
            vInOutPoints.insert(txin.prevout);
        }
    }

    if (IsCoinBase()) {
        if (vin[0].scriptSig.size() < 2 || vin[0].scriptSig.size() > 100) {
            if (sourceBlockPtr) {
                sourceBlockPtr->reject =
                    CBlockReject(REJECT_INVALID, "bad-cb-length", sourceBlockPtr->GetHash());
            }
            DoS(100, false);
            return Err(MakeInvalidTxState(TxValidationResult::TX_CONSENSUS, "bad-cb-length"));
        }
    } else {
        for (const CTxIn& txin : vin)
            if (txin.prevout.IsNull()) {
                DoS(10, false);
                return Err(
                    MakeInvalidTxState(TxValidationResult::TX_CONSENSUS, "bad-txns-prevout-null"));
            }
    }

    return Ok();
}

CAmount CTransaction::GetMinFee(const ITxDB& txdb, unsigned int nBlockSize, enum GetMinFee_mode mode,
                                unsigned int nBytes) const
{
    // Base fee is either MIN_TX_FEE or MIN_RELAY_TX_FEE
    CAmount nBaseFee = (mode == GMF_RELAY) ? MIN_RELAY_TX_FEE : MIN_TX_FEE;

    unsigned int nNewBlockSize = nBlockSize + nBytes;
    CAmount      nMinFee       = (1 + (CAmount)nBytes / 1000) * nBaseFee;

    // To limit dust spam, require MIN_TX_FEE/MIN_RELAY_TX_FEE if any output is less than 0.01
    if (nMinFee < nBaseFee) {
        for (const CTxOut& txout : vout)
            if (txout.nValue < CENT)
                nMinFee = nBaseFee;
    }

    // Raise the price as the block approaches full
    unsigned int nSizeLimit = MaxBlockSize(txdb);
    if (nBlockSize != 1 && nNewBlockSize >= nSizeLimit / 2) {
        if (nNewBlockSize >= nSizeLimit)
            return MAX_MONEY;
        nMinFee *= nSizeLimit / (nSizeLimit - nNewBlockSize);
    }

    if (!MoneyRange(nMinFee))
        nMinFee = MAX_MONEY;
    return nMinFee;
}

bool CTransaction::ReadFromDisk(CDiskTxPos pos, const ITxDB& txdb) { return txdb.ReadTx(pos, *this); }

bool CTransaction::DisconnectInputs(CTxDB& txdb)
{
    // Relinquish previous transactions' spent pointers
    if (!IsCoinBase()) {
        for (const CTxIn& txin : vin) {
            COutPoint prevout = txin.prevout;

            // Get prev txindex from disk
            CTxIndex txindex;
            if (!txdb.ReadTxIndex(prevout.hash, txindex))
                return NLog.error("DisconnectInputs() : ReadTxIndex failed");

            if (prevout.n >= txindex.vSpent.size())
                return NLog.error("DisconnectInputs() : prevout.n out of range");

            // Mark outpoint as not spent
            txindex.vSpent[prevout.n].SetNull();

            // Write back
            if (!txdb.UpdateTxIndex(prevout.hash, txindex))
                return NLog.error("DisconnectInputs() : UpdateTxIndex failed");
        }
    }

    // Remove transaction from index
    // This can fail if a duplicate of this transaction was in a chain that got
    // reorganized away. This is only possible if this transaction was completely
    // spent, so erasing it would be a no-op anyway.
    txdb.EraseTxIndex(this->GetHash());

    return true;
}

bool CTransaction::FetchInputs(const ITxDB& txdb, const std::map<uint256, CTxIndex>& mapTestPool,
                               bool fBlock, bool fMiner, MapPrevTx& inputsRet, bool& fInvalid) const
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
                          : NLog.error("FetchInputs() : {} prev tx {} index entry not found",
                                       GetHash().ToString(), prevout.hash.ToString());

        // Read txPrev
        CTransaction& txPrev = inputsRet[prevout.hash].second;
        if (!fFound || txindex.pos == CDiskTxPos(1, 1)) {
            // Get prev tx from single transactions in memory
            if (!mempool.lookup(prevout.hash, txPrev))
                return NLog.error("FetchInputs() : {} mempool Tx prev not found {}",
                                  GetHash().ToString(), prevout.hash.ToString());
            if (!fFound)
                txindex.vSpent.resize(txPrev.vout.size());
        } else {
            // Get prev tx from disk
            if (!txPrev.ReadFromDisk(txindex.pos, txdb))
                return NLog.error("FetchInputs() : {} ReadFromDisk prev tx {} failed",
                                  GetHash().ToString(), prevout.hash.ToString());
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
            return DoS(100,
                       NLog.error("FetchInputs() : {} prevout.n out of range {} {} {}"
                                  " prev tx {}\n{}",
                                  GetHash().ToString(), prevout.n, txPrev.vout.size(),
                                  txindex.vSpent.size(), prevout.hash.ToString(), txPrev.ToString()));
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

CAmount CTransaction::GetValueIn(const MapPrevTx& inputs) const
{
    if (IsCoinBase())
        return 0;

    CAmount nResult = 0;
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

Result<void, TxValidationState>
CTransaction::ConnectInputs(const ITxDB& txdb, MapPrevTx inputs,
                            std::map<uint256, CTxIndex>& mapTestPool, const CDiskTxPos& posThisTx,
                            const boost::optional<CBlockIndex>& pindexBlock, bool fBlock, bool fMiner,
                            CBlock* sourceBlockPtr) const
{
    // Take over previous transactions' spent pointers
    // fBlock is true when this is called from AcceptBlock when a new best-block is added to the
    // blockchain fMiner is true when called from the internal bitcoin miner
    // ... both are false when called from CTransaction::AcceptToMemoryPool
    if (!IsCoinBase()) {
        CAmount nValueIn = 0;
        CAmount nFees    = 0;
        for (unsigned int i = 0; i < vin.size(); i++) {
            COutPoint prevout = vin[i].prevout;
            assert(inputs.count(prevout.hash) > 0);
            CTxIndex&     txindex = inputs[prevout.hash].first;
            CTransaction& txPrev  = inputs[prevout.hash].second;

            if (prevout.n >= txPrev.vout.size() || prevout.n >= txindex.vSpent.size()) {
                DoS(100, false);
                return Err(MakeInvalidTxState(
                    TxValidationResult::TX_INVALID_INPUTS, "bad-txns-inputs-invalid",
                    fmt::format("ConnectInputs() : {} prevout.n out of range {} {} {}"
                                " prev tx {}\n{}",
                                GetHash().ToString(), prevout.n, txPrev.vout.size(),
                                txindex.vSpent.size(), prevout.hash.ToString(), txPrev.ToString())));
            }

            // If prev is coinbase or coinstake, check that it's matured
            int nCbM = Params().CoinbaseMaturity(txdb);
            if (txPrev.IsCoinBase() || txPrev.IsCoinStake())
                for (boost::optional<CBlockIndex> pindex = pindexBlock;
                     pindex && pindexBlock->nHeight - pindex->nHeight < nCbM;
                     pindex = pindex->getPrev(txdb)) {
                    static_assert(std::is_same<decltype(pindex->GetBlockHash()),
                                               decltype(txindex.pos.nBlockPos)>::value,
                                  "Expected same types");
                    if (pindex->GetBlockHash() == txindex.pos.nBlockPos) {
                        if (sourceBlockPtr) {
                            sourceBlockPtr->reject = CBlockReject(
                                REJECT_INVALID, "bad-txns-premature-spend-of-coinbase/coinstake",
                                sourceBlockPtr->GetHash());
                        }
                        const std::string msg = txPrev.IsCoinBase()
                                                    ? "bad-txns-premature-spend-of-coinbase"
                                                    : "bad-txns-premature-spend-of-coinstake";
                        return Err(MakeInvalidTxState(
                            TxValidationResult::TX_PREMATURE_SPEND, msg,
                            fmt::format("ConnectInputs() : tried to spend {} at depth {}",
                                        txPrev.IsCoinBase() ? "coinbase" : "coinstake",
                                        pindexBlock->nHeight - pindex->nHeight)));
                    }
                }

            // ppcoin: check transaction timestamp
            if (txPrev.nTime > nTime) {
                DoS(100, false);
                return Err(MakeInvalidTxState(
                    TxValidationResult::TX_CONSENSUS, "bad-txns-input-time-order",
                    "ConnectInputs() : transaction timestamp earlier than input transaction"));
            }

            // Check for negative or overflow input values
            nValueIn += txPrev.vout[prevout.n].nValue;
            if (!MoneyRange(txPrev.vout[prevout.n].nValue) || !MoneyRange(nValueIn)) {
                DoS(100, false);
                return Err(MakeInvalidTxState(TxValidationResult::TX_CONSENSUS,
                                              "bad-txns-inputvalues-outofrange",
                                              "ConnectInputs() : txin values out of range"));
            }
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
            if (!txindex.vSpent[prevout.n].IsNull()) {
                const auto code = TxValidationResult::TX_MISSING_INPUTS;
                const auto msg  = "bad-txns-inputs-missingorspent";
                if (fMiner) {
                    return Err(MakeInvalidTxState(code, msg));
                }
                return Err(MakeInvalidTxState(
                    code, msg,
                    fmt::format("ConnectInputs() : {} prev tx already used at {}", GetHash().ToString(),
                                txindex.vSpent[prevout.n].ToString())));
            }

            // Skip ECDSA signature verification when connecting blocks (fBlock=true)
            // before the last blockchain checkpoint. This is safe because block merkle hashes are
            // still computed and checked, and any change will be caught at the next checkpoint.
            if (!(fBlock &&
                  (txdb.GetBestChainHeight().value_or(0) < Checkpoints::GetTotalBlocksEstimate()))) {
                // Verify signature
                bool       fStrictPayToScriptHash = true;
                const auto verifyRes =
                    VerifySignature(txPrev, *this, i, fStrictPayToScriptHash, false, 0);
                if (verifyRes.isErr()) {
                    // only during transition phase for P2SH: do not invoke anti-DoS code for
                    // potentially old clients relaying bad P2SH transactions
                    if (fStrictPayToScriptHash) {
                        const auto verifyResP2SH = VerifySignature(txPrev, *this, i, false, false, 0);
                        if (verifyResP2SH.isOk()) {
                            return Err(MakeInvalidTxState(
                                TxValidationResult::TX_NOT_STANDARD,
                                fmt::format("non-mandatory-script-verify-flag ({})",
                                            ScriptErrorString(verifyResP2SH.unwrapErr())),
                                fmt::format("ConnectInputs() : {} P2SH VerifySignature failed",
                                            GetHash().ToString())));
                        }
                    }

                    const std::string msg = fmt::format("mandatory-script-verify-flag-failed ({})",
                                                        ScriptErrorString(verifyRes.unwrapErr()));

                    if (sourceBlockPtr) {
                        sourceBlockPtr->reject =
                            CBlockReject(REJECT_INVALID, msg, sourceBlockPtr->GetHash());
                    }
                    this->reject = CTransaction::CTxReject(REJECT_INVALID, msg, GetHash());
                    DoS(100, false);
                    return Err(
                        MakeInvalidTxState(TxValidationResult::TX_CONSENSUS, msg,
                                           fmt::format("ConnectInputs() : {} VerifySignature failed",
                                                       GetHash().ToString())));
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
            if (nValueIn < GetValueOut()) {
                if (sourceBlockPtr) {
                    sourceBlockPtr->reject =
                        CBlockReject(REJECT_INVALID, "bad-txns-in-belowout", sourceBlockPtr->GetHash());
                }
                DoS(100, false);
                return Err(
                    MakeInvalidTxState(TxValidationResult::TX_CONSENSUS, "bad-txns-in-belowout",
                                       fmt::format("ConnectInputs() : {} value in ({}) < value out ({})",
                                                   GetHash().ToString(), nValueIn, GetValueOut())));
            }

            // Tally transaction fees
            CAmount nTxFee = nValueIn - GetValueOut();
            if (nTxFee < 0) {
                DoS(100, false);
                return Err(MakeInvalidTxState(
                    TxValidationResult::TX_CONSENSUS, "bad-txns-fee-outofrange1",
                    fmt::format("ConnectInputs() : {} nTxFee < 0", GetHash().ToString())));
            }

            // enforce transaction fees for every block
            if (nTxFee < GetMinFee(txdb)) {
                if (fBlock) {
                    DoS(100, NLog.error("ConnectInputs() : {} not paying required fee={}, paid={}",
                                        GetHash().ToString(), FormatMoney(GetMinFee(txdb)),
                                        FormatMoney(nTxFee)));
                }
                return Err(MakeInvalidTxState(
                    TxValidationResult::TX_CONSENSUS, "bad-txns-fee-outofrange2",
                    fmt::format("ConnectInputs() : {} nTxFee < 0", GetHash().ToString())));
            }

            nFees += nTxFee;
            if (!MoneyRange(nFees)) {
                return Err(
                    MakeInvalidTxState(TxValidationResult::TX_CONSENSUS, "bad-txns-fee-outofrange3"));
            }
        }
    }

    return Ok();
}

// ppcoin: total coin age spent in transaction, in the unit of coin-days.
// Only those coins meeting minimum age requirement counts. As those
// transactions not in main chain are not currently indexed so we
// might not find out about their coin age. Older transactions are
// guaranteed to be in main chain by sync-checkpoint. This rule is
// introduced to help nodes establish a consistent view of the coin
// age (trust score) of competing branches.
bool CTransaction::GetCoinAge(const ITxDB& txdb, uint64_t& nCoinAge) const
{
    CBigNum      bnCentSecond = 0; // coin age in the unit of cent-seconds
    unsigned int nSMA         = Params().StakeMinAge(txdb);
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

        CAmount nValueIn = txPrev.vout[txin.prevout.n].nValue;
        bnCentSecond += CBigNum(nValueIn) * (nTime - txPrev.nTime) / CENT;

        if (fDebug)
            NLog.write(b_sev::debug, "coin age nValueIn={} nTimeDiff={} bnCentSecond={}", nValueIn,
                       nTime - txPrev.nTime, bnCentSecond.ToString());
    }

    CBigNum bnCoinDay = bnCentSecond * CENT / COIN / (24 * 60 * 60);
    if (fDebug)
        NLog.write(b_sev::debug, "coin age bnCoinDay={}", bnCoinDay.ToString());
    nCoinAge = bnCoinDay.getuint64();
    return true;
}

CTransaction CTransaction::FetchTxFromDisk(const uint256& txid)
{
    CTxDB txdb;
    return FetchTxFromDisk(txid, txdb);
}

CTransaction CTransaction::FetchTxFromDisk(const uint256& txid, const ITxDB& txdb)
{
    CTransaction result;
    CTxIndex     txPos;
    if (!txdb.ReadTxIndex(txid, txPos)) {
        NLog.write(b_sev::err, "Unable to read standard transaction from db: {}", txid.ToString());
        throw std::runtime_error("Unable to read standard transaction from db: " + txid.ToString());
    }
    if (!result.ReadFromDisk(txPos.pos, txdb)) {
        NLog.write(b_sev::err,
                   "Unable to read standard transaction from disk with the "
                   "index given by db: {}",
                   txid.ToString());
        throw std::runtime_error("Unable to read standard transaction from db: " + txid.ToString());
    }
    return result;
}

/**
 * Get all relevant keys of a transaction that are in our wallet. If an output number is defined, only
 * the key of that output is returned.
 * @brief CTransaction::GetThisWalletKeysOfTx
 * @param txid
 * @param outputNumber
 * @return the keys of the transaction given by txid that are in our wallet. If outputNumber is defined,
 * only that key will be returned, if it exists in our wallet
 */
std::vector<CKey> CTransaction::GetThisWalletKeysOfTx(const uint256&            txid,
                                                      boost::optional<unsigned> outputNumber)
{
    CTransaction tx;
    // first we try to find it in the mempool, if not found, we look on disk
    bool foundInMempool = mempool.lookup(txid, tx);
    if (!foundInMempool) {
        tx = CTransaction::FetchTxFromDisk(txid);
    }

    std::vector<CKey> keys;

    for (unsigned i = 0; i < tx.vout.size(); i++) {
        if (outputNumber.is_initialized() && *outputNumber != i) {
            continue;
        }
        const CTxOut&                     out = tx.vout[i];
        txnouttype                        outtype;
        std::vector<std::vector<uint8_t>> vSolutions;
        // this solution can be improved later for multiple kinds of transactions, here we only support
        // P2PKH transactions, more in CScript class's source file
        Solver(CTxDB(), out.scriptPubKey, outtype, vSolutions);
        if (outtype == TX_PUBKEYHASH) {
            CKeyID keyId = CKeyID(uint160(vSolutions[0]));
            if (!CBitcoinAddress(keyId).IsValid()) {
                continue;
            }
            CKey key;
            if (!pwalletMain->GetKey(keyId, key)) {
                continue;
            }
            // this is O(N^2), but this is OK, because the number of outputs is low
            // we're comparing public keys because CKey objects are not comparable
            if (std::find_if(keys.cbegin(), keys.cend(), [&key](const CKey& k) {
                    return k.GetPubKey() == key.GetPubKey();
                }) == keys.cend()) {
                keys.push_back(key);
            }
        }
    }

    // we don't retrieve input keys if the key of a specific output number is requested
    if (!outputNumber.is_initialized()) {
        for (unsigned i = 0; i < tx.vin.size(); i++) {
            const CTxIn&          in     = tx.vin[i];
            boost::optional<CKey> pubKey = CTransaction::GetPublicKeyFromScriptSig(in.scriptSig);
            if (!pubKey.is_initialized()) {
                continue;
            }

            CKey   key;
            CKeyID keyId = pubKey->GetPubKey().GetID();
            if (!pwalletMain->GetKey(keyId, key)) {
                continue;
            }

            // this is O(N^2), but this is OK, because the numebr of inputs is low
            // we're comparing public keys because CKey objects are not comparable
            if (std::find_if(keys.cbegin(), keys.cend(), [&key](const CKey& k) {
                    return k.GetPubKey() == key.GetPubKey();
                }) == keys.cend()) {
                keys.push_back(key);
            }
        }
    }

    return keys;
}

std::string CTransaction::DecryptMetadataOfTx(const StringViewT metadataStr, const uint256& txid,
                                              boost::optional<std::string>& error)
{
    std::vector<CKey> keysVec = GetThisWalletKeysOfTx(txid);
    if (keysVec.empty()) {
        error = "No valid keys were found in the transaction: " + txid.ToString();
        return "";
    }

    std::string decryptedMessage;
    for (const CKey& key : keysVec) {
        try {
            decryptedMessage = NTP1Script::DecryptMetadata(metadataStr, key);
            break;
        } catch (...) {
        }
    }
    if (decryptedMessage.empty()) {
        error = "None of the available keys in the following txid: " + txid.ToString() +
                " were able to decrypted the message: " + metadataStr.to_string();
        return "";
    }
    return decryptedMessage;
}

boost::optional<CKey> CTransaction::GetPublicKeyFromScriptSig(const CScript& scriptSig)
{
    opcodetype                 opt;
    auto                       beg = scriptSig.cbegin();
    std::vector<unsigned char> vchSig, vchPub;

    if (!scriptSig.GetOp(beg, opt, vchSig)) {
        return boost::none;
    }
    if (!scriptSig.GetOp(beg, opt, vchPub)) {
        return boost::none;
    }
    if (vchSig.empty() || !IsCanonicalSignature(vchSig)) {
        return boost::none;
    }
    if (vchPub.empty() || !IsCanonicalPubKey(vchPub)) {
        return boost::none;
    }

    CKey resultKey;
    if (!resultKey.SetPubKey(vchPub)) {
        return boost::none;
    }
    return resultKey;
}

boost::optional<CKey> CTransaction::GetOnePublicKeyFromInputs(const CTransaction& tx)
{
    for (const CTxIn& in : tx.vin) {
        boost::optional<CKey> res = GetPublicKeyFromScriptSig(in.scriptSig);
        if (res.is_initialized()) {
            return res;
        }
    }
    return boost::none;
}
