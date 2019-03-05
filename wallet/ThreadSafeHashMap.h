#ifndef THREADSAFEHASHMAP_H
#define THREADSAFEHASHMAP_H

#include <boost/thread.hpp>
#include <unordered_map>

template <typename K, typename V, typename Hasher = std::hash<K>>
class ThreadSafeHashMap
{
    std::unordered_map<K, V, Hasher> theMap;
    mutable boost::shared_mutex      mtx;

public:
    using MapType = std::unordered_map<K, V, Hasher>;

    ThreadSafeHashMap();
    ThreadSafeHashMap(const std::unordered_map<K, V, Hasher>& rhs);
    ThreadSafeHashMap(const ThreadSafeHashMap<K, V, Hasher>& rhs);
    ThreadSafeHashMap<K, V, Hasher>& operator=(const ThreadSafeHashMap<K, V, Hasher>& rhs);
    std::size_t                      erase(const K& key);
    void                             set(const K& key, const V& value);
    bool                             exists(const K& key) const;
    std::size_t                      size() const;
    bool                             empty() const;
    template <typename K_, typename V_>
    friend bool operator==(const ThreadSafeHashMap<K_, V_>& lhs, const ThreadSafeHashMap<K_, V_>& rhs);
    bool        get(const K& key, V& value) const;
    void        clear();
    std::unordered_map<K, V, Hasher> getInternalMap() const;
    void                             setInternalMap(const std::unordered_map<K, V, Hasher>& TheMap);
};

template <typename K, typename V, typename Hasher>
ThreadSafeHashMap<K, V, Hasher>::ThreadSafeHashMap(const ThreadSafeHashMap<K, V, Hasher>& rhs)
{
    boost::unique_lock<boost::shared_mutex> lock1(mtx);
    boost::shared_lock<boost::shared_mutex> lock2(rhs.mtx);
    this->theMap = rhs.theMap;
}

template <typename K, typename V, typename Hasher>
ThreadSafeHashMap<K, V, Hasher>& ThreadSafeHashMap<K, V, Hasher>::
                                 operator=(const ThreadSafeHashMap<K, V, Hasher>& rhs)
{
    boost::unique_lock<boost::shared_mutex> lock1(mtx);
    boost::shared_lock<boost::shared_mutex> lock2(rhs.mtx);
    this->theMap = rhs.theMap;
    return *this;
}

template <typename K_, typename V_>
bool operator==(const ThreadSafeHashMap<K_, V_>& lhs, const ThreadSafeHashMap<K_, V_>& rhs)
{
    if (&lhs == &rhs) {
        return true;
    }
    boost::shared_lock<boost::shared_mutex> lock1(lhs.mtx);
    boost::shared_lock<boost::shared_mutex> lock2(rhs.mtx);
    return (lhs.theMap == rhs.theMap);
}

template <typename K, typename V, typename Hasher>
void ThreadSafeHashMap<K, V, Hasher>::set(const K& key, const V& value)
{
    boost::unique_lock<boost::shared_mutex> lock(mtx);
    theMap[key] = value;
}

template <typename K, typename V, typename Hasher>
bool ThreadSafeHashMap<K, V, Hasher>::get(const K& key, V& value) const
{
    boost::shared_lock<boost::shared_mutex>                   lock(mtx);
    typename std::unordered_map<K, V, Hasher>::const_iterator it = theMap.find(key);
    if (it == theMap.cend()) {
        return false;
    } else {
        value = it->second;
        return true;
    }
}

template <typename K, typename V, typename Hasher>
void ThreadSafeHashMap<K, V, Hasher>::clear()
{
    boost::unique_lock<boost::shared_mutex> lock(mtx);
    theMap.clear();
}

template <typename K, typename V, typename Hasher>
bool ThreadSafeHashMap<K, V, Hasher>::exists(const K& key) const
{
    boost::shared_lock<boost::shared_mutex> lock(mtx);
    return (theMap.find(key) != theMap.end());
}

template <typename K, typename V, typename Hasher>
std::size_t ThreadSafeHashMap<K, V, Hasher>::size() const
{
    boost::shared_lock<boost::shared_mutex> lock(mtx);
    return theMap.size();
}

template <typename K, typename V, typename Hasher>
bool ThreadSafeHashMap<K, V, Hasher>::empty() const
{
    boost::shared_lock<boost::shared_mutex> lock(mtx);
    return theMap.empty();
}

template <typename K, typename V, typename Hasher>
ThreadSafeHashMap<K, V, Hasher>::ThreadSafeHashMap()
{
}

template <typename K, typename V, typename Hasher>
ThreadSafeHashMap<K, V, Hasher>::ThreadSafeHashMap(const std::unordered_map<K, V, Hasher>& rhs)
{
    boost::unique_lock<boost::shared_mutex> lock(mtx);
    theMap = rhs;
}

template <typename K, typename V, typename Hasher>
std::size_t ThreadSafeHashMap<K, V, Hasher>::erase(const K& key)
{
    boost::unique_lock<boost::shared_mutex> lock(mtx);
    return theMap.erase(key);
}

template <typename K, typename V, typename Hasher>
std::unordered_map<K, V, Hasher> ThreadSafeHashMap<K, V, Hasher>::getInternalMap() const
{
    boost::unique_lock<boost::shared_mutex> lock(mtx);
    std::unordered_map<K, V, Hasher>        safeCopy = theMap;
    return safeCopy;
}

template <typename K, typename V, typename Hasher>
void ThreadSafeHashMap<K, V, Hasher>::setInternalMap(const std::unordered_map<K, V, Hasher>& TheMap)
{
    boost::unique_lock<boost::shared_mutex> lock(mtx);
    theMap = TheMap;
}

#endif // THREADSAFEHASHMAP_H
