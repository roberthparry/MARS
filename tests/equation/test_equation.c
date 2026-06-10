#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "equation.h"
#include "expression.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static expr_t *test_equation_const_d(double value)
{
    number_t number = num_create_from_double(value);
    expr_t *expr = expr_new_const(number);

    num_destroy(&number);
    return expr;
}

static expr_t *test_equation_named_var_d(double value, const char *name)
{
    number_t number = num_create_from_double(value);
    expr_t *expr = expr_new_named_var(number, name);

    num_destroy(&number);
    return expr;
}

static bool test_number_equals_long(number_t actual, long expected_value)
{
    number_t expected = num_create_from_long(expected_value);
    bool ok = num_eq(actual, expected);

    num_destroy(&expected);
    return ok;
}

static bool test_equation_result_contains_long(const equation_solve_result_t *result,
                                               long expected_value)
{
    for (size_t i = 0u; i < result->count; ++i) {
        number_t value = expr_eval(equation_rhs(result->solutions[i]));
        bool match = test_number_equals_long(value, expected_value);

        num_destroy(&value);
        if (match)
            return true;
    }

    return false;
}

static bool test_equation_result_has_solution_for(const equation_solve_result_t *result,
                                                  const expr_t *wrt)
{
    for (size_t i = 0u; i < result->count; ++i) {
        if (equation_is_solved_for(result->solutions[i], wrt))
            return true;
    }

    return false;
}

static void test_equation_assert_residual_small(const equation_t *equation,
                                                const char *tolerance_text)
{
    expr_t *residual = equation_residual(equation);
    number_t value;
    number_t mag;
    number_t tolerance;

    ASSERT_NOT_NULL(residual);
    value = expr_eval(residual);
    mag = num_abs(value);
    tolerance = num_create_from_string(tolerance_text);

    ASSERT_TRUE(num_lt(mag, tolerance));

    num_destroy(&tolerance);
    num_destroy(&mag);
    num_destroy(&value);
    expr_free(residual);
}

static bool test_equation_rhs_string_equals(const equation_t *equation,
                                            const char *expected)
{
    string_t *text;
    bool ok;

    if (!equation)
        return false;

    text = expr_to_text(equation_rhs(equation), style_EXPRESSION);
    if (!text)
        return false;

    ok = strcmp(string_c_str(text), expected) == 0;
    string_free(text);
    return ok;
}

static bool test_equation_rhs_text_contains(const equation_t *equation,
                                            style_t style,
                                            const char *expected)
{
    string_t *text;
    bool ok;

    if (!equation)
        return false;

    text = expr_to_text(equation_rhs(equation), style);
    if (!text)
        return false;

    ok = strstr(string_c_str(text), expected) != NULL;
    string_free(text);
    return ok;
}

static bool test_equation_result_has_rhs_text_containing(
    const equation_solve_result_t *result,
    style_t style,
    const char *expected)
{
    for (size_t i = 0u; i < result->count; ++i) {
        if (test_equation_rhs_text_contains(result->solutions[i], style,
                                            expected))
            return true;
    }

    return false;
}

static void test_equation_from_string_shares_symbols_across_sides(void)
{
    expr_bindings_t *bindings = NULL;
    equation_t *equation = equation_from_string("{ x = x + 1 | x = NAN }", &bindings);
    expr_t *x;
    equation_solve_result_t result;

    ASSERT_NOT_NULL(equation);
    ASSERT_NOT_NULL(bindings);
    x = expr_bindings_get(bindings, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_FALSE(equation_is_solved_for(equation, x));
    ASSERT_EQ_INT(equation_solve_for(equation, x, &result), 0);
    ASSERT_EQ_INT(result.status, EQUATION_SOLVE_UNSOLVED);
    ASSERT_EQ_INT((int)result.count, 0);

    equation_solve_result_clear(&result);
    expr_bindings_free(bindings);
    equation_free(equation);
}

static void test_equation_from_string_accepts_bare_equation(void)
{
    expr_bindings_t *bindings = NULL;
    equation_t *equation = equation_from_string("2*x + 3 = 7", &bindings);
    expr_t *x;
    equation_solve_result_t result;

    ASSERT_NOT_NULL(equation);
    ASSERT_NOT_NULL(bindings);
    x = expr_bindings_get(bindings, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equation_solve_for(equation, x, &result), 0);
    ASSERT_EQ_INT(result.status, EQUATION_SOLVE_SOLVED);
    ASSERT_EQ_INT((int)result.count, 1);
    ASSERT_TRUE(test_equation_result_contains_long(&result, 2L));

    equation_solve_result_clear(&result);
    expr_bindings_free(bindings);
    equation_free(equation);
}

static void test_equation_numeric_solves_all_variable_bindings(void)
{
    expr_bindings_t *bindings = NULL;
    equation_t *equation = equation_from_string(
        "{ x^2 + y^2 - 5 = 0 | x = 1, y = 1 }",
        &bindings);
    expr_t *x;
    expr_t *y;
    expr_goal_seek_options_t options = {
        .precision_digits = 32u,
        .max_iterations = 160u,
        .allow_complex = false,
        .simplify_result = false
    };
    equation_solve_result_t result;

    ASSERT_NOT_NULL(equation);
    ASSERT_NOT_NULL(bindings);
    x = expr_bindings_get(bindings, "x");
    y = expr_bindings_get(bindings, "y");
    ASSERT_NOT_NULL(x);
    ASSERT_NOT_NULL(y);

    ASSERT_EQ_INT(equation_solve_numeric(equation, bindings, &options, &result), 0);
    ASSERT_EQ_INT(result.status, EQUATION_SOLVE_SOLVED);
    ASSERT_EQ_INT((int)result.count, 2);
    ASSERT_TRUE(test_equation_result_has_solution_for(&result, x));
    ASSERT_TRUE(test_equation_result_has_solution_for(&result, y));
    test_equation_assert_residual_small(equation, "1e-24");

    equation_solve_result_clear(&result);
    expr_bindings_free(bindings);
    equation_free(equation);
}

static void test_equation_numeric_rejects_unresolved_parameter_residual(void)
{
    expr_bindings_t *bindings = NULL;
    equation_t *equation = equation_from_string(
        "{ a*x^2 + b*x + c = 0 | x = 0 }",
        &bindings);
    expr_t *x;
    number_t x_value;
    expr_goal_seek_options_t options = {
        .precision_digits = 32u,
        .max_iterations = 160u,
        .allow_complex = true,
        .simplify_result = false
    };
    equation_solve_result_t result;

    ASSERT_NOT_NULL(equation);
    ASSERT_NOT_NULL(bindings);
    x = expr_bindings_get(bindings, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equation_solve_numeric(equation, bindings, &options, &result), 0);
    ASSERT_EQ_INT(result.status, EQUATION_SOLVE_UNSOLVED);
    ASSERT_EQ_INT((int)result.count, 0);

    x_value = expr_eval(x);
    ASSERT_TRUE(test_number_equals_long(x_value, 0L));

    num_destroy(&x_value);
    equation_solve_result_clear(&result);
    expr_bindings_free(bindings);
    equation_free(equation);
}

static void test_equation_to_text_round_trips_through_parser(void)
{
    expr_bindings_t *bindings = NULL;
    expr_bindings_t *roundtrip_bindings = NULL;
    equation_t *equation = equation_from_string("{ 2*x + 3 = 7 | x = 4 }", &bindings);
    string_t *text;
    equation_t *roundtrip;
    expr_t *x;
    equation_solve_result_t result;
    number_t x_value;

    ASSERT_NOT_NULL(equation);
    ASSERT_NOT_NULL(bindings);

    text = equation_to_text(equation, style_EXPRESSION);
    ASSERT_NOT_NULL(text);
    roundtrip = equation_from_text(text, &roundtrip_bindings);
    ASSERT_NOT_NULL(roundtrip);
    ASSERT_NOT_NULL(roundtrip_bindings);

    x = expr_bindings_get(roundtrip_bindings, "x");
    ASSERT_NOT_NULL(x);
    x_value = expr_eval(x);
    ASSERT_TRUE(test_number_equals_long(x_value, 4L));
    num_destroy(&x_value);

    ASSERT_EQ_INT(equation_solve_for(roundtrip, x, &result), 0);
    ASSERT_EQ_INT(result.status, EQUATION_SOLVE_SOLVED);
    ASSERT_EQ_INT((int)result.count, 1);
    ASSERT_TRUE(test_equation_result_contains_long(&result, 2L));

    equation_solve_result_clear(&result);
    equation_free(roundtrip);
    string_free(text);
    expr_bindings_free(roundtrip_bindings);
    expr_bindings_free(bindings);
    equation_free(equation);
}

static void test_equation_detects_already_solved_form(void)
{
    expr_t *x = test_equation_named_var_d(0.0, "x");
    expr_t *two = test_equation_const_d(2.0);
    equation_t *equation = equation_new(x, two);
    equation_solve_result_t result;

    ASSERT_NOT_NULL(equation);
    ASSERT_TRUE(equation_lhs(equation) == x);
    ASSERT_TRUE(equation_rhs(equation) == two);
    ASSERT_TRUE(equation_is_solved_for(equation, x));

    ASSERT_EQ_INT(equation_solve_for(equation, x, &result), 0);
    ASSERT_EQ_INT(result.status, EQUATION_SOLVE_SOLVED);
    ASSERT_EQ_INT((int)result.count, 1);
    ASSERT_TRUE(equation_is_solved_for(result.solutions[0], x));

    equation_solve_result_clear(&result);
    equation_free(equation);
    expr_free(two);
    expr_free(x);
}

static void test_equation_rejects_rhs_containing_solve_variable(void)
{
    expr_t *x = test_equation_named_var_d(0.0, "x");
    expr_t *one = test_equation_const_d(1.0);
    expr_t *x_plus_one = expr_add(x, one);
    equation_t *equation = equation_new(x, x_plus_one);
    equation_solve_result_t result;

    ASSERT_NOT_NULL(equation);
    ASSERT_FALSE(equation_is_solved_for(equation, x));
    ASSERT_EQ_INT(equation_solve_for(equation, x, &result), 0);
    ASSERT_EQ_INT(result.status, EQUATION_SOLVE_UNSOLVED);
    ASSERT_EQ_INT((int)result.count, 0);

    equation_solve_result_clear(&result);
    equation_free(equation);
    expr_free(x_plus_one);
    expr_free(one);
    expr_free(x);
}

static void test_equation_solves_simple_affine_equation(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *lhs = expr_from_string("{ 2*x + 3 | x = NAN }", &bindings);
    expr_t *rhs = test_equation_const_d(7.0);
    expr_t *x;
    equation_t *equation;
    equation_solve_result_t result;

    ASSERT_NOT_NULL(lhs);
    ASSERT_NOT_NULL(bindings);
    x = expr_bindings_get(bindings, "x");
    ASSERT_NOT_NULL(x);

    equation = equation_new(lhs, rhs);
    ASSERT_NOT_NULL(equation);
    ASSERT_EQ_INT(equation_solve_for(equation, x, &result), 0);
    ASSERT_EQ_INT(result.status, EQUATION_SOLVE_SOLVED);
    ASSERT_EQ_INT((int)result.count, 1);
    ASSERT_TRUE(equation_is_solved_for(result.solutions[0], x));
    ASSERT_TRUE(test_equation_result_contains_long(&result, 2L));

    equation_solve_result_clear(&result);
    equation_free(equation);
    expr_free(rhs);
    expr_bindings_free(bindings);
    expr_free(lhs);
}

static void test_equation_solves_symbolic_affine_equation(void)
{
    expr_bindings_t *bindings = NULL;
    equation_t *equation = equation_from_string(
        "{ b*x + c = 0 | x = NAN; b = NAN, c = NAN }",
        &bindings);
    expr_t *x;
    equation_solve_result_t result;

    ASSERT_NOT_NULL(equation);
    ASSERT_NOT_NULL(bindings);
    x = expr_bindings_get(bindings, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equation_solve_for(equation, x, &result), 0);
    ASSERT_EQ_INT(result.status, EQUATION_SOLVE_SOLVED);
    ASSERT_EQ_INT((int)result.count, 1);
    ASSERT_TRUE(equation_is_solved_for(result.solutions[0], x));
    ASSERT_TRUE(test_equation_rhs_text_contains(result.solutions[0],
                                                style_UNBOUND, "-c/b"));

    equation_solve_result_clear(&result);
    expr_bindings_free(bindings);
    equation_free(equation);
}

static void test_equation_solves_affine_variable_from_rhs(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *lhs = test_equation_const_d(7.0);
    expr_t *rhs = expr_from_string("{ 2*x + 3 | x = NAN }", &bindings);
    expr_t *x;
    equation_t *equation;
    equation_solve_result_t result;

    ASSERT_NOT_NULL(rhs);
    ASSERT_NOT_NULL(bindings);
    x = expr_bindings_get(bindings, "x");
    ASSERT_NOT_NULL(x);

    equation = equation_new(lhs, rhs);
    ASSERT_NOT_NULL(equation);
    ASSERT_EQ_INT(equation_solve_for(equation, x, &result), 0);
    ASSERT_EQ_INT(result.status, EQUATION_SOLVE_SOLVED);
    ASSERT_EQ_INT((int)result.count, 1);
    ASSERT_TRUE(equation_is_solved_for(result.solutions[0], x));
    ASSERT_TRUE(test_equation_result_contains_long(&result, 2L));

    equation_solve_result_clear(&result);
    equation_free(equation);
    expr_bindings_free(bindings);
    expr_free(rhs);
    expr_free(lhs);
}

static void test_equation_solves_symbolic_quadratic_formula(void)
{
    expr_bindings_t *bindings = NULL;
    equation_t *equation = equation_from_string(
        "{ a*x^2 + b*x + c = 0 | x = NAN; a = NAN, b = NAN, c = NAN }",
        &bindings);
    expr_t *x;
    equation_solve_result_t result;

    ASSERT_NOT_NULL(equation);
    ASSERT_NOT_NULL(bindings);
    x = expr_bindings_get(bindings, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equation_solve_for(equation, x, &result), 0);
    ASSERT_EQ_INT(result.status, EQUATION_SOLVE_SOLVED);
    ASSERT_EQ_INT((int)result.count, 2);
    ASSERT_TRUE(equation_is_solved_for(result.solutions[0], x));
    ASSERT_TRUE(equation_is_solved_for(result.solutions[1], x));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(
        &result, style_TEX, "\\sqrt{b^{2} - 4 a c}"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(
        &result, style_TEX, "\\frac{\\sqrt{b^{2} - 4 a c} - b}{2 a}"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(
        &result, style_TEX, "\\frac{-\\sqrt{b^{2} - 4 a c} - b}{2 a}"));

    equation_solve_result_clear(&result);
    expr_bindings_free(bindings);
    equation_free(equation);
}

static void test_equation_solves_quadratic_two_roots(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *lhs = expr_from_string("{ x^2 | x = NAN }", &bindings);
    expr_t *rhs = test_equation_const_d(4.0);
    expr_t *x;
    equation_t *equation;
    equation_solve_result_t result;

    ASSERT_NOT_NULL(lhs);
    ASSERT_NOT_NULL(bindings);
    x = expr_bindings_get(bindings, "x");
    ASSERT_NOT_NULL(x);

    equation = equation_new(lhs, rhs);
    ASSERT_NOT_NULL(equation);
    ASSERT_EQ_INT(equation_solve_for(equation, x, &result), 0);
    ASSERT_EQ_INT(result.status, EQUATION_SOLVE_SOLVED);
    ASSERT_EQ_INT((int)result.count, 2);
    ASSERT_TRUE(test_equation_result_contains_long(&result, -2L));
    ASSERT_TRUE(test_equation_result_contains_long(&result, 2L));

    equation_solve_result_clear(&result);
    equation_free(equation);
    expr_free(rhs);
    expr_bindings_free(bindings);
    expr_free(lhs);
}

static void test_equation_solves_quadratic_zero_product(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *lhs = expr_from_string("{ (x - 2)*(x + 3) | x = NAN }", &bindings);
    expr_t *rhs = test_equation_const_d(0.0);
    expr_t *x;
    equation_t *equation;
    equation_solve_result_t result;

    ASSERT_NOT_NULL(lhs);
    ASSERT_NOT_NULL(bindings);
    x = expr_bindings_get(bindings, "x");
    ASSERT_NOT_NULL(x);

    equation = equation_new(lhs, rhs);
    ASSERT_NOT_NULL(equation);
    ASSERT_EQ_INT(equation_solve_for(equation, x, &result), 0);
    ASSERT_EQ_INT(result.status, EQUATION_SOLVE_SOLVED);
    ASSERT_EQ_INT((int)result.count, 2);
    ASSERT_TRUE(test_equation_result_contains_long(&result, -3L));
    ASSERT_TRUE(test_equation_result_contains_long(&result, 2L));

    equation_solve_result_clear(&result);
    equation_free(equation);
    expr_free(rhs);
    expr_bindings_free(bindings);
    expr_free(lhs);
}

static void test_equation_solves_quadratic_double_root_once(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *lhs = expr_from_string("{ x^2 - 4*x + 4 | x = NAN }", &bindings);
    expr_t *rhs = test_equation_const_d(0.0);
    expr_t *x;
    equation_t *equation;
    equation_solve_result_t result;

    ASSERT_NOT_NULL(lhs);
    ASSERT_NOT_NULL(bindings);
    x = expr_bindings_get(bindings, "x");
    ASSERT_NOT_NULL(x);

    equation = equation_new(lhs, rhs);
    ASSERT_NOT_NULL(equation);
    ASSERT_EQ_INT(equation_solve_for(equation, x, &result), 0);
    ASSERT_EQ_INT(result.status, EQUATION_SOLVE_SOLVED);
    ASSERT_EQ_INT((int)result.count, 1);
    ASSERT_TRUE(test_equation_result_contains_long(&result, 2L));

    equation_solve_result_clear(&result);
    equation_free(equation);
    expr_free(rhs);
    expr_bindings_free(bindings);
    expr_free(lhs);
}

static void test_equation_solves_cubic_three_real_roots(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *lhs = expr_from_string("{ x^3 - 6*x^2 + 11*x - 6 | x = NAN }",
                                   &bindings);
    expr_t *rhs = test_equation_const_d(0.0);
    expr_t *x;
    equation_t *equation;
    equation_solve_result_t result;

    ASSERT_NOT_NULL(lhs);
    ASSERT_NOT_NULL(bindings);
    x = expr_bindings_get(bindings, "x");
    ASSERT_NOT_NULL(x);

    equation = equation_new(lhs, rhs);
    ASSERT_NOT_NULL(equation);
    ASSERT_EQ_INT(equation_solve_for(equation, x, &result), 0);
    ASSERT_EQ_INT(result.status, EQUATION_SOLVE_SOLVED);
    ASSERT_EQ_INT((int)result.count, 3);
    ASSERT_TRUE(test_equation_result_contains_long(&result, 1L));
    ASSERT_TRUE(test_equation_result_contains_long(&result, 2L));
    ASSERT_TRUE(test_equation_result_contains_long(&result, 3L));

    equation_solve_result_clear(&result);
    equation_free(equation);
    expr_free(rhs);
    expr_bindings_free(bindings);
    expr_free(lhs);
}

static void test_equation_solves_cubic_complex_pair(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *lhs = expr_from_string("{ x^3 - 1 | x = NAN }", &bindings);
    expr_t *rhs = test_equation_const_d(0.0);
    expr_t *x;
    equation_t *equation;
    equation_solve_result_t result;

    ASSERT_NOT_NULL(lhs);
    ASSERT_NOT_NULL(bindings);
    x = expr_bindings_get(bindings, "x");
    ASSERT_NOT_NULL(x);

    equation = equation_new(lhs, rhs);
    ASSERT_NOT_NULL(equation);
    ASSERT_EQ_INT(equation_solve_for(equation, x, &result), 0);
    ASSERT_EQ_INT(result.status, EQUATION_SOLVE_SOLVED);
    ASSERT_EQ_INT((int)result.count, 3);
    ASSERT_TRUE(test_equation_result_contains_long(&result, 1L));

    equation_solve_result_clear(&result);
    equation_free(equation);
    expr_free(rhs);
    expr_bindings_free(bindings);
    expr_free(lhs);
}

static void test_equation_solves_cubic_repeated_root_once(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *lhs = expr_from_string("{ x^3 - 3*x + 2 | x = NAN }", &bindings);
    expr_t *rhs = test_equation_const_d(0.0);
    expr_t *x;
    equation_t *equation;
    equation_solve_result_t result;

    ASSERT_NOT_NULL(lhs);
    ASSERT_NOT_NULL(bindings);
    x = expr_bindings_get(bindings, "x");
    ASSERT_NOT_NULL(x);

    equation = equation_new(lhs, rhs);
    ASSERT_NOT_NULL(equation);
    ASSERT_EQ_INT(equation_solve_for(equation, x, &result), 0);
    ASSERT_EQ_INT(result.status, EQUATION_SOLVE_SOLVED);
    ASSERT_EQ_INT((int)result.count, 2);
    ASSERT_TRUE(test_equation_result_contains_long(&result, -2L));
    ASSERT_TRUE(test_equation_result_contains_long(&result, 1L));

    equation_solve_result_clear(&result);
    equation_free(equation);
    expr_free(rhs);
    expr_bindings_free(bindings);
    expr_free(lhs);
}

static void test_equation_solves_symbolic_cubic_cardano(void)
{
    expr_bindings_t *bindings = NULL;
    equation_t *equation = equation_from_string(
        "{ a*x^3 + b*x^2 + c*x + d = 0 | x = NAN; "
        "a = NAN, b = NAN, c = NAN, d = NAN }",
        &bindings);
    expr_t *x;
    equation_solve_result_t result;

    ASSERT_NOT_NULL(equation);
    ASSERT_NOT_NULL(bindings);
    x = expr_bindings_get(bindings, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equation_solve_for(equation, x, &result), 0);
    ASSERT_EQ_INT(result.status, EQUATION_SOLVE_SOLVED);
    ASSERT_EQ_INT((int)result.count, 3);
    ASSERT_TRUE(equation_is_solved_for(result.solutions[0], x));
    ASSERT_TRUE(equation_is_solved_for(result.solutions[1], x));
    ASSERT_TRUE(equation_is_solved_for(result.solutions[2], x));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(
        &result, style_TEX, "^{\\frac{1}{3}}"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(
        &result, style_TEX, "\\sqrt{"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(
        &result, style_TEX, "3 a c - b^{2}"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(
        &result, style_TEX, "27 a d - 9 b c"));

    equation_solve_result_clear(&result);
    expr_bindings_free(bindings);
    equation_free(equation);
}

static void test_equation_future_inverts_log(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *lhs = expr_from_string("{ ln(x) | x = NAN }", &bindings);
    expr_t *rhs = test_equation_const_d(3.0);
    expr_t *x;
    equation_t *equation;
    equation_solve_result_t result;

    ASSERT_NOT_NULL(lhs);
    ASSERT_NOT_NULL(bindings);
    x = expr_bindings_get(bindings, "x");
    ASSERT_NOT_NULL(x);

    equation = equation_new(lhs, rhs);
    ASSERT_NOT_NULL(equation);
    ASSERT_EQ_INT(equation_solve_for(equation, x, &result), 0);
    ASSERT_EQ_INT(result.status, EQUATION_SOLVE_SOLVED);
    ASSERT_EQ_INT((int)result.count, 1);
    ASSERT_TRUE(test_equation_rhs_string_equals(result.solutions[0], "exp(3)"));

    equation_solve_result_clear(&result);
    equation_free(equation);
    expr_free(rhs);
    expr_bindings_free(bindings);
    expr_free(lhs);
}

static void test_equation_future_inverts_exp(void)
{
    expr_bindings_t *bindings = NULL;
    expr_t *lhs = expr_from_string("{ exp(x) | x = NAN }", &bindings);
    expr_t *rhs = test_equation_const_d(5.0);
    expr_t *x;
    equation_t *equation;
    equation_solve_result_t result;

    ASSERT_NOT_NULL(lhs);
    ASSERT_NOT_NULL(bindings);
    x = expr_bindings_get(bindings, "x");
    ASSERT_NOT_NULL(x);

    equation = equation_new(lhs, rhs);
    ASSERT_NOT_NULL(equation);
    ASSERT_EQ_INT(equation_solve_for(equation, x, &result), 0);
    ASSERT_EQ_INT(result.status, EQUATION_SOLVE_SOLVED);
    ASSERT_EQ_INT((int)result.count, 1);
    ASSERT_TRUE(test_equation_rhs_string_equals(result.solutions[0], "ln(5)"));

    equation_solve_result_clear(&result);
    equation_free(equation);
    expr_bindings_free(bindings);
    expr_free(rhs);
    expr_free(lhs);
}

static void test_equation_basics(void)
{
    TEST_RUN_SUBTEST(test_equation_from_string_shares_symbols_across_sides, NULL);
    TEST_RUN_SUBTEST(test_equation_from_string_accepts_bare_equation, NULL);
    TEST_RUN_SUBTEST(test_equation_numeric_solves_all_variable_bindings, NULL);
    TEST_RUN_SUBTEST(test_equation_numeric_rejects_unresolved_parameter_residual, NULL);
    TEST_RUN_SUBTEST(test_equation_to_text_round_trips_through_parser, NULL);
    TEST_RUN_SUBTEST(test_equation_detects_already_solved_form, NULL);
    TEST_RUN_SUBTEST(test_equation_rejects_rhs_containing_solve_variable, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_simple_affine_equation, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_symbolic_affine_equation, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_affine_variable_from_rhs, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_symbolic_quadratic_formula, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_quadratic_two_roots, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_quadratic_zero_product, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_quadratic_double_root_once, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_cubic_three_real_roots, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_cubic_complex_pair, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_cubic_repeated_root_once, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_symbolic_cubic_cardano, NULL);
}

static void test_equation_future_solver_cases(void)
{
    TEST_RUN_SUBTEST(test_equation_future_inverts_log, "future,inverse");
    TEST_RUN_SUBTEST(test_equation_future_inverts_exp, "future,inverse");
}

int tests_main(void)
{
    TEST_SECTION("Equation Basics");
    TEST_RUN_CASE(test_equation_basics, NULL);

    TEST_SECTION("Future Solver Cases");
    TEST_RUN_CASE(test_equation_future_solver_cases, "future");

    return TEST_EXIT_CODE();
}
