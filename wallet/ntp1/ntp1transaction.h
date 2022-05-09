#ifndef NTP1TRANSACTION_H
#define NTP1TRANSACTION_H

#include "ntp1/ntp1script.h"
#include "ntp1/ntp1script_burn.h"
#include "ntp1/ntp1script_issuance.h"
#include "ntp1/ntp1script_transfer.h"
#include "ntp1tokenmetadata.h"
#include "ntp1txin.h"
#include "ntp1txout.h"
#include "transaction.h"
#include "txindex.h"
#include "uint256.h"

#include <boost/filesystem/path.hpp>
#include <numeric>
#include <string>
#include <unordered_map>
#include <vector>

extern const std::array<std::array<uint8_t, 3>, 2> NTP1ScriptPossiblePrefixes;

class CTxOut;
class CTxIndex;

struct TokenMinimalData
{
    NTP1Int     amount;
    std::string tokenName;
    std::string tokenId;

    TokenMinimalData() : amount(0) {}
};

bool AreTokenSymbolsEquivalent(std::string lhs, std::string rhs);

/**
 * @brief The NTP1Transaction class
 * A single NTP1 transaction
 */
class NTP1Transaction
{
    static const int       CURRENT_VERSION = 1;
    int                    nVersion;
    uint256                txHash = 0;
    std::vector<NTP1TxIn>  vin;
    std::vector<NTP1TxOut> vout;
    uint64_t               nLockTime;
    uint64_t               nTime;
    NTP1TransactionType    ntp1TransactionType = NTP1TxType_NOT_NTP1;

    template <typename ScriptType>
    void __TransferTokens(int blockHeight, const std::shared_ptr<ScriptType>& scriptPtrD,
                          const CTransaction&                                          tx,
                          const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputsTxs,
                          bool                                                         burnOutput31);

public:
    static const uint64_t IssuanceFee = 1000000000; // 10 nebls

    // clang-format off
    IMPLEMENT_SERIALIZE(
                        READWRITE(this->nVersion);
                        nVersionIn = this->nVersion;
                        READWRITE(nTime);
                        READWRITE(txHash);
                        READWRITE(vin);
                        READWRITE(vout);
                        READWRITE(nLockTime);
                        READWRITE(ntp1TransactionType);
                        )
    // clang-format on

    NTP1Transaction();
    void                setNull();
    bool                isNull() const noexcept;
    void                importJsonData(const std::string& data);
    json_spirit::Value  exportDatabaseJsonData() const;
    void                importDatabaseJsonData(const json_spirit::Value& data);
    uint256             getTxHash() const;
    uint64_t            getLockTime() const;
    uint64_t            getTime() const;
    unsigned long       getTxInCount() const;
    const NTP1TxIn&     getTxIn(unsigned long index) const;
    unsigned long       getTxOutCount() const;
    const NTP1TxOut&    getTxOut(unsigned long index) const;
    NTP1TransactionType getTxType() const;
    std::string         getTokenSymbolIfIssuance() const;
    std::string         getTokenIdIfIssuance(std::string input0txid, unsigned int input0index) const;
    friend inline bool  operator==(const NTP1Transaction& lhs, const NTP1Transaction& rhs);

    static std::unordered_map<std::string, TokenMinimalData>
    CalculateTotalInputTokens(const NTP1Transaction& ntp1tx);
    static std::unordered_map<std::string, TokenMinimalData>
    CalculateTotalOutputTokens(const NTP1Transaction& ntp1tx);

    static json_spirit::Value GetNTP1IssuanceMetadata(int blockHeight, const uint256& issuanceTxid);
    static NTP1TokenMetaData  GetFullNTP1IssuanceMetadata(const CTransaction&    issuanceTx,
                                                          const NTP1Transaction& ntp1IssuanceTx);
    static NTP1TokenMetaData  GetFullNTP1IssuanceMetadata(int blockHeight, const uint256& issuanceTxid);

    static void
    ReorderTokenInputsToGoFirst(CTransaction&                                                tx,
                                const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputsTxs);

    static unsigned int
    CountTokenKindsInInputs(const CTransaction&                                          tx,
                            const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputsTxs);

    static void
    EnsureInputsHashesMatch(const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputsTxs);

    static void
    EnsureInputTokensRelateToTx(const CTransaction&                                          tx,
                                const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputsTxs);

    /**
     * from a list of previous transactions, get the pair of CTransaction/NTP1Transaction that matches
     * the given hash
     *
     * @brief GetPrevInputIt
     * @param tx is the current transaction; used only for hash information
     * @param hash
     * @param inputsTxs
     * @return
     */
    static std::vector<std::pair<CTransaction, NTP1Transaction>>::const_iterator
    GetPrevInputIt(const CTransaction& tx, const uint256& hash,
                   const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputsTxs);

    /**
     * takes a standard transaction and deals and tries to find NTP1-related problems in this transaction
     * and solve them; for example, NTP1 token change is located and is put in the transaction
     *
     * @brief ComplementStdTxWithNTP1
     * @param tx
     */
    static void AmendStdTxWithNTP1(const ITxDB& txdb, CTransaction& tx, int changeIndex);
    static void AmendStdTxWithNTP1(const int blockHeight, CTransaction& tx_,
                                   const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputs,
                                   int changeIndex);

    void __manualSet(int NVersion, uint256 TxHash, std::vector<NTP1TxIn> Vin,
                     std::vector<NTP1TxOut> Vout, uint64_t NLockTime, uint64_t NTime,
                     NTP1TransactionType Ntp1TransactionType);

    static bool IsScriptNTP1(const std::vector<uint8_t>& opReturnScript);

    std::vector<uint8_t> getNTP1OpReturnScript() const;
    std::string          getNTP1OpReturnScriptHex() const;

    /**
     * sets only shallow information from the source transaction (no token information)
     * it's not possible to set token information without prev inputs information; for that, use the
     * non-minimal version
     * @brief readNTP1DataFromTx_minimal
     * @param tx the source Neblio transcation
     */
    void readNTP1DataFromTx_minimal(int blockHeight, const CTransaction& tx);

    void readNTP1DataFromTx(int blockHeight, const CTransaction& tx,
                            const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputsTxs);

    static bool IsTxNTP1(const CTransaction* tx, std::string* opReturnArg = nullptr);

    /** for a certain transaction, retrieve all NTP1 data from the database */
    static std::vector<std::pair<CTransaction, NTP1Transaction>>
    GetAllNTP1InputsOfTx(CTransaction tx, const ITxDB& txdb, bool recoverProtection,
                         int recursionCount = 0);

    static std::vector<std::pair<CTransaction, NTP1Transaction>> GetAllNTP1InputsOfTx(
        CTransaction tx, const ITxDB& txdb, bool recoverProtection,
        const std::map<uint256, std::vector<std::pair<CTransaction, NTP1Transaction>>>&
                                           mapQueuedNTP1Inputs,
        const std::map<uint256, CTxIndex>& queuedAcceptedTxs = std::map<uint256, CTxIndex>(),
        int                                recursionCount    = 0);

    /** Take a list of standard neblio transactions and return pairs of neblio and NTP1 transactions */
    static std::vector<std::pair<CTransaction, NTP1Transaction>> StdFetchedInputTxsToNTP1(
        const CTransaction& tx, const MapPrevTx& mapInputs, const ITxDB& txdb, bool recoverProtection,
        const std::map<uint256, CTxIndex>& queuedAcceptedTxs = std::map<uint256, CTxIndex>(),
        int                                recursionCount    = 0);

    static std::vector<std::pair<CTransaction, NTP1Transaction>> StdFetchedInputTxsToNTP1(
        const CTransaction& tx, const MapPrevTx& mapInputs, const ITxDB& txdb, bool recoverProtection,
        const std::map<uint256, std::vector<std::pair<CTransaction, NTP1Transaction>>>&
                                           mapQueuedNTP1Inputs,
        const std::map<uint256, CTxIndex>& queuedAcceptedTxs = std::map<uint256, CTxIndex>(),
        int                                recursionCount    = 0);

    static int GetCurrentBlockHeight(const ITxDB& txdb);
};

bool operator==(const NTP1Transaction& lhs, const NTP1Transaction& rhs)
{
    return (lhs.nVersion == rhs.nVersion && lhs.txHash == rhs.txHash && lhs.vin == rhs.vin &&
            lhs.vout == rhs.vout && lhs.nLockTime == rhs.nLockTime && lhs.nTime == rhs.nTime &&
            lhs.ntp1TransactionType == rhs.ntp1TransactionType);
}

/** given a neblio tx, get the corresponding NTP1 tx */
void FetchNTP1TxFromDisk(std::pair<CTransaction, NTP1Transaction>& txPair, const ITxDB& txdb,
                         bool recoverProtection, unsigned recurseDepth = 0);

#endif // NTP1TRANSACTION_H
