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
    TaskListener(std::shared_ptr<std::atomic<bool>> completed_, std::function<void()> func);
    void onEventTriggered() override;

private:
    std::shared_ptr<std::atomic<bool>> done_;
    std::function<void()> callback_;
};

class TaskNode : public std::enable_shared_from_this<TaskNode> {
public:

    TaskNode(std::shared_ptr<BaseTask> task_,
        std::function<void(std::shared_ptr<BaseTask>)> func,
        DependencyType mode);
    void initialize();
    void addDependencyA(const std::shared_ptr<TaskNode>& dep);
    void addDependencyB(const std::shared_ptr<TaskNode>& dep);
    void setBinB();
    void dependencyCompleted();
    void execute();
    bool isCompleted() const;
    void completeTask();
    int getRemainingDependencies() const;
    void setDependencyMode(DependencyType mode);
private:
    bool isReady() const;
    
    DependencyType mode_ = DependencyType::AND;
    std::atomic<bool> bin_b_{ false };
    std::shared_ptr<Listener> listener_;
    std::function<void(std::shared_ptr<BaseTask>)> task_func_;
    std::shared_ptr<BaseTask> task_;
    std::vector<std::weak_ptr<TaskNode>> dependents_;
    std::vector<std::shared_ptr<TaskNode>> dep_a_;
    std::vector<std::shared_ptr<TaskNode>> dep_b_;
    std::atomic<int> remaining_a_{ 0 };
    std::atomic<int> remaining_b_{ 0 };
    std::shared_ptr<std::atomic<bool>> completed_flag_ = std::make_shared<std::atomic<bool>>(false);
};