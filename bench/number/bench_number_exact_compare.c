#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mint.h"
#include "mrational.h"
#include "number.h"

typedef number_t (*number_binary_fn)(number_t, number_t);
typedef number_t (*number_unary_fn)(number_t);
typedef int (*mint_binary_fn)(mint_t *, const mint_t *);
typedef int (*mint_unary_fn)(mint_t *);
typedef int (*mrational_binary_fn)(mrational_t *, const mrational_t *);
typedef int (*mrational_unary_fn)(mrational_t *);

typedef struct bench_stats_t {
    double median_us;
    double min_us;
    double max_us;
} bench_stats_t;

typedef enum bench_family_t {
    BENCH_MINT_BINARY,
    BENCH_MINT_DIVMOD,
    BENCH_MINT_POWMOD,
    BENCH_MINT_UNARY,
    BENCH_MRATIONAL_BINARY,
    BENCH_MRATIONAL_UNARY
} bench_family_t;

typedef struct bench_case_t {
    const char *label;
    bench_family_t family;
    int iters;
    union {
        struct {
            number_binary_fn number_fn;
            mint_binary_fn legacy_fn;
        } mint_binary;
        struct {
            number_unary_fn number_fn;
            mint_unary_fn legacy_fn;
        } mint_unary;
        struct {
            number_binary_fn number_fn;
            mrational_binary_fn legacy_fn;
        } mrational_binary;
        struct {
            number_unary_fn number_fn;
            mrational_unary_fn legacy_fn;
        } mrational_unary;
    } fn;
} bench_case_t;

static volatile long long bench_sink;

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
    long parsed;

    if (!text || !*text)
        return fallback;
    parsed = strtol(text, &end, 10);
    if (!end || *end != '\0' || parsed < 1)
        return fallback;
    return (int)parsed;
}

static int markdown_enabled(void)
{
    const char *format = getenv("MARS_BENCH_FORMAT");

    return format && strcmp(format, "md") == 0;
}

static int repeats(void)
{
    return env_int("MARS_BENCH_REPEATS", markdown_enabled() ? 9 : 15);
}

static int scaled_iters(int base_iters)
{
    return base_iters * env_int("MARS_BENCH_SCALE", 1);
}

static int compare_double(const void *lhs, const void *rhs)
{
    const double a = *(const double *)lhs;
    const double b = *(const double *)rhs;

    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}

static bench_stats_t estimate(const double *samples, int count)
{
    bench_stats_t stats = {0};
    double *sorted;

    if (count <= 0)
        return stats;
    sorted = (double *)malloc((size_t)count * sizeof(*sorted));
    if (!sorted)
        return stats;
    memcpy(sorted, samples, (size_t)count * sizeof(*sorted));
    qsort(sorted, (size_t)count, sizeof(*sorted), compare_double);
    stats.median_us = sorted[count / 2];
    stats.min_us = sorted[0];
    stats.max_us = sorted[count - 1];
    free(sorted);
    return stats;
}

static void consume_number(number_t value)
{
    bench_sink += (long long)num_sign(value);
}

static void consume_mint(const mint_t *value)
{
    bench_sink += (long long)mi_cmp_long(value, 0);
}

static void consume_mrational(const mrational_t *value)
{
    bench_sink += (long long)mr_cmp(value, MR_HALF);
}

static mint_t *make_exact_bits(size_t bits, unsigned salt)
{
    mint_t *value;

    if (bits <= 1u)
        return mi_create_long(1);
    value = mi_create_2pow((uint64_t)(bits - 1u));
    if (!value)
        return NULL;
    if (mi_add_long(value, (long)(salt | 1u)) != 0) {
        mi_free(value);
        return NULL;
    }
    return value;
}

static number_t number_from_mint(const mint_t *value)
{
    char *text = mi_to_string(value);
    number_t number = text ? num_create_from_string(text) : num_new();

    free(text);
    return number;
}

static mrational_t *make_rational_bits(size_t numerator_bits,
                                       size_t denominator_bits,
                                       unsigned salt)
{
    mint_t *numerator = make_exact_bits(numerator_bits, salt);
    mint_t *denominator = make_exact_bits(denominator_bits, salt + 97u);
    mrational_t *value = NULL;

    if (numerator && denominator)
        value = mr_create_mints(numerator, denominator);
    mi_free(numerator);
    mi_free(denominator);
    return value;
}

static number_t number_from_mrational(const mrational_t *value)
{
    char *text = mr_to_string(value);
    number_t number = text ? num_create_from_string(text) : num_new();

    free(text);
    return number;
}

static int mint_sqrt_clone(mint_t *value)
{
    return mi_sqrt(value);
}

static number_t number_divmod_case(number_t a, number_t b)
{
    number_t quotient = NUM_NAN;
    number_t remainder = NUM_NAN;

    if (num_divmod(a, b, &quotient, &remainder) != 0)
        return NUM_NAN;
    consume_number(remainder);
    num_destroy(&remainder);
    return quotient;
}

static int mint_divmod_case(mint_t *a, const mint_t *b)
{
    mint_t *quotient = mi_new();
    mint_t *remainder = mi_new();
    int rc = -1;

    if (quotient && remainder)
        rc = mi_divmod(a, b, quotient, remainder);
    if (rc == 0) {
        consume_mint(quotient);
        consume_mint(remainder);
    }
    mi_free(quotient);
    mi_free(remainder);
    return rc;
}

static number_t number_powmod_case(number_t base, number_t modulus)
{
    number_t exponent = num_create_from_long(65537);
    number_t result = num_powmod(base, exponent, modulus);

    num_destroy(&exponent);
    return result;
}

static int mint_powmod_case(mint_t *base, const mint_t *modulus)
{
    mint_t *exponent = mi_create_long(65537);
    int rc = exponent ? mi_powmod(base, exponent, modulus) : -1;

    mi_free(exponent);
    return rc;
}

static bench_stats_t run_number_binary(number_binary_fn fn,
                                       number_t a,
                                       number_t b,
                                       int iters)
{
    double *samples = (double *)calloc((size_t)repeats(), sizeof(*samples));
    int sample_count = samples ? repeats() : 0;

    for (int r = 0; r < sample_count; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i) {
            number_t result = fn(a, b);

            consume_number(result);
            num_destroy(&result);
        }
        samples[r] = (double)(now_ns() - start) / 1000.0 / (double)iters;
    }
    bench_stats_t stats = estimate(samples, sample_count);
    free(samples);
    return stats;
}

static bench_stats_t run_number_unary(number_unary_fn fn, number_t a, int iters)
{
    double *samples = (double *)calloc((size_t)repeats(), sizeof(*samples));
    int sample_count = samples ? repeats() : 0;

    for (int r = 0; r < sample_count; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i) {
            number_t result = fn(a);

            consume_number(result);
            num_destroy(&result);
        }
        samples[r] = (double)(now_ns() - start) / 1000.0 / (double)iters;
    }
    bench_stats_t stats = estimate(samples, sample_count);
    free(samples);
    return stats;
}

static bench_stats_t run_mint_binary(mint_binary_fn fn,
                                     const mint_t *a,
                                     const mint_t *b,
                                     int iters)
{
    double *samples = (double *)calloc((size_t)repeats(), sizeof(*samples));
    int sample_count = samples ? repeats() : 0;

    for (int r = 0; r < sample_count; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i) {
            mint_t *result = mi_clone(a);

            if (result && fn(result, b) == 0)
                consume_mint(result);
            mi_free(result);
        }
        samples[r] = (double)(now_ns() - start) / 1000.0 / (double)iters;
    }
    bench_stats_t stats = estimate(samples, sample_count);
    free(samples);
    return stats;
}

static bench_stats_t run_mint_unary(mint_unary_fn fn, const mint_t *a, int iters)
{
    double *samples = (double *)calloc((size_t)repeats(), sizeof(*samples));
    int sample_count = samples ? repeats() : 0;

    for (int r = 0; r < sample_count; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i) {
            mint_t *result = mi_clone(a);

            if (result && fn(result) == 0)
                consume_mint(result);
            mi_free(result);
        }
        samples[r] = (double)(now_ns() - start) / 1000.0 / (double)iters;
    }
    bench_stats_t stats = estimate(samples, sample_count);
    free(samples);
    return stats;
}

static bench_stats_t run_mrational_binary(mrational_binary_fn fn,
                                          const mrational_t *a,
                                          const mrational_t *b,
                                          int iters)
{
    double *samples = (double *)calloc((size_t)repeats(), sizeof(*samples));
    int sample_count = samples ? repeats() : 0;

    for (int r = 0; r < sample_count; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i) {
            mrational_t *result = mr_clone(a);

            if (result && fn(result, b) == 0)
                consume_mrational(result);
            mr_free(result);
        }
        samples[r] = (double)(now_ns() - start) / 1000.0 / (double)iters;
    }
    bench_stats_t stats = estimate(samples, sample_count);
    free(samples);
    return stats;
}

static bench_stats_t run_mrational_unary(mrational_unary_fn fn,
                                         const mrational_t *a,
                                         int iters)
{
    double *samples = (double *)calloc((size_t)repeats(), sizeof(*samples));
    int sample_count = samples ? repeats() : 0;

    for (int r = 0; r < sample_count; ++r) {
        uint64_t start = now_ns();

        for (int i = 0; i < iters; ++i) {
            mrational_t *result = mr_clone(a);

            if (result && fn(result) == 0)
                consume_mrational(result);
            mr_free(result);
        }
        samples[r] = (double)(now_ns() - start) / 1000.0 / (double)iters;
    }
    bench_stats_t stats = estimate(samples, sample_count);
    free(samples);
    return stats;
}

static number_t number_mpq_pow_int(number_t value)
{
    return num_pow_int(value, 17);
}

static int mrational_pow_int_case(mrational_t *value)
{
    return mr_pow_int(value, 17);
}

static const bench_case_t bench_cases[] = {
    {"integer add", BENCH_MINT_BINARY, 2000, {.mint_binary = {num_add, mi_add}}},
    {"integer sub", BENCH_MINT_BINARY, 2000, {.mint_binary = {num_sub, mi_sub}}},
    {"integer mul", BENCH_MINT_BINARY, 1000, {.mint_binary = {num_mul, mi_mul}}},
    {"integer divmod", BENCH_MINT_DIVMOD, 800, {.mint_binary = {number_divmod_case, mint_divmod_case}}},
    {"integer mod", BENCH_MINT_BINARY, 1000, {.mint_binary = {num_mod, mi_mod}}},
    {"integer gcd", BENCH_MINT_BINARY, 1000, {.mint_binary = {num_gcd, mi_gcd}}},
    {"integer lcm", BENCH_MINT_BINARY, 600, {.mint_binary = {num_lcm, mi_lcm}}},
    {"integer powmod", BENCH_MINT_POWMOD, 100, {.mint_binary = {number_powmod_case, mint_powmod_case}}},
    {"integer isqrt", BENCH_MINT_UNARY, 1000, {.mint_unary = {num_isqrt, mint_sqrt_clone}}},
    {"rational add", BENCH_MRATIONAL_BINARY, 1000, {.mrational_binary = {num_add, mr_add}}},
    {"rational sub", BENCH_MRATIONAL_BINARY, 1000, {.mrational_binary = {num_sub, mr_sub}}},
    {"rational mul", BENCH_MRATIONAL_BINARY, 1000, {.mrational_binary = {num_mul, mr_mul}}},
    {"rational div", BENCH_MRATIONAL_BINARY, 800, {.mrational_binary = {num_div, mr_div}}},
    {"rational neg", BENCH_MRATIONAL_UNARY, 1500, {.mrational_unary = {num_neg, mr_neg}}},
    {"rational inv", BENCH_MRATIONAL_UNARY, 1000, {.mrational_unary = {num_inv, mr_inv}}},
    {"rational pow_int", BENCH_MRATIONAL_UNARY, 120, {.mrational_unary = {number_mpq_pow_int, mrational_pow_int_case}}}
};

static void print_row(const char *label,
                      bench_stats_t number_stats,
                      bench_stats_t legacy_stats)
{
    double ratio = legacy_stats.median_us > 0.0
        ? number_stats.median_us / legacy_stats.median_us
        : 0.0;

    if (markdown_enabled()) {
        printf("| `%s` | %.3f | %.3f | %.2fx |\n",
               label,
               number_stats.median_us,
               legacy_stats.median_us,
               ratio);
        return;
    }

    printf("%-18s number=%9.3f us  legacy=%9.3f us  ratio=%5.2fx\n",
           label,
           number_stats.median_us,
           legacy_stats.median_us,
           ratio);
}

int main(void)
{
    mint_t *mint_a = make_exact_bits(1024u, 17u);
    mint_t *mint_b = make_exact_bits(512u, 43u);
    mrational_t *mr_a = make_rational_bits(768u, 257u, 71u);
    mrational_t *mr_b = make_rational_bits(640u, 193u, 113u);
    number_t num_int_a;
    number_t num_int_b;
    number_t num_rat_a;
    number_t num_rat_b;

    if (!mint_a || !mint_b || !mr_a || !mr_b) {
        fprintf(stderr, "failed to create benchmark inputs\n");
        mi_free(mint_a);
        mi_free(mint_b);
        mr_free(mr_a);
        mr_free(mr_b);
        return 1;
    }

    num_int_a = number_from_mint(mint_a);
    num_int_b = number_from_mint(mint_b);
    num_rat_a = number_from_mrational(mr_a);
    num_rat_b = number_from_mrational(mr_b);

    if (markdown_enabled()) {
        puts("| Case | `number_t` (us) | legacy (us) | number / legacy |");
        puts("|---|---:|---:|---:|");
    } else {
        puts("number_t exact backend vs legacy mint_t/mrational_t");
        puts("Per-operation median time. Lower is better.");
    }

    for (size_t i = 0u; i < sizeof(bench_cases) / sizeof(bench_cases[0]); ++i) {
        const bench_case_t *bench_case = &bench_cases[i];
        int iters = scaled_iters(bench_case->iters);
        bench_stats_t number_stats = {0};
        bench_stats_t legacy_stats = {0};

        switch (bench_case->family) {
        case BENCH_MINT_BINARY:
        case BENCH_MINT_DIVMOD:
        case BENCH_MINT_POWMOD:
            number_stats = run_number_binary(bench_case->fn.mint_binary.number_fn,
                                             num_int_a,
                                             num_int_b,
                                             iters);
            legacy_stats = run_mint_binary(bench_case->fn.mint_binary.legacy_fn,
                                           mint_a,
                                           mint_b,
                                           iters);
            break;
        case BENCH_MINT_UNARY:
            number_stats = run_number_unary(bench_case->fn.mint_unary.number_fn,
                                            num_int_a,
                                            iters);
            legacy_stats = run_mint_unary(bench_case->fn.mint_unary.legacy_fn,
                                          mint_a,
                                          iters);
            break;
        case BENCH_MRATIONAL_BINARY:
            number_stats = run_number_binary(bench_case->fn.mrational_binary.number_fn,
                                             num_rat_a,
                                             num_rat_b,
                                             iters);
            legacy_stats = run_mrational_binary(bench_case->fn.mrational_binary.legacy_fn,
                                                mr_a,
                                                mr_b,
                                                iters);
            break;
        case BENCH_MRATIONAL_UNARY:
            number_stats = run_number_unary(bench_case->fn.mrational_unary.number_fn,
                                            num_rat_a,
                                            iters);
            legacy_stats = run_mrational_unary(bench_case->fn.mrational_unary.legacy_fn,
                                               mr_a,
                                               iters);
            break;
        }

        print_row(bench_case->label, number_stats, legacy_stats);
    }

    if (!markdown_enabled())
        printf("sink=%lld\n", bench_sink);

    num_destroy(&num_int_a);
    num_destroy(&num_int_b);
    num_destroy(&num_rat_a);
    num_destroy(&num_rat_b);
    mi_free(mint_a);
    mi_free(mint_b);
    mr_free(mr_a);
    mr_free(mr_b);
    return 0;
}
