#include "TaskManager.h"
std::shared_ptr<TaskScheduler> TaskManager::task_scheduler_ = nullptr;
std::mutex TaskManager::mutex;
std::queue<std::shared_ptr<BaseTask>> TaskManager::mainThreadQueue;
std::mutex TaskManager::mainQueueMutex;
// Get the singleton instance of TaskScheduler
std::shared_ptr<TaskScheduler> TaskManager::Get() {
    std::lock_guard<std::mutex> lock(mutex);
    if (!task_scheduler_) {
        task_scheduler_ = std::make_shared<TaskScheduler>();  // First-time initialization
    }
    return task_scheduler_;
}

std::shared_ptr<std::unordered_map<std::thread::id, std::shared_ptr<T_Thread>>> TaskManager::GetThreadMap()
{
    return Get()->GetThreadMap();
}

std::shared_ptr<Clock> TaskManager::GetClock()
{
    return Get()->GetClock();
}

void TaskManager::EnqueueToMain(const std::shared_ptr<BaseTask>& task)
{
    std::lock_guard<std::mutex> lock(mainQueueMutex);
    mainThreadQueue.push(task);
}


void TaskManager::ProcessMainThreadTasks() {
    std::lock_guard<std::mutex> lock(mainQueueMutex);
    while (!mainThreadQueue.empty()) {
        mainThreadQueue.front()->Execute();
        mainThreadQueue.pop();
    }
}