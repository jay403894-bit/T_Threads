#pragma once
#define NOMINMAX
#include <thread>
#include <condition_variable>
#include <vector>
#include <mutex>
#include <queue>
#include <algorithm>
#include "T_Thread.h"
#include "TaskQueue.h"
#include "Task.h"
#include "../Utilities/Clock.h"

constexpr size_t MAX_PRIORITY_BINS = 8;

struct PeriodicTask {
    std::shared_ptr<BaseTask> task_; 
    float scheduledTime;  
    float interval;             
    std::shared_ptr<Clock> clock_;  
    bool cancelled_ = false; 
    PriorityLevel priority_ = PriorityLevel::NORMAL;
    PeriodicTask();
    PeriodicTask(const std::shared_ptr<BaseTask>& task_, float interval_, const std::shared_ptr<Clock>& timer);
    bool isTimeToRun() const;
    void updateExecutionTime();
};
struct DelayedTask {
    std::shared_ptr<BaseTask> task_; 
    float scheduledTime; 
    std::shared_ptr<Clock> clock_; 
    bool executed = false; 
    PriorityLevel priority_ = PriorityLevel::NORMAL;
    DelayedTask();
    DelayedTask(const std::shared_ptr<BaseTask>& task_, float delayMS, const std::shared_ptr<Clock>& timer)
        : task_(task_), clock_(timer) {
        scheduledTime = clock_->elapsedMs() + delayMS;
    }
    bool isTimeToRun() const {
        return !executed && (clock_->elapsedMs() >= scheduledTime);
    }
    void markExecuted() { executed = true; }
};

struct DelayedTaskCompare {
    bool operator()(const DelayedTask& a, const DelayedTask& b) const {
        return a.scheduledTime > b.scheduledTime; 
    }
};
struct PeriodicTaskCompare {
    bool operator()(const PeriodicTask& a, const PeriodicTask& b) const {
        return a.scheduledTime > b.scheduledTime; 
    }
};
struct PriorityCompare {
    bool operator()(const std::shared_ptr<BaseTask>& a,
        const std::shared_ptr<BaseTask>& b) const {
        return a->getPriority() > b->getPriority(); 
    }
};
struct TaskCandidate {
    std::shared_ptr<BaseTask> task_;                                 
    enum class Source { None, PriorityBin, Delayed, Periodic, Fallback } src_;
    size_t bin_index_;
    std::string id_;
    std::optional<DelayedTask> delayed_copy_;
    std::optional<PeriodicTask> periodic_copy_;
};
class TaskScheduler : public SharedQueues {
public:
    TaskScheduler();
    ~TaskScheduler();
    void addTask(const std::shared_ptr<BaseTask>& task_, int cpuID = -1);
    void scheduleTask(const std::shared_ptr<BaseTask>& task_, float interval, int cpuID = -1);
    void scheduleDelayedTask(const std::shared_ptr<BaseTask>& task_, float delayMS,int cpuID = -1);
    void join();
    void stopTask(const std::string& id_);
    void pauseTask(std::string id_);
    void resumeTask(std::string id_);
    std::string timeStamp();
    void startPool(unsigned int numWorkers=0);
private:
    void pushDelayed(const DelayedTask& t);
    void pushPeriodic(const PeriodicTask& t);
    bool popNextDelayed(TaskCandidate& out);
    bool popNextPeriodic(TaskCandidate& out);
    int pickWorkerForTask(const std::shared_ptr<BaseTask>& task_);
    std::vector<TaskCandidate> getNextBatch(size_t maxBatch);
    void worker();
    bool anyTaskReady();   

    inline static std::atomic<bool> constructed_{ false };
    std::atomic<int> next_worker_{ 0 }; 
    std::atomic<bool> stop_flag_ = false; 
    std::atomic<int> next_index_{ -1 };
    std::vector<std::shared_ptr<T_Thread>> workers_; 
    std::vector<DelayedTask> delayed_heap_;
    std::vector<PeriodicTask> periodic_heap_;
    std::vector<std::vector<std::shared_ptr<BaseTask>>> priority_bins_;
    std::vector<int> core_occupied_;
    std::queue<std::shared_ptr<BaseTask>> fallback_queue_;
    std::unordered_map<std::string, PeriodicTask> scheduled_tasks_; 
    std::unordered_map<std::string, DelayedTask> delayed_tasks_;
    std::unordered_map<std::string, std::shared_ptr<BaseTask>> task_map_;
    std::shared_ptr<Clock> clock_;
    std::condition_variable cv_;
    std::thread worker_thread_; 
    std::mutex task_mutex_; 
    std::mutex worker_mutex_;
    std::unique_ptr<std::mutex> priority_bin_mutexes_[MAX_PRIORITY_BINS];
};
