#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "qcomplex.h"

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
                           qcomplex_t (*fn)(qcomplex_t),
                           int iters)
{
    qcomplex_t src;
    qcomplex_t warm;
    uint64_t start;
    uint64_t end;
    double avg_us;

    if (!bench_case_enabled(label))
        return;

    src = qc_from_string(text);
    warm = fn(src);

    if (qc_isnan(warm)) {
        fprintf(stderr, "%s warmup failed\n", label);
        return;
    }

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        qcomplex_t value = fn(src);

        if (qc_isnan(value)) {
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
                            qcomplex_t (*fn)(qcomplex_t, qcomplex_t),
                            int iters)
{
    qcomplex_t lhs;
    qcomplex_t rhs;
    qcomplex_t warm;
    uint64_t start;
    uint64_t end;
    double avg_us;

    if (!bench_case_enabled(label))
        return;

    lhs = qc_from_string(lhs_text);
    rhs = qc_from_string(rhs_text);
    warm = fn(lhs, rhs);

    if (qc_isnan(warm)) {
        fprintf(stderr, "%s warmup failed\n", label);
        return;
    }

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        qcomplex_t value = fn(lhs, rhs);

        if (qc_isnan(value)) {
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

static qcomplex_t bench_qc_gammainv_gamma_2_5_plus_0_3i(qcomplex_t unused)
{
    static int initialised = 0;
    static qcomplex_t gamma_value;

    (void)unused;

    if (!initialised) {
        gamma_value = qc_gamma(qc_make(qf_from_double(2.5), qf_from_double(0.3)));
        initialised = 1;
    }

    return qc_gammainv(gamma_value);
}

int main(void)
{
    puts("== qcomplex maths bench ==");
    puts("Scale iterations with MARS_BENCH_SCALE=<n> if you want longer runs.");
    puts("");

    run_unary_case("exp_1_plus_1i", "(1,1)", qc_exp, bench_scaled_iters(1000));
    run_unary_case("log_1_plus_1i", "(1,1)", qc_log, bench_scaled_iters(1000));
    run_unary_case("erf_0_5_plus_0_5i", "(0.5,0.5)", qc_erf, bench_scaled_iters(500));
    run_unary_case("erfc_0_5_plus_0_5i", "(0.5,0.5)", qc_erfc, bench_scaled_iters(500));

    run_unary_case("gamma_1_5_plus_0_7i", "(1.5,0.7)", qc_gamma, bench_scaled_iters(200));
    run_unary_case("lgamma_1_5_plus_0_7i", "(1.5,0.7)", qc_lgamma, bench_scaled_iters(200));
    run_unary_case("digamma_2_plus_1i", "(2,1)", qc_digamma, bench_scaled_iters(500));
    run_unary_case("trigamma_2_plus_0_5i", "(2,0.5)", qc_trigamma, bench_scaled_iters(300));
    run_unary_case("tetragamma_2_plus_0_5i", "(2,0.5)", qc_tetragamma, bench_scaled_iters(300));
    run_unary_case("gammainv_gamma_2_5", "3.323350970447842551184064031264648", qc_gammainv, bench_scaled_iters(100));
    run_unary_case("gammainv_gamma_2_5_0_3i", "(0,0)", bench_qc_gammainv_gamma_2_5_plus_0_3i, bench_scaled_iters(100));

    run_unary_case("productlog_1_plus_1i", "(1,1)", qc_productlog, bench_scaled_iters(300));
    run_unary_case("lambert_wm1_-0_2_-0_1i", "(-0.2,-0.1)", qc_lambert_wm1, bench_scaled_iters(200));
    run_unary_case("ei_1_plus_1i", "(1,1)", qc_ei, bench_scaled_iters(300));
    run_unary_case("e1_1_plus_1i", "(1,1)", qc_e1, bench_scaled_iters(300));

    run_binary_case("beta_1_5_0_5__2_-0_3", "(1.5,0.5)", "(2,-0.3)", qc_beta, bench_scaled_iters(200));
    run_binary_case("logbeta_1_5_0_5__2_-0_3", "(1.5,0.5)", "(2,-0.3)", qc_logbeta, bench_scaled_iters(200));

    return 0;
}
