#include "Task.h"
#include <iostream>

void BaseTask::SetPriority(const PriorityLevel& priority_in) {
    std::lock_guard<std::mutex> lock(task_mutex_);
    priority_ = priority_in;
}; //set priority level
PriorityLevel BaseTask::GetPriority() {
    std::lock_guard<std::mutex> lock(task_mutex_);
    return priority_;
}; //return the tasks priority level
void BaseTask::SetCompleted() {
    std::shared_ptr<TaskCompletedEvent> evtCopy;
    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        ResetTaken();
        completed = true;
        evtCopy = taskComplete; // capture event under lock
    }

    // wake waiters
    wait_cv_.notify_all();

    // notify listeners outside the task mutex to avoid lock inversion
    if (evtCopy) {
        try { evtCopy->notify(); }
        catch (...) { /* log if you have logger */ }
    }
} //set the task_ as completed
bool BaseTask::IsCompleted() {
    std::lock_guard<std::mutex> lock(task_mutex_);
    return completed;
} //return if the task_ is completed or not

//return if the task_ is paused
bool BaseTask::IsPaused() {
    std::lock_guard<std::mutex> lock(task_mutex_);
    return paused;
}
bool BaseTask::IsCancelled()
{
    return cancelled;
}
//try to take the task, ensures only one thread gets it 
bool BaseTask::TryTake() {
    bool expected = false;
    bool success = taken.compare_exchange_strong(expected, true);
  /*  if (!success) {
        std::cout << "[Task] TryTake failed for " << GetID() << "\n";
        f++;
    }
    else
        s++;
    uint64_t total = s + f;
    rate = total ? (double)s / (double)total * 100.0 : 0.0;
    std::cout << "Success rate: " << rate << 
        " Succeeeded: " << s << 
        " failed: " << f 
        << " total " << total << std::endl ;
    */
    return success;
}
//check if the task is taken
bool BaseTask::IsTaken() const {
    return taken.load();
}
//reset the taken token
void BaseTask::ResetTaken() {
    taken.store(false);
}
//wait until complete
void BaseTask::WaitUntilComplete() {
std::unique_lock<std::mutex> lock(wait_mutex_);
wait_cv_.wait(lock, [this] {
    std::lock_guard<std::mutex> taskLock(task_mutex_);
    return completed.load(std::memory_order_acquire);
    });
}
//get core affinity
int BaseTask::GetCoreAffinity() {
    std::lock_guard<std::mutex> lock(task_mutex_);
    return cpuCoreAffinity;
}
//set cpu core affinity
void BaseTask::SetCoreAffinity(int cpuID)
{
    std::lock_guard<std::mutex> lock(task_mutex_);
    cpuCoreAffinity = cpuID;
    coreGroupAffinity = -1;
}
void BaseTask::CancelTask()
{
    cancelled.store(true,std::memory_order_release);
}

void BaseTask::PauseTask()
{
    paused.store(true, std::memory_order_release);
}
void BaseTask::ResumeTask()
{
    paused.store(false, std::memory_order_release);
}
//return the task complete event

//constructor
Task::Task(std::function<void()> task_fn)
    : task_fn_(task_fn) {
}
//execute the task
void Task::Execute() {
    try {
        task_fn_();
    }
    catch (const std::exception& e) {
        std::string tmp = e.what();
    }
    SetCompleted();
};

