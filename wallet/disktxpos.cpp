#include "disktxpos.h"

#include "util.h"

std::string CDiskTxPos::ToString() const
{
    if (IsNull())
        return "null";
    else
        return fmt::format("(nBlockPos={}, nTxPos={})", nBlockPos.ToString(), nTxPos);
}

void CDiskTxPos::print() const { NLog.write(b_sev::trace, "{}", ToString()); }
