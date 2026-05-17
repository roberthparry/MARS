#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "dval.h"
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

typedef dval_t *(*build_fn)(dval_t **x_out, dval_t **y_out);

static dval_t *bench_dv_new_var_qf(qfloat_t x)
{
    number_t n = num_create_from_qfloat(x);
    dval_t *dv = dv_new_var(n);

    num_destroy(&n);
    return dv;
}

static dval_t *bench_dv_add_d(const dval_t *dv, double x)
{
    number_t n = num_create_from_double(x);
    dval_t *out = dv_add_num(dv, &n);

    num_destroy(&n);
    return out;
}

static dval_t *bench_dv_mul_d(const dval_t *dv, double x)
{
    number_t n = num_create_from_double(x);
    dval_t *out = dv_mul_num(dv, &n);

    num_destroy(&n);
    return out;
}

static dval_t *bench_dv_pow_d(const dval_t *dv, double x)
{
    number_t n = num_create_from_double(x);
    dval_t *out = dv_pow(dv, &n);

    num_destroy(&n);
    return out;
}

#define dv_add_d bench_dv_add_d
#define dv_mul_d bench_dv_mul_d
#define dv_pow_d bench_dv_pow_d

static dval_t *build_affine(dval_t *x, dval_t *y, double constant)
{
    dval_t *two_y = dv_mul_d(y, 2.0);
    dval_t *sum_xy = dv_add(x, two_y);
    dval_t *affine = dv_add_d(sum_xy, constant);

    dv_free(sum_xy);
    dv_free(two_y);
    return affine;
}

static dval_t *build_affine_exp(dval_t **x_out, dval_t **y_out)
{
    dval_t *x = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *y = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *affine = build_affine(x, y, 3.0);
    dval_t *expr = dv_exp(affine);

    dv_free(affine);
    *x_out = x;
    *y_out = y;
    return expr;
}

static dval_t *build_affine_square(dval_t **x_out, dval_t **y_out)
{
    dval_t *x = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *y = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *affine = build_affine(x, y, 3.0);
    dval_t *expr = dv_mul(affine, affine);

    dv_free(affine);
    *x_out = x;
    *y_out = y;
    return expr;
}

static dval_t *build_affine_quartic(dval_t **x_out, dval_t **y_out)
{
    dval_t *x = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *y = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *affine = build_affine(x, y, 3.0);
    dval_t *expr = dv_mul(dv_mul(affine, affine), dv_mul(affine, affine));

    dv_free(affine);
    *x_out = x;
    *y_out = y;
    return expr;
}

static dval_t *build_affine_cube_times_exp(dval_t **x_out, dval_t **y_out)
{
    dval_t *x = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *y = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *affine = build_affine(x, y, 3.0);
    dval_t *cube = dv_pow_d(affine, 3.0);
    dval_t *exp_affine = dv_exp(affine);
    dval_t *expr = dv_mul(cube, exp_affine);

    dv_free(exp_affine);
    dv_free(cube);
    dv_free(affine);
    *x_out = x;
    *y_out = y;
    return expr;
}

static dval_t *build_affine_cube_times_sin(dval_t **x_out, dval_t **y_out)
{
    dval_t *x = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *y = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *affine = build_affine(x, y, 3.0);
    dval_t *cube = dv_pow_d(affine, 3.0);
    dval_t *sin_affine = dv_sin(affine);
    dval_t *expr = dv_mul(cube, sin_affine);

    dv_free(sin_affine);
    dv_free(cube);
    dv_free(affine);
    *x_out = x;
    *y_out = y;
    return expr;
}

static dval_t *build_affine_cube_times_sinh(dval_t **x_out, dval_t **y_out)
{
    dval_t *x = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *y = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *affine = build_affine(x, y, 3.0);
    dval_t *cube = dv_pow_d(affine, 3.0);
    dval_t *sinh_affine = dv_sinh(affine);
    dval_t *expr = dv_mul(cube, sinh_affine);

    dv_free(sinh_affine);
    dv_free(cube);
    dv_free(affine);
    *x_out = x;
    *y_out = y;
    return expr;
}

static dval_t *build_affine_times_exp(dval_t **x_out, dval_t **y_out)
{
    dval_t *x = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *y = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *affine = build_affine(x, y, 3.0);
    dval_t *exp_affine = dv_exp(affine);
    dval_t *expr = dv_mul(affine, exp_affine);

    dv_free(exp_affine);
    dv_free(affine);
    *x_out = x;
    *y_out = y;
    return expr;
}

static dval_t *build_affine_times_sin(dval_t **x_out, dval_t **y_out)
{
    dval_t *x = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *y = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *affine = build_affine(x, y, 3.0);
    dval_t *sin_affine = dv_sin(affine);
    dval_t *expr = dv_mul(affine, sin_affine);

    dv_free(sin_affine);
    dv_free(affine);
    *x_out = x;
    *y_out = y;
    return expr;
}

static dval_t *build_affine_times_sinh(dval_t **x_out, dval_t **y_out)
{
    dval_t *x = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *y = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *affine = build_affine(x, y, 3.0);
    dval_t *sinh_affine = dv_sinh(affine);
    dval_t *expr = dv_mul(affine, sinh_affine);

    dv_free(sinh_affine);
    dv_free(affine);
    *x_out = x;
    *y_out = y;
    return expr;
}

static dval_t *build_near_miss_square(dval_t **x_out, dval_t **y_out)
{
    dval_t *x = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *y = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *left = build_affine(x, y, 3.0);
    dval_t *right = build_affine(x, y, 4.0);
    dval_t *expr = dv_mul(left, right);

    dv_free(right);
    dv_free(left);
    *x_out = x;
    *y_out = y;
    return expr;
}

static dval_t *build_near_miss_quartic(dval_t **x_out, dval_t **y_out)
{
    dval_t *x = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *y = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *a3 = build_affine(x, y, 3.0);
    dval_t *a4 = build_affine(x, y, 4.0);
    dval_t *expr = dv_mul(dv_mul(a3, a3), dv_mul(a3, a4));

    dv_free(a4);
    dv_free(a3);
    *x_out = x;
    *y_out = y;
    return expr;
}

static dval_t *build_near_miss_times_exp(dval_t **x_out, dval_t **y_out)
{
    dval_t *x = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *y = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *factor = build_affine(x, y, 4.0);
    dval_t *arg = build_affine(x, y, 3.0);
    dval_t *exp_affine = dv_exp(arg);
    dval_t *expr = dv_mul(factor, exp_affine);

    dv_free(exp_affine);
    dv_free(arg);
    dv_free(factor);
    *x_out = x;
    *y_out = y;
    return expr;
}

static dval_t *build_near_miss_times_sin(dval_t **x_out, dval_t **y_out)
{
    dval_t *x = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *y = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *factor = build_affine(x, y, 4.0);
    dval_t *arg = build_affine(x, y, 3.0);
    dval_t *sin_affine = dv_sin(arg);
    dval_t *expr = dv_mul(factor, sin_affine);

    dv_free(sin_affine);
    dv_free(arg);
    dv_free(factor);
    *x_out = x;
    *y_out = y;
    return expr;
}

static dval_t *build_near_miss_times_sinh(dval_t **x_out, dval_t **y_out)
{
    dval_t *x = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *y = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *factor = build_affine(x, y, 4.0);
    dval_t *arg = build_affine(x, y, 3.0);
    dval_t *sinh_affine = dv_sinh(arg);
    dval_t *expr = dv_mul(factor, sinh_affine);

    dv_free(sinh_affine);
    dv_free(arg);
    dv_free(factor);
    *x_out = x;
    *y_out = y;
    return expr;
}

static dval_t *build_near_miss_cube_times_exp(dval_t **x_out, dval_t **y_out)
{
    dval_t *x = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *y = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *a3 = build_affine(x, y, 3.0);
    dval_t *a4 = build_affine(x, y, 4.0);
    dval_t *cube = dv_mul(dv_mul(a3, a3), a4);
    dval_t *exp_affine = dv_exp(a3);
    dval_t *expr = dv_mul(cube, exp_affine);

    dv_free(exp_affine);
    dv_free(cube);
    dv_free(a4);
    dv_free(a3);
    *x_out = x;
    *y_out = y;
    return expr;
}

static dval_t *build_near_miss_cube_times_sin(dval_t **x_out, dval_t **y_out)
{
    dval_t *x = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *y = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *a3 = build_affine(x, y, 3.0);
    dval_t *a4 = build_affine(x, y, 4.0);
    dval_t *cube = dv_mul(dv_mul(a3, a3), a4);
    dval_t *sin_affine = dv_sin(a3);
    dval_t *expr = dv_mul(cube, sin_affine);

    dv_free(sin_affine);
    dv_free(cube);
    dv_free(a4);
    dv_free(a3);
    *x_out = x;
    *y_out = y;
    return expr;
}

static dval_t *build_near_miss_cube_times_sinh(dval_t **x_out, dval_t **y_out)
{
    dval_t *x = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *y = bench_dv_new_var_qf(qf_from_double(0.0));
    dval_t *a3 = build_affine(x, y, 3.0);
    dval_t *a4 = build_affine(x, y, 4.0);
    dval_t *cube = dv_mul(dv_mul(a3, a3), a4);
    dval_t *sinh_affine = dv_sinh(a3);
    dval_t *expr = dv_mul(cube, sinh_affine);

    dv_free(sinh_affine);
    dv_free(cube);
    dv_free(a4);
    dv_free(a3);
    *x_out = x;
    *y_out = y;
    return expr;
}

static void run_case(const char *label, build_fn builder, int iters)
{
    integrator_t *ig = ig_new();
    dval_t *x = NULL;
    dval_t *y = NULL;
    dval_t *expr = builder(&x, &y);
    dval_t *vars[2] = { x, y };
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
        dv_free(expr);
        dv_free(y);
        dv_free(x);
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

    dv_free(expr);
    dv_free(y);
    dv_free(x);
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
