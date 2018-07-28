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
