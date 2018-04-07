#pragma once

#include "atomic_marked_pointer.h"
#include "arena_allocator.h"

#include <iostream>
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

///////////////////////////////////////////////////////////////////////

template <typename Element>
class LockFreeLinkedSet {
 private:
    struct Node {
        Element element_;
        AtomicMarkedPointer<Node> next_;

        Node(const Element& element, Node* next = nullptr)
            : element_{element},
              next_{next} {
        }

        bool Marked() const {
            return next_.Marked();
        }

        Node* NextPointer() const {
            return next_.LoadPointer();
        }
    };

    struct Edge {
        Node* pred_;
        Node* curr_;

        Edge(Node* pred, Node* curr)
            : pred_(pred),
              curr_(curr) {
        }
    };

 public:
    explicit LockFreeLinkedSet(ArenaAllocator& allocator)
        : allocator_(allocator) {
        CreateEmptyList();
        size_ = 0;
    }

    bool Insert(const Element& element) {
        Edge insert_between{nullptr, nullptr};
        Node *node = allocator_.New<Node>(element);
        insert_between = Locate(element);
        node->next_.Store(insert_between.curr_);

        if (insert_between.curr_->element_ == element) {
            return false;
        }
        
        while (!insert_between.pred_->next_.CompareAndSet({insert_between.curr_, false},
                                                          {node, false})) {
            insert_between = Locate(element);
            node->next_.Store(insert_between.curr_);
            if (insert_between.curr_->element_ == element) {
                return false;
            }
        }
        size_.fetch_add(1);
        return true;
    }

    bool Remove(const Element& element) {
        Edge insert_between{nullptr, nullptr};

        insert_between = Locate(element);

        if (insert_between.curr_->element_ != element) {
            return false;
        }

        while (!insert_between.pred_->next_.CompareAndSet({insert_between.curr_, false},
                                                          {insert_between.curr_->next_.LoadPointer(), false})) {
            insert_between = Locate(element);

            if (insert_between.curr_->element_ != element) {
                return false;
            }
        }

        while(!insert_between.curr_->next_.TryMark(insert_between.curr_->NextPointer())) {
            // try mark
        }

        size_.fetch_sub(1);
        return true;
    }

    bool Contains(const Element& element) const {
        Edge insert_between = Locate(element);
        if (insert_between.curr_->element_ == element) {
            return true;
        }
        return false;
    }

    size_t Size() const {
        return  size_;
    }

 private:
    void CreateEmptyList() {
        head_.Store(allocator_.New<Node>(ElementTraits<Element>::Min()));
        head_.LoadPointer()->next_.Store(allocator_.New<Node>(std::numeric_limits<Element>::max()));
    }

    Edge Locate(const Element& element) const {
        Node *pred = head_.LoadPointer();
        Node *curr = head_.LoadPointer()->next_.LoadPointer();

        while (curr->element_ < element) {
            pred = curr;
            curr = curr->next_.LoadPointer();

            if (curr->Marked()) {
                if (!pred->next_.CompareAndSet({curr, false}, {curr->next_.LoadPointer(), false})) {
                    pred = head_.LoadPointer();
                    curr = head_.LoadPointer()->next_.LoadPointer();
                }
            }
        }

        return Edge{pred, curr};
    }

 private:
    ArenaAllocator& allocator_;
    AtomicMarkedPointer<Node> head_;
    std::atomic<int> size_;
};

template <typename T> using ConcurrentSet = LockFreeLinkedSet<T>;
