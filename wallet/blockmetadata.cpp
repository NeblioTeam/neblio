#include "blockmetadata.h"

BlockMetadata::BlockMetadata(const uint256& blockhashOfBlock, CAmount moneySupplyAtBlock,
                             CAmount mintedAtBlock)
    : blockHash(blockhashOfBlock), nMoneySupply(moneySupplyAtBlock), nMint(mintedAtBlock)
{
}

CAmount BlockMetadata::getMoneySupply() const { return nMoneySupply; }

CAmount BlockMetadata::getMint() const { return nMint; }

uint256 BlockMetadata::getBlockHash() const { return blockHash; }
