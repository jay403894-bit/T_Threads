#include <atomic>
#include <memory>
#include <vector>
#include <optional>
#include <mutex>
#include <queue>
#include "Task.h"
#include <stdexcept>
#include <sstream>

template <typename T>
class ChaseLevDeque {
public:
    explicit ChaseLevDeque(size_t capacity = 1024)
        : buffer(capacity), mask_(capacity - 1), top(0), bottom(0) {
        if ((capacity & (capacity - 1)) != 0) {
            throw std::runtime_error("Capacity must be a power of 2");
        }
    }

    // Push from owner thread (fast-path, single-producer)
    bool push_bottom(const T& item) {
        size_t b = bottom.load(std::memory_order_relaxed);
        size_t t = top.load(std::memory_order_acquire);

        buffer[b & mask_] = item;

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
            T item = buffer[b & mask_];
            buffer[b & mask_] = T(); // clear slot if T supports it

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
            T item = buffer[t & mask_];
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
    size_t mask_;
    std::atomic<size_t> top;
    std::atomic<size_t> bottom;
};