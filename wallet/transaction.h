#ifndef TRANSACTION_H
#define TRANSACTION_H

#include "blockreject.h"
#include "globals.h"
#include "inpoint.h"
#include "outpoint.h"
#include "serialize.h"
#include "txdb.h"
#include "txin.h"
#include "txindex.h"
#include "txout.h"
#include "uint256.h"
#include "validation.h"
#include <vector>

class CTransaction;

enum GetMinFee_mode
{
    GMF_BLOCK,
    GMF_RELAY,
    GMF_SEND,
};

using MapPrevTx = std::map<uint256, std::pair<CTxIndex, CTransaction>>;

/** The basic transaction that is broadcasted on the network and contained in
 * blocks.  A transaction can contain multiple inputs and outputs.
 */
class CTransaction
{
public:
    static const int    CURRENT_VERSION = 1;
    int                 nVersion;
    unsigned int        nTime;
    std::vector<CTxIn>  vin;
    std::vector<CTxOut> vout;
    unsigned int        nLockTime;

    struct CTxReject
    {
        unsigned char chRejectCode;
        std::string   strRejectReason;
        uint256       hashTx;
        CTxReject(int rejectCode = 0, const std::string& rejectReason = "", const uint256& txHash = 0)
            : chRejectCode(static_cast<unsigned char>(rejectCode)), strRejectReason(rejectReason),
              hashTx(txHash)
        {
        }
    };

    mutable boost::optional<CTxReject> reject;

    // Denial-of-service detection:
    mutable int nDoS;
    bool        DoS(int nDoSIn, bool fIn) const
    {
        nDoS += nDoSIn;
        return fIn;
    }

    CTransaction() { SetNull(); }

    // clang-format off
    IMPLEMENT_SERIALIZE(
                        READWRITE(this->nVersion);
                        nVersion = this->nVersion;
                        READWRITE(nTime);
                        READWRITE(vin);
                        READWRITE(vout);
                        READWRITE(nLockTime);
                        )
    // clang-format on

    void SetNull();

    bool IsNull() const { return (vin.empty() && vout.empty()); }

    uint256 GetHash() const;

    bool IsNewerThan(const CTransaction& old) const;

    bool IsCoinBase() const { return (vin.size() == 1 && vin[0].prevout.IsNull() && vout.size() >= 1); }

    bool CheckColdStake(const CScript& script) const;

    bool IsCoinStake() const;

    bool HasP2CSOutputs() const;

    /** Check for standard transaction types
        @param[in] mapInputs	Map of previous transactions that have outputs we're spending
        @return True if all inputs (scriptSigs) use only standard transaction forms
        @see CTransaction::FetchInputs
    */
    bool AreInputsStandard(const MapPrevTx& mapInputs) const;

    /** Count ECDSA signature operations the old-fashioned (pre-0.6) way
        @return number of sigops this transaction's outputs will produce when spent
        @see CTransaction::FetchInputs
    */
    unsigned int GetLegacySigOpCount() const;

    /** Count ECDSA signature operations in pay-to-script-hash inputs.

    @param[in] mapInputs	Map of previous transactions that have outputs we're spending
        @return maximum number of sigops required to validate this transaction's inputs
        @see CTransaction::FetchInputs
        */
    unsigned int GetP2SHSigOpCount(const MapPrevTx& mapInputs) const;

    /** Amount of bitcoins spent by this transaction.
        @return sum of all outputs (note: does not include fees)
     */
    int64_t GetValueOut() const;

    /** Amount of bitcoins coming in to this transaction
        Note that lightweight clients may not know anything besides the hash of previous transactions,
        so may not be able to calculate this.

    @param[in] mapInputs	Map of previous transactions that have outputs we're spending
        @return	Sum of value of all inputs (scriptSigs)
            @see CTransaction::FetchInputs
        */
    int64_t GetValueIn(const MapPrevTx& mapInputs) const;

    int64_t GetMinFee(const ITxDB& txdb, unsigned int nBlockSize = 1,
                      enum GetMinFee_mode mode = GMF_BLOCK, unsigned int nBytes = 0, bool sigsIncluded = true) const;

    bool ReadFromDisk(CDiskTxPos pos, const ITxDB& txdb);

    friend bool operator==(const CTransaction& a, const CTransaction& b)
    {
        return (a.nVersion == b.nVersion && a.nTime == b.nTime && a.vin == b.vin && a.vout == b.vout &&
                a.nLockTime == b.nLockTime);
    }

    friend bool operator!=(const CTransaction& a, const CTransaction& b) { return !(a == b); }

    std::string ToStringShort() const;

    std::string ToString() const;

    void print() const;

    bool ReadFromDisk(const ITxDB& txdb, COutPoint prevout, CTxIndex& txindexRet);
    bool ReadFromDisk(CTxDB& txdb, COutPoint prevout);
    bool DisconnectInputs(CTxDB& txdb);

    /** Fetch from memory and/or disk. inputsRet keys are transaction hashes.

    @param[in] txdb	Transaction database
            @param[in] mapTestPool	List of pending changes to the transaction index database
            @param[in] fBlock	True if being called to add a new best-block to the chain
            @param[in] fMiner	True if being called by CreateNewBlock
            @param[out] inputsRet	Pointers to this transaction's inputs
            @param[out] fInvalid	returns true if transaction is invalid
            @return	Returns true if all inputs are in txdb or mapTestPool
                */
    bool FetchInputs(const ITxDB& txdb, const std::map<uint256, CTxIndex>& mapTestPool, bool fBlock,
                     bool fMiner, MapPrevTx& inputsRet, bool& fInvalid) const;

    /** Sanity check previous transactions, then, if all checks succeed,
        mark them as spent by this transaction.

    @param[in] inputs	Previous transactions (from FetchInputs)
            @param[out] mapTestPool	Keeps track of inputs that need to be updated on disk
        @param[in] posThisTx	Position of this transaction on disk
        @param[in] pindexBlock
        @param[in] fBlock	true if called from ConnectBlock
        @param[in] fMiner	true if called from CreateNewBlock
        @return Returns true if all checks succeed
        */
    Result<void, TxValidationState>
    ConnectInputs(const ITxDB& txdb, MapPrevTx inputs, std::map<uint256, CTxIndex>& mapTestPool,
                  const CDiskTxPos& posThisTx, const boost::optional<CBlockIndex>& pindexBlock,
                  bool fBlock, bool fMiner, CBlock* sourceBlockPtr = nullptr) const;
    Result<void, TxValidationState> CheckTransaction(const ITxDB& txdb,
                                                     CBlock*      sourceBlock = nullptr) const;
    bool GetCoinAge(const ITxDB& txdb, uint64_t& nCoinAge) const; // ppcoin: get transaction coin age

    [[nodiscard]] static CTransaction FetchTxFromDisk(const uint256& txid);
    [[nodiscard]] static CTransaction FetchTxFromDisk(const uint256& txid, const ITxDB& txdb);

    [[nodiscard]] static std::vector<CKey>
    GetThisWalletKeysOfTx(const uint256& txid, boost::optional<unsigned> outputNumber = boost::none);
    [[nodiscard]] static std::string DecryptMetadataOfTx(const StringViewT             metadataStr,
                                                         const uint256&                txid,
                                                         boost::optional<std::string>& error);

    [[nodiscard]] static boost::optional<CKey> GetPublicKeyFromScriptSig(const CScript& scriptSig);
    [[nodiscard]] static boost::optional<CKey> GetOnePublicKeyFromInputs(const CTransaction& tx);

protected:
    const CTxOut& GetOutputFor(const CTxIn& input, const MapPrevTx& inputs) const;
};

#endif // TRANSACTION_H
