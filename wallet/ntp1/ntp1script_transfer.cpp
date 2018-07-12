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
