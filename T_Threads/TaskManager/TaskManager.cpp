#include "TaskManager.h"

/// a listener_ for Event
class TestListener : public Listener {
    void onEventTriggered() override {
      
    }
};

TaskManager::TaskManager() {
    task_scheduler_ = std::make_shared<TaskScheduler>();
}

void TaskManager::callback_(std::shared_ptr<BaseTask> t)
{
    task_scheduler_->addTask(t);
}

void TaskManager::addTask(const std::shared_ptr<BaseTask>& task_, int cpu_id, 
    const std::shared_ptr<std::vector<std::shared_ptr<BaseTask>>>& dep_a_, 
    const std::shared_ptr<std::vector<std::shared_ptr<BaseTask>>>& dep_b_)
{
    auto scheduler_ = task_scheduler_;
    task_->setCoreAffinity(cpu_id);

    // Determine dependency mode first so we can pass it into the TaskNode ctor.
    DependencyType mode = DependencyType::AND;
    if (dep_a_ && dep_b_) mode = DependencyType::OR;

    // Create the node for this task_ (pass mode)
    auto taskNode = std::make_shared<TaskNode>(
        task_,
        // call scheduler_->addTask with cpu_id (matches scheduler_ signature)
        [scheduler_, cpu_id](std::shared_ptr<BaseTask> t) {
            if (scheduler_) scheduler_->addTask(t, cpu_id);
        },
        mode
    );

    // Safely initialize listener_
    taskNode->initialize();

    // Attach dep_a_
    if (dep_a_) {
        for (auto& depTask : *dep_a_) {
            if (!depTask) continue;
            auto depNode = std::make_shared<TaskNode>(
                depTask,
                [scheduler_, cpu_id](std::shared_ptr<BaseTask> t) {
                    if (scheduler_) scheduler_->addTask(t, cpu_id);
                },
                mode
            );
            depNode->initialize();
            taskNode->addDependencyA(depNode);
        }
    }
    //attach dep_b_
    else if (dep_b_) {
        for (auto& depTask : *dep_b_) {
            if (!depTask) continue;
            auto depNode = std::make_shared<TaskNode>(
                depTask,
                [scheduler_, cpu_id](std::shared_ptr<BaseTask> t) {
                    if (scheduler_) scheduler_->addTask(t, cpu_id);
                },
                mode
            );
            depNode->initialize();
            taskNode->addDependencyB(depNode);
        }
    }

    // If ready_ to go now (no deps), execute directly
    if (taskNode->isCompleted() || taskNode->getRemainingDependencies() == 0) {
        taskNode->execute();
    }
}

void TaskManager::scheduleTask(const std::shared_ptr<BaseTask>& task_, float interval, int cpu_id)
{
    task_scheduler_->scheduleTask(task_, interval, cpu_id);
}
void TaskManager::scheduleDelayedTask(const std::shared_ptr<BaseTask>& task_, float delay_ms, int cpu_id)
{
    task_scheduler_->scheduleDelayedTask(task_, delay_ms, cpu_id);
}
void TaskManager::stopTask(const std::string& task_id)
{
    task_scheduler_->stopTask(task_id);
}

void TaskManager::pauseTask(const std::string& task_id)
{
    task_scheduler_->pauseTask(task_id);
}
void TaskManager::resumeTask(const std::string& task_id)
{
    task_scheduler_->resumeTask(task_id);
}
void TaskManager::enqueueToMain(const std::shared_ptr<BaseTask>& task)
{
    queue_.push(task);
}
void TaskManager::join()
{
    if (task_scheduler_) 
    {
        task_scheduler_->join();
    }
}

void TaskManager::processMainThreadTasks()
{
    while (auto wrapper = queue_.pop()) {
        if (!wrapper) continue;
        std::shared_ptr<BaseTask> taskPtr = *wrapper; 
        if (taskPtr) taskPtr->execute();
    }
}

std::string TaskManager::timeStamp() 
{
    return task_scheduler_ ? task_scheduler_->timeStamp() : "";
}