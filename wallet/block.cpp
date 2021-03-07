#include "block.h"

#include "NetworkForks.h"
#include "blockindex.h"
#include "blockindexlrucache.h"
#include "blocklocator.h"
#include "blockmetadata.h"
#include "checkpoints.h"
#include "kernel.h"
#include "main.h"
#include "merkle.h"
#include "ntp1/ntp1transaction.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "util.h"
#include "wallet.h"
#include "work.h"
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/scope_exit.hpp>
#include <mutex>

void CBlock::print() const
{
    NLog.write(b_sev::info,
               "CBlock(hash={}, ver={}, hashPrevBlock={}, hashMerkleRoot={}, nTime={}, nBits={:08x}, "
               "nNonce={}, vtx={}, vchBlockSig={})",
               GetHash().ToString(), nVersion, hashPrevBlock.ToString(), hashMerkleRoot.ToString(),
               nTime, nBits, nNonce, vtx.size(), HexStr(vchBlockSig.begin(), vchBlockSig.end()));
    for (unsigned int i = 0; i < vtx.size(); i++) {
        NLog.write(b_sev::info, "  ");
        vtx[i].print();
    }
    NLog.write(b_sev::info, "  vMerkleTree: ");
    std::vector<uint256> vMerkleTree = BlockMerkleTree(*this);
    for (unsigned int i = 0; i < vMerkleTree.size(); i++)
        NLog.write(b_sev::info, "{}", vMerkleTree[i].ToString().substr(0, 10));
    NLog.write(b_sev::info, "transaction count: {}", vtx.size());
    NLog.write(b_sev::info, "transactions:");
    for (const CTransaction& tx : vtx) {
        tx.print();
    }
}

uint256 CBlock::CheckMerkleBranch(uint256 hash, const std::vector<uint256>& vMerkleBranch, int nIndex)
{
    if (nIndex == -1)
        return 0;
    for (const uint256& otherside : vMerkleBranch) {
        if (nIndex & 1)
            hash = Hash(BEGIN(otherside), END(otherside), BEGIN(hash), END(hash));
        else
            hash = Hash(BEGIN(hash), END(hash), BEGIN(otherside), END(otherside));
        nIndex >>= 1;
    }
    return hash;
}

std::vector<uint256> CBlock::GetMerkleBranch(int nIndex) const
{
    std::vector<uint256> vMerkleTree = BlockMerkleTree(*this);
    std::vector<uint256> vMerkleBranch;
    int                  j = 0;
    for (int nSize = vtx.size(); nSize > 1; nSize = (nSize + 1) / 2) {
        int i = std::min(nIndex ^ 1, nSize - 1);
        vMerkleBranch.push_back(vMerkleTree[j + i]);
        nIndex >>= 1;
        j += nSize;
    }
    return vMerkleBranch;
}

uint256 CBlock::GetMerkleRoot(bool* fMutated) const { return BlockMerkleRoot(*this, fMutated); }

int64_t CBlock::GetMaxTransactionTime() const
{
    int64_t maxTransactionTime = 0;
    for (const CTransaction& tx : vtx)
        maxTransactionTime = std::max(maxTransactionTime, (int64_t)tx.nTime);
    return maxTransactionTime;
}

std::pair<COutPoint, unsigned int> CBlock::GetProofOfStake() const
{
    return IsProofOfStake() ? std::make_pair(vtx[1].vin[0].prevout, vtx[1].nTime)
                            : std::make_pair(COutPoint(), (unsigned int)0);
}

unsigned int CBlock::GetStakeEntropyBit(const uint256& hash) const
{
    // Take last bit of block hash as entropy bit
    unsigned int nEntropyBit = ((hash.Get64()) & UINT64_C(1));
    if (fDebug)
        NLog.write(b_sev::debug, "GetStakeEntropyBit: hashBlock={} nEntropyBit={}", hash.ToString(),
                   nEntropyBit);
    return nEntropyBit;
}

CBlock CBlock::GetBlockHeader() const
{
    CBlock block;
    block.nVersion       = nVersion;
    block.hashPrevBlock  = hashPrevBlock;
    block.hashMerkleRoot = hashMerkleRoot;
    block.nTime          = nTime;
    block.nBits          = nBits;
    block.nNonce         = nNonce;
    return block;
}

void CBlock::SetNull()
{
    nVersion       = CBlock::CURRENT_VERSION;
    hashPrevBlock  = 0;
    hashMerkleRoot = 0;
    nTime          = 0;
    nBits          = 0;
    nNonce         = 0;
    vtx.clear();
    vchBlockSig.clear();
    nDoS = 0;
}

uint256 CBlock::GetPoWHash() const { return scrypt_blockhash(CVOIDBEGIN(nVersion)); }

int64_t CBlock::GetBlockTime() const { return (int64_t)nTime; }

uint256 CBlock::GetHash() const { return GetPoWHash(); }

bool CBlock::IsNull() const { return (nBits == 0); }

void CBlock::UpdateTime(const CBlockIndex* /*pindexPrev*/)
{
    nTime = std::max(GetBlockTime(), GetAdjustedTime());
}

bool CBlock::DisconnectBlock(CTxDB& txdb, const CBlockIndex& pindex)
{
    // Disconnect in reverse order
    for (int i = vtx.size() - 1; i >= 0; i--)
        if (!vtx[i].DisconnectInputs(txdb))
            return false;

    if (!txdb.EraseBlockHashOfHeight(pindex.nHeight))
        return NLog.error("DisconnectBlock() : EraseBlockHashOfHeight failed");

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex.hashPrev != 0) {
        boost::optional<CBlockIndex> blockindexPrev = pindex.getPrev(txdb);
        if (!blockindexPrev) {
            return NLog.error("DisconnectBlock() : ReadBlockIndex for hashPrev failed");
        }
        blockindexPrev->hashNext = 0;

        if (!txdb.WriteBlockIndex(*blockindexPrev))
            return NLog.error("DisconnectBlock() : WriteBlockIndex failed");

        // we change the best hash. Remember this is within a transaction and will be reverted in case of
        // failure.
        txdb.WriteHashBestChain(blockindexPrev->GetBlockHash());

        if (blockindexPrev->nHeight == 0) {
            pindexGenesisBlock->hashNext = 0;
        }
    }

    // ppcoin: clean up wallet after disconnecting coinstake
    for (const CTransaction& tx : vtx)
        SyncWithWallets(txdb, tx, this);

    return true;
}

/// returns all the blocks from the tip of the main chain up to the common ancestor (without the common
/// ancestor)
Result<CBlock::CommonAncestorSuccessorBlocks, CBlock::VIUError>
CBlock::GetBlocksUpToCommonAncestorInMainChain(const ITxDB& txdb) const
{
    CommonAncestorSuccessorBlocks res;

    // fork part
    boost::optional<CBlockIndex>       currBI        = boost::none;
    const uint256                      prevBlockHash = this->hashPrevBlock;
    const boost::optional<CBlockIndex> biTarget      = txdb.ReadBlockIndex(prevBlockHash);

    if (!biTarget) {
        NLog.write(b_sev::critical,
                   "CRITCAL ERROR: Failed to prev block index at the "
                   "beginning of VerifyInputsUnspent() for block with hash {}",
                   hashPrevBlock.ToString());
        return Err(VIUError::BlockIndexOfPrevBlockNotFound);
    }

    currBI = biTarget;
    // keep stepping back from the orphan (new block) until we find the main chain
    while (!currBI->IsInMainChain(txdb)) {
        NLog.write(b_sev::trace, "Block in fork chain: {}\t{}", currBI->GetBlockHash().ToString(),
                   currBI->nHeight);

        // this map will be empty if the fork from main chain has only this block
        res.inFork.push_back(currBI->GetBlockHash());
        currBI = currBI->getPrev(txdb);
        if (!currBI) {
            NLog.write(b_sev::err, "Failed to read prev block index for height {} and block index {}",
                       currBI->nHeight, currBI->blockHash.ToString());
            return Err(VIUError::CommonAncestorSearchFailed);
        }
    }

    // the fork should be in temporal order because it's to be spent in order later
    std::reverse(res.inFork.begin(), res.inFork.end());

    res.commonAncestor = *currBI;
    return Ok(res);
}

/**
 * @brief CBlock::GetAlternateChainTxsUpToCommonAncestor
 * @return
 * This function will get all the blocks from the main chain up to the common ancestor with this block;
 * then, it'll unspend all the transactions in these blocks and return them in the map. This is necessary
 * to solve the problem of stake attack described in VerifyInputsUnspent()
 */
Result<CBlock::ChainReplaceTxs, CBlock::VIUError>
CBlock::GetAlternateChainTxsUpToCommonAncestor(const ITxDB& txdb) const
{
    CommonAncestorSuccessorBlocks commonAncestory = TRY(GetBlocksUpToCommonAncestorInMainChain(txdb));
    std::vector<CBlock>           forkChainBlocks; // to be reconnected
    forkChainBlocks.reserve(commonAncestory.inFork.size() + 1);

    // get all txs in the fork leading to this transaction (in order to test spending them later)
    for (const uint256& bh : commonAncestory.inFork) {
        CBlock blk;
        //        std::cout << "In fork block hash: " << bh.ToString() << std::endl;
        if (!txdb.ReadBlock(bh, blk, true)) {
            NLog.write(b_sev::err, "In fork chain search, block {} was not found in the database",
                       bh.ToString());
            return Err(VIUError::BlockCannotBeReadFromDB);
        }
        forkChainBlocks.push_back(blk);
    }

    ChainReplaceTxs result;
    result.commonAncestorBlockIndex = commonAncestory.commonAncestor;

    // since the fork can spend transactions from itself, we need to put them in such a way they're
    // reachable in O(1)
    std::unordered_map<uint256, CTransaction> forkTxs;

    // unspend/disconnect all inputs that are in the fork
    // we also need to unspent this block, so we add it and pop it later
    forkChainBlocks.push_back(*this);
    for (const CBlock& blk : forkChainBlocks) {
        for (const CTransaction& tx : blk.vtx) {
            forkTxs[tx.GetHash()] = tx;
        }

        for (const CTransaction& tx : blk.vtx) {
            if (tx.IsCoinBase()) {
                continue;
            }
            const std::vector<CTxIn>& vin = tx.vin;
            for (unsigned int inIdx = 0; inIdx < vin.size(); inIdx++) {
                const CTxIn& txin          = vin[inIdx];
                uint256      outputTxHash  = txin.prevout.hash;
                unsigned     outputNumInTx = txin.prevout.n;

                // we see if we already have prev txindex in the list of modified outputs
                auto     idxIt = result.modifiedOutputsTxs.find(outputTxHash);
                CTxIndex txindex;
                if (idxIt == result.modifiedOutputsTxs.cend()) {
                    // It's not in the list of modified outputs, so we get prev txindex from disk
                    if (!txdb.ReadTxIndex(outputTxHash, txindex)) {
                        // the only place left is on the fork itself: the block is spending a tx on the
                        // fork
                        auto forkTxIt = forkTxs.find(outputTxHash);
                        if (forkTxIt != forkTxs.cend()) {
                            // we don't unspend transaction on the fork, because they're already unspent
                            // since they're not main chain
                            continue;
                        } else {
                            NLog.write(b_sev::err,
                                       "ReadTxIndex failed for transaction {} and the transaction was "
                                       "not found in the fork itself (1)",
                                       outputTxHash.ToString());
                            return Err(VIUError::TxNonExistent_ReadTxIndexFailed_Case1);
                        }
                    }

                } else {
                    txindex = idxIt->second;
                }

                // check range
                if (outputNumInTx >= txindex.vSpent.size()) {
                    NLog.write(b_sev::err, "prevout.n out of range for transaction {} and output {} (1)",
                               outputTxHash.ToString(), outputNumInTx);
                    return Err(VIUError::TxInputIndexOutOfRange_Case3);
                }

                if (txindex.vSpent[outputNumInTx].IsNull()) {
                    // it's already unspent
                    continue;
                }

                uint256 spenderBlockHash              = txindex.vSpent[outputNumInTx].nBlockPos;
                const boost::optional<CBlockIndex> bi = txdb.ReadBlockIndex(spenderBlockHash);
                if (!bi) {
                    NLog.write(b_sev::err,
                               "The input of transaction {} whose index {} and hash {} is found to be "
                               "in block {} but that block is not found in the block index. This "
                               "should never happen.",
                               tx.GetHash().ToString(), outputNumInTx, outputTxHash.ToString(),
                               spenderBlockHash.ToString());
                    return Err(VIUError::ReadBlockIndexFailed);
                }

                // this should be true anyway, because ReadTxIndex has only blocks with spent
                // transactions from the main chain, but we double check for consistency
                if (!bi->IsInMainChain(txdb)) {
                    NLog.write(
                        b_sev::err,
                        "The input of transaction {} whose index {} and hash {} is found to be in "
                        "block {} but while that block was found, it's not in the main chain",
                        tx.GetHash().ToString(), outputNumInTx, outputTxHash.ToString(),
                        spenderBlockHash.ToString());
                    return Err(VIUError::BlockIsNotInMainChainEvenThoughItShould);
                }

                // make sure that the spending transaction is in the main chain above the common ancestor
                if (bi->nHeight > commonAncestory.commonAncestor.nHeight) {
                    // unspend, which is equivalent to disconnecting the blockchain (without updating
                    // the database)
                    txindex.vSpent[outputNumInTx].SetNull();

                    // this way, all main-chain spent transactions are in this map, while marked
                    // unspent
                    result.modifiedOutputsTxs[outputTxHash] = txindex;
                }
            }
        }
    }
    // now that this block's inputs are unspent in the fork, we pop it
    forkChainBlocks.pop_back();

    // at this point, all transactions from the fork that are spent in the main chain are now unspent

    //    std::cout << "Start reconnecting txs" << std::endl;
    // respend the transactions on the fork (to test whether this block is valid at this chain)
    for (const CBlock& blk : forkChainBlocks) {
        for (const CTransaction& tx : blk.vtx) {
            if (tx.IsCoinBase()) {
                continue;
            }
            // std::cout << "Reconnecting inputs in: " << tx.GetHash().ToString() << std::endl;
            const std::vector<CTxIn>& vin = tx.vin;
            for (unsigned int inIdx = 0; inIdx < vin.size(); inIdx++) {
                const CTxIn& txin          = vin[inIdx];
                uint256      outputTxHash  = txin.prevout.hash;
                unsigned     outputNumInTx = txin.prevout.n;

                auto     idxIt = result.modifiedOutputsTxs.find(outputTxHash);
                CTxIndex txindex;
                if (idxIt == result.modifiedOutputsTxs.cend()) {
                    // Get prev txindex from disk
                    if (!txdb.ReadTxIndex(outputTxHash, txindex)) {
                        // the only place left is on the fork itself: the block is spending a tx on the
                        // fork
                        auto forkTxIt = forkTxs.find(outputTxHash);
                        if (forkTxIt != forkTxs.cend()) {
                            const CTransaction& txInFork = forkTxIt->second;
                            txindex = CTxIndex(CDiskTxPos(blk.GetHash(), 42), txInFork.vout.size());
                        } else {
                            NLog.write(b_sev::err,
                                       "ReadTxIndex failed for transaction {} and the transaction was "
                                       "not found in the fork itself (2)",
                                       outputTxHash.ToString());
                            return Err(VIUError::TxNonExistent_ReadTxIndexFailed_Case2);
                        }
                    }
                } else {
                    txindex = idxIt->second;
                }

                // check range
                if (outputNumInTx >= txindex.vSpent.size()) {
                    NLog.write(b_sev::err, "prevout.n out of range for transaction " +
                                               outputTxHash.ToString() + " and output " +
                                               std::to_string(outputNumInTx));
                    return Err(VIUError::TxInputIndexOutOfRange_Case4);
                }

                // spend the output (without updating the database)
                txindex.vSpent[outputNumInTx] = CreateFakeSpentTxPos(txindex.pos.nBlockPos);

                result.modifiedOutputsTxs[outputTxHash] = txindex;
            }
            // since we're respending the transactions, we should store them in modified list because
            // they can be spent in subsequent blocks
            auto idxIt = result.modifiedOutputsTxs.find(tx.GetHash());
            if (idxIt == result.modifiedOutputsTxs.cend()) {
                result.modifiedOutputsTxs[tx.GetHash()] =
                    CTxIndex(CDiskTxPos(this->GetHash(), 3), tx.vout.size());
            }
        }
    }
    //    std::cout << "End reconnecting txs" << std::endl;
    return Ok(result);
}

Result<void, CBlock::VIUError> CBlock::VerifyInputsUnspent(const CTxDB& txdb) const
{
    // this function solves the problem in
    // https://medium.com/@dsl_uiuc/fake-stake-attacks-on-chain-based-proof-of-stake-cryptocurrencies-b8b05723f806
    // this function doesn't modify the database or the block being analyzed

    // in order to do this, we reverse/disconnect transactions up to the common ancestor (without
    // modifying the database), then we replay the new chain up the this block, finally we check whether
    // this block is valid in the chain constructed from common ancestor to this

    // queued transactions are the inputs that we already found (even from this block). The map stores
    // whether transactions are spent already. This solves the problem of spending an output in the same
    // block where it's created
    CBlock::ChainReplaceTxs alternateChainTxs;

    try {
        alternateChainTxs = TRY(GetAlternateChainTxsUpToCommonAncestor(txdb));
    } catch (std::exception& ex) {
        NLog.error("Failed to verify unspent inputs for block {}; error: {}", this->GetHash().ToString(),
                   ex.what());
        return Err(VIUError::UnknownErrorWhileCollectingTxs);
    }

    std::unordered_map<uint256, CTxIndex>& queuedTxs = alternateChainTxs.modifiedOutputsTxs;

    for (const CTransaction& tx : vtx) {
        {
            // if an output in the transaction is spent in the same block, it should also be found in the
            // queued transactions list in order for the tests below to work because it's not in the
            // blockchain yet
            auto it = queuedTxs.find(tx.GetHash());
            if (it == queuedTxs.cend()) {
                // unspent tx (all vSpent are null)
                CTxIndex txindex(CDiskTxPos(this->GetHash(), 2), tx.vout.size());
                queuedTxs[tx.GetHash()] = txindex;
            }
        }

        // coinbase don't have any inputs
        if (tx.IsCoinBase()) {
            continue;
        }

        // loop over inputs of this transaction, and check whether the outputs are already spent
        const std::vector<CTxIn>& vin = tx.vin;
        for (unsigned int inIdx = 0; inIdx < vin.size(); inIdx++) {
            uint256  outputTxHash  = vin[inIdx].prevout.hash;
            unsigned outputNumInTx = vin[inIdx].prevout.n;
            CTxIndex txindex;
            auto     it                = queuedTxs.find(vin[inIdx].prevout.hash);
            bool     inputFoundInQueue = (it != queuedTxs.cend());
            if (inputFoundInQueue) {
                if (outputNumInTx >= it->second.vSpent.size()) {
                    NLog.error("Output number {} in tx {} which is an input to tx {} "
                               "has an invalid input index in block {} (1)",
                               outputNumInTx, outputTxHash.ToString(), tx.GetHash().ToString(),
                               this->GetHash().ToString());
                    return Err(VIUError::TxInputIndexOutOfRange_Case1);
                }

                if (it->second.vSpent[outputNumInTx].IsNull()) {
                    // tx is not spent yet, so we mark it as spent
                    it->second.vSpent[outputNumInTx] = CreateFakeSpentTxPos(this->GetHash());
                } else {
                    NLog.error("Output number {} in tx {} which is an input to tx {} is attempting to "
                               "double-spend in the same block {}",
                               outputNumInTx, outputTxHash.ToString(), tx.GetHash().ToString(),
                               this->GetHash().ToString());
                    return Err(VIUError::DoublespendAttempt_Case2);
                }
            } else if (txdb.ReadTxIndex(outputTxHash, txindex)) {
                if (outputNumInTx >= txindex.vSpent.size()) {
                    NLog.error("Output number {} in tx {} which is an input to tx {} "
                               "has an invalid input index in block {} (2)",
                               outputNumInTx, outputTxHash.ToString(), tx.GetHash().ToString(),
                               this->GetHash().ToString());
                    return Err(VIUError::TxInputIndexOutOfRange_Case2);
                }

                queuedTxs[outputTxHash] = txindex;
                if (txindex.vSpent[outputNumInTx].IsNull()) {
                    queuedTxs.find(outputTxHash)->second.vSpent[outputNumInTx] =
                        CreateFakeSpentTxPos(this->GetHash());
                } else {
                    NLog.error("Output number {} in tx {} which is an input to tx {} is being "
                               "spent in block {} +++++ it was already spent in block {}, this is a "
                               "double-spend attempt",
                               outputNumInTx, outputTxHash.ToString(), tx.GetHash().ToString(),
                               this->GetHash().ToString(),
                               txindex.vSpent[outputNumInTx].nBlockPos.ToString());
                    return Err(VIUError::DoublespendAttempt_Case2);
                }
            } else {
                NLog.error("Output number {} in tx {} which is an input to tx {} and is being "
                           "attempted to spend it in block {}. it's an invalid tx",
                           outputNumInTx, outputTxHash.ToString(), tx.GetHash().ToString(),
                           vin[inIdx].prevout.hash.ToString());
                return Err(VIUError::SpendingNonexistentTx);
            }
        }
    }
    return Ok();
}

bool CBlock::CheckBIP30Attack(ITxDB& txdb, const uint256& hashTx)
{
    // Do not allow blocks that contain transactions which 'overwrite' older transactions,
    // unless those are already completely spent.
    // If such overwrites are allowed, coinbases and transactions depending upon those
    // can be duplicated to remove the ability to spend the first instance -- even after
    // being sent to another address.
    // See BIP30 and http://r6.ca/blog/20120206T005236Z.html for more information.
    // This logic is not necessary for memory pool transactions, as AcceptToMemoryPool
    // already refuses previously-known transaction ids entirely.
    // This rule was originally applied all blocks whose timestamp was after March 15, 2012, 0:00
    // UTC. Now that the whole chain is irreversibly beyond that time it is applied to all blocks
    // except the two in the chain that violate it. This prevents exploiting the issue against nodes
    // in their initial block download.

    CTxIndex txindexOld;
    if (txdb.ReadTxIndex(hashTx, txindexOld)) {
        for (CDiskTxPos& pos : txindexOld.vSpent)
            if (pos.IsNull())
                return false;
    }
    return true;
}

bool CBlock::ConnectBlock(ITxDB& txdb, const boost::optional<CBlockIndex>& pindex, bool fJustCheck)
{
    const uint256 blockHash = pindex->GetBlockHash();

    NLog.write(b_sev::info, "Connecting block: {}", blockHash.ToString());

    // Check it again in case a previous version let a bad block in, but skip BlockSig checking
    if (!CheckBlock(txdb, blockHash, !fJustCheck, !fJustCheck, false))
        return false;

    //// issue here: it doesn't know the version
    unsigned int nTxPos;
    if (fJustCheck)
        // FetchInputs treats CDiskTxPos(1,1,1) as a special "refer to memorypool" indicator
        // Since we're just checking the block and not actually connecting it, it might not (and probably
        // shouldn't) be on the disk to get the transaction from
        nTxPos = 1;
    else
        nTxPos = ::GetSerializeSize(CBlock(), SER_DISK, CLIENT_VERSION) - (2 * GetSizeOfCompactSize(0)) +
                 GetSizeOfCompactSize(vtx.size());

    // Comment by Sam: mapQueuedChanges is the list of transactions that already happened in the same
    // block. This is necessary for verifying outputs that are being spent in the same blocks

    std::map<uint256, CTxIndex> mapQueuedChanges;
    int64_t                     nFees        = 0;
    int64_t                     nValueIn     = 0;
    int64_t                     nValueOut    = 0;
    int64_t                     nStakeReward = 0;

    std::map<uint256, std::vector<std::pair<CTransaction, NTP1Transaction>>> mapQueuedNTP1Inputs;

    unsigned int nSigOps = 0;

    // map of issued token names in this block vs token hashes
    // this is used to prevent duplicate token names
    std::unordered_map<std::string, uint256> issuedTokensSymbolsInThisBlock;

    for (const CTransaction& tx : vtx) {
        const uint256 hashTx = tx.GetHash();

        std::vector<std::pair<CTransaction, NTP1Transaction>> inputsWithNTP1;

        if (!CheckBIP30Attack(txdb, hashTx)) {
            return NLog.error(
                "Block {} was rejected as it seems that an attempt of BIP30 attack was attempted",
                this->GetHash().ToString());
        }

        nSigOps += tx.GetLegacySigOpCount();
        if (nSigOps > MAX_BLOCK_SIGOPS) {
            reject = CBlockReject(REJECT_INVALID, "bad-blk-sigops", this->GetHash());
            return DoS(100, NLog.error("ConnectBlock() : too many sigops"));
        }

        CDiskTxPos posThisTx(pindex->GetBlockHash(), nTxPos);
        if (!fJustCheck)
            nTxPos += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);

        MapPrevTx mapInputs;
        if (tx.IsCoinBase())
            nValueOut += tx.GetValueOut();
        else {
            bool fInvalid;
            if (!tx.FetchInputs(txdb, mapQueuedChanges, true, false, mapInputs, fInvalid))
                return false;

            // Add in sigops done by pay-to-script-hash inputs;
            // this is to prevent a "rogue miner" from creating
            // an incredibly-expensive-to-validate block.
            nSigOps += tx.GetP2SHSigOpCount(mapInputs);
            if (nSigOps > MAX_BLOCK_SIGOPS) {
                reject = CBlockReject(REJECT_INVALID, "bad-blk-sigops", this->GetHash());
                return DoS(100, NLog.error("ConnectBlock() : too many sigops"));
            }

            CAmount nTxValueIn  = tx.GetValueIn(mapInputs);
            CAmount nTxValueOut = tx.GetValueOut();
            nValueIn += nTxValueIn;
            nValueOut += nTxValueOut;
            if (!tx.IsCoinStake())
                nFees += nTxValueIn - nTxValueOut;
            if (tx.IsCoinStake())
                nStakeReward = nTxValueOut - nTxValueIn;

            if (Params().GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON, txdb)) {
                try {
                    if (NTP1Transaction::IsTxNTP1(&tx)) {
                        // check if there are inputs already cached
                        inputsWithNTP1 = NTP1Transaction::GetAllNTP1InputsOfTx(
                            tx, txdb, true, mapQueuedNTP1Inputs, mapQueuedChanges);

                        // write NTP1 transactions' data
                        NTP1Transaction ntp1tx;
                        ntp1tx.readNTP1DataFromTx(txdb, tx, inputsWithNTP1);
                    }
                } catch (std::exception& ex) {
                    return NLog.error(
                        "Error while verifying NTP1Transaction validity in ConnectBlock(): "
                        "{}",
                        ex.what());
                } catch (...) {
                    return NLog.error(
                        "Error while verifying NTP1Transaction validity in ConnectBlock(). "
                        "Unknown exception thrown");
                }
            }

            if (EnableEnforceUniqueTokenSymbols(txdb)) {
                try {
                    AssertIssuanceUniquenessInBlock(issuedTokensSymbolsInThisBlock, txdb, tx,
                                                    mapQueuedNTP1Inputs, mapQueuedChanges);
                } catch (std::exception& ex) {
                    reject = CBlockReject(REJECT_INVALID, "ntp1-error-issuance-symbol-duplicate",
                                          this->GetHash());
                    return NLog.error("Error while verifying the uniqueness of issued token symbol in "
                                      "ConnectBlock(): "
                                      "{}",
                                      ex.what());
                } catch (...) {
                    reject = CBlockReject(REJECT_INVALID,
                                          "ntp1-error-issuance-symbol-duplicate-unknown-error",
                                          this->GetHash());
                    return NLog.error("Error while verifying the uniqueness of issued token symbol in "
                                      "ConnectBlock(). "
                                      "Unknown exception thrown");
                }
            }

            if (tx.ConnectInputs(txdb, mapInputs, mapQueuedChanges, posThisTx, pindex, true, false, this)
                    .isErr()) {
                return false;
            }
        }

        mapQueuedChanges[hashTx]          = CTxIndex(posThisTx, tx.vout.size());
        mapQueuedNTP1Inputs[tx.GetHash()] = inputsWithNTP1;
    }

    if (IsProofOfWork()) {
        const CAmount nExpectedReward = GetProofOfWorkReward(txdb, nFees);
        const CAmount nRewardInBlock  = vtx[0].GetValueOut();
        // Check coinbase reward
        if (nRewardInBlock > nExpectedReward) {
            reject = CBlockReject(REJECT_INVALID, "bad-cb-amount", this->GetHash());
            return DoS(
                50, NLog.error("ConnectBlock() : coinbase reward exceeded (actual={} vs calculated={})",
                               vtx[0].GetValueOut(), nExpectedReward));
        }
    }
    if (IsProofOfStake()) {
        // ppcoin: coin stake tx earns reward instead of paying fee
        uint64_t nCoinAge;
        if (!vtx[1].GetCoinAge(txdb, nCoinAge))
            return NLog.error("ConnectBlock() : {} unable to get coin age for coinstake",
                              vtx[1].GetHash().ToString());

        const CAmount nCalculatedStakeReward = GetProofOfStakeReward(txdb, nCoinAge, nFees);

        if (nStakeReward > nCalculatedStakeReward)
            return DoS(100,
                       NLog.error("ConnectBlock() : coinstake pays too much(actual={} vs calculated={})",
                                  nStakeReward, nCalculatedStakeReward));
    }

    if (fJustCheck)
        return true;

    // ppcoin: track money supply and mint amount info
    const CAmount                  nMint            = nValueOut - nValueIn + nFees;
    const boost::optional<CAmount> nPrevMoneySupply = [&]() {
        // genesis and block 1 have prev money supply = 0
        if (pindex->hashPrev != 0 && pindex->nHeight > 1) {
            const uint256 prevHash = pindex->hashPrev;

            const boost::optional<BlockMetadata> blockMetadata = txdb.ReadBlockMetadata(prevHash);
            if (blockMetadata) {
                return boost::make_optional(blockMetadata->getMoneySupply());
            } else {
                return boost::optional<CAmount>();
            }
        }
        return boost::make_optional<CAmount>(0);
    }();
    if (!nPrevMoneySupply) {
        return NLog.error("Connect() : Failed to retrieve prev money supply from block metadata");
    }

    const CAmount nMoneySupply = *nPrevMoneySupply + nValueOut - nValueIn;

    const BlockMetadata blockMetadata(blockHash, nMoneySupply, nMint);
    if (!txdb.WriteBlockMetadata(blockMetadata))
        return NLog.error("Connect() : WriteBlockMetadata for blockMetadata failed");

    if (!txdb.WriteBlockIndex(*pindex))
        return NLog.error("Connect() : WriteBlockIndex for pindex failed");

    if (!txdb.WriteBlockHashOfHeight(pindex->nHeight, pindex->GetBlockHash()))
        return NLog.error("Connect() : WriteBlockHashOfHeight for pindex failed");

    // Write queued txindex changes
    for (std::map<uint256, CTxIndex>::iterator mi = mapQueuedChanges.begin();
         mi != mapQueuedChanges.end(); ++mi) {
        if (!txdb.UpdateTxIndex((*mi).first, (*mi).second))
            return NLog.error("ConnectBlock() : UpdateTxIndex failed");
    }

    // This scope does NTP1 data writing
    {
        try {
            WriteNTP1BlockTransactionsToDisk(vtx, txdb);
        } catch (std::exception& ex) {
            if (Params().GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON, txdb)) {
                return NLog.error("Unable to get NTP1 transaction written in ConnectBlock(). Error: {}",
                                  ex.what());
            }
        } catch (...) {
            if (Params().GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON, txdb)) {
                return NLog.error("Unable to get NTP1 transaction written in ConnectBlock(). An unknown "
                                  "exception was "
                                  "thrown");
            }
        }
    }

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->hashPrev != 0) {
        boost::optional<CBlockIndex> blockindexPrev = pindex->getPrev(txdb);
        if (!blockindexPrev) {
            return NLog.error("ConnectBlock() : ReadBlockIndex failed for hashPrev");
        }
        blockindexPrev->hashNext = pindex->GetBlockHash();
        if (!txdb.WriteBlockIndex(*blockindexPrev))
            return NLog.error("ConnectBlock() : WriteBlockIndex failed");
        if (blockindexPrev->nHeight == 0) {
            pindexGenesisBlock->hashNext = pindex->GetBlockHash();
        }
    }

    // we change the best hash. Remember this is within a transaction and will be reverted in case of
    // failure.
    txdb.WriteHashBestChain(pindex->GetBlockHash());

    return true;
}

// Called from inside SetBestChain: attaches a block to the new best chain being built
bool CBlock::SetBestChainInner(CTxDB& txdb, const boost::optional<CBlockIndex>& pindexNew,
                               const bool createDbTransaction)
{
    const uint256 hash = pindexNew->GetBlockHash();

    // Adding to current best branch
    if (!ConnectBlock(txdb, pindexNew) || !txdb.WriteHashBestChain(hash)) {
        if (createDbTransaction) {
            txdb.TxnAbort();
        }
        InvalidChainFound(*pindexNew, txdb);
        return false;
    }
    if (createDbTransaction && !txdb.TxnCommit())
        return NLog.error("SetBestChain() : TxnCommit failed");

    // Add to current best branch
    //    pindexNew->pprev->pnext = pindexNew;

    // Delete redundant memory transactions
    for (const CTransaction& tx : vtx)
        mempool.remove(tx);

    return true;
}

bool CBlock::SetBestChain(CTxDB& txdb, const boost::optional<CBlockIndex>& pindexNew,
                          const bool createDbTransaction)
{
    const uint256 hash = pindexNew->GetBlockHash();

    if (createDbTransaction && !txdb.TxnBegin())
        return NLog.error("SetBestChain() : TxnBegin failed");

    if (pindexGenesisBlock == nullptr && hash == Params().GenesisBlockHash()) {
        txdb.WriteHashBestChain(hash);
        pindexGenesisBlock = boost::make_shared<CBlockIndex>(*pindexNew);
        if (!txdb.WriteBlockIndex(*pindexGenesisBlock)) {
            return NLog.error("Failed to write genesis block index");
        }
        if (!txdb.WriteBlockHashOfHeight(0, hash)) {
            return NLog.error("Failed to write genesis block height");
        }
        if (createDbTransaction && !txdb.TxnCommit())
            return NLog.error("SetBestChain() : TxnCommit failed");
    } else if (hashPrevBlock == txdb.GetBestBlockHash()) {
        if (!SetBestChainInner(txdb, pindexNew, createDbTransaction))
            return NLog.error("SetBestChain() : SetBestChainInner failed");
    } else {
        // the first block in the new chain that will cause it to become the new best chain
        boost::optional<CBlockIndex> pindexIntermediate = *pindexNew;

        // list of blocks that need to be connected afterwards
        std::vector<boost::optional<CBlockIndex>> vpindexSecondary;

        // Reorganize is costly in terms of db load, as it works in a single db transaction.
        // Try to limit how much needs to be done inside
        const uint256 bestChainTrust = txdb.GetBestBlockIndex()->nChainTrust;
        while (pindexIntermediate && pindexIntermediate->hashPrev != 0) {
            boost::optional<CBlockIndex> prevIndex = pindexIntermediate->getPrev(txdb);
            if (!prevIndex) {
                return NLog.error(
                    "CRITICAL: Even though hashPrev for block index {} with hash {} is not zero "
                    "(hashPrev={}), it was not found in the database",
                    pindexIntermediate->nHeight, pindexIntermediate->blockHash.ToString(),
                    pindexIntermediate->hashPrev.ToString());
            }
            if (prevIndex->nChainTrust <= bestChainTrust) {
                break;
            }
            vpindexSecondary.push_back(pindexIntermediate);
            pindexIntermediate = prevIndex;
        }

        if (!vpindexSecondary.empty())
            NLog.write(b_sev::info, "Postponing {} reconnects", vpindexSecondary.size());

        // Switch to new best branch
        if (!Reorganize(txdb, pindexIntermediate, createDbTransaction)) {
            if (createDbTransaction) {
                txdb.TxnAbort();
            }
            InvalidChainFound(*pindexNew, txdb);
            return NLog.error("SetBestChain() : Reorganize failed");
        }

        // Connect further blocks
        BOOST_REVERSE_FOREACH(const boost::optional<CBlockIndex>& pindex, vpindexSecondary)
        {
            CBlock block;
            if (!block.ReadFromDisk(&*pindex, txdb)) {
                NLog.write(b_sev::err, "SetBestChain() : ReadFromDisk failed");
                break;
            }
            if (createDbTransaction && !txdb.TxnBegin()) {
                NLog.write(b_sev::err, "SetBestChain() : TxnBegin 2 failed");
                break;
            }
            // errors now are not fatal, we still did a reorganisation to a new chain in a valid way
            if (!block.SetBestChainInner(txdb, pindex, createDbTransaction))
                break;
        }
    }

    // Update best block in wallet (so we can detect restored wallets)
    const bool fIsInitialDownload = IsInitialBlockDownload(txdb);
    if (!fIsInitialDownload) {
        const CBlockLocator locator(&*pindexNew, txdb);
        ::SetBestChain(locator);
    }

    // New best block (the best chain is now done with WriteHashBestChain)
    nTimeLastBestBlockReceived = GetTime();
    nTransactionsUpdated++;

    const boost::optional<CBlockIndex>& pindexBestPtr = pindexNew;
    uint256                             nBestBlockTrust =
        pindexBestPtr->nHeight != 0
                                        ? (pindexBestPtr->nChainTrust - pindexBestPtr->getPrev(txdb)->nChainTrust)
                                        : pindexBestPtr->nChainTrust;

    NLog.write(b_sev::info, "SetBestChain: new best={}  height={}  trust={}  blocktrust={}  date={}",
               txdb.GetBestBlockHash().ToString(), txdb.GetBestChainHeight().value_or(0),
               CBigNum(txdb.GetBestChainTrust().value_or(0)).ToString(), nBestBlockTrust.Get64(),
               DateTimeStrFormat("%x %H:%M:%S", pindexBestPtr->GetBlockTime()));

    // Check the version of the last 100 blocks to see if we need to upgrade:
    if (!fIsInitialDownload) {
        using BlockVersionCacheType = BlockIndexLRUCache<int32_t>;

        static thread_local typename BlockVersionCacheType::ExtractorFunc extractorFunc =
            [](const CBlockIndex& bi) { return bi.nVersion; };

        static thread_local BlockVersionCacheType blockIndexCache(1000, extractorFunc);

        int     nUpgraded = 0;
        uint256 h         = txdb.GetBestBlockHash();
        for (int i = 0; i < 100 && h != 0; i++) {
            const boost::optional<BlockVersionCacheType::BICacheEntry> blockIndexVersionData =
                blockIndexCache.get(txdb, h);
            if (!blockIndexVersionData) {
                NLog.write(b_sev::err, "CRITICAL: Error retrieving previous block of block {}",
                           h.ToString());
                break;
            }
            if (blockIndexVersionData->value > CBlock::CURRENT_VERSION)
                ++nUpgraded;
            h = blockIndexVersionData->prevHash;
        }
        if (nUpgraded > 0)
            NLog.write(b_sev::info, "SetBestChain: {} of last 100 blocks above version {}", nUpgraded,
                       (int)CBlock::CURRENT_VERSION);
        if (nUpgraded > 100 / 2)
            // strMiscWarning is read by GetWarnings(), called by Qt and the JSON-RPC code to warn the
            // user:
            strMiscWarning = _("Warning: This version is obsolete, upgrade required!");
    }

    std::string strCmd = GetArg("-blocknotify", "");

    if (!fIsInitialDownload && !strCmd.empty()) {
        boost::replace_all(strCmd, "%s", txdb.GetBestBlockHash().GetHex());
        boost::thread t(runCommand, strCmd); // thread runs free
    }

    return true;
}

bool CBlock::ReadFromDisk(const CBlockIndex* pindex, const ITxDB& txdb, bool fReadTransactions)
{
    if (!fReadTransactions) {
        *this = pindex->GetBlockHeader();
        return true;
    }
    if (!ReadFromDisk(pindex->GetBlockHash(), txdb, fReadTransactions))
        return false;
    if (GetHash() != pindex->GetBlockHash())
        return NLog.error("CBlock::ReadFromDisk() : GetHash() doesn't match index");
    return true;
}

bool CBlock::IsProofOfStake() const { return (vtx.size() > 1 && vtx[1].IsCoinStake()); }

boost::optional<CBlockIndex> CBlock::FindBlockByHeight(int nHeight)
{
    const CTxDB              txdb;
    boost::optional<uint256> hash = txdb.ReadBlockHashOfHeight(nHeight);
    if (!hash) {
        return boost::none;
    }
    return txdb.ReadBlockIndex(*hash);
}

void CBlock::InvalidChainFound(const CBlockIndex& pindexNew, ITxDB& txdb)
{
    if (pindexNew.nChainTrust > nBestInvalidTrust) {
        nBestInvalidTrust = pindexNew.nChainTrust;
        txdb.WriteBestInvalidTrust(CBigNum(nBestInvalidTrust));
        uiInterface.NotifyBlocksChanged();
    }

    boost::optional<CBlockIndex> prevBI = pindexNew.getPrev(txdb);
    if (!prevBI) {
        NLog.write(b_sev::err,
                   "CRITICAL CONSISTENCY FAILURE: Failed to get prev block for invalid chain trust");
    }
    uint256 nBestInvalidBlockTrust = pindexNew.nChainTrust - prevBI.value_or(CBlockIndex()).nChainTrust;

    const boost::optional<CBlockIndex> pindexBestPtr = txdb.GetBestBlockIndex();

    uint256 nBestBlockTrust =
        pindexBestPtr->nHeight != 0
            ? (pindexBestPtr->nChainTrust - pindexBestPtr->getPrev(txdb)->nChainTrust)
            : pindexBestPtr->nChainTrust;

    NLog.write(b_sev::err,
               "InvalidChainFound: invalid block={}  height={}  trust={}  blocktrust={} date={}",
               pindexNew.GetBlockHash().ToString(), pindexNew.nHeight,
               CBigNum(pindexNew.nChainTrust).ToString(), nBestInvalidBlockTrust.Get64(),
               DateTimeStrFormat("%x %H:%M:%S", pindexNew.GetBlockTime()));
    NLog.write(b_sev::err,
               "InvalidChainFound:  current best={}  height={}  trust={}  blocktrust={} date={}",
               txdb.GetBestBlockHash().ToString(), txdb.GetBestChainHeight().value_or(0),
               CBigNum(pindexBestPtr->nChainTrust).ToString(), nBestBlockTrust.Get64(),
               DateTimeStrFormat("%x %H:%M:%S", pindexBestPtr->GetBlockTime()));
}

bool CBlock::Reorganize(CTxDB& txdb, const boost::optional<CBlockIndex>& pindexNew,
                        const bool createDbTransaction)
{
    NLog.write(b_sev::info, "REORGANIZE");

    // Find the fork
    boost::optional<CBlockIndex> pfork   = txdb.GetBestBlockIndex();
    boost::optional<CBlockIndex> plonger = pindexNew;
    while (pfork->GetBlockHash() != plonger->GetBlockHash()) {
        while (plonger->nHeight > pfork->nHeight)
            if (!(plonger = plonger->getPrev(txdb)))
                return NLog.error("Reorganize() : plonger->pprev is null");
        if (pfork->GetBlockHash() == plonger->GetBlockHash())
            break;
        if (!(pfork = pfork->getPrev(txdb)))
            return NLog.error("Reorganize() : pfork->pprev is null");
    }

    // List of what to disconnect
    std::vector<boost::optional<CBlockIndex>> vDisconnect;
    for (boost::optional<CBlockIndex> pindex                     = txdb.GetBestBlockIndex();
         pindex->GetBlockHash() != pfork->GetBlockHash(); pindex = pindex->getPrev(txdb)) {
        vDisconnect.push_back(pindex);
    }

    // List of what to connect
    std::vector<boost::optional<CBlockIndex>> vConnect;
    for (boost::optional<CBlockIndex> pindex                     = pindexNew;
         pindex->GetBlockHash() != pfork->GetBlockHash(); pindex = pindex->getPrev(txdb)) {
        vConnect.push_back(pindex);
    }
    reverse(vConnect.begin(), vConnect.end());

    NLog.write(b_sev::info, "REORGANIZE: Disconnect {} blocks; {}..{}", vDisconnect.size(),
               pfork->GetBlockHash().ToString(), txdb.GetBestBlockHash().ToString());
    NLog.write(b_sev::info, "REORGANIZE: Connect {} blocks; {}..{}", vConnect.size(),
               pfork->GetBlockHash().ToString(), pindexNew->GetBlockHash().ToString());

    // Disconnect shorter branch
    std::list<CTransaction> vResurrect;
    for (boost::optional<CBlockIndex>& pindex : vDisconnect) {
        CBlock block;
        if (!block.ReadFromDisk(&*pindex, txdb))
            return NLog.error("Reorganize() : ReadFromDisk for disconnect failed");
        if (!block.DisconnectBlock(txdb, *pindex))
            return NLog.error("Reorganize() : DisconnectBlock {} failed",
                              pindex->GetBlockHash().ToString());

        // Queue memory transactions to resurrect.
        // We only do this for blocks after the last checkpoint (reorganisation before that
        // point should only happen with -reindex/-loadblock, or a misbehaving peer.
        BOOST_REVERSE_FOREACH(const CTransaction& tx, block.vtx)
        if (!(tx.IsCoinBase() || tx.IsCoinStake()) &&
            pindex->nHeight > Checkpoints::GetTotalBlocksEstimate())
            vResurrect.push_front(tx);
    }

    // Connect longer branch
    std::vector<CTransaction> vDelete;
    for (unsigned int i = 0; i < vConnect.size(); i++) {
        boost::optional<CBlockIndex> pindex = vConnect[i];
        CBlock                       block;
        if (!block.ReadFromDisk(&*pindex, txdb))
            return NLog.error("Reorganize() : ReadFromDisk for connect failed");
        // this is necessary to register in CBlockReject why a block was rejected
        CBlock* blockPtr = nullptr;
        if (block.GetHash() == this->GetHash()) {
            blockPtr = this;
        } else {
            blockPtr = &block;
        }
        if (!blockPtr->ConnectBlock(txdb, pindex)) {
            // Invalid block
            return NLog.error("Reorganize() : ConnectBlock {} failed",
                              pindex->GetBlockHash().ToString());
        }

        // Queue memory transactions to delete
        for (const CTransaction& tx : block.vtx)
            vDelete.push_back(tx);
    }
    if (!txdb.WriteHashBestChain(pindexNew->GetBlockHash()))
        return NLog.error("Reorganize() : WriteHashBestChain failed");

    // Make sure it's successfully written to disk before changing memory structure
    if (createDbTransaction && !txdb.TxnCommit())
        return NLog.error("Reorganize() : TxnCommit failed");

    //    // Disconnect shorter branch
    //    for (boost::optional<CBlockIndex>& pindex : vDisconnect)
    //        if (pindex->pprev)
    //            pindex->pprev->pnext = nullptr;

    //    // Connect longer branch
    //    for (boost::optional<CBlockIndex>& pindex : vConnect)
    //        if (pindex->pprev)
    //            pindex->pprev->pnext = pindex;

    // Resurrect memory transactions that were in the disconnected branch
    for (CTransaction& tx : vResurrect)
        AcceptToMemoryPool(mempool, tx, &txdb);

    // Delete redundant memory transactions that are in the connected branch
    for (const CTransaction& tx : vDelete) {
        mempool.remove(tx);
        mempool.removeConflicts(tx);
    }

    NLog.write(b_sev::info, "REORGANIZE: done");

    return true;
}

boost::optional<CBlockIndex> CBlock::AddToBlockIndex(const uint256&                      blockHash,
                                                     const boost::optional<CBlockIndex>& prevBlockIndex,
                                                     const uint256& hashProof, CTxDB& txdb,
                                                     const bool createDbTransaction)
{
    // Check for duplicate
    if (txdb.ReadBlockIndex(blockHash))
        return NLog.errorn("AddToBlockIndex() : {} already exists", blockHash.ToString());

    // Construct new block index object
    CBlockIndex pindexNew = CBlockIndex(blockHash, *this);

    pindexNew.blockHash = blockHash;
    if (prevBlockIndex) {
        //        pindexNew.pprev    = biPrev;
        pindexNew.hashPrev = hashPrevBlock;
        pindexNew.nHeight  = prevBlockIndex->nHeight + 1;
    }

    // ppcoin: compute chain trust score
    pindexNew.nChainTrust =
        (prevBlockIndex ? prevBlockIndex->nChainTrust : 0) + pindexNew.GetBlockTrust();

    // ppcoin: compute stake entropy bit for stake modifier
    if (!pindexNew.SetStakeEntropyBit(GetStakeEntropyBit(blockHash)))
        return NLog.errorn("AddToBlockIndex() : SetStakeEntropyBit() failed");

    // Record proof hash value
    pindexNew.hashProof = hashProof;

    // ppcoin: compute stake modifier
    uint64_t nStakeModifier          = 0;
    bool     fGeneratedStakeModifier = false;
    if (!ComputeNextStakeModifier(txdb, prevBlockIndex ? &*prevBlockIndex : nullptr, nStakeModifier,
                                  fGeneratedStakeModifier))
        return NLog.errorn("AddToBlockIndex() : ComputeNextStakeModifier() failed");
    pindexNew.SetStakeModifier(nStakeModifier, fGeneratedStakeModifier);
    pindexNew.nStakeModifierChecksum = GetStakeModifierChecksum(&pindexNew, txdb);
    if (!CheckStakeModifierCheckpoints(pindexNew.nHeight, pindexNew.nStakeModifierChecksum)) {
        return NLog.errorn("AddToBlockIndex() : Rejected by stake modifier checkpoint height={}, "
                           "modifier=0x{:016x}",
                           pindexNew.nHeight, nStakeModifier);
    }
    // return NLog.error("AddToBlockIndex() : Rejected by stake modifier checkpoint height={},
    // checksum=0x{:016x}", pindexNew.nHeight, pindexNew.nStakeModifierChecksum);

    // Add to mapBlockIndex
    //    mapBlockIndex.set(hash, pindexNew);
    if (pindexNew.IsProofOfStake())
        txdb.WriteStakeSeen(std::make_pair(pindexNew.prevoutStake, pindexNew.nStakeTime));

    // Write to disk block index
    if (createDbTransaction && !txdb.TxnBegin())
        return boost::none;
    txdb.WriteBlockIndex(pindexNew);
    if (createDbTransaction && !txdb.TxnCommit())
        return boost::none;

    LOCK(cs_main);

    // New best
    if (pindexNew.nChainTrust > txdb.GetBestChainTrust().value_or(0))
        if (!SetBestChain(txdb, pindexNew, createDbTransaction))
            return boost::none;

    if (pindexNew.GetBlockHash() == txdb.GetBestBlockHash()) {
        // Notify UI to display prev block's coinbase if it was ours
        static uint256 hashPrevBestCoinBase;
        UpdatedTransaction(hashPrevBestCoinBase);
        hashPrevBestCoinBase = vtx[0].GetHash();
    }

    uiInterface.NotifyBlocksChanged();

    return boost::make_optional(std::move(pindexNew));
}

bool CBlock::CheckBlock(const ITxDB& txdb, const uint256& blockHash, bool fCheckPOW,
                        bool fCheckMerkleRoot, bool fCheckSig)
{
    // These are checks that are independent of context
    // that can be verified before saving an orphan block.

    // Size limits
    const unsigned int nSizeLimit = MaxBlockSize(txdb);
    if (vtx.empty() || vtx.size() > nSizeLimit ||
        ::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION) > nSizeLimit) {
        reject = CBlockReject(REJECT_INVALID, "bad-blk-length", this->GetHash());
        return DoS(100, NLog.error("CheckBlock() : size limits failed"));
    }

    // Check proof of work matches claimed amount
    if (fCheckPOW && IsProofOfWork() && !CheckProofOfWork(blockHash, nBits)) {
        reject = CBlockReject(REJECT_INVALID, "high-hash", this->GetHash());
        return DoS(50, NLog.error("CheckBlock() : proof of work failed"));
    }

    // Check timestamp
    if (GetBlockTime() > FutureDrift(GetAdjustedTime())) {
        reject = CBlockReject(REJECT_INVALID, "time-too-new", this->GetHash());
        return NLog.error("CheckBlock() : block timestamp too far in the future");
    }

    // if no transactions exist, it's an invalid block
    if (vtx.empty()) {
        reject = CBlockReject(REJECT_INVALID, "bad-blk-length", this->GetHash());
        return DoS(100, NLog.error("CheckBlock() : block with no transactions"));
    }

    // First transaction must be coinbase, the rest must not be
    if (vtx.empty() || !vtx[0].IsCoinBase()) {
        reject = CBlockReject(REJECT_INVALID, "bad-cb-missing", this->GetHash());
        return DoS(100, NLog.error("CheckBlock() : first tx is not coinbase"));
    }
    for (unsigned int i = 1; i < vtx.size(); i++) {
        if (vtx[i].IsCoinBase()) {
            reject = CBlockReject(REJECT_INVALID, "bad-cb-multiple", this->GetHash());
            return DoS(100, NLog.error("CheckBlock() : more than one coinbase"));
        }
    }

    // Check coinbase timestamp
    if (GetBlockTime() > FutureDrift((int64_t)vtx[0].nTime))
        return DoS(50, NLog.error("CheckBlock() : coinbase timestamp is too early"));

    if (IsProofOfStake()) {
        // Coinbase output should be empty if proof-of-stake block
        if (vtx[0].vout.size() != 1 || !vtx[0].vout[0].IsEmpty())
            return DoS(100,
                       NLog.error("CheckBlock() : coinbase output not empty for proof-of-stake block"));

        // Second transaction must be coinstake, the rest must not be
        if (vtx.empty() || !vtx[1].IsCoinStake())
            return DoS(100, NLog.error("CheckBlock() : second tx is not coinstake"));
        for (unsigned int i = 2; i < vtx.size(); i++)
            if (vtx[i].IsCoinStake())
                return DoS(100, NLog.error("CheckBlock() : more than one coinstake"));

        // Check coinstake timestamp
        if (!CheckCoinStakeTimestamp(GetBlockTime(), (int64_t)vtx[1].nTime))
            return DoS(
                50, NLog.error("CheckBlock() : coinstake timestamp violation nTimeBlock={} nTimeTx={}",
                               GetBlockTime(), vtx[1].nTime));

        // NovaCoin: check proof-of-stake block signature
        if (fCheckSig && !CheckBlockSignature(txdb, blockHash))
            return DoS(100, NLog.error("CheckBlock() : bad proof-of-stake block signature"));
    }

    // Check transactions
    for (uint32_t i = 0; i < vtx.size(); i++) {
        const CTransaction& tx = vtx[i];

        const auto checkTxResult = tx.CheckTransaction(txdb, this);
        if (checkTxResult.isErr())
            return DoS(tx.nDoS,
                       NLog.error("CheckBlock() : CheckTransaction failed: (Msg: {}) - (Debug: {})",
                                  checkTxResult.unwrapErr().GetRejectReason(),
                                  checkTxResult.unwrapErr().GetDebugMessage()));

        // ppcoin: check transaction timestamp
        if (GetBlockTime() < (int64_t)tx.nTime)
            return DoS(50, NLog.error("CheckBlock() : block timestamp ({}) is earlier than transaction "
                                      "(tx number {} in block) timestamp ({})",
                                      GetBlockTime(), i, (int64_t)tx.nTime));
    }

    // Check for duplicate txids. This is caught by ConnectInputs(),
    // but catching it earlier avoids a potential DoS attack:
    std::set<uint256> uniqueTx;
    for (const CTransaction& tx : vtx) {
        uniqueTx.insert(tx.GetHash());
    }
    if (uniqueTx.size() != vtx.size()) {
        reject = CBlockReject(REJECT_INVALID, "bad-txns-duplicate", this->GetHash());
        return DoS(100, NLog.error("CheckBlock() : duplicate transaction"));
    }

    unsigned int nSigOps = 0;
    for (const CTransaction& tx : vtx) {
        nSigOps += tx.GetLegacySigOpCount();
    }
    if (nSigOps > MAX_BLOCK_SIGOPS) {
        reject = CBlockReject(REJECT_INVALID, "bad-blk-sigops", this->GetHash());
        return DoS(100, NLog.error("CheckBlock() : out-of-bounds SigOpCount"));
    }

    // Check merkle root
    bool merkleRootMutated;
    if (fCheckMerkleRoot && hashMerkleRoot != GetMerkleRoot(&merkleRootMutated)) {
        reject = CBlockReject(REJECT_INVALID, "bad-txnmrklroot", this->GetHash());
        return DoS(100, NLog.error("CheckBlock() : hashMerkleRoot mismatch"));
    }

    // Check for merkle tree malleability (CVE-2012-2459): repeating sequences
    // of transactions in a block without affecting the merkle root of a block,
    // while still invalidating it.
    if (merkleRootMutated) {
        reject = CBlockReject(REJECT_INVALID, "bad-txns-duplicate", this->GetHash());
        return DoS(100, NLog.error("CheckBlock() : hashMerkleRoot duplicate: duplicate transaction"));
    }

    return true;
}

bool CBlock::AcceptBlock(const CBlockIndex& prevBlockIndex, const uint256& blockHash)
{
    AssertLockHeld(cs_main);

    assert(prevBlockIndex.GetBlockHash() == hashPrevBlock);

    const CTxDB txdb;

    if (nVersion > CURRENT_VERSION)
        return DoS(100, NLog.error("AcceptBlock() : reject unknown block version {}", nVersion));

    // Check for duplicate
    if (txdb.ReadBlockIndex(blockHash))
        return NLog.error("AcceptBlock() : block already in database");

    // protect against a possible attack where an attacker sends predecessors of very early blocks in the
    // blockchain, forcing a non-necessary scan of the whole blockchain
    const int64_t maxCheckpointBlockHeight = Checkpoints::GetLastCheckpointBlockHeight();
    if (txdb.GetBestChainHeight().value_or(0) > maxCheckpointBlockHeight + 1) {
        const uint256 prevBlockHash           = this->hashPrevBlock;
        int64_t       newBlockPrevBlockHeight = prevBlockIndex.nHeight;
        if (newBlockPrevBlockHeight + 1 < maxCheckpointBlockHeight) {
            return DoS(
                100,
                NLog.error("Prevblock of block {}, which is {}, is behind the latest checkpoint block "
                           "height: {}",
                           blockHash.ToString(), prevBlockHash.ToString(), maxCheckpointBlockHeight));
        }
    }

    try {
        const auto viuResult = VerifyInputsUnspent(txdb);
        if (viuResult.isErr()) {
            reject = CBlockReject(REJECT_INVALID,
                                  fmt::format("bad-txns-inputs-missingorspent-{}",
                                              VIUErrorToString(viuResult.unwrapErr())),
                                  blockHash);
            return DoS(100, NLog.error("VerifyInputsUnspent() failed for block {} ({})",
                                       blockHash.ToString(), VIUErrorToString(viuResult.unwrapErr())));
        }
    } catch (const std::exception& ex) {
        return NLog.critical("VerifyInputsUnspent() threw an exception for block {}; with error: {}",
                             blockHash.ToString(), ex.what());
    }

    const int nHeight = prevBlockIndex.nHeight + 1;

    if (IsProofOfWork() && nHeight > Params().LastPoWBlock())
        return DoS(100, NLog.error("AcceptBlock() : reject proof-of-work at height {}", nHeight));

    {
        const auto hasColdStakingResult = HasColdStaking(txdb);
        if (hasColdStakingResult.isErr()) {
            return DoS(100,
                       NLog.error("AcceptBlock() : reject cold-stake at height {} with error", nHeight));
        }

        if (hasColdStakingResult.unwrap() && !Params().IsColdStakingEnabled(txdb)) {
            return DoS(100, NLog.error("AcceptBlock() : reject cold-staked at height {}", nHeight));
        }
    }

    // Check proof-of-work or proof-of-stake
    if (nBits != GetNextTargetRequired(txdb, &prevBlockIndex, IsProofOfStake())) {
        reject = CBlockReject(REJECT_INVALID, "bad-diffbits", blockHash);
        return DoS(100, NLog.error("AcceptBlock() : incorrect {}",
                                   IsProofOfWork() ? "proof-of-work" : "proof-of-stake"));
    }

    // Check timestamp against prev
    if (GetBlockTime() <= prevBlockIndex.GetPastTimeLimit(txdb) ||
        FutureDrift(GetBlockTime()) < prevBlockIndex.GetBlockTime()) {
        reject = CBlockReject(REJECT_INVALID, "time-too-old", blockHash);
        return NLog.error("AcceptBlock() : block's timestamp is too early");
    }

    // Check that all transactions are finalized
    for (const CTransaction& tx : vtx)
        if (!IsFinalTx(tx, txdb, nHeight, GetBlockTime())) {
            reject = CBlockReject(REJECT_INVALID, "bad-txns-nonfinal", blockHash);
            return DoS(10, NLog.error("AcceptBlock() : contains a non-final transaction"));
        }

    // Check that the block chain matches the known block chain up to a checkpoint
    if (!Checkpoints::CheckHardened(nHeight, blockHash))
        return DoS(100,
                   NLog.error("AcceptBlock() : rejected by hardened checkpoint lock-in at {}", nHeight));

    uint256 hashProof;
    // Verify hash target and signature of coinstake tx
    if (IsProofOfStake()) {
        uint256 targetProofOfStake;
        if (!CheckProofOfStake(txdb, vtx[1], nBits, hashProof, targetProofOfStake)) {
            NLog.write(b_sev::err, "WARNING: AcceptBlock(): check proof-of-stake failed for block {}",
                       blockHash.ToString());
            return false; // do not error here as we expect this during initial block download
        }
    }
    // PoW is checked in CheckBlock()
    if (IsProofOfWork()) {
        hashProof = GetPoWHash();
    }

    const bool cpSatisfies = Checkpoints::CheckSync(txdb, blockHash, &prevBlockIndex);

    // Check that the block satisfies synchronized checkpoint
    if (CheckpointsMode == Checkpoints::CPMode_STRICT && !cpSatisfies)
        return NLog.error("AcceptBlock() : rejected by synchronized checkpoint");

    if (CheckpointsMode == Checkpoints::CPMode_ADVISORY && !cpSatisfies)
        strMiscWarning = _("WARNING: syncronized checkpoint violation detected, but skipped!");

    // Enforce rule that the coinbase starts with serialized block height
    CScript expect = CScript() << nHeight;
    if (vtx[0].vin[0].scriptSig.size() < expect.size() ||
        !std::equal(expect.begin(), expect.end(), vtx[0].vin[0].scriptSig.begin()))
        return DoS(100, NLog.error("AcceptBlock() : block height mismatch in coinbase"));

    // Write block to history file
    if (!CheckDiskSpace(::GetSerializeSize(*this, SER_DISK, CLIENT_VERSION)))
        return NLog.error("AcceptBlock() : out of disk space");

    if (!WriteToDisk(prevBlockIndex, hashProof, blockHash))
        return NLog.error("AcceptBlock() : WriteToDisk failed");

    // Relay inventory, but don't relay old inventory during initial block download
    int nBlockEstimate = Checkpoints::GetTotalBlocksEstimate();
    if (txdb.GetBestBlockHash() == blockHash) {
        LOCK(cs_vNodes);
        for (CNode* pnode : vNodes)
            if (txdb.GetBestChainHeight().value_or(0) >
                (pnode->nStartingHeight != -1 ? pnode->nStartingHeight - 2000 : nBlockEstimate))
                pnode->PushInventory(CInv(MSG_BLOCK, blockHash));
    }

    return true;
}

boost::optional<CKeyID> GetKeyIDFromOutput(const CTxDB& txdb, const CTxOut& txout)
{
    std::vector<valtype> vSolutions;
    txnouttype           whichType;
    if (!Solver(txdb, txout.scriptPubKey, whichType, vSolutions))
        return boost::none;
    if (whichType == TX_PUBKEY) {
        return CPubKey(vSolutions[0]).GetID();
    } else if (whichType == TX_PUBKEYHASH || whichType == TX_COLDSTAKE) {
        return CKeyID(uint160(vSolutions[0]));
    } else {
        return boost::none;
    }
    return boost::none;
}

// novacoin: attempt to generate suitable proof-of-stake
bool CBlock::SignBlock(const CTxDB& txdb, const CWallet& wallet, int64_t nFees,
                       const boost::optional<std::set<std::pair<uint256, unsigned>>>& customInputs,
                       const CAmount                                                  extraPayoutForTest)
{
    // if we are trying to sign
    //    something except proof-of-stake block template
    if (!vtx[0].vout[0].IsEmpty())
        return false;

    // if we are trying to sign
    //    a complete proof-of-stake block
    if (IsProofOfStake())
        return true;

    boost::optional<CBlockIndex>        pindexBestPtr = txdb.GetBestBlockIndex();
    const boost::optional<CTransaction> coinStake     = stakeMaker.CreateCoinStake(
        txdb, wallet, nBits, nFees, nReserveBalance, customInputs, extraPayoutForTest);

    if (!coinStake) {
        return false;
    }

    const int64_t minTime =
        std::max(pindexBestPtr->GetPastTimeLimit(txdb) + 1, PastDrift(pindexBestPtr->GetBlockTime()));

    if (coinStake->nTime < minTime) {
        return false;
    }

    // make sure coinstake would meet timestamp protocol
    // as it would be the same as the block timestamp
    vtx[0].nTime = nTime = coinStake->nTime;
    nTime                = std::max(pindexBestPtr->GetPastTimeLimit(txdb) + 1, GetMaxTransactionTime());
    nTime                = std::max(GetBlockTime(), PastDrift(pindexBestPtr->GetBlockTime()));

    // we have to make sure that we have no future timestamps in our transactions set
    for (auto it = vtx.begin(); it != vtx.end();) {
        if (it->nTime > nTime) {
            it = vtx.erase(it);
        } else {
            ++it;
        }
    }

    // tx[1] is the coinstake transaction
    vtx.insert(vtx.begin() + 1, *coinStake);
    hashMerkleRoot = GetMerkleRoot();

    const boost::optional<CKeyID> keyID = GetKeyIDFromOutput(txdb, vtx[1].vout[1]);
    if (!keyID) {
        return NLog.error("{}: failed to find key for coinstake", __func__);
    }
    CKey key;
    if (!wallet.GetKey(*keyID, key)) {
        return NLog.error("{}: failed to get key from keystore", __func__);
    }

    // append a signature to our block
    return key.Sign(GetHash(), vchBlockSig);
}

bool CBlock::SignBlockWithSpecificKey(const ITxDB& txdb, const COutPoint& outputToStake,
                                      const CKey& keyOfOutput, int64_t nFees)
{
    // if we are trying to sign
    //    something except proof-of-stake block template
    if (!vtx[0].vout[0].IsEmpty())
        return false;

    // if we are trying to sign
    //    a complete proof-of-stake block
    if (IsProofOfStake())
        return true;

    const boost::optional<CBlockIndex>  pindexBestPtr = txdb.GetBestBlockIndex();
    const boost::optional<CTransaction> coinStake =
        stakeMaker.CreateCoinStakeFromSpecificOutput(outputToStake, keyOfOutput, nBits, nFees);

    if (!coinStake) {
        return false;
    }

    const int64_t minTime =
        std::max(pindexBestPtr->GetPastTimeLimit(txdb) + 1, PastDrift(pindexBestPtr->GetBlockTime()));

    if (coinStake->nTime < minTime) {
        return false;
    }

    // make sure coinstake would meet timestamp protocol
    // as it would be the same as the block timestamp
    vtx[0].nTime = nTime = coinStake->nTime;
    nTime                = std::max(pindexBestPtr->GetPastTimeLimit(txdb) + 1, GetMaxTransactionTime());
    nTime                = std::max(GetBlockTime(), PastDrift(pindexBestPtr->GetBlockTime()));

    // we have to make sure that we have no future timestamps in our transactions set
    for (auto it = vtx.begin(); it != vtx.end();) {
        if (it->nTime > nTime) {
            it = vtx.erase(it);
        } else {
            ++it;
        }
    }

    // tx[1] is the coinstake transaction
    vtx.insert(vtx.begin() + 1, *coinStake);
    hashMerkleRoot = GetMerkleRoot();

    // append a signature to our block
    return keyOfOutput.Sign(GetHash(), vchBlockSig);
}

static Result<CKey, CBlock::ColdStakeKeyExtractionError> ExtractColdStakePubKey(const CBlock& block)
{
    CKey         key;
    const CTxIn& coinstakeKernel = block.vtx[1].vin[0];
    int          start           = 1 + (int)*coinstakeKernel.scriptSig.begin(); // skip sig
    start += 1 + (int)*(coinstakeKernel.scriptSig.begin() + start);             // skip flag
    const auto beg = coinstakeKernel.scriptSig.begin() + start + 1;
    const auto end = coinstakeKernel.scriptSig.end();
    if (beg > end) {
        return Err(CBlock::ColdStakeKeyExtractionError::KeySizeInvalid);
    }
    const CPubKey pubkey(std::vector<unsigned char>(beg, end));
    key.SetPubKey(pubkey);
    return Ok(key);
}

Result<bool, CBlock::BlockColdStakingCheckError> CBlock::HasColdStaking(const ITxDB& txdb) const
{
    if (IsProofOfWork())
        return Ok(false);

    // check the signature type of the staking reward
    const CTxOut& txout = vtx[1].vout[1];

    std::vector<valtype> vSolutions;
    txnouttype           whichType;
    if (!Solver(txdb, txout.scriptPubKey, whichType, vSolutions))
        return Err(BlockColdStakingCheckError::SolverOnStakeTransactionFailed);

    if (whichType == TX_COLDSTAKE) {
        return Ok(true);
    }

    // Check transactions
    for (unsigned i = 0; i < vtx.size(); i++) {
        const CTransaction& tx = vtx[i];
        if (tx.HasP2CSOutputs()) {
            return Ok(true);
        }
    }

    return Ok(false);
}

bool CBlock::CheckBlockSignature(const ITxDB& txdb, const uint256& blockHash) const
{
    if (IsProofOfWork())
        return vchBlockSig.empty();

    std::vector<valtype> vSolutions;
    txnouttype           whichType;

    const CTxOut& txout = vtx[1].vout[1];

    if (!Solver(txdb, txout.scriptPubKey, whichType, vSolutions))
        return NLog.error("CheckBlockSignature(): Failed to solve for scriptPubKey type");

    CKey key;
    if (whichType == TX_PUBKEY) {
        const valtype& vchPubKey = vSolutions[0];
        if (!key.SetPubKey(vchPubKey))
            return false;
        if (vchBlockSig.empty())
            return false;
        return key.Verify(blockHash, vchBlockSig);
    } else if (whichType == TX_COLDSTAKE) {
        auto keyResult = ExtractColdStakePubKey(*this);
        if (keyResult.isErr()) {
            return NLog.error("CheckBlockSignature(): ColdStaking key extraction failed");
        }
        key = keyResult.unwrap();
        return key.Verify(blockHash, vchBlockSig);
    }

    const std::string sigTypeStr = [&]() {
        if (whichType == TX_PUBKEY) {
            return "PubKey";
        } else if (whichType == TX_COLDSTAKE) {
            return "ColdStake";
        } else {
            return "Unrecognized";
        }
    }();

    return NLog.error("CheckBlockSignature(): Failed to verify block signature of type {}", sigTypeStr);
}

bool CBlock::WriteBlockPubKeys(CTxDB& txdb)
{
    bool success = true;
    for (const CTransaction& tx : vtx) {
        for (unsigned i = 0; i < tx.vin.size(); i++) {
            try {
                if (tx.IsCoinBase()) {
                    // coinbase transactions don't require signatures
                    continue;
                }
                const CTxIn&               in = tx.vin[i];
                opcodetype                 opt;
                auto                       beg = in.scriptSig.cbegin();
                std::vector<unsigned char> vchSig, vchPub;
                if (!in.scriptSig.GetOp(beg, opt, vchSig)) {
                    continue;
                }
                if (!in.scriptSig.GetOp(beg, opt, vchPub)) {
                    continue;
                }
                if (!IsCanonicalSignature(vchSig)) {
                    continue;
                }
                if (!IsCanonicalPubKey(vchPub)) {
                    continue;
                }
                CKey key;
                if (!key.SetPubKey(vchPub)) {
                    NLog.write(b_sev::err, "Failed to CKey::SetPubKey() for input number {} of tx {}", i,
                               tx.GetHash().ToString());
                    success = false;
                    continue;
                }
                CBitcoinAddress addr(key.GetPubKey().GetID());

                std::vector<uint8_t> storedPubkey;

                bool readSuccess = txdb.ReadAddressPubKey(addr, storedPubkey);
                if (!readSuccess) {
                    success = success && txdb.WriteAddressPubKey(addr, key.GetPubKey().Raw());
                }
            } catch (std::exception& ex) {
                NLog.write(
                    b_sev::err,
                    "While writing the public key of input {} of tx {}, an exception was thrown: {}", i,
                    tx.GetHash().ToString(), ex.what());
                success = false;
            } catch (...) {
                NLog.write(
                    b_sev::err,
                    "While writing the public key of input {} of tx {}, an unknown exception was thrown",
                    i, tx.GetHash().ToString());
                success = false;
            }
        }
    }
    return success;
}

void UpdateWallets(const uint256& prevBestChain, const ITxDB& txdb)
{
    using BlockIndexCacheType = BlockIndexLRUCache<bool>;

    static thread_local typename BlockIndexCacheType::ExtractorFunc extractorFunc =
        [](const CBlockIndex&) -> int64_t { return false; };

    static thread_local BlockIndexCacheType blockIndexCache(1000, extractorFunc);

    {
        /**
         * Syncing wallets requires that the current state of best block be correct.
         * Because of this, we have to call SyncWithWallets() only after updating the global variables
         * of the blockchain state (bestChain of BestChainState).
         * Given that a reorg can occur, the call to SyncWithWallets() should happen only after all kinds
         * of reorgs happen (including ConnectBlock). Therefore, we do it at the very end. Here.
         */
        if (txdb.GetBestChainHeight().value_or(0) > 0) {
            // get the highest block in the previous check that's main chain
            boost::optional<CBlockIndex> ancestorOfPrevInMainChain = txdb.ReadBlockIndex(prevBestChain);
            assert(ancestorOfPrevInMainChain);
            while (ancestorOfPrevInMainChain->hashPrev != 0 &&
                   !ancestorOfPrevInMainChain->IsInMainChain(txdb)) {
                // we can't cache this because IsInMainChain() call can change over time
                ancestorOfPrevInMainChain = ancestorOfPrevInMainChain->getPrev(txdb);
            }

            std::vector<uint256> mainChain;

            // get the common ancestor between current chain and previous chain
            uint256 mainChainCurrentHash = txdb.GetBestBlockHash();
            while (mainChainCurrentHash != 0 &&
                   mainChainCurrentHash != ancestorOfPrevInMainChain->GetBlockHash()) {
                mainChain.push_back(mainChainCurrentHash);
                boost::optional<BlockIndexCacheType::BICacheEntry> prev =
                    blockIndexCache.get(txdb, mainChain.back());
                if (!prev || prev->prevHash == ancestorOfPrevInMainChain->GetBlockHash()) {
                    // we exclude the last block that matches the common ancestor since it's already in
                    // the wallet
                    break;
                }
                mainChainCurrentHash = prev->prevHash;
            }

            // loop over all blocks from the common ancestor, to now, and sync these txs
            for (const uint256& h : boost::adaptors::reverse(mainChain)) {
                CBlock block;
                if (!block.ReadFromDisk(h, txdb)) {
                    NLog.write(b_sev::err,
                               "SetBestChain() : CRITICAL ReadFromDisk failed + couldn't sync with "
                               "wallet (block hash: {})",
                               h.ToString());
                    continue;
                }

                // Watch for transactions paying to me
                for (CTransaction& tx : block.vtx) {
                    SyncWithWallets(txdb, tx, &block);
                }
            }
        } else {
            // this is for genesis
            // Watch for transactions paying to me
            const CBlock genesis = Params().GenesisBlock();
            for (const CTransaction& tx : genesis.vtx) {
                SyncWithWallets(txdb, tx, &genesis);
            }
        }
    }
}

bool CBlock::WriteToDisk(const boost::optional<CBlockIndex>& prevBlockIndex, const uint256& hashProof,
                         const uint256& blockHash)
{
    /**
     * @brief txdb
     * This function writes a whole block in an ACID transaction
     */

    CTxDB txdb;

    // before adding the new block, we keep in mind what the current best block is
    const uint256 prevBestChain = txdb.GetBestBlockHash();

    std::size_t req_size = 1000 * ::GetSerializeSize(*this, SER_DISK, CLIENT_VERSION);
    if (!txdb.TxnBegin(req_size)) {
        NLog.write(b_sev::err, "Failed to start transaction for writing a new block.");
        return false;
    }

    bool success = false;

    // this is an RAII hack to guarantee that the function will commit/abort the transaction on exit
    auto txReverseFunctor = [&txdb, &success](bool*) {
        if (success) {
            txdb.TxnCommit();
        } else {
            txdb.TxnAbort();
        }
    };
    std::unique_ptr<bool, decltype(txReverseFunctor)> txEnder(&success, txReverseFunctor);

    if (!txdb.WriteBlock(blockHash, *this)) {
        return NLog.error("WriteBlock failed");
    }

    const boost::optional<CBlockIndex> pindexNew =
        AddToBlockIndex(blockHash, prevBlockIndex, hashProof, txdb, false);

    // database transactions are disabled in there because we already have a transaction around here
    if (!pindexNew) {
        return NLog.error("AddToBlockIndex failed");
    }

    if (!WriteBlockPubKeys(txdb)) {
        NLog.write(b_sev::err,
                   "Failed to write address vs public key values for some transactions in the block {}",
                   blockHash.ToString());
    }

    uiInterface.NotifyBlockTip(IsInitialBlockDownload(txdb), *pindexNew);

    success = true;
    txEnder.reset();

    // after having (potentially) updated the best block, we sync with wallets
    UpdateWallets(prevBestChain, txdb);

    return true;
}

bool CBlock::ReadFromDisk(const uint256& hash, const ITxDB& txdb, bool fReadTransactions)
{
    SetNull();
    return txdb.ReadBlock(hash, *this, fReadTransactions);
}

const char* CBlock::VIUErrorToString(VIUError err)
{
    switch (err) {
    case VIUError::UnknownErrorWhileCollectingTxs:
        return "UnknownErrorWhileCollectingTxs";
    case VIUError::TxInputIndexOutOfRange_Case1:
        return "TxInputIndexOutOfRange_Case1";
    case VIUError::TxInputIndexOutOfRange_Case2:
        return "TxInputIndexOutOfRange_Case2";
    case VIUError::TxInputIndexOutOfRange_Case3:
        return "TxInputIndexOutOfRange_Case3";
    case VIUError::TxInputIndexOutOfRange_Case4:
        return "TxInputIndexOutOfRange_Case4";
    case VIUError::DoublespendAttempt_Case1:
        return "DoublespendAttempt_Case1";
    case VIUError::DoublespendAttempt_Case2:
        return "DoublespendAttempt_Case2";
    case VIUError::SpendingNonexistentTx:
        return "SpendingNonexistentTx";
    case VIUError::BlockCannotBeReadFromDB:
        return "BlockCannotBeReadFromDB";
    case VIUError::TxNonExistent_ReadTxIndexFailed_Case1:
        return "TxNonExistent_ReadTxIndexFailed_Case1";
    case VIUError::TxNonExistent_ReadTxIndexFailed_Case2:
        return "TxNonExistent_ReadTxIndexFailed_Case2";
    case VIUError::ReadBlockIndexFailed:
        return "ReadBlockIndexFailed";
    case VIUError::BlockIsNotInMainChainEvenThoughItShould:
        return "BlockIsNotInMainChainEvenThoughItShould";
    case VIUError::BlockIndexOfPrevBlockNotFound:
        return "BlockIndexOfPrevBlockNotFound";
    case VIUError::CommonAncestorSearchFailed:
        return "CommonAncestorSearchFailed";
    }
    return "Unknown";
}
