#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "mint.h"

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void run_gcd_case(const char *label,
                         const mint_t *lhs_src,
                         const mint_t *rhs,
                         int iters)
{
    mint_t *warm = mi_clone(lhs_src);
    uint64_t start;
    uint64_t end;
    double avg_us;

    if (!warm) {
        fprintf(stderr, "%s warmup clone failed\n", label);
        return;
    }
    if (mi_gcd(warm, rhs) != 0) {
        fprintf(stderr, "%s warmup gcd failed\n", label);
        mi_free(warm);
        return;
    }
    mi_free(warm);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *value = mi_clone(lhs_src);

        if (!value || mi_gcd(value, rhs) != 0) {
            fprintf(stderr, "%s timed gcd failed\n", label);
            mi_free(value);
            return;
        }
        mi_free(value);
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s avg_µs=%10.3f avg_ms=%10.3f\n",
           label, avg_us, avg_us / 1000.0);
}

static void run_lcm_case(const char *label,
                         const mint_t *lhs_src,
                         const mint_t *rhs,
                         int iters)
{
    mint_t *warm = mi_clone(lhs_src);
    uint64_t start;
    uint64_t end;
    double avg_us;

    if (!warm) {
        fprintf(stderr, "%s warmup clone failed\n", label);
        return;
    }
    if (mi_lcm(warm, rhs) != 0) {
        fprintf(stderr, "%s warmup lcm failed\n", label);
        mi_free(warm);
        return;
    }
    mi_free(warm);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *value = mi_clone(lhs_src);

        if (!value || mi_lcm(value, rhs) != 0) {
            fprintf(stderr, "%s timed lcm failed\n", label);
            mi_free(value);
            return;
        }
        mi_free(value);
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s avg_µs=%10.3f avg_ms=%10.3f\n",
           label, avg_us, avg_us / 1000.0);
}

static void run_modinv_case(const char *label,
                            const mint_t *src,
                            const mint_t *modulus,
                            int iters)
{
    mint_t *warm = mi_clone(src);
    uint64_t start;
    uint64_t end;
    double avg_us;

    if (!warm) {
        fprintf(stderr, "%s warmup clone failed\n", label);
        return;
    }
    if (mi_modinv(warm, modulus) != 0) {
        fprintf(stderr, "%s warmup modinv failed\n", label);
        mi_free(warm);
        return;
    }
    mi_free(warm);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *value = mi_clone(src);

        if (!value || mi_modinv(value, modulus) != 0) {
            fprintf(stderr, "%s timed modinv failed\n", label);
            mi_free(value);
            return;
        }
        mi_free(value);
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s avg_µs=%10.3f avg_ms=%10.3f\n",
           label, avg_us, avg_us / 1000.0);
}

int main(void)
{
    const int iters = 10000;
    mint_t *small_a = mi_create_long(84);
    mint_t *small_b = mi_create_long(30);
    mint_t *wide_a = mi_create_string("1234567890123456789012345678901234567890");
    mint_t *wide_b = mi_create_string("987654321098765432109876543210987654321");
    mint_t *fib_a = mi_create_string("1134903170");
    mint_t *fib_b = mi_create_string("701408733");
    mint_t *inv_small = mi_create_long(3);
    mint_t *inv_mod_small = mi_create_long(11);
    mint_t *inv_wide = mi_create_long(2);
    mint_t *inv_mod_wide = mi_create_string("987654321098765432109876543210987654321");

    if (!small_a || !small_b || !wide_a || !wide_b || !fib_a || !fib_b ||
        !inv_small || !inv_mod_small || !inv_wide || !inv_mod_wide) {
        fprintf(stderr, "failed to create benchmark inputs\n");
        mi_free(small_a);
        mi_free(small_b);
        mi_free(wide_a);
        mi_free(wide_b);
        mi_free(fib_a);
        mi_free(fib_b);
        mi_free(inv_small);
        mi_free(inv_mod_small);
        mi_free(inv_wide);
        mi_free(inv_mod_wide);
        return 1;
    }

    printf("iters=%d\n\n", iters);
    printf("Mint gcd/lcm\n");
    run_gcd_case("gcd_small", small_a, small_b, iters);
    run_gcd_case("gcd_wide", wide_a, wide_b, iters);
    run_gcd_case("gcd_fibonacciish", fib_a, fib_b, iters);
    run_lcm_case("lcm_small", small_a, small_b, iters);
    run_lcm_case("lcm_wide", wide_a, wide_b, iters);
    run_modinv_case("modinv_small", inv_small, inv_mod_small, iters);
    run_modinv_case("modinv_wide", inv_wide, inv_mod_wide, iters);

    mi_free(small_a);
    mi_free(small_b);
    mi_free(wide_a);
    mi_free(wide_b);
    mi_free(fib_a);
    mi_free(fib_b);
    mi_free(inv_small);
    mi_free(inv_mod_small);
    mi_free(inv_wide);
    mi_free(inv_mod_wide);
    return 0;
}
