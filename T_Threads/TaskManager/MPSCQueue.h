#pragma once
#include <atomic>
#include <memory>

template <typename T>
class MPSCQueue {
private:
    struct Node {
        std::shared_ptr<T> data;
        std::atomic<Node*> next;
        Node(std::shared_ptr<T> d = nullptr) : data(std::move(d)), next(nullptr) {}
    };

    std::atomic<Node*> tail;  // producers push here
    Node* head;                // consumer pops from here

public:
    MPSCQueue() {
        head = new Node();  // dummy node
        tail.store(head, std::memory_order_relaxed);
    }

    ~MPSCQueue() {
        while (head) {
            Node* tmp = head;
            head = head->next;
            delete tmp;
        }
    }

    // push by raw object (copied into a shared_ptr internally)
    void push(const T& item) {
        Node* node = new Node(std::make_shared<T>(item));
        node->next.store(nullptr, std::memory_order_relaxed);
        Node* prev = tail.exchange(node, std::memory_order_acq_rel);
        prev->next.store(node, std::memory_order_release);
    }

    // push by move
    void push(T&& item) {
        Node* node = new Node(std::make_shared<T>(std::move(item)));
        node->next.store(nullptr, std::memory_order_relaxed);
        Node* prev = tail.exchange(node, std::memory_order_acq_rel);
        prev->next.store(node, std::memory_order_release);
    }

    // push an existing shared_ptr
    void push(std::shared_ptr<T> item) {
        Node* node = new Node(std::move(item));
        node->next.store(nullptr, std::memory_order_relaxed);
        Node* prev = tail.exchange(node, std::memory_order_acq_rel);
        prev->next.store(node, std::memory_order_release);
    }

    // Consumer: pop task_
    std::shared_ptr<T> pop() {
        Node* next = head->next.load(std::memory_order_acquire);
        if (!next) return nullptr;  // queue_ empty
        std::shared_ptr<T> result = next->data;
        delete head;
        head = next;
        return result;
    }

    bool empty() const {
        return head->next.load(std::memory_order_acquire) == nullptr;
    }
    void clear() {
        // Manually destroy current nodes
        while (head) {
            Node* tmp = head;
            head = head->next;
            delete tmp;
        }

        // Reinitialize dummy node (same as in constructor)
        head = new Node();
        tail.store(head, std::memory_order_relaxed);
    }
};