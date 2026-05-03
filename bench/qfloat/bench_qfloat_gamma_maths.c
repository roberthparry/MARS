#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "qfloat.h"

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static int bench_scaled_iters(int base_iters)
{
    const char *scale_text = getenv("MARS_BENCH_SCALE");
    long scale = 1;

    if (scale_text && *scale_text) {
        char *end = NULL;
        long parsed = strtol(scale_text, &end, 10);

        if (end && *end == '\0' && parsed > 0)
            scale = parsed;
    }
    if (base_iters < 1)
        base_iters = 1;
    return (int)(base_iters * scale);
}

static int bench_case_enabled(const char *label)
{
    const char *filter = getenv("MARS_BENCH_FILTER");

    if (!filter || !*filter)
        return 1;
    return strstr(label, filter) != NULL;
}

static void run_unary_case(const char *label,
                           const char *text,
                           qfloat_t (*fn)(qfloat_t),
                           int iters)
{
    qfloat_t src;
    qfloat_t warm;
    uint64_t start;
    uint64_t end;
    double avg_us;

    if (!bench_case_enabled(label))
        return;

    src = qf_from_string(text);
    warm = fn(src);

    if (qf_isnan(warm)) {
        fprintf(stderr, "%s warmup failed\n", label);
        return;
    }

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        qfloat_t value = fn(src);

        if (qf_isnan(value)) {
            fprintf(stderr, "%s timed run failed\n", label);
            return;
        }
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s avg_us=%10.3f avg_ms=%10.3f\n",
           label,
           avg_us,
           avg_us / 1000.0);
}

int main(void)
{
    puts("== qfloat gamma maths bench ==");
    puts("Scale iterations with MARS_BENCH_SCALE=<n> if you want longer runs.");
    puts("");
    run_unary_case("gamma_2_3", "2.3", qf_gamma, bench_scaled_iters(500));
    run_unary_case("lgamma_2_3", "2.3", qf_lgamma, bench_scaled_iters(500));
    run_unary_case("gamma_2_5", "2.5", qf_gamma, bench_scaled_iters(1000));
    run_unary_case("lgamma_2_5", "2.5", qf_lgamma, bench_scaled_iters(1000));
    run_unary_case("gamma_3_5", "3.5", qf_gamma, bench_scaled_iters(1000));
    run_unary_case("lgamma_3_5", "3.5", qf_lgamma, bench_scaled_iters(1000));
    return 0;
}
