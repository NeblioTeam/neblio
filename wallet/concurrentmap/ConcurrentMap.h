#ifndef CONCURRENTMAP_H
#define CONCURRENTMAP_H

#include <boost/optional.hpp>
#include <boost/thread/pthread/shared_mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <cstddef>
#include <mutex>
#include <type_traits>
#include <vector>

template <typename K, typename V, std::size_t BucketCount, typename Hasher = std::hash<K>>
class ConcurrentMap
{
    static_assert(BucketCount > 0, "Some buckets?");

public:
    using BucketMapType = std::map<K, V>;

private:
    using MutexType = boost::shared_mutex;

    std::array<BucketMapType, BucketCount>               buckets = {};
    mutable std::array<boost::shared_mutex, BucketCount> locks   = {};

public:
    ConcurrentMap();
    ~ConcurrentMap() = default;

    std::size_t                      erase(const K& key);
    void                             set(const K& key, const V& value);
    [[nodiscard]] bool               exists(const K& key) const;
    [[nodiscard]] std::size_t        size() const;
    [[nodiscard]] bool               empty() const;
    [[nodiscard]] boost::optional<V> get(const K& key) const;
    void                             clear();

    template <typename T>
    struct function_traits : public function_traits<decltype(&T::operator())>
    {
    };
    // For generic types, directly use the result of the signature of its 'operator()'

    template <typename ClassType, typename ReturnType, typename... Args>
    struct function_traits<ReturnType (ClassType::*)(Args...) const>
    // we specialize for pointers to member function
    {
        enum
        {
            arity = sizeof...(Args)
        };
        // arity is the number of arguments.

        typedef ReturnType result_type;

        template <size_t i>
        struct arg
        {
            typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
            // the i-th argument is equivalent to the i-th tuple element of a tuple
            // composed of those arguments.
        };
    };

    template <typename Func>
    void apply(const K& key, Func&& func)
    {
        static_assert(std::is_convertible<Func, std::function<void(BucketMapType&, const K&)>>::value,
                      "Invalid function signature");

        std::size_t bucketIndex = Hasher()(key) % BucketCount;

        boost::unique_lock<boost::shared_mutex> lg(locks[bucketIndex]);
        func(buckets[bucketIndex], key);
    }

    template <typename Func, typename Ret = typename function_traits<Func>::result_type>
    Ret applyWithRet(const K& key, Func&& func)
    {
        static_assert(std::is_convertible<Func, std::function<Ret(BucketMapType&, const K&)>>::value,
                      "Invalid function signature");

        std::size_t bucketIndex = Hasher()(key) % BucketCount;

        boost::unique_lock<boost::shared_mutex> lg(locks[bucketIndex]);
        return func(buckets[bucketIndex], key);
    }

    template <typename Func, typename Ret = typename function_traits<Func>::result_type>
    Ret produce(const K& key, Func&& func) const
    {
        static_assert(
            std::is_convertible<Func, std::function<Ret(const BucketMapType&, const K&)>>::value,
            "Invalid function signature");

        std::size_t bucketIndex = Hasher()(key) % BucketCount;

        boost::shared_lock<boost::shared_mutex> lg(locks[bucketIndex]);
        return func(buckets[bucketIndex], key);
    }

    BucketMapType getAllData() const;
};

template <typename K, typename V, std::size_t C, typename Hasher>
ConcurrentMap<K, V, C, Hasher>::ConcurrentMap()
{
}

template <typename K, typename V, std::size_t C, typename Hasher>
std::size_t ConcurrentMap<K, V, C, Hasher>::erase(const K& key)
{

    std::size_t bucketIndex = Hasher()(key) % C;

    boost::unique_lock<boost::shared_mutex> lg(locks[bucketIndex]);
    return buckets[bucketIndex].erase(key);
}

template <typename K, typename V, std::size_t C, typename Hasher>
void ConcurrentMap<K, V, C, Hasher>::set(const K& key, const V& value)
{
    std::size_t bucketIndex = Hasher()(key) % C;

    boost::unique_lock<boost::shared_mutex> lg(locks[bucketIndex]);

    auto it = buckets[bucketIndex].find(key);
    if (it != buckets[bucketIndex].end()) {
        it->second = value;
    } else {
        buckets[bucketIndex].insert(std::make_pair(key, value));
    }
}

template <typename K, typename V, std::size_t C, typename Hasher>
bool ConcurrentMap<K, V, C, Hasher>::exists(const K& key) const
{
    std::size_t bucketIndex = Hasher()(key) % C;

    boost::shared_lock<boost::shared_mutex> lg(locks[bucketIndex]);
    return buckets[bucketIndex].count(key);
}

template <typename K, typename V, std::size_t C, typename Hasher>
std::size_t ConcurrentMap<K, V, C, Hasher>::size() const
{
    std::size_t result = 0;
    for (int i = 0; i < C; i++) {
        boost::shared_lock<boost::shared_mutex> lg(locks[i]);
        result += buckets[i].size();
    }
    return result;
}

template <typename K, typename V, std::size_t C, typename Hasher>
bool ConcurrentMap<K, V, C, Hasher>::empty() const
{
    return size() == 0;
}

template <typename K, typename V, std::size_t C, typename Hasher>
boost::optional<V> ConcurrentMap<K, V, C, Hasher>::get(const K& key) const
{
    std::size_t bucketIndex = Hasher()(key) % C;

    boost::shared_lock<boost::shared_mutex> lg(locks[bucketIndex]);

    const auto& m  = buckets[bucketIndex];
    auto        it = buckets[bucketIndex].find(key);
    if (it == buckets[bucketIndex].cend()) {
        return boost::none;
    }
    return it->second;
}

template <typename K, typename V, std::size_t C, typename Hasher>
void ConcurrentMap<K, V, C, Hasher>::clear()
{
    for (std::size_t i = 0; i < C; i++) {
        boost::unique_lock<boost::shared_mutex> lg(locks[i]);
        buckets[i].clear();
    }
}

template <typename K, typename V, std::size_t C, typename Hasher>
typename ConcurrentMap<K, V, C, Hasher>::BucketMapType ConcurrentMap<K, V, C, Hasher>::getAllData() const
{
    BucketMapType result;
    for (std::size_t i = 0; i < C; i++) {
        boost::shared_lock<boost::shared_mutex> lg(locks[i]);
        result.insert(buckets[i].begin(), buckets[i].end());
    }
    return result;
}

#endif
