#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

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
    { 769u, 1024u, "769-1024" },
    { 1025u, 2048u, "1025-2048" },
    { 2049u, 4096u, "2049-4096" }
};

#define BAND_COUNT (sizeof(bit_bands) / sizeof(bit_bands[0]))
#define SAMPLES_PER_BAND 4u

typedef struct bench_stats_t {
    double estimate;
    double mad;
    double ci_low;
    double ci_high;
    int samples;
} bench_stats_t;

static int bench_docs_table_enabled(void);

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

static int bench_repeat_count(void)
{
    const char *repeat_text = getenv("MARS_BENCH_REPEATS");
    long repeats = bench_docs_table_enabled() ? 51 : 31;

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

static bench_stats_t bench_estimate_u64(const uint64_t *values, int count)
{
    bench_stats_t stats = {0};
    double *converted;

    if (count <= 0)
        return stats;

    converted = (double *)malloc((size_t)count * sizeof(*converted));
    if (!converted)
        return stats;
    for (int i = 0; i < count; ++i)
        converted[i] = (double)values[i];
    stats = bench_estimate_double(converted, count);
    free(converted);
    return stats;
}

static int bench_wants_operation(const char *name)
{
    const char *filter = getenv("MARS_BENCH_FILTER");

    if (!filter || !*filter)
        return 1;
    return strstr(name, filter) != NULL;
}

static int bench_docs_table_enabled(void)
{
    const char *format = getenv("MARS_BENCH_FORMAT");

    return format && strcmp(format, "md") == 0;
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
    const int repeats = bench_repeat_count();
    double cell_estimates[SAMPLES_PER_BAND * SAMPLES_PER_BAND];
    size_t total_runs = 0u;

    for (size_t num_sample = 0u; num_sample < SAMPLES_PER_BAND; ++num_sample) {
        for (size_t den_sample = 0u; den_sample < SAMPLES_PER_BAND; ++den_sample) {
            size_t num_bits = band_sample_bits(numerator_band, num_sample);
            size_t den_bits = band_sample_bits(denominator_band, den_sample);
            mrational_t *lhs = make_band_rational(num_bits, den_bits, (unsigned)(11u * num_sample + den_sample));
            mrational_t *rhs = make_band_rational(num_bits, den_bits, (unsigned)(97u + 13u * num_sample + den_sample));
            mrational_t *warm = NULL;
            mrational_t **values = NULL;
            uint64_t *samples = NULL;
            int i;

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

            values = (mrational_t **)calloc((size_t)iters, sizeof(*values));
            if (!values) {
                fprintf(stderr, "binary work array alloc failed for bits=%zu/%zu\n", num_bits, den_bits);
                mr_free(lhs);
                mr_free(rhs);
                return -1.0;
            }
            for (i = 0; i < iters; ++i) {
                values[i] = mr_clone(lhs);
                if (!values[i]) {
                    fprintf(stderr, "binary work clone failed for bits=%zu/%zu\n", num_bits, den_bits);
                    for (int j = 0; j < i; ++j)
                        mr_free(values[j]);
                    free(values);
                    mr_free(lhs);
                    mr_free(rhs);
                    return -1.0;
                }
            }
            samples = (uint64_t *)calloc((size_t)repeats, sizeof(*samples));
            if (!samples) {
                fprintf(stderr, "repeat sample alloc failed for bits=%zu/%zu\n", num_bits, den_bits);
                for (i = 0; i < iters; ++i)
                    mr_free(values[i]);
                free(values);
                mr_free(lhs);
                mr_free(rhs);
                return -1.0;
            }

            {
                for (int repeat = 0; repeat < repeats; ++repeat) {
                    uint64_t start;
                    uint64_t end;

                    for (i = 0; i < iters; ++i) {
                        mr_free(values[i]);
                        values[i] = mr_clone(lhs);
                        if (!values[i]) {
                            fprintf(stderr, "binary work clone failed for bits=%zu/%zu\n", num_bits, den_bits);
                            for (int j = 0; j < iters; ++j)
                                mr_free(values[j]);
                            free(values);
                            free(samples);
                            mr_free(lhs);
                            mr_free(rhs);
                            return -1.0;
                        }
                    }

                    start = now_ns();
                    for (i = 0; i < iters; ++i) {
                        if (fn(values[i], rhs) != 0) {
                            fprintf(stderr, "binary timed run failed for bits=%zu/%zu\n", num_bits, den_bits);
                            for (int j = 0; j < iters; ++j)
                                mr_free(values[j]);
                            free(values);
                            free(samples);
                            mr_free(lhs);
                            mr_free(rhs);
                            return -1.0;
                        }
                    }
                    end = now_ns();
                    samples[repeat] = end - start;
                }

                cell_estimates[total_runs++] = bench_estimate_u64(samples, repeats).estimate;
            }
            for (i = 0; i < iters; ++i)
                mr_free(values[i]);
            free(values);
            free(samples);
            mr_free(lhs);
            mr_free(rhs);
        }
    }

    return total_runs == 0u ? -1.0 : bench_estimate_double(cell_estimates, (int)total_runs).estimate / (double)iters;
}

static double measure_unary_cell(mr_unary_fn fn,
                                 size_t numerator_band,
                                 size_t denominator_band,
                                 int iters)
{
    const int repeats = bench_repeat_count();
    double cell_estimates[SAMPLES_PER_BAND * SAMPLES_PER_BAND];
    size_t total_runs = 0u;

    for (size_t num_sample = 0u; num_sample < SAMPLES_PER_BAND; ++num_sample) {
        for (size_t den_sample = 0u; den_sample < SAMPLES_PER_BAND; ++den_sample) {
            size_t num_bits = band_sample_bits(numerator_band, num_sample);
            size_t den_bits = band_sample_bits(denominator_band, den_sample);
            mrational_t *src = make_band_rational(num_bits, den_bits, (unsigned)(211u + 17u * num_sample + den_sample));
            mrational_t *warm = NULL;
            mrational_t **values = NULL;
            uint64_t *samples = NULL;
            int i;

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

            values = (mrational_t **)calloc((size_t)iters, sizeof(*values));
            if (!values) {
                fprintf(stderr, "unary work array alloc failed for bits=%zu/%zu\n", num_bits, den_bits);
                mr_free(src);
                return -1.0;
            }
            for (i = 0; i < iters; ++i) {
                values[i] = mr_clone(src);
                if (!values[i]) {
                    fprintf(stderr, "unary work clone failed for bits=%zu/%zu\n", num_bits, den_bits);
                    for (int j = 0; j < i; ++j)
                        mr_free(values[j]);
                    free(values);
                    mr_free(src);
                    return -1.0;
                }
            }
            samples = (uint64_t *)calloc((size_t)repeats, sizeof(*samples));
            if (!samples) {
                fprintf(stderr, "repeat sample alloc failed for bits=%zu/%zu\n", num_bits, den_bits);
                for (i = 0; i < iters; ++i)
                    mr_free(values[i]);
                free(values);
                mr_free(src);
                return -1.0;
            }

            {
                for (int repeat = 0; repeat < repeats; ++repeat) {
                    uint64_t start;
                    uint64_t end;

                    for (i = 0; i < iters; ++i) {
                        mr_free(values[i]);
                        values[i] = mr_clone(src);
                        if (!values[i]) {
                            fprintf(stderr, "unary work clone failed for bits=%zu/%zu\n", num_bits, den_bits);
                            for (int j = 0; j < iters; ++j)
                                mr_free(values[j]);
                            free(values);
                            free(samples);
                            mr_free(src);
                            return -1.0;
                        }
                    }

                    start = now_ns();
                    for (i = 0; i < iters; ++i) {
                        if (fn(values[i]) != 0) {
                            fprintf(stderr, "unary timed run failed for bits=%zu/%zu\n", num_bits, den_bits);
                            for (int j = 0; j < iters; ++j)
                                mr_free(values[j]);
                            free(values);
                            free(samples);
                            mr_free(src);
                            return -1.0;
                        }
                    }
                    end = now_ns();
                    samples[repeat] = end - start;
                }

                cell_estimates[total_runs++] = bench_estimate_u64(samples, repeats).estimate;
            }
            for (i = 0; i < iters; ++i)
                mr_free(values[i]);
            free(values);
            free(samples);
            mr_free(src);
        }
    }

    return total_runs == 0u ? -1.0 : bench_estimate_double(cell_estimates, (int)total_runs).estimate / (double)iters;
}

static void print_matrix_header(void)
{
    if (bench_docs_table_enabled()) {
        puts("<table>");
        puts("<thead>");
        puts("<tr>");
        puts("<th rowspan=\"2\">Operation</th>");
        puts("<th rowspan=\"2\" style=\"text-align: center;\">Numerator bits</th>");
        printf("<th colspan=\"%zu\" style=\"text-align: center;\">Denominator bits</th>\n", BAND_COUNT);
        puts("</tr>");
        puts("<tr>");
        for (size_t col = 0u; col < BAND_COUNT; ++col) {
            printf("<th style=\"text-align: center;\"><span style=\"display: inline-block; text-align: center; font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;\">%10s</span></th>\n",
                   bit_bands[col].label);
        }
        puts("</tr>");
        puts("</thead>");
        puts("<tbody>");
        return;
    }

    printf("| Operation | Num bits");
    for (size_t col = 0u; col < BAND_COUNT; ++col)
        printf(" | Den %s", bit_bands[col].label);
    printf(" |\n");
    printf("|---|---:");
    for (size_t col = 0u; col < BAND_COUNT; ++col)
        printf("|---:");
    printf("|\n");
}

static void print_matrix_footer(void)
{
    if (!bench_docs_table_enabled())
        return;

    puts("</tbody>");
    puts("</table>");
}

static void format_duration_ns(double value_ns, char *buffer, size_t buffer_size)
{
    if (value_ns < 1000.0)
        snprintf(buffer, buffer_size, "%.1f ns", value_ns);
    else if (value_ns < 1000000.0)
        snprintf(buffer, buffer_size, "%.3f µs", value_ns / 1000.0);
    else if (value_ns < 1000000000.0)
        snprintf(buffer, buffer_size, "%.3f ms", value_ns / 1000000.0);
    else
        snprintf(buffer, buffer_size, "%.3f s", value_ns / 1000000000.0);
}

static void print_matrix_operation_row(const char *name, size_t row, const double cell_ns[BAND_COUNT][BAND_COUNT])
{
    size_t col;

    if (!bench_docs_table_enabled()) {
        printf("| `%s` | `%s` |", name, bit_bands[row].label);
        for (col = 0u; col < BAND_COUNT; ++col) {
            char formatted[32];

            format_duration_ns(cell_ns[row][col], formatted, sizeof(formatted));
            printf(" `%s` |", formatted);
        }
        putchar('\n');
        return;
    }

    printf("<tr>");
    if (row == 0u)
        printf("<td rowspan=\"%zu\"><span style=\"font-size: 0.88em;\"><strong>%s</strong></span></td>", BAND_COUNT, name);
    printf("<td><span style=\"font-family: monospace; white-space: pre; font-size: 0.76em; font-weight: 700;\">%s</span></td>",
           bit_bands[row].label);
    for (col = 0u; col < BAND_COUNT; ++col) {
        char formatted[32];

        format_duration_ns(cell_ns[row][col], formatted, sizeof(formatted));
        printf("<td style=\"text-align: center;\"><span style=\"font-family: monospace; font-size: 0.74em;\">%s</span></td>",
               formatted);
    }
    puts("</tr>");
}

static void run_binary_operation_matrix(const char *name, mr_binary_fn fn, int iters)
{
    double cell_ns[BAND_COUNT][BAND_COUNT];

    if (!bench_wants_operation(name))
        return;

    for (size_t row = 0u; row < BAND_COUNT; ++row) {
        for (size_t col = 0u; col < BAND_COUNT; ++col)
            cell_ns[row][col] = measure_binary_cell(fn, row, col, iters);
    }

    for (size_t row = 0u; row < BAND_COUNT; ++row)
        print_matrix_operation_row(name, row, cell_ns);
}

static void run_unary_operation_matrix(const char *name, mr_unary_fn fn, int iters)
{
    double cell_ns[BAND_COUNT][BAND_COUNT];

    if (!bench_wants_operation(name))
        return;

    for (size_t row = 0u; row < BAND_COUNT; ++row) {
        for (size_t col = 0u; col < BAND_COUNT; ++col)
            cell_ns[row][col] = measure_unary_cell(fn, row, col, iters);
    }

    for (size_t row = 0u; row < BAND_COUNT; ++row)
        print_matrix_operation_row(name, row, cell_ns);
}

int main(void)
{
    const int add_sub_iters = bench_scaled_iters(200);
    const int mul_iters = bench_scaled_iters(160);
    const int div_iters = bench_scaled_iters(80);
    const int inv_iters = bench_scaled_iters(120);

    if (!bench_docs_table_enabled()) {
        puts("Scale iterations with MARS_BENCH_SCALE=<n> if you want longer runs.");
        puts("Filter operations with MARS_BENCH_FILTER=<substring>.");
        puts("Tune repeats with MARS_BENCH_REPEATS=<n> and bootstrap resamples with MARS_BENCH_BOOTSTRAP=<n>.");
        puts("Each cell reports a robust median across representative inputs, with each input estimated by the sample median timed batch.");
        putchar('\n');
    }

    print_matrix_header();
    run_binary_operation_matrix("mr_add", mr_add, add_sub_iters);
    run_binary_operation_matrix("mr_sub", mr_sub, add_sub_iters);
    run_binary_operation_matrix("mr_mul", mr_mul, mul_iters);
    run_binary_operation_matrix("mr_div", mr_div, div_iters);
    run_unary_operation_matrix("mr_inv", mr_inv, inv_iters);
    print_matrix_footer();
    return 0;
}
