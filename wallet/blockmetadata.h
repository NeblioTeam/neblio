#ifndef BLOCKMETADATA_H
#define BLOCKMETADATA_H

#include "amount.h"
#include "serialize.h"
#include "uint256.h"

class BlockMetadata
{
    uint256 blockHash    = 0;
    CAmount nMoneySupply = 0;
    CAmount nMint        = 0;

public:
    BlockMetadata(const uint256& blockhashOfBlock, CAmount moneySupplyAtBlock, CAmount mintedAtBlock);

    // clang-format off
    IMPLEMENT_SERIALIZE(
        READWRITE(blockHash);
        READWRITE(nMoneySupply);
        READWRITE(nMint);
    )
    // clang-format on

    uint256 getBlockHash() const;
    CAmount getMoneySupply() const;
    CAmount getMint() const;
};

#endif // BLOCKMETADATA_H
