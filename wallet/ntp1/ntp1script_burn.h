#ifndef NTP1SCRIPT_BURN_H
#define NTP1SCRIPT_BURN_H

#include "ntp1script.h"

class NTP1Script_Burn : public NTP1Script
{
    std::string metadata;

protected:
    std::vector<TransferInstruction> transferInstructions;

public:
    NTP1Script_Burn();

    std::string                      getHexMetadata() const;
    std::string                      getRawMetadata() const;
    unsigned                         getTransferInstructionsCount() const;
    TransferInstruction              getTransferInstruction(unsigned index) const;
    std::vector<TransferInstruction> getTransferInstructions() const;

    static std::shared_ptr<NTP1Script_Burn> ParseBurnPostHeaderData(std::string ScriptBin,
                                                                    std::string OpCodeBin);
};

#endif // NTP1SCRIPT_BURN_H
