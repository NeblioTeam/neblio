#ifndef INTP1WALLET_H
#define INTP1WALLET_H

#include "ntp1/ntp1outpoint.h"
#include "ntp1/ntp1script.h"
#include "ntp1/ntp1transaction.h"

class INTP1Wallet : public boost::enable_shared_from_this<INTP1Wallet>
{
public:
    INTP1Wallet()                                          = default;
    virtual void                                  update() = 0;
    virtual std::string                           getTokenName(const std::string& tokenID) const    = 0;
    virtual NTP1Int                               getTokenBalance(const std::string& tokenID) const = 0;
    virtual std::string                           getTokenName(int index) const                     = 0;
    virtual std::string                           getTokenId(int index) const                       = 0;
    virtual std::string                           getTokenIssuanceTxid(int index) const             = 0;
    virtual std::string                           getTokenDescription(int index) const              = 0;
    virtual NTP1Int                               getTokenBalance(int index) const                  = 0;
    virtual std::string                           getTokenIcon(int index)                           = 0;
    virtual int64_t                               getNumberOfTokens() const                         = 0;
    virtual const std::map<std::string, NTP1Int>& getBalancesMap() const                            = 0;
    virtual const std::unordered_map<NTP1OutPoint, NTP1Transaction>&
                                           getWalletOutputsWithTokens() const                      = 0;
    virtual void                           clear()                                                 = 0;
    virtual void                           setMinMaxConfirmations(int minConfs, int maxConfs = -1) = 0;
    virtual std::map<std::string, NTP1Int> getBalances() const                                     = 0;
    virtual bool                           getRetrieveFullMetadata() const                         = 0;
    virtual void                           setRetrieveFullMetadata(bool value)                     = 0;
    virtual void exportToFile(const boost::filesystem::path& filePath) const                       = 0;
    virtual void importFromFile(const boost::filesystem::path& filePath)                           = 0;
    virtual void setTokenIcon(const std::string& tokenId, const std::string& iconData)             = 0;
};

#endif // INTP1WALLET_H
