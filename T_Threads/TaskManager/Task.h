#pragma once
#include <functional>
#include "../Utilities/UniqueID.h"
#include "Event.hpp"

class TaskCompletedEvent : public Event {
    std::string get_event_type() const override {
        return "task_completed";
    }
};


enum class PriorityLevel {
    SCHEDULED = 0,
    EXCLUSIVE = 1,
    VERY_HIGH = 2,
    HIGH = 3,
    MEDIUM = 4,
    NORMAL = 5,
    LOW = 6,
    BLOCKED = 7
};

/// <BaseTask>
///  BaseTask is a partial virtual base class of a Task
/// </BaseTask>
class BaseTask : public UniqueID {
public:
    static inline uint64_t s;
    static inline uint64_t f;
    static inline double rate;
    std::shared_ptr<TaskCompletedEvent> taskComplete = std::make_shared<TaskCompletedEvent>();
    BaseTask() = default; //constructor
    BaseTask operator=(const BaseTask& other) = delete; //non movable
    BaseTask(const BaseTask& other) = delete; //non movable
    ~BaseTask() = default; //default destructor 
    virtual void Execute() = 0; //Execute the task_
    void PauseTask(); //pause the task
    void ResumeTask(); //resume the task
    void SetPriority(const PriorityLevel& priority_in); //set priority level
    PriorityLevel GetPriority(); //return the tasks priority level
    void SetCompleted(); //set the task_ as completed
    bool IsCompleted(); //return if the task_ is completed or not
    bool IsPaused();//return if the task_ is paused
    bool IsCancelled(); //return if the task is cancelled
    bool TryTake(); //try to take the task to assign it
    bool IsTaken() const; //check if the task is taken
    void ResetTaken(); //reset the taken token
    void WaitUntilComplete(); //wait until complete
    int GetCoreAffinity(); //get core affinity
    void SetCoreAffinity(int cpuID); //set the task core affinity
    void CancelTask(); //set the task as cancelled
    inline std::shared_ptr<TaskCompletedEvent> GetEvent() { return taskComplete; };
protected:
    PriorityLevel priority_ = PriorityLevel::NORMAL; //the priority level of the task_
    std::atomic<bool> completed{ false }; //flag for whether the task is completed
    std::atomic<bool> paused{ false }; //paused flag
    std::atomic<bool> cancelled{ false }; //canceled flag
    std::mutex task_mutex_; //task mutex
    std::mutex wait_mutex_; //wait mutex
    std::condition_variable wait_cv_; // for waiters
    std::condition_variable cv; //condition variable
    std::vector <std::weak_ptr<BaseTask>> depA; //tasks this task is dependent on
    std::atomic<bool> taken{ false }; //taken flag/token
    int cpuCoreAffinity = -1; //the cpu core affinity, default any core
    int coreGroupAffinity = -1; // the core group affinity
};


//a task_ that returns data
class Task : public BaseTask {
public:
    //default constructor 
    Task(std::function<void()> task_fn);
    //delete the copy constructor 
    Task(const Task& other) = delete;
    //delete the copy constructor
    Task& operator=(const Task& other) = delete;
    //destructor
    virtual ~Task() = default;
    //execute override
    virtual void Execute() override;
protected:
    std::function<void()> task_fn_; //the tasks related function

};

struct SequencedTask {
    std::shared_ptr<BaseTask> task;
    uint64_t sequence;
};