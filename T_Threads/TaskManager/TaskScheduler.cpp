#include "TaskScheduler.h"
#include "TaskManager.h"

PeriodicTask::PeriodicTask()
    : task_(nullptr), scheduledTime(0.0f), interval(0.0f), clock_(nullptr) {
}
DelayedTask::DelayedTask()
    : task_(nullptr), scheduledTime(0.0f), clock_(nullptr), executed(false) {
}
PeriodicTask::PeriodicTask(const std::shared_ptr<BaseTask>& task_, float interval_,const std::shared_ptr<Clock>& timer)
    : task_(task_), interval(interval_), clock_(timer) {
    scheduledTime = clock_->elapsedMs();  
}
bool PeriodicTask::isTimeToRun() const {
    return !cancelled_ && (clock_->elapsedMs() >= scheduledTime);
}
void PeriodicTask::updateExecutionTime() {
    scheduledTime = clock_->elapsedMs() + interval;
}
TaskScheduler::TaskScheduler() {
    if (constructed_.load(std::memory_order_acquire)) {
        throw std::runtime_error("Only one TaskScheduler allowed!");
    }
    constructed_.store(true,std::memory_order_release);
    clock_ = std::make_shared<Clock>();
    startPool();
}
TaskScheduler::~TaskScheduler() {
    if (!stop_flag_)
        join();
}
void TaskScheduler::addTask(const std::shared_ptr<BaseTask>& task_, int cpuID) {
    if (stop_flag_.load()) {
        startPool();
        stop_flag_.store(false);
    }
    task_->setCoreAffinity(cpuID);
    task_map_[task_->getId()] = task_;
    size_t bin_index_ = static_cast<size_t>(task_->getPriority());
    if (bin_index_ >= priority_bins_.size()) return;
    {
        priority_bin_mutexes_[bin_index_]->lock();
        priority_bins_[bin_index_].push_back(task_);
        std::push_heap(priority_bins_[bin_index_].begin(), priority_bins_[bin_index_].end(), PriorityCompare());
        priority_bin_mutexes_[bin_index_]->unlock();
    }
    cv_.notify_one();
}
void TaskScheduler::scheduleTask(const std::shared_ptr<BaseTask>& task_, float interval, int cpuID) {
    if (stop_flag_)
    {
        startPool();
        stop_flag_ = false;
    }
    task_->setCoreAffinity(cpuID);    std::string id_ = task_->getId();
    task_map_[task_->getId()] = task_;
    PeriodicTask pt(task_, interval, clock_);
    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        scheduled_tasks_[id_] = pt;
        periodic_heap_.push_back(pt);
        std::push_heap(periodic_heap_.begin(), periodic_heap_.end(), PeriodicTaskCompare());
    }
}
void TaskScheduler::scheduleDelayedTask(const std::shared_ptr<BaseTask>& task, float delayMS, int cpuID) {
    if (stop_flag_.load(std::memory_order_acquire)) {
        startPool();
        stop_flag_.store(false, std::memory_order_release);
    }
    task->setCoreAffinity(cpuID);
    task_map_[task->getId()] = task;
    std::string id_ = task->getId();
    DelayedTask dt(task, delayMS, clock_);
    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        delayed_tasks_[id_] = dt;
        delayed_heap_.push_back(dt);
        std::push_heap(delayed_heap_.begin(), delayed_heap_.end(), DelayedTaskCompare());
    }
}
void TaskScheduler::join() {
    stop_flag_ = true;
    cv_.notify_all(); 
    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        for (auto& t : scheduled_tasks_) {
            t.second.cancelled_ = true;
        }
    }
    for (auto& worker : workers_) {
        worker->join(); 
    }
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        scheduled_tasks_.clear();
        delayed_tasks_.clear();
        for (auto& bin : priority_bins_) {
            bin.clear();
        }
    }
    workers_.clear();
}
void TaskScheduler::stopTask(const std::string& id) {
    std::lock_guard<std::mutex> lock(task_mutex_);
    task_map_[id]->cancelTask();
}
void TaskScheduler::pauseTask(std::string id) {
    std::lock_guard<std::mutex> lock(task_mutex_);
    task_map_[id]->pauseTask();

}
void TaskScheduler::resumeTask(std::string id) {
    std::lock_guard<std::mutex> lock(task_mutex_);
    task_map_[id]->resumeTask();
}
std::string TaskScheduler::timeStamp() { return clock_->toString(); }
void TaskScheduler::startPool(unsigned int num_workers) {
    if (!constructed_.load(std::memory_order_acquire)) {}
    size_t levels = static_cast<size_t>(PriorityLevel::BLOCKED) + 1;
    priority_bins_.clear();
    priority_bins_.resize(levels);
    for (size_t i = 0; i < levels; ++i) {
        priority_bins_[i].clear();
        priority_bin_mutexes_[i] = std::make_unique<std::mutex>();
    }
    unsigned int hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 4;
    if (num_workers == 0) num_workers = std::max(2u, hw - 2u);
    workers_.clear();
    queues_.clear();
    workers_.reserve(num_workers);
    queues_.reserve(num_workers);
    core_occupied_.clear();
    core_occupied_.assign(num_workers, 0);       
    for (unsigned int i = 0; i < num_workers; ++i) {
        queues_.push_back(std::make_unique<ChaseLevDeque<std::shared_ptr<BaseTask>>>());
        auto worker = std::make_shared<T_Thread>();
        worker->setAffinity(i + 2, core_occupied_, num_workers);
        worker->setQueueIndex(i);
        workers_.push_back(worker);
        worker->startWorker(i + 2);
    }
    auto ready_ = std::make_shared<std::atomic<bool>>(false);
    worker_thread_ = std::thread([this, ready_]() { // <-- capture by value
        while (!ready_->load(std::memory_order_acquire)) std::this_thread::yield();
        this->worker();
        });
#ifdef _WIN32
    DWORD_PTR mask_ = 1ULL << 1;
    SetThreadAffinityMask(worker_thread_.native_handle(), mask_);
#endif
    ready_->store(true, std::memory_order_release);
    constructed_.store(true,std::memory_order_release);
}
void TaskScheduler::pushDelayed(const DelayedTask& t) {
    delayed_heap_.push_back(t);
    std::push_heap(delayed_heap_.begin(), delayed_heap_.end(), DelayedTaskCompare());
}
void TaskScheduler::pushPeriodic(const PeriodicTask& t) {
    periodic_heap_.push_back(t);
    std::push_heap(periodic_heap_.begin(), periodic_heap_.end(), PeriodicTaskCompare());
}
bool TaskScheduler::popNextDelayed(TaskCandidate& out) {
    if (delayed_heap_.empty()) return false;
    if (!delayed_heap_.front().isTimeToRun()) return false;

    auto top = delayed_heap_.front();
    std::pop_heap(delayed_heap_.begin(), delayed_heap_.end(), DelayedTaskCompare());
    delayed_heap_.pop_back();
    delayed_tasks_.erase(top.task_->getId());

    out.task_ = top.task_;
    out.src_ = TaskCandidate::Source::Delayed;
    out.id_ = top.task_->getId();
    out.delayed_copy_ = top;
    return true;
}
bool TaskScheduler::popNextPeriodic(TaskCandidate& out) {
    if (periodic_heap_.empty()) return false;
    if (!periodic_heap_.front().isTimeToRun()) return false;
    auto top = periodic_heap_.front();
    std::pop_heap(periodic_heap_.begin(), periodic_heap_.end(), PeriodicTaskCompare());
    periodic_heap_.pop_back();
    scheduled_tasks_.erase(top.task_->getId());
    out.task_ = top.task_;
    out.src_ = TaskCandidate::Source::Periodic;
    out.id_ = top.task_->getId();
    out.periodic_copy_ = top;
    return true;
}
int TaskScheduler::pickWorkerForTask(const std::shared_ptr<BaseTask>& task) {
    std::lock_guard<std::mutex> lock(task_mutex_);
    if (task->getCoreAffinity() >= 2) {
        return (task->getCoreAffinity()-2) % workers_.size();
    }
    return next_worker_.fetch_add(1) % workers_.size();
}
std::vector<TaskCandidate> TaskScheduler::getNextBatch(size_t maxBatch)
{
    std::lock_guard<std::mutex> lock(task_mutex_);
    std::vector<TaskCandidate> batch;
    batch.reserve(maxBatch);
    while (!fallback_queue_.empty() && batch.size() < maxBatch) {
        auto task_ = fallback_queue_.front();
        fallback_queue_.pop();
        if (!task_) continue;
        if (task_->isCancelled()) continue;
        if (task_->isPaused()) {
            fallback_queue_.push(task_);
            continue;
        }
        else
            if (!task_->tryTake()) continue;
        batch.push_back({ 
            task_, 
            TaskCandidate::Source::Fallback, 
            SIZE_MAX, 
            "" });
    }
    for (size_t bin_idx = 0; bin_idx < priority_bins_.size() && batch.size() < maxBatch; ++bin_idx) {
        auto& bin = priority_bins_[bin_idx];
        auto& mutex = priority_bin_mutexes_[bin_idx];
        priority_bin_mutexes_[bin_idx]->lock();
        while (!bin.empty() && batch.size() < maxBatch) {
            std::pop_heap(bin.begin(), bin.end(), PriorityCompare());
            auto task_ = bin.back();
            bin.pop_back();
            if (!task_ || task_->isCancelled()) continue;
            if (!task_->tryTake()) continue;
            batch.push_back({ task_, TaskCandidate::Source::PriorityBin, bin_idx, "" });
        }
        priority_bin_mutexes_[bin_idx]->unlock();
    }
    while (batch.size() < maxBatch && !delayed_heap_.empty()) {
        auto& top = delayed_heap_.front();
        if (!top.isTimeToRun()) break;
        DelayedTask snapshot = top;
        std::pop_heap(delayed_heap_.begin(), delayed_heap_.end(), DelayedTaskCompare());
        delayed_heap_.pop_back();
        auto task_ = snapshot.task_;
        if (!task_) continue;
        if (task_->isCancelled()) continue;
        if (task_->isPaused()) {
            pushDelayed(snapshot);
            continue;
        }
        else
            if (!task_->tryTake()) continue;
        delayed_tasks_.erase(task_->getId());
        TaskCandidate c;
        c.task_ = task_;
        c.src_ = TaskCandidate::Source::Delayed;
        c.id_ = task_->getId();
        c.delayed_copy_ = std::move(snapshot);
        batch.push_back(std::move(c));
    }
    while (batch.size() < maxBatch && !periodic_heap_.empty()) {
        auto& top = periodic_heap_.front();
        if (!top.isTimeToRun()) break;
        PeriodicTask snapshot = top;
        std::pop_heap(periodic_heap_.begin(), periodic_heap_.end(), PeriodicTaskCompare());
        periodic_heap_.pop_back();
        auto task_ = snapshot.task_;
        if (!task_) continue;
        if (task_->isCancelled()) continue;
        if (task_->isPaused()) {
            pushPeriodic(snapshot);
            continue;
        }
        else
            if (!task_->tryTake()) continue;
        scheduled_tasks_.erase(task_->getId());
        TaskCandidate c;
        c.task_ = task_;
        c.src_ = TaskCandidate::Source::Periodic;
        c.id_ = task_->getId();
        c.periodic_copy_ = std::move(snapshot);
        batch.push_back(std::move(c));
    }
    return batch;
}
void TaskScheduler::worker() {
    clock_->reset();
    const size_t batchSize = 8;
    while (!stop_flag_.load(std::memory_order_acquire)) {
        auto candidates = getNextBatch(batchSize);
        if (candidates.empty()) {
            std::unique_lock<std::mutex> lock(worker_mutex_);
            cv_.wait(lock, [this]() {
                return stop_flag_.load(std::memory_order_acquire) || anyTaskReady();
                });
            continue;
        }
        for (auto& c : candidates) {
            if (!c.task_) continue;
            if (c.task_->isPaused()) {
                std::lock_guard<std::mutex> lock(task_mutex_);
                switch (c.src_) {
                case TaskCandidate::Source::PriorityBin:
                    if (c.bin_index_ < priority_bins_.size())
                        priority_bins_[c.bin_index_].push_back(c.task_);
                    else
                        fallback_queue_.push(c.task_);
                    break;
                case TaskCandidate::Source::Fallback:
                    fallback_queue_.push(c.task_);
                    break;
                case TaskCandidate::Source::Delayed:
                    if (c.delayed_copy_) pushDelayed(*c.delayed_copy_);
                    break;
                case TaskCandidate::Source::Periodic:
                    if (c.periodic_copy_) pushPeriodic(*c.periodic_copy_);
                    break;
                default: break;
                }
                continue;
            }
            auto& task_ = c.task_;
            int workerIndex = pickWorkerForTask(task_);
            if (workers_.empty() || queues_.empty()) {
                std::lock_guard<std::mutex> lock(task_mutex_);
                fallback_queue_.push(task_);
                continue;
            }
            if (!queues_[workerIndex]->push_bottom(task_)) {
                int fallback = next_worker_.fetch_add(1) % queues_.size();
                if (!queues_[fallback]->push_bottom(task_)) {
                    std::lock_guard<std::mutex> lock(task_mutex_);
                    fallback_queue_.push(task_);
                    continue;
                }
                else {
                    workers_[fallback]->notifyWorker();
                }
            }
            else {
                workers_[workerIndex]->notifyWorker();
            }
            if (c.src_ == TaskCandidate::Source::Periodic && c.periodic_copy_) {
                PeriodicTask next = *c.periodic_copy_;
                if (next.task_->isCancelled()) continue;
                next.updateExecutionTime();
                std::lock_guard<std::mutex> lock(task_mutex_);
                pushPeriodic(next);
            }
        }
    }
}

bool TaskScheduler::anyTaskReady() {
    std::lock_guard<std::mutex> lock(task_mutex_);
    for (const auto& bin : priority_bins_) {
        if (!bin.size() == 0) return true;
    }
    if (!delayed_tasks_.empty()) return true;
    if (!scheduled_tasks_.empty()) return true;
    return false;
}