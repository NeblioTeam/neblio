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

    std::string                      getHexMetadata() const override;
    std::string                      getRawMetadata() const override;
    std::string                      getInflatedMetadata() const override;
    unsigned                         getTransferInstructionsCount() const;
    TransferInstruction              getTransferInstruction(unsigned index) const;
    std::vector<TransferInstruction> getTransferInstructions() const;

    static std::shared_ptr<NTP1Script_Burn> ParseBurnPostHeaderData(std::string ScriptBin,
                                                                    std::string OpCodeBin);
    static std::shared_ptr<NTP1Script_Burn> ParseNTP1v3BurnPostHeaderData(std::string ScriptBin);
    static std::string                      Create_OpCodeFromMetadata(const std::string& metadata);
    static std::shared_ptr<NTP1Script_Burn>
    CreateScript(const std::vector<NTP1Script::TransferInstruction>& transferInstructions,
                 const std::string&                                  Metadata);

    // NTP1Script interface
public:
    std::string            calculateScriptBin() const override;
    std::set<unsigned int> getNTP1OutputIndices() const override;
};

#endif // NTP1SCRIPT_BURN_H
