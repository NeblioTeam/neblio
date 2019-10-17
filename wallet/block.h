#ifndef BLOCK_H
#define BLOCK_H

#include "globals.h"
#include "outpoint.h"
#include "transaction.h"
#include "txindex.h"
#include "uint256.h"
#include <unordered_map>
#include <vector>

class CBlockIndex;
class CTxDB;
class CWallet;

/** Nodes collect new transactions into a block, hash them into a hash tree,
 * and scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements.  When they solve the proof-of-work, they broadcast the block
 * to everyone and the block is added to the block chain.  The first transaction
 * in the block is a special one that creates a new coin owned by the creator
 * of the block.
 *
 * Blocks are appended to blk0001.dat files on disk (not anymore, now they're in lmdb).
 * Their location on disk is indexed by CBlockIndex objects in memory.
 */
class CBlock
{
public:
    // header
    static const int CURRENT_VERSION = 6;
    int              nVersion;
    uint256          hashPrevBlock;
    uint256          hashMerkleRoot;
    unsigned int     nTime;
    unsigned int     nBits;
    unsigned int     nNonce;

    // network and disk
    std::vector<CTransaction> vtx;

    // ppcoin: block signature - signed by one of the coin base txout[N]'s owner
    std::vector<unsigned char> vchBlockSig;

    // memory only
    mutable std::vector<uint256> vMerkleTree;

    // Denial-of-service detection:
    mutable int nDoS;
    bool        DoS(int nDoSIn, bool fIn) const
    {
        nDoS += nDoSIn;
        return fIn;
    }

    CBlock() { SetNull(); }

    IMPLEMENT_SERIALIZE(
        READWRITE(this->nVersion); nVersion = this->nVersion; READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot); READWRITE(nTime); READWRITE(nBits); READWRITE(nNonce);

        // ConnectBlock depends on vtx following header to generate CDiskTxPos
        if (!(nType & (SER_GETHASH | SER_BLOCKHEADERONLY))) {
            READWRITE(vtx);
            READWRITE(vchBlockSig);
        } else if (fRead) {
            const_cast<CBlock*>(this)->vtx.clear();
            const_cast<CBlock*>(this)->vchBlockSig.clear();
        })

    void SetNull();

    CBlock GetBlockHeader() const;

    bool IsNull() const;

    uint256 GetHash() const;

    uint256 GetPoWHash() const;

    int64_t GetBlockTime() const;

    void UpdateTime(const CBlockIndex* pindexPrev);

    // entropy bit for stake modifier if chosen by modifier
    unsigned int GetStakeEntropyBit() const;

    // ppcoin: two types of block: proof-of-work or proof-of-stake
    bool IsProofOfStake() const;

    bool IsProofOfWork() const { return !IsProofOfStake(); }

    std::pair<COutPoint, unsigned int> GetProofOfStake() const;

    // ppcoin: get max transaction timestamp
    int64_t GetMaxTransactionTime() const;

    uint256 BuildMerkleTree() const;

    std::vector<uint256> GetMerkleBranch(int nIndex) const;

    static uint256 CheckMerkleBranch(uint256 hash, const std::vector<uint256>& vMerkleBranch,
                                     int nIndex);

    bool WriteToDisk(const uint256& nBlockPos, const uint256& hashProof);

    bool ReadFromDisk(const uint256& hash, bool fReadTransactions = true);
    bool ReadFromDisk(const uint256& hash, CTxDB& txdb, bool fReadTransactions = true);

    void print() const;

    static bool CheckBIP30Attack(CTxDB& txdb, const uint256& hashTx);

    struct ChainReplaceTxs
    {
        // transactions found in the blocks up to common ancestor in main chain
        std::unordered_set<uint256> disconnectedRootTxs;
        // transactions that are being spent in the above ones
        std::unordered_map<uint256, CTxIndex> modifiedOutputsTxs;
    };

    struct CommonAncestorSuccessorBlocks
    {
        // while finding the common ancestor, this is the part in the main chain (not part of this block)
        std::unordered_set<uint256> inMainChain;
        // while finding the common ancestor, this is the part of this block's chain (excluding this
        // block)
        std::vector<uint256>
            inFork; // order matters here because we want to simulate respending these in order
    };

    CommonAncestorSuccessorBlocks GetBlocksUpToCommonAncestorInMainChain() const;
    ChainReplaceTxs               GetAlternateChainTxsUpToCommonAncestor(CTxDB& txdb) const;

    bool DisconnectBlock(CTxDB& txdb, CBlockIndexSmartPtr& pindex);
    bool ConnectBlock(CTxDB& txdb, const CBlockIndexSmartPtr& pindex, bool fJustCheck = false);
    bool VerifyInputsUnspent(CTxDB& txdb) const;
    bool VerifyBlock(CTxDB& txdb);
    bool ReadFromDisk(const CBlockIndex* pindex, bool fReadTransactions = true);
    bool ReadFromDisk(const CBlockIndex* pindex, CTxDB& txdb, bool fReadTransactions = true);
    bool SetBestChain(CTxDB& txdb, const CBlockIndexSmartPtr& pindexNew,
                      const bool createDbTransaction = true);
    bool AddToBlockIndex(uint256 nBlockPos, const uint256& hashProof, CTxDB& txdb,
                         CBlockIndexSmartPtr* newBlockIdxPtr      = nullptr,
                         const bool           createDbTransaction = true);
    bool CheckBlock(bool fCheckPOW = true, bool fCheckMerkleRoot = true, bool fCheckSig = true) const;
    bool AcceptBlock();
    bool GetCoinAge(uint64_t& nCoinAge) const; // ppcoin: calculate total coin age spent in block
    bool SignBlock(CWallet& keystore, int64_t nFees);
    bool CheckBlockSignature() const;

    static CBlockIndexSmartPtr FindBlockByHeight(int nHeight);

    static void InvalidChainFound(const CBlockIndexSmartPtr& pindexNew, CTxDB& txdb);

    static bool Reorganize(CTxDB& txdb, CBlockIndexSmartPtr& pindexNew,
                           const bool createDbTransaction = true);

private:
    bool SetBestChainInner(CTxDB& txdb, const CBlockIndexSmartPtr& pindexNew,
                           const bool createDbTransaction = true);
};

#endif // BLOCK_H
