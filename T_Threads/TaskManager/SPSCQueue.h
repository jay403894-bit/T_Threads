#pragma once
#include <atomic>
#include <optional>
#include <array>

template <size_t Capacity>
class SPSCQueue {
public:
    SPSCQueue() : head(0), tail(0) {}

    bool push(const std::shared_ptr<BaseTask>& item) {
        size_t h = head.load(std::memory_order_relaxed);
        size_t next = (h + 1) & (Capacity - 1);
        if (next == tail.load(std::memory_order_acquire)) {
            return false; // full
        }
        buffer[h] = item;
        head.store(next, std::memory_order_release);
        return true;
    }

    std::optional<std::shared_ptr<BaseTask>> pop() {
        size_t t = tail.load(std::memory_order_relaxed);
        if (t == head.load(std::memory_order_acquire)) {
            return std::nullopt; // empty
        }
        auto item = buffer[t];
        tail.store((t + 1) & (Capacity - 1), std::memory_order_release);
        return item;
    }

    bool empty() const {
        return head.load(std::memory_order_acquire) == tail.load(std::memory_order_acquire);
    }

    bool full() const {
        return ((head.load(std::memory_order_acquire) + 1) & (Capacity - 1)) == tail.load(std::memory_order_acquire);
    }

    void clear() {
        head_ = tail_ = 0;
    }

private:
    std::array<std::shared_ptr<BaseTask>, Capacity> buffer;
    std::atomic<size_t> head;
    std::atomic<size_t> tail;
};