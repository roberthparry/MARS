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

static void run_binary_case(const char *label,
                            const char *lhs_text,
                            const char *rhs_text,
                            size_t precision,
                            int (*fn)(mfloat_t *, const mfloat_t *),
                            int iters)
{
    size_t old_prec;
    mfloat_t *lhs_src;
    mfloat_t *rhs;
    uint64_t start;
    uint64_t end;
    double avg_us;

    old_prec = mf_get_default_precision();
    if (mf_set_default_precision(precision) != 0) {
        fprintf(stderr, "%s set default precision failed\n", label);
        return;
    }

    lhs_src = mf_create_string(lhs_text);
    rhs = mf_create_string(rhs_text);
    if (!lhs_src || !rhs) {
        fprintf(stderr, "%s source create failed\n", label);
        mf_free(lhs_src);
        mf_free(rhs);
        (void)mf_set_default_precision(old_prec);
        return;
    }

    {
        mfloat_t *warm = mf_clone(lhs_src);

        if (!warm || fn(warm, rhs) != 0) {
            fprintf(stderr, "%s warmup failed\n", label);
            mf_free(warm);
            mf_free(lhs_src);
            mf_free(rhs);
            (void)mf_set_default_precision(old_prec);
            return;
        }
        mf_free(warm);
    }

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mfloat_t *value = mf_clone(lhs_src);

        if (!value || fn(value, rhs) != 0) {
            fprintf(stderr, "%s timed run failed\n", label);
            mf_free(value);
            mf_free(lhs_src);
            mf_free(rhs);
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

    mf_free(lhs_src);
    mf_free(rhs);
    (void)mf_set_default_precision(old_prec);
}

static void run_const_case(const char *label,
                           size_t precision,
                           mfloat_t *(*fn)(void),
                           int iters)
{
    size_t old_prec;
    uint64_t start;
    uint64_t end;
    double avg_us;

    old_prec = mf_get_default_precision();
    if (mf_set_default_precision(precision) != 0) {
        fprintf(stderr, "%s set default precision failed\n", label);
        return;
    }

    {
        mfloat_t *warm = fn();

        if (!warm) {
            fprintf(stderr, "%s warmup failed\n", label);
            (void)mf_set_default_precision(old_prec);
            return;
        }
        mf_free(warm);
    }

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mfloat_t *value = fn();

        if (!value) {
            fprintf(stderr, "%s timed run failed\n", label);
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

    (void)mf_set_default_precision(old_prec);
}

int main(void)
{
    puts("== mfloat native math bench ==");
    run_unary_case("exp_256", "1.23456789", 256u, mf_exp, 20);
    run_unary_case("log_256", "1.23456789", 256u, mf_log, 20);
    run_unary_case("sin_256", "0.7", 256u, mf_sin, 20);
    run_unary_case("cos_256", "0.7", 256u, mf_cos, 20);
    run_unary_case("atan_256", "0.7", 256u, mf_atan, 20);
    run_unary_case("sinh_256", "0.7", 256u, mf_sinh, 12);
    run_unary_case("cosh_256", "0.7", 256u, mf_cosh, 12);
    run_unary_case("tanh_256", "0.7", 256u, mf_tanh, 12);
    run_binary_case("pow_256", "1.23456789", "3.5", 256u, mf_pow, 12);

    puts("");
    run_const_case("pi_512", 512u, mf_pi, 8);
    run_const_case("e_512", 512u, mf_e, 8);
    run_const_case("gamma_512", 512u, mf_euler_mascheroni, 4);
    run_unary_case("exp_512", "1.23456789", 512u, mf_exp, 6);
    run_unary_case("log_512", "1.23456789", 512u, mf_log, 6);
    run_unary_case("sin_512", "0.7", 512u, mf_sin, 6);
    run_unary_case("atan_512", "0.7", 512u, mf_atan, 6);
    run_binary_case("pow_512", "1.23456789", "3.5", 512u, mf_pow, 4);

    return EXIT_SUCCESS;
}
