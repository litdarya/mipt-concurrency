#include <assert.h>
#include <stdlib.h>
#include "blocking_queue.h"
#include "randThread.h"

int N = 100;
int queries = 0;
queueRandParams params;
queueRand* begin = NULL;
queueRand* end = NULL;

pthread_mutex_t rand_mutex;
pthread_cond_t work_cv;

void Activate() {
	srand(time(NULL));
	while(!atomic_load(&params.shutted_down)) {
		Work();
	}
}

void* GenerateArray() {
	int* arr = (int*)malloc(sizeof(int)*N);
	for (int i = 0; i < N; ++i) {
		arr[i] = rand();
	}
	return (void*)arr;
}

void Work() {
	pthread_mutex_lock(&rand_mutex);

	while(atomic_load(&queries) == 0 && !atomic_load(&params.shutted_down)) {
		pthread_cond_wait(&work_cv, &rand_mutex);
	}

	if (!atomic_load(&params.shutted_down)) {
    	Put(&params, GenerateArray(), &end, &begin);
    	atomic_fetch_add(&queries, -1);
	}
	pthread_mutex_unlock(&rand_mutex);
}


int* Ask() {
	void* res;
	pthread_mutex_lock(&rand_mutex);
    atomic_fetch_add(&queries, 1);
    pthread_cond_signal(&work_cv);
    pthread_mutex_unlock(&rand_mutex);
    int tmp = Get(&params, &begin, &res);
    assert(tmp);
    return (int*)res;
}

void ShutDownThread() {
	pthread_mutex_lock(&rand_mutex);
	Shutdown(&params);
    pthread_cond_broadcast(&work_cv);
    pthread_mutex_unlock(&rand_mutex);
}