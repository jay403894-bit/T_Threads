#include "T_Thread.h"
//constructor 
T_Thread::T_Thread() : message(MessageType::Pool), t_thread(&T_Thread::Worker, this), globalPool(nullptr) {};

//destructor
T_Thread::~T_Thread() {
	Stop();
	if (t_thread.joinable()) {
		t_thread.join();
	}
};
//get the thread id
std::thread::id T_Thread::GetID() {
	return t_thread.get_id();
}
//try to reserve the thread
bool T_Thread::TryReserve() {
	std::lock_guard<std::mutex> lock(threadMutex);
	if (reserved_ || task_) return false;
	reserved_ = true;
	return true;
}

//set the task
bool T_Thread::SetTask(std::shared_ptr<BaseTask>& newTask) {
	if (!newTask) return false;
	{
		std::lock_guard<std::mutex> lock(threadMutex);
		if (!reserved_ || task_) {
			return false;
		}
		task_ = newTask;
		reserved_ = false;
		message = MessageType::Run;
	}
	cv.notify_one();
	return true;
}
//set the thread local storage data
void T_Thread::SetData(std::any dataIn)
{
	std::lock_guard<std::mutex> lock(dataMutex);
	data = dataIn;
};
//retrieve the thread local storage data 
std::any T_Thread::GetData() {
	std::lock_guard<std::mutex> lock(dataMutex);
	return data;
}
//Stop the thread
void T_Thread::Stop() {
	std::lock_guard<std::mutex> lock(threadMutex);
	message = MessageType::Stop;
	cv.notify_one();
};
//return the thread's message
MessageType T_Thread::GetMessage()
{
	std::lock_guard<std::mutex> lock(threadMutex);
	return message;
}
//set the threads message
void T_Thread::SetMessage(const MessageType& msg)
{
	std::lock_guard<std::mutex> lock(threadMutex);
	message = msg;
};
//Release thread reservation
void T_Thread::ReleaseReservation() {
	std::lock_guard<std::mutex> lock(threadMutex);
	reserved_ = false;
}
//t_thread pools the thread as a Worker awaiting orders
void T_Thread::Worker() {
	while (message != MessageType::Stop) {
		std::shared_ptr<BaseTask> current_task;
		{
			std::unique_lock<std::mutex> lock(threadMutex);
			cv.wait(lock, [this]() { return task_ != nullptr ||
				!taskQueue.empty() ||
				message == MessageType::Stop; });

			if (message == MessageType::Stop) break;

			if (task_) {
				current_task = task_;
				task_ = nullptr;
			}
			else if (!taskQueue.empty()) {
				auto task = taskQueue.pop();
				if (task.has_value()) {
					current_task = *task;
				}
			}
		}

		if (current_task) {
			message = MessageType::Run;
			current_task->Execute();
			message = MessageType::Pool;
			current_task->SetCompleted();
		}
	}
}
