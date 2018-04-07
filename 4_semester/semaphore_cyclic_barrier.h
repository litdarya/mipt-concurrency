#pragma once
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <thread>

using std::array;
using std::condition_variable;
using std::thread;

template <class ConditionVariable = std::condition_variable>

class Semaphore {
 public:
    explicit Semaphore(int supposed_numb_threads)
            : numb_threads_(supposed_numb_threads) {
        current_amount_ = 0;
    }

    void Wait() {
        std::unique_lock<std::mutex> lock(own_mutex_);

        while (current_amount_ == 0) {
            cv_.wait(lock);
        }
        --current_amount_;
    }

    void SignalAllThreads() {
        std::unique_lock<std::mutex> lock(own_mutex_);

        current_amount_ += numb_threads_;
        cv_.notify_all();
    }

 private:
    const int numb_threads_;
    std::atomic<int> current_amount_;
    std::mutex own_mutex_;
    ConditionVariable cv_;
};

template <class ConditionVariable = std::condition_variable>

class CyclicBarrier {
 public:
    explicit CyclicBarrier(size_t num_threads)
            : number_threads_(num_threads),
              enter_sem_(num_threads - 1),
              exit_sem_(num_threads - 1) {
        enter_count_ = 0;
        exit_count_ = 0;
    }

    void Pass() {
        Lock();
    }

 private:
    const int number_threads_;
    std::atomic<int> enter_count_;
    std::atomic<int> exit_count_;
    Semaphore<ConditionVariable> enter_sem_;
    Semaphore<ConditionVariable> exit_sem_;

    void EnterExitBarrier(std::atomic<int>& count, Semaphore<ConditionVariable>& sem) {
        if (std::atomic_fetch_add(&count, 1) == number_threads_ - 1) {
            count = 0;
            sem.SignalAllThreads();
        } else {
            sem.Wait();
        }
    }

    void Lock() {
        EnterExitBarrier(enter_count_, enter_sem_);
        EnterExitBarrier(exit_count_, exit_sem_);
    }
};
