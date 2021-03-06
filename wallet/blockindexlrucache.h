#ifndef BLOCKINDEXLRUCACHE_H
#define BLOCKINDEXLRUCACHE_H

#include "blockindex.h"
#include "itxdb.h"
#include "uint256.h"
#include <boost/multi_array/index_gen.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/thread/lock_guard.hpp>
#include <cstdint>

namespace neblio_dummy {
class dummy_mutex
{
public:
    void lock() {}
    bool try_lock() { return true; }
    void unlock() {}
};
} // namespace neblio_dummy

template <typename T, typename MutexType = neblio_dummy::dummy_mutex>
class BlockIndexLRUCache
{
public:
    struct BICacheEntry
    {
        uint256 hash;
        uint256 prevHash;
        T       value;
    };

    // function that extracts the T value from the block index
    using ExtractorFunc = std::function<T(const CBlockIndex&)>;

private:
    mutable MutexType mtx;

    struct KeyTag
    {
    };

    struct PrevKeyTag
    {
    };

    struct OrderTag
    {
    };

    struct BIInternalCacheEntry
    {
        uint256            key;
        T                  val;
        std::uint_fast64_t order;
        uint256            prevKey;
        BICacheEntry       toValue() const
        {
            BICacheEntry result;
            result.hash     = key;
            result.prevHash = prevKey;
            result.value    = val;
            return result;
        }
    };

    using BlockIndexLRUCacheContainer = boost::multi_index_container<
        BIInternalCacheEntry,
        boost::multi_index::indexed_by<
            // clang-format off
            boost::multi_index::ordered_unique<boost::multi_index::tag<OrderTag>,
                                               BOOST_MULTI_INDEX_MEMBER(BIInternalCacheEntry,
                                                                        std::uint_fast64_t,
                                                                        order)>,
            boost::multi_index::ordered_unique<boost::multi_index::tag<KeyTag>,
                                               BOOST_MULTI_INDEX_MEMBER(BIInternalCacheEntry,
                                                                        uint256,
                                                                        key)>,
            boost::multi_index::ordered_unique<boost::multi_index::tag<PrevKeyTag>,
                                               BOOST_MULTI_INDEX_MEMBER(BIInternalCacheEntry,
                                                                        uint256,
                                                                        prevKey)>
            // clang-format on
            >>;

    BlockIndexLRUCacheContainer cacheContainer;
    std::size_t           maxCacheSize;
    ExtractorFunc               extractor;
    std::uint_fast64_t          counter = 0;

    bool moveToTop_unsafe(const uint256& key);

    boost::optional<BICacheEntry> FromDBRetriever(const ITxDB& txdb, const uint256& hash)
    {
        const boost::optional<CBlockIndex> bi = txdb.ReadBlockIndex(hash);
        if (!bi) {
            return boost::none;
        }
        BICacheEntry result;
        result.hash     = bi->blockHash;
        result.prevHash = bi->hashPrev;
        result.value    = extractor(*bi);
        return boost::make_optional(std::move(result));
    }

public:
    BlockIndexLRUCache(std::size_t CacheSize, const ExtractorFunc& extractorFunc);
    boost::optional<BICacheEntry> get(const ITxDB& txdb, const uint256& key);
    boost::optional<BICacheEntry> getFromCache(const uint256& key) const;
    boost::optional<BICacheEntry> getOrderedFront() const;
    boost::optional<BICacheEntry> getOrderedBack() const;
    void                          manualAdd(const CBlockIndex& bi);
    void                          setExtractor(const ExtractorFunc& extractorFunc);
    std::size_t                   size() const;
    bool                          empty() const;
    void                          popOne();
    void                          popOne_unsafe();
    void                          updateCacheSize(std::size_t newCacheSize);
    void                          clear();
};

template <typename T, typename MutexType>
bool BlockIndexLRUCache<T, MutexType>::moveToTop_unsafe(const uint256& key)
{
    auto& byKey = cacheContainer.template get<KeyTag>();
    auto  it    = byKey.find(key);
    if (it == byKey.end()) {
        return false;
    }
    byKey.modify(it, [this](BIInternalCacheEntry& p) { p.order = counter++; });
    return true;
}

template <typename T, typename MutexType>
void BlockIndexLRUCache<T, MutexType>::popOne_unsafe()
{
    if (cacheContainer.empty()) {
        return;
    }
    auto& byOrder = cacheContainer.template get<OrderTag>();
    byOrder.erase(byOrder.begin());
}

template<typename T, typename MutexType>
void BlockIndexLRUCache<T,MutexType>::updateCacheSize(std::size_t newCacheSize)
{
    boost::lock_guard<MutexType> lg(mtx);
    maxCacheSize = newCacheSize;
}

template <typename T, typename MutexType>
void BlockIndexLRUCache<T, MutexType>::popOne()
{
    boost::lock_guard<MutexType> lg(mtx);
    popOne_unsafe();
}

template <typename T, typename MutexType>
BlockIndexLRUCache<T, MutexType>::BlockIndexLRUCache(std::size_t          CacheSize,
                                                     const ExtractorFunc& extractorFunc)
    : maxCacheSize(CacheSize), extractor(extractorFunc)
{
}

template <typename T, typename MutexType>
boost::optional<typename BlockIndexLRUCache<T, MutexType>::BICacheEntry>
BlockIndexLRUCache<T, MutexType>::get(const ITxDB& txdb, const uint256& key)
{
    const boost::optional<BICacheEntry> valFromCache = getFromCache(key);
    if (valFromCache) {
        return valFromCache;
    }

    boost::lock_guard<MutexType> lg(mtx);

    const boost::optional<BICacheEntry> val = FromDBRetriever(txdb, key);
    if (val) {
        BIInternalCacheEntry entry;
        entry.key     = val->hash;
        entry.val     = val->value;
        entry.prevKey = val->prevHash;
        entry.order   = counter++;
        cacheContainer.insert(entry);
    }

    while (cacheContainer.size() > maxCacheSize) {
        popOne_unsafe();
    }
    return val;
}

template <typename T, typename MutexType>
boost::optional<typename BlockIndexLRUCache<T, MutexType>::BICacheEntry>
BlockIndexLRUCache<T, MutexType>::getFromCache(const uint256& key) const
{
    boost::lock_guard<MutexType> lg(mtx);
    auto&                        byKey = cacheContainer.template get<KeyTag>();
    auto                         it    = byKey.find(key);
    if (it != byKey.cend()) {
        // we don't move to top because it doesn't help performance when scanning back through many
        // blocks moveToTop_unsafe(key);
        return it->toValue();
    }
    return boost::none;
}

template <typename T, typename MutexType>
boost::optional<typename BlockIndexLRUCache<T, MutexType>::BICacheEntry>
BlockIndexLRUCache<T, MutexType>::getOrderedFront() const
{
    boost::lock_guard<MutexType> lg(mtx);
    if (cacheContainer.empty()) {
        return boost::none;
    }
    const auto& byOrder = cacheContainer.template get<OrderTag>();
    return boost::make_optional(byOrder.begin()->toValue());
}

template <typename T, typename MutexType>
boost::optional<typename BlockIndexLRUCache<T, MutexType>::BICacheEntry>
BlockIndexLRUCache<T, MutexType>::getOrderedBack() const
{
    boost::lock_guard<MutexType> lg(mtx);
    if (cacheContainer.empty()) {
        return boost::none;
    }
    const auto& byOrder = cacheContainer.template get<OrderTag>();
    return boost::make_optional(byOrder.rbegin()->toValue());
}

template <typename T, typename MutexType>
void BlockIndexLRUCache<T, MutexType>::manualAdd(const CBlockIndex& bi)
{
    BIInternalCacheEntry entry;
    entry.key     = bi.GetBlockHash();
    entry.val     = extractor(bi);
    entry.prevKey = bi.hashPrev;
    entry.order   = counter++;
    boost::lock_guard<MutexType> lg(mtx);
    cacheContainer.insert(entry);

    while (cacheContainer.size() > maxCacheSize) {
        popOne_unsafe();
    }
}

template<typename T, typename MutexType>
void BlockIndexLRUCache<T,MutexType>::setExtractor(const ExtractorFunc& extractorFunc)
{
    extractor = extractorFunc;
}

template <typename T, typename MutexType>
bool BlockIndexLRUCache<T, MutexType>::empty() const
{
    boost::lock_guard<MutexType> lg(mtx);
    return cacheContainer.empty();
}

template <typename T, typename MutexType>
void BlockIndexLRUCache<T, MutexType>::clear()
{
    boost::lock_guard<MutexType> lg(mtx);
    cacheContainer.clear();
}

template <typename T, typename MutexType>
std::size_t BlockIndexLRUCache<T, MutexType>::size() const
{
    boost::lock_guard<MutexType> lg(mtx);
    return cacheContainer.size();
}

#endif // BLOCKINDEXLRUCACHE_H
