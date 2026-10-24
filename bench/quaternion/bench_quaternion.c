/**
 * @file bench_quaternion.c
 * @brief Compare checked direct and eight-product quaternion kernels, including rounding limitations.
 *
 * Runs serial native timings with dependent rotations and identical validation
 * semantics. The experimental Howell-Lafon factorisation follows Algorithm 41
 * of Dumas, Pernet and Sedoglavic, https://arxiv.org/abs/2101.01025. It is not a
 * library implementation: its extra additions can discard small components or
 * overflow intermediate sums even when the direct product is representable.
 * Counts exclude checks and memory operations: direct 16 products + 12 sums;
 * factored 8 variable products + 28 sums + 4 multiplications by one half.
 */
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include "quaternion-t.h"

static bool factored_product(quaternion_t output, const quaternion_t a, const quaternion_t b)
{
    if (!output || !a || !b) return false;
    double p1 = (a[3] + a[0]) * (b[3] + b[0]);
    double p2 = (a[2] - a[1]) * (b[1] - b[2]);
    double p3 = (a[0] - a[3]) * (b[1] + b[2]);
    double p4 = (a[1] + a[2]) * (b[0] - b[3]);
    double p5 = (a[0] + a[2]) * (b[0] + b[1]);
    double p6 = (a[0] - a[2]) * (b[0] - b[1]);
    double p7 = (a[3] + a[1]) * (b[3] - b[2]);
    double p8 = (a[3] - a[1]) * (b[3] + b[2]);
    double sum56 = p5 + p6, sum78 = p7 + p8, difference56 = p5 - p6, difference78 = p7 - p8;
    double x = p1 - (sum56 + sum78) * .5;
    double y = (difference56 + difference78) * .5 - p3;
    double z = (difference56 - difference78) * .5 - p4;
    double w = p2 + (sum78 - sum56) * .5;
    if (!isfinite(x) || !isfinite(y) || !isfinite(z) || !isfinite(w)) return false;
    output[0] = x; output[1] = y; output[2] = z; output[3] = w;
    return true;
}

static bool check_products(void)
{
    /* All 3^8 small integer inputs are exactly representable in both algorithms. */
    for (unsigned sample = 0; sample < 6561; ++sample) {
        unsigned digits = sample;
        quaternion_t a, b, direct, factored;
        for (unsigned i = 0; i < 4; ++i) { a[i] = (int)(digits % 3) - 1; digits /= 3; }
        for (unsigned i = 0; i < 4; ++i) { b[i] = (int)(digits % 3) - 1; digits /= 3; }
        if (!quaternion_multiply(direct, a, b) || !factored_product(factored, a, b)) return false;
        for (unsigned i = 0; i < 4; ++i) if (direct[i] != factored[i]) return false;
    }
    quaternion_t small = {1e-20, 0, 0, 1}, identity = {0, 0, 0, 1}, direct, factored;
    if (!quaternion_multiply(direct, small, identity) || !factored_product(factored, small, identity)) return false;
    printf("Tiny x component: direct %.17g, factored %.17g\n", direct[0], factored[0]);
    quaternion_t large = {DBL_MAX, 0, 0, DBL_MAX}, scale = {0, 0, 0, DBL_MIN};
    bool direct_ok = quaternion_multiply(direct, large, scale), factored_ok = factored_product(factored, large, scale);
    printf("Finite extreme product accepted: direct %d, factored %d\n", direct_ok, factored_ok);
    return direct_ok;
}

static bool measure(const char *name, bool (*product)(quaternion_t, const quaternion_t, const quaternion_t))
{
    const unsigned iterations = 2000000;
    const double axis[] = {1, 2, 3};
    quaternion_t step, value;
    quaternion_identity(value);
    if (!quaternion_axis_angle(step, axis, .00001)) return false;
    struct timespec start, end;
    if (clock_gettime(CLOCK_MONOTONIC, &start)) return false;
    for (unsigned i = 0; i < iterations; ++i) if (!product(value, value, step)) return false;
    if (clock_gettime(CLOCK_MONOTONIC, &end)) return false;
    double nanos = (end.tv_sec - start.tv_sec) * 1e9 + end.tv_nsec - start.tv_nsec;
    printf("%-8s %.3f ns/product; checksum %.12f\n", name, nanos / iterations, value[3]);
    return quaternion_is_unit(value, 1e-6);
}

int main(void)
{
    if (!check_products()) return 1;
    for (unsigned pass = 0; pass < 6; ++pass) {
        bool ok = pass % 2 ? measure("factored", factored_product) && measure("direct", quaternion_multiply)
                           : measure("direct", quaternion_multiply) && measure("factored", factored_product);
        if (!ok) return 1;
    }
    return 0;
}
