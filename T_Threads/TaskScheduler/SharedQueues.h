#pragma once
#include "Task.h"
#include "TaskDeque.h"
#include "MPSCQueue.h"
#include "../ThirdParty/concurrentqueue.h"
#include <array>
namespace T_Threads {
    struct QTraits : moodycamel::ConcurrentQueueDefaultTraits {
        static constexpr size_t BLOCK_SIZE = 32768; //32768
        static constexpr size_t IMPLICIT_INITIAL_INDEX_SIZE = 1024;
        //32k * 1024 = ~ 32,768,000 tasks at once
    };

    class SharedQueues {
    public:
        static inline std::atomic<bool> paused_{ false };
        static inline std::vector<std::unique_ptr<TaskDeque>> thread_queues_;
        static inline std::vector<std::unique_ptr<MPSCQueue<Task*>>> inboxes_;
        static inline std::vector<std::unique_ptr<MPSCQueue<Task*>>> inboxes_local_;
        static inline std::array <moodycamel::ConcurrentQueue<Task*, QTraits>, 5 > priority_queue_;
    };
};