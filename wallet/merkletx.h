#ifndef MERKLETX_H
#define MERKLETX_H

#include "serialize.h"
#include "uint256.h"
#include "globals.h"
#include "transaction.h"
#include <vector>


/** A transaction with a merkle branch linking it to the block chain. */
class CMerkleTx : public CTransaction
{
private:
    int GetDepthInMainChainINTERNAL(CBlockIndex*& pindexRet) const;

public:
    uint256              hashBlock;
    std::vector<uint256> vMerkleBranch;
    int                  nIndex;

    // memory only
    mutable bool fMerkleVerified;

    CMerkleTx() { Init(); }

    CMerkleTx(const CTransaction& txIn);

    void Init()
    {
        hashBlock       = 0;
        nIndex          = -1;
        fMerkleVerified = false;
    }

    IMPLEMENT_SERIALIZE(nSerSize += SerReadWrite(s, *(CTransaction*)this, nType, nVersion, ser_action);
                        nVersion = this->nVersion; READWRITE(hashBlock); READWRITE(vMerkleBranch);
                        READWRITE(nIndex);)

    int SetMerkleBranch(const CBlock* pblock = NULL);

    // Return depth of transaction in blockchain:
    // -1  : not in blockchain, and not in memory pool (conflicted transaction)
    //  0  : in memory pool, waiting to be included in a block
    // >=1 : this many blocks deep in the main chain
    int GetDepthInMainChain(CBlockIndex*& pindexRet) const;
    int GetDepthInMainChain() const
    {
        CBlockIndex* pindexRet;
        return GetDepthInMainChain(pindexRet);
    }
    bool IsInMainChain() const
    {
        CBlockIndex* pindexRet;
        return GetDepthInMainChainINTERNAL(pindexRet) > 0;
    }
    int  GetBlocksToMaturity() const;
    bool AcceptToMemoryPool();
};

#endif // MERKLETX_H
