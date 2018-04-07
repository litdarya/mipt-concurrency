#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <mpi.h>

typedef struct pair {
    int start;
    int end;
} pair;

typedef struct particle {
    int x;
    int y;
    int counter;
} particle;

typedef struct probabilities {
	double pl;
	double pr;
	double pu;
	double pd;
} probabilities;

typedef struct Node
{
	struct Node* prev;
	struct Node* next;
	particle dot;
} Node;

typedef struct BufferController {
	MPI_Request request;
	MPI_Status status;
	int* buffer;
} BufferController;

typedef struct BufferNode
{
	struct BufferNode* prev;
	struct BufferNode* next;
	BufferController controller;
} BufferNode;

BufferNode* AddBufferNode(BufferNode* oldNode, int* buffer) {
	BufferNode* newNode = (BufferNode*)malloc(sizeof(BufferNode));
	if (oldNode != NULL) {
		oldNode->next = newNode;
	}
	newNode->prev = oldNode;
	newNode->next = NULL;

	newNode->controller.buffer = buffer;

	return newNode;
}

BufferNode* DeleteBuffer(BufferNode* node) {
	assert(node != NULL);
	BufferNode* res;
	res = node->next;
	if (res != NULL) {
		res->prev = node->prev;
	}
	if (node->prev != NULL) {
		(node->prev)->next = res;
	}
	free(node->controller.buffer);
	free(node);
	return res;
}

int TryToFree(BufferNode* node) {
	int flag;
	MPI_Test(&node->controller.request, &flag, &node->controller.status);
	return flag;
}

BufferNode* WalkAndFree(BufferNode** start) {
	BufferNode* currNode = *start;
	while(currNode != NULL) {
		if (TryToFree(currNode)) {
			if (currNode == *start) {
				*start = DeleteBuffer(*start);
				currNode = *start;
			} else {
				currNode = DeleteBuffer(currNode);
			}
		} else {
			currNode = currNode->next;
		}
	}
	return currNode;
}

Node* AddNode(Node* oldNode, int x, int y, int counter) {
	Node* newNode = (Node*)malloc(sizeof(Node));
	if (oldNode != NULL) {
		oldNode->next = newNode;
	}
	newNode->prev = oldNode;
	newNode->next = NULL;

	newNode->dot.x = x;
	newNode->dot.y = y;
	newNode->dot.counter = counter;

	return newNode;
}

Node* Delete(Node* node) {
	assert(node != NULL);
	Node* res;
	if (node->next == NULL && node->prev == NULL) {
		res = NULL;
	} else {
		assert(node->prev == NULL);
		res = node->next;
		res->prev = NULL;
	}
	free(node);
	return res;
}

int GetLength(Node* begin) {
	int length = 0;
	for (Node* it = begin; it != NULL; it = it->next) {
		++length;
	}
	return length;
}

void ProbabilitiesToIntervals(probabilities* prob) {
	prob->pl *= RAND_MAX;
	prob->pr = prob->pl + prob->pr*RAND_MAX;
	prob->pu = prob->pr + prob->pu*RAND_MAX; 
	prob->pd = prob->pu + prob->pd*RAND_MAX; 
}

int checkBoundaries(pair xArea, pair yArea, particle dot) {
	return xArea.start - 1 <= dot.x && dot.x <= xArea.end  + 1 &&
		yArea.start - 1 <= dot.y && dot.y <= yArea.end + 1;
}

int GetRank(int x, int y, int a, int l) {
	int x_ind = x/l;
	int y_ind = y/l; 
	return a*y_ind + x_ind;
}

void SendNewSizeToAll(int value, int size, int rank, BufferNode** end) {
	for (int i = 0; i < size; ++i) {
		if (i != rank) {
			int* buf = (int*)malloc(sizeof(int));
			buf[0] = value;
			*end = AddBufferNode(*end, buf);
	    	MPI_Isend(buf, 1, MPI_INT, i, 1, MPI_COMM_WORLD,
	    		&(*end)->controller.request);
		}
	}
}

void ListenToOthersSize(int size, int rank, int* shared_counter) {
	int flag = 0;
	int* buf = (int*)malloc(sizeof(int));

	MPI_Status status;

	for (int i = 0; i < size; ++i) {
		if (i != rank) {
			MPI_Iprobe(i, 1, MPI_COMM_WORLD, &flag, &status);
				if (flag) {
					MPI_Recv(buf, 1, MPI_INT, i, 1,
						MPI_COMM_WORLD, &status);
					*shared_counter += buf[0];
				}
		}
	}
	free(buf);
}

void Step(particle* dot, int rand, probabilities* prob) {
	if (rand <= prob->pl) {
		--dot->x;
	} else {
		if (rand <= prob->pr) {
			++dot->x;
		} else {
			if (rand <= prob->pu) {
				++dot->y;
			} else {
				--dot->y;
			}
		}
	}
	++dot->counter;
}

void Walk(particle dot, int* counter, pair xArea, pair yArea, int n,
	probabilities* prob, int a, int b, int l, BufferNode** end) {
	int random;
    while (dot.counter < n &&
    	checkBoundaries(xArea, yArea, dot)) {

        random = rand();
    	Step(&dot, random, prob);

        dot.x += a*l;
    	dot.x %= a*l; 
    	dot.y += b*l;
    	dot.y %= b*l;
    }

    if (dot.counter < n) {
    	int rank = GetRank(dot.x, dot.y, a, l);

    	int* buf = (int*)malloc(3*sizeof(int));
    	buf[0] = dot.x;
    	buf[1] = dot.y;
    	buf[2] = dot.counter;
    	*end = AddBufferNode(*end, buf);

    	MPI_Isend(buf, 3, MPI_INT, rank, 0, MPI_COMM_WORLD,
    		&(*end)->controller.request);
    } else {
    	++(*counter);
    }
}

void ListenToOthersParticles(int* buf, int size, int rank,
	Node** start, Node** currNode) {
	int flag;
	MPI_Status status;

	for (int i = 0; i < size; ++i) {
		if (i != rank) {
			MPI_Iprobe(i, 0, MPI_COMM_WORLD, &flag, &status);
			if (flag) {					
				MPI_Recv(buf, 3, MPI_INT, i, 0,
					MPI_COMM_WORLD, &status);

				if (*start == NULL) {
					*start = AddNode(*start, buf[0], buf[1], buf[2]);
					*currNode = *start;
				} else {
					*currNode = AddNode(*currNode, buf[0], buf[1], buf[2]);
				}					
			}
		}
	}
}

void randomWalk(int rank, probabilities* prob, int n,
	pair xArea, pair yArea, Node* start, int a, int b, int l, int size, int N) {
	srand(rank*time(NULL));
	int* buf = malloc(3 * sizeof(int));
	int counter = 0;
	int my_counter = 0;
	int shared_counter = 0;

	BufferNode* buffer_start = NULL;
	BufferNode* buffer_end = NULL;
	BufferNode* counter_buffer_start = NULL;
	BufferNode* counter_buffer_end = NULL;
	Node* currNode = start;

	while(shared_counter != size*N) {
		counter = 0;

		while (start != NULL) {
			Walk(start->dot, &counter, xArea, yArea,
				n, prob, a, b, l, &buffer_end);
			start = Delete(start);
		}

		my_counter += counter;

		if (buffer_start == NULL) {
			buffer_start = buffer_end;
		}

		if (counter_buffer_start == NULL) {
			counter_buffer_start = counter_buffer_end;
		}

		shared_counter += counter;

		if (counter != 0) {
			SendNewSizeToAll(counter, size, rank, &counter_buffer_end);
		}

		if (buffer_start != NULL) {
			buffer_end = WalkAndFree(&buffer_start);
		}

		if (counter_buffer_start != NULL) {
			counter_buffer_end = WalkAndFree(&counter_buffer_start);
		}

		ListenToOthersSize(size, rank, &shared_counter);
		ListenToOthersParticles(buf, size, rank, &start, &currNode);
	}
	buffer_end = WalkAndFree(&buffer_start);
	counter_buffer_end = WalkAndFree(&counter_buffer_start);
	free(buf);
	MPI_Send(&my_counter, 1, MPI_INT, 0, 2, MPI_COMM_WORLD);
}

Node* GenerateNodes(pair xArea, pair yArea, int N) {
	Node* start = AddNode(NULL, xArea.start, yArea.start, 0);
    Node* currNode = start;

    for (int i = 1; i < N; ++i) {
    	currNode = AddNode(currNode, xArea.start, yArea.start, 0);
    }
    currNode->next = NULL;

    return start;
}

int main(int argc, char** argv) {
	assert(argc == 10);
	int l, a, b, n, N;
	double pl, pr, pu, pd;
	struct timeval begin, end;

	l = atoi(argv[1]); // сторона квадрата
	a = atoi(argv[2]); // кол-во квадратов Ох
	b = atoi(argv[3]); // кол-во квадратов Оу
	n = atoi(argv[4]); // остановка процесса
	N = atoi(argv[5]); // число точек в каждом узле

	pl = atof(argv[6]); // вероятности переходов
	pr = atof(argv[7]);
	pu = atof(argv[8]);
	pd = atof(argv[9]);

	probabilities prob;
	prob.pl = pl;
	prob.pr = pr;
	prob.pu = pu;
	prob.pd = pd;
	ProbabilitiesToIntervals(&prob);

	MPI_Init(&argc, &argv);
	int rank, size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	assert(size == a*b);

    pair xArea;
    xArea.start = l*(rank%a);
    xArea.end = l*(rank%a + 1) - 1;

    pair yArea;
    yArea.start = l*(rank/a);
    yArea.end = l*(rank/a + 1) - 1;

    Node* start = GenerateNodes(xArea, yArea, N);

    assert(gettimeofday(&begin, NULL) == 0);
    randomWalk(rank, &prob, n, xArea,
    	yArea, start, a, b, l, size, N);
    assert(gettimeofday(&end, NULL) == 0);
    double delta = ((end.tv_sec - begin.tv_sec) * 1000000u +
                    end.tv_usec - begin.tv_usec) / 1e6;

    if (rank == 0) {
	    FILE* f = fopen("stats.txt", "a");

	    if (f == NULL) {
	        printf("File opening error...\n");
	        exit(1);
	    }

	    fprintf(f, "%d %d %d %d %d %f %f %f %f %fs\n", l, a,
	    	b, n, N, pl, pr, pu, pd, delta);

	    int* buf = (int*)malloc(sizeof(int));
	    MPI_Status status;

    	for (int i = 0; i < size; ++i) {
    		MPI_Recv(buf, 1, MPI_INT, i, 2,
						MPI_COMM_WORLD, &status);
			fprintf(f, "%d: %d\n", i, buf[0]);	
    	}

    	free(buf);
    	fclose(f);
	}

	MPI_Finalize();

	return 0;
}
