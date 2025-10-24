#include <iostream>        
#include <vector>          
#include <memory>          
#include <functional>      
#include <atomic>         
#include <thread>         
#include <cmath>          
#include "TaskManager/Event.hpp"      
#include "TaskManager/TaskManager.h"  

class TestListener : public Listener {
    void onEventTriggered() override {
        std::cout << TaskManager::instance().timeStamp() << std::endl;
    }
};
class TestEvent : public Event {
    std::string get_event_type() const override {
        return "test_event";
    }
};
auto delayed_task = std::make_shared<Task>([]() {
    std::cout << "Delayed task executed at " << TaskManager::instance().timeStamp() << "\n";
    });
std::shared_ptr<BaseTask> create_hello_world_task(std::shared_ptr<Event> e) {
    auto task_fn = [e]() {
        std::cout << "Hello, World! \n";
        e->notify();
        };
    return std::make_shared<Task>(task_fn);
}
std::shared_ptr<BaseTask> createHelloTask() {
    auto task_fn = []() {
        std::cout << "Hello, World! \n";
        };
    return std::make_shared<Task>(task_fn);
}
std::shared_ptr<BaseTask> dependency_task() {
    auto task_fn = []() {
        std::cout << "Dependency Fulfilled" << std::endl;
        };
    return std::make_shared<Task>(task_fn);
}
void print_callback() {
    std::cout << TaskManager::instance().timeStamp() << std::endl;
}
std::function<int(int)> int_func(int a, int b) {
    return [a, b](int x) {
        return a + b + x;
        };
}
int int_func_2(int c) {
   return c + 10;
}
class Int_Task : public BaseTask {
public:
    Int_Task() {
    }

    Int_Task(std::function<int(int)> task_fn) : taskFn_(task_fn) {}
    Int_Task(const Int_Task& other) = delete;
    Int_Task& operator=(const Int_Task& other) = delete;
    virtual ~Int_Task() = default;
    virtual void execute() override {
        for (size_t i = 0; i < input_data_.size() - 1; i++) {
            auto fn = int_func(input_data_[i], input_data_[i + 1]);
            int c = fn(5); 
            int res = int_func_2(c);
            std::cout << res << std::endl;
            output_data_.push_back(res);
        }
    }
    virtual void setData(std::vector<int> dataIn) {
        input_data_ = dataIn;
    }
    virtual std::vector<int> getData() {
        return output_data_;
    }
protected:
    std::vector<int> input_data_; 
    std::vector<int> output_data_; 
    static std::mutex task_mutex_; 
    std::function<int(int)> taskFn_; 
};
class PrimeCounterTask : public Task {
public:
    PrimeCounterTask(int start, int end, std::shared_ptr<std::atomic<int>> result)
        : Task([=]() {}), start_(start), end_(end), result_(result)
    {
        task_fn_ = [this]() { executeTask(); };
    }
    void executeTask() {
        int localCount = 0;
        for (int n = start_; n <= end_; ++n) {
            if (isPrime(n)) ++localCount;
        }
        result_->fetch_add(localCount, std::memory_order_relaxed);
    }
private:
    bool isPrime(int n) {
        if (n <= 1) return false;
        int limit = static_cast<int>(sqrt(n));
        for (int i = 2; i <= limit; ++i) {
            if (n % i == 0) return false;
        }
        return true;
    }
    int start_;
    int end_;
    std::shared_ptr<std::atomic<int>> result_;
};

int test1() {
    std::shared_ptr<TestEvent> testEvent = std::make_shared<TestEvent>();
    std::shared_ptr<TestListener> listener_ = std::make_shared<TestListener>();
    testEvent->subscribe(listener_);
    auto i_task = std::make_shared<Int_Task>(); 
    std::vector<int> intData = { 1, 2, 3, 4 };  
    i_task->setData(intData);  
    TaskManager::instance().scheduleDelayedTask(delayed_task, 1000);
    auto dep_task = dependency_task();
    std::shared_ptr<std::vector<std::shared_ptr<BaseTask>>> deps = std::make_shared<std::vector<std::shared_ptr<BaseTask>>>();
    deps->push_back(i_task);
    deps->push_back(delayed_task);
    TaskManager::instance().addTask(dep_task,0,deps);
    auto hello_task = create_hello_world_task(testEvent);
    TaskManager::instance().scheduleTask(hello_task, 100);
    TaskManager::instance().addTask(i_task);
    dep_task->waitUntilComplete();
    TaskManager::instance().join();
    testEvent->unsubscribe(listener_);
    TaskManager::instance().stopTask(hello_task->getId());
    return 0;
}
int test2(const int& r) {
    const int range = r;
    const int numThreads = std::thread::hardware_concurrency()-2; //number of threads is -1 because taskscheduler uses 1
    const int chunkSize = range / numThreads;
    std::shared_ptr<std::atomic<int>> totalPrimes = std::make_shared<std::atomic<int>>(0);
    std::vector<std::shared_ptr<Task>> tasks;
    for (int i = 0; i < numThreads; ++i) {
        int start = i * chunkSize + 1;
        int end = (i == numThreads - 1) ? range : (start + chunkSize - 1);
        auto task_ = make_shared<PrimeCounterTask>(start, end, totalPrimes);
        TaskManager::instance().addTask(task_);
        tasks.push_back(task_);
    }
    bool allDone = true;

    for (auto task_ : tasks) {
        task_->waitUntilComplete();
    }
    std::cout << "Total primes from 1 to " << range << " = " << *totalPrimes << std::endl;
    return 0;
}

int test3() {
    auto hello_task = create_hello_world_task(std::make_shared<TestEvent>());
    std::vector<std::shared_ptr<BaseTask>> tasks;
    auto enqueueToMainTask = std::make_shared<Task>([hello_task]() {
        TaskManager::instance().enqueueToMain(hello_task);
        });
    int tasksadded = 0;
    for (int i = 0; i < 100; ++i) {
        TaskManager::instance().addTask(enqueueToMainTask);
        tasks.push_back(hello_task);
        tasksadded++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (tasksadded > tasks.size())
        std::cout << "Missed tasks!\n";
    else
        std::cout << "All Tasks completed!\n";
    TaskManager::instance().processMainThreadTasks();
    return 0;
}

int main() {
    test1();
    test2(1000000);
    test3();
    return 0;
}