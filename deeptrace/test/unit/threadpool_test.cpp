#include "infrastructure/threadpool/threadpool.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

using deeptrace::internal::ThreadPool;

TEST(ThreadPool, AllTasksRun) {
    ThreadPool pool(4);
    std::atomic<int> count{0};
    for (int i = 0; i < 200; ++i) {
        pool.enqueue([&count] { count.fetch_add(1); });
    }
    pool.wait();
    EXPECT_EQ(count.load(), 200);
}

TEST(ThreadPool, WaitIdempotent) {
    ThreadPool pool(2);
    std::atomic<int> count{0};
    pool.enqueue([&count] { count.fetch_add(1); });
    pool.wait();
    pool.wait();  // no pending tasks -> returns immediately
    EXPECT_EQ(count.load(), 1);
}

TEST(ThreadPool, WorkerCountExplicitAndFallback) {
    ThreadPool p4(4);
    EXPECT_EQ(p4.workers(), 4u);
    // 0 -> hardware_concurrency (>= 1)
    ThreadPool p0(0);
    EXPECT_GE(p0.workers(), 1u);
}

TEST(ThreadPool, EnqueueAfterWaitReuse) {
    // mirrors the pointer-map walk: enqueue -> wait -> enqueue again
    ThreadPool pool(3);
    std::atomic<int> count{0};
    for (int i = 0; i < 50; ++i) pool.enqueue([&count] { count.fetch_add(1); });
    pool.wait();
    for (int i = 0; i < 50; ++i) pool.enqueue([&count] { count.fetch_add(1); });
    pool.wait();
    EXPECT_EQ(count.load(), 100);
}

TEST(ThreadPool, ConcurrentResultCollection) {
    ThreadPool pool(4);
    std::atomic<int> sum{0};
    for (int i = 0; i < 64; ++i) {
        pool.enqueue([&sum, i] { sum.fetch_add(i); });
    }
    pool.wait();
    EXPECT_EQ(sum.load(), 64 * 63 / 2);
}
