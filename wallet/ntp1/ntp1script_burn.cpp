#include "ntp1script_burn.h"

#include <boost/algorithm/hex.hpp>

#include "init.h"
#include "main.h"
#include "util.h"

NTP1Script_Burn::NTP1Script_Burn() {}

std::string NTP1Script_Burn::getHexMetadata() const { return boost::algorithm::hex(metadata); }

std::string NTP1Script_Burn::getRawMetadata() const { return metadata; }

std::string NTP1Script_Burn::getInflatedMetadata() const
{
    if (!metadata.empty())
        return ZlibDecompress(getRawMetadata());
    else
        return "";
}

unsigned NTP1Script_Burn::getTransferInstructionsCount() const { return transferInstructions.size(); }

NTP1Script_Burn::TransferInstruction NTP1Script_Burn::getTransferInstruction(unsigned index) const
{
    return transferInstructions[index];
}

std::vector<NTP1Script::TransferInstruction> NTP1Script_Burn::getTransferInstructions() const
{
    return transferInstructions;
}

std::shared_ptr<NTP1Script_Burn> NTP1Script_Burn::ParseBurnPostHeaderData(std::string ScriptBin,
                                                                          std::string OpCodeBin)
{
    std::shared_ptr<NTP1Script_Burn> result = std::make_shared<NTP1Script_Burn>();

    // get metadata then drop it
    result->metadata = ParseMetadataFromLongEnoughString(ScriptBin, OpCodeBin);
    ScriptBin.erase(ScriptBin.begin(), ScriptBin.begin() + result->metadata.size());

    // parse transfer instructions
    int totalTransferInstructionsSize = 0;
    result->transferInstructions =
        ParseTransferInstructionsFromLongEnoughString(ScriptBin, totalTransferInstructionsSize);
    ScriptBin.erase(ScriptBin.begin(), ScriptBin.begin() + totalTransferInstructionsSize);

    // burn should have at least one transfer instruction output with index 31 as the burn address
    // other addresses are transfer, not burn
    auto it = std::find_if(result->transferInstructions.begin(), result->transferInstructions.end(),
                           [](const TransferInstruction& t) { return t.outputIndex == 31; });

    if (it == result->transferInstructions.end()) {
        throw std::runtime_error("A burn transaction was created, but transfer instructions had invalid "
                                 "outputs. At least one of the outputs should have the index 31, "
                                 "indicating the amount to burn.");
    }

    return result;
}

std::shared_ptr<NTP1Script_Burn> NTP1Script_Burn::ParseNTP1v3BurnPostHeaderData(std::string ScriptBin)
{
    std::shared_ptr<NTP1Script_Burn> result = std::make_shared<NTP1Script_Burn>();

    // parse transfer instructions
    int totalTransferInstructionsSize = 0;
    result->transferInstructions =
        ParseNTP1v3TransferInstructionsFromLongEnoughString(ScriptBin, totalTransferInstructionsSize);
    ScriptBin.erase(ScriptBin.begin(), ScriptBin.begin() + totalTransferInstructionsSize);

    result->metadata = ParseNTP1v3MetadataFromLongEnoughString(ScriptBin);
    if (result->metadata.size() > 0) {
        ScriptBin.erase(ScriptBin.begin(),
                        ScriptBin.begin() + result->metadata.size() + 4); // + 4 for size
    }

    if (ScriptBin.size() != 0) {
        throw std::runtime_error("Garbage data after the metadata (unaccounted for in size).");
    }

    // burn should have at least one transfer instruction output with index 31 as the burn address
    // other addresses are transfer, not burn
    auto it = std::find_if(result->transferInstructions.begin(), result->transferInstructions.end(),
                           [](const TransferInstruction& t) { return t.outputIndex == 31; });

    if (it == result->transferInstructions.end()) {
        throw std::runtime_error("A burn transaction was created, but transfer instructions had invalid "
                                 "outputs. At least one of the outputs should have the index 31, "
                                 "indicating the amount to burn.");
    }

    return result;
}

std::string NTP1Script_Burn::calculateScriptBin() const
{
    if (protocolVersion == 1) {
        std::string result;
        result += headerBin;
        result += opCodeBin;
        result += metadata;

        for (const auto& ti : transferInstructions) {
            result += TransferInstructionToBinScript(ti);
        }

        return result;
    } else if (protocolVersion == 3) {
        std::string result;
        result += headerBin;
        result += opCodeBin;

        if (transferInstructions.size() > 0xff) {
            throw std::runtime_error(
                "The number of transfer instructions exceeded the allowed maximum, 255");
        }

        unsigned char TIsSize = static_cast<unsigned char>(transferInstructions.size());

        result.push_back(static_cast<char>(TIsSize));
        for (const auto& ti : transferInstructions) {
            result += TransferInstructionToBinScript(ti);
        }

        // check that there's at least one burn instruction, identified by sening to output 31
        const auto& TIs = transferInstructions;
        auto        it  = std::find_if(TIs.cbegin(), TIs.cend(),
                               [](const TransferInstruction& ti) { return ti.outputIndex == 31; });
        if (it == TIs.cend()) {
            throw std::runtime_error(
                "Attempted to create a burn instruction that does not burn anything");
        }

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
        throw std::runtime_error(
            "While calculating/constructing NTP1Script, an unknown protocol version was found");
    }
}

std::set<unsigned int> NTP1Script_Burn::getNTP1OutputIndices() const
{
    std::set<unsigned int> result;
    for (const auto& ti : transferInstructions) {
        // output index 31 is for burning, doesn't count
        if (ti.outputIndex != 31) {
            result.insert(ti.outputIndex);
        }
    }
    return result;
}

std::shared_ptr<NTP1Script_Burn>
NTP1Script_Burn::CreateScript(const std::vector<NTP1Script::TransferInstruction>& transferInstructions,
                              const std::string&                                  Metadata)
{
    std::shared_ptr<NTP1Script_Burn> script = std::make_shared<NTP1Script_Burn>();

    script->protocolVersion      = 3;
    script->headerBin            = boost::algorithm::unhex(std::string("4e5403"));
    script->metadata             = Metadata;
    script->opCodeBin            = boost::algorithm::hex(std::string("20"));
    script->transferInstructions = transferInstructions;
    script->txType               = TxType::TxType_Burn;

    return script;
}

std::string NTP1Script_Burn::Create_OpCodeFromMetadata(const std::string& metadata)
{
    const auto& sz = metadata.size();
    std::string result;
    if (sz == 0) {
        return std::string(1, static_cast<char>(uint8_t(0x25)));
    } else if (sz == 20) {
        return std::string(1, static_cast<char>(uint8_t(0x21)));
    } else if (sz == 52) {
        return std::string(1, static_cast<char>(uint8_t(0x20)));
    } else {
        throw std::runtime_error("Invalid metadata size; can only be 0, 20 or 52");
    }
    return boost::algorithm::unhex(result);
}
