#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>

pthread_mutex_t mutex;
atomic_int available_threads;

int Busy() {
    pthread_mutex_lock(&mutex);

    if (available_threads == 0) {
        pthread_mutex_unlock(&mutex);
        return 1;
    }
    --available_threads;
    pthread_mutex_unlock(&mutex);
    return 0;
}

void NotifyOne() {
    atomic_store(&available_threads, 1);
}

struct sortArgs {
    int* array;
    int left;
    int right;
    int m;
    int* copy_array;
};

struct mergeArgs {
    int* basic_array;
    int* copy_array;
    int left;
    int right;
    int left_;
    int right_;
    int m;
};

struct mergeArgs* GenerateMergeStruct(int* basic_array, int* copy_array,
                                      int left, int right,
                                      int left_, int right_, int m) {

    struct mergeArgs* args_ = malloc(sizeof(struct mergeArgs));
    args_->basic_array = basic_array;
    args_->copy_array = copy_array;
    args_->left = left;
    args_->right = right;
    args_->left_ = left_;
    args_->right_ = right_;
    args_->m = m;

    return args_;
}

int* generateArray(int n) {
    int* res = (int*)malloc(sizeof(int)*n);

    srand(time(NULL));

    for (int i = 0; i < n; ++i) {
        res[i] = rand();
    }

    return res;
}

struct sortArgs* GenerateArgsStruct(struct sortArgs* arg, int left, int right, int* T) {
    struct sortArgs* args_ = malloc(sizeof(struct sortArgs));
    args_->array = arg->array;
    args_->left = left;
    args_->right = right;
    args_->m = arg->m;
    args_->copy_array = T;

    return args_;
}

int comparator(const void* a, const void* b) {
    return *(int*)a - *(int*)b;
}

void printArray(int* array, int n) {
    FILE *f = fopen("data.txt", "a");

    if (f == NULL) {
        printf("File opening error...\n");
        exit(1);
    }

    fprintf(f, "\n");
    fclose(f);
}

int binarySearch(int x, int* array, int start, int end) {
    int left = start;
    int right;

    if (left < end + 1) {
        right = end + 1;
    } else {
        right = left;
    }

    while (left < right) {
        int middle = (left + right)/2;
        if (array[middle] >= x) {
            right = middle;
        }
        else {
            left = middle + 1;
        }
    }
    return right;
}

void exchange(int* a, int* b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

void mergeOneThread(void* args) {
    struct mergeArgs *arg = (struct mergeArgs *)args;

    int i_left = 0;
    int i_right = 0;

    while (arg->left + i_left - 1 < arg->right
           && arg->left_ + i_right - 1 < arg->right_) {
        if (arg->basic_array[arg->left + i_left] <
            arg->basic_array[arg->left_ + i_right]) {
            arg->copy_array[i_left + i_right]
                    = arg->basic_array[arg->left + i_left];
            ++i_left;
        } else {
            arg->copy_array[i_left + i_right]
                    = arg->basic_array[arg->left_ + i_right];
            ++i_right;
        }
    }
    while (arg->left + i_left - 1 < arg->right) {
        arg->copy_array[i_left + i_right]
                = arg->basic_array[arg->left + i_left];
        ++i_left;
    }

    while (arg->left_ + i_right - 1 < arg->right_) {
        arg->copy_array[i_left + i_right]
                = arg->basic_array[arg->left_ + i_right];
        ++i_right;
    }
}

void* merge(void* args) {
    struct mergeArgs *arg = (struct mergeArgs *) args;

    if (arg->right - arg->left < arg->right_ - arg->left_) {
        exchange(&arg->left, &arg->left_);
        exchange(&arg->right, &arg->right_);
    }

    int n = arg->right - arg->left + 1;

    if (n > 0) {
         if (n < arg->m) {
            mergeOneThread(args);
        } else {
        int mid = (arg->left + arg->right) / 2;

        int mid_ = binarySearch(arg->basic_array[mid],
                                arg->basic_array, arg->left_,
                                arg->right_);
        int new_mid = mid - arg->left + mid_ - arg->left_;

        arg->copy_array[new_mid] = arg->basic_array[mid];

        struct mergeArgs *args_left = GenerateMergeStruct(arg->basic_array,
                                                          arg->copy_array, arg->left,
                                                          mid - 1,
                                                          arg->left_, mid_ - 1, arg->m);

        struct mergeArgs *args_right = GenerateMergeStruct(arg->basic_array,
                                                           arg->copy_array +
                                                           new_mid + 1,
                                                           mid + 1, arg->right, mid_,
                                                           arg->right_, arg->m);


        pthread_t thread;
        int new_thread = 1;

        if (Busy()) {
            new_thread = 0;
            merge((void *) args_left);
        } else {
            pthread_create(&thread, NULL, merge, (void *) args_left);
        }

        merge((void *) args_right);

        if (new_thread) {
            pthread_join(thread, NULL);
            NotifyOne();
        }

        free(args_left);
        free(args_right);
        }
    }
}

void* sort(void* args) {
    struct sortArgs *arg = (struct sortArgs *)args;

    int n = arg->right - arg->left + 1;

    if (n <= arg->m) {
        qsort(arg->array + arg->left, n, sizeof(n), comparator);

        for (int i = 0; i < n; ++i) {
            arg->copy_array[i] = arg->array[arg->left + i];
        }

    } else {
        int *T = (int *) malloc(sizeof(int) * n);
        int mid = (arg->left + arg->right) / 2;

        struct sortArgs* args_left = GenerateArgsStruct(arg, arg->left, mid, T);
        struct sortArgs *args_right = GenerateArgsStruct(arg, mid + 1,
                                                         arg->right, T + mid - arg->left + 1);
        pthread_t thread;
        int new_thread = 1;

        if (Busy()) {
            new_thread = 0;
            sort((void *)args_left);
        } else {
            pthread_create(&thread, NULL, sort, (void *) args_left);
        }

        sort((void* )args_right);

        if (new_thread) {
            pthread_join(thread, NULL);
            NotifyOne();
        }

        free(args_left);
        free(args_right);

        struct mergeArgs* args_merge = GenerateMergeStruct(T, arg->copy_array,
                                                           0, mid - arg->left,
                                                           mid - arg->left + 1, n - 1, arg->m);

        merge(args_merge);

        free(T);
        free(args_merge);
    }
}

void Print (char* file, int n, int m, int P, double delta) {
    FILE *f = fopen(file, "a");

    if (f == NULL) {
        printf("File opening error...\n");
        exit(1);
    }

    fprintf(f, "%f %d %d %d \n", delta, n, m, P);

    fclose(f);
}

void qSortTime(int* array, int n, int m, int P) {
    struct timeval start, end;

    assert(gettimeofday(&start, NULL) == 0);
    qsort(array, n, sizeof(int), comparator);
    assert(gettimeofday(&end, NULL) == 0);

    printArray(array, n);

    double delta = ((end.tv_sec - start.tv_sec) * 1000000u +
                    end.tv_usec - start.tv_usec) / 1.e6;

    Print("qsort.txt", n, m, P, delta);
}

int* copy (int* array, int n) {
    int* res = (int*)malloc(sizeof(int)*n);

    for (int i = 0; i < n; ++i) {
        res[i] = array[i];
    }

    return res;
}

int main(int argc, char** argv) {
    int n, m, P;
    struct timeval start, end;

    assert (argc == 4);

    n = atoi(argv[1]);
    m = atoi(argv[2]);
    P = atoi(argv[3]);

    int* array = generateArray(n);
    printArray(array, n);

    int* copy_ = copy(array, n);
    qSortTime(copy_, n, m, P);

    available_threads = P - 1;

    int* result = (int*)malloc(sizeof(int)*n);

    struct sortArgs *args_ = malloc(sizeof(struct sortArgs));
    args_->array = array;
    args_->left = 0;
    args_->right = n - 1;
    args_->m = m;
    args_->copy_array = result;

    assert(gettimeofday(&start, NULL) == 0);
    sort(args_);
    assert(gettimeofday(&end, NULL) == 0);

    double delta = ((end.tv_sec - start.tv_sec) * 1000000u +
                    end.tv_usec - start.tv_usec) / 1.e6;


    Print("stats.txt", n, m, P,delta);

    printArray(result, n);

    for (int i = 0; i < n; ++i) {
        assert(result[i] == copy_[i]);
    }

    free(args_);
    free(copy_);
    free(array);
    free(result);
    return 0;
}
