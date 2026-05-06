#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mcomplex.h"

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

static int bench_run_unary_inplace(mcomplex_t *value, int (*fn)(mcomplex_t *))
{
    return fn ? fn(value) : -1;
}

static int bench_run_binary_inplace(mcomplex_t *lhs,
                                    const mcomplex_t *rhs,
                                    int (*fn)(mcomplex_t *, const mcomplex_t *))
{
    return fn ? fn(lhs, rhs) : -1;
}

static void run_unary_case(const char *label,
                           const char *text,
                           size_t precision,
                           int (*fn)(mcomplex_t *),
                           int iters)
{
    size_t old_prec;
    mcomplex_t *src = NULL;
    mcomplex_t *work = NULL;
    uint64_t start;
    uint64_t end;
    double avg_us;

    if (!bench_case_enabled(label))
        return;

    old_prec = mf_get_default_precision();
    if (mf_set_default_precision(precision) != 0) {
        fprintf(stderr, "%s set default precision failed\n", label);
        return;
    }

    src = mc_create_string(text);
    work = mc_clone(src);
    if (!src || !work) {
        fprintf(stderr, "%s setup failed\n", label);
        goto cleanup;
    }

    if (bench_run_unary_inplace(work, fn) != 0 || mc_isnan(work)) {
        fprintf(stderr, "%s warmup failed\n", label);
        goto cleanup;
    }

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        if (mc_set(work, mc_real(src), mc_imag(src)) != 0 ||
            bench_run_unary_inplace(work, fn) != 0 ||
            mc_isnan(work)) {
            fprintf(stderr, "%s timed run failed\n", label);
            goto cleanup;
        }
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s bits=%-4zu avg_µs=%10.3f avg_ms=%10.3f\n",
           label,
           precision,
           avg_us,
           avg_us / 1000.0);

cleanup:
    mc_free(work);
    mc_free(src);
    (void)mf_set_default_precision(old_prec);
}

static void run_binary_case(const char *label,
                            const char *lhs_text,
                            const char *rhs_text,
                            size_t precision,
                            int (*fn)(mcomplex_t *, const mcomplex_t *),
                            int iters)
{
    size_t old_prec;
    mcomplex_t *lhs = NULL;
    mcomplex_t *rhs = NULL;
    mcomplex_t *work = NULL;
    uint64_t start;
    uint64_t end;
    double avg_us;

    if (!bench_case_enabled(label))
        return;

    old_prec = mf_get_default_precision();
    if (mf_set_default_precision(precision) != 0) {
        fprintf(stderr, "%s set default precision failed\n", label);
        return;
    }

    lhs = mc_create_string(lhs_text);
    rhs = mc_create_string(rhs_text);
    work = mc_clone(lhs);
    if (!lhs || !rhs || !work) {
        fprintf(stderr, "%s setup failed\n", label);
        goto cleanup;
    }

    if (bench_run_binary_inplace(work, rhs, fn) != 0 || mc_isnan(work)) {
        fprintf(stderr, "%s warmup failed\n", label);
        goto cleanup;
    }

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        if (mc_set(work, mc_real(lhs), mc_imag(lhs)) != 0 ||
            bench_run_binary_inplace(work, rhs, fn) != 0 ||
            mc_isnan(work)) {
            fprintf(stderr, "%s timed run failed\n", label);
            goto cleanup;
        }
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s bits=%-4zu avg_µs=%10.3f avg_ms=%10.3f\n",
           label,
           precision,
           avg_us,
           avg_us / 1000.0);

cleanup:
    mc_free(work);
    mc_free(rhs);
    mc_free(lhs);
    (void)mf_set_default_precision(old_prec);
}

static int bench_mc_gammainv_gamma_2_5_plus_0_3i(mcomplex_t *value)
{
    static mcomplex_t *gamma_value = NULL;

    if (!value)
        return -1;

    if (!gamma_value) {
        gamma_value = mc_create_string("2.5 + 0.3i");
        if (!gamma_value || mc_gamma(gamma_value) != 0)
            return -1;
    }

    if (mc_set(value, mc_real(gamma_value), mc_imag(gamma_value)) != 0)
        return -1;
    return mc_gammainv(value);
}

int main(void)
{
    puts("== mcomplex maths bench ==");
    puts("Prepared for post-implementation benchmarking.");
    puts("Do not treat timings from the current wrapper-backed implementation as final.");
    puts("Scale iterations with MARS_BENCH_SCALE=<n> if you want longer runs.");
    puts("Filter individual cases with MARS_BENCH_FILTER=<substring>.");
    puts("");

    run_unary_case("exp_1_plus_1i", "1 + 1i", 256u, mc_exp, bench_scaled_iters(1000));
    run_unary_case("log_1_plus_1i", "1 + 1i", 256u, mc_log, bench_scaled_iters(1000));
    run_unary_case("sin_0_567_plus_0_321i", "0.567 + 0.321i", 256u, mc_sin, bench_scaled_iters(8));
    run_unary_case("cos_0_567_plus_0_321i", "0.567 + 0.321i", 256u, mc_cos, bench_scaled_iters(8));
    run_unary_case("tan_0_567_plus_0_321i", "0.567 + 0.321i", 256u, mc_tan, bench_scaled_iters(6));
    run_unary_case("atan_0_321_plus_0_123i", "0.321 + 0.123i", 256u, mc_atan, bench_scaled_iters(6));
    run_binary_case("atan2_0_5_0_25i__-0_75_0_1i", "0.5 + 0.25i", "-0.75 + 0.1i", 256u, mc_atan2, bench_scaled_iters(4));
    run_unary_case("asin_0_321_plus_0_123i", "0.321 + 0.123i", 256u, mc_asin, bench_scaled_iters(4));
    run_unary_case("acos_0_321_plus_0_123i", "0.321 + 0.123i", 256u, mc_acos, bench_scaled_iters(4));
    run_unary_case("sinh_0_567_plus_0_321i", "0.567 + 0.321i", 256u, mc_sinh, bench_scaled_iters(8));
    run_unary_case("cosh_0_567_plus_0_321i", "0.567 + 0.321i", 256u, mc_cosh, bench_scaled_iters(8));
    run_unary_case("tanh_0_567_plus_0_321i", "0.567 + 0.321i", 256u, mc_tanh, bench_scaled_iters(6));
    run_unary_case("asinh_0_321_plus_0_123i", "0.321 + 0.123i", 256u, mc_asinh, bench_scaled_iters(4));
    run_unary_case("acosh_2_plus_0_5i", "2 + 0.5i", 256u, mc_acosh, bench_scaled_iters(4));
    run_unary_case("atanh_0_321_plus_0_123i", "0.321 + 0.123i", 256u, mc_atanh, bench_scaled_iters(4));
    run_unary_case("erf_0_5_plus_0_5i", "0.5 + 0.5i", 256u, mc_erf, bench_scaled_iters(500));
    run_unary_case("erfc_0_5_plus_0_5i", "0.5 + 0.5i", 256u, mc_erfc, bench_scaled_iters(500));

    run_unary_case("gamma_2_3_plus_0i", "2.3 + 0i", 256u, mc_gamma, bench_scaled_iters(4));
    run_unary_case("lgamma_2_3_plus_0i", "2.3 + 0i", 256u, mc_lgamma, bench_scaled_iters(4));
    run_unary_case("gamma_1_5_plus_0_7i", "1.5 + 0.7i", 256u, mc_gamma, bench_scaled_iters(8));
    run_unary_case("lgamma_1_5_plus_0_7i", "1.5 + 0.7i", 256u, mc_lgamma, bench_scaled_iters(8));
    run_unary_case("digamma_2_plus_1i", "2 + 1i", 256u, mc_digamma, bench_scaled_iters(500));
    run_unary_case("trigamma_2_plus_0_5i", "2 + 0.5i", 256u, mc_trigamma, bench_scaled_iters(300));
    run_unary_case("tetragamma_2_plus_0_5i", "2 + 0.5i", 256u, mc_tetragamma, bench_scaled_iters(300));
    run_unary_case("gammainv_gamma_2_5", "3.323350970447842551184064031264648", 256u, mc_gammainv, bench_scaled_iters(100));
    run_unary_case("gammainv_gamma_2_5_0_3i", "0 + 0i", 256u, bench_mc_gammainv_gamma_2_5_plus_0_3i, bench_scaled_iters(100));

    run_unary_case("lambert_w0_1", "1", 256u, mc_lambert_w0, bench_scaled_iters(2000));
    run_unary_case("productlog_1", "1", 256u, mc_productlog, bench_scaled_iters(2000));
    run_unary_case("lambert_wm1_-0_1", "-0.1", 256u, mc_lambert_wm1, bench_scaled_iters(1000));
    run_unary_case("productlog_1_plus_1i", "1 + 1i", 256u, mc_productlog, bench_scaled_iters(8));
    run_unary_case("lambert_wm1_-0_2_-0_1i", "-0.2 - 0.1i", 256u, mc_lambert_wm1, bench_scaled_iters(6));
    run_unary_case("ei_1_plus_1i", "1 + 1i", 256u, mc_ei, bench_scaled_iters(300));
    run_unary_case("e1_1_plus_1i", "1 + 1i", 256u, mc_e1, bench_scaled_iters(300));

    run_binary_case("beta_1_5_0_5__2_-0_3", "1.5 + 0.5i", "2 - 0.3i", 256u, mc_beta, bench_scaled_iters(200));
    run_binary_case("logbeta_1_5_0_5__2_-0_3", "1.5 + 0.5i", "2 - 0.3i", 256u, mc_logbeta, bench_scaled_iters(200));

    run_unary_case("exp_1_plus_1i_512", "1 + 1i", 512u, mc_exp, bench_scaled_iters(40));
    run_unary_case("log_1_plus_1i_512", "1 + 1i", 512u, mc_log, bench_scaled_iters(40));
    run_unary_case("sin_0_567_plus_0_321i_512", "0.567 + 0.321i", 512u, mc_sin, bench_scaled_iters(4));
    run_unary_case("cos_0_567_plus_0_321i_512", "0.567 + 0.321i", 512u, mc_cos, bench_scaled_iters(4));
    run_unary_case("tan_0_567_plus_0_321i_512", "0.567 + 0.321i", 512u, mc_tan, bench_scaled_iters(3));
    run_unary_case("atan_0_321_plus_0_123i_512", "0.321 + 0.123i", 512u, mc_atan, bench_scaled_iters(3));
    run_binary_case("atan2_0_5_0_25i__-0_75_0_1i_512", "0.5 + 0.25i", "-0.75 + 0.1i", 512u, mc_atan2, bench_scaled_iters(8));
    run_unary_case("asin_0_321_plus_0_123i_512", "0.321 + 0.123i", 512u, mc_asin, bench_scaled_iters(8));
    run_unary_case("acos_0_321_plus_0_123i_512", "0.321 + 0.123i", 512u, mc_acos, bench_scaled_iters(8));
    run_unary_case("sinh_0_567_plus_0_321i_512", "0.567 + 0.321i", 512u, mc_sinh, bench_scaled_iters(4));
    run_unary_case("cosh_0_567_plus_0_321i_512", "0.567 + 0.321i", 512u, mc_cosh, bench_scaled_iters(4));
    run_unary_case("tanh_0_567_plus_0_321i_512", "0.567 + 0.321i", 512u, mc_tanh, bench_scaled_iters(12));
    run_unary_case("asinh_0_321_plus_0_123i_512", "0.321 + 0.123i", 512u, mc_asinh, bench_scaled_iters(8));
    run_unary_case("acosh_2_plus_0_5i_512", "2 + 0.5i", 512u, mc_acosh, bench_scaled_iters(8));
    run_unary_case("atanh_0_321_plus_0_123i_512", "0.321 + 0.123i", 512u, mc_atanh, bench_scaled_iters(8));
    run_unary_case("lambert_w0_1_plus_1i_512", "1 + 1i", 512u, mc_lambert_w0, bench_scaled_iters(4));
    run_unary_case("lambert_wm1_-0_2_-0_1i_512", "-0.2 - 0.1i", 512u, mc_lambert_wm1, bench_scaled_iters(2));
    run_unary_case("productlog_1_plus_1i_512", "1 + 1i", 512u, mc_productlog, bench_scaled_iters(4));
    run_unary_case("ei_1_plus_1i_512", "1 + 1i", 512u, mc_ei, bench_scaled_iters(8));
    run_unary_case("e1_1_plus_1i_512", "1 + 1i", 512u, mc_e1, bench_scaled_iters(8));
    run_unary_case("gamma_2_3_plus_0i_512", "2.3 + 0i", 512u, mc_gamma, bench_scaled_iters(1));
    run_unary_case("lgamma_2_3_plus_0i_512", "2.3 + 0i", 512u, mc_lgamma, bench_scaled_iters(1));
    run_unary_case("gamma_1_5_plus_0_7i_512", "1.5 + 0.7i", 512u, mc_gamma, bench_scaled_iters(1));
    run_unary_case("lgamma_1_5_plus_0_7i_512", "1.5 + 0.7i", 512u, mc_lgamma, bench_scaled_iters(1));

    run_unary_case("exp_1_plus_1i_768", "1 + 1i", 768u, mc_exp, bench_scaled_iters(4));
    run_unary_case("log_1_plus_1i_768", "1 + 1i", 768u, mc_log, bench_scaled_iters(4));
    run_unary_case("sin_0_567_plus_0_321i_768", "0.567 + 0.321i", 768u, mc_sin, bench_scaled_iters(2));
    run_unary_case("cos_0_567_plus_0_321i_768", "0.567 + 0.321i", 768u, mc_cos, bench_scaled_iters(2));
    run_unary_case("tan_0_567_plus_0_321i_768", "0.567 + 0.321i", 768u, mc_tan, bench_scaled_iters(2));
    run_unary_case("atan_0_321_plus_0_123i_768", "0.321 + 0.123i", 768u, mc_atan, bench_scaled_iters(2));
    run_binary_case("atan2_0_5_0_25i__-0_75_0_1i_768", "0.5 + 0.25i", "-0.75 + 0.1i", 768u, mc_atan2, bench_scaled_iters(2));
    run_unary_case("asin_0_321_plus_0_123i_768", "0.321 + 0.123i", 768u, mc_asin, bench_scaled_iters(2));
    run_unary_case("acos_0_321_plus_0_123i_768", "0.321 + 0.123i", 768u, mc_acos, bench_scaled_iters(2));
    run_unary_case("sinh_0_567_plus_0_321i_768", "0.567 + 0.321i", 768u, mc_sinh, bench_scaled_iters(2));
    run_unary_case("cosh_0_567_plus_0_321i_768", "0.567 + 0.321i", 768u, mc_cosh, bench_scaled_iters(2));
    run_unary_case("tanh_0_567_plus_0_321i_768", "0.567 + 0.321i", 768u, mc_tanh, bench_scaled_iters(2));
    run_unary_case("asinh_0_321_plus_0_123i_768", "0.321 + 0.123i", 768u, mc_asinh, bench_scaled_iters(2));
    run_unary_case("acosh_2_plus_0_5i_768", "2 + 0.5i", 768u, mc_acosh, bench_scaled_iters(2));
    run_unary_case("atanh_0_321_plus_0_123i_768", "0.321 + 0.123i", 768u, mc_atanh, bench_scaled_iters(2));
    run_unary_case("lambert_w0_1_plus_1i_768", "1 + 1i", 768u, mc_lambert_w0, bench_scaled_iters(1));
    run_unary_case("lambert_wm1_-0_2_-0_1i_768", "-0.2 - 0.1i", 768u, mc_lambert_wm1, bench_scaled_iters(1));
    run_unary_case("productlog_1_plus_1i_768", "1 + 1i", 768u, mc_productlog, bench_scaled_iters(1));
    run_unary_case("gamma_1_5_plus_0_7i_768", "1.5 + 0.7i", 768u, mc_gamma, bench_scaled_iters(1));
    run_unary_case("lgamma_1_5_plus_0_7i_768", "1.5 + 0.7i", 768u, mc_lgamma, bench_scaled_iters(1));

    run_unary_case("exp_1_plus_1i_1024", "1 + 1i", 1024u, mc_exp, bench_scaled_iters(1));
    run_unary_case("log_1_plus_1i_1024", "1 + 1i", 1024u, mc_log, bench_scaled_iters(1));
    run_unary_case("sin_0_567_plus_0_321i_1024", "0.567 + 0.321i", 1024u, mc_sin, bench_scaled_iters(1));
    run_unary_case("cos_0_567_plus_0_321i_1024", "0.567 + 0.321i", 1024u, mc_cos, bench_scaled_iters(1));
    run_unary_case("tan_0_567_plus_0_321i_1024", "0.567 + 0.321i", 1024u, mc_tan, bench_scaled_iters(1));
    run_unary_case("atan_0_321_plus_0_123i_1024", "0.321 + 0.123i", 1024u, mc_atan, bench_scaled_iters(1));
    run_binary_case("atan2_0_5_0_25i__-0_75_0_1i_1024", "0.5 + 0.25i", "-0.75 + 0.1i", 1024u, mc_atan2, bench_scaled_iters(1));
    run_unary_case("asin_0_321_plus_0_123i_1024", "0.321 + 0.123i", 1024u, mc_asin, bench_scaled_iters(1));
    run_unary_case("acos_0_321_plus_0_123i_1024", "0.321 + 0.123i", 1024u, mc_acos, bench_scaled_iters(1));
    run_unary_case("sinh_0_567_plus_0_321i_1024", "0.567 + 0.321i", 1024u, mc_sinh, bench_scaled_iters(1));
    run_unary_case("cosh_0_567_plus_0_321i_1024", "0.567 + 0.321i", 1024u, mc_cosh, bench_scaled_iters(1));
    run_unary_case("tanh_0_567_plus_0_321i_1024", "0.567 + 0.321i", 1024u, mc_tanh, bench_scaled_iters(1));
    run_unary_case("asinh_0_321_plus_0_123i_1024", "0.321 + 0.123i", 1024u, mc_asinh, bench_scaled_iters(1));
    run_unary_case("acosh_2_plus_0_5i_1024", "2 + 0.5i", 1024u, mc_acosh, bench_scaled_iters(1));
    run_unary_case("atanh_0_321_plus_0_123i_1024", "0.321 + 0.123i", 1024u, mc_atanh, bench_scaled_iters(1));
    run_unary_case("lambert_w0_1_plus_1i_1024", "1 + 1i", 1024u, mc_lambert_w0, bench_scaled_iters(1));
    run_unary_case("lambert_wm1_-0_2_-0_1i_1024", "-0.2 - 0.1i", 1024u, mc_lambert_wm1, bench_scaled_iters(1));
    run_unary_case("productlog_1_plus_1i_1024", "1 + 1i", 1024u, mc_productlog, bench_scaled_iters(1));
    run_unary_case("gamma_1_5_plus_0_7i_1024", "1.5 + 0.7i", 1024u, mc_gamma, bench_scaled_iters(1));
    run_unary_case("lgamma_1_5_plus_0_7i_1024", "1.5 + 0.7i", 1024u, mc_lgamma, bench_scaled_iters(1));

    return 0;
}
