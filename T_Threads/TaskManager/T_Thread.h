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
    T_Thread();
    T_Thread(const T_Thread& other) = delete;
    T_Thread& operator=(const T_Thread& other) = delete;
    ~T_Thread();
    void startWorker(int cpuAffinity);
    std::thread::id getId();
    bool setTask(const std::shared_ptr<BaseTask>& task_);
    void setQueueIndex(int index);
    void join();
    void notifyWorker();
#ifdef _WIN32
    //set cpu core affinity (singular)
    bool setAffinity(int cpu_id, std::vector<int>& core_occupied, int num_cores);
#else
    // Implement POSIX sched_setaffinity if cross-platform later
#endif
private:
    //the worker function 
    void worker(); 

    static std::mutex affinity_mutex_;
    static std::atomic<int> next_index_;
    std::atomic<bool> running_{ false };
    std::atomic<bool> ready_{ false };
    std::atomic<bool> joining_{ false };
    int queue_index_ = 0;
    std::mutex thread_mutex_;
    std::mutex worker_mutex_;
    std::mutex join_mutex_; 
    std::condition_variable cv_worker_done_;
    std::condition_variable cv_; 
    std::condition_variable cv_affinity_;
    std::shared_ptr<BaseTask> task_; 
    std::thread thread_;
    std::thread::native_handle_type native_handle_; 
#ifdef _WIN32
    DWORD_PTR mask_;
#else
    // Implement POSIX sched_setaffinity if cross-platform later
#endif

};