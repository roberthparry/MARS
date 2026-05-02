#include <limits.h>
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

static void run_add_case(const char *label,
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
    if (mi_add(warm, rhs) != 0) {
        fprintf(stderr, "%s warmup add failed\n", label);
        mi_free(warm);
        return;
    }
    mi_free(warm);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *value = mi_clone(lhs_src);

        if (!value || mi_add(value, rhs) != 0) {
            fprintf(stderr, "%s timed add failed\n", label);
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

static void run_sub_case(const char *label,
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
    if (mi_sub(warm, rhs) != 0) {
        fprintf(stderr, "%s warmup sub failed\n", label);
        mi_free(warm);
        return;
    }
    mi_free(warm);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *value = mi_clone(lhs_src);

        if (!value || mi_sub(value, rhs) != 0) {
            fprintf(stderr, "%s timed sub failed\n", label);
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

static void run_add_long_case(const char *label,
                              const mint_t *src,
                              long value,
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
    if (mi_add_long(warm, value) != 0) {
        fprintf(stderr, "%s warmup add_long failed\n", label);
        mi_free(warm);
        return;
    }
    mi_free(warm);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *m = mi_clone(src);

        if (!m || mi_add_long(m, value) != 0) {
            fprintf(stderr, "%s timed add_long failed\n", label);
            mi_free(m);
            return;
        }
        mi_free(m);
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s avg_µs=%10.3f avg_ms=%10.3f\n",
           label, avg_us, avg_us / 1000.0);
}

static void run_sub_long_case(const char *label,
                              const mint_t *src,
                              long value,
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
    if (mi_sub_long(warm, value) != 0) {
        fprintf(stderr, "%s warmup sub_long failed\n", label);
        mi_free(warm);
        return;
    }
    mi_free(warm);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        mint_t *m = mi_clone(src);

        if (!m || mi_sub_long(m, value) != 0) {
            fprintf(stderr, "%s timed sub_long failed\n", label);
            mi_free(m);
            return;
        }
        mi_free(m);
    }
    end = now_ns();

    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s avg_µs=%10.3f avg_ms=%10.3f\n",
           label, avg_us, avg_us / 1000.0);
}

static void run_cmp_long_case(const char *label,
                              const mint_t *src,
                              long value,
                              int iters)
{
    volatile int sink = 0;
    uint64_t start = now_ns();
    uint64_t end;
    double avg_us;

    for (int i = 0; i < iters; ++i)
        sink ^= mi_cmp_long(src, value);

    end = now_ns();
    avg_us = ((double)(end - start) / (double)iters) / 1000.0;
    printf("%-28s avg_µs=%10.3f avg_ms=%10.3f\n",
           label, avg_us, avg_us / 1000.0);
    if (sink == 123456789)
        printf("ignore %d\n", sink);
}

int main(void)
{
    const int iters = 50000;
    mint_t *big = mi_create_string("18446744073709551616");
    mint_t *wide = mi_create_string("123456789012345678901234567890123456789");
    mint_t *small = mi_create_long(42);
    mint_t *neg_wide = mi_create_string("-123456789012345678901234567890123456789");
    mint_t *five = mi_create_long(5);
    mint_t *neg_five = mi_create_long(-5);
    mint_t *one_limb = mi_create_long(123456789);
    mint_t *neg_one_limb = mi_create_long(-123456789);

    if (!big || !wide || !small || !neg_wide ||
        !five || !neg_five || !one_limb || !neg_one_limb) {
        fprintf(stderr, "failed to create benchmark inputs\n");
        mi_free(big);
        mi_free(wide);
        mi_free(small);
        mi_free(neg_wide);
        mi_free(five);
        mi_free(neg_five);
        mi_free(one_limb);
        mi_free(neg_one_limb);
        return 1;
    }

    printf("iters=%d\n\n", iters);
    printf("Mint add/subtract\n");
    run_add_case("add_big_plus_small", big, five, iters);
    run_add_case("add_big_plus_negsmall", big, neg_five, iters);
    run_add_case("add_wide_plus_1limb", wide, one_limb, iters);
    run_add_case("add_small_plus_negwide", small, neg_wide, iters);
    run_sub_case("sub_big_minus_small", big, five, iters);
    run_sub_case("sub_wide_minus_1limb", wide, one_limb, iters);
    run_sub_case("sub_small_minus_wide", small, wide, iters);

    printf("\nMint add/subtract by long\n");
    run_add_long_case("add_long_small", big, 5, iters);
    run_add_long_case("add_long_negsmall", big, -5, iters);
    run_sub_long_case("sub_long_small", big, 5, iters);
    run_sub_long_case("sub_long_negsmall", big, -5, iters);

    printf("\nMint compare to long\n");
    run_cmp_long_case("cmp_long_equal", one_limb, 123456789L, iters);
    run_cmp_long_case("cmp_long_negative", neg_one_limb, -123456789L, iters);
    run_cmp_long_case("cmp_long_big", wide, LONG_MAX, iters);

    mi_free(big);
    mi_free(wide);
    mi_free(small);
    mi_free(neg_wide);
    mi_free(five);
    mi_free(neg_five);
    mi_free(one_limb);
    mi_free(neg_one_limb);
    return 0;
}
