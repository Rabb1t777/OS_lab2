#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static int running = 1;
static int peak = 1;
static int create_errors = 0;

static void enter_thread(void) {
    pthread_mutex_lock(&m);
    running++;
    if (running > peak) peak = running;
    pthread_mutex_unlock(&m);
}

static void leave_thread(void) {
    pthread_mutex_lock(&m);
    running--;
    pthread_mutex_unlock(&m);
}

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

static size_t partition(int *a, size_t left, size_t right) {
    int pivot = a[right];
    size_t i = left;
    for (size_t j = left; j < right; j++) {
        if (a[j] <= pivot) {
            int temp = a[i];
            a[i] = a[j];
            a[j] = temp;
            i++;
        }
    }
    int temp = a[i];
    a[i] = a[right];
    a[right] = temp;
    return i;
}

static void sort_seq(int *a, size_t left, size_t right) {
    if (left < right) {
        size_t pi = partition(a, left, right);
        if (pi > 0) sort_seq(a, left, pi - 1);
        sort_seq(a, pi + 1, right);
    }
}

typedef struct {
    int *a;
    size_t left, right;
    int threads_left;
    bool demo;
} task_t;

static void *thread_func(void *arg);

static void sort_par(int *a, size_t left, size_t right, int threads, bool demo) {
    if (left >= right) return;
    
    size_t pi = partition(a, left, right);
    
    if (threads > 1) {
        pthread_t thread;
        task_t task = {a, pi + 1, right, threads / 2, demo};
        
        int result = pthread_create(&thread, NULL, thread_func, &task);
        if (result != 0) {
            create_errors++;
            // Fallback: выполняем работу в текущем потоке
            sort_par(a, pi + 1, right, threads / 2, false);
        } else {
            // Текущий поток сортирует левую часть
            if (pi > 0) {
                sort_par(a, left, pi - 1, threads - threads / 2, demo);
            }
            pthread_join(thread, NULL);
        }
    } else {
        // Один поток - сортируем последовательно
        if (pi > 0) sort_par(a, left, pi - 1, 1, false);
        sort_par(a, pi + 1, right, 1, false);
    }
}

static void *thread_func(void *arg) {
    task_t *t = arg;
    enter_thread();
    if (t->demo) sleep(5);
    sort_par(t->a, t->left, t->right, t->threads_left, false);
    leave_thread();
    return NULL;
}

static void sort_par_wrapper(int *a, size_t N, int p, bool demo) {
    if (p < 1) p = 1;
    if (N < 2) return;
    
    pthread_t thread;
    task_t task = {a, 0, N - 1, p - 1, demo};
    
    int result = pthread_create(&thread, NULL, thread_func, &task);
    if (result != 0) {
        create_errors++;
        // Fallback: выполняем всё в главном потоке
        sort_par(a, 0, N - 1, 1, false);
    } else {
        // Главный поток тоже работает
        sort_par(a, 0, N - 1, 1, demo);
        pthread_join(thread, NULL);
    }
}

static int is_sorted(const int *a, size_t n) {
    for (size_t i = 1; i < n; i++)
        if (a[i - 1] > a[i]) return 0;
    return 1;
}

static void print_arr(const char *title, const int *a, size_t n) {
    size_t k = n < 10 ? n : 10;
    printf("%s\n начало:", title);
    for (size_t i = 0; i < k; i++) printf(" %d", a[i]);
    printf("\n конец: ");
    for (size_t i = n - k; i < n; i++) printf(" %d", a[i]);
    printf("\n");
}

int main(int argc, char **argv) {
    long n = 10000000;
    int p = 1;
    bool demo = false;
    bool show = false;
    int o;
    
    while ((o = getopt(argc, argv, "n:t:dp")) != -1) {
        if (o == 'n') n = atol(optarg);
        else if (o == 't') p = atoi(optarg);
        else if (o == 'd') demo = true;
        else if (o == 'p') show = true;
        else return 1;
    }
    
    if (p < 1) p = 1;
    if (n < 2) {
        fprintf(stderr, "Размер массива должен быть не меньше 2\n");
        return 1;
    }
    
    int *orig = malloc(n * sizeof(int));
    int *a = malloc(n * sizeof(int));
    int *b = malloc(n * sizeof(int));
    if (!orig || !a || !b) {
        fprintf(stderr, "Не хватает памяти\n");
        return 1;
    }
    
    srand(1);
    for (long i = 0; i < n; i++) orig[i] = rand() % 1000000;
    
    printf("PID=%d, логических ядер=%ld, n=%ld, потоков (лимит)=%d\n",
           getpid(), sysconf(_SC_NPROCESSORS_ONLN), n, p);
    if (show) print_arr("Массив до сортировки:", orig, n);
    
    memcpy(a, orig, n * sizeof(int));
    double t0 = now_ms();
    sort_seq(a, 0, n - 1);
    double ts = now_ms() - t0;
    
    memcpy(b, orig, n * sizeof(int));
    t0 = now_ms();
    sort_par_wrapper(b, n, p, demo);
    double tp = now_ms() - t0;
    
    if (show) print_arr("Массив после параллельной сортировки:", b, n);
    
    bool ok = memcmp(a, b, n * sizeof(int)) == 0 && is_sorted(b, n);
    double sp = ts / tp;
    double ef = sp / p;
    
    printf("Проверка: %s, пик потоков: %d\n", ok ? "OK" : "ОШИБКА", peak);
    if (create_errors > 0)
        printf("Ошибок создания потоков: %d (их работу выполнил главный поток)\n", create_errors);
    printf("Время последовательной версии: %.2f мс\n", ts);
    printf("Время параллельной версии: %.2f мс\n", tp);
    printf("Ускорение: %.3f\n", sp);
    printf("Эффективность: %.3f\n", ef);
    
    free(orig);
    free(a);
    free(b);
    return ok ? 0 : 2;
}
