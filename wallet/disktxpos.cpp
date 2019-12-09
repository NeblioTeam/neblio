#include "disktxpos.h"

#include "util.h"

std::string CDiskTxPos::ToString() const
{
    if (IsNull())
        return "null";
    else
        return strprintf("(nBlockPos=%s, nTxPos=%u)", nBlockPos.ToString().c_str(), nTxPos);
}

void CDiskTxPos::print() const { printf("%s", ToString().c_str()); }
