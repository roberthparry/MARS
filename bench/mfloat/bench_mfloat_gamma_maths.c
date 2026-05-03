#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "mfloat.h"

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

static void run_unary_case(const char *label,
                           const char *text,
                           size_t precision,
                           int (*fn)(mfloat_t *),
                           int iters)
{
    size_t old_prec;
    mfloat_t *src;
    uint64_t start;
    uint64_t end;
    double avg_us;

    old_prec = mf_get_default_precision();
    if (mf_set_default_precision(precision) != 0) {
        fprintf(stderr, "%s set default precision failed\n", label);
        return;
    }

    src = mf_create_string(text);
    if (!src) {
        fprintf(stderr, "%s source create failed\n", label);
        (void)mf_set_default_precision(old_prec);
        return;
    }

    {
        mfloat_t *warm = mf_clone(src);

        if (!warm || fn(warm) != 0) {
            fprintf(stderr, "%s warmup failed\n", label);
            mf_free(warm);
            mf_free(src);
            (void)mf_set_default_precision(old_prec);
            return;
        }
        mf_free(warm);
    }

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mfloat_t *value = mf_clone(src);

        if (!value || fn(value) != 0) {
            fprintf(stderr, "%s timed run failed\n", label);
            mf_free(value);
            mf_free(src);
            (void)mf_set_default_precision(old_prec);
            return;
        }
        mf_free(value);
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s avg_us=%10.3f avg_ms=%10.3f\n",
           label,
           avg_us,
           avg_us / 1000.0);

    mf_free(src);
    (void)mf_set_default_precision(old_prec);
}

int main(void)
{
    puts("== mfloat gamma maths bench ==");
    puts("Scale iterations with MARS_BENCH_SCALE=<n> if you want longer runs.");
    puts("");
    run_unary_case("gamma_2_5_256", "2.5", 256u, mf_gamma, bench_scaled_iters(8));
    run_unary_case("lgamma_2_5_256", "2.5", 256u, mf_lgamma, bench_scaled_iters(8));
    run_unary_case("gamma_3_5_256", "3.5", 256u, mf_gamma, bench_scaled_iters(8));
    run_unary_case("lgamma_3_5_256", "3.5", 256u, mf_lgamma, bench_scaled_iters(8));
    run_unary_case("gamma_2_5_512", "2.5", 512u, mf_gamma, bench_scaled_iters(2));
    run_unary_case("lgamma_2_5_512", "2.5", 512u, mf_lgamma, bench_scaled_iters(2));
    return 0;
}
