#pragma once
#include "Task.h"
#include "Event.hpp"
#include <memory>
#include <vector>
#include <atomic>
#include <functional>
enum class DependencyType { AND, OR };

class TaskListener : public Listener {
public:
    TaskListener(std::shared_ptr<std::atomic<bool>> completed, std::function<void()> func)
        : done(std::move(completed)), callback(std::move(func)) {
    }

    void on_event_triggered() override {
        done->store(true, std::memory_order_release);
        if (callback) callback();
    }

private:
    std::shared_ptr<std::atomic<bool>> done;
    std::function<void()> callback;
};

class TaskNode : public std::enable_shared_from_this<TaskNode> {
public:

    TaskNode(
        std::shared_ptr<BaseTask> task,
        std::function<void(std::shared_ptr<BaseTask>)> func,
        DependencyType mode)
        : task_(std::move(task)),
        mode_(mode),
        taskFunc(std::move(func)),
        remainingA(0),
        remainingB(0),
        completedFlag(std::make_shared<std::atomic<bool>>(false))
    {
    }

    void Initialize() {
        // now that task_ and completedFlag exist, safely subscribe
        auto self = shared_from_this();
        listener = std::make_shared<TaskListener>(
            completedFlag,
            [self]() { self->CompleteTask(); }
        );
        task_->GetEvent()->subscribe(listener);
    }

    void AddDependencyA(const std::shared_ptr<TaskNode>& dep) {
        depA.push_back(dep);
        dep->dependents.push_back(shared_from_this());
        ++remainingA;
    }
    void AddDependencyB(const std::shared_ptr<TaskNode>& dep) {
        dep->SetBinB();       // mark the new dependency as BinB
        depB.push_back(dep);  // add to the B bin
        dep->dependents.push_back(shared_from_this());
        ++remainingB;
    }
    void SetBinB() {
        binB.store(true, std::memory_order_acquire);
    }
    void DependencyCompleted(bool isBinB = false) {
        if (completedFlag->load(std::memory_order_acquire)) return;

        if (isBinB) {
            if (--remainingB < 0) remainingB = 0;
        }
        else {
            if (--remainingA < 0) remainingA = 0;
        }

        if (IsReady()) {
            Execute();
        }
    }

    void Execute() {
        if (taskFunc) taskFunc(task_);
    }

    bool IsCompleted() const {
        return completedFlag->load(std::memory_order_acquire);
    }

    void CompleteTask() {
        // Notify dependents
        for (auto& dep : dependents) {
            if (auto d = dep.lock())
                d->DependencyCompleted();
        }
    }
    int GetRemainingDependencies() const {
        return remainingA.load();
    }
    void SetDependencyMode(DependencyType mode) {
        mode_ = mode;
    }
private:
    bool IsReady() const {
        if (mode_ == DependencyType::AND) {
            return remainingA == 0 && remainingB == 0;
        }
        else { // OR mode
            return (remainingA == 0) || (remainingB == 0);
        }
    }
    
    DependencyType mode_ = DependencyType::AND;
    std::atomic<bool> binB{ false };
    std::shared_ptr<TaskScheduler> scheduler;
    std::shared_ptr<Listener> listener;
    std::function<void(std::shared_ptr<BaseTask>)> taskFunc;
    std::shared_ptr<BaseTask> task_;
    std::vector<std::weak_ptr<TaskNode>> dependents;
    std::vector<std::shared_ptr<TaskNode>> depA;
    std::vector<std::shared_ptr<TaskNode>> depB;
    std::atomic<int> remainingA{ 0 };
    std::atomic<int> remainingB{ 0 };
    std::shared_ptr<std::atomic<bool>> completedFlag = std::make_shared<std::atomic<bool>>(false);
};