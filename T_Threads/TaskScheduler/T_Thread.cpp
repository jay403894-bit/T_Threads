#include "T_Thread.h"
#include <iostream>
using namespace T_Threads;

T_Thread::T_Thread() {
}
T_Thread::~T_Thread() {
}
void T_Thread::startWorker(size_t cpu_affinity)
{
	auto ready = std::make_shared<std::atomic<bool>>(false);
	thread_ = std::thread([this, ready]() {
		while (!ready->load(std::memory_order_acquire)) std::this_thread::yield();
		this->worker();
		});
	native_handle_ = thread_.native_handle();
#ifdef _WIN32
	DWORD_PTR mask_ = 1ULL << cpu_affinity;
	SetThreadAffinityMask(native_handle_, mask_);
#endif
	ready->store(true, std::memory_order_release);
};
std::thread::id T_Thread::getId() {
	return thread_.get_id();
}
bool T_Thread::setImmediateTask(Task* new_task) {
	if (!new_task) return false;
	{
		immediate_task_ = new_task;
		immediate_.store(true, std::memory_order_release);
	}
	cv_.notify_one();
	return true;
}
int T_Thread::getQueueLoad()
{
	return queue_load_.load(std::memory_order_acquire);
}
void T_Thread::setQueueIndex(size_t index)
{
	queue_index_ = index;
};
void T_Thread::join() {
	bool expected = false;
	if (!joining_.compare_exchange_strong(expected, true)) return;

	running_.store(false, std::memory_order_release);
	notifyWorker();

	std::unique_lock<std::mutex> lock(join_mutex_);
	cv_worker_done_.wait(lock, [this] {
		return !running_.load(std::memory_order_acquire);
		});

	if (thread_.joinable())
		thread_.join();

	joining_.store(false, std::memory_order_release);
}
void T_Thread::notifyWorker()
{
	cv_.notify_one();
}
void T_Thread::pushTask()
{
	queue_load_.fetch_add(1, std::memory_order_relaxed);
}
void T_Thread::popTask()
{
	queue_load_.fetch_sub(1, std::memory_order_relaxed);
}
bool T_Thread::allQueuesEmpty() {
	if (!local_queue_.empty())
		return false;
	if (!overflow_.empty())
		return false;
	if (!inboxes_local_[queue_index_]->empty())
		return false;
	if (!inboxes_[queue_index_]->empty())
		return false;
	for (const auto& q : thread_queues_) {
		if (!q->empty()) {
			return false;
		}
	}
	if (priority_queue_[0].size_approx() > 0)
		return false;
	if (priority_queue_[1].size_approx() > 0)
		return false;
	if (priority_queue_[2].size_approx() > 0)
		return false;
	if (priority_queue_[3].size_approx() > 0)
		return false;
	if (priority_queue_[4].size_approx() > 0)
		return false;
	return true;
}
bool T_Thread::ready()
{
	return ready_.load(std::memory_order_acquire);
}
#ifdef _WIN32
bool T_Thread::setAffinity(int cpu_id, int num_cores_)
{
	if (cpu_id == -1)
		return true;
	if (cpu_id < 0 || cpu_id >= num_cores_)
		return false;
	{
		mask_ = 1ULL << cpu_id;
	}
	DWORD_PTR result = SetThreadAffinityMask(native_handle_, mask_);
	if (result != 0) {
		return true;
	}
	return false;
}
#else
//implement posix later
#endif
void T_Thread::worker() {
	running_.store(true, std::memory_order_release);
	while (running_.load(std::memory_order_acquire)) {
		Task* task_to_run = nullptr;

		// Wait only if there is nothing in immediate tasks, priority queue, or worker queue
		{
			ready_.store(true, std::memory_order_release);
			std::unique_lock<std::mutex> lock(worker_mutex_);
			cv_.wait(lock, [this]() {
				return !running_.load(std::memory_order_acquire)
					|| immediate_.load(std::memory_order_acquire)
					|| (!paused_.load(std::memory_order_acquire) && !allQueuesEmpty());
				});

			if (!running_.load(std::memory_order_acquire)) break;
		}
		if (!inboxes_local_[queue_index_]->empty()) // NOT empty
		{
			while (auto sp = inboxes_local_[queue_index_]->pop()) {
				Task* t = *sp; // shared_ptr -> raw pointer
				local_queue_.push_back(t);
			}
		}

		if (!overflow_.empty()){
			while (thread_queues_[queue_index_]->size() < thread_queues_[queue_index_]->capacity()) {
				thread_queues_[queue_index_]->push_bottom(overflow_.back());
				overflow_.pop_back();
			}
		}

		//push work onto thread queue if inbox not empty
		if (!inboxes_[queue_index_]->empty()) {
			while (!inboxes_[queue_index_]->empty()) {
				if (thread_queues_[queue_index_]->size() < thread_queues_[queue_index_]->capacity()) {
					auto sp = inboxes_[queue_index_]->pop();  // sp is std::shared_ptr<Task>
					if(sp)
						thread_queues_[queue_index_]->push_bottom(*sp);
				}
				else
				{
					auto sp = inboxes_[queue_index_]->pop();  // sp is std::shared_ptr<Task>
					if (sp)
						overflow_.push_back(*sp);
				}
			}
		}
		// --- 1. Immediate task execution ---
		{
			if (immediate_task_ != nullptr) {
				if (!local_queue_.empty()) {
					while (!local_queue_.empty()) {
						Task* t = local_queue_.back();
						local_queue_.pop_back();
						thread_queues_[queue_index_]->push_bottom(t);
					}
				}
				task_to_run = immediate_task_;
				current_task = immediate_task_;
				immediate_task_ = nullptr;
				immediate_.store(false, std::memory_order_release);
			}
		}
		{
			// --- 2. Local worker queue  of set affinity---
			if (!local_queue_.empty()) {
				task_to_run = local_queue_.back();
				local_queue_.pop_back();
			}

			//--- 3 local work stealing queue of no affinity
			if (!task_to_run) {
				auto opt = thread_queues_[queue_index_]->pop_bottom();
				if (opt) {
					Task* task = *opt;
					if (!task) { std::cerr << "Null task!" << std::endl; }
					else { task_to_run = task; current_task = task; }
				}
			}

			// --- 4. Work stealing ---
			if (!task_to_run) {
				for (size_t i = 0; i < thread_queues_.size(); ++i) {
					if (i == queue_index_) continue;
					auto opt = thread_queues_[i]->steal();
					if (opt) {
						task_to_run = *opt;
						current_task = task_to_run;
						break;
					}
				}
			}
		}
		{
			// --- 5. Priority Queue ---
			bool success = false;
			Task* task;

			for (int i = 0; i < 5; i++) {
				success = priority_queue_[i].try_dequeue(task);
				if (success) {
					task_to_run = task;
					break;
				}
			}
		}


		// --- 6. Execute task if found ---
		if (task_to_run) {
			task_to_run->execute();
			popTask();
			if (task_to_run->auto_delete)
				delete task_to_run;
			task_ = nullptr;
			current_task = nullptr;
			task_to_run = nullptr;
		}
		EpochManager::instance().tick();
	}

	running_.store(false, std::memory_order_release);
	cv_worker_done_.notify_all();
}
