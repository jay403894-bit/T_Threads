#include "T_Thread.h"
#include <iostream>
std::mutex T_Thread::affinity_mutex_;

T_Thread::T_Thread(){
}
T_Thread::~T_Thread() {
}
void T_Thread::startWorker(int cpu_affinity)
{
	auto ready_ = std::make_shared<std::atomic<bool>>(false);
	thread_ = std::thread([this, ready_]() {
		while (!ready_->load(std::memory_order_acquire)) std::this_thread::yield();
		this->worker();
		});
	native_handle_ = thread_.native_handle();
#ifdef _WIN32
	DWORD_PTR mask_ = 1ULL << cpu_affinity;
	SetThreadAffinityMask(native_handle_, mask_);
#endif
	ready_->store(true, std::memory_order_release);
};
std::thread::id T_Thread::getId() {
	return thread_.get_id();
}
bool T_Thread::setTask(const std::shared_ptr<BaseTask>& new_task) {
	if (!new_task) return false;
	{
		std::lock_guard<std::mutex> lock(thread_mutex_);
		if (!task_) {
			return false;
		}
		task_ = new_task;
	}
	cv_.notify_one();
	return true;
}
void T_Thread::setQueueIndex(int index)
{
	std::lock_guard<std::mutex> lock(thread_mutex_);
	queue_index_ = index;
};
void T_Thread::join() {
	bool expected = false;
	if (!joining_.compare_exchange_strong(expected, true)) {
		return;
	}
	{
		std::lock_guard<std::mutex> lock(thread_mutex_);
		running_.store(false, std::memory_order_release);
	}
	cv_.notify_one();
	const int timeout_ms = 5000;
	std::unique_lock<std::mutex> lock(join_mutex_);
	cv_worker_done_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
		[this] { return !running_.load(std::memory_order_acquire); });
	std::thread local_thread;
	{
		std::lock_guard<std::mutex> lock(thread_mutex_);
		if (thread_.joinable()) {
			local_thread = std::move(thread_);
		}
		else {
			joining_.store(false, std::memory_order_release);
			return;
		}
	}
	if (local_thread.joinable()) {
		local_thread.join();
	}
	joining_.store(false, std::memory_order_release);
}
void T_Thread::notifyWorker()
{
	std::lock_guard<std::mutex> lock(thread_mutex_);
	cv_.notify_one();
}
#ifdef _WIN32
bool T_Thread::setAffinity(int cpu_id, std::vector<int>& core_occupied_, int num_cores_)
{
	if (cpu_id == -1)
		return true;
	if (cpu_id < 0 || cpu_id >= num_cores_)
		return false;
	std::lock_guard<std::mutex> guard(affinity_mutex_);
	if (core_occupied_[cpu_id] != 0) {
		return false;
	}
	core_occupied_[cpu_id] = 1;
	mask_ = 1ULL << cpu_id;
	DWORD_PTR result = SetThreadAffinityMask(native_handle_, mask_);
	if (result != 0) {
		return true;
	}
	core_occupied_[cpu_id] = 0;
	return false;
}
#else
 //implement posix later
#endif
void T_Thread::worker() {
	running_.store(true, std::memory_order_release);
	while (running_.load(std::memory_order_acquire)) {
		std::shared_ptr<BaseTask> task_;
		{
			std::unique_lock<std::mutex> lock(worker_mutex_);
			cv_.wait(lock, [this]() {
				return !running_.load(std::memory_order_acquire) || !queues_[queue_index_]->empty();
				});
			if (!running_.load(std::memory_order_acquire)) break;
			auto opt = queues_[queue_index_]->pop_bottom();
			if (opt) task_ = *opt;
		}
		if (!task_) {
			size_t numQueues = queues_.size();
			for (size_t i = 0; i < numQueues; ++i) {
				if (i == queue_index_) continue;
				auto opt = queues_[i]->steal();
				if (opt) {
					task_ = *opt;
					break;
				}
			}
		}
		if (!task_) continue; 
		task_->execute();
		task_->setCompleted();
	}
	running_.store(false, std::memory_order_release);
}
