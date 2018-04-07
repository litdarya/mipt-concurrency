#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <stdatomic.h>
#include "graph.h"
#include "geneticAlgorytm.h"
#include "randThread.h"
#include "threadPool.h"

double MutationProbability = 0.2*RAND_MAX;
double Mutated = 0.2;
double cub = 0.8;

int comparator(const void* a, const void* b) {
    return ((way_t*)a)->fitness - ((way_t*)b)->fitness;
}

int Random(int** res, int* i) {
    if (*res == NULL) {
        *res = Ask();
        *i = 0;
    } else {
        extern int N;
        if (*i == N - 1) {
            free(*res);
            *res = Ask();
            *i = 0;
        } else {
            ++(*i);
        }
    }
    return (*res)[*i];
}

void PrintWay(const int N, const way_t* way) {
    for(int i = 0; i < N; ++i) {
        printf("%d ", way->way[i]);
    }
    printf("\n");
}

void FitnessFunction(void* params) {
    functionPackaged* fitness_params = (functionPackaged*)params;
    if (fitness_params->ways->way != NULL) {
        int res = 0;
        int tmp_weight = 0;

        for (int i = 0; i < fitness_params->graph->n; ++i) {
            tmp_weight = graph_weight(fitness_params->graph, fitness_params->ways->way[i],
                fitness_params->ways->way[(i + 1)%fitness_params->graph->n]);
            res += tmp_weight;
        }

        if (fitness_params->graph->n == 2) {
            res /= 2;
        }
        fitness_params->ways->fitness = res;
    } else {
        fitness_params->ways->fitness = INFINITY;
    }
}

int IndInArray(const int* way, const int n, const int ind) {
    for (int i = 0; i < n; ++i) {
        if (way[i] == ind) {
            return 1;
        }
    }
    return 0;
}

int* Crossover(const int size, const way_t* wayA, const way_t* wayB) {
    int* res = (int*)malloc(size*sizeof(int));
    
    assert(wayA->way != NULL && wayB->way != NULL);

    for (int i = 0; i < size/2; ++i) {
        res[i] = wayA->way[i];
    }

    int j = 0;
    for (int i = 0; i < size; ++i) {
        if (1 - IndInArray(res, size/2 + j, wayB->way[i])) {
            res[size/2 + j] = wayB->way[i];
            ++j;
            if (2*j >= size) {
                break;
            }
        }
    }
    return res;
}

int* Mutation(const graph_t* graph, way_t* way, int** rand, int* rand_ind) {
    if (Random(rand, rand_ind) <= MutationProbability) {
        int Mutated_ind = Random(rand, rand_ind)%graph->n;
        int neighbour_right = (Mutated_ind + 1)%graph->n;
        int* res = (int*)malloc(graph->n*sizeof(int));
        for (int i = 0; i < graph->n; ++i) {
            res[i] = way->way[i];
        }
        res[Mutated_ind] = way->way[neighbour_right];
        res[neighbour_right] = way->way[Mutated_ind];
        return res;
    }
    return NULL;
}

int CompatibleNeighbour(int* way, int n, int candidate, int a, const graph_t* graph) {
    if (graph_weight(graph, a, candidate) != -1) {
        for (int i = 0; i < n; ++i) {
            if (way[i] == candidate) {
                return 0;
            }
        }
        return 1;
    }
    return 0;
}

int* GenerateRandomWay(const graph_t* graph, int** rand, int* rand_ind) {
    int* way = (int*)malloc(graph->n*sizeof(int));
    way[0] = Random(rand, rand_ind)%graph->n;
    int candidate = -1;

    for (int i = 1; i < graph->n - 1; ++i) {
        candidate = Random(rand, rand_ind)%graph->n;
        while(!CompatibleNeighbour(way, i - 1, candidate, way[i - 1], graph)) {
            candidate = Random(rand, rand_ind)%graph->n;
        }
        way[i] = candidate;
    }

    candidate = Random(rand, rand_ind)%graph->n;
    while(!CompatibleNeighbour(way, graph->n - 1, candidate, way[graph->n - 2], graph)
            || !CompatibleNeighbour(way, graph->n - 1, candidate, way[0], graph)) {
        candidate = Random(rand, rand_ind)%graph->n;
    }
    way[graph->n - 1] = candidate;

    return way;
}

way_t* FirstGeneration(const int N, const graph_t* graph, const int addition, int** rand, int* rand_ind) {
    way_t* ways = (way_t*)malloc((N + addition)*sizeof(way_t));

    for (int i = 0; i < N; ++i) {
        ways[i].way = GenerateRandomWay(graph, rand, rand_ind);
        assert(ways[i].way[graph->n - 1] != ways[i].way[0]);
    }

    for (int i = N; i < N + addition; ++i) {
        ways[i].way = NULL;
    }

    return ways;
}

int BestParent(const int N, way_t* ways, int** rand, int* rand_ind) {
    int ind1 = Random(rand, rand_ind)%N;
    int ind2 = Random(rand, rand_ind)%N;

    while (ind1 == ind2) {
        ind2 = Random(rand, rand_ind)%N;
    }

    if (ind2 < ind1) {
        int tmp = ind1;
        ind1 = ind2;
        ind2 = tmp;
    }

    int res = ind1;

    for (int i = ind1; i <= ind2; ++i) {
        if (ways[i].fitness < ways[res].fitness) {
            res = i;
        }
    }

    return res;
}

void Breeding(void* params) {
    functionPackaged* params_child = (functionPackaged*)params;
    int parent1 = BestParent(params_child->N, params_child->ways,
        params_child->rand, params_child->rand_ind);
    int parent2 = BestParent(params_child->N, params_child->ways,
        params_child->rand, params_child->rand_ind);
    if (params_child->ways[params_child->N + params_child->i].way != NULL) {
        free(params_child->ways[params_child->N + params_child->i].way);
    }
    params_child->ways[params_child->N + params_child->i].way
    = Crossover(params_child->size,
        &params_child->ways[parent1], &params_child->ways[parent2]);
}

void Mutate(void* params) {
    functionPackaged* params_mut = (functionPackaged*)params;

    int delta = cub*params_mut->N;
    int* tmp = NULL;
    int ind = Random(params_mut->rand, params_mut->rand_ind)%params_mut->N;
    tmp = Mutation(params_mut->graph, &params_mut->ways[ind],
        params_mut->rand, params_mut->rand_ind);
    if (tmp != NULL) {
        if (params_mut->ways[params_mut->N + delta + params_mut->i].way != NULL) {
            free(params_mut->ways[params_mut->N + delta + params_mut->i].way);
        }
        params_mut->ways[params_mut->N + delta + params_mut->i].way = tmp;
    }
}

void concurrentMutations(const graph_t* graph, way_t* ways, int N, pthread_t* threads,
    functionPackaged* array, int number_threads, int mutant_number) {
    for (int i = 0; i < mutant_number; ++i) {
            array[i].graph = graph;
            array[i].N = N;
            array[i].ways = ways;
            array[i].i = i;
            array[i].func_pointer = &Mutate;
            //Mutate(graph, N, ways, &rand, &rand_ind, i);
        }

    Init(number_threads);

    for (int i = 0; i < mutant_number; ++i) {
        Submit((void*)&array[i]);
    }

    for (int i = 0; i < number_threads; ++i) {
        pthread_create(&threads[i], NULL, WorkThreadWorkers, NULL);
        //printf("CREATED\n");
    }

    ShutDownThreadPool();

    for (int i = 0; i < number_threads; ++i) {
        pthread_join(threads[i], NULL);
    }
}

void concurrentFitnessFunction(functionPackaged* array, const graph_t* graph, pthread_t* threads,
    way_t* ways, int N, int number_threads) {

    for (int i = 0; i < N; ++i) {
            array[i].graph = graph;
            array[i].ways = &ways[i];
            array[i].func_pointer = &FitnessFunction;
    }

    Init(number_threads);

    for (int i = 0; i < N; ++i) {
        Submit((void*)&array[i]);
    }
    
    for (int i = 0; i < number_threads; ++i) {
        pthread_create(&threads[i], NULL, WorkThreadWorkers, NULL);
    }

    ShutDownThreadPool();

    for (int i = 0; i < number_threads; ++i) {
        pthread_join(threads[i], NULL);
    }
}

void concurrentBreeding(int size, way_t* ways, int N, pthread_t* threads,
    functionPackaged* array, int number_threads, int cubs_number) {
    for (int i = 0; i < cubs_number; ++i) {
            array[i].size = size;
            array[i].N = N;
            array[i].ways = ways;
            array[i].i = i;
            array[i].func_pointer = &Breeding;
        }

    Init(number_threads);

    for (int i = 0; i < cubs_number; ++i) {
        Submit((void*)&array[i]);
    }

    for (int i = 0; i < number_threads; ++i) {
        pthread_create(&threads[i], NULL, WorkThreadWorkers, NULL);
    }

    ShutDownThreadPool();

    for (int i = 0; i < number_threads; ++i) {
        pthread_join(threads[i], NULL);
    }
}

void Selection(const int N, const int S, const graph_t* graph, int num_treads, FILE* f) {
    struct timeval start, end;
    assert(gettimeofday(&start, NULL) == 0);
    int addition = cub*N + Mutated*N;
    if (addition == 0) {
        printf("N shoul be at least 5!\n");
        assert(0);
    }
    int counter = 0;
    int iterations = 0;

    int* rand = NULL;
    int rand_ind = 0;    
    way_t* ways = FirstGeneration(N, graph, addition, &rand, &rand_ind);
    free(rand);

    functionPackaged* func_params = (functionPackaged*)malloc((N +
        addition)*sizeof(functionPackaged));
    pthread_t* threads = (pthread_t*)malloc(sizeof(pthread_t)*num_treads);

    concurrentFitnessFunction(func_params, graph, threads,
            ways, N, num_treads);

    int res = ways[0].fitness;
    int tmp_res = res;
 
    while(counter != S) {
        concurrentBreeding(graph->n, ways, N, threads,
            func_params, num_treads, N*cub);
        concurrentMutations(graph, ways, N, threads,
            func_params, num_treads, N*Mutated);

        concurrentFitnessFunction(func_params, graph, threads,
            ways, N + addition, num_treads);
        
        qsort(ways, N + addition, sizeof(way_t), comparator);
        tmp_res = ways[0].fitness;

        if (res == tmp_res) {
            ++counter;
        } else {
            if (tmp_res < res) {
                res = tmp_res;
            } else {
                printf("%d \n", tmp_res);
                printf("Smth in Selection is wrong! \n");
                assert(0);
            }
            counter = 0;
        }
        ++iterations;
    }
    assert(gettimeofday(&end, NULL) == 0);
    free(threads);

    double delta = ((end.tv_sec - start.tv_sec) * 1000000u +
                    end.tv_usec - start.tv_usec) / 1.e6;

    fprintf(f, "%d %fs %d \n", iterations - counter, delta, res);

    for(int i = 0; i < graph->n; ++i) {
        fprintf(f, "%d ", ways[0].way[i]);
    }
    fprintf(f, "\n");

    for (int i = 0; i < N + addition; ++i) {
        if (ways[i].way != NULL) {
            free(ways[i].way);
        }
    }

    free(func_params);
    free(ways);
}
