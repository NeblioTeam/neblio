#include "outpoint.h"

#include "util.h"

COutPoint::COutPoint(uint256 hashIn, uint32_t nIn)
{
    hash = hashIn;
    n    = nIn;
}

std::string COutPoint::ToString() const
{
    return strprintf("COutPoint(%s, %u)", hash.ToString().substr(0, 10).c_str(), n);
}
