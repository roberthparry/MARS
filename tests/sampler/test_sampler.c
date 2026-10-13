/**
 * @file test_sampler.c
 * @brief Public sampler regression coverage and executable README examples.
 *
 * Verifies exact coordinates, callback ownership, adaptive gaps, resource limits,
 * cancellation and independent expression bindings. Examples execute last.
 */
#include <errno.h>
#include <stdio.h>

#include "computation-t.h"
#include "sampler-t.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static sampler_status_t square(number_t t, number_t *x, number_t *y, void *argument)
{
    (void)argument;
    *x = num_clone(t);
    *y = num_mul(t, t);
    return SAMPLER_VALUE;
}

static void test_sampler_uniform(void)
{
    sampler_options_t options = sampler_options_default();
    options.method = SAMPLER_UNIFORM;
    options.intervals = 4;
    sampler_t *sampler = sampler_new(square, NULL, NUM_ZERO, NUM_ONE, &options);
    TEST_ASSERT_NOT_NULL(sampler);
    bool ok = sampler_count(sampler) == 5 && sampler_evaluations(sampler) == 5;
    number_t expected = num_create_from_frac(1, 16);
    ok = ok && num_eq(sampler_point(sampler, 1)->y, expected) && sampler_point(sampler, 0)->break_before &&
         !sampler_point(sampler, 1)->break_before && num_eq(sampler_point(sampler, 4)->parameter, NUM_ONE);
    num_destroy(&expected);
    sampler_free(sampler);
    TEST_ASSERT_TRUE(ok, "uniform exact fractions and endpoint inclusion");
}

static void test_sampler_adaptive(void)
{
    sampler_options_t options = sampler_options_default();
    options.intervals = 1;
    sampler_t *sampler = sampler_new(square, NULL, NUM_ZERO, NUM_ONE, &options);
    TEST_ASSERT_NOT_NULL(sampler);
    bool ok = sampler_count(sampler) > 3 && sampler_evaluations(sampler) <= options.max_evaluations;
    for (size_t i = 1; i < sampler_count(sampler); ++i)
        ok = ok && num_lt(sampler_point(sampler, i - 1)->parameter, sampler_point(sampler, i)->parameter);
    sampler_free(sampler);
    options.max_depth = 0;
    sampler = sampler_new(square, NULL, NUM_ZERO, NUM_ONE, &options);
    ok = ok && sampler && sampler_count(sampler) == 3 && sampler_point(sampler, 1)->break_before &&
         sampler_point(sampler, 2)->break_before;
    sampler_free(sampler);
    TEST_ASSERT_TRUE(ok, "adaptive refinement is ordered and unresolved edges are disconnected");
}

static sampler_status_t reciprocal(number_t t, number_t *x, number_t *y, void *argument)
{
    (void)argument;
    *x = num_clone(t);
    *y = num_is_zero(t) ? num_clone(NUM_NAN) : num_div(NUM_ONE, t);
    return SAMPLER_VALUE;
}

static void test_sampler_gaps(void)
{
    sampler_options_t options = sampler_options_default();
    options.intervals = 2;
    options.max_depth = 5;
    number_t low = num_create_from_long(-1);
    sampler_t *sampler = sampler_new(reciprocal, NULL, low, NUM_ONE, &options);
    TEST_ASSERT_NOT_NULL(sampler);
    bool gap = false, ok = true;
    for (size_t i = 0; i < sampler_count(sampler); ++i) {
        const sampler_point_t *point = sampler_point(sampler, i);
        if (!point->valid) {
            gap = true;
            ok = ok && point->break_before;
            if (i + 1 < sampler_count(sampler))
                ok = ok && sampler_point(sampler, i + 1)->break_before;
        }
    }
    sampler_free(sampler);
    num_destroy(&low);
    TEST_ASSERT_TRUE(ok && gap, "no segment through an evaluated pole");
}

static void test_sampler_unsampled_poles(void)
{
    size_t precision = num_get_default_prec_bits();
    num_set_default_prec_bits(128);
    const char *const sources[] = {"1/(x-1/3)", "zeta(x)", "tan(x)"};
    number_t pi = num_const(NUM_PI), pole[] = {num_create_from_frac(1, 3), NUM_ONE, num_mul(pi, NUM_HALF)};
    number_t lo = num_create_from_long(-10), hi = num_create_from_long(10);
    bool ok = true;
    for (size_t i = 0; i < 3; ++i) {
        expr_bindings_t *bindings = NULL;
        expr_t *expression = expr_from_string(sources[i], &bindings);
        sampler_options_t options = sampler_options_default();
        options.intervals = 64;
        options.max_depth = 8;
        options.relative_tolerance = 0.002;
        options.absolute_tolerance = 1e-5;
        sampler_t *samples = sampler_expression(expression, expr_bindings_get(bindings, "x"), NULL, lo, hi, &options);
        bool crossed = false;
        for (size_t j = 1; j < sampler_count(samples); ++j) {
            const sampler_point_t *a = sampler_point(samples, j - 1), *b = sampler_point(samples, j);
            if (num_lt(a->parameter, pole[i]) && num_gt(b->parameter, pole[i])) {
                crossed = true;
                ok = ok && (!a->valid || !b->valid || b->break_before);
            }
        }
        ok = ok && samples && crossed;
        sampler_free(samples);
        expr_free(expression);
        expr_bindings_free(bindings);
    }
    num_destroy(&hi);
    num_destroy(&lo);
    for (size_t i = 0; i < 3; ++i)
        num_destroy(&pole[i]);
    num_destroy(&pi);
    num_set_default_prec_bits(precision);
    TEST_ASSERT_TRUE(ok, "rational, zeta and tangent poles between grid points never become connecting segments");
}

static sampler_status_t fail_callback(number_t t, number_t *x, number_t *y, void *argument)
{
    (void)argument;
    *x = num_clone(t);
    *y = num_clone(t);
    errno = EACCES;
    return SAMPLER_ERROR;
}

static void test_sampler_failures(void)
{
    sampler_options_t options = sampler_options_default();
    options.intervals = 1;
    options.max_evaluations = 2;
    TEST_ASSERT_TRUE(!sampler_new(square, NULL, NUM_ZERO, NUM_ONE, &options) && errno == E2BIG, "evaluation cap");
    TEST_ASSERT_TRUE(!sampler_new(fail_callback, NULL, NUM_ZERO, NUM_ONE, NULL) && errno == EACCES, "callback errno");
    TEST_ASSERT_TRUE(!sampler_new(NULL, NULL, NUM_ZERO, NUM_ONE, NULL) && errno == EINVAL, "callback required");
    TEST_ASSERT_TRUE(!sampler_new(square, NULL, NUM_ONE, NUM_ZERO, NULL) && errno == EINVAL, "ordered range");
    options.max_depth = 25;
    TEST_ASSERT_TRUE(!sampler_new(square, NULL, NUM_ZERO, NUM_ONE, &options) && errno == EINVAL, "depth bounded");
    TEST_ASSERT_TRUE(!sampler_point(NULL, 0) && errno == EINVAL, "NULL point");
    sampler_free(NULL);
}

static bool sample_under_control(void *argument)
{
    sampler_t **result = argument;
    *result = sampler_new(square, NULL, NUM_ZERO, NUM_ONE, NULL);
    return *result != NULL;
}

static void test_sampler_control(void)
{
    computation_options_t options = {.work_limit = 2};
    computation_t *control = computation_new(&options);
    sampler_t *result = NULL;
    bool ok = control && !computation_run(control, sample_under_control, &result) && !result &&
              computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED;
    computation_free(control);
    sampler_free(result);
    TEST_ASSERT_TRUE(ok, "cooperative limit discards partial samples");
}

static void test_sampler_expression(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{x^2+a | x=7; a=3}", &bindings);
    TEST_ASSERT_NOT_NULL(expr);
    expr_t *x = expr_bindings_get(bindings, "x");
    sampler_options_t options = sampler_options_default();
    options.method = SAMPLER_UNIFORM;
    options.intervals = 2;
    sampler_t *samples = sampler_expression(expr, x, NULL, NUM_ZERO, NUM_ONE, &options);
    number_t before = expr_eval(x), three = num_create_from_long(3), four = num_create_from_long(4);
    number_t seven = num_create_from_long(7);
    bool ok = samples && num_eq(sampler_point(samples, 0)->y, three) &&
              num_eq(sampler_point(samples, 2)->y, four) && num_eq(before, seven);
    num_destroy(&before);
    num_destroy(&seven);
    num_destroy(&four);
    num_destroy(&three);
    sampler_free(samples);
    expr_free(expr);
    expr_bindings_free(bindings);
    TEST_ASSERT_TRUE(ok, "expression samples use private variable bindings and preserve parameters");
}

static void test_sampler_precision(void)
{
    number_t low = num_create_from_string("1000000000000000000000000000000");
    number_t high = num_add(low, NUM_ONE);
    sampler_options_t options = sampler_options_default();
    options.method = SAMPLER_UNIFORM;
    options.intervals = 4;
    sampler_t *samples = sampler_new(square, NULL, low, high, &options);
    TEST_ASSERT_NOT_NULL(samples);
    bool ok = true;
    for (size_t i = 1; i < sampler_count(samples); ++i)
        ok = ok && num_gt(sampler_point(samples, i)->x, sampler_point(samples, i - 1)->x);
    sampler_free(samples);
    num_destroy(&high);
    num_destroy(&low);
    TEST_ASSERT_TRUE(ok, "large offsets do not collapse to double coordinates");
}

/* README example: sample a formula without changing its original variable. */
static void example_sampler_expression(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expression = expr_from_string("x^2", &bindings);
    sampler_options_t options = sampler_options_default();
    options.method = SAMPLER_UNIFORM;
    options.intervals = 2;
    sampler_t *samples = sampler_expression(expression, expr_bindings_get(bindings, "x"), NULL,
                                             NUM_ZERO, NUM_ONE, &options);
    if (samples) {
        printf("Points: %zu\n", sampler_count(samples));
        num_printf("Last point: (%n, %n)\n", sampler_point(samples, 2)->x, sampler_point(samples, 2)->y);
    }
    bool ok = samples != NULL;
    sampler_free(samples);
    expr_free(expression);
    expr_bindings_free(bindings);
    TEST_ASSERT_TRUE(ok, "README expression samples");
}

static sampler_status_t parametric_curve(number_t t, number_t *x, number_t *y, void *argument)
{
    (void)argument;
    *x = num_mul(t, t);
    *y = num_mul(*x, t);
    return SAMPLER_VALUE;
}

/* README example: the sampler owns both callback outputs, including the parameter-independent x. */
static void example_sampler_callback(void)
{
    sampler_options_t options = sampler_options_default();
    options.method = SAMPLER_UNIFORM;
    options.intervals = 2;
    number_t lower = num_create_from_long(-1);
    sampler_t *samples = sampler_new(parametric_curve, NULL, lower, NUM_ONE, &options);
    if (samples) {
        const sampler_point_t *first = sampler_point(samples, 0);
        const sampler_point_t *last = sampler_point(samples, 2);
        num_printf("Curve endpoints: (%n, %n) to (%n, %n)\n", first->x, first->y, last->x, last->y);
    }
    bool ok = samples != NULL;
    sampler_free(samples);
    num_destroy(&lower);
    TEST_ASSERT_TRUE(ok, "README parametric callback");
}

int tests_main(void)
{
    TEST_RUN_CASE(test_sampler_uniform, NULL);
    TEST_RUN_CASE(test_sampler_adaptive, NULL);
    TEST_RUN_CASE(test_sampler_gaps, NULL);
    TEST_RUN_CASE(test_sampler_unsampled_poles, NULL);
    TEST_RUN_CASE(test_sampler_failures, NULL);
    TEST_RUN_CASE(test_sampler_control, NULL);
    TEST_RUN_CASE(test_sampler_expression, NULL);
    TEST_RUN_CASE(test_sampler_precision, NULL);
    TEST_SECTION("README examples");
    TEST_RUN_OUTPUT(example_sampler_expression);
    TEST_RUN_OUTPUT(example_sampler_callback);
    return TEST_EXIT_CODE();
}
