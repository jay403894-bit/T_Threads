#include "T_Thread.h"
//constructor 
std::unordered_map<int, std::vector<int>> T_Thread::coreGroups; // a map of core groups
unsigned int T_Thread::numCores; // number of cores
unsigned int T_Thread::groupSize; //core group size
unsigned int T_Thread::numGroups; //number of groups
bool T_Thread::firstRun = true; //first initialization flag

T_Thread::T_Thread()
	: tStatus(ThreadStatus::Pool)
{
	if (firstRun)
	{
		firstRun = false;
		numCores = std::thread::hardware_concurrency();
		if (numCores <= 8) {
			groupSize = 2;
		}
		else if (numCores <= 16) {
			groupSize = 4;
		}
		else {
			groupSize = 8;
		}
		numGroups = (numCores + groupSize - 1) / groupSize;
		coreGroups.clear();
		for (unsigned int g = 0; g < numGroups; ++g) {
			std::vector<int> group;
			for (unsigned int c = g * groupSize; c < (g + 1) * groupSize && c < numCores; ++c) {
				group.push_back(c);
			}
			coreGroups[g] = group; // key is group index 0..numGroups-1
		}
	}
	t_thread = std::thread(&T_Thread::Worker, this);
	nativeHandle = t_thread.native_handle();

}

//destructor
T_Thread::~T_Thread() {
	Stop();
	if (t_thread.joinable()) {
		t_thread.join();
	}
};

//get the thread id
std::thread::id T_Thread::GetID() {
	return t_thread.get_id();
}
//try to reserve the thread
bool T_Thread::TryReserve() {
	std::lock_guard<std::mutex> lock(threadMutex);
	if (reserved_ || task_) return false;
	reserved_ = true;
	return true;
}

//set the task
bool T_Thread::SetTask(const std::shared_ptr<BaseTask>& newTask) {
	if (!newTask) return false;
	{
		std::lock_guard<std::mutex> lock(threadMutex);
		if (!reserved_ || task_) {
			return false;
		}
		task_ = newTask;
		reserved_ = false;
		tStatus = ThreadStatus::Run;
	}
	cv.notify_one();
	return true;
}
//Stop the thread
void T_Thread::Stop() {
	std::lock_guard<std::mutex> lock(threadMutex);
	tStatus = ThreadStatus::Stop;
	cv.notify_one();
};
//return the thread's tStatus
ThreadStatus T_Thread::GetStatus()
{
	std::lock_guard<std::mutex> lock(threadMutex);
	return tStatus;
}
//set the threads tStatus
void T_Thread::SetStatus(const ThreadStatus& msg)
{
	std::lock_guard<std::mutex> lock(threadMutex);
	tStatus = msg;
}

//Release thread reservation
void T_Thread::ReleaseReservation() {
	std::lock_guard<std::mutex> lock(threadMutex);
	reserved_ = false;
}
//set cpu affinity
#ifdef _WIN32
bool T_Thread::SetAffinity(int cpuID)
{
	if (cpuID == -1) {
		return true; // leave affinity unchanged
	}
	else if (cpuID < -1) {
		return false;
	}
	mask = 1ULL << cpuID;
	DWORD_PTR result = SetThreadAffinityMask(nativeHandle, mask);
	return result != 0;
}

bool T_Thread::SetGroupAffinity(int groupID) {
	auto it = coreGroups.find(groupID);
	if (it == coreGroups.end()) return false;

	DWORD_PTR groupMask = 0;
	for (int cpuID : it->second) {
		groupMask |= 1ULL << cpuID;
	}

	DWORD_PTR result = SetThreadAffinityMask(nativeHandle, groupMask);
	return result != 0;
}
#else
 //implement posix later
#endif
//t_thread pools the thread as a Worker awaiting orders
void T_Thread::Worker() {
	while (tStatus != ThreadStatus::Stop) {
		std::shared_ptr<BaseTask> current_task;
		{
			std::unique_lock<std::mutex> lock(workerMutex);
			cv.wait(lock, [this]() { return task_ != nullptr ||
				!taskQueue.empty() ||
				tStatus == ThreadStatus::Stop; });

			if (tStatus == ThreadStatus::Stop) break;

			if (task_) {
				current_task = task_;
				task_ = nullptr;
			}
			else if (!taskQueue.empty()) {
				auto task = taskQueue.pop();
				if (task.has_value()) {
					current_task = *task;
				}
			}
		}

		if (current_task) {
			tStatus = ThreadStatus::Run;
			int groupAffinity = current_task->GetGroupAffinity();
			if (groupAffinity > -1)
				SetGroupAffinity(groupAffinity);
			else
				SetAffinity(current_task->GetCoreAffinity());
			current_task->Execute();
			tStatus = ThreadStatus::Pool;
			current_task->SetCompleted();
		}
	}
}
