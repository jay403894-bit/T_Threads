#include "TaskScheduler.h"

// Default constructor
TaskScheduler::Periodic_Task::Periodic_Task()
    : task_(nullptr), nextExecutionTime(0.0f), interval(0.0f), clock(nullptr) {
}
//default constructor
TaskScheduler::Delayed_Task::Delayed_Task()
    : task_(nullptr), scheduledTime(0.0f), clock(nullptr), executed(false) {
}

// Parameterized constructor for initializing the task_ with interval and game timer
TaskScheduler::Periodic_Task::Periodic_Task(const std::shared_ptr<BaseTask>& task_, float interval_, std::shared_ptr<Clock>& timer)
    : task_(task_), interval(interval_), clock(timer) {
    nextExecutionTime = clock->ElapsedMS();  // Initialize to the current game time
}

// Check if it's time to run the task_ based on TotalTime and interval
bool TaskScheduler::Periodic_Task::IsTimeToRun() const {
    return !cancelled && (clock->ElapsedMS() >= nextExecutionTime);
}

// update the periodic execution time
void TaskScheduler::Periodic_Task::UpdateExecutionTime() {
    nextExecutionTime = clock->ElapsedMS() + interval;
}

//constructor
TaskScheduler::TaskScheduler() {
    if (constructed) {
        throw std::runtime_error("Only one TaskScheduler allowed!");
    }
    constructed = true;
    clock = std::make_shared<Clock>();
    StartPool();
}

//destructor
TaskScheduler::~TaskScheduler() {
    if (!stopFlag)
        StopAll();
    constructed = false;
}

//add a task
void TaskScheduler::AddTask(const std::shared_ptr<BaseTask>& task_, int cpuID, bool isGroup) {
    if (stopFlag)
    {
        StartPool();
    }
    if (isGroup)
        task_->SetGroupAffinity(cpuID);
    else
        task_->SetCoreAffinity(cpuID);
    size_t bin_index = static_cast<size_t>(task_->GetPriority());
    if (bin_index < priorityBins.size()) {
        std::lock_guard<std::mutex> lock(taskMutex);  
        priorityBins[bin_index].push_back(task_);
        Logger::Get()->LogInfo(Log_Level::Info, "Task added to bin: ");
        cv.notify_one(); 
    }
    else {
        Logger::Get()->LogInfo(Log_Level::Error, "Invalid priority level!");
    }
};
//schedule a pereiodic task
void TaskScheduler::ScheduleTask(const std::shared_ptr<BaseTask>& task_, float interval, int cpuID, bool isGroup) {
    if (stopFlag)
    {
        StartPool();
    }
    if (isGroup)
        task_->SetGroupAffinity(cpuID);
    else
        task_->SetCoreAffinity(cpuID);    std::string id = task_->GetID();
    Periodic_Task pt(task_, interval, clock);
    {
        std::lock_guard<std::mutex> lock(taskMutex);
        scheduledTasks[id] = pt;
    }
}
//schedule a delayed task
void TaskScheduler::ScheduleDelayedTask(const std::shared_ptr<BaseTask>& task_, float delayMS, int cpuID, bool isGroup) {
    if (stopFlag)
    {
        StartPool();
    }
    if (isGroup)
        task_->SetGroupAffinity(cpuID);
    else
        task_->SetCoreAffinity(cpuID);   
    std::string id = task_->GetID();
    Delayed_Task dt(task_, delayMS, clock);
    std::lock_guard<std::mutex> lock(taskMutex);
    delayedTasks[id] = dt;
}
//stop all tasks and threads
void TaskScheduler::StopAll() {
    stopFlag = true;
    cv.notify_all();
    {
        std::lock_guard<std::mutex> lock(taskMutex);
        for (auto& t : scheduledTasks) {
            t.second.cancelled = true;
        }
    }
    for (auto& thread : threadPool) {
        thread.second->Stop();
    }
    {
        std::lock_guard<std::mutex> lock(taskMutex);
        for (auto& bin : priorityBins) {
            std::list<std::shared_ptr<BaseTask>> empty;
            std::swap(bin, empty); 
        }
        scheduledTasks.clear(); 
    }
    if (workerThread.joinable()) {
        workerThread.join(); 
    }
    threadPool.clear();
    priorityBins.clear(); 
    Logger::Get()->LogInfo(Log_Level::Info, "All tasks and threads have been cleared.");
}
//stop a task_
void TaskScheduler::StopTask(const std::string& id) {
    std::lock_guard<std::mutex> lock(taskMutex);
    scheduledTasks.erase(id);
}
//pause a task_
void TaskScheduler::PauseTask(std::string id) {
    std::lock_guard<std::mutex> lock(taskMutex);
    auto it = scheduledTasks.find(id);
    if (it != scheduledTasks.end()) {
        it->second.task_->PauseTask();
    }
    else {
        Logger::Get()->LogInfo(Log_Level::Warning, "Pause failed: task not found with id: " + id);
    }
}
//resume a task
void TaskScheduler::ResumeTask(std::string id) {
    std::lock_guard<std::mutex> lock(taskMutex);
    auto it = scheduledTasks.find(id);
    if (it != scheduledTasks.end()) {
        it->second.task_->ResumeTask();
    }
    else {
        Logger::Get()->LogInfo(Log_Level::Warning, "Resume failed: task not found with id: " + id);
    }
}
//return the threadmap so the user side code can retrieve T_Thread data by thread id
std::shared_ptr<std::unordered_map<std::thread::id, std::shared_ptr<T_Thread>>> TaskScheduler::GetThreadMap() {
    return std::make_shared<std::unordered_map<std::thread::id, std::shared_ptr<T_Thread>>>(threadPool);
}
//return the clock
std::shared_ptr<Clock> TaskScheduler::GetClock() { return clock; }

//start pool
void TaskScheduler::StartPool() {
    stopFlag = false;
    priorityBins.resize(static_cast<size_t>(PriorityLevel::BLOCKED) + 1);

    for (size_t i = 0; i < std::thread::hardware_concurrency() - 1; ++i) {
        std::shared_ptr<T_Thread> new_thread = std::make_shared<T_Thread>();
        threadPool.insert({ new_thread->GetID(), new_thread });
    }

    workerThread = std::thread(&TaskScheduler::Worker, this);
}
//worker loop
void TaskScheduler::Worker() {
    clock->Start();

    while (!stopFlag) {
        std::shared_ptr<BaseTask> task = GetNextTask();

        if (!task) {
            std::unique_lock<std::mutex> lock(taskMutex);
            cv.wait(lock, [this]() { return stopFlag || AnyTaskReady(); });
            continue;
        }

        auto thread = get_available_thread(); 
        if (thread) {
            if (!thread->TryReserve()) {
                taskQueue.push(task);
            }
            else {
                bool ok = thread->SetTask(task); 
                if (!ok) {
                    thread->ReleaseReservation();
                    taskQueue.push(task);
                }
            }
        }
        else {
            taskQueue.push(task);
        }

    }
}
//check if any task is ready
bool TaskScheduler::AnyTaskReady() {
    return !AllQueuesEmpty() || !scheduledTasks.empty() || !delayedTasks.empty();
}
//check if all queues are empty
bool TaskScheduler::AllQueuesEmpty() {
    for (const auto& bin : priorityBins) {
        if (!bin.empty()) return false;
    }
    return true;
};
//get the next task
std::shared_ptr<BaseTask> TaskScheduler::GetNextTask() {
    std::lock_guard<std::mutex> lock(taskMutex);
    if (stopFlag) return nullptr;
    std::shared_ptr<BaseTask> selectedTask = nullptr;
    // 1. Regular priority bins
    for (auto& bin : priorityBins) {
        for (auto it = bin.begin(); it != bin.end(); ) {
            auto task = *it; 
            if (!task) { ++it; continue; }
            if (!task->IsPaused() && task->AreDependenciesComplete() && task->TryTake()) {

                it = bin.erase(it);   
                return task;          
            }
            else {
                ++it;
            }
        }
    }
    // 2. Delayed tasks
    if (!selectedTask) {
        for (auto& [id, dt] : delayedTasks) {
            auto& t = dt.task_;
            if (!t->IsPaused() && t->AreDependenciesComplete() && dt.IsTimeToRun() && t->TryTake()) {
                selectedTask = t;
                dt.MarkExecuted(); 
                break;
            }
        }
    }
    // 3. Periodic tasks
    for (auto& [id, pt] : scheduledTasks) {
        auto& t = pt.task_;
        if (!t->IsPaused() && t->AreDependenciesComplete() && pt.IsTimeToRun()) {
            if (t->TryTake()) {
                selectedTask = t;
                pt.UpdateExecutionTime();
                break; 
            }
        }
    }
    return selectedTask;
}

//get an available thread
std::shared_ptr<T_Thread> TaskScheduler::get_available_thread() {
    for (auto& thread : threadPool) {
        if (thread.second->GetStatus() == ThreadStatus::Pool) {
            return thread.second;
        }
    }
    return nullptr; 
}




