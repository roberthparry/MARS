#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "dval.h"
#include "matrix.h"

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void free_dvals(dval_t **vals, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        dv_free(vals[i]);
}

static void run_solve_case(const char *label,
                           matrix_t *A,
                           matrix_t *B,
                           int iters)
{
    matrix_t *warm = NULL;
    uint64_t start;
    uint64_t end;
    double avg_us;

    warm = mat_solve(A, B);
    if (!warm) {
        fprintf(stderr, "%s warmup failed\n", label);
        return;
    }
    mat_free(warm);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        matrix_t *X = mat_solve(A, B);
        if (!X) {
            fprintf(stderr, "%s timed run failed\n", label);
            break;
        }
        mat_free(X);
    }
    end = now_ns();
    avg_us = ((double)(end - start) / (double)iters) / 1000.0;

    printf("%-24s avg_µs=%10.3f avg_ms=%10.3f\n",
           label,
           avg_us,
           avg_us / 1000.0);
}

static void run_inverse_case(const char *label,
                             matrix_t *A,
                             int iters)
{
    matrix_t *warm = NULL;
    uint64_t start;
    uint64_t end;
    double avg_us;

    warm = mat_inverse(A);
    if (!warm) {
        fprintf(stderr, "%s warmup failed\n", label);
        return;
    }
    mat_free(warm);

    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        matrix_t *Ai = mat_inverse(A);
        if (!Ai) {
            fprintf(stderr, "%s timed run failed\n", label);
            break;
        }
        mat_free(Ai);
    }
    end = now_ns();
    avg_us = ((double)(end - start) / (double)iters) / 1000.0;

    printf("%-24s avg_µs=%10.3f avg_ms=%10.3f\n",
           label,
           avg_us,
           avg_us / 1000.0);
}

static void bench_solve_3x3(int iters)
{
    dval_t *a = dv_new_named_var_d(4.0, "a");
    dval_t *b = dv_new_named_var_d(5.0, "b");
    dval_t *c = dv_new_named_var_d(6.0, "c");
    dval_t *u = dv_new_named_var_d(2.0, "u");
    dval_t *v = dv_new_named_var_d(3.0, "v");
    dval_t *zero = dv_new_const_d(0.0);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *two = dv_new_const_d(2.0);
    dval_t *three = dv_new_const_d(3.0);
    dval_t *four = dv_new_const_d(4.0);
    dval_t *A_vals[9] = {
        a,    one,  zero,
        one,  b,    one,
        zero, one, c
    };
    dval_t *X_vals[6] = {
        u,    one,
        two,  v,
        three, four
    };
    dval_t *owned[] = { a, b, c, u, v, zero, one, two, three, four };
    matrix_t *A = mat_create_dv(3, 3, A_vals);
    matrix_t *X_expected = mat_create_dv(3, 2, X_vals);
    matrix_t *B = mat_mul(A, X_expected);

    run_solve_case("solve_dense3x3_rhs2", A, B, iters);

    mat_free(B);
    mat_free(X_expected);
    mat_free(A);
    free_dvals(owned, sizeof(owned) / sizeof(owned[0]));
}

static void bench_solve_6x6(int iters)
{
    dval_t *a = dv_new_named_var_d(5.0, "a");
    dval_t *b = dv_new_named_var_d(6.0, "b");
    dval_t *c = dv_new_named_var_d(7.0, "c");
    dval_t *d = dv_new_named_var_d(8.0, "d");
    dval_t *e = dv_new_named_var_d(9.0, "e");
    dval_t *f = dv_new_named_var_d(10.0, "f");
    dval_t *u = dv_new_named_var_d(11.0, "u");
    dval_t *v = dv_new_named_var_d(13.0, "v");
    dval_t *zero = dv_new_const_d(0.0);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *two = dv_new_const_d(2.0);
    dval_t *three = dv_new_const_d(3.0);
    dval_t *four = dv_new_const_d(4.0);
    dval_t *five = dv_new_const_d(5.0);
    dval_t *six = dv_new_const_d(6.0);
    dval_t *seven = dv_new_const_d(7.0);
    dval_t *A_vals[36] = {
        a,    one,  two,  zero, zero, zero,
        one,  b,    one,  zero, zero, zero,
        two,  one,  c,    one,  zero, zero,
        zero, zero, one,  d,    one,  two,
        zero, zero, zero, one,  e,    one,
        zero, zero, zero, two,  one,  f
    };
    dval_t *X_vals[12] = {
        u,     one,
        two,   v,
        three, four,
        four,  five,
        five,  six,
        six,   seven
    };
    dval_t *owned[] = {
        a, b, c, d, e, f, u, v, zero, one, two, three, four, five, six, seven
    };
    matrix_t *A = mat_create_dv(6, 6, A_vals);
    matrix_t *X_expected = mat_create_dv(6, 2, X_vals);
    matrix_t *B = mat_mul(A, X_expected);

    run_solve_case("solve_dense6x6_rhs2", A, B, iters);

    mat_free(B);
    mat_free(X_expected);
    mat_free(A);
    free_dvals(owned, sizeof(owned) / sizeof(owned[0]));
}

static void bench_inverse_4x4(int iters)
{
    dval_t *u = dv_new_named_var_d(5.0, "u");
    dval_t *v = dv_new_named_var_d(6.0, "v");
    dval_t *w = dv_new_named_var_d(7.0, "w");
    dval_t *t = dv_new_named_var_d(8.0, "t");
    dval_t *zero = dv_new_const_d(0.0);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *two = dv_new_const_d(2.0);
    dval_t *owned[] = { u, v, w, t, zero, one, two };
    dval_t *vals[16] = {
        u,    one, zero, two,
        one,  v,   one,  zero,
        zero, one, w,    one,
        two,  zero, one, t
    };
    matrix_t *A = mat_create_dv(4, 4, vals);

    run_inverse_case("inverse_dense4x4", A, iters);

    mat_free(A);
    free_dvals(owned, sizeof(owned) / sizeof(owned[0]));
}

static void bench_inverse_6x6(int iters)
{
    dval_t *a = dv_new_named_var_d(5.0, "a");
    dval_t *b = dv_new_named_var_d(6.0, "b");
    dval_t *c = dv_new_named_var_d(7.0, "c");
    dval_t *d = dv_new_named_var_d(8.0, "d");
    dval_t *e = dv_new_named_var_d(9.0, "e");
    dval_t *f = dv_new_named_var_d(10.0, "f");
    dval_t *zero = dv_new_const_d(0.0);
    dval_t *one = dv_new_const_d(1.0);
    dval_t *two = dv_new_const_d(2.0);
    dval_t *owned[] = { a, b, c, d, e, f, zero, one, two };
    dval_t *vals[36] = {
        a,    one,  two,  zero, zero, zero,
        one,  b,    one,  zero, zero, zero,
        two,  one,  c,    one,  zero, zero,
        zero, zero, one,  d,    one,  two,
        zero, zero, zero, one,  e,    one,
        zero, zero, zero, two,  one,  f
    };
    matrix_t *A = mat_create_dv(6, 6, vals);

    run_inverse_case("inverse_dense6x6", A, iters);

    mat_free(A);
    free_dvals(owned, sizeof(owned) / sizeof(owned[0]));
}

int main(void)
{
    const int iters = 40;

    printf("iters=%d\n", iters);
    printf("\nSymbolic dval solve\n");
    bench_solve_3x3(iters);
    bench_solve_6x6(iters);

    printf("\nSymbolic dval inverse\n");
    bench_inverse_4x4(iters);
    bench_inverse_6x6(iters);

    return 0;
}
