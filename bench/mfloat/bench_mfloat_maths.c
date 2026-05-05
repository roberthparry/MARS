#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static int bench_wants_section(const char *name)
{
    const char *section = getenv("MARS_BENCH_SECTION");

    /* Default to the full bench, but allow quicker section-by-section profiling. */
    if (!section || !*section || strcmp(section, "all") == 0)
        return 1;
    return strcmp(section, name) == 0;
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
                           size_t precision,
                           int (*fn)(mfloat_t *),
                           int iters)
{
    size_t old_prec;
    mfloat_t *src;
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
    printf("%-28s bits=%-4zu avg_µs=%10.3f avg_ms=%10.3f\n",
           label,
           precision,
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

    if (!bench_case_enabled(label))
        return;

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
    printf("%-28s bits=%-4zu avg_µs=%10.3f avg_ms=%10.3f\n",
           label,
           precision,
           avg_us,
           avg_us / 1000.0);

    mf_free(lhs_src);
    mf_free(rhs);
    (void)mf_set_default_precision(old_prec);
}

static void run_ternary_case(const char *label,
                             const char *x_text,
                             const char *a_text,
                             const char *b_text,
                             size_t precision,
                             int (*fn)(mfloat_t *, const mfloat_t *, const mfloat_t *),
                             int iters)
{
    size_t old_prec;
    mfloat_t *x_src;
    mfloat_t *a;
    mfloat_t *b;
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

    x_src = mf_create_string(x_text);
    a = mf_create_string(a_text);
    b = mf_create_string(b_text);
    if (!x_src || !a || !b) {
        fprintf(stderr, "%s source create failed\n", label);
        mf_free(x_src);
        mf_free(a);
        mf_free(b);
        (void)mf_set_default_precision(old_prec);
        return;
    }

    {
        mfloat_t *warm = mf_clone(x_src);

        if (!warm || fn(warm, a, b) != 0) {
            fprintf(stderr, "%s warmup failed\n", label);
            mf_free(warm);
            mf_free(x_src);
            mf_free(a);
            mf_free(b);
            (void)mf_set_default_precision(old_prec);
            return;
        }
        mf_free(warm);
    }

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mfloat_t *value = mf_clone(x_src);

        if (!value || fn(value, a, b) != 0) {
            fprintf(stderr, "%s timed run failed\n", label);
            mf_free(value);
            mf_free(x_src);
            mf_free(a);
            mf_free(b);
            (void)mf_set_default_precision(old_prec);
            return;
        }
        mf_free(value);
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s bits=%-4zu avg_µs=%10.3f avg_ms=%10.3f\n",
           label,
           precision,
           avg_us,
           avg_us / 1000.0);

    mf_free(x_src);
    mf_free(a);
    mf_free(b);
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

    if (!bench_case_enabled(label))
        return;

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
    printf("%-28s bits=%-4zu avg_µs=%10.3f avg_ms=%10.3f\n",
           label,
           precision,
           avg_us,
           avg_us / 1000.0);

    (void)mf_set_default_precision(old_prec);
}

int main(void)
{
    puts("== mfloat native math bench ==");
    puts("Scale iterations with MARS_BENCH_SCALE=<n> if you want longer runs.");
        puts("Limit to one section with MARS_BENCH_SECTION=constants|elem256|triage256|special256|selected512|selected768.");
    puts("Filter individual cases with MARS_BENCH_FILTER=<substring>.");

    if (bench_wants_section("constants")) {
        puts("");
        puts("-- constants --");
        run_const_case("pi_256", 256u, mf_pi, bench_scaled_iters(8));
        run_const_case("e_256", 256u, mf_e, bench_scaled_iters(8));
        run_const_case("gamma_256", 256u, mf_euler_mascheroni, bench_scaled_iters(6));
        run_const_case("pi_512", 512u, mf_pi, bench_scaled_iters(4));
        run_const_case("e_512", 512u, mf_e, bench_scaled_iters(4));
        run_const_case("gamma_512", 512u, mf_euler_mascheroni, bench_scaled_iters(3));
    }

    if (bench_wants_section("elem256")) {
        puts("");
        puts("-- elementary 256-bit --");
        run_unary_case("exp_256", "1.23456789", 256u, mf_exp, bench_scaled_iters(8));
        run_unary_case("log_256", "1.23456789", 256u, mf_log, bench_scaled_iters(8));
        run_unary_case("sqrt_256", "2.25", 256u, mf_sqrt, bench_scaled_iters(8));
        run_unary_case("sin_256", "0.7", 256u, mf_sin, bench_scaled_iters(8));
        run_unary_case("cos_256", "0.7", 256u, mf_cos, bench_scaled_iters(8));
        run_unary_case("tan_256", "0.7", 256u, mf_tan, bench_scaled_iters(6));
        run_unary_case("atan_256", "0.7", 256u, mf_atan, bench_scaled_iters(8));
        run_unary_case("asin_256", "0.5", 256u, mf_asin, bench_scaled_iters(4));
        run_unary_case("acos_256", "0.5", 256u, mf_acos, bench_scaled_iters(4));
        run_binary_case("atan2_256", "1", "-1", 256u, mf_atan2, bench_scaled_iters(4));
        run_unary_case("sinh_256", "0.7", 256u, mf_sinh, bench_scaled_iters(4));
        run_unary_case("cosh_256", "0.7", 256u, mf_cosh, bench_scaled_iters(4));
        run_unary_case("tanh_256", "0.7", 256u, mf_tanh, bench_scaled_iters(4));
        run_unary_case("asinh_256", "0.5", 256u, mf_asinh, bench_scaled_iters(3));
        run_unary_case("acosh_256", "2.0", 256u, mf_acosh, bench_scaled_iters(3));
        run_unary_case("atanh_256", "0.5", 256u, mf_atanh, bench_scaled_iters(3));
        run_binary_case("pow_256", "1.23456789", "3.5", 256u, mf_pow, bench_scaled_iters(4));
    }

    if (bench_wants_section("triage256")) {
        puts("");
        puts("-- triage 256-bit --");
        run_unary_case("sinh_256", "0.7", 256u, mf_sinh, bench_scaled_iters(1));
        run_unary_case("asinh_256", "0.5", 256u, mf_asinh, bench_scaled_iters(1));
        run_unary_case("acosh_256", "2.0", 256u, mf_acosh, bench_scaled_iters(1));
        run_unary_case("atanh_256", "0.5", 256u, mf_atanh, bench_scaled_iters(1));
    }

    if (bench_wants_section("special256")) {
        puts("");
        puts("-- special 256-bit --");
        run_unary_case("gamma_256", "2.5", 256u, mf_gamma, bench_scaled_iters(3));
        run_unary_case("lgamma_256", "2.5", 256u, mf_lgamma, bench_scaled_iters(3));
        run_unary_case("digamma_256", "2.5", 256u, mf_digamma, bench_scaled_iters(3));
        run_unary_case("trigamma_256", "2.5", 256u, mf_trigamma, bench_scaled_iters(3));
        run_unary_case("tetragamma_256", "2.5", 256u, mf_tetragamma, bench_scaled_iters(2));
        run_unary_case("erf_256", "0.5", 256u, mf_erf, bench_scaled_iters(4));
        run_unary_case("erfc_256", "0.5", 256u, mf_erfc, bench_scaled_iters(4));
        run_unary_case("erfinv_256", "0.5", 256u, mf_erfinv, bench_scaled_iters(2));
        run_unary_case("erfcinv_256", "0.5", 256u, mf_erfcinv, bench_scaled_iters(2));
        run_unary_case("gammainv_256", "3", 256u, mf_gammainv, bench_scaled_iters(2));
        run_unary_case("lambert_w0_256", "1", 256u, mf_lambert_w0, bench_scaled_iters(3));
        run_unary_case("lambert_wm1_256", "-0.1", 256u, mf_lambert_wm1, bench_scaled_iters(2));
        run_binary_case("beta_256", "2.5", "3.5", 256u, mf_beta, bench_scaled_iters(3));
        run_binary_case("logbeta_256", "2.5", "3.5", 256u, mf_logbeta, bench_scaled_iters(2));
        run_binary_case("binomial_256", "5.5", "2.5", 256u, mf_binomial, bench_scaled_iters(2));
        run_ternary_case("beta_pdf_256", "0.5", "2.5", "3.5", 256u, mf_beta_pdf, bench_scaled_iters(2));
        run_ternary_case("logbeta_pdf_256", "0.5", "2.5", "3.5", 256u, mf_logbeta_pdf, bench_scaled_iters(2));
        run_unary_case("normal_pdf_256", "0.5", 256u, mf_normal_pdf, bench_scaled_iters(3));
        run_unary_case("normal_cdf_256", "0.5", 256u, mf_normal_cdf, bench_scaled_iters(3));
        run_unary_case("normal_logpdf_256", "0.5", 256u, mf_normal_logpdf, bench_scaled_iters(3));
        run_binary_case("gammainc_P_256", "1", "1", 256u, mf_gammainc_P, bench_scaled_iters(2));
        run_binary_case("gammainc_Q_256", "1", "1", 256u, mf_gammainc_Q, bench_scaled_iters(2));
        run_binary_case("gammainc_lo_256", "1", "1", 256u, mf_gammainc_lower, bench_scaled_iters(2));
        run_binary_case("gammainc_hi_256", "1", "1", 256u, mf_gammainc_upper, bench_scaled_iters(2));
        run_unary_case("ei_256", "1", 256u, mf_ei, bench_scaled_iters(2));
        run_unary_case("e1_256", "1", 256u, mf_e1, bench_scaled_iters(2));
    }

    if (bench_wants_section("selected512")) {
        puts("");
        puts("-- selected 512-bit --");
        run_unary_case("exp_512", "1.23456789", 512u, mf_exp, bench_scaled_iters(2));
        run_unary_case("log_512", "1.23456789", 512u, mf_log, bench_scaled_iters(2));
        run_unary_case("sqrt_512", "2.25", 512u, mf_sqrt, bench_scaled_iters(2));
        run_unary_case("sin_512", "0.7", 512u, mf_sin, bench_scaled_iters(2));
        run_unary_case("cos_512", "0.7", 512u, mf_cos, bench_scaled_iters(2));
        run_unary_case("tan_512", "0.7", 512u, mf_tan, bench_scaled_iters(1));
        run_unary_case("atan_512", "0.7", 512u, mf_atan, bench_scaled_iters(2));
        run_unary_case("asin_512", "0.5", 512u, mf_asin, bench_scaled_iters(1));
        run_unary_case("acos_512", "0.5", 512u, mf_acos, bench_scaled_iters(1));
        run_unary_case("asinh_512", "0.5", 512u, mf_asinh, bench_scaled_iters(1));
        run_unary_case("acosh_512", "2.0", 512u, mf_acosh, bench_scaled_iters(1));
        run_unary_case("atanh_512", "0.5", 512u, mf_atanh, bench_scaled_iters(1));
        run_unary_case("erf_512", "0.5", 512u, mf_erf, bench_scaled_iters(2));
        run_unary_case("lambert_w0_512", "1", 512u, mf_lambert_w0, bench_scaled_iters(2));
        run_unary_case("lambert_wm1_512", "-0.1", 512u, mf_lambert_wm1, bench_scaled_iters(1));
        run_binary_case("pow_512", "1.23456789", "3.5", 512u, mf_pow, bench_scaled_iters(1));
        run_binary_case("logbeta_512", "2.5", "3.5", 512u, mf_logbeta, bench_scaled_iters(1));
        run_ternary_case("beta_pdf_512", "0.5", "2.5", "3.5", 512u, mf_beta_pdf, bench_scaled_iters(1));
        run_unary_case("normal_pdf_512", "0.5", 512u, mf_normal_pdf, bench_scaled_iters(1));
        run_unary_case("ei_512", "1", 512u, mf_ei, bench_scaled_iters(1));
        run_unary_case("e1_512", "1", 512u, mf_e1, bench_scaled_iters(1));
    }

    if (bench_wants_section("selected768")) {
        puts("");
        puts("-- selected 768-bit --");
        run_unary_case("exp_768", "1.23456789", 768u, mf_exp, bench_scaled_iters(1));
        run_unary_case("log_768", "1.23456789", 768u, mf_log, bench_scaled_iters(1));
        run_unary_case("sqrt_768", "2.25", 768u, mf_sqrt, bench_scaled_iters(1));
        run_unary_case("sin_768", "0.7", 768u, mf_sin, bench_scaled_iters(1));
        run_unary_case("cos_768", "0.7", 768u, mf_cos, bench_scaled_iters(1));
        run_unary_case("tan_768", "0.7", 768u, mf_tan, bench_scaled_iters(1));
        run_unary_case("atan_768", "0.7", 768u, mf_atan, bench_scaled_iters(1));
        run_unary_case("asin_768", "0.5", 768u, mf_asin, bench_scaled_iters(1));
        run_unary_case("acos_768", "0.5", 768u, mf_acos, bench_scaled_iters(1));
        run_unary_case("asinh_768", "0.5", 768u, mf_asinh, bench_scaled_iters(1));
        run_unary_case("acosh_768", "2.0", 768u, mf_acosh, bench_scaled_iters(1));
        run_unary_case("atanh_768", "0.5", 768u, mf_atanh, bench_scaled_iters(1));
        run_unary_case("erf_768", "0.5", 768u, mf_erf, bench_scaled_iters(1));
        run_unary_case("lambert_w0_768", "1", 768u, mf_lambert_w0, bench_scaled_iters(1));
        run_unary_case("lambert_wm1_768", "-0.1", 768u, mf_lambert_wm1, bench_scaled_iters(1));
    }

    return EXIT_SUCCESS;
}
