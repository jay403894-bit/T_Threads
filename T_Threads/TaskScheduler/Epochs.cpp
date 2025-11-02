#include "Epochs.h"
namespace T_Threads {
	thread_local size_t thread_id = 0;
	thread_local std::vector<RetiredAlloc> retired;
	std::shared_ptr<Clock> EpochManager::clock_ = std::make_shared<Clock>();
}