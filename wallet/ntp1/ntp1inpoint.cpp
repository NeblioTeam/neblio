#include "ntp1inpoint.h"


NTP1InPoint::NTP1InPoint()
{
    setNull();
}

NTP1InPoint::NTP1InPoint(boost::shared_ptr<NTP1Transaction> ptxIn, unsigned int indexIn)
{
    tx = ptxIn;
    index = indexIn;
}

void NTP1InPoint::setNull()
{
    tx.reset();
    index = (unsigned int) -1;
}

bool NTP1InPoint::isNull() const
{
    return (tx == nullptr && index == (unsigned int) -1);
}
