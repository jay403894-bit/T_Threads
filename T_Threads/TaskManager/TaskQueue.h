#pragma once
#include <mutex>
#include <queue>
#include <optional>
#include "Task.h"

// TQue that holds messages to be processed by the main thread
class TQue {
public:
    TQue() = default;
    TQue(const TQue& other);
    TQue& operator=(const TQue& other);
    bool operator==(const TQue& other);
    bool operator!=(const TQue& other);
    std::optional<std::shared_ptr<BaseTask>> pop();
    std::optional<std::shared_ptr<BaseTask>> top();
    void push(const std::shared_ptr<BaseTask>& task);
    bool empty() const;
private:
    std::queue<std::shared_ptr<BaseTask>> queue;
    std::condition_variable cv;
    std::mutex queueMtx;
};

//this is a base class to inherit from to share the task queue
class TaskQueue {
public:
    static TQue taskQueue;
};