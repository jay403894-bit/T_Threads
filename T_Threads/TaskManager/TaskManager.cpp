#include "TaskManager.h"

// Get the singleton instance of TaskScheduler
std::shared_ptr<TaskScheduler> TaskManager::Get() {
    std::lock_guard<std::mutex> lock(mutex);
    if (!task_scheduler_) {
        task_scheduler_ = std::make_shared<TaskScheduler>();  // First-time initialization
    }
    return task_scheduler_;
}

void TaskManager::EnqueueToMain(const std::shared_ptr<BaseTask>& task)
{
    mainThreadQueue.push(task);
}


void TaskManager::ProcessMainThreadTasks() {
    while (!mainThreadQueue.empty()) {
        mainThreadQueue.pop()->get()->Execute();
    }
}