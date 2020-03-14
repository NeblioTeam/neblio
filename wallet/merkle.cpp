// Copyright (c) 2015-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "merkle.h"

#include "block.h"
#include "hash.h"

/*     WARNING! If you're reading this because you're learning about crypto
       and/or designing a new system that will use merkle trees, keep in mind
       that the following merkle tree algorithm has a serious flaw related to
       duplicate txids, resulting in a vulnerability (CVE-2012-2459).

       The reason is that if the number of hashes in the list at a given time
       is odd, the last one is duplicated before computing the next level (which
       is unusual in Merkle trees). This results in certain sequences of
       transactions leading to the same merkle root. For example, these two
       trees:

                    A               A
                  /  \            /   \
                B     C         B       C
               / \    |        / \     / \
              D   E   F       D   E   F   F
             / \ / \ / \     / \ / \ / \ / \
             1 2 3 4 5 6     1 2 3 4 5 6 5 6

       for transaction lists [1,2,3,4,5,6] and [1,2,3,4,5,6,5,6] (where 5 and
       6 are repeated) result in the same root hash A (because the hash of both
       of (F) and (F,F) is C).

       The vulnerability results from being able to send a block with such a
       transaction list, with the same merkle root, and the same block hash as
       the original without duplication, resulting in failed validation. If the
       receiving node proceeds to mark that block as permanently invalid
       however, it will fail to accept further unmodified (and thus potentially
       valid) versions of the same block. We defend against this by detecting
       the case where we would hash two identical hashes at the end of the list
       together, and treating that identically to the block having an invalid
       merkle root. Assuming no double-SHA256 collisions, this will detect all
       known ways of changing the transactions without affecting the merkle
       root.
*/

uint256 ComputeMerkleRoot(std::vector<uint256> hashes, bool* mutated)
{
    bool mutation = false;
    while (hashes.size() > 1) {
        if (mutated) {
            for (size_t pos = 0; pos + 1 < hashes.size(); pos += 2) {
                if (hashes[pos] == hashes[pos + 1])
                    mutation = true;
            }
        }
        if (hashes.size() & 1) {
            hashes.push_back(hashes.back());
        }
        for (unsigned i = 0; i < hashes.size() / 2; i++) {
            hashes[i] = Hash(BEGIN(hashes[2 * i]), END(hashes[2 * i]), BEGIN(hashes[2 * i + 1]),
                             END(hashes[2 * i + 1]));
        }
        hashes.resize(hashes.size() / 2);
    }
    if (mutated)
        *mutated = mutation;
    if (hashes.size() == 0)
        return uint256();
    return hashes[0];
}

uint256 BlockMerkleRoot(const CBlock& block, bool* mutated)
{
    std::vector<uint256> leaves;
    leaves.resize(block.vtx.size());
    for (size_t s = 0; s < block.vtx.size(); s++) {
        leaves[s] = block.vtx[s].GetHash();
    }
    return ComputeMerkleRoot(std::move(leaves), mutated);
}

std::vector<uint256> GetMerkleTree(const std::vector<uint256>& leaves, bool* fMutated)
{
    // first part of the merkle tree is the leaves
    std::vector<uint256> vMerkleTree = leaves;

    int  j       = 0;
    bool mutated = false;
    for (int nSize = leaves.size(); nSize > 1; nSize = (nSize + 1) / 2) {
        for (int i = 0; i < nSize; i += 2) {
            int i2 = std::min(i + 1, nSize - 1);
            if (i2 == i + 1 && i2 + 1 == nSize && vMerkleTree[j + i] == vMerkleTree[j + i2]) {
                // Two identical hashes at the end of the list at a particular level.
                mutated = true;
            }
            vMerkleTree.push_back(Hash(vMerkleTree[j + i].begin(), vMerkleTree[j + i].end(),
                                       vMerkleTree[j + i2].begin(), vMerkleTree[j + i2].end()));
        }
        j += nSize;
    }
    if (fMutated) {
        *fMutated = mutated;
    }
    return vMerkleTree;
}
