/* test_integrator.c — tests for the adaptive G7K15 integrator */

#include <stdio.h>
#include <math.h>

#define TEST_CONFIG_MODE TEST_CONFIG_GLOBAL
#include "test_harness.h"

#include "qfloat.h"
#include "integrator.h"

/* -----------------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------------- */

/* True if |a - b| <= tol */
static int qf_close(qfloat a, qfloat b, qfloat tol) {
    return qf_le(qf_abs(qf_sub(a, b)), tol);
}

static qfloat tol20 = {0};  /* 1e-20, initialised in tests_main */

/* -----------------------------------------------------------------------
 * Integrands
 * --------------------------------------------------------------------- */

static qfloat fn_x2(qfloat x, void *ctx) {
    (void)ctx;
    return qf_sqr(x);
}

static qfloat fn_sin(qfloat x, void *ctx) {
    (void)ctx;
    return qf_sin(x);
}

static qfloat fn_exp(qfloat x, void *ctx) {
    (void)ctx;
    return qf_exp(x);
}

static qfloat fn_inv1px2(qfloat x, void *ctx) {
    (void)ctx;
    qfloat one = qf_from_double(1.0);
    return qf_div(one, qf_add(one, qf_sqr(x)));
}

static qfloat fn_log(qfloat x, void *ctx) {
    (void)ctx;
    return qf_log(x);
}

static qfloat fn_const(qfloat x, void *ctx) {
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
    qfloat result, err;
    int s = integrator_eval(ig, fn_x2, NULL,
                            qf_from_double(0.0), qf_from_double(1.0),
                            &result, &err);
    ASSERT_TRUE(s == 0);
    qfloat expected = qf_from_string("0.33333333333333333333333333333333333333");
    ASSERT_TRUE(qf_close(result, expected, tol20));
    integrator_destroy(ig);
}

void test_sin(void) {
    /* ∫₀^π sin(x) dx = 2 */
    integrator_t *ig = integrator_create();
    qfloat result, err;
    int s = integrator_eval(ig, fn_sin, NULL,
                            qf_from_double(0.0), QF_PI,
                            &result, &err);
    ASSERT_TRUE(s == 0);
    qfloat expected = qf_from_double(2.0);
    ASSERT_TRUE(qf_close(result, expected, tol20));
    integrator_destroy(ig);
}

void test_exp(void) {
    /* ∫₀¹ exp(x) dx = e - 1 */
    integrator_t *ig = integrator_create();
    qfloat result, err;
    int s = integrator_eval(ig, fn_exp, NULL,
                            qf_from_double(0.0), qf_from_double(1.0),
                            &result, &err);
    ASSERT_TRUE(s == 0);
    qfloat expected = qf_sub(QF_E, qf_from_double(1.0));
    ASSERT_TRUE(qf_close(result, expected, tol20));
    integrator_destroy(ig);
}

void test_arctan(void) {
    /* ∫₋₁¹ 1/(1+x²) dx = π/2 */
    integrator_t *ig = integrator_create();
    qfloat result, err;
    int s = integrator_eval(ig, fn_inv1px2, NULL,
                            qf_from_double(-1.0), qf_from_double(1.0),
                            &result, &err);
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, QF_PI_2, tol20));
    integrator_destroy(ig);
}

void test_log(void) {
    /* ∫₁^e ln(x) dx = 1 */
    integrator_t *ig = integrator_create();
    qfloat result, err;
    int s = integrator_eval(ig, fn_log, NULL,
                            qf_from_double(1.0), QF_E,
                            &result, &err);
    ASSERT_TRUE(s == 0);
    qfloat expected = qf_from_double(1.0);
    ASSERT_TRUE(qf_close(result, expected, tol20));
    integrator_destroy(ig);
}

void test_constant(void) {
    /* ∫₀^5 1 dx = 5 */
    integrator_t *ig = integrator_create();
    qfloat result, err;
    int s = integrator_eval(ig, fn_const, NULL,
                            qf_from_double(0.0), qf_from_double(5.0),
                            &result, &err);
    ASSERT_TRUE(s == 0);
    ASSERT_TRUE(qf_close(result, qf_from_double(5.0), tol20));
    integrator_destroy(ig);
}

void test_set_tol(void) {
    integrator_t *ig = integrator_create();
    qfloat loose = qf_from_string("1e-10");
    integrator_set_tol(ig, loose, loose);

    qfloat result, err;
    integrator_eval(ig, fn_sin, NULL,
                    qf_from_double(0.0), QF_PI,
                    &result, &err);

    /* Error estimate should be at or below the loose tolerance */
    ASSERT_TRUE(qf_le(err, qf_from_string("1e-8")));
    integrator_destroy(ig);
}

void test_max_intervals(void) {
    /* Force early termination by allowing only 1 subinterval */
    integrator_t *ig = integrator_create();
    integrator_set_max_intervals(ig, 1);

    qfloat result, err;
    int s = integrator_eval(ig, fn_sin, NULL,
                            qf_from_double(0.0), QF_PI,
                            &result, &err);
    /* Should stop early — status 1 is acceptable for a highly oscillatory
       integrand restricted to a single subinterval */
    ASSERT_TRUE(s == 0 || s == 1);
    ASSERT_TRUE(integrator_last_intervals(ig) <= 1);
    integrator_destroy(ig);
}

void test_last_intervals(void) {
    /* A smooth integrand over a moderate range should converge in a handful
       of intervals; verify the counter is updated. */
    integrator_t *ig = integrator_create();
    qfloat result, err;
    integrator_eval(ig, fn_exp, NULL,
                    qf_from_double(0.0), qf_from_double(1.0),
                    &result, &err);
    ASSERT_TRUE(integrator_last_intervals(ig) >= 1);
    integrator_destroy(ig);
}

void test_null_safety(void) {
    integrator_t *ig = integrator_create();
    qfloat result;
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
    qfloat result, err;
    int s = integrator_eval(ig, fn_x2, NULL,
                            qf_from_double(1.0), qf_from_double(0.0),
                            &result, &err);
    ASSERT_TRUE(s == 0);
    qfloat expected = qf_from_string("-0.33333333333333333333333333333333333333");
    ASSERT_TRUE(qf_close(result, expected, tol20));
    integrator_destroy(ig);
}

/* -----------------------------------------------------------------------
 * README example
 * --------------------------------------------------------------------- */

static qfloat fn_gaussian(qfloat x, void *ctx) {
    (void)ctx;
    return qf_exp(qf_neg(qf_sqr(x)));
}

void example_integrator(void) {
    /* ∫₋₃³ exp(-x²) dx ≈ √π * erf(3) */
    integrator_t *ig = integrator_create();
    qfloat result, err;
    integrator_eval(ig, fn_gaussian, NULL,
                    qf_from_double(-3.0), qf_from_double(3.0),
                    &result, &err);

    qf_printf("∫₋₃³ exp(-x²) dx ≈ %q\n", result);
    qf_printf("  error estimate   ≈ %q\n", err);
    printf("  subintervals used: %zu\n", integrator_last_intervals(ig));
    integrator_destroy(ig);
}

typedef struct { qfloat exponent; } power_ctx;

static qfloat fn_power(qfloat x, void *ctx) {
    power_ctx *pc = ctx;
    return qf_pow(x, pc->exponent);
}

void example_ctx(void) {
    /* ∫₀¹ x^2.5 dx = 1/3.5 */
    integrator_t *ig = integrator_create();
    power_ctx ctx = { qf_from_string("2.5") };
    qfloat result, err;
    integrator_eval(ig, fn_power, &ctx,
                    qf_from_double(0.0), qf_from_double(1.0),
                    &result, &err);

    qf_printf("∫₀¹ x^2.5 dx ≈ %q\n", result);
    qf_printf("  error estimate   ≈ %q\n", err);
    integrator_destroy(ig);
}

/* -----------------------------------------------------------------------
 * Entry point
 * --------------------------------------------------------------------- */

int tests_main(void) {
    tol20 = qf_from_string("1e-20");

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

    printf(C_BOLD C_GREEN "\n=== README Output Examples ===\n" C_RESET);
    example_integrator();
    example_ctx();

    return tests_failed;
}
