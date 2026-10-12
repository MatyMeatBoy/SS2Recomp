#pragma once
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace ss2::online {
// Ordered, bounded persistence. Jobs own immutable bytes and paths, never a
// live simulation. Disk flushes must not run on the authority's frame thread.
class StorageWorker {
    struct Job { std::size_t bytes; std::function<void()> write; };
    std::mutex mutex_;
    std::condition_variable wake_, drained_;
    std::deque<Job> jobs_;
    std::size_t bytes_=0;
    bool stopping_=false, busy_=false;
    std::exception_ptr error_;
    std::thread thread_;
    void run() {
        std::unique_lock lock(mutex_);
        for (;;) {
            wake_.wait(lock,[&]{return stopping_ || !jobs_.empty();});
            if (jobs_.empty()) return;
            auto job=std::move(jobs_.front()); jobs_.pop_front(); busy_=true;
            lock.unlock();
            std::exception_ptr failure;
            try { job.write(); } catch (...) { failure=std::current_exception(); }
            lock.lock(); bytes_-=job.bytes; busy_=false;
            if (failure) { error_=failure; jobs_.clear(); bytes_=0; stopping_=true; }
            drained_.notify_all();
        }
    }
public:
    StorageWorker() : thread_([this]{run();}) {}
    StorageWorker(const StorageWorker&)=delete;
    StorageWorker& operator=(const StorageWorker&)=delete;
    ~StorageWorker() {
        { std::lock_guard lock(mutex_); stopping_=true; }
        wake_.notify_one(); thread_.join();
    }
    void check() {
        std::lock_guard lock(mutex_);
        if (error_) std::rethrow_exception(error_);
    }
    void submit(std::size_t bytes, std::function<void()> write) {
        std::lock_guard lock(mutex_);
        if (error_) std::rethrow_exception(error_);
        if (stopping_ || jobs_.size()>=512 || bytes>16*1024*1024-bytes_)
            throw std::runtime_error("recovery persistence backlog exceeded its bounded budget");
        jobs_.push_back({bytes,std::move(write)}); bytes_+=bytes;
        wake_.notify_one();
    }
    void flush() {
        std::unique_lock lock(mutex_);
        drained_.wait(lock,[&]{return error_ || (jobs_.empty() && !busy_);});
        if (error_) std::rethrow_exception(error_);
    }
};
}
