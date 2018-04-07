#include <iostream>
#include <mutex>

#include "arena_allocator.h"

#include <atomic>
#include <limits>

template <typename T>
struct ElementTraits {
    static T Min() {
        return std::numeric_limits<T>::min();
    }
    static T Max() {
        return std::numeric_limits<T>::max();
    }
};

class SpinLock {
 public:
    explicit SpinLock() {
        owner_ticket_ = 0;
        next_ticket_ = 0;
    }

    void Lock() {
        int ticket = next_ticket_.fetch_add(1);
        while(ticket != owner_ticket_) {
            std::this_thread::yield();
        }
    }

    void Unlock() {
        owner_ticket_.fetch_add(1);
    }

    void lock() {
        Lock();
    }

    void unlock() {
        Unlock();
    }

 private:
    std::atomic<int> owner_ticket_;
    std::atomic<int> next_ticket_;
};

template <typename T>
class OptimisticLinkedSet {
 private:
    struct Node {
        T element_;
        std::atomic<Node*> next_;
        SpinLock lock_{};
        std::atomic<bool> marked_{false};

        Node(const T& element, Node* next = nullptr)
                : element_(element),
                  next_(next) {}
    };

    struct Edge {
        Node* pred_;
        Node* curr_;

        Edge(Node* pred, Node* curr)
                : pred_(pred),
                  curr_(curr) {}
    };

 public:
    explicit OptimisticLinkedSet(ArenaAllocator& allocator)
            : allocator_(allocator),
              head_(nullptr) {
        CreateEmptyList();
        size_ = 0;
    }

    bool Insert(const T& element) {
        Edge insert_between{nullptr, nullptr};
        bool validation;

        insert_between = Locate(element);
        std::unique_lock<SpinLock> lock(insert_between.pred_->lock_);

        while (true) {
            validation = Validate(insert_between);

            if (!validation) {
                lock.unlock();
            } else {
                break;
            }

            insert_between = Locate(element);
            lock = std::unique_lock<SpinLock>(insert_between.pred_->lock_);
        }

        if (insert_between.curr_->element_ == element) {
            return false;
        }

        Node *node = allocator_.New<Node>(element);
        node->next_.store(insert_between.pred_->next_);
        insert_between.pred_->next_ = node;

        size_.fetch_add(1);
        return true;
    }

    bool Remove(const T& element) {
        Edge insert_between{nullptr, nullptr};
        bool validate;

        insert_between = Locate(element);
        std::unique_lock<SpinLock> pred_lock(insert_between.pred_->lock_);
        std::unique_lock<SpinLock> curr_lock(insert_between.curr_->lock_);

        while (true) {
            validate = Validate(insert_between);

            if (!validate) {
                pred_lock.unlock();
                curr_lock.unlock();
            } else {
                break;
            }

            insert_between = Locate(element);
            pred_lock = std::unique_lock<SpinLock>(insert_between.pred_->lock_);
            curr_lock = std::unique_lock<SpinLock>(insert_between.curr_->lock_);
        }

        if (insert_between.curr_->element_ != element) {
            return false;
        }

        insert_between.pred_->next_ = insert_between.curr_->next_.load();
        insert_between.curr_->marked_ = true;
        size_.fetch_sub(1);
        return true;
    }

    bool Contains(const T& element) const {
        Edge insert_between = Locate(element);
        if (insert_between.curr_->element_ == element) {
            return true;
        }
        return false;
    }

    size_t Size() const {
        return size_;
    }

 private:
    void CreateEmptyList() {
        head_ = allocator_.New<Node>(ElementTraits<T>::Min());
        head_->next_ = allocator_.New<Node>(ElementTraits<T>::Max());
    }

    Edge Locate(const T& elem_num) const {
        Node* pred = head_;
        Node* curr = head_->next_;

        while(curr->next_ != nullptr && curr->element_ < elem_num) {
            pred = curr;
            curr = curr->next_;
        }

        return Edge{pred, curr};
    }

    bool Validate(const Edge& edge) const {
        if (edge.pred_->next_ == edge.curr_) {
            if (!edge.pred_->marked_ && !edge.curr_->marked_) {
                return true;
            }
        }
        return false;
    }


 private:
    ArenaAllocator& allocator_;
    Node* head_;
    std::atomic<int> size_;
};

template <typename T> using ConcurrentSet = OptimisticLinkedSet<T>;
