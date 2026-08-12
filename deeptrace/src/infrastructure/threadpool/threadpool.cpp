#include "infrastructure/threadpool/threadpool.h"

#include <thread>

namespace deeptrace::internal {

ThreadPool::ThreadPool(size_t worker_count) {
    size_t n = worker_count;
    if (n == 0) {
        unsigned hw = std::thread::hardware_concurrency();
        n = hw > 0 ? static_cast<size_t>(hw) : 1;
    }
    workers_.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        workers_.emplace_back([this] { worker_loop(); });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        stop_ = true;
    }
    cv_work_.notify_all();
    for (auto& w : workers_) w.join();
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lk(mutex_);
        tasks_.push(std::move(task));
        ++pending_;
    }
    cv_work_.notify_one();
}

void ThreadPool::wait() {
    std::unique_lock<std::mutex> lk(mutex_);
    cv_done_.wait(lk, [this] { return pending_ == 0; });
}

void ThreadPool::worker_loop() {
    for (;;) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lk(mutex_);
            cv_work_.wait(lk, [this] { return stop_ || !tasks_.empty(); });
            if (stop_ && tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
        {
            std::lock_guard<std::mutex> lk(mutex_);
            --pending_;
        }
        cv_done_.notify_all();
    }
}

}  // namespace deeptrace::internal
