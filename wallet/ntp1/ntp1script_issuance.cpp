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
NTP1Script_Issuance::ParseIssuancePostHeaderData(std::string scriptBin, std::string OpCodeBin)
{
    std::shared_ptr<NTP1Script_Issuance> result = std::make_shared<NTP1Script_Issuance>();

    // get token symbol
    // TODO: is the size always 5?
    result->tokenSymbol = scriptBin.substr(0, 5);
    scriptBin.erase(scriptBin.begin(), scriptBin.begin() + result->tokenSymbol.size());

    // get metadata then drop it
    result->metadata = ParseMetadataFromLongEnoughString(scriptBin, OpCodeBin);
    scriptBin.erase(scriptBin.begin(), scriptBin.begin() + result->metadata.size());

    // parse amount
    int amountRawSize = 0;
    result->amount    = ParseAmountFromLongEnoughString(scriptBin, amountRawSize);
    scriptBin.erase(scriptBin.begin(), scriptBin.begin() + amountRawSize);

    // parse transfer instructions
    int totalTransferInstructionsSize = 0;
    result->transferInstructions =
        ParseTransferInstructionsFromLongEnoughString(scriptBin, totalTransferInstructionsSize);
    scriptBin.erase(scriptBin.begin(), scriptBin.begin() + totalTransferInstructionsSize);

    // the expected remaining byte is the issuance flag, otherwise a problem is there
    if (scriptBin.size() != 1) {
        throw std::runtime_error(
            "Last expected byte is the issuance flag, but the remaining bytes are: " +
            boost::algorithm::hex(scriptBin) + ", starting from " + boost::algorithm::hex(scriptBin));
    }

    result->issuanceFlags = IssuanceFlags::ParseIssuanceFlag(scriptBin.at(0));
    return result;
}
