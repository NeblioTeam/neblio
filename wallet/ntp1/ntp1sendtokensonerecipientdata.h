#ifndef NTP1SENDTOKENSONERECIPIENTDATA_H
#define NTP1SENDTOKENSONERECIPIENTDATA_H

#include "json_spirit.h"
#include "ntp1script.h"
#include <string>

class NTP1SendTokensOneRecipientData
{
public:
    std::string         tokenId;
    std::string         destination;
    NTP1Int             amount;
    json_spirit::Object exportJsonData() const;
    std::string         exportJsonDataAsString() const;
};

#endif // NTP1SENDTOKENSONERECIPIENTDATA_H
