#pragma once
#include <functional>
#include <atomic>
#include "../Utilities/Clock.h"

namespace T_Threads {
    struct Task {
        using Func = void(*)(void*);

        Func fn;
        void* data = nullptr;
        std::atomic<bool> stop_flag{ false };
        std::function<void()> onComplete;
        std::atomic<bool> complete{ false };
        bool auto_delete = false;

        std::atomic<int> dependencies_left{ 0 };
        std::vector<Task*> dependents;
        std::vector<Task*> dependencies;
        std::function<void(Task*)> notify_dependents;

        Task(Func f, void* d = nullptr, bool del = false)
            : fn(f), data(d), auto_delete(del) {
        }

        inline void execute() noexcept {
            fn(data);
            complete.store(true,std::memory_order_release);
            if (onComplete) onComplete();
            if (notify_dependents) notify_dependents(this);
        }

        inline void addDependency(Task* dep) {
            ++dependencies_left;
            dependencies.push_back(dep);
            dep->dependents.push_back(this);
        }
        inline void stop() {
            stop_flag.store(true, std::memory_order_release);
        }
    };
    template<typename F>
    class LambdaTask : public Task {
    public:
        LambdaTask(F&& f)
            : Task(nullptr, nullptr, true)  
        {
            struct Wrapper { F f; };
            Wrapper* w = new Wrapper{ std::forward<F>(f) };

            this->fn = [](void* ptr) {
                Wrapper* w = static_cast<Wrapper*>(ptr);
                w->f();
                delete w;
                };

            this->data = w;
        }
    };
    struct PeriodicTask {
        Task* task;
        std::string id;
        float scheduled_time;
        float interval;
        Clock* clock;
        bool cancelled = false;
        PeriodicTask() : task(nullptr), scheduled_time(0.0f), interval(0.0f), clock(nullptr)
        {
        };
        PeriodicTask(Task*& task, std::string id, float inter, Clock*& timer)
            : task(task), id(id), interval(inter), clock(timer) {
            scheduled_time = clock->elapsedMs();
            task->auto_delete = false;
        }
        PeriodicTask(const PeriodicTask& other) {
            task = other.task;
            scheduled_time = other.scheduled_time;
            interval = other.interval;
            clock = other.clock;
            cancelled = other.cancelled;
        }
        void stop() {
            cancelled = true;
        }
        bool isTimeToRun() const {
            return !cancelled && (clock->elapsedMs() >= scheduled_time);
        }
        void updateExecutionTime() {
            scheduled_time = clock->elapsedMs() + interval;
        }
    };
    struct DelayedTask {
        Task* task;
        double scheduled_time;
        Clock* clock;
        DelayedTask() : task(nullptr), scheduled_time(0.0), clock(nullptr) {
        };
        DelayedTask(Task*& task, float delay_ms,Clock*& timer)
            : task(task), clock(timer) {
            scheduled_time = clock->elapsedMs() + delay_ms;
            task->auto_delete = true;
        }
        DelayedTask(const DelayedTask& other) {
            task = other.task;
            scheduled_time = other.scheduled_time;
            clock = other.clock;
        }
        bool isTimeToRun() const {
            return (clock->elapsedMs() >= scheduled_time);
        }
    };
    struct PeriodicTaskCompare {
        // take pointers, not references
        bool operator()(const PeriodicTask* a, const PeriodicTask* b) const {
            return a->scheduled_time > b->scheduled_time;
            // min-heap: smallest scheduled_time at front
        }
    };

    struct DelayedTaskCompare {
        bool operator()(const DelayedTask* a, const DelayedTask* b) const {
            return a->scheduled_time > b->scheduled_time;
        }
    };
 
};