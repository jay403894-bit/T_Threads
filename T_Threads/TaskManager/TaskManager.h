#pragma once
#include <mutex>
#include "TaskScheduler.h"
#include "MPSCQueue.h"
#include "TaskNode.h"

class TaskManager {
public:
    // Singleton access
    static TaskManager& instance() {
        static TaskManager instance;
        return instance;
    }

    // Delete copy/move
    TaskManager(const TaskManager&) = delete;
    TaskManager& operator=(const TaskManager&) = delete;
        
    // Task operations
    void addTask(const std::shared_ptr<BaseTask>& task_, int cpu_id=-1,
        const std::shared_ptr<std::vector<std::shared_ptr<BaseTask>>>& dep_a_=nullptr,
        const std::shared_ptr<std::vector<std::shared_ptr<BaseTask>>>& dep_b_=nullptr);
    void scheduleTask(const std::shared_ptr<BaseTask>& task_, float interval, int cpu_id = -1);
    void scheduleDelayedTask(const std::shared_ptr<BaseTask>& task_, float delay_ms, int cpu_id = -1);
    void stopTask(const std::string& task_id);
    void pauseTask(const std::string& task_id);
    void resumeTask(const std::string& task_id);
    void enqueueToMain(const std::shared_ptr<BaseTask>& task_);
    void processMainThreadTasks();
    void join();
    std::string timeStamp();

private:
    TaskManager(); // private constructor
    void callback_(std::shared_ptr<BaseTask> t);
    MPSCQueue<std::shared_ptr<BaseTask>> queue_;
    std::shared_ptr<TaskScheduler> task_scheduler_ = nullptr;
    std::unordered_map<std::shared_ptr<BaseTask>, std::vector<std::shared_ptr<BaseTask>>> tasks_;
};