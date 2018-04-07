#include "blocking_queue.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>

void push_back(queueRandParams* queue, queueRand** end, queueRand** begin, void* array) {
    if (*begin != NULL && *end != NULL) {
        queueRand* new_end = (queueRand*)malloc(sizeof(queueRand));
        new_end->next = NULL;
        (*end)->next = new_end;
        new_end->random = array;
        *end = new_end;
    } else {
        *end = (queueRand*)malloc(sizeof(queueRand));
        (*end)->next = NULL;
        (*end)->random = array;
        *begin = *end;
        atomic_store(&queue->size, 0);
        atomic_store(&queue->shutted_down, 0);
    }
    atomic_fetch_add(&queue->size, 1);
}

int* pop_front(queueRandParams* queue, queueRand** begin) {
    int* res = (*begin)->random;

    if ((*begin)->next != NULL) {
        ((*begin)->next)->prev = NULL;
    }
    queueRand* tmp = (*begin)->next;
    free(*begin);
    *begin = tmp;
    atomic_fetch_sub(&queue->size, 1);
    return res;
}

void printArray(int n, int* arr) {
    for (int i = 0; i < n; ++i) {
        printf("%d ", arr[i]);
    }
    printf("\n");
}

void printQueue(queueRand* begin) {
    printf("----------- \n");
    for (queueRand* it = begin; it != NULL; it = it->next) {
        printArray(3, it->random);
    }
    printf("----------- \n");
}

void Put(queueRandParams* queue, void* element, queueRand** end, queueRand** begin) {
    if (!atomic_load(&queue->shutted_down)) {
        pthread_mutex_lock(&queue->my_mutex);
        while (atomic_load(&queue->size) + 1 >
            queue->capacity && !atomic_load(&queue->shutted_down)) {
            pthread_cond_wait(&queue->put_cv, &queue->my_mutex); 
        }
        push_back(queue, end, begin, element);
        pthread_cond_signal(&queue->get_cv);
        pthread_mutex_unlock(&queue->my_mutex);
    } else {
        assert(0);
    }
}

int Get(queueRandParams* queue, queueRand** begin, void** result) {
    pthread_mutex_lock(&queue->my_mutex);

    while (atomic_load(&queue->size) == 0 && !atomic_load(&queue->shutted_down)) {
        pthread_cond_wait(&queue->get_cv, &queue->my_mutex);
    }

    if (atomic_load(&queue->size) == 0 && atomic_load(&queue->shutted_down)) {
        pthread_mutex_unlock(&queue->my_mutex);
        return 0;
    }
    *result = pop_front(queue, begin);
    pthread_cond_signal(&queue->put_cv);
    pthread_mutex_unlock(&queue->my_mutex);
    return 1;
}

void Shutdown(queueRandParams* queue) {
    pthread_mutex_lock(&queue->my_mutex);
    atomic_store(&queue->shutted_down, 1);
    pthread_cond_broadcast(&queue->put_cv);
    pthread_cond_broadcast(&queue->get_cv);
    pthread_mutex_unlock(&queue->my_mutex);
}