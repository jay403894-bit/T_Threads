#include "TaskScheduler.h"
#include "TaskManager.h"



// Default constructor
PeriodicTask::PeriodicTask()
    : task_(nullptr), scheduledTime(0.0f), interval(0.0f), clock(nullptr) {
}
//default constructor
DelayedTask::DelayedTask()
    : task_(nullptr), scheduledTime(0.0f), clock(nullptr), executed(false) {
}

// Parameterized constructor for initializing the task_ with interval and game timer
PeriodicTask::PeriodicTask(const std::shared_ptr<BaseTask>& task_, float interval_,const std::shared_ptr<Clock>& timer)
    : task_(task_), interval(interval_), clock(timer) {
    scheduledTime = clock->ElapsedMS();  // Initialize to the current game time
}

// Check if it's time to run the task_ based on TotalTime and interval
bool PeriodicTask::IsTimeToRun() const {
    return !cancelled && (clock->ElapsedMS() >= scheduledTime);
}

// update the periodic execution time
void PeriodicTask::UpdateExecutionTime() {
    scheduledTime = clock->ElapsedMS() + interval;
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
    if (stopFlag.load()) {
        StartPool();
        stopFlag.store(false);
    }
    task_->SetCoreAffinity(cpuID);
    allTasks[task_->GetID()] = task_;

    size_t bin_index = static_cast<size_t>(task_->GetPriority());
    if (bin_index >= priorityBins.size()) return;

    {
        priorityBinMutexes[bin_index]->lock();
        priorityBins[bin_index].push_back(task_);
        std::push_heap(priorityBins[bin_index].begin(), priorityBins[bin_index].end(), PriorityCompare());
        priorityBinMutexes[bin_index]->unlock();
    }
    cv.notify_one();
}
//schedule a periodic task
void TaskScheduler::ScheduleTask(const std::shared_ptr<BaseTask>& task_, float interval, int cpuID) {
    if (stopFlag)
    {
        StartPool();
        stopFlag = false;
    }
    task_->SetCoreAffinity(cpuID);    std::string id = task_->GetID();
    allTasks[task_->GetID()] = task_;
    PeriodicTask pt(task_, interval, clock);
    {
        std::lock_guard<std::mutex> lock(taskMutex);
        scheduledTasks[id] = pt;
        periodicHeap.push_back(pt);
        std::push_heap(periodicHeap.begin(), periodicHeap.end(), PeriodicTaskCompare());
    }
}
//schedule a delayed task
void TaskScheduler::ScheduleDelayedTask(const std::shared_ptr<BaseTask>& task_, float delayMS, int cpuID) {
    if (stopFlag.load(std::memory_order_acquire)) {
        StartPool();
        stopFlag.store(false, std::memory_order_release);
    }
    task_->SetCoreAffinity(cpuID);
    allTasks[task_->GetID()] = task_;
    std::string id = task_->GetID();
    DelayedTask dt(task_, delayMS, clock);
    {
        std::lock_guard<std::mutex> lock(taskMutex);
        delayedTasks[id] = dt;
        delayedHeap.push_back(dt);
        std::push_heap(delayedHeap.begin(), delayedHeap.end(), DelayedTaskCompare());
    }
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
    allTasks[id]->CancelTask();
}

//pause a task_
void TaskScheduler::PauseTask(std::string id) {
    std::lock_guard<std::mutex> lock(taskMutex);
    allTasks[id]->PauseTask();

}
//resume a task
void TaskScheduler::ResumeTask(std::string id) {
    std::lock_guard<std::mutex> lock(taskMutex);
    allTasks[id]->ResumeTask();

}
//return a timestamp in ms
std::string TaskScheduler::TimeStamp() { return clock->ToString(); }

void TaskScheduler::StartPool(unsigned int numWorkers) {
    if (constructed == false) { /* already handled in ctor, but be defensive */ }
    // create per-priority MPSC queues
    size_t levels = static_cast<size_t>(PriorityLevel::BLOCKED) + 1;
    priorityBins.clear();
    priorityBins.resize(levels);
    for (size_t i = 0; i < levels; ++i) {
        priorityBins[i].clear();
        priorityBinMutexes[i] = std::make_unique<std::mutex>();
    }
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


void TaskScheduler::PushDelayed(const DelayedTask& t) {
    delayedHeap.push_back(t);
    std::push_heap(delayedHeap.begin(), delayedHeap.end(), DelayedTaskCompare());
}

void TaskScheduler::PushPeriodic(const PeriodicTask& t) {
    periodicHeap.push_back(t);
    std::push_heap(periodicHeap.begin(), periodicHeap.end(), PeriodicTaskCompare());
}
bool TaskScheduler::PopNextDelayed(TaskCandidate& out) {
    if (delayedHeap.empty()) return false;
    if (!delayedHeap.front().IsTimeToRun()) return false;

    auto top = delayedHeap.front();
    std::pop_heap(delayedHeap.begin(), delayedHeap.end(), DelayedTaskCompare());
    delayedHeap.pop_back();
    delayedTasks.erase(top.task_->GetID());

    out.task = top.task_;
    out.src = TaskCandidate::Source::Delayed;
    out.id = top.task_->GetID();
    out.delayed_copy = top;
    return true;
}

bool TaskScheduler::PopNextPeriodic(TaskCandidate& out) {
    if (periodicHeap.empty()) return false;
    if (!periodicHeap.front().IsTimeToRun()) return false;

    auto top = periodicHeap.front();
    std::pop_heap(periodicHeap.begin(), periodicHeap.end(), PeriodicTaskCompare());
    periodicHeap.pop_back();
    scheduledTasks.erase(top.task_->GetID());

    out.task = top.task_;
    out.src = TaskCandidate::Source::Periodic;
    out.id = top.task_->GetID();
    out.periodic_copy = top;
    return true;
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

std::vector<TaskCandidate> TaskScheduler::GetNextBatch(size_t maxBatch)
{

    std::lock_guard<std::mutex> lock(taskMutex);
    std::vector<TaskCandidate> batch;
    batch.reserve(maxBatch);

    // 1) fallback queue
    while (!fallbackQueue.empty() && batch.size() < maxBatch) {
        auto task = fallbackQueue.front();
        fallbackQueue.pop();

        if (!task) continue;
        if (task->IsCancelled()) continue;
        if (task->IsPaused()) {
            fallbackQueue.push(task);
            continue;
        }
        else
            // Claim task immediately
            if (!task->TryTake()) continue;

        batch.push_back({ 
            task, 
            TaskCandidate::Source::Fallback, 
            SIZE_MAX, 
            "" });
    }

    // 2) priority bins
    for (size_t bin_idx = 0; bin_idx < priorityBins.size() && batch.size() < maxBatch; ++bin_idx) {
        auto& bin = priorityBins[bin_idx];
        auto& mutex = priorityBinMutexes[bin_idx];

        priorityBinMutexes[bin_idx]->lock();
        while (!bin.empty() && batch.size() < maxBatch) {
            std::pop_heap(bin.begin(), bin.end(), PriorityCompare());
            auto task = bin.back();
            bin.pop_back();

            if (!task || task->IsCancelled()) continue;

            // Claim task immediately
            if (!task->TryTake()) continue;

            batch.push_back({ task, TaskCandidate::Source::PriorityBin, bin_idx, "" });
        }
        priorityBinMutexes[bin_idx]->unlock();
    }
    // 3) delayed heap
    while (batch.size() < maxBatch && !delayedHeap.empty()) {
        auto& top = delayedHeap.front();
        if (!top.IsTimeToRun()) break;

        DelayedTask snapshot = top;
        std::pop_heap(delayedHeap.begin(), delayedHeap.end(), DelayedTaskCompare());
        delayedHeap.pop_back();

        auto task = snapshot.task_;
        if (!task) continue;
        if (task->IsCancelled()) continue;
        if (task->IsPaused()) {
            PushDelayed(snapshot);
            continue;
        }
        else
            // Claim task immediately
            if (!task->TryTake()) continue;

        delayedTasks.erase(task->GetID());
        TaskCandidate c;
        c.task = task;
        c.src = TaskCandidate::Source::Delayed;
        c.id = task->GetID();
        c.delayed_copy = std::move(snapshot);
        batch.push_back(std::move(c));
    }

    // 4) periodic heap
    while (batch.size() < maxBatch && !periodicHeap.empty()) {
        auto& top = periodicHeap.front();
        if (!top.IsTimeToRun()) break;

        PeriodicTask snapshot = top;
        std::pop_heap(periodicHeap.begin(), periodicHeap.end(), PeriodicTaskCompare());
        periodicHeap.pop_back();

        auto task = snapshot.task_;
        if (!task) continue;
        if (task->IsCancelled()) continue;
        if (task->IsPaused()) {
            PushPeriodic(snapshot);
            continue;
        }
        else
            // Claim task immediately
            if (!task->TryTake()) continue;

        scheduledTasks.erase(task->GetID());
        TaskCandidate c;
        c.task = task;
        c.src = TaskCandidate::Source::Periodic;
        c.id = task->GetID();
        c.periodic_copy = std::move(snapshot);
        batch.push_back(std::move(c));
    }

    return batch;
}


// Modified Worker() to use batching and dispatch
void TaskScheduler::Worker() {
    clock->Reset();
    const size_t batchSize = 8;

    while (!stopFlag.load(std::memory_order_acquire)) {
        // --- Phase A: Gather candidates ---
        auto candidates = GetNextBatch(batchSize);

        if (candidates.empty()) {
            std::unique_lock<std::mutex> lock(workerMutex);
            cv.wait(lock, [this]() {
                return stopFlag.load(std::memory_order_acquire) || AnyTaskReady();
                });
            continue;
        }

        // --- Phase B: Dispatch claimed tasks to workers ---
        for (auto& c : candidates) {
            if (!c.task) continue;

            // Handle paused tasks — reinsert where they came from
            if (c.task->IsPaused()) {
                std::lock_guard<std::mutex> lock(taskMutex);
                switch (c.src) {
                case TaskCandidate::Source::PriorityBin:
                    if (c.bin_index < priorityBins.size())
                        priorityBins[c.bin_index].push_back(c.task);
                    else
                        fallbackQueue.push(c.task);
                    break;
                case TaskCandidate::Source::Fallback:
                    fallbackQueue.push(c.task);
                    break;
                case TaskCandidate::Source::Delayed:
                    if (c.delayed_copy) PushDelayed(*c.delayed_copy);
                    break;
                case TaskCandidate::Source::Periodic:
                    if (c.periodic_copy) PushPeriodic(*c.periodic_copy);
                    break;
                default: break;
                }
                continue;
            }

            // Already claimed, push to worker
            auto& task = c.task;
            int workerIndex = PickWorkerForTask(task);
            if (workers.empty() || queues.empty()) {
                std::lock_guard<std::mutex> lock(taskMutex);
                fallbackQueue.push(task);
                continue;
            }

            if (!queues[workerIndex]->push_bottom(task)) {
                int fallback = nextWorker.fetch_add(1) % queues.size();
                if (!queues[fallback]->push_bottom(task)) {
                    std::lock_guard<std::mutex> lock(taskMutex);
                    fallbackQueue.push(task);
                    continue;
                }
                else {
                    workers[fallback]->NotifyWorker();
                }
            }
            else {
                workers[workerIndex]->NotifyWorker();
            }

            // Post-dispatch reinsertion for periodic tasks
            if (c.src == TaskCandidate::Source::Periodic && c.periodic_copy) {
                PeriodicTask next = *c.periodic_copy;

                if (next.task_->IsCancelled()) continue;

                next.UpdateExecutionTime();
                std::lock_guard<std::mutex> lock(taskMutex);
                PushPeriodic(next);
            }
        }
    }
}




//check if any task is ready
bool TaskScheduler::AnyTaskReady() {
    std::lock_guard<std::mutex> lock(taskMutex);
    // 1. Check priority bins
    for (const auto& bin : priorityBins) {
        if (!bin.size() == 0) return true;
    }
    // 2. Check delayed tasks
    if (!delayedTasks.empty()) return true;
    // 3. Check periodic tasks
    if (!scheduledTasks.empty()) return true;

    return false;
}





