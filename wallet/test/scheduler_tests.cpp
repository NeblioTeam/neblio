// Copyright (c) 2012-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "googletest/googletest/include/gtest/gtest.h"

#include "environment.h"

#include "scheduler.h"
#include "util.h"

#include <boost/thread.hpp>
#include <chrono>
#include <random>
#include <thread>

static void microTask(CScheduler& s, boost::mutex& mutex, int& counter, int delta,
                      boost::chrono::system_clock::time_point rescheduleTime)
{
    {
        boost::unique_lock<boost::mutex> lock(mutex);
        counter += delta;
    }
    boost::chrono::system_clock::time_point noTime = boost::chrono::system_clock::time_point::min();
    if (rescheduleTime != noTime) {
        CScheduler::Function f =
            std::bind(&microTask, std::ref(s), std::ref(mutex), std::ref(counter), -delta + 1, noTime);
        s.schedule(f, rescheduleTime);
    }
}

static void MicroSleep(uint64_t n) { std::this_thread::sleep_for(std::chrono::microseconds(n)); }

TEST(scheduler_tests, manythreads)
{
    // Stress test: hundreds of microsecond-scheduled tasks,
    // serviced by 10 threads.
    //
    // So... ten shared counters, which if all the tasks execute
    // properly will sum to the number of tasks done.
    // Each task adds or subtracts a random amount from one of the
    // counters, and then schedules another task 0-1000
    // microseconds in the future to subtract or add from
    // the counter -random_amount+1, so in the end the shared
    // counters should sum to the number of initial tasks performed.
    CScheduler microTasks;

    boost::mutex counterMutex[10];
    int          counter[10] = {0};
    auto         zeroToNine  = []() -> int { return insecure_rand() % 10; };              // [0, 9]
    auto         randomMsec  = []() -> int { return -11 + (int)insecure_rand() % 1012; }; // [-11, 1000]
    auto         randomDelta = []() -> int { return -1000 + insecure_rand() % 2001; }; // [-1000, 1000]

    boost::chrono::system_clock::time_point start = boost::chrono::system_clock::now();
    boost::chrono::system_clock::time_point now   = start;
    boost::chrono::system_clock::time_point first, last;
    size_t                                  nTasks = microTasks.getQueueInfo(first, last);
    EXPECT_TRUE(nTasks == 0);

    for (int i = 0; i < 100; ++i) {
        boost::chrono::system_clock::time_point t = now + boost::chrono::microseconds(randomMsec());
        boost::chrono::system_clock::time_point tReschedule =
            now + boost::chrono::microseconds(500 + randomMsec());
        int                  whichCounter = zeroToNine();
        CScheduler::Function f =
            std::bind(&microTask, std::ref(microTasks), std::ref(counterMutex[whichCounter]),
                      std::ref(counter[whichCounter]), randomDelta(), tReschedule);
        microTasks.schedule(f, t);
    }
    nTasks = microTasks.getQueueInfo(first, last);
    EXPECT_TRUE(nTasks == 100);
    EXPECT_TRUE(first < last);
    EXPECT_TRUE(last > now);

    // As soon as these are created they will start running and servicing the queue
    boost::thread_group microThreads;
    for (int i = 0; i < 5; i++)
        microThreads.create_thread(std::bind(&CScheduler::serviceQueue, &microTasks));

    MicroSleep(600);
    now = boost::chrono::system_clock::now();

    // More threads and more tasks:
    for (int i = 0; i < 5; i++)
        microThreads.create_thread(std::bind(&CScheduler::serviceQueue, &microTasks));
    for (int i = 0; i < 100; i++) {
        boost::chrono::system_clock::time_point t = now + boost::chrono::microseconds(randomMsec());
        boost::chrono::system_clock::time_point tReschedule =
            now + boost::chrono::microseconds(500 + randomMsec());
        int                  whichCounter = zeroToNine();
        CScheduler::Function f =
            std::bind(&microTask, std::ref(microTasks), std::ref(counterMutex[whichCounter]),
                      std::ref(counter[whichCounter]), randomDelta(), tReschedule);
        microTasks.schedule(f, t);
    }

    // Drain the task queue then exit threads
    microTasks.stop(true);
    microThreads.join_all(); // ... wait until all the threads are done

    int counterSum = 0;
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(counter[i] != 0);
        counterSum += counter[i];
    }
    EXPECT_EQ(counterSum, 200);
}

TEST(scheduler_tests, singlethreadedscheduler_ordered)
{
    CScheduler scheduler;

    // each queue should be well ordered with respect to itself but not other queues
    SingleThreadedSchedulerClient queue1(&scheduler);
    SingleThreadedSchedulerClient queue2(&scheduler);

    // create more threads than queues
    // if the queues only permit execution of one task at once then
    // the extra threads should effectively be doing nothing
    // if they don't we'll get out of order behaviour
    boost::thread_group threads;
    for (int i = 0; i < 5; ++i) {
        threads.create_thread(std::bind(&CScheduler::serviceQueue, &scheduler));
    }

    // these are not atomic, if SinglethreadedSchedulerClient prevents
    // parallel execution at the queue level no synchronization should be required here
    int counter1 = 0;
    int counter2 = 0;

    // just simply count up on each queue - if execution is properly ordered then
    // the callbacks should run in exactly the order in which they were enqueued
    for (int i = 0; i < 100; ++i) {
        queue1.AddToProcessQueue([i, &counter1]() {
            bool expectation = i == counter1++;
            assert(expectation);
        });

        queue2.AddToProcessQueue([i, &counter2]() {
            bool expectation = i == counter2++;
            assert(expectation);
        });
    }

    // finish up
    scheduler.stop(true);
    threads.join_all();

    EXPECT_EQ(counter1, 100);
    EXPECT_EQ(counter2, 100);
}
