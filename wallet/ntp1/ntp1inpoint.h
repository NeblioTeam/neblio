#ifndef NTP1INPOINT_H
#define NTP1INPOINT_H

#include "ntp1transaction.h"

#include <boost/shared_ptr.hpp>

class NTP1InPoint
{
    boost::shared_ptr<NTP1Transaction> tx;
    unsigned int index;

public:
    NTP1InPoint();
    NTP1InPoint(boost::shared_ptr<NTP1Transaction> ptxIn, unsigned int nIn);
    void setNull();
    bool isNull() const;
};

#endif // NTP1INPOINT_H
