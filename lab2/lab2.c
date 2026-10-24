#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

typedef struct {
    size_t n_points;
    size_t hits;
    double radius;
    unsigned int seed;
} ThreadArgs;

void *work(void *ptr)
{
    ThreadArgs *args = (ThreadArgs *)ptr;
    unsigned int seed = args->seed;
    size_t hits = 0;
    double r = args->radius;

    for (size_t i = 0; i < args->n_points; ++i) {
        double x = (2.0 * rand_r(&seed) / (RAND_MAX + 1.0) - 1.0) * r;
        double y = (2.0 * rand_r(&seed) / (RAND_MAX + 1.0) - 1.0) * r;

        if (x * x + y * y <= r * r)
            ++hits;
    }

    args->hits = hits;
    return NULL;
}

int main(int argc, char **argv)
{
    double radius = -1.0;
    size_t n_points = 0;
    size_t n_threads = 0;

    if ((argc - 1) % 2 != 0) {
        printf("Example: ./lab2 --radius 5 --points 10000000 --threads 4\n");
        return 1;
    }

    for (int i = 1; i < argc; i += 2) {
        if (strcmp(argv[i], "--radius") == 0) {
            if (sscanf(argv[i + 1], "%lf", &radius) != 1) return 1;
        } else if (strcmp(argv[i], "--points") == 0) {
            if (sscanf(argv[i + 1], "%zu", &n_points) != 1) return 1;
        } else if (strcmp(argv[i], "--threads") == 0) {
            if (sscanf(argv[i + 1], "%zu", &n_threads) != 1) return 1;
        } else {
            printf("Unknown argument: %s\n", argv[i]);
            return 1;
        }
    }

    if (radius <= 0 || n_points == 0 || n_threads == 0 || n_threads > n_points) {
        printf("Need radius > 0, points >= 1, 1 <= threads <= points\n");
        return 1;
    }

    struct timespec start, end;
    ThreadArgs serial = {n_points, 0, radius, 12345};

    clock_gettime(CLOCK_MONOTONIC, &start);
    work(&serial);
    clock_gettime(CLOCK_MONOTONIC, &end);
    double serial_ms = (end.tv_sec - start.tv_sec) * 1000.0
                     + (end.tv_nsec - start.tv_nsec) / 1000000.0;

    pthread_t *threads = malloc((n_threads - 1) * sizeof(pthread_t));
    ThreadArgs *args = malloc(n_threads * sizeof(ThreadArgs));
    if ((n_threads > 1 && threads == NULL) || args == NULL) {
        printf("Memory allocation failed\n");
        free(threads);
        free(args);
        return 1;
    }

    for (size_t i = 0; i < n_threads; ++i) {
        args[i] = (ThreadArgs){
            .n_points = n_points / n_threads + (i < n_points % n_threads),
            .hits = 0,
            .radius = radius,
            .seed = 12345u + (unsigned int)i * 7919u
        };
    }

    clock_gettime(CLOCK_MONOTONIC, &start);

    size_t created = 0;
    for (size_t i = 0; i + 1 < n_threads; ++i) {
        if (pthread_create(&threads[i], NULL, work, &args[i]) != 0) {
            printf("Could not create thread\n");
            for (size_t j = 0; j < created; ++j)
                pthread_join(threads[j], NULL);
            free(args);
            free(threads);
            return 1;
        }
        ++created;
    }

    work(&args[n_threads - 1]);

    for (size_t i = 0; i < created; ++i)
        pthread_join(threads[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double parallel_ms = (end.tv_sec - start.tv_sec) * 1000.0
                       + (end.tv_nsec - start.tv_nsec) / 1000000.0;

    size_t total_hits = 0;
    for (size_t i = 0; i < n_threads; ++i)
        total_hits += args[i].hits;

    double square_area = 4.0 * radius * radius;
    double serial_area = square_area * (double)serial.hits / (double)n_points;
    double parallel_area = square_area * (double)total_hits / (double)n_points;
    double speedup = serial_ms / parallel_ms;
    double efficiency = speedup / n_threads;

    printf("Radius: %.2f; points: %zu\n", radius, n_points);
    printf("Sequential area: %.6f; time: %.3f ms\n", serial_area, serial_ms);
    printf("Parallel area:   %.6f; time: %.3f ms\n", parallel_area, parallel_ms);
    printf("\nThreads | Time (ms) | Speedup | Efficiency (%%)\n");
    printf("%7zu | %9.3f | %7.3f | %14.2f\n",
           n_threads, parallel_ms, speedup, efficiency * 100.0);

    free(args);
    free(threads);
    return 0;
}
