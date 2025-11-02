#pragma once
#include <atomic>
#include <memory>
#include <iostream>

namespace T_Threads {
    template <typename T>
    class MPSCQueue {
    private:
        struct Node {
            std::shared_ptr<T> data_;
            std::atomic<Node*> next_;
            Node(std::shared_ptr<T> d = nullptr) : data_(std::move(d)), next_(nullptr) {}
        };

        std::atomic<Node*> tail_;  
        Node* head_;               
        std::atomic<size_t> size_;
    public:
        MPSCQueue() {
            head_ = new Node();  
            tail_.store(head_, std::memory_order_relaxed);
            size_ = 0;
        }

        ~MPSCQueue() {
            while (head_) {
                Node* tmp = head_;
                head_ = head_->next_;
                delete tmp;
            }
        }

        void push(const T& item) {
            size_.fetch_add(1, std::memory_order_relaxed);
            //std::cout << "size " << size_.load();
            Node* node = new Node(std::make_shared<T>(item));
            node->next_.store(nullptr, std::memory_order_relaxed);
            Node* prev = tail_.exchange(node, std::memory_order_acq_rel);
            prev->next_.store(node, std::memory_order_release);
        }

        void push(T&& item) {
            size_++;
            //  std::cout << "size " << size_.load();
            Node* node = new Node(std::make_shared<T>(std::move(item)));
            node->next_.store(nullptr, std::memory_order_relaxed);
            Node* prev = tail_.exchange(node, std::memory_order_acq_rel);
            prev->next_.store(node, std::memory_order_release);
        }

        void push(std::shared_ptr<T> item) {
            size_.fetch_add(1, std::memory_order_relaxed);
           // std::cout << "size " << size_.load();
            Node* node = new Node(std::move(item));
            node->next_.store(nullptr, std::memory_order_relaxed);
            Node* prev = tail_.exchange(node, std::memory_order_acq_rel);
            prev->next_.store(node, std::memory_order_release);
        }

        std::shared_ptr<T> pop() {
            Node* next_ = head_->next_.load(std::memory_order_acquire);
            if (!next_) return nullptr;  
            if (size_.load(std::memory_order_acquire) > 0)
                size_.fetch_sub(1, std::memory_order_relaxed);

            std::shared_ptr<T> result = next_->data_;
            delete head_;
            head_ = next_;
            return result;
        }

        std::shared_ptr<T> peek() const {
            Node* next_ = head_->next_.load(std::memory_order_acquire);
            if (!next_) return nullptr;  
            return next_->data_;          
        }

        bool empty() const {
            return head_->next_.load(std::memory_order_acquire) == nullptr;
        }
        void clear() {
            while (head_) {
                Node* tmp = head_;
                head_ = head_->next_;
                delete tmp;
            }
           // size_.store(0, std::memory_order_release);
            head_ = new Node();
            tail_.store(head_, std::memory_order_relaxed);
        }
        size_t size() {
            std::cout << "size return " << size_.load(std::memory_order_acquire) << "\n";
            return size_.load();
        }
    };
};