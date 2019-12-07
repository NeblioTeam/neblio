#ifndef NTP1SCRIPT_H
#define NTP1SCRIPT_H

#include "boost/algorithm/string.hpp"
#include "crypto_highlevel.h"
#include "json_spirit.h"
#include <bitset>
#include <boost/algorithm/hex.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/regex.hpp>
#include <cmath>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <vector>

using NTP1Int = boost::multiprecision::cpp_int;

// You should NEVER change these without changing the database version
// These go to the database for verifying issuance transactions duplication
typedef uint32_t          NTP1TransactionType;
const NTP1TransactionType NTP1TxType_UNKNOWN  = 0;
const NTP1TransactionType NTP1TxType_NOT_NTP1 = 1;
const NTP1TransactionType NTP1TxType_ISSUANCE = 2;
const NTP1TransactionType NTP1TxType_TRANSFER = 3;
const NTP1TransactionType NTP1TxType_BURN     = 4;

constexpr const char* METADATA_SER_FIELD__VERSION               = "SerializationVersion";
constexpr const char* METADATA_SER_FIELD__TARGET_PUBLIC_KEY_HEX = "TargetPubKeyHex";
constexpr const char* METADATA_SER_FIELD__SOURCE_PUBLIC_KEY_HEX = "SourcePubKeyHex";
constexpr const char* METADATA_SER_FIELD__CIPHER_BASE64         = "Cipher64";

const std::string  HexBytesRegexStr("^([0-9a-fA-F][0-9a-fA-F])+$");
const boost::regex HexBytexRegex(HexBytesRegexStr);

const NTP1Int NTP1MaxAmount = std::numeric_limits<int64_t>::max();

class CKey;
class CTransaction;
class NTP1SendTokensOneRecipientData;

struct RawNTP1MetadataBeforeSend
{
    RawNTP1MetadataBeforeSend(std::string Metadata = "", bool DoEncrypt = false)
    {
        metadata = std::move(Metadata);
        encrypt  = DoEncrypt;
    }
    bool        encrypt = false;
    std::string metadata;

    /**
     * @param ntp1metadata
     * @param wtxNew
     * @param ntp1TxData
     * @return If RawNTP1MetadataBeforeSend has encrypt set to true, the message will be encrypted and
     * returned. Otherwise, it'll be returned as is
     */
    std::string
    applyMetadataEncryption(const CTransaction&                                wtxNew,
                            const std::vector<NTP1SendTokensOneRecipientData>& recipients) const;
};

class NTP1Script
{
    std::string parsedScriptHex;

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

    TxType txType                  = TxType_None;
    bool   enableOpReturnSizeCheck = true;

    void setCommonParams(std::string Header, int ProtocolVersion, std::string OpCodeBin,
                         std::string scriptHex);

public:
    void setEnableOpReturnSizeCheck(bool value = true);
    bool isOpReturnSizeCheckEnabled() const;

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
        NTP1Int     amount;
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
                                         std::to_string(static_cast<int>(this->aggregationPolicy)));
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

    virtual std::string getHexMetadata() const      = 0;
    virtual std::string getRawMetadata() const      = 0;
    virtual std::string getInflatedMetadata() const = 0;

    virtual ~NTP1Script() = default;
    static uint64_t    CalculateMetadataSize(const std::string& op_code_bin);
    static TxType      CalculateTxType(const std::string& op_code_bin);
    static TxType      CalculateTxTypeNTP1v3(const std::string& op_code_bin);
    static uint64_t    CalculateAmountSize(uint8_t firstChar);
    static NTP1Int     ParseAmountFromLongEnoughString(const std::string& BinAmountStartsAtByte0,
                                                       int&               rawSize);
    static std::string ParseOpCodeFromLongEnoughString(const std::string& BinOpCodeStartsAtByte0);
    static std::string ParseMetadataFromLongEnoughString(const std::string& BinMetadataStartsAtByte0,
                                                         const std::string& op_code_bin,
                                                         const std::string& wholeScriptHex = "");
    static std::string
    ParseNTP1v3MetadataFromLongEnoughString(const std::string& BinMetadataSizeStartsAtByte0,
                                            const std::string& wholeScriptHex = "");
    static std::string
    ParseTokenSymbolFromLongEnoughString(const std::string& BinTokenSymbolStartsAtByte0);
    static std::vector<TransferInstruction>
    ParseTransferInstructionsFromLongEnoughString(const std::string& BinInstructionsStartFromByte0,
                                                  int&               totalRawSize);
    static std::vector<TransferInstruction>
    ParseNTP1v3TransferInstructionsFromLongEnoughString(const std::string& BinInstructionsStartFromByte0,
                                                        int&               totalRawSize);

    std::string getHeader() const;
    std::string getOpCodeBin() const;
    TxType      getTxType() const;

    static std::shared_ptr<NTP1Script> ParseScript(const std::string& scriptHex);
    std::string                        getParsedScriptHex() const;
    int                                getProtocolVersion() const;

    static NTP1Int     NTP1AmountHexToNumber(std::string hexVal);
    static NTP1Int     GetTrailingZeros(const NTP1Int& num);
    static std::string NumberToHexNTP1Amount(const NTP1Int& num, bool caps = false);

    static std::string        GetMetadataAsString(const NTP1Script*   ntp1script,
                                                  const CTransaction& tx) noexcept;
    static json_spirit::Value GetMetadataAsJson(const NTP1Script*   ntp1script,
                                                const CTransaction& tx) noexcept;

    static bool IsNTP1TokenSymbolValid(const std::string& symbol);
    static bool IsTokenSymbolCharValid(const char c);

    [[nodiscard]] static std::string EncryptMetadataWithEphemeralKey(
        const StringViewT data, const CKey& publicKey, CHL::EncryptionAlgorithm encAlgo,
        CHL::AuthKeyRatchetAlgorithm ratchetAlgo, CHL::AuthenticationAlgorithm authAlgo);

    [[nodiscard]] static std::string EncryptMetadata(const StringViewT data, const CKey& privateKey,
                                                     const CKey&                  publicKey,
                                                     CHL::EncryptionAlgorithm     encAlgo,
                                                     CHL::AuthKeyRatchetAlgorithm ratchetAlgo,
                                                     CHL::AuthenticationAlgorithm authAlgo);
    [[nodiscard]] static std::string DecryptMetadata(const StringViewT data, const CKey& privateKey);

    [[nodiscard]] static std::string EncryptMetadataBeforeSend(const StringViewT ntp1metadata,
                                                               const CKey&       inputPrivateKey,
                                                               const StringViewT recipientAddress);
};

template <typename Bitset>
void set_in_range(Bitset& b, uint8_t value, int from, int to)
{
    for (int i = from; i < to; ++i, value >>= 1) {
        b[i] = (value & 1);
    }
}

#endif // NTP1SCRIPT_H
