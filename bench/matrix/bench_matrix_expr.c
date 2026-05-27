#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "expression.h"
#include "matrix.h"
#include "number.h"

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static int bench_read_positive_int(const char *name, int fallback)
{
    const char *value = getenv(name);
    char *end = NULL;
    long parsed;

    if (!value || !*value)
        return fallback;

    parsed = strtol(value, &end, 10);
    if (end == value || *end != '\0' || parsed <= 0 || parsed > INT32_MAX)
        return fallback;

    return (int)parsed;
}

static int bench_read_bool(const char *name)
{
    const char *value = getenv(name);

    return value && *value && value[0] != '0';
}

static expr_t *bench_expr_new_const_d(double x)
{
    number_t n = num_create_from_double(x);
    expr_t *dv = expr_new_const(n);

    num_destroy(&n);
    return dv;
}

static expr_t *bench_expr_new_named_var_d(double x, const char *name)
{
    number_t n = num_create_from_double(x);
    expr_t *dv = expr_new_named_var(n, name);

    num_destroy(&n);
    return dv;
}

static void free_exprs(expr_t **vals, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        expr_free(vals[i]);
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
    fflush(stdout);
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
    fflush(stdout);
}

static void bench_solve_3x3(int iters)
{
    expr_t *a = bench_expr_new_named_var_d(4.0, "a");
    expr_t *b = bench_expr_new_named_var_d(5.0, "b");
    expr_t *c = bench_expr_new_named_var_d(6.0, "c");
    expr_t *u = bench_expr_new_named_var_d(2.0, "u");
    expr_t *v = bench_expr_new_named_var_d(3.0, "v");
    expr_t *zero = bench_expr_new_const_d(0.0);
    expr_t *one = bench_expr_new_const_d(1.0);
    expr_t *two = bench_expr_new_const_d(2.0);
    expr_t *three = bench_expr_new_const_d(3.0);
    expr_t *four = bench_expr_new_const_d(4.0);
    expr_t *A_vals[9] = {
        a,    one,  zero,
        one,  b,    one,
        zero, one, c
    };
    expr_t *X_vals[6] = {
        u,    one,
        two,  v,
        three, four
    };
    expr_t *owned[] = { a, b, c, u, v, zero, one, two, three, four };
    matrix_t *A = mat_create_expr(3, 3, A_vals);
    matrix_t *X_expected = mat_create_expr(3, 2, X_vals);
    matrix_t *B = mat_mul(A, X_expected);

    run_solve_case("solve_dense3x3_rhs2", A, B, iters);

    mat_free(B);
    mat_free(X_expected);
    mat_free(A);
    free_exprs(owned, sizeof(owned) / sizeof(owned[0]));
}

static void bench_solve_6x6(int iters)
{
    expr_t *a = bench_expr_new_named_var_d(5.0, "a");
    expr_t *b = bench_expr_new_named_var_d(6.0, "b");
    expr_t *c = bench_expr_new_named_var_d(7.0, "c");
    expr_t *d = bench_expr_new_named_var_d(8.0, "d");
    expr_t *e = bench_expr_new_named_var_d(9.0, "e");
    expr_t *f = bench_expr_new_named_var_d(10.0, "f");
    expr_t *u = bench_expr_new_named_var_d(11.0, "u");
    expr_t *v = bench_expr_new_named_var_d(13.0, "v");
    expr_t *zero = bench_expr_new_const_d(0.0);
    expr_t *one = bench_expr_new_const_d(1.0);
    expr_t *two = bench_expr_new_const_d(2.0);
    expr_t *three = bench_expr_new_const_d(3.0);
    expr_t *four = bench_expr_new_const_d(4.0);
    expr_t *five = bench_expr_new_const_d(5.0);
    expr_t *six = bench_expr_new_const_d(6.0);
    expr_t *seven = bench_expr_new_const_d(7.0);
    expr_t *A_vals[36] = {
        a,    one,  two,  zero, zero, zero,
        one,  b,    one,  zero, zero, zero,
        two,  one,  c,    one,  zero, zero,
        zero, zero, one,  d,    one,  two,
        zero, zero, zero, one,  e,    one,
        zero, zero, zero, two,  one,  f
    };
    expr_t *X_vals[12] = {
        u,     one,
        two,   v,
        three, four,
        four,  five,
        five,  six,
        six,   seven
    };
    expr_t *owned[] = {
        a, b, c, d, e, f, u, v, zero, one, two, three, four, five, six, seven
    };
    matrix_t *A = mat_create_expr(6, 6, A_vals);
    matrix_t *X_expected = mat_create_expr(6, 2, X_vals);
    matrix_t *B = mat_mul(A, X_expected);

    run_solve_case("solve_dense6x6_rhs2", A, B, iters);

    mat_free(B);
    mat_free(X_expected);
    mat_free(A);
    free_exprs(owned, sizeof(owned) / sizeof(owned[0]));
}

static void bench_inverse_4x4(int iters)
{
    expr_t *u = bench_expr_new_named_var_d(5.0, "u");
    expr_t *v = bench_expr_new_named_var_d(6.0, "v");
    expr_t *w = bench_expr_new_named_var_d(7.0, "w");
    expr_t *t = bench_expr_new_named_var_d(8.0, "t");
    expr_t *zero = bench_expr_new_const_d(0.0);
    expr_t *one = bench_expr_new_const_d(1.0);
    expr_t *two = bench_expr_new_const_d(2.0);
    expr_t *owned[] = { u, v, w, t, zero, one, two };
    expr_t *vals[16] = {
        u,    one, zero, two,
        one,  v,   one,  zero,
        zero, one, w,    one,
        two,  zero, one, t
    };
    matrix_t *A = mat_create_expr(4, 4, vals);

    run_inverse_case("inverse_dense4x4", A, iters);

    mat_free(A);
    free_exprs(owned, sizeof(owned) / sizeof(owned[0]));
}

static void bench_inverse_6x6(int iters)
{
    expr_t *a = bench_expr_new_named_var_d(5.0, "a");
    expr_t *b = bench_expr_new_named_var_d(6.0, "b");
    expr_t *c = bench_expr_new_named_var_d(7.0, "c");
    expr_t *d = bench_expr_new_named_var_d(8.0, "d");
    expr_t *e = bench_expr_new_named_var_d(9.0, "e");
    expr_t *f = bench_expr_new_named_var_d(10.0, "f");
    expr_t *zero = bench_expr_new_const_d(0.0);
    expr_t *one = bench_expr_new_const_d(1.0);
    expr_t *two = bench_expr_new_const_d(2.0);
    expr_t *owned[] = { a, b, c, d, e, f, zero, one, two };
    expr_t *vals[36] = {
        a,    one,  two,  zero, zero, zero,
        one,  b,    one,  zero, zero, zero,
        two,  one,  c,    one,  zero, zero,
        zero, zero, one,  d,    one,  two,
        zero, zero, zero, one,  e,    one,
        zero, zero, zero, two,  one,  f
    };
    matrix_t *A = mat_create_expr(6, 6, vals);

    run_inverse_case("inverse_dense6x6", A, iters);

    mat_free(A);
    free_exprs(owned, sizeof(owned) / sizeof(owned[0]));
}

int main(void)
{
    const int iters = bench_read_positive_int("MARS_BENCH_MATRIX_EXPR_ITERS", 3);
    const int full = bench_read_bool("MARS_BENCH_MATRIX_EXPR_FULL");

    printf("iters=%d\n", iters);
    printf("\nSymbolic expr solve\n");
    fflush(stdout);
    bench_solve_3x3(iters);

    printf("\nSymbolic expr inverse\n");
    fflush(stdout);
    bench_inverse_4x4(iters);

    if (!full) {
        puts("\nSet MARS_BENCH_MATRIX_EXPR_FULL=1 to include 6x6 symbolic cases.");
        return 0;
    }

    printf("\nSymbolic expr solve (large)\n");
    fflush(stdout);
    bench_solve_6x6(iters);

    printf("\nSymbolic expr inverse (large)\n");
    fflush(stdout);
    bench_inverse_6x6(iters);

    return 0;
}
