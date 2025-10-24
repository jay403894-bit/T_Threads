#pragma once
#define NOMINMAX
#include <list>
#include <thread>
#include <condition_variable>
#include <vector>
#include <mutex>
#include <queue>
#include <algorithm>
#include <unordered_set>
#include "T_Thread.h"
#include "TaskQueue.h"
#include "Task.h"
#include "../Utilities/Clock.h"

constexpr size_t MAX_PRIORITY_BINS = 8; // or whatever your maximum is

//a periodic task_ 
struct PeriodicTask {
    std::shared_ptr<BaseTask> task_; //task pointer
    float scheduledTime;  // When to run the task_ next (in seconds)
    float interval;             // Interval in seconds
    std::shared_ptr<Clock> clock;  // Pointer to the clock
    bool cancelled = false; //task canceled flag
    PriorityLevel priority_ = PriorityLevel::NORMAL;
    // Default constructor
    PeriodicTask();
    // Parameterized constructor for initializing the task_ with interval and game timer
    PeriodicTask(const std::shared_ptr<BaseTask>& task_, float interval_, const std::shared_ptr<Clock>& timer);
    // Check if it's time to run the task_ based on TotalTime and interval
    bool IsTimeToRun() const;
    // Update the next execution time
    void UpdateExecutionTime();
};
struct DelayedTask {
    std::shared_ptr<BaseTask> task_; //task pointer
    float scheduledTime; //the scheduled delay time
    std::shared_ptr<Clock> clock; //pointer to the clock
    bool executed = false; //executed flag
    PriorityLevel priority_ = PriorityLevel::NORMAL;
    //default constructor
    DelayedTask();
    // Constructor for a delayed / one-shot task
    DelayedTask(const std::shared_ptr<BaseTask>& task, float delayMS, const std::shared_ptr<Clock>& timer)
        : task_(task), clock(timer) {
        scheduledTime = clock->ElapsedMS() + delayMS;
    }
    // Returns true if it’s time to run the task
    bool IsTimeToRun() const {
        return !executed && (clock->ElapsedMS() >= scheduledTime);
    }
    //mark task executed
    void MarkExecuted() { executed = true; }
};

struct DelayedTaskCompare {
    bool operator()(const DelayedTask& a, const DelayedTask& b) const {
        return a.scheduledTime > b.scheduledTime; // min-heap: top = earliest time
    }
};
struct PeriodicTaskCompare {
    bool operator()(const PeriodicTask& a, const PeriodicTask& b) const {
        return a.scheduledTime > b.scheduledTime; // min-heap
    }
};
struct PriorityCompare {
    bool operator()(const std::shared_ptr<BaseTask>& a,
        const std::shared_ptr<BaseTask>& b) const {
        return a->GetPriority() > b->GetPriority(); // min-heap
    }
};

struct TaskCandidate {
    std::shared_ptr<BaseTask> task;                                 // raw pointer cached
    enum class Source { None, PriorityBin, Delayed, Periodic, Fallback } src;
    size_t bin_index;
    std::string id;
    std::optional<DelayedTask> delayed_copy;
    std::optional<PeriodicTask> periodic_copy;
};
class TaskScheduler : public SharedQueues {
public:


    // Constructor 
    TaskScheduler();
    //destructor 
    ~TaskScheduler();
    //add a task_
    void AddTask(const std::shared_ptr<BaseTask>& task_, int cpuID = -1);
    // Add a periodic task_ that executes at fixed intervals
    void ScheduleTask(const std::shared_ptr<BaseTask>& task_, float interval, int cpuID = -1);
    //add a delayed task
    void ScheduleDelayedTask(const std::shared_ptr<BaseTask>& task, float delayMS,int cpuID = -1);
    //stop all threads and join the pool
    void Join();
    //stop a task_
    void StopTask(const std::string& id);
    //pause a task_
    void PauseTask(std::string id);
    //Resume a task_
    void ResumeTask(std::string id);
    //get the clock
    std::string TimeStamp();
    //start the thread pool
    void StartPool(unsigned int numWorkers=0);
private:
    void PushDelayed(const DelayedTask& t);
    void PushPeriodic(const PeriodicTask& t);
    bool PopNextDelayed(TaskCandidate& out);
    bool PopNextPeriodic(TaskCandidate& out);
    int PickWorkerForTask(const std::shared_ptr<BaseTask>& task); // Choose queue index
    std::vector<TaskCandidate> GetNextBatch(size_t maxBatch);
    //worker loop
    void Worker();
    //check if any task is ready
    bool AnyTaskReady();
    //get the next task
    std::shared_ptr<BaseTask> GetNextTask();
    std::atomic<int> nextWorker{ 0 }; // for round-robin
    std::vector<std::shared_ptr<T_Thread>> workers; // indexable
    std::vector<DelayedTask> delayedHeap;
    std::vector<PeriodicTask> periodicHeap;
    std::vector<std::vector<std::shared_ptr<BaseTask>>> priorityBins;

    std::unordered_map<std::string, PeriodicTask> scheduledTasks; //scheduled tasks mapped
    std::unordered_map<std::string, DelayedTask> delayedTasks; //delayed tasks mapped
    std::shared_ptr<Clock> clock;  // Add clock to track time
    std::mutex taskMutex; //task mutex
    std::condition_variable cv; // Condition variable for thread synchronization
    std::atomic<bool> stopFlag = false; // stop flag 
    std::thread workerThread; // Worker thread
    inline static bool constructed = false; //constructed flag to ensure singleton like behavior without the pattern

    std::vector<int> coreOccupied;
    std::mutex workerMutex;
    unsigned int numCores = 0;
    std::atomic<int> nextIndex{ -1 };
    std::queue<std::shared_ptr<BaseTask>> fallbackQueue;
    std::unordered_map<std::string, std::shared_ptr<BaseTask>> allTasks;
    std::unique_ptr<std::mutex> priorityBinMutexes[MAX_PRIORITY_BINS];
};
