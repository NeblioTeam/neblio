#include "block.h"

#include "NetworkForks.h"
#include "blockindex.h"
#include "checkpoints.h"
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
