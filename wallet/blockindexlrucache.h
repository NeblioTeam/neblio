#ifndef BLOCKINDEXLRUCACHE_H
#define BLOCKINDEXLRUCACHE_H

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
#include <cstdint>

template <typename T>
class BlockIndexLRUCache
{
public:
    struct BICacheEntry
    {
        uint256 hash;
        uint256 prevHash;
        T       value;
    };

private:
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

    BlockIndexLRUCacheContainer                                                cacheContainer;
    const std::size_t                                                          maxCacheSize;
    std::function<boost::optional<BICacheEntry>(const ITxDB&, const uint256&)> retriever;
    std::uint_fast64_t                                                         counter = 0;

    bool moveToTop(const uint256& key);

public:
    using RetrieverFunc = std::function<boost::optional<BICacheEntry>(const ITxDB&, const uint256&)>;

    BlockIndexLRUCache(std::size_t CacheSize, const RetrieverFunc& retrieverFunc);
    boost::optional<BICacheEntry> get(const ITxDB& txdb, const uint256& key);
    boost::optional<BICacheEntry> getFromCache(const uint256& key);
    boost::optional<BICacheEntry> getOrderedFront() const;
    boost::optional<BICacheEntry> getOrderedBack() const;
    std::size_t                   size() const;
    bool                          empty() const;
    void                          popOne();
    void                          clear();
};

template <typename T>
bool BlockIndexLRUCache<T>::moveToTop(const uint256& key)
{
    auto& byKey = cacheContainer.template get<KeyTag>();
    auto  it    = byKey.find(key);
    if (it == byKey.end()) {
        return false;
    }
    byKey.modify(it, [this](BIInternalCacheEntry& p) { p.order = counter++; });
    return true;
}

template <typename T>
void BlockIndexLRUCache<T>::popOne()
{
    if (cacheContainer.empty()) {
        return;
    }
    auto& byOrder = cacheContainer.template get<OrderTag>();
    byOrder.erase(byOrder.begin());
}

template <typename T>
BlockIndexLRUCache<T>::BlockIndexLRUCache(std::size_t CacheSize, const RetrieverFunc& retrieverFunc)
    : maxCacheSize(CacheSize), retriever(retrieverFunc)
{
}

template <typename T>
boost::optional<typename BlockIndexLRUCache<T>::BICacheEntry>
BlockIndexLRUCache<T>::get(const ITxDB& txdb, const uint256& key)
{
    boost::optional<BICacheEntry> valFromCache = getFromCache(key);
    if (valFromCache) {
        return valFromCache;
    }

    const boost::optional<BICacheEntry> val = retriever(txdb, key);
    if (val) {
        BIInternalCacheEntry entry;
        entry.key     = val->hash;
        entry.val     = val->value;
        entry.prevKey = val->prevHash;
        entry.order   = counter++;
        cacheContainer.insert(entry);
    }

    if (size() > maxCacheSize) {
        popOne();
    }
    return val;
}

template <typename T>
boost::optional<typename BlockIndexLRUCache<T>::BICacheEntry>
BlockIndexLRUCache<T>::getFromCache(const uint256& key)
{
    auto& byKey = cacheContainer.template get<KeyTag>();
    auto  it    = byKey.find(key);
    if (it != byKey.cend()) {
        moveToTop(key);
        return it->toValue();
    }
    return boost::none;
}

template <typename T>
boost::optional<typename BlockIndexLRUCache<T>::BICacheEntry>
BlockIndexLRUCache<T>::getOrderedFront() const
{
    if (cacheContainer.empty()) {
        return boost::none;
    }
    const auto& byOrder = cacheContainer.template get<OrderTag>();
    return boost::make_optional(byOrder.begin()->toValue());
}

template <typename T>
boost::optional<typename BlockIndexLRUCache<T>::BICacheEntry>
BlockIndexLRUCache<T>::getOrderedBack() const
{
    if (cacheContainer.empty()) {
        return boost::none;
    }
    const auto& byOrder = cacheContainer.template get<OrderTag>();
    return boost::make_optional(byOrder.rbegin()->toValue());
}

template <typename T>
bool BlockIndexLRUCache<T>::empty() const
{
    return cacheContainer.empty();
}

template <typename T>
void BlockIndexLRUCache<T>::clear()
{
    cacheContainer.clear();
}

template <typename T>
std::size_t BlockIndexLRUCache<T>::size() const
{
    return cacheContainer.size();
}

#endif // BLOCKINDEXLRUCACHE_H
