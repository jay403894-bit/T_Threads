#pragma once
#include <mutex>
#include "TaskScheduler.h"
#include "MPSCQueue.h"
#include "TaskNode.h"

class TaskManager {
public:
    // Singleton access
    static TaskManager& Instance() {
        static TaskManager instance;
        return instance;
    }

    // Delete copy/move
    TaskManager(const TaskManager&) = delete;
    TaskManager& operator=(const TaskManager&) = delete;
        
    // Task operations
    void AddTask(const std::shared_ptr<BaseTask>& task, int cpuID=-1,
        const std::shared_ptr<std::vector<std::shared_ptr<BaseTask>>>& depA=nullptr,
        const std::shared_ptr<std::vector<std::shared_ptr<BaseTask>>>& depB=nullptr);
    void ScheduleTask(const std::shared_ptr<BaseTask>& task, float interval, int cpuID = -1);
    void ScheduleDelayedTask(const std::shared_ptr<BaseTask>& task, float delayMS, int cpuID = -1);
    void StopTask(const std::string& taskID);
    void PauseTask(const std::string& taskID);
    void ResumeTask(const std::string& taskID);
    void EnqueueToMain(const std::shared_ptr<BaseTask>& task);
    void ProcessMainThreadTasks();
    void Join();
    std::string TimeStamp();

private:
    TaskManager(); // private constructor
    void callback(std::shared_ptr<BaseTask> t) {
        task_scheduler_->AddTask(t);
    }
    std::mutex mutex;
    MPSCQueue<std::shared_ptr<BaseTask>> queue;
    std::shared_ptr<TaskScheduler> task_scheduler_ = nullptr;
    std::unordered_map<std::shared_ptr<BaseTask>, std::vector<std::shared_ptr<BaseTask>>> tasks_;
};