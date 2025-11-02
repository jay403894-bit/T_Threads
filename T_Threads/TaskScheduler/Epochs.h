#pragma once
#include <atomic>
#include <thread>
#include <vector>
#include "../Utilities/Clock.h"

namespace T_Threads {
	struct RetiredAlloc {
		void* ptr;
		size_t epoch;
		void (*deleter)(void*);
	};
	struct DelayedTask;
	struct PeriodicTask;
	extern thread_local std::vector<RetiredAlloc> retired;
	extern thread_local size_t thread_id;

	class EpochManager {
	private:
		static std::shared_ptr<Clock> clock_;
		std::atomic<double> threshold_ms_; // ms between epoch bumps
		std::atomic<double> last_epoch_time_; //the last time we incremented
		std::atomic<size_t> global_epoch_{ 0 };

		struct ThreadEpoch {
			std::atomic<size_t> local_epoch_{ 0 };
		};
		std::vector<ThreadEpoch*> thread_epochs_;
		EpochManager() = default;
	public:
		EpochManager(const EpochManager&) = delete;
		EpochManager& operator=(const EpochManager&) = delete;
		static EpochManager& instance() {
			static EpochManager mgr;
			return mgr;
		}
		void tick()
		{
			double nowMs = clock_->elapsedMs();
			double elapsed = nowMs - last_epoch_time_.load(std::memory_order_acquire);

			if (elapsed >= threshold_ms_)
			{
				advanceEpoch();
				last_epoch_time_.store(nowMs, std::memory_order_release);
				tryReclaim();
			}
		}

		void setThreshold(double ms) {
			threshold_ms_.store(ms, std::memory_order_release);
		}
		void reclaim(size_t safeEpoch) {
			auto it = retired.begin();
			while (it != retired.end()) {
				if (it->epoch < safeEpoch) {
					it->deleter(it->ptr);
					it = retired.erase(it);
				}
				else {
					++it;
				}
			}
		}
		void tryReclaim() {
			size_t safe = minActiveEpoch();
			reclaim(safe);
		}
		void init(size_t maxThreads)
		{
			clock_->reset();
			thread_epochs_.resize(maxThreads);
			for (size_t i = 0; i < maxThreads; i++) {
				thread_epochs_[i] = new ThreadEpoch();
			}
		}

		void enterEpoch(size_t threadId) {
			thread_epochs_[threadId]->local_epoch_.store(global_epoch_.load(std::memory_order_acquire),
				std::memory_order_release);
		}
		void leaveEpoch(size_t threadId) {
			thread_epochs_[threadId]->local_epoch_.store(SIZE_MAX, std::memory_order_release);
		}
		size_t currentEpoch() { return global_epoch_.load(std::memory_order_acquire); }
		size_t minActiveEpoch() {
			size_t minEpoch = global_epoch_.load(std::memory_order_acquire);
			for (auto& te : thread_epochs_) {
				size_t e = te->local_epoch_.load(std::memory_order_acquire);
				if (e != SIZE_MAX && e < minEpoch) minEpoch = e;
			}
			return minEpoch;
		}
		template<typename T>
		void retirePtr(T* p, size_t epoch) {
			retired.push_back({ static_cast<void*>(p), epoch,
				[](void* ptr) { delete static_cast<T*>(ptr); } });
		}

		void retireDelayed(DelayedTask* s, size_t epoch) {
			retirePtr(s, epoch);
		}
		void retirePeriodic(PeriodicTask* s, size_t epoch) {
			retirePtr(s, epoch);
		}
	private:
		void advanceEpoch() { global_epoch_.fetch_add(1, std::memory_order_acq_rel); }


	};
};