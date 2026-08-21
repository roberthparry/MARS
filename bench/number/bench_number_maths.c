#include <math.h>
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

static const bench_md_row_t bench_md_rows[] = {{"exp", "num_exp(1.23456789)"},
                                               {"log", "num_log(2.345678)"},
                                               {"sqrt", "num_sqrt(1.23456789)"},
                                               {"sin", "num_sin(0.567)"},
                                               {"cos", "num_cos(0.7)"},
                                               {"sincos", "num_sincos(0.7)"},
                                               {"tan", "num_tan(0.7)"},
                                               {"atan", "num_atan(0.7)"},
                                               {"asin_general", "num_asin(0.7)"},
                                               {"acos_general", "num_acos(0.7)"},
                                               {"atan2_general", "num_atan2(0.5,-0.75)"},
                                               {"sinh", "num_sinh(0.7)"},
                                               {"cosh", "num_cosh(0.7)"},
                                               {"sinhcosh", "num_sinhcosh(0.7)"},
                                               {"tanh", "num_tanh(0.7)"},
                                               {"asinh", "num_asinh(0.5)"},
                                               {"acosh", "num_acosh(2)"},
                                               {"atanh", "num_atanh(0.5)"},
                                               {"lambert_w0", "num_lambert_w0(0.7)"},
                                               {"lambert_wm1", "num_lambert_wm1(-0.2)"},
                                               {"gamma", "num_gamma(2.345)"},
                                               {"lgamma", "num_lgamma(2.345)"},
                                               {"digamma", "num_digamma(2.345)"},
                                               {"trigamma", "num_trigamma(2.345)"},
                                               {"tetragamma", "num_tetragamma(2.345)"},
                                               {"ei_5", "num_Ei(5)"},
                                               {"e1_5", "num_E1(5)"}};

static int bench_markdown_enabled(void);
static int bench_doc_iters(int base_iters);

static number_t bench_num_pi(void)
{
    return num_clone(NUM_PI);
}
static number_t bench_num_e(void)
{
    return num_clone(NUM_E);
}
static number_t bench_num_euler_mascheroni(void)
{
    return num_clone(NUM_EULER_MASCHERONI);
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

static int bench_markdown_enabled(void)
{
    const char *format = getenv("MARS_BENCH_FORMAT");

    return format && strcmp(format, "md") == 0;
}

static void bench_print_us(const char *label, size_t precision, bench_stats_t stats)
{
    const char *format = getenv("MARS_BENCH_FORMAT");

    if (format && strcmp(format, "md") == 0) {
        if (bench_result_count < sizeof(bench_results) / sizeof(bench_results[0])) {
            snprintf(bench_results[bench_result_count].label, sizeof(bench_results[bench_result_count].label), "%s",
                     label);
            bench_results[bench_result_count].precision = precision;
            bench_results[bench_result_count].avg_us = stats.estimate;
            ++bench_result_count;
        }
        return;
    }

    printf("%-28s bits=%-4zu med_µs=%10.3f mad_µs=%9.3f ci95=[%9.3f,%9.3f] n=%d\n", label, precision, stats.estimate,
           stats.mad, stats.ci_low, stats.ci_high, stats.samples);
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
    static const size_t precisions[] = {256u, 512u, 768u, 1024u, 2048u, 4096u};

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
            } else
                printf(" - |");
        }
        putchar('\n');
    }
}

static void bench_destroy_number_array(number_t *values, int count)
{
    if (!values)
        return;
    for (int i = 0; i < count; ++i)
        num_destroy(&values[i]);
    free(values);
}

static void run_unary_case(const char *label, const char *text, size_t precision, number_unary_fn fn, int iters)
{
    size_t old_prec;
    double *samples = NULL;
    number_t src;
    number_t value;
    const int repeats = bench_repeat_count();

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

    samples = (double *)calloc((size_t)repeats, sizeof(*samples));
    if (!samples) {
        fprintf(stderr, "%s repeat sample alloc failed\n", label);
        num_destroy(&src);
        (void)num_set_default_prec_bits(old_prec);
        return;
    }

    for (int repeat = 0; repeat < repeats; ++repeat) {
        uint64_t start;
        uint64_t end;
        double batch_us;
        number_t *values = (number_t *)calloc((size_t)iters, sizeof(*values));

        if (!values) {
            fprintf(stderr, "%s output array alloc failed\n", label);
            num_destroy(&src);
            (void)num_set_default_prec_bits(old_prec);
            return;
        }

        start = now_ns();
        for (int i = 0; i < iters; ++i)
            values[i] = fn(src);
        end = now_ns();

        batch_us = ((double)(end - start) / (double)iters) / 1000.0;
        samples[repeat] = batch_us;
        bench_destroy_number_array(values, iters);
    }

    bench_print_us(label, precision, bench_estimate_double(samples, repeats));
    free(samples);
    num_destroy(&src);
    (void)num_set_default_prec_bits(old_prec);
}

static void run_binary_case(const char *label, const char *lhs_text, const char *rhs_text, size_t precision,
                            number_binary_fn fn, int iters)
{
    size_t old_prec;
    double *samples = NULL;
    number_t lhs;
    number_t rhs;
    number_t value;
    const int repeats = bench_repeat_count();

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

    samples = (double *)calloc((size_t)repeats, sizeof(*samples));
    if (!samples) {
        fprintf(stderr, "%s repeat sample alloc failed\n", label);
        num_destroy(&lhs);
        num_destroy(&rhs);
        (void)num_set_default_prec_bits(old_prec);
        return;
    }

    for (int repeat = 0; repeat < repeats; ++repeat) {
        uint64_t start;
        uint64_t end;
        double batch_us;
        number_t *values = (number_t *)calloc((size_t)iters, sizeof(*values));

        if (!values) {
            fprintf(stderr, "%s output array alloc failed\n", label);
            num_destroy(&lhs);
            num_destroy(&rhs);
            (void)num_set_default_prec_bits(old_prec);
            return;
        }

        start = now_ns();
        for (int i = 0; i < iters; ++i)
            values[i] = fn(lhs, rhs);
        end = now_ns();

        batch_us = ((double)(end - start) / (double)iters) / 1000.0;
        samples[repeat] = batch_us;
        bench_destroy_number_array(values, iters);
    }

    bench_print_us(label, precision, bench_estimate_double(samples, repeats));
    free(samples);
    num_destroy(&lhs);
    num_destroy(&rhs);
    (void)num_set_default_prec_bits(old_prec);
}

static void run_sincos_case(const char *label, const char *text, size_t precision, int iters)
{
    size_t old_prec;
    double *samples = NULL;
    number_t src;
    number_t sin_value = num_new();
    number_t cos_value = num_new();
    const int repeats = bench_repeat_count();

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

    samples = (double *)calloc((size_t)repeats, sizeof(*samples));
    if (!samples) {
        fprintf(stderr, "%s repeat sample alloc failed\n", label);
        num_destroy(&src);
        (void)num_set_default_prec_bits(old_prec);
        return;
    }

    for (int repeat = 0; repeat < repeats; ++repeat) {
        uint64_t start;
        uint64_t end;
        double batch_us;
        number_t *sin_values = (number_t *)calloc((size_t)iters, sizeof(*sin_values));
        number_t *cos_values = (number_t *)calloc((size_t)iters, sizeof(*cos_values));

        if (!sin_values || !cos_values) {
            fprintf(stderr, "%s output array alloc failed\n", label);
            free(sin_values);
            free(cos_values);
            num_destroy(&src);
            (void)num_set_default_prec_bits(old_prec);
            return;
        }

        start = now_ns();
        for (int i = 0; i < iters; ++i) {
            if (num_sincos(src, &sin_values[i], &cos_values[i]) != 0) {
                fprintf(stderr, "%s timed run failed\n", label);
                bench_destroy_number_array(sin_values, iters);
                bench_destroy_number_array(cos_values, iters);
                num_destroy(&src);
                (void)num_set_default_prec_bits(old_prec);
                return;
            }
        }
        end = now_ns();

        batch_us = ((double)(end - start) / (double)iters) / 1000.0;
        samples[repeat] = batch_us;
        bench_destroy_number_array(sin_values, iters);
        bench_destroy_number_array(cos_values, iters);
    }

    bench_print_us(label, precision, bench_estimate_double(samples, repeats));
    free(samples);
    num_destroy(&src);
    (void)num_set_default_prec_bits(old_prec);
}

static void run_sinhcosh_case(const char *label, const char *text, size_t precision, int iters)
{
    size_t old_prec;
    double *samples = NULL;
    number_t src;
    number_t sinh_value = num_new();
    number_t cosh_value = num_new();
    const int repeats = bench_repeat_count();

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

    samples = (double *)calloc((size_t)repeats, sizeof(*samples));
    if (!samples) {
        fprintf(stderr, "%s repeat sample alloc failed\n", label);
        num_destroy(&src);
        (void)num_set_default_prec_bits(old_prec);
        return;
    }

    for (int repeat = 0; repeat < repeats; ++repeat) {
        uint64_t start;
        uint64_t end;
        double batch_us;
        number_t *sinh_values = (number_t *)calloc((size_t)iters, sizeof(*sinh_values));
        number_t *cosh_values = (number_t *)calloc((size_t)iters, sizeof(*cosh_values));

        if (!sinh_values || !cosh_values) {
            fprintf(stderr, "%s output array alloc failed\n", label);
            free(sinh_values);
            free(cosh_values);
            num_destroy(&src);
            (void)num_set_default_prec_bits(old_prec);
            return;
        }

        start = now_ns();
        for (int i = 0; i < iters; ++i) {
            if (num_sinhcosh(src, &sinh_values[i], &cosh_values[i]) != 0) {
                fprintf(stderr, "%s timed run failed\n", label);
                bench_destroy_number_array(sinh_values, iters);
                bench_destroy_number_array(cosh_values, iters);
                num_destroy(&src);
                (void)num_set_default_prec_bits(old_prec);
                return;
            }
        }
        end = now_ns();

        batch_us = ((double)(end - start) / (double)iters) / 1000.0;
        samples[repeat] = batch_us;
        bench_destroy_number_array(sinh_values, iters);
        bench_destroy_number_array(cosh_values, iters);
    }

    bench_print_us(label, precision, bench_estimate_double(samples, repeats));
    free(samples);
    num_destroy(&src);
    (void)num_set_default_prec_bits(old_prec);
}

static void run_ternary_case(const char *label, const char *x_text, const char *a_text, const char *b_text,
                             size_t precision, number_ternary_fn fn, int iters)
{
    size_t old_prec;
    double *samples = NULL;
    number_t x;
    number_t a;
    number_t b;
    number_t value;
    const int repeats = bench_repeat_count();

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

    samples = (double *)calloc((size_t)repeats, sizeof(*samples));
    if (!samples) {
        fprintf(stderr, "%s repeat sample alloc failed\n", label);
        num_destroy(&x);
        num_destroy(&a);
        num_destroy(&b);
        (void)num_set_default_prec_bits(old_prec);
        return;
    }

    for (int repeat = 0; repeat < repeats; ++repeat) {
        uint64_t start;
        uint64_t end;
        double batch_us;
        number_t *values = (number_t *)calloc((size_t)iters, sizeof(*values));

        if (!values) {
            fprintf(stderr, "%s output array alloc failed\n", label);
            num_destroy(&x);
            num_destroy(&a);
            num_destroy(&b);
            (void)num_set_default_prec_bits(old_prec);
            return;
        }

        start = now_ns();
        for (int i = 0; i < iters; ++i)
            values[i] = fn(x, a, b);
        end = now_ns();

        batch_us = ((double)(end - start) / (double)iters) / 1000.0;
        samples[repeat] = batch_us;
        bench_destroy_number_array(values, iters);
    }

    bench_print_us(label, precision, bench_estimate_double(samples, repeats));
    free(samples);
    num_destroy(&x);
    num_destroy(&a);
    num_destroy(&b);
    (void)num_set_default_prec_bits(old_prec);
}

static void run_const_case(const char *label, size_t precision, number_const_fn fn, int iters)
{
    size_t old_prec;
    double *samples = NULL;
    number_t value;
    const int repeats = bench_repeat_count();

    if (!bench_case_enabled(label))
        return;

    old_prec = num_get_default_prec_bits();
    if (num_set_default_prec_bits(precision) != 0) {
        fprintf(stderr, "%s set default precision failed\n", label);
        return;
    }

    value = fn();
    num_destroy(&value);

    samples = (double *)calloc((size_t)repeats, sizeof(*samples));
    if (!samples) {
        fprintf(stderr, "%s repeat sample alloc failed\n", label);
        (void)num_set_default_prec_bits(old_prec);
        return;
    }

    for (int repeat = 0; repeat < repeats; ++repeat) {
        uint64_t start;
        uint64_t end;
        double batch_us;
        number_t *values = (number_t *)calloc((size_t)iters, sizeof(*values));

        if (!values) {
            fprintf(stderr, "%s output array alloc failed\n", label);
            (void)num_set_default_prec_bits(old_prec);
            return;
        }

        start = now_ns();
        for (int i = 0; i < iters; ++i)
            values[i] = fn();
        end = now_ns();

        batch_us = ((double)(end - start) / (double)iters) / 1000.0;
        samples[repeat] = batch_us;
        bench_destroy_number_array(values, iters);
    }

    bench_print_us(label, precision, bench_estimate_double(samples, repeats));
    free(samples);
    (void)num_set_default_prec_bits(old_prec);
}

#define RUN_NUMBER_SELECTED_ONES(PREC)                                                                                 \
    run_unary_case("exp_" #PREC, "1.23456789", PREC##u, num_exp, bench_scaled_iters(1));                               \
    run_unary_case("log_" #PREC, "2.345678", PREC##u, num_log, bench_scaled_iters(1));                                 \
    run_unary_case("sqrt_" #PREC, "1.23456789", PREC##u, num_sqrt, bench_scaled_iters(1));                             \
    run_unary_case("sin_" #PREC, "0.567", PREC##u, num_sin, bench_scaled_iters(1));                                    \
    run_unary_case("cos_" #PREC, "0.7", PREC##u, num_cos, bench_scaled_iters(1));                                      \
    run_sincos_case("sincos_" #PREC, "0.7", PREC##u, bench_scaled_iters(1));                                           \
    run_unary_case("tan_" #PREC, "0.7", PREC##u, num_tan, bench_scaled_iters(1));                                      \
    run_unary_case("atan_" #PREC, "0.7", PREC##u, num_atan, bench_scaled_iters(1));                                    \
    run_unary_case("asin_" #PREC, "0.5", PREC##u, num_asin, bench_scaled_iters(1));                                    \
    run_unary_case("asin_general_" #PREC, "0.7", PREC##u, num_asin, bench_scaled_iters(1));                            \
    run_unary_case("acos_" #PREC, "0.5", PREC##u, num_acos, bench_scaled_iters(1));                                    \
    run_unary_case("acos_general_" #PREC, "0.7", PREC##u, num_acos, bench_scaled_iters(1));                            \
    run_binary_case("atan2_" #PREC, "1", "-1", PREC##u, num_atan2, bench_scaled_iters(1));                             \
    run_binary_case("atan2_general_" #PREC, "0.5", "-0.75", PREC##u, num_atan2, bench_scaled_iters(1));                \
    run_unary_case("sinh_" #PREC, "0.7", PREC##u, num_sinh, bench_scaled_iters(1));                                    \
    run_unary_case("cosh_" #PREC, "0.7", PREC##u, num_cosh, bench_scaled_iters(1));                                    \
    run_sinhcosh_case("sinhcosh_" #PREC, "0.7", PREC##u, bench_scaled_iters(1));                                       \
    run_unary_case("tanh_" #PREC, "0.7", PREC##u, num_tanh, bench_scaled_iters(1));                                    \
    run_unary_case("asinh_" #PREC, "0.5", PREC##u, num_asinh, bench_scaled_iters(1));                                  \
    run_unary_case("acosh_" #PREC, "2.0", PREC##u, num_acosh, bench_scaled_iters(1));                                  \
    run_unary_case("atanh_" #PREC, "0.5", PREC##u, num_atanh, bench_scaled_iters(1));                                  \
    run_unary_case("erf_" #PREC, "0.567", PREC##u, num_erf, bench_scaled_iters(1));                                    \
    run_unary_case("gamma_" #PREC, "2.345", PREC##u, num_gamma, bench_scaled_iters(1));                                \
    run_unary_case("lgamma_" #PREC, "2.345", PREC##u, num_lgamma, bench_scaled_iters(1));                              \
    run_unary_case("digamma_" #PREC, "2.345", PREC##u, num_digamma, bench_scaled_iters(1));                            \
    run_unary_case("trigamma_" #PREC, "2.345", PREC##u, num_trigamma, bench_scaled_iters(1));                          \
    run_unary_case("tetragamma_" #PREC, "2.345", PREC##u, num_tetragamma, bench_scaled_iters(1));                      \
    run_unary_case("lambert_w0_" #PREC, "0.7", PREC##u, num_lambert_w0, bench_scaled_iters(1));                        \
    run_unary_case("lambert_wm1_" #PREC, "-0.2", PREC##u, num_lambert_wm1, bench_scaled_iters(1));                     \
    run_binary_case("pow_" #PREC, "1.23456789", "3.5", PREC##u, num_pow, bench_scaled_iters(1));                       \
    run_binary_case("logbeta_" #PREC, "2.5", "3.5", PREC##u, num_logbeta, bench_scaled_iters(1));                      \
    run_ternary_case("beta_pdf_" #PREC, "0.5", "2.5", "3.5", PREC##u, num_beta_pdf, bench_scaled_iters(1));            \
    run_unary_case("normal_pdf_" #PREC, "0.5", PREC##u, num_normal_pdf, bench_scaled_iters(1));                        \
    run_unary_case("ei_5_" #PREC, "5", PREC##u, num_Ei, bench_scaled_iters(1));                                        \
    run_unary_case("e1_5_" #PREC, "5", PREC##u, num_E1, bench_scaled_iters(1))

int main(void)
{
    if (!bench_markdown_enabled()) {
        puts("== number maths bench ==");
        puts("Scale iterations with MARS_BENCH_SCALE=<n> if you want longer runs.");
        puts("Tune repeats with MARS_BENCH_REPEATS=<n> and bootstrap resamples with MARS_BENCH_BOOTSTRAP=<n>.");
        puts("Reported timings use the sample median with MAD and a bootstrap 95% CI.");
        puts("Limit to one section with "
             "MARS_BENCH_SECTION=constants|exp|log|elem256|triage256|special256|selected512|selected768|selected1024|"
             "selected2048|selected4096.");
        puts("Filter individual cases with MARS_BENCH_FILTER=<substring>.");
    }

    if (bench_wants_section("constants")) {
        if (!bench_markdown_enabled()) {
            puts("");
            puts("-- constants --");
        }
        run_const_case("pi_256", 256u, bench_num_pi, bench_scaled_iters(8));
        run_const_case("e_256", 256u, bench_num_e, bench_scaled_iters(8));
        run_const_case("gamma_256", 256u, bench_num_euler_mascheroni, bench_scaled_iters(6));
        run_const_case("pi_512", 512u, bench_num_pi, bench_scaled_iters(4));
        run_const_case("e_512", 512u, bench_num_e, bench_scaled_iters(4));
        run_const_case("gamma_512", 512u, bench_num_euler_mascheroni, bench_scaled_iters(3));
    }

    if (bench_wants_section("exp")) {
        if (!bench_markdown_enabled()) {
            puts("");
            puts("-- exp --");
        }
        run_unary_case("exp_256", "1.23456789", 256u, num_exp, bench_scaled_iters(12));
        run_unary_case("exp_512", "1.23456789", 512u, num_exp, bench_scaled_iters(6));
        run_unary_case("exp_768", "1.23456789", 768u, num_exp, bench_scaled_iters(4));
        run_unary_case("exp_1024", "1.23456789", 1024u, num_exp, bench_scaled_iters(4));
    }

    if (bench_wants_section("log")) {
        if (!bench_markdown_enabled()) {
            puts("");
            puts("-- log --");
        }
        run_unary_case("log_256", "2.345678", 256u, num_log, bench_scaled_iters(12));
        run_unary_case("log_512", "2.345678", 512u, num_log, bench_scaled_iters(6));
        run_unary_case("log_768", "2.345678", 768u, num_log, bench_scaled_iters(4));
        run_unary_case("log_1024", "2.345678", 1024u, num_log, bench_scaled_iters(4));
    }

    if (bench_wants_section("elem256")) {
        if (!bench_markdown_enabled()) {
            puts("");
            puts("-- elementary 256-bit --");
        }
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
        if (!bench_markdown_enabled()) {
            puts("");
            puts("-- triage 256-bit --");
        }
        run_unary_case("sinh_256", "0.7", 256u, num_sinh, bench_scaled_iters(1));
        run_unary_case("asinh_256", "0.5", 256u, num_asinh, bench_scaled_iters(1));
        run_unary_case("acosh_256", "2.0", 256u, num_acosh, bench_scaled_iters(1));
        run_unary_case("atanh_256", "0.5", 256u, num_atanh, bench_scaled_iters(1));
    }

    if (bench_wants_section("special256")) {
        if (!bench_markdown_enabled()) {
            puts("");
            puts("-- special 256-bit --");
        }
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
        run_unary_case("ei_5_256", "5", 256u, num_Ei, bench_scaled_iters(2));
        run_unary_case("e1_5_256", "5", 256u, num_E1, bench_scaled_iters(2));
    }

    if (bench_wants_section("selected512")) {
        if (!bench_markdown_enabled()) {
            puts("");
            puts("-- selected 512-bit --");
        }
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
        run_unary_case("ei_5_512", "5", 512u, num_Ei, bench_scaled_iters(1));
        run_unary_case("e1_5_512", "5", 512u, num_E1, bench_scaled_iters(1));
    }

    if (bench_wants_section("selected768")) {
        if (!bench_markdown_enabled()) {
            puts("");
            puts("-- selected 768-bit --");
        }
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
        run_unary_case("ei_5_768", "5", 768u, num_Ei, bench_scaled_iters(1));
        run_unary_case("e1_5_768", "5", 768u, num_E1, bench_scaled_iters(1));
        run_unary_case("lambert_w0_768", "0.7", 768u, num_lambert_w0, bench_scaled_iters(1));
        run_unary_case("lambert_wm1_768", "-0.2", 768u, num_lambert_wm1, bench_scaled_iters(1));
    }

    if (bench_wants_section("selected1024")) {
        if (!bench_markdown_enabled()) {
            puts("");
            puts("-- selected 1024-bit --");
        }
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
        run_unary_case("ei_5_1024", "5", 1024u, num_Ei, bench_scaled_iters(1));
        run_unary_case("e1_5_1024", "5", 1024u, num_E1, bench_scaled_iters(1));
    }

    if (bench_wants_section("selected2048")) {
        if (!bench_markdown_enabled()) {
            puts("");
            puts("-- selected 2048-bit --");
        }
        run_unary_case("exp_2048", "1.23456789", 2048u, num_exp, bench_scaled_iters(1));
        run_unary_case("log_2048", "2.345678", 2048u, num_log, bench_scaled_iters(1));
        run_unary_case("sqrt_2048", "1.23456789", 2048u, num_sqrt, bench_scaled_iters(1));
        run_unary_case("sin_2048", "0.567", 2048u, num_sin, bench_scaled_iters(1));
        run_unary_case("cos_2048", "0.7", 2048u, num_cos, bench_scaled_iters(1));
        run_sincos_case("sincos_2048", "0.7", 2048u, bench_scaled_iters(1));
        run_unary_case("tan_2048", "0.7", 2048u, num_tan, bench_scaled_iters(1));
        run_unary_case("atan_2048", "0.7", 2048u, num_atan, bench_scaled_iters(1));
        run_unary_case("asin_2048", "0.5", 2048u, num_asin, bench_scaled_iters(1));
        run_unary_case("asin_general_2048", "0.7", 2048u, num_asin, bench_scaled_iters(1));
        run_unary_case("acos_2048", "0.5", 2048u, num_acos, bench_scaled_iters(1));
        run_unary_case("acos_general_2048", "0.7", 2048u, num_acos, bench_scaled_iters(1));
        run_binary_case("atan2_2048", "1", "-1", 2048u, num_atan2, bench_scaled_iters(1));
        run_binary_case("atan2_general_2048", "0.5", "-0.75", 2048u, num_atan2, bench_scaled_iters(1));
        run_unary_case("sinh_2048", "0.7", 2048u, num_sinh, bench_scaled_iters(1));
        run_unary_case("cosh_2048", "0.7", 2048u, num_cosh, bench_scaled_iters(1));
        run_sinhcosh_case("sinhcosh_2048", "0.7", 2048u, bench_scaled_iters(1));
        run_unary_case("tanh_2048", "0.7", 2048u, num_tanh, bench_scaled_iters(1));
        run_unary_case("asinh_2048", "0.5", 2048u, num_asinh, bench_scaled_iters(1));
        run_unary_case("acosh_2048", "2.0", 2048u, num_acosh, bench_scaled_iters(1));
        run_unary_case("atanh_2048", "0.5", 2048u, num_atanh, bench_scaled_iters(1));
        run_unary_case("erf_2048", "0.567", 2048u, num_erf, bench_scaled_iters(1));
        run_unary_case("gamma_2048", "2.345", 2048u, num_gamma, bench_scaled_iters(1));
        run_unary_case("lgamma_2048", "2.345", 2048u, num_lgamma, bench_scaled_iters(1));
        run_unary_case("digamma_2048", "2.345", 2048u, num_digamma, bench_scaled_iters(1));
        run_unary_case("trigamma_2048", "2.345", 2048u, num_trigamma, bench_scaled_iters(1));
        run_unary_case("tetragamma_2048", "2.345", 2048u, num_tetragamma, bench_scaled_iters(1));
        run_unary_case("lambert_w0_2048", "0.7", 2048u, num_lambert_w0, bench_scaled_iters(1));
        run_unary_case("lambert_wm1_2048", "-0.2", 2048u, num_lambert_wm1, bench_scaled_iters(1));
        run_binary_case("pow_2048", "1.23456789", "3.5", 2048u, num_pow, bench_scaled_iters(1));
        run_binary_case("logbeta_2048", "2.5", "3.5", 2048u, num_logbeta, bench_scaled_iters(1));
        run_ternary_case("beta_pdf_2048", "0.5", "2.5", "3.5", 2048u, num_beta_pdf, bench_scaled_iters(1));
        run_unary_case("normal_pdf_2048", "0.5", 2048u, num_normal_pdf, bench_scaled_iters(1));
        run_unary_case("ei_5_2048", "5", 2048u, num_Ei, bench_scaled_iters(1));
        run_unary_case("e1_5_2048", "5", 2048u, num_E1, bench_scaled_iters(1));
    }

    if (bench_wants_section("selected4096")) {
        if (!bench_markdown_enabled()) {
            puts("");
            puts("-- selected 4096-bit --");
        }
        run_unary_case("exp_4096", "1.23456789", 4096u, num_exp, bench_scaled_iters(1));
        run_unary_case("log_4096", "2.345678", 4096u, num_log, bench_scaled_iters(1));
        run_unary_case("sqrt_4096", "1.23456789", 4096u, num_sqrt, bench_scaled_iters(1));
        run_unary_case("sin_4096", "0.567", 4096u, num_sin, bench_scaled_iters(1));
        run_unary_case("cos_4096", "0.7", 4096u, num_cos, bench_scaled_iters(1));
        run_sincos_case("sincos_4096", "0.7", 4096u, bench_scaled_iters(1));
        run_unary_case("tan_4096", "0.7", 4096u, num_tan, bench_scaled_iters(1));
        run_unary_case("atan_4096", "0.7", 4096u, num_atan, bench_scaled_iters(1));
        run_unary_case("asin_4096", "0.5", 4096u, num_asin, bench_scaled_iters(1));
        run_unary_case("asin_general_4096", "0.7", 4096u, num_asin, bench_scaled_iters(1));
        run_unary_case("acos_4096", "0.5", 4096u, num_acos, bench_scaled_iters(1));
        run_unary_case("acos_general_4096", "0.7", 4096u, num_acos, bench_scaled_iters(1));
        run_binary_case("atan2_4096", "1", "-1", 4096u, num_atan2, bench_scaled_iters(1));
        run_binary_case("atan2_general_4096", "0.5", "-0.75", 4096u, num_atan2, bench_scaled_iters(1));
        run_unary_case("sinh_4096", "0.7", 4096u, num_sinh, bench_scaled_iters(1));
        run_unary_case("cosh_4096", "0.7", 4096u, num_cosh, bench_scaled_iters(1));
        run_sinhcosh_case("sinhcosh_4096", "0.7", 4096u, bench_scaled_iters(1));
        run_unary_case("tanh_4096", "0.7", 4096u, num_tanh, bench_scaled_iters(1));
        run_unary_case("asinh_4096", "0.5", 4096u, num_asinh, bench_scaled_iters(1));
        run_unary_case("acosh_4096", "2.0", 4096u, num_acosh, bench_scaled_iters(1));
        run_unary_case("atanh_4096", "0.5", 4096u, num_atanh, bench_scaled_iters(1));
        run_unary_case("erf_4096", "0.567", 4096u, num_erf, bench_scaled_iters(1));
        run_unary_case("gamma_4096", "2.345", 4096u, num_gamma, bench_scaled_iters(1));
        run_unary_case("lgamma_4096", "2.345", 4096u, num_lgamma, bench_scaled_iters(1));
        run_unary_case("digamma_4096", "2.345", 4096u, num_digamma, bench_scaled_iters(1));
        run_unary_case("trigamma_4096", "2.345", 4096u, num_trigamma, bench_scaled_iters(1));
        run_unary_case("tetragamma_4096", "2.345", 4096u, num_tetragamma, bench_scaled_iters(1));
        run_unary_case("lambert_w0_4096", "0.7", 4096u, num_lambert_w0, bench_scaled_iters(1));
        run_unary_case("lambert_wm1_4096", "-0.2", 4096u, num_lambert_wm1, bench_scaled_iters(1));
        run_binary_case("pow_4096", "1.23456789", "3.5", 4096u, num_pow, bench_scaled_iters(1));
        run_binary_case("logbeta_4096", "2.5", "3.5", 4096u, num_logbeta, bench_scaled_iters(1));
        run_ternary_case("beta_pdf_4096", "0.5", "2.5", "3.5", 4096u, num_beta_pdf, bench_scaled_iters(1));
        run_unary_case("normal_pdf_4096", "0.5", 4096u, num_normal_pdf, bench_scaled_iters(1));
        run_unary_case("ei_5_4096", "5", 4096u, num_Ei, bench_scaled_iters(1));
        run_unary_case("e1_5_4096", "5", 4096u, num_E1, bench_scaled_iters(1));
    }

    bench_print_markdown_table();
    return 0;
}
