#ifndef NTP1TRANSACTION_H
#define NTP1TRANSACTION_H

#include "main.h"
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
    unsigned int nFile;
    unsigned int nTxPos;

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
};

boost::filesystem::path NTP1TxsFilePath(unsigned int nFile);

extern unsigned int nCurrentNTP1TxsFile;

extern FILE* AppendNTP1TxsFile(unsigned int& nFileRet);

extern FILE* OpenNTP1TxsFile(unsigned int nFile, unsigned int nTxPos, const char* pszMode);

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

    void __manualSet(int NVersion, uint256 TxHash, std::vector<unsigned char> TxSerialized,
                     std::vector<NTP1TxIn> Vin, std::vector<NTP1TxOut> Vout, uint64_t NLockTime,
                     uint64_t NTime, NTP1TransactionType Ntp1TransactionType);

    void readNTP1DataFromTx(const CTransaction&                                          tx,
                            const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputsTxs);

    bool writeToDisk(unsigned int& nFileRet, unsigned int& nTxPosRet, FILE* customFile = nullptr);
    bool readFromDisk(DiskNTP1TxPos pos, FILE** pfileRet = nullptr, FILE* customFile = nullptr);
};

bool operator==(const NTP1Transaction& lhs, const NTP1Transaction& rhs)
{
    return (lhs.nVersion == rhs.nVersion && lhs.txHash == rhs.txHash &&
            lhs.txSerialized == rhs.txSerialized && lhs.vin == rhs.vin && lhs.vout == rhs.vout &&
            lhs.nLockTime == rhs.nLockTime && lhs.nTime == rhs.nTime &&
            lhs.ntp1TransactionType == rhs.ntp1TransactionType);
}

template <typename ScriptType>
void NTP1Transaction::__TransferTokens(
    const std::shared_ptr<ScriptType>& scriptPtrD, const CTransaction& tx,
    const std::vector<std::pair<CTransaction, NTP1Transaction>>& inputsTxs, bool burnOutput31)
{
    static_assert(std::is_same<ScriptType, NTP1Script_Transfer>::value ||
                      std::is_same<ScriptType, NTP1Script_Burn>::value,
                  "Script types can only be Transfer and Burn in this function");

    int currentInputIndex = 0;

    // calculate total tokens in inputs
    std::vector<std::vector<uint64_t>>        totalTokensLeftInInputs(inputsTxs.size());
    std::vector<std::vector<NTP1TokenTxData>> tokensKindsInInputs(inputsTxs.size());
    for (unsigned i = 0; i < tx.vin.size(); i++) {
        const auto& n    = tx.vin[i].prevout.n;
        const auto& hash = tx.vin[i].prevout.hash;

        auto it = std::find_if(inputsTxs.cbegin(), inputsTxs.cend(),
                               [this, i, &hash](const std::pair<CTransaction, NTP1Transaction>& in) {
                                   return in.first.GetHash() == hash;
                               });

        if (it == inputsTxs.end()) {
            throw std::runtime_error("Could not find all relevant inputs in the inputs list while "
                                     "attempting to calculate total required tokens");
        }

        if (it->second.getTxHash() != hash) {
            throw std::runtime_error(
                "Inputs in pair of CTransaction and NTP1Transaction don't have matching hashes");
        }

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

    for (int i = 0; i < (int)scriptPtrD->getTransferInstructionsCount(); i++) {

        // if skip, move on to the next input
        if (TIs[i].skipInput) {
            currentInputIndex++;
            continue;
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
        for (int j = 0; j < (int)totalTokensLeftInInputs[currentInputIndex].size(); j++) {

            uint64_t totalAdjacentTokensOfOneKind = totalTokensLeftInInputs[currentInputIndex][j];
            auto     currentTokenId = tokensKindsInInputs[currentInputIndex][j].getTokenId();
            for (int k = currentInputIndex + 1; k < (int)totalTokensLeftInInputs.size(); k++) {
                // an empty input in between means inputs are not adjacent
                if (totalTokensLeftInInputs[k].size() == 0) {
                    break;
                }
                for (int l = j + 1; l < (int)totalTokensLeftInInputs[k].size(); l++) {
                    if (tokensKindsInInputs[k][l].getTokenId() == currentTokenId) {
                        totalAdjacentTokensOfOneKind += totalTokensLeftInInputs[k][l];
                    } else {
                        break;
                    }
                }
            }

            if (currentOutputAmount > totalAdjacentTokensOfOneKind) {
                throw std::runtime_error(
                    "Insufficient tokens for transaction from inputs for transaction: " +
                    tx.GetHash().ToString() + " from input: " + ::ToString(j));
            }

            const auto&    currentTokenObj = tokensKindsInInputs[currentInputIndex][j];
            const uint64_t amountToCredit =
                std::min(totalTokensLeftInInputs[currentInputIndex][j], currentOutputAmount);

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
                for (int l = j; l < (int)totalTokensLeftInInputs[k].size(); l++) {
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
                if (totalTokensLeftInInputs[k].size() == 0 && amountLeftToSubtract > 0) {
                    throw std::runtime_error("Unable to decredit the available balances from inputs");
                }
            }

            // all required output amount is spent. The rest will be redirected as change
            if (currentOutputAmount == 0) {
                break;
            }

            if (totalTokensLeftInInputs[currentInputIndex][j] == 0) {
                currentInputIndex++;
            }

            // TODO: aggregation of adjacent tokens of the same kind
        }
    }

    for (int i = 0; i < (int)totalTokensLeftInInputs.size(); i++) {
        for (int j = 0; j < (int)totalTokensLeftInInputs[i].size(); j++) {
            if (totalTokensLeftInInputs[i][j] == 0) {
                continue;
            }

            const auto& currentTokenObj = tokensKindsInInputs[currentInputIndex][j];
            uint64_t    amountToCredit  = totalTokensLeftInInputs[currentInputIndex][j];

            // create the token object that will be added to the output
            NTP1TokenTxData ntp1tokenTxData;
            ntp1tokenTxData.setAmount(amountToCredit);
            ntp1tokenTxData.setTokenId(currentTokenObj.getTokenId());
            ntp1tokenTxData.setAggregationPolicy(currentTokenObj.getAggregationPolicy());
            ntp1tokenTxData.setDivisibility(currentTokenObj.getDivisibility());
            ntp1tokenTxData.setTokenSymbol(currentTokenObj.getTokenSymbol());
            ntp1tokenTxData.setLockStatus(currentTokenObj.getLockStatus());
            ntp1tokenTxData.setIssueTxIdHex(currentTokenObj.getIssueTxId().ToString());

            // add the token to the last output
            vout.back().tokens.push_back(ntp1tokenTxData);

            // reduce the available balance
            totalTokensLeftInInputs[currentInputIndex][j] -= amountToCredit;
        }
    }
}

#endif // NTP1TRANSACTION_H
