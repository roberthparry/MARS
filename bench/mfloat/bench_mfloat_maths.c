#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "mfloat.h"
#include "mfloat/mfloat_internal.h"

typedef struct bench_result_t {
    char label[64];
    size_t precision;
    double avg_us;
} bench_result_t;

static bench_result_t bench_results[512];
static size_t bench_result_count = 0u;

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

typedef enum bench_dispatch_kind_t {
    BENCH_DISPATCH_UNARY = 0,
    BENCH_DISPATCH_BINARY,
    BENCH_DISPATCH_TERNARY,
    BENCH_DISPATCH_SINCOS,
    BENCH_DISPATCH_SINHCOSH
} bench_dispatch_kind_t;

typedef struct bench_dispatch_case_t {
    const char *label_base;
    bench_dispatch_kind_t kind;
    const char *x_text;
    const char *a_text;
    const char *b_text;
    int base_iters;
    union {
        int (*unary)(mfloat_t *);
        int (*binary)(mfloat_t *, const mfloat_t *);
        int (*ternary)(mfloat_t *, const mfloat_t *, const mfloat_t *);
    } fn;
} bench_dispatch_case_t;

static const bench_md_row_t bench_md_rows[] = {
    { "exp", "mf_exp(1.23456789)" },
    { "log", "mf_log(2.345678)" },
    { "sqrt", "mf_sqrt(1.23456789)" },
    { "sin", "mf_sin(0.567)" },
    { "cos", "mf_cos(0.7)" },
    { "sincos", "mf_sincos(0.7)" },
    { "tan", "mf_tan(0.7)" },
    { "atan", "mf_atan(0.7)" },
    { "asin_general", "mf_asin(0.7)" },
    { "acos_general", "mf_acos(0.7)" },
    { "atan2_general", "mf_atan2(0.5,-0.75)" },
    { "sinh", "mf_sinh(0.7)" },
    { "cosh", "mf_cosh(0.7)" },
    { "sinhcosh", "mf_sinhcosh(0.7)" },
    { "tanh", "mf_tanh(0.7)" },
    { "asinh", "mf_asinh(0.5)" },
    { "acosh", "mf_acosh(2)" },
    { "atanh", "mf_atanh(0.5)" },
    { "lambert_w0", "mf_lambert_w0(0.7)" },
    { "lambert_wm1", "mf_lambert_wm1(-0.2)" },
    { "gamma", "mf_gamma(2.345)" },
    { "lgamma", "mf_lgamma(2.345)" },
    { "digamma", "mf_digamma(2.345)" },
    { "trigamma", "mf_trigamma(2.345)" },
    { "tetragamma", "mf_tetragamma(2.345)" },
    { "ei_5", "mf_ei(5)" },
    { "e1_5", "mf_e1(5)" }
};

static int bench_markdown_enabled(void);
static int bench_doc_iters(int base_iters);

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

static int bench_wants_section(const char *name)
{
    const char *section = getenv("MARS_BENCH_SECTION");

    /* Default to the full bench, but allow quicker section-by-section profiling. */
    if (!section || !*section || strcmp(section, "all") == 0)
        return 1;
    return strcmp(section, name) == 0;
}

static int bench_wants_exact_section(const char *name)
{
    const char *section = getenv("MARS_BENCH_SECTION");

    if (!section || !*section || strcmp(section, "all") == 0)
        return 0;
    return strcmp(section, name) == 0;
}

static int bench_case_enabled(const char *label)
{
    const char *filter = getenv("MARS_BENCH_FILTER");

    if (!filter || !*filter)
        return 1;
    return strstr(label, filter) != NULL;
}

static int bench_markdown_enabled(void)
{
    const char *format = getenv("MARS_BENCH_FORMAT");

    return format && strcmp(format, "md") == 0;
}

static void bench_fail(const char *label, const char *message)
{
    fprintf(stderr, "%s %s\n", label, message);
    exit(EXIT_FAILURE);
}

static void bench_print_us(const char *label, size_t precision, bench_stats_t stats)
{
    if (bench_markdown_enabled()) {
        if (bench_result_count < sizeof(bench_results) / sizeof(bench_results[0])) {
            snprintf(bench_results[bench_result_count].label,
                     sizeof(bench_results[bench_result_count].label),
                     "%s",
                     label);
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

static int bench_find_result(const char *base_label, size_t precision, double *avg_us)
{
    char wanted[80];

    snprintf(wanted, sizeof(wanted), "%s_%zu", base_label, precision);
    for (size_t i = bench_result_count; i > 0u; --i) {
        size_t index = i - 1u;

        if (strcmp(bench_results[index].label, wanted) == 0) {
            *avg_us = bench_results[index].avg_us;
            return 1;
        }
    }
    return 0;
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
    static const size_t precisions[] = { 256u, 512u, 768u, 1024u, 2048u, 4096u };
    if (!bench_markdown_enabled())
        return;

    puts("| Case | `256` bits | `512` bits | `768` bits | `1024` bits | `2048` bits | `4096` bits |");
    puts("|---|---:|---:|---:|---:|---:|---:|");
    for (size_t row = 0u; row < sizeof(bench_md_rows) / sizeof(bench_md_rows[0]); ++row) {
        printf("| `%s` |", bench_md_rows[row].display);
        for (size_t col = 0u; col < sizeof(precisions) / sizeof(precisions[0]); ++col) {
            double avg_us;
            char formatted[32];

            if (bench_find_result(bench_md_rows[row].base_label, precisions[col], &avg_us)) {
                bench_format_markdown_duration_us(avg_us, formatted, sizeof(formatted));
                printf(" `%s` |", formatted);
            }
            else
                printf(" - |");
        }
        putchar('\n');
    }
}

static int bench_verify_binary_case(const char *label,
                                    const mfloat_t *lhs_src,
                                    const mfloat_t *rhs,
                                    int (*fn)(mfloat_t *, const mfloat_t *))
{
    mfloat_t *actual = NULL;
    int rc = -1;

    if (!(strncmp(label, "add_", 4) == 0 || strncmp(label, "sub_", 4) == 0))
        return 0;

    actual = mf_clone(lhs_src);
    if (!actual)
        goto cleanup;

    if (fn(actual, rhs) != 0)
        goto cleanup;

    rc = 0;

cleanup:
    mf_free(actual);
    return rc;
}

static void run_unary_case(const char *label,
                           const char *text,
                           size_t precision,
                           int (*fn)(mfloat_t *),
                           int iters)
{
    size_t old_prec;
    mfloat_t *src;
    mfloat_t **values = NULL;
    double *samples = NULL;
    const int repeats = bench_repeat_count();
    int i;

    if (!bench_case_enabled(label))
        return;

    old_prec = mf_get_default_precision();
    if (mf_set_default_precision(precision) != 0) {
        bench_fail(label, "set default precision failed");
    }

    src = mf_create_string(text);
    if (!src) {
        (void)mf_set_default_precision(old_prec);
        bench_fail(label, "source create failed");
    }

    values = (mfloat_t **)calloc((size_t)iters, sizeof(*values));
    if (!values) {
        mf_free(src);
        (void)mf_set_default_precision(old_prec);
        bench_fail(label, "work array alloc failed");
    }

    for (i = 0; i < iters; ++i) {
        values[i] = mf_clone(src);
        if (!values[i]) {
            for (int j = 0; j < i; ++j)
                mf_free(values[j]);
            free(values);
            mf_free(src);
            (void)mf_set_default_precision(old_prec);
            bench_fail(label, "work clone failed");
        }
    }

    {
        mfloat_t *warm = mf_clone(src);

        if (!warm || fn(warm) != 0) {
            mf_free(warm);
            for (i = 0; i < iters; ++i)
                mf_free(values[i]);
            free(values);
            mf_free(src);
            (void)mf_set_default_precision(old_prec);
            bench_fail(label, "warmup failed");
        }
        mf_free(warm);
    }

    samples = (double *)calloc((size_t)repeats, sizeof(*samples));
    if (!samples) {
        for (i = 0; i < iters; ++i)
            mf_free(values[i]);
        free(values);
        mf_free(src);
        (void)mf_set_default_precision(old_prec);
        bench_fail(label, "repeat sample alloc failed");
    }

    for (int repeat = 0; repeat < repeats; ++repeat) {
        uint64_t start;
        uint64_t end;
        double batch_us;

        for (i = 0; i < iters; ++i) {
            mf_free(values[i]);
            values[i] = mf_clone(src);
            if (!values[i]) {
                for (int j = 0; j < iters; ++j)
                    mf_free(values[j]);
                free(values);
                mf_free(src);
                (void)mf_set_default_precision(old_prec);
                bench_fail(label, "work clone failed");
            }
        }

        start = now_ns();
        for (i = 0; i < iters; ++i) {
            if (fn(values[i]) != 0) {
                for (int j = 0; j < iters; ++j)
                    mf_free(values[j]);
                free(values);
                mf_free(src);
                (void)mf_set_default_precision(old_prec);
                bench_fail(label, "timed run failed");
            }
        }
        end = now_ns();

        batch_us = ((double)(end - start) / (double)iters) / 1000.0;
        samples[repeat] = batch_us;
    }

    bench_print_us(label, precision, bench_estimate_double(samples, repeats));
    free(samples);

    for (i = 0; i < iters; ++i)
        mf_free(values[i]);
    free(values);
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
    mfloat_t *warm = NULL;
    mfloat_t **values = NULL;
    double *samples = NULL;
    const int repeats = bench_repeat_count();
    int i;

    if (!bench_case_enabled(label))
        return;

    old_prec = mf_get_default_precision();
    if (mf_set_default_precision(precision) != 0) {
        bench_fail(label, "set default precision failed");
    }

    lhs_src = mf_create_string(lhs_text);
    rhs = mf_create_string(rhs_text);
    warm = lhs_src ? mf_clone(lhs_src) : NULL;
    if (!lhs_src || !rhs || !warm) {
        mf_free(lhs_src);
        mf_free(rhs);
        mf_free(warm);
        (void)mf_set_default_precision(old_prec);
        bench_fail(label, "source create failed");
    }
    if (bench_verify_binary_case(label, lhs_src, rhs, fn) != 0) {
        mf_free(lhs_src);
        mf_free(rhs);
        mf_free(warm);
        (void)mf_set_default_precision(old_prec);
        bench_fail(label, "reference verification failed");
    }

    if (fn(warm, rhs) != 0) {
        mf_free(lhs_src);
        mf_free(rhs);
        mf_free(warm);
        (void)mf_set_default_precision(old_prec);
        bench_fail(label, "warmup failed");
    }

    values = (mfloat_t **)calloc((size_t)iters, sizeof(*values));
    if (!values) {
        mf_free(lhs_src);
        mf_free(rhs);
        mf_free(warm);
        (void)mf_set_default_precision(old_prec);
        bench_fail(label, "work array alloc failed");
    }
    for (i = 0; i < iters; ++i) {
        values[i] = mf_clone(lhs_src);
        if (!values[i]) {
            for (int j = 0; j < i; ++j)
                mf_free(values[j]);
            free(values);
            mf_free(lhs_src);
            mf_free(rhs);
            mf_free(warm);
            (void)mf_set_default_precision(old_prec);
            bench_fail(label, "work clone failed");
        }
    }

    samples = (double *)calloc((size_t)repeats, sizeof(*samples));
    if (!samples) {
        for (i = 0; i < iters; ++i)
            mf_free(values[i]);
        free(values);
        mf_free(lhs_src);
        mf_free(rhs);
        mf_free(warm);
        (void)mf_set_default_precision(old_prec);
        bench_fail(label, "repeat sample alloc failed");
    }

    for (int repeat = 0; repeat < repeats; ++repeat) {
        uint64_t start;
        uint64_t end;
        double batch_us;

        for (i = 0; i < iters; ++i) {
            mf_free(values[i]);
            values[i] = mf_clone(lhs_src);
            if (!values[i]) {
                for (int j = 0; j < iters; ++j)
                    mf_free(values[j]);
                free(values);
                mf_free(lhs_src);
                mf_free(rhs);
                mf_free(warm);
                (void)mf_set_default_precision(old_prec);
                bench_fail(label, "work clone failed");
            }
        }

        start = now_ns();
        for (i = 0; i < iters; ++i) {
            if (fn(values[i], rhs) != 0) {
                for (int j = 0; j < iters; ++j)
                    mf_free(values[j]);
                free(values);
                mf_free(lhs_src);
                mf_free(rhs);
                mf_free(warm);
                (void)mf_set_default_precision(old_prec);
                bench_fail(label, "timed run failed");
            }
        }
        end = now_ns();

        batch_us = ((double)(end - start) / (double)iters) / 1000.0;
        samples[repeat] = batch_us;
    }

    bench_print_us(label, precision, bench_estimate_double(samples, repeats));
    free(samples);

    mf_free(lhs_src);
    mf_free(rhs);
    mf_free(warm);
    for (i = 0; i < iters; ++i)
        mf_free(values[i]);
    free(values);
    (void)mf_set_default_precision(old_prec);
}

static void run_sincos_case(const char *label, const char *text, size_t precision, int iters)
{
    size_t old_prec;
    mfloat_t *src;
    mfloat_t *sin_value;
    mfloat_t *cos_value;
    double *samples = NULL;
    const int repeats = bench_repeat_count();

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
    sin_value = mf_new_prec(precision);
    cos_value = mf_new_prec(precision);
    if (!sin_value || !cos_value) {
        fprintf(stderr, "%s output alloc failed\n", label);
        mf_free(sin_value);
        mf_free(cos_value);
        mf_free(src);
        (void)mf_set_default_precision(old_prec);
        return;
    }

    {
        if (mf_sincos(src, sin_value, cos_value) != 0) {
            fprintf(stderr, "%s warmup failed\n", label);
            mf_free(sin_value);
            mf_free(cos_value);
            mf_free(src);
            (void)mf_set_default_precision(old_prec);
            return;
        }
    }

    samples = (double *)calloc((size_t)repeats, sizeof(*samples));
    if (!samples) {
        fprintf(stderr, "%s repeat sample alloc failed\n", label);
        mf_free(sin_value);
        mf_free(cos_value);
        mf_free(src);
        (void)mf_set_default_precision(old_prec);
        return;
    }

    for (int repeat = 0; repeat < repeats; ++repeat) {
        uint64_t start;
        uint64_t end;
        double batch_us;

        start = now_ns();
        for (int i = 0; i < iters; ++i) {
            if (mf_sincos(src, sin_value, cos_value) != 0) {
                fprintf(stderr, "%s timed run failed\n", label);
                mf_free(sin_value);
                mf_free(cos_value);
                mf_free(src);
                (void)mf_set_default_precision(old_prec);
                return;
            }
        }
        end = now_ns();

        batch_us = ((double)(end - start) / (double)iters) / 1000.0;
        samples[repeat] = batch_us;
    }

    bench_print_us(label, precision, bench_estimate_double(samples, repeats));
    free(samples);

    mf_free(sin_value);
    mf_free(cos_value);
    mf_free(src);
    (void)mf_set_default_precision(old_prec);
}

static void run_sinhcosh_case(const char *label, const char *text, size_t precision, int iters)
{
    size_t old_prec;
    mfloat_t *src;
    mfloat_t **sinh_values = NULL;
    mfloat_t **cosh_values = NULL;
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

    src = mf_create_string(text);
    if (!src) {
        fprintf(stderr, "%s source create failed\n", label);
        (void)mf_set_default_precision(old_prec);
        return;
    }

    sinh_values = (mfloat_t **)calloc((size_t)iters, sizeof(*sinh_values));
    cosh_values = (mfloat_t **)calloc((size_t)iters, sizeof(*cosh_values));
    if (!sinh_values || !cosh_values) {
        fprintf(stderr, "%s output array alloc failed\n", label);
        free(sinh_values);
        free(cosh_values);
        mf_free(src);
        (void)mf_set_default_precision(old_prec);
        return;
    }

    for (i = 0; i < iters; ++i) {
        sinh_values[i] = mf_new_prec(precision);
        cosh_values[i] = mf_new_prec(precision);
        if (!sinh_values[i] || !cosh_values[i]) {
            fprintf(stderr, "%s output alloc failed\n", label);
            for (int j = 0; j <= i; ++j) {
                mf_free(sinh_values[j]);
                mf_free(cosh_values[j]);
            }
            free(sinh_values);
            free(cosh_values);
            mf_free(src);
            (void)mf_set_default_precision(old_prec);
            return;
        }
    }

    if (mf_sinhcosh(src, sinh_values[0], cosh_values[0]) != 0) {
        fprintf(stderr, "%s warmup failed\n", label);
        for (i = 0; i < iters; ++i) {
            mf_free(sinh_values[i]);
            mf_free(cosh_values[i]);
        }
        free(sinh_values);
        free(cosh_values);
        mf_free(src);
        (void)mf_set_default_precision(old_prec);
        return;
    }

    samples = (double *)calloc((size_t)repeats, sizeof(*samples));
    if (!samples) {
        fprintf(stderr, "%s repeat sample alloc failed\n", label);
        for (i = 0; i < iters; ++i) {
            mf_free(sinh_values[i]);
            mf_free(cosh_values[i]);
        }
        free(sinh_values);
        free(cosh_values);
        mf_free(src);
        (void)mf_set_default_precision(old_prec);
        return;
    }

    for (int repeat = 0; repeat < repeats; ++repeat) {
        uint64_t start;
        uint64_t end;
        double batch_us;

        start = now_ns();
        for (i = 0; i < iters; ++i) {
            if (mf_sinhcosh(src, sinh_values[i], cosh_values[i]) != 0) {
                fprintf(stderr, "%s timed run failed\n", label);
                for (int j = 0; j < iters; ++j) {
                    mf_free(sinh_values[j]);
                    mf_free(cosh_values[j]);
                }
                free(sinh_values);
                free(cosh_values);
                mf_free(src);
                (void)mf_set_default_precision(old_prec);
                return;
            }
        }
        end = now_ns();

        batch_us = ((double)(end - start) / (double)iters) / 1000.0;
        samples[repeat] = batch_us;
    }

    bench_print_us(label, precision, bench_estimate_double(samples, repeats));
    free(samples);

    for (i = 0; i < iters; ++i) {
        mf_free(sinh_values[i]);
        mf_free(cosh_values[i]);
    }
    free(sinh_values);
    free(cosh_values);
    mf_free(src);
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
    mfloat_t **values = NULL;
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

    values = (mfloat_t **)calloc((size_t)iters, sizeof(*values));
    if (!values) {
        fprintf(stderr, "%s work array alloc failed\n", label);
        mf_free(x_src);
        mf_free(a);
        mf_free(b);
        (void)mf_set_default_precision(old_prec);
        return;
    }
    for (i = 0; i < iters; ++i) {
        values[i] = mf_clone(x_src);
        if (!values[i]) {
            fprintf(stderr, "%s work clone failed\n", label);
            for (int j = 0; j < i; ++j)
                mf_free(values[j]);
            free(values);
            mf_free(x_src);
            mf_free(a);
            mf_free(b);
            (void)mf_set_default_precision(old_prec);
            return;
        }
    }

    samples = (double *)calloc((size_t)repeats, sizeof(*samples));
    if (!samples) {
        fprintf(stderr, "%s repeat sample alloc failed\n", label);
        for (i = 0; i < iters; ++i)
            mf_free(values[i]);
        free(values);
        mf_free(x_src);
        mf_free(a);
        mf_free(b);
        (void)mf_set_default_precision(old_prec);
        return;
    }

    for (int repeat = 0; repeat < repeats; ++repeat) {
        uint64_t start;
        uint64_t end;
        double batch_us;

        for (i = 0; i < iters; ++i) {
            mf_free(values[i]);
            values[i] = mf_clone(x_src);
            if (!values[i]) {
                fprintf(stderr, "%s work clone failed\n", label);
                for (int j = 0; j < iters; ++j)
                    mf_free(values[j]);
                free(values);
                mf_free(x_src);
                mf_free(a);
                mf_free(b);
                (void)mf_set_default_precision(old_prec);
                return;
            }
        }

        start = now_ns();
        for (i = 0; i < iters; ++i) {
            if (fn(values[i], a, b) != 0) {
                fprintf(stderr, "%s timed run failed\n", label);
                for (int j = 0; j < iters; ++j)
                    mf_free(values[j]);
                free(values);
                mf_free(x_src);
                mf_free(a);
                mf_free(b);
                (void)mf_set_default_precision(old_prec);
                return;
            }
        }
        end = now_ns();

        batch_us = ((double)(end - start) / (double)iters) / 1000.0;
        samples[repeat] = batch_us;
    }

    bench_print_us(label, precision, bench_estimate_double(samples, repeats));
    free(samples);

    for (i = 0; i < iters; ++i)
        mf_free(values[i]);
    free(values);
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

    {
        mfloat_t *warm = fn();

        if (!warm) {
            fprintf(stderr, "%s warmup failed\n", label);
            (void)mf_set_default_precision(old_prec);
            return;
        }
        mf_free(warm);
    }

    samples = (double *)calloc((size_t)repeats, sizeof(*samples));
    if (!samples) {
        fprintf(stderr, "%s repeat sample alloc failed\n", label);
        (void)mf_set_default_precision(old_prec);
        return;
    }

    for (int repeat = 0; repeat < repeats; ++repeat) {
        uint64_t start;
        uint64_t end;
        double batch_us;
        mfloat_t **values = (mfloat_t **)calloc((size_t)iters, sizeof(*values));

        if (!values) {
            fprintf(stderr, "%s output array alloc failed\n", label);
            (void)mf_set_default_precision(old_prec);
            free(samples);
            return;
        }

        start = now_ns();
        for (i = 0; i < iters; ++i) {
            values[i] = fn();
            if (!values[i]) {
                fprintf(stderr, "%s timed run failed\n", label);
                for (int j = 0; j < i; ++j)
                    mf_free(values[j]);
                free(values);
                (void)mf_set_default_precision(old_prec);
                free(samples);
                return;
            }
        }
        end = now_ns();

        batch_us = ((double)(end - start) / (double)iters) / 1000.0;
        samples[repeat] = batch_us;

        for (i = 0; i < iters; ++i)
            mf_free(values[i]);
        free(values);
    }

    bench_print_us(label, precision, bench_estimate_double(samples, repeats));
    free(samples);
    (void)mf_set_default_precision(old_prec);
}

static void run_dispatch_case(const bench_dispatch_case_t *test_case, size_t precision)
{
    char label[96];

    snprintf(label, sizeof(label), "%s_%zu", test_case->label_base, precision);
    switch (test_case->kind) {
        case BENCH_DISPATCH_UNARY:
            run_unary_case(label, test_case->x_text, precision, test_case->fn.unary,
                           bench_scaled_iters(test_case->base_iters));
            break;
        case BENCH_DISPATCH_BINARY:
            run_binary_case(label, test_case->x_text, test_case->a_text, precision,
                            test_case->fn.binary, bench_scaled_iters(test_case->base_iters));
            break;
        case BENCH_DISPATCH_TERNARY:
            run_ternary_case(label, test_case->x_text, test_case->a_text, test_case->b_text,
                             precision, test_case->fn.ternary,
                             bench_scaled_iters(test_case->base_iters));
            break;
        case BENCH_DISPATCH_SINCOS:
            run_sincos_case(label, test_case->x_text, precision, bench_scaled_iters(test_case->base_iters));
            break;
        case BENCH_DISPATCH_SINHCOSH:
            run_sinhcosh_case(label, test_case->x_text, precision, bench_scaled_iters(test_case->base_iters));
            break;
    }
}

static void run_dispatch_case_table(const bench_dispatch_case_t *cases,
                                    size_t case_count,
                                    size_t precision)
{
    for (size_t i = 0u; i < case_count; ++i)
        run_dispatch_case(&cases[i], precision);
}

static const bench_dispatch_case_t bench_selected512_cases[] = {
    { "exp", BENCH_DISPATCH_UNARY, "1.23456789", NULL, NULL, 1, { .unary = mf_exp } },
    { "log", BENCH_DISPATCH_UNARY, "2.345678", NULL, NULL, 1, { .unary = mf_log } },
    { "sqrt", BENCH_DISPATCH_UNARY, "1.23456789", NULL, NULL, 1, { .unary = mf_sqrt } },
    { "sin", BENCH_DISPATCH_UNARY, "0.567", NULL, NULL, 1, { .unary = mf_sin } },
    { "cos", BENCH_DISPATCH_UNARY, "0.7", NULL, NULL, 1, { .unary = mf_cos } },
    { "sincos", BENCH_DISPATCH_SINCOS, "0.7", NULL, NULL, 1, { .unary = NULL } },
    { "tan", BENCH_DISPATCH_UNARY, "0.7", NULL, NULL, 1, { .unary = mf_tan } },
    { "atan", BENCH_DISPATCH_UNARY, "0.7", NULL, NULL, 1, { .unary = mf_atan } },
    { "asin", BENCH_DISPATCH_UNARY, "0.5", NULL, NULL, 1, { .unary = mf_asin } },
    { "asin_general", BENCH_DISPATCH_UNARY, "0.7", NULL, NULL, 1, { .unary = mf_asin } },
    { "acos", BENCH_DISPATCH_UNARY, "0.5", NULL, NULL, 1, { .unary = mf_acos } },
    { "acos_general", BENCH_DISPATCH_UNARY, "0.7", NULL, NULL, 1, { .unary = mf_acos } },
    { "atan2", BENCH_DISPATCH_BINARY, "1", "-1", NULL, 1, { .binary = mf_atan2 } },
    { "atan2_general", BENCH_DISPATCH_BINARY, "0.5", "-0.75", NULL, 1, { .binary = mf_atan2 } },
    { "sinh", BENCH_DISPATCH_UNARY, "0.7", NULL, NULL, 1, { .unary = mf_sinh } },
    { "cosh", BENCH_DISPATCH_UNARY, "0.7", NULL, NULL, 1, { .unary = mf_cosh } },
    { "sinhcosh", BENCH_DISPATCH_SINHCOSH, "0.7", NULL, NULL, 1, { .unary = NULL } },
    { "tanh", BENCH_DISPATCH_UNARY, "0.7", NULL, NULL, 1, { .unary = mf_tanh } },
    { "asinh", BENCH_DISPATCH_UNARY, "0.5", NULL, NULL, 1, { .unary = mf_asinh } },
    { "acosh", BENCH_DISPATCH_UNARY, "2.0", NULL, NULL, 1, { .unary = mf_acosh } },
    { "atanh", BENCH_DISPATCH_UNARY, "0.5", NULL, NULL, 1, { .unary = mf_atanh } },
    { "erf", BENCH_DISPATCH_UNARY, "0.567", NULL, NULL, 1, { .unary = mf_erf } },
    { "gamma", BENCH_DISPATCH_UNARY, "2.345", NULL, NULL, 1, { .unary = mf_gamma } },
    { "lgamma", BENCH_DISPATCH_UNARY, "2.345", NULL, NULL, 1, { .unary = mf_lgamma } },
    { "digamma", BENCH_DISPATCH_UNARY, "2.345", NULL, NULL, 1, { .unary = mf_digamma } },
    { "trigamma", BENCH_DISPATCH_UNARY, "2.345", NULL, NULL, 1, { .unary = mf_trigamma } },
    { "tetragamma", BENCH_DISPATCH_UNARY, "2.345", NULL, NULL, 1, { .unary = mf_tetragamma } },
    { "lambert_w0", BENCH_DISPATCH_UNARY, "0.7", NULL, NULL, 1, { .unary = mf_lambert_w0 } },
    { "lambert_wm1", BENCH_DISPATCH_UNARY, "-0.2", NULL, NULL, 1, { .unary = mf_lambert_wm1 } },
    { "pow", BENCH_DISPATCH_BINARY, "1.23456789", "3.5", NULL, 1, { .binary = mf_pow } },
    { "logbeta", BENCH_DISPATCH_BINARY, "2.5", "3.5", NULL, 1, { .binary = mf_logbeta } },
    { "beta_pdf", BENCH_DISPATCH_TERNARY, "0.5", "2.5", "3.5", 1, { .ternary = mf_beta_pdf } },
    { "normal_pdf", BENCH_DISPATCH_UNARY, "0.5", NULL, NULL, 1, { .unary = mf_normal_pdf } },
    { "ei_5", BENCH_DISPATCH_UNARY, "5", NULL, NULL, 1, { .unary = mf_ei } },
    { "e1_5", BENCH_DISPATCH_UNARY, "5", NULL, NULL, 1, { .unary = mf_e1 } }
};

int main(void)
{
    if (!bench_markdown_enabled()) {
        puts("== mfloat native math bench ==");
        puts("Scale iterations with MARS_BENCH_SCALE=<n> if you want longer runs.");
        puts("Tune repeats with MARS_BENCH_REPEATS=<n> and bootstrap resamples with MARS_BENCH_BOOTSTRAP=<n>.");
        puts("Reported timings use the sample median with MAD and a bootstrap 95% CI.");
        puts("Limit to one section with MARS_BENCH_SECTION=constants|arith|exp|log|elem256|triage256|special256|selected512|selected768|selected1024|selected2048|selected4096.");
        puts("Filter individual cases with MARS_BENCH_FILTER=<substring>.");
    }

    if (bench_wants_section("constants")) {
        if (!bench_markdown_enabled()) {
            puts("");
            puts("-- constants --");
        }
        run_const_case("pi_256", 256u, mf_pi, bench_scaled_iters(8));
        run_const_case("e_256", 256u, mf_e, bench_scaled_iters(8));
        run_const_case("gamma_256", 256u, mf_euler_mascheroni, bench_scaled_iters(6));
        run_const_case("pi_512", 512u, mf_pi, bench_scaled_iters(4));
        run_const_case("e_512", 512u, mf_e, bench_scaled_iters(4));
        run_const_case("gamma_512", 512u, mf_euler_mascheroni, bench_scaled_iters(3));
    }

    if (bench_wants_exact_section("arith")) {
        if (!bench_markdown_enabled()) {
            puts("");
            puts("-- arith --");
        }
        run_binary_case("add_256", "1.23456789", "2.345678", 256u, mf_add, bench_scaled_iters(120));
        run_binary_case("sub_256", "1.23456789", "2.345678", 256u, mf_sub, bench_scaled_iters(120));
        run_binary_case("mul_256", "1.23456789", "2.345678", 256u, mf_mul, bench_scaled_iters(120));
        run_binary_case("div_256", "1.23456789", "2.345678", 256u, mf_div, bench_scaled_iters(80));
        run_binary_case("add_512", "1.23456789", "2.345678", 512u, mf_add, bench_scaled_iters(80));
        run_binary_case("sub_512", "1.23456789", "2.345678", 512u, mf_sub, bench_scaled_iters(80));
        run_binary_case("mul_512", "1.23456789", "2.345678", 512u, mf_mul, bench_scaled_iters(80));
        run_binary_case("div_512", "1.23456789", "2.345678", 512u, mf_div, bench_scaled_iters(40));
        run_binary_case("add_768", "1.23456789", "2.345678", 768u, mf_add, bench_scaled_iters(40));
        run_binary_case("sub_768", "1.23456789", "2.345678", 768u, mf_sub, bench_scaled_iters(40));
        run_binary_case("mul_768", "1.23456789", "2.345678", 768u, mf_mul, bench_scaled_iters(40));
        run_binary_case("div_768", "1.23456789", "2.345678", 768u, mf_div, bench_scaled_iters(20));
        run_binary_case("add_1024", "1.23456789", "2.345678", 1024u, mf_add, bench_scaled_iters(20));
        run_binary_case("sub_1024", "1.23456789", "2.345678", 1024u, mf_sub, bench_scaled_iters(20));
        run_binary_case("mul_1024", "1.23456789", "2.345678", 1024u, mf_mul, bench_scaled_iters(20));
        run_binary_case("div_1024", "1.23456789", "2.345678", 1024u, mf_div, bench_scaled_iters(10));
    }

    if (bench_wants_exact_section("exp")) {
        if (!bench_markdown_enabled()) {
            puts("");
            puts("-- exp --");
        }
        run_unary_case("exp_256", "1.23456789", 256u, mf_exp, bench_scaled_iters(12));
        run_unary_case("exp_512", "1.23456789", 512u, mf_exp, bench_scaled_iters(6));
        run_unary_case("exp_768", "1.23456789", 768u, mf_exp, bench_scaled_iters(4));
        run_unary_case("exp_1024", "1.23456789", 1024u, mf_exp, bench_scaled_iters(4));
    }

    if (bench_wants_exact_section("log")) {
        if (!bench_markdown_enabled()) {
            puts("");
            puts("-- log --");
        }
        run_unary_case("log_256", "2.345678", 256u, mf_log, bench_scaled_iters(12));
        run_unary_case("log_512", "2.345678", 512u, mf_log, bench_scaled_iters(6));
        run_unary_case("log_768", "2.345678", 768u, mf_log, bench_scaled_iters(4));
        run_unary_case("log_1024", "2.345678", 1024u, mf_log, bench_scaled_iters(4));
    }

    if (bench_wants_section("elem256")) {
        if (!bench_markdown_enabled()) {
            puts("");
            puts("-- elementary 256-bit --");
        }
        run_unary_case("exp_256", "1.23456789", 256u, mf_exp, bench_scaled_iters(8));
        run_unary_case("log_256", "2.345678", 256u, mf_log, bench_scaled_iters(8));
        run_unary_case("sqrt_256", "1.23456789", 256u, mf_sqrt, bench_scaled_iters(8));
        run_unary_case("sin_256", "0.567", 256u, mf_sin, bench_scaled_iters(8));
        run_unary_case("cos_256", "0.7", 256u, mf_cos, bench_scaled_iters(8));
        run_sincos_case("sincos_256", "0.7", 256u, bench_scaled_iters(6));
        run_unary_case("tan_256", "0.7", 256u, mf_tan, bench_scaled_iters(6));
        run_unary_case("atan_256", "0.7", 256u, mf_atan, bench_scaled_iters(8));
        run_unary_case("asin_256", "0.5", 256u, mf_asin, bench_scaled_iters(4));
        run_unary_case("asin_general_256", "0.7", 256u, mf_asin, bench_scaled_iters(2));
        run_unary_case("acos_256", "0.5", 256u, mf_acos, bench_scaled_iters(4));
        run_unary_case("acos_general_256", "0.7", 256u, mf_acos, bench_scaled_iters(2));
        run_binary_case("atan2_256", "1", "-1", 256u, mf_atan2, bench_scaled_iters(4));
        run_binary_case("atan2_general_256", "0.5", "-0.75", 256u, mf_atan2, bench_scaled_iters(2));
        run_unary_case("sinh_256", "0.7", 256u, mf_sinh, bench_scaled_iters(4));
        run_unary_case("cosh_256", "0.7", 256u, mf_cosh, bench_scaled_iters(4));
        run_sinhcosh_case("sinhcosh_256", "0.7", 256u, bench_scaled_iters(3));
        run_unary_case("tanh_256", "0.7", 256u, mf_tanh, bench_scaled_iters(4));
        run_unary_case("asinh_256", "0.5", 256u, mf_asinh, bench_scaled_iters(3));
        run_unary_case("acosh_256", "2.0", 256u, mf_acosh, bench_scaled_iters(3));
        run_unary_case("atanh_256", "0.5", 256u, mf_atanh, bench_scaled_iters(3));
        run_binary_case("pow_256", "1.23456789", "3.5", 256u, mf_pow, bench_scaled_iters(4));
    }

    if (bench_wants_section("triage256")) {
        if (!bench_markdown_enabled()) {
            puts("");
            puts("-- triage 256-bit --");
        }
        run_unary_case("sinh_256", "0.7", 256u, mf_sinh, bench_scaled_iters(1));
        run_unary_case("asinh_256", "0.5", 256u, mf_asinh, bench_scaled_iters(1));
        run_unary_case("acosh_256", "2.0", 256u, mf_acosh, bench_scaled_iters(1));
        run_unary_case("atanh_256", "0.5", 256u, mf_atanh, bench_scaled_iters(1));
    }

    if (bench_wants_section("special256")) {
        if (!bench_markdown_enabled()) {
            puts("");
            puts("-- special 256-bit --");
        }
        run_unary_case("gamma_256", "2.345", 256u, mf_gamma, bench_scaled_iters(3));
        run_unary_case("lgamma_256", "2.345", 256u, mf_lgamma, bench_scaled_iters(3));
        run_unary_case("digamma_256", "2.345", 256u, mf_digamma, bench_scaled_iters(3));
        run_unary_case("trigamma_256", "2.345", 256u, mf_trigamma, bench_scaled_iters(3));
        run_unary_case("tetragamma_256", "2.345", 256u, mf_tetragamma, bench_scaled_iters(2));
        run_unary_case("erf_256", "0.567", 256u, mf_erf, bench_scaled_iters(4));
        run_unary_case("erfc_256", "0.5", 256u, mf_erfc, bench_scaled_iters(4));
        run_unary_case("erfinv_256", "0.5", 256u, mf_erfinv, bench_scaled_iters(2));
        run_unary_case("erfcinv_256", "0.5", 256u, mf_erfcinv, bench_scaled_iters(2));
        run_unary_case("gammainv_256", "3", 256u, mf_gammainv, bench_scaled_iters(2));
        run_unary_case("lambert_w0_256", "0.7", 256u, mf_lambert_w0, bench_scaled_iters(3));
        run_unary_case("lambert_wm1_256", "-0.2", 256u, mf_lambert_wm1, bench_scaled_iters(2));
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
        run_unary_case("ei_5_256", "5", 256u, mf_ei, bench_scaled_iters(2));
        run_unary_case("e1_5_256", "5", 256u, mf_e1, bench_scaled_iters(2));
    }

    if (bench_wants_section("selected512")) {
        if (!bench_markdown_enabled()) {
            puts("");
            puts("-- selected 512-bit --");
        }
        run_dispatch_case_table(bench_selected512_cases,
                                sizeof(bench_selected512_cases) / sizeof(bench_selected512_cases[0]),
                                512u);
    }

    if (bench_wants_section("selected768")) {
        if (!bench_markdown_enabled()) {
            puts("");
            puts("-- selected 768-bit --");
        }
        run_dispatch_case_table(bench_selected512_cases,
                                sizeof(bench_selected512_cases) / sizeof(bench_selected512_cases[0]),
                                768u);
    }

    if (bench_wants_section("selected1024")) {
        if (!bench_markdown_enabled()) {
            puts("");
            puts("-- selected 1024-bit --");
        }
        run_dispatch_case_table(bench_selected512_cases,
                                sizeof(bench_selected512_cases) / sizeof(bench_selected512_cases[0]),
                                1024u);
    }

    if (bench_wants_section("selected2048")) {
        if (!bench_markdown_enabled()) {
            puts("");
            puts("-- selected 2048-bit --");
        }
        run_dispatch_case_table(bench_selected512_cases,
                                sizeof(bench_selected512_cases) / sizeof(bench_selected512_cases[0]),
                                2048u);
    }

    if (bench_wants_section("selected4096")) {
        if (!bench_markdown_enabled()) {
            puts("");
            puts("-- selected 4096-bit --");
        }
        run_dispatch_case_table(bench_selected512_cases,
                                sizeof(bench_selected512_cases) / sizeof(bench_selected512_cases[0]),
                                4096u);
    }

    bench_print_markdown_table();
    return EXIT_SUCCESS;
}
