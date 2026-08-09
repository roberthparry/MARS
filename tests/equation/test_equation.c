#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "equation.h"
#include "expression.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

/*
 * Readme example worth keeping visible:
 *
 *   num_set_default_prec_digits(64);
 *
 *   equation_t *kepler =
 *       equ_from_string("{ M = E - e*sin(E) | E = 1.5; M = 1.5, e = 0.0167 }");
 *
 * This demonstrates the current equation model:
 * - the equation text carries both variable and constant bindings
 * - symbolic solving is tried first
 * - numeric fallback reuses the supplied variable value as its starting point
 * - precision can be fixed up front in significant decimal digits
 */

typedef equation_solutions_t equation_solve_result_t;

static int test_equation_derive_solutions_call(const equation_t *equation, equation_solutions_t **out)
{
    *out = equ_derive_solutions(equation);
    return *out ? 0 : -1;
}

#define ASSERT_NEW_RESULT(name) equation_solutions_t *(name) = NULL
#define RESULT_COUNT(result_ptr) equ_solutions_count((result_ptr))
#define RESULT_IS_SOLVED(result_ptr) (equ_solutions_count((result_ptr)) > 0u)
#define RESULT_SOLUTION(result_ptr, index) equ_solutions_at((result_ptr), (index))
#define equ_solve_result_free(ptr) equ_solutions_free((ptr))
#define equ_solve_for(equation, wrt, result_ptr) test_equation_derive_solutions_call((equation), &(result_ptr))
#define equ_solve_numeric(equation, bindings, options, result_ptr)                                                     \
    test_equation_derive_solutions_call((equation), &(result_ptr))

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

static bool test_equation_result_contains_long(const equation_solve_result_t *result, long expected_value)
{
    for (size_t i = 0u; i < RESULT_COUNT(result); ++i) {
        number_t value = expr_eval(equ_rhs(RESULT_SOLUTION(result, i)));
        bool match = test_number_equals_long(value, expected_value);

        num_destroy(&value);
        if (match)
            return true;
    }

    return false;
}

static bool test_equation_result_has_solution_for(const equation_solve_result_t *result, const expr_t *wrt)
{
    for (size_t i = 0u; i < RESULT_COUNT(result); ++i) {
        if (equ_is_solved_for(RESULT_SOLUTION(result, i), wrt))
            return true;
    }

    return false;
}

static bool test_equation_all_solutions_satisfy(const equation_t *equation, const expr_t *wrt,
                                                const equation_solve_result_t *result, const char *tolerance_text)
{
    expr_t *residual = equ_residual(equation);
    number_t tolerance = num_create_from_string(tolerance_text);
    bool ok = residual != NULL;

    for (size_t i = 0u; ok && i < RESULT_COUNT(result); ++i) {
        const expr_t *rhs = equ_rhs(RESULT_SOLUTION(result, i));
        expr_t *substituted = expr_substitute(residual, wrt, rhs);
        number_t value = substituted ? expr_eval(substituted) : num_new();
        number_t magnitude = num_abs(value);

        ok = substituted && num_is_finite(magnitude) && num_le(magnitude, tolerance);
        num_destroy(&magnitude);
        num_destroy(&value);
        expr_free(substituted);
    }

    num_destroy(&tolerance);
    expr_free(residual);
    return ok;
}

static bool test_equation_nonreal_solutions_have_conjugates(const equation_solve_result_t *result)
{
    for (size_t i = 0u; i < RESULT_COUNT(result); ++i) {
        number_t value = expr_eval(equ_rhs(RESULT_SOLUTION(result, i)));
        number_t conjugate;
        bool found = false;

        if (num_is_real(value)) {
            num_destroy(&value);
            continue;
        }
        conjugate = num_conj(value);
        for (size_t j = 0u; j < RESULT_COUNT(result); ++j) {
            number_t candidate = expr_eval(equ_rhs(RESULT_SOLUTION(result, j)));

            found = num_eq(candidate, conjugate);
            num_destroy(&candidate);
            if (found)
                break;
        }
        num_destroy(&conjugate);
        num_destroy(&value);
        if (!found)
            return false;
    }
    return true;
}

static bool test_equation_rhs_string_equals(const equation_t *equation, const char *expected)
{
    string_t *text;
    bool ok;

    if (!equation)
        return false;

    text = expr_to_text(equ_rhs(equation), style_EXPRESSION);
    if (!text)
        return false;

    ok = strcmp(string_c_str(text), expected) == 0;
    string_free(text);
    return ok;
}

static bool test_equation_result_has_rhs_string(const equation_solve_result_t *result, const char *expected)
{
    for (size_t i = 0u; i < RESULT_COUNT(result); ++i) {
        string_t *text = expr_to_text(equ_rhs(RESULT_SOLUTION(result, i)), style_UNBOUND);
        bool ok = text && strcmp(string_c_str(text), expected) == 0;

        string_free(text);
        if (ok)
            return true;
    }

    return false;
}

static bool test_equation_rhs_text_contains(const equation_t *equation, style_t style, const char *expected)
{
    string_t *text;
    bool ok;

    if (!equation)
        return false;

    text = expr_to_text(equ_rhs(equation), style);
    if (!text)
        return false;

    ok = strstr(string_c_str(text), expected) != NULL;
    string_free(text);
    return ok;
}

static bool test_equation_result_has_rhs_text_containing(const equation_solve_result_t *result, style_t style,
                                                         const char *expected)
{
    for (size_t i = 0u; i < RESULT_COUNT(result); ++i) {
        if (test_equation_rhs_text_contains(RESULT_SOLUTION(result, i), style, expected))
            return true;
    }

    return false;
}

static bool test_equation_result_has_rhs_text_containing_either(const equation_solve_result_t *result, style_t style,
                                                                const char *left, const char *right)
{
    return test_equation_result_has_rhs_text_containing(result, style, left) ||
           test_equation_result_has_rhs_text_containing(result, style, right);
}

static void example_equation_quadratic_solve(void)
{
    size_t saved_digits = num_get_default_prec_digits();

    num_set_default_prec_digits(72u);

    equation_t *equation = equ_from_string("x^2 - x - 1 = 0");
    equation_solutions_t *solutions = equ_derive_solutions(equation);

    for (size_t i = 0; i < equ_solutions_count(solutions); ++i) {
        const equation_t *solution = equ_solutions_at(solutions, i);
        string_t *text = equ_to_text(solution, style_UNBOUND);

        if (text)
            string_printf("solution[%zu] = %S\n", i, text);
        string_free(text);
    }

    equ_solutions_free(solutions);
    equ_free(equation);
    num_set_default_prec_digits(saved_digits);
}

static void example_equation_sextic_solve(void)
{
    size_t saved_digits = num_get_default_prec_digits();

    num_set_default_prec_digits(72u);

    equation_t *equation = equ_from_string("x^6 - 3x^5 + 2x^2 + x + 5 = 0");
    equation_solutions_t *solutions = equ_derive_solutions(equation);

    for (size_t i = 0; i < equ_solutions_count(solutions); ++i) {
        const equation_t *solution = equ_solutions_at(solutions, i);
        string_t *text = equ_to_text(solution, style_UNBOUND);

        if (text)
            string_printf("solution[%zu] = %S\n", i, text);
        string_free(text);
    }

    equ_solutions_free(solutions);
    equ_free(equation);
    num_set_default_prec_digits(saved_digits);
}

static void example_equation_kepler(void)
{
    size_t saved_digits = num_get_default_prec_digits();
    equation_t *kepler;
    equation_solutions_t *solutions;
    const equation_t *first;

    num_set_default_prec_digits(64u);

    kepler = equ_from_string("{ M = E - e*sin(E) | E = 1.5; M = 1.5, e = 0.0167 }");
    solutions = equ_derive_solutions(kepler);
    first = equ_solutions_at(solutions, 0u);

    if (first)
        equ_print(first);

    equ_solutions_free(solutions);
    equ_free(kepler);
    num_set_default_prec_digits(saved_digits);
}

static void example_equation_output_form(void)
{
    equation_t *equation = equ_from_string("2*x + 3 = 7");
    string_t *text = equ_to_text(equation, style_UNBOUND);

    if (text)
        string_printf("%S\n", text);

    string_free(text);
    equ_free(equation);
}

static void test_equation_from_string_shares_symbols_across_sides(void)
{
    equation_t *equation = equ_from_string("{ x = x + 1 | x = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_FALSE(equ_is_solved_for(equation, x));
    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_FALSE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 0);

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_from_string_accepts_bare_equation(void)
{
    equation_t *equation = equ_from_string("2*x + 3 = 7");
    equation_t *multi = equ_from_string("x + y = z");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    ASSERT_NOT_NULL(multi);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);
    ASSERT_EQ_INT((int)expr_bindings_count(equ_bindings(multi)), 3);
    ASSERT_NOT_NULL(equ_binding(multi, "x"));
    ASSERT_NOT_NULL(equ_binding(multi, "y"));
    ASSERT_NOT_NULL(equ_binding(multi, "z"));

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 1);
    ASSERT_TRUE(test_equation_result_contains_long(result, 2L));

    equ_solve_result_free(result);
    equ_free(multi);
    equ_free(equation);
}

static void test_equation_numeric_solves_all_variable_bindings(void)
{
    equation_t *equation = equ_from_string("{ x^2 + y^2 - 5 = 0 | x = 1, y = 1 }");
    expr_t *x;
    expr_t *y;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    y = equ_binding(equation, "y");
    ASSERT_NOT_NULL(x);
    ASSERT_NOT_NULL(y);

    ASSERT_EQ_INT(equ_solve_numeric(equation, NULL, NULL, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 4);
    ASSERT_TRUE(test_equation_result_has_solution_for(result, x));
    ASSERT_TRUE(test_equation_result_has_solution_for(result, y));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "√(5 - y²)"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "√(5 - x²)"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_numeric_rejects_unresolved_parameter_residual(void)
{
    equation_t *equation = equ_from_string("{ a*x^2 + b*x + c = 0 | x = 0 }");
    expr_t *x;
    number_t x_value;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_numeric(equation, NULL, NULL, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 2);
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "√(b² - 4ac)"));

    x_value = expr_eval(x);
    ASSERT_TRUE(test_number_equals_long(x_value, 0L));

    num_destroy(&x_value);
    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_to_text_round_trips_through_parser(void)
{
    equation_t *equation = equ_from_string("{ 2*x + 3 = 7 | x = 4 }");
    string_t *text;
    equation_t *roundtrip;
    expr_t *x;
    ASSERT_NEW_RESULT(result);
    number_t x_value;

    ASSERT_NOT_NULL(equation);

    text = equ_to_text(equation, style_EXPRESSION);
    ASSERT_NOT_NULL(text);
    roundtrip = equ_from_text(text);
    ASSERT_NOT_NULL(roundtrip);

    x = equ_binding(roundtrip, "x");
    ASSERT_NOT_NULL(x);
    x_value = expr_eval(x);
    ASSERT_TRUE(test_number_equals_long(x_value, 4L));
    num_destroy(&x_value);

    ASSERT_EQ_INT(equ_solve_for(roundtrip, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 1);
    ASSERT_TRUE(test_equation_result_contains_long(result, 2L));

    equ_solve_result_free(result);
    equ_free(roundtrip);
    string_free(text);
    equ_free(equation);
}

static void test_equation_function_style_preserves_both_sides(void)
{
    equation_t *equation = equ_from_string("{ x + y = a*z | x = 1, y = 2, z = 3; a = 4 }");
    string_t *text;
    string_t *formatted;

    ASSERT_NOT_NULL(equation);

    text = equ_to_text(equation, style_FUNCTION);
    ASSERT_NOT_NULL(text);
    ASSERT_TRUE(strstr(string_c_str(text), "equation equ(x, y, z, const a)") != NULL);
    ASSERT_TRUE(strstr(string_c_str(text), "return equation(x + y = a * z);") != NULL);
    ASSERT_TRUE(strstr(string_c_str(text), "return x + y - a * z;") == NULL);
    ASSERT_TRUE(strstr(string_c_str(text), "output(equ(x, y, z, a).solve());") != NULL);
    ASSERT_TRUE(strstr(string_c_str(text), "constant[] solve") == NULL);
    ASSERT_TRUE(strstr(string_c_str(text), "print(") == NULL);
    ASSERT_TRUE(strstr(string_c_str(text), "equ_eval") == NULL);
    ASSERT_TRUE(strstr(string_c_str(text), "equ_values") == NULL);

    formatted = equ_sprintf_text("%nf", equation);
    ASSERT_NOT_NULL(formatted);
    ASSERT_TRUE(strcmp(string_c_str(text), string_c_str(formatted)) == 0);

    string_free(formatted);
    string_free(text);
    equ_free(equation);
}

static void test_equation_display_expansion_distributes_sum_products(void)
{
    equation_t *equation = equ_from_string("(x^9 - 10x^8 + 27x^7 + 10x^6 - 125x^5 + 130x^4 - "
                                           "27x^3 - 10x^2 + 124x - 120)(x + 10) = 0");
    expr_t *x;
    equation_t *expanded;
    string_t *unbound;
    string_t *function;

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);
    expanded = equ_display_expanded(equation, x);
    ASSERT_NOT_NULL(expanded);
    unbound = equ_to_text(expanded, style_UNBOUND);
    function = equ_to_text(expanded, style_FUNCTION);
    ASSERT_NOT_NULL(unbound);
    ASSERT_NOT_NULL(function);

    ASSERT_TRUE(strstr(string_c_str(unbound), "x¹⁰ - 73x⁸ + 280x⁷ - 25x⁶") != NULL);
    ASSERT_TRUE(strstr(string_c_str(unbound), "+ 24x² + 1120x - 1200 = 0") != NULL);
    ASSERT_TRUE(strstr(string_c_str(unbound), "(x + 10)") == NULL);
    ASSERT_TRUE(strstr(string_c_str(function), "x^10") != NULL);
    ASSERT_TRUE(strstr(string_c_str(function), "(x + 10) *") == NULL);
    ASSERT_TRUE(strstr(string_c_str(function), "return equation(\n"
                                               "          x^10 - 73 * x^8 + 280 * x^7 - 25 * x^6 "
                                               "- 1120 * x^5 + 1273 * x^4 - 280 * x^3\n") != NULL);
    ASSERT_TRUE(strstr(string_c_str(function), "        + 24 * x^2 + 1120 * x - 1200 = 0\n"
                                               "    );") != NULL);

    string_free(function);
    string_free(unbound);
    equ_free(expanded);
    equ_free(equation);
}

static void test_equation_expands_conjugate_factors_and_solves_all_roots(void)
{
    equation_t *equation = equ_from_string("(x-(1+i))(x-(1-i))(x-(1+5i))(x-(1-5i))"
                                           "(x-1)(x+1)(x-2)(x+2)(x+5) = 0");
    equation_t *expanded;
    expr_t *x;
    string_t *unbound;
    string_t *function;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);
    expanded = equ_display_expanded(equation, x);
    ASSERT_NOT_NULL(expanded);
    unbound = equ_to_text(expanded, style_UNBOUND);
    function = equ_to_text(expanded, style_FUNCTION);
    ASSERT_NOT_NULL(unbound);
    ASSERT_NOT_NULL(function);

    ASSERT_TRUE(strcmp(string_c_str(unbound), "x⁹ + x⁸ + 7x⁷ + 99x⁶ - 284x⁵ - 256x⁴ + "
                                              "1188x³ - 884x² - 912x + 1040 = 0") == 0);
    ASSERT_TRUE(strstr(string_c_str(function), "          x^9 + x^8 + 7 * x^7 + 99 * x^6 "
                                               "- 284 * x^5 - 256 * x^4 + 1188 * x^3\n"
                                               "        - 884 * x^2 - 912 * x + 1040 = 0\n") != NULL);
    ASSERT_TRUE(strstr(string_c_str(function), "    );\n"
                                               "}\n\n"
                                               "// x = ?\n"
                                               "output(equ(x).solve());") != NULL);
    ASSERT_TRUE(strstr(string_c_str(function), "(x - (1 + i))") == NULL);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 9);
    ASSERT_TRUE(test_equation_result_contains_long(result, -5L));
    ASSERT_TRUE(test_equation_result_contains_long(result, -2L));
    ASSERT_TRUE(test_equation_result_contains_long(result, -1L));
    ASSERT_TRUE(test_equation_result_contains_long(result, 1L));
    ASSERT_TRUE(test_equation_result_contains_long(result, 2L));
    ASSERT_TRUE(test_equation_nonreal_solutions_have_conjugates(result));
    ASSERT_TRUE(test_equation_all_solutions_satisfy(equation, x, result, "1e-28"));

    equ_solve_result_free(result);
    string_free(function);
    string_free(unbound);
    equ_free(expanded);
    equ_free(equation);
}

static void test_equation_detects_already_solved_form(void)
{
    expr_t *x = test_equation_named_var_d(0.0, "x");
    expr_t *two = test_equation_const_d(2.0);
    equation_t *equation = equ_new(x, two);
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    ASSERT_TRUE(equ_lhs(equation) == x);
    ASSERT_TRUE(equ_rhs(equation) == two);
    ASSERT_TRUE(equ_is_solved_for(equation, x));

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 1);
    ASSERT_TRUE(equ_is_solved_for(RESULT_SOLUTION(result, 0u), x));

    equ_solve_result_free(result);
    equ_free(equation);
    expr_free(two);
    expr_free(x);
}

static void test_equation_rejects_rhs_containing_solve_variable(void)
{
    expr_t *x = test_equation_named_var_d(0.0, "x");
    expr_t *one = test_equation_const_d(1.0);
    expr_t *x_plus_one = expr_add(x, one);
    equation_t *equation = equ_new(x, x_plus_one);
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    ASSERT_FALSE(equ_is_solved_for(equation, x));
    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_FALSE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 0);

    equ_solve_result_free(result);
    equ_free(equation);
    expr_free(x_plus_one);
    expr_free(one);
    expr_free(x);
}

static void test_equation_solves_simple_affine_equation(void)
{
    equation_t *equation = equ_from_string("{ 2*x + 3 = 7 | x = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 1);
    ASSERT_TRUE(equ_is_solved_for(RESULT_SOLUTION(result, 0u), x));
    ASSERT_TRUE(test_equation_result_contains_long(result, 2L));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_symbolic_affine_equation(void)
{
    equation_t *equation = equ_from_string("{ b*x + c = 0 | x = NAN; b = NAN, c = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 1);
    ASSERT_TRUE(equ_is_solved_for(RESULT_SOLUTION(result, 0u), x));
    ASSERT_TRUE(test_equation_rhs_text_contains(RESULT_SOLUTION(result, 0u), style_UNBOUND, "-c/b"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_affine_variable_from_rhs(void)
{
    equation_t *equation = equ_from_string("{ 7 = 2*x + 3 | x = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 1);
    ASSERT_TRUE(equ_is_solved_for(RESULT_SOLUTION(result, 0u), x));
    ASSERT_TRUE(test_equation_result_contains_long(result, 2L));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_symbolic_quadratic_formula(void)
{
    equation_t *equation = equ_from_string("{ a*x^2 + b*x + c = 0 | x = NAN; a = NAN, b = NAN, c = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 2);
    ASSERT_TRUE(equ_is_solved_for(RESULT_SOLUTION(result, 0u), x));
    ASSERT_TRUE(equ_is_solved_for(RESULT_SOLUTION(result, 1u), x));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_TEX, "\\sqrt{b^{2} - 4 a c}"));
    ASSERT_TRUE(
        test_equation_result_has_rhs_text_containing(result, style_TEX, "\\frac{\\sqrt{b^{2} - 4 a c} - b}{2 a}"));
    ASSERT_TRUE(
        test_equation_result_has_rhs_text_containing(result, style_TEX, "\\frac{-\\sqrt{b^{2} - 4 a c} - b}{2 a}"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_quadratic_two_roots(void)
{
    equation_t *equation = equ_from_string("{ x^2 = 4 | x = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 2);
    ASSERT_TRUE(test_equation_result_contains_long(result, -2L));
    ASSERT_TRUE(test_equation_result_contains_long(result, 2L));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_quadratic_complex_roots(void)
{
    equation_t *equation = equ_from_string("{ x*x = -3 | x = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 2);
    ASSERT_TRUE(equ_is_solved_for(RESULT_SOLUTION(result, 0u), x));
    ASSERT_TRUE(equ_is_solved_for(RESULT_SOLUTION(result, 1u), x));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "i"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "-"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_atan_sum_as_branch_valid_surd(void)
{
    equation_t *equation = equ_from_string("atan(2x) + atan(x) = pi/4");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 1);
    ASSERT_TRUE(equ_is_solved_for(RESULT_SOLUTION(result, 0u), x));
    ASSERT_TRUE(test_equation_rhs_text_contains(RESULT_SOLUTION(result, 0u), style_TEX, "\\sqrt{17}"));
    ASSERT_TRUE(test_equation_all_solutions_satisfy(equation, x, result, "1e-24"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_self_power_with_lambert_w(void)
{
    equation_t *equation = equ_from_string("{ x^x = -3 | x = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 1);
    ASSERT_TRUE(equ_is_solved_for(RESULT_SOLUTION(result, 0u), x));
    ASSERT_TRUE(test_equation_result_has_rhs_string(result, "(ln(-3) + 2iπn)/Wₙ(k, ln(-3) + 2iπn)"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_quadratic_zero_product(void)
{
    equation_t *equation = equ_from_string("{ (x - 2)*(x + 3) = 0 | x = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 2);
    ASSERT_TRUE(test_equation_result_contains_long(result, -3L));
    ASSERT_TRUE(test_equation_result_contains_long(result, 2L));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_symbolic_zero_product_factors(void)
{
    equation_t *equation = equ_from_string("{ (x-a)(x-b)(x-c) = 0 | x = NAN; a = NAN, b = NAN, c = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 3);
    ASSERT_TRUE(test_equation_result_has_rhs_string(result, "a"));
    ASSERT_TRUE(test_equation_result_has_rhs_string(result, "b"));
    ASSERT_TRUE(test_equation_result_has_rhs_string(result, "c"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_quadratic_double_root_once(void)
{
    equation_t *equation = equ_from_string("{ x^2 - 4*x + 4 = 0 | x = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 1);
    ASSERT_TRUE(test_equation_result_contains_long(result, 2L));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_cubic_three_real_roots(void)
{
    equation_t *equation = equ_from_string("{ x^3 - 6*x^2 + 11*x - 6 = 0 | x = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 3);
    ASSERT_TRUE(test_equation_result_contains_long(result, 1L));
    ASSERT_TRUE(test_equation_result_contains_long(result, 2L));
    ASSERT_TRUE(test_equation_result_contains_long(result, 3L));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_cubic_complex_pair(void)
{
    equation_t *equation = equ_from_string("{ x^3 - 1 = 0 | x = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 3);
    ASSERT_TRUE(test_equation_result_contains_long(result, 1L));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_cubic_repeated_root_once(void)
{
    equation_t *equation = equ_from_string("{ x^3 - 3*x + 2 = 0 | x = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 2);
    ASSERT_TRUE(test_equation_result_contains_long(result, -2L));
    ASSERT_TRUE(test_equation_result_contains_long(result, 1L));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_cubic_with_numeric_parameter_bindings(void)
{
    equation_t *equation = equ_from_string("{ a*x^3 + b*x^2 + c*x + d = 0 | x = NAN; "
                                           "a = 1, b = -6, c = 11, d = -6 }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 3);
    ASSERT_TRUE(test_equation_result_contains_long(result, 1L));
    ASSERT_TRUE(test_equation_result_contains_long(result, 2L));
    ASSERT_TRUE(test_equation_result_contains_long(result, 3L));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_symbolic_cubic_cardano(void)
{
    equation_t *equation = equ_from_string("{ a*x^3 + b*x^2 + c*x + d = 0 | x = NAN; "
                                           "a = NAN, b = NAN, c = NAN, d = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 3);
    ASSERT_TRUE(equ_is_solved_for(RESULT_SOLUTION(result, 0u), x));
    ASSERT_TRUE(equ_is_solved_for(RESULT_SOLUTION(result, 1u), x));
    ASSERT_TRUE(equ_is_solved_for(RESULT_SOLUTION(result, 2u), x));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_TEX, "^{\\frac{1}{3}}"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_TEX, "\\sqrt{"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_TEX, "3 a c - b^{2}"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_TEX, "27 a d - 9 b c"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_expanded_quartic_four_real_roots(void)
{
    equation_t *equation = equ_from_string("x^4 - 5*x^2 + 4 = 0");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 4);
    ASSERT_TRUE(test_equation_result_contains_long(result, -2L));
    ASSERT_TRUE(test_equation_result_contains_long(result, -1L));
    ASSERT_TRUE(test_equation_result_contains_long(result, 1L));
    ASSERT_TRUE(test_equation_result_contains_long(result, 2L));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_expanded_quartic_complex_roots(void)
{
    equation_t *equation = equ_from_string("x^4 + 5*x^2 + 4 = 0");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 4);
    ASSERT_TRUE(test_equation_result_has_rhs_string(result, "i"));
    ASSERT_TRUE(test_equation_result_has_rhs_string(result, "-i"));
    ASSERT_TRUE(test_equation_result_has_rhs_string(result, "2i"));
    ASSERT_TRUE(test_equation_result_has_rhs_string(result, "-2i"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_deduplicates_repeated_quartic_root(void)
{
    equation_t *equation = equ_from_string("x^4 - 4*x^3 + 6*x^2 - 4*x + 1 = 0");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 1);
    ASSERT_TRUE(test_equation_result_contains_long(result, 1L));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_deduplicates_repeated_irrational_quartic_roots(void)
{
    size_t saved_digits = num_get_default_prec_digits();
    equation_t *equation;
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    num_set_default_prec_digits(40u);
    equation = equ_from_string("x^4 - 4*x^2 + 4 = 0");

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 2);
    ASSERT_TRUE(test_equation_all_solutions_satisfy(equation, x, result, "1e-30"));

    equ_solve_result_free(result);
    equ_free(equation);
    num_set_default_prec_digits(saved_digits);
}

static void test_equation_quartic_roots_satisfy_original_polynomial(void)
{
    equation_t *equation = equ_from_string("x^4 + x + 1 = 0");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 4);
    ASSERT_TRUE(test_equation_all_solutions_satisfy(equation, x, result, "1e-40"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_expanded_quintic_five_real_roots(void)
{
    equation_t *equation = equ_from_string("x^5 - 5*x^3 + 4*x = 0");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 5);
    ASSERT_TRUE(test_equation_result_contains_long(result, -2L));
    ASSERT_TRUE(test_equation_result_contains_long(result, -1L));
    ASSERT_TRUE(test_equation_result_contains_long(result, 0L));
    ASSERT_TRUE(test_equation_result_contains_long(result, 1L));
    ASSERT_TRUE(test_equation_result_contains_long(result, 2L));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_quintic_complex_roots(void)
{
    equation_t *equation = equ_from_string("x^5 - 1 = 0");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 5);
    ASSERT_TRUE(test_equation_result_contains_long(result, 1L));
    ASSERT_TRUE(test_equation_all_solutions_satisfy(equation, x, result, "1e-40"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_deduplicates_repeated_quintic_root(void)
{
    equation_t *equation = equ_from_string("x^5 - 5*x^4 + 10*x^3 - 10*x^2 + 5*x - 1 = 0");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 1);
    ASSERT_TRUE(test_equation_result_contains_long(result, 1L));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_quintic_roots_satisfy_original_polynomial(void)
{
    equation_t *equation = equ_from_string("x^5 + x + 1 = 0");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 5);
    ASSERT_TRUE(test_equation_all_solutions_satisfy(equation, x, result, "1e-40"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_sextic_six_real_roots(void)
{
    equation_t *equation = equ_from_string("x^6 - 14*x^4 + 49*x^2 - 36 = 0");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 6);
    ASSERT_TRUE(test_equation_result_contains_long(result, -3L));
    ASSERT_TRUE(test_equation_result_contains_long(result, -2L));
    ASSERT_TRUE(test_equation_result_contains_long(result, -1L));
    ASSERT_TRUE(test_equation_result_contains_long(result, 1L));
    ASSERT_TRUE(test_equation_result_contains_long(result, 2L));
    ASSERT_TRUE(test_equation_result_contains_long(result, 3L));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_degree_twelve_complex_roots(void)
{
    equation_t *equation = equ_from_string("x^12 - 1 = 0");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 12);
    ASSERT_TRUE(test_equation_result_contains_long(result, -1L));
    ASSERT_TRUE(test_equation_result_contains_long(result, 1L));
    ASSERT_TRUE(test_equation_result_has_rhs_string(result, "i"));
    ASSERT_TRUE(test_equation_result_has_rhs_string(result, "-i"));
    ASSERT_TRUE(test_equation_all_solutions_satisfy(equation, x, result, "1e-40"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_geometric_sum_with_exact_roots(void)
{
    equation_t *equation = equ_from_string("x^7 + x^6 + x^5 + x^4 + x^3 + x^2 + x + 1 = 0");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 7);
    ASSERT_TRUE(test_equation_result_has_rhs_string(result, "-1"));
    ASSERT_TRUE(test_equation_result_has_rhs_string(result, "i"));
    ASSERT_TRUE(test_equation_result_has_rhs_string(result, "-i"));
    ASSERT_TRUE(test_equation_result_has_rhs_string(result, "(-1 + i)·√(2)/2"));
    ASSERT_TRUE(test_equation_result_has_rhs_string(result, "(-1 - i)·√(2)/2"));
    ASSERT_TRUE(test_equation_result_has_rhs_string(result, "(1 + i)·√(2)/2"));
    ASSERT_TRUE(test_equation_result_has_rhs_string(result, "(1 - i)·√(2)/2"));
    ASSERT_TRUE(test_equation_all_solutions_satisfy(equation, x, result, "1e-40"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_geometric_sum_uses_degree_and_scale_rule(void)
{
    equation_t *equation = equ_from_string("3*x^4 + 3*x^3 + 3*x^2 + 3*x + 3 = 0");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 4);
    ASSERT_TRUE(test_equation_all_solutions_satisfy(equation, x, result, "1e-40"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_general_polynomial_roots_satisfy_original(void)
{
    equation_t *equation = equ_from_string("x^6 + x + 1 = 0");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 6);
    ASSERT_TRUE(test_equation_all_solutions_satisfy(equation, x, result, "1e-40"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_real_polynomial_returns_exact_conjugate_pairs(void)
{
    equation_t *equation = equ_from_string("x^6 - 3*x^5 + 2*x^2 + x + 5 = 0");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 6);
    ASSERT_TRUE(test_equation_nonreal_solutions_have_conjugates(result));
    ASSERT_TRUE(test_equation_all_solutions_satisfy(equation, x, result, "1e-40"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_deduplicates_repeated_high_degree_root(void)
{
    equation_t *equation = equ_from_string("x^6 - 6*x^5 + 15*x^4 - 20*x^3 + 15*x^2 - 6*x + 1 = 0");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 1);
    ASSERT_TRUE(test_equation_result_contains_long(result, 1L));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_collects_high_degree_composite_power(void)
{
    equation_t *equation = equ_from_string("(x^2 + 1)^4 = 0");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 2);
    ASSERT_TRUE(test_equation_result_has_rhs_string(result, "i"));
    ASSERT_TRUE(test_equation_result_has_rhs_string(result, "-i"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_derives_kepler_solution_from_bindings(void)
{
    equation_t *equation = equ_from_string("{ M = E - e*sin(E) | E = 1.5; M = 1.5, e = 0.0167 }");
    expr_t *residual;
    expr_t *E;
    expr_t *M;
    expr_t *e;
    ASSERT_NEW_RESULT(result);
    number_t E_value;
    number_t residual_value;
    number_t residual_mag;
    number_t tolerance;

    ASSERT_NOT_NULL(equation);
    E = equ_binding(equation, "E");
    M = equ_binding(equation, "M");
    e = equ_binding(equation, "e");
    ASSERT_NOT_NULL(E);
    ASSERT_NOT_NULL(M);
    ASSERT_NOT_NULL(e);

    ASSERT_EQ_INT(equ_solve_for(equation, E, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 1);
    ASSERT_TRUE(equ_is_solved_for(RESULT_SOLUTION(result, 0u), E));

    E_value = expr_eval(E);
    residual = equ_residual(equation);
    ASSERT_NOT_NULL(residual);
    residual_value = expr_eval(residual);
    residual_mag = num_abs(residual_value);
    tolerance = num_create_from_string("1e-24");

    ASSERT_TRUE(num_lt(residual_mag, tolerance));
    ASSERT_FALSE(num_is_nan(E_value));
    ASSERT_TRUE(test_equation_rhs_text_contains(RESULT_SOLUTION(result, 0u), style_UNBOUND, "1.516"));

    num_destroy(&tolerance);
    num_destroy(&residual_mag);
    num_destroy(&residual_value);
    num_destroy(&E_value);
    expr_free(residual);
    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_future_inverts_log(void)
{
    equation_t *equation = equ_from_string("{ ln(x) = 3 | x = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 1);
    ASSERT_TRUE(test_equation_rhs_string_equals(RESULT_SOLUTION(result, 0u), "exp(3)"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_future_inverts_exp(void)
{
    equation_t *equation = equ_from_string("{ exp(x) = 5 | x = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 1);
    ASSERT_TRUE(test_equation_rhs_string_equals(RESULT_SOLUTION(result, 0u), "ln(5)"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_future_solves_sin_family(void)
{
    equation_t *equation = equ_from_string("{ sin(x) = 1 | x = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "π/2"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "4n + 1"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_future_reduces_exp_sin_to_periodic_family(void)
{
    equation_t *equation = equ_from_string("{ exp(sin(x)) = e | x = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "π/2"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "4n + 1"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_future_keeps_periodic_sin_family_symbolic(void)
{
    equation_t *equation = equ_from_string("{ sin(x) = sqrt(0.2) | x = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "2π"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "asin"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "π·(2n + 1) - asin"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_future_simplifies_schoolbook_periodic_sin_families(void)
{
    equation_t *equation = equ_from_string("{ sin(x) = 1/sqrt(2) | x = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing_either(result, style_UNBOUND, "π/4", "¼π"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "π"));
    ASSERT_FALSE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "asin"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_future_simplifies_schoolbook_periodic_cos_families(void)
{
    equation_t *equation = equ_from_string("{ cos(x) = 1/2 | x = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing_either(result, style_UNBOUND, "π/3", "⅓π"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "π"));
    ASSERT_FALSE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "acos"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_future_simplifies_schoolbook_periodic_tan_families(void)
{
    equation_t *equation = equ_from_string("{ tan(x) = 1/sqrt(3) | x = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing_either(result, style_UNBOUND, "π/6", "⅙π"));
    ASSERT_FALSE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "atan"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_future_keeps_tan_sqrt_three_family_symbolic(void)
{
    equation_t *equation = equ_from_string("{ tan(x) = sqrt(3) | x = NAN }");
    expr_t *x;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    x = equ_binding(equation, "x");
    ASSERT_NOT_NULL(x);

    ASSERT_EQ_INT(equ_solve_for(equation, x, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "1/3·π·(3n + 1)"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_basics(void)
{
    TEST_RUN_SUBTEST(test_equation_from_string_shares_symbols_across_sides, NULL);
    TEST_RUN_SUBTEST(test_equation_from_string_accepts_bare_equation, NULL);
    TEST_RUN_SUBTEST(test_equation_numeric_solves_all_variable_bindings, NULL);
    TEST_RUN_SUBTEST(test_equation_numeric_rejects_unresolved_parameter_residual, NULL);
    TEST_RUN_SUBTEST(test_equation_to_text_round_trips_through_parser, NULL);
    TEST_RUN_SUBTEST(test_equation_function_style_preserves_both_sides, NULL);
    TEST_RUN_SUBTEST(test_equation_display_expansion_distributes_sum_products, NULL);
    TEST_RUN_SUBTEST(test_equation_expands_conjugate_factors_and_solves_all_roots, NULL);
    TEST_RUN_SUBTEST(test_equation_detects_already_solved_form, NULL);
    TEST_RUN_SUBTEST(test_equation_rejects_rhs_containing_solve_variable, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_simple_affine_equation, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_symbolic_affine_equation, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_affine_variable_from_rhs, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_symbolic_quadratic_formula, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_quadratic_two_roots, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_quadratic_complex_roots, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_atan_sum_as_branch_valid_surd, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_self_power_with_lambert_w, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_quadratic_zero_product, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_symbolic_zero_product_factors, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_quadratic_double_root_once, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_cubic_three_real_roots, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_cubic_complex_pair, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_cubic_repeated_root_once, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_cubic_with_numeric_parameter_bindings, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_symbolic_cubic_cardano, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_expanded_quartic_four_real_roots, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_expanded_quartic_complex_roots, NULL);
    TEST_RUN_SUBTEST(test_equation_deduplicates_repeated_quartic_root, NULL);
    TEST_RUN_SUBTEST(test_equation_deduplicates_repeated_irrational_quartic_roots, NULL);
    TEST_RUN_SUBTEST(test_equation_quartic_roots_satisfy_original_polynomial, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_expanded_quintic_five_real_roots, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_quintic_complex_roots, NULL);
    TEST_RUN_SUBTEST(test_equation_deduplicates_repeated_quintic_root, NULL);
    TEST_RUN_SUBTEST(test_equation_quintic_roots_satisfy_original_polynomial, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_sextic_six_real_roots, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_degree_twelve_complex_roots, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_geometric_sum_with_exact_roots, NULL);
    TEST_RUN_SUBTEST(test_equation_geometric_sum_uses_degree_and_scale_rule, NULL);
    TEST_RUN_SUBTEST(test_equation_general_polynomial_roots_satisfy_original, NULL);
    TEST_RUN_SUBTEST(test_equation_real_polynomial_returns_exact_conjugate_pairs, NULL);
    TEST_RUN_SUBTEST(test_equation_deduplicates_repeated_high_degree_root, NULL);
    TEST_RUN_SUBTEST(test_equation_collects_high_degree_composite_power, NULL);
    TEST_RUN_SUBTEST(test_equation_derives_kepler_solution_from_bindings, NULL);
}

static void test_equation_future_solver_cases(void)
{
    TEST_RUN_SUBTEST(test_equation_future_inverts_log, "future,inverse");
    TEST_RUN_SUBTEST(test_equation_future_inverts_exp, "future,inverse");
    TEST_RUN_SUBTEST(test_equation_future_solves_sin_family, "future,periodic");
    TEST_RUN_SUBTEST(test_equation_future_reduces_exp_sin_to_periodic_family, "future,periodic");
    TEST_RUN_SUBTEST(test_equation_future_keeps_periodic_sin_family_symbolic, "future,periodic");
    TEST_RUN_SUBTEST(test_equation_future_simplifies_schoolbook_periodic_sin_families, "future,periodic");
    TEST_RUN_SUBTEST(test_equation_future_simplifies_schoolbook_periodic_cos_families, "future,periodic");
    TEST_RUN_SUBTEST(test_equation_future_simplifies_schoolbook_periodic_tan_families, "future,periodic");
    TEST_RUN_SUBTEST(test_equation_future_keeps_tan_sqrt_three_family_symbolic, "future,periodic");
}

int tests_main(void)
{
    TEST_SECTION("Equation Basics");
    TEST_RUN_IN_GROUP(test_equation_basics, tests, NULL);

    TEST_SECTION("Future Solver Cases");
    TEST_RUN_IN_GROUP(test_equation_future_solver_cases, tests, "future");

    TEST_SECTION("README Output Examples");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_equation_quadratic_solve, readme_examples, "equation,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_equation_sextic_solve, readme_examples, "equation,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_equation_kepler, readme_examples, "equation,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_equation_output_form, readme_examples, "equation,readme,output");

    return TEST_EXIT_CODE();
}
