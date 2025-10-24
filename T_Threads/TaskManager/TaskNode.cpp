#include "TaskNode.h"
TaskListener::TaskListener(std::shared_ptr<std::atomic<bool>> completed_, std::function<void()> func)
    : done_(std::move(completed_)), callback_(std::move(func)) {
}

void TaskListener::onEventTriggered() {
    done_->store(true, std::memory_order_release);
    if (callback_) callback_();
}

TaskNode::TaskNode(
    std::shared_ptr<BaseTask> task_,
    std::function<void(std::shared_ptr<BaseTask>)> func,
    DependencyType mode)
    : task_(std::move(task_)),
    mode_(mode),
    task_func_(std::move(func)),
    remaining_a_(0),
    remaining_b_(0),
    completed_flag_(std::make_shared<std::atomic<bool>>(false)){
}
void TaskNode::initialize() {
    // now that task_ and completed_flag_ exist, safely subscribe
    auto self = shared_from_this();
    listener_ = std::make_shared<TaskListener>(
        completed_flag_,
        [self]() { self->completeTask(); }
    );
    task_->getEvent()->subscribe(listener_);
}
void TaskNode::addDependencyA(const std::shared_ptr<TaskNode>& dep) {
    dep_a_.push_back(dep);
    dep->dependents_.push_back(shared_from_this());
    ++remaining_a_;
}
void TaskNode::addDependencyB(const std::shared_ptr<TaskNode>& dep) {
    dep->setBinB();       // mark the new dependency as BinB
    dep_b_.push_back(dep);  // add to the B bin
    dep->dependents_.push_back(shared_from_this());
    ++remaining_b_;
}
void TaskNode::setBinB() {
    bin_b_.store(true, std::memory_order_acquire);
}
void TaskNode::dependencyCompleted() {
    if (completed_flag_->load(std::memory_order_acquire)) return;

    if (bin_b_.load(std::memory_order_acquire)) {
        --remaining_b_;
    }
    else {
        --remaining_a_;
    }

    if (isReady()) {
        execute();
    }
}
void TaskNode::execute() {
    if (task_func_) task_func_(task_);
}
bool TaskNode::isCompleted() const {
    return completed_flag_->load(std::memory_order_acquire);
}
void TaskNode::completeTask() {
    // Notify dependents_
    for (auto& dep : dependents_) {
        if (auto d = dep.lock())
            d->dependencyCompleted();
    }
}
int TaskNode::getRemainingDependencies() const {
    return remaining_a_.load();
}
void TaskNode::setDependencyMode(DependencyType mode) {
    mode_ = mode;
}

bool TaskNode::isReady() const {
    if (mode_ == DependencyType::AND) {
        return remaining_a_ == 0 && remaining_b_ == 0;
    }
    else { // OR mode
        return (remaining_a_ == 0) || (remaining_b_ == 0);
    }
}