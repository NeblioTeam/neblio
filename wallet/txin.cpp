#include "txin.h"

CTxIn::CTxIn(uint256 hashPrevTx, unsigned int nOut, CScript scriptSigIn, unsigned int nSequenceIn)
{
    prevout   = COutPoint(hashPrevTx, nOut);
    scriptSig = scriptSigIn;
    nSequence = nSequenceIn;
}

bool CTxIn::IsFinal() const { return (nSequence == std::numeric_limits<unsigned int>::max()); }

std::string CTxIn::ToStringShort() const
{
    return strprintf(" %s %d", prevout.hash.ToString().c_str(), prevout.n);
}

std::string CTxIn::ToString() const
{
    std::string str;
    str += "CTxIn(";
    str += prevout.ToString();
    if (prevout.IsNull())
        str += strprintf(", coinbase %s", HexStr(scriptSig).c_str());
    else
        str += strprintf(", scriptSig=%s", scriptSig.ToString().substr(0, 24).c_str());
    if (nSequence != std::numeric_limits<unsigned int>::max())
        str += strprintf(", nSequence=%u", nSequence);
    str += ")";
    return str;
}
