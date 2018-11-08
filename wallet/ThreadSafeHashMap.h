#ifndef THREADSAFEHASHMAP_H
#define THREADSAFEHASHMAP_H

#include <boost/thread.hpp>
#include <unordered_map>

template <typename K, typename V>
class ThreadSafeHashMap
{
    std::unordered_map<K, V>    theMap;
    mutable boost::shared_mutex mtx;

public:
    ThreadSafeHashMap();
    ThreadSafeHashMap(const ThreadSafeHashMap<K, V>& rhs);
    ThreadSafeHashMap<K, V>& operator=(const ThreadSafeHashMap<K, V>& rhs);
    std::size_t              erase(const K& key);
    void                     set(const K& key, const V& value);
    bool                     exists(const K& key) const;
    template <typename K_, typename V_>
    friend bool operator==(const ThreadSafeHashMap<K_, V_>& lhs, const ThreadSafeHashMap<K_, V_>& rhs);
    bool        get(const K& key, V& value) const;
    void        clear();
    std::unordered_map<K, V> getInternalMap() const;
    void                     setInternalMap(const std::unordered_map<K, V>& TheMap);
};

template <typename K, typename V>
ThreadSafeHashMap<K, V>::ThreadSafeHashMap(const ThreadSafeHashMap<K, V>& rhs)
{
    boost::unique_lock<boost::shared_mutex> lock1(mtx);
    boost::shared_lock<boost::shared_mutex> lock2(rhs.mtx);
    this->theMap = rhs.theMap;
}

template <typename K, typename V>
ThreadSafeHashMap<K, V>& ThreadSafeHashMap<K, V>::operator=(const ThreadSafeHashMap<K, V>& rhs)
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

template <typename K, typename V>
void ThreadSafeHashMap<K, V>::set(const K& key, const V& value)
{
    boost::unique_lock<boost::shared_mutex> lock(mtx);
    theMap[key] = value;
}

template <typename K, typename V>
bool ThreadSafeHashMap<K, V>::get(const K& key, V& value) const
{
    boost::shared_lock<boost::shared_mutex>           lock(mtx);
    typename std::unordered_map<K, V>::const_iterator it = theMap.find(key);
    if (it == theMap.cend()) {
        return false;
    } else {
        value = it->second;
        return true;
    }
}

template <typename K, typename V>
void ThreadSafeHashMap<K, V>::clear()
{
    boost::unique_lock<boost::shared_mutex> lock(mtx);
    theMap.clear();
}

template <typename K, typename V>
bool ThreadSafeHashMap<K, V>::exists(const K& key) const
{
    boost::shared_lock<boost::shared_mutex> lock(mtx);
    return (theMap.find(key) != theMap.end());
}

template <typename K, typename V>
ThreadSafeHashMap<K, V>::ThreadSafeHashMap()
{
}

template <typename K, typename V>
std::size_t ThreadSafeHashMap<K, V>::erase(const K& key)
{
    boost::unique_lock<boost::shared_mutex> lock(mtx);
    return theMap.erase(key);
}

template <typename K, typename V>
std::unordered_map<K, V> ThreadSafeHashMap<K, V>::getInternalMap() const
{
    boost::unique_lock<boost::shared_mutex> lock(mtx);
    std::unordered_map<K, V>                safeCopy = theMap;
    return safeCopy;
}

template <typename K, typename V>
void ThreadSafeHashMap<K, V>::setInternalMap(const std::unordered_map<K, V>& TheMap)
{
    boost::unique_lock<boost::shared_mutex> lock(mtx);
    theMap = TheMap;
}

#endif // THREADSAFEHASHMAP_H
