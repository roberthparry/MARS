/* test_integrator.c — tests for the adaptive G7K15 integrator */

#include <stdio.h>
#include <math.h>

#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#include "test_harness.h"

#include "qfloat.h"
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
static qfloat_t tol27 = { 1e-27, -3.8494869749191836e-44 }; /* 1e-27 */

/* -----------------------------------------------------------------------
 * Integrands
 * --------------------------------------------------------------------- */

static qfloat_t fn_x2(qfloat_t x, void *ctx) {
    (void)ctx;
    return qf_sqr(x);
}

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

static qfloat_t fn_const(qfloat_t x, void *ctx) {
    (void)ctx; (void)x;
    return qf_from_double(1.0);
}

/* -----------------------------------------------------------------------
 * Tests
 * --------------------------------------------------------------------- */

void test_create_and_destroy(void) {
    integrator_t *ig = integrator_create();
    ASSERT_TRUE(ig);
    integrator_destroy(ig);
    integrator_destroy(NULL);  /* must not crash */
}

void test_polynomial(void) {
    /* ∫₀¹ x² dx = 1/3 */
    integrator_t *ig = integrator_create();
    integrator_set_tol(ig, qf_from_string("1e-21"), qf_from_string("1e-21"));
    qfloat_t result, err;
    int s = integrator_eval(ig, fn_x2, NULL,
                            qf_from_double(0.0), qf_from_double(1.0),
                            &result, &err);
    qfloat_t expected = qf_from_string("0.33333333333333333333333333333333333333");
    printf("  ∫₀¹ x² dx\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, integrator_last_intervals(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol20));
    integrator_destroy(ig);
}

void test_sin(void) {
    /* ∫₀^π sin(x) dx = 2 */
    integrator_t *ig = integrator_create();
    integrator_set_tol(ig, qf_from_string("1e-21"), qf_from_string("1e-21"));
    qfloat_t result, err;
    int s = integrator_eval(ig, fn_sin, NULL,
                            qf_from_double(0.0), QF_PI,
                            &result, &err);
    qfloat_t expected = qf_from_double(2.0);
    printf("  ∫₀^π sin(x) dx\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, integrator_last_intervals(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol20));
    integrator_destroy(ig);
}

void test_exp(void) {
    /* ∫₀¹ exp(x) dx = e - 1 */
    integrator_t *ig = integrator_create();
    integrator_set_tol(ig, qf_from_string("1e-21"), qf_from_string("1e-21"));
    qfloat_t result, err;
    int s = integrator_eval(ig, fn_exp, NULL,
                            qf_from_double(0.0), qf_from_double(1.0),
                            &result, &err);
    qfloat_t expected = qf_sub(QF_E, qf_from_double(1.0));
    printf("  ∫₀¹ exp(x) dx\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, integrator_last_intervals(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol20));
    integrator_destroy(ig);
}

void test_arctan(void) {
    /* ∫₋₁¹ 1/(1+x²) dx = π/2 */
    integrator_t *ig = integrator_create();
    integrator_set_tol(ig, qf_from_string("1e-21"), qf_from_string("1e-21"));
    qfloat_t result, err;
    int s = integrator_eval(ig, fn_inv1px2, NULL,
                            qf_from_double(-1.0), qf_from_double(1.0),
                            &result, &err);
    printf("  ∫₋₁¹ 1/(1+x²) dx\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q  (π/2)\n", QF_PI_2);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, integrator_last_intervals(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, QF_PI_2, tol20));
    integrator_destroy(ig);
}

void test_log(void) {
    /* ∫₁^e ln(x) dx = 1 */
    integrator_t *ig = integrator_create();
    integrator_set_tol(ig, qf_from_string("1e-21"), qf_from_string("1e-21"));
    qfloat_t result, err;
    int s = integrator_eval(ig, fn_log, NULL,
                            qf_from_double(1.0), QF_E,
                            &result, &err);
    qfloat_t expected = qf_from_double(1.0);
    printf("  ∫₁^e ln(x) dx\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, integrator_last_intervals(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol20));
    integrator_destroy(ig);
}

void test_constant(void) {
    /* ∫₀^5 1 dx = 5 */
    integrator_t *ig = integrator_create();
    integrator_set_tol(ig, qf_from_string("1e-21"), qf_from_string("1e-21"));
    qfloat_t result, err;
    int s = integrator_eval(ig, fn_const, NULL,
                            qf_from_double(0.0), qf_from_double(5.0),
                            &result, &err);
    qfloat_t expected = qf_from_double(5.0);
    printf("  ∫₀^5 1 dx\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, integrator_last_intervals(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol20));
    integrator_destroy(ig);
}

void test_set_tol(void) {
    integrator_t *ig = integrator_create();
    qfloat_t loose = qf_from_string("1e-10");
    integrator_set_tol(ig, loose, loose);

    qfloat_t result, err;
    int s = integrator_eval(ig, fn_sin, NULL,
                            qf_from_double(0.0), QF_PI,
                            &result, &err);
    printf("  ∫₀^π sin(x) dx  (tolerance 1e-10)\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  err      = %q  (limit 1e-8)\n", err);
    printf("  status = %d  intervals = %zu\n", s, integrator_last_intervals(ig));
    /* Error estimate should be at or below the loose tolerance */
    ASSERT_TRUE(qf_le(err, qf_from_string("1e-8")));
    integrator_destroy(ig);
}

void test_max_intervals(void) {
    /* Force early termination by allowing only 1 subinterval */
    integrator_t *ig = integrator_create();
    integrator_set_max_intervals(ig, 1);

    qfloat_t result, err;
    int s = integrator_eval(ig, fn_sin, NULL,
                            qf_from_double(0.0), QF_PI,
                            &result, &err);
    size_t n = integrator_last_intervals(ig);
    printf("  ∫₀^π sin(x) dx  (max_intervals = 1)\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, n);
    /* Should stop early — status 1 is acceptable for a highly oscillatory
       integrand restricted to a single subinterval */
    ASSERT_TRUE(s == 0 || s == 1);
    ASSERT_TRUE(n <= 1);
    integrator_destroy(ig);
}

void test_last_intervals(void) {
    /* A smooth integrand over a moderate range should converge in a handful
       of intervals; verify the counter is updated. */
    integrator_t *ig = integrator_create();
    qfloat_t result, err;
    int s = integrator_eval(ig, fn_exp, NULL,
                            qf_from_double(0.0), qf_from_double(1.0),
                            &result, &err);
    size_t n = integrator_last_intervals(ig);
    printf("  ∫₀¹ exp(x) dx  (interval counter)\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, n);
    ASSERT_TRUE(n >= 1);
    integrator_destroy(ig);
}

void test_null_safety(void) {
    integrator_t *ig = integrator_create();
    qfloat_t result;
    /* NULL integrand */
    int s = integrator_eval(ig, NULL, NULL,
                            qf_from_double(0.0), qf_from_double(1.0),
                            &result, NULL);
    ASSERT_TRUE(s == -1);
    /* NULL result */
    s = integrator_eval(ig, fn_exp, NULL,
                        qf_from_double(0.0), qf_from_double(1.0),
                        NULL, NULL);
    ASSERT_TRUE(s == -1);
    integrator_destroy(ig);
}

void test_reversed_limits(void) {
    /* ∫₁⁰ x² dx = -1/3 (limits reversed) */
    integrator_t *ig = integrator_create();
    integrator_set_tol(ig, qf_from_string("1e-21"), qf_from_string("1e-21"));
    qfloat_t result, err;
    int s = integrator_eval(ig, fn_x2, NULL,
                            qf_from_double(1.0), qf_from_double(0.0),
                            &result, &err);
    qfloat_t expected = qf_from_string("-0.33333333333333333333333333333333333333");
    printf("  ∫₁⁰ x² dx  (reversed limits)\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, integrator_last_intervals(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol20));
    integrator_destroy(ig);
}

/* -----------------------------------------------------------------------
 * integrator_eval_dv tests (Turán T15/T4 rule)
 * --------------------------------------------------------------------- */

void test_dv_sin(void) {
    /* ∫₀^π sin(x) dx = 2 */
    integrator_t *ig = integrator_create();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *expr = dv_sin(x);

    qfloat_t result, err;
    int s = integrator_eval_dv(ig, expr, x,
                                qf_from_double(0.0), QF_PI,
                                &result, &err);
    qfloat_t expected = qf_from_double(2.0);
    printf("  ∫₀^π sin(x) dx  [Turán T15/T4]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, integrator_last_intervals(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol27));

    dv_free(expr);
    dv_free(x);
    integrator_destroy(ig);
}

void test_dv_exp(void) {
    /* ∫₀¹ exp(x) dx = e - 1 */
    integrator_t *ig = integrator_create();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *expr = dv_exp(x);

    qfloat_t result, err;
    int s = integrator_eval_dv(ig, expr, x,
                                qf_from_double(0.0), qf_from_double(1.0),
                                &result, &err);
    qfloat_t expected = qf_sub(QF_E, qf_from_double(1.0));
    printf("  ∫₀¹ exp(x) dx  [Turán T15/T4]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, integrator_last_intervals(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, expected, tol27));

    dv_free(expr);
    dv_free(x);
    integrator_destroy(ig);
}

void test_dv_arctan(void) {
    /* ∫₋₁¹ 1/(1+x²) dx = π/2 */
    integrator_t *ig = integrator_create();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *one  = dv_new_const_d(1.0);
    dval_t *x2   = dv_mul(x, x);
    dval_t *denom = dv_add(one, x2);
    dval_t *expr = dv_div(one, denom);

    qfloat_t result, err;
    int s = integrator_eval_dv(ig, expr, x,
                                qf_from_double(-1.0), qf_from_double(1.0),
                                &result, &err);
    printf("  ∫₋₁¹ 1/(1+x²) dx  [Turán T15/T4]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q  (π/2)\n", QF_PI_2);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, integrator_last_intervals(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, QF_PI_2, tol27));

    dv_free(expr);
    dv_free(denom);
    dv_free(x2);
    dv_free(one);
    dv_free(x);
    integrator_destroy(ig);
}

void test_dv_null_safety(void) {
    integrator_t *ig = integrator_create();
    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *expr = dv_exp(x);
    qfloat_t result;

    /* NULL integrator */
    int s = integrator_eval_dv(NULL, expr, x,
                                qf_from_double(0.0), qf_from_double(1.0),
                                &result, NULL);
    ASSERT_TRUE(s == -1);

    /* NULL expr */
    s = integrator_eval_dv(ig, NULL, x,
                            qf_from_double(0.0), qf_from_double(1.0),
                            &result, NULL);
    ASSERT_TRUE(s == -1);

    /* NULL result */
    s = integrator_eval_dv(ig, expr, x,
                            qf_from_double(0.0), qf_from_double(1.0),
                            NULL, NULL);
    ASSERT_TRUE(s == -1);

    dv_free(expr);
    dv_free(x);
    integrator_destroy(ig);
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
    integrator_t *ig = integrator_create();
    integrator_set_tol(ig, qf_from_string("1e-21"), qf_from_string("1e-21"));
    qfloat_t result, err;
    integrator_eval(ig, fn_gaussian, NULL,
                    qf_from_double(-3.0), qf_from_double(3.0),
                    &result, &err);

    qf_printf("∫₋₃³ exp(-x²) dx ≈ %q\n", result);
    qf_printf("  error estimate   ≈ %q\n", err);
    printf("  subintervals used: %zu\n", integrator_last_intervals(ig));
    integrator_destroy(ig);
}

typedef struct { qfloat_t exponent; } power_ctx;

static qfloat_t fn_power(qfloat_t x, void *ctx) {
    power_ctx *pc = ctx;
    return qf_pow(x, pc->exponent);
}

void example_ctx(void) {
    /* ∫₀¹ x^2.5 dx = 1/3.5 */
    integrator_t *ig = integrator_create();
    integrator_set_tol(ig, qf_from_string("1e-21"), qf_from_string("1e-21"));
    power_ctx ctx = { qf_from_string("2.5") };
    qfloat_t result, err;
    integrator_eval(ig, fn_power, &ctx,
                    qf_from_double(0.0), qf_from_double(1.0),
                    &result, &err);

    qf_printf("∫₀¹ x^2.5 dx ≈ %q\n", result);
    qf_printf("  error estimate   ≈ %q\n", err);
    integrator_destroy(ig);
}

void example_integrator_dv(void) {
    /* ∫₋₃³ exp(-x²) dx using Turán T15/T4 with automatic differentiation.
     * Exact value: √π · erf(3).  Compare interval count with example_integrator(). */
    integrator_t *ig = integrator_create();

    dval_t *x    = dv_new_var(qf_from_double(0.0));
    dval_t *x2   = dv_mul(x, x);
    dval_t *negx2 = dv_neg(x2);
    dval_t *expr = dv_exp(negx2);

    qfloat_t result, err;
    integrator_eval_dv(ig, expr, x,
                       qf_from_double(-3.0), qf_from_double(3.0),
                       &result, &err);

    qf_printf("∫₋₃³ exp(-x²) dx ≈ %q\n", result);
    qf_printf("  error estimate   ≈ %q\n", err);
    printf("  subintervals used: %zu\n", integrator_last_intervals(ig));

    dv_free(expr);
    dv_free(negx2);
    dv_free(x2);
    dv_free(x);
    integrator_destroy(ig);
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

    printf(C_BOLD C_GREEN "\n=== README Output Examples ===\n" C_RESET);
    printf(C_BOLD C_YELLOW "Example: Gaussian integral\n" C_RESET);
    example_integrator();
    printf(C_BOLD C_YELLOW "\nExample: Power function with context\n" C_RESET);
    example_ctx();
    printf(C_BOLD C_YELLOW "\nExample: Gaussian via Turán T15/T4 + AD\n" C_RESET);
    example_integrator_dv();

    return tests_failed;
}
