#include <atomic>
#include <memory>
#include <vector>
#include <optional>
#include <mutex>
#include <queue>
#include "Task.h"
#include <stdexcept>
/*/
template <typename T>
class ChaseLevDeque {
public:
    ChaseLevDeque(size_t capacity = 1024)
        : buffer(capacity), mask(capacity - 1), bottom(0), top(0) {
        if ((capacity & (capacity - 1)) != 0) {
            throw std::runtime_error("Capacity must be a power of 2");
        }
    }

    // Push from owner thread
    bool push_bottom(const std::shared_ptr<BaseTask>& item) {
        size_t b = bottom.load(std::memory_order_relaxed);
        size_t t = top.load(std::memory_order_acquire);

        if (b - t >= buffer.size()) {
            // queue full
            return false;
        }

        // write slot first
        buffer[b & mask] = item;

        // ensure the write to buffer is visible before we publish bottom
        std::atomic_thread_fence(std::memory_order_release);

        // publish new bottom
        bottom.store(b + 1, std::memory_order_relaxed);
        
        return true;
    }

    std::optional<T> pop_bottom() {
        size_t b = bottom.load(std::memory_order_relaxed);
        if (b == 0) return std::nullopt; // avoid underflow

        b = b - 1; // tentative bottom index
        bottom.store(b, std::memory_order_relaxed);

        // full memory barrier before reading top
        std::atomic_thread_fence(std::memory_order_seq_cst);
        size_t t = top.load(std::memory_order_acquire);

        if (t <= b) {
            auto item = buffer[b & mask];
            buffer[b & mask] = nullptr; // clear slot

            if (t == b) {
                // last element, compete with stealers
                // try to move top forward to indicate removal
                if (!top.compare_exchange_strong(t, t + 1,
                    std::memory_order_seq_cst, std::memory_order_relaxed)) {
                    // lost race: a stealer took it
                    bottom.store(b + 1, std::memory_order_relaxed);
                    //std::cout << "[pop_bottom] lost race, returning null\n";
                    return std::nullopt;
                }

                // success: we've removed the last element; set bottom = t+1 (empty)
                bottom.store(t + 1, std::memory_order_relaxed);
            }

            return item;
        }
        else {
            // empty
            bottom.store(t, std::memory_order_relaxed);
            return std::nullopt;
        }
    }

    std::optional<T> steal() {
        size_t t = top.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        size_t b = bottom.load(std::memory_order_acquire);

        if (t < b) {
            auto item = buffer[t & mask];
            if (!top.compare_exchange_strong(t, t + 1,
                std::memory_order_seq_cst, std::memory_order_relaxed)) {
                return std::nullopt;
            }
            return item;
        }
        return std::nullopt;
    }

    bool empty() const {
        size_t t = top.load(std::memory_order_acquire);
        size_t b = bottom.load(std::memory_order_acquire);
        return t >= b;
    }

    size_t size() const {
        size_t t = top.load(std::memory_order_acquire);
        size_t b = bottom.load(std::memory_order_acquire);
        return b >= t ? b - t : 0; // normally b >= t in this implementation
    }
private:
    std::vector<T> buffer;
    size_t mask;
    std::atomic<size_t> bottom;   // owner index (modified by owner thread)
    std::atomic<size_t> top;      // thief index (modified by stealers)
};
*/

#pragma once
#include <vector>
#include <atomic>
#include <optional>
#include <stdexcept>
#include <sstream>

template <typename T>
class ChaseLevDeque {
public:
    explicit ChaseLevDeque(size_t capacity = 1024)
        : buffer(capacity), mask(capacity - 1), top(0), bottom(0) {
        if ((capacity & (capacity - 1)) != 0) {
            throw std::runtime_error("Capacity must be a power of 2");
        }
    }

    // Push from owner thread (fast-path, single-producer)
    bool push_bottom(const T& item) {
        size_t b = bottom.load(std::memory_order_relaxed);
        size_t t = top.load(std::memory_order_acquire);

        buffer[b & mask] = item;

        // ensure the store to buffer is visible before we publish bottom
        std::atomic_thread_fence(std::memory_order_release);

        bottom.store(b + 1, std::memory_order_release);
        return true;
    }

    // Pop from owner (may race with stealers)
    std::optional<T> pop_bottom() {
        size_t b = bottom.load(std::memory_order_relaxed);
        if (b == 0) {
            // empty
            return std::nullopt;
        }

        b = b - 1;
        bottom.store(b, std::memory_order_relaxed);

        // full barrier before reading top to observe concurrent steals
        std::atomic_thread_fence(std::memory_order_seq_cst);
        size_t t = top.load(std::memory_order_acquire);

        if (t <= b) {
            T item = buffer[b & mask];
            buffer[b & mask] = T(); // clear slot if T supports it

            if (t == b) {
                // last element: compete with stealers
                if (!top.compare_exchange_strong(t, t + 1,
                    std::memory_order_seq_cst, std::memory_order_relaxed)) {
                    // lost race: a stealer stole the item
                    bottom.store(b + 1, std::memory_order_relaxed);
                    return std::nullopt;
                }
                // success: we removed last element; keep invariant bottom == top == t+1
                bottom.store(t + 1, std::memory_order_relaxed);
            }

            return item;
        }
        else {
            // empty, restore bottom to top
            bottom.store(t, std::memory_order_relaxed);
            return std::nullopt;
        }
    }

    // Steal from other threads (concurrent)
    std::optional<T> steal() {
        size_t t = top.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        size_t b = bottom.load(std::memory_order_acquire);

        if (t < b) {
            T item = buffer[t & mask];
            if (!top.compare_exchange_strong(t, t + 1,
                std::memory_order_seq_cst, std::memory_order_relaxed)) {
                // lost race
                return std::nullopt;
            }
            return item;
        }
        return std::nullopt;
    }

    bool empty() const {
        size_t t = top.load(std::memory_order_acquire);
        size_t b = bottom.load(std::memory_order_acquire);
        return t >= b;
    }

    size_t size() const {
        size_t t = top.load(std::memory_order_acquire);
        size_t b = bottom.load(std::memory_order_acquire);
        return (b >= t) ? (b - t) : 0;
    }

private:
    std::vector<T> buffer;
    size_t mask;
    std::atomic<size_t> top;
    std::atomic<size_t> bottom;
};