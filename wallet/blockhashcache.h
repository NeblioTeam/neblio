#ifndef BLOCKHASHCACHE_H
#define BLOCKHASHCACHE_H

#include <array>
#include <boost/atomic.hpp>
#include <boost/optional.hpp>
#include <uint256.h>

class CBlockHeader;

class BlockHashCache
{
    static const int HeaderSize = 80;

    mutable boost::atomic_flag              cacheLockFlag = BOOST_ATOMIC_FLAG_INIT;
    mutable boost::optional<uint256>        cachedBlockHash;
    mutable std::array<uint8_t, HeaderSize> cachedSerializedHeader{};

public:
    BlockHashCache();

    BlockHashCache(const BlockHashCache& /*other*/);

    BlockHashCache(BlockHashCache&& /*other*/);

    BlockHashCache& operator=(const BlockHashCache& other);

    BlockHashCache& operator=(BlockHashCache&& other);

    uint256 GetBlockHash(const CBlockHeader& block) const;
};

#endif // BLOCKHASHCACHE_H
