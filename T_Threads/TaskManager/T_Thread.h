#include <mutex>
#include <condition_variable>
#include <thread>
#include <any>
#include "Task.h"
#include "TaskQueue.h"
#ifdef _WIN32
#include <windows.h>
#else
//implement posix later
#endif


enum class MessageType {
    Pause,
    Run,
    Pool,
    Stop,
    Task
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
    //get the thread id
    std::thread::id GetID();
    // Assign a task (copy) to the thread. Returns true on success.
    bool SetTask(std::shared_ptr<BaseTask>& task);
    //set the data
    void SetData(std::any dataIn);
    //get the data
    std::any GetData();
    //Stop the thread
    void Stop();
    //return the thread's message
    MessageType GetMessage();
    void SetMessage(const MessageType& msg);
    // Attempt to reserve the thread for assignment. Returns true if reserved.
    bool TryReserve();
    // Release reservation (scheduler calls this if it fails to set the task)
    void ReleaseReservation();
    //set cpu core affinity 
#ifdef _WIN32
    bool SetAffinity(int cpuID);
#else
    // Implement POSIX sched_setaffinity if cross-platform later
#endif
private:

    //t_thread pools the thread as a Worker awaiting orders
    void Worker();
    std::mutex threadMutex; //mutex to lock the thread class
    std::condition_variable cv; //condition variable to notify the Worker
    std::shared_ptr<BaseTask> task_; //pointer to the task the thread is directed to
    MessageType message; //the threads message as pooling, running, or stopping
    std::thread t_thread; //the thread
    std::any data; //thread local storage 
    std::mutex dataMutex; //mutex for thread local storage
    bool reserved_ = false; // reserved by scheduler for imminent assignment
    std::thread::native_handle_type nativeHandle; //native thread handle
#ifdef _WIN32
    DWORD_PTR mask;
#else
    // Implement POSIX sched_setaffinity if cross-platform later
#endif
};