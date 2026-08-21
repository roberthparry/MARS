#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "qcomplex.h"

typedef struct bench_result_t {
    char label[64];
    double avg_us;
} bench_result_t;

typedef enum bench_case_kind_t { BENCH_CASE_UNARY = 0, BENCH_CASE_BINARY } bench_case_kind_t;

typedef struct bench_case_t {
    const char *label;
    const char *display;
    bench_case_kind_t kind;
    const char *lhs_text;
    const char *rhs_text;
    int base_iters;
    union {
        qcomplex_t (*unary)(qcomplex_t);
        qcomplex_t (*binary)(qcomplex_t, qcomplex_t);
    } fn;
} bench_case_t;

typedef struct bench_stats_t {
    double estimate;
    double mad;
    double ci_low;
    double ci_high;
    int samples;
} bench_stats_t;

static void run_unary_case(const char *label, const char *text, qcomplex_t (*fn)(qcomplex_t), int iters);
static void run_binary_case(const char *label, const char *lhs_text, const char *rhs_text,
                            qcomplex_t (*fn)(qcomplex_t, qcomplex_t), int iters);
static qcomplex_t bench_qc_gammainv_gamma_2_5_plus_0_3i(qcomplex_t unused);

#define BENCH_UNARY_CASE(label, display, text, iters, fn)                                                              \
    {                                                                                                                  \
        label, display, BENCH_CASE_UNARY, text, NULL, iters,                                                           \
        {                                                                                                              \
            .unary = fn                                                                                                \
        }                                                                                                              \
    }

#define BENCH_BINARY_CASE(label, display, lhs_text, rhs_text, iters, fn)                                               \
    {                                                                                                                  \
        label, display, BENCH_CASE_BINARY, lhs_text, rhs_text, iters,                                                  \
        {                                                                                                              \
            .binary = fn                                                                                               \
        }                                                                                                              \
    }

static bench_result_t bench_results[64];
static size_t bench_result_count = 0u;

static const bench_case_t bench_cases[] = {
    BENCH_UNARY_CASE("exp_1_plus_1i", "qc_exp(1+i)", "(1,1)", 1000, qc_exp),
    BENCH_UNARY_CASE("log_1_plus_1i", "qc_log(1+i)", "(1,1)", 1000, qc_log),
    BENCH_UNARY_CASE("erf_0_5_plus_0_5i", "qc_erf(0.5+0.5i)", "(0.5,0.5)", 500, qc_erf),
    BENCH_UNARY_CASE("erfc_0_5_plus_0_5i", "qc_erfc(0.5+0.5i)", "(0.5,0.5)", 500, qc_erfc),
    BENCH_UNARY_CASE("gamma_1_5_plus_0_7i", "qc_gamma(1.5+0.7i)", "(1.5,0.7)", 200, qc_gamma),
    BENCH_UNARY_CASE("lgamma_1_5_plus_0_7i", "qc_lgamma(1.5+0.7i)", "(1.5,0.7)", 200, qc_lgamma),
    BENCH_UNARY_CASE("digamma_2_plus_1i", "qc_digamma(2+i)", "(2,1)", 500, qc_digamma),
    BENCH_UNARY_CASE("trigamma_2_plus_0_5i", "qc_trigamma(2+0.5i)", "(2,0.5)", 300, qc_trigamma),
    BENCH_UNARY_CASE("tetragamma_2_plus_0_5i", "qc_tetragamma(2+0.5i)", "(2,0.5)", 300, qc_tetragamma),
    BENCH_UNARY_CASE("gammainv_gamma_2_5", "qc_gammainv(3.323350970447842551184064031264648)",
                     "3.323350970447842551184064031264648", 100, qc_gammainv),
    BENCH_UNARY_CASE("gammainv_gamma_2_5_0_3i", "qc_gammainv(qc_gamma(2.5+0.3i))", "(0,0)", 100,
                     bench_qc_gammainv_gamma_2_5_plus_0_3i),
    BENCH_UNARY_CASE("productlog_1_plus_1i", "qc_productlog(1+i)", "(1,1)", 300, qc_productlog),
    BENCH_UNARY_CASE("lambert_wm1_-0_2_-0_1i", "qc_lambert_wm1(-0.2-0.1i)", "(-0.2,-0.1)", 200, qc_lambert_wm1),
    BENCH_UNARY_CASE("ei_1_plus_1i", "qc_Ei(1+i)", "(1,1)", 300, qc_Ei),
    BENCH_UNARY_CASE("e1_1_plus_1i", "qc_E1(1+i)", "(1,1)", 300, qc_E1),
    BENCH_BINARY_CASE("beta_1_5_0_5__2_-0_3", "qc_beta(1.5+0.5i, 2-0.3i)", "(1.5,0.5)", "(2,-0.3)", 200, qc_beta),
    BENCH_BINARY_CASE("logbeta_1_5_0_5__2_-0_3", "qc_logbeta(1.5+0.5i, 2-0.3i)", "(1.5,0.5)", "(2,-0.3)", 200,
                      qc_logbeta)};

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static int bench_markdown_enabled(void)
{
    const char *format = getenv("MARS_BENCH_FORMAT");

    return format && strcmp(format, "md") == 0;
}

static int bench_doc_iters(int base_iters)
{
    if (bench_markdown_enabled() && base_iters < 3)
        return 3;
    return base_iters;
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

static void bench_format_markdown_duration_us(double value_us, char *buffer, size_t buffer_size)
{
    if (value_us < 1000.0)
        snprintf(buffer, buffer_size, "%.1f µs", value_us);
    else if (value_us < 1000000.0)
        snprintf(buffer, buffer_size, "%.3f ms", value_us / 1000.0);
    else
        snprintf(buffer, buffer_size, "%.3f s", value_us / 1000000.0);
}

static void bench_print_us(const char *label, bench_stats_t stats)
{
    if (bench_markdown_enabled()) {
        if (bench_result_count < sizeof(bench_results) / sizeof(bench_results[0])) {
            snprintf(bench_results[bench_result_count].label, sizeof(bench_results[bench_result_count].label), "%s",
                     label);
            bench_results[bench_result_count].avg_us = stats.estimate;
            ++bench_result_count;
        }
        return;
    }

    printf("%-28s med_µs=%10.3f mad_µs=%9.3f ci95=[%9.3f,%9.3f] n=%d\n", label, stats.estimate, stats.mad, stats.ci_low,
           stats.ci_high, stats.samples);
}

static double *bench_samples_alloc(int repeats, const char *label)
{
    double *samples = (double *)malloc((size_t)repeats * sizeof(*samples));

    if (!samples) {
        fprintf(stderr, "%s sample allocation failed\n", label);
        exit(EXIT_FAILURE);
    }
    return samples;
}

static int bench_find_result(const char *label, double *avg_us)
{
    for (size_t i = bench_result_count; i > 0u; --i) {
        size_t index = i - 1u;

        if (strcmp(bench_results[index].label, label) == 0) {
            *avg_us = bench_results[index].avg_us;
            return 1;
        }
    }
    return 0;
}

static void bench_print_markdown_table(void)
{
    if (!bench_markdown_enabled())
        return;

    puts("| Case | Time |");
    puts("|---|---:|");
    for (size_t row = 0u; row < sizeof(bench_cases) / sizeof(bench_cases[0]); ++row) {
        double avg_us;
        char formatted[32];

        printf("| `%s` | ", bench_cases[row].display);
        if (bench_find_result(bench_cases[row].label, &avg_us)) {
            bench_format_markdown_duration_us(avg_us, formatted, sizeof(formatted));
            printf("`%s` |\n", formatted);
        } else {
            puts("- |");
        }
    }
}

static void run_case(const bench_case_t *bench_case)
{
    int iters = bench_scaled_iters(bench_case->base_iters);

    if (bench_case->kind == BENCH_CASE_UNARY) {
        run_unary_case(bench_case->label, bench_case->lhs_text, bench_case->fn.unary, iters);
        return;
    }

    run_binary_case(bench_case->label, bench_case->lhs_text, bench_case->rhs_text, bench_case->fn.binary, iters);
}

static void run_unary_case(const char *label, const char *text, qcomplex_t (*fn)(qcomplex_t), int iters)
{
    qcomplex_t src;
    qcomplex_t warm;
    double *samples = NULL;
    const int repeats = bench_repeat_count();

    if (!bench_case_enabled(label))
        return;

    src = qc_from_string(text);
    warm = fn(src);

    if (qc_isnan(warm)) {
        fprintf(stderr, "%s warmup failed\n", label);
        return;
    }

    samples = bench_samples_alloc(repeats, label);
    for (int r = 0; r < repeats; ++r) {
        uint64_t start = now_ns();
        for (int i = 0; i < iters; ++i) {
            qcomplex_t value = fn(src);

            if (qc_isnan(value)) {
                fprintf(stderr, "%s timed run failed\n", label);
                free(samples);
                return;
            }
        }
        samples[r] = ((double)(now_ns() - start) / (double)iters) / 1000.0;
    }

    bench_print_us(label, bench_estimate_double(samples, repeats));
    free(samples);
}

static void run_binary_case(const char *label, const char *lhs_text, const char *rhs_text,
                            qcomplex_t (*fn)(qcomplex_t, qcomplex_t), int iters)
{
    qcomplex_t lhs;
    qcomplex_t rhs;
    qcomplex_t warm;
    double *samples = NULL;
    const int repeats = bench_repeat_count();

    if (!bench_case_enabled(label))
        return;

    lhs = qc_from_string(lhs_text);
    rhs = qc_from_string(rhs_text);
    warm = fn(lhs, rhs);

    if (qc_isnan(warm)) {
        fprintf(stderr, "%s warmup failed\n", label);
        return;
    }

    samples = bench_samples_alloc(repeats, label);
    for (int r = 0; r < repeats; ++r) {
        uint64_t start = now_ns();
        for (int i = 0; i < iters; ++i) {
            qcomplex_t value = fn(lhs, rhs);

            if (qc_isnan(value)) {
                fprintf(stderr, "%s timed run failed\n", label);
                free(samples);
                return;
            }
        }
        samples[r] = ((double)(now_ns() - start) / (double)iters) / 1000.0;
    }

    bench_print_us(label, bench_estimate_double(samples, repeats));
    free(samples);
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
    if (!bench_markdown_enabled()) {
        puts("== qcomplex maths bench ==");
        puts("Scale iterations with MARS_BENCH_SCALE=<n> if you want longer runs.");
        puts("Tune repeats with MARS_BENCH_REPEATS=<n> and bootstrap resamples with MARS_BENCH_BOOTSTRAP=<n>.");
        puts("Reported timings use the sample median with MAD and a bootstrap 95% CI.");
        puts("Filter individual cases with MARS_BENCH_FILTER=<substring>.");
        puts("");
    }

    for (size_t i = 0u; i < sizeof(bench_cases) / sizeof(bench_cases[0]); ++i)
        run_case(&bench_cases[i]);

    bench_print_markdown_table();
    return 0;
}
