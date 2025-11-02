#pragma once
#ifdef _WIN32
#include <windows.h>
#else
//implement posix later
#endif
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include "Task.h"
#include "SharedQueues.h"
#include "Epochs.h"

namespace T_Threads {
    inline thread_local Task* current_task = nullptr;

    class T_Thread :public SharedQueues {
    public:
        T_Thread();
        T_Thread(const T_Thread& other) = delete;
        T_Thread& operator=(const T_Thread& other) = delete;
        ~T_Thread();
        void startWorker(size_t cpu_affinity);
        std::thread::id getId();
        bool setImmediateTask(Task* task_);
        int getQueueLoad();
        void setQueueIndex(size_t index);
        void join();
        void notifyWorker();
        void pushTask();
        void popTask();
        bool allQueuesEmpty();
        bool ready();
#ifdef _WIN32
        //set cpu core affinity (singular)
        bool setAffinity(int cpu_id, int num_cores);
#else
        // Implement POSIX sched_setaffinity if cross-platform later
#endif
    private:
        //the worker function 
        void worker();

        static std::atomic<int> next_index_;
        std::atomic<int> queue_load_{ 0 };
        std::atomic<bool> immediate_{ false };
        std::atomic<bool> running_{ false };
        std::atomic<bool> ready_{ false };
        std::atomic<bool> joining_{ false };
        int queue_index_ = 0;
        std::mutex worker_mutex_;
        std::mutex join_mutex_;
        std::condition_variable cv_worker_done_;
        std::condition_variable cv_;
        std::condition_variable cv_affinity_;
        Task* task_ = nullptr;
        Task* immediate_task_ = nullptr;
        std::thread thread_;
        std::thread::native_handle_type native_handle_;
        std::vector<Task*> local_queue_;
        std::vector<Task*> overflow_;
#ifdef _WIN32
        DWORD_PTR mask_ = 0;
#else
        // Implement POSIX sched_setaffinity if cross-platform later
#endif

    };
};