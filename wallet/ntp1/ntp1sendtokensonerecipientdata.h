#ifndef NTP1SENDTOKENSONERECIPIENTDATA_H
#define NTP1SENDTOKENSONERECIPIENTDATA_H

#include <string>
#include "json_spirit.h"

class NTP1SendTokensOneRecipientData
{
public:
    std::string tokenId;
    std::string destination;
    uint64_t amount;
    json_spirit::Object exportJsonData() const;
    std::string exportJsonDataAsString() const;
};

#endif // NTP1SENDTOKENSONERECIPIENTDATA_H
