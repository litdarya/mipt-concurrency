#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>
#include "threadPool.h"
#include "blocking_queue.h"
#include "graph.h"
#include "randThread.h"
#include "Semaphore.h"

int enter_atomic = 0;
int exit_atomic = 0;
int number_threads;
int current_amount_enter;
int current_amount_exit;

pthread_mutex_t enter_mutex;
pthread_mutex_t exit_mutex;
pthread_cond_t enter_cv;
pthread_cond_t exit_cv;

queueRandParams params_queue;
queueRand* begin_threadPoolQueue = NULL;
queueRand* end_threadPoolQueue = NULL;

void Init(int number_threads_) {
	number_threads = number_threads_;

	params_queue.shutted_down = 0;
	params_queue.capacity = 10000;

	enter_atomic = 0;
	exit_atomic = 0;
	current_amount_exit = 0;
	current_amount_enter = 0;
}

void Submit(void* task) {
	if (!atomic_load(&params_queue.shutted_down)) {
	    Put(&params_queue, task, &end_threadPoolQueue, &begin_threadPoolQueue);
	}
}

void WorkThreadWorkers() {
	void* task;
	functionPackaged* func;
	int tmp = 1;
	int* rand_arr = NULL;
	int rand_ind = 0;
	while(tmp) {
		task = NULL;
		tmp = Get(&params_queue, &begin_threadPoolQueue, &task);
		if (tmp && task != NULL) {
			func = (functionPackaged*)task;
			func->rand = &rand_arr;
			func->rand_ind = &rand_ind;
			(*func->func_pointer)((void*)func);
		}
	}
	free(rand_arr);
	Lock();
}

void EnterExitBarrier(int* atomic, pthread_mutex_t* mutex, pthread_cond_t* cv, int* curr) {
	atomic_fetch_add(atomic, 1);
	if (atomic_load(atomic) == number_threads) {
		SignalAllThreads(mutex, number_threads, cv, curr);		
	} else {
		SemaphoreWait(mutex, cv, curr);
	}
}

void Lock() {
	EnterExitBarrier(&enter_atomic, &enter_mutex, &enter_cv, &current_amount_enter);
	EnterExitBarrier(&exit_atomic, &exit_mutex, &exit_cv, &current_amount_exit);
}

void ShutDownThreadPool() {
	Shutdown(&params_queue);	
}