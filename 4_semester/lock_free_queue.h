#pragma once

#include <iostream>
#include <thread>
#include <atomic>

template <typename T, template <typename U> class Atomic = std::atomic>
class LockFreeQueue {
    struct Node {
        T element_{};
        Atomic<Node *> next_{nullptr};

        explicit Node(T element, Node *next = nullptr)
                : element_(std::move(element)), next_(next) {
        }

        explicit Node() {
        }
    };

public:
    explicit LockFreeQueue() {
        Node *dummy = new Node{};
        head_ = dummy;
        tail_ = dummy;
        rubbish_head_ = dummy;
    }

    ~LockFreeQueue() {
        Node *rubbish;
        while (rubbish_head_.load() != nullptr) {
            rubbish = rubbish_head_.load();
            rubbish_head_ = rubbish->next_.load();
            delete rubbish;
        }
    }

    void Enqueue(T element) {
        counter_.fetch_add(1);
        Node *new_node = new Node(element);
        Node *current_tail;
        Node *next;

        while (true) {
            current_tail = tail_.load();
            next = current_tail->next_.load();

            if (next == nullptr) {
                if (current_tail->next_.compare_exchange_strong(next, new_node)) {
                    break;
                }
            } else {
                tail_.compare_exchange_strong(current_tail, current_tail->next_.load());
            }
        }
        tail_.compare_exchange_strong(current_tail, new_node);
        counter_.fetch_sub(1);
    }

    void FreeRubbish(Node *current_head) {
        Node *rubbish;
        while (rubbish_head_.load() != current_head) {
            rubbish = rubbish_head_.load();
            rubbish_head_ = rubbish->next_.load();
            delete rubbish;
        }
    }

    bool Dequeue(T &element) {
        counter_.fetch_add(1);
        Node *current_head;
        Node *current_tail;

        while (true) {
            current_head = head_.load();
            current_tail = tail_.load();

            if (current_head == current_tail) {
                if (current_head->next_.load() == nullptr) {
                    if (counter_.load() == 1) {
                        FreeRubbish(current_head);
                    }
                    counter_.fetch_sub(1);
                    return false;
                } else {
                    tail_.compare_exchange_strong(current_head, current_head->next_.load());
                }

            } else {
                if (head_.compare_exchange_strong(current_head, current_head->next_.load())) {
                    element = current_head->next_.load()->element_;
                    if (counter_.load() == 1) {
                        FreeRubbish(current_head);
                    }
                    counter_.fetch_sub(1);
                    return true;
                }
            }
        }
    }

private:
    Atomic<Node*> head_{nullptr};
    Atomic<Node*> tail_{nullptr};
    Atomic<Node*> rubbish_head_{nullptr};
    Atomic<int> counter_{0};
};