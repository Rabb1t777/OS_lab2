#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define ARRAY_SIZE 10000000

typedef struct {
    int* arr;
    int left;
    int right;
    int threads_left;
} ThreadData;

int partition(int* arr, int left, int right);
void sequential_quick_sort(int* arr, int left, int right);
void parallel_quick_sort(int* arr, int left, int right, int threads);
void* parallel_sort_worker(void* arg);
double get_time_ms(struct timespec start, struct timespec end);
void generate_array(int* arr, int size);
void copy_array(int* dest, int* src, int size);

int partition(int* arr, int left, int right) {
    int pivot = arr[right];
    int i = left - 1;
    for (int j = left; j < right; j++) {
        if (arr[j] <= pivot) {
            i++;
            int temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
        }
    }
    int temp = arr[i + 1];
    arr[i + 1] = arr[right];
    arr[right] = temp;
    return i + 1;
}

void sequential_quick_sort(int* arr, int left, int right) {
    if (left < right) {
        int pi = partition(arr, left, right);
        sequential_quick_sort(arr, left, pi - 1);
        sequential_quick_sort(arr, pi + 1, right);
    }
}

void* parallel_sort_worker(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    parallel_quick_sort(data->arr, data->left, data->right, data->threads_left);
    return NULL;
}

void parallel_quick_sort(int* arr, int left, int right, int threads) {
    if (left >= right) return;
    int pi = partition(arr, left, right);
    if (threads > 1) {
        pthread_t thread;
        ThreadData right_data = {arr, pi + 1, right, threads / 2};
        pthread_create(&thread, NULL, parallel_sort_worker, &right_data);
        parallel_quick_sort(arr, left, pi - 1, threads - threads / 2);
        pthread_join(thread, NULL);
    } else {
        parallel_quick_sort(arr, left, pi - 1, 1);
        parallel_quick_sort(arr, pi + 1, right, 1);
    }
}

double get_time_ms(struct timespec start, struct timespec end) {
    return ((end.tv_sec - start.tv_sec) * 1000.0) + 
           ((end.tv_nsec - start.tv_nsec) / 1000000.0);
}

void generate_array(int* arr, int size) {
    for (int i = 0; i < size; i++) {
        arr[i] = rand() % 1000000;
    }
}

void copy_array(int* dest, int* src, int size) {
    for (int i = 0; i < size; i++) {
        dest[i] = src[i];
    }
}

int main() {
    int* original_arr = (int*)malloc(ARRAY_SIZE * sizeof(int));
    int* work_arr = (int*)malloc(ARRAY_SIZE * sizeof(int));
    srand(time(NULL));
    generate_array(original_arr, ARRAY_SIZE);
    struct timespec start, end;

    copy_array(work_arr, original_arr, ARRAY_SIZE);
    clock_gettime(CLOCK_MONOTONIC, &start);
    sequential_quick_sort(work_arr, 0, ARRAY_SIZE - 1);
    clock_gettime(CLOCK_MONOTONIC, &end);
    double seq_time = get_time_ms(start, end);
    printf("Sequential (1 thread): %.2f ms\n", seq_time);

    int thread_counts[] = {1, 2, 4, 6, 8, 16, 64, 256};
    int num_tests = sizeof(thread_counts) / sizeof(thread_counts[0]);
    printf("\n%-10s | %-15s | %-10s | %-10s\n", "Threads", "Time (ms)", "Speedup", "Efficiency");
    printf("------------------------------------------------------\n");
    printf("%-10d | %-15.2f | %-10.2f | %-10.2f\n", 1, seq_time, 1.0, 1.0);
    for (int i = 1; i < num_tests; i++) {
        int t = thread_counts[i];
        copy_array(work_arr, original_arr, ARRAY_SIZE);
        clock_gettime(CLOCK_MONOTONIC, &start);
        parallel_quick_sort(work_arr, 0, ARRAY_SIZE - 1, t);
        clock_gettime(CLOCK_MONOTONIC, &end);
        double par_time = get_time_ms(start, end);
        double speedup = seq_time / par_time;
        double efficiency = speedup / t;
        printf("%-10d | %-15.2f | %-10.2f | %-10.4f\n", t, par_time, speedup, efficiency);
    }
    free(original_arr);
    free(work_arr);
    return 0;
}
