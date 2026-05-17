#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "mcomplex.h"

#define BENCH_MAX_RESULTS 512u
#define BENCH_ROW_COUNT 26u

typedef struct bench_result_t {
    const char *label;
    size_t precision;
    double avg_us;
} bench_result_t;

typedef struct bench_md_row_t {
    const char *base_label;
    const char *display;
} bench_md_row_t;

typedef struct bench_stats_t {
    double estimate;
    double mad;
    double ci_low;
    double ci_high;
    int samples;
} bench_stats_t;

static bench_result_t bench_results[BENCH_MAX_RESULTS];
static size_t bench_result_count;

static const bench_md_row_t bench_md_rows[BENCH_ROW_COUNT] = {
    {"exp_0_567_plus_0_321i", "mc_exp(0.567 + 0.321i)"},
    {"log_0_567_plus_0_321i", "mc_log(0.567 + 0.321i)"},
    {"sqrt_0_567_plus_0_321i", "mc_sqrt(0.567 + 0.321i)"},
    {"sin_0_567_plus_0_321i", "mc_sin(0.567 + 0.321i)"},
    {"cos_0_567_plus_0_321i", "mc_cos(0.567 + 0.321i)"},
    {"tan_0_567_plus_0_321i", "mc_tan(0.567 + 0.321i)"},
    {"atan_0_321_plus_0_123i", "mc_atan(0.321 + 0.123i)"},
    {"asin_0_321_plus_0_123i", "mc_asin(0.321 + 0.123i)"},
    {"acos_0_321_plus_0_123i", "mc_acos(0.321 + 0.123i)"},
    {"atan2_0_5_0_25i__-0_75_0_1i", "mc_atan2(0.5 + 0.25i, -0.75 + 0.1i)"},
    {"sinh_0_567_plus_0_321i", "mc_sinh(0.567 + 0.321i)"},
    {"cosh_0_567_plus_0_321i", "mc_cosh(0.567 + 0.321i)"},
    {"tanh_0_567_plus_0_321i", "mc_tanh(0.567 + 0.321i)"},
    {"asinh_0_321_plus_0_123i", "mc_asinh(0.321 + 0.123i)"},
    {"acosh_2_plus_0_5i", "mc_acosh(2 + 0.5i)"},
    {"atanh_0_321_plus_0_123i", "mc_atanh(0.321 + 0.123i)"},
    {"lambert_w0_1_plus_1i", "mc_lambert_w0(1 + 1i)"},
    {"lambert_wm1_-0_2_-0_1i", "mc_lambert_wm1(-0.2 - 0.1i)"},
    {"productlog_1_plus_1i", "mc_productlog(1 + 1i)"},
    {"gamma_1_5_plus_0_7i", "mc_gamma(1.5 + 0.7i)"},
    {"lgamma_1_5_plus_0_7i", "mc_lgamma(1.5 + 0.7i)"},
    {"digamma_2_plus_1i", "mc_digamma(2 + 1i)"},
    {"trigamma_2_plus_0_5i", "mc_trigamma(2 + 0.5i)"},
    {"tetragamma_2_plus_0_5i", "mc_tetragamma(2 + 0.5i)"},
    {"ei_1_plus_1i", "mc_ei(1 + 1i)"},
    {"e1_1_plus_1i", "mc_e1(1 + 1i)"},
};

static int bench_markdown_enabled(void);
static int bench_doc_iters(int base_iters);

static int bench_markdown_enabled(void)
{
    const char *format = getenv("MARS_BENCH_FORMAT");

    return format && strcmp(format, "md") == 0;
}

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
    return bench_doc_iters((int)(base_iters * scale));
}

static int bench_repeat_count(void)
{
    const char *repeat_text = getenv("MARS_BENCH_REPEATS");
    long repeats = bench_markdown_enabled() ? 51 : 31;

    if (repeat_text && *repeat_text) {
        char *end = NULL;
        long parsed = strtol(repeat_text, &end, 10);

        if (end && *end == '\0' && parsed > 0)
            repeats = parsed;
    }
    return (int)repeats;
}

static int bench_bootstrap_count(void)
{
    const char *count_text = getenv("MARS_BENCH_BOOTSTRAP");
    long count = 1001;

    if (count_text && *count_text) {
        char *end = NULL;
        long parsed = strtol(count_text, &end, 10);

        if (end && *end == '\0' && parsed > 0)
            count = parsed;
    }
    return (int)count;
}

static int bench_doc_iters(int base_iters)
{
    if (bench_markdown_enabled() && base_iters < 3)
        return 3;
    return base_iters;
}

static int bench_compare_double(const void *lhs, const void *rhs)
{
    const double a = *(const double *)lhs;
    const double b = *(const double *)rhs;

    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}

static uint64_t bench_bootstrap_next(uint64_t *state)
{
    *state = *state * 6364136223846793005ull + 1442695040888963407ull;
    return *state;
}

static double bench_quantile_sorted_double(const double *values, int count, double q)
{
    double pos;
    int lo;
    int hi;
    double frac;

    if (count <= 0)
        return 0.0;
    if (count == 1)
        return values[0];

    if (q <= 0.0)
        return values[0];
    if (q >= 1.0)
        return values[count - 1];

    pos = q * (double)(count - 1);
    lo = (int)pos;
    hi = lo + 1;
    frac = pos - (double)lo;
    if (hi >= count)
        return values[count - 1];
    return values[lo] + (values[hi] - values[lo]) * frac;
}

static double bench_median_sorted_double(const double *values, int count)
{
    return bench_quantile_sorted_double(values, count, 0.5);
}

static bench_stats_t bench_estimate_double(const double *values, int count)
{
    bench_stats_t stats = {0};
    double *sorted = NULL;
    double *deviations = NULL;
    double *boot = NULL;
    double *resampled = NULL;
    int bootstrap_count;
    uint64_t state = 0x9e3779b97f4a7c15ull;

    if (count <= 0)
        return stats;

    sorted = (double *)malloc((size_t)count * sizeof(*sorted));
    deviations = (double *)malloc((size_t)count * sizeof(*deviations));
    if (!sorted || !deviations)
        goto cleanup;

    memcpy(sorted, values, (size_t)count * sizeof(*sorted));
    qsort(sorted, (size_t)count, sizeof(*sorted), bench_compare_double);
    stats.estimate = bench_median_sorted_double(sorted, count);
    for (int i = 0; i < count; ++i)
        deviations[i] = fabs(values[i] - stats.estimate);
    qsort(deviations, (size_t)count, sizeof(*deviations), bench_compare_double);
    stats.mad = bench_median_sorted_double(deviations, count);
    stats.ci_low = stats.estimate;
    stats.ci_high = stats.estimate;
    stats.samples = count;

    bootstrap_count = bench_bootstrap_count();
    if (bootstrap_count < 1)
        goto cleanup;

    boot = (double *)malloc((size_t)bootstrap_count * sizeof(*boot));
    resampled = (double *)malloc((size_t)count * sizeof(*resampled));
    if (!boot || !resampled)
        goto cleanup;

    for (int b = 0; b < bootstrap_count; ++b) {
        for (int i = 0; i < count; ++i)
            resampled[i] = values[(int)(bench_bootstrap_next(&state) % (uint64_t)count)];
        qsort(resampled, (size_t)count, sizeof(*resampled), bench_compare_double);
        boot[b] = bench_median_sorted_double(resampled, count);
    }
    qsort(boot, (size_t)bootstrap_count, sizeof(*boot), bench_compare_double);
    stats.ci_low = bench_quantile_sorted_double(boot, bootstrap_count, 0.025);
    stats.ci_high = bench_quantile_sorted_double(boot, bootstrap_count, 0.975);

cleanup:
    free(sorted);
    free(deviations);
    free(boot);
    free(resampled);
    return stats;
}

static int bench_case_enabled(const char *label)
{
    const char *filter = getenv("MARS_BENCH_FILTER");

    if (!filter || !*filter)
        return 1;
    return strstr(label, filter) != NULL;
}

static void bench_print_us(const char *label, size_t precision, bench_stats_t stats)
{
    if (bench_markdown_enabled()) {
        if (bench_result_count < BENCH_MAX_RESULTS) {
            bench_results[bench_result_count].label = label;
            bench_results[bench_result_count].precision = precision;
            bench_results[bench_result_count].avg_us = stats.estimate;
            ++bench_result_count;
        }
        return;
    }

    printf("%-28s bits=%-4zu med_µs=%10.3f mad_µs=%9.3f ci95=[%9.3f,%9.3f] n=%d\n",
           label,
           precision,
           stats.estimate,
           stats.mad,
           stats.ci_low,
           stats.ci_high,
           stats.samples);
}

static const bench_result_t *bench_find_result_label(const char *label)
{
    size_t i = bench_result_count;

    while (i > 0u) {
        --i;
        if (strcmp(bench_results[i].label, label) == 0)
            return &bench_results[i];
    }
    return NULL;
}

static const bench_result_t *bench_find_result(const char *base_label, size_t precision)
{
    char wanted[128];

    if (precision == 256u)
        snprintf(wanted, sizeof(wanted), "%s", base_label);
    else
        snprintf(wanted, sizeof(wanted), "%s_%zu", base_label, precision);
    return bench_find_result_label(wanted);
}

static void bench_format_markdown_duration_us(double value_us, char *buffer, size_t buffer_size)
{
    if (value_us < 1000.0)
        snprintf(buffer, buffer_size, "%.1f µs", value_us);
    else if (value_us < 1000000.0)
        snprintf(buffer, buffer_size, "%.3f ms", value_us / 1000.0);
    else
        snprintf(buffer, buffer_size, "%.3f s", value_us / 1000000.0);
}

static void bench_print_markdown_table(void)
{
    size_t row;
    size_t col;

    if (!bench_markdown_enabled())
        return;

    static const size_t precisions[] = { 256u, 512u, 768u, 1024u, 2048u, 4096u };

    puts("| Case | `256` bits | `512` bits | `768` bits | `1024` bits | `2048` bits | `4096` bits |");
    puts("|---|---:|---:|---:|---:|---:|---:|");
    for (row = 0u; row < BENCH_ROW_COUNT; ++row) {
        printf("| `%s` |", bench_md_rows[row].display);
        for (col = 0u; col < sizeof(precisions) / sizeof(precisions[0]); ++col) {
            const bench_result_t *result = bench_find_result(bench_md_rows[row].base_label,
                                                             precisions[col]);
            char formatted[32];

            if (result) {
                bench_format_markdown_duration_us(result->avg_us, formatted, sizeof(formatted));
                printf(" `%s` |", formatted);
            }
            else
                printf(" `n/a` |");
        }
        putchar('\n');
    }
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
    mcomplex_t **values = NULL;
    double *samples = NULL;
    const int repeats = bench_repeat_count();
    int i;

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

    values = (mcomplex_t **)calloc((size_t)iters, sizeof(*values));
    if (!values) {
        fprintf(stderr, "%s work array alloc failed\n", label);
        goto cleanup;
    }
    for (i = 0; i < iters; ++i) {
        values[i] = mc_clone(src);
        if (!values[i]) {
            fprintf(stderr, "%s work clone failed\n", label);
            goto cleanup;
        }
    }

    samples = (double *)calloc((size_t)repeats, sizeof(*samples));
    if (!samples) {
        fprintf(stderr, "%s repeat sample alloc failed\n", label);
        goto cleanup;
    }

    for (int repeat = 0; repeat < repeats; ++repeat) {
        uint64_t start;
        uint64_t end;
        double batch_us;

        for (i = 0; i < iters; ++i) {
            mc_free(values[i]);
            values[i] = mc_clone(src);
            if (!values[i]) {
                fprintf(stderr, "%s work clone failed\n", label);
                goto cleanup;
            }
        }

        start = now_ns();
        for (i = 0; i < iters; ++i) {
            if (bench_run_unary_inplace(values[i], fn) != 0 || mc_isnan(values[i])) {
                fprintf(stderr, "%s timed run failed\n", label);
                goto cleanup;
            }
        }
        end = now_ns();

        batch_us = ((double)(end - start) / (double)iters) / 1000.0;
        samples[repeat] = batch_us;
    }

    bench_print_us(label, precision, bench_estimate_double(samples, repeats));

cleanup:
    free(samples);
    if (values) {
        for (i = 0; i < iters; ++i)
            mc_free(values[i]);
        free(values);
    }
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
    mcomplex_t **values = NULL;
    double *samples = NULL;
    const int repeats = bench_repeat_count();
    int i;

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

    values = (mcomplex_t **)calloc((size_t)iters, sizeof(*values));
    if (!values) {
        fprintf(stderr, "%s work array alloc failed\n", label);
        goto cleanup;
    }
    for (i = 0; i < iters; ++i) {
        values[i] = mc_clone(lhs);
        if (!values[i]) {
            fprintf(stderr, "%s work clone failed\n", label);
            goto cleanup;
        }
    }

    samples = (double *)calloc((size_t)repeats, sizeof(*samples));
    if (!samples) {
        fprintf(stderr, "%s repeat sample alloc failed\n", label);
        goto cleanup;
    }

    for (int repeat = 0; repeat < repeats; ++repeat) {
        uint64_t start;
        uint64_t end;
        double batch_us;

        for (i = 0; i < iters; ++i) {
            mc_free(values[i]);
            values[i] = mc_clone(lhs);
            if (!values[i]) {
                fprintf(stderr, "%s work clone failed\n", label);
                goto cleanup;
            }
        }

        start = now_ns();
        for (i = 0; i < iters; ++i) {
            if (bench_run_binary_inplace(values[i], rhs, fn) != 0 || mc_isnan(values[i])) {
                fprintf(stderr, "%s timed run failed\n", label);
                goto cleanup;
            }
        }
        end = now_ns();

        batch_us = ((double)(end - start) / (double)iters) / 1000.0;
        samples[repeat] = batch_us;
    }

    bench_print_us(label, precision, bench_estimate_double(samples, repeats));

cleanup:
    free(samples);
    if (values) {
        for (i = 0; i < iters; ++i)
            mc_free(values[i]);
        free(values);
    }
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
    if (!bench_markdown_enabled()) {
        puts("== mcomplex maths bench ==");
        puts("Prepared for post-implementation benchmarking.");
        puts("Do not treat timings from the current wrapper-backed implementation as final.");
        puts("Scale iterations with MARS_BENCH_SCALE=<n> if you want longer runs.");
        puts("Tune repeats with MARS_BENCH_REPEATS=<n> and bootstrap resamples with MARS_BENCH_BOOTSTRAP=<n>.");
        puts("Reported timings use the sample median with MAD and a bootstrap 95% CI.");
        puts("Filter individual cases with MARS_BENCH_FILTER=<substring>.");
        puts("");
    }

    run_unary_case("exp_0_567_plus_0_321i", "0.567 + 0.321i", 256u, mc_exp, bench_scaled_iters(8));
    run_unary_case("log_0_567_plus_0_321i", "0.567 + 0.321i", 256u, mc_log, bench_scaled_iters(8));
    run_unary_case("sqrt_0_567_plus_0_321i", "0.567 + 0.321i", 256u, mc_sqrt, bench_scaled_iters(8));
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
    run_unary_case("lambert_w0_1_plus_1i", "1 + 1i", 256u, mc_lambert_w0, bench_scaled_iters(2));
    run_unary_case("productlog_1_plus_1i", "1 + 1i", 256u, mc_productlog, bench_scaled_iters(8));
    run_unary_case("lambert_wm1_-0_2_-0_1i", "-0.2 - 0.1i", 256u, mc_lambert_wm1, bench_scaled_iters(6));
    run_unary_case("ei_1_plus_1i", "1 + 1i", 256u, mc_ei, bench_scaled_iters(300));
    run_unary_case("e1_1_plus_1i", "1 + 1i", 256u, mc_e1, bench_scaled_iters(300));

    run_binary_case("beta_1_5_0_5__2_-0_3", "1.5 + 0.5i", "2 - 0.3i", 256u, mc_beta, bench_scaled_iters(200));
    run_binary_case("logbeta_1_5_0_5__2_-0_3", "1.5 + 0.5i", "2 - 0.3i", 256u, mc_logbeta, bench_scaled_iters(200));

    run_unary_case("exp_0_567_plus_0_321i_512", "0.567 + 0.321i", 512u, mc_exp, bench_scaled_iters(4));
    run_unary_case("log_0_567_plus_0_321i_512", "0.567 + 0.321i", 512u, mc_log, bench_scaled_iters(4));
    run_unary_case("sqrt_0_567_plus_0_321i_512", "0.567 + 0.321i", 512u, mc_sqrt, bench_scaled_iters(4));
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
    run_unary_case("digamma_2_plus_1i_512", "2 + 1i", 512u, mc_digamma, bench_scaled_iters(1));
    run_unary_case("trigamma_2_plus_0_5i_512", "2 + 0.5i", 512u, mc_trigamma, bench_scaled_iters(1));
    run_unary_case("tetragamma_2_plus_0_5i_512", "2 + 0.5i", 512u, mc_tetragamma, bench_scaled_iters(1));

    run_unary_case("exp_0_567_plus_0_321i_768", "0.567 + 0.321i", 768u, mc_exp, bench_scaled_iters(2));
    run_unary_case("log_0_567_plus_0_321i_768", "0.567 + 0.321i", 768u, mc_log, bench_scaled_iters(2));
    run_unary_case("sqrt_0_567_plus_0_321i_768", "0.567 + 0.321i", 768u, mc_sqrt, bench_scaled_iters(2));
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
    run_unary_case("digamma_2_plus_1i_768", "2 + 1i", 768u, mc_digamma, bench_scaled_iters(1));
    run_unary_case("trigamma_2_plus_0_5i_768", "2 + 0.5i", 768u, mc_trigamma, bench_scaled_iters(1));
    run_unary_case("tetragamma_2_plus_0_5i_768", "2 + 0.5i", 768u, mc_tetragamma, bench_scaled_iters(1));
    run_unary_case("ei_1_plus_1i_768", "1 + 1i", 768u, mc_ei, bench_scaled_iters(1));
    run_unary_case("e1_1_plus_1i_768", "1 + 1i", 768u, mc_e1, bench_scaled_iters(1));

    run_unary_case("exp_0_567_plus_0_321i_1024", "0.567 + 0.321i", 1024u, mc_exp, bench_scaled_iters(1));
    run_unary_case("log_0_567_plus_0_321i_1024", "0.567 + 0.321i", 1024u, mc_log, bench_scaled_iters(1));
    run_unary_case("sqrt_0_567_plus_0_321i_1024", "0.567 + 0.321i", 1024u, mc_sqrt, bench_scaled_iters(1));
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
    run_unary_case("digamma_2_plus_1i_1024", "2 + 1i", 1024u, mc_digamma, bench_scaled_iters(1));
    run_unary_case("trigamma_2_plus_0_5i_1024", "2 + 0.5i", 1024u, mc_trigamma, bench_scaled_iters(1));
    run_unary_case("tetragamma_2_plus_0_5i_1024", "2 + 0.5i", 1024u, mc_tetragamma, bench_scaled_iters(1));
    run_unary_case("ei_1_plus_1i_1024", "1 + 1i", 1024u, mc_ei, bench_scaled_iters(1));
    run_unary_case("e1_1_plus_1i_1024", "1 + 1i", 1024u, mc_e1, bench_scaled_iters(1));

    run_unary_case("exp_0_567_plus_0_321i_2048", "0.567 + 0.321i", 2048u, mc_exp, bench_scaled_iters(1));
    run_unary_case("log_0_567_plus_0_321i_2048", "0.567 + 0.321i", 2048u, mc_log, bench_scaled_iters(1));
    run_unary_case("sqrt_0_567_plus_0_321i_2048", "0.567 + 0.321i", 2048u, mc_sqrt, bench_scaled_iters(1));
    run_unary_case("sin_0_567_plus_0_321i_2048", "0.567 + 0.321i", 2048u, mc_sin, bench_scaled_iters(1));
    run_unary_case("cos_0_567_plus_0_321i_2048", "0.567 + 0.321i", 2048u, mc_cos, bench_scaled_iters(1));
    run_unary_case("tan_0_567_plus_0_321i_2048", "0.567 + 0.321i", 2048u, mc_tan, bench_scaled_iters(1));
    run_unary_case("atan_0_321_plus_0_123i_2048", "0.321 + 0.123i", 2048u, mc_atan, bench_scaled_iters(1));
    run_binary_case("atan2_0_5_0_25i__-0_75_0_1i_2048", "0.5 + 0.25i", "-0.75 + 0.1i", 2048u, mc_atan2, bench_scaled_iters(1));
    run_unary_case("asin_0_321_plus_0_123i_2048", "0.321 + 0.123i", 2048u, mc_asin, bench_scaled_iters(1));
    run_unary_case("acos_0_321_plus_0_123i_2048", "0.321 + 0.123i", 2048u, mc_acos, bench_scaled_iters(1));
    run_unary_case("sinh_0_567_plus_0_321i_2048", "0.567 + 0.321i", 2048u, mc_sinh, bench_scaled_iters(1));
    run_unary_case("cosh_0_567_plus_0_321i_2048", "0.567 + 0.321i", 2048u, mc_cosh, bench_scaled_iters(1));
    run_unary_case("tanh_0_567_plus_0_321i_2048", "0.567 + 0.321i", 2048u, mc_tanh, bench_scaled_iters(1));
    run_unary_case("asinh_0_321_plus_0_123i_2048", "0.321 + 0.123i", 2048u, mc_asinh, bench_scaled_iters(1));
    run_unary_case("acosh_2_plus_0_5i_2048", "2 + 0.5i", 2048u, mc_acosh, bench_scaled_iters(1));
    run_unary_case("atanh_0_321_plus_0_123i_2048", "0.321 + 0.123i", 2048u, mc_atanh, bench_scaled_iters(1));
    run_unary_case("lambert_w0_1_plus_1i_2048", "1 + 1i", 2048u, mc_lambert_w0, bench_scaled_iters(1));
    run_unary_case("lambert_wm1_-0_2_-0_1i_2048", "-0.2 - 0.1i", 2048u, mc_lambert_wm1, bench_scaled_iters(1));
    run_unary_case("productlog_1_plus_1i_2048", "1 + 1i", 2048u, mc_productlog, bench_scaled_iters(1));
    run_unary_case("gamma_1_5_plus_0_7i_2048", "1.5 + 0.7i", 2048u, mc_gamma, bench_scaled_iters(1));
    run_unary_case("lgamma_1_5_plus_0_7i_2048", "1.5 + 0.7i", 2048u, mc_lgamma, bench_scaled_iters(1));
    run_unary_case("digamma_2_plus_1i_2048", "2 + 1i", 2048u, mc_digamma, bench_scaled_iters(1));
    run_unary_case("trigamma_2_plus_0_5i_2048", "2 + 0.5i", 2048u, mc_trigamma, bench_scaled_iters(1));
    run_unary_case("tetragamma_2_plus_0_5i_2048", "2 + 0.5i", 2048u, mc_tetragamma, bench_scaled_iters(1));
    run_unary_case("ei_1_plus_1i_2048", "1 + 1i", 2048u, mc_ei, bench_scaled_iters(1));
    run_unary_case("e1_1_plus_1i_2048", "1 + 1i", 2048u, mc_e1, bench_scaled_iters(1));

    run_unary_case("exp_0_567_plus_0_321i_4096", "0.567 + 0.321i", 4096u, mc_exp, bench_scaled_iters(1));
    run_unary_case("log_0_567_plus_0_321i_4096", "0.567 + 0.321i", 4096u, mc_log, bench_scaled_iters(1));
    run_unary_case("sqrt_0_567_plus_0_321i_4096", "0.567 + 0.321i", 4096u, mc_sqrt, bench_scaled_iters(1));
    run_unary_case("sin_0_567_plus_0_321i_4096", "0.567 + 0.321i", 4096u, mc_sin, bench_scaled_iters(1));
    run_unary_case("cos_0_567_plus_0_321i_4096", "0.567 + 0.321i", 4096u, mc_cos, bench_scaled_iters(1));
    run_unary_case("tan_0_567_plus_0_321i_4096", "0.567 + 0.321i", 4096u, mc_tan, bench_scaled_iters(1));
    run_unary_case("atan_0_321_plus_0_123i_4096", "0.321 + 0.123i", 4096u, mc_atan, bench_scaled_iters(1));
    run_binary_case("atan2_0_5_0_25i__-0_75_0_1i_4096", "0.5 + 0.25i", "-0.75 + 0.1i", 4096u, mc_atan2, bench_scaled_iters(1));
    run_unary_case("asin_0_321_plus_0_123i_4096", "0.321 + 0.123i", 4096u, mc_asin, bench_scaled_iters(1));
    run_unary_case("acos_0_321_plus_0_123i_4096", "0.321 + 0.123i", 4096u, mc_acos, bench_scaled_iters(1));
    run_unary_case("sinh_0_567_plus_0_321i_4096", "0.567 + 0.321i", 4096u, mc_sinh, bench_scaled_iters(1));
    run_unary_case("cosh_0_567_plus_0_321i_4096", "0.567 + 0.321i", 4096u, mc_cosh, bench_scaled_iters(1));
    run_unary_case("tanh_0_567_plus_0_321i_4096", "0.567 + 0.321i", 4096u, mc_tanh, bench_scaled_iters(1));
    run_unary_case("asinh_0_321_plus_0_123i_4096", "0.321 + 0.123i", 4096u, mc_asinh, bench_scaled_iters(1));
    run_unary_case("acosh_2_plus_0_5i_4096", "2 + 0.5i", 4096u, mc_acosh, bench_scaled_iters(1));
    run_unary_case("atanh_0_321_plus_0_123i_4096", "0.321 + 0.123i", 4096u, mc_atanh, bench_scaled_iters(1));
    run_unary_case("lambert_w0_1_plus_1i_4096", "1 + 1i", 4096u, mc_lambert_w0, bench_scaled_iters(1));
    run_unary_case("lambert_wm1_-0_2_-0_1i_4096", "-0.2 - 0.1i", 4096u, mc_lambert_wm1, bench_scaled_iters(1));
    run_unary_case("productlog_1_plus_1i_4096", "1 + 1i", 4096u, mc_productlog, bench_scaled_iters(1));
    run_unary_case("gamma_1_5_plus_0_7i_4096", "1.5 + 0.7i", 4096u, mc_gamma, bench_scaled_iters(1));
    run_unary_case("lgamma_1_5_plus_0_7i_4096", "1.5 + 0.7i", 4096u, mc_lgamma, bench_scaled_iters(1));
    run_unary_case("digamma_2_plus_1i_4096", "2 + 1i", 4096u, mc_digamma, bench_scaled_iters(1));
    run_unary_case("trigamma_2_plus_0_5i_4096", "2 + 0.5i", 4096u, mc_trigamma, bench_scaled_iters(1));
    run_unary_case("tetragamma_2_plus_0_5i_4096", "2 + 0.5i", 4096u, mc_tetragamma, bench_scaled_iters(1));
    run_unary_case("ei_1_plus_1i_4096", "1 + 1i", 4096u, mc_ei, bench_scaled_iters(1));
    run_unary_case("e1_1_plus_1i_4096", "1 + 1i", 4096u, mc_e1, bench_scaled_iters(1));

    bench_print_markdown_table();

    return 0;
}
