#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "number/number_internal.h"

typedef number_t (*bench_unary_fn)(number_t);
typedef number_t (*bench_binary_fn)(number_t, number_t);

typedef enum bench_case_kind_t {
    BENCH_CASE_UNARY,
    BENCH_CASE_BINARY
} bench_case_kind_t;

typedef struct bench_case_t {
    const char *label;
    const char *display;
    const char *lhs_text;
    const char *rhs_text;
    int base_iters;
    bench_case_kind_t kind;
    union {
        bench_unary_fn unary;
        bench_binary_fn binary;
    } fn;
} bench_case_t;

typedef struct bench_stats_t {
    double median_us;
    double min_us;
    double max_us;
} bench_stats_t;

static volatile size_t bench_sink = 0u;

static const bench_case_t bench_cases[] = {
    {"exp", "exp(0.567 + 0.321i)", "0.567 + 0.321i", NULL, 20, BENCH_CASE_UNARY, {.unary = num_exp}},
    {"log", "log(0.567 + 0.321i)", "0.567 + 0.321i", NULL, 20, BENCH_CASE_UNARY, {.unary = num_log}},
    {"sqrt", "sqrt(0.567 + 0.321i)", "0.567 + 0.321i", NULL, 80, BENCH_CASE_UNARY, {.unary = num_sqrt}},
    {"sin", "sin(0.567 + 0.321i)", "0.567 + 0.321i", NULL, 20, BENCH_CASE_UNARY, {.unary = num_sin}},
    {"cos", "cos(0.567 + 0.321i)", "0.567 + 0.321i", NULL, 20, BENCH_CASE_UNARY, {.unary = num_cos}},
    {"tan", "tan(0.567 + 0.321i)", "0.567 + 0.321i", NULL, 20, BENCH_CASE_UNARY, {.unary = num_tan}},
    {"atan", "atan(0.321 + 0.123i)", "0.321 + 0.123i", NULL, 8, BENCH_CASE_UNARY, {.unary = num_atan}},
    {"asin", "asin(0.321 + 0.123i)", "0.321 + 0.123i", NULL, 2, BENCH_CASE_UNARY, {.unary = num_asin}},
    {"acos", "acos(0.321 + 0.123i)", "0.321 + 0.123i", NULL, 2, BENCH_CASE_UNARY, {.unary = num_acos}},
    {"atan2", "atan2(0.5 + 0.25i, -0.75 + 0.1i)", "0.5 + 0.25i", "-0.75 + 0.1i", 8, BENCH_CASE_BINARY, {.binary = num_atan2}},
    {"sinh", "sinh(0.567 + 0.321i)", "0.567 + 0.321i", NULL, 20, BENCH_CASE_UNARY, {.unary = num_sinh}},
    {"cosh", "cosh(0.567 + 0.321i)", "0.567 + 0.321i", NULL, 20, BENCH_CASE_UNARY, {.unary = num_cosh}},
    {"tanh", "tanh(0.567 + 0.321i)", "0.567 + 0.321i", NULL, 20, BENCH_CASE_UNARY, {.unary = num_tanh}},
    {"asinh", "asinh(0.321 + 0.123i)", "0.321 + 0.123i", NULL, 2, BENCH_CASE_UNARY, {.unary = num_asinh}},
    {"acosh", "acosh(2 + 0.5i)", "2 + 0.5i", NULL, 12, BENCH_CASE_UNARY, {.unary = num_acosh}},
    {"atanh", "atanh(0.321 + 0.123i)", "0.321 + 0.123i", NULL, 8, BENCH_CASE_UNARY, {.unary = num_atanh}},
    {"lambert_w0", "lambert_w0(1 + 1i)", "1 + 1i", NULL, 4, BENCH_CASE_UNARY, {.unary = num_lambert_w0}},
    {"lambert_wm1", "lambert_wm1(-0.2 - 0.1i)", "-0.2 - 0.1i", NULL, 4, BENCH_CASE_UNARY, {.unary = num_lambert_wm1}},
    {"productlog", "productlog(1 + 1i)", "1 + 1i", NULL, 4, BENCH_CASE_UNARY, {.unary = num_productlog}},
    {"gamma", "gamma(1.5 + 0.7i)", "1.5 + 0.7i", NULL, 20, BENCH_CASE_UNARY, {.unary = num_gamma}},
    {"lgamma", "lgamma(1.5 + 0.7i)", "1.5 + 0.7i", NULL, 20, BENCH_CASE_UNARY, {.unary = num_lgamma}},
    {"digamma", "digamma(2 + 1i)", "2 + 1i", NULL, 20, BENCH_CASE_UNARY, {.unary = num_digamma}},
    {"trigamma", "trigamma(2 + 0.5i)", "2 + 0.5i", NULL, 20, BENCH_CASE_UNARY, {.unary = num_trigamma}},
    {"tetragamma", "tetragamma(2 + 0.5i)", "2 + 0.5i", NULL, 20, BENCH_CASE_UNARY, {.unary = num_tetragamma}},
    {"ei", "ei(1 + 1i)", "1 + 1i", NULL, 20, BENCH_CASE_UNARY, {.unary = num_ei}},
    {"e1", "e1(1 + 1i)", "1 + 1i", NULL, 20, BENCH_CASE_UNARY, {.unary = num_e1}},
    {"exact_mul", "(5+i)*(5-i)", "5 + i", "5 - i", 200, BENCH_CASE_BINARY, {.binary = num_mul}},
    {"exact_div", "(5+i)/(5-i)", "5 + i", "5 - i", 200, BENCH_CASE_BINARY, {.binary = num_div}},
    {"inexact_mul", "(1.23456789+2.34567891i)*(3.45678912-0.45678912i)", "1.23456789 + 2.34567891i", "3.45678912 - 0.45678912i", 200, BENCH_CASE_BINARY, {.binary = num_mul}},
    {"inexact_div", "(1.23456789+2.34567891i)/(3.45678912-0.45678912i)", "1.23456789 + 2.34567891i", "3.45678912 - 0.45678912i", 200, BENCH_CASE_BINARY, {.binary = num_div}}
};

static uint64_t now_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static int env_int(const char *name, int fallback)
{
    const char *text = getenv(name);
    char *end = NULL;
    long value;

    if (!text || !*text)
        return fallback;
    value = strtol(text, &end, 10);
    return end && *end == '\0' && value > 0 ? (int)value : fallback;
}

static int markdown_enabled(void)
{
    const char *format = getenv("MARS_BENCH_FORMAT");

    return format && strcmp(format, "md") == 0;
}

static int scaled_iters(int base_iters)
{
    return env_int("MARS_BENCH_COMPLEX_ITERS", base_iters);
}

static size_t bench_precision_bits(void)
{
    return (size_t)env_int("MARS_BENCH_COMPLEX_BITS", 256);
}

static int compare_double(const void *lhs, const void *rhs)
{
    double a = *(const double *)lhs;
    double b = *(const double *)rhs;

    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}

static bench_stats_t estimate(double *samples, int count)
{
    bench_stats_t stats = {0};

    qsort(samples, (size_t)count, sizeof(*samples), compare_double);
    stats.min_us = samples[0];
    stats.max_us = samples[count - 1];
    stats.median_us = count % 2
        ? samples[count / 2]
        : (samples[count / 2 - 1] + samples[count / 2]) / 2.0;
    return stats;
}

static number_t make_complex_from_text(const char *text, size_t precision_bits)
{
    number_t out = num_create_from_string(text);

    if (precision_bits != 0u)
        num_set_prec_bits(&out, precision_bits);
    return out;
}

static number_t make_mcomplex_import_from_text(const char *text, size_t precision_bits)
{
    mcomplex_t *value = mc_create_string(text);

    if (!value || mc_set_precision(value, precision_bits) != 0) {
        mc_free(value);
        return number_invalid();
    }
    {
        number_t out = num_create_from_mcomplex_with_prec_bits(value, precision_bits);

        mc_free(value);
        return out;
    }
}

static void use_result(number_t value)
{
    bench_sink += num_is_real(value) ? 1u : 2u;
    bench_sink += num_get_prec_bits(value) & 1u;
}

static bench_stats_t run_case(const bench_case_t *bench_case,
                              int use_mcomplex_import,
                              int iters,
                              int repeats,
                              size_t precision_bits)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));

    if (!samples) {
        fprintf(stderr, "out of memory for %s\n", bench_case->label);
        exit(2);
    }
    for (int r = 0; r < repeats; ++r) {
        number_t a = use_mcomplex_import
            ? make_mcomplex_import_from_text(bench_case->lhs_text, precision_bits)
            : make_complex_from_text(bench_case->lhs_text, precision_bits);
        number_t b = bench_case->rhs_text
            ? (use_mcomplex_import
                ? make_mcomplex_import_from_text(bench_case->rhs_text, precision_bits)
                : make_complex_from_text(bench_case->rhs_text, precision_bits))
            : number_invalid();
        uint64_t start;
        uint64_t end;

        start = now_ns();
        for (int i = 0; i < iters; ++i) {
            number_t value = bench_case->kind == BENCH_CASE_BINARY
                ? bench_case->fn.binary(a, b)
                : bench_case->fn.unary(a);

            use_result(value);
            num_destroy(&value);
        }
        end = now_ns();
        samples[r] = (double)(end - start) / (double)iters / 1000.0;
        num_destroy(&a);
        num_destroy(&b);
    }
    bench_stats_t stats = estimate(samples, repeats);

    free(samples);
    return stats;
}

static void print_stats(const char *label, bench_stats_t stats)
{
    printf("%-34s median %9.3f us  min %9.3f  max %9.3f\n",
           label,
           stats.median_us,
           stats.min_us,
           stats.max_us);
}

static void print_ratio(const char *complex_label,
                        bench_stats_t complex_stats,
                        const char *import_label,
                        bench_stats_t import_stats)
{
    if (import_stats.median_us <= 0.0)
        return;
    printf("  ratio %-24s / %-24s = %.2fx\n",
           complex_label,
           import_label,
           complex_stats.median_us / import_stats.median_us);
}

static void format_duration(double value_us, char *buffer, size_t buffer_size)
{
    if (value_us >= 1000.0)
        snprintf(buffer, buffer_size, "%.3f ms", value_us / 1000.0);
    else
        snprintf(buffer, buffer_size, "%.1f µs", value_us);
}

static void print_markdown_row(const bench_case_t *bench_case,
                               bench_stats_t complex_stats,
                               bench_stats_t import_stats)
{
    char complex_text[32];
    char import_text[32];
    double ratio = import_stats.median_us > 0.0
        ? complex_stats.median_us / import_stats.median_us
        : 0.0;

    format_duration(complex_stats.median_us, complex_text, sizeof(complex_text));
    format_duration(import_stats.median_us, import_text, sizeof(import_text));
    printf("| `%s` | `%s` | `%s` | `%.2fx` |\n",
           bench_case->display,
           complex_text,
           import_text,
           ratio);
}

int main(void)
{
    int repeats = env_int("MARS_BENCH_REPEATS", markdown_enabled() ? 9 : 15);
    size_t precision_bits = bench_precision_bits();

    if (markdown_enabled()) {
        printf("| Case | direct complex parse | mcomplex import | Ratio |\n");
        printf("|---|---:|---:|---:|\n");
    }
    else {
        puts("== number complex construction-path bench ==");
        printf("repeats=%d precision=%zu bits\n\n", repeats, precision_bits);
    }

    for (size_t i = 0u; i < sizeof(bench_cases) / sizeof(bench_cases[0]); ++i) {
        const bench_case_t *bench_case = &bench_cases[i];
        int iters = scaled_iters(bench_case->base_iters);
        bench_stats_t complex_stats = run_case(bench_case, 0, iters, repeats, precision_bits);
        bench_stats_t import_stats = run_case(bench_case, 1, iters, repeats, precision_bits);

        if (markdown_enabled()) {
            print_markdown_row(bench_case, complex_stats, import_stats);
            continue;
        }
        print_stats(bench_case->display, complex_stats);
        print_stats("mcomplex_t import", import_stats);
        print_ratio("direct parse", complex_stats, "mcomplex import", import_stats);
        putchar('\n');
    }
    if (!markdown_enabled())
        printf("bench sink: %zu\n", bench_sink);
    return 0;
}
