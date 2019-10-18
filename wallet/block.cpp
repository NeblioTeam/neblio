#include "block.h"

#include "NetworkForks.h"
#include "blockindex.h"
#include "checkpoints.h"
#include "kernel.h"
#include "main.h"
#include "txmempool.h"
#include "util.h"
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>

void CBlock::print() const
{
    printf("CBlock(hash=%s, ver=%d, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, "
           "nNonce=%u, vtx=%" PRIszu ", vchBlockSig=%s)\n",
           GetHash().ToString().c_str(), nVersion, hashPrevBlock.ToString().c_str(),
           hashMerkleRoot.ToString().c_str(), nTime, nBits, nNonce, vtx.size(),
           HexStr(vchBlockSig.begin(), vchBlockSig.end()).c_str());
    for (unsigned int i = 0; i < vtx.size(); i++) {
        printf("  ");
        vtx[i].print();
    }
    printf("  vMerkleTree: ");
    for (unsigned int i = 0; i < vMerkleTree.size(); i++)
        printf("%s ", vMerkleTree[i].ToString().substr(0, 10).c_str());
    printf("\n");
}

uint256 CBlock::CheckMerkleBranch(uint256 hash, const std::vector<uint256>& vMerkleBranch, int nIndex)
{
    if (nIndex == -1)
        return 0;
    BOOST_FOREACH (const uint256& otherside, vMerkleBranch) {
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
    if (vMerkleTree.empty())
        BuildMerkleTree();
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

uint256 CBlock::BuildMerkleTree() const
{
    vMerkleTree.clear();
    BOOST_FOREACH (const CTransaction& tx, vtx)
        vMerkleTree.push_back(tx.GetHash());
    int j = 0;
    for (int nSize = vtx.size(); nSize > 1; nSize = (nSize + 1) / 2) {
        for (int i = 0; i < nSize; i += 2) {
            int i2 = std::min(i + 1, nSize - 1);
            vMerkleTree.push_back(Hash(BEGIN(vMerkleTree[j + i]), END(vMerkleTree[j + i]),
                                       BEGIN(vMerkleTree[j + i2]), END(vMerkleTree[j + i2])));
        }
        j += nSize;
    }
    return (vMerkleTree.empty() ? 0 : vMerkleTree.back());
}

int64_t CBlock::GetMaxTransactionTime() const
{
    int64_t maxTransactionTime = 0;
    BOOST_FOREACH (const CTransaction& tx, vtx)
        maxTransactionTime = std::max(maxTransactionTime, (int64_t)tx.nTime);
    return maxTransactionTime;
}

std::pair<COutPoint, unsigned int> CBlock::GetProofOfStake() const
{
    return IsProofOfStake() ? std::make_pair(vtx[1].vin[0].prevout, vtx[1].nTime)
                            : std::make_pair(COutPoint(), (unsigned int)0);
}

unsigned int CBlock::GetStakeEntropyBit() const
{
    // Take last bit of block hash as entropy bit
    unsigned int nEntropyBit = ((GetHash().Get64()) & 1llu);
    if (fDebug)
        printf("GetStakeEntropyBit: hashBlock=%s nEntropyBit=%u\n", GetHash().ToString().c_str(),
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
    vMerkleTree.clear();
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

bool CBlock::DisconnectBlock(CTxDB& txdb, CBlockIndexSmartPtr& pindex)
{
    // Disconnect in reverse order
    for (int i = vtx.size() - 1; i >= 0; i--)
        if (!vtx[i].DisconnectInputs(txdb))
            return false;

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->pprev) {
        CDiskBlockIndex blockindexPrev(boost::atomic_load(&pindex->pprev).get());
        blockindexPrev.hashNext = 0;
        if (!txdb.WriteBlockIndex(blockindexPrev))
            return error("DisconnectBlock() : WriteBlockIndex failed");
    }

    // ppcoin: clean up wallet after disconnecting coinstake
    for (CTransaction& tx : vtx)
        SyncWithWallets(tx, this, false, false);

    return true;
}

/// returns all the blocks from the tip of the main chain up to the common ancestor (without the common
/// ancestor)
CBlock::CommonAncestorSuccessorBlocks CBlock::GetBlocksUpToCommonAncestorInMainChain() const
{
    CommonAncestorSuccessorBlocks res;

    // we expect both the main chain and the fork's previous block to be in the block index

    // main chain part
    CBlockIndexSmartPtr I = boost::atomic_load(&pindexBest);

    // fork part
    CBlockIndexSmartPtr T                  = nullptr;
    const uint256       commonAncestorHash = this->hashPrevBlock;
    auto                itTarget           = mapBlockIndex.find(commonAncestorHash);

    if (itTarget != mapBlockIndex.end()) {
        T = boost::atomic_load(&itTarget->second);
        // keep stepping back from the orphan (new block) until we find the main chain
        while (!T->IsInMainChain()) {
            // this map will be empty if the fork from main chain has only this block
            res.inFork.push_back(T->GetBlockHash());
            //            std::cout << "Block in fork chain: " << T->GetBlockHash().ToString() << "\t" <<
            //            T->nHeight
            //                      << std::endl;
            T = boost::atomic_load(&T->pprev);
        }
        //        std::cout << "Traversing " << itBest->second->nHeight - itTarget->second->nHeight << "
        //        blocks" << std::endl;
    } else {
        throw std::runtime_error("Failed to find target block " + commonAncestorHash.ToString() +
                                 "in block index in " + std::string(__PRETTY_FUNCTION__));
    }

    // the fork should be in temporal order because it's to be spent in order later
    std::reverse(res.inFork.begin(), res.inFork.end());

    assert(I != nullptr);
    assert(T != nullptr);
    // step back until we meet the main chain point in the fork
    while (I->GetBlockHash() != T->GetBlockHash() && I->GetBlockHash() != hashGenesisBlock &&
           I->GetBlockHash() != hashGenesisBlockTestNet) {
        res.inMainChain.insert(I->GetBlockHash());
        //        std::cout << "Block in main chain: " << I->GetBlockHash().ToString() << "\t" <<
        //        I->nHeight << std::endl;
        I = boost::atomic_load(&I->pprev);
    }
    //    std::cout << "End block hashes" << std::endl << std::endl;
    return res;
}

/**
 * @brief CBlock::GetAlternateChainTxsUpToCommonAncestor
 * @return
 * This function will get all the blocks from the main chain up to the common ancestor with this block;
 * then, it'll unspend all the transactions in these blocks and return them in the map. This is necessary
 * to solve the problem of stake attack described in VerifyInputsUnspent()
 */
CBlock::ChainReplaceTxs CBlock::GetAlternateChainTxsUpToCommonAncestor(CTxDB& txdb) const
{
    ChainReplaceTxs result;

    CommonAncestorSuccessorBlocks commonAncestory = GetBlocksUpToCommonAncestorInMainChain();
    std::vector<CTransaction>     mainChainBlocksTxs; // to be disconnected
    std::vector<CTransaction>     forkChainBlocksTxs; // to be reconnected

    // get all txs in blocks up to the common ancestor in the main chain
    for (const uint256& bh : commonAncestory.inMainChain) {
        CBlock blk;
        if (!txdb.ReadBlock(bh, blk, true)) {
            throw std::runtime_error("In main chain search, block " + bh.ToString() +
                                     " was not found in the database");
        }
        auto getHashFunc = [](const CTransaction& tx) { return tx.GetHash(); };
        std::transform(blk.vtx.cbegin(), blk.vtx.cend(),
                       std::inserter(result.disconnectedRootTxs, result.disconnectedRootTxs.begin()),
                       getHashFunc);
        std::move(blk.vtx.begin(), blk.vtx.end(), std::back_inserter(mainChainBlocksTxs));
    }

    // get all txs in the fork leading to this transaction (in order to test spending them later)
    for (const uint256& bh : commonAncestory.inFork) {
        CBlock blk;
        //        std::cout << "In fork block hash: " << bh.ToString() << std::endl;
        if (!txdb.ReadBlock(bh, blk, true)) {
            throw std::runtime_error("In fork chain search, block " + bh.ToString() +
                                     " was not found in the database");
        }
        //        for (const CTransaction& t : blk.vtx) {
        //            std::cout << "In fork tx hash: " << t.GetHash().ToString() << std::endl;
        //        }
        std::copy(blk.vtx.cbegin(), blk.vtx.cend(), std::back_inserter(forkChainBlocksTxs));
    }

    // unspend transactions that go from the main-chain up to the common ancessor (excluding the common
    // ancestor)
    //    std::cout << "Start disconnected txs" << std::endl;
    for (const CTransaction& tx : mainChainBlocksTxs) {
        //        std::cout << "Disconnecting inputs of: " << tx.GetHash().ToString() << std::endl;
        const std::vector<CTxIn>& vin = tx.vin;
        for (unsigned int inIdx = 0; inIdx < vin.size(); inIdx++) {
            const CTxIn& txin = vin[inIdx];
            if (tx.IsCoinBase()) {
                continue;
            }
            uint256  outputTxHash  = txin.prevout.hash;
            unsigned outputNumInTx = txin.prevout.n;

            auto     idxIt = result.modifiedOutputsTxs.find(outputTxHash);
            CTxIndex txindex;
            if (idxIt == result.modifiedOutputsTxs.cend()) {
                // Get prev txindex from disk
                if (!txdb.ReadTxIndex(outputTxHash, txindex)) {
                    throw std::runtime_error(std::string(__PRETTY_FUNCTION__) +
                                             ": ReadTxIndex failed for transaction " +
                                             outputTxHash.ToString());
                }
            } else {
                // Get txindex from the map
                txindex = idxIt->second;
            }

            // check range
            if (outputNumInTx >= txindex.vSpent.size())
                throw std::runtime_error(
                    std::string(__PRETTY_FUNCTION__) + ": prevout.n out of range for transaction " +
                    outputTxHash.ToString() + " and output " + std::to_string(outputNumInTx));

            // unspend, which is equivalent to disconnecting the blockchain (without updating the
            // database)
            txindex.vSpent[outputNumInTx].SetNull();

            // this way, all main-chain spent transactions are in this map, while marked unspent
            result.modifiedOutputsTxs[outputTxHash] = txindex;
        }
    }

    //    std::cout << "End disconnected txs" << std::endl;

    //    std::cout << "Start reconnecting txs" << std::endl;
    // respend the transactions on the fork (to test whether this block is valid at this chain)
    for (const CTransaction& tx : forkChainBlocksTxs) {
        //        std::cout << "Reconnecting inputs in: " << tx.GetHash().ToString() << std::endl;
        const std::vector<CTxIn>& vin = tx.vin;
        for (unsigned int inIdx = 0; inIdx < vin.size(); inIdx++) {
            const CTxIn& txin = vin[inIdx];
            if (tx.IsCoinBase()) {
                continue;
            }
            uint256  outputTxHash  = txin.prevout.hash;
            unsigned outputNumInTx = txin.prevout.n;

            auto     idxIt = result.modifiedOutputsTxs.find(outputTxHash);
            CTxIndex txindex;
            if (idxIt == result.modifiedOutputsTxs.cend()) {
                // Get prev txindex from disk
                if (!txdb.ReadTxIndex(outputTxHash, txindex)) {
                    throw std::runtime_error(std::string(__PRETTY_FUNCTION__) +
                                             ": ReadTxIndex failed for transaction " +
                                             outputTxHash.ToString());
                }
            } else {
                txindex = idxIt->second;
            }

            // check range
            if (outputNumInTx >= txindex.vSpent.size()) {
                throw std::runtime_error(
                    std::string(__PRETTY_FUNCTION__) + ": prevout.n out of range for transaction " +
                    outputTxHash.ToString() + " and output " + std::to_string(outputNumInTx));
            }

            // spend the output (without updating the database)
            txindex.vSpent[outputNumInTx] = CreateFakeSpentTxPos(txindex.pos.nBlockPos);

            result.modifiedOutputsTxs[outputTxHash] = txindex;
        }
        // since we're respending the transactions, we should store them in modified list because they
        // can be spent in subsequent blocks
        auto idxIt = result.modifiedOutputsTxs.find(tx.GetHash());
        if (idxIt == result.modifiedOutputsTxs.cend()) {
            result.modifiedOutputsTxs[tx.GetHash()] =
                CTxIndex(CDiskTxPos(this->GetHash(), 3), tx.vout.size());
        }
    }
    //    std::cout << "End reconnecting txs" << std::endl;
    return result;
}

bool CBlock::VerifyInputsUnspent(CTxDB& txdb) const
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
    ChainReplaceTxs alternateChainTxs;

    try {
        alternateChainTxs = GetAlternateChainTxsUpToCommonAncestor(txdb);
    } catch (std::exception& ex) {
        return error("Failed to verify unspent inputs for block %s; error: %s",
                     this->GetHash().ToString().c_str(), ex.what());
    }

    std::unordered_map<uint256, CTxIndex>& queuedTxs = alternateChainTxs.modifiedOutputsTxs;
    const std::unordered_set<uint256>&     disconnectedTxsFromMainChain =
        alternateChainTxs.disconnectedRootTxs;

    for (const CTransaction& tx : vtx) {
        //        std::cout << "Working on tx: " << tx.GetHash().ToString() << std::endl;
        {
            // since this function checks whether all transactions are spent, this should exclude BIP30
            // cases, where a duplicate transaction is being rewritten
            CTxIndex  txindexOld;
            uint256&& txHash = tx.GetHash();
            // if the transaction already exists in the blockchain and is not part of the reversed
            // transactions
            bool foundInForkedBlocks =
                disconnectedTxsFromMainChain.find(txHash) != disconnectedTxsFromMainChain.cend();
            if (txdb.ContainsTx(txHash) && txdb.ReadTxIndex(txHash, txindexOld) &&
                !foundInForkedBlocks) {

                printf("Found a transaction %s in a new block while already in the blockchain in block "
                       "%s; this is a BIP30 possible attack\n",
                       txHash.ToString().c_str(), txindexOld.pos.nBlockPos.ToString().c_str());
                continue;
            }
        }

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
                if (it->second.vSpent[outputNumInTx].IsNull()) {
                    // tx is not spent yet, so we mark it as spent
                    it->second.vSpent[outputNumInTx] = CreateFakeSpentTxPos(this->GetHash());
                } else {
                    return error("Output number %u in tx %s which is an input to tx %s is attempting to "
                                 "double-spend in the same block %s",
                                 outputNumInTx, outputTxHash.ToString().c_str(),
                                 tx.GetHash().ToString().c_str(), this->GetHash().ToString().c_str());
                }
            } else if (txdb.ContainsTx(outputTxHash) && txdb.ReadTxIndex(outputTxHash, txindex)) {
                queuedTxs[outputTxHash] = txindex;
                if (txindex.vSpent[outputNumInTx].IsNull()) {
                    queuedTxs.find(outputTxHash)->second.vSpent[outputNumInTx] =
                        CreateFakeSpentTxPos(this->GetHash());
                } else {
                    return error("Output number %u in tx %s which is an input to tx %s and is being "
                                 "attempted to spend it in block %s, this is a "
                                 "double-spend attempt",
                                 outputNumInTx, outputTxHash.ToString().c_str(),
                                 tx.GetHash().ToString().c_str(), this->GetHash().ToString().c_str());
                }
            } else {
                return error("Output number %u in tx %s which is an input to tx %s and is being "
                             "attempted to spend it in block %s. it's an invalid tx",
                             outputNumInTx, outputTxHash.ToString().c_str(),
                             tx.GetHash().ToString().c_str(),
                             vin[inIdx].prevout.hash.ToString().c_str());
            }
        }
    }
    return true;
}

bool CBlock::CheckBIP30Attack(CTxDB& txdb, const uint256& hashTx)
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
    if (txdb.ContainsTx(hashTx) && txdb.ReadTxIndex(hashTx, txindexOld)) {
        for (CDiskTxPos& pos : txindexOld.vSpent)
            if (pos.IsNull())
                return false;
    }
    return true;
}

bool CBlock::ConnectBlock(CTxDB& txdb, const CBlockIndexSmartPtr& pindex, bool fJustCheck)
{
    // Check it again in case a previous version let a bad block in, but skip BlockSig checking
    if (!CheckBlock(!fJustCheck, !fJustCheck, false))
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

    for (CTransaction& tx : vtx) {
        uint256 hashTx = tx.GetHash();

        std::vector<std::pair<CTransaction, NTP1Transaction>> inputsWithNTP1;

        if (!CheckBIP30Attack(txdb, hashTx)) {
            return error(
                "Block %s was rejected as it seems that an attempt of BIP30 attack was attempted\n",
                this->GetHash().ToString().c_str());
        }

        nSigOps += tx.GetLegacySigOpCount();
        if (nSigOps > MAX_BLOCK_SIGOPS)
            return DoS(100, error("ConnectBlock() : too many sigops"));

        CDiskTxPos posThisTx(pindex->blockKeyInDB, nTxPos);
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
                return DoS(100, error("ConnectBlock() : too many sigops"));
            }

            int64_t nTxValueIn  = tx.GetValueIn(mapInputs);
            int64_t nTxValueOut = tx.GetValueOut();
            nValueIn += nTxValueIn;
            nValueOut += nTxValueOut;
            if (!tx.IsCoinStake())
                nFees += nTxValueIn - nTxValueOut;
            if (tx.IsCoinStake())
                nStakeReward = nTxValueOut - nTxValueIn;

            if (GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON)) {
                try {
                    if (NTP1Transaction::IsTxNTP1(&tx)) {
                        // check if there are inputs already cached
                        inputsWithNTP1 = NTP1Transaction::GetAllNTP1InputsOfTx(
                            tx, txdb, true, mapQueuedNTP1Inputs, mapQueuedChanges);

                        // write NTP1 transactions' data
                        NTP1Transaction ntp1tx;
                        ntp1tx.readNTP1DataFromTx(tx, inputsWithNTP1);
                    }
                } catch (std::exception& ex) {
                    return error("Error while verifying NTP1Transaction validity in ConnectBlock(): "
                                 "%s\n",
                                 ex.what());
                } catch (...) {
                    return error("Error while verifying NTP1Transaction validity in ConnectBlock(). "
                                 "Unknown exception thrown\n");
                }
            }

            if (EnableEnforceUniqueTokenSymbols()) {
                try {
                    AssertIssuanceUniquenessInBlock(issuedTokensSymbolsInThisBlock, txdb, tx,
                                                    mapQueuedNTP1Inputs, mapQueuedChanges);
                } catch (std::exception& ex) {
                    return error("Error while verifying the uniqueness of issued token symbol in "
                                 "ConnectBlock(): "
                                 "%s\n",
                                 ex.what());
                } catch (...) {
                    return error("Error while verifying the uniqueness of issued token symbol in "
                                 "ConnectBlock(). "
                                 "Unknown exception thrown\n");
                }
            }

            if (!tx.ConnectInputs(txdb, mapInputs, mapQueuedChanges, posThisTx, pindex, true, false)) {
                return false;
            }
        }

        mapQueuedChanges[hashTx]          = CTxIndex(posThisTx, tx.vout.size());
        mapQueuedNTP1Inputs[tx.GetHash()] = inputsWithNTP1;
    }

    if (IsProofOfWork()) {
        int64_t nReward = GetProofOfWorkReward(nFees);
        // Check coinbase reward
        if (vtx[0].GetValueOut() > nReward)
            return DoS(50, error("ConnectBlock() : coinbase reward exceeded (actual=%" PRId64
                                 " vs calculated=%" PRId64 ")",
                                 vtx[0].GetValueOut(), nReward));
    }
    if (IsProofOfStake()) {
        // ppcoin: coin stake tx earns reward instead of paying fee
        uint64_t nCoinAge;
        if (!vtx[1].GetCoinAge(txdb, nCoinAge))
            return error("ConnectBlock() : %s unable to get coin age for coinstake",
                         vtx[1].GetHash().ToString().c_str());

        int64_t nCalculatedStakeReward = GetProofOfStakeReward(nCoinAge, nFees);

        if (nStakeReward > nCalculatedStakeReward)
            return DoS(100, error("ConnectBlock() : coinstake pays too much(actual=%" PRId64
                                  " vs calculated=%" PRId64 ")",
                                  nStakeReward, nCalculatedStakeReward));
    }

    // ppcoin: track money supply and mint amount info
    pindex->nMint        = nValueOut - nValueIn + nFees;
    pindex->nMoneySupply = (pindex->pprev ? pindex->pprev->nMoneySupply : 0) + nValueOut - nValueIn;
    if (!txdb.WriteBlockIndex(CDiskBlockIndex(pindex.get())))
        return error("Connect() : WriteBlockIndex for pindex failed");

    if (fJustCheck)
        return true;

    // Write queued txindex changes
    for (std::map<uint256, CTxIndex>::iterator mi = mapQueuedChanges.begin();
         mi != mapQueuedChanges.end(); ++mi) {
        if (!txdb.UpdateTxIndex((*mi).first, (*mi).second))
            return error("ConnectBlock() : UpdateTxIndex failed");
    }

    // This scope does NTP1 data writing
    {
        try {
            WriteNTP1BlockTransactionsToDisk(vtx, txdb);
        } catch (std::exception& ex) {
            if (GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON)) {
                return error("Unable to get NTP1 transaction written in ConnectBlock(). Error: %s\n",
                             ex.what());
            }
        } catch (...) {
            if (GetNetForks().isForkActivated(NetworkFork::NETFORK__3_TACHYON)) {
                return error("Unable to get NTP1 transaction written in ConnectBlock(). An unknown "
                             "exception was "
                             "thrown");
            }
        }
    }

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->pprev) {
        CDiskBlockIndex blockindexPrev(boost::atomic_load(&pindex->pprev).get());
        blockindexPrev.hashNext = pindex->GetBlockHash();
        if (!txdb.WriteBlockIndex(blockindexPrev))
            return error("ConnectBlock() : WriteBlockIndex failed");
    }

    // Watch for transactions paying to me
    for (CTransaction& tx : vtx)
        SyncWithWallets(tx, this, true);

    return true;
}

// Called from inside SetBestChain: attaches a block to the new best chain being built
bool CBlock::SetBestChainInner(CTxDB& txdb, const CBlockIndexSmartPtr& pindexNew,
                               const bool createDbTransaction)
{
    uint256 hash = GetHash();

    // Adding to current best branch
    if (!ConnectBlock(txdb, pindexNew) || !txdb.WriteHashBestChain(hash)) {
        if (createDbTransaction) {
            txdb.TxnAbort();
        }
        InvalidChainFound(pindexNew, txdb);
        return false;
    }
    if (createDbTransaction && !txdb.TxnCommit())
        return error("SetBestChain() : TxnCommit failed");

    // Add to current best branch
    pindexNew->pprev->pnext = pindexNew;

    // Delete redundant memory transactions
    for (CTransaction& tx : vtx)
        mempool.remove(tx);

    return true;
}

bool CBlock::SetBestChain(CTxDB& txdb, const CBlockIndexSmartPtr& pindexNew,
                          const bool createDbTransaction)
{
    uint256 hash = GetHash();

    if (createDbTransaction && !txdb.TxnBegin())
        return error("SetBestChain() : TxnBegin failed");

    if (pindexGenesisBlock == NULL && hash == (!fTestNet ? hashGenesisBlock : hashGenesisBlockTestNet)) {
        txdb.WriteHashBestChain(hash);
        if (createDbTransaction && !txdb.TxnCommit())
            return error("SetBestChain() : TxnCommit failed");
        pindexGenesisBlock = pindexNew;
    } else if (hashPrevBlock == hashBestChain) {
        if (!SetBestChainInner(txdb, pindexNew, createDbTransaction))
            return error("SetBestChain() : SetBestChainInner failed");
    } else {
        // the first block in the new chain that will cause it to become the new best chain
        CBlockIndexSmartPtr pindexIntermediate = pindexNew;

        // list of blocks that need to be connected afterwards
        std::vector<CBlockIndexSmartPtr> vpindexSecondary;

        // Reorganize is costly in terms of db load, as it works in a single db transaction.
        // Try to limit how much needs to be done inside
        while (pindexIntermediate->pprev &&
               pindexIntermediate->pprev->nChainTrust > boost::atomic_load(&pindexBest)->nChainTrust) {
            vpindexSecondary.push_back(pindexIntermediate);
            pindexIntermediate = pindexIntermediate->pprev;
        }

        if (!vpindexSecondary.empty())
            printf("Postponing %" PRIszu " reconnects\n", vpindexSecondary.size());

        // Switch to new best branch
        if (!Reorganize(txdb, pindexIntermediate, createDbTransaction)) {
            if (createDbTransaction) {
                txdb.TxnAbort();
            }
            InvalidChainFound(pindexNew, txdb);
            return error("SetBestChain() : Reorganize failed");
        }

        // Connect further blocks
        BOOST_REVERSE_FOREACH(CBlockIndexSmartPtr pindex, vpindexSecondary)
        {
            CBlock block;
            if (!block.ReadFromDisk(pindex.get(), txdb)) {
                printf("SetBestChain() : ReadFromDisk failed\n");
                break;
            }
            if (createDbTransaction && !txdb.TxnBegin()) {
                printf("SetBestChain() : TxnBegin 2 failed\n");
                break;
            }
            // errors now are not fatal, we still did a reorganisation to a new chain in a valid way
            if (!block.SetBestChainInner(txdb, pindex, createDbTransaction))
                break;
        }
    }

    // Update best block in wallet (so we can detect restored wallets)
    bool fIsInitialDownload = IsInitialBlockDownload();
    if (!fIsInitialDownload) {
        const CBlockLocator locator(pindexNew.get());
        ::SetBestChain(locator);
    }

    // New best block
    hashBestChain = hash;
    boost::atomic_store(&pindexBest, pindexNew);
    CBlockIndexSmartPtr pindexBestPtr = boost::atomic_load(&pindexBest);
    pblockindexFBBHLast               = nullptr;
    nBestHeight                       = pindexBestPtr->nHeight;
    nBestChainTrust                   = pindexNew->nChainTrust;
    nTimeBestReceived                 = GetTime();
    nTransactionsUpdated++;

    uint256 nBestBlockTrust = pindexBestPtr->nHeight != 0
                                  ? (pindexBestPtr->nChainTrust - pindexBestPtr->pprev->nChainTrust)
                                  : pindexBestPtr->nChainTrust;

    printf("SetBestChain: new best=%s  height=%d  trust=%s  blocktrust=%" PRId64 "  date=%s\n",
           hashBestChain.ToString().c_str(), nBestHeight.load(),
           CBigNum(nBestChainTrust).ToString().c_str(), nBestBlockTrust.Get64(),
           DateTimeStrFormat("%x %H:%M:%S", pindexBestPtr->GetBlockTime()).c_str());

    // Check the version of the last 100 blocks to see if we need to upgrade:
    if (!fIsInitialDownload) {
        int                      nUpgraded = 0;
        ConstCBlockIndexSmartPtr pindex    = boost::atomic_load(&pindexBest);
        for (int i = 0; i < 100 && pindex != NULL; i++) {
            if (pindex->nVersion > CBlock::CURRENT_VERSION)
                ++nUpgraded;
            pindex = pindex->pprev;
        }
        if (nUpgraded > 0)
            printf("SetBestChain: %d of last 100 blocks above version %d\n", nUpgraded,
                   CBlock::CURRENT_VERSION);
        if (nUpgraded > 100 / 2)
            // strMiscWarning is read by GetWarnings(), called by Qt and the JSON-RPC code to warn the
            // user:
            strMiscWarning = _("Warning: This version is obsolete, upgrade required!");
    }

    std::string strCmd = GetArg("-blocknotify", "");

    if (!fIsInitialDownload && !strCmd.empty()) {
        boost::replace_all(strCmd, "%s", hashBestChain.GetHex());
        boost::thread t(runCommand, strCmd); // thread runs free
    }

    return true;
}

bool CBlock::ReadFromDisk(const CBlockIndex* pindex, bool fReadTransactions)
{
    if (!fReadTransactions) {
        *this = pindex->GetBlockHeader();
        return true;
    }
    if (!ReadFromDisk(pindex->blockKeyInDB, fReadTransactions))
        return false;
    if (GetHash() != pindex->GetBlockHash())
        return error("CBlock::ReadFromDisk() : GetHash() doesn't match index");
    return true;
}

bool CBlock::ReadFromDisk(const CBlockIndex* pindex, CTxDB& txdb, bool fReadTransactions)
{
    if (!fReadTransactions) {
        *this = pindex->GetBlockHeader();
        return true;
    }
    if (!ReadFromDisk(pindex->blockKeyInDB, txdb, fReadTransactions))
        return false;
    if (GetHash() != pindex->GetBlockHash())
        return error("CBlock::ReadFromDisk() : GetHash() doesn't match index");
    return true;
}

bool CBlock::IsProofOfStake() const { return (vtx.size() > 1 && vtx[1].IsCoinStake()); }

CBlockIndexSmartPtr CBlock::FindBlockByHeight(int nHeight)
{
    CBlockIndexSmartPtr pblockindex;
    if (nHeight < nBestHeight / 2) {
        pblockindex = boost::atomic_load(&pindexGenesisBlock);
    } else {
        pblockindex = boost::atomic_load(&pindexBest);
    }
    if (pblockindexFBBHLast &&
        abs(nHeight - pblockindex->nHeight) > abs(nHeight - pblockindexFBBHLast->nHeight)) {
        pblockindex = pblockindexFBBHLast;
    }
    while (pblockindex->nHeight > nHeight) {
        pblockindex = pblockindex->pprev;
    }
    while (pblockindex->nHeight < nHeight) {
        pblockindex = pblockindex->pnext;
    }
    pblockindexFBBHLast = pblockindex;
    return pblockindex;
}

void CBlock::InvalidChainFound(const CBlockIndexSmartPtr& pindexNew, CTxDB& txdb)
{
    if (pindexNew->nChainTrust > nBestInvalidTrust) {
        nBestInvalidTrust = pindexNew->nChainTrust;
        txdb.WriteBestInvalidTrust(CBigNum(nBestInvalidTrust));
        uiInterface.NotifyBlocksChanged();
    }

    uint256 nBestInvalidBlockTrust = pindexNew->nChainTrust - pindexNew->pprev->nChainTrust;

    CBlockIndexSmartPtr pindexBestPtr = boost::atomic_load(&pindexBest);

    uint256 nBestBlockTrust = pindexBestPtr->nHeight != 0
                                  ? (pindexBestPtr->nChainTrust - pindexBestPtr->pprev->nChainTrust)
                                  : pindexBestPtr->nChainTrust;

    printf("InvalidChainFound: invalid block=%s  height=%d  trust=%s  blocktrust=%" PRId64 "  date=%s\n",
           pindexNew->GetBlockHash().ToString().c_str(), pindexNew->nHeight,
           CBigNum(pindexNew->nChainTrust).ToString().c_str(), nBestInvalidBlockTrust.Get64(),
           DateTimeStrFormat("%x %H:%M:%S", pindexNew->GetBlockTime()).c_str());
    printf("InvalidChainFound:  current best=%s  height=%d  trust=%s  blocktrust=%" PRId64 "  date=%s\n",
           hashBestChain.ToString().c_str(), nBestHeight.load(),
           CBigNum(pindexBestPtr->nChainTrust).ToString().c_str(), nBestBlockTrust.Get64(),
           DateTimeStrFormat("%x %H:%M:%S", pindexBestPtr->GetBlockTime()).c_str());
}

bool CBlock::Reorganize(CTxDB& txdb, CBlockIndexSmartPtr& pindexNew, const bool createDbTransaction)
{
    printf("REORGANIZE\n");

    // Find the fork
    CBlockIndexSmartPtr pfork   = boost::atomic_load(&pindexBest);
    CBlockIndexSmartPtr plonger = pindexNew;
    while (pfork != plonger) {
        while (plonger->nHeight > pfork->nHeight)
            if (!(plonger = boost::atomic_load(&plonger->pprev)))
                return error("Reorganize() : plonger->pprev is null");
        if (pfork == plonger)
            break;
        if (!(pfork = pfork->pprev))
            return error("Reorganize() : pfork->pprev is null");
    }

    // List of what to disconnect
    std::vector<CBlockIndexSmartPtr> vDisconnect;
    for (CBlockIndexSmartPtr pindex = boost::atomic_load(&pindexBest); pindex != pfork;
         pindex                     = boost::atomic_load(&pindex->pprev)) {
        vDisconnect.push_back(pindex);
    }

    // List of what to connect
    std::vector<CBlockIndexSmartPtr> vConnect;
    for (CBlockIndexSmartPtr pindex = pindexNew; pindex != pfork;
         pindex                     = boost::atomic_load(&pindex->pprev)) {
        vConnect.push_back(pindex);
    }
    reverse(vConnect.begin(), vConnect.end());

    printf("REORGANIZE: Disconnect %" PRIszu " blocks; %s..%s\n", vDisconnect.size(),
           pfork->GetBlockHash().ToString().c_str(),
           boost::atomic_load(&pindexBest)->GetBlockHash().ToString().c_str());
    printf("REORGANIZE: Connect %" PRIszu " blocks; %s..%s\n", vConnect.size(),
           pfork->GetBlockHash().ToString().c_str(), pindexNew->GetBlockHash().ToString().c_str());

    // Disconnect shorter branch
    std::list<CTransaction> vResurrect;
    for (CBlockIndexSmartPtr& pindex : vDisconnect) {
        CBlock block;
        if (!block.ReadFromDisk(pindex.get()))
            return error("Reorganize() : ReadFromDisk for disconnect failed");
        if (!block.DisconnectBlock(txdb, pindex))
            return error("Reorganize() : DisconnectBlock %s failed",
                         pindex->GetBlockHash().ToString().c_str());

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
        CBlockIndexSmartPtr pindex = vConnect[i];
        CBlock              block;
        if (!block.ReadFromDisk(pindex.get(), txdb))
            return error("Reorganize() : ReadFromDisk for connect failed");
        if (!block.ConnectBlock(txdb, pindex)) {
            // Invalid block
            return error("Reorganize() : ConnectBlock %s failed",
                         pindex->GetBlockHash().ToString().c_str());
        }

        // Queue memory transactions to delete
        for (const CTransaction& tx : block.vtx)
            vDelete.push_back(tx);
    }
    if (!txdb.WriteHashBestChain(pindexNew->GetBlockHash()))
        return error("Reorganize() : WriteHashBestChain failed");

    // Make sure it's successfully written to disk before changing memory structure
    if (createDbTransaction && !txdb.TxnCommit())
        return error("Reorganize() : TxnCommit failed");

    // Disconnect shorter branch
    for (CBlockIndexSmartPtr& pindex : vDisconnect)
        if (pindex->pprev)
            pindex->pprev->pnext = nullptr;

    // Connect longer branch
    for (CBlockIndexSmartPtr& pindex : vConnect)
        if (pindex->pprev)
            pindex->pprev->pnext = pindex;

    // Resurrect memory transactions that were in the disconnected branch
    for (CTransaction& tx : vResurrect)
        AcceptToMemoryPool(mempool, tx, NULL);

    // Delete redundant memory transactions that are in the connected branch
    for (CTransaction& tx : vDelete) {
        mempool.remove(tx);
        mempool.removeConflicts(tx);
    }

    printf("REORGANIZE: done\n");

    return true;
}

// ppcoin: total coin age spent in block, in the unit of coin-days.
bool CBlock::GetCoinAge(uint64_t& nCoinAge) const
{
    nCoinAge = 0;

    CTxDB txdb("r");
    for (const CTransaction& tx : vtx) {
        uint64_t nTxCoinAge;
        if (tx.GetCoinAge(txdb, nTxCoinAge))
            nCoinAge += nTxCoinAge;
        else
            return false;
    }

    if (nCoinAge == 0) // block coin age minimum 1 coin-day
        nCoinAge = 1;
    if (fDebug)
        printf("block coin age total nCoinDays=%" PRId64 "\n", nCoinAge);
    return true;
}

bool CBlock::AddToBlockIndex(uint256 nBlockPos, const uint256& hashProof, CTxDB& txdb,
                             CBlockIndexSmartPtr* newBlockIdxPtr, const bool createDbTransaction)
{
    // Check for duplicate
    uint256 hash = GetHash();
    if (mapBlockIndex.count(hash))
        return error("AddToBlockIndex() : %s already exists", hash.ToString().c_str());

    // Construct new block index object
    CBlockIndexSmartPtr pindexNew = boost::make_shared<CBlockIndex>(nBlockPos, *this);
    if (!pindexNew)
        return error("AddToBlockIndex() : new CBlockIndex failed");
    pindexNew->phashBlock              = &hash;
    BlockIndexMapType::iterator miPrev = mapBlockIndex.find(hashPrevBlock);
    if (miPrev != mapBlockIndex.end()) {
        pindexNew->pprev   = boost::atomic_load(&miPrev->second);
        pindexNew->nHeight = pindexNew->pprev->nHeight + 1;
    }

    // ppcoin: compute chain trust score
    pindexNew->nChainTrust =
        (pindexNew->pprev ? pindexNew->pprev->nChainTrust : 0) + pindexNew->GetBlockTrust();

    // ppcoin: compute stake entropy bit for stake modifier
    if (!pindexNew->SetStakeEntropyBit(GetStakeEntropyBit()))
        return error("AddToBlockIndex() : SetStakeEntropyBit() failed");

    // Record proof hash value
    pindexNew->hashProof = hashProof;

    // ppcoin: compute stake modifier
    uint64_t nStakeModifier          = 0;
    bool     fGeneratedStakeModifier = false;
    if (!ComputeNextStakeModifier(pindexNew->pprev.get(), nStakeModifier, fGeneratedStakeModifier))
        return error("AddToBlockIndex() : ComputeNextStakeModifier() failed");
    pindexNew->SetStakeModifier(nStakeModifier, fGeneratedStakeModifier);
    pindexNew->nStakeModifierChecksum = GetStakeModifierChecksum(pindexNew.get());
    if (!CheckStakeModifierCheckpoints(pindexNew->nHeight, pindexNew->nStakeModifierChecksum))
        return error("AddToBlockIndex() : Rejected by stake modifier checkpoint height=%d, "
                     "modifier=0x%016" PRIx64,
                     pindexNew->nHeight, nStakeModifier);
    // return error("AddToBlockIndex() : Rejected by stake modifier checkpoint height=%d,
    // checksum=0x%016" PRIx64, pindexNew->nHeight, pindexNew->nStakeModifierChecksum);

    // Add to mapBlockIndex
    BlockIndexMapType::iterator mi = mapBlockIndex.insert(std::make_pair(hash, pindexNew)).first;
    if (pindexNew->IsProofOfStake())
        setStakeSeen.insert(std::make_pair(pindexNew->prevoutStake, pindexNew->nStakeTime));
    pindexNew->phashBlock = &((*mi).first);

    // Write to disk block index
    if (createDbTransaction && !txdb.TxnBegin())
        return false;
    txdb.WriteBlockIndex(CDiskBlockIndex(pindexNew.get()));
    if (createDbTransaction && !txdb.TxnCommit())
        return false;

    LOCK(cs_main);

    // New best
    if (pindexNew->nChainTrust > nBestChainTrust)
        if (!SetBestChain(txdb, pindexNew, createDbTransaction))
            return false;

    if (pindexNew == pindexBest) {
        // Notify UI to display prev block's coinbase if it was ours
        static uint256 hashPrevBestCoinBase;
        UpdatedTransaction(hashPrevBestCoinBase);
        hashPrevBestCoinBase = vtx[0].GetHash();
    }

    uiInterface.NotifyBlocksChanged();

    if (newBlockIdxPtr != nullptr) {
        *newBlockIdxPtr = pindexNew;
    }
    // TODO: Sam: this seems to need an "else" case. It doesn't make sense to ignore it

    return true;
}

bool CBlock::CheckBlock(bool fCheckPOW, bool fCheckMerkleRoot, bool fCheckSig) const
{
    // These are checks that are independent of context
    // that can be verified before saving an orphan block.

    // Size limits
    unsigned int nSizeLimit = MaxBlockSize();
    if (vtx.empty() || vtx.size() > nSizeLimit ||
        ::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION) > nSizeLimit)
        return DoS(100, error("CheckBlock() : size limits failed"));

    // Check proof of work matches claimed amount
    if (fCheckPOW && IsProofOfWork() && !CheckProofOfWork(GetPoWHash(), nBits))
        return DoS(50, error("CheckBlock() : proof of work failed"));

    // Check timestamp
    if (GetBlockTime() > FutureDrift(GetAdjustedTime()))
        return error("CheckBlock() : block timestamp too far in the future");

    // First transaction must be coinbase, the rest must not be
    if (vtx.empty() || !vtx[0].IsCoinBase())
        return DoS(100, error("CheckBlock() : first tx is not coinbase"));
    for (unsigned int i = 1; i < vtx.size(); i++)
        if (vtx[i].IsCoinBase())
            return DoS(100, error("CheckBlock() : more than one coinbase"));

    // Check coinbase timestamp
    if (GetBlockTime() > FutureDrift((int64_t)vtx[0].nTime))
        return DoS(50, error("CheckBlock() : coinbase timestamp is too early"));

    if (IsProofOfStake()) {
        // Coinbase output should be empty if proof-of-stake block
        if (vtx[0].vout.size() != 1 || !vtx[0].vout[0].IsEmpty())
            return DoS(100, error("CheckBlock() : coinbase output not empty for proof-of-stake block"));

        // Second transaction must be coinstake, the rest must not be
        if (vtx.empty() || !vtx[1].IsCoinStake())
            return DoS(100, error("CheckBlock() : second tx is not coinstake"));
        for (unsigned int i = 2; i < vtx.size(); i++)
            if (vtx[i].IsCoinStake())
                return DoS(100, error("CheckBlock() : more than one coinstake"));

        // Check coinstake timestamp
        if (!CheckCoinStakeTimestamp(GetBlockTime(), (int64_t)vtx[1].nTime))
            return DoS(50, error("CheckBlock() : coinstake timestamp violation nTimeBlock=%" PRId64
                                 " nTimeTx=%u",
                                 GetBlockTime(), vtx[1].nTime));

        // NovaCoin: check proof-of-stake block signature
        if (fCheckSig && !CheckBlockSignature())
            return DoS(100, error("CheckBlock() : bad proof-of-stake block signature"));
    }

    // Check transactions
    for (const CTransaction& tx : vtx) {
        if (!tx.CheckTransaction())
            return DoS(tx.nDoS, error("CheckBlock() : CheckTransaction failed"));

        // ppcoin: check transaction timestamp
        if (GetBlockTime() < (int64_t)tx.nTime)
            return DoS(50, error("CheckBlock() : block timestamp earlier than transaction timestamp"));
    }

    // Check for duplicate txids. This is caught by ConnectInputs(),
    // but catching it earlier avoids a potential DoS attack:
    std::set<uint256> uniqueTx;
    for (const CTransaction& tx : vtx) {
        uniqueTx.insert(tx.GetHash());
    }
    if (uniqueTx.size() != vtx.size())
        return DoS(100, error("CheckBlock() : duplicate transaction"));

    unsigned int nSigOps = 0;
    for (const CTransaction& tx : vtx) {
        nSigOps += tx.GetLegacySigOpCount();
    }
    if (nSigOps > MAX_BLOCK_SIGOPS)
        return DoS(100, error("CheckBlock() : out-of-bounds SigOpCount"));

    // Check merkle root
    if (fCheckMerkleRoot && hashMerkleRoot != BuildMerkleTree())
        return DoS(100, error("CheckBlock() : hashMerkleRoot mismatch"));

    return true;
}

bool CBlock::AcceptBlock()
{
    AssertLockHeld(cs_main);

    if (nVersion > CURRENT_VERSION)
        return DoS(100, error("AcceptBlock() : reject unknown block version %d", nVersion));

    // Check for duplicate
    uint256 hash = GetHash();
    if (mapBlockIndex.count(hash))
        return error("AcceptBlock() : block already in mapBlockIndex");

    // protect against a possible attack where an attacker sends predecessors of very early blocks in the
    // blockchain, forcing a non-necessary scan of the whole blockchain
    int64_t maxCheckpointBlockHeight = Checkpoints::GetLastCheckpointBlockHeight();
    if (nBestHeight > maxCheckpointBlockHeight + 1) {
        const uint256 prevBlockHash = this->hashPrevBlock;
        auto          it            = mapBlockIndex.find(prevBlockHash);
        if (it != mapBlockIndex.cend()) {
            int64_t newBlockPrevBlockHeight = it->second->nHeight;
            if (newBlockPrevBlockHeight + 1 < maxCheckpointBlockHeight) {
                return DoS(
                    25,
                    error("Prevblock of block %s, which is %s, is behind the latest checkpoint block "
                          "height: %" PRId64 "\n",
                          this->GetHash().ToString().c_str(), prevBlockHash.ToString().c_str(),
                          maxCheckpointBlockHeight));
            }
        } else {
            return error("The prevblock of %s, which is %s, is not in the blockindex. This should never "
                         "happen in AcceptBlock(), where this error occurred\n",
                         this->GetHash().ToString().c_str(), prevBlockHash.ToString().c_str());
        }
    }

    try {
        CTxDB txdb;
        if (!VerifyInputsUnspent(txdb)) {
            return error("VerifyInputsUnspent() failed for block %s\n",
                         this->GetHash().ToString().c_str());
        }
    } catch (std::exception& ex) {
        return error("VerifyInputsUnspent() threw an exception for block %s; with error: %s\n",
                     this->GetHash().ToString().c_str(), ex.what());
    }

    // Get prev block index
    BlockIndexMapType::iterator mi = mapBlockIndex.find(hashPrevBlock);
    if (mi == mapBlockIndex.end())
        return DoS(10, error("AcceptBlock() : prev block not found\n"));
    CBlockIndexSmartPtr pindexPrev = boost::atomic_load(&mi->second);
    int                 nHeight    = pindexPrev->nHeight + 1;

    if (IsProofOfWork() && nHeight > LAST_POW_BLOCK)
        return DoS(100, error("AcceptBlock() : reject proof-of-work at height %d", nHeight));

    // Check proof-of-work or proof-of-stake
    if (nBits != GetNextTargetRequired(pindexPrev.get(), IsProofOfStake()))
        return DoS(100, error("AcceptBlock() : incorrect %s",
                              IsProofOfWork() ? "proof-of-work" : "proof-of-stake"));

    // Check timestamp against prev
    if (GetBlockTime() <= pindexPrev->GetPastTimeLimit() ||
        FutureDrift(GetBlockTime()) < pindexPrev->GetBlockTime())
        return error("AcceptBlock() : block's timestamp is too early");

    // Check that all transactions are finalized
    for (const CTransaction& tx : vtx)
        if (!IsFinalTx(tx, nHeight, GetBlockTime()))
            return DoS(10, error("AcceptBlock() : contains a non-final transaction"));

    // Check that the block chain matches the known block chain up to a checkpoint
    if (!Checkpoints::CheckHardened(nHeight, hash))
        return DoS(100, error("AcceptBlock() : rejected by hardened checkpoint lock-in at %d", nHeight));

    uint256 hashProof;
    // Verify hash target and signature of coinstake tx
    if (IsProofOfStake()) {
        uint256 targetProofOfStake;
        if (!CheckProofOfStake(vtx[1], nBits, hashProof, targetProofOfStake)) {
            printf("WARNING: AcceptBlock(): check proof-of-stake failed for block %s\n",
                   hash.ToString().c_str());
            return false; // do not error here as we expect this during initial block download
        }
    }
    // PoW is checked in CheckBlock()
    if (IsProofOfWork()) {
        hashProof = GetPoWHash();
    }

    bool cpSatisfies = Checkpoints::CheckSync(hash, pindexPrev.get());

    // Check that the block satisfies synchronized checkpoint
    if (CheckpointsMode == Checkpoints::CPMode_STRICT && !cpSatisfies)
        return error("AcceptBlock() : rejected by synchronized checkpoint");

    if (CheckpointsMode == Checkpoints::CPMode_ADVISORY && !cpSatisfies)
        strMiscWarning = _("WARNING: syncronized checkpoint violation detected, but skipped!");

    // Enforce rule that the coinbase starts with serialized block height
    CScript expect = CScript() << nHeight;
    if (vtx[0].vin[0].scriptSig.size() < expect.size() ||
        !std::equal(expect.begin(), expect.end(), vtx[0].vin[0].scriptSig.begin()))
        return DoS(100, error("AcceptBlock() : block height mismatch in coinbase"));

    // Write block to history file
    if (!CheckDiskSpace(::GetSerializeSize(*this, SER_DISK, CLIENT_VERSION)))
        return error("AcceptBlock() : out of disk space");
    uint256 nBlockPos = hash;
    if (!WriteToDisk(nBlockPos, hashProof))
        return error("AcceptBlock() : WriteToDisk failed");

    // Relay inventory, but don't relay old inventory during initial block download
    int nBlockEstimate = Checkpoints::GetTotalBlocksEstimate();
    if (hashBestChain == hash) {
        LOCK(cs_vNodes);
        for (CNode* pnode : vNodes)
            if (nBestHeight >
                (pnode->nStartingHeight != -1 ? pnode->nStartingHeight - 2000 : nBlockEstimate))
                pnode->PushInventory(CInv(MSG_BLOCK, hash));
    }

    // ppcoin: check pending sync-checkpoint
    Checkpoints::AcceptPendingSyncCheckpoint();

    return true;
}

// novacoin: attempt to generate suitable proof-of-stake
bool CBlock::SignBlock(CWallet& wallet, int64_t nFees)
{
    // if we are trying to sign
    //    something except proof-of-stake block template
    if (!vtx[0].vout[0].IsEmpty())
        return false;

    // if we are trying to sign
    //    a complete proof-of-stake block
    if (IsProofOfStake())
        return true;

    static int64_t nLastCoinStakeSearchTime = GetAdjustedTime(); // startup timestamp

    CKey         key;
    CTransaction txCoinStake;
    int64_t      nSearchTime = txCoinStake.nTime; // search to current time

    CBlockIndexSmartPtr pindexBestPtr = boost::atomic_load(&pindexBest);
    if (nSearchTime > nLastCoinStakeSearchTime) {
        if (wallet.CreateCoinStake(wallet, nBits, nSearchTime - nLastCoinStakeSearchTime, nFees,
                                   txCoinStake, key)) {
            if (txCoinStake.nTime >= std::max(pindexBestPtr->GetPastTimeLimit() + 1,
                                              PastDrift(pindexBestPtr->GetBlockTime()))) {
                // make sure coinstake would meet timestamp protocol
                //    as it would be the same as the block timestamp
                vtx[0].nTime = nTime = txCoinStake.nTime;
                nTime = std::max(pindexBestPtr->GetPastTimeLimit() + 1, GetMaxTransactionTime());
                nTime = std::max(GetBlockTime(), PastDrift(pindexBestPtr->GetBlockTime()));

                // we have to make sure that we have no future timestamps in
                //    our transactions set
                for (std::vector<CTransaction>::iterator it = vtx.begin(); it != vtx.end();)
                    if (it->nTime > nTime) {
                        it = vtx.erase(it);
                    } else {
                        ++it;
                    }

                vtx.insert(vtx.begin() + 1, txCoinStake);
                hashMerkleRoot = BuildMerkleTree();

                // append a signature to our block
                return key.Sign(GetHash(), vchBlockSig);
            }
        }
        nLastCoinStakeSearchInterval = nSearchTime - nLastCoinStakeSearchTime;
        nLastCoinStakeSearchTime     = nSearchTime;
    }

    return false;
}

bool CBlock::CheckBlockSignature() const
{
    if (IsProofOfWork())
        return vchBlockSig.empty();

    std::vector<valtype> vSolutions;
    txnouttype           whichType;

    const CTxOut& txout = vtx[1].vout[1];

    if (!Solver(txout.scriptPubKey, whichType, vSolutions))
        return false;

    if (whichType == TX_PUBKEY) {
        valtype& vchPubKey = vSolutions[0];
        CKey     key;
        if (!key.SetPubKey(vchPubKey))
            return false;
        if (vchBlockSig.empty())
            return false;
        return key.Verify(GetHash(), vchBlockSig);
    }

    return false;
}

bool CBlock::WriteToDisk(const uint256& nBlockPos, const uint256& hashProof)
{
    /**
     * @brief txdb
     * This function writes a whole block in an ACID transaction
     */

    CTxDB       txdb;
    std::size_t req_size = 500 * ::GetSerializeSize(*this, SER_DISK, CLIENT_VERSION);
    if (!txdb.TxnBegin(req_size)) {
        printf("Failed to start transaction for writing a new block.");
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

    if (!txdb.WriteBlock(this->GetHash(), *this)) {
        return false;
    }

    CBlockIndexSmartPtr pindexNew = nullptr;

    if (!AddToBlockIndex(nBlockPos, hashProof, txdb, &pindexNew, false)) {
        return error("AcceptBlock() : AddToBlockIndex failed");
    }

    if (!pindexNew) {
        return error("Major Error: A nullptr CBlockIndex() was found in CBlock::WriteToDisk(), although "
                     "AddToBlockIndex() succeeded. This should never happen!");
    }

    success = true;
    txEnder.reset();
    return true;
}

bool CBlock::ReadFromDisk(const uint256& hash, bool fReadTransactions)
{
    SetNull();
    return CTxDB().ReadBlock(hash, *this, fReadTransactions);
}

bool CBlock::ReadFromDisk(const uint256& hash, CTxDB& txdb, bool fReadTransactions)
{
    SetNull();
    return txdb.ReadBlock(hash, *this, fReadTransactions);
}
