#include "T_Thread.h"
//constructor 
std::unordered_map<int, std::vector<int>> T_Thread::coreGroups; // a map of core groups
std::vector<int> T_Thread::coreOccupied; //vector representing core occupancy
std::vector<int> T_Thread::groupOccupancy; //vector representing core group occupancy
std::mutex T_Thread::affinityMutex; //core affinity mutex
unsigned int T_Thread::numCores; // number of cores
unsigned int T_Thread::groupSize; //core group size
unsigned int T_Thread::numGroups; //number of groups
bool T_Thread::firstRun = true; //first initialization flag
bool T_Thread::taskHasRun = false; //taskhasrun flag

//constructor
T_Thread::T_Thread()
	: tStatus(ThreadStatus::Pool)
{
	if (firstRun) {
		firstRun = false;
		numCores = std::thread::hardware_concurrency()-1;
		groupSize = (numCores <= 8) ? 2 : (numCores <= 16 ? 4 : 8);
		numGroups = (numCores + groupSize - 1) / groupSize;

		coreGroups.clear();
		for (unsigned int g = 0; g < numGroups; ++g) {
			std::vector<int> group;
			for (unsigned int c = g * groupSize; c < (g + 1) * groupSize && c < numCores; ++c) {
				group.push_back(c);
			}
			coreGroups[g] = group;
		}

		// occupancy
		coreOccupied.clear();
		coreOccupied.assign(numCores, 0);        // 0 == free, 1 == occupied

		groupOccupancy.clear();
		groupOccupancy.assign(numGroups, 0);     // occupancy count per group
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
//set group size
//SETGROUPSIZE MUST 
bool T_Thread::SetGroupSize(unsigned int size) {
	std::lock_guard<std::mutex> lock(affinityMutex); // or a static init lock
	if (size == 0 || size > numCores - 1 || taskHasRun) return false;   // sanitize input

	groupSize = size;
	numGroups = (numCores + groupSize - 1) / groupSize;

	coreGroups.clear();
	for (unsigned int g = 0; g < numGroups; ++g) {
		std::vector<int> group;
		for (unsigned int c = g * groupSize; c < (g + 1) * groupSize && c < numCores; ++c) {
			group.push_back(c);
		}
		coreGroups[g] = group;
	}
	return true;
}
//set cpu affinity
#ifdef _WIN32
bool T_Thread::SetAffinity(int cpuID)
{
	if (cpuID == -1)
		return true;
	if (cpuID < 0 || cpuID >= static_cast<int>(numCores))
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
//set group affinity
bool T_Thread::SetGroupAffinity(int groupID)
{
	auto it = coreGroups.find(groupID);
	if (it == coreGroups.end()) return false;

	std::lock_guard<std::mutex> guard(affinityMutex);

	if (groupOccupancy[groupID] >= static_cast<int>(it->second.size()))
		return false;

	DWORD_PTR groupMask = 0;
	std::vector<int> claimed; claimed.reserve(it->second.size());
	for (int cpuID : it->second) {
		if (coreOccupied[cpuID] == 0) {
			coreOccupied[cpuID] = 1;
			claimed.push_back(cpuID);
			groupMask |= (1ULL << cpuID);
		}
	}

	if (groupMask == 0) {
		// couldn't claim any core
		return false;
	}

	DWORD_PTR result = SetThreadAffinityMask(nativeHandle, groupMask);
	if (result != 0) {
		groupOccupancy[groupID] += 1;
		return true;
	}

	// OS call failed, rollback claimed cores:
	for (int cpuID : claimed) coreOccupied[cpuID] = 0;
	return false;
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
			int attempts = 0;
			if (groupAffinity > -1)
				while (!SetGroupAffinity(groupAffinity)) {
					if (++attempts < 5) {
						std::this_thread::yield(); // quick retry
					}
					else {
						std::this_thread::sleep_for(std::chrono::milliseconds(5));
						attempts = 0; // reset
					}
				}
			else
				while (!SetAffinity(current_task->GetCoreAffinity()))
				{
					if (++attempts < 5) {
						std::this_thread::yield(); // quick retry
					}
					else {
						std::this_thread::sleep_for(std::chrono::milliseconds(5));
						attempts = 0; // reset
					}
				}
			taskHasRun = true;
			current_task->Execute();                                                                                              
			tStatus = ThreadStatus::Pool;
			current_task->SetCompleted();
			{
				std::lock_guard<std::mutex> guard(affinityMutex);
				if (current_task->GetGroupAffinity() > -1) {
					int groupID = current_task->GetGroupAffinity();
					if (groupID >= 0 && groupID < (int)groupOccupancy.size())
						groupOccupancy[groupID] -= 1;
					for (int cpuID : coreGroups[groupID]) {
						if (cpuID >= 0 && cpuID < (int)coreOccupied.size())
							coreOccupied[cpuID] = 0;
					}
				}
				else if (current_task->GetCoreAffinity() > -1) {
					int cpuID = current_task->GetCoreAffinity();
					if (cpuID >= 0 && cpuID < (int)coreOccupied.size())
						coreOccupied[cpuID] = 0;
				}
			}
		}
	}
}
