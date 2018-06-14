#ifndef NTP1SENDTOKENSDATA_H
#define NTP1SENDTOKENSDATA_H

#include "ntp1/ntp1wallet.h"
#include "ntp1sendtokensonerecipientdata.h"
#include <deque>
#include <string>

class NTP1SendTokensData
{
    std::deque<NTP1SendTokensOneRecipientData> recipients;
    std::deque<std::string>                    tokenSourceAddresses;
    int64_t                                    fee;

public:
    NTP1SendTokensData();
    void               addRecipient(const NTP1SendTokensOneRecipientData& data);
    void               addTokenSourceAddress(const std::string& tokenSourceAddress);
    void               calculateSources(boost::shared_ptr<NTP1Wallet> wallet, bool recalculateFee);
    void               setFee(uint64_t Fee);
    json_spirit::Value exportJsonData() const;
    std::string        exportToAPIFormat() const;
    // returns the total balance in the selected addresses (tokenSourceAddresses)
    int64_t        __addAddressesThatCoverFees();
    static int64_t EstimateTxSizeInBytes(int64_t num_of_inputs, int64_t num_of_outputs);
    static int64_t EstimateTxFee(int64_t num_of_inputs, int64_t num_of_outputs);
};

#endif // NTP1SENDTOKENSDATA_H
