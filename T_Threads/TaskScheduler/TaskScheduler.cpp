#include "TaskScheduler.h"
using namespace T_Threads;

TaskScheduler::TaskScheduler() {
    if (constructed_.load(std::memory_order_acquire)) {
        throw SchedulerException("Only one TaskScheduler allowed!");
    }
    clock_ = new Clock();
    constructed_.store(true, std::memory_order_release);
}
TaskScheduler::~TaskScheduler() {
    if (!stop_flag_) {
        join();
    }
    delete clock_;
}
bool TaskScheduler::enqueueToMain(Task*& task)
{
    if (!pool_active_) return false;
    if (!task)
        return false;
    main_queue_.push(task);
    return true;
}
void TaskScheduler::processMainThread()
{
    if (!pool_active_) return;

    while (auto wrapper = main_queue_.pop()) {
        if (!wrapper) continue;
        Task* taskPtr = *wrapper;
        if (taskPtr) taskPtr->execute();
    }
}
void TaskScheduler::join() {
    if (!pool_active_) return;
    stop_flag_ = true;
    notifyAll();
    for (auto& worker : workers_) {
        worker->join();
    }
    {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        workers_.clear();
        main_queue_.clear();

        immediate_cores_in_use.clear();

    }
    pool_active_.store(false, std::memory_order_release);
}

void T_Threads::TaskScheduler::notifyAll()
{
    for (int i = 0; i < workers_.size(); i++)
        workers_[i]->notifyWorker();
}
void TaskScheduler::startPool(uint8_t clock_worker_cpu_id) {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    stop_flag_.store(false, std::memory_order_release);
    next_worker_ = 0;
    if (!constructed_.load(std::memory_order_acquire)) {}
    unsigned int hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 4;
    unsigned int num_workers = std::max(1u, hw - 1u);
    EpochManager::instance().setThreshold(500);
    workers_.clear();
    thread_queues_.clear();
    workers_.reserve(num_workers);
    thread_queues_.reserve(num_workers);
    immediate_cores_in_use.clear();
    immediate_cores_in_use.reserve(num_workers);

    for (unsigned int i = 0; i < num_workers; ++i) {
        immediate_cores_in_use.push_back(std::make_unique<std::atomic<bool>>(false));
        thread_queues_.push_back(std::make_unique<TaskDeque>());
        inboxes_.push_back(std::make_unique<MPSCQueue<Task*>>());
        inboxes_local_.push_back(std::make_unique<MPSCQueue<Task*>>());

    }
    for (unsigned int i = 0; i < num_workers; ++i) {
        auto worker = std::make_shared<T_Thread>();
        worker->setAffinity(i + 1, num_workers);
        worker->setQueueIndex(i);
        workers_.push_back(worker);
        worker->startWorker(i + 1);
    }
    for (int i = 0; i < workers_.size(); i++)
    {
        while (!workers_[i]->ready()) {
            std::this_thread::yield();
        }
    }
    worker_task = new Task(worker, this, true);
    bootstrap(clock_worker_cpu_id, worker_task);
    pool_active_.store(true, std::memory_order_release);
}
bool TaskScheduler::submitLocal(Task*& task)
{
    return pushLocal(task);
}
bool TaskScheduler::submitLocal(uint8_t cpu_affinity, Task*& task)
{
   return pushLocal(task, cpu_affinity);
}
bool TaskScheduler::submitPQ(Task*& task)
{
   return push(task);
}
bool TaskScheduler::submitPQ(uint8_t priority, Task*& task)
{
   return push(task, priority);
}
bool TaskScheduler::submitFork(size_t cpu_affinity, Task*& task)
{
    if (!task)
        return false;
    return pushToCore(cpu_affinity, task);
}

bool TaskScheduler::submitPeriodic(std::string id, double ms, Task*& task)
{
    return pushPeriodic(id, ms, task);
}

bool TaskScheduler::submitDelayed(double ms, Task*& task)
{
    return pushDelayed(ms, task);
}
void TaskScheduler::stop() {
    stop_flag_.store(true, std::memory_order_release);
    worker_task->stop();
}

Clock* TaskScheduler::getClock()
{
    return clock_;
}
void TaskScheduler::worker(void* data)
{
    TaskScheduler* self = static_cast<TaskScheduler*>(data);
    self->clock_->reset();
    
    while (!self->stop_flag_.load(std::memory_order_acquire)) {
        // --- 1. Drain inboxes into heaps ---
        while (!self->delayed_inbox_.empty()) {
            auto sp = self->delayed_inbox_.pop();
            if (sp) {
                self->delayed_heap_.push_back(*sp);
                std::push_heap(self->delayed_heap_.begin(), self->delayed_heap_.end(), DelayedTaskCompare());
            }
        }

        while (!self->periodic_inbox_.empty()) {
            auto sp = self->periodic_inbox_.pop();
            if (sp) {
                self->periodic_heap_.push_back(*sp);
                std::push_heap(self->periodic_heap_.begin(), self->periodic_heap_.end(), PeriodicTaskCompare());
            }
        }

        double now = self->clock_->elapsedMs();

        // --- 2. Run all delayed tasks ready to execute ---
        while (!self->delayed_heap_.empty() &&
            self->delayed_heap_.front()->scheduled_time <= now)
        {
            auto delayed = self->delayed_heap_.front();
            std::pop_heap(self->delayed_heap_.begin(), self->delayed_heap_.end(), DelayedTaskCompare());
            self->delayed_heap_.pop_back();

            if (delayed->task) {
                self->submitLocal(delayed->task); // push task to worker queue
            }
            EpochManager::instance().retireDelayed(delayed,EpochManager::instance().currentEpoch());
        }

        // --- 3. Run periodic tasks ready to execute ---
        while (!self->periodic_heap_.empty()) {
            auto& periodic = self->periodic_heap_.front(); // reference to top of heap
            if (periodic->scheduled_time > now)
                break; 

            // pop the top element
            std::pop_heap(self->periodic_heap_.begin(), self->periodic_heap_.end(), PeriodicTaskCompare());
            self->periodic_heap_.pop_back();
     
            if (periodic->task->stop_flag.load(std::memory_order_acquire)) {
                periodic->cancelled = true;
                EpochManager::instance().retirePeriodic(periodic, EpochManager::instance().currentEpoch());
            }
            // execute and reinsert if still active
            if (periodic->task && !periodic->cancelled) {
                self->submitLocal(periodic->task); // push to worker queue
                periodic->updateExecutionTime();    // schedule next execution
                self->periodic_heap_.push_back(periodic);
                std::push_heap(self->periodic_heap_.begin(), self->periodic_heap_.end(), PeriodicTaskCompare());
            }
        }

        // --- 4. Notify workers in case new tasks were submitted ---
        self->notifyAll();

        // --- 5. Sleep for a short interval (10ms here) ---
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
bool TaskScheduler::pushDelayed(double ms, Task*& task)
{
    if (pool_active_.load(std::memory_order_acquire))
    {
        if (!task)
            return false;
        task->auto_delete = true;
        DelayedTask* dt = new DelayedTask(task, ms, clock_);
        delayed_inbox_.push(dt);
    }
    return true;
}
bool TaskScheduler::pushPeriodic(std::string id, double ms, Task*& task)
{
    if (pool_active_.load(std::memory_order_acquire))
    {
        if (!task)
            return false;
        PeriodicTask* pt = new PeriodicTask(task, id, ms,clock_);
        periodic_inbox_.push(pt);
    }
    return true;
}
bool TaskScheduler::pushLocal(Task*& task, uint8_t cpuaffinity) {
    if (!task)
        return false;
    
    if (cpuaffinity > 0) {
        if (!immediate_cores_in_use[cpuaffinity - 1]->load(std::memory_order_acquire)) {
            inboxes_local_[cpuaffinity - 1]->push(task);
            workers_[cpuaffinity - 1]->notifyWorker();
        }
        else
            return false;
    }
    else {
        uint8_t chosen = pickNextWorker();
        inboxes_[chosen]->push(task);
        notifyAll();
    }
    return true;
}
bool TaskScheduler::push(Task*& task, uint8_t priority)
{
    //simple guard if less than pin ot priority 0 if greater max at 5
    if (priority > 4)
        priority = 4;
 
    if (!task)
        return false;
    if (!priority_queue_[priority].try_enqueue(task)) {
        //currently no overflow handling if you go over about 32 mil tasks backed up 
        //but 32 mil tasks is a lot 
    };
    for (int i = 0; i < workers_.size(); i++) {
        workers_[i]->notifyWorker();
    }
    return true;
}
bool TaskScheduler::pushToCore(size_t core_id, Task*& task)
{
    if (!pool_active_) return false;
    if (!task)
        return false;

    if (immediate_cores_in_use[core_id % workers_.size()]->load(std::memory_order_acquire) || core_id < 1) {
        return false;
    }
    immediate_cores_in_use[core_id-1 % workers_.size()]->store(true, std::memory_order_release);
    workers_[core_id-1 % workers_.size()]->setImmediateTask(task);
    return true;
}
void TaskScheduler::bootstrap(size_t core_id, Task*& task)
{
    immediate_cores_in_use[core_id-1 % workers_.size()]->store(true, std::memory_order_release);
    workers_[core_id-1 % workers_.size()]->setImmediateTask(task);
}
int TaskScheduler::pickNextWorker() {
    size_t n = workers_.size();
    for (size_t i = 0; i < n; ++i) {
        size_t idx = (next_worker_ + i) % n;
        // Skip workers that are busy with immediate tasks
        if (!immediate_cores_in_use[idx]->load(std::memory_order_acquire)) {
            next_worker_ = (idx + 1) % n; // advance round-robin pointer
            return static_cast<int>(idx);
        }
    }
    // If all cores are busy, just pick the next in round-robin
    int fallback = static_cast<int>(next_worker_);
    next_worker_ = (fallback + 1) % n;
    return fallback;
}


