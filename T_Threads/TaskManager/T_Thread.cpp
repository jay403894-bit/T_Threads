#include "T_Thread.h"
#include <iostream>
std::mutex T_Thread::affinityMutex;

//constructor
T_Thread::T_Thread(){
}
//destructor
T_Thread::~T_Thread() {
}
void T_Thread::StartWorker(int cpuAffinity)
{

	auto ready = std::make_shared<std::atomic<bool>>(false);

	t_thread = std::thread([this, ready]() { // <-- capture by value
		while (!ready->load(std::memory_order_acquire)) std::this_thread::yield();
		this->Worker();
		});
	nativeHandle = t_thread.native_handle();
	// set affinity
#ifdef _WIN32
	DWORD_PTR mask = 1ULL << cpuAffinity;
	SetThreadAffinityMask(nativeHandle, mask);
#endif

	// signal the thread it can start
	ready->store(true, std::memory_order_release);
};

//get the thread id
std::thread::id T_Thread::GetID() {
	return t_thread.get_id();
}

//set the task
bool T_Thread::SetTask(const std::shared_ptr<BaseTask>& newTask) {
	if (!newTask) return false;
	{
		std::lock_guard<std::mutex> lock(threadMutex);
		if (!task_) {
			return false;
		}
		task_ = newTask;
	}
	cv.notify_one();
	return true;
}

void T_Thread::SetQueueIndex(int index)
{
	std::lock_guard<std::mutex> lock(threadMutex);
	queueIndex = index;
};

//explicit join
void T_Thread::Join() {
	// avoid concurrent Join()
	bool expected = false;
	if (!joining.compare_exchange_strong(expected, true)) {
		return;
	}
	// request stop and wake worker
	{
		std::lock_guard<std::mutex> lock(threadMutex);
		running.store(false, std::memory_order_release);
	}
	cv.notify_one();

	// bounded wait for running flag to clear (worker exit)
	const int timeoutMs = 5000;
	const int pollMs = 10;
	int waited = 0;
	while (running.load(std::memory_order_acquire) && waited < timeoutMs) {
		std::this_thread::sleep_for(std::chrono::milliseconds(pollMs));
		waited += pollMs;
	}


	// move thread out under lock, then join outside
	std::thread localThread;
	{
		std::lock_guard<std::mutex> lock(threadMutex);
		if (t_thread.joinable()) {
			localThread = std::move(t_thread);
		}
		else {
			joining.store(false, std::memory_order_release);
			return;
		}
	}
	if (localThread.joinable()) {
		localThread.join();
	}
	joining.store(false, std::memory_order_release);
}


void T_Thread::NotifyWorker()
{
	std::lock_guard<std::mutex> lock(threadMutex);
	cv.notify_one();
}
//set cpu affinity
#ifdef _WIN32
bool T_Thread::SetAffinity(int cpuID, std::vector<int>& coreOccupied, int numCores)
{
	if (cpuID == -1)
		return true;
	if (cpuID < 0 || cpuID >= numCores)
		return false;

	std::lock_guard<std::mutex> guard(affinityMutex);

	if (coreOccupied[cpuID] != 0) {
		// already occupied
		return false;
	}

	// mark as occupied
	coreOccupied[cpuID] = 1;

	mask = 1ULL << cpuID;
	DWORD_PTR result = SetThreadAffinityMask(nativeHandle, mask);
	if (result != 0) {
		return true;
	}

	// failed to set OS affinity — revert
	coreOccupied[cpuID] = 0;
	return false;
}
#else
 //implement posix later
#endif
//t_thread pools the thread as a Worker awaiting orders
void T_Thread::Worker() {
	running.store(true, std::memory_order_release);
	while (running.load(std::memory_order_acquire)) {
		std::shared_ptr<BaseTask> task;

		// First, try to get a task from our own queue
		{
			std::unique_lock<std::mutex> lock(workerMutex);
			cv.wait(lock, [this]() {
				return !running.load(std::memory_order_acquire) || !queues[queueIndex]->empty();
				});

			if (!running.load(std::memory_order_acquire)) break;

			auto opt = queues[queueIndex]->pop_bottom();
			if (opt) task = *opt;
		}

		// If own queue empty, try stealing from others
		if (!task) {
			size_t numQueues = queues.size();
			for (size_t i = 0; i < numQueues; ++i) {
				if (i == queueIndex) continue; // skip self
				auto opt = queues[i]->steal();
				if (opt) {
					task = *opt;
					break;
				}
			}
		}

		if (!task) continue; // nothing to do, loop again

		task->Execute();
		task->SetCompleted();
	}
	running.store(false, std::memory_order_release);
}
