#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>

#define MAX_ARRAY_SIZE 10000000
#define MAX_THREADS 1024

// Структура для передачи аргументов в поток
typedef struct {
    int* arr;
    int left;
    int right;
    int threads_left;
} ThreadData;

// Самописная функция вывода числа (замена printf)
void write_int(int fd, int num) {
    char buf[20];
    int i = 0;
    int is_negative = 0;
    
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }
    
    if (num == 0) {
        buf[i++] = '0';
    } else {
        while (num > 0) {
            buf[i++] = (num % 10) + '0';
            num /= 10;
        }
    }
    
    if (is_negative) buf[i++] = '-';
    
    // Разворачиваем строку
    for (int j = 0; j < i / 2; j++) {
        char tmp = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = tmp;
    }
    
    write(fd, buf, i);
}

void write_str(int fd, const char* str) {
    int len = 0;
    while (str[len]) len++;
    write(fd, str, len);
}

// Разделение массива
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

// Последовательная сортировка
void sequential_quick_sort(int* arr, int left, int right) {
    if (left < right) {
        int pi = partition(arr, left, right);
        sequential_quick_sort(arr, left, pi - 1);
        sequential_quick_sort(arr, pi + 1, right);
    }
}

// Воркер для потока
void* parallel_sort_worker(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    parallel_quick_sort(data->arr, data->left, data->right, data->threads_left);
    return NULL;
}

// Параллельная сортировка
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

// Замер времени
double get_time_ms(struct timespec start, struct timespec end) {
    return ((end.tv_sec - start.tv_sec) * 1000.0) + 
           ((end.tv_nsec - start.tv_nsec) / 1000000.0);
}

// Свой генератор случайных чисел (LCG)
unsigned int seed = 12345;
int my_rand(void) {
    seed = seed * 1103515245 + 12345;
    return (seed / 65536) % 32768;
}

// Генерация массива
void generate_array(int* arr, int size) {
    for (int i = 0; i < size; i++) {
        arr[i] = my_rand() % 1000000;
    }
}

// Копирование массива
void copy_array(int* dest, int* src, int size) {
    for (int i = 0; i < size; i++) {
        dest[i] = src[i];
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        write_str(STDERR_FILENO, "Usage: ./lab2 <max_threads>\n");
        return 1;
    }
    
    // Парсим ключ запуска
    int max_threads = 0;
    for (int i = 0; argv[1][i]; i++) {
        max_threads = max_threads * 10 + (argv[1][i] - '0');
    }
    
    if (max_threads < 1 || max_threads > MAX_THREADS) {
        write_str(STDERR_FILENO, "Error: threads must be between 1 and 1024\n");
        return 1;
    }
    
    // Статический массив вместо malloc
    static int original_arr[MAX_ARRAY_SIZE];
    static int work_arr[MAX_ARRAY_SIZE];
    
    generate_array(original_arr, MAX_ARRAY_SIZE);
    
    struct timespec start, end;
    
    // 1. Последовательная сортировка
    write_str(STDOUT_FILENO, "Sequential (1 thread): ");
    copy_array(work_arr, original_arr, MAX_ARRAY_SIZE);
    clock_gettime(CLOCK_MONOTONIC, &start);
    sequential_quick_sort(work_arr, 0, MAX_ARRAY_SIZE - 1);
    clock_gettime(CLOCK_MONOTONIC, &end);
    double seq_time = get_time_ms(start, end);
    write_int(STDOUT_FILENO, (int)seq_time);
    write_str(STDOUT_FILENO, " ms\n\n");
    
    // 2. Параллельная сортировка с разным числом потоков
    write_str(STDOUT_FILENO, "Threads | Time (ms) | Speedup | Efficiency\n");
    write_str(STDOUT_FILENO, "-------------------------------------------\n");
    
    write_int(STDOUT_FILENO, 1);
    write_str(STDOUT_FILENO, "        | ");
    write_int(STDOUT_FILENO, (int)seq_time);
    write_str(STDOUT_FILENO, "       | 1.00      | 1.0000\n");
    
    int test_counts[] = {2, 4, 8, 16, 32, 64, 128, 256};
    int num_tests = sizeof(test_counts) / sizeof(test_counts[0]);
    
    for (int i = 0; i < num_tests; i++) {
        int t = test_counts[i];
        if (t > max_threads) break;  // Не превышаем лимит из ключа
        
        copy_array(work_arr, original_arr, MAX_ARRAY_SIZE);
        
        clock_gettime(CLOCK_MONOTONIC, &start);
        parallel_quick_sort(work_arr, 0, MAX_ARRAY_SIZE - 1, t);
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        double par_time = get_time_ms(start, end);
        double speedup = seq_time / par_time;
        double efficiency = speedup / t;
        
        write_int(STDOUT_FILENO, t);
        write_str(STDOUT_FILENO, "        | ");
        write_int(STDOUT_FILENO, (int)par_time);
        write_str(STDOUT_FILENO, "       | ");
        write_int(STDOUT_FILENO, (int)(speedup * 100));
        write_str(STDOUT_FILENO, ".");
        int frac = (int)(speedup * 10000) % 100;
        if (frac < 10) write_str(STDOUT_FILENO, "0");
        write_int(STDOUT_FILENO, frac);
        write_str(STDOUT_FILENO, "      | ");
        write_int(STDOUT_FILENO, (int)(efficiency * 10000));
        write_str(STDOUT_FILENO, "\n");
    }
    
    write_str(STDOUT_FILENO, "\nTo check number of threads, run:\n");
    write_str(STDOUT_FILENO, "  ps -o pid,nlwp,cmd -p <PID>\n");
    write_str(STDOUT_FILENO, "  top -H -p <PID>\n");
    write_str(STDOUT_FILENO, "  cat /proc/<PID>/status | grep Threads\n");
    
    return 0;
}
