#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "number.h"

typedef number_t (*number_unary_fn)(number_t);
typedef number_t (*number_binary_fn)(number_t, number_t);
typedef double _Complex (*cdouble_unary_fn)(double _Complex);
typedef double _Complex (*cdouble_binary_fn)(double _Complex, double _Complex);

typedef enum bench_case_kind_t { BENCH_UNARY, BENCH_BINARY } bench_case_kind_t;

typedef struct bench_stats_t {
    double median_us;
    double min_us;
    double max_us;
} bench_stats_t;

typedef struct cdouble_case_t {
    const char *label;
    bench_case_kind_t kind;
    double _Complex lhs;
    double _Complex rhs;
    int base_iters;
    union {
        number_unary_fn number_unary;
        number_binary_fn number_binary;
    } number_fn;
    union {
        cdouble_unary_fn cdouble_unary;
        cdouble_binary_fn cdouble_binary;
    } cdouble_fn;
} cdouble_case_t;

static volatile double bench_double_sink = 0.0;
static volatile size_t bench_size_sink = 0u;

static double _Complex cdouble_neg(double _Complex z)
{
    return -z;
}
static double _Complex cdouble_abs(double _Complex z)
{
    return cabs(z);
}
static double _Complex cdouble_inv(double _Complex z)
{
    return 1.0 / z;
}
static double _Complex cdouble_conj(double _Complex z)
{
    return conj(z);
}
static double _Complex cdouble_floor(double _Complex z)
{
    return floor(creal(z)) + floor(cimag(z)) * I;
}
static double _Complex cdouble_log10(double _Complex z)
{
    return clog(z) / log(10.0);
}
static double _Complex cdouble_add(double _Complex a, double _Complex b)
{
    return a + b;
}
static double _Complex cdouble_sub(double _Complex a, double _Complex b)
{
    return a - b;
}
static double _Complex cdouble_mul(double _Complex a, double _Complex b)
{
    return a * b;
}
static double _Complex cdouble_div(double _Complex a, double _Complex b)
{
    return a / b;
}

static const cdouble_case_t cdouble_cases[] = {
    {"add",
     BENCH_BINARY,
     1.23456789 + 2.34567891 * I,
     3.45678912 - 0.45678912 * I,
     1000,
     {.number_binary = num_add},
     {.cdouble_binary = cdouble_add}},
    {"sub",
     BENCH_BINARY,
     1.23456789 + 2.34567891 * I,
     3.45678912 - 0.45678912 * I,
     1000,
     {.number_binary = num_sub},
     {.cdouble_binary = cdouble_sub}},
    {"mul",
     BENCH_BINARY,
     1.23456789 + 2.34567891 * I,
     1.00000001 - 0.00000001 * I,
     1000,
     {.number_binary = num_mul},
     {.cdouble_binary = cdouble_mul}},
    {"div",
     BENCH_BINARY,
     1.23456789 + 2.34567891 * I,
     1.00000001 - 0.00000001 * I,
     1000,
     {.number_binary = num_div},
     {.cdouble_binary = cdouble_div}},
    {"pow",
     BENCH_BINARY,
     1.23456789 + 2.34567891 * I,
     1.25 - 0.125 * I,
     300,
     {.number_binary = num_pow},
     {.cdouble_binary = cpow}},
    {"neg", BENCH_UNARY, 0.567 + 0.321 * I, 0.0, 1000, {.number_unary = num_neg}, {.cdouble_unary = cdouble_neg}},
    {"abs", BENCH_UNARY, 0.567 + 0.321 * I, 0.0, 1000, {.number_unary = num_abs}, {.cdouble_unary = cdouble_abs}},
    {"inv", BENCH_UNARY, 0.567 + 0.321 * I, 0.0, 1000, {.number_unary = num_inv}, {.cdouble_unary = cdouble_inv}},
    {"conj", BENCH_UNARY, 0.567 + 0.321 * I, 0.0, 1000, {.number_unary = num_conj}, {.cdouble_unary = cdouble_conj}},
    {"floor", BENCH_UNARY, 0.567 + 0.321 * I, 0.0, 1000, {.number_unary = num_floor}, {.cdouble_unary = cdouble_floor}},
    {"sqrt", BENCH_UNARY, 0.567 + 0.321 * I, 0.0, 600, {.number_unary = num_sqrt}, {.cdouble_unary = csqrt}},
    {"exp", BENCH_UNARY, 0.567 + 0.321 * I, 0.0, 600, {.number_unary = num_exp}, {.cdouble_unary = cexp}},
    {"log", BENCH_UNARY, 0.567 + 0.321 * I, 0.0, 600, {.number_unary = num_log}, {.cdouble_unary = clog}},
    {"log10", BENCH_UNARY, 0.567 + 0.321 * I, 0.0, 600, {.number_unary = num_log10}, {.cdouble_unary = cdouble_log10}},
    {"sin", BENCH_UNARY, 0.567 + 0.321 * I, 0.0, 600, {.number_unary = num_sin}, {.cdouble_unary = csin}},
    {"cos", BENCH_UNARY, 0.567 + 0.321 * I, 0.0, 600, {.number_unary = num_cos}, {.cdouble_unary = ccos}},
    {"tan", BENCH_UNARY, 0.567 + 0.321 * I, 0.0, 600, {.number_unary = num_tan}, {.cdouble_unary = ctan}},
    {"asin", BENCH_UNARY, 0.321 + 0.123 * I, 0.0, 300, {.number_unary = num_asin}, {.cdouble_unary = casin}},
    {"acos", BENCH_UNARY, 0.321 + 0.123 * I, 0.0, 300, {.number_unary = num_acos}, {.cdouble_unary = cacos}},
    {"atan", BENCH_UNARY, 0.321 + 0.123 * I, 0.0, 300, {.number_unary = num_atan}, {.cdouble_unary = catan}},
    {"sinh", BENCH_UNARY, 0.567 + 0.321 * I, 0.0, 600, {.number_unary = num_sinh}, {.cdouble_unary = csinh}},
    {"cosh", BENCH_UNARY, 0.567 + 0.321 * I, 0.0, 600, {.number_unary = num_cosh}, {.cdouble_unary = ccosh}},
    {"tanh", BENCH_UNARY, 0.567 + 0.321 * I, 0.0, 600, {.number_unary = num_tanh}, {.cdouble_unary = ctanh}},
    {"asinh", BENCH_UNARY, 0.321 + 0.123 * I, 0.0, 300, {.number_unary = num_asinh}, {.cdouble_unary = casinh}},
    {"acosh", BENCH_UNARY, 1.567 + 0.321 * I, 0.0, 300, {.number_unary = num_acosh}, {.cdouble_unary = cacosh}},
    {"atanh", BENCH_UNARY, 0.321 + 0.123 * I, 0.0, 300, {.number_unary = num_atanh}, {.cdouble_unary = catanh}}};

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

static int scaled_iters(int base_iters)
{
    return base_iters * env_int("MARS_BENCH_SCALE", 1);
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
    bench_stats_t stats;

    qsort(samples, (size_t)count, sizeof(*samples), compare_double);
    stats.min_us = samples[0];
    stats.max_us = samples[count - 1];
    stats.median_us = count % 2 ? samples[count / 2] : (samples[count / 2 - 1] + samples[count / 2]) / 2.0;
    return stats;
}

static void consume_cdouble(double _Complex value)
{
    bench_double_sink += creal(value);
    bench_double_sink += cimag(value);
}

static void consume_number(number_t value)
{
    bench_size_sink += (size_t)value.storage[0];
    bench_size_sink += (size_t)value.storage[1];
}

static void fail_case(const char *backend, const char *label, const char *message)
{
    fprintf(stderr, "%s %s: %s\n", backend, label, message);
    exit(EXIT_FAILURE);
}

static bench_stats_t time_cdouble_case(const cdouble_case_t *bench_case, int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    int iters = scaled_iters(bench_case->base_iters);

    if (!samples)
        fail_case("cdouble", bench_case->label, "sample allocation failed");
    for (int r = 0; r < repeats; ++r) {
        uint64_t start;

        if (bench_case->kind == BENCH_BINARY) {
            double _Complex acc = bench_case->lhs;
            double _Complex rhs = bench_case->rhs;

            start = now_ns();
            for (int i = 0; i < iters; ++i)
                acc = bench_case->cdouble_fn.cdouble_binary(acc, rhs);
            samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;
            consume_cdouble(acc);
        } else {
            double _Complex src = bench_case->lhs;

            start = now_ns();
            for (int i = 0; i < iters; ++i)
                consume_cdouble(bench_case->cdouble_fn.cdouble_unary(src));
            samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;
        }
    }

    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static bench_stats_t time_number_case(const cdouble_case_t *bench_case, int repeats)
{
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    int iters = scaled_iters(bench_case->base_iters);

    if (!samples)
        fail_case("number/cdouble", bench_case->label, "sample allocation failed");
    for (int r = 0; r < repeats; ++r) {
        uint64_t start;

        if (bench_case->kind == BENCH_BINARY) {
            number_t acc = num_create_from_cdouble(bench_case->lhs);
            number_t rhs = num_create_from_cdouble(bench_case->rhs);

            start = now_ns();
            for (int i = 0; i < iters; ++i)
                acc = bench_case->number_fn.number_binary(acc, rhs);
            samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;
            consume_number(acc);
            num_destroy(&acc);
            num_destroy(&rhs);
        } else {
            number_t src = num_create_from_cdouble(bench_case->lhs);

            start = now_ns();
            for (int i = 0; i < iters; ++i) {
                number_t out = bench_case->number_fn.number_unary(src);

                consume_number(out);
                num_destroy(&out);
            }
            samples[r] = (double)(now_ns() - start) / (double)iters / 1000.0;
            num_destroy(&src);
        }
    }

    bench_stats_t stats = estimate(samples, repeats);
    free(samples);
    return stats;
}

static void print_header(void)
{
    printf("number cdouble vs C double _Complex\n");
    printf("repeats=%d scale=%d\n", env_int("MARS_BENCH_REPEATS", 9), env_int("MARS_BENCH_SCALE", 1));
    printf("%-12s %12s %12s %10s\n", "case", "raw_us", "number_us", "ratio");
    printf("%-12s %12s %12s %10s\n", "----", "------", "---------", "-----");
}

static void print_row(const char *label, bench_stats_t raw, bench_stats_t number)
{
    double ratio = raw.median_us > 0.0 ? number.median_us / raw.median_us : 0.0;

    printf("%-12s %12.3f %12.3f %9.2fx\n", label, raw.median_us, number.median_us, ratio);
}

int main(void)
{
    int repeats = env_int("MARS_BENCH_REPEATS", 9);
    const char *filter = getenv("MARS_BENCH_FILTER");

    print_header();
    for (size_t i = 0u; i < sizeof(cdouble_cases) / sizeof(cdouble_cases[0]); ++i) {
        const cdouble_case_t *bench_case = &cdouble_cases[i];

        if (filter && *filter && !strstr(bench_case->label, filter))
            continue;
        print_row(bench_case->label, time_cdouble_case(bench_case, repeats), time_number_case(bench_case, repeats));
    }
    printf("\nsinks: %.3f %zu\n", bench_double_sink, bench_size_sink);
    return 0;
}
