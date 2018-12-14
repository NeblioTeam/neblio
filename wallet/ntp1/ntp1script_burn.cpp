#include "ntp1script_burn.h"

#include <boost/algorithm/hex.hpp>

NTP1Script_Burn::NTP1Script_Burn() {}

std::string NTP1Script_Burn::getHexMetadata() const { return boost::algorithm::hex(metadata); }

std::string NTP1Script_Burn::getRawMetadata() const { return metadata; }

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

std::string NTP1Script_Burn::calculateScriptBin() const
{
    using namespace boost::algorithm;

    std::string result;
    result += headerBin;
    result += opCodeBin;
    result += metadata;

    for (const auto& ti : transferInstructions) {
        result += TransferInstructionToBinScript(ti);
    }

    return result;
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

    script->protocolVersion      = 1;
    script->headerBin            = boost::algorithm::unhex(std::string("4e5401"));
    script->metadata             = Metadata;
    script->opCodeBin            = Create_OpCodeFromMetadata(Metadata);
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
