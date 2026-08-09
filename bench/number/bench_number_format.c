#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "number.h"
#include "ustring.h"

typedef enum bench_format_op_t { BENCH_OP_TO_STRING, BENCH_OP_SPRINTF } bench_format_op_t;

typedef struct bench_case_t {
    const char *label;
    const number_t *value;
    bench_format_op_t op;
    int base_iters;
} bench_case_t;

static volatile size_t bench_sink = 0u;

static double bench_now_seconds(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static int bench_read_positive_int(const char *name, int fallback)
{
    const char *text = getenv(name);
    char *end = NULL;
    long value;

    if (!text || !*text)
        return fallback;
    value = strtol(text, &end, 10);
    if (!end || *end != '\0' || value <= 0 || value > 100000000L)
        return fallback;
    return (int)value;
}

static int bench_scale_iters(int base_iters)
{
    int scale = bench_read_positive_int("MARS_BENCH_SCALE", 1);

    if (base_iters > 100000000 / scale)
        return 100000000;
    return base_iters * scale;
}

static int bench_repeat_count(void)
{
    return bench_read_positive_int("MARS_BENCH_REPEATS", 31);
}

static int bench_compare_double(const void *lhs, const void *rhs)
{
    double a = *(const double *)lhs;
    double b = *(const double *)rhs;

    return (a > b) - (a < b);
}

static double bench_median(double *values, int count)
{
    qsort(values, (size_t)count, sizeof(*values), bench_compare_double);
    if (count % 2)
        return values[count / 2];
    return (values[count / 2 - 1] + values[count / 2]) / 2.0;
}

static size_t bench_consume_char_text(char *text)
{
    size_t len = text ? strlen(text) : 0u;

    free(text);
    return len;
}

static size_t bench_consume_string_text(string_t *text)
{
    size_t len = text ? strlen(string_c_str(text)) : 0u;

    string_free(text);
    return len;
}

#define bench_consume_num_text(expr)                                                                                   \
    _Generic((expr), char *: bench_consume_char_text, string_t *: bench_consume_string_text)(expr)

static void bench_run_to_string(const number_t *value, int iters)
{
    for (int i = 0; i < iters; ++i)
        bench_sink += bench_consume_num_text(num_to_string(*value));
}

static void bench_run_sprintf(const number_t *value, int iters)
{
    char buffer[512];

    for (int i = 0; i < iters; ++i) {
        int written = num_sprintf(buffer, sizeof(buffer), "%n", *value);

        if (written > 0)
            bench_sink += (size_t)written;
    }
}

static double bench_sample_case(const bench_case_t *bench_case)
{
    int iters = bench_scale_iters(bench_case->base_iters);
    double start;
    double elapsed;

    start = bench_now_seconds();
    switch (bench_case->op) {
        case BENCH_OP_TO_STRING:
            bench_run_to_string(bench_case->value, iters);
            break;
        case BENCH_OP_SPRINTF:
            bench_run_sprintf(bench_case->value, iters);
            break;
    }
    elapsed = bench_now_seconds() - start;
    return (elapsed * 1000000.0) / (double)iters;
}

static void bench_run_case(const bench_case_t *bench_case)
{
    int repeats = bench_repeat_count();
    double *samples = calloc((size_t)repeats, sizeof(*samples));
    double *median_samples;
    double *deviations;
    double median;
    double mad;

    if (!samples) {
        fprintf(stderr, "failed to allocate benchmark samples\n");
        exit(1);
    }
    median_samples = calloc((size_t)repeats, sizeof(*median_samples));
    deviations = calloc((size_t)repeats, sizeof(*deviations));
    if (!median_samples || !deviations) {
        fprintf(stderr, "failed to allocate benchmark statistics\n");
        free(samples);
        free(median_samples);
        free(deviations);
        exit(1);
    }

    (void)bench_sample_case(bench_case);
    for (int i = 0; i < repeats; ++i) {
        samples[i] = bench_sample_case(bench_case);
        median_samples[i] = samples[i];
    }

    median = bench_median(median_samples, repeats);
    for (int i = 0; i < repeats; ++i) {
        double delta = samples[i] - median;

        deviations[i] = delta < 0.0 ? -delta : delta;
    }
    mad = bench_median(deviations, repeats);

    printf("%-24s median_us=%9.3f mad_us=%8.3f\n", bench_case->label, median, mad);

    free(samples);
    free(median_samples);
    free(deviations);
}

int main(void)
{
    size_t saved_precision = num_get_default_prec_bits();
    number_t small_integer = num_create_from_long(123456789L);
    number_t large_integer = num_create_from_string("123456789012345678901234567890123456789");
    number_t rational = num_create_from_string("355/113");
    number_t decimal;
    number_t complex_value;
    bench_case_t cases[10];
    size_t count = 0u;

    num_set_default_prec_bits(256u);
    decimal = num_create_from_string("1.2345678901234567890123456789");
    complex_value = num_create_from_string("1.234567890123456789 + 2.34567890123456789i");

    cases[count++] = (bench_case_t){"to_string_small_int", &small_integer, BENCH_OP_TO_STRING, 50000};
    cases[count++] = (bench_case_t){"to_string_big_int", &large_integer, BENCH_OP_TO_STRING, 20000};
    cases[count++] = (bench_case_t){"to_string_rational", &rational, BENCH_OP_TO_STRING, 20000};
    cases[count++] = (bench_case_t){"to_string_decimal", &decimal, BENCH_OP_TO_STRING, 10000};
    cases[count++] = (bench_case_t){"to_string_complex", &complex_value, BENCH_OP_TO_STRING, 5000};

    cases[count++] = (bench_case_t){"sprintf_small_int", &small_integer, BENCH_OP_SPRINTF, 50000};
    cases[count++] = (bench_case_t){"sprintf_big_int", &large_integer, BENCH_OP_SPRINTF, 20000};
    cases[count++] = (bench_case_t){"sprintf_rational", &rational, BENCH_OP_SPRINTF, 20000};
    cases[count++] = (bench_case_t){"sprintf_decimal", &decimal, BENCH_OP_SPRINTF, 10000};
    cases[count++] = (bench_case_t){"sprintf_complex", &complex_value, BENCH_OP_SPRINTF, 5000};

    puts("== number formatting bench ==");
    puts("Reports median microseconds per format+release call.");
    puts("Scale iterations with MARS_BENCH_SCALE=<n>; tune repeats with MARS_BENCH_REPEATS=<n>.");
    for (size_t i = 0u; i < count; ++i)
        bench_run_case(&cases[i]);

    num_destroy(&complex_value);
    num_destroy(&decimal);
    num_destroy(&rational);
    num_destroy(&large_integer);
    num_destroy(&small_integer);
    num_set_default_prec_bits(saved_precision);

    return bench_sink == 0u ? 1 : 0;
}
