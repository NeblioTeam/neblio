#ifndef CMERKLEBLOCK_H
#define CMERKLEBLOCK_H

#include "block.h"
#include "bloom.h"
#include "partialmerkletree.h"

/** Used to relay blocks as header + vector<merkle branch>
 * to filtered nodes.
 */
class CMerkleBlock
{
public:
    // Public only for unit testing
    CBlock             header;
    CPartialMerkleTree txn;

public:
    // Public only for unit testing and relay testing
    // (not relayed)
    std::vector<std::pair<unsigned int, uint256>> vMatchedTxn;

    // Create from a CBlock, filtering transactions according to filter
    // Note that this will call IsRelevantAndUpdate on the filter for each transaction,
    // thus the filter will likely be modified.
    CMerkleBlock(const CBlock& block, CBloomFilter& filter);

    IMPLEMENT_SERIALIZE(READWRITE(header); READWRITE(txn);)
};

#endif // CMERKLEBLOCK_H
