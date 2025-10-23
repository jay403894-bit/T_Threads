#include "Task.h"
void BaseTask::PauseTask() {
    std::lock_guard<std::mutex> lock(task_mutex_);
    paused = true;
}
void BaseTask::ResumeTask() {
    std::lock_guard<std::mutex> lock(task_mutex_);
    paused = false;
}
void BaseTask::SetPriority(const PriorityLevel& priority_in) {
    std::lock_guard<std::mutex> lock(task_mutex_);
    priority_ = priority_in;
}; //set priority level
PriorityLevel BaseTask::GetPriority() {
    std::lock_guard<std::mutex> lock(task_mutex_);
    return priority_;
}; //return the tasks priority level
void BaseTask::SetCompleted() {
    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        ResetTaken();
        completed = true;
    }
    wait_cv_.notify_all(); // wake up threads waiting on WaitUntilComplete

}; //set the task_ as completed
bool BaseTask::IsCompleted() {
    std::lock_guard<std::mutex> lock(task_mutex_);
    return completed;
} //return if the task_ is completed or not
bool BaseTask::AreDependenciesComplete() {
    std::lock_guard<std::mutex> lock(task_mutex_);
    for (auto& dep : dependencies) {
        if (auto t = dep.lock()) {
            if (!t->IsCompleted()) return false;
        }
    }
    return true;
}
// Dependencies
void BaseTask::AddDependency(const std::shared_ptr<BaseTask>& dependency) {
    std::lock_guard<std::mutex> lock(task_mutex_);
    dependencies.push_back(dependency);
}
//return if the task_ is paused
bool BaseTask::IsPaused() {
    std::lock_guard<std::mutex> lock(task_mutex_);
    return paused;
}
//try to take the task, ensures only one thread gets it 
bool BaseTask::TryTake() {
    bool expected = false;
    return taken.compare_exchange_strong(expected, true);
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
    return completed;
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

