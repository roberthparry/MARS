/**
 * @file test_context_solver.c
 * @brief Structural evidence, row exchange and guarded symbolic division regressions.
 *
 * Checks that numerical variable guesses never authorise division and that missing
 * assumptions remain distinct from structural singularity. All cases use public APIs.
 */
#include <errno.h>
#include <stdlib.h>

#include "matrix-t.h"
#include "test_harness.h"
#include "test_context_solver.h"

static void require_setup(bool ready)
{
    if (!ready)
        abort();
}

static void test_context_structural_proofs(void)
{
    expr_t *x = expr_new_named_var(NUM_ONE, "x");
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *sum = expr_add(x, one);
    expr_t *ratio = expr_div(one, x);
    expr_t *transcendental = expr_sin(x);
    expr_context_t *context = expr_context_new();
    require_setup(context && x && one && sum && ratio && transcendental);
    require_setup(expr_context_set(context, x, EXPR_CONTEXT_FREE, 0, NUM_ONE));
    TEST_ASSERT_TRUE(!expr_context_proves(context, x, EXPR_CONTEXT_NONZERO), "a variable sample is not a proof");
    TEST_ASSERT_TRUE(!expr_context_proves(context, ratio, EXPR_CONTEXT_NONZERO), "unknown divisor stays unknown");
    require_setup(expr_context_set(context, x, EXPR_CONTEXT_FREE, EXPR_CONTEXT_POSITIVE, NUM_NAN));
    TEST_ASSERT_TRUE(expr_context_proves(context, sum, EXPR_CONTEXT_POSITIVE | EXPR_CONTEXT_REAL),
                     "positive summands establish a positive real sum");
    TEST_ASSERT_TRUE(expr_context_proves(context, ratio, EXPR_CONTEXT_POSITIVE), "proven divisor permits a quotient");
    TEST_ASSERT_TRUE(!expr_context_proves(context, transcendental, EXPR_CONTEXT_NONZERO),
                     "a positive argument does not prove its sine non-zero");
    require_setup(expr_context_set(context, x, EXPR_CONTEXT_PARAMETER, 0, NUM_ONE));
    TEST_ASSERT_TRUE(expr_context_proves(context, x, EXPR_CONTEXT_NONZERO), "parameter evidence is available");
    TEST_ASSERT_TRUE(!expr_context_proves(context, transcendental, EXPR_CONTEXT_NONZERO),
                     "compound numerical evaluation is not structural solver evidence");
    matrix_t *compound = mat_create_expr(1, 1, &transcendental);
    matrix_t *inverse = mat_inverse_with_context(compound, context);
    TEST_ASSERT_TRUE(!inverse && errno == EAGAIN, "solver refuses a merely sampled compound pivot");
    mat_free(compound);
    require_setup(expr_context_set(context, x, EXPR_CONTEXT_PARAMETER, 0, NUM_ZERO));
    TEST_ASSERT_TRUE(!expr_context_proves(context, x, EXPR_CONTEXT_NONZERO), "context value overrides source sample");
    TEST_ASSERT_TRUE(!expr_context_proves(context, ratio, EXPR_CONTEXT_REAL), "zero divisor establishes no quotient facts");
    TEST_ASSERT_TRUE(!expr_context_proves(NULL, sum, EXPR_CONTEXT_REAL), "null context rejected");
    TEST_ASSERT_TRUE(!expr_context_proves(context, sum, 0), "empty query rejected");
    TEST_ASSERT_TRUE(!expr_context_proves(context, sum, 128), "unknown query bits rejected");
    expr_context_free(context);
    expr_free(transcendental);
    expr_free(ratio);
    expr_free(sum);
    expr_free(one);
    expr_free(x);
}

static void test_context_guarded_inverse(void)
{
    expr_t *x = expr_new_named_var(NUM_ONE, "x");
    matrix_t *source = mat_create_expr(1, 1, &x);
    expr_context_t *context = expr_context_new();
    require_setup(source && context);
    require_setup(expr_context_set(context, x, EXPR_CONTEXT_FREE, 0, NUM_ONE));
    errno = 0;
    matrix_t *inverse = mat_inverse_with_context(source, context);
    TEST_ASSERT_TRUE(!inverse && errno == EAGAIN, "unknown non-zero pivot reports insufficient evidence");
    require_setup(expr_context_set(context, x, EXPR_CONTEXT_FREE, EXPR_CONTEXT_NONZERO, NUM_TWO));
    inverse = mat_inverse_with_context(source, context);
    require_setup(inverse != NULL);
    expr_t *entry = NULL;
    mat_get(inverse, 0, 0, &entry);
    number_t value = expr_context_evaluate(context, entry);
    number_t half = num_div(NUM_ONE, NUM_TWO);
    TEST_ASSERT_TRUE(num_eq(value, half), "assumed non-zero pivot produces its reciprocal");
    number_t original = expr_eval(x);
    TEST_ASSERT_TRUE(num_eq(original, NUM_ONE), "solver leaves source binding untouched");
    num_destroy(&original);
    num_destroy(&half);
    num_destroy(&value);
    mat_free(inverse);
    require_setup(expr_context_set(context, x, EXPR_CONTEXT_PARAMETER, 0, NUM_ZERO));
    inverse = mat_inverse_with_context(source, context);
    TEST_ASSERT_TRUE(!inverse && errno == EAGAIN, "zero parameter never authorises a division");
    mat_free(source);
    expr_free(x);
    expr_context_free(context);
}

static void expect_identity(const matrix_t *matrix, const expr_context_t *context)
{
    require_setup(matrix != NULL);
    for (size_t row = 0; row < mat_get_row_count(matrix); ++row)
        for (size_t col = 0; col < mat_get_col_count(matrix); ++col) {
            expr_t *entry = NULL;
            mat_get(matrix, row, col, &entry);
            number_t value = expr_context_evaluate(context, entry);
            bool equal = num_eq(value, row == col ? NUM_ONE : NUM_ZERO);
            num_destroy(&value);
            TEST_ASSERT_TRUE(equal, "matrix times guarded inverse is the identity");
        }
}

static void test_context_guarded_row_exchange(void)
{
    matrix_t *source = mat_from_string_expr("(x,1;1,0)", NULL);
    expr_context_t *context = expr_context_new();
    matrix_t *inverse = mat_inverse_with_context(source, context);
    require_setup(inverse != NULL);
    matrix_t *product = mat_mul(source, inverse);
    expect_identity(product, context);
    mat_free(product);
    mat_free(inverse);
    mat_free(source);
    source = mat_from_string_expr("(1,2;2,4)", NULL);
    errno = 0;
    inverse = mat_inverse_with_context(source, context);
    TEST_ASSERT_TRUE(!inverse && errno == EDOM, "structurally singular system is distinct from unknown pivots");
    mat_free(source);
    TEST_ASSERT_TRUE(!mat_inverse_with_context(NULL, context) && errno == EINVAL, "null matrix rejected");
    expr_context_free(context);
}

static void test_context_guarded_multiple_rhs(void)
{
    matrix_t *a = mat_from_string_expr("(0,2,1;1,1,0;2,0,1)", NULL);
    matrix_t *b = mat_create_identity(3);
    expr_context_t *context = expr_context_new();
    matrix_t *solution = mat_solve_with_context(a, b, context);
    require_setup(solution != NULL);
    matrix_t *product = mat_mul(a, solution);
    expect_identity(product, context);
    mat_free(product);
    mat_free(solution);
    TEST_ASSERT_TRUE(!mat_solve_with_context(a, b, NULL) && errno == EINVAL, "explicit context required");
    mat_free(b);
    b = mat_create_identity(2);
    TEST_ASSERT_TRUE(!mat_solve_with_context(a, b, context) && errno == EINVAL, "incompatible dimensions rejected");
    mat_free(b);
    mat_free(a);
    expr_context_free(context);
}

static void test_context_guarded_parameter_snapshot(void)
{
    expr_t *a = expr_new_named_const(NUM_ZERO, "a");
    matrix_t *source = mat_create_expr(1, 1, &a);
    expr_context_t *context = expr_context_new();
    require_setup(expr_context_set(context, a, EXPR_CONTEXT_PARAMETER, 0, NUM_TWO));
    matrix_t *inverse = mat_inverse_with_context(source, context);
    require_setup(inverse != NULL);
    expr_t *entry = NULL;
    mat_get(inverse, 0, 0, &entry);
    number_t value = expr_context_evaluate(context, entry);
    number_t half = num_div(NUM_ONE, NUM_TWO);
    TEST_ASSERT_TRUE(num_eq(value, half), "updated parameter value governs solving, not its old zero binding");
    num_destroy(&value);
    num_destroy(&half);
    value = expr_eval(a);
    TEST_ASSERT_TRUE(num_eq(value, NUM_ZERO), "original constant binding remains zero");
    num_destroy(&value);
    mat_free(inverse);
    mat_free(source);
    expr_free(a);
    expr_context_free(context);
}

/* Register independent proof and elimination cases with the sequential test runner. */
void test_context_solver_cases(void)
{
    TEST_RUN_CASE(test_context_structural_proofs, NULL);
    TEST_RUN_CASE(test_context_guarded_inverse, NULL);
    TEST_RUN_CASE(test_context_guarded_row_exchange, NULL);
    TEST_RUN_CASE(test_context_guarded_multiple_rhs, NULL);
    TEST_RUN_CASE(test_context_guarded_parameter_snapshot, NULL);
}
