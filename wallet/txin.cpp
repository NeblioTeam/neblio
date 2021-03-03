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
    return fmt::format(" {} {}", prevout.hash.ToString().c_str(), prevout.n);
}

std::string CTxIn::ToString() const
{
    std::string str;
    str += "CTxIn(";
    str += prevout.ToString();
    if (prevout.IsNull())
        str += fmt::format(", coinbase {}", HexStr(scriptSig));
    else
        str += fmt::format(", scriptSig={}", scriptSig.ToString().substr(0, 24));
    if (nSequence != std::numeric_limits<unsigned int>::max())
        str += fmt::format(", nSequence={}", nSequence);
    str += ")";
    return str;
}
