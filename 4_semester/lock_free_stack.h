#include <iostream>

#pragma once

#include <atomic>
#include <thread>

template <typename T>
class LockFreeStack {
    struct Node {
        Node (T element_): element(element_) {}
        T element;
        std::atomic<Node*> next{nullptr};
    };

 public:
    LockFreeStack() {}

    ~LockFreeStack() {
        Node* it = garbage_stack_;
        Node* new_it;

        while(it != nullptr) {
            new_it = it->next;
            delete it;
            it = new_it;
        }

        it = top_;
        while (it != nullptr) {
            new_it = it->next;
            delete it;
            it = new_it;
        }
    }

    void Push(T element) {
        Node* new_tail = new Node(element);

        Node* last_tail = top_;
        new_tail->next.store(top_);

        while(!top_.compare_exchange_strong(last_tail, new_tail)) {
            new_tail->next.store(top_);
        }
    }

    void PushGarbage(Node* element) {
        Node* last_tail = garbage_stack_;
        element->next.store(garbage_stack_);

        while(!garbage_stack_.compare_exchange_strong(last_tail, element)) {
            element->next.store(garbage_stack_);
        }
    }

    bool Pop(T& element) {
        Node* last_tail = top_;

        while(true) {
            if (last_tail == nullptr) {
                return false;
            }

            if (top_.compare_exchange_strong(last_tail, last_tail->next)) {
                PushGarbage(last_tail);
                element = last_tail->element;
                break;
            }
        }

        return true;
    }

 private:
    std::atomic<Node*> top_{nullptr};
    std::atomic<Node*> garbage_stack_{nullptr};
};

template <typename T>
using ConcurrentStack = LockFreeStack<T>;
