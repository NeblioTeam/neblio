#ifndef NTP1SENDTXDATA_H
#define NTP1SENDTXDATA_H

#include "globals.h"
#include "ntp1/ntp1wallet.h"
#include "ntp1sendtokensonerecipientdata.h"
#include <deque>
#include <string>
#include <unordered_set>

struct IntermediaryTI
{
    static const unsigned int CHANGE_OUTPUT_FAKE_INDEX = 10000000;

    std::vector<NTP1Script::TransferInstruction> TIs;
    NTP1OutPoint                                 input;

    bool isNTP1TokenIssuance = false;
};

struct IssueTokenData
{
    IssueTokenData(NTP1Int Amount, std::string Symbol, std::string Metadata)
        : amount(std::move(Amount)), symbol(std::move(Symbol)), metadata(std::move(Metadata))
    {
    }
    NTP1Int     amount;
    std::string symbol;
    std::string metadata;
};

/**
 * This class locates available NPT1 tokens in an NTP1Wallet object and either finds the required amount
 * of tokens in the wallet or simply ensures that the required amounts exist in the wallet
 *
 * @brief The NTP1SendTxData class
 */
class NTP1SendTxData
{
    // this is a vector because order must be preserved
    std::vector<NTP1OutPoint>                   tokenSourceInputs;
    std::vector<IntermediaryTI>                 intermediaryTIs;
    std::map<std::string, NTP1Int>              totalChangeTokens;
    std::map<std::string, NTP1Int>              totalTokenAmountsInSelectedInputs;
    std::vector<NTP1SendTokensOneRecipientData> recipientsList;
    boost::shared_ptr<NTP1Wallet>               usedWallet;

    int64_t __addInputsThatCoversNeblAmount(uint64_t neblAmount);
    bool    ready = false;

    boost::optional<IssueTokenData> tokenToIssueData;

public:
    NTP1SendTxData();
    /**
     * @brief calculateSources
     * @param wallet
     * @param inputs: inputs to be used; if no inputs provided, everything in the wallet will be used
     * @param recipients
     * @param recalculateFee
     * @param neblAmount amount to be sent with the transaction
     */
    void selectNTP1Tokens(boost::shared_ptr<NTP1Wallet> wallet, std::vector<NTP1OutPoint> inputs,
                          std::vector<NTP1SendTokensOneRecipientData> recipients,
                          bool                                        addMoreInputsIfRequired);
    void selectNTP1Tokens(boost::shared_ptr<NTP1Wallet> wallet, const std::vector<COutPoint>& inputs,
                          const std::vector<NTP1SendTokensOneRecipientData>& recipients,
                          bool                                               addMoreInputsIfRequired);

    void                            issueNTP1Token(const IssueTokenData& data);
    boost::optional<IssueTokenData> getNTP1TokenIssuanceData() const;
    bool                            getWhetherIssuanceExists() const;

    static const std::string NEBL_TOKEN_ID;
    static const std::string TO_ISSUE_TOKEN_ID;

    // returns the total balance in the selected addresses (tokenSourceAddresses)
    static int64_t EstimateTxSizeInBytes(int64_t num_of_inputs, int64_t num_of_outputs);
    static int64_t EstimateTxFee(int64_t num_of_inputs, int64_t num_of_outputs);

    void
    verifyNTP1IssuanceRecipientsValidity(const std::vector<NTP1SendTokensOneRecipientData>& recipients);

    std::vector<NTP1OutPoint>      getUsedInputs() const;
    std::map<std::string, NTP1Int> getChangeTokens() const;
    std::map<std::string, NTP1Int> getTotalTokensInInputs() const;
    bool                           isReady() const;
    // list of recipients after removing Nebl recipients
    std::vector<NTP1SendTokensOneRecipientData> getNTP1TokenRecipientsList() const;
    boost::shared_ptr<NTP1Wallet>               getWallet() const;

    std::vector<IntermediaryTI> getIntermediaryTIs() const;
    /**
     * @brief hasNTP1Tokens
     * @return true if the resulting inputs have NTP1 tokens
     */
    bool     hasNTP1Tokens() const;
    uint64_t getRequiredNeblsForOutputs() const;

    static void FixTIsChangeOutputIndex(std::vector<NTP1Script::TransferInstruction>& TIs,
                                        int                                           changeOutputIndex);
};

#endif // NTP1SENDTXDATA_H
