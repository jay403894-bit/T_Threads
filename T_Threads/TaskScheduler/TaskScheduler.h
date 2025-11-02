#pragma once
#define NOMINMAX
#include <thread>
#include <array>
#include <condition_variable>
#include <vector>
#include <mutex>
#include <queue>
#include <algorithm>
#include <chrono>
#include "T_Thread.h"
#include "SharedQueues.h"
#include "Task.h"
#include "MPSCQueue.h"
#include "Epochs.h"
#include "../Utilities/Exceptions.h"
#include "../Utilities/Clock.h"

namespace T_Threads {


	class TaskScheduler : public SharedQueues {
	public:
		static TaskScheduler& instance() {
			static TaskScheduler instance;
			return instance;
		}
		~TaskScheduler();
		bool enqueueToMain(Task*& task);
		void processMainThread();
		void join();
		void notifyAll();
		void startPool(uint8_t clock_worker_cpu_id);
		bool submitLocal(Task*& task);
		bool submitLocal(uint8_t cpu_affinity, Task*& task);
		bool submitPQ(Task*& task);
		bool submitPQ(uint8_t priority, Task*& task);
		bool submitFork(size_t cpu_affinity,Task*& task);
		bool submitPeriodic(std::string id, double ms, Task*& task);
		bool submitDelayed(double ms, Task*& task);
		void stop();
		Clock* getClock();
		template <class F>
		void submitLocal(F&& f) {
			Task* t = new LambdaTask(std::forward<F>(f));
			pushLocal(t);
		}
		template <class F>
		void submitLocal(uint8_t cpu_affinity, F&& f) {
			Task* t = new LambdaTask(std::forward<F>(f));
			pushLocal(t, cpu_affinity);
		}
		template <class F>
		void submitPQ(F&& f) {
			Task* t = new LambdaTask(std::forward<F>(f));
			push(t);
		}
		template <class F>
		void submitPQ(uint8_t priority, F&& f) {
			Task* t = new LambdaTask(std::forward<F>(f));
			push(t, priority);
		}
		template <class F>
		void submitFork(size_t coreID, F&& f) {
			Task* t = new LambdaTask(std::forward<F>(f));
			pushToCore(coreID, t);
		}


	private:
		static void worker(void* data);
		bool pushPeriodic(std::string id, double ms, Task*& task);
		bool pushDelayed(double ms, Task*& task);
		bool pushLocal(Task*& task, uint8_t cpuaffinity = 0);
		bool push(Task*& task, uint8_t priority = 3);
		bool pushToCore(size_t core_id, Task*& task);
		void bootstrap(size_t core_id, Task*& task);
		TaskScheduler();
		int pickNextWorker();
		
		
		Task* worker_task = nullptr;
		std::vector<DelayedTask*> delayed_heap_;
		std::vector<PeriodicTask*> periodic_heap_;
		Clock* clock_;
		inline static std::atomic<bool> constructed_{ false };
		std::atomic<bool> pool_active_{ false };
		std::atomic<int> next_worker_{ 0 };
		std::atomic<bool> stop_flag_{ false };
		std::atomic<int> next_index_{ -1 };
		std::vector<std::shared_ptr<T_Thread>> workers_;
		std::vector<std::unique_ptr<std::atomic<bool>>> immediate_cores_in_use;
		MPSCQueue<Task*> main_queue_;
		MPSCQueue<DelayedTask*> delayed_inbox_;
		MPSCQueue<PeriodicTask*> periodic_inbox_;
		std::condition_variable cv_;
		std::thread worker_thread_;
		std::mutex pool_mutex_;
		std::mutex worker_mutex_;
		std::atomic<int> cycles_since_switched{ 0 };
	};
};
