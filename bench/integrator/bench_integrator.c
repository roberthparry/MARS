#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "expression.h"
#include "integrator.h"
#include "qfloat.h"

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

typedef expr_t *(*build_fn)(expr_t **x_out, expr_t **y_out);

static expr_t *bench_expr_new_var_qf(qfloat_t x)
{
    number_t n = num_create_from_qfloat(x);
    expr_t *dv = expr_new_var(n);

    num_destroy(&n);
    return dv;
}

static expr_t *bench_expr_add_d(const expr_t *dv, double x)
{
    number_t n = num_create_from_double(x);
    expr_t *out = expr_add_num(dv, &n);

    num_destroy(&n);
    return out;
}

static expr_t *bench_expr_mul_d(const expr_t *dv, double x)
{
    number_t n = num_create_from_double(x);
    expr_t *out = expr_mul_num(dv, &n);

    num_destroy(&n);
    return out;
}

static expr_t *bench_expr_pow_d(const expr_t *dv, double x)
{
    number_t n = num_create_from_double(x);
    expr_t *out = expr_pow(dv, &n);

    num_destroy(&n);
    return out;
}

#define expr_add_d bench_expr_add_d
#define expr_mul_d bench_expr_mul_d
#define expr_pow_d bench_expr_pow_d

static expr_t *build_affine(expr_t *x, expr_t *y, double constant)
{
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, constant);

    expr_free(sum_xy);
    expr_free(two_y);
    return affine;
}

static expr_t *build_affine_exp(expr_t **x_out, expr_t **y_out)
{
    expr_t *x = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *affine = build_affine(x, y, 3.0);
    expr_t *expr = expr_exp(affine);

    expr_free(affine);
    *x_out = x;
    *y_out = y;
    return expr;
}

static expr_t *build_affine_square(expr_t **x_out, expr_t **y_out)
{
    expr_t *x = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *affine = build_affine(x, y, 3.0);
    expr_t *expr = expr_mul(affine, affine);

    expr_free(affine);
    *x_out = x;
    *y_out = y;
    return expr;
}

static expr_t *build_affine_quartic(expr_t **x_out, expr_t **y_out)
{
    expr_t *x = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *affine = build_affine(x, y, 3.0);
    expr_t *expr = expr_mul(expr_mul(affine, affine), expr_mul(affine, affine));

    expr_free(affine);
    *x_out = x;
    *y_out = y;
    return expr;
}

static expr_t *build_affine_cube_times_exp(expr_t **x_out, expr_t **y_out)
{
    expr_t *x = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *affine = build_affine(x, y, 3.0);
    expr_t *cube = expr_pow_d(affine, 3.0);
    expr_t *exp_affine = expr_exp(affine);
    expr_t *expr = expr_mul(cube, exp_affine);

    expr_free(exp_affine);
    expr_free(cube);
    expr_free(affine);
    *x_out = x;
    *y_out = y;
    return expr;
}

static expr_t *build_affine_cube_times_sin(expr_t **x_out, expr_t **y_out)
{
    expr_t *x = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *affine = build_affine(x, y, 3.0);
    expr_t *cube = expr_pow_d(affine, 3.0);
    expr_t *sin_affine = expr_sin(affine);
    expr_t *expr = expr_mul(cube, sin_affine);

    expr_free(sin_affine);
    expr_free(cube);
    expr_free(affine);
    *x_out = x;
    *y_out = y;
    return expr;
}

static expr_t *build_affine_cube_times_sinh(expr_t **x_out, expr_t **y_out)
{
    expr_t *x = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *affine = build_affine(x, y, 3.0);
    expr_t *cube = expr_pow_d(affine, 3.0);
    expr_t *sinh_affine = expr_sinh(affine);
    expr_t *expr = expr_mul(cube, sinh_affine);

    expr_free(sinh_affine);
    expr_free(cube);
    expr_free(affine);
    *x_out = x;
    *y_out = y;
    return expr;
}

static expr_t *build_affine_times_exp(expr_t **x_out, expr_t **y_out)
{
    expr_t *x = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *affine = build_affine(x, y, 3.0);
    expr_t *exp_affine = expr_exp(affine);
    expr_t *expr = expr_mul(affine, exp_affine);

    expr_free(exp_affine);
    expr_free(affine);
    *x_out = x;
    *y_out = y;
    return expr;
}

static expr_t *build_affine_times_sin(expr_t **x_out, expr_t **y_out)
{
    expr_t *x = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *affine = build_affine(x, y, 3.0);
    expr_t *sin_affine = expr_sin(affine);
    expr_t *expr = expr_mul(affine, sin_affine);

    expr_free(sin_affine);
    expr_free(affine);
    *x_out = x;
    *y_out = y;
    return expr;
}

static expr_t *build_affine_times_sinh(expr_t **x_out, expr_t **y_out)
{
    expr_t *x = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *affine = build_affine(x, y, 3.0);
    expr_t *sinh_affine = expr_sinh(affine);
    expr_t *expr = expr_mul(affine, sinh_affine);

    expr_free(sinh_affine);
    expr_free(affine);
    *x_out = x;
    *y_out = y;
    return expr;
}

static expr_t *build_near_miss_square(expr_t **x_out, expr_t **y_out)
{
    expr_t *x = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *left = build_affine(x, y, 3.0);
    expr_t *right = build_affine(x, y, 4.0);
    expr_t *expr = expr_mul(left, right);

    expr_free(right);
    expr_free(left);
    *x_out = x;
    *y_out = y;
    return expr;
}

static expr_t *build_near_miss_quartic(expr_t **x_out, expr_t **y_out)
{
    expr_t *x = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *a3 = build_affine(x, y, 3.0);
    expr_t *a4 = build_affine(x, y, 4.0);
    expr_t *expr = expr_mul(expr_mul(a3, a3), expr_mul(a3, a4));

    expr_free(a4);
    expr_free(a3);
    *x_out = x;
    *y_out = y;
    return expr;
}

static expr_t *build_near_miss_times_exp(expr_t **x_out, expr_t **y_out)
{
    expr_t *x = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *factor = build_affine(x, y, 4.0);
    expr_t *arg = build_affine(x, y, 3.0);
    expr_t *exp_affine = expr_exp(arg);
    expr_t *expr = expr_mul(factor, exp_affine);

    expr_free(exp_affine);
    expr_free(arg);
    expr_free(factor);
    *x_out = x;
    *y_out = y;
    return expr;
}

static expr_t *build_near_miss_times_sin(expr_t **x_out, expr_t **y_out)
{
    expr_t *x = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *factor = build_affine(x, y, 4.0);
    expr_t *arg = build_affine(x, y, 3.0);
    expr_t *sin_affine = expr_sin(arg);
    expr_t *expr = expr_mul(factor, sin_affine);

    expr_free(sin_affine);
    expr_free(arg);
    expr_free(factor);
    *x_out = x;
    *y_out = y;
    return expr;
}

static expr_t *build_near_miss_times_sinh(expr_t **x_out, expr_t **y_out)
{
    expr_t *x = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *factor = build_affine(x, y, 4.0);
    expr_t *arg = build_affine(x, y, 3.0);
    expr_t *sinh_affine = expr_sinh(arg);
    expr_t *expr = expr_mul(factor, sinh_affine);

    expr_free(sinh_affine);
    expr_free(arg);
    expr_free(factor);
    *x_out = x;
    *y_out = y;
    return expr;
}

static expr_t *build_near_miss_cube_times_exp(expr_t **x_out, expr_t **y_out)
{
    expr_t *x = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *a3 = build_affine(x, y, 3.0);
    expr_t *a4 = build_affine(x, y, 4.0);
    expr_t *cube = expr_mul(expr_mul(a3, a3), a4);
    expr_t *exp_affine = expr_exp(a3);
    expr_t *expr = expr_mul(cube, exp_affine);

    expr_free(exp_affine);
    expr_free(cube);
    expr_free(a4);
    expr_free(a3);
    *x_out = x;
    *y_out = y;
    return expr;
}

static expr_t *build_near_miss_cube_times_sin(expr_t **x_out, expr_t **y_out)
{
    expr_t *x = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *a3 = build_affine(x, y, 3.0);
    expr_t *a4 = build_affine(x, y, 4.0);
    expr_t *cube = expr_mul(expr_mul(a3, a3), a4);
    expr_t *sin_affine = expr_sin(a3);
    expr_t *expr = expr_mul(cube, sin_affine);

    expr_free(sin_affine);
    expr_free(cube);
    expr_free(a4);
    expr_free(a3);
    *x_out = x;
    *y_out = y;
    return expr;
}

static expr_t *build_near_miss_cube_times_sinh(expr_t **x_out, expr_t **y_out)
{
    expr_t *x = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = bench_expr_new_var_qf(qf_from_double(0.0));
    expr_t *a3 = build_affine(x, y, 3.0);
    expr_t *a4 = build_affine(x, y, 4.0);
    expr_t *cube = expr_mul(expr_mul(a3, a3), a4);
    expr_t *sinh_affine = expr_sinh(a3);
    expr_t *expr = expr_mul(cube, sinh_affine);

    expr_free(sinh_affine);
    expr_free(cube);
    expr_free(a4);
    expr_free(a3);
    *x_out = x;
    *y_out = y;
    return expr;
}

static void run_case(const char *label, build_fn builder, int iters)
{
    integrator_t *ig = ig_new();
    expr_t *x = NULL;
    expr_t *y = NULL;
    expr_t *expr = builder(&x, &y);
    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result;
    qfloat_t err;
    size_t first_intervals;
    uint64_t start;
    uint64_t end;
    double avg_us;

    ig_set_tolerance(ig, qf_from_string("1e-21"), qf_from_string("1e-21"));
    if (ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err) != 0) {
        fprintf(stderr, "%s failed on warmup\n", label);
        expr_free(expr);
        expr_free(y);
        expr_free(x);
        ig_free(ig);
        return;
    }

    first_intervals = ig_get_interval_count_used(ig);
    start = now_ns();
    for (int i = 0; i < iters; ++i) {
        if (ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err) != 0) {
            fprintf(stderr, "%s failed during timed run\n", label);
            break;
        }
    }
    end = now_ns();
    avg_us = ((double)(end - start) / (double)iters) / 1000.0;

    printf("%-22s intervals=%-4zu avg_µs=%10.3f avg_ms=%10.3f\n",
           label,
           first_intervals,
           avg_us,
           avg_us / 1000.0);
    fflush(stdout);

    expr_free(expr);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

int main(void)
{
    const int iters = bench_read_positive_int("MARS_BENCH_INTEGRATOR_ITERS", 10);
    const int full = bench_read_bool("MARS_BENCH_INTEGRATOR_FULL");

    printf("iters=%d\n", iters);
    printf("\nMatched shortcut families\n");
    fflush(stdout);
    run_case("affine_exp", build_affine_exp, iters);
    run_case("affine_square", build_affine_square, iters);
    run_case("affine_quartic", build_affine_quartic, iters);
    run_case("affine_times_exp", build_affine_times_exp, iters);
    run_case("affine_cube_exp", build_affine_cube_times_exp, iters);
    run_case("affine_times_sin", build_affine_times_sin, iters);
    run_case("affine_cube_sin", build_affine_cube_times_sin, iters);
    run_case("affine_times_sinh", build_affine_times_sinh, iters);
    run_case("affine_cube_sinh", build_affine_cube_times_sinh, iters);

    printf("\nNear misses (generic path)\n");
    fflush(stdout);
    run_case("near_miss_square", build_near_miss_square, iters);
    run_case("near_miss_quartic", build_near_miss_quartic, iters);
    run_case("near_miss_exp", build_near_miss_times_exp, iters);
    run_case("near_miss_sin", build_near_miss_times_sin, iters);
    run_case("near_miss_sinh", build_near_miss_times_sinh, iters);

    if (!full) {
        puts("\nSet MARS_BENCH_INTEGRATOR_FULL=1 to include cube near-miss cases.");
        return 0;
    }

    printf("\nNear misses (expensive cube generic path)\n");
    fflush(stdout);
    run_case("near_miss_cube_exp", build_near_miss_cube_times_exp, iters);
    run_case("near_miss_cube_sin", build_near_miss_cube_times_sin, iters);
    run_case("near_miss_cube_sinh", build_near_miss_cube_times_sinh, iters);
    return 0;
}
