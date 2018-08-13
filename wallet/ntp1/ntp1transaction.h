#ifndef NTP1TRANSACTION_H
#define NTP1TRANSACTION_H

#include "main.h"
#include "ntp1/ntp1script.h"
#include "ntp1/ntp1script_burn.h"
#include "ntp1/ntp1script_issuance.h"
#include "ntp1/ntp1script_transfer.h"
#include "ntp1txin.h"
#include "ntp1txout.h"
#include "uint256.h"

#include <boost/filesystem/path.hpp>
#include <numeric>
#include <string>
#include <vector>

/** Position on disk for a particular transaction. */
class DiskNTP1TxPos
{
public:
    static unsigned int nCurrentNTP1TxsFile;
    unsigned int        nFile;
    unsigned int        nTxPos;

    DiskNTP1TxPos() { SetNull(); }

    DiskNTP1TxPos(unsigned int nFileIn, unsigned int nTxPosIn)
    {
        nFile  = nFileIn;
        nTxPos = nTxPosIn;
    }

    IMPLEMENT_SERIALIZE(READWRITE(FLATDATA(*this));)
    void SetNull()
    {
        nFile  = (unsigned int)-1;
        nTxPos = 0;
    }
    bool IsNull() const { return (nFile == (unsigned int)-1); }

    friend bool operator==(const DiskNTP1TxPos& a, const DiskNTP1TxPos& b)
    {
        return (a.nFile == b.nFile && a.nTxPos == b.nTxPos);
    }

    friend bool operator!=(const DiskNTP1TxPos& a, const DiskNTP1TxPos& b) { return !(a == b); }

    std::string ToString() const
    {
        if (IsNull())
            return "null";
        else
            return strprintf("(nFile=%u, nTxPos=%u)", nFile, nTxPos);
    }

    void print() const { printf("%s", ToString().c_str()); }

    static boost::filesystem::path NTP1TxsFilePath(unsigned int nFile);

    static FILE* AppendNTP1TxsFile(unsigned int& nFileRet);

    static FILE* OpenNTP1TxsFile(unsigned int nFile, unsigned int nTxPos, const char* pszMode);
};

/**
 * @brief The NTP1Transaction class
 * A single NTP1 transaction
 */
class NTP1Transaction
{
    static const int           CURRENT_VERSION = 1;
    int                        nVersion;
    uint256                    txHash;
    std::vector<unsigned char> txSerialized;
    std::vector<NTP1TxIn>      vin;
    std::vector<NTP1TxOut>     vout;
    uint64_t                   nLockTime;
    uint64_t                   nTime;
    NTP1TransactionType        ntp1TransactionType = NTP1TxType_NOT_NTP1;

    template <typename ScriptType>
    void __TransferTokens(const std::shared_ptr<ScriptType>& scriptPtrD, const CTransaction& tx,
                          const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputsTxs,
                          bool                                                         burnOutput31);

    static const uint64_t IssuanceFee = 1000000000; // 10 nebls

public:
    // clang-format off
    IMPLEMENT_SERIALIZE(
                        READWRITE(this->nVersion);
                        nVersion = this->nVersion;
                        READWRITE(nTime);
                        READWRITE(txHash);
                        READWRITE(vin);
                        READWRITE(vout);
                        READWRITE(nLockTime);
                        READWRITE(ntp1TransactionType);
                        )
    // clang-format on

    NTP1Transaction();
    void               setNull();
    bool               isNull() const;
    void               importJsonData(const std::string& data);
    json_spirit::Value exportDatabaseJsonData() const;
    void               importDatabaseJsonData(const json_spirit::Value& data);
    void               setHex(const std::string& Hex);
    std::string        getHex() const;
    uint256            getTxHash() const;
    uint64_t           getLockTime() const;
    uint64_t           getTime() const;
    unsigned long      getTxInCount() const;
    const NTP1TxIn&    getTxIn(unsigned long index) const;
    unsigned long      getTxOutCount() const;
    const NTP1TxOut&   getTxOut(unsigned long index) const;
    friend inline bool operator==(const NTP1Transaction& lhs, const NTP1Transaction& rhs);

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
    static void AmendStdTxWithNTP1(CTransaction& tx, int changeIndex);
    static void AmendStdTxWithNTP1(CTransaction&                                                tx_,
                                   const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputs,
                                   int changeIndex);

    void __manualSet(int NVersion, uint256 TxHash, std::vector<unsigned char> TxSerialized,
                     std::vector<NTP1TxIn> Vin, std::vector<NTP1TxOut> Vout, uint64_t NLockTime,
                     uint64_t NTime, NTP1TransactionType Ntp1TransactionType);

    /**
     * sets only shallow information from the source transaction (no token information)
     * it's not possible to set token information without prev inputs information; for that, use the
     * non-minimal version
     * @brief readNTP1DataFromTx_minimal
     * @param tx the source Neblio transcation
     */
    void readNTP1DataFromTx_minimal(const CTransaction& tx);

    void readNTP1DataFromTx(const CTransaction&                                          tx,
                            const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputsTxs);

    bool writeToDisk(unsigned int& nFileRet, unsigned int& nTxPosRet, FILE* customFile = nullptr) const;
    bool readFromDisk(DiskNTP1TxPos pos, FILE** pfileRet = nullptr, FILE* customFile = nullptr);
};

bool operator==(const NTP1Transaction& lhs, const NTP1Transaction& rhs)
{
    return (lhs.nVersion == rhs.nVersion && lhs.txHash == rhs.txHash &&
            lhs.txSerialized == rhs.txSerialized && lhs.vin == rhs.vin && lhs.vout == rhs.vout &&
            lhs.nLockTime == rhs.nLockTime && lhs.nTime == rhs.nTime &&
            lhs.ntp1TransactionType == rhs.ntp1TransactionType);
}

// TODO: aggregate only if aggregation policy allows so

template <typename ScriptType>
void NTP1Transaction::__TransferTokens(
    const std::shared_ptr<ScriptType>& scriptPtrD, const CTransaction& tx,
    const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputsTxs, bool burnOutput31)
{
    static_assert(std::is_same<ScriptType, NTP1Script_Transfer>::value ||
                      std::is_same<ScriptType, NTP1Script_Burn>::value,
                  "Script types can only be Transfer and Burn in this function");

    int currentInputIndex = 0;

    EnsureInputsHashesMatch(inputsTxs);
    EnsureInputTokensRelateToTx(tx, inputsTxs);

    // calculate total tokens in inputs
    std::vector<std::vector<uint64_t>>        totalTokensLeftInInputs(tx.vin.size());
    std::vector<std::vector<NTP1TokenTxData>> tokensKindsInInputs(tx.vin.size());
    for (unsigned i = 0; i < tx.vin.size(); i++) {
        const auto& n    = tx.vin[i].prevout.n;
        const auto& hash = tx.vin[i].prevout.hash;

        auto it = GetPrevInputIt(tx, hash, inputsTxs);

        const NTP1Transaction& ntp1tx = it->second;
        // this array keeps track of all tokens left
        totalTokensLeftInInputs[i].resize(ntp1tx.getTxOut(n).getNumOfTokens());
        // this object relates tokens in the last list to their corresponding information
        tokensKindsInInputs[i].resize(ntp1tx.getTxOut(n).getNumOfTokens());
        // loop over tokens of a single input (outputs from previous transactions)
        for (int j = 0; j < (int)ntp1tx.getTxOut(n).getNumOfTokens(); j++) {
            const auto& tokenObj          = ntp1tx.getTxOut(n).getToken(j);
            totalTokensLeftInInputs[i][j] = tokenObj.getAmount();
            tokensKindsInInputs[i][j]     = tokenObj;
        }
    }

    std::vector<NTP1Script::TransferInstruction> TIs = scriptPtrD->getTransferInstructions();

    // the operation is invalid if input 0 has no tokens; this artificial failure is forced for this to
    // be compliant with the API
    bool invalid = false;
    if (totalTokensLeftInInputs.size() == 0) {
        invalid = true;
    } else {
        uint64_t totalTokensInInput0 =
            std::accumulate(totalTokensLeftInInputs[0].begin(), totalTokensLeftInInputs[0].end(), 0);
        invalid = !totalTokensInInput0;
    }

    for (int i = 0; i < (int)scriptPtrD->getTransferInstructionsCount(); i++) {
        // if the transaction is invalid, break here and go straight to change calculations
        if (invalid) {
            break;
        }

        if (currentInputIndex >= static_cast<int>(vin.size())) {
            throw std::runtime_error(
                "An input of transfer instruction is outside the available "
                "range of inputs in NTP1 OP_RETURN argument: " +
                scriptPtrD->getParsedScript() + ", where the number of available inputs is " +
                ::ToString(tx.vin.size()) + " in transaction " + tx.GetHash().ToString());
        }

        const int outputIndex    = TIs[i].outputIndex;
        bool      burnThisOutput = (burnOutput31 && outputIndex == 31);

        if (outputIndex >= static_cast<int>(vout.size()) && !burnThisOutput) {
            throw std::runtime_error(
                "An output of transfer instruction is outside the available "
                "range of outputs in NTP1 OP_RETURN argument: " +
                scriptPtrD->getParsedScript() + ", where the number of available outputs is " +
                ::ToString(tx.vout.size()) + " in transaction " + tx.GetHash().ToString());
        }

        // loop over the kinds of tokens in the input and distribute them over outputs
        // note: there's no way to switch from one token to the next unless its content depletes
        uint64_t currentOutputAmount = TIs[i].amount;

        //  token index at which to start subtraction, helps in skipping empty tokens when
        // subtracting spent amount
        int startTokenIndex = 0;

        // if input is empty, just move to the next one since empty inputs don't break adjacency
        bool     stopInstructions = false;
        uint64_t totalTokensInCurrentInput =
            std::accumulate(totalTokensLeftInInputs[currentInputIndex].begin(),
                            totalTokensLeftInInputs[currentInputIndex].end(), 0);
        while (totalTokensLeftInInputs[currentInputIndex].size() == 0 ||
               totalTokensInCurrentInput == 0) {
            currentInputIndex++;
            if (currentInputIndex >= static_cast<int>(vin.size())) {
                stopInstructions = true;
                break;
            }
            totalTokensInCurrentInput =
                std::accumulate(totalTokensLeftInInputs[currentInputIndex].begin(),
                                totalTokensLeftInInputs[currentInputIndex].end(), 0);
        }

        if (stopInstructions) {
            break;
        }

        for (int j = 0; j < (int)totalTokensLeftInInputs[currentInputIndex].size(); j++) {

            // calculate the total number of available tokens for spending
            uint64_t    totalAdjacentTokensOfOneKind = 0;
            std::string currentTokenId;
            bool        inputDone = false;
            for (int k = currentInputIndex; k < (int)totalTokensLeftInInputs.size(); k++) {
                // an empty input in between doesn't break adjacency
                //                if (totalTokensLeftInInputs[k].size() == 0) {
                //                    break;
                //                }
                for (int l = (k == currentInputIndex ? j : 0);
                     l < (int)totalTokensLeftInInputs[k].size(); l++) {
                    // if the amount of tokens currently is zero, it's simply skipped and the token ID is
                    // changed to the next adjacent kind
                    if (tokensKindsInInputs[k][l].getTokenId() != currentTokenId &&
                        totalAdjacentTokensOfOneKind > 0) {
                        inputDone = true;
                        break;
                    } else {
                        if (totalAdjacentTokensOfOneKind == 0) {
                            startTokenIndex = l;
                            currentTokenId  = tokensKindsInInputs[currentInputIndex][l].getTokenId();
                        }
                        totalAdjacentTokensOfOneKind += totalTokensLeftInInputs[k][l];
                    }
                }
                if (inputDone) {
                    break;
                }
            }

            // ensure that gaps won't create a problem; an empty input is automatically skipped here
            if (totalAdjacentTokensOfOneKind == 0) {
                if (j + 1 >= (int)totalTokensLeftInInputs[currentInputIndex].size()) {
                    currentInputIndex++;
                    j = -1; // resets j to 0 in the next iteration
                }
                continue;
            }

            if (currentOutputAmount > totalAdjacentTokensOfOneKind) {
                throw std::runtime_error(
                    "Insufficient tokens for transaction from inputs for transaction: " +
                    tx.GetHash().ToString() + " from input: " + ::ToString(currentInputIndex) +
                    ". Required output amount: " + ::ToString(currentOutputAmount) +
                    "; and the total available amount in all (possibly adjacent) inputs: " +
                    ::ToString(totalAdjacentTokensOfOneKind) +
                    "; in transfer instruction number: " + ::ToString(i));
            }

            const auto&    currentTokenObj = tokensKindsInInputs[currentInputIndex][startTokenIndex];
            const uint64_t amountToCredit  = std::min(totalAdjacentTokensOfOneKind, currentOutputAmount);

            if (!burnThisOutput) {
                // create the token object that will be added to the output
                NTP1TokenTxData ntp1tokenTxData;
                ntp1tokenTxData.setAmount(amountToCredit);
                ntp1tokenTxData.setTokenId(currentTokenObj.getTokenId());
                ntp1tokenTxData.setAggregationPolicy(currentTokenObj.getAggregationPolicy());
                ntp1tokenTxData.setDivisibility(currentTokenObj.getDivisibility());
                ntp1tokenTxData.setTokenSymbol(currentTokenObj.getTokenSymbol());
                ntp1tokenTxData.setLockStatus(currentTokenObj.getLockStatus());
                ntp1tokenTxData.setIssueTxIdHex(currentTokenObj.getIssueTxId().ToString());

                // add the token to the output
                vout[outputIndex].tokens.push_back(ntp1tokenTxData);
            }

            // reduce the available output amount
            if (amountToCredit > currentOutputAmount) {
                currentOutputAmount = 0;
            } else {
                currentOutputAmount -= amountToCredit;
            }

            // reduce the available balance from the array that tracks all available inputs
            uint64_t amountLeftToSubtract = amountToCredit;
            for (int k = currentInputIndex; k < (int)totalTokensLeftInInputs.size(); k++) {
                // an empty input in between means inputs are not adjacent
                for (int l = (k == currentInputIndex ? startTokenIndex : 0);
                     l < (int)totalTokensLeftInInputs[k].size(); l++) {
                    if (tokensKindsInInputs[k][l].getTokenId() == currentTokenId) {
                        if (totalTokensLeftInInputs[k][l] >= amountLeftToSubtract) {
                            totalTokensLeftInInputs[k][l] -= amountLeftToSubtract;
                            amountLeftToSubtract = 0;
                        } else {
                            amountLeftToSubtract -= totalTokensLeftInInputs[k][l];
                            totalTokensLeftInInputs[k][l] = 0;
                        }
                    } else {
                        if (amountLeftToSubtract == 0) {
                            break;
                        } else {
                            throw std::runtime_error(
                                "Unable to decredit the available balances from inputs");
                        }
                    }
                    if (amountLeftToSubtract == 0) {
                        break;
                    }
                }
                if (amountLeftToSubtract == 0) {
                    break;
                }
            }
            if (amountLeftToSubtract > 0) {
                throw std::runtime_error("Unable to decredit the available balances from inputs");
            }

            // if skip, move on to the next input
            if (TIs[i].skipInput) {
                currentInputIndex++;
            }

            uint64_t totalTokensLeftInCurrentInput =
                std::accumulate(totalTokensLeftInInputs[currentInputIndex].begin(),
                                totalTokensLeftInInputs[currentInputIndex].end(), 0);
            if (totalTokensLeftInCurrentInput == 0) {
                // avoid incrementing twice
                if (!TIs[i].skipInput) {
                    currentInputIndex++;
                }
            }

            // all required output amount is spent. The rest will be redirected as change
            if (currentOutputAmount == 0) {
                break;
            }
        }
    }

    for (int i = 0; i < (int)totalTokensLeftInInputs.size(); i++) {
        for (int j = 0; j < (int)totalTokensLeftInInputs[i].size(); j++) {
            if (totalTokensLeftInInputs[i][j] == 0) {
                continue;
            }

            const auto& currentTokenObj = tokensKindsInInputs[i][j];
            uint64_t    amountToCredit  = totalTokensLeftInInputs[i][j];

            // create the token object that will be added to the output
            NTP1TokenTxData ntp1tokenTxData;
            ntp1tokenTxData.setAmount(amountToCredit);
            ntp1tokenTxData.setTokenId(currentTokenObj.getTokenId());
            ntp1tokenTxData.setAggregationPolicy(currentTokenObj.getAggregationPolicy());
            ntp1tokenTxData.setDivisibility(currentTokenObj.getDivisibility());
            ntp1tokenTxData.setTokenSymbol(currentTokenObj.getTokenSymbol());
            ntp1tokenTxData.setLockStatus(currentTokenObj.getLockStatus());
            ntp1tokenTxData.setIssueTxIdHex(currentTokenObj.getIssueTxId().ToString());

            if (vout.size() == 0) {
                throw std::runtime_error("No outputs in transaction: " + tx.GetHash().ToString());
            }

            // reduce the available balance
            totalTokensLeftInInputs[i][j] -= amountToCredit;
            amountToCredit = 0;

            bool stopLooping = false;
            for (int k = i; k < (int)totalTokensLeftInInputs.size(); k++) {
                for (int l = j; l < (int)totalTokensLeftInInputs[k].size(); l++) {
                    if (k == i && l == j) {
                        continue; // ignore the balance that was already credited
                    }
                    if (tokensKindsInInputs[i][j].getTokenId() !=
                        tokensKindsInInputs[k][l].getTokenId()) {
                        stopLooping = true;
                        break;
                    }
                    amountToCredit                = totalTokensLeftInInputs[k][l];
                    totalTokensLeftInInputs[k][l] = 0;
                    ntp1tokenTxData.setAmount(ntp1tokenTxData.getAmount() + amountToCredit);
                }

                // stop the outer loop
                if (stopLooping) {
                    break;
                }
            }

            // add the token to the last output
            vout.back().tokens.push_back(ntp1tokenTxData);
        }
    }

    // delete empty token slots
    for (int i = 0; i < (int)vout.size(); i++) {
        for (int j = 0; j < (int)vout[i].getNumOfTokens(); j++) {
            if (vout[i].getToken(j).getAmount() == 0) {
                vout[i].tokens.erase(vout[i].tokens.begin() + j);
                j--;
            }
        }
    }
}

#endif // NTP1TRANSACTION_H
