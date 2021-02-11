#include "outpoint.h"

#include "util.h"
#include "logging/logger.h"

COutPoint::COutPoint(uint256 hashIn, uint32_t nIn)
{
    hash = hashIn;
    n    = nIn;
}

std::string COutPoint::ToString() const
{
    return fmt::format("COutPoint({}, {})", hash.ToString().substr(0, 10), n);
}

void COutPoint::print() const { NLog.write(b_sev::info, "{}", ToString()); }
