#include "TaskQueue.h"
TQue TaskQueue::taskQueue;

TQue::TQue(const TQue& other) {
    std::lock_guard<std::mutex> lock_this(queueMtx);  // Lock this queue's mutex

    // Copy the contents of the queue
    queue = other.queue;
}

TQue& TQue::operator=(const TQue& other) {
    if (this != &other) {  // Self-assignment check
        std::lock_guard<std::mutex> lock_this(queueMtx);  // Lock this queue's mutex

        // Copy the contents of the queue
        queue = other.queue;
    }
    return *this;
}

bool TQue::operator==(const TQue& other) {
    std::lock_guard<std::mutex> lock_this(queueMtx);  // Lock this queue's mutex

    // Compare the contents of both queues by size and element-by-element
    if (queue.size() != other.queue.size()) {
        return false;
    }

    // Compare element by element
    auto this_copy = queue;
    auto other_copy = other.queue;

    while (!this_copy.empty() && !other_copy.empty()) {
        if (this_copy.front() != other_copy.front()) {
            return false;
        }

        this_copy.pop();
        other_copy.pop();
    }

    return true;
}

bool TQue::operator!=(const TQue& other) {
    return !(*this ==(other));
}


void TQue::push(const std::shared_ptr<BaseTask>& task) {
    std::lock_guard<std::mutex> lock(queueMtx);
    queue.push(task);
}
std::optional<std::shared_ptr<BaseTask>> TQue::pop() {
    std::unique_lock<std::mutex> lock(queueMtx);
    cv.wait(lock, [this] { return !queue.empty(); });

    if (!queue.empty()) {
        std::shared_ptr<BaseTask> task  = queue.front();
        queue.pop();
        return task;
    }

    return std::nullopt;  // Return an empty optional if the queue is empty
}
std::optional<std::shared_ptr<BaseTask>> TQue::top() {
    std::unique_lock<std::mutex> lock(queueMtx);
    cv.wait(lock, [this] { return !queue.empty(); });

    if (!queue.empty()) {
        return queue.front();
    }

    return std::nullopt;  // Return an empty optional if the queue is empty
}
bool TQue::empty() const {
    return queue.empty();
}