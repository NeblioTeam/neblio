#ifndef NTP1SCRIPT_TRANSFER_H
#define NTP1SCRIPT_TRANSFER_H

#include "ntp1script.h"

class NTP1Script_Transfer : public NTP1Script
{
    std::string metadata;

protected:
    std::vector<TransferInstruction> transferInstructions;

public:
    NTP1Script_Transfer();

    std::string                      getHexMetadata() const override;
    std::string                      getRawMetadata() const override;
    std::string                      getInflatedMetadata() const override;
    unsigned                         getTransferInstructionsCount() const;
    TransferInstruction              getTransferInstruction(unsigned index) const;
    std::vector<TransferInstruction> getTransferInstructions() const;

    static std::shared_ptr<NTP1Script_Transfer> ParseTransferPostHeaderData(std::string ScriptBin,
                                                                            std::string OpCodeBin);
    static std::shared_ptr<NTP1Script_Transfer> ParseNTP1v3TransferPostHeaderData(std::string ScriptBin);
    static std::string                          Create_OpCodeFromMetadata(const std::string& metadata);
    static std::shared_ptr<NTP1Script_Transfer>
    CreateScript(const std::vector<NTP1Script::TransferInstruction>& transferInstructions,
                 const std::string&                                  Metadata);

    // NTP1Script interface
public:
    std::string            calculateScriptBin() const override;
    std::set<unsigned int> getNTP1OutputIndices() const override;
};

#endif // NTP1SCRIPT_TRANSFER_H
