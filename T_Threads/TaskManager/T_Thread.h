#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "Task.h"
#include "TaskQueue.h"
#ifdef _WIN32
#include <windows.h>
#else
//implement posix later
#endif

enum class ThreadStatus {
    Run,
    Pool,
    Stop                                                                    
};

struct CoreGroup {
    int groupID;
    std::vector<int> cores; // core indices in this group
};

class T_Thread :public TaskQueue {
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
    void StartWorker();
    //get the thread id
    std::thread::id GetID();
    // Assign a task to the thread. Returns true on success.
    bool SetTask(const std::shared_ptr<BaseTask>& task)                                   ;
    //Stop the thread
    void Stop();
    //return the thread's tStatus
    ThreadStatus GetStatus();
    //set the tStatus
    void SetStatus(const ThreadStatus& msg);
    // Attempt to reserve the thread for assignment. Returns true if reserved.
    bool TryReserve();
    // Release reservation (scheduler calls this if it fails to set the task)
    void ReleaseReservation();
    //manually set the group size
    bool SetGroupSize(unsigned int size);
private:
#ifdef _WIN32
    //set cpu core affinity (singular)
    bool SetAffinity(int cpuID);
    //set cpu core affinity by group
    bool SetGroupAffinity(int coreGroupID);
#else
    // Implement POSIX sched_setaffinity if cross-platform later
#endif
    //t_thread pools the thread as a Worker awaiting orders
    void Worker();
    std::mutex threadMutex; //mutex to lock the thread class
    std::mutex workerMutex; //worker mutex for cv
    std::condition_variable cv; //condition variable to notify the Worker
    std::condition_variable cvAffinity;
    std::shared_ptr<BaseTask> task_; //pointer to the task the thread is directed to
    ThreadStatus tStatus; //the threads tStatus as pooling, running, or stopping
    std::thread t_thread; //the thread
    bool reserved_ = false; // reserved by scheduler for imminent assignment
    std::thread::native_handle_type nativeHandle; //native thread handle
#ifdef _WIN32
    DWORD_PTR mask;
#else
    // Implement POSIX sched_setaffinity if cross-platform later
#endif
    bool group = false;
    static std::unordered_map<int, std::vector<int>> coreGroups; // a map of core groups
    static std::vector<int> coreOccupied;
    static std::vector<int> groupOccupancy;
    static std::mutex affinityMutex;
    static unsigned int numCores; // number of cores
    static unsigned int groupSize; //core group size
    static unsigned int numGroups; //number of groups
    static bool firstRun; //flag for first initialization of statics
    static bool taskHasRun; // task has run flag for setgroupsize
};