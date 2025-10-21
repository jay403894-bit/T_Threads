#pragma once
#include <mutex>
#include "TaskScheduler.h"


class TaskManager {
public:
    // Delete the constructor so no one else can create instances of this manager
    TaskManager() = delete;
    TaskManager(const TaskManager& other) = delete;
    TaskManager& operator=(const TaskManager& other) = delete;
    // Delete the constructor so no one else can create instances of this manager
    ~TaskManager() = default;

    // Singleton accessor can also potentially access the global clock if needed
    static std::shared_ptr<TaskScheduler> Get();
    static std::shared_ptr<std::unordered_map<std::thread::id, std::shared_ptr<T_Thread>>>GetThreadMap();
    static std::shared_ptr<Clock> GetClock();
    static void EnqueueToMain(const std::shared_ptr<BaseTask>& task);
    static void ProcessMainThreadTasks();

private:
    static std::mutex mutex;
    static std::queue<std::shared_ptr<BaseTask>> mainThreadQueue;
    static std::mutex mainQueueMutex;
    static std::shared_ptr<TaskScheduler> task_scheduler_;  // The singleton TaskScheduler instance
};
