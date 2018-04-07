#pragma once

#include <iostream>
#include <atomic>
#include <thread>

template <template <typename T> class Atomic = std::atomic>
class MCSSpinLock {
 public:
    class Guard {
     public:
        explicit Guard(MCSSpinLock& spinlock)
                : spinlock_(spinlock) {
            Acquire();
        }

        ~Guard() {
            Release();
        }

     private:
        void Acquire() {
            Guard* prev_tail = spinlock_.tail_.exchange(this);
            if (prev_tail != nullptr) {
                prev_tail->next_ = this;
                while (!owner_.load()) {
                }
            }
            owner_ = true;
        }

        void Release() {
            owner_ = false;
            Guard* tmp_to_compare = this;
            if (!spinlock_.tail_.compare_exchange_strong(tmp_to_compare, nullptr)) {
                while (next_.load() == nullptr) {
                }
                next_.load()->owner_ = true;
            }
        }

     private:
        MCSSpinLock& spinlock_;

        Atomic<bool> owner_{false};
        Atomic<Guard*> next_{nullptr};
    };

 private:
    Atomic<Guard*> tail_{nullptr};
};

template <template <typename T> class Atomic = std::atomic>
using SpinLock = MCSSpinLock<Atomic>;
