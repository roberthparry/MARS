#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "expression.h"
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

static expr_t *build_simplify_expr(const expr_t *x, const expr_t *y,
                                   const expr_t *z, const expr_t *w)
{
    number_t three = num_create_from_long(3);
    number_t minus_three = num_create_from_long(-3);
    number_t six = num_create_from_long(6);
    number_t two = num_create_from_long(2);

    expr_t *a0 = expr_log(x);
    expr_t *a1 = expr_exp(a0);
    expr_t *a2 = expr_mul_num(y, &minus_three);
    expr_t *a3 = expr_neg(a2);
    expr_t *a4 = expr_mul_num(z, &six);
    expr_t *a5 = expr_div_num(a4, &three);
    expr_t *a6 = expr_sqrt(w);
    expr_t *a7 = expr_pow(a6, &two);

    expr_t *left = expr_add(a1, a3);
    expr_t *right = expr_add(a5, a7);
    expr_t *expr = expr_add(left, right);

    expr_free(left);
    expr_free(right);
    expr_free(a7);
    expr_free(a6);
    expr_free(a5);
    expr_free(a4);
    expr_free(a3);
    expr_free(a2);
    expr_free(a1);
    expr_free(a0);

    num_destroy(&two);
    num_destroy(&six);
    num_destroy(&minus_three);
    num_destroy(&three);

    return expr;
}

static expr_t *build_reverse_expr(const expr_t *x, const expr_t *y, const expr_t *z)
{
    expr_t *xy = expr_mul(x, y);
    expr_t *sin_xy = expr_sin(xy);
    expr_t *exp_term = expr_exp(sin_xy);
    expr_t *x_sq = expr_mul(x, x);
    expr_t *y_sq = expr_mul(y, y);
    expr_t *sum_sq = expr_add(x_sq, y_sq);
    expr_t *sqrt_term = expr_sqrt(sum_sq);
    expr_t *xz = expr_div(x, z);
    expr_t *tanh_term = expr_tanh(xz);
    expr_t *log_term = expr_log(z);

    expr_t *left = expr_add(exp_term, sqrt_term);
    expr_t *right = expr_add(tanh_term, log_term);
    expr_t *expr = expr_mul(left, right);

    expr_free(right);
    expr_free(left);
    expr_free(log_term);
    expr_free(tanh_term);
    expr_free(xz);
    expr_free(sqrt_term);
    expr_free(sum_sq);
    expr_free(y_sq);
    expr_free(x_sq);
    expr_free(exp_term);
    expr_free(sin_xy);
    expr_free(xy);

    return expr;
}

static double run_simplify_bench(int iterations)
{
    number_t x0 = num_create_from_long(7);
    number_t y0 = num_create_from_long(5);
    number_t z0 = num_create_from_long(11);
    number_t w0 = num_create_from_long(81);

    expr_t *x = expr_new_named_var(x0, "x");
    expr_t *y = expr_new_named_var(y0, "y");
    expr_t *z = expr_new_named_var(z0, "z");
    expr_t *w = expr_new_named_var(w0, "w");
    expr_t *expr = build_simplify_expr(x, y, z, w);
    double start = bench_now_seconds();

    for (int i = 0; i < iterations; ++i) {
        expr_t *simp = expr_simplify(expr);
        number_t value = expr_eval(simp);

        g_sink += num_to_double(value);
        num_destroy(&value);
        expr_free(simp);
    }

    expr_free(expr);
    expr_free(w);
    expr_free(z);
    expr_free(y);
    expr_free(x);
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
    expr_t *x = expr_new_named_var(x0, "x");
    expr_t *y = expr_new_named_var(y0, "y");
    expr_t *z = expr_new_named_var(z0, "z");
    expr_t *expr = build_reverse_expr(x, y, z);
    const expr_t *vars[] = {x, y, z};
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

        expr_set_val(x, nx);
        expr_set_val(y, ny);
        expr_set_val(z, nz);

        (void)expr_eval_derivatives(expr, 3u, vars, &value, grads);
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

    expr_free(expr);
    expr_free(z);
    expr_free(y);
    expr_free(x);
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

    print_stats("expr_simplify hotspot", simplify_samples, runs, simplify_iters);
    print_stats("expr_reverse hotspot", reverse_samples, runs, reverse_iters);
    printf("sink=%.15f\n", g_sink);
    puts("Scale iterations with MARS_BENCH_SCALE=<n> if you want longer runs.");

    return 0;
}
