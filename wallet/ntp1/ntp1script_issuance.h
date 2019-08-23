#ifndef NTP1SCRIPT_ISSUANCE_H
#define NTP1SCRIPT_ISSUANCE_H

#include "ntp1script.h"

class NTP1Script_Issuance : public NTP1Script
{
    std::string   tokenSymbol;
    std::string   metadata;
    NTP1Int       amount;
    IssuanceFlags issuanceFlags;

    friend class NTP1Script;

    std::string __getAggregAndLockStatusTokenIDHexValue() const;

protected:
    std::vector<TransferInstruction> transferInstructions;

public:
    NTP1Script_Issuance();

    std::string getHexMetadata() const override;
    std::string getRawMetadata() const override;
    std::string getInflatedMetadata() const override;

    int                                         getDivisibility() const;
    bool                                        isLocked() const;
    IssuanceFlags::AggregationPolicy            getAggregationPolicy() const;
    std::string                                 getAggregationPolicyStr() const;
    std::string                                 getTokenSymbol() const;
    NTP1Int                                     getAmount() const;
    unsigned                                    getTransferInstructionsCount() const;
    TransferInstruction                         getTransferInstruction(unsigned index) const;
    std::vector<TransferInstruction>            getTransferInstructions() const;
    static std::shared_ptr<NTP1Script_Issuance> ParseIssuancePostHeaderData(std::string ScriptBin,
                                                                            std::string OpCodeBin);
    static std::shared_ptr<NTP1Script_Issuance> ParseNTP1v3IssuancePostHeaderData(std::string ScriptBin);
    std::string getTokenID(std::string input0txid, unsigned int input0index) const;

    static std::shared_ptr<NTP1Script_Issuance>
                       CreateScript(const std::string& Symbol, NTP1Int amount,
                                    const std::vector<TransferInstruction>& transferInstructions,
                                    const std::string& Metadata, bool locked, unsigned int divisibility,
                                    IssuanceFlags::AggregationPolicy aggrPolicy);
    static std::string Create_OpCodeFromMetadata(const std::string& metadata);
    static std::string Create_ProcessTokenSymbol(const std::string& symbol);

    // NTP1Script interface
public:
    std::string            calculateScriptBin() const override;
    std::set<unsigned int> getNTP1OutputIndices() const override;
};

#endif // NTP1SCRIPT_ISSUANCE_H
