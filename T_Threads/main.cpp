#include <iostream>        // cout, endl
#include <vector>          // vector
#include <memory>          // shared_ptr, make_shared
#include <functional>      // function, lambdas
#include <atomic>          // atomic<int>
#include <thread>          // this_thread::sleep_for, hardware_concurrency
#include <cmath>           // sqrt

#include "TaskManager/Event.hpp"      
#include "TaskManager/TaskManager.h"  

//a listener event
class TestListener : public Listener {
    void on_event_triggered() override {
        std::cout << TaskManager::Instance().TimeStamp() << std::endl;
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
    std::cout << "Delayed task executed at " << TaskManager::Instance().TimeStamp() << "\n";
    });

// Define a simple task that prints Hello, World! and the time it was printed
//uses an event to print the time from the main thread -- multithread events
std::shared_ptr<BaseTask> create_hello_world_task(std::shared_ptr<Event> e) {
    auto task_fn = [e]() {
        std::cout << "Hello, World! \n";
        e->notify();
        };
    
    return std::make_shared<Task>(task_fn);
}
std::shared_ptr<BaseTask> create_hello_task() {
    auto task_fn = []() {
        std::cout << "Hello, World! \n";
        };

    return std::make_shared<Task>(task_fn);
}
// a task that will be used to show dependency management
// will display dependency fulfilled once its dependent tasks have completed
std::shared_ptr<BaseTask> dependency_task() {
    auto task_fn = []() {
        std::cout << "Dependency Fulfilled" << std::endl;
        };
    return std::make_shared<Task>(task_fn);
}
//a callback function to show how events work cross thread
void print_callback() {
    std::cout << TaskManager::Instance().TimeStamp() << std::endl;
}

//a simple integer function using std function to use with int task
std::function<int(int)> int_func(int a, int b) {
    return [a, b](int x) {
        return a + b + x;
        };
}

//another simple integer function using a regular function for the thread path
int int_func_2(int c) {
   return c + 10;
}


//this random task class shows basic i/o with the main thread
//and also how you would link a task up with functions outside
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

    //multithreaded event test
    //pointer to multiproducer single consumer queue of events 
    std::shared_ptr<TestEvent> testEvent = std::make_shared<TestEvent>();
    std::shared_ptr<TestListener> listener = std::make_shared<TestListener>();

    // Set callback function
    testEvent->subscribe(listener);
       
   
    // setup an instance of the int task 
    auto i_task = std::make_shared<Int_Task>(); // Initialize the shared pointer here


    //set the input data for the task
    std::vector<int> intData = { 1, 2, 3, 4 };  // Input data
    i_task->SetData(intData);  

    //schedule the delated task to run after 50ms
    TaskManager::Instance().ScheduleDelayedTask(delayed_task, 1000);
    //create the dependency task and wait for int task and delayed_task to complete
    auto dep_task = dependency_task();
    std::shared_ptr<std::vector<std::shared_ptr<BaseTask>>> deps = std::make_shared<std::vector<std::shared_ptr<BaseTask>>>();
    deps->push_back(i_task);
    deps->push_back(delayed_task);
    TaskManager::Instance().AddTask(dep_task,0,deps);
    // Create the Hello World task and pass in the test event
    auto hello_task = create_hello_world_task(testEvent);
    // Add the periodic task to the scheduler (500ms interval)
    TaskManager::Instance().ScheduleTask(hello_task, 100);

    // Add the Int_Task to the TaskManager
    TaskManager::Instance().AddTask(i_task);

    //wait for the dependent task to complete 
    dep_task->WaitUntilComplete();
    // you can choose to just stop the periodic task with 
   // TaskManager::Get()->StopTask(hello_task->GetID());
    //but if you want to shut it down completely temporarily you can
    TaskManager::Instance().Join();
    //to join and collpase the entire pool
    //however this incurs a small overhead to restart so only do this if you 
    //dont intend to continue using the task manager for awhile 
    //here for testing purposes we just join it and restart
    
    //before exiting if you want the event to stop notifying you have to unsubscribe
    //otherwise the event will continue calling back past the scope of this function
    testEvent->unsubscribe(listener);
 //   std::this_thread::yield();
    TaskManager::Instance().StopTask(hello_task->GetID());
    return 0;
}

//test2 shows the prime number task
int test2(const int& r) {
    const int range = r;
    const int numThreads = std::thread::hardware_concurrency()-2; //number of threads is -1 because taskscheduler uses 1
    const int chunkSize = range / numThreads;

    std::shared_ptr<std::atomic<int>> totalPrimes = std::make_shared<std::atomic<int>>(0);
    std::vector<std::shared_ptr<Task>> tasks;
    // Create and schedule tasks
    for (int i = 0; i < numThreads; ++i) {
        int start = i * chunkSize + 1;
        int end = (i == numThreads - 1) ? range : (start + chunkSize - 1);
        auto task = make_shared<PrimeCounterTask>(start, end, totalPrimes);
        TaskManager::Instance().AddTask(task);
        tasks.push_back(task);
    }
    bool allDone = true;

    for (auto task : tasks) {
        task->WaitUntilComplete();
    }
    std::cout << "Total primes from 1 to " << range << " = " << *totalPrimes << std::endl;
    return 0;
}

int test3() {
    auto hello_task = create_hello_world_task(std::make_shared<TestEvent>());
    std::vector<std::shared_ptr<BaseTask>> tasks;
    // Add the periodic task to the scheduler (500ms interval)
    auto enqueueToMainTask = std::make_shared<Task>([hello_task]() {
        TaskManager::Instance().EnqueueToMain(hello_task);
        });
    int tasksadded = 0;
    for (int i = 0; i < 100; ++i) {
        TaskManager::Instance().AddTask(enqueueToMainTask);
        tasks.push_back(hello_task);
        tasksadded++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (tasksadded > tasks.size())
        std::cout << "Missed tasks!\n";
    else
        std::cout << "All Tasks completed!\n";

    TaskManager::Instance().ProcessMainThreadTasks();

    return 0;
}

int main() {
    //run test 1
    test1();
    //run test 2
    //prime number test
    test2(1000000);
    //run test 3 
    //main thread enqueue test
    test3();

    return 0;
}