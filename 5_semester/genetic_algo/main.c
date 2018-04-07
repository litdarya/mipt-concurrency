#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>

#include "graph.h"
#include "geneticAlgorytm.h"
#include "blocking_queue.h"
#include "randThread.h"

void Initialize() {
    extern queueRandParams params;
    params.capacity = 10000;
}

graph_t* GetGraph(char* flag, char* paths, int num_vertex) {
    if (flag[2] == 'g') {
        return graph_generate(num_vertex, 1000);
    }
    return graph_read_file(paths);
}

int main(int argc, char** argv) {
    int num_treads, N, S, num_vertex;
    char flag[10], paths[50];
    num_treads = atoi(argv[1]);
    N = atoi(argv[2]);
    S = atoi(argv[3]);
    strcpy(flag, argv[4]);
    if (flag[2] == 'g') {
        num_vertex = atoi(argv[5]);
    } else {
        strcpy(paths, argv[5]);
    }

    graph_t* g = GetGraph(flag, paths, num_vertex);

    FILE* f = fopen("stats.txt", "a");
    if (f == NULL) {
        printf("File opening error...\n");
        exit(1);
    }

    fprintf(f, "%d %d %d %d ", num_treads, N, S, g->n);

    Initialize();

    pthread_t thread_rand;
    pthread_create(&thread_rand, NULL, Activate, NULL);
    Selection(N, S, g, num_treads, f);
    ShutDownThread();
    pthread_join(thread_rand, NULL);

    fclose(f);
    graph_destroy(g);
    return 0;
}