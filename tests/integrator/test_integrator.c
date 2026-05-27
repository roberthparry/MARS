/* test_integrator.c — tests for the adaptive integrators */

#include <stdio.h>
#include <math.h>

#include "test_harness.h"

#include "qfloat.h"
#include "qcomplex.h"
#include "integrator.h"
#include "expression.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);
static int integrator_qfloat_validity_equal(const void *actual, const void *expected, void *ctx);
static void integrator_qfloat_validity_format(const void *value, char *buf, size_t buf_size, void *ctx);
static bool test_integrator_suite_setup(void);
static bool test_assert_integrator_qfloat_close_tol(qfloat_t actual,
                                                    qfloat_t expected,
                                                    qfloat_t tol,
                                                    const char *file,
                                                    int line);
TEST_SUITE_SETUP(test_integrator_suite_setup);

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
static const test_validity_contract_t integrator_qfloat_close_contract =
    TEST_VALIDITY_CONTRACT("integrator-qfloat-close",
                           integrator_qfloat_validity_equal,
                           integrator_qfloat_validity_format,
                           &tol15);

#define TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE(actual_value, expected_value) \
    do { \
        qfloat_t test_integrator_actual__ = (actual_value); \
        qfloat_t test_integrator_expected__ = (expected_value); \
        TEST_ASSERT_VALID_NAMED("integrator-qfloat-close", \
                                &test_integrator_actual__, \
                                &test_integrator_expected__); \
    } while (0)

#define TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(actual_value, expected_value, tol_value) \
    do { \
        if (!test_assert_integrator_qfloat_close_tol((actual_value), \
                                                     (expected_value), \
                                                     (tol_value), \
                                                     __FILE__, \
                                                     __LINE__)) \
            return; \
    } while (0)

static int integrator_qfloat_validity_equal(const void *actual, const void *expected, void *ctx)
{
    const qfloat_t *a = (const qfloat_t *)actual;
    const qfloat_t *b = (const qfloat_t *)expected;
    const qfloat_t tol = ctx ? *(const qfloat_t *)ctx : tol15;

    return qf_close(*a, *b, tol);
}

static void integrator_qfloat_validity_format(const void *value, char *buf, size_t buf_size, void *ctx)
{
    (void)ctx;
    qf_to_string(*(const qfloat_t *)value, buf, buf_size);
}

static bool test_integrator_suite_setup(void)
{
    test_register_validity_checker("integrator-qfloat-close",
                                   &integrator_qfloat_close_contract);
    return TEST_REQUIRE_VALIDITY_CHECKER("integrator-qfloat-close");
}

static bool test_assert_integrator_qfloat_close_tol(qfloat_t actual,
                                                    qfloat_t expected,
                                                    qfloat_t tol,
                                                    const char *file,
                                                    int line)
{
    const test_validity_contract_t contract =
        TEST_VALIDITY_CONTRACT("integrator-qfloat-close",
                               integrator_qfloat_validity_equal,
                               integrator_qfloat_validity_format,
                               &tol);

    return test_assert_validity(&contract, &actual, &expected, file, line);
}

static expr_t *test_expr_new_const_d(double x)
{
    number_t n = num_create_from_qfloat(qf_from_double(x));
    expr_t *dv = expr_new_const(n);

    num_destroy(&n);
    return dv;
}

static expr_t *test_expr_new_var_qf(qfloat_t x)
{
    number_t n = num_create_from_qfloat(x);
    expr_t *dv = expr_new_var(n);

    num_destroy(&n);
    return dv;
}

static expr_t *test_expr_add_d(const expr_t *dv, double x)
{
    number_t n = num_create_from_double(x);
    expr_t *out = expr_add_num(dv, &n);

    num_destroy(&n);
    return out;
}

static expr_t *test_expr_sub_d(const expr_t *dv, double x)
{
    number_t n = num_create_from_double(x);
    expr_t *out = expr_sub_num(dv, &n);

    num_destroy(&n);
    return out;
}

static expr_t *test_expr_mul_d(const expr_t *dv, double x)
{
    number_t n = num_create_from_double(x);
    expr_t *out = expr_mul_num(dv, &n);

    num_destroy(&n);
    return out;
}

static expr_t *test_expr_pow_d(const expr_t *dv, double x)
{
    number_t n = num_create_from_double(x);
    expr_t *out = expr_pow(dv, &n);

    num_destroy(&n);
    return out;
}

#define expr_add_d test_expr_add_d
#define expr_sub_d test_expr_sub_d
#define expr_mul_d test_expr_mul_d
#define expr_pow_d test_expr_pow_d

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
    expr_t *x    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *expr = expr_mul(x, x);

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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE(result, expected);
    expr_free(expr);
    expr_free(x);
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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE(result, expected);
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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE(result, expected);
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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, QF_PI_2, tol20);
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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE(result, expected);
    ig_free(ig);
}

void test_constant(void) {
    /* ∫₀^5 1 dx = 5 — constant integrand; Turán is polynomially exact at qfloat precision */
    integrator_t *ig = ig_new();
    expr_t *x    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *expr = test_expr_new_const_d(1.0);

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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE(result, expected);
    expr_free(expr);
    expr_free(x);
    ig_free(ig);
}

void test_linear(void) {
    /* ∫₀^5 x dx = 12.5 — linear integrand; Turán is polynomially exact at qfloat precision */
    integrator_t *ig = ig_new();
    expr_t *x    = test_expr_new_var_qf(qf_from_double(0.0));

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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE(result, expected);
    expr_free(x);
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
    expr_t *x    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *expr = expr_mul(x, x);

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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE(result, expected);
    expr_free(expr);
    expr_free(x);
    ig_free(ig);
}

/* -----------------------------------------------------------------------
 * ig_single_integral tests (Turán T15/T4 rule)
 * --------------------------------------------------------------------- */

void test_expr_sin(void) {
    /* ∫₀^π sin(x) dx = 2 */
    integrator_t *ig = ig_new();
    expr_t *x    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *expr = expr_sin(x);

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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE(result, expected);

    expr_free(expr);
    expr_free(x);
    ig_free(ig);
}

void test_expr_exp(void) {
    /* ∫₀¹ exp(x) dx = e - 1 */
    integrator_t *ig = ig_new();
    expr_t *x    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *expr = expr_exp(x);

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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol20);

    expr_free(expr);
    expr_free(x);
    ig_free(ig);
}

void test_expr_arctan(void) {
    /* ∫₋₁¹ 1/(1+x²) dx = π/2 */
    integrator_t *ig = ig_new();
    expr_t *x    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *one  = test_expr_new_const_d(1.0);
    expr_t *x2   = expr_mul(x, x);
    expr_t *denom = expr_add(one, x2);
    expr_t *expr = expr_div(one, denom);

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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, QF_PI_2, tol27);

    expr_free(expr);
    expr_free(denom);
    expr_free(x2);
    expr_free(one);
    expr_free(x);
    ig_free(ig);
}

void test_expr_null_safety(void) {
    integrator_t *ig = ig_new();
    expr_t *x    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *expr = expr_exp(x);
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

    expr_free(expr);
    expr_free(x);
    ig_free(ig);
}

/* -----------------------------------------------------------------------
 * ig_double_integral tests
 * --------------------------------------------------------------------- */

void test_double_polynomial(void) {
    /* ∫₀¹∫₀¹ x·y dx dy = 1/4 — polynomial; Turán is exact to full qfloat precision */
    integrator_t *ig = ig_new();
    expr_t *x    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *expr = expr_mul(x, y);

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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol20);
    expr_free(expr);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_double_exp(void) {
    /* ∫₀¹∫₀¹ exp(x+y) dx dy = (e−1)² */
    integrator_t *ig = ig_new();
    expr_t *x    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *sum  = expr_add(x, y);           // store intermediate
    expr_t *expr = expr_exp(sum);

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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol20);
    expr_free(expr);
    expr_free(sum);   // free intermediate
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_double_nonunit_bounds(void) {
    /* ∫₀²∫₀³ x·y dx dy = 9 — polynomial with non-unit bounds */
    integrator_t *ig = ig_new();
    expr_t *x    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *expr = expr_mul(x, y);

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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    expr_free(expr);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_double_null_safety(void) {
    integrator_t *ig = ig_new();
    expr_t *x    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *expr = expr_mul(x, y);
    qfloat_t result;
    qfloat_t z = qf_from_double(0.0), o = qf_from_double(1.0);

    ASSERT_TRUE(ig_double_integral(NULL, expr, x, z, o, y, z, o, &result, NULL) == -1);
    ASSERT_TRUE(ig_double_integral(ig, NULL, x, z, o, y, z, o, &result, NULL) == -1);
    ASSERT_TRUE(ig_double_integral(ig, expr, x, z, o, y, z, o, NULL, NULL) == -1);
    expr_free(expr);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

/* -----------------------------------------------------------------------
 * ig_triple_integral tests
 * --------------------------------------------------------------------- */

void test_triple_polynomial(void) {
    /* ∫₀¹∫₀¹∫₀¹ x·y·z dx dy dz = 1/8 — polynomial exact */
    integrator_t *ig = ig_new();
    expr_t *x    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *z    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *xy   = expr_mul(x, y);           // store intermediate
    expr_t *expr = expr_mul(xy, z);

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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    expr_free(expr);
    expr_free(xy);    // free intermediate
    expr_free(z);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_triple_exp(void) {
    /* ∫₀¹∫₀¹∫₀¹ exp(x+y+z) dx dy dz = (e−1)³ */
    integrator_t *ig = ig_new();
    expr_t *x    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *z    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *xy   = expr_add(x, y);           // store intermediate
    expr_t *xyz  = expr_add(xy, z);          // store intermediate
    expr_t *expr = expr_exp(xyz);

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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    expr_free(expr);
    expr_free(xyz);   // free intermediate
    expr_free(xy);    // free intermediate
    expr_free(z);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_triple_null_safety(void) {
    integrator_t *ig = ig_new();
    expr_t *x    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *z    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *xy   = expr_mul(x, y);           // store intermediate
    expr_t *expr = expr_mul(xy, z);
    qfloat_t result;
    qfloat_t lo = qf_from_double(0.0), hi = qf_from_double(1.0);

    ASSERT_TRUE(ig_triple_integral(NULL, expr, x, lo, hi, y, lo, hi, z, lo, hi, &result, NULL) == -1);
    ASSERT_TRUE(ig_triple_integral(ig, NULL, x, lo, hi, y, lo, hi, z, lo, hi, &result, NULL) == -1);
    ASSERT_TRUE(ig_triple_integral(ig, expr, x, lo, hi, y, lo, hi, z, lo, hi, NULL, NULL) == -1);

    expr_free(expr);
    expr_free(xy);    // free intermediate
    expr_free(z);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

/* -----------------------------------------------------------------------
 * ig_integral_multi tests (N-dimensional Turán T15/T4)
 * --------------------------------------------------------------------- */

void test_multi_2d(void) {
    /* ∫₀¹ ∫₀¹ (x+y) dx dy = 1 — linear; expect qfloat precision */
    integrator_t *ig = ig_new();
    expr_t *x    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *expr = expr_add(x, y);

    expr_t *vars[2] = { x, y };
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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);

    expr_free(expr);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_3d(void) {
    /* ∫₀¹ ∫₀¹ ∫₀¹ (x+y+z) dx dy dz = 1.5 — linear; expect qfloat precision */
    integrator_t *ig = ig_new();
    expr_t *x    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *z    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *xy   = expr_add(x, y);           // store intermediate
    expr_t *expr = expr_add(xy, z);

    expr_t *vars[3] = { x, y, z };
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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);

    expr_free(expr);
    expr_free(xy);    // free intermediate
    expr_free(z);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_null_safety(void) {
    integrator_t *ig = ig_new();
    expr_t *x    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *expr = expr_exp(x);
    expr_t *vars[1] = { x };
    qfloat_t lo[1] = { qf_from_double(0.0) };
    qfloat_t hi[1] = { qf_from_double(1.0) };
    qfloat_t result;

    ASSERT_TRUE(ig_integral_multi(NULL, expr, 1, vars, lo, hi, &result, NULL) == -1);
    ASSERT_TRUE(ig_integral_multi(ig, NULL, 1, vars, lo, hi, &result, NULL) == -1);
    ASSERT_TRUE(ig_integral_multi(ig, expr, 0, vars, lo, hi, &result, NULL) == -1);
    ASSERT_TRUE(ig_integral_multi(ig, expr, 1, vars, lo, hi, NULL, NULL) == -1);

    expr_free(expr);
    expr_free(x);
    ig_free(ig);
}

void test_multi_nd1(void) {
    /* ndim=1 degenerates to ig_single_integral: ∫₀¹ exp(x) dx = e−1 */
    integrator_t *ig = ig_new();
    expr_t *x    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *expr = expr_exp(x);
    expr_t *vars[1] = { x };
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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    expr_free(expr);
    expr_free(x);
    ig_free(ig);
}

void test_multi_4d(void) {
    /* ∫₀¹∫₀¹∫₀¹∫₀¹ (x+y+z+w) dx dy dz dw = 2.0 — linear polynomial in 4D, exact */
    integrator_t *ig = ig_new();
    expr_t *x    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *z    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *w    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *xy   = expr_add(x, y);           // store intermediate
    expr_t *zw   = expr_add(z, w);           // store intermediate
    expr_t *expr = expr_add(xy, zw);

    expr_t *vars[4] = { x, y, z, w };
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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    expr_free(expr);
    expr_free(zw);    // free intermediate
    expr_free(xy);    // free intermediate
    expr_free(w);
    expr_free(z);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_4d_exp(void) {
    /* ∫₀¹∫₀¹∫₀¹∫₀¹ exp(x+y+z+w) dx dy dz dw = (e−1)⁴ */
    integrator_t *ig = ig_new();
    expr_t *x    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *z    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *w    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *xy   = expr_add(x, y);           // store intermediate
    expr_t *zw   = expr_add(z, w);           // store intermediate
    expr_t *sum  = expr_add(xy, zw);         // store intermediate
    expr_t *expr = expr_exp(sum);

    expr_t *vars[4] = { x, y, z, w };
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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    expr_free(expr);
    expr_free(sum);   // free intermediate
    expr_free(zw);    // free intermediate
    expr_free(xy);    // free intermediate
    expr_free(w);
    expr_free(z);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_4d_exp_affine(void) {
    /* ∫ exp(2x - y + 0.5z + 3w + 1) dV = e * Π_i ∫ exp(a_i t) dt on [0,1]^4 */
    integrator_t *ig = ig_new();
    expr_t *x    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *z    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *w    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_x = expr_mul_d(x, 2.0);
    expr_t *neg_y = expr_neg(y);
    expr_t *half_z = expr_mul_d(z, 0.5);
    expr_t *three_w = expr_mul_d(w, 3.0);
    expr_t *sum_xy = expr_add(two_x, neg_y);
    expr_t *sum_xyz = expr_add(sum_xy, half_z);
    expr_t *sum_xyzw = expr_add(sum_xyz, three_w);
    expr_t *affine = expr_add_d(sum_xyzw, 1.0);
    expr_t *expr = expr_exp(affine);

    expr_t *vars[4] = { x, y, z, w };
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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);

    expr_free(expr);
    expr_free(affine);
    expr_free(sum_xyzw);
    expr_free(sum_xyz);
    expr_free(sum_xy);
    expr_free(three_w);
    expr_free(half_z);
    expr_free(neg_y);
    expr_free(two_x);
    expr_free(w);
    expr_free(z);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_3d_sinh_affine(void) {
    /* ∫₀¹∫₀¹∫₀¹ sinh(x - 2y + 0.5z + 1) dV */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *z = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *neg2y = expr_mul_d(y, -2.0);
    expr_t *halfz = expr_mul_d(z, 0.5);
    expr_t *sum_xy = expr_add(x, neg2y);
    expr_t *sum_xyz = expr_add(sum_xy, halfz);
    expr_t *affine = expr_add_d(sum_xyz, 1.0);
    expr_t *expr = expr_sinh(affine);

    expr_t *vars[3] = { x, y, z };
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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);

    expr_free(expr);
    expr_free(affine);
    expr_free(sum_xyz);
    expr_free(sum_xy);
    expr_free(halfz);
    expr_free(neg2y);
    expr_free(z);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_3d_cosh_affine(void) {
    /* ∫₀¹∫₀¹∫₀¹ cosh(1.5x + y - z + 0.25) dV */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *z = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *onept5x = expr_mul_d(x, 1.5);
    expr_t *negz = expr_neg(z);
    expr_t *sum_xy = expr_add(onept5x, y);
    expr_t *sum_xyz = expr_add(sum_xy, negz);
    expr_t *affine = expr_add_d(sum_xyz, 0.25);
    expr_t *expr = expr_cosh(affine);

    expr_t *vars[3] = { x, y, z };
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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);

    expr_free(expr);
    expr_free(affine);
    expr_free(sum_xyz);
    expr_free(sum_xy);
    expr_free(negz);
    expr_free(onept5x);
    expr_free(z);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_3d_sin_affine(void) {
    /* ∫₀¹∫₀¹∫₀¹ sin(x+2y-z+0.3) dx dy dz  [affine sin] */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *z = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *neg_z = expr_neg(z);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *sum_xyz = expr_add(sum_xy, neg_z);
    expr_t *affine = expr_add_d(sum_xyz, 0.3);
    expr_t *expr = expr_sin(affine);

    expr_t *vars[3] = { x, y, z };
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
    qfloat_t expected = qc_imag(expected_z);

    printf("  ∫₀¹∫₀¹∫₀¹ sin(x+2y-z+0.3) dx dy dz  [affine sin]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);

    expr_free(expr);
    expr_free(affine);
    expr_free(sum_xyz);
    expr_free(sum_xy);
    expr_free(neg_z);
    expr_free(two_y);
    expr_free(z);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_3d_cos_affine(void) {
    /* ∫₀¹∫₀¹∫₀¹ cos(0.5x-y+1.5z-0.2) dx dy dz  [affine cos] */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *z = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *half_x = expr_mul_d(x, 0.5);
    expr_t *neg_y = expr_neg(y);
    expr_t *onept5_z = expr_mul_d(z, 1.5);
    expr_t *sum_xy = expr_add(half_x, neg_y);
    expr_t *sum_xyz = expr_add(sum_xy, onept5_z);
    expr_t *affine = expr_sub_d(sum_xyz, 0.2);
    expr_t *expr = expr_cos(affine);

    expr_t *vars[3] = { x, y, z };
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
    qfloat_t expected = qc_real(expected_z);

    printf("  ∫₀¹∫₀¹∫₀¹ cos(0.5x-y+1.5z-0.2) dx dy dz  [affine cos]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);

    expr_free(expr);
    expr_free(affine);
    expr_free(sum_xyz);
    expr_free(sum_xy);
    expr_free(onept5_z);
    expr_free(neg_y);
    expr_free(half_x);
    expr_free(z);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_3d_scaled_sum_specials(void) {
    /* 2*exp(x+y) - 3*cosh(z+0.5) + 4 */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *z = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *xy = expr_add(x, y);
    expr_t *exp_xy = expr_exp(xy);
    expr_t *term1 = expr_mul_d(exp_xy, 2.0);
    expr_t *zshift = expr_add_d(z, 0.5);
    expr_t *cosh_z = expr_cosh(zshift);
    expr_t *term2 = expr_mul_d(cosh_z, 3.0);
    expr_t *partial = expr_sub(term1, term2);
    expr_t *expr = expr_add_d(partial, 4.0);

    expr_t *vars[3] = { x, y, z };
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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);

    expr_free(expr);
    expr_free(partial);
    expr_free(term2);
    expr_free(cosh_z);
    expr_free(zshift);
    expr_free(term1);
    expr_free(exp_xy);
    expr_free(xy);
    expr_free(z);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_sum_of_specials(void) {
    /* sin(x+0.2) + cos(2y-0.1) + exp(x-y) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *sin_arg = expr_add_d(x, 0.2);
    expr_t *sin_term = expr_sin(sin_arg);
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *cos_arg = expr_sub_d(two_y, 0.1);
    expr_t *cos_term = expr_cos(cos_arg);
    expr_t *exp_arg = expr_sub(x, y);
    expr_t *exp_term = expr_exp(exp_arg);
    expr_t *sum1 = expr_add(sin_term, cos_term);
    expr_t *expr = expr_add(sum1, exp_term);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };

    qfloat_t result, err;
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    qcomplex_t sin_expected_z = qc_mul(qc_exp(qc_make(QF_ZERO, qf_from_double(0.2))),
                                       qc_div(qc_sub(qc_exp(qc_make(QF_ZERO, qf_from_double(1.0))), QC_ONE),
                                              qc_make(QF_ZERO, qf_from_double(1.0))));
    qfloat_t sin_expected = qc_imag(sin_expected_z);
    qcomplex_t cos_expected_z = qc_mul(qc_exp(qc_make(QF_ZERO, qf_from_double(-0.1))),
                                       qc_div(qc_sub(qc_exp(qc_make(QF_ZERO, qf_from_double(2.0))), QC_ONE),
                                              qc_make(QF_ZERO, qf_from_double(2.0))));
    qfloat_t cos_expected = qc_real(cos_expected_z);
    qfloat_t exp_expected = qf_mul(qf_sub(QF_E, QF_ONE),
                                   qf_sub(QF_ONE, qf_div(QF_ONE, QF_E)));
    qfloat_t expected = qf_add(qf_add(sin_expected, cos_expected), exp_expected);

    printf("  ∫∫ [sin(x+0.2)+cos(2y-0.1)+exp(x-y)] dA  [sum specials]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);

    expr_free(expr);
    expr_free(sum1);
    expr_free(exp_term);
    expr_free(exp_arg);
    expr_free(cos_term);
    expr_free(cos_arg);
    expr_free(two_y);
    expr_free(sin_term);
    expr_free(sin_arg);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_3d_separable_product(void) {
    /* exp(x) * cos(2y-0.1) * sinh(z+0.2) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *z = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *exp_x = expr_exp(x);
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *cos_arg = expr_sub_d(two_y, 0.1);
    expr_t *cos_y = expr_cos(cos_arg);
    expr_t *sinh_arg = expr_add_d(z, 0.2);
    expr_t *sinh_z = expr_sinh(sinh_arg);
    expr_t *prod_xy = expr_mul(exp_x, cos_y);
    expr_t *expr = expr_mul(prod_xy, sinh_z);

    expr_t *vars[3] = { x, y, z };
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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE(result, expected);

    expr_free(expr);
    expr_free(prod_xy);
    expr_free(sinh_z);
    expr_free(sinh_arg);
    expr_free(cos_y);
    expr_free(cos_arg);
    expr_free(two_y);
    expr_free(exp_x);
    expr_free(z);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_3d_regrouped_separable_product(void) {
    /* (x*cos(y)) * (x*exp(z)) -> x^2 * cos(y) * exp(z) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *z = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *cos_y = expr_cos(y);
    expr_t *exp_z = expr_exp(z);
    expr_t *left = expr_mul(x, cos_y);
    expr_t *right = expr_mul(x, exp_z);
    expr_t *expr = expr_mul(left, right);

    expr_t *vars[3] = { x, y, z };
    qfloat_t lo[3] = { qf_from_double(0.0), qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[3] = { qf_from_double(1.0), qf_from_double(1.0), qf_from_double(1.0) };

    qfloat_t result, err;
    int s = ig_integral_multi(ig, expr, 3, vars, lo, hi, &result, &err);

    qfloat_t expected = qf_mul(qf_div(qf_from_double(1.0), qf_from_double(3.0)),
                               qf_mul(qf_sin(qf_from_double(1.0)),
                                      qf_sub(qf_exp(qf_from_double(1.0)), QF_ONE)));

    printf("  ∫ (x*cos(y))*(x*exp(z)) dV  [regrouped separable product]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE(result, expected);

    expr_free(expr);
    expr_free(right);
    expr_free(left);
    expr_free(exp_z);
    expr_free(cos_y);
    expr_free(z);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_sum_of_separable_products(void) {
    /* exp(x)cos(y) + sinh(x+0.1)exp(y) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *exp_x = expr_exp(x);
    expr_t *cos_y = expr_cos(y);
    expr_t *term1 = expr_mul(exp_x, cos_y);
    expr_t *x_shift = expr_add_d(x, 0.1);
    expr_t *sinh_x = expr_sinh(x_shift);
    expr_t *exp_y = expr_exp(y);
    expr_t *term2 = expr_mul(sinh_x, exp_y);
    expr_t *expr = expr_add(term1, term2);

    expr_t *vars[2] = { x, y };
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
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE(result, expected);

    expr_free(expr);
    expr_free(term2);
    expr_free(exp_y);
    expr_free(sinh_x);
    expr_free(x_shift);
    expr_free(term1);
    expr_free(cos_y);
    expr_free(exp_x);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_affine_square(void) {
    /* (x + 2y + 3)^2 */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *expr = expr_mul(affine, affine);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t expected = qf_div(qf_from_double(62.0), qf_from_double(3.0));
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^2 dA  [affine square]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_affine_cube(void) {
    /* (x + 2y + 3)^3 */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *square = expr_mul(affine, affine);
    expr_t *expr = expr_mul(square, affine);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t expected = qf_div(qf_from_double(387.0), qf_from_double(4.0));
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^3 dA  [affine cube]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(square);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_affine_quartic(void) {
    /* (x + 2y + 3)^4 */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *lhs = expr_mul(affine, affine);
    expr_t *rhs = expr_mul(affine, affine);
    expr_t *expr = expr_mul(lhs, rhs);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t expected = qf_div(qf_from_double(6916.0), qf_from_double(15.0));
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^4 dA  [affine quartic]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(rhs);
    expr_free(lhs);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_affine_poly_deg4(void) {
    /* 3(x + 2y + 3)^4 - 2(x + 2y + 3)^2 + (x + 2y + 3) + 7 */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *quartic = expr_pow_d(affine, 4.0);
    expr_t *square = expr_pow_d(affine, 2.0);
    expr_t *scaled_quartic = expr_mul_d(quartic, 3.0);
    expr_t *scaled_square = expr_mul_d(square, 2.0);
    expr_t *poly_core = expr_sub(scaled_quartic, scaled_square);
    expr_t *seven = test_expr_new_const_d(7.0);
    expr_t *affine_plus_seven = expr_add(affine, seven);
    expr_t *poly = expr_add(poly_core, affine_plus_seven);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t expected = qf_div(qf_from_double(40601.0), qf_from_double(30.0));
    int s = ig_integral_multi(ig, poly, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ [3a^4-2a^2+a+7] dA  [affine poly]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(poly);
    expr_free(affine_plus_seven);
    expr_free(seven);
    expr_free(poly_core);
    expr_free(scaled_square);
    expr_free(scaled_quartic);
    expr_free(square);
    expr_free(quartic);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_affine_times_exp_affine(void) {
    /* (x + 2y + 3) * exp(x + 2y + 3) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *exp_affine = expr_exp(affine);
    expr_t *expr = expr_mul(affine, exp_affine);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t e3 = qf_exp(qf_from_double(3.0));
    qfloat_t e4 = qf_exp(qf_from_double(4.0));
    qfloat_t e5 = qf_exp(qf_from_double(5.0));
    qfloat_t e6 = qf_exp(qf_from_double(6.0));
    qfloat_t expected = qf_add(qf_sub(qf_mul_double(e6, 2.0), e4),
                               qf_sub(qf_mul_double(e3, 0.5), qf_mul_double(e5, 1.5)));
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)exp(x+2y+3) dA  [affine*exp(affine)]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(exp_affine);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_square_affine_times_exp_affine(void) {
    /* (x + 2y + 3)^2 * exp(x + 2y + 3) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *square = expr_mul(affine, affine);
    expr_t *exp_affine = expr_exp(affine);
    expr_t *expr = expr_mul(square, exp_affine);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t e3 = qf_exp(qf_from_double(3.0));
    qfloat_t e4 = qf_exp(qf_from_double(4.0));
    qfloat_t e5 = qf_exp(qf_from_double(5.0));
    qfloat_t e6 = qf_exp(qf_from_double(6.0));
    qfloat_t expected = qf_mul_double(
        qf_add(qf_sub(qf_mul_double(e6, 18.0), qf_mul_double(e5, 11.0)),
               qf_sub(qf_mul_double(e3, 3.0), qf_mul_double(e4, 6.0))),
        0.5);
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^2exp(x+2y+3) dA  [affine^2*exp(affine)]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(exp_affine);
    expr_free(square);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_affine_times_sin_affine(void) {
    /* (x + 2y + 3) * sin(x + 2y + 3) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *sin_affine = expr_sin(affine);
    expr_t *expr = expr_mul(affine, sin_affine);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t u3 = qf_from_double(3.0);
    qfloat_t u4 = qf_from_double(4.0);
    qfloat_t u5 = qf_from_double(5.0);
    qfloat_t u6 = qf_from_double(6.0);
    qfloat_t f3 = qf_sub(qf_neg(qf_mul(u3, qf_sin(u3))), qf_mul_double(qf_cos(u3), 2.0));
    qfloat_t f4 = qf_sub(qf_neg(qf_mul(u4, qf_sin(u4))), qf_mul_double(qf_cos(u4), 2.0));
    qfloat_t f5 = qf_sub(qf_neg(qf_mul(u5, qf_sin(u5))), qf_mul_double(qf_cos(u5), 2.0));
    qfloat_t f6 = qf_sub(qf_neg(qf_mul(u6, qf_sin(u6))), qf_mul_double(qf_cos(u6), 2.0));
    qfloat_t expected = qf_mul_double(qf_sub(qf_sub(f6, f4), qf_sub(f5, f3)), 0.5);
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)sin(x+2y+3) dA  [affine*sin(affine)]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(sin_affine);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_square_affine_times_sin_affine(void) {
    /* (x + 2y + 3)^2 * sin(x + 2y + 3) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *square = expr_pow_d(affine, 2.0);
    expr_t *sin_affine = expr_sin(affine);
    expr_t *expr = expr_mul(square, sin_affine);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t u3 = qf_from_double(3.0);
    qfloat_t u4 = qf_from_double(4.0);
    qfloat_t u5 = qf_from_double(5.0);
    qfloat_t u6 = qf_from_double(6.0);
    qfloat_t f3 = qf_add(qf_neg(qf_mul(qf_mul(u3, u3), qf_sin(u3))),
                         qf_add(qf_mul_double(qf_mul(u3, qf_cos(u3)), -4.0),
                                qf_mul_double(qf_sin(u3), 6.0)));
    qfloat_t f4 = qf_add(qf_neg(qf_mul(qf_mul(u4, u4), qf_sin(u4))),
                         qf_add(qf_mul_double(qf_mul(u4, qf_cos(u4)), -4.0),
                                qf_mul_double(qf_sin(u4), 6.0)));
    qfloat_t f5 = qf_add(qf_neg(qf_mul(qf_mul(u5, u5), qf_sin(u5))),
                         qf_add(qf_mul_double(qf_mul(u5, qf_cos(u5)), -4.0),
                                qf_mul_double(qf_sin(u5), 6.0)));
    qfloat_t f6 = qf_add(qf_neg(qf_mul(qf_mul(u6, u6), qf_sin(u6))),
                         qf_add(qf_mul_double(qf_mul(u6, qf_cos(u6)), -4.0),
                                qf_mul_double(qf_sin(u6), 6.0)));
    qfloat_t expected = qf_mul_double(qf_sub(qf_sub(f6, f4), qf_sub(f5, f3)), 0.5);
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^2sin(x+2y+3) dA  [affine^2*sin(affine)]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(sin_affine);
    expr_free(square);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_affine_times_cos_affine(void) {
    /* (x + 2y + 3) * cos(x + 2y + 3) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *cos_affine = expr_cos(affine);
    expr_t *expr = expr_mul(cos_affine, affine);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t u3 = qf_from_double(3.0);
    qfloat_t u4 = qf_from_double(4.0);
    qfloat_t u5 = qf_from_double(5.0);
    qfloat_t u6 = qf_from_double(6.0);
    qfloat_t g3 = qf_add(qf_neg(qf_mul(u3, qf_cos(u3))), qf_mul_double(qf_sin(u3), 2.0));
    qfloat_t g4 = qf_add(qf_neg(qf_mul(u4, qf_cos(u4))), qf_mul_double(qf_sin(u4), 2.0));
    qfloat_t g5 = qf_add(qf_neg(qf_mul(u5, qf_cos(u5))), qf_mul_double(qf_sin(u5), 2.0));
    qfloat_t g6 = qf_add(qf_neg(qf_mul(u6, qf_cos(u6))), qf_mul_double(qf_sin(u6), 2.0));
    qfloat_t expected = qf_mul_double(qf_sub(qf_sub(g6, g4), qf_sub(g5, g3)), 0.5);
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)cos(x+2y+3) dA  [affine*cos(affine)]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(cos_affine);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_square_affine_times_cos_affine(void) {
    /* (x + 2y + 3)^2 * cos(x + 2y + 3) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *square = expr_mul(affine, affine);
    expr_t *cos_affine = expr_cos(affine);
    expr_t *expr = expr_mul(cos_affine, square);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t u3 = qf_from_double(3.0);
    qfloat_t u4 = qf_from_double(4.0);
    qfloat_t u5 = qf_from_double(5.0);
    qfloat_t u6 = qf_from_double(6.0);
    qfloat_t g3 = qf_add(qf_neg(qf_mul(qf_mul(u3, u3), qf_cos(u3))),
                         qf_add(qf_mul_double(qf_mul(u3, qf_sin(u3)), 4.0),
                                qf_mul_double(qf_cos(u3), 6.0)));
    qfloat_t g4 = qf_add(qf_neg(qf_mul(qf_mul(u4, u4), qf_cos(u4))),
                         qf_add(qf_mul_double(qf_mul(u4, qf_sin(u4)), 4.0),
                                qf_mul_double(qf_cos(u4), 6.0)));
    qfloat_t g5 = qf_add(qf_neg(qf_mul(qf_mul(u5, u5), qf_cos(u5))),
                         qf_add(qf_mul_double(qf_mul(u5, qf_sin(u5)), 4.0),
                                qf_mul_double(qf_cos(u5), 6.0)));
    qfloat_t g6 = qf_add(qf_neg(qf_mul(qf_mul(u6, u6), qf_cos(u6))),
                         qf_add(qf_mul_double(qf_mul(u6, qf_sin(u6)), 4.0),
                                qf_mul_double(qf_cos(u6), 6.0)));
    qfloat_t expected = qf_mul_double(qf_sub(qf_sub(g6, g4), qf_sub(g5, g3)), 0.5);
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^2cos(x+2y+3) dA  [affine^2*cos(affine)]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(cos_affine);
    expr_free(square);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_affine_times_sinh_affine(void) {
    /* (x + 2y + 3) * sinh(x + 2y + 3) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *sinh_affine = expr_sinh(affine);
    expr_t *expr = expr_mul(affine, sinh_affine);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t e3 = qf_exp(qf_from_double(3.0));
    qfloat_t e4 = qf_exp(qf_from_double(4.0));
    qfloat_t e5 = qf_exp(qf_from_double(5.0));
    qfloat_t e6 = qf_exp(qf_from_double(6.0));
    qfloat_t em3 = qf_exp(qf_from_double(-3.0));
    qfloat_t em4 = qf_exp(qf_from_double(-4.0));
    qfloat_t em5 = qf_exp(qf_from_double(-5.0));
    qfloat_t em6 = qf_exp(qf_from_double(-6.0));
    qfloat_t pos = qf_add(qf_sub(qf_mul_double(e6, 2.0), e4),
                          qf_sub(qf_mul_double(e3, 0.5), qf_mul_double(e5, 1.5)));
    qfloat_t minus = qf_add(qf_sub(qf_mul_double(em6, 4.0), qf_mul_double(em4, 3.0)),
                            qf_sub(qf_mul_double(em3, 2.5), qf_mul_double(em5, 3.5)));
    qfloat_t expected = qf_mul_double(qf_sub(pos, minus), 0.5);
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)sinh(x+2y+3) dA  [affine*sinh(affine)]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(sinh_affine);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_square_affine_times_sinh_affine(void) {
    /* (x + 2y + 3)^2 * sinh(x + 2y + 3) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *square = expr_pow_d(affine, 2.0);
    expr_t *sinh_affine = expr_sinh(affine);
    expr_t *expr = expr_mul(square, sinh_affine);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t e3 = qf_exp(qf_from_double(3.0));
    qfloat_t e4 = qf_exp(qf_from_double(4.0));
    qfloat_t e5 = qf_exp(qf_from_double(5.0));
    qfloat_t e6 = qf_exp(qf_from_double(6.0));
    qfloat_t em3 = qf_exp(qf_from_double(-3.0));
    qfloat_t em4 = qf_exp(qf_from_double(-4.0));
    qfloat_t em5 = qf_exp(qf_from_double(-5.0));
    qfloat_t em6 = qf_exp(qf_from_double(-6.0));
    qfloat_t pos = qf_mul_double(
        qf_add(qf_sub(qf_mul_double(e6, 18.0), qf_mul_double(e5, 11.0)),
               qf_sub(qf_mul_double(e3, 3.0), qf_mul_double(e4, 6.0))),
        0.5);
    qfloat_t neg = qf_mul_double(
        qf_add(qf_sub(qf_mul_double(em6, 66.0), qf_mul_double(em5, 51.0)),
               qf_sub(qf_mul_double(em3, 27.0), qf_mul_double(em4, 38.0))),
        0.5);
    qfloat_t expected = qf_mul_double(qf_sub(pos, neg), 0.5);
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^2sinh(x+2y+3) dA  [affine^2*sinh(affine)]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(sinh_affine);
    expr_free(square);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_affine_times_cosh_affine(void) {
    /* (x + 2y + 3) * cosh(x + 2y + 3) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *cosh_affine = expr_cosh(affine);
    expr_t *expr = expr_mul(cosh_affine, affine);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t e3 = qf_exp(qf_from_double(3.0));
    qfloat_t e4 = qf_exp(qf_from_double(4.0));
    qfloat_t e5 = qf_exp(qf_from_double(5.0));
    qfloat_t e6 = qf_exp(qf_from_double(6.0));
    qfloat_t em3 = qf_exp(qf_from_double(-3.0));
    qfloat_t em4 = qf_exp(qf_from_double(-4.0));
    qfloat_t em5 = qf_exp(qf_from_double(-5.0));
    qfloat_t em6 = qf_exp(qf_from_double(-6.0));
    qfloat_t pos = qf_add(qf_sub(qf_mul_double(e6, 2.0), e4),
                          qf_sub(qf_mul_double(e3, 0.5), qf_mul_double(e5, 1.5)));
    qfloat_t minus = qf_add(qf_sub(qf_mul_double(em6, 4.0), qf_mul_double(em4, 3.0)),
                            qf_sub(qf_mul_double(em3, 2.5), qf_mul_double(em5, 3.5)));
    qfloat_t expected = qf_mul_double(qf_add(pos, minus), 0.5);
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)cosh(x+2y+3) dA  [affine*cosh(affine)]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(cosh_affine);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_square_affine_times_cosh_affine(void) {
    /* (x + 2y + 3)^2 * cosh(x + 2y + 3) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *square = expr_mul(affine, affine);
    expr_t *cosh_affine = expr_cosh(affine);
    expr_t *expr = expr_mul(cosh_affine, square);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t e3 = qf_exp(qf_from_double(3.0));
    qfloat_t e4 = qf_exp(qf_from_double(4.0));
    qfloat_t e5 = qf_exp(qf_from_double(5.0));
    qfloat_t e6 = qf_exp(qf_from_double(6.0));
    qfloat_t em3 = qf_exp(qf_from_double(-3.0));
    qfloat_t em4 = qf_exp(qf_from_double(-4.0));
    qfloat_t em5 = qf_exp(qf_from_double(-5.0));
    qfloat_t em6 = qf_exp(qf_from_double(-6.0));
    qfloat_t pos = qf_mul_double(
        qf_add(qf_sub(qf_mul_double(e6, 18.0), qf_mul_double(e5, 11.0)),
               qf_sub(qf_mul_double(e3, 3.0), qf_mul_double(e4, 6.0))),
        0.5);
    qfloat_t neg = qf_mul_double(
        qf_add(qf_sub(qf_mul_double(em6, 66.0), qf_mul_double(em5, 51.0)),
               qf_sub(qf_mul_double(em3, 27.0), qf_mul_double(em4, 38.0))),
        0.5);
    qfloat_t expected = qf_mul_double(qf_add(pos, neg), 0.5);
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^2cosh(x+2y+3) dA  [affine^2*cosh(affine)]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(cosh_affine);
    expr_free(square);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_cube_affine_times_exp_affine(void) {
    /* (x + 2y + 3)^3 * exp(x + 2y + 3) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *cube = expr_pow_d(affine, 3.0);
    expr_t *exp_affine = expr_exp(affine);
    expr_t *expr = expr_mul(cube, exp_affine);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t e3 = qf_exp(qf_from_double(3.0));
    qfloat_t e4 = qf_exp(qf_from_double(4.0));
    qfloat_t e5 = qf_exp(qf_from_double(5.0));
    qfloat_t e6 = qf_exp(qf_from_double(6.0));
    qfloat_t expected = qf_mul_double(
        qf_add(qf_sub(qf_mul_double(e6, 84.0), qf_mul_double(e5, 41.0)),
               qf_sub(qf_mul_double(e3, 3.0), qf_mul_double(e4, 16.0))),
        0.5);
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^3exp(x+2y+3) dA  [affine^3*exp(affine)]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(exp_affine);
    expr_free(cube);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_cube_affine_times_sin_affine(void) {
    /* (x + 2y + 3)^3 * sin(x + 2y + 3) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *cube_square = expr_mul(affine, affine);
    expr_t *cube = expr_mul(cube_square, affine);
    expr_t *sin_affine = expr_sin(affine);
    expr_t *expr = expr_mul(cube, sin_affine);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t u3 = qf_from_double(3.0);
    qfloat_t u4 = qf_from_double(4.0);
    qfloat_t u5 = qf_from_double(5.0);
    qfloat_t u6 = qf_from_double(6.0);
    qfloat_t f3 = qf_add(qf_mul(qf_add(qf_neg(qf_mul(u3, qf_mul(u3, u3))),
                                       qf_mul_double(u3, 18.0)), qf_sin(u3)),
                         qf_mul(qf_add(qf_mul_double(qf_mul(u3, u3), -6.0),
                                       qf_from_double(24.0)), qf_cos(u3)));
    qfloat_t f4 = qf_add(qf_mul(qf_add(qf_neg(qf_mul(u4, qf_mul(u4, u4))),
                                       qf_mul_double(u4, 18.0)), qf_sin(u4)),
                         qf_mul(qf_add(qf_mul_double(qf_mul(u4, u4), -6.0),
                                       qf_from_double(24.0)), qf_cos(u4)));
    qfloat_t f5 = qf_add(qf_mul(qf_add(qf_neg(qf_mul(u5, qf_mul(u5, u5))),
                                       qf_mul_double(u5, 18.0)), qf_sin(u5)),
                         qf_mul(qf_add(qf_mul_double(qf_mul(u5, u5), -6.0),
                                       qf_from_double(24.0)), qf_cos(u5)));
    qfloat_t f6 = qf_add(qf_mul(qf_add(qf_neg(qf_mul(u6, qf_mul(u6, u6))),
                                       qf_mul_double(u6, 18.0)), qf_sin(u6)),
                         qf_mul(qf_add(qf_mul_double(qf_mul(u6, u6), -6.0),
                                       qf_from_double(24.0)), qf_cos(u6)));
    qfloat_t expected = qf_mul_double(qf_sub(qf_sub(f6, f4), qf_sub(f5, f3)), 0.5);
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^3sin(x+2y+3) dA  [affine^3*sin(affine)]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(sin_affine);
    expr_free(cube_square);
    expr_free(cube);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_cube_affine_times_cos_affine(void) {
    /* (x + 2y + 3)^3 * cos(x + 2y + 3) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *cube = expr_pow_d(affine, 3.0);
    expr_t *cos_affine = expr_cos(affine);
    expr_t *expr = expr_mul(cos_affine, cube);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t u3 = qf_from_double(3.0);
    qfloat_t u4 = qf_from_double(4.0);
    qfloat_t u5 = qf_from_double(5.0);
    qfloat_t u6 = qf_from_double(6.0);
    qfloat_t g3 = qf_add(qf_mul(qf_add(qf_neg(qf_mul(u3, qf_mul(u3, u3))),
                                       qf_mul_double(u3, 18.0)), qf_cos(u3)),
                         qf_mul(qf_add(qf_mul_double(qf_mul(u3, u3), 6.0),
                                       qf_from_double(-24.0)), qf_sin(u3)));
    qfloat_t g4 = qf_add(qf_mul(qf_add(qf_neg(qf_mul(u4, qf_mul(u4, u4))),
                                       qf_mul_double(u4, 18.0)), qf_cos(u4)),
                         qf_mul(qf_add(qf_mul_double(qf_mul(u4, u4), 6.0),
                                       qf_from_double(-24.0)), qf_sin(u4)));
    qfloat_t g5 = qf_add(qf_mul(qf_add(qf_neg(qf_mul(u5, qf_mul(u5, u5))),
                                       qf_mul_double(u5, 18.0)), qf_cos(u5)),
                         qf_mul(qf_add(qf_mul_double(qf_mul(u5, u5), 6.0),
                                       qf_from_double(-24.0)), qf_sin(u5)));
    qfloat_t g6 = qf_add(qf_mul(qf_add(qf_neg(qf_mul(u6, qf_mul(u6, u6))),
                                       qf_mul_double(u6, 18.0)), qf_cos(u6)),
                         qf_mul(qf_add(qf_mul_double(qf_mul(u6, u6), 6.0),
                                       qf_from_double(-24.0)), qf_sin(u6)));
    qfloat_t expected = qf_mul_double(qf_sub(qf_sub(g6, g4), qf_sub(g5, g3)), 0.5);
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^3cos(x+2y+3) dA  [affine^3*cos(affine)]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(cos_affine);
    expr_free(cube);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_cube_affine_times_sinh_affine(void) {
    /* (x + 2y + 3)^3 * sinh(x + 2y + 3) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *cube = expr_pow_d(affine, 3.0);
    expr_t *sinh_affine = expr_sinh(affine);
    expr_t *expr = expr_mul(cube, sinh_affine);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t e3 = qf_exp(qf_from_double(3.0));
    qfloat_t e4 = qf_exp(qf_from_double(4.0));
    qfloat_t e5 = qf_exp(qf_from_double(5.0));
    qfloat_t e6 = qf_exp(qf_from_double(6.0));
    qfloat_t em3 = qf_exp(qf_from_double(-3.0));
    qfloat_t em4 = qf_exp(qf_from_double(-4.0));
    qfloat_t em5 = qf_exp(qf_from_double(-5.0));
    qfloat_t em6 = qf_exp(qf_from_double(-6.0));
    qfloat_t pos = qf_mul_double(
        qf_add(qf_sub(qf_mul_double(e6, 84.0), qf_mul_double(e5, 41.0)),
               qf_sub(qf_mul_double(e3, 3.0), qf_mul_double(e4, 16.0))),
        0.5);
    qfloat_t neg = qf_mul_double(
        qf_add(qf_sub(qf_mul_double(em6, 564.0), qf_mul_double(em5, 389.0)),
               qf_sub(qf_mul_double(em3, 159.0), qf_mul_double(em4, 256.0))),
        0.5);
    qfloat_t expected = qf_mul_double(qf_sub(pos, neg), 0.5);
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^3sinh(x+2y+3) dA  [affine^3*sinh(affine)]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(sinh_affine);
    expr_free(cube);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_cube_affine_times_cosh_affine(void) {
    /* (x + 2y + 3)^3 * cosh(x + 2y + 3) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *cube_square = expr_mul(affine, affine);
    expr_t *cube = expr_mul(cube_square, affine);
    expr_t *cosh_affine = expr_cosh(affine);
    expr_t *expr = expr_mul(cosh_affine, cube);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t e3 = qf_exp(qf_from_double(3.0));
    qfloat_t e4 = qf_exp(qf_from_double(4.0));
    qfloat_t e5 = qf_exp(qf_from_double(5.0));
    qfloat_t e6 = qf_exp(qf_from_double(6.0));
    qfloat_t em3 = qf_exp(qf_from_double(-3.0));
    qfloat_t em4 = qf_exp(qf_from_double(-4.0));
    qfloat_t em5 = qf_exp(qf_from_double(-5.0));
    qfloat_t em6 = qf_exp(qf_from_double(-6.0));
    qfloat_t pos = qf_mul_double(
        qf_add(qf_sub(qf_mul_double(e6, 84.0), qf_mul_double(e5, 41.0)),
               qf_sub(qf_mul_double(e3, 3.0), qf_mul_double(e4, 16.0))),
        0.5);
    qfloat_t neg = qf_mul_double(
        qf_add(qf_sub(qf_mul_double(em6, 564.0), qf_mul_double(em5, 389.0)),
               qf_sub(qf_mul_double(em3, 159.0), qf_mul_double(em4, 256.0))),
        0.5);
    qfloat_t expected = qf_mul_double(qf_add(pos, neg), 0.5);
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^3cosh(x+2y+3) dA  [affine^3*cosh(affine)]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(cosh_affine);
    expr_free(cube_square);
    expr_free(cube);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_quartic_affine_times_exp_affine(void) {
    /* (x + 2y + 3)^4 * exp(x + 2y + 3) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *quartic = expr_pow_d(affine, 4.0);
    expr_t *exp_affine = expr_exp(affine);
    expr_t *expr = expr_mul(quartic, exp_affine);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t expected =
        qf_from_string("68737.53818332082704696161172519695");
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^4exp(x+2y+3) dA  [affine^4*exp(affine)]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(exp_affine);
    expr_free(quartic);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_quartic_affine_times_sin_affine(void) {
    /* (x + 2y + 3)^4 * sin(x + 2y + 3) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *quartic_lhs = expr_mul(affine, affine);
    expr_t *quartic_rhs = expr_mul(affine, affine);
    expr_t *quartic = expr_mul(quartic_lhs, quartic_rhs);
    expr_t *sin_affine = expr_sin(affine);
    expr_t *expr = expr_mul(quartic, sin_affine);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t expected =
        qf_from_string("-381.33814729825575506728041524097607853401008090198");
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^4sin(x+2y+3) dA  [affine^4*sin(affine)]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(sin_affine);
    expr_free(quartic_rhs);
    expr_free(quartic_lhs);
    expr_free(quartic);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_quartic_affine_times_cos_affine(void) {
    /* (x + 2y + 3)^4 * cos(x + 2y + 3) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *quartic = expr_pow_d(affine, 4.0);
    expr_t *cos_affine = expr_cos(affine);
    expr_t *expr = expr_mul(cos_affine, quartic);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t expected =
        qf_from_string("56.617810832398686377797715265898455798291870430519");
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^4cos(x+2y+3) dA  [affine^4*cos(affine)]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(cos_affine);
    expr_free(quartic);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_quartic_affine_times_sinh_affine(void) {
    /* (x + 2y + 3)^4 * sinh(x + 2y + 3) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *quartic = expr_pow_d(affine, 4.0);
    expr_t *sinh_affine = expr_sinh(affine);
    expr_t *expr = expr_mul(quartic, sinh_affine);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t expected =
        qf_from_string("34366.578859352871623151816024873924373424902707813");
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^4sinh(x+2y+3) dA  [affine^4*sinh(affine)]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(sinh_affine);
    expr_free(quartic);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_quartic_affine_times_cosh_affine(void) {
    /* (x + 2y + 3)^4 * cosh(x + 2y + 3) */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *quartic_lhs = expr_mul(affine, affine);
    expr_t *quartic_rhs = expr_mul(affine, affine);
    expr_t *quartic = expr_mul(quartic_lhs, quartic_rhs);
    expr_t *cosh_affine = expr_cosh(affine);
    expr_t *expr = expr_mul(cosh_affine, quartic);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t expected =
        qf_from_string("34370.95932396795542380979570032394");
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (x+2y+3)^4cosh(x+2y+3) dA  [affine^4*cosh(affine)]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(cosh_affine);
    expr_free(quartic_rhs);
    expr_free(quartic_lhs);
    expr_free(quartic);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_affine_poly_times_exp_affine_combination(void) {
    /* (3a^4 - 2a^2 + a) * exp(a), a = x + 2y + 3 */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *square = expr_pow_d(affine, 2.0);
    expr_t *quartic = expr_pow_d(affine, 4.0);
    expr_t *exp_affine = expr_exp(affine);
    expr_t *affine_term = expr_mul(affine, exp_affine);
    expr_t *square_term = expr_mul(square, exp_affine);
    expr_t *quartic_term = expr_mul(quartic, exp_affine);
    expr_t *scaled_quartic = expr_mul_d(quartic_term, 3.0);
    expr_t *scaled_square = expr_mul_d(square_term, -2.0);
    expr_t *sum = expr_add(scaled_quartic, scaled_square);
    expr_t *expr = expr_add(sum, affine_term);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t expected = qf_add(
        qf_add(qf_mul_double(qf_from_string("68737.53818332082704696161172519695"), 3.0),
               qf_mul_double(qf_from_string("2680.920621655793569036410945540741"), -2.0)),
        qf_from_string("539.6824667600549348774549946503721"));
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (3a^4-2a^2+a)exp(a) dA  [affine poly * exp(affine)]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(sum);
    expr_free(scaled_square);
    expr_free(scaled_quartic);
    expr_free(quartic_term);
    expr_free(square_term);
    expr_free(affine_term);
    expr_free(exp_affine);
    expr_free(quartic);
    expr_free(square);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

void test_multi_2d_affine_poly_times_sin_affine_combination(void) {
    /* (2a^4 + a^2 - 3a) * sin(a), a = x + 2y + 3 */
    integrator_t *ig = ig_new();
    expr_t *x = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *y = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *two_y = expr_mul_d(y, 2.0);
    expr_t *sum_xy = expr_add(x, two_y);
    expr_t *affine = expr_add_d(sum_xy, 3.0);
    expr_t *square = expr_pow_d(affine, 2.0);
    expr_t *quartic = expr_pow_d(affine, 4.0);
    expr_t *sin_affine = expr_sin(affine);
    expr_t *affine_term = expr_mul(affine, sin_affine);
    expr_t *square_term = expr_mul(square, sin_affine);
    expr_t *quartic_term = expr_mul(quartic, sin_affine);
    expr_t *scaled_quartic = expr_mul_d(quartic_term, 2.0);
    expr_t *scaled_affine = expr_mul_d(affine_term, -3.0);
    expr_t *sum = expr_add(scaled_quartic, square_term);
    expr_t *expr = expr_add(sum, scaled_affine);

    expr_t *vars[2] = { x, y };
    qfloat_t lo[2] = { qf_from_double(0.0), qf_from_double(0.0) };
    qfloat_t hi[2] = { qf_from_double(1.0), qf_from_double(1.0) };
    qfloat_t result, err;
    qfloat_t expected = qf_add(
        qf_add(qf_mul_double(qf_from_string("-381.3381472982557550672804152409705"), 2.0),
               qf_from_string("-16.88885619742372162769129239240872")),
        qf_mul_double(qf_from_string("-3.624508420217032103141223583565993"), -3.0));
    int s = ig_integral_multi(ig, expr, 2, vars, lo, hi, &result, &err);

    printf("  ∫∫ (2a^4+a^2-3a)sin(a) dA  [affine poly * sin(affine)]\n");
    qf_printf("  result   = %q\n", result);
    qf_printf("  expected = %q\n", expected);
    qf_printf("  err      = %q\n", err);
    printf("  status = %d  intervals = %zu\n", s, ig_get_interval_count_used(ig));
    ASSERT_TRUE(s == 0);
    TEST_ASSERT_INTEGRATOR_QFLOAT_CLOSE_TOL(result, expected, tol27);
    ASSERT_TRUE(ig_get_interval_count_used(ig) == 1);

    expr_free(expr);
    expr_free(sum);
    expr_free(scaled_affine);
    expr_free(scaled_quartic);
    expr_free(quartic_term);
    expr_free(square_term);
    expr_free(affine_term);
    expr_free(sin_affine);
    expr_free(quartic);
    expr_free(square);
    expr_free(affine);
    expr_free(sum_xy);
    expr_free(two_y);
    expr_free(y);
    expr_free(x);
    ig_free(ig);
}

/* -----------------------------------------------------------------------
 * README examples
 * --------------------------------------------------------------------- */

static qfloat_t fn_gaussian(qfloat_t x, void *ctx) {
    (void)ctx;
    return qf_exp(qf_neg(qf_sqr(x)));
}

static void example_integrator(void) {
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

static void example_ctx(void) {
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

static void example_integrator_expr(void) {
    /* ∫₋₃³ exp(-x²) dx using Turán T15/T4 with automatic differentiation.
     * Exact value: √π · erf(3).  Compare interval count with example_integrator(). */
    integrator_t *ig = ig_new();

    expr_t *x    = test_expr_new_var_qf(qf_from_double(0.0));
    expr_t *x2   = expr_mul(x, x);
    expr_t *negx2 = expr_neg(x2);
    expr_t *expr = expr_exp(negx2);

    qfloat_t result, err;
    ig_single_integral(ig, expr, x,
                       qf_from_double(-3.0), qf_from_double(3.0),
                       &result, &err);

    qf_printf("∫₋₃³ exp(-x²) dx ≈ %q\n", result);
    qf_printf("  error estimate   ≈ %q\n", err);
    printf("  subintervals used: %zu\n", ig_get_interval_count_used(ig));

    expr_free(expr);
    expr_free(negx2);
    expr_free(x2);
    expr_free(x);
    ig_free(ig);
}

/* -----------------------------------------------------------------------
 * Entry point
 * --------------------------------------------------------------------- */

int tests_main(void) {
    TEST_SECTION("Lifecycle Tests");
    TEST_RUN_CASE(test_create_and_destroy, NULL);
    TEST_RUN_CASE(test_null_safety, NULL);

    TEST_SECTION("Integral Value Tests");
    TEST_RUN_CASE(test_polynomial, NULL);
    TEST_RUN_CASE(test_sin, NULL);
    TEST_RUN_CASE(test_exp, NULL);
    TEST_RUN_CASE(test_arctan, NULL);
    TEST_RUN_CASE(test_log, NULL);
    TEST_RUN_CASE(test_constant, NULL);
    TEST_RUN_CASE(test_linear, NULL);
    TEST_RUN_CASE(test_reversed_limits, NULL);

    TEST_SECTION("Configuration Tests");
    TEST_RUN_CASE(test_set_tol, NULL);
    TEST_RUN_CASE(test_max_intervals, NULL);
    TEST_RUN_CASE(test_last_intervals, NULL);

    TEST_SECTION("Turan T15/T4 expr_t Tests");
    TEST_RUN_CASE(test_expr_sin, NULL);
    TEST_RUN_CASE(test_expr_exp, NULL);
    TEST_RUN_CASE(test_expr_arctan, NULL);
    TEST_RUN_CASE(test_expr_null_safety, NULL);

    TEST_SECTION("ig_double_integral Tests");
    TEST_RUN_CASE(test_double_polynomial, NULL);
    TEST_RUN_CASE(test_double_exp, NULL);
    TEST_RUN_CASE(test_double_nonunit_bounds, NULL);
    TEST_RUN_CASE(test_double_null_safety, NULL);

    TEST_SECTION("ig_triple_integral Tests");
    TEST_RUN_CASE(test_triple_polynomial, NULL);
    TEST_RUN_CASE(test_triple_exp, NULL);
    TEST_RUN_CASE(test_triple_null_safety, NULL);

    TEST_SECTION("N-dimensional Turan T15/T4 Tests");
    TEST_RUN_CASE(test_multi_2d, NULL);
    TEST_RUN_CASE(test_multi_3d, NULL);
    TEST_RUN_CASE(test_multi_null_safety, NULL);
    TEST_RUN_CASE(test_multi_nd1, NULL);
    TEST_RUN_CASE(test_multi_4d, NULL);
    TEST_RUN_CASE(test_multi_4d_exp, NULL);
    TEST_RUN_CASE(test_multi_4d_exp_affine, NULL);
    TEST_RUN_CASE(test_multi_3d_sinh_affine, NULL);
    TEST_RUN_CASE(test_multi_3d_cosh_affine, NULL);
    TEST_RUN_CASE(test_multi_3d_sin_affine, NULL);
    TEST_RUN_CASE(test_multi_3d_cos_affine, NULL);
    TEST_RUN_CASE(test_multi_3d_scaled_sum_specials, NULL);
    TEST_RUN_CASE(test_multi_2d_sum_of_specials, NULL);
    TEST_RUN_CASE(test_multi_3d_separable_product, NULL);
    TEST_RUN_CASE(test_multi_3d_regrouped_separable_product, NULL);
    TEST_RUN_CASE(test_multi_2d_sum_of_separable_products, NULL);
    TEST_RUN_CASE(test_multi_2d_affine_square, NULL);
    TEST_RUN_CASE(test_multi_2d_affine_cube, NULL);
    TEST_RUN_CASE(test_multi_2d_affine_quartic, NULL);
    TEST_RUN_CASE(test_multi_2d_affine_poly_deg4, NULL);
    TEST_RUN_CASE(test_multi_2d_affine_times_exp_affine, NULL);
    TEST_RUN_CASE(test_multi_2d_square_affine_times_exp_affine, NULL);
    TEST_RUN_CASE(test_multi_2d_cube_affine_times_exp_affine, NULL);
    TEST_RUN_CASE(test_multi_2d_quartic_affine_times_exp_affine, NULL);
    TEST_RUN_CASE(test_multi_2d_affine_poly_times_exp_affine_combination, NULL);
    TEST_RUN_CASE(test_multi_2d_affine_times_sin_affine, NULL);
    TEST_RUN_CASE(test_multi_2d_square_affine_times_sin_affine, NULL);
    TEST_RUN_CASE(test_multi_2d_cube_affine_times_sin_affine, NULL);
    TEST_RUN_CASE(test_multi_2d_quartic_affine_times_sin_affine, NULL);
    TEST_RUN_CASE(test_multi_2d_affine_poly_times_sin_affine_combination, NULL);
    TEST_RUN_CASE(test_multi_2d_affine_times_cos_affine, NULL);
    TEST_RUN_CASE(test_multi_2d_square_affine_times_cos_affine, NULL);
    TEST_RUN_CASE(test_multi_2d_cube_affine_times_cos_affine, NULL);
    TEST_RUN_CASE(test_multi_2d_quartic_affine_times_cos_affine, NULL);
    TEST_RUN_CASE(test_multi_2d_affine_times_sinh_affine, NULL);
    TEST_RUN_CASE(test_multi_2d_square_affine_times_sinh_affine, NULL);
    TEST_RUN_CASE(test_multi_2d_cube_affine_times_sinh_affine, NULL);
    TEST_RUN_CASE(test_multi_2d_quartic_affine_times_sinh_affine, NULL);
    TEST_RUN_CASE(test_multi_2d_affine_times_cosh_affine, NULL);
    TEST_RUN_CASE(test_multi_2d_square_affine_times_cosh_affine, NULL);
    TEST_RUN_CASE(test_multi_2d_cube_affine_times_cosh_affine, NULL);
    TEST_RUN_CASE(test_multi_2d_quartic_affine_times_cosh_affine, NULL);

    TEST_SECTION("README Output Examples");
    printf(C_BOLD C_YELLOW "Running README examples...\n" C_RESET);
    TEST_RUN_OUTPUT_TAGS(example_integrator, "integrator,readme,output");
    TEST_RUN_OUTPUT_TAGS(example_ctx, "integrator,readme,output");
    TEST_RUN_OUTPUT_TAGS(example_integrator_expr, "integrator,readme,output");

    return TESTS_EXIT_CODE();
}
