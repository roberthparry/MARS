#include <string.h>

#include "test_expr.h"

static void assert_residual_small(number_t residual, const char *tolerance_text)
{
    number_t mag = num_abs(residual);
    number_t tolerance = num_create_from_string(tolerance_text);

    ASSERT_TRUE(num_lt(mag, tolerance));
    num_destroy(&tolerance);
    num_destroy(&mag);
}

static void test_goal_seek_real_one_variable(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ x^2 - 9 | x = NAN }", &bindings);
    expr_t *x;
    number_t target = num_create_from_long(0L);
    number_t tolerance = num_create_from_string("1e-20");
    expr_goal_seek_options_t options = {
        .precision_digits = 32u,
        .max_iterations = 160u,
        .allow_complex = true,
        .simplify_result = false,
        .tolerance = tolerance
    };
    expr_goal_seek_result_t result;
    number_t x_value;
    number_t expected;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(bindings);
    ASSERT_EQ_INT(expr_goal_seek(expr, bindings, target, &options, &result), 0);
    ASSERT_TRUE(result.converged);
    ASSERT_FALSE(result.used_complex);
    assert_residual_small(result.residual, "1e-18");

    x = expr_bindings_get(bindings, "x");
    ASSERT_NOT_NULL(x);
    x_value = expr_eval(x);
    expected = num_create_from_long(3L);
    ASSERT_EXPR_NUMBER_CLOSE(x_value, expected);
    num_destroy(&expected);
    num_destroy(&x_value);

    expr_goal_seek_result_clear(&result);
    num_destroy(&tolerance);
    num_destroy(&target);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_goal_seek_complex_fallback(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ x^2 - i | x = NAN }", &bindings);
    number_t target = num_create_from_long(0L);
    number_t tolerance = num_create_from_string("1e-20");
    expr_goal_seek_options_t options = {
        .precision_digits = 40u,
        .max_iterations = 80u,
        .allow_complex = true,
        .simplify_result = false,
        .tolerance = tolerance
    };
    expr_goal_seek_result_t result;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(bindings);
    ASSERT_EQ_INT(expr_goal_seek(expr, bindings, target, &options, &result), 0);
    ASSERT_TRUE(result.converged);
    ASSERT_TRUE(result.used_complex);
    assert_residual_small(result.residual, "1e-18");

    expr_goal_seek_result_clear(&result);
    num_destroy(&tolerance);
    num_destroy(&target);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_goal_seek_complex_fallback_from_real_axis(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ x^2 + 1 | x = NAN }", &bindings);
    expr_t *x;
    number_t target = num_create_from_long(0L);
    number_t tolerance = num_create_from_string("1e-20");
    expr_goal_seek_options_t options = {
        .precision_digits = 40u,
        .max_iterations = 120u,
        .allow_complex = true,
        .simplify_result = false,
        .tolerance = tolerance
    };
    expr_goal_seek_result_t result;
    number_t x_value;
    number_t x_real;
    number_t x_imag;
    number_t x_imag_abs;
    char *text;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(bindings);
    ASSERT_EQ_INT(expr_goal_seek(expr, bindings, target, &options, &result), 0);
    ASSERT_TRUE(result.converged);
    ASSERT_TRUE(result.used_complex);
    assert_residual_small(result.residual, "1e-18");

    x = expr_bindings_get(bindings, "x");
    ASSERT_NOT_NULL(x);
    x_value = expr_eval(x);
    x_real = num_real_part(x_value);
    x_imag = num_imag_part(x_value);
    x_imag_abs = num_abs(x_imag);
    ASSERT_TRUE(num_is_zero(x_real));
    ASSERT_EXPR_NUMBER_CLOSE(x_imag_abs, NUM_ONE);
    num_destroy(&x_imag_abs);
    num_destroy(&x_imag);
    num_destroy(&x_real);
    num_destroy(&x_value);

    text = expr_to_string(expr, style_EXPRESSION);
    ASSERT_NOT_NULL(text);
    ASSERT_TRUE(strcmp(text, "{ x² + 1 | x = i }") == 0 ||
                strcmp(text, "{ x² + 1 | x = -i }") == 0);
    free(text);

    expr_goal_seek_result_clear(&result);
    num_destroy(&tolerance);
    num_destroy(&target);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_goal_seek_complex_uses_guard_precision(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ x^4 + 1 | x = NAN }", &bindings);
    number_t target = num_create_from_long(0L);
    expr_goal_seek_options_t options = {
        .precision_digits = 8u,
        .max_iterations = 120u,
        .allow_complex = true,
        .simplify_result = false
    };
    expr_goal_seek_result_t result;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(bindings);
    ASSERT_EQ_INT(expr_goal_seek(expr, bindings, target, &options, &result), 0);
    ASSERT_TRUE(result.converged);
    ASSERT_TRUE(result.used_complex);
    assert_residual_small(result.residual, "1.01e-8");

    expr_goal_seek_result_clear(&result);
    num_destroy(&target);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_goal_seek_complex_lgamma_imaginary_axis(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ lgamma(ix) | x = NAN }", &bindings);
    number_t target = num_create_from_long(24L);
    expr_goal_seek_options_t options = {
        .precision_digits = 64u,
        .allow_complex = true,
        .simplify_result = false
    };
    expr_goal_seek_result_t result;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(bindings);
    ASSERT_EQ_INT(expr_goal_seek(expr, bindings, target, &options, &result), 0);
    ASSERT_TRUE(result.converged);
    ASSERT_TRUE(result.used_complex);
    assert_residual_small(result.residual, "1e-50");

    expr_goal_seek_result_clear(&result);
    num_destroy(&target);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_goal_seek_real_multi_variable(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string(
        "{ ax + by + cz - 8 | a = NAN, b = NAN, c = NAN; x = 1, y = 2, z = 3 }",
        &bindings);
    number_t target = num_create_from_long(0L);
    number_t tolerance = num_create_from_string("1e-20");
    expr_goal_seek_options_t options = {
        .precision_digits = 32u,
        .max_iterations = 160u,
        .allow_complex = true,
        .simplify_result = false,
        .tolerance = tolerance
    };
    expr_goal_seek_result_t result;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(bindings);
    ASSERT_EQ_INT(expr_goal_seek(expr, bindings, target, &options, &result), 0);
    ASSERT_TRUE(result.converged);
    ASSERT_FALSE(result.used_complex);
    assert_residual_small(result.residual, "1e-18");

    expr_goal_seek_result_clear(&result);
    num_destroy(&tolerance);
    num_destroy(&target);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_goal_seek_default_tolerance_uses_precision_digits(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ x^2 - 2 | x = NAN }", &bindings);
    number_t target = num_create_from_long(0L);
    expr_goal_seek_options_t options = {
        .precision_digits = 32u,
        .max_iterations = 160u,
        .allow_complex = false,
        .simplify_result = false
    };
    expr_goal_seek_result_t result;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(bindings);
    ASSERT_EQ_INT(expr_goal_seek(expr, bindings, target, &options, &result), 0);
    ASSERT_TRUE(result.converged);
    ASSERT_FALSE(result.used_complex);
    assert_residual_small(result.residual, "1.01e-32");

    expr_goal_seek_result_clear(&result);
    num_destroy(&target);
    expr_bindings_free(bindings);
    expr_free(expr);
}

static void test_goal_seek_default_iterations_scale_with_precision(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *expr = expr_from_string("{ x^2 - 2 | x = NAN }", &bindings);
    number_t target = num_create_from_long(0L);
    expr_goal_seek_options_t options = {
        .precision_digits = 96u,
        .allow_complex = false,
        .simplify_result = false
    };
    expr_goal_seek_result_t result;

    ASSERT_NOT_NULL(expr);
    ASSERT_NOT_NULL(bindings);
    ASSERT_EQ_INT(expr_goal_seek(expr, bindings, target, &options, &result), 0);
    ASSERT_TRUE(result.converged);
    ASSERT_FALSE(result.used_complex);
    assert_residual_small(result.residual, "1.01e-96");

    expr_goal_seek_result_clear(&result);
    num_destroy(&target);
    expr_bindings_free(bindings);
    expr_free(expr);
}

void test_expr_t_goal_seek(void)
{
    TEST_RUN_SUBTEST(test_goal_seek_real_one_variable, NULL);
    TEST_RUN_SUBTEST(test_goal_seek_complex_fallback, NULL);
    TEST_RUN_SUBTEST(test_goal_seek_complex_fallback_from_real_axis, NULL);
    TEST_RUN_SUBTEST(test_goal_seek_complex_uses_guard_precision, NULL);
    TEST_RUN_SUBTEST(test_goal_seek_complex_lgamma_imaginary_axis, NULL);
    TEST_RUN_SUBTEST(test_goal_seek_real_multi_variable, NULL);
    TEST_RUN_SUBTEST(test_goal_seek_default_tolerance_uses_precision_digits, NULL);
    TEST_RUN_SUBTEST(test_goal_seek_default_iterations_scale_with_precision, NULL);
}
