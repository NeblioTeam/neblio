#ifndef NTP1OUTPOINT_H
#define NTP1OUTPOINT_H

#include <string>

#include "uint256.h"

class NTP1OutPoint
{
    uint256 hash;
    unsigned int index;
public:
    NTP1OutPoint();
    NTP1OutPoint(uint256 hashIn, unsigned int indexIn);
    void setNull();
    bool isNull() const;
    uint256 getHash() const;
    unsigned int getIndex() const;
};

#endif // NTP1OUTPOINT_H
