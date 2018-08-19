#ifndef NTP1SCRIPT_H
#define NTP1SCRIPT_H

#include <bitset>
#include <memory>
#include <set>
#include <string>
#include <vector>

typedef uint32_t          NTP1TransactionType;
const NTP1TransactionType NTP1TxType_UNKNOWN  = 0;
const NTP1TransactionType NTP1TxType_NOT_NTP1 = 1;
const NTP1TransactionType NTP1TxType_ISSUANCE = 2;
const NTP1TransactionType NTP1TxType_TRANSFER = 3;
const NTP1TransactionType NTP1TxType_BURN     = 4;

class NTP1Script
{
    std::string parsedScript;

public:
    enum TxType
    {
        TxType_None = 0,
        TxType_Issuance,
        TxType_Transfer,
        TxType_Burn
    };

protected:
    std::string headerBin;
    int         protocolVersion;
    std::string opCodeBin;

    TxType txType;

    void setCommonParams(std::string Header, int ProtocolVersion, std::string OpCodeBin);

public:
    struct TransferInstruction
    {
        TransferInstruction()
        {
            amount       = 0;
            skipInput    = false;
            outputIndex  = -1;
            firstRawByte = 1;
        }
        unsigned char firstRawByte;
        // transfer instructions act on inputs in order until they're empty, so instruction 0 will act on
        // input 0, and instruction 1 will act on input 0, etc... until input 0 is empty, or a skip
        // instruction is given to move to the next input
        bool         skipInput;
        unsigned int outputIndex;

        std::string rawAmount;
        uint64_t    amount;
    };

    struct IssuanceFlags
    {
        unsigned int divisibility;
        bool         locked; // no more issuing allowed
        enum AggregationPolicy
        {
            AggregationPolicy_Aggregatable,
            AggregationPolicy_NonAggregatable,
            AggregationPolicy_Unknown
        };
        AggregationPolicy aggregationPolicy;

        static const std::string AggregationPolicy_Aggregatable_Str;
        static const std::string AggregationPolicy_NonAggregatable_Str;

        static IssuanceFlags ParseIssuanceFlag(uint8_t flags);
        uint8_t              convertToByte() const
        {
            if (this->divisibility > 7) {
                throw std::runtime_error("Divisibility cannot be larger than 7");
            }

            std::bitset<3> divisibility_bits(static_cast<uint8_t>(this->divisibility));
            std::string    lockStatusStrBits = (this->locked ? "1" : "0");
            std::string    aggrPolicyStrBits;
            if (this->aggregationPolicy ==
                IssuanceFlags::AggregationPolicy::AggregationPolicy_Aggregatable) {
                aggrPolicyStrBits = "00";
            } else if (this->aggregationPolicy ==
                       IssuanceFlags::AggregationPolicy::AggregationPolicy_NonAggregatable) {
                aggrPolicyStrBits = "10";
            } else {
                throw std::runtime_error("Unknown aggregation policy:" +
                                         static_cast<int>(this->aggregationPolicy));
            }
            std::string issuanceFlagsBitsStr =
                divisibility_bits.to_string() + lockStatusStrBits + aggrPolicyStrBits + "00";
            if (issuanceFlagsBitsStr.size() != 8) {
                throw std::runtime_error("Error while constructing issuance flags");
            }
            std::bitset<8> issuanceFlagsBits(issuanceFlagsBitsStr);
            return static_cast<uint8_t>(issuanceFlagsBits.to_ulong());
        }
    };

    static std::string TransferInstructionToBinScript(const TransferInstruction& inst);

    virtual std::string calculateScriptBin() const = 0;
    /**
     * @brief getNTP1Indices
     * @return a list of all NTP1 output indices in this script
     */
    virtual std::set<unsigned int> getNTP1OutputIndices() const = 0;

    virtual ~NTP1Script() = default;
    static uint64_t    CalculateMetadataSize(const std::string& op_code_bin);
    static TxType      CalculateTxType(const std::string& op_code_bin);
    static uint64_t    CalculateAmountSize(uint8_t firstChar);
    static uint64_t    ParseAmountFromLongEnoughString(const std::string& BinAmountStartsAtByte0,
                                                       int&               rawSize);
    static std::string ParseOpCodeFromLongEnoughString(const std::string& BinOpCodeStartsAtByte0);
    static std::string ParseMetadataFromLongEnoughString(const std::string& BinMetadataStartsAtByte0,
                                                         const std::string& op_code_bin,
                                                         const std::string& wholeScriptHex = "");
    static std::string
    ParseTokenSymbolFromLongEnoughString(const std::string& BinTokenSymbolStartsAtByte0);
    static std::vector<TransferInstruction>
    ParseTransferInstructionsFromLongEnoughString(const std::string& BinInstructionsStartFromByte0,
                                                  int&               totalRawSize);

    std::string getHeader() const;
    std::string getOpCodeBin() const;
    TxType      getTxType() const;

    static std::shared_ptr<NTP1Script> ParseScript(const std::string& scriptHex);
    std::string                        getParsedScript() const;
};

#endif // NTP1SCRIPT_H
