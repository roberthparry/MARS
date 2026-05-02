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

static void run_mul_case(const char *label,
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
    if (mi_mul(warm, rhs) != 0) {
        fprintf(stderr, "%s warmup multiply failed\n", label);
        mi_free(warm);
        return;
    }
    mi_free(warm);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *value = mi_clone(lhs_src);

        if (!value || mi_mul(value, rhs) != 0) {
            fprintf(stderr, "%s timed multiply failed\n", label);
            mi_free(value);
            break;
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

static void run_mul_case_arith(const char *label,
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

        if (!value || mi_mul(value, rhs) != 0) {
            fprintf(stderr, "%s arith timed multiply failed\n", label);
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
    if (avg_us < 0.0)
        avg_us = 0.0;
    printf("%-28s avg_µs=%10.3f avg_ms=%10.3f\n",
           label,
           avg_us,
           avg_us / 1000.0);
}

static void run_mul_long_case(const char *label,
                              const mint_t *src,
                              long factor,
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
    if (mi_mul_long(warm, factor) != 0) {
        fprintf(stderr, "%s warmup mul_long failed\n", label);
        mi_free(warm);
        return;
    }
    mi_free(warm);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *value = mi_clone(src);

        if (!value || mi_mul_long(value, factor) != 0) {
            fprintf(stderr, "%s timed mul_long failed\n", label);
            mi_free(value);
            break;
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

static void run_mul_long_case_arith(const char *label,
                                    const mint_t *src,
                                    long factor,
                                    int iters)
{
    uint64_t reset_start, reset_end, work_start, work_end;
    double avg_us;

    reset_start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *value = mi_clone(src);

        if (!value) {
            fprintf(stderr, "%s arith reset-only clone failed\n", label);
            return;
        }
        mi_free(value);
    }
    reset_end = now_ns();

    work_start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *value = mi_clone(src);

        if (!value || mi_mul_long(value, factor) != 0) {
            fprintf(stderr, "%s arith timed mul_long failed\n", label);
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
    if (avg_us < 0.0)
        avg_us = 0.0;
    printf("%-28s avg_µs=%10.3f avg_ms=%10.3f\n",
           label,
           avg_us,
           avg_us / 1000.0);
}

static void run_square_case(const char *label,
                            const mint_t *src,
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
    if (mi_square(warm) != 0) {
        fprintf(stderr, "%s warmup square failed\n", label);
        mi_free(warm);
        return;
    }
    mi_free(warm);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *value = mi_clone(src);

        if (!value || mi_square(value) != 0) {
            fprintf(stderr, "%s timed square failed\n", label);
            mi_free(value);
            break;
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

static void run_square_case_arith(const char *label,
                                  const mint_t *src,
                                  int iters)
{
    uint64_t reset_start, reset_end, work_start, work_end;
    double avg_us;

    reset_start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *value = mi_clone(src);

        if (!value) {
            fprintf(stderr, "%s arith reset-only clone failed\n", label);
            return;
        }
        mi_free(value);
    }
    reset_end = now_ns();

    work_start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *value = mi_clone(src);

        if (!value || mi_square(value) != 0) {
            fprintf(stderr, "%s arith timed square failed\n", label);
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
    if (avg_us < 0.0)
        avg_us = 0.0;
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
    mint_t *test_mul_a = mi_create_string("18446744073709551616");
    mint_t *test_mul_b = mi_create_long(10);
    mint_t *big = mi_create_hex("123456789abcdef00112233445566778899aabbccddeeff0011223344556677");
    mint_t *wide = mi_create_hex("fedcba9876543210ffeeddccbbaa99887766554433221100ffeeddccbbaa9988");
    mint_t *wide8_a = mi_create_hex(
        "123456789abcdef00112233445566778899aabbccddeeff0"
        "00112233445566778899aabbccddeeff0011223344556677");
    mint_t *wide8_b = mi_create_hex(
        "fedcba9876543210ffeeddccbbaa99887766554433221100"
        "ffeeddccbbaa99887766554433221100ffeeddccbbaa9988");
    mint_t *wide16_a = mi_create_hex(
        "123456789abcdef00112233445566778899aabbccddeeff0"
        "00112233445566778899aabbccddeeff0011223344556677"
        "89abcdef00112233445566778899aabbccddeeff00112233"
        "445566778899aabbccddeeff00112233445566778899abcd");
    mint_t *wide16_b = mi_create_hex(
        "fedcba9876543210ffeeddccbbaa99887766554433221100"
        "ffeeddccbbaa99887766554433221100ffeeddccbbaa9988"
        "7766554433221100ffeeddccbbaa99887766554433221100"
        "ffeeddccbbaa99887766554433221100ffeeddccbbaa9988");
    mint_t *small = mi_create_hex("d34db33f");

    if (!one || !minus_one || !test_mul_a || !test_mul_b ||
        !big || !wide || !wide8_a || !wide8_b || !wide16_a || !wide16_b ||
        !small) {
        fprintf(stderr, "benchmark setup failed\n");
        mi_free(one);
        mi_free(minus_one);
        mi_free(test_mul_a);
        mi_free(test_mul_b);
        mi_free(big);
        mi_free(wide);
        mi_free(wide8_a);
        mi_free(wide8_b);
        mi_free(wide16_a);
        mi_free(wide16_b);
        mi_free(small);
        return 1;
    }

    printf("iters=%d\n", iters);
    printf("\nMint multiplication\n");
    run_mul_case("mul_by_one_big", big, one, iters);
    run_mul_case("mul_by_minus_one_big", big, minus_one, iters);
    run_mul_case("mul_test_case", test_mul_a, test_mul_b, iters);
    run_mul_case("mul_1limb_by_wide", small, wide, iters);
    run_mul_case("mul_wide_by_wide", big, wide, iters);
    run_mul_case("mul_8limb_by_8limb", wide8_a, wide8_b, iters);
    run_mul_case("mul_16limb_by_16limb", wide16_a, wide16_b, iters);

    printf("\nMint multiplication (arith only)\n");
    run_mul_case_arith("mul_by_one_big", big, one, iters);
    run_mul_case_arith("mul_by_minus_one_big", big, minus_one, iters);
    run_mul_case_arith("mul_test_case", test_mul_a, test_mul_b, iters);
    run_mul_case_arith("mul_1limb_by_wide", small, wide, iters);
    run_mul_case_arith("mul_wide_by_wide", big, wide, iters);
    run_mul_case_arith("mul_8limb_by_8limb", wide8_a, wide8_b, iters);
    run_mul_case_arith("mul_16limb_by_16limb", wide16_a, wide16_b, iters);

    printf("\nMint multiplication by long\n");
    run_mul_long_case("mul_long_minus_one", big, -1, iters);
    run_mul_long_case("mul_long_small", big, 37, iters);

    printf("\nMint multiplication by long (arith only)\n");
    run_mul_long_case_arith("mul_long_minus_one", big, -1, iters);
    run_mul_long_case_arith("mul_long_small", big, 37, iters);

    printf("\nMint squaring\n");
    run_square_case("square_small", test_mul_b, iters);
    run_square_case("square_big_pow2", test_mul_a, iters);
    run_square_case("square_wide", big, iters);
    run_square_case("square_8limb", wide8_a, iters);
    run_square_case("square_16limb", wide16_a, iters);

    printf("\nMint squaring (arith only)\n");
    run_square_case_arith("square_small", test_mul_b, iters);
    run_square_case_arith("square_big_pow2", test_mul_a, iters);
    run_square_case_arith("square_wide", big, iters);
    run_square_case_arith("square_8limb", wide8_a, iters);
    run_square_case_arith("square_16limb", wide16_a, iters);

    mi_free(small);
    mi_free(wide16_b);
    mi_free(wide16_a);
    mi_free(wide8_b);
    mi_free(wide8_a);
    mi_free(wide);
    mi_free(big);
    mi_free(test_mul_b);
    mi_free(test_mul_a);
    mi_free(minus_one);
    mi_free(one);
    return 0;
}
