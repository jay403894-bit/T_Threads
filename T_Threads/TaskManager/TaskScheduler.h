#pragma once
#include <list>
#include <thread>
#include <condition_variable>
#include <vector>
#include <mutex>
#include "../Utilities/Logger.h"
#include "T_Thread.h"
#include "TaskQueue.h"
#include "Task.h"
#include "../Utilities/Clock.h"

class TaskScheduler : public TaskQueue {
public:
    //a periodic task_ 
    struct Periodic_Task {
        std::shared_ptr<BaseTask> task_; //task pointer
        float nextExecutionTime;  // When to run the task_ next (in seconds)
        float interval;             // Interval in seconds
        std::shared_ptr<Clock> clock;  // Pointer to the clock
        bool cancelled = false; //task canceled flag
        PriorityLevel priority_ = PriorityLevel::NORMAL;
        // Default constructor
        Periodic_Task();
        // Parameterized constructor for initializing the task_ with interval and game timer
        Periodic_Task(const std::shared_ptr<BaseTask>& task_, float interval_,const std::shared_ptr<Clock>& timer);
        // Check if it's time to run the task_ based on TotalTime and interval
        bool IsTimeToRun() const;
        // Update the next execution time
        void UpdateExecutionTime();
    };
    struct Delayed_Task {
        std::shared_ptr<BaseTask> task_; //task pointer
        float scheduledTime; //the scheduled delay time
        std::shared_ptr<Clock> clock; //pointer to the clock
        bool executed = false; //executed flag
        PriorityLevel priority_ = PriorityLevel::NORMAL;
        //default constructor
        Delayed_Task();
        // Constructor for a delayed / one-shot task
        Delayed_Task(const std::shared_ptr<BaseTask>& task, float delayMS,const std::shared_ptr<Clock>& timer)
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
    // Constructor 
    TaskScheduler();
    //destructor 
    ~TaskScheduler();
    //add a task_
    void AddTask(const std::shared_ptr<BaseTask>& task_, int cpuID = -1, bool isGroup = false);
    // Add a periodic task_ that executes at fixed intervals
    void ScheduleTask(const std::shared_ptr<BaseTask>& task_, float interval, int cpuID = -1, bool isGroup = false);
    //add a delayed task
    void ScheduleDelayedTask(const std::shared_ptr<BaseTask>& task, float delayMS,int cpuID = -1, bool isGroup = false);
    //stop all threads
    void StopAll();
    //stop a task_
    void StopTask(const std::string& id);
    //pause a task_
    void PauseTask(std::string id);
    //Resume a task_
    void ResumeTask(std::string id);
    //return a pointer to the system scheduler clock to use for timings
    std::shared_ptr<std::unordered_map<std::thread::id, std::shared_ptr<T_Thread>>> GetThreadMap();
    //get the clock
    std::shared_ptr<Clock> GetClock();
    //set the group size
    bool SetGroupSize(unsigned int size);
private:
    //start pool
    void StartPool();
    //worker loop
    void Worker();
    //check if any task is ready
    bool AnyTaskReady();
    //check if all queues are empty
    bool AllQueuesEmpty();
    //get the next task
    std::shared_ptr<BaseTask> GetNextTask();
    //return a thread thats pooling available for a task_
    std::shared_ptr<T_Thread> get_available_thread();

    std::unordered_map<std::string, Periodic_Task> scheduledTasks; //scheduled tasks mapped
    std::unordered_map<std::string, Delayed_Task> delayedTasks; //delayed tasks mapped
    std::unordered_map<std::thread::id, std::shared_ptr<T_Thread>> threadPool; //the thread pool mapped
    std::shared_ptr<Clock> clock;  // Add clock to track time
    std::shared_ptr<T_Thread> threadPtr = nullptr; //pointer to a t_thread
    std::vector<std::list<std::shared_ptr<BaseTask>>> priorityBins;  // priority bins of tasks
    std::mutex taskMutex; //task mutex
    std::mutex taskQueueMutex; //task queue mutex
    std::condition_variable cv; // Condition variable for thread synchronization
    std::atomic<bool> stopFlag = false; // stop flag 
    std::thread workerThread; // Worker thread
    inline static bool constructed = false; //constructed flag to ensure singleton like behavior without the pattern
};
