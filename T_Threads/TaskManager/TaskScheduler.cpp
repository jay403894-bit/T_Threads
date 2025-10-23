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
TaskScheduler::Periodic_Task::Periodic_Task(const std::shared_ptr<BaseTask>& task_, float interval_,const std::shared_ptr<Clock>& timer)
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
        Join();
}

//add a task
void TaskScheduler::AddTask(const std::shared_ptr<BaseTask>& task_, int cpuID) {
    if (stopFlag)
    {
        StartPool();
        stopFlag = false;
    }
    task_->SetCoreAffinity(cpuID);
    size_t bin_index = static_cast<size_t>(task_->GetPriority());
    if (bin_index < priorityBins.size()) {
        std::lock_guard<std::mutex> lock(taskMutex);  
        priorityBins[bin_index].push_back(task_);
        cv.notify_one(); 
    }

};
//schedule a pereiodic task
void TaskScheduler::ScheduleTask(const std::shared_ptr<BaseTask>& task_, float interval, int cpuID) {
    if (stopFlag)
    {
        StartPool();
        stopFlag = false;
    }
    task_->SetCoreAffinity(cpuID);    std::string id = task_->GetID();
    Periodic_Task pt(task_, interval, clock);
    {
        std::lock_guard<std::mutex> lock(taskMutex);
        scheduledTasks[id] = pt;
    }
}
//schedule a delayed task
void TaskScheduler::ScheduleDelayedTask(const std::shared_ptr<BaseTask>& task_, float delayMS, int cpuID) {
    if (stopFlag)
    {
        StartPool();
        stopFlag = false;
    }
    task_->SetCoreAffinity(cpuID);   
    std::string id = task_->GetID();
    Delayed_Task dt(task_, delayMS, clock);
    std::lock_guard<std::mutex> lock(taskMutex);
    delayedTasks[id] = dt;
}
//stop all tasks and threads
void TaskScheduler::Join() {
    // Signal shutdown
    stopFlag = true;
    cv.notify_all(); // wake scheduler thread

    // Cancel scheduled tasks
    {
        std::lock_guard<std::mutex> lock(taskMutex);
        for (auto& t : scheduledTasks) {
            t.second.cancelled = true;
        }
    }

    //join all workers 
    for (auto& worker : workers) {
        worker->Join(); // blocks until each worker finishes
    }

    // Join scheduler thread (bootstrap)
    if (workerThread.joinable()) {
        workerThread.join();
    }

    // Clear tasks and containers
    {
        std::lock_guard<std::mutex> lock(taskMutex);
        scheduledTasks.clear();
        delayedTasks.clear();
        for (auto& bin : priorityBins) {
            bin.clear();
        }
    }

    workers.clear();
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
}
//resume a task
void TaskScheduler::ResumeTask(std::string id) {
    std::lock_guard<std::mutex> lock(taskMutex);
    auto it = scheduledTasks.find(id);
    if (it != scheduledTasks.end()) {
        it->second.task_->ResumeTask();
    }
}
//return a timestamp in ms
std::string TaskScheduler::TimeStamp() { return clock->ToString(); }

void TaskScheduler::StartPool(unsigned int numWorkers) {
    if (constructed == false) { /* already handled in ctor, but be defensive */ }
    priorityBins.resize(static_cast<size_t>(PriorityLevel::BLOCKED) + 1);

    // determine a default numWorkers if caller passed 0
    unsigned int hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 4;
    if (numWorkers == 0) numWorkers = std::max(2u, hw - 2u); // keep two cores for main and scheduler
    // Preallocate
    workers.clear();
    queues.clear();
    workers.reserve(numWorkers);
    queues.reserve(numWorkers);

    // occupancy
    coreOccupied.clear();
    coreOccupied.assign(numWorkers, 0);        // 0 == free, 1 == occupied


    for (unsigned int i = 0; i < numWorkers; ++i) {
        // create queue and worker
        queues.push_back(std::make_unique<ChaseLevDeque<std::shared_ptr<BaseTask>>>());

        auto worker = std::make_shared<T_Thread>();
        worker->SetAffinity(i + 2, coreOccupied, numWorkers);
        worker->SetQueueIndex(i);
        workers.push_back(worker);
        worker->StartWorker(i + 2);
    }
    //bootstrap scheduler
    // use a shared atomic so the lambda can safely observe the ready flag
    auto ready = std::make_shared<std::atomic<bool>>(false);

    // store the thread in the member so its destructor won't call std::terminate
    workerThread = std::thread([this, ready]() { // <-- capture by value
        while (!ready->load(std::memory_order_acquire)) std::this_thread::yield();
        this->Worker();
        });

    // set affinity for the scheduler thread
#ifdef _WIN32
    DWORD_PTR mask = 1ULL << 1;
    SetThreadAffinityMask(workerThread.native_handle(), mask);
#endif

    // signal the thread it can start
    ready->store(true, std::memory_order_release);
    constructed = true;
}

int TaskScheduler::PickWorkerForTask(const std::shared_ptr<BaseTask>& task) {
    std::lock_guard<std::mutex> lock(taskMutex);
    // Use core affinity if available
    if (task->GetCoreAffinity() >= 2) {
        return (task->GetCoreAffinity()-2) % workers.size();
    }
    // Otherwise round-robin over all workers
    return nextWorker.fetch_add(1) % workers.size();
}
//worker loop
void TaskScheduler::Worker() {
    clock->Reset();
    int startIdx;
    int workerIndex;
    bool assigned;
    while (!stopFlag) {
        std::shared_ptr<BaseTask> task = GetNextTask();
        if (!task) {
            std::unique_lock<std::mutex> lock(workerMutex);
            cv.wait(lock, [this]() { return stopFlag || AnyTaskReady(); });
            continue;
        }
        startIdx = nextWorker.fetch_add(1) % queues.size();
        workerIndex = startIdx;
        assigned = false;
        do {
            if (queues[workerIndex]->push_bottom(task)) {
                assigned = true;
                break;
            }
            workerIndex = (workerIndex + 1) % queues.size();
        } while (workerIndex != startIdx);
        if (!assigned)
            fallbackQueue.push(task);
        else
            workers[workerIndex]->NotifyWorker();
    }
}
//check if any task is ready
bool TaskScheduler::AnyTaskReady() {
    std::lock_guard<std::mutex> lock(taskMutex);
    // 1. Check priority bins
    for (const auto& bin : priorityBins) {
        if (!bin.empty()) return true;
    }
    // 2. Check delayed tasks
    if (!delayedTasks.empty()) return true;
    // 3. Check periodic tasks
    if (!scheduledTasks.empty()) return true;

    return false;
}
//get the next task
std::shared_ptr<BaseTask> TaskScheduler::GetNextTask() {
    if (stopFlag) return nullptr;

    std::shared_ptr<BaseTask> candidate = nullptr;

    // Phase 1: pick a candidate while holding the mutex, but do NOT call TryTake()
    {
        std::lock_guard<std::mutex> lock(taskMutex);

        if (!fallbackQueue.empty()) {
            candidate = fallbackQueue.front();
            fallbackQueue.pop();
        }
        else {
            // 1. Regular priority bins (no per-call sort; bins represent priorities)
            for (auto& bin : priorityBins) {
                for (auto it = bin.begin(); it != bin.end(); ++it) {
                    auto t = *it;
                    if (!t) continue;
                    // check basic eligibility without taking the task
                    if (!t->IsPaused() && t->AreDependenciesComplete()) {
                        candidate = t;
                        bin.erase(it);
                        break;
                    }
                }
                if (candidate) break;
            }

            // 2. Delayed tasks
            if (!candidate) {
                for (auto it = delayedTasks.begin(); it != delayedTasks.end(); ++it) {
                    auto& dt = it->second;
                    auto& t = dt.task_;
                    if (!t) continue;
                    if (!t->IsPaused() && t->AreDependenciesComplete() && dt.IsTimeToRun()) {
                        candidate = t;
                        dt.MarkExecuted();
                        // remove delayed entry so it won't be considered again
                        delayedTasks.erase(it);
                        break;
                    }
                }
            }

            // 3. Periodic tasks
            if (!candidate) {
                for (auto& entry : scheduledTasks) {
                    auto& pt = entry.second;
                    auto& t = pt.task_;
                    if (!t) continue;
                    if (!t->IsPaused() && t->AreDependenciesComplete() && pt.IsTimeToRun()) {
                        candidate = t;
                        pt.UpdateExecutionTime();
                        break;
                    }
                }
            }
        }
    } // mutex released here

    if (!candidate) return nullptr;

    // Phase 2: attempt to take the candidate outside the lock.
    if (candidate->TryTake()) {
        return candidate;
    }

    // TryTake failed (another thread raced). Reinsert candidate back into its priority bin.
    {
        std::lock_guard<std::mutex> lock(taskMutex);
        size_t bin_index = static_cast<size_t>(candidate->GetPriority());
        if (bin_index < priorityBins.size()) {
            priorityBins[bin_index].push_back(candidate);
        }
        else {
            // fallback if priority is out of range
            fallbackQueue.push(candidate);
        }
    }

    return nullptr;
}




