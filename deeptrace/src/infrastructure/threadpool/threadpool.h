#pragma once
// Minimal fixed-worker thread pool (v2.12.0). Standard library only.
// Used to parallelize the pointer-chain memory scan across memory chunks.

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace deeptrace::internal {

class ThreadPool {
public:
    // worker_count = 0 -> hardware_concurrency (>= 1).
    explicit ThreadPool(size_t worker_count = 0);
    ~ThreadPool();  // joins all workers

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Enqueue a task; safe to call from any thread. The task runs on a worker
    // when one is free. wait() must be called before destructor.
    void enqueue(std::function<void()> task);

    // Block until all queued tasks have completed. Idempotent.
    void wait();

    size_t workers() const { return workers_.size(); }

private:
    void worker_loop();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_work_;
    std::condition_variable cv_done_;
    size_t pending_ = 0;   // queued + running
    bool stop_ = false;
};

}  // namespace deeptrace::internal
