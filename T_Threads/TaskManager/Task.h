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

class BaseTask : public UniqueID {
public:
    std::shared_ptr<TaskCompletedEvent> taskComplete = std::make_shared<TaskCompletedEvent>();
    BaseTask() = default; 
    BaseTask operator=(const BaseTask& other) = delete; 
    BaseTask(const BaseTask& other) = delete; 
    ~BaseTask() = default; 
    virtual void execute() = 0; 
    void pauseTask();
    void resumeTask(); 
    void setPriority(const PriorityLevel& priority_in);
    PriorityLevel getPriority();
    void setCompleted();
    bool isCompleted();
    bool isPaused();
    bool isCancelled(); 
    bool tryTake();
    bool isTaken() const; 
    void resetTaken(); 
    void waitUntilComplete(); 
    int getCoreAffinity(); 
    void setCoreAffinity(int cpuID);
    void cancelTask(); 
    inline std::shared_ptr<TaskCompletedEvent> getEvent() { return taskComplete; };
protected:
    PriorityLevel priority_ = PriorityLevel::NORMAL; 
    std::atomic<bool> completed_{ false };
    std::atomic<bool> paused_{ false }; 
    std::atomic<bool> cancelled_{ false }; 
    std::mutex task_mutex_; 
    std::mutex wait_mutex_; 
    std::condition_variable wait_cv_;
    std::condition_variable cv_; 
    std::atomic<bool> taken_{ false }; 
    int cpu_core_affinity = -1;
};


class Task : public BaseTask {
public:
    Task(std::function<void()> task_fn);
    Task(const Task& other) = delete;
    Task& operator=(const Task& other) = delete;
    virtual ~Task() = default;
    virtual void execute() override;
protected:
    std::function<void()> task_fn_; 

};

struct SequencedTask {
    std::shared_ptr<BaseTask> task_;
    uint64_t sequence;
};