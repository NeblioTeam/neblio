#include "ntp1script_issuance.h"

#include <boost/algorithm/hex.hpp>

NTP1Script_Issuance::NTP1Script_Issuance() {}

std::string NTP1Script_Issuance::getHexMetadata() const { return boost::algorithm::hex(metadata); }

std::string NTP1Script_Issuance::getRawMetadata() const { return metadata; }

int NTP1Script_Issuance::getDivisibility() const { return issuanceFlags.divisibility; }

bool NTP1Script_Issuance::isLocked() const { return issuanceFlags.locked; }

NTP1Script::IssuanceFlags::AggregationPolicy NTP1Script_Issuance::getAggregationPolicy() const
{
    return issuanceFlags.aggregationPolicty;
}

std::string NTP1Script_Issuance::getTokenSymbol() const { return tokenSymbol; }

uint64_t NTP1Script_Issuance::getAmount() const { return amount; }

unsigned NTP1Script_Issuance::getTransferInstructionsCount() const
{
    return transferInstructions.size();
}

NTP1Script::TransferInstruction NTP1Script_Issuance::getTransferInstruction(unsigned index) const
{
    return transferInstructions[index];
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

    // the expected remaining byte is the issuance flag, otherwise a problem is there
    if (ScriptBin.size() != 1) {
        throw std::runtime_error(
            "Last expected byte is the issuance flag, but the remaining bytes are: " +
            boost::algorithm::hex(ScriptBin) + ", starting from " + boost::algorithm::hex(ScriptBin));
    }

    result->issuanceFlags = IssuanceFlags::ParseIssuanceFlag(ScriptBin.at(0));
    return result;
}
