#pragma once
#include "ChaseLevDeque.h"
#include "Task.h"

class SharedQueues {
public:
    static inline std::vector<std::unique_ptr<ChaseLevDeque<std::shared_ptr<BaseTask>>>> queues;
};

class TaskQueue {
private:
    struct Node {
        std::shared_ptr<BaseTask> data;
        std::atomic<Node*> next;
        Node(std::shared_ptr<BaseTask> d = nullptr) : data(std::move(d)), next(nullptr) {}
    };

    std::atomic<Node*> tail;  // producers push here
    Node* head;               // consumer pops from here

public:
    TaskQueue() {
        head = new Node();  // dummy node
        tail.store(head, std::memory_order_relaxed);
    }

    ~TaskQueue() {
        while (head) {
            Node* tmp = head;
            head = head->next;
            delete tmp;
        }
    }

    // Push a task
    void push(const std::shared_ptr<BaseTask>& task) {
        Node* node = new Node(task);
        node->next.store(nullptr, std::memory_order_relaxed);
        Node* prev = tail.exchange(node, std::memory_order_acq_rel);
        prev->next.store(node, std::memory_order_release);
    }

    // Pop a task
    std::shared_ptr<BaseTask> pop() {
        Node* next = head->next.load(std::memory_order_acquire);
        if (!next) return nullptr;  // empty
        std::shared_ptr<BaseTask> result = next->data;
        delete head;
        head = next;
        return result;
    }

    bool empty() const {
        return head->next.load(std::memory_order_acquire) == nullptr;
    }

    void clear() {
        while (head) {
            Node* tmp = head;
            head = head->next;
            delete tmp;
        }
        head = new Node();
        tail.store(head, std::memory_order_relaxed);
    }
};