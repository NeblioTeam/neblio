#include "merkleblock.h"

#include "block.h"
#include "uint256.h"
#include <utility>

CMerkleBlock::CMerkleBlock(const CBlock& block, CBloomFilter& filter)
{
    header = block.GetBlockHeader();

    std::vector<bool>    vMatch;
    std::vector<uint256> vHashes;

    vMatch.reserve(block.vtx.size());
    vHashes.reserve(block.vtx.size());

    for (unsigned int i = 0; i < block.vtx.size(); i++) {
        uint256 hash = block.vtx[i].GetHash();
        if (filter.IsRelevantAndUpdate(block.vtx[i])) {
            vMatch.push_back(true);
            vMatchedTxn.push_back(std::make_pair(i, hash));
        } else {
            vMatch.push_back(false);
        }
        vHashes.push_back(hash);
    }

    txn = CPartialMerkleTree(vHashes, vMatch);
}
