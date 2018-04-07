#include "graph.h"
#include "geneticAlgorytm.h"

void Init(int num_threads);
void Submit(void* task);
void WorkThreadWorkers();
void Lock();
void ShutDownThreadPool();