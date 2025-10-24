#include "Task.h"
#include <iostream>

void BaseTask::setPriority(const PriorityLevel& priority_in) {
    std::lock_guard<std::mutex> lock(task_mutex_);
    priority_ = priority_in;
}; //set priority level
PriorityLevel BaseTask::getPriority() {
    std::lock_guard<std::mutex> lock(task_mutex_);
    return priority_;
}; //return the tasks priority level
void BaseTask::setCompleted() {
    std::shared_ptr<TaskCompletedEvent> evtCopy;
    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        resetTaken();
        completed_ = true;
        evtCopy = taskComplete; // capture event under lock
    }

    // wake waiters
    wait_cv_.notify_all();

    // notify listeners outside the task_ mutex to avoid lock inversion
    if (evtCopy) {
        try { evtCopy->notify(); }
        catch (...) { /* log if you have logger */ }
    }
} //set the task_ as completed_
bool BaseTask::isCompleted() {
    std::lock_guard<std::mutex> lock(task_mutex_);
    return completed_;
} //return if the task_ is completed_ or not

//return if the task_ is paused_
bool BaseTask::isPaused() {
    std::lock_guard<std::mutex> lock(task_mutex_);
    return paused_;
}
bool BaseTask::isCancelled()
{
    return cancelled_;
}
//try to take the task_, ensures only one thread gets it 
bool BaseTask::tryTake() {
    bool expected = false;
    bool success = taken_.compare_exchange_strong(expected, true);
    return success;
}
//check if the task_ is taken_
bool BaseTask::isTaken() const {
    return taken_.load();
}
//reset the taken_ token
void BaseTask::resetTaken() {
    taken_.store(false);
}
//wait until complete
void BaseTask::waitUntilComplete() {
std::unique_lock<std::mutex> lock(wait_mutex_);
wait_cv_.wait(lock, [this] {
    std::lock_guard<std::mutex> taskLock(task_mutex_);
    return completed_.load(std::memory_order_acquire);
    });
}
//get core affinity
int BaseTask::getCoreAffinity() {
    std::lock_guard<std::mutex> lock(task_mutex_);
    return cpu_core_affinity;
}
//set cpu core affinity
void BaseTask::setCoreAffinity(int cpuID)
{
    std::lock_guard<std::mutex> lock(task_mutex_);
    cpu_core_affinity = cpuID;
}
void BaseTask::cancelTask()
{
    cancelled_.store(true,std::memory_order_release);
}

void BaseTask::pauseTask()
{
    paused_.store(true, std::memory_order_release);
}
void BaseTask::resumeTask()
{
    paused_.store(false, std::memory_order_release);
}
//return the task_ complete event

//constructor
Task::Task(std::function<void()> task_fn)
    : task_fn_(task_fn) {
}
//execute the task_
void Task::execute() {
    try {
        task_fn_();
    }
    catch (const std::exception& e) {
        std::string tmp = e.what();
    }
    setCompleted();
};

