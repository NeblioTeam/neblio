#ifndef INPOINT_H
#define INPOINT_H

class CTransaction;

/** An inpoint - a combination of a transaction and an index n into its vin */
class CInPoint
{
public:
    CTransaction* ptx;
    unsigned int  n;

    CInPoint() { SetNull(); }
    CInPoint(CTransaction* ptxIn, unsigned int nIn)
    {
        ptx = ptxIn;
        n   = nIn;
    }
    void SetNull()
    {
        ptx = nullptr;
        n   = (unsigned int)-1;
    }
    bool IsNull() const { return (ptx == nullptr && n == (unsigned int)-1); }
};

#endif // INPOINT_H
