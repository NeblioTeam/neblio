#ifndef BLOCK_H
#define BLOCK_H

#include "blockreject.h"
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

    enum class BlockColdStakingCheckError
    {
        SolverOnStakeTransactionFailed,
    };

    enum class ColdStakeKeyExtractionError
    {
        KeySizeInvalid,
    };

    boost::optional<CBlockReject> reject;

    // Denial-of-service detection:
    mutable int nDoS;
    bool        DoS(int nDoSIn, bool fIn) const
    {
        nDoS += nDoSIn;
        return fIn;
    }

    CBlock() { SetNull(); }

    // clang-format off
    IMPLEMENT_SERIALIZE(
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(hashPrevBlock);
        READWRITE(hashMerkleRoot);
        READWRITE(nTime);
        READWRITE(nBits);
        READWRITE(nNonce);

        // ConnectBlock depends on vtx following header to generate CDiskTxPos
        if (!(nType & (SER_GETHASH | SER_BLOCKHEADERONLY))) {
            READWRITE(vtx);
            READWRITE(vchBlockSig);
        } else if (fRead) {
            const_cast<CBlock*>(this)->vtx.clear();
            const_cast<CBlock*>(this)->vchBlockSig.clear();
        })
    // clang-format on

    void SetNull();

    CBlock GetBlockHeader() const;

    bool IsNull() const;

    uint256 GetHash() const;

    uint256 GetPoWHash() const;

    int64_t GetBlockTime() const;

    void UpdateTime(const CBlockIndex* pindexPrev);

    // entropy bit for stake modifier if chosen by modifier
    unsigned int GetStakeEntropyBit(const uint256& hash) const;

    // ppcoin: two types of block: proof-of-work or proof-of-stake
    bool IsProofOfStake() const;

    bool IsProofOfWork() const { return !IsProofOfStake(); }

    std::pair<COutPoint, unsigned int> GetProofOfStake() const;

    // ppcoin: get max transaction timestamp
    int64_t GetMaxTransactionTime() const;

    [[nodiscard]] uint256 GetMerkleRoot(bool* fMutated = nullptr) const;

    [[nodiscard]] std::vector<uint256> GetMerkleBranch(int nIndex) const;

    static uint256 CheckMerkleBranch(uint256 hash, const std::vector<uint256>& vMerkleBranch,
                                     int nIndex);

    bool WriteToDisk(const boost::optional<CBlockIndex>& prevBlockIndex, const uint256& hashProof,
                     const uint256& blockHash);

    bool WriteBlockPubKeys(CTxDB& txdb);

    bool ReadFromDisk(const uint256& hash, const ITxDB& txdb, bool fReadTransactions = true);

    void print() const;

    static bool CheckBIP30Attack(ITxDB& txdb, const uint256& hashTx);

    struct ChainReplaceTxs
    {
        // transactions that are being spent in the above ones
        std::unordered_map<uint256, CTxIndex> modifiedOutputsTxs;
        // the common ancestor block between the new fork of the new block and the main chain
        CBlockIndex commonAncestorBlockIndex;
    };

    struct CommonAncestorSuccessorBlocks
    {
        // while finding the common ancestor, this is the part of this block's chain (excluding this
        // block)
        std::vector<uint256>
                    inFork; // order matters here because we want to simulate respending these in order
        CBlockIndex commonAncestor;
    };

    enum class VIUError
    {
        UnknownErrorWhileCollectingTxs,
        TxInputIndexOutOfRange_Case1,
        TxInputIndexOutOfRange_Case2,
        TxInputIndexOutOfRange_Case3,
        TxInputIndexOutOfRange_Case4,
        DoublespendAttempt_Case1,
        DoublespendAttempt_Case2,
        SpendingNonexistentTx,
        BlockUnreadable,
        ReadTxIndexFailed_Case1,
        ReadTxIndexFailed_Case2,
        ReadBlockIndexFailed,
        BlockIsNotInMainChainEvenThoughItShould,
        BlockIndexOfPrevBlockNotFound,
    };

    Result<CommonAncestorSuccessorBlocks, VIUError>
                                      GetBlocksUpToCommonAncestorInMainChain(const ITxDB& txdb) const;
    Result<ChainReplaceTxs, VIUError> GetAlternateChainTxsUpToCommonAncestor(const ITxDB& txdb) const;

    bool DisconnectBlock(CTxDB& txdb, const CBlockIndex& pindex);
    bool ConnectBlock(ITxDB& txdb, const boost::optional<CBlockIndex>& pindex, bool fJustCheck = false);
    Result<void, CBlock::VIUError> VerifyInputsUnspent(const CTxDB& txdb) const;
    bool                           VerifyBlock(CTxDB& txdb);
    bool ReadFromDisk(const CBlockIndex* pindex, const ITxDB& txdb, bool fReadTransactions = true);
    bool SetBestChain(CTxDB& txdb, const boost::optional<CBlockIndex>& pindexNew,
                      const bool createDbTransaction = true);
    boost::optional<CBlockIndex> AddToBlockIndex(const uint256&                      blockHash,
                                                 const boost::optional<CBlockIndex>& prevBlockIndex,
                                                 const uint256& hashProof, CTxDB& txdb,
                                                 const bool createDbTransaction = true);
    bool CheckBlock(const ITxDB& txdb, const uint256& blockHash, bool fCheckPOW = true,
                    bool fCheckMerkleRoot = true, bool fCheckSig = true);
    bool AcceptBlock(const CBlockIndex& prevBlockIndex, const uint256& blockHash);
    bool
         SignBlock(const CTxDB& txdb, const CWallet& keystore, int64_t nFees,
                   const boost::optional<std::set<std::pair<uint256, unsigned>>>& customInputs = boost::none,
                   CAmount                                                        extraPayoutForTest = 0);
    bool SignBlockWithSpecificKey(const ITxDB& txdb, const COutPoint& outputToStake,
                                  const CKey& keyOfOutput, int64_t nFees);

    bool CheckBlockSignature(const ITxDB& txdb, const uint256& blockHash) const;
    Result<bool, BlockColdStakingCheckError> HasColdStaking(const ITxDB& txdb) const;

    static boost::optional<CBlockIndex> FindBlockByHeight(int nHeight);

    static void InvalidChainFound(const CBlockIndex& pindexNew, ITxDB& txdb);

    bool Reorganize(CTxDB& txdb, const boost::optional<CBlockIndex>& pindexNew,
                    const bool createDbTransaction = true);

private:
    bool SetBestChainInner(CTxDB& txdb, const boost::optional<CBlockIndex>& pindexNew,
                           const bool createDbTransaction = true);
};

#endif // BLOCK_H
