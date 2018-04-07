#include <math.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>

typedef struct queueRandParams {
	pthread_mutex_t my_mutex;
	int shutted_down;
	int size;
	int capacity;
	pthread_cond_t put_cv;
	pthread_cond_t get_cv;

}  queueRandParams;

typedef struct queueRand {
    struct queueRand* next;
    struct queueRand* prev;
    void* random;
} queueRand;

void push_back(queueRandParams* queue, queueRand** end, queueRand** begin, void* array);

int* pop_front(queueRandParams* queue, queueRand** begin);

void printArray(int n, int* arr);

void printQueue(queueRand* begin);

void Put(queueRandParams* queue, void* element, queueRand** end, queueRand** begin);

int Get(queueRandParams* queue, queueRand** begin, void** result);

void Shutdown(queueRandParams* queue);