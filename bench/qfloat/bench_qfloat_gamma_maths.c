#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "qfloat.h"

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
        qfloat_t (*unary)(qfloat_t);
        qfloat_t (*binary)(qfloat_t, qfloat_t);
    } fn;
} bench_case_t;

typedef struct bench_stats_t {
    double estimate;
    double mad;
    double ci_low;
    double ci_high;
    int samples;
} bench_stats_t;

static void run_unary_case(const char *label, const char *text, qfloat_t (*fn)(qfloat_t), int iters);
static void run_binary_case(const char *label, const char *lhs_text, const char *rhs_text,
                            qfloat_t (*fn)(qfloat_t, qfloat_t), int iters);

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
    BENCH_UNARY_CASE("exp_1", "qf_exp(1)", "1", 2000, qf_exp),
    BENCH_UNARY_CASE("log_10", "qf_log(10)", "10", 2000, qf_log),
    BENCH_UNARY_CASE("erf_0_5", "qf_erf(0.5)", "0.5", 2000, qf_erf),
    BENCH_UNARY_CASE("erfc_0_5", "qf_erfc(0.5)", "0.5", 2000, qf_erfc),
    BENCH_UNARY_CASE("gamma_2_3", "qf_gamma(2.3)", "2.3", 500, qf_gamma),
    BENCH_UNARY_CASE("lgamma_2_3", "qf_lgamma(2.3)", "2.3", 500, qf_lgamma),
    BENCH_UNARY_CASE("gamma_2_5", "qf_gamma(2.5)", "2.5", 1000, qf_gamma),
    BENCH_UNARY_CASE("lgamma_2_5", "qf_lgamma(2.5)", "2.5", 1000, qf_lgamma),
    BENCH_UNARY_CASE("gamma_3_5", "qf_gamma(3.5)", "3.5", 1000, qf_gamma),
    BENCH_UNARY_CASE("lgamma_3_5", "qf_lgamma(3.5)", "3.5", 1000, qf_lgamma),
    BENCH_UNARY_CASE("digamma_2_3", "qf_digamma(2.3)", "2.3", 1000, qf_digamma),
    BENCH_UNARY_CASE("trigamma_2_3", "qf_trigamma(2.3)", "2.3", 500, qf_trigamma),
    BENCH_UNARY_CASE("tetragamma_2_3", "qf_tetragamma(2.3)", "2.3", 500, qf_tetragamma),
    BENCH_UNARY_CASE("gammainv_9_5", "qf_gammainv(119292.4619946090070787515047110059)",
                     "119292.4619946090070787515047110059", 200, qf_gammainv),
    BENCH_UNARY_CASE("lambert_w0_1", "qf_lambert_w0(1)", "1", 2000, qf_lambert_w0),
    BENCH_UNARY_CASE("lambert_wm1_-0_1", "qf_lambert_wm1(-0.1)", "-0.1", 1000, qf_lambert_wm1),
    BENCH_UNARY_CASE("ei_1", "qf_Ei(1)", "1", 1000, qf_Ei),
    BENCH_UNARY_CASE("e1_1", "qf_E1(1)", "1", 1000, qf_E1),
    BENCH_BINARY_CASE("beta_2_3_4_5", "qf_beta(2.3, 4.5)", "2.3", "4.5", 500, qf_beta),
    BENCH_BINARY_CASE("logbeta_2_3_4_5", "qf_logbeta(2.3, 4.5)", "2.3", "4.5", 500, qf_logbeta)};

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

static void run_unary_case(const char *label, const char *text, qfloat_t (*fn)(qfloat_t), int iters)
{
    qfloat_t src;
    qfloat_t warm;
    double *samples = NULL;
    const int repeats = bench_repeat_count();

    if (!bench_case_enabled(label))
        return;

    src = qf_from_string(text);
    warm = fn(src);

    if (qf_isnan(warm)) {
        fprintf(stderr, "%s warmup failed\n", label);
        return;
    }

    samples = bench_samples_alloc(repeats, label);
    for (int r = 0; r < repeats; ++r) {
        uint64_t start = now_ns();
        for (int i = 0; i < iters; ++i) {
            qfloat_t value = fn(src);

            if (qf_isnan(value)) {
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
                            qfloat_t (*fn)(qfloat_t, qfloat_t), int iters)
{
    qfloat_t lhs;
    qfloat_t rhs;
    qfloat_t warm;
    double *samples = NULL;
    const int repeats = bench_repeat_count();

    if (!bench_case_enabled(label))
        return;

    lhs = qf_from_string(lhs_text);
    rhs = qf_from_string(rhs_text);
    warm = fn(lhs, rhs);

    if (qf_isnan(warm)) {
        fprintf(stderr, "%s warmup failed\n", label);
        return;
    }

    samples = bench_samples_alloc(repeats, label);
    for (int r = 0; r < repeats; ++r) {
        uint64_t start = now_ns();
        for (int i = 0; i < iters; ++i) {
            qfloat_t value = fn(lhs, rhs);

            if (qf_isnan(value)) {
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

int main(void)
{
    if (!bench_markdown_enabled()) {
        puts("== qfloat gamma maths bench ==");
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
