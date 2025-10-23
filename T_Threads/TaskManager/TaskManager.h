#pragma once
#include <mutex>
#include "TaskScheduler.h"
#include "MPSCQueue.h"

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
    static void EnqueueToMain(const std::shared_ptr<BaseTask>& task);
    static void ProcessMainThreadTasks();

private:
    static inline std::mutex mutex;
    static inline MPSCQueue<std::shared_ptr<BaseTask>> mainThreadQueue;
    static inline std::mutex mainQueueMutex;
    static inline std::shared_ptr<TaskScheduler> task_scheduler_ = nullptr;  // The singleton TaskScheduler instance
};
