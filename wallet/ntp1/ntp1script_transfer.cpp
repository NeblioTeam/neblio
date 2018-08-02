#include "ntp1script_transfer.h"

#include <boost/algorithm/hex.hpp>

NTP1Script_Transfer::NTP1Script_Transfer() {}

std::string NTP1Script_Transfer::getHexMetadata() const { return boost::algorithm::hex(metadata); }

std::string NTP1Script_Transfer::getRawMetadata() const { return metadata; }

unsigned NTP1Script_Transfer::getTransferInstructionsCount() const
{
    return transferInstructions.size();
}

NTP1Script_Transfer::TransferInstruction
NTP1Script_Transfer::getTransferInstruction(unsigned index) const
{
    return transferInstructions[index];
}

std::vector<NTP1Script::TransferInstruction> NTP1Script_Transfer::getTransferInstructions() const
{
    return transferInstructions;
}

std::shared_ptr<NTP1Script_Transfer>
NTP1Script_Transfer::ParseTransferPostHeaderData(std::string ScriptBin, std::string OpCodeBin)
{
    std::shared_ptr<NTP1Script_Transfer> result = std::make_shared<NTP1Script_Transfer>();

    // get metadata then drop it
    result->metadata = ParseMetadataFromLongEnoughString(ScriptBin, OpCodeBin);
    ScriptBin.erase(ScriptBin.begin(), ScriptBin.begin() + result->metadata.size());

    // parse transfer instructions
    int totalTransferInstructionsSize = 0;
    result->transferInstructions =
        ParseTransferInstructionsFromLongEnoughString(ScriptBin, totalTransferInstructionsSize);
    ScriptBin.erase(ScriptBin.begin(), ScriptBin.begin() + totalTransferInstructionsSize);

    return result;
}

std::string NTP1Script_Transfer::calculateScriptBin() const
{
    std::string result;
    result += headerBin;
    result += opCodeBin;
    result += metadata;

    for (const auto& ti : transferInstructions) {
        result += TransferInstructionToBinScript(ti);
    }

    return result;
}

std::set<unsigned int> NTP1Script_Transfer::getNTP1OutputIndices() const
{
    std::set<unsigned int> result;
    for (const auto& ti : transferInstructions) {
        result.insert(ti.outputIndex);
    }
    return result;
}

std::shared_ptr<NTP1Script_Transfer> NTP1Script_Transfer::CreateScript(
    const std::vector<NTP1Script::TransferInstruction>& transferInstructions,
    const std::string&                                  Metadata)
{
    std::shared_ptr<NTP1Script_Transfer> script = std::make_shared<NTP1Script_Transfer>();

    script->protocolVersion      = 1;
    script->headerBin            = boost::algorithm::unhex(std::string("4e5401"));
    script->metadata             = Metadata;
    script->opCodeBin            = Create_OpCodeFromMetadata(Metadata);
    script->transferInstructions = transferInstructions;
    script->txType               = TxType::TxType_Transfer;

    return script;
}

std::string NTP1Script_Transfer::Create_OpCodeFromMetadata(const std::string& metadata)
{
    const auto& sz = metadata.size();
    std::string result;
    if (sz == 0) {
        return result = "12";
    } else if (sz == 20) {
        return result = "11";
    } else if (sz == 52) {
        return result = "15";
    } else {
        throw std::runtime_error("Invalid metadata size; can only be 0, 20 or 52");
    }
    return boost::algorithm::unhex(result);
}
