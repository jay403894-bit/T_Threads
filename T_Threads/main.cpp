#include <iostream>        
#include <vector>          
#include <memory>          
#include <functional>      
#include <atomic>         
#include <thread>         
#include <cmath>          
#include "TaskScheduler/TaskScheduler.h"  
#include <assert.h>

using T_Threads::TaskDAG;
using T_Threads::Task;
using T_Threads::TaskScheduler;

void forkedTask(void* data) {
    int ctr=0;
    while (true)
    {
        if (T_Threads::current_task->stop_flag.load(std::memory_order_acquire))
            break;

        ctr = (ctr + 1) % 7;
        int capturedCtr = ctr; // new variable per iteration
        TaskScheduler::instance().submitPQ(capturedCtr, [capturedCtr]() {
            std::cout << "priority: " << capturedCtr << std::endl;
            });
        TaskScheduler::instance().submitLocal([]() {
            std::cout << "local from offthread "<< std::endl;
            });
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// Example function for tasks
void simpleTaskFn(void* data) {
    int* id = static_cast<int*>(data);
    std::this_thread::yield();
    std::cout << "Task " << *id
        << " executed on thread " << std::this_thread::get_id()
        << std::endl << std::flush;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void fork() {
    TaskScheduler& scheduler = TaskScheduler::instance();

    Task* forkt = new Task(forkedTask, nullptr, false);


    scheduler.submitFork(3,forkt);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    forkt->stop();
    while (!forkt->complete.load()) {
        std::this_thread::yield();
    }
    delete forkt;
}

void hellotask(void* data) {
    auto str = reinterpret_cast<char*>(data);
    std::cout << "Hello " << str << std::endl;
}

void depA(void* data) {
    std::cout << "depA\n";
}
void depB(void* data) {
    std::cout << "depB\n";
}
void depC(void* data) {
    std::cout << "depC\n";
}
void depD(void* data) {
    std::cout << "depD\n";
}

int main() {
    TaskScheduler& scheduler = TaskScheduler::instance();
    
    scheduler.startPool(1);
    
    const int iterations = 10;
    const int tasksPerIteration = 100;
    int ctr = 0;

    std::vector<Task*> tasks;
    for (int it = 0; it < iterations; ++it) {
        for (int t = 0; t < tasksPerIteration; ++t) {
            // allocate id on heap to keep it alive for the function
            int* id = new int(t);

            Task* task = new Task(simpleTaskFn, id, false);

             scheduler.submitLocal(task);
             tasks.push_back(task);
             std::this_thread::yield();
        }
        std::this_thread::yield();
    }    

 
    std::cout << "Stress test completed." << std::endl;
    
    char n[5] = "Josh";
    Task* hello = new Task(hellotask, static_cast<void*>(&n), true);

    scheduler.submitPeriodic(50.0, hello);

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    hello->stop();
     std::this_thread::sleep_for(std::chrono::milliseconds(300));

    fork();
    
    for (auto t : tasks) {
        while (!t->complete.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    }
    for (auto t : tasks) {
        delete t;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return 0;
}