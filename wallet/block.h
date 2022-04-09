#ifndef BLOCK_H
#define BLOCK_H

#include "blockindex.h"
#include "blockreject.h"
#include "forkspendsimulator.h"
#include "globals.h"
#include "outpoint.h"
#include "transaction.h"
#include "txindex.h"
#include "uint256.h"
#include "viucache.h"
#include <unordered_map>
#include <vector>

class CBlockIndex;
class ITxDB;
class CWallet;

extern VIUCache viuCache;
extern unsigned VIUCachePushProbabilityNumerator;
extern unsigned VIUCachePushProbabilityDenominator;

struct ColdStakeShares
{
    CAmount stakerShare;
    CAmount ownerShare;
    ColdStakeShares(CAmount StakerShare, CAmount OwnerShare)
        : stakerShare(StakerShare), ownerShare(OwnerShare)
    {
    }
};

// TODO: write unit tests for this and all its static members
class DistributedColdStakeV1
{
    static const uint8_t CURRENT_VERSION = 1;
    uint16_t             numerator;
    uint16_t             denominator;
    CScript              allowedDestination;

public:
    static const std::set<txnouttype>& GetAllowedOutputTypes()
    {
        static const std::set<txnouttype> result{TX_POOLCOLDSTAKE, TX_PUBKEY, TX_PUBKEYHASH};
        return result;
    }

    static boost::optional<DistributedColdStakeV1> FromData(const std::vector<uint8_t>& data);
    static DistributedColdStakeV1                  Make(uint16_t numerator, uint16_t denominator,
                                                        const CScript& destination);
    std::vector<uint8_t>                           toData() const;
    bool                                           validate() const;
    boost::optional<ColdStakeShares>               distributeReward(CAmount total) const;
    const CScript&                                 getDestination() const;

    [[nodiscard]] static bool CheckAllowedOutputTypes(const ITxDB&               txdb,
                                                      const std::vector<CTxOut>& outputs);

    [[nodiscard]] static std::map<CScript, CAmount>
    MakeDestinationVsAmountMap(const std::vector<CTxOut>& outputs);
    [[nodiscard]] static boost::optional<CAmount>
    GetTotalColdStakeOwnerAmount(const ITxDB&                      txdb,
                                 const std::map<CScript, CAmount>& destinationVsAmounts);
    [[nodiscard]] static CAmount
    GetTotalColdStakePoolAmount(const std::map<CScript, CAmount>& destinationVsAmounts,
                                const CScript                     allowedDest);
    [[nodiscard]] static bool CheckAllScriptsInInputsMatch(const ITxDB&        txdb,
                                                           const MapPrevTx&    cachedInputs,
                                                           const CTransaction& tx);
    [[nodiscard]] static bool IsAnyInputAPoolColdStake(const ITxDB& txdb, const MapPrevTx& cachedInputs,
                                                       const CTransaction& tx);

    // clang-format off
    IMPLEMENT_SERIALIZE(
        READWRITE(numerator);
        READWRITE(denominator);
        READWRITE(allowedDestination);
        );
    // clang-format on
};

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
    static const int32_t CURRENT_VERSION = 6;
    int32_t              nVersion;
    uint256              hashPrevBlock;
    uint256              hashMerkleRoot;
    uint32_t             nTime;
    uint32_t             nBits;
    uint32_t             nNonce;

    // network and disk
    std::vector<CTransaction> vtx;

    // ppcoin: block signature - signed by one of the coin base txout[N]'s owner
    std::vector<uint8_t> vchBlockSig;

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
        nVersionIn = this->nVersion;
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

    bool WriteBlockPubKeys(ITxDB& txdb);

    bool ReadFromDisk(const uint256& hash, const ITxDB& txdb, bool fReadTransactions = true);

    void print() const;

    static bool CheckBIP30Attack(ITxDB& txdb, const uint256& hashTx);

    static const char* VIUErrorToString(ForkSpendSimulator::VIUError err);

    Result<void, ForkSpendSimulator::VIUError> VerifyInputsUnspent_Internal(const ITxDB& txdb) const;

    [[nodiscard]] bool CheckPoolColdStake(const ITxDB& txdb, const CTransaction& tx,
                                          const CAmount nTxValueOut, const CAmount nTxValueIn,
                                          const MapPrevTx& mapInputs);
    [[nodiscard]] bool DisconnectBlock(ITxDB& txdb, const CBlockIndex& pindex);
    [[nodiscard]] bool ConnectBlock(ITxDB& txdb, const boost::optional<CBlockIndex>& pindex,
                                    bool fJustCheck = false);
    Result<void, ForkSpendSimulator::VIUError> VerifyInputsUnspent(const ITxDB& txdb) const;
    bool                                       VerifyBlock(ITxDB& txdb);
    bool ReadFromDisk(const CBlockIndex* pindex, const ITxDB& txdb, bool fReadTransactions = true);
    bool SetBestChain(ITxDB& txdb, const boost::optional<CBlockIndex>& pindexNew,
                      const bool createDbTransaction = true);
    boost::optional<CBlockIndex> AddToBlockIndex(const uint256&                      blockHash,
                                                 const boost::optional<CBlockIndex>& prevBlockIndex,
                                                 const uint256& hashProof, ITxDB& txdb,
                                                 const bool createDbTransaction = true);
    bool CheckBlock(const ITxDB& txdb, const uint256& blockHash, bool fCheckPOW = true,
                    bool fCheckMerkleRoot = true, bool fCheckSig = true);
    bool AcceptBlock(const CBlockIndex& prevBlockIndex, const uint256& blockHash);
    bool
         SignBlock(const ITxDB& txdb, const CWallet& keystore, int64_t nFees,
                   const boost::optional<std::set<std::pair<uint256, unsigned>>>& customInputs = boost::none,
                   CAmount                                                        extraPayoutForTest = 0);
    bool SignBlockWithSpecificKey(const ITxDB& txdb, const COutPoint& outputToStake,
                                  const CKey& keyOfOutput, int64_t nFees);

    bool CheckBlockSignature(const ITxDB& txdb, const uint256& blockHash) const;
    Result<bool, BlockColdStakingCheckError> HasColdStaking(const ITxDB& txdb) const;

    static boost::optional<CBlockIndex> FindBlockByHeight(int nHeight);

    static void InvalidChainFound(const CBlockIndex& pindexNew, ITxDB& txdb);

    static void WriteNTP1BlockTransactionsToDisk(const std::vector<CTransaction>& vtx, ITxDB& txdb);
    static void WriteNTP1TxToDiskFromRawTx(const CTransaction& tx, ITxDB& txdb);
    static void WriteNTP1TxToDbAndDisk(const NTP1Transaction& ntp1tx, ITxDB& txdb);
    static void AssertIssuanceUniquenessInBlock(
        std::unordered_map<std::string, uint256>& issuedTokensSymbolsInThisBlock, const ITxDB& txdb,
        const CTransaction& tx,
        const std::map<uint256, std::vector<std::pair<CTransaction, NTP1Transaction>>>&
                                           mapQueuedNTP1Inputs,
        const std::map<uint256, CTxIndex>& queuedAcceptedTxs);

    bool Reorganize(ITxDB& txdb, const boost::optional<CBlockIndex>& pindexNew,
                    const bool createDbTransaction = true);

private:
    bool SetBestChainInner(ITxDB& txdb, const boost::optional<CBlockIndex>& pindexNew,
                           const bool createDbTransaction = true);
};

#endif // BLOCK_H
