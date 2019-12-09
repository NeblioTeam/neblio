#include "ntp1script_issuance.h"

#include "base58.h"
#include "hash.h"
#include "init.h"
#include "main.h"
#include "util.h"
#include <boost/algorithm/hex.hpp>

std::string NTP1Script_Issuance::__getAggregAndLockStatusTokenIDHexValue() const
{
    std::string aggregatableHex;
    if (isLocked()) {
        if (getAggregationPolicy() == IssuanceFlags::AggregationPolicy::AggregationPolicy_Aggregatable) {
            aggregatableHex = "20ce";
        } else if (getAggregationPolicy() ==
                   IssuanceFlags::AggregationPolicy::AggregationPolicy_NonAggregatable) {
            aggregatableHex = "20e4";
        } else {
            throw std::runtime_error("Unknown aggregation policity for token: " + getTokenSymbol());
        }
    } else {
        if (getAggregationPolicy() == IssuanceFlags::AggregationPolicy::AggregationPolicy_Aggregatable) {
            aggregatableHex = "2e37";
        } else if (getAggregationPolicy() ==
                   IssuanceFlags::AggregationPolicy::AggregationPolicy_NonAggregatable) {
            aggregatableHex = "2e4e";
        } else {
            throw std::runtime_error("Unknown aggregation policity for token: " + getTokenSymbol());
        }
    }
    return aggregatableHex;
}

NTP1Script_Issuance::NTP1Script_Issuance() {}

std::string NTP1Script_Issuance::getHexMetadata() const { return boost::algorithm::hex(metadata); }

std::string NTP1Script_Issuance::getRawMetadata() const { return metadata; }

std::string NTP1Script_Issuance::getInflatedMetadata() const
{
    if (!metadata.empty())
        return ZlibDecompress(getRawMetadata());
    else
        return "";
}

int NTP1Script_Issuance::getDivisibility() const { return issuanceFlags.divisibility; }

bool NTP1Script_Issuance::isLocked() const { return issuanceFlags.locked; }

NTP1Script::IssuanceFlags::AggregationPolicy NTP1Script_Issuance::getAggregationPolicy() const
{
    return issuanceFlags.aggregationPolicy;
}

std::string NTP1Script_Issuance::getAggregationPolicyStr() const
{
    if (issuanceFlags.aggregationPolicy == NTP1Script::IssuanceFlags::AggregationPolicy_Aggregatable) {
        return IssuanceFlags::AggregationPolicy_Aggregatable_Str;
    } else if (issuanceFlags.aggregationPolicy ==
               NTP1Script::IssuanceFlags::AggregationPolicy_Aggregatable) {
        return IssuanceFlags::AggregationPolicy_NonAggregatable_Str;
    } else {
        return IssuanceFlags::AggregationPolicy_Aggregatable_Str;
    }
}

std::string NTP1Script_Issuance::getTokenSymbol() const { return tokenSymbol; }

NTP1Int NTP1Script_Issuance::getAmount() const { return amount; }

unsigned NTP1Script_Issuance::getTransferInstructionsCount() const
{
    return transferInstructions.size();
}

NTP1Script::TransferInstruction NTP1Script_Issuance::getTransferInstruction(unsigned index) const
{
    return transferInstructions[index];
}

std::vector<NTP1Script::TransferInstruction> NTP1Script_Issuance::getTransferInstructions() const
{
    return transferInstructions;
}

std::shared_ptr<NTP1Script_Issuance>
NTP1Script_Issuance::ParseIssuancePostHeaderData(std::string ScriptBin, std::string OpCodeBin)
{
    std::shared_ptr<NTP1Script_Issuance> result = std::make_shared<NTP1Script_Issuance>();

    // get token symbol (size always = 5 bytes)
    result->tokenSymbol = ParseTokenSymbolFromLongEnoughString(ScriptBin);
    ScriptBin.erase(ScriptBin.begin(), ScriptBin.begin() + 5);

    // get metadata then drop it
    result->metadata = ParseMetadataFromLongEnoughString(ScriptBin, OpCodeBin);
    ScriptBin.erase(ScriptBin.begin(), ScriptBin.begin() + result->metadata.size());

    // parse amount
    int amountRawSize = 0;
    result->amount    = ParseAmountFromLongEnoughString(ScriptBin, amountRawSize);
    ScriptBin.erase(ScriptBin.begin(), ScriptBin.begin() + amountRawSize);

    // parse transfer instructions
    int totalTransferInstructionsSize = 0;
    result->transferInstructions =
        ParseTransferInstructionsFromLongEnoughString(ScriptBin, totalTransferInstructionsSize);
    ScriptBin.erase(ScriptBin.begin(), ScriptBin.begin() + totalTransferInstructionsSize);

    // check that no skip transfer instructions exist; as it's forbidden in issuance
    for (const auto& inst : result->transferInstructions) {
        if (inst.skipInput) {
            throw std::runtime_error("An issuance script contained a skip transfer instruction: " +
                                     boost::algorithm::hex(ScriptBin));
        }
    }

    // the expected remaining byte is the issuance flag, otherwise a problem is there
    if (ScriptBin.size() != 1) {
        throw std::runtime_error(
            "Last expected byte is the issuance flag, but the remaining bytes are: " +
            boost::algorithm::hex(ScriptBin) + ", starting from " + boost::algorithm::hex(ScriptBin));
    }

    result->issuanceFlags = IssuanceFlags::ParseIssuanceFlag(ScriptBin.at(0));
    return result;
}

std::shared_ptr<NTP1Script_Issuance>
NTP1Script_Issuance::ParseNTP1v3IssuancePostHeaderData(std::string ScriptBin)
{
    std::shared_ptr<NTP1Script_Issuance> result = std::make_shared<NTP1Script_Issuance>();

    // get token symbol (size always = 5 bytes)
    result->tokenSymbol = ParseTokenSymbolFromLongEnoughString(ScriptBin);
    ScriptBin.erase(ScriptBin.begin(), ScriptBin.begin() + 5);

    // parse amount
    int amountRawSize = 0;
    result->amount    = ParseAmountFromLongEnoughString(ScriptBin, amountRawSize);
    ScriptBin.erase(ScriptBin.begin(), ScriptBin.begin() + amountRawSize);

    // parse transfer instructions
    int totalTransferInstructionsSize = 0;
    result->transferInstructions =
        ParseNTP1v3TransferInstructionsFromLongEnoughString(ScriptBin, totalTransferInstructionsSize);
    ScriptBin.erase(ScriptBin.begin(), ScriptBin.begin() + totalTransferInstructionsSize);

    // check that no skip transfer instructions exist; as it's forbidden in issuance
    for (const auto& inst : result->transferInstructions) {
        if (inst.skipInput) {
            throw std::runtime_error("An issuance script contained a skip transfer instruction: " +
                                     boost::algorithm::hex(ScriptBin));
        }
    }

    // remaining is 1 byte for issuance flag and 4 for metadata start flag + the metadata itself
    if (ScriptBin.size() < 1) {
        throw std::runtime_error("The data remaining cannot fit the issuance flags. It is empty.");
    }

    result->issuanceFlags = IssuanceFlags::ParseIssuanceFlag(ScriptBin.at(0));
    ScriptBin.erase(ScriptBin.begin(), ScriptBin.begin() + 1);

    result->metadata = ParseNTP1v3MetadataFromLongEnoughString(ScriptBin);
    if (result->metadata.size() > 0) {
        ScriptBin.erase(ScriptBin.begin(),
                        ScriptBin.begin() + result->metadata.size() + 4); // + 4 for size
    }

    if (ScriptBin.size() != 0) {
        throw std::runtime_error("Garbage data after the metadata (unaccounted for in size).");
    }

    return result;
}

std::string NTP1Script_Issuance::getTokenID(std::string input0txid, unsigned int input0index) const
{
    // txid should be lower case
    std::transform(input0txid.begin(), input0txid.end(), input0txid.begin(), ::tolower);

    std::string tohash = input0txid + ":" + std::to_string(input0index);

    std::vector<unsigned char> sha256_result(32);
    std::vector<unsigned char> rip160_result(20);
    SHA256(reinterpret_cast<unsigned char*>(&tohash.front()), tohash.size(), &sha256_result.front());
    RIPEMD160(&sha256_result.front(), sha256_result.size(), &rip160_result.front());

    // get padded divisibility
    std::string divisibilityHex = ToHexString(getDivisibility(), false);
    if (divisibilityHex.size() > 4) {
        throw std::runtime_error("Divisibility hex value has more than 4 digits for token " +
                                 getTokenSymbol());
    }
    divisibilityHex             = std::string(4 - divisibilityHex.size(), '0') + divisibilityHex;
    std::string divisibilityBin = boost::algorithm::unhex(divisibilityHex);

    // aggregation policy and lock status
    std::string aggregatableHex = __getAggregAndLockStatusTokenIDHexValue();
    std::string aggregatableBin = boost::algorithm::unhex(aggregatableHex);

    std::vector<unsigned char> toBase58Check;
    toBase58Check.insert(toBase58Check.end(), aggregatableBin.begin(), aggregatableBin.end());
    toBase58Check.insert(toBase58Check.end(), rip160_result.begin(), rip160_result.end());
    toBase58Check.insert(toBase58Check.end(), divisibilityBin.begin(), divisibilityBin.end());

    std::string result = EncodeBase58Check(toBase58Check);

    return result;
}

std::string NTP1Script_Issuance::calculateScriptBin() const
{
    using namespace boost::algorithm;

    if (protocolVersion == 1) {
        std::string result;
        result += headerBin;
        result += opCodeBin;
        result += Create_ProcessTokenSymbol(tokenSymbol);
        result += metadata;
        result += unhex(NumberToHexNTP1Amount(amount));

        for (const auto& ti : transferInstructions) {
            result += TransferInstructionToBinScript(ti);
        }

        uint8_t issuanceFlagsByte = issuanceFlags.convertToByte();
        result.push_back(static_cast<char>(issuanceFlagsByte));

        return result;
    } else if (protocolVersion == 3) {
        std::string result;
        result += headerBin;
        result += opCodeBin;
        result += Create_ProcessTokenSymbol(tokenSymbol);

        result += unhex(NumberToHexNTP1Amount(amount));

        if (transferInstructions.size() > 0xff) {
            throw std::runtime_error(
                "The number of transfer instructions exceeded the allowed maximum, 255");
        }

        unsigned char TIsSize = static_cast<unsigned char>(transferInstructions.size());

        result.push_back(static_cast<char>(TIsSize));
        for (const auto& ti : transferInstructions) {
            result += TransferInstructionToBinScript(ti);
        }

        uint8_t issuanceFlagsByte = issuanceFlags.convertToByte();
        result.push_back(static_cast<char>(issuanceFlagsByte));

        if (metadata.size() > 0) {
            uint32_t metadataSize = metadata.size();

            MakeBigEndian(metadataSize);

            std::string metadataSizeStr;
            metadataSizeStr.resize(4);
            static_assert(sizeof(metadataSize) == 4,
                          "Metadata size must be 4 bytes accoring to NTP1v3 standard");
            memcpy(&metadataSizeStr.front(), &metadataSize, 4);

            result += metadataSizeStr;
            result += metadata;
        }

        if (isOpReturnSizeCheckEnabled() && result.size() > DataSize()) {
            throw std::runtime_error("Calculated script size (" + std::to_string(result.size()) +
                                     " bytes) is larger than the maximum allowed (" +
                                     std::to_string(DataSize()) + " bytes)");
        }

        return result;

    } else {
        throw std::runtime_error("While creating issuance transcation, unknown protocol version + " +
                                 std::to_string(protocolVersion));
    }
}

std::shared_ptr<NTP1Script_Issuance> NTP1Script_Issuance::CreateScript(
    const std::string& Symbol, NTP1Int amount,
    const std::vector<NTP1Script::TransferInstruction>& transferInstructions,
    const std::string& Metadata, bool Locked, unsigned int Divisibility,
    IssuanceFlags::AggregationPolicy AggrPolicy)
{
    std::shared_ptr<NTP1Script_Issuance> script = std::make_shared<NTP1Script_Issuance>();

    script->metadata    = (Metadata.empty() ? "" : ZlibCompress(Metadata));
    script->opCodeBin   = boost::algorithm::unhex(std::string("01"));
    script->tokenSymbol = Symbol;
    script->amount      = amount;
    IssuanceFlags issuanceFlags;
    issuanceFlags.aggregationPolicy = AggrPolicy;
    issuanceFlags.divisibility      = static_cast<int>(Divisibility);
    issuanceFlags.locked            = Locked;
    script->transferInstructions    = transferInstructions;
    script->issuanceFlags           = issuanceFlags;
    script->headerBin               = boost::algorithm::unhex(std::string("4e5403"));
    script->protocolVersion         = 3;
    script->txType                  = TxType::TxType_Issuance;

    return script;
}

std::string NTP1Script_Issuance::Create_OpCodeFromMetadata(const std::string& metadata)
{
    const auto& sz = metadata.size();
    std::string result;
    if (sz == 0) {
        return std::string(1, static_cast<char>(uint8_t(0x05)));
    } else if (sz == 20) {
        return std::string(1, static_cast<char>(uint8_t(0x02)));
    } else if (sz == 52) {
        return std::string(1, static_cast<char>(uint8_t(0x01)));
    } else {
        throw std::runtime_error("Invalid metadata size; can only be 0, 20 or 52");
    }
    return boost::algorithm::unhex(result);
}

std::string NTP1Script_Issuance::Create_ProcessTokenSymbol(const std::string& symbol)
{
    if (symbol.size() > 5) {
        throw std::runtime_error("Symbol cannot be more than 5 characters");
    }
    std::string result = symbol;
    while (result.size() < 5) {
        result.push_back(0x20);
    }
    return result;
}

std::set<unsigned int> NTP1Script_Issuance::getNTP1OutputIndices() const
{
    std::set<unsigned int> result;
    for (const auto& ti : transferInstructions) {
        result.insert(ti.outputIndex);
    }
    return result;
}
