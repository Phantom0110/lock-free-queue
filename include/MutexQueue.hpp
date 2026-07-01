#pragma once

#include <queue>
#include <mutex>
#include <optional>

// ============================================================================
// MutexQueue<T> -- Thread-safe queue using std::mutex.
// Used as the correctness and performance baseline against LockFreeQueue
// ============================================================================

template <typename T>
class MutexQueue {
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;

public:
    void enqueue(T value) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(value));
    }

    std::optional<T> dequeue() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return std::nullopt;
        T val = std::move(queue_.front());
        queue_.pop();
        return val;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
};
