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
    printf("%-28s avg_µs=%10.3f avg_ms=%10.3f\n",
           label,
           avg_us,
           avg_us / 1000.0);
}

static void run_binary_case(const char *label,
                            const char *lhs_text,
                            const char *rhs_text,
                            qfloat_t (*fn)(qfloat_t, qfloat_t),
                            int iters)
{
    qfloat_t lhs;
    qfloat_t rhs;
    qfloat_t warm;
    uint64_t start;
    uint64_t end;
    double avg_us;

    if (!bench_case_enabled(label))
        return;

    lhs = qf_from_string(lhs_text);
    rhs = qf_from_string(rhs_text);
    warm = fn(lhs, rhs);

    if (qf_isnan(warm)) {
        fprintf(stderr, "%s warmup failed\n", label);
        return;
    }

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        qfloat_t value = fn(lhs, rhs);

        if (qf_isnan(value)) {
            fprintf(stderr, "%s timed run failed\n", label);
            return;
        }
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s avg_µs=%10.3f avg_ms=%10.3f\n",
           label,
           avg_us,
           avg_us / 1000.0);
}

int main(void)
{
    puts("== qfloat gamma maths bench ==");
    puts("Scale iterations with MARS_BENCH_SCALE=<n> if you want longer runs.");
    puts("");

    run_unary_case("exp_1", "1", qf_exp, bench_scaled_iters(2000));
    run_unary_case("log_10", "10", qf_log, bench_scaled_iters(2000));
    run_unary_case("erf_0_5", "0.5", qf_erf, bench_scaled_iters(2000));
    run_unary_case("erfc_0_5", "0.5", qf_erfc, bench_scaled_iters(2000));

    run_unary_case("gamma_2_3", "2.3", qf_gamma, bench_scaled_iters(500));
    run_unary_case("lgamma_2_3", "2.3", qf_lgamma, bench_scaled_iters(500));
    run_unary_case("gamma_2_5", "2.5", qf_gamma, bench_scaled_iters(1000));
    run_unary_case("lgamma_2_5", "2.5", qf_lgamma, bench_scaled_iters(1000));
    run_unary_case("gamma_3_5", "3.5", qf_gamma, bench_scaled_iters(1000));
    run_unary_case("lgamma_3_5", "3.5", qf_lgamma, bench_scaled_iters(1000));

    run_unary_case("digamma_2_3", "2.3", qf_digamma, bench_scaled_iters(1000));
    run_unary_case("trigamma_2_3", "2.3", qf_trigamma, bench_scaled_iters(500));
    run_unary_case("tetragamma_2_3", "2.3", qf_tetragamma, bench_scaled_iters(500));
    run_unary_case("gammainv_9_5", "119292.4619946090070787515047110059", qf_gammainv, bench_scaled_iters(200));
    run_unary_case("lambert_w0_1", "1", qf_lambert_w0, bench_scaled_iters(2000));
    run_unary_case("lambert_wm1_-0_1", "-0.1", qf_lambert_wm1, bench_scaled_iters(1000));
    run_unary_case("ei_1", "1", qf_ei, bench_scaled_iters(1000));
    run_unary_case("e1_1", "1", qf_e1, bench_scaled_iters(1000));
    run_binary_case("beta_2_3_4_5", "2.3", "4.5", qf_beta, bench_scaled_iters(500));
    run_binary_case("logbeta_2_3_4_5", "2.3", "4.5", qf_logbeta, bench_scaled_iters(500));

    return 0;
}
