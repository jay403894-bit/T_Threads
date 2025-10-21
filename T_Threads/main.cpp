#include <iostream>        // cout, endl
#include <vector>          // vector
#include <memory>          // shared_ptr, make_shared
#include <functional>      // function, lambdas
#include <atomic>          // atomic<int>
#include <thread>          // this_thread::sleep_for, hardware_concurrency
#include <cmath>           // sqrt

#include "TaskManager/Event.hpp"       // Event, Listener
#include "TaskManager/TaskManager.h"  // TaskManager, TaskScheduler, Task

//a listener event
class TestListener : public Listener {
    void on_event_triggered() override {
        std::cout << "event triggered\n";
    }
};
//an event
class TestEvent : public Event {
    std::string get_event_type() const override {
        return "test_event";
    }
};

//setup a task to be used as a delayed task
//the delay isnt set here its just named that for clarity to show how its being used
auto delayed_task = std::make_shared<Task>([]() {
    std::cout << "Delayed task executed at " << TaskManager::GetClock()->ToString() << "\n";
    });

// Define a simple task that prints Hello, World! and the time it was printed
std::shared_ptr<BaseTask> create_hello_world_task() {
    auto task_fn = []() {
        std::cout << "Hello, World! " << TaskManager::GetClock()->ToString() << "\n";
        };
    return std::make_shared<Task>(task_fn);
}
// a task that will be used to show dependency management
std::shared_ptr<BaseTask> dependency_task() {
    auto task_fn = []() {
        std::cout << "Dependency Fulfilled" << std::endl;
        };
    return std::make_shared<Task>(task_fn);
}
//a callback function to show how events work
void print_callback() {
    std::cout << "callback\n";
}

//a simple integer function using std function
std::function<int(int)> int_func(int a, int b) {
    return [a, b](int x) {
        return a + b + x;
        };
}

//another simple integer function using a regular function
int int_func_2(int c) {
   return c + 10;
}


//another way to build a task, int task shows how you can set and retrieve data from a task
class Int_Task : public BaseTask {
public:
    // Default constructor definition
    Int_Task() {
        // Initialize or set default values for member variables if necessary
    }

    // Constructor with a function (if needed)
    Int_Task(std::function<int(int)> task_fn) : taskFn_(task_fn) {}

    Int_Task(const Int_Task& other) = delete;
    Int_Task& operator=(const Int_Task& other) = delete;
    virtual ~Int_Task() = default;

    virtual void Execute() override {
        for (size_t i = 0; i < inputData.size() - 1; i++) {
            auto fn = int_func(inputData[i], inputData[i + 1]);
            int c = fn(5); // or any constant/int you want passed into fn(x)
            int res = int_func_2(c);
            std::cout << res << std::endl;
            outputData.push_back(res);
        }
    }

    virtual void SetData(std::vector<int> dataIn) {
        inputData = dataIn;
    }

    virtual std::vector<int> GetData() {
        return outputData;
    }

protected:
    std::vector<int> inputData;  // input data
    std::vector<int> outputData; // result data
    static std::mutex taskMutex; // Mutex for thread-safety
    std::function<int(int)> taskFn_; // function for task logic
};

// A simple CPU-heavy task
class PrimeCounterTask : public Task {
public:
    PrimeCounterTask(int start, int end, std::shared_ptr<std::atomic<int>> result)
        : Task([=]() {}), start_(start), end_(end), result_(result)
    {
        task_fn_ = [this]() { ExecuteTask(); };
    }

    void ExecuteTask() {
        int localCount = 0;
        for (int n = start_; n <= end_; ++n) {
            if (IsPrime(n)) ++localCount;
        }
        result_->fetch_add(localCount, std::memory_order_relaxed);
    }

private:
    bool IsPrime(int n) {
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



//test 1 shows the basic features and usage of the scheduler
int test1() {
    // Create a test event and 2 test listeners
    TestEvent testEvent;
    std::shared_ptr<TestListener> listener1 = std::make_shared<TestListener>();
    std::shared_ptr<TestListener> listener2 = std::make_shared<TestListener>();

    // Set callback function
    testEvent.set_callback(print_callback);
    testEvent.subscribe(listener1);
    testEvent.subscribe(listener2);

    // Create a TaskScheduler
    auto i_task = std::make_shared<Int_Task>(); // Initialize the shared pointer here

    TaskManager::Get()->ScheduleDelayedTask(delayed_task,10);

    std::vector<int> intData = { 1, 2, 3, 4 };  // Input data
    i_task->SetData(intData);  // Now it's safe to call SetData()
   auto dep_task = dependency_task();
    dep_task.get()->AddDependency(i_task);
    TaskManager::Get()->AddTask(dep_task);
    // Create the Hello World task
    auto hello_task = create_hello_world_task();
    // Add the periodic task to the scheduler (500ms interval)
    TaskManager::Get()->ScheduleTask(hello_task, 500);
    //notify the test event
    testEvent.notify();

    //let the thread sleep to run automated tasks for 5 seconds
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Add the Int_Task to the TaskManager
    TaskManager::Get()->AddTask(i_task);

    // Run for 5 seconds to process tasks
    std::this_thread::sleep_for(std::chrono::seconds(5));
    //stop the periodic task from running
    TaskManager::Get()->StopTask(hello_task->GetID());
    return 0;
}

//test2 shows the prime number task
int test2(const int& r) {
    auto scheduler = TaskManager::Get();
    const int range = r;
    const int numThreads = std::thread::hardware_concurrency()-1; //number of threads is -1 because taskscheduler uses 1
    const int chunkSize = range / numThreads;

    std::shared_ptr<std::atomic<int>> totalPrimes = std::make_shared<std::atomic<int>>(0);
    std::vector<std::shared_ptr<Task>> tasks;
    // Create and schedule tasks
    for (int i = 0; i < numThreads; ++i) {
        int start = i * chunkSize + 1;
        int end = (i == numThreads - 1) ? range : (start + chunkSize - 1);
        auto task = make_shared<PrimeCounterTask>(start, end, totalPrimes);
        scheduler->AddTask(task);
        tasks.push_back(task);
    }
    bool allDone = true;
    //for (auto task : tasks) {
     //   while(!task->IsCompleted()){}
   // }
    for (auto task : tasks) {
        task->WaitUntilComplete();
      }
    std::cout << "Total primes from 1 to " << range << " = " << *totalPrimes << std::endl;
    return 0;
}

int main() {
    //run test 1
    test1();
    //run test 2
    test2(10000);
    //sleep the thread for a few seconds to show that the hello task was stopped successfully 
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Stop the scheduler after the test duration
    TaskManager::Get()->StopAll();
    return 0;
}