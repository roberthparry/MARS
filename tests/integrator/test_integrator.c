/* test_integrator.c — tests for the adaptive integrators */

#include <stdio.h>
#include <math.h>

#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#define TEST_CONFIG_MAIN
#include "test_harness.h"

#include "qfloat.h"
#include "qcomplex.h"
#include "integrator.h"
#include "dval.h"

/* -----------------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------------- */

/* True if |a - b| <= tol */
static int qf_close(qfloat_t a, qfloat_t b, qfloat_t tol) {
    return qf_le(qf_abs(qf_sub(a, b)), tol);
}

static qfloat_t tol20 = { 9.9999999999999995e-21, 5.4846728545790429e-37 };  /* 1e-20 */
static qfloat_t tol15 = { 1.0000000000000001e-15, -4.3320984004882613e-32 }; /* 1e-15 */
static qfloat_t tol27 = { 1e-27, -3.8494869749191836e-44 }; /* 1e-27 */

/* -----------------------------------------------------------------------
 * Integrands
 * --------------------------------------------------------------------- */

static qfloat_t fn_sin(qfloat_t x, void *ctx) {
    (void)ctx;
    return qf_sin(x);
}

static qfloat_t fn_exp(qfloat_t x, void *ctx) {
    (void)ctx;
    return qf_exp(x);
}

static qfloat_t fn_inv1px2(qfloat_t x, void *ctx) {
    (void)ctx;
    qfloat_t one = qf_from_double(1.0);
    return qf_div(one, qf_add(one, qf_sqr(x)));
}

static qfloat_t fn_log(qfloat_t x, void *ctx) {
    (void)ctx;
    return qf_log(x);
}


/* -----------------------------------------------------------------------
 * Tests
 * --------------------------------------------------------------------- */

void test_create_and_destroy(void) {
    integrator_t *ig = ig_new();
    ASSERT_TRUE(ig);
    ig_free(ig);
    ig_free(NULL);  /* must not crash */
}

void test_polynomial(void) {
    /* ∫₀¹ x² dx = 1/3 — degree-2 polynomial; Turán is exact to full qfloat_t precision */
    integrator_t *ig = ig_new();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *expr = dv_mul(x, x);

    qfloat_t result, err;
    int s = ig_single_integral(ig, expr, x,
                               qf_from_double(0.0), qf_from_double(1.0),
                               &result, &err);
    qfloat_t expected = qf_from_string("0.33333333333333333333333333333333333333");
    printf("  ∫₀¹ x² dx\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol15));
    dv_free(expr);
    dv_free(x);
    ig_free(ig);
}

void test_sin(void) {
    /* ∫₀^π sin(x) dx = 2 */
    integrator_t *ig = ig_new();
    ig_set_tolerance(ig, qf_from_string("1e-21"), qf_from_string("1e-21"));
    qfloat_t result, err;
    int s = ig_integral(ig, fn_sin, NULL,
                        qf_from_double(0.0), QF_PI,
                        &result, &err);
    qfloat_t expected = qf_from_double(2.0);
    printf("  ∫₀^π sin(x) dx\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol15));
    ig_free(ig);
}

void test_exp(void) {
    /* ∫₀¹ exp(x) dx = e - 1 */
    integrator_t *ig = ig_new();
    ig_set_tolerance(ig, qf_from_string("1e-21"), qf_from_string("1e-21"));
    qfloat_t result, err;
    int s = ig_integral(ig, fn_exp, NULL,
                        qf_from_double(0.0), qf_from_double(1.0),
                        &result, &err);
    qfloat_t expected = qf_sub(QF_E, qf_from_double(1.0));
    printf("  ∫₀¹ exp(x) dx\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol15));
    ig_free(ig);
}

void test_arctan(void) {
    /* ∫₋₁¹ 1/(1+x²) dx = π/2 */
    integrator_t *ig = ig_new();
    ig_set_tolerance(ig, qf_from_string("1e-21"), qf_from_string("1e-21"));
    qfloat_t result, err;
    int s = ig_integral(ig, fn_inv1px2, NULL,
                        qf_from_double(-1.0), qf_from_double(1.0),
                        &result, &err);
    printf("  ∫₋₁¹ 1/(1+x²) dx\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q  (π/2)\n", QF_PI_2);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, QF_PI_2, tol20));
    ig_free(ig);
}

void test_log(void) {
    /* ∫₁^e ln(x) dx = 1 */
    integrator_t *ig = ig_new();
    ig_set_tolerance(ig, qf_from_string("1e-21"), qf_from_string("1e-21"));
    qfloat_t result, err;
    int s = ig_integral(ig, fn_log, NULL,
                        qf_from_double(1.0), QF_E,
                        &result, &err);
    qfloat_t expected = qf_from_double(1.0);
    printf("  ∫₁^e ln(x) dx\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol15));
    ig_free(ig);
}

void test_constant(void) {
    /* ∫₀^5 1 dx = 5 — constant integrand; Turán is polynomially exact at qfloat precision */
    integrator_t *ig = ig_new();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *expr = dv_new_const_d(1.0);

    qfloat_t result, err;
    int s = ig_single_integral(ig, expr, x,
                               qf_from_double(0.0), qf_from_double(5.0),
                               &result, &err);
    qfloat_t expected = qf_from_double(5.0);
    printf("  ∫₀^5 1 dx  (rectangle — Turán should be qfloat-exact)\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol15));
    dv_free(expr);
    dv_free(x);
    ig_free(ig);
}

void test_linear(void) {
    /* ∫₀^5 x dx = 12.5 — linear integrand; Turán is polynomially exact at qfloat precision */
    integrator_t *ig = ig_new();
    dval_t *x    = dv_new_var(qf_from_double(0.0));

    qfloat_t result, err;
    int s = ig_single_integral(ig, x, x,
                               qf_from_double(0.0), qf_from_double(5.0),
                               &result, &err);
    qfloat_t expected = qf_from_string("12.5");
    printf("  ∫₀^5 x dx  (triangle — Turán should be qfloat-exact)\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol15));
    dv_free(x);
    ig_free(ig);
}

void test_set_tol(void) {
    integrator_t *ig = ig_new();
    qfloat_t loose = qf_from_string("1e-10");
    ig_set_tolerance(ig, loose, loose);

    qfloat_t result, err;
    int s = ig_integral(ig, fn_sin, NULL,
                        qf_from_double(0.0), QF_PI,
                        &result, &err);
    printf("  ∫₀^π sin(x) dx  (tolerance 1e-10)\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  err      = %q  (limit 1e-8)\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    /* Error estimate should be at or below the loose tolerance */
    ASSERT_TRUE(qf_le(err, qf_from_string("1e-8")));
    ig_free(ig);
}

void test_max_intervals(void) {
    /* Force early termination by allowing only 1 subinterval */
    integrator_t *ig = ig_new();
    ig_set_interval_count_max(ig, 1);

    qfloat_t result, err;
    int s = ig_integral(ig, fn_sin, NULL,
                        qf_from_double(0.0), QF_PI,
                        &result, &err);
    size_t n = ig_get_interval_count_used(ig);
    printf("  ∫₀^π sin(x) dx  (max_intervals = 1)\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, n);
    /* Should stop early — status 1 is acceptable for a highly oscillatory
       integrand restricted to a single subinterval */
    ASSERT_TRUE(s == 0 || s == 1);
    ASSERT_TRUE(n <= 1);
    ig_free(ig);
}

void test_last_intervals(void) {
    /* A smooth integrand over a moderate range should converge in a handful
       of intervals; verify the counter is updated. */
    integrator_t *ig = ig_new();
    qfloat_t result, err;
    int s = ig_integral(ig, fn_exp, NULL,
                        qf_from_double(0.0), qf_from_double(1.0),
                        &result, &err);
    size_t n = ig_get_interval_count_used(ig);
    printf("  ∫₀¹ exp(x) dx  (interval counter)\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, n);
    ASSERT_TRUE(n >= 1);
    ig_free(ig);
}

void test_null_safety(void) {
    integrator_t *ig = ig_new();
    qfloat_t result;
    /* NULL integrand */
    int s = ig_integral(ig, NULL, NULL,
                        qf_from_double(0.0), qf_from_double(1.0),
                        &result, NULL);
    ASSERT_TRUE(s == -1);
    /* NULL result */
    s = ig_integral(ig, fn_exp, NULL,
                    qf_from_double(0.0), qf_from_double(1.0),
                    NULL, NULL);
    ASSERT_TRUE(s == -1);
    ig_free(ig);
}

void test_reversed_limits(void) {
    /* ∫₁⁰ x² dx = -1/3 — reversed limits; Turán handles sign and is polynomially exact */
    integrator_t *ig = ig_new();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *expr = dv_mul(x, x);

    qfloat_t result, err;
    int s = ig_single_integral(ig, expr, x,
                               qf_from_double(1.0), qf_from_double(0.0),
                               &result, &err);
    qfloat_t expected = qf_from_string("-0.33333333333333333333333333333333333333");
    printf("  ∫₁⁰ x² dx  (reversed limits)\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol15));
    dv_free(expr);
    dv_free(x);
    ig_free(ig);
}

/* -----------------------------------------------------------------------
 * ig_single_integral tests (Turán T15/T4 rule)
 * --------------------------------------------------------------------- */

void test_dv_sin(void) {
    /* ∫₀^π sin(x) dx = 2 */
    integrator_t *ig = ig_new();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *expr = dv_sin(x);

    qfloat_t result, err;
    int s = ig_single_integral(ig, expr, x,
                               qf_from_double(0.0), QF_PI,
                               &result, &err);
    qfloat_t expected = qf_from_double(2.0);
    printf("  ∫₀^π sin(x) dx  [Turán T15/T4]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol15));

    dv_free(expr);
    dv_free(x);
    ig_free(ig);
}

void test_dv_exp(void) {
    /* ∫₀¹ exp(x) dx = e - 1 */
    integrator_t *ig = ig_new();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *expr = dv_exp(x);

    qfloat_t result, err;
    int s = ig_single_integral(ig, expr, x,
                               qf_from_double(0.0), qf_from_double(1.0),
                               &result, &err);
    qfloat_t expected = qf_sub(QF_E, qf_from_double(1.0));
    printf("  ∫₀¹ exp(x) dx  [Turán T15/T4]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol20));

    dv_free(expr);
    dv_free(x);
    ig_free(ig);
}

void test_dv_arctan(void) {
    /* ∫₋₁¹ 1/(1+x²) dx = π/2 */
    integrator_t *ig = ig_new();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *one  = dv_new_const_d(1.0);
    dval_t *x2   = dv_mul(x, x);
    dval_t *denom = dv_add(one, x2);
    dval_t *expr = dv_div(one, denom);

    qfloat_t result, err;
    int s = ig_single_integral(ig, expr, x,
                               qf_from_double(-1.0), qf_from_double(1.0),
                               &result, &err);
    printf("  ∫₋₁¹ 1/(1+x²) dx  [Turán T15/T4]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q  (π/2)\n", QF_PI_2);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, QF_PI_2, tol27));

    dv_free(expr);
    dv_free(denom);
    dv_free(x2);
    dv_free(one);
    dv_free(x);
    ig_free(ig);
}

void test_dv_null_safety(void) {
    integrator_t *ig = ig_new();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *expr = dv_exp(x);
    qfloat_t result;

    /* NULL integrator */
    int s = ig_single_integral(NULL, expr, x,
                               qf_from_double(0.0), qf_from_double(1.0),
                               &result, NULL);
    ASSERT_TRUE(s == -1);

    /* NULL expr */
    s = ig_single_integral(ig, NULL, x,
                           qf_from_double(0.0), qf_from_double(1.0),
                           &result, NULL);
    ASSERT_TRUE(s == -1);

    /* NULL result */
    s = ig_single_integral(ig, expr, x,
                           qf_from_double(0.0), qf_from_double(1.0),
                           NULL, NULL);
    ASSERT_TRUE(s == -1);

    dv_free(expr);
    dv_free(x);
    ig_free(ig);
}

/* -----------------------------------------------------------------------
 * ig_double_integral tests
 * --------------------------------------------------------------------- */

void test_double_polynomial(void) {
    /* ∫₀¹∫₀¹ x·y dx dy = 1/4 — polynomial; Turán is exact to full qfloat precision */
    integrator_t *ig = ig_new();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *y    = dv_new_var(qf_from_double(0.0));
    dval_t *expr = dv_mul(x, y);

    qfloat_t result, err;
    int s = ig_double_integral(ig, expr,
                               x, qf_from_double(0.0), qf_from_double(1.0),
                               y, qf_from_double(0.0), qf_from_double(1.0),
                               &result, &err);
    qfloat_t expected = qf_from_double(0.25);
    printf("  ∫₀¹∫₀¹ x·y dx dy\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol20));
    dv_free(expr);
    dv_free(y);
    dv_free(x);
    ig_free(ig);
}

void test_double_exp(void) {
    /* ∫₀¹∫₀¹ exp(x+y) dx dy = (e−1)² */
    integrator_t *ig = ig_new();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *y    = dv_new_var(qf_from_double(0.0));
    dval_t *sum  = dv_add(x, y);           // store intermediate
    dval_t *expr = dv_exp(sum);

    qfloat_t result, err;
    int s = ig_double_integral(ig, expr,
                               x, qf_from_double(0.0), qf_from_double(1.0),
                               y, qf_from_double(0.0), qf_from_double(1.0),
                               &result, &err);
    qfloat_t em1      = qf_sub(QF_E, qf_from_double(1.0));
    qfloat_t expected = qf_mul(em1, em1);
    printf("  ∫₀¹∫₀¹ exp(x+y) dx dy  [(e−1)²]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol20));
    dv_free(expr);
    dv_free(sum);   // free intermediate
    dv_free(y);
    dv_free(x);
    ig_free(ig);
}

void test_double_nonunit_bounds(void) {
    /* ∫₀²∫₀³ x·y dx dy = 9 — polynomial with non-unit bounds */
    integrator_t *ig = ig_new();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *y    = dv_new_var(qf_from_double(0.0));
    dval_t *expr = dv_mul(x, y);

    qfloat_t result, err;
    int s = ig_double_integral(ig, expr,
                               x, qf_from_double(0.0), qf_from_double(2.0),
                               y, qf_from_double(0.0), qf_from_double(3.0),
                               &result, &err);
    qfloat_t expected = qf_from_double(9.0);
    printf("  ∫₀²∫₀³ x·y dx dy\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol27));
    dv_free(expr);
    dv_free(y);
    dv_free(x);
    ig_free(ig);
}

void test_double_null_safety(void) {
    integrator_t *ig = ig_new();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *y    = dv_new_var(qf_from_double(0.0));
    dval_t *expr = dv_mul(x, y);
    qfloat_t result;
    qfloat_t z = qf_from_double(0.0), o = qf_from_double(1.0);

    ASSERT_TRUE(ig_double_integral(NULL, expr, x, z, o, y, z, o, &result, NULL) == -1);
    ASSERT_TRUE(ig_double_integral(ig, NULL, x, z, o, y, z, o, &result, NULL) == -1);
    ASSERT_TRUE(ig_double_integral(ig, expr, x, z, o, y, z, o, NULL, NULL) == -1);
    dv_free(expr);
    dv_free(y);
    dv_free(x);
    ig_free(ig);
}

/* -----------------------------------------------------------------------
 * ig_triple_integral tests
 * --------------------------------------------------------------------- */

void test_triple_polynomial(void) {
    /* ∫₀¹∫₀¹∫₀¹ x·y·z dx dy dz = 1/8 — polynomial exact */
    integrator_t *ig = ig_new();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *y    = dv_new_var(qf_from_double(0.0));
    dval_t *z    = dv_new_var(qf_from_double(0.0));
    dval_t *xy   = dv_mul(x, y);           // store intermediate
    dval_t *expr = dv_mul(xy, z);

    qfloat_t result, err;
    int s = ig_triple_integral(ig, expr,
                               x, qf_from_double(0.0), qf_from_double(1.0),
                               y, qf_from_double(0.0), qf_from_double(1.0),
                               z, qf_from_double(0.0), qf_from_double(1.0),
                               &result, &err);
    qfloat_t expected = qf_from_double(0.125);
    printf("  ∫₀¹∫₀¹∫₀¹ x·y·z dx dy dz\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol27));
    dv_free(expr);
    dv_free(xy);    // free intermediate
    dv_free(z);
    dv_free(y);
    dv_free(x);
    ig_free(ig);
}

void test_triple_exp(void) {
    /* ∫₀¹∫₀¹∫₀¹ exp(x+y+z) dx dy dz = (e−1)³ */
    integrator_t *ig = ig_new();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *y    = dv_new_var(qf_from_double(0.0));
    dval_t *z    = dv_new_var(qf_from_double(0.0));
    dval_t *xy   = dv_add(x, y);           // store intermediate
    dval_t *xyz  = dv_add(xy, z);          // store intermediate
    dval_t *expr = dv_exp(xyz);

    qfloat_t result, err;
    int s = ig_triple_integral(ig, expr,
                               x, qf_from_double(0.0), qf_from_double(1.0),
                               y, qf_from_double(0.0), qf_from_double(1.0),
                               z, qf_from_double(0.0), qf_from_double(1.0),
                               &result, &err);
    qfloat_t em1      = qf_sub(QF_E, qf_from_double(1.0));
    qfloat_t expected = qf_mul(qf_mul(em1, em1), em1);
    printf("  ∫₀¹∫₀¹∫₀¹ exp(x+y+z) dx dy dz  [(e−1)³]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol27));
    dv_free(expr);
    dv_free(xyz);   // free intermediate
    dv_free(xy);    // free intermediate
    dv_free(z);
    dv_free(y);
    dv_free(x);
    ig_free(ig);
}

void test_triple_null_safety(void) {
    integrator_t *ig = ig_new();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *y    = dv_new_var(qf_from_double(0.0));
    dval_t *z    = dv_new_var(qf_from_double(0.0));
    dval_t *xy   = dv_mul(x, y);           // store intermediate
    dval_t *expr = dv_mul(xy, z);
    qfloat_t result;
    qfloat_t lo = qf_from_double(0.0), hi = qf_from_double(1.0);

    ASSERT_TRUE(ig_triple_integral(NULL, expr, x, lo, hi, y, lo, hi, z, lo, hi, &result, NULL) == -1);
    ASSERT_TRUE(ig_triple_integral(ig, NULL, x, lo, hi, y, lo, hi, z, lo, hi, &result, NULL) == -1);
    ASSERT_TRUE(ig_triple_integral(ig, expr, x, lo, hi, y, lo, hi, z, lo, hi, NULL, NULL) == -1);

    dv_free(expr);
    dv_free(xy);    // free intermediate
    dv_free(z);
    dv_free(y);
    dv_free(x);
    ig_free(ig);
}

/* -----------------------------------------------------------------------
 * ig_integral_multi tests (N-dimensional Turán T15/T4)
 * --------------------------------------------------------------------- */

void test_multi_2d(void) {
    /* ∫₀¹ ∫₀¹ (x+y) dx dy = 1 — linear; expect qfloat precision */
    integrator_t *ig = ig_new();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *y    = dv_new_var(qf_from_double(0.0));
    dval_t *expr = dv_add(x, y);

    dval_t *vars[2] = { x, y };
    qfloat_t lo[2]  = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2]  = { qf_from_double(1.0), qf_from_double(1.0) };

    qfloat_t result, err;
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);
    qfloat_t expected = qf_from_double(1.0);
    printf("  ∫₀¹∫₀¹ (x+y) dx dy  [double integral]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol27));

    dv_free(expr);
    dv_free(y);
    dv_free(x);
    ig_free(ig);
}

void test_multi_3d(void) {
    /* ∫₀¹ ∫₀¹ ∫₀¹ (x+y+z) dx dy dz = 1.5 — linear; expect qfloat precision */
    integrator_t *ig = ig_new();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *y    = dv_new_var(qf_from_double(0.0));
    dval_t *z    = dv_new_var(qf_from_double(0.0));
    dval_t *xy   = dv_add(x, y);           // store intermediate
    dval_t *expr = dv_add(xy, z);

    dval_t *vars[3] = { x, y, z };
    qfloat_t lo[3]  = { qf_from_double(0.0), qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[3]  = { qf_from_double(1.0), qf_from_double(1.0), qf_from_double(1.0) };

    qfloat_t result, err;
    int s = ig_integral_multi(ig, expr, 3, vars, lo, hi, &result, &err);
    qfloat_t expected = qf_from_string("1.5");
    printf("  ∫₀¹∫₀¹∫₀¹ (x+y+z) dx dy dz  [triple integral]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol27));

    dv_free(expr);
    dv_free(xy);    // free intermediate
    dv_free(z);
    dv_free(y);
    dv_free(x);
    ig_free(ig);
}

void test_multi_null_safety(void) {
    integrator_t *ig = ig_new();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *expr = dv_exp(x);
    dval_t *vars[1] = { x };
    qfloat_t lo[1] = { qf_from_double(0.0) };
    qfloat_t hi[1] = { qf_from_double(1.0) };
    qfloat_t result;

    ASSERT_TRUE(ig_integral_multi(NULL, expr, 1, vars, lo, hi, &result, NULL) == -1);
    ASSERT_TRUE(ig_integral_multi(ig, NULL, 1, vars, lo, hi, &result, NULL) == -1);
    ASSERT_TRUE(ig_integral_multi(ig, expr, 0, vars, lo, hi, &result, NULL) == -1);
    ASSERT_TRUE(ig_integral_multi(ig, expr, 1, vars, lo, hi, NULL, NULL) == -1);

    dv_free(expr);
    dv_free(x);
    ig_free(ig);
}

void test_multi_nd1(void) {
    /* ndim=1 degenerates to ig_single_integral: ∫₀¹ exp(x) dx = e−1 */
    integrator_t *ig = ig_new();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *expr = dv_exp(x);
    dval_t *vars[1] = { x };
    qfloat_t lo[1]  = { qf_from_double(0.0) };
    qfloat_t hi[1]  = { qf_from_double(1.0) };

    qfloat_t result, err;
    int s = ig_integral_multi(ig, expr, 1, vars, lo, hi, &result, &err);
    qfloat_t expected = qf_sub(QF_E, qf_from_double(1.0));
    printf("  ∫₀¹ exp(x) dx  [multi ndim=1]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol27));
    dv_free(expr);
    dv_free(x);
    ig_free(ig);
}

void test_multi_4d(void) {
    /* ∫₀¹∫₀¹∫₀¹∫₀¹ (x+y+z+w) dx dy dz dw = 2.0 — linear polynomial in 4D, exact */
    integrator_t *ig = ig_new();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *y    = dv_new_var(qf_from_double(0.0));
    dval_t *z    = dv_new_var(qf_from_double(0.0));
    dval_t *w    = dv_new_var(qf_from_double(0.0));
    dval_t *xy   = dv_add(x, y);           // store intermediate
    dval_t *zw   = dv_add(z, w);           // store intermediate
    dval_t *expr = dv_add(xy, zw);

    dval_t *vars[4] = { x, y, z, w };
    qfloat_t lo[4]  = { qf_from_double(0.0), qf_from_double(0.0),
                        qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[4]  = { qf_from_double(1.0), qf_from_double(1.0),
                        qf_from_double(1.0), qf_from_double(1.0) };

    qfloat_t result, err;
    int s = ig_integral_multi(ig, expr, 4, vars, lo, hi, &result, &err);
    qfloat_t expected = qf_from_double(2.0);
    printf("  ∫₀¹∫₀¹∫₀¹∫₀¹ (x+y+z+w) dx dy dz dw  [quadruple integral]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol27));
    dv_free(expr);
    dv_free(zw);    // free intermediate
    dv_free(xy);    // free intermediate
    dv_free(w);
    dv_free(z);
    dv_free(y);
    dv_free(x);
    ig_free(ig);
}

void test_multi_4d_exp(void) {
    /* ∫₀¹∫₀¹∫₀¹∫₀¹ exp(x+y+z+w) dx dy dz dw = (e−1)⁴ */
    integrator_t *ig = ig_new();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *y    = dv_new_var(qf_from_double(0.0));
    dval_t *z    = dv_new_var(qf_from_double(0.0));
    dval_t *w    = dv_new_var(qf_from_double(0.0));
    dval_t *xy   = dv_add(x, y);           // store intermediate
    dval_t *zw   = dv_add(z, w);           // store intermediate
    dval_t *sum  = dv_add(xy, zw);         // store intermediate
    dval_t *expr = dv_exp(sum);

    dval_t *vars[4] = { x, y, z, w };
    qfloat_t lo[4]  = { qf_from_double(0.0), qf_from_double(0.0),
                        qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[4]  = { qf_from_double(1.0), qf_from_double(1.0),
                        qf_from_double(1.0), qf_from_double(1.0) };

    qfloat_t result, err;
    int s = ig_integral_multi(ig, expr, 4, vars, lo, hi, &result, &err);
    qfloat_t em1      = qf_sub(QF_E, qf_from_double(1.0));
    qfloat_t em1sq    = qf_mul(em1, em1);
    qfloat_t expected = qf_mul(em1sq, em1sq);
    printf("  ∫₀¹∫₀¹∫₀¹∫₀¹ exp(x+y+z+w) dx dy dz dw  [(e - 1)⁴]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol27));
    dv_free(expr);
    dv_free(sum);   // free intermediate
    dv_free(zw);    // free intermediate
    dv_free(xy);    // free intermediate
    dv_free(w);
    dv_free(z);
    dv_free(y);
    dv_free(x);
    ig_free(ig);
}

void test_multi_4d_exp_affine(void) {
    /* ∫ exp(2x - y + 0.5z + 3w + 1) dV = e * Π_i ∫ exp(a_i t) dt on [0,1]^4 */
    integrator_t *ig = ig_new();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *y    = dv_new_var(qf_from_double(0.0));
    dval_t *z    = dv_new_var(qf_from_double(0.0));
    dval_t *w    = dv_new_var(qf_from_double(0.0));
    dval_t *two_x = dv_mul_d(x, 2.0);
    dval_t *neg_y = dv_neg(y);
    dval_t *half_z = dv_mul_d(z, 0.5);
    dval_t *three_w = dv_mul_d(w, 3.0);
    dval_t *sum_xy = dv_add(two_x, neg_y);
    dval_t *sum_xyz = dv_add(sum_xy, half_z);
    dval_t *sum_xyzw = dv_add(sum_xyz, three_w);
    dval_t *affine = dv_add_d(sum_xyzw, 1.0);
    dval_t *expr = dv_exp(affine);

    dval_t *vars[4] = { x, y, z, w };
    qfloat_t lo[4]  = { qf_from_double(0.0), qf_from_double(0.0),
                        qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[4]  = { qf_from_double(1.0), qf_from_double(1.0),
                        qf_from_double(1.0), qf_from_double(1.0) };

    qfloat_t result, err;
    int s = ig_integral_multi(ig, expr, 4, vars, lo, hi, &result, &err);

    qfloat_t ex = qf_mul_double(qf_sub(qf_exp(qf_from_double(2.0)), QF_ONE), 0.5);
    qfloat_t ey = qf_sub(QF_ONE, qf_div(QF_ONE, QF_E));
    qfloat_t ez = qf_mul_double(qf_sub(qf_exp(qf_from_double(0.5)), QF_ONE), 2.0);
    qfloat_t ew = qf_div(qf_sub(qf_exp(qf_from_double(3.0)), QF_ONE),
                         qf_from_double(3.0));
    qfloat_t expected = qf_mul(QF_E, qf_mul(qf_mul(ex, ey), qf_mul(ez, ew)));

    printf("  ∫₀¹∫₀¹∫₀¹∫₀¹ exp(2x-y+0.5z+3w+1) dx dy dz dw  [affine exp]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol27));

    dv_free(expr);
    dv_free(affine);
    dv_free(sum_xyzw);
    dv_free(sum_xyz);
    dv_free(sum_xy);
    dv_free(three_w);
    dv_free(half_z);
    dv_free(neg_y);
    dv_free(two_x);
    dv_free(w);
    dv_free(z);
    dv_free(y);
    dv_free(x);
    ig_free(ig);
}

void test_multi_3d_sinh_affine(void) {
    /* ∫₀¹∫₀¹∫₀¹ sinh(x - 2y + 0.5z + 1) dV */
    integrator_t *ig = ig_new();
    dval_t *x = dv_new_var(qf_from_double(0.0));
    dval_t *y = dv_new_var(qf_from_double(0.0));
    dval_t *z = dv_new_var(qf_from_double(0.0));
    dval_t *neg2y = dv_mul_d(y, -2.0);
    dval_t *halfz = dv_mul_d(z, 0.5);
    dval_t *sum_xy = dv_add(x, neg2y);
    dval_t *sum_xyz = dv_add(sum_xy, halfz);
    dval_t *affine = dv_add_d(sum_xyz, 1.0);
    dval_t *expr = dv_sinh(affine);

    dval_t *vars[3] = { x, y, z };
    qfloat_t lo[3] = { qf_from_double(0.0), qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[3] = { qf_from_double(1.0), qf_from_double(1.0), qf_from_double(1.0) };

    qfloat_t result, err;
    int s = ig_integral_multi(ig, expr, 3, vars, lo, hi, &result, &err);

    qfloat_t ix_p = qf_sub(QF_E, QF_ONE);
    qfloat_t iy_p = qf_mul_double(qf_sub(QF_ONE, qf_exp(qf_from_double(-2.0))), 0.5);
    qfloat_t iz_p = qf_mul_double(qf_sub(qf_exp(qf_from_double(0.5)), QF_ONE), 2.0);
    qfloat_t i_pos = qf_mul(QF_E, qf_mul(ix_p, qf_mul(iy_p, iz_p)));

    qfloat_t ix_n = qf_sub(QF_ONE, qf_div(QF_ONE, QF_E));
    qfloat_t iy_n = qf_mul_double(qf_sub(qf_exp(qf_from_double(2.0)), QF_ONE), 0.5);
    qfloat_t iz_n = qf_mul_double(qf_sub(QF_ONE, qf_exp(qf_from_double(-0.5))), 2.0);
    qfloat_t i_neg = qf_mul(qf_div(QF_ONE, QF_E), qf_mul(ix_n, qf_mul(iy_n, iz_n)));
    qfloat_t expected = qf_mul_double(qf_sub(i_pos, i_neg), 0.5);

    printf("  ∫₀¹∫₀¹∫₀¹ sinh(x-2y+0.5z+1) dx dy dz  [affine sinh]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol27));

    dv_free(expr);
    dv_free(affine);
    dv_free(sum_xyz);
    dv_free(sum_xy);
    dv_free(halfz);
    dv_free(neg2y);
    dv_free(z);
    dv_free(y);
    dv_free(x);
    ig_free(ig);
}

void test_multi_3d_cosh_affine(void) {
    /* ∫₀¹∫₀¹∫₀¹ cosh(1.5x + y - z + 0.25) dV */
    integrator_t *ig = ig_new();
    dval_t *x = dv_new_var(qf_from_double(0.0));
    dval_t *y = dv_new_var(qf_from_double(0.0));
    dval_t *z = dv_new_var(qf_from_double(0.0));
    dval_t *onept5x = dv_mul_d(x, 1.5);
    dval_t *negz = dv_neg(z);
    dval_t *sum_xy = dv_add(onept5x, y);
    dval_t *sum_xyz = dv_add(sum_xy, negz);
    dval_t *affine = dv_add_d(sum_xyz, 0.25);
    dval_t *expr = dv_cosh(affine);

    dval_t *vars[3] = { x, y, z };
    qfloat_t lo[3] = { qf_from_double(0.0), qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[3] = { qf_from_double(1.0), qf_from_double(1.0), qf_from_double(1.0) };

    qfloat_t result, err;
    int s = ig_integral_multi(ig, expr, 3, vars, lo, hi, &result, &err);

    qfloat_t ix_p = qf_div(qf_sub(qf_exp(qf_from_double(1.5)), QF_ONE),
                           qf_from_double(1.5));
    qfloat_t iy_p = qf_sub(QF_E, QF_ONE);
    qfloat_t iz_p = qf_sub(QF_ONE, qf_div(QF_ONE, QF_E));
    qfloat_t i_pos = qf_mul(qf_exp(qf_from_double(0.25)), qf_mul(ix_p, qf_mul(iy_p, iz_p)));

    qfloat_t ix_n = qf_div(qf_sub(QF_ONE, qf_exp(qf_from_double(-1.5))),
                           qf_from_double(1.5));
    qfloat_t iy_n = qf_sub(QF_ONE, qf_div(QF_ONE, QF_E));
    qfloat_t iz_n = qf_sub(QF_E, QF_ONE);
    qfloat_t i_neg = qf_mul(qf_exp(qf_from_double(-0.25)), qf_mul(ix_n, qf_mul(iy_n, iz_n)));
    qfloat_t expected = qf_mul_double(qf_add(i_pos, i_neg), 0.5);

    printf("  ∫₀¹∫₀¹∫₀¹ cosh(1.5x+y-z+0.25) dx dy dz  [affine cosh]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol27));

    dv_free(expr);
    dv_free(affine);
    dv_free(sum_xyz);
    dv_free(sum_xy);
    dv_free(negz);
    dv_free(onept5x);
    dv_free(z);
    dv_free(y);
    dv_free(x);
    ig_free(ig);
}

void test_multi_3d_sin_affine(void) {
    /* ∫₀¹∫₀¹∫₀¹ sin(x+2y-z+0.3) dx dy dz  [affine sin] */
    integrator_t *ig = ig_new();
    dval_t *x = dv_new_var(qf_from_double(0.0));
    dval_t *y = dv_new_var(qf_from_double(0.0));
    dval_t *z = dv_new_var(qf_from_double(0.0));
    dval_t *two_y = dv_mul_d(y, 2.0);
    dval_t *neg_z = dv_neg(z);
    dval_t *sum_xy = dv_add(x, two_y);
    dval_t *sum_xyz = dv_add(sum_xy, neg_z);
    dval_t *affine = dv_add_d(sum_xyz, 0.3);
    dval_t *expr = dv_sin(affine);

    dval_t *vars[3] = { x, y, z };
    qfloat_t lo[3] = { qf_from_double(0.0), qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[3] = { qf_from_double(1.0), qf_from_double(1.0), qf_from_double(1.0) };

    qfloat_t result, err;
    int s = ig_integral_multi(ig, expr, 3, vars, lo, hi, &result, &err);

    qcomplex_t expected_z = qc_mul(qc_exp(qc_make(QF_ZERO, qf_from_double(0.3))),
                                   qc_mul(qc_div(qc_sub(qc_exp(qc_make(QF_ZERO, qf_from_double(1.0))), QC_ONE),
                                                 qc_make(QF_ZERO, qf_from_double(1.0))),
                                          qc_mul(qc_div(qc_sub(qc_exp(qc_make(QF_ZERO, qf_from_double(2.0))), QC_ONE),
                                                        qc_make(QF_ZERO, qf_from_double(2.0))),
                                                 qc_div(qc_sub(qc_exp(qc_make(QF_ZERO, qf_from_double(-1.0))), QC_ONE),
                                                        qc_make(QF_ZERO, qf_from_double(-1.0))))));
    qfloat_t expected = expected_z.im;

    printf("  ∫₀¹∫₀¹∫₀¹ sin(x+2y-z+0.3) dx dy dz  [affine sin]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol27));

    dv_free(expr);
    dv_free(affine);
    dv_free(sum_xyz);
    dv_free(sum_xy);
    dv_free(neg_z);
    dv_free(two_y);
    dv_free(z);
    dv_free(y);
    dv_free(x);
    ig_free(ig);
}

void test_multi_3d_cos_affine(void) {
    /* ∫₀¹∫₀¹∫₀¹ cos(0.5x-y+1.5z-0.2) dx dy dz  [affine cos] */
    integrator_t *ig = ig_new();
    dval_t *x = dv_new_var(qf_from_double(0.0));
    dval_t *y = dv_new_var(qf_from_double(0.0));
    dval_t *z = dv_new_var(qf_from_double(0.0));
    dval_t *half_x = dv_mul_d(x, 0.5);
    dval_t *neg_y = dv_neg(y);
    dval_t *onept5_z = dv_mul_d(z, 1.5);
    dval_t *sum_xy = dv_add(half_x, neg_y);
    dval_t *sum_xyz = dv_add(sum_xy, onept5_z);
    dval_t *affine = dv_sub_d(sum_xyz, 0.2);
    dval_t *expr = dv_cos(affine);

    dval_t *vars[3] = { x, y, z };
    qfloat_t lo[3] = { qf_from_double(0.0), qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[3] = { qf_from_double(1.0), qf_from_double(1.0), qf_from_double(1.0) };

    qfloat_t result, err;
    int s = ig_integral_multi(ig, expr, 3, vars, lo, hi, &result, &err);

    qcomplex_t expected_z = qc_mul(qc_exp(qc_make(QF_ZERO, qf_from_double(-0.2))),
                                   qc_mul(qc_div(qc_sub(qc_exp(qc_make(QF_ZERO, qf_from_double(0.5))), QC_ONE),
                                                 qc_make(QF_ZERO, qf_from_double(0.5))),
                                          qc_mul(qc_div(qc_sub(qc_exp(qc_make(QF_ZERO, qf_from_double(-1.0))), QC_ONE),
                                                        qc_make(QF_ZERO, qf_from_double(-1.0))),
                                                 qc_div(qc_sub(qc_exp(qc_make(QF_ZERO, qf_from_double(1.5))), QC_ONE),
                                                        qc_make(QF_ZERO, qf_from_double(1.5))))));
    qfloat_t expected = expected_z.re;

    printf("  ∫₀¹∫₀¹∫₀¹ cos(0.5x-y+1.5z-0.2) dx dy dz  [affine cos]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol27));

    dv_free(expr);
    dv_free(affine);
    dv_free(sum_xyz);
    dv_free(sum_xy);
    dv_free(onept5_z);
    dv_free(neg_y);
    dv_free(half_x);
    dv_free(z);
    dv_free(y);
    dv_free(x);
    ig_free(ig);
}

void test_multi_3d_scaled_sum_specials(void) {
    /* 2*exp(x+y) - 3*cosh(z+0.5) + 4 */
    integrator_t *ig = ig_new();
    dval_t *x = dv_new_var(qf_from_double(0.0));
    dval_t *y = dv_new_var(qf_from_double(0.0));
    dval_t *z = dv_new_var(qf_from_double(0.0));
    dval_t *xy = dv_add(x, y);
    dval_t *exp_xy = dv_exp(xy);
    dval_t *term1 = dv_mul_d(exp_xy, 2.0);
    dval_t *zshift = dv_add_d(z, 0.5);
    dval_t *cosh_z = dv_cosh(zshift);
    dval_t *term2 = dv_mul_d(cosh_z, 3.0);
    dval_t *partial = dv_sub(term1, term2);
    dval_t *expr = dv_add_d(partial, 4.0);

    dval_t *vars[3] = { x, y, z };
    qfloat_t lo[3] = { qf_from_double(0.0), qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[3] = { qf_from_double(1.0), qf_from_double(1.0), qf_from_double(1.0) };

    qfloat_t result, err;
    int s = ig_integral_multi(ig, expr, 3, vars, lo, hi, &result, &err);

    qfloat_t em1 = qf_sub(QF_E, QF_ONE);
    qfloat_t term1_expected = qf_mul_double(qf_mul(em1, em1), 2.0);
    qfloat_t term2_expected = qf_mul_double(qf_sub(qf_sinh(qf_from_double(1.5)),
                                                   qf_sinh(qf_from_double(0.5))), 3.0);
    qfloat_t expected = qf_add(qf_sub(term1_expected, term2_expected), qf_from_double(4.0));

    printf("  ∫ (2exp(x+y)-3cosh(z+0.5)+4) dV  [scaled sum specials]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol27));

    dv_free(expr);
    dv_free(partial);
    dv_free(term2);
    dv_free(cosh_z);
    dv_free(zshift);
    dv_free(term1);
    dv_free(exp_xy);
    dv_free(xy);
    dv_free(z);
    dv_free(y);
    dv_free(x);
    ig_free(ig);
}

void test_multi_2d_sum_of_specials(void) {
    /* sin(x+0.2) + cos(2y-0.1) + exp(x-y) */
    integrator_t *ig = ig_new();
    dval_t *x = dv_new_var(qf_from_double(0.0));
    dval_t *y = dv_new_var(qf_from_double(0.0));
    dval_t *sin_arg = dv_add_d(x, 0.2);
    dval_t *sin_term = dv_sin(sin_arg);
    dval_t *two_y = dv_mul_d(y, 2.0);
    dval_t *cos_arg = dv_sub_d(two_y, 0.1);
    dval_t *cos_term = dv_cos(cos_arg);
    dval_t *exp_arg = dv_sub(x, y);
    dval_t *exp_term = dv_exp(exp_arg);
    dval_t *sum1 = dv_add(sin_term, cos_term);
    dval_t *expr = dv_add(sum1, exp_term);

    dval_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };

    qfloat_t result, err;
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    qcomplex_t sin_expected_z = qc_mul(qc_exp(qc_make(QF_ZERO, qf_from_double(0.2))),
                                       qc_div(qc_sub(qc_exp(qc_make(QF_ZERO, qf_from_double(1.0))), QC_ONE),
                                              qc_make(QF_ZERO, qf_from_double(1.0))));
    qfloat_t sin_expected = sin_expected_z.im;
    qcomplex_t cos_expected_z = qc_mul(qc_exp(qc_make(QF_ZERO, qf_from_double(-0.1))),
                                       qc_div(qc_sub(qc_exp(qc_make(QF_ZERO, qf_from_double(2.0))), QC_ONE),
                                              qc_make(QF_ZERO, qf_from_double(2.0))));
    qfloat_t cos_expected = cos_expected_z.re;
    qfloat_t exp_expected = qf_mul(qf_sub(QF_E, QF_ONE),
                                   qf_sub(QF_ONE, qf_div(QF_ONE, QF_E)));
    qfloat_t expected = qf_add(qf_add(sin_expected, cos_expected), exp_expected);

    printf("  ∫∫ [sin(x+0.2)+cos(2y-0.1)+exp(x-y)] dA  [sum specials]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol27));

    dv_free(expr);
    dv_free(sum1);
    dv_free(exp_term);
    dv_free(exp_arg);
    dv_free(cos_term);
    dv_free(cos_arg);
    dv_free(two_y);
    dv_free(sin_term);
    dv_free(sin_arg);
    dv_free(y);
    dv_free(x);
    ig_free(ig);
}

void test_multi_3d_separable_product(void) {
    /* exp(x) * cos(2y-0.1) * sinh(z+0.2) */
    integrator_t *ig = ig_new();
    dval_t *x = dv_new_var(qf_from_double(0.0));
    dval_t *y = dv_new_var(qf_from_double(0.0));
    dval_t *z = dv_new_var(qf_from_double(0.0));
    dval_t *exp_x = dv_exp(x);
    dval_t *two_y = dv_mul_d(y, 2.0);
    dval_t *cos_arg = dv_sub_d(two_y, 0.1);
    dval_t *cos_y = dv_cos(cos_arg);
    dval_t *sinh_arg = dv_add_d(z, 0.2);
    dval_t *sinh_z = dv_sinh(sinh_arg);
    dval_t *prod_xy = dv_mul(exp_x, cos_y);
    dval_t *expr = dv_mul(prod_xy, sinh_z);

    dval_t *vars[3] = { x, y, z };
    qfloat_t lo[3] = { qf_from_double(0.0), qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[3] = { qf_from_double(1.0), qf_from_double(1.0), qf_from_double(1.0) };

    qfloat_t result, err;
    int s = ig_integral_multi(ig, expr, 3, vars, lo, hi, &result, &err);

    qfloat_t exp1_minus_1 = qf_sub(qf_exp(qf_from_double(1.0)), QF_ONE);
    qfloat_t cos_part = qf_mul_double(qf_sub(qf_sin(qf_from_double(1.9)),
                                             qf_sin(qf_from_double(-0.1))), 0.5);
    qfloat_t sinh_part = qf_sub(qf_cosh(qf_from_double(1.2)),
                                qf_cosh(qf_from_double(0.2)));
    qfloat_t left_part = qf_mul(exp1_minus_1, cos_part);
    qfloat_t expected = qf_mul(left_part, sinh_part);

    printf("  ∫ exp(x)cos(2y-0.1)sinh(z+0.2) dV  [separable product]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol15));

    dv_free(expr);
    dv_free(prod_xy);
    dv_free(sinh_z);
    dv_free(sinh_arg);
    dv_free(cos_y);
    dv_free(cos_arg);
    dv_free(two_y);
    dv_free(exp_x);
    dv_free(z);
    dv_free(y);
    dv_free(x);
    ig_free(ig);
}

void test_multi_2d_sum_of_separable_products(void) {
    /* exp(x)cos(y) + sinh(x+0.1)exp(y) */
    integrator_t *ig = ig_new();
    dval_t *x = dv_new_var(qf_from_double(0.0));
    dval_t *y = dv_new_var(qf_from_double(0.0));
    dval_t *exp_x = dv_exp(x);
    dval_t *cos_y = dv_cos(y);
    dval_t *term1 = dv_mul(exp_x, cos_y);
    dval_t *sinh_x = dv_sinh(dv_add_d(x, 0.1));
    dval_t *exp_y = dv_exp(y);
    dval_t *term2 = dv_mul(sinh_x, exp_y);
    dval_t *expr = dv_add(term1, term2);

    dval_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };

    qfloat_t result, err;
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    qfloat_t exp1_minus_1 = qf_sub(qf_exp(qf_from_double(1.0)), QF_ONE);
    qfloat_t term1_expected = qf_mul(exp1_minus_1, qf_sin(qf_from_double(1.0)));
    qfloat_t term2_expected = qf_mul(qf_sub(qf_cosh(qf_from_double(1.1)),
                                            qf_cosh(qf_from_double(0.1))),
                                     exp1_minus_1);
    qfloat_t expected = qf_add(term1_expected, term2_expected);

    printf("  ∫∫ [exp(x)cos(y)+sinh(x+0.1)exp(y)] dA  [sum separable products]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol15));

    dv_free(expr);
    dv_free(term2);
    dv_free(exp_y);
    dv_free(sinh_x);
    dv_free(term1);
    dv_free(cos_y);
    dv_free(exp_x);
    dv_free(y);
    dv_free(x);
    ig_free(ig);
}

/* -----------------------------------------------------------------------
 * README examples
 * --------------------------------------------------------------------- */

static qfloat_t fn_gaussian(qfloat_t x, void *ctx) {
    (void)ctx;
    return qf_exp(qf_neg(qf_sqr(x)));
}

void example_integrator(void) {
    /* ∫₋₃³ exp(-x²) dx ≈ √π * erf(3) */
    integrator_t *ig = ig_new();
    ig_set_tolerance(ig, qf_from_string("1e-21"), qf_from_string("1e-21"));
    qfloat_t result, err;
    ig_integral(ig, fn_gaussian, NULL,
                qf_from_double(-3.0), qf_from_double(3.0),
                &result, &err);

    qf_printf("∫₋₃³ exp(-x²) dx ≈ %q\n", result);
    qf_printf("  error estimate   ≈ %q\n", err);
    printf("  subintervals used: %zu\n", ig_get_interval_count_used(ig));
    ig_free(ig);
}

typedef struct { qfloat_t exponent; } power_ctx;

static qfloat_t fn_power(qfloat_t x, void *ctx) {
    power_ctx *pc = ctx;
    return qf_pow(x, pc->exponent);
}

void example_ctx(void) {
    /* ∫₀¹ x^2.5 dx = 1/3.5 */
    integrator_t *ig = ig_new();
    ig_set_tolerance(ig, qf_from_string("1e-21"), qf_from_string("1e-21"));
    power_ctx ctx = { qf_from_string("2.5") };
    qfloat_t result, err;
    ig_integral(ig, fn_power, &ctx,
                qf_from_double(0.0), qf_from_double(1.0),
                &result, &err);

    qf_printf("∫₀¹ x^2.5 dx ≈ %q\n", result);
    qf_printf("  error estimate   ≈ %q\n", err);
    ig_free(ig);
}

void example_integrator_dv(void) {
    /* ∫₋₃³ exp(-x²) dx using Turán T15/T4 with automatic differentiation.
     * Exact value: √π · erf(3).  Compare interval count with example_integrator(). */
    integrator_t *ig = ig_new();

    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *x2   = dv_mul(x, x);
    dval_t *negx2 = dv_neg(x2);
    dval_t *expr = dv_exp(negx2);

    qfloat_t result, err;
    ig_single_integral(ig, expr, x,
                       qf_from_double(-3.0), qf_from_double(3.0),
                       &result, &err);

    qf_printf("∫₋₃³ exp(-x²) dx ≈ %q\n", result);
    qf_printf("  error estimate   ≈ %q\n", err);
    printf("  subintervals used: %zu\n", ig_get_interval_count_used(ig));

    dv_free(expr);
    dv_free(negx2);
    dv_free(x2);
    dv_free(x);
    ig_free(ig);
}

/* -----------------------------------------------------------------------
 * Entry point
 * --------------------------------------------------------------------- */

int tests_main(void) {
    printf(C_BOLD C_CYAN "=== Lifecycle Tests ===\n" C_RESET);
    RUN_TEST(test_create_and_destroy, NULL);
    RUN_TEST(test_null_safety, NULL);

    printf(C_BOLD C_CYAN "=== Integral Value Tests ===\n" C_RESET);
    RUN_TEST(test_polynomial, NULL);
    RUN_TEST(test_sin, NULL);
    RUN_TEST(test_exp, NULL);
    RUN_TEST(test_arctan, NULL);
    RUN_TEST(test_log, NULL);
    RUN_TEST(test_constant, NULL);
    RUN_TEST(test_linear, NULL);
    RUN_TEST(test_reversed_limits, NULL);

    printf(C_BOLD C_CYAN "=== Configuration Tests ===\n" C_RESET);
    RUN_TEST(test_set_tol, NULL);
    RUN_TEST(test_max_intervals, NULL);
    RUN_TEST(test_last_intervals, NULL);

    printf(C_BOLD C_CYAN "=== Turán T15/T4 dval_t Tests ===\n" C_RESET);
    RUN_TEST(test_dv_sin, NULL);
    RUN_TEST(test_dv_exp, NULL);
    RUN_TEST(test_dv_arctan, NULL);
    RUN_TEST(test_dv_null_safety, NULL);

    printf(C_BOLD C_CYAN "=== ig_double_integral Tests ===\n" C_RESET);
    RUN_TEST(test_double_polynomial, NULL);
    RUN_TEST(test_double_exp, NULL);
    RUN_TEST(test_double_nonunit_bounds, NULL);
    RUN_TEST(test_double_null_safety, NULL);

    printf(C_BOLD C_CYAN "=== ig_triple_integral Tests ===\n" C_RESET);
    RUN_TEST(test_triple_polynomial, NULL);
    RUN_TEST(test_triple_exp, NULL);
    RUN_TEST(test_triple_null_safety, NULL);

    printf(C_BOLD C_CYAN "=== N-dimensional Turán T15/T4 Tests ===\n" C_RESET);
    RUN_TEST(test_multi_2d, NULL);
    RUN_TEST(test_multi_3d, NULL);
    RUN_TEST(test_multi_null_safety, NULL);
    RUN_TEST(test_multi_nd1, NULL);
    RUN_TEST(test_multi_4d, NULL);
    RUN_TEST(test_multi_4d_exp, NULL);
    RUN_TEST(test_multi_4d_exp_affine, NULL);
    RUN_TEST(test_multi_3d_sinh_affine, NULL);
    RUN_TEST(test_multi_3d_cosh_affine, NULL);
    RUN_TEST(test_multi_3d_sin_affine, NULL);
    RUN_TEST(test_multi_3d_cos_affine, NULL);
    RUN_TEST(test_multi_3d_scaled_sum_specials, NULL);
    RUN_TEST(test_multi_2d_sum_of_specials, NULL);
    RUN_TEST(test_multi_3d_separable_product, NULL);
    RUN_TEST(test_multi_2d_sum_of_separable_products, NULL);

    printf(C_BOLD C_GREEN "\n=== README Output Examples ===\n" C_RESET);
    printf(C_BOLD C_YELLOW "Example: Gaussian integral\n" C_RESET);
    example_integrator();
    printf(C_BOLD C_YELLOW "\nExample: Power function with context\n" C_RESET);
    example_ctx();
    printf(C_BOLD C_YELLOW "\nExample: Gaussian via Turán T15/T4 + AD\n" C_RESET);
    example_integrator_dv();

    return tests_failed;
}
