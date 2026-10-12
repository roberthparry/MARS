/**
 * @file test_computation_expression.c
 * @brief Scoped expression stopping, cache recovery and inherited-control regressions.
 *
 * Exercises interrupted operations at several checkpoint positions and reuses
 * their graphs afterwards. These tests run sequentially with the control suite.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "computation-t.h"
#include "integrator-t.h"
#include "thread-t.h"
#include "test_harness.h"
#include "test_computation_support.h"

typedef struct {
    expr_t *formula;
    expr_t *x;
    integrator_t *integrator;
    computation_t *control;
    computation_t *other;
    unsigned polls;
    bool cancel;
} scoped_test_t;

static bool scoped_eval(void *argument)
{
    scoped_test_t *test = argument;
    number_t value = expr_eval(test->formula);
    num_destroy(&value);
    return true;
}

static bool scoped_derivative(void *argument)
{
    scoped_test_t *test = argument;
    expr_t *result = expr_create_deriv(test->formula, test->x);
    expr_free(result);
    return true;
}

static bool scoped_integrate(void *argument)
{
    scoped_test_t *test = argument;
    expr_t *result = expr_integrate(test->formula, test->x);
    expr_free(result);
    return true;
}

static bool scoped_simplify(void *argument)
{
    scoped_test_t *test = argument;
    expr_t *result = expr_simplify(test->formula);
    expr_free(result);
    return true;
}

static bool scoped_beautify(void *argument)
{
    scoped_test_t *test = argument;
    expr_t *result = expr_beautify(test->formula);
    expr_free(result);
    return true;
}

static bool scoped_reverse(void *argument)
{
    scoped_test_t *test = argument;
    const expr_t *vars[] = {test->x};
    number_t value = NUM_NAN, derivative = NUM_NAN;
    expr_eval_derivatives(test->formula, 1, vars, &value, &derivative);
    num_destroy(&value);
    num_destroy(&derivative);
    return true;
}

static void test_computation_expression_recovery(void)
{
    static const computation_callback_t operations[] = {
        scoped_eval, scoped_derivative, scoped_integrate, scoped_simplify, scoped_beautify, scoped_reverse
    };
    for (size_t operation = 0; operation < sizeof(operations) / sizeof(*operations); ++operation) {
        for (uint64_t budget = 1; budget <= 32; ++budget) {
            expr_bindings_t *bindings = NULL;
            scoped_test_t test = {.formula = expr_from_string("x*x*x + 2*x*x + 3*x + 4", &bindings)};
            test.x = expr_bindings_get(bindings, "x");
            if (!test.formula || !test.x)
                abort();
            expr_set_val(test.x, NUM_TWO);
            computation_options_t options = {.work_limit = budget};
            computation_t *control = computation_new(&options);
            bool completed = computation_run(control, operations[operation], &test);
            TEST_ASSERT_TRUE(completed || computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED,
                             "interrupted operation has an explicit stopping reason");
            TEST_ASSERT_TRUE(!computation_current(), "scope restored after operation");
            number_t value = expr_eval(test.formula), expected = num_create_from_long(26);
            TEST_ASSERT_TRUE(num_eq(value, expected), "cancelled evaluation does not poison primal cache");
            expr_t *derivative = expr_create_deriv(test.formula, test.x);
            number_t slope = expr_eval(derivative), expected_slope = num_create_from_long(23);
            TEST_ASSERT_TRUE(num_eq(slope, expected_slope), "cancelled operation does not poison derivative cache");
            num_destroy(&value);
            num_destroy(&expected);
            num_destroy(&slope);
            num_destroy(&expected_slope);
            expr_free(derivative);
            expr_free(test.formula);
            expr_bindings_free(bindings);
            computation_free(control);
        }
    }
}

static bool scoped_noop(void *argument)
{
    return computation_current() == argument;
}

static void test_computation_sum_product_recovery(void)
{
    expr_t *index = expr_new_var(NUM_ONE), *square = expr_mul(index, index), *term = expr_sin(square);
    number_t endpoint = num_create_from_long(100);
    expr_t *lower = expr_new_const(NUM_ONE), *upper = expr_new_const(endpoint);
    expr_t *expressions[] = {expr_new_finite_summation_range(term, index, lower, upper),
                            expr_new_finite_product_range(term, index, lower, upper)};
    for (size_t i = 0; i < 2; ++i) {
        scoped_test_t test = {.formula = expressions[i]};
        computation_options_t options = {.work_limit = 40};
        computation_t *control = computation_new(&options);
        if (!test.formula || !control)
            abort();
        TEST_ASSERT_TRUE(!computation_run(control, scoped_eval, &test), "sum/product loop stops within its budget");
        TEST_ASSERT_TRUE(computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED, "loop work is charged");
        number_t value = expr_eval(test.formula);
        TEST_ASSERT_TRUE(num_is_finite(value), "sum/product cache remains reusable");
        num_destroy(&value);
        computation_free(control);
        expr_free(expressions[i]);
    }
    expr_free(upper);
    expr_free(lower);
    expr_free(term);
    expr_free(square);
    expr_free(index);
    num_destroy(&endpoint);
}

static bool scoped_nested(void *argument)
{
    scoped_test_t *test = argument;
    bool same = computation_run(test->control, scoped_noop, test->control);
    bool blocked = !computation_run(test->other, scoped_noop, test->other) && errno == EBUSY;
    return same && blocked && computation_current() == test->control;
}

static void *scoped_thread(void *argument)
{
    bool isolated = !computation_current() && computation_run(argument, scoped_noop, argument) && !computation_current();
    return isolated ? argument : NULL;
}

static bool scoped_thread_owner(void *argument)
{
    thread_t *thread = thread_new(scoped_thread, argument);
    void *result = NULL;
    bool ok = thread && thread_join(thread, &result);
    bool released = thread_free(thread);
    return ok && released && result == argument && computation_current() == argument;
}

static bool scoped_cancel(void *argument)
{
    scoped_test_t *test = argument;
    ++test->polls;
    return test->cancel;
}

static void test_computation_scopes(void)
{
    scoped_test_t test = {.other = computation_new(NULL)};
    computation_options_t options = {.cancelled = scoped_cancel, .argument = &test};
    test.control = computation_new(&options);
    TEST_ASSERT_TRUE(computation_run(test.control, scoped_nested, &test), "nested scope succeeds");
    TEST_ASSERT_TRUE(!computation_current() && test.polls > 0, "poll callback runs in ordinary context");
    TEST_ASSERT_TRUE(computation_run(test.other, scoped_thread_owner, test.other), "thread scope explicitly entered and restored");
    test.cancel = true;
    TEST_ASSERT_TRUE(!computation_run(test.control, scoped_noop, test.control) && errno == ECANCELED,
                     "external flag stops subsequent work");
    TEST_ASSERT_TRUE(computation_status(test.control) == COMPUTATION_CANCELLED, "external cancellation sticky");
    computation_free(test.control);
    computation_free(test.other);
}

static bool scoped_quadrature(void *argument)
{
    scoped_test_t *test = argument;
    number_t value = NUM_NAN;
    int status = intg_integral(test->integrator, test->formula, test->x, NUM_ZERO, NUM_ONE, &value, NULL);
    bool stopped = status == -2 && num_is_nan(value);
    num_destroy(&value);
    return stopped;
}

static void test_computation_inherited_integrator(void)
{
    expr_bindings_t *bindings = NULL;
    scoped_test_t test = {.formula = expr_from_string("sin(x*x)", &bindings), .integrator = intg_new()};
    test.x = expr_bindings_get(bindings, "x");
    computation_options_t options = {.work_limit = 2};
    computation_t *control = computation_new(&options);
    TEST_ASSERT_TRUE(!computation_run(control, scoped_quadrature, &test), "unattached integrator inherits scope");
    TEST_ASSERT_TRUE(computation_status(control) == COMPUTATION_BUDGET_EXHAUSTED, "shared expression budget");
    computation_free(control);
    control = computation_new(NULL);
    computation_t *other = computation_new(NULL);
    intg_set_computation(test.integrator, other);
    TEST_ASSERT_TRUE(computation_run(control, scoped_quadrature, &test) && errno == EBUSY,
                     "conflicting explicit control rejected without stopping outer control");
    computation_free(other);
    computation_free(control);
    intg_free(test.integrator);
    expr_free(test.formula);
    expr_bindings_free(bindings);
}

/* Register ordinary scoped-control regressions before the suite's README examples. */
void test_computation_expression_cases(void)
{
    TEST_RUN_CASE(test_computation_scopes, NULL);
    TEST_RUN_CASE(test_computation_expression_recovery, NULL);
    TEST_RUN_CASE(test_computation_inherited_integrator, NULL);
    TEST_RUN_CASE(test_computation_sum_product_recovery, NULL);
}

/* README example: a stopped evaluation leaves its graph reusable outside the scope. */
static void example_computation_scope(void)
{
    expr_bindings_t *bindings = NULL;
    scoped_test_t test = {.formula = expr_from_string("x*x*x + 2*x*x + 3*x + 4", &bindings)};
    test.x = expr_bindings_get(bindings, "x");
    computation_options_t options = {.work_limit = 1};
    computation_t *control = computation_new(&options);
    if (!test.formula || !test.x || !control)
        abort();
    expr_set_val(test.x, NUM_TWO);
    bool stopped = !computation_run(control, scoped_eval, &test);
    number_t value = expr_eval(test.formula), expected = num_create_from_long(26);
    bool reusable = num_eq(value, expected);
    printf("Stopped: %d; graph reusable: %d\n", stopped, reusable);
    num_destroy(&expected);
    num_destroy(&value);
    expr_free(test.formula);
    expr_bindings_free(bindings);
    computation_free(control);
    TEST_ASSERT_TRUE(stopped && reusable, "README scope and graph recovery");
}

/* Keep the documented example after all ordinary test cases. */
void test_computation_expression_readme(void)
{
    TEST_RUN_OUTPUT(example_computation_scope);
}
