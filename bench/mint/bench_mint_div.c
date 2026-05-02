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

static void run_div_case(const char *label,
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
    if (mi_div(warm, rhs, NULL) != 0) {
        fprintf(stderr, "%s warmup divide failed\n", label);
        mi_free(warm);
        return;
    }
    mi_free(warm);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *value = mi_clone(lhs_src);

        if (!value || mi_div(value, rhs, NULL) != 0) {
            fprintf(stderr, "%s timed divide failed\n", label);
            mi_free(value);
            return;
        }
        mi_free(value);
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s avg_µs=%10.3f avg_ms=%10.3f\n",
           label,
           avg_us,
           avg_us / 1000.0);
}

static void run_div_case_arith(const char *label,
                               const mint_t *lhs_src,
                               const mint_t *rhs,
                               int iters)
{
    uint64_t reset_start, reset_end, work_start, work_end;
    double avg_us;

    reset_start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *value = mi_clone(lhs_src);

        if (!value) {
            fprintf(stderr, "%s arith reset-only clone failed\n", label);
            return;
        }
        mi_free(value);
    }
    reset_end = now_ns();

    work_start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *value = mi_clone(lhs_src);

        if (!value || mi_div(value, rhs, NULL) != 0) {
            fprintf(stderr, "%s arith timed divide failed\n", label);
            mi_free(value);
            return;
        }
        mi_free(value);
    }
    work_end = now_ns();

    {
        uint64_t reset_ns = reset_end - reset_start;
        uint64_t work_ns = work_end - work_start;
        uint64_t arith_ns = work_ns > reset_ns ? (work_ns - reset_ns) : 0;

        avg_us = ((double)arith_ns / (double)iters) / 1000.0;
    }
    printf("%-28s avg_µs=%10.3f avg_ms=%10.3f\n",
           label,
           avg_us,
           avg_us / 1000.0);
}

static void run_div_long_case(const char *label,
                              const mint_t *src,
                              long divisor,
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
    if (mi_div_long(warm, divisor, NULL) != 0) {
        fprintf(stderr, "%s warmup div_long failed\n", label);
        mi_free(warm);
        return;
    }
    mi_free(warm);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *value = mi_clone(src);

        if (!value || mi_div_long(value, divisor, NULL) != 0) {
            fprintf(stderr, "%s timed div_long failed\n", label);
            mi_free(value);
            return;
        }
        mi_free(value);
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s avg_µs=%10.3f avg_ms=%10.3f\n",
           label,
           avg_us,
           avg_us / 1000.0);
}

static void run_mod_long_case(const char *label,
                              const mint_t *src,
                              long divisor,
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
    if (mi_mod_long(warm, divisor) != 0) {
        fprintf(stderr, "%s warmup mod_long failed\n", label);
        mi_free(warm);
        return;
    }
    mi_free(warm);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *value = mi_clone(src);

        if (!value || mi_mod_long(value, divisor) != 0) {
            fprintf(stderr, "%s timed mod_long failed\n", label);
            mi_free(value);
            return;
        }
        mi_free(value);
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s avg_µs=%10.3f avg_ms=%10.3f\n",
           label,
           avg_us,
           avg_us / 1000.0);
}

static void run_mod_case(const char *label,
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
    if (mi_mod(warm, rhs) != 0) {
        fprintf(stderr, "%s warmup mod failed\n", label);
        mi_free(warm);
        return;
    }
    mi_free(warm);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *value = mi_clone(lhs_src);

        if (!value || mi_mod(value, rhs) != 0) {
            fprintf(stderr, "%s timed mod failed\n", label);
            mi_free(value);
            return;
        }
        mi_free(value);
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s avg_µs=%10.3f avg_ms=%10.3f\n",
           label,
           avg_us,
           avg_us / 1000.0);
}

static void run_mod_case_arith(const char *label,
                               const mint_t *lhs_src,
                               const mint_t *rhs,
                               int iters)
{
    uint64_t reset_start, reset_end, work_start, work_end;
    double avg_us;

    reset_start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *value = mi_clone(lhs_src);

        if (!value) {
            fprintf(stderr, "%s arith reset-only clone failed\n", label);
            return;
        }
        mi_free(value);
    }
    reset_end = now_ns();

    work_start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *value = mi_clone(lhs_src);

        if (!value || mi_mod(value, rhs) != 0) {
            fprintf(stderr, "%s arith timed mod failed\n", label);
            mi_free(value);
            return;
        }
        mi_free(value);
    }
    work_end = now_ns();

    {
        uint64_t reset_ns = reset_end - reset_start;
        uint64_t work_ns = work_end - work_start;
        uint64_t arith_ns = work_ns > reset_ns ? (work_ns - reset_ns) : 0;

        avg_us = ((double)arith_ns / (double)iters) / 1000.0;
    }
    printf("%-28s avg_µs=%10.3f avg_ms=%10.3f\n",
           label,
           avg_us,
           avg_us / 1000.0);
}

int main(void)
{
    const int iters = 20000;
    mint_t *one = mi_create_long(1);
    mint_t *minus_one = mi_create_long(-1);
    mint_t *ten = mi_create_long(10);
    mint_t *five = mi_create_long(5);
    mint_t *small = mi_create_string("18446744073709551621");
    mint_t *big = mi_create_hex("123456789abcdef00112233445566778899aabbccddeeff0011223344556677");
    mint_t *wide = mi_create_hex("fedcba9876543210ffeeddccbbaa99887766554433221100ffeeddccbbaa9988");
    mint_t *wide_divisor = mi_create_hex("123456789abcdef0011223344556677");
    mint_t *pow2_64 = mi_create_string("18446744073709551616");
    mint_t *pow2_127 = mi_create_string("170141183460469231731687303715884105728");

    if (!one || !minus_one || !ten || !five || !small || !big || !wide ||
        !wide_divisor || !pow2_64 || !pow2_127) {
        fprintf(stderr, "benchmark setup failed\n");
        mi_free(one);
        mi_free(minus_one);
        mi_free(ten);
        mi_free(five);
        mi_free(small);
        mi_free(big);
        mi_free(wide);
        mi_free(wide_divisor);
        mi_free(pow2_64);
        mi_free(pow2_127);
        return 1;
    }

    printf("iters=%d\n", iters);
    printf("\nMint division\n");
    run_div_case("div_by_one_big", big, one, iters);
    run_div_case("div_by_minus_one_big", big, minus_one, iters);
    run_div_case("div_test_case", small, ten, iters);
    run_div_case("div_wide_by_small", wide, five, iters);
    run_div_case("div_big_by_wide", big, wide_divisor, iters);
    run_div_case("div_big_by_2pow64", big, pow2_64, iters);
    run_div_case("div_big_by_2pow127", big, pow2_127, iters);

    printf("\nMint division (arith only)\n");
    run_div_case_arith("div_by_one_big", big, one, iters);
    run_div_case_arith("div_by_minus_one_big", big, minus_one, iters);
    run_div_case_arith("div_test_case", small, ten, iters);
    run_div_case_arith("div_wide_by_small", wide, five, iters);
    run_div_case_arith("div_big_by_wide", big, wide_divisor, iters);
    run_div_case_arith("div_big_by_2pow64", big, pow2_64, iters);
    run_div_case_arith("div_big_by_2pow127", big, pow2_127, iters);

    printf("\nMint division by long\n");
    run_div_long_case("div_long_minus_one", big, -1, iters);
    run_div_long_case("div_long_small", big, 37, iters);

    printf("\nMint modulo\n");
    run_mod_case("mod_test_case", small, ten, iters);
    run_mod_case("mod_wide_by_small", wide, five, iters);
    run_mod_case("mod_big_by_wide", big, wide_divisor, iters);
    run_mod_case("mod_big_by_2pow64", big, pow2_64, iters);
    run_mod_case("mod_big_by_2pow127", big, pow2_127, iters);

    printf("\nMint modulo (arith only)\n");
    run_mod_case_arith("mod_test_case", small, ten, iters);
    run_mod_case_arith("mod_wide_by_small", wide, five, iters);
    run_mod_case_arith("mod_big_by_wide", big, wide_divisor, iters);
    run_mod_case_arith("mod_big_by_2pow64", big, pow2_64, iters);
    run_mod_case_arith("mod_big_by_2pow127", big, pow2_127, iters);

    printf("\nMint modulo by long\n");
    run_mod_long_case("mod_long_small", big, 37, iters);
    run_mod_long_case("mod_long_neg", big, -37, iters);

    mi_free(wide);
    mi_free(big);
    mi_free(small);
    mi_free(five);
    mi_free(ten);
    mi_free(minus_one);
    mi_free(one);
    mi_free(wide_divisor);
    mi_free(pow2_127);
    mi_free(pow2_64);
    return 0;
}
