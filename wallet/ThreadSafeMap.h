#ifndef THREADSAFEMAP_H
#define THREADSAFEMAP_H

#include <boost/optional.hpp>
#include <boost/thread.hpp>
#include <map>
#include <memory>

template <typename K, typename V>
class ThreadSafeMap
{
    using MutexType = boost::shared_mutex;

    std::map<K, V>    theMap;
    mutable MutexType mtx;

public:
    using MapType     = std::map<K, V>;
    using key_type    = K;
    using mapped_type = V;
    using value_type  = std::pair<K, V>;

    ThreadSafeMap();
    ThreadSafeMap(const std::map<K, V>& rhs);
    ThreadSafeMap(const ThreadSafeMap<K, V>& rhs);
    ThreadSafeMap<K, V>&      operator=(const ThreadSafeMap<K, V>& rhs);
    std::size_t               erase(const K& key);
    void                      set(const K& key, const V& value);
    [[nodiscard]] bool        exists(const K& key) const;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] bool        empty() const;
    template <typename K_, typename V_>
    friend bool operator==(const ThreadSafeMap<K_, V_>& lhs, const ThreadSafeMap<K_, V_>& rhs);
    [[nodiscard]] boost::optional<V> get(const K& key) const;
    [[nodiscard]] boost::optional<V> front() const;
    [[nodiscard]] boost::optional<V> back() const;
    void                             clear();
    [[nodiscard]] std::map<K, V>     getInternalMap() const;
    void                             setInternalMap(const std::map<K, V>& TheMap);
    void                             setInternalMap(std::map<K, V>&& TheMap);

    //! Substitute for C++14 std::make_unique.
    template <typename T, typename... Args>
    static std::unique_ptr<T> __InternalMakeUnique(Args&&... args)
    {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }

    [[nodiscard]] std::unique_ptr<boost::lock_guard<MutexType>> get_lock() const;

    [[nodiscard]] std::unique_ptr<boost::unique_lock<MutexType>> get_try_lock() const;

    [[nodiscard]] std::unique_ptr<boost::shared_lock<MutexType>> get_shared_lock() const;

    [[nodiscard]] std::unique_ptr<boost::shared_lock<MutexType>> get_try_shared_lock() const;

    [[nodiscard]] boost::optional<V> get_unsafe(const K& key) const;

    [[nodiscard]] boost::optional<V> front_unsafe() const;

    [[nodiscard]] boost::optional<V> back_unsafe() const;

    void clear_unsafe();

    std::size_t erase_unsafe(const K& key);

    void set_unsafe(const K& key, const V& value);

    [[nodiscard]] bool exists_unsafe(const K& key) const;

    [[nodiscard]] std::size_t size_unsafe() const;

    [[nodiscard]] bool empty_unsafe() const;
};

template <typename K, typename V>
ThreadSafeMap<K, V>::ThreadSafeMap(const ThreadSafeMap<K, V>& rhs)
{
    boost::unique_lock<MutexType> lock1(mtx);
    boost::shared_lock<MutexType> lock2(rhs.mtx);
    this->theMap = rhs.theMap;
}

template <typename K, typename V>
ThreadSafeMap<K, V>& ThreadSafeMap<K, V>::operator=(const ThreadSafeMap<K, V>& rhs)
{
    boost::unique_lock<MutexType> lock1(mtx);
    boost::shared_lock<MutexType> lock2(rhs.mtx);
    this->theMap = rhs.theMap;
    return *this;
}

template <typename K_, typename V_>
bool operator==(const ThreadSafeMap<K_, V_>& lhs, const ThreadSafeMap<K_, V_>& rhs)
{
    if (&lhs == &rhs) {
        return true;
    }
    boost::shared_lock<typename ThreadSafeMap<K_, V_>::MutexType> lock1(lhs.mtx);
    boost::shared_lock<typename ThreadSafeMap<K_, V_>::MutexType> lock2(rhs.mtx);
    return (lhs.theMap == rhs.theMap);
}

template <typename K, typename V>
void ThreadSafeMap<K, V>::set(const K& key, const V& value)
{
    boost::unique_lock<MutexType> lock(mtx);
    set_unsafe(key, value);
}

template <typename K, typename V>
boost::optional<V> ThreadSafeMap<K, V>::get(const K& key) const
{
    boost::shared_lock<MutexType> lock(mtx);
    return get_unsafe(key);
}

template <typename K, typename V>
boost::optional<V> ThreadSafeMap<K, V>::front() const
{
    boost::shared_lock<MutexType> lock(mtx);
    return front_unsafe();
}

template <typename K, typename V>
boost::optional<V> ThreadSafeMap<K, V>::back() const
{
    boost::shared_lock<MutexType> lock(mtx);
    return back_unsafe();
}

template <typename K, typename V>
void ThreadSafeMap<K, V>::clear()
{
    boost::unique_lock<MutexType> lock(mtx);
    clear_unsafe();
}

template <typename K, typename V>
bool ThreadSafeMap<K, V>::exists(const K& key) const
{
    boost::shared_lock<MutexType> lock(mtx);
    return exists_unsafe(key);
}

template <typename K, typename V>
std::size_t ThreadSafeMap<K, V>::size() const
{
    boost::shared_lock<MutexType> lock(mtx);
    return size_unsafe();
}

template <typename K, typename V>
bool ThreadSafeMap<K, V>::empty() const
{
    boost::shared_lock<MutexType> lock(mtx);
    return empty_unsafe();
}

template <typename K, typename V>
ThreadSafeMap<K, V>::ThreadSafeMap()
{
}

template <typename K, typename V>
ThreadSafeMap<K, V>::ThreadSafeMap(const std::map<K, V>& rhs)
{
    boost::unique_lock<MutexType> lock(mtx);
    theMap = rhs;
}

template <typename K, typename V>
std::size_t ThreadSafeMap<K, V>::erase(const K& key)
{
    boost::unique_lock<MutexType> lock(mtx);
    return erase_unsafe(key);
}

template <typename K, typename V>
std::map<K, V> ThreadSafeMap<K, V>::getInternalMap() const
{
    boost::unique_lock<MutexType> lock(mtx);
    std::map<K, V>                safeCopy = theMap;
    return safeCopy;
}

template <typename K, typename V>
void ThreadSafeMap<K, V>::setInternalMap(const std::map<K, V>& TheMap)
{
    boost::unique_lock<MutexType> lock(mtx);
    theMap = TheMap;
}

template <typename K, typename V>
void ThreadSafeMap<K, V>::setInternalMap(std::map<K, V>&& TheMap)
{
    boost::unique_lock<MutexType> lock(mtx);
    theMap = std::move(TheMap);
}

template <typename K, typename V>
std::unique_ptr<boost::lock_guard<typename ThreadSafeMap<K, V>::MutexType>>
ThreadSafeMap<K, V>::get_lock() const
{
    return __InternalMakeUnique<boost::lock_guard<MutexType>>(mtx);
}

template <typename K, typename V>
std::unique_ptr<boost::unique_lock<typename ThreadSafeMap<K, V>::MutexType>>
ThreadSafeMap<K, V>::get_try_lock() const
{
    auto lock = __InternalMakeUnique<boost::unique_lock<MutexType>>(mtx, boost::defer_lock);
    if (mtx.try_lock()) {
        return lock;
    } else {
        return nullptr;
    }
}

template <typename K, typename V>
std::unique_ptr<boost::shared_lock<typename ThreadSafeMap<K, V>::MutexType>>
ThreadSafeMap<K, V>::get_shared_lock() const
{
    return __InternalMakeUnique<boost::shared_lock<MutexType>>(mtx);
}

template <typename K, typename V>
std::unique_ptr<boost::shared_lock<typename ThreadSafeMap<K, V>::MutexType>>
ThreadSafeMap<K, V>::get_try_shared_lock() const
{
    auto lock = __InternalMakeUnique<boost::shared_lock<MutexType>>(mtx, boost::defer_lock);
    if (mtx.try_lock()) {
        return lock;
    } else {
        return nullptr;
    }
}

template <typename K, typename V>
boost::optional<V> ThreadSafeMap<K, V>::get_unsafe(const K& key) const
{
    typename std::map<K, V>::const_iterator it = theMap.find(key);
    if (it == theMap.cend()) {
        return boost::none;
    } else {
        return boost::make_optional(it->second);
    }
}

template <typename K, typename V>
boost::optional<V> ThreadSafeMap<K, V>::front_unsafe() const
{
    if (theMap.size() == 0) {
        return boost::none;
    }
    return boost::make_optional(*theMap.cbegin());
}

template <typename K, typename V>
boost::optional<V> ThreadSafeMap<K, V>::back_unsafe() const
{
    if (theMap.size() == 0) {
        return boost::none;
    }
    return boost::make_optional(*theMap.crbegin());
}

template <typename K, typename V>
void ThreadSafeMap<K, V>::clear_unsafe()
{
    theMap.clear();
}

template <typename K, typename V>
std::size_t ThreadSafeMap<K, V>::erase_unsafe(const K& key)
{
    return theMap.erase(key);
}

template <typename K, typename V>
void ThreadSafeMap<K, V>::set_unsafe(const K& key, const V& value)
{
    theMap[key] = value;
}

template <typename K, typename V>
bool ThreadSafeMap<K, V>::exists_unsafe(const K& key) const
{
    return (theMap.find(key) != theMap.end());
}

template <typename K, typename V>
std::size_t ThreadSafeMap<K, V>::size_unsafe() const
{
    return theMap.size();
}

template <typename K, typename V>
bool ThreadSafeMap<K, V>::empty_unsafe() const
{
    return theMap.empty();
}

#endif // THREADSAFEMAP_H
