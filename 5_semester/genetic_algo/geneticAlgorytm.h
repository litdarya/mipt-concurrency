//
// Created by darya on 12.11.17.
//
#ifndef HW4_GENETICALGORYTM_H
#define HW4_GENETICALGORYTM_H
#include "threadPool.h"

typedef struct way_t {
    int fitness;
    int* way;
} way_t;

typedef struct functionPackaged {
    int size;
    int N;
    int i;
    int** rand;
    int* rand_ind;
    graph_t* graph;
    way_t* ways;
    void (*func_pointer)(void* params)
} functionPackaged;

int comparator(const void* a, const void* b);

int Random(int** res, int* i);

void PrintWay(const int N, const way_t* way);

void FitnessFunction(void* params);

int IndInArray(const int* way, const int n, const int ind);

int* Crossover(const int size, const way_t* wayA, const way_t* wayB);

int* Mutation(const graph_t* graph, way_t* way, int** rand, int* rand_ind);

int CompatibleNeighbour(int* way, int n, int candidate, int a, const graph_t* graph);

int* GenerateRandomWay(const graph_t* graph, int** rand, int* rand_ind);

way_t* FirstGeneration(const int N, const graph_t* graph, const int addition,
	int** rand, int* rand_ind);

int BestParent(const int N, way_t* ways, int** rand, int* rand_ind);

void Breeding(void* params);

void Mutate(void* params);

void concurrentMutations(const graph_t* graph, way_t* ways, int N, pthread_t* threads,
    functionPackaged* array, int number_threads, int mutant_number);

void concurrentFitnessFunction(functionPackaged* array, const graph_t* graph, pthread_t* threads,
    way_t* ways, int N, int number_threads);

void concurrentBreeding(int size, way_t* ways, int N, pthread_t* threads,
    functionPackaged* array, int number_threads, int cubs_number);

void Selection(const int N, const int S, const graph_t* graph, int num_treads, FILE* f);

#endif //HW4_GENETICALGORYTM_H