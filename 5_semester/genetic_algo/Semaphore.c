#include <pthread.h>
#include <stdatomic.h>
#include "Semaphore.h"

void SemaphoreWait(pthread_mutex_t* semaphore_mutex, pthread_cond_t* cv, int* current_amount) {
    pthread_mutex_lock(semaphore_mutex);
    while (*current_amount == 0) {
        pthread_cond_wait(cv, semaphore_mutex);
    }
    --(*current_amount);
    pthread_mutex_unlock(semaphore_mutex);
}

void SignalAllThreads(pthread_mutex_t* semaphore_mutex, 
    const int numb_threads, pthread_cond_t* cv, int* current_amount) {
    pthread_mutex_lock(semaphore_mutex);
    (*current_amount) += numb_threads;
    pthread_cond_broadcast(cv);
    pthread_mutex_unlock(semaphore_mutex);
}
