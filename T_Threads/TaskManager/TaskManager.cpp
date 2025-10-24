#include "TaskManager.h"

/// a listener for Event
class TestListener : public Listener {
    void on_event_triggered() override {
      
    }
};

TaskManager::TaskManager() {
    task_scheduler_ = std::make_shared<TaskScheduler>();
}

void TaskManager::AddTask(const std::shared_ptr<BaseTask>& task, int cpuID, 
    const std::shared_ptr<std::vector<std::shared_ptr<BaseTask>>>& depA, 
    const std::shared_ptr<std::vector<std::shared_ptr<BaseTask>>>& depB)
{
    auto scheduler = task_scheduler_;
    task->SetCoreAffinity(cpuID);

    // Determine dependency mode first so we can pass it into the TaskNode ctor.
    DependencyType mode = DependencyType::AND;
    if (depA && depB) mode = DependencyType::OR;

    // Create the node for this task (pass mode)
    auto taskNode = std::make_shared<TaskNode>(
        task,
        // call scheduler->AddTask with cpuID (matches scheduler signature)
        [scheduler, cpuID](std::shared_ptr<BaseTask> t) {
            if (scheduler) scheduler->AddTask(t, cpuID);
        },
        mode
    );

    // Safely initialize listener
    taskNode->Initialize();

    // Attach depA
    if (depA) {
        for (auto& depTask : *depA) {
            if (!depTask) continue;
            auto depNode = std::make_shared<TaskNode>(
                depTask,
                [scheduler, cpuID](std::shared_ptr<BaseTask> t) {
                    if (scheduler) scheduler->AddTask(t, cpuID);
                },
                mode
            );
            depNode->Initialize();
            taskNode->AddDependencyA(depNode);
        }
    }
    //attach depB
    else if (depB) {
        for (auto& depTask : *depB) {
            if (!depTask) continue;
            auto depNode = std::make_shared<TaskNode>(
                depTask,
                [scheduler, cpuID](std::shared_ptr<BaseTask> t) {
                    if (scheduler) scheduler->AddTask(t, cpuID);
                },
                mode
            );
            depNode->Initialize();
            taskNode->AddDependencyB(depNode);
        }
    }

    // If ready to go now (no deps), execute directly
    if (taskNode->IsCompleted() || taskNode->GetRemainingDependencies() == 0) {
        taskNode->Execute();
    }
}

void TaskManager::ScheduleTask(const std::shared_ptr<BaseTask>& task, float interval, int cpuID)
{
    task_scheduler_->ScheduleTask(task, interval, cpuID);
}
void TaskManager::ScheduleDelayedTask(const std::shared_ptr<BaseTask>& task, float delayMS, int cpuID)
{
    task_scheduler_->ScheduleDelayedTask(task, delayMS, cpuID);
}
void TaskManager::StopTask(const std::string& taskID)
{
    task_scheduler_->StopTask(taskID);
}

void TaskManager::PauseTask(const std::string& taskID)
{
    task_scheduler_->PauseTask(taskID);
}
void TaskManager::ResumeTask(const std::string& taskID)
{
    task_scheduler_->ResumeTask(taskID);
}
void TaskManager::EnqueueToMain(const std::shared_ptr<BaseTask>& task)
{
    queue.push(task);
}
void TaskManager::Join()
{
    if (task_scheduler_) 
    {
        task_scheduler_->Join();
    }
}

void TaskManager::ProcessMainThreadTasks()
{
    while (auto wrapper = queue.pop()) {
        if (!wrapper) continue;
        std::shared_ptr<BaseTask> taskPtr = *wrapper; 
        if (taskPtr) taskPtr->Execute();
    }
}

std::string TaskManager::TimeStamp() 
{
    return task_scheduler_ ? task_scheduler_->TimeStamp() : "";
}