#ifndef MERKLETX_H
#define MERKLETX_H

#include "globals.h"
#include "serialize.h"
#include "transaction.h"
#include "uint256.h"
#include <vector>

/** A transaction with a merkle branch linking it to the block chain. */
class CMerkleTx : public CTransaction
{
private:
    /** Constant used in hashBlock to indicate tx has been abandoned (unused for now in neblio) */
    static const uint256 ABANDON_HASH;

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

    int SetMerkleBranch(const ITxDB& txdb, const CBlock* pblock = nullptr);

    // Return depth of transaction in blockchain:
    // <0  : conflicts with a transaction, this deep in the blockchain
    //  0  : in memory pool, waiting to be included in a block
    // >=1 : this many blocks deep in the main chain
    int GetDepthInMainChain(boost::optional<CBlockIndex>& pindexRet, const ITxDB& txdb) const;
    int GetDepthInMainChain(const ITxDB& txdb) const
    {
        boost::optional<CBlockIndex> pindexRet;
        return GetDepthInMainChain(pindexRet, txdb);
    }
    bool IsInMainChain(const ITxDB& txdb) const
    {
        boost::optional<CBlockIndex> pindexRet;
        return GetDepthInMainChain(pindexRet, txdb) > 0;
    }
    int                             GetBlocksToMaturity(const ITxDB& txdb) const;
    Result<void, TxValidationState> AcceptToMemoryPool() const;
    bool                            hashUnset() const;
    bool                            isAbandoned() const;
    void                            setAbandoned();
};

#endif // MERKLETX_H
