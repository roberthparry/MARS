#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mint.h"

typedef int (*mi_binary_fn)(mint_t *, const mint_t *);

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

static mint_t *make_exact_bits(size_t bits, unsigned salt)
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

    if (bits == 2u)
        extra = 1l;
    else
        extra = 1l + 2l * (long)(salt % 7u);

    if (mi_add_long(value, extra) != 0) {
        mi_free(value);
        return NULL;
    }
    if (mi_bit_length(value) != bits) {
        mi_free(value);
        return NULL;
    }
    return value;
}

static mint_t *make_band_mint(size_t bits, unsigned salt)
{
    for (unsigned attempt = 0u; attempt < 16u; ++attempt) {
        mint_t *value = make_exact_bits(bits, salt + attempt);

        if (value)
            return value;
    }
    return NULL;
}

static int mi_div_no_rem(mint_t *mint, const mint_t *other)
{
    return mi_div(mint, other, NULL);
}

static double measure_binary_cell(mi_binary_fn fn,
                                  size_t lhs_band,
                                  size_t rhs_band,
                                  int iters)
{
    uint64_t total_ns = 0ull;
    size_t total_runs = 0u;

    for (size_t lhs_sample = 0u; lhs_sample < SAMPLES_PER_BAND; ++lhs_sample) {
        for (size_t rhs_sample = 0u; rhs_sample < SAMPLES_PER_BAND; ++rhs_sample) {
            size_t lhs_bits = band_sample_bits(lhs_band, lhs_sample);
            size_t rhs_bits = band_sample_bits(rhs_band, rhs_sample);
            mint_t *lhs = make_band_mint(lhs_bits, (unsigned)(11u * lhs_sample + rhs_sample));
            mint_t *rhs = make_band_mint(rhs_bits, (unsigned)(97u + 13u * lhs_sample + rhs_sample));
            mint_t *warm = NULL;
            uint64_t start;
            uint64_t end;

            if (!lhs || !rhs) {
                fprintf(stderr, "binary cell input generation failed for bits=%zu/%zu\n",
                        lhs_bits, rhs_bits);
                mi_free(lhs);
                mi_free(rhs);
                return -1.0;
            }

            warm = mi_clone(lhs);
            if (!warm || fn(warm, rhs) != 0) {
                fprintf(stderr, "binary warmup failed for bits=%zu/%zu\n", lhs_bits, rhs_bits);
                mi_free(warm);
                mi_free(lhs);
                mi_free(rhs);
                return -1.0;
            }
            mi_free(warm);

            start = now_ns();
            for (int i = 0; i < iters; ++i) {
                mint_t *value = mi_clone(lhs);

                if (!value || fn(value, rhs) != 0) {
                    fprintf(stderr, "binary timed run failed for bits=%zu/%zu\n",
                            lhs_bits, rhs_bits);
                    mi_free(value);
                    mi_free(lhs);
                    mi_free(rhs);
                    return -1.0;
                }
                mi_free(value);
            }
            end = now_ns();

            total_ns += end - start;
            total_runs += (size_t)iters;
            mi_free(lhs);
            mi_free(rhs);
        }
    }

    return total_runs == 0u ? -1.0 : (double)total_ns / (double)total_runs;
}

static void print_matrix_header(void)
{
    printf("| Operation | Left bits | Right 1-256 | Right 257-512 | Right 513-768 | Right 769-1024 |\n");
    printf("|---|---:|---:|---:|---:|---:|\n");
}

static void run_binary_operation_matrix(const char *name, mi_binary_fn fn, int iters)
{
    double cell_ns[BAND_COUNT][BAND_COUNT];

    if (!bench_wants_operation(name))
        return;

    for (size_t row = 0u; row < BAND_COUNT; ++row) {
        for (size_t col = 0u; col < BAND_COUNT; ++col)
            cell_ns[row][col] = measure_binary_cell(fn, row, col, iters);
    }

    for (size_t row = 0u; row < BAND_COUNT; ++row) {
        printf("| `%s` | `%s` |", name, bit_bands[row].label);
        for (size_t col = 0u; col < BAND_COUNT; ++col)
            printf(" `%.1f ns` |", cell_ns[row][col]);
        putchar('\n');
    }
}

int main(void)
{
    const int add_sub_iters = bench_scaled_iters(400);
    const int mul_iters = bench_scaled_iters(240);
    const int div_mod_iters = bench_scaled_iters(120);
    const int gcd_lcm_iters = bench_scaled_iters(80);

    puts("Scale iterations with MARS_BENCH_SCALE=<n> if you want longer runs.");
    puts("Filter operations with MARS_BENCH_FILTER=<substring>.");
    puts("Each cell averages representative exact integers sampled within the bit band.");
    putchar('\n');

    print_matrix_header();
    run_binary_operation_matrix("mi_add", mi_add, add_sub_iters);
    run_binary_operation_matrix("mi_sub", mi_sub, add_sub_iters);
    run_binary_operation_matrix("mi_mul", mi_mul, mul_iters);
    run_binary_operation_matrix("mi_div", mi_div_no_rem, div_mod_iters);
    run_binary_operation_matrix("mi_mod", mi_mod, div_mod_iters);
    run_binary_operation_matrix("mi_gcd", mi_gcd, gcd_lcm_iters);
    run_binary_operation_matrix("mi_lcm", mi_lcm, gcd_lcm_iters);
    return 0;
}
