#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "dval.h"
#include "number.h"

enum {
    DEFAULT_SIMPLIFY_ITERS = 4000,
    DEFAULT_REVERSE_ITERS  = 2500,
    DEFAULT_RUNS           = 5
};

static volatile double g_sink = 0.0;

static double bench_now_seconds(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static int bench_scale(void)
{
    const char *scale_text = getenv("MARS_BENCH_SCALE");
    long value;

    if (!scale_text || !*scale_text)
        return 1;

    value = strtol(scale_text, NULL, 10);
    return value > 0 ? (int)value : 1;
}

static dval_t *build_simplify_expr(const dval_t *x, const dval_t *y,
                                   const dval_t *z, const dval_t *w)
{
    number_t three = num_create_from_long(3);
    number_t minus_three = num_create_from_long(-3);
    number_t six = num_create_from_long(6);
    number_t two = num_create_from_long(2);

    dval_t *a0 = dv_log(x);
    dval_t *a1 = dv_exp(a0);
    dval_t *a2 = dv_mul_num(y, &minus_three);
    dval_t *a3 = dv_neg(a2);
    dval_t *a4 = dv_mul_num(z, &six);
    dval_t *a5 = dv_div_num(a4, &three);
    dval_t *a6 = dv_sqrt(w);
    dval_t *a7 = dv_pow(a6, &two);

    dval_t *left = dv_add(a1, a3);
    dval_t *right = dv_add(a5, a7);
    dval_t *expr = dv_add(left, right);

    dv_free(left);
    dv_free(right);
    dv_free(a7);
    dv_free(a6);
    dv_free(a5);
    dv_free(a4);
    dv_free(a3);
    dv_free(a2);
    dv_free(a1);
    dv_free(a0);

    num_destroy(&two);
    num_destroy(&six);
    num_destroy(&minus_three);
    num_destroy(&three);

    return expr;
}

static dval_t *build_reverse_expr(const dval_t *x, const dval_t *y, const dval_t *z)
{
    dval_t *xy = dv_mul(x, y);
    dval_t *sin_xy = dv_sin(xy);
    dval_t *exp_term = dv_exp(sin_xy);
    dval_t *x_sq = dv_mul(x, x);
    dval_t *y_sq = dv_mul(y, y);
    dval_t *sum_sq = dv_add(x_sq, y_sq);
    dval_t *sqrt_term = dv_sqrt(sum_sq);
    dval_t *xz = dv_div(x, z);
    dval_t *tanh_term = dv_tanh(xz);
    dval_t *log_term = dv_log(z);

    dval_t *left = dv_add(exp_term, sqrt_term);
    dval_t *right = dv_add(tanh_term, log_term);
    dval_t *expr = dv_mul(left, right);

    dv_free(right);
    dv_free(left);
    dv_free(log_term);
    dv_free(tanh_term);
    dv_free(xz);
    dv_free(sqrt_term);
    dv_free(sum_sq);
    dv_free(y_sq);
    dv_free(x_sq);
    dv_free(exp_term);
    dv_free(sin_xy);
    dv_free(xy);

    return expr;
}

static double run_simplify_bench(int iterations)
{
    number_t x0 = num_create_from_long(7);
    number_t y0 = num_create_from_long(5);
    number_t z0 = num_create_from_long(11);
    number_t w0 = num_create_from_long(81);

    dval_t *x = dv_new_named_var(x0, "x");
    dval_t *y = dv_new_named_var(y0, "y");
    dval_t *z = dv_new_named_var(z0, "z");
    dval_t *w = dv_new_named_var(w0, "w");
    dval_t *expr = build_simplify_expr(x, y, z, w);
    double start = bench_now_seconds();

    for (int i = 0; i < iterations; ++i) {
        dval_t *simp = dv_simplify(expr);
        number_t value = dv_eval(simp);

        g_sink += num_to_double(value);
        num_destroy(&value);
        dv_free(simp);
    }

    dv_free(expr);
    dv_free(w);
    dv_free(z);
    dv_free(y);
    dv_free(x);
    num_destroy(&w0);
    num_destroy(&z0);
    num_destroy(&y0);
    num_destroy(&x0);

    return bench_now_seconds() - start;
}

static double run_reverse_bench(int iterations)
{
    number_t x0 = num_create_from_string("1.25");
    number_t y0 = num_create_from_string("0.75");
    number_t z0 = num_create_from_string("2.50");
    dval_t *x = dv_new_named_var(x0, "x");
    dval_t *y = dv_new_named_var(y0, "y");
    dval_t *z = dv_new_named_var(z0, "z");
    dval_t *expr = build_reverse_expr(x, y, z);
    const dval_t *vars[] = {x, y, z};
    double start = bench_now_seconds();

    for (int i = 0; i < iterations; ++i) {
        number_t nx = num_create_from_double(1.25 + 0.0001 * (double)(i % 23));
        number_t ny = num_create_from_double(0.75 + 0.0002 * (double)(i % 17));
        number_t nz = num_create_from_double(2.50 + 0.0003 * (double)(i % 11));
        number_t value = num_new();
        number_t grads[3];

        grads[0] = num_new();
        grads[1] = num_new();
        grads[2] = num_new();

        dv_set_val(x, nx);
        dv_set_val(y, ny);
        dv_set_val(z, nz);

        (void)dv_eval_derivatives(expr, 3u, vars, &value, grads);
        g_sink += num_to_double(value);
        g_sink += num_to_double(grads[0]);

        num_destroy(&grads[2]);
        num_destroy(&grads[1]);
        num_destroy(&grads[0]);
        num_destroy(&value);
        num_destroy(&nz);
        num_destroy(&ny);
        num_destroy(&nx);
    }

    dv_free(expr);
    dv_free(z);
    dv_free(y);
    dv_free(x);
    num_destroy(&z0);
    num_destroy(&y0);
    num_destroy(&x0);

    return bench_now_seconds() - start;
}

static void print_stats(const char *label, double *samples, int runs, int iterations)
{
    double sum = 0.0;
    double best = samples[0];
    double worst = samples[0];

    for (int i = 0; i < runs; ++i) {
        sum += samples[i];
        if (samples[i] < best)
            best = samples[i];
        if (samples[i] > worst)
            worst = samples[i];
    }

    printf("%-24s avg=%8.3f ms  best=%8.3f ms  worst=%8.3f ms  per-iter=%8.3f us\n",
           label,
           (sum / (double)runs) * 1000.0,
           best * 1000.0,
           worst * 1000.0,
           (sum / (double)runs) * 1e6 / (double)iterations);
}

int main(void)
{
    const int scale = bench_scale();
    const int runs = DEFAULT_RUNS;
    const int simplify_iters = DEFAULT_SIMPLIFY_ITERS * scale;
    const int reverse_iters = DEFAULT_REVERSE_ITERS * scale;
    double simplify_samples[DEFAULT_RUNS];
    double reverse_samples[DEFAULT_RUNS];

    for (int i = 0; i < runs; ++i)
        simplify_samples[i] = run_simplify_bench(simplify_iters);

    for (int i = 0; i < runs; ++i)
        reverse_samples[i] = run_reverse_bench(reverse_iters);

    print_stats("dval_simplify hotspot", simplify_samples, runs, simplify_iters);
    print_stats("dval_reverse hotspot", reverse_samples, runs, reverse_iters);
    printf("sink=%.15f\n", g_sink);
    puts("Scale iterations with MARS_BENCH_SCALE=<n> if you want longer runs.");

    return 0;
}
