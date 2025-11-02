#pragma once
#include <atomic>
#include <vector>
#include <optional>
#include <stdexcept>
#include "Task.h"
/// <summary>
/// powers of 2 compatible 
/// typical sizes - 1024-4096 for small tasks high frequency
/// medium/mixed 8192 to 16384
/// large/coarse 32768
/// </summary>
namespace T_Threads {
    class TaskDeque {
    public:
        explicit TaskDeque(size_t capacity = 32768)
            : buffer_(capacity, nullptr),
            mask_(capacity - 1),
            top_(0),
            bottom_(0),
            capacity_(capacity)
        {
            if ((capacity & (capacity - 1)) != 0) {
                throw std::runtime_error("Capacity must be a power of 2");
            }
        }

        bool push_bottom(Task* item) {
            size_t b = bottom_.load(std::memory_order_relaxed);
            buffer_[b & mask_] = item;
            std::atomic_thread_fence(std::memory_order_release);
            bottom_.store(b + 1, std::memory_order_release);
            return true;
        }

        std::optional<Task*> pop_bottom() {
            size_t b = bottom_.load(std::memory_order_relaxed);
            if (b == 0) return std::nullopt;

            b = b - 1;
            bottom_.store(b, std::memory_order_relaxed);

            std::atomic_thread_fence(std::memory_order_seq_cst);
            size_t t = top_.load(std::memory_order_acquire);

            if (t <= b) {
                Task* item = buffer_[b & mask_];
                buffer_[b & mask_] = nullptr;

                if (t == b) {
                    if (!top_.compare_exchange_strong(t, t + 1,
                        std::memory_order_seq_cst, std::memory_order_relaxed)) {
                        bottom_.store(b + 1, std::memory_order_relaxed);
                        return std::nullopt;
                    }
                    bottom_.store(t + 1, std::memory_order_relaxed);
                }
                return item;
            }
            else {
                bottom_.store(t, std::memory_order_relaxed);
                return std::nullopt;
            }
        }

        std::optional<Task*> steal() {
            size_t t = top_.load(std::memory_order_acquire);
            std::atomic_thread_fence(std::memory_order_seq_cst);
            size_t b = bottom_.load(std::memory_order_acquire);

            if (t < b) {
                Task* item = buffer_[t & mask_];
                if (!top_.compare_exchange_strong(t, t + 1,
                    std::memory_order_seq_cst, std::memory_order_relaxed)) {
                    return std::nullopt;
                }
                return item;
            }
            return std::nullopt;
        }

        bool empty() const {
            size_t t = top_.load(std::memory_order_acquire);
            size_t b = bottom_.load(std::memory_order_acquire);
            return t >= b;
        }

        size_t size() const {
            size_t t = top_.load(std::memory_order_acquire);
            size_t b = bottom_.load(std::memory_order_acquire);
            return (b >= t) ? (b - t) : 0;
        }

        size_t capacity()const {
            return capacity_;
        }
    private:
        std::vector<Task*> buffer_;
        size_t mask_;
        std::atomic<size_t> top_;
        std::atomic<size_t> bottom_;
        size_t capacity_;
    };
};