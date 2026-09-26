#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "expression.h"

enum { SAMPLE_COUNT = 7, DEFAULT_ITERATIONS = 1000 };

static const struct {
    const char *name;
    const char *sources[6];
} workloads[] = {
    {"ascii_calls", {"sin(x)", "besseli(n,x)", "struvel(n,x)", "normalpdf(x)", "gamma(x)", "zeta(x)"}},
    {"unicode_calls", {"Γ(x)", "ζ(x)", "𝐇(n,x)", "𝐋(n,x)", "ψ(0,x)", "ℋ(n,x)"}},
    {"misses", {"x+y", "a*b+c", "p^2+q^2", "x_1+x_2", "[unregistered](x)", "s*i*n"}},
    {"powers_aliases", {"sin²(x)", "sin^n(x)", "W_-1(x)", "cos^-1(x)", "ln|x|", "sin⁻¹(x)"}},
};

static double now_seconds(void)
{
    struct timespec stamp;
    clock_gettime(CLOCK_MONOTONIC, &stamp);
    return (double)stamp.tv_sec + (double)stamp.tv_nsec * 1e-9;
}

static int compare_samples(const void *a, const void *b)
{
    double left = *(const double *)a, right = *(const double *)b;
    return (left > right) - (left < right);
}

static int parse_workload(size_t index, long iterations)
{
    for (long repeat = 0; repeat < iterations; ++repeat) {
        for (size_t item = 0u; item < 6u; ++item) {
            expr_t *expr = expr_from_string(workloads[index].sources[item], NULL);
            if (!expr) {
                fprintf(stderr, "Failed to parse %s\n", workloads[index].sources[item]);
                return 1;
            }
            expr_free(expr);
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    bool check = argc == 2 && strcmp(argv[1], "--check") == 0;
    long iterations = DEFAULT_ITERATIONS;
    if (argc == 2 && !check) {
        char *end = NULL;
        iterations = strtol(argv[1], &end, 10);
        if (!end || *end || iterations < 1 || iterations > 1000000)
            return 2;
    } else if (argc > 2) {
        return 2;
    }
    for (size_t group = 0u; group < sizeof(workloads) / sizeof(workloads[0]); ++group) {
        if (parse_workload(group, 1))
            return 1;
        if (check)
            continue;
        double samples[SAMPLE_COUNT];
        for (size_t sample = 0u; sample < SAMPLE_COUNT; ++sample) {
            double start = now_seconds();
            if (parse_workload(group, iterations))
                return 1;
            samples[sample] = (now_seconds() - start) * 1e9 / ((double)iterations * 6.0);
        }
        qsort(samples, SAMPLE_COUNT, sizeof(samples[0]), compare_samples);
        printf("%-16s median_ns_per_parse=%.1f min=%.1f max=%.1f\n", workloads[group].name,
               samples[SAMPLE_COUNT / 2], samples[0], samples[SAMPLE_COUNT - 1]);
    }
    if (check)
        puts("parser benchmark: 24 inputs verified");
    return 0;
}
