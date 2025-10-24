#ifdef _WIN32
#include <windows.h>
#else
//implement posix later
#endif
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "Task.h"
#include "TaskQueue.h"

class T_Thread :public SharedQueues {
public:
    //constructor 
    T_Thread();
    //non movable 
    T_Thread(const T_Thread& other) = delete;
    //non copyable 
    T_Thread& operator=(const T_Thread& other) = delete;
    //destructor
    ~T_Thread();
    //starts the worker
    void StartWorker(int cpuAffinity);
    //get the thread id
    std::thread::id GetID();
    // Assign a task to the thread. Returns true on success.
    bool SetTask(const std::shared_ptr<BaseTask>& task);
    //set queue index
    void SetQueueIndex(int index);
    //explicit join 
    void Join();
    //notify worker
    void NotifyWorker();
#ifdef _WIN32
    //set cpu core affinity (singular)
    bool SetAffinity(int cpuID, std::vector<int>& coreOccupied, int numCores);
#else
    // Implement POSIX sched_setaffinity if cross-platform later
#endif
private:
    //the worker function 
    void Worker(); 
    std::mutex threadMutex; //mutex to lock the thread class
    std::mutex workerMutex; //worker mutex for cv
    std::mutex joinMutex;  //join thread mutex
    std::condition_variable cvWorkerDone; //worker done cv
    std::condition_variable cv; //condition variable to notify the Worker
    std::condition_variable cvAffinity;
    std::shared_ptr<BaseTask> task_; //pointer to the task the thread is directed to
    std::thread t_thread; //the thread
    std::thread::native_handle_type nativeHandle; //native thread handle
#ifdef _WIN32
    DWORD_PTR mask;
#else
    // Implement POSIX sched_setaffinity if cross-platform later
#endif
    std::atomic<bool> running{ false }; //running flag
    std::atomic<bool> ready{ false }; //ready flag
    std::atomic<bool> joining{ false };
    int queueIndex = 0;
    static std::mutex affinityMutex;
    static std::atomic<int> nextIndex;
};