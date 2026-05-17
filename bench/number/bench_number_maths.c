#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "number.h"

typedef number_t (*number_unary_fn)(number_t);
typedef number_t (*number_binary_fn)(number_t, number_t);
typedef number_t (*number_ternary_fn)(number_t, number_t, number_t);
typedef number_t (*number_const_fn)(void);

static number_t bench_num_pi(void) { return num_clone(NUM_PI); }
static number_t bench_num_e(void) { return num_clone(NUM_E); }
static number_t bench_num_euler_mascheroni(void) { return num_clone(NUM_EULER_MASCHERONI); }

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
                           number_unary_fn fn,
                           int iters)
{
    size_t old_prec;
    uint64_t start;
    uint64_t end;
    double avg_us;
    number_t src;
    number_t value;

    if (!bench_case_enabled(label))
        return;

    old_prec = num_get_default_prec_bits();
    if (num_set_default_prec_bits(precision) != 0) {
        fprintf(stderr, "%s set default precision failed\n", label);
        return;
    }

    src = num_create_from_string(text);
    value = fn(src);
    num_destroy(&value);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        value = fn(src);
        num_destroy(&value);
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s bits=%-4zu avg_µs=%10.3f\n",
           label,
           precision,
           avg_us);

    num_destroy(&src);
    (void)num_set_default_prec_bits(old_prec);
}

static void run_binary_case(const char *label,
                            const char *lhs_text,
                            const char *rhs_text,
                            size_t precision,
                            number_binary_fn fn,
                            int iters)
{
    size_t old_prec;
    uint64_t start;
    uint64_t end;
    double avg_us;
    number_t lhs;
    number_t rhs;
    number_t value;

    if (!bench_case_enabled(label))
        return;

    old_prec = num_get_default_prec_bits();
    if (num_set_default_prec_bits(precision) != 0) {
        fprintf(stderr, "%s set default precision failed\n", label);
        return;
    }

    lhs = num_create_from_string(lhs_text);
    rhs = num_create_from_string(rhs_text);
    value = fn(lhs, rhs);
    num_destroy(&value);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        value = fn(lhs, rhs);
        num_destroy(&value);
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s bits=%-4zu avg_µs=%10.3f\n",
           label,
           precision,
           avg_us);

    num_destroy(&lhs);
    num_destroy(&rhs);
    (void)num_set_default_prec_bits(old_prec);
}

static void run_sincos_case(const char *label, const char *text, size_t precision, int iters)
{
    size_t old_prec;
    uint64_t start;
    uint64_t end;
    double avg_us;
    number_t src;
    number_t sin_value = num_new();
    number_t cos_value = num_new();

    if (!bench_case_enabled(label))
        return;

    old_prec = num_get_default_prec_bits();
    if (num_set_default_prec_bits(precision) != 0) {
        fprintf(stderr, "%s set default precision failed\n", label);
        return;
    }

    src = num_create_from_string(text);
    if (num_sincos(src, &sin_value, &cos_value) != 0) {
        fprintf(stderr, "%s warmup failed\n", label);
        num_destroy(&src);
        (void)num_set_default_prec_bits(old_prec);
        return;
    }
    num_destroy(&sin_value);
    num_destroy(&cos_value);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        if (num_sincos(src, &sin_value, &cos_value) != 0) {
            fprintf(stderr, "%s timed run failed\n", label);
            num_destroy(&src);
            (void)num_set_default_prec_bits(old_prec);
            return;
        }
        num_destroy(&sin_value);
        num_destroy(&cos_value);
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s bits=%-4zu avg_µs=%10.3f\n",
           label,
           precision,
           avg_us);

    num_destroy(&src);
    (void)num_set_default_prec_bits(old_prec);
}

static void run_sinhcosh_case(const char *label, const char *text, size_t precision, int iters)
{
    size_t old_prec;
    uint64_t start;
    uint64_t end;
    double avg_us;
    number_t src;
    number_t sinh_value = num_new();
    number_t cosh_value = num_new();

    if (!bench_case_enabled(label))
        return;

    old_prec = num_get_default_prec_bits();
    if (num_set_default_prec_bits(precision) != 0) {
        fprintf(stderr, "%s set default precision failed\n", label);
        return;
    }

    src = num_create_from_string(text);
    if (num_sinhcosh(src, &sinh_value, &cosh_value) != 0) {
        fprintf(stderr, "%s warmup failed\n", label);
        num_destroy(&src);
        (void)num_set_default_prec_bits(old_prec);
        return;
    }
    num_destroy(&sinh_value);
    num_destroy(&cosh_value);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        if (num_sinhcosh(src, &sinh_value, &cosh_value) != 0) {
            fprintf(stderr, "%s timed run failed\n", label);
            num_destroy(&src);
            (void)num_set_default_prec_bits(old_prec);
            return;
        }
        num_destroy(&sinh_value);
        num_destroy(&cosh_value);
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s bits=%-4zu avg_µs=%10.3f\n",
           label,
           precision,
           avg_us);

    num_destroy(&src);
    (void)num_set_default_prec_bits(old_prec);
}

static void run_ternary_case(const char *label,
                             const char *x_text,
                             const char *a_text,
                             const char *b_text,
                             size_t precision,
                             number_ternary_fn fn,
                             int iters)
{
    size_t old_prec;
    uint64_t start;
    uint64_t end;
    double avg_us;
    number_t x;
    number_t a;
    number_t b;
    number_t value;

    if (!bench_case_enabled(label))
        return;

    old_prec = num_get_default_prec_bits();
    if (num_set_default_prec_bits(precision) != 0) {
        fprintf(stderr, "%s set default precision failed\n", label);
        return;
    }

    x = num_create_from_string(x_text);
    a = num_create_from_string(a_text);
    b = num_create_from_string(b_text);
    value = fn(x, a, b);
    num_destroy(&value);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        value = fn(x, a, b);
        num_destroy(&value);
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s bits=%-4zu avg_µs=%10.3f\n",
           label,
           precision,
           avg_us);

    num_destroy(&x);
    num_destroy(&a);
    num_destroy(&b);
    (void)num_set_default_prec_bits(old_prec);
}

static void run_const_case(const char *label,
                           size_t precision,
                           number_const_fn fn,
                           int iters)
{
    size_t old_prec;
    uint64_t start;
    uint64_t end;
    double avg_us;
    number_t value;

    if (!bench_case_enabled(label))
        return;

    old_prec = num_get_default_prec_bits();
    if (num_set_default_prec_bits(precision) != 0) {
        fprintf(stderr, "%s set default precision failed\n", label);
        return;
    }

    value = fn();
    num_destroy(&value);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        value = fn();
        num_destroy(&value);
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s bits=%-4zu avg_µs=%10.3f\n",
           label,
           precision,
           avg_us);

    (void)num_set_default_prec_bits(old_prec);
}

int main(void)
{
    puts("== number maths bench ==");
    puts("Scale iterations with MARS_BENCH_SCALE=<n> if you want longer runs.");
    puts("Limit to one section with MARS_BENCH_SECTION=constants|exp|log|elem256|triage256|special256|selected512|selected768|selected1024.");
    puts("Filter individual cases with MARS_BENCH_FILTER=<substring>.");

    if (bench_wants_section("constants")) {
        puts("");
        puts("-- constants --");
        run_const_case("pi_256", 256u, bench_num_pi, bench_scaled_iters(8));
        run_const_case("e_256", 256u, bench_num_e, bench_scaled_iters(8));
        run_const_case("gamma_256", 256u, bench_num_euler_mascheroni, bench_scaled_iters(6));
        run_const_case("pi_512", 512u, bench_num_pi, bench_scaled_iters(4));
        run_const_case("e_512", 512u, bench_num_e, bench_scaled_iters(4));
        run_const_case("gamma_512", 512u, bench_num_euler_mascheroni, bench_scaled_iters(3));
    }

    if (bench_wants_section("exp")) {
        puts("");
        puts("-- exp --");
        run_unary_case("exp_256", "1.23456789", 256u, num_exp, bench_scaled_iters(12));
        run_unary_case("exp_512", "1.23456789", 512u, num_exp, bench_scaled_iters(6));
        run_unary_case("exp_768", "1.23456789", 768u, num_exp, bench_scaled_iters(4));
        run_unary_case("exp_1024", "1.23456789", 1024u, num_exp, bench_scaled_iters(4));
    }

    if (bench_wants_section("log")) {
        puts("");
        puts("-- log --");
        run_unary_case("log_256", "2.345678", 256u, num_log, bench_scaled_iters(12));
        run_unary_case("log_512", "2.345678", 512u, num_log, bench_scaled_iters(6));
        run_unary_case("log_768", "2.345678", 768u, num_log, bench_scaled_iters(4));
        run_unary_case("log_1024", "2.345678", 1024u, num_log, bench_scaled_iters(4));
    }

    if (bench_wants_section("elem256")) {
        puts("");
        puts("-- elementary 256-bit --");
        run_unary_case("exp_256", "1.23456789", 256u, num_exp, bench_scaled_iters(8));
        run_unary_case("log_256", "2.345678", 256u, num_log, bench_scaled_iters(8));
        run_unary_case("sqrt_256", "1.23456789", 256u, num_sqrt, bench_scaled_iters(8));
        run_unary_case("sin_256", "0.567", 256u, num_sin, bench_scaled_iters(8));
        run_unary_case("cos_256", "0.7", 256u, num_cos, bench_scaled_iters(8));
        run_sincos_case("sincos_256", "0.7", 256u, bench_scaled_iters(6));
        run_unary_case("tan_256", "0.7", 256u, num_tan, bench_scaled_iters(6));
        run_unary_case("atan_256", "0.7", 256u, num_atan, bench_scaled_iters(8));
        run_unary_case("asin_256", "0.5", 256u, num_asin, bench_scaled_iters(4));
        run_unary_case("asin_general_256", "0.7", 256u, num_asin, bench_scaled_iters(2));
        run_unary_case("acos_256", "0.5", 256u, num_acos, bench_scaled_iters(4));
        run_unary_case("acos_general_256", "0.7", 256u, num_acos, bench_scaled_iters(2));
        run_binary_case("atan2_256", "1", "-1", 256u, num_atan2, bench_scaled_iters(4));
        run_binary_case("atan2_general_256", "0.5", "-0.75", 256u, num_atan2, bench_scaled_iters(2));
        run_unary_case("sinh_256", "0.7", 256u, num_sinh, bench_scaled_iters(4));
        run_unary_case("cosh_256", "0.7", 256u, num_cosh, bench_scaled_iters(4));
        run_sinhcosh_case("sinhcosh_256", "0.7", 256u, bench_scaled_iters(3));
        run_unary_case("tanh_256", "0.7", 256u, num_tanh, bench_scaled_iters(4));
        run_unary_case("asinh_256", "0.5", 256u, num_asinh, bench_scaled_iters(3));
        run_unary_case("acosh_256", "2.0", 256u, num_acosh, bench_scaled_iters(3));
        run_unary_case("atanh_256", "0.5", 256u, num_atanh, bench_scaled_iters(3));
        run_binary_case("pow_256", "1.23456789", "3.5", 256u, num_pow, bench_scaled_iters(4));
    }

    if (bench_wants_section("triage256")) {
        puts("");
        puts("-- triage 256-bit --");
        run_unary_case("sinh_256", "0.7", 256u, num_sinh, bench_scaled_iters(1));
        run_unary_case("asinh_256", "0.5", 256u, num_asinh, bench_scaled_iters(1));
        run_unary_case("acosh_256", "2.0", 256u, num_acosh, bench_scaled_iters(1));
        run_unary_case("atanh_256", "0.5", 256u, num_atanh, bench_scaled_iters(1));
    }

    if (bench_wants_section("special256")) {
        puts("");
        puts("-- special 256-bit --");
        run_unary_case("gamma_256", "2.345", 256u, num_gamma, bench_scaled_iters(3));
        run_unary_case("lgamma_256", "2.345", 256u, num_lgamma, bench_scaled_iters(3));
        run_unary_case("digamma_256", "2.345", 256u, num_digamma, bench_scaled_iters(3));
        run_unary_case("trigamma_256", "2.345", 256u, num_trigamma, bench_scaled_iters(3));
        run_unary_case("tetragamma_256", "2.345", 256u, num_tetragamma, bench_scaled_iters(2));
        run_unary_case("erf_256", "0.567", 256u, num_erf, bench_scaled_iters(4));
        run_unary_case("erfc_256", "0.5", 256u, num_erfc, bench_scaled_iters(4));
        run_unary_case("erfinv_256", "0.5", 256u, num_erfinv, bench_scaled_iters(2));
        run_unary_case("erfcinv_256", "0.5", 256u, num_erfcinv, bench_scaled_iters(2));
        run_unary_case("gammainv_256", "3", 256u, num_gammainv, bench_scaled_iters(2));
        run_unary_case("lambert_w0_256", "0.7", 256u, num_lambert_w0, bench_scaled_iters(3));
        run_unary_case("lambert_wm1_256", "-0.2", 256u, num_lambert_wm1, bench_scaled_iters(2));
        run_binary_case("beta_256", "2.5", "3.5", 256u, num_beta, bench_scaled_iters(3));
        run_binary_case("logbeta_256", "2.5", "3.5", 256u, num_logbeta, bench_scaled_iters(2));
        run_binary_case("binomial_256", "5.5", "2.5", 256u, num_binomial, bench_scaled_iters(2));
        run_ternary_case("beta_pdf_256", "0.5", "2.5", "3.5", 256u, num_beta_pdf, bench_scaled_iters(2));
        run_ternary_case("logbeta_pdf_256", "0.5", "2.5", "3.5", 256u, num_logbeta_pdf, bench_scaled_iters(2));
        run_unary_case("normal_pdf_256", "0.5", 256u, num_normal_pdf, bench_scaled_iters(3));
        run_unary_case("normal_cdf_256", "0.5", 256u, num_normal_cdf, bench_scaled_iters(3));
        run_unary_case("normal_logpdf_256", "0.5", 256u, num_normal_logpdf, bench_scaled_iters(3));
        run_binary_case("gammainc_P_256", "1", "1", 256u, num_gammainc_P, bench_scaled_iters(2));
        run_binary_case("gammainc_Q_256", "1", "1", 256u, num_gammainc_Q, bench_scaled_iters(2));
        run_binary_case("gammainc_lo_256", "1", "1", 256u, num_gammainc_lower, bench_scaled_iters(2));
        run_binary_case("gammainc_hi_256", "1", "1", 256u, num_gammainc_upper, bench_scaled_iters(2));
        run_unary_case("ei_5_256", "5", 256u, num_ei, bench_scaled_iters(2));
        run_unary_case("e1_5_256", "5", 256u, num_e1, bench_scaled_iters(2));
    }

    if (bench_wants_section("selected512")) {
        puts("");
        puts("-- selected 512-bit --");
        run_unary_case("exp_512", "1.23456789", 512u, num_exp, bench_scaled_iters(2));
        run_unary_case("log_512", "2.345678", 512u, num_log, bench_scaled_iters(2));
        run_unary_case("sqrt_512", "1.23456789", 512u, num_sqrt, bench_scaled_iters(2));
        run_unary_case("sin_512", "0.567", 512u, num_sin, bench_scaled_iters(2));
        run_unary_case("cos_512", "0.7", 512u, num_cos, bench_scaled_iters(2));
        run_sincos_case("sincos_512", "0.7", 512u, bench_scaled_iters(1));
        run_unary_case("tan_512", "0.7", 512u, num_tan, bench_scaled_iters(1));
        run_unary_case("atan_512", "0.7", 512u, num_atan, bench_scaled_iters(2));
        run_unary_case("asin_512", "0.5", 512u, num_asin, bench_scaled_iters(1));
        run_unary_case("asin_general_512", "0.7", 512u, num_asin, bench_scaled_iters(1));
        run_unary_case("acos_512", "0.5", 512u, num_acos, bench_scaled_iters(1));
        run_unary_case("acos_general_512", "0.7", 512u, num_acos, bench_scaled_iters(1));
        run_binary_case("atan2_512", "1", "-1", 512u, num_atan2, bench_scaled_iters(2));
        run_binary_case("atan2_general_512", "0.5", "-0.75", 512u, num_atan2, bench_scaled_iters(1));
        run_unary_case("sinh_512", "0.7", 512u, num_sinh, bench_scaled_iters(1));
        run_unary_case("cosh_512", "0.7", 512u, num_cosh, bench_scaled_iters(1));
        run_sinhcosh_case("sinhcosh_512", "0.7", 512u, bench_scaled_iters(1));
        run_unary_case("tanh_512", "0.7", 512u, num_tanh, bench_scaled_iters(1));
        run_unary_case("asinh_512", "0.5", 512u, num_asinh, bench_scaled_iters(1));
        run_unary_case("acosh_512", "2.0", 512u, num_acosh, bench_scaled_iters(1));
        run_unary_case("atanh_512", "0.5", 512u, num_atanh, bench_scaled_iters(1));
        run_unary_case("erf_512", "0.567", 512u, num_erf, bench_scaled_iters(2));
        run_unary_case("gamma_512", "2.345", 512u, num_gamma, bench_scaled_iters(1));
        run_unary_case("lgamma_512", "2.345", 512u, num_lgamma, bench_scaled_iters(1));
        run_unary_case("digamma_512", "2.345", 512u, num_digamma, bench_scaled_iters(1));
        run_unary_case("trigamma_512", "2.345", 512u, num_trigamma, bench_scaled_iters(1));
        run_unary_case("tetragamma_512", "2.345", 512u, num_tetragamma, bench_scaled_iters(1));
        run_unary_case("lambert_w0_512", "0.7", 512u, num_lambert_w0, bench_scaled_iters(2));
        run_unary_case("lambert_wm1_512", "-0.2", 512u, num_lambert_wm1, bench_scaled_iters(1));
        run_binary_case("pow_512", "1.23456789", "3.5", 512u, num_pow, bench_scaled_iters(1));
        run_binary_case("logbeta_512", "2.5", "3.5", 512u, num_logbeta, bench_scaled_iters(1));
        run_ternary_case("beta_pdf_512", "0.5", "2.5", "3.5", 512u, num_beta_pdf, bench_scaled_iters(1));
        run_unary_case("normal_pdf_512", "0.5", 512u, num_normal_pdf, bench_scaled_iters(1));
        run_unary_case("ei_5_512", "5", 512u, num_ei, bench_scaled_iters(1));
        run_unary_case("e1_5_512", "5", 512u, num_e1, bench_scaled_iters(1));
    }

    if (bench_wants_section("selected768")) {
        puts("");
        puts("-- selected 768-bit --");
        run_unary_case("exp_768", "1.23456789", 768u, num_exp, bench_scaled_iters(1));
        run_unary_case("log_768", "2.345678", 768u, num_log, bench_scaled_iters(1));
        run_unary_case("sqrt_768", "1.23456789", 768u, num_sqrt, bench_scaled_iters(1));
        run_unary_case("sin_768", "0.567", 768u, num_sin, bench_scaled_iters(1));
        run_unary_case("cos_768", "0.7", 768u, num_cos, bench_scaled_iters(1));
        run_sincos_case("sincos_768", "0.7", 768u, bench_scaled_iters(1));
        run_unary_case("tan_768", "0.7", 768u, num_tan, bench_scaled_iters(1));
        run_unary_case("atan_768", "0.7", 768u, num_atan, bench_scaled_iters(1));
        run_unary_case("asin_768", "0.5", 768u, num_asin, bench_scaled_iters(1));
        run_unary_case("asin_general_768", "0.7", 768u, num_asin, bench_scaled_iters(1));
        run_unary_case("acos_768", "0.5", 768u, num_acos, bench_scaled_iters(1));
        run_unary_case("acos_general_768", "0.7", 768u, num_acos, bench_scaled_iters(1));
        run_binary_case("atan2_768", "1", "-1", 768u, num_atan2, bench_scaled_iters(1));
        run_binary_case("atan2_general_768", "0.5", "-0.75", 768u, num_atan2, bench_scaled_iters(1));
        run_unary_case("sinh_768", "0.7", 768u, num_sinh, bench_scaled_iters(1));
        run_unary_case("cosh_768", "0.7", 768u, num_cosh, bench_scaled_iters(1));
        run_sinhcosh_case("sinhcosh_768", "0.7", 768u, bench_scaled_iters(1));
        run_unary_case("tanh_768", "0.7", 768u, num_tanh, bench_scaled_iters(1));
        run_unary_case("asinh_768", "0.5", 768u, num_asinh, bench_scaled_iters(1));
        run_unary_case("acosh_768", "2.0", 768u, num_acosh, bench_scaled_iters(1));
        run_unary_case("atanh_768", "0.5", 768u, num_atanh, bench_scaled_iters(1));
        run_unary_case("erf_768", "0.567", 768u, num_erf, bench_scaled_iters(1));
        run_unary_case("gamma_768", "2.345", 768u, num_gamma, bench_scaled_iters(1));
        run_unary_case("lgamma_768", "2.345", 768u, num_lgamma, bench_scaled_iters(1));
        run_unary_case("digamma_768", "2.345", 768u, num_digamma, bench_scaled_iters(1));
        run_unary_case("trigamma_768", "2.345", 768u, num_trigamma, bench_scaled_iters(1));
        run_unary_case("tetragamma_768", "2.345", 768u, num_tetragamma, bench_scaled_iters(1));
        run_unary_case("ei_5_768", "5", 768u, num_ei, bench_scaled_iters(1));
        run_unary_case("e1_5_768", "5", 768u, num_e1, bench_scaled_iters(1));
        run_unary_case("lambert_w0_768", "0.7", 768u, num_lambert_w0, bench_scaled_iters(1));
        run_unary_case("lambert_wm1_768", "-0.2", 768u, num_lambert_wm1, bench_scaled_iters(1));
    }

    if (bench_wants_section("selected1024")) {
        puts("");
        puts("-- selected 1024-bit --");
        run_unary_case("exp_1024", "1.23456789", 1024u, num_exp, bench_scaled_iters(1));
        run_unary_case("log_1024", "2.345678", 1024u, num_log, bench_scaled_iters(1));
        run_unary_case("sqrt_1024", "1.23456789", 1024u, num_sqrt, bench_scaled_iters(1));
        run_unary_case("sin_1024", "0.567", 1024u, num_sin, bench_scaled_iters(1));
        run_unary_case("cos_1024", "0.7", 1024u, num_cos, bench_scaled_iters(1));
        run_sincos_case("sincos_1024", "0.7", 1024u, bench_scaled_iters(1));
        run_unary_case("tan_1024", "0.7", 1024u, num_tan, bench_scaled_iters(1));
        run_unary_case("atan_1024", "0.7", 1024u, num_atan, bench_scaled_iters(1));
        run_unary_case("asin_1024", "0.5", 1024u, num_asin, bench_scaled_iters(1));
        run_unary_case("asin_general_1024", "0.7", 1024u, num_asin, bench_scaled_iters(1));
        run_unary_case("acos_1024", "0.5", 1024u, num_acos, bench_scaled_iters(1));
        run_unary_case("acos_general_1024", "0.7", 1024u, num_acos, bench_scaled_iters(1));
        run_binary_case("atan2_1024", "1", "-1", 1024u, num_atan2, bench_scaled_iters(1));
        run_binary_case("atan2_general_1024", "0.5", "-0.75", 1024u, num_atan2, bench_scaled_iters(1));
        run_unary_case("sinh_1024", "0.7", 1024u, num_sinh, bench_scaled_iters(1));
        run_unary_case("cosh_1024", "0.7", 1024u, num_cosh, bench_scaled_iters(1));
        run_sinhcosh_case("sinhcosh_1024", "0.7", 1024u, bench_scaled_iters(1));
        run_unary_case("tanh_1024", "0.7", 1024u, num_tanh, bench_scaled_iters(1));
        run_unary_case("asinh_1024", "0.5", 1024u, num_asinh, bench_scaled_iters(1));
        run_unary_case("acosh_1024", "2.0", 1024u, num_acosh, bench_scaled_iters(1));
        run_unary_case("atanh_1024", "0.5", 1024u, num_atanh, bench_scaled_iters(1));
        run_unary_case("erf_1024", "0.567", 1024u, num_erf, bench_scaled_iters(1));
        run_unary_case("gamma_1024", "2.345", 1024u, num_gamma, bench_scaled_iters(1));
        run_unary_case("lgamma_1024", "2.345", 1024u, num_lgamma, bench_scaled_iters(1));
        run_unary_case("digamma_1024", "2.345", 1024u, num_digamma, bench_scaled_iters(1));
        run_unary_case("trigamma_1024", "2.345", 1024u, num_trigamma, bench_scaled_iters(1));
        run_unary_case("tetragamma_1024", "2.345", 1024u, num_tetragamma, bench_scaled_iters(1));
        run_unary_case("lambert_w0_1024", "0.7", 1024u, num_lambert_w0, bench_scaled_iters(1));
        run_unary_case("lambert_wm1_1024", "-0.2", 1024u, num_lambert_wm1, bench_scaled_iters(1));
        run_binary_case("pow_1024", "1.23456789", "3.5", 1024u, num_pow, bench_scaled_iters(1));
        run_binary_case("logbeta_1024", "2.5", "3.5", 1024u, num_logbeta, bench_scaled_iters(1));
        run_ternary_case("beta_pdf_1024", "0.5", "2.5", "3.5", 1024u, num_beta_pdf, bench_scaled_iters(1));
        run_unary_case("normal_pdf_1024", "0.5", 1024u, num_normal_pdf, bench_scaled_iters(1));
        run_unary_case("ei_5_1024", "5", 1024u, num_ei, bench_scaled_iters(1));
        run_unary_case("e1_5_1024", "5", 1024u, num_e1, bench_scaled_iters(1));
    }

    return 0;
}
