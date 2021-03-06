// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SYNC_H
#define BITCOIN_SYNC_H

#include "threadsafety.h"

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/atomic.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/optional.hpp>


////////////////////////////////////////////////
//                                            //
// THE SIMPLE DEFINITON, EXCLUDING DEBUG CODE //
//                                            //
////////////////////////////////////////////////

/*
 
 
 
CCriticalSection mutex;
    boost::recursive_mutex mutex;

LOCK(mutex);
    boost::unique_lock<boost::recursive_mutex> criticalblock(mutex);

LOCK2(mutex1, mutex2);
    boost::unique_lock<boost::recursive_mutex> criticalblock1(mutex1);
    boost::unique_lock<boost::recursive_mutex> criticalblock2(mutex2);

TRY_LOCK(mutex, name);
    boost::unique_lock<boost::recursive_mutex> name(mutex, boost::try_to_lock_t);

ENTER_CRITICAL_SECTION(mutex); // no RAII
    mutex.lock();

LEAVE_CRITICAL_SECTION(mutex); // no RAII
    mutex.unlock();
 
 
 
 */



///////////////////////////////
//                           //
// THE ACTUAL IMPLEMENTATION //
//                           //
///////////////////////////////

// Template mixin that adds -Wthread-safety locking annotations to a
// subset of the mutex API.
template <typename PARENT>
class LOCKABLE AnnotatedMixin : public PARENT
{
public:
    void lock() EXCLUSIVE_LOCK_FUNCTION()
    {
      PARENT::lock();
    }

    void unlock() UNLOCK_FUNCTION()
    {
      PARENT::unlock();
    }

    bool try_lock() EXCLUSIVE_TRYLOCK_FUNCTION(true)
    {
      return PARENT::try_lock();
    }
};

/** Wrapped boost mutex: supports recursive locking, but no waiting  */
// TODO: We should move away from using the recursive lock by default.
typedef AnnotatedMixin<boost::recursive_mutex> CCriticalSection;

/** Wrapped boost mutex: supports waiting but not recursive locking */
typedef AnnotatedMixin<boost::mutex> CWaitableCriticalSection;

#ifdef DEBUG_LOCKORDER
void EnterCritical(const char* pszName, const char* pszFile, int nLine, void* cs, bool fTry = false);
void LeaveCritical();
std::string LocksHeld();
void AssertLockHeldInternal(const char* pszName, const char* pszFile, int nLine, void *cs);
#else
void static inline EnterCritical(const char* /*pszName*/, const char* /*pszFile*/, int /*nLine*/, void* /*cs*/, bool /*fTry*/ = false) {}
void static inline LeaveCritical() {}
void static inline AssertLockHeldInternal(const char* /*pszName*/, const char* /*pszFile*/, int /*nLine*/, void */*cs*/) {}
#endif
#define AssertLockHeld(cs) AssertLockHeldInternal(#cs, __FILE__, __LINE__, &cs)

#ifdef DEBUG_LOCKCONTENTION
void PrintLockContention(const char* pszName, const char* pszFile, int nLine);
#endif

/** Wrapper around boost::unique_lock<Mutex> */
template<typename Mutex>
class CMutexLock
{
private:
    boost::unique_lock<Mutex> lock;

    void Enter(const char* pszName, const char* pszFile, int nLine)
    {
        EnterCritical(pszName, pszFile, nLine, (void*)(lock.mutex()));
#ifdef DEBUG_LOCKCONTENTION
        if (!lock.try_lock())
        {
            PrintLockContention(pszName, pszFile, nLine);
#endif
        lock.lock();
#ifdef DEBUG_LOCKCONTENTION
        }
#endif
    }

    bool TryEnter(const char* pszName, const char* pszFile, int nLine)
    {
        EnterCritical(pszName, pszFile, nLine, (void*)(lock.mutex()), true);
        lock.try_lock();
        if (!lock.owns_lock())
            LeaveCritical();
        return lock.owns_lock();
    }

public:
    CMutexLock(Mutex& mutexIn, const char* pszName, const char* pszFile, int nLine, bool fTry = false) : lock(mutexIn, boost::defer_lock)
    {
        if (fTry)
            TryEnter(pszName, pszFile, nLine);
        else
            Enter(pszName, pszFile, nLine);
    }

    ~CMutexLock()
    {
        if (lock.owns_lock())
            LeaveCritical();
    }

    operator bool()
    {
        return lock.owns_lock();
    }
};

typedef CMutexLock<CCriticalSection> CCriticalBlock;

//! Substitute for C++14 std::make_unique for this file.
template <typename T, typename... Args>
std::unique_ptr<T> __InternalSyncMakeUnique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template <typename M1, typename M2>
auto _lock2_internal(M1&& m1, M2&& m2) -> std::pair<std::unique_ptr<boost::unique_lock<typename std::decay<decltype(m1)>::type>>, std::unique_ptr<boost::unique_lock<typename std::decay<decltype(m2)>::type>>>
{
    auto res =
        std::make_pair(
            __InternalSyncMakeUnique<boost::unique_lock<typename std::decay<decltype(m1)>::type>>(m1, boost::defer_lock),
            __InternalSyncMakeUnique<boost::unique_lock<typename std::decay<decltype(m2)>::type>>(m2, boost::defer_lock));
    boost::lock(*res.first, *res.second);
    return res;
}

template <typename M>
auto _trylock_internal(M&& m) -> std::unique_ptr<boost::unique_lock<typename std::decay<decltype(m)>::type>> {
    auto lm = __InternalSyncMakeUnique<boost::unique_lock<typename std::decay<decltype(m)>::type>>(m, boost::defer_lock);
    if (lm->try_lock()) {
        return lm;
    } else {
        return nullptr;
    }
}

using __Lock2ReturnType__ = boost::optional<std::pair<
            std::unique_ptr<boost::unique_lock<CCriticalSection>>,
            std::unique_ptr<boost::unique_lock<CCriticalSection>>
            >
        >;

template <typename M1, typename M2>
auto _trylock2_internal(M1&& m1, M2&& m2) -> __Lock2ReturnType__ {
    auto res = __Lock2ReturnType__(std::make_pair(
        __InternalSyncMakeUnique<boost::unique_lock<typename std::decay<decltype(m1)>::type>>(m1, boost::defer_lock),
        __InternalSyncMakeUnique<boost::unique_lock<typename std::decay<decltype(m2)>::type>>(m2, boost::defer_lock)));
    if (boost::try_lock(*res->first, *res->second)) {
        return res;
    } else {
        return boost::none;
    }
}

using __Lock4ReturnType__ = boost::optional<std::tuple<
            std::unique_ptr<boost::unique_lock<CCriticalSection>>,
            std::unique_ptr<boost::unique_lock<CCriticalSection>>,
            std::unique_ptr<boost::unique_lock<CCriticalSection>>,
            std::unique_ptr<boost::unique_lock<CCriticalSection>>
            >
        >;

// Unfortunately, no variadic templates gymnastics until C++17...
// we need std::apply to avoid having a function for every number of locks
template <typename M1, typename M2, typename M3, typename M4>
auto _trylock4_internal(M1&& m1, M2&& m2, M3&& m3, M4&& m4) -> __Lock4ReturnType__
{
    auto res = __Lock4ReturnType__(std::make_tuple(
        __InternalSyncMakeUnique<boost::unique_lock<typename std::decay<decltype(m1)>::type>>(m1, boost::defer_lock),
        __InternalSyncMakeUnique<boost::unique_lock<typename std::decay<decltype(m2)>::type>>(m2, boost::defer_lock),
        __InternalSyncMakeUnique<boost::unique_lock<typename std::decay<decltype(m2)>::type>>(m3, boost::defer_lock),
        __InternalSyncMakeUnique<boost::unique_lock<typename std::decay<decltype(m3)>::type>>(m4, boost::defer_lock)));
    if (boost::try_lock(*std::get<0>(*res),
                        *std::get<1>(*res),
                        *std::get<2>(*res),
                        *std::get<3>(*res))) {
        return res;
    } else {
        return boost::none;
    }
}

#define LOCK(cs) boost::lock_guard<decltype(cs)> __lockguard__(cs)
#define LOCKN(cs, name) boost::lock_guard<decltype(cs)> name(cs)
#define LOCK2(cs1, cs2) auto __lockguard2__ = _lock2_internal(cs1, cs2)
#define LOCK2N(cs1, cs2, name) auto name = _lock2_internal(cs1, cs2)
#define TRY_LOCK(cs, name) auto name = _trylock_internal(cs)
#define TRY_LOCK2(cs1, cs2, name) auto name = _trylock2_internal(cs1, cs2)
#define TRY_LOCK4(cs1, cs2, cs3, cs4, name) auto name = _trylock4_internal(cs1, cs2, cs3, cs4)

#define ENTER_CRITICAL_SECTION(cs) \
    { \
        EnterCritical(#cs, __FILE__, __LINE__, (void*)(&cs)); \
        (cs).lock(); \
    }

#define LEAVE_CRITICAL_SECTION(cs) \
    { \
        (cs).unlock(); \
        LeaveCritical(); \
    }

class CSemaphore
{
private:
    boost::condition_variable condition;
    boost::mutex mutex;
    int value;

public:
    CSemaphore(int init) : value(init) {}

    void wait() {
        boost::unique_lock<boost::mutex> lock(mutex);
        while (value < 1) {
            condition.wait(lock);
        }
        value--;
    }

    bool try_wait() {
        boost::unique_lock<boost::mutex> lock(mutex);
        if (value < 1)
            return false;
        value--;
        return true;
    }

    void post() {
        {
            boost::unique_lock<boost::mutex> lock(mutex);
            value++;
        }
        condition.notify_one();
    }
};

/** RAII-style semaphore lock */
class CSemaphoreGrant
{
private:
    boost::atomic<CSemaphore*> sem;
    boost::atomic_bool fHaveGrant;

public:
    void Acquire() {
        if (fHaveGrant)
            return;
        sem.load()->wait();
        fHaveGrant = true;
    }

    void Release() {
        if (!fHaveGrant)
            return;
        sem.load()->post();
        fHaveGrant = false;
    }

    bool TryAcquire() {
        if (!fHaveGrant && sem.load()->try_wait())
            fHaveGrant = true;
        return fHaveGrant;
    }

    void MoveTo(CSemaphoreGrant &grant) {
        grant.Release();
        {
            grant.sem = sem.load();
        }
        grant.fHaveGrant = fHaveGrant.load();
        sem = NULL;
        fHaveGrant = false;
    }

    CSemaphoreGrant() : sem(NULL), fHaveGrant(false) {}

    CSemaphoreGrant(CSemaphore &sema, bool fTry = false) : sem(&sema), fHaveGrant(false) {
        if (fTry)
            TryAcquire();
        else
            Acquire();
    }

    ~CSemaphoreGrant() {
        Release();
    }

    operator bool() {
        return fHaveGrant;
    }
};
#endif

