#ifndef NTP1SCRIPT_H
#define NTP1SCRIPT_H

#include <memory>
#include <string>
#include <vector>

typedef uint32_t          NTP1TransactionType;
const NTP1TransactionType NTP1TxType_INVALID  = 0;
const NTP1TransactionType NTP1TxType_NOT_NTP1 = 1;
const NTP1TransactionType NTP1TxType_ISSUANCE = 2;
const NTP1TransactionType NTP1TxType_TRANSFER = 3;
const NTP1TransactionType NTP1TxType_BURN     = 4;

class NTP1Script
{
public:
    enum TxType
    {
        TxType_None = 0,
        TxType_Issuance,
        TxType_Transfer,
        TxType_Burn
    };

protected:
    std::string header;
    int         protocolVersion;
    std::string opCodeBin;

    TxType txType;

    void setCommonParams(std::string Header, int ProtocolVersion, std::string OpCodeBin);

public:
    struct TransferInstruction
    {
        unsigned char firstRawByte;
        // transfer instructions act on inputs in order until they're empty, so instruction 0 will act on
        // input 0, and instruction 1 will act on input 0, etc... until input 0 is empty, or a skip
        // instruction is given to move to the next input
        bool skipInput;
        int  outputIndex;

        std::string rawAmount;
        uint64_t    amount;
    };

    struct IssuanceFlags
    {
        int  divisibility;
        bool locked; // no more issuing allowed
        enum AggregationPolicy
        {
            AggregationPolicy_Aggregatable,
            AggregationPolicy_NonAggregatable,
            AggregationPolicy_Unknown
        };
        AggregationPolicy aggregationPolicty;

        static IssuanceFlags ParseIssuanceFlag(uint8_t flags);
    };

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
};

#endif // NTP1SCRIPT_H
