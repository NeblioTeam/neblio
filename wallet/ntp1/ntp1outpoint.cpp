#include "ntp1outpoint.h"

NTP1OutPoint::NTP1OutPoint()
{
    setNull();
}

NTP1OutPoint::NTP1OutPoint(uint256 hashIn, unsigned int indexIn)
{
    hash = hashIn;
    index = indexIn;
}

void NTP1OutPoint::setNull()
{
    hash = 0;
    index = (unsigned int)-1;
}

bool NTP1OutPoint::isNull() const
{
    return (hash == 0 && index == (unsigned int) -1);
}

uint256 NTP1OutPoint::getHash() const
{
    return hash;
}

unsigned int NTP1OutPoint::getIndex() const
{
    return index;
}
