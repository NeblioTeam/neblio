#include "blockhashcache.h"

#include <block.h>
#include <boost/scope_exit.hpp>

BlockHashCache::BlockHashCache()
    : cachedBlockHash(boost::none), cachedSerializedHeader(std::array<uint8_t, HeaderSize>{})
{
}

BlockHashCache::BlockHashCache(const BlockHashCache&)
    : cachedBlockHash(boost::none), cachedSerializedHeader(std::array<uint8_t, HeaderSize>{})
{
}

BlockHashCache::BlockHashCache(BlockHashCache&&)
    : cachedBlockHash(boost::none), cachedSerializedHeader(std::array<uint8_t, HeaderSize>{})
{
}

BlockHashCache& BlockHashCache::operator=(const BlockHashCache& other)
{
    if (&other == this) {
        return *this;
    }
    cachedBlockHash        = boost::none;
    cachedSerializedHeader = std::array<uint8_t, HeaderSize>{};
    return *this;
}

BlockHashCache& BlockHashCache::operator=(BlockHashCache&& other)
{
    if (&other == this) {
        return *this;
    }
    cachedBlockHash        = boost::none;
    cachedSerializedHeader = std::array<uint8_t, HeaderSize>{};
    return *this;
}

uint256 BlockHashCache::GetBlockHash(const CBlock& block) const
{
    while (cacheLockFlag.test_and_set()) {
    }
    BOOST_SCOPE_EXIT_ALL(this) { cacheLockFlag.clear(); };

    const bool recache = std::memcmp(cachedSerializedHeader.data(), &block.nVersion, HeaderSize) != 0;
    if (recache || !cachedBlockHash) {
        cachedBlockHash = block.GetHash(false);
        std::memcpy(cachedSerializedHeader.data(), &block.nVersion, HeaderSize);
    }
    //    assert(*cachedBlockHash == GetPoWHash());
    return *cachedBlockHash;
}
