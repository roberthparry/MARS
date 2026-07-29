#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diffequation.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static bool test_diffequ_expect_text(const char *label,
                                     const char *actual,
                                     const char *expected,
                                     const char *file,
                                     int line)
{
    printf("  %s\n"
           "    expected: %s\n"
           "    actual:   %s\n",
           label,
           expected ? expected : "NULL",
           actual ? actual : "NULL");
    return test_assert_cstr_eq(actual, expected, file, line);
}

static bool test_diffequ_expect_pointer(const char *label,
                                        const void *actual,
                                        bool expected_nonnull,
                                        const char *file,
                                        int line)
{
    return test_diffequ_expect_text(
        label,
        actual ? "non-NULL" : "NULL",
        expected_nonnull ? "non-NULL" : "NULL",
        file,
        line);
}

static bool test_diffequ_expect_long(const char *label,
                                     long actual,
                                     long expected,
                                     const char *file,
                                     int line)
{
    printf("  %s\n"
           "    expected: %ld\n"
           "    actual:   %ld\n",
           label,
           expected,
           actual);
    return test_assert_long_eq(actual, expected, file, line);
}

static bool test_diffequ_expect_number(const char *label,
                                       number_t actual,
                                       number_t expected,
                                       const char *file,
                                       int line)
{
    string_t *actual_text = num_to_string(actual);
    string_t *expected_text = num_to_string(expected);
    bool equal;

    printf("  %s\n"
           "    expected: %s\n"
           "    actual:   %s\n",
           label,
           expected_text ? string_c_str(expected_text) : "NULL",
           actual_text ? string_c_str(actual_text) : "NULL");
    equal = actual_text && expected_text && num_eq(actual, expected);
    string_free(expected_text);
    string_free(actual_text);
    return test_assert_true(equal, file, line, label);
}

#define EXPECT_TEXT(label, actual, expected) \
    TEST_HARNESS_RETURN_UNLESS(test_diffequ_expect_text( \
        (label), (actual), (expected), __FILE__, __LINE__))

#define EXPECT_POINTER(label, actual, expected_nonnull) \
    TEST_HARNESS_RETURN_UNLESS(test_diffequ_expect_pointer( \
        (label), (actual), (expected_nonnull), __FILE__, __LINE__))

#define EXPECT_LONG(label, actual, expected) \
    TEST_HARNESS_RETURN_UNLESS(test_diffequ_expect_long( \
        (label), (actual), (expected), __FILE__, __LINE__))

#define EXPECT_NUMBER(label, actual, expected) \
    TEST_HARNESS_RETURN_UNLESS(test_diffequ_expect_number( \
        (label), (actual), (expected), __FILE__, __LINE__))

static void test_diffequ_lifecycle_null_safety(void)
{
    diffequ_solve_result_t *invalid_result;

    EXPECT_POINTER("de_new(NULL)", de_new(NULL), false);
    EXPECT_POINTER("de_from_string(NULL)", de_from_string(NULL), false);
    EXPECT_POINTER("de_from_text(NULL)", de_from_text(NULL), false);
    EXPECT_POINTER("de_equation(NULL)", de_equation(NULL), false);
    EXPECT_LONG(
        "de_independent_count(NULL)",
        (long)de_independent_count(NULL),
        0L);
    EXPECT_POINTER(
        "de_independent_at(NULL, 0)", de_independent_at(NULL, 0u), false);
    EXPECT_POINTER("de_constants(NULL)", de_constants(NULL), false);
    EXPECT_POINTER(
        "de_constant(NULL, \"a\")", de_constant(NULL, "a"), false);
    EXPECT_LONG(
        "de_condition_count(NULL)", (long)de_condition_count(NULL), 0L);
    EXPECT_POINTER(
        "de_condition_at(NULL, 0)", de_condition_at(NULL, 0u), false);
    EXPECT_LONG(
        "de_condition_argument_count(NULL, 0)",
        (long)de_condition_argument_count(NULL, 0u),
        0L);
    EXPECT_POINTER(
        "de_condition_argument_at(NULL, 0, 0)",
        de_condition_argument_at(NULL, 0u, 0u),
        false);
    invalid_result = de_solve(NULL);
    EXPECT_POINTER("de_solve(NULL)", invalid_result, true);
    EXPECT_LONG(
        "de_solve(NULL) status",
        (long)de_solve_result_status(invalid_result),
        (long)DE_SOLVE_STATUS_INVALID);
    EXPECT_LONG(
        "de_solve_result_count(NULL)",
        (long)de_solve_result_count(NULL),
        0L);
    EXPECT_POINTER(
        "de_solve_result_at(NULL, 0)",
        de_solve_result_at(NULL, 0u),
        false);

    de_solve_result_free(invalid_result);
    de_solve_result_free(NULL);
    de_free(NULL);
}

static void test_diffequ_constructs_from_equation(void)
{
    equation_t *equation = equ_from_string("x = 1");
    diffequ_t *de;
    string_t *text;

    EXPECT_POINTER("source equation", equation, true);
    if (!equation)
        return;

    de = de_new(equation);
    EXPECT_POINTER("constructed differential equation", de, true);
    equ_free(equation);
    if (!de)
        return;

    EXPECT_POINTER("retained base equation", de_equation(de), true);
    text = de_to_text(de, style_UNBOUND);
    EXPECT_POINTER("rendered base equation", text, true);
    if (text)
        EXPECT_TEXT("rendered base equation text", string_c_str(text), "x = 1");

    string_free(text);
    de_free(de);
}

static void test_diffequ_parses_separable_ode(void)
{
    diffequ_t *de =
        de_from_string("{ Dx(y) = x*y | x = ?;; y(0) = 1 }");
    string_t *base_text;
    char *text;

    EXPECT_POINTER("parsed separable ODE", de, true);
    if (!de)
        return;

    EXPECT_POINTER("base equation", de_equation(de), true);
    EXPECT_LONG(
        "independent-variable count",
        (long)de_independent_count(de),
        1L);
    EXPECT_POINTER(
        "first independent variable", de_independent_at(de, 0u), true);
    EXPECT_LONG("condition count", (long)de_condition_count(de), 1L);
    EXPECT_POINTER("first condition", de_condition_at(de, 0u), true);

    base_text = equ_to_text(de_equation(de), style_UNBOUND);
    EXPECT_POINTER("rendered base equation", base_text, true);
    if (base_text)
        EXPECT_TEXT(
            "rendered base equation text",
            string_c_str(base_text),
            "Dx(y) = xy");

    text = de_to_string(de, style_EXPRESSION);
    EXPECT_POINTER("rendered differential equation", text, true);
    if (text)
        EXPECT_TEXT(
            "rendered differential-equation text",
            text,
            "{ Dx(y) = x*y | x = ?; ; y(0) = 1 }");

    free(text);
    string_free(base_text);
    de_free(de);
}

static void test_diffequ_parses_linear_ode_and_constant(void)
{
    diffequ_t *de = de_from_string(
        "{ Dx(y) + a*y = x | x = ?; a = 2; y(0) = 1 }");
    expr_t *constant;
    number_t value;

    EXPECT_POINTER("parsed linear ODE", de, true);
    if (!de)
        return;

    constant = de_constant(de, "a");
    EXPECT_POINTER("constant a", constant, true);
    if (constant) {
        number_t expected = num_create_from_long(2L);

        value = expr_eval(constant);
        EXPECT_NUMBER("constant a value", value, expected);
        num_destroy(&value);
        num_destroy(&expected);
    }

    de_free(de);
}

static void test_diffequ_expression_text_round_trips(void)
{
    diffequ_t *first = de_from_string(
        "{ Dxx(y) + 3*Dx(y) + 2*y = 0 | x = ?;; y(0) = 1, Dx(y)(0) = 0 }");
    diffequ_t *second = NULL;
    char *first_text = NULL;
    char *second_text = NULL;

    EXPECT_POINTER("first parse", first, true);
    if (!first)
        return;

    first_text = de_to_string(first, style_EXPRESSION);
    EXPECT_POINTER("first rendered text", first_text, true);
    if (first_text)
        second = de_from_string(first_text);
    EXPECT_POINTER("second parse", second, true);
    if (second)
        second_text = de_to_string(second, style_EXPRESSION);
    EXPECT_POINTER("second rendered text", second_text, true);
    if (first_text && second_text)
        EXPECT_TEXT("round-trip text", second_text, first_text);

    free(second_text);
    free(first_text);
    de_free(second);
    de_free(first);
}

static void test_diffequ_parses_ode_shorthand(void)
{
    diffequ_t *de =
        de_from_string("Dxx(y) = y; y(0) = 1; y'(0) = 1");
    char *text;

    EXPECT_POINTER("parsed shorthand ODE", de, true);
    if (!de)
        return;

    EXPECT_LONG(
        "inferred independent-variable count",
        (long)de_independent_count(de),
        1L);
    EXPECT_LONG(
        "shorthand condition count", (long)de_condition_count(de), 2L);

    text = de_to_string(de, style_EXPRESSION);
    EXPECT_POINTER("normalized shorthand text", text, true);
    if (text)
        EXPECT_TEXT(
            "normalized shorthand",
            text,
            "{ Dxx(y) = y | x = ?; ; y(0) = 1, Dx(y)(0) = 1 }");

    free(text);
    de_free(de);
}

static void test_diffequ_parses_and_solves_prime_ode_shorthand(void)
{
    const char *source = "y'' + 4y = 0";
    diffequ_t *de = de_from_string(source);
    diffequ_t *explicit =
        de_from_string("{ y'' + 4y = 0 | t = ?;; }");
    diffequ_t *forced = de_from_string("y'' + 4y = e^x");
    diffequ_t *time_dependent = de_from_string("x'' + x = 0");
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    diffequ_solve_result_t *explicit_result =
        explicit ? de_solve(explicit) : NULL;
    diffequ_solve_result_t *forced_result =
        forced ? de_solve(forced) : NULL;
    diffequ_solve_result_t *time_dependent_result =
        time_dependent ? de_solve(time_dependent) : NULL;
    const equation_t *solution =
        result ? de_solve_result_at(result, 0u) : NULL;
    const equation_t *explicit_solution = explicit_result
        ? de_solve_result_at(explicit_result, 0u)
        : NULL;
    const equation_t *forced_solution = forced_result
        ? de_solve_result_at(forced_result, 0u)
        : NULL;
    const equation_t *time_dependent_solution = time_dependent_result
        ? de_solve_result_at(time_dependent_result, 0u)
        : NULL;
    string_t *equation_text =
        de ? equ_to_text(de_equation(de), style_UNBOUND) : NULL;
    string_t *solution_text =
        solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    string_t *explicit_equation_text = explicit
        ? equ_to_text(de_equation(explicit), style_UNBOUND)
        : NULL;
    string_t *explicit_solution_text = explicit_solution
        ? equ_to_text(explicit_solution, style_UNBOUND)
        : NULL;
    string_t *forced_equation_text = forced
        ? equ_to_text(de_equation(forced), style_UNBOUND)
        : NULL;
    string_t *forced_solution_text = forced_solution
        ? equ_to_text(forced_solution, style_UNBOUND)
        : NULL;
    string_t *time_dependent_equation_text = time_dependent
        ? equ_to_text(de_equation(time_dependent), style_UNBOUND)
        : NULL;
    string_t *time_dependent_solution_text = time_dependent_solution
        ? equ_to_text(time_dependent_solution, style_UNBOUND)
        : NULL;

    EXPECT_POINTER("parsed prime-notation ODE", de, true);
    EXPECT_TEXT(
        "normalized prime-notation ODE",
        equation_text ? string_c_str(equation_text) : NULL,
        "Dxx(y) + 4y = 0");
    EXPECT_LONG(
        "prime-notation solve status",
        result ? (long)de_solve_result_status(result) : -1L,
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_TEXT(
        "prime-notation solution",
        solution_text ? string_c_str(solution_text) : NULL,
        "y = C₁·cos(2x) + C₂·sin(2x)");
    EXPECT_TEXT(
        "prime notation uses the declared independent variable",
        explicit_equation_text ? string_c_str(explicit_equation_text) : NULL,
        "Dtt(y) + 4y = 0");
    EXPECT_TEXT(
        "declared-variable prime-notation solution",
        explicit_solution_text ? string_c_str(explicit_solution_text) : NULL,
        "y = C₁·cos(2t) + C₂·sin(2t)");
    EXPECT_TEXT(
        "prime notation accepts the standard constant e",
        forced_equation_text ? string_c_str(forced_equation_text) : NULL,
        "Dxx(y) + 4y = exp(x)");
    EXPECT_LONG(
        "exponential-forcing solve status",
        forced_result ? (long)de_solve_result_status(forced_result) : -1L,
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_TEXT(
        "compact exponential-forcing solution",
        forced_solution_text ? string_c_str(forced_solution_text) : NULL,
        "y = ⅕·exp(x) + C₁·cos(2x) + C₂·sin(2x)");
    EXPECT_TEXT(
        "prime x defaults to differentiation with respect to t",
        time_dependent_equation_text
            ? string_c_str(time_dependent_equation_text)
            : NULL,
        "Dtt(x) + x = 0");
    EXPECT_TEXT(
        "time-dependent prime-notation solution",
        time_dependent_solution_text
            ? string_c_str(time_dependent_solution_text)
            : NULL,
        "x = C₁·cos(t) + C₂·sin(t)");

    string_free(time_dependent_solution_text);
    string_free(time_dependent_equation_text);
    string_free(forced_solution_text);
    string_free(forced_equation_text);
    string_free(explicit_solution_text);
    string_free(explicit_equation_text);
    string_free(solution_text);
    string_free(equation_text);
    de_solve_result_free(time_dependent_result);
    de_solve_result_free(forced_result);
    de_solve_result_free(explicit_result);
    de_solve_result_free(result);
    de_free(time_dependent);
    de_free(forced);
    de_free(explicit);
    de_free(de);
}

static void test_diffequ_rejects_noncanonical_text(void)
{
    diffequ_t *missing_derivative = de_from_string("y = 1");
    diffequ_t *missing_section =
        de_from_string("{ Dx(y) = y | x = ?; }");

    EXPECT_POINTER(
        "shorthand without a derivative", missing_derivative, false);
    EXPECT_POINTER(
        "explicit form missing a section", missing_section, false);
    de_free(missing_section);
    de_free(missing_derivative);
}

static void test_diffequ_solves_separable_initial_value_problem(void)
{
    diffequ_t *de =
        de_from_string("Dx(y) = x*y; y(0) = 1");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER("parsed separable initial-value problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("separable solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG(
        "separable solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG(
        "selected solver",
        (long)de_solve_result_solver(result),
        (long)DE_SOLVER_SEPARABLE);
    EXPECT_LONG(
        "separable solution count",
        (long)de_solve_result_count(result),
        1L);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("separable solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("separable solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "separable solution",
            string_c_str(text),
            "y = exp(½x²)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_linear_initial_value_problem(void)
{
    diffequ_t *de =
        de_from_string("Dx(y) + y = x; y(0) = 0");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER("parsed linear initial-value problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("linear solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG(
        "linear solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG(
        "selected solver",
        (long)de_solve_result_solver(result),
        (long)DE_SOLVER_LINEAR);
    EXPECT_LONG(
        "linear solution count",
        (long)de_solve_result_count(result),
        1L);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("linear solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("linear solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "linear solution",
            string_c_str(text),
            "y = ((x - 1)·exp(x) + 1)/exp(x)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_quadratic_separable_problem(void)
{
    diffequ_t *de =
        de_from_string("Dx(y) = x*y^2; y(0) = 1");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER("parsed quadratic separable problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("quadratic separable solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG(
        "quadratic separable solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG(
        "selected solver",
        (long)de_solve_result_solver(result),
        (long)DE_SOLVER_SEPARABLE);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("quadratic separable solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("quadratic separable solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "quadratic separable solution",
            string_c_str(text),
            "y = -1/(½x² - 1)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_variable_coefficient_linear_problem(void)
{
    diffequ_t *de = de_from_string(
        "Dx(y) + 2*x*y = exp(-x^2); y(0) = 0");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER(
        "parsed variable-coefficient linear problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER(
        "variable-coefficient linear solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG(
        "variable-coefficient linear solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG(
        "selected solver",
        (long)de_solve_result_solver(result),
        (long)DE_SOLVER_LINEAR);
    EXPECT_TEXT(
        "linear solver diagnostic",
        de_solve_result_diagnostic(result),
        "solved as a first-order linear ODE");
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER(
        "variable-coefficient linear solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER(
        "variable-coefficient linear solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "variable-coefficient linear solution",
            string_c_str(text),
            "y = x/exp(x²)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_rational_integrating_factor_problem(void)
{
    diffequ_t *de =
        de_from_string("Dx(y) + y/x = x^2; y(1) = 1");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER(
        "parsed rational integrating-factor problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER(
        "rational integrating-factor solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG(
        "rational integrating-factor solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG(
        "selected solver",
        (long)de_solve_result_solver(result),
        (long)DE_SOLVER_LINEAR);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER(
        "rational integrating-factor solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER(
        "rational integrating-factor solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "rational integrating-factor solution",
            string_c_str(text),
            "y = (x⁴ + 3)/(4x)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_linear_solution_retains_arbitrary_constant(void)
{
    diffequ_t *de = de_from_string("Dx(y) + y = x");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER("parsed unconditioned linear ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("unconditioned linear solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG(
        "unconditioned linear solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG(
        "selected solver",
        (long)de_solve_result_solver(result),
        (long)DE_SOLVER_LINEAR);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("unconditioned linear solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("unconditioned linear solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "unconditioned linear solution",
            string_c_str(text),
            "y = (C + (x - 1)·exp(x))/exp(x)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_linear_solution_uses_special_function(void)
{
    diffequ_t *de =
        de_from_string("Dx(y) + 2*x*y = 1; y(0) = 0");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER("parsed non-elementary linear ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("non-elementary linear solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG(
        "non-elementary linear solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG(
        "selected solver",
        (long)de_solve_result_solver(result),
        (long)DE_SOLVER_LINEAR);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("special-function linear solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("special-function solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "special-function linear solution",
            string_c_str(text),
            "y = ½·√(π)·(erf(x·√(-1)) - erf(0))/(√(-1)·exp(x²))");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_linear_solution_retains_formal_integral(void)
{
    diffequ_t *de = de_from_string(
        "Dx(y) + y = exp(cosh(x)); y(0) = 0");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER("parsed formal-integral linear ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("formal-integral linear solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG(
        "formal-integral linear solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG(
        "selected solver",
        (long)de_solve_result_solver(result),
        (long)DE_SOLVER_LINEAR);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("formal-integral linear solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("formal-integral solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "formal-integral linear solution",
            string_c_str(text),
            "y = ∫^x exp(cosh(t) + t)·dt/exp(x)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_linear_solution_retains_formal_factor(void)
{
    diffequ_t *de = de_from_string(
        "Dx(y) + exp(cosh(x))*y = 0; y(0) = 1");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER("parsed formal-factor linear ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("formal-factor linear solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG(
        "formal-factor linear solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG(
        "selected solver",
        (long)de_solve_result_solver(result),
        (long)DE_SOLVER_LINEAR);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("formal-factor linear solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("formal-factor solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "formal-factor linear solution",
            string_c_str(text),
            "y = 1/exp(∫^x exp(cosh(t))·dt)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_first_order_homogeneous_problem(void)
{
    diffequ_t *de = de_from_string(
        "Dx(y) = y/x + x/y; y(1) = 1");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER("parsed homogeneous ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("homogeneous solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG(
        "homogeneous solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG(
        "selected solver",
        (long)de_solve_result_solver(result),
        (long)DE_SOLVER_HOMOGENEOUS);
    EXPECT_TEXT(
        "homogeneous solver diagnostic",
        de_solve_result_diagnostic(result),
        "solved as a first-order homogeneous ODE");
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("homogeneous solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("homogeneous solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "homogeneous solution",
            string_c_str(text),
            "½·(y/x)² = ln(|x|) + 0.5");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_homogeneous_solution_retains_constant(void)
{
    diffequ_t *de =
        de_from_string("Dx(y) = y/x + x/y");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER("parsed unconditioned homogeneous ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("unconditioned homogeneous solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG(
        "unconditioned homogeneous solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG(
        "selected solver",
        (long)de_solve_result_solver(result),
        (long)DE_SOLVER_HOMOGENEOUS);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("unconditioned homogeneous solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER(
        "unconditioned homogeneous solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "unconditioned homogeneous solution",
            string_c_str(text),
            "½·(y/x)² = ln(|x|) + C");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_affine_combination_substitution(void)
{
    diffequ_t *de =
        de_from_string("Dx(y) = (x + y)^2; y(0) = 1");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER("parsed affine-substitution ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("affine-substitution solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG(
        "affine-substitution solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG(
        "selected solver",
        (long)de_solve_result_solver(result),
        (long)DE_SOLVER_LINEAR_SUBSTITUTION);
    EXPECT_TEXT(
        "linear-substitution solver diagnostic",
        de_solve_result_diagnostic(result),
        "solved by a first-order linear substitution");
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("affine-substitution solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("affine-substitution solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "affine-substitution solution",
            string_c_str(text),
            "atan(x + y) = x + π/4");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_shifted_homogeneous_substitution(void)
{
    diffequ_t *de = de_from_string(
        "Dx(y) = ((y - 1)/(x + 2))^2 + "
        "(y - 1)/(x + 2); y(0) = 3");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER("parsed shifted-homogeneous ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("shifted-homogeneous solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG(
        "shifted-homogeneous solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG(
        "selected solver",
        (long)de_solve_result_solver(result),
        (long)DE_SOLVER_LINEAR_SUBSTITUTION);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("shifted-homogeneous solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("shifted-homogeneous solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "shifted-homogeneous solution",
            string_c_str(text),
            "-(x + 2)/(y - 1) = ln(|x + 2|) - ln(2) - 1");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_linear_change_of_variables(void)
{
    diffequ_t *de = de_from_string(
        "Dx(y) = (1 - (x + y)*exp(x - y))/"
        "(1 + (x + y)*exp(x - y)); y(0) = 0");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER("parsed linear-transformation ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("linear-transformation solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG(
        "linear-transformation solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG(
        "selected solver",
        (long)de_solve_result_solver(result),
        (long)DE_SOLVER_LINEAR_TRANSFORMATION);
    EXPECT_TEXT(
        "linear-transformation solver diagnostic",
        de_solve_result_diagnostic(result),
        "solved by a linear change of variables");
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("linear-transformation solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("linear-transformation solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "linear-transformation solution",
            string_c_str(text),
            "½·(x + y)² = 1 - exp(-(x - y))");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_retains_arbitrary_constant(void)
{
    diffequ_t *de = de_from_string("Dx(y) = x*y");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER("parsed unconditioned ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("unconditioned solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG(
        "unconditioned solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("unconditioned solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("unconditioned solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "arbitrary integration constant",
            string_c_str(text),
            "y = C·exp(½x²)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_preserves_zero_singular_solution(void)
{
    diffequ_t *de =
        de_from_string("Dx(y) = x*y^2; y(0) = 0");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER("parsed zero initial-value problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("zero initial-value solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG(
        "zero initial-value solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("zero singular solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("zero singular solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "zero singular solution",
            string_c_str(text),
            "y = 0");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_quadratic_bernoulli_problem(void)
{
    diffequ_t *de =
        de_from_string("Dx(y) + y = x*y^2; y(0) = 1");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER("parsed Bernoulli initial-value problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("Bernoulli solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG(
        "Bernoulli solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG(
        "selected solver",
        (long)de_solve_result_solver(result),
        (long)DE_SOLVER_BERNOULLI);
    EXPECT_LONG(
        "Bernoulli solution count",
        (long)de_solve_result_count(result),
        1L);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("Bernoulli solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("Bernoulli solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "Bernoulli solution",
            string_c_str(text),
            "y = 1/(x + 1)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_derivative_quadratic_problem(void)
{
    static const char *expected[] = {
        "x = ½·(√(8y + 1) - "
        "ln(|½·(√(8y + 1) + 1)|) + 1) + C",
        "x = ½·(1 - √(8y + 1) - "
        "ln(|½·(1 - √(8y + 1))|)) + C",
        "y = 0"
    };
    diffequ_t *de =
        de_from_string("(y')^2 = y' + 2y");
    diffequ_solve_result_t *result;

    EXPECT_POINTER("parsed derivative-quadratic problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("derivative-quadratic solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG(
        "derivative-quadratic solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG(
        "derivative-quadratic selected solver",
        (long)de_solve_result_solver(result),
        (long)DE_SOLVER_DERIVATIVE_QUADRATIC);
    EXPECT_LONG(
        "derivative-quadratic solution count",
        (long)de_solve_result_count(result),
        3L);

    for (size_t i = 0u; i < 3u; ++i) {
        const equation_t *solution =
            de_solve_result_at(result, i);
        string_t *text =
            solution ? equ_to_text(solution, style_UNBOUND) : NULL;

        EXPECT_POINTER(
            "derivative-quadratic solution", solution, true);
        EXPECT_TEXT(
            "derivative-quadratic solution text",
            text ? string_c_str(text) : NULL,
            expected[i]);
        string_free(text);
    }

    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_linearizes_exact_third_order_problem(void)
{
    static const char *expected[] = {
        "y = 2·ln(|Σ_(n=0)^∞ c_(n)·x^n|)",
        "c_(0) = C₂",
        "c_(1) = C₃",
        "c_(-1) = 0",
        "c_(-2) = 0",
        "c_(-3) = 0",
        "c_(n + 2) = (C₁·c_(n) + c_(n - 3))/(2·(n + 2)·(n + 1))"
    };
    diffequ_t *de =
        de_from_string("y''' + y''*y' = 3x^2");
    diffequ_solve_result_t *result;

    EXPECT_POINTER("parsed exact third-order problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("exact third-order solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG(
        "exact third-order solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG(
        "exact third-order selected solver",
        (long)de_solve_result_solver(result),
        (long)DE_SOLVER_EXACT_DERIVATIVE_LINEARIZATION);
    EXPECT_TEXT(
        "exact third-order solver diagnostic",
        de_solve_result_diagnostic(result),
        "linearized exactly, then solved by a convergent "
        "power-series recurrence");
    EXPECT_LONG(
        "exact third-order solution count",
        (long)de_solve_result_count(result),
        7L);

    for (size_t i = 0u; i < 7u; ++i) {
        const equation_t *solution =
            de_solve_result_at(result, i);
        string_t *text =
            solution ? equ_to_text(solution, style_UNBOUND) : NULL;

        EXPECT_POINTER(
            "exact third-order solution", solution, true);
        EXPECT_TEXT(
            "exact third-order solution text",
            text ? string_c_str(text) : NULL,
            expected[i]);
        string_free(text);
    }

    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_second_order_sturm_liouville_problem(void)
{
    diffequ_t *de =
        de_from_string("Dxx(y) = y; y(0) = 1; y'(0) = 1");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER("parsed second-order problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("Sturm-Liouville solve result", result, true);
    if (result) {
        EXPECT_LONG(
            "Sturm-Liouville solve status",
            (long)de_solve_result_status(result),
            (long)DE_SOLVE_STATUS_SOLVED);
        EXPECT_LONG(
            "selected solver",
            (long)de_solve_result_solver(result),
            (long)DE_SOLVER_STURM_LIOUVILLE);
        EXPECT_LONG(
            "Sturm-Liouville solution count",
            (long)de_solve_result_count(result),
            1L);
        EXPECT_TEXT(
            "Sturm-Liouville diagnostic",
            de_solve_result_diagnostic(result),
            "solved as a second-order linear Sturm-Liouville equation");
    }
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("Sturm-Liouville solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("Sturm-Liouville solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "Sturm-Liouville solution",
            string_c_str(text),
            "y = exp(x)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_does_not_invent_cubic_potential_functions(void)
{
    diffequ_t *de = de_from_string(
        "y'' = 1/2*(x^3+a)*y; y(0) = 1; y'(0) = 0");
    diffequ_solve_result_t *result;

    EXPECT_POINTER("parsed cubic-potential IVP", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("cubic-potential solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG(
        "cubic-potential solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_UNSUPPORTED);
    EXPECT_LONG(
        "cubic-potential selected solver",
        (long)de_solve_result_solver(result),
        (long)DE_SOLVER_NONE);
    EXPECT_LONG(
        "cubic-potential solution count",
        (long)de_solve_result_count(result),
        0L);

    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_repeated_characteristic_root(void)
{
    diffequ_t *de = de_from_string(
        "Dxx(y) + 2*Dx(y) + y = 0; y(0) = 1; y'(0) = 0");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER("parsed repeated-root problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("repeated-root solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }
    EXPECT_LONG(
        "repeated-root solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG(
        "repeated-root selected solver",
        (long)de_solve_result_solver(result),
        (long)DE_SOLVER_STURM_LIOUVILLE);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("repeated-root solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("repeated-root solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "repeated-root solution",
            string_c_str(text),
            "y = (x + 1)·exp(-x)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_oscillatory_sturm_liouville_problem(void)
{
    diffequ_t *de =
        de_from_string("Dxx(y) + y = 0; y(0) = 0; y'(0) = 1");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER("parsed oscillatory problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("oscillatory solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }
    EXPECT_LONG(
        "oscillatory solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG(
        "oscillatory selected solver",
        (long)de_solve_result_solver(result),
        (long)DE_SOLVER_STURM_LIOUVILLE);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("oscillatory solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("oscillatory solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "oscillatory solution",
            string_c_str(text),
            "y = sin(x)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_normalizes_variable_coefficient_sturm_liouville(void)
{
    diffequ_t *de =
        de_from_string("x*Dxx(y) + Dx(y) + y = 0");
    diffequ_solve_result_t *result;

    EXPECT_POINTER("parsed variable-coefficient problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("variable-coefficient solve result", result, true);
    if (result) {
        EXPECT_LONG(
            "variable-coefficient solve status",
            (long)de_solve_result_status(result),
            (long)DE_SOLVE_STATUS_UNSUPPORTED);
        EXPECT_LONG(
            "variable-coefficient solution count",
            (long)de_solve_result_count(result),
            0L);
        EXPECT_TEXT(
            "variable-coefficient diagnostic",
            de_solve_result_diagnostic(result),
            "the second-order linear equation has no supported "
            "closed-form basis");
    }

    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_third_order_constant_coefficient_problem(void)
{
    const char *source =
        "Dxxx(y) - Dx(y) = 0; "
        "y(0) = 1; y'(0) = 1; y''(0) = 1";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    printf("  differential equation\n    input:    %s\n", source);
    EXPECT_POINTER("parsed third-order problem", de, true);
    if (!de)
        return;
    result = de_solve(de);
    EXPECT_POINTER("third-order solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }
    EXPECT_LONG(
        "third-order solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG(
        "third-order selected solver",
        (long)de_solve_result_solver(result),
        (long)DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("third-order solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("third-order solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "third-order solution",
            string_c_str(text),
            "y = exp(x)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_high_order_repeated_root(void)
{
    const char *source =
        "Dxxx(y) - 3*Dxx(y) + 3*Dx(y) - y = 0; "
        "y(0) = 1; y'(0) = 1; y''(0) = 1";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    printf("  differential equation\n    input:    %s\n", source);
    EXPECT_POINTER("parsed high-order repeated-root problem", de, true);
    if (!de)
        return;
    result = de_solve(de);
    EXPECT_POINTER("high-order repeated-root result", result, true);
    if (!result) {
        de_free(de);
        return;
    }
    EXPECT_LONG(
        "high-order repeated-root status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("high-order repeated-root solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("high-order repeated-root solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "high-order repeated-root solution",
            string_c_str(text),
            "y = exp(x)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_nonhomogeneous_constant_coefficient_problem(
    void)
{
    const char *source =
        "Dxx(y) - y = exp(2*x); y(0) = 0; y'(0) = 0";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    printf("  differential equation\n    input:    %s\n", source);
    EXPECT_POINTER("parsed nonhomogeneous second-order problem", de, true);
    if (!de)
        return;
    result = de_solve(de);
    EXPECT_POINTER("nonhomogeneous solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }
    EXPECT_LONG(
        "nonhomogeneous solve status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG(
        "nonhomogeneous selected solver",
        (long)de_solve_result_solver(result),
        (long)DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("nonhomogeneous solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("nonhomogeneous solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "nonhomogeneous solution",
            string_c_str(text),
            "y = ⅙·(2·exp(2x) - 3·exp(x) + exp(-x))");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_repeated_complex_roots(void)
{
    const char *source =
        "Dxxxx(y) + 2*Dxx(y) + y = 0; "
        "y(0) = 1; Dx(y)(0) = 0; "
        "Dxx(y)(0) = -1; Dxxx(y)(0) = 0";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    printf("  differential equation\n    input:    %s\n", source);
    EXPECT_POINTER("parsed repeated-complex-root problem", de, true);
    if (!de)
        return;
    result = de_solve(de);
    EXPECT_POINTER("repeated-complex-root result", result, true);
    if (!result) {
        de_free(de);
        return;
    }
    EXPECT_LONG(
        "repeated-complex-root status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("repeated-complex-root solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("repeated-complex-root solution text", text, true);
    if (text)
        EXPECT_TEXT(
            "repeated-complex-root solution",
            string_c_str(text),
            "y = cos(x)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_degree_six_characteristic_polynomial(void)
{
    diffequ_t *de = de_from_string(
        "Dxxxxxx(y) - 4*Dxxxx(y) - Dxx(y) + 4*y = 0");
    diffequ_solve_result_t *result;

    EXPECT_POINTER("parsed sixth-order problem", de, true);
    if (!de)
        return;
    result = de_solve(de);
    EXPECT_POINTER("sixth-order solve result", result, true);
    if (result) {
        EXPECT_LONG(
            "sixth-order solve status",
            (long)de_solve_result_status(result),
            (long)DE_SOLVE_STATUS_SOLVED);
        EXPECT_LONG(
            "sixth-order selected solver",
            (long)de_solve_result_solver(result),
            (long)DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR);
        EXPECT_LONG(
            "sixth-order solution count",
            (long)de_solve_result_count(result),
            1L);
    }

    de_solve_result_free(result);
    de_free(de);
}

static bool test_diffequ_expect_constant_linear_solution(
    const char *source,
    const char *expected,
    const char *file,
    int line)
{
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution =
        result ? de_solve_result_at(result, 0u) : NULL;
    string_t *text =
        solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    const char *actual = text ? string_c_str(text) : NULL;
    bool valid =
        de &&
        result &&
        de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
        de_solve_result_solver(result) ==
            DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR &&
        de_solve_result_count(result) == 1u &&
        actual &&
        strcmp(actual, expected) == 0;

    printf("  differential equation\n"
           "    input:    %s\n"
           "    expected: %s\n"
           "    actual:   %s\n",
           source,
           expected,
           actual ? actual : "NULL");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
    return test_assert_true(
        valid, file, line, "constant-coefficient linear solution");
}

#define EXPECT_CONSTANT_LINEAR_SOLUTION(source, expected) \
    TEST_HARNESS_RETURN_UNLESS( \
        test_diffequ_expect_constant_linear_solution( \
            (source), (expected), __FILE__, __LINE__))

static void test_diffequ_general_solution_with_distinct_real_roots(void)
{
    EXPECT_CONSTANT_LINEAR_SOLUTION(
        "Dxxx(y) - 6*Dxx(y) + 11*Dx(y) - 6*y = 0",
        "y = C₁·exp(3x) + C₂·exp(2x) + C₃·exp(x)");
}

static void test_diffequ_general_solution_with_repeated_real_root(void)
{
    EXPECT_CONSTANT_LINEAR_SOLUTION(
        "Dxxx(y) - 3*Dxx(y) + 3*Dx(y) - y = 0",
        "y = exp(x)·(C₃x² + C₂x + C₁)");
}

static void test_diffequ_general_solution_with_real_and_complex_roots(void)
{
    EXPECT_CONSTANT_LINEAR_SOLUTION(
        "Dxxx(y) - Dxx(y) + Dx(y) - y = 0",
        "y = C₁·exp(x) + C₂·cos(x) + C₃·sin(x)");
}

static void test_diffequ_general_solution_with_repeated_complex_roots(void)
{
    EXPECT_CONSTANT_LINEAR_SOLUTION(
        "Dxxxx(y) + 2*Dxx(y) + y = 0",
        "y = C₁·cos(x) + C₂·sin(x) + C₃x·cos(x) + C₄x·sin(x)");
}

static void test_diffequ_general_sixth_order_solution(void)
{
    EXPECT_CONSTANT_LINEAR_SOLUTION(
        "Dxxxxxx(y) - 4*Dxxxx(y) - Dxx(y) + 4*y = 0",
        "y = C₁·exp(x) + C₂·exp(2x) + C₃·exp(-x) + C₄·exp(-2x) + "
        "C₅·cos(x) + C₆·sin(x)");
}

static void test_diffequ_general_nonhomogeneous_solution(void)
{
    EXPECT_CONSTANT_LINEAR_SOLUTION(
        "Dxx(y) - y = exp(2*x)",
        "y = ⅓·exp(2x) + C₁·exp(x) + C₂·exp(-x)");
    EXPECT_CONSTANT_LINEAR_SOLUTION(
        "y'' + 4y = e^x + x^3",
        "y = ⅕·exp(x) + ¼x³ - ⅜x + "
        "C₁·cos(2x) + C₂·sin(2x)");
}

static void test_diffequ_general_trigonometric_forcing_solution(void)
{
    EXPECT_CONSTANT_LINEAR_SOLUTION(
        "Dxx(y) + y = cos(2*x)",
        "y = ⅙·(sin(x)·(sin(3x) - 3·sin(-x)) - "
        "cos(x)·(3·cos(-x) - cos(3x))) + "
        "C₁·cos(x) + C₂·sin(x)");
}

static void test_diffequ_general_third_order_forced_solution(void)
{
    EXPECT_CONSTANT_LINEAR_SOLUTION(
        "Dxxx(y) - Dx(y) = exp(2*x)",
        "y = ⅙·exp(2x) + C₁·exp(x) + C₂ + C₃·exp(-x)");
}

static bool test_diffequ_expect_transport_solution(
    const char *source,
    const char *expected,
    de_solver_t expected_solver,
    const char *file,
    int line)
{
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution =
        result ? de_solve_result_at(result, 0u) : NULL;
    string_t *text =
        solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    const char *actual = text ? string_c_str(text) : NULL;
    bool valid =
        de &&
        result &&
        de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
        de_solve_result_solver(result) == expected_solver &&
        de_solve_result_count(result) == 1u &&
        actual &&
        strcmp(actual, expected) == 0;

    printf("  partial differential equation\n"
           "    input:    %s\n"
           "    expected: %s\n"
           "    actual:   %s\n",
           source,
           expected,
           actual ? actual : "NULL");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
    return test_assert_true(
        valid, file, line, "constant-coefficient transport solution");
}

#define EXPECT_TRANSPORT_SOLUTION(source, expected) \
    TEST_HARNESS_RETURN_UNLESS( \
        test_diffequ_expect_transport_solution( \
            (source), \
            (expected), \
            DE_SOLVER_CONSTANT_COEFFICIENT_TRANSPORT, \
            __FILE__, \
            __LINE__))

#define EXPECT_CHARACTERISTIC_SOLUTION(source, expected) \
    TEST_HARNESS_RETURN_UNLESS( \
        test_diffequ_expect_transport_solution( \
            (source), \
            (expected), \
            DE_SOLVER_CHARACTERISTICS, \
            __FILE__, \
            __LINE__))

static void test_diffequ_parses_pde_boundary_arguments(void)
{
    const char *source =
        "{ 2*Dx(u) + Dy(u) = 0 | x = ?, y = ?;; "
        "u(x, 0) = x^2 }";
    const char *expected_problem =
        "{ 2*Dx(u) + Dy(u) = 0 | x = ?, y = ?; ; "
        "u(x, 0) = x^2 }";
    diffequ_t *de = de_from_string(source);
    char *problem = de ? de_to_string(de, style_EXPRESSION) : NULL;
    const expr_t *first =
        de ? de_condition_argument_at(de, 0u, 0u) : NULL;
    const expr_t *second =
        de ? de_condition_argument_at(de, 0u, 1u) : NULL;
    number_t second_value = second ? expr_eval(second) : num_new();
    number_t zero = num_create_from_long(0L);

    EXPECT_POINTER("parsed PDE", de, true);
    EXPECT_LONG(
        "PDE independent-variable count",
        (long)de_independent_count(de),
        2L);
    EXPECT_LONG(
        "PDE boundary-argument count",
        (long)de_condition_argument_count(de, 0u),
        2L);
    EXPECT_TEXT(
        "PDE first boundary argument shares x",
        first == de_independent_at(de, 0u) ? "same" : "different",
        "same");
    EXPECT_NUMBER("PDE second boundary argument", second_value, zero);
    EXPECT_TEXT("PDE canonical problem", problem, expected_problem);

    num_destroy(&zero);
    num_destroy(&second_value);
    free(problem);
    de_free(de);
}

static void test_diffequ_solves_constant_transport_from_y_boundary(void)
{
    EXPECT_TRANSPORT_SOLUTION(
        "{ 2*Dx(u) + Dy(u) = 0 | x = ?, y = ?;; "
        "u(x, 0) = x^2 }",
        "u = (x - 2y)²");
}

static void test_diffequ_solves_constant_transport_from_x_boundary(void)
{
    EXPECT_TRANSPORT_SOLUTION(
        "{ Dx(u) + 3*Dy(u) = 0 | x = ?, y = ?;; "
        "u(0, y) = exp(y) }",
        "u = exp(y - 3x)");
}

static void test_diffequ_solves_unbounded_homogeneous_transport(void)
{
    EXPECT_TRANSPORT_SOLUTION(
        "Dt(u) + c*Dx(u) = 0",
        "u = F(x - ct)");
}

static void test_diffequ_solves_unbounded_inhomogeneous_transport(void)
{
    EXPECT_TRANSPORT_SOLUTION(
        "Dt(u) + c*Dx(u) = 1",
        "u = F(x - ct) + t");
}

static void test_diffequ_solves_nonlinear_characteristic_pde(void)
{
    diffequ_t *de =
        de_from_string("Dx(z) + Dy(z) = 6*(x+y)^2*z^2");
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *general =
        result ? de_solve_result_at(result, 0u) : NULL;
    const equation_t *singular =
        result ? de_solve_result_at(result, 1u) : NULL;
    string_t *general_text =
        general ? equ_to_text(general, style_UNBOUND) : NULL;
    string_t *singular_text =
        singular ? equ_to_text(singular, style_UNBOUND) : NULL;

    EXPECT_LONG(
        "nonlinear characteristic status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG(
        "nonlinear characteristic solution count",
        (long)de_solve_result_count(result),
        2L);
    EXPECT_TEXT(
        "nonlinear characteristic general solution",
        general_text ? string_c_str(general_text) : NULL,
        "z = 1/(F(x - y) - (x + y)³)");
    EXPECT_TEXT(
        "nonlinear characteristic singular solution",
        singular_text ? string_c_str(singular_text) : NULL,
        "z = 0");

    string_free(singular_text);
    string_free(general_text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_rotating_characteristic_pde(void)
{
    EXPECT_CHARACTERISTIC_SOLUTION(
        "(x+y)*Dx(z) + (y-x)*Dy(z) = 0",
        "z = F(½·(ln(x² + y²) + 2·atan2(y, x)))");
}

static void test_diffequ_solves_parameter_linear_pde(void)
{
    const char *source = "Dy(z) + 2*y*z = x*y^3";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution =
        result ? de_solve_result_at(result, 0u) : NULL;
    string_t *text =
        solution ? equ_to_text(solution, style_UNBOUND) : NULL;

    printf("  parameter-dependent linear PDE\n"
           "    input:    %s\n"
           "    expected: %s\n"
           "    actual:   %s\n",
           source,
           "z = ½x·(y² - 1) + F(x)·exp(-y²)",
           text ? string_c_str(text) : "NULL");
    EXPECT_LONG(
        "parameter-linear PDE status",
        (long)de_solve_result_status(result),
        (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG(
        "parameter-linear PDE solver",
        (long)de_solve_result_solver(result),
        (long)DE_SOLVER_PARAMETER_LINEAR_PDE);
    EXPECT_TEXT(
        "parameter-linear PDE solution",
        text ? string_c_str(text) : NULL,
        "z = ½x·(y² - 1) + F(x)·exp(-y²)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

/*
 * Keep this example synchronized with docs/diffequation.md.
 */
static void example_diffequation_solving_an_ode(void)
{
    const char *source = "Dx(y) = x*y; y(0) = 1";
    const char *expected_problem =
        "{ Dx(y) = x*y | x = ?; ; y(0) = 1 }";
    const char *expected_solution = "y = exp(½x²)";
    diffequ_t *ode = de_from_string(source);
    diffequ_solve_result_t *result = de_solve(ode);
    const equation_t *solution = de_solve_result_at(result, 0u);
    char *problem_text = de_to_string(ode, style_EXPRESSION);
    string_t *solution_text =
        solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    bool valid =
        ode &&
        problem_text &&
        strcmp(problem_text, expected_problem) == 0 &&
        de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
        de_solve_result_solver(result) == DE_SOLVER_SEPARABLE &&
        de_solve_result_count(result) == 1u &&
        solution_text &&
        strcmp(string_c_str(solution_text), expected_solution) == 0;

    printf("input = %s\n", source);
    printf("problem = %s\n", problem_text ? problem_text : "NULL");
    printf(
        "solution = %s\n",
        solution_text ? string_c_str(solution_text) : "NULL");

    string_free(solution_text);
    free(problem_text);
    de_solve_result_free(result);
    de_free(ode);
    ASSERT_TRUE(valid);
}

int tests_main(void)
{
    RUN_TEST_CASE(test_diffequ_lifecycle_null_safety);
    RUN_TEST_CASE(test_diffequ_constructs_from_equation);
    RUN_TEST_CASE(test_diffequ_parses_separable_ode);
    RUN_TEST_CASE(test_diffequ_parses_linear_ode_and_constant);
    RUN_TEST_CASE(test_diffequ_expression_text_round_trips);
    RUN_TEST_CASE(test_diffequ_parses_ode_shorthand);
    RUN_TEST_CASE(test_diffequ_parses_and_solves_prime_ode_shorthand);
    RUN_TEST_CASE(test_diffequ_rejects_noncanonical_text);
    RUN_TEST_CASE(test_diffequ_solves_separable_initial_value_problem);
    RUN_TEST_CASE(test_diffequ_solves_linear_initial_value_problem);
    RUN_TEST_CASE(test_diffequ_solves_quadratic_separable_problem);
    RUN_TEST_CASE(test_diffequ_solves_variable_coefficient_linear_problem);
    RUN_TEST_CASE(test_diffequ_solves_rational_integrating_factor_problem);
    RUN_TEST_CASE(test_diffequ_linear_solution_retains_arbitrary_constant);
    RUN_TEST_CASE(test_diffequ_linear_solution_uses_special_function);
    RUN_TEST_CASE(test_diffequ_linear_solution_retains_formal_integral);
    RUN_TEST_CASE(test_diffequ_linear_solution_retains_formal_factor);
    RUN_TEST_CASE(test_diffequ_solves_first_order_homogeneous_problem);
    RUN_TEST_CASE(test_diffequ_homogeneous_solution_retains_constant);
    RUN_TEST_CASE(test_diffequ_solves_affine_combination_substitution);
    RUN_TEST_CASE(test_diffequ_solves_shifted_homogeneous_substitution);
    RUN_TEST_CASE(test_diffequ_solves_linear_change_of_variables);
    RUN_TEST_CASE(test_diffequ_retains_arbitrary_constant);
    RUN_TEST_CASE(test_diffequ_preserves_zero_singular_solution);
    RUN_TEST_CASE(test_diffequ_solves_quadratic_bernoulli_problem);
    RUN_TEST_CASE(test_diffequ_solves_derivative_quadratic_problem);
    RUN_TEST_CASE(test_diffequ_linearizes_exact_third_order_problem);
    RUN_TEST_CASE(test_diffequ_solves_second_order_sturm_liouville_problem);
    RUN_TEST_CASE(
        test_diffequ_does_not_invent_cubic_potential_functions);
    RUN_TEST_CASE(test_diffequ_solves_repeated_characteristic_root);
    RUN_TEST_CASE(test_diffequ_solves_oscillatory_sturm_liouville_problem);
    RUN_TEST_CASE(
        test_diffequ_normalizes_variable_coefficient_sturm_liouville);
    RUN_TEST_CASE(
        test_diffequ_solves_third_order_constant_coefficient_problem);
    RUN_TEST_CASE(test_diffequ_solves_high_order_repeated_root);
    RUN_TEST_CASE(
        test_diffequ_solves_nonhomogeneous_constant_coefficient_problem);
    RUN_TEST_CASE(test_diffequ_solves_repeated_complex_roots);
    RUN_TEST_CASE(
        test_diffequ_solves_degree_six_characteristic_polynomial);
    RUN_TEST_CASE(
        test_diffequ_general_solution_with_distinct_real_roots);
    RUN_TEST_CASE(
        test_diffequ_general_solution_with_repeated_real_root);
    RUN_TEST_CASE(
        test_diffequ_general_solution_with_real_and_complex_roots);
    RUN_TEST_CASE(
        test_diffequ_general_solution_with_repeated_complex_roots);
    RUN_TEST_CASE(test_diffequ_general_sixth_order_solution);
    RUN_TEST_CASE(test_diffequ_general_nonhomogeneous_solution);
    RUN_TEST_CASE(test_diffequ_general_trigonometric_forcing_solution);
    RUN_TEST_CASE(test_diffequ_general_third_order_forced_solution);
    RUN_TEST_CASE(test_diffequ_parses_pde_boundary_arguments);
    RUN_TEST_CASE(
        test_diffequ_solves_constant_transport_from_y_boundary);
    RUN_TEST_CASE(
        test_diffequ_solves_constant_transport_from_x_boundary);
    RUN_TEST_CASE(
        test_diffequ_solves_unbounded_homogeneous_transport);
    RUN_TEST_CASE(
        test_diffequ_solves_unbounded_inhomogeneous_transport);
    RUN_TEST_CASE(test_diffequ_solves_nonlinear_characteristic_pde);
    RUN_TEST_CASE(test_diffequ_solves_rotating_characteristic_pde);
    RUN_TEST_CASE(test_diffequ_solves_parameter_linear_pde);

    TEST_SECTION("README Output Example");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(
        example_diffequation_solving_an_ode,
        readme_examples,
        "diffequation,readme,output");

    return TESTS_EXIT_CODE();
}
