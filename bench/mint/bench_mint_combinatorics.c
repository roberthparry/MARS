#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "mint.h"

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void run_factorial_case(const char *label, unsigned long n, int iters)
{
    uint64_t start, end;
    double avg_us;

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *m = mi_new();

        if (!m || mi_factorial(m, n) != 0) {
            fprintf(stderr, "%s failed\n", label);
            mi_free(m);
            return;
        }
        mi_free(m);
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s avg_µs=%10.3f avg_ms=%10.3f\n",
           label, avg_us, avg_us / 1000.0);
}

static void run_fibonacci_case(const char *label, unsigned long n, int iters)
{
    uint64_t start, end;
    double avg_us;

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *m = mi_new();

        if (!m || mi_fibonacci(m, n) != 0) {
            fprintf(stderr, "%s failed\n", label);
            mi_free(m);
            return;
        }
        mi_free(m);
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s avg_µs=%10.3f avg_ms=%10.3f\n",
           label, avg_us, avg_us / 1000.0);
}

static void run_binomial_case(const char *label,
                              unsigned long n,
                              unsigned long k,
                              int iters)
{
    uint64_t start, end;
    double avg_us;

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *m = mi_new();

        if (!m || mi_binomial(m, n, k) != 0) {
            fprintf(stderr, "%s failed\n", label);
            mi_free(m);
            return;
        }
        mi_free(m);
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s avg_µs=%10.3f avg_ms=%10.3f\n",
           label, avg_us, avg_us / 1000.0);
}

int main(void)
{
    const int iters_small = 2000;
    const int iters_large = 200;

    printf("Combinatorics and sequences\n");
    printf("iters_small=%d iters_large=%d\n\n", iters_small, iters_large);

    run_factorial_case("factorial_100", 100, iters_small);
    run_factorial_case("factorial_1000", 1000, iters_large);

    printf("\n");
    run_fibonacci_case("fibonacci_1000", 1000, iters_small);
    run_fibonacci_case("fibonacci_10000", 10000, iters_large);

    printf("\n");
    run_binomial_case("binomial_100_50", 100, 50, iters_small);
    run_binomial_case("binomial_1000_500", 1000, 500, iters_large);

    return 0;
}
