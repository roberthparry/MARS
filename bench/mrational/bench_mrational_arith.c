#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mrational.h"

typedef int (*mr_binary_fn)(mrational_t *, const mrational_t *);
typedef int (*mr_unary_fn)(mrational_t *);

typedef struct bit_band_t {
    size_t lo;
    size_t hi;
    const char *label;
} bit_band_t;

static const bit_band_t bit_bands[] = {
    { 1u, 256u, "1-256" },
    { 257u, 512u, "257-512" },
    { 513u, 768u, "513-768" },
    { 769u, 1024u, "769-1024" }
};

#define BAND_COUNT (sizeof(bit_bands) / sizeof(bit_bands[0]))
#define SAMPLES_PER_BAND 4u

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

static int bench_wants_operation(const char *name)
{
    const char *filter = getenv("MARS_BENCH_FILTER");

    if (!filter || !*filter)
        return 1;
    return strstr(name, filter) != NULL;
}

static size_t band_sample_bits(size_t band_index, size_t sample_index)
{
    const bit_band_t *band = &bit_bands[band_index];
    size_t span = band->hi - band->lo;

    if (sample_index == 0u)
        return band->lo;
    if (sample_index + 1u >= SAMPLES_PER_BAND)
        return band->hi;
    return band->lo + (span * sample_index) / (SAMPLES_PER_BAND - 1u);
}

static mint_t *make_exact_bits(size_t bits, unsigned salt, int denominator_mode)
{
    mint_t *value;
    long extra;

    if (bits == 0u)
        return NULL;
    if (bits == 1u)
        return mi_create_long(1);

    value = mi_create_2pow((uint64_t)(bits - 1u));
    if (!value)
        return NULL;

    if (denominator_mode) {
        extra = 1l + 2l * (long)(salt % 5u);
    } else if (bits == 2u) {
        extra = 1l;
    } else {
        extra = 2l + 2l * (long)(salt % 5u);
    }

    if (mi_add_long(value, extra) != 0) {
        mi_free(value);
        return NULL;
    }
    return value;
}

static mrational_t *make_band_rational(size_t numerator_bits, size_t denominator_bits, unsigned salt)
{
    for (unsigned attempt = 0u; attempt < 16u; ++attempt) {
        mint_t *num = make_exact_bits(numerator_bits, salt + attempt, 0);
        mint_t *den = make_exact_bits(denominator_bits, salt + 17u + attempt, 1);
        mrational_t *rational = NULL;
        const mint_t *norm_num;
        const mint_t *norm_den;

        if (!num || !den) {
            mi_free(num);
            mi_free(den);
            return NULL;
        }
        if ((salt + attempt) & 1u) {
            if (mi_neg(num) != 0) {
                mi_free(num);
                mi_free(den);
                return NULL;
            }
        }

        rational = mr_create_mints(num, den);
        mi_free(num);
        mi_free(den);
        if (!rational)
            return NULL;

        norm_num = mr_numerator(rational);
        norm_den = mr_denominator(rational);
        if (norm_num && norm_den &&
            mi_bit_length(norm_num) == numerator_bits &&
            mi_bit_length(norm_den) == denominator_bits)
            return rational;

        mr_free(rational);
    }

    return NULL;
}

static double measure_binary_cell(mr_binary_fn fn,
                                  size_t numerator_band,
                                  size_t denominator_band,
                                  int iters)
{
    uint64_t total_ns = 0ull;
    size_t total_runs = 0u;

    for (size_t num_sample = 0u; num_sample < SAMPLES_PER_BAND; ++num_sample) {
        for (size_t den_sample = 0u; den_sample < SAMPLES_PER_BAND; ++den_sample) {
            size_t num_bits = band_sample_bits(numerator_band, num_sample);
            size_t den_bits = band_sample_bits(denominator_band, den_sample);
            mrational_t *lhs = make_band_rational(num_bits, den_bits, (unsigned)(11u * num_sample + den_sample));
            mrational_t *rhs = make_band_rational(num_bits, den_bits, (unsigned)(97u + 13u * num_sample + den_sample));
            mrational_t *warm = NULL;
            uint64_t start;
            uint64_t end;

            if (!lhs || !rhs) {
                fprintf(stderr, "binary cell input generation failed for bits=%zu/%zu\n", num_bits, den_bits);
                mr_free(lhs);
                mr_free(rhs);
                return -1.0;
            }

            warm = mr_clone(lhs);
            if (!warm || fn(warm, rhs) != 0) {
                fprintf(stderr, "binary warmup failed for bits=%zu/%zu\n", num_bits, den_bits);
                mr_free(warm);
                mr_free(lhs);
                mr_free(rhs);
                return -1.0;
            }
            mr_free(warm);

            start = now_ns();
            for (int i = 0; i < iters; ++i) {
                mrational_t *value = mr_clone(lhs);

                if (!value || fn(value, rhs) != 0) {
                    fprintf(stderr, "binary timed run failed for bits=%zu/%zu\n", num_bits, den_bits);
                    mr_free(value);
                    mr_free(lhs);
                    mr_free(rhs);
                    return -1.0;
                }
                mr_free(value);
            }
            end = now_ns();

            total_ns += end - start;
            total_runs += (size_t)iters;
            mr_free(lhs);
            mr_free(rhs);
        }
    }

    return total_runs == 0u ? -1.0 : ((double)total_ns / (double)total_runs) / 1000000.0;
}

static double measure_unary_cell(mr_unary_fn fn,
                                 size_t numerator_band,
                                 size_t denominator_band,
                                 int iters)
{
    uint64_t total_ns = 0ull;
    size_t total_runs = 0u;

    for (size_t num_sample = 0u; num_sample < SAMPLES_PER_BAND; ++num_sample) {
        for (size_t den_sample = 0u; den_sample < SAMPLES_PER_BAND; ++den_sample) {
            size_t num_bits = band_sample_bits(numerator_band, num_sample);
            size_t den_bits = band_sample_bits(denominator_band, den_sample);
            mrational_t *src = make_band_rational(num_bits, den_bits, (unsigned)(211u + 17u * num_sample + den_sample));
            mrational_t *warm = NULL;
            uint64_t start;
            uint64_t end;

            if (!src) {
                fprintf(stderr, "unary cell input generation failed for bits=%zu/%zu\n", num_bits, den_bits);
                return -1.0;
            }

            warm = mr_clone(src);
            if (!warm || fn(warm) != 0) {
                fprintf(stderr, "unary warmup failed for bits=%zu/%zu\n", num_bits, den_bits);
                mr_free(warm);
                mr_free(src);
                return -1.0;
            }
            mr_free(warm);

            start = now_ns();
            for (int i = 0; i < iters; ++i) {
                mrational_t *value = mr_clone(src);

                if (!value || fn(value) != 0) {
                    fprintf(stderr, "unary timed run failed for bits=%zu/%zu\n", num_bits, den_bits);
                    mr_free(value);
                    mr_free(src);
                    return -1.0;
                }
                mr_free(value);
            }
            end = now_ns();

            total_ns += end - start;
            total_runs += (size_t)iters;
            mr_free(src);
        }
    }

    return total_runs == 0u ? -1.0 : ((double)total_ns / (double)total_runs) / 1000000.0;
}

static void print_matrix_header(void)
{
    printf("| Operation | Num bits | Den 1-256 | Den 257-512 | Den 513-768 | Den 769-1024 |\n");
    printf("|---|---:|---:|---:|---:|---:|\n");
}

static void run_binary_operation_matrix(const char *name, mr_binary_fn fn, int iters)
{
    double cell_ms[BAND_COUNT][BAND_COUNT];

    if (!bench_wants_operation(name))
        return;

    for (size_t row = 0u; row < BAND_COUNT; ++row) {
        for (size_t col = 0u; col < BAND_COUNT; ++col)
            cell_ms[row][col] = measure_binary_cell(fn, row, col, iters);
    }

    for (size_t row = 0u; row < BAND_COUNT; ++row) {
        printf("| `%s` | `%s` |", name, bit_bands[row].label);
        for (size_t col = 0u; col < BAND_COUNT; ++col)
            printf(" `%.3f ms` |", cell_ms[row][col]);
        putchar('\n');
    }
}

static void run_unary_operation_matrix(const char *name, mr_unary_fn fn, int iters)
{
    double cell_ms[BAND_COUNT][BAND_COUNT];

    if (!bench_wants_operation(name))
        return;

    for (size_t row = 0u; row < BAND_COUNT; ++row) {
        for (size_t col = 0u; col < BAND_COUNT; ++col)
            cell_ms[row][col] = measure_unary_cell(fn, row, col, iters);
    }

    for (size_t row = 0u; row < BAND_COUNT; ++row) {
        printf("| `%s` | `%s` |", name, bit_bands[row].label);
        for (size_t col = 0u; col < BAND_COUNT; ++col)
            printf(" `%.3f ms` |", cell_ms[row][col]);
        putchar('\n');
    }
}

int main(void)
{
    const int add_sub_iters = bench_scaled_iters(200);
    const int mul_iters = bench_scaled_iters(160);
    const int div_iters = bench_scaled_iters(80);
    const int inv_iters = bench_scaled_iters(120);

    puts("Scale iterations with MARS_BENCH_SCALE=<n> if you want longer runs.");
    puts("Filter operations with MARS_BENCH_FILTER=<substring>.");
    puts("Each cell averages representative exact rationals sampled within the bit band.");
    putchar('\n');

    print_matrix_header();
    run_binary_operation_matrix("mr_add", mr_add, add_sub_iters);
    run_binary_operation_matrix("mr_sub", mr_sub, add_sub_iters);
    run_binary_operation_matrix("mr_mul", mr_mul, mul_iters);
    run_binary_operation_matrix("mr_div", mr_div, div_iters);
    run_unary_operation_matrix("mr_inv", mr_inv, inv_iters);
    return 0;
}
