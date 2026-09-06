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

static bool test_equation_result_contains_number_text(const equation_solve_result_t *result, const char *expected_text)
{
    number_t expected = num_create_from_string(expected_text);
    bool found = false;

    for (size_t i = 0u; i < RESULT_COUNT(result); ++i) {
        number_t value = expr_eval(equ_rhs(RESULT_SOLUTION(result, i)));

        found = num_eq(value, expected);
        num_destroy(&value);
        if (found)
            break;
    }

    num_destroy(&expected);
    return found;
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

static void test_equation_expands_algebraic_sequence_ellipsis(void)
{
    {
        const char *scoped_equations[] = {
            "sum(k, 1, 3, k^s) = 14 | s = 2",
            "Σ_(k=1)^3 k^s = 14 | s = 2",
            "product(k, 1, 3, k^s) = 36 | s = 2",
            "Π_(k=1)^3 k^s = 36 | s = 2",
            "2^s + 3^s + 5^s + ... + 7^s = 87 | s = 2",
        };

        for (size_t i = 0u; i < sizeof(scoped_equations)/sizeof(scoped_equations[0]); ++i) {
            equation_t *equation = equ_from_string(scoped_equations[i]);
            expr_t *residual;
            number_t value;

            ASSERT_NOT_NULL(equation);
            ASSERT_EQ_INT((int)expr_bindings_count(equ_bindings(equation)), 1);
            residual = equ_residual(equation);
            ASSERT_NOT_NULL(residual);
            value = expr_eval(residual);
            ASSERT_TRUE(num_is_zero(value));
            num_destroy(&value);
            expr_free(residual);
            equ_free(equation);
        }
        equation_t *prime_equation = equ_from_string(
            "0.6243299885435508709929363831008372441796426201805292869735519024956380888551132544624602761955398688691404103940627431"
            " = 1/2^s + 1/3^s + 1/5^s + 1/7^s + 1/11^s + 1/13^s + 1/17^s + ... + 1/99991^s");

        ASSERT_NOT_NULL(prime_equation);
        ASSERT_EQ_INT((int)expr_bindings_count(equ_bindings(prime_equation)), 1);
        ASSERT_NOT_NULL(equ_binding(prime_equation, "s"));
        equ_free(prime_equation);
    }
    const char *linear_input = "x + 2x + 3x + ... + 10x = 100";
    const char *arithmetic_inputs[] = {
        "x + 3x + ... + 9x = 25",
        "x+4x+7x+10x+13x+16x+19x+22x+...+64x = 1430",
        "x + 4*x + 7*x + … + 64*x = 1430",
    };
    const long arithmetic_solutions[] = {
        1L,
        2L,
        2L,
    };
    const char *quadratic_inputs[] = {
        "x+4x+9x+...+100x = 20",
        "x + 4*x + 9*x + 16*x + … + 100*x = 20",
    };
    const char *cubic_inputs[] = {
        "x + 8x + 27x + ... + 1000x = 2000",
        "x + 8x + 27x + 64x + ... + 1000x = 2000",
    };
    const char *quartic_input = "x + 16x + 81x + 256x + ... + 10000x = 2000";
    const char *sparse_polynomial_inputs[] = {
        "x + 17x + 83x + 259x + ... + 10009x = 10000",
        "6x + 15x + 36x + ... + 1023x = 3165",
        "7x + 41x + 259x + 1051x + 3167x + ... + 100177x = 221500",
    };
    const char *lagrange_input = "x + 21x + 92x + 275x + ... + 10109x = 10000";
    const char *harmonic_input = "x + x/2 + x/3 + x/4 + ... + x/10 = 1";
    const char *geometric_input = "x + 2x + 4x + ... + 64x = 127";
    const char *large_input = "100000000000000000000x+100000000000000000003x+100000000000000000008x+"
                              "...+100000000000000000099x = 1";

    {
        equation_t *equation = equ_from_string(linear_input);
        equation_solutions_t *solutions;
        string_t *TeX;
        string_t *expanded_text;

        ASSERT_NOT_NULL(equation);
        expanded_text = equ_to_text(equation, style_UNBOUND);
        TeX = equ_to_text(equation, style_LATEX);
        ASSERT_NOT_NULL(expanded_text);
        ASSERT_NOT_NULL(TeX);
        ASSERT_TRUE(strcmp(string_c_str(expanded_text), "55x = 100") == 0);
        ASSERT_TRUE(strstr(string_c_str(TeX), "\\sum_{n=1}^{10}") != NULL);
        ASSERT_TRUE(strstr(string_c_str(TeX), "n\\mkern-2mu x") != NULL);

        solutions = equ_derive_solutions(equation);
        ASSERT_NOT_NULL(solutions);
        ASSERT_EQ_INT((int)equ_solutions_count(solutions), 1);
        ASSERT_TRUE(test_equation_result_contains_number_text(solutions, "20/11"));

        equ_solutions_free(solutions);
        string_free(TeX);
        string_free(expanded_text);
        equ_free(equation);
    }

    for (size_t i = 0u; i < sizeof(arithmetic_inputs) / sizeof(arithmetic_inputs[0]); ++i) {
        equation_t *equation = equ_from_string(arithmetic_inputs[i]);
        equation_solutions_t *solutions;

        ASSERT_NOT_NULL(equation);
        solutions = equ_derive_solutions(equation);
        ASSERT_NOT_NULL(solutions);
        ASSERT_EQ_INT((int)equ_solutions_count(solutions), 1);
        ASSERT_TRUE(test_equation_result_contains_long(solutions, arithmetic_solutions[i]));

        equ_solutions_free(solutions);
        equ_free(equation);
    }

    for (size_t i = 0u; i < sizeof(cubic_inputs) / sizeof(cubic_inputs[0]); ++i) {
        equation_t *equation = equ_from_string(cubic_inputs[i]);
        equation_solutions_t *solutions;

        ASSERT_NOT_NULL(equation);
        solutions = equ_derive_solutions(equation);
        ASSERT_NOT_NULL(solutions);
        ASSERT_EQ_INT((int)equ_solutions_count(solutions), 1);
        ASSERT_TRUE(test_equation_result_contains_number_text(solutions, "80/121"));

        equ_solutions_free(solutions);
        equ_free(equation);
    }

    {
        equation_t *equation = equ_from_string(quartic_input);
        equation_solutions_t *solutions;

        ASSERT_NOT_NULL(equation);
        solutions = equ_derive_solutions(equation);
        ASSERT_NOT_NULL(solutions);
        ASSERT_EQ_INT((int)equ_solutions_count(solutions), 1);
        ASSERT_TRUE(test_equation_result_contains_number_text(solutions, "2000/25333"));

        equ_solutions_free(solutions);
        equ_free(equation);
    }

    for (size_t i = 0u; i < sizeof(sparse_polynomial_inputs) / sizeof(sparse_polynomial_inputs[0]); ++i) {
        equation_t *equation = equ_from_string(sparse_polynomial_inputs[i]);
        equation_solutions_t *solutions;

        ASSERT_NOT_NULL(equation);
        solutions = equ_derive_solutions(equation);
        ASSERT_NOT_NULL(solutions);
        ASSERT_EQ_INT((int)equ_solutions_count(solutions), 1);
        if (i == 0u)
            ASSERT_TRUE(test_equation_result_contains_number_text(solutions, "5000/12689"));
        else
            ASSERT_TRUE(test_equation_result_contains_long(solutions, 1L));

        equ_solutions_free(solutions);
        equ_free(equation);
    }

    {
        equation_t *equation = equ_from_string(lagrange_input);
        equation_solutions_t *solutions;
        string_t *TeX;

        ASSERT_NOT_NULL(equation);
        TeX = equ_to_text(equation, style_LATEX);
        ASSERT_NOT_NULL(TeX);
        ASSERT_TRUE(strstr(string_c_str(TeX), "\\sum_{n=1}^{10}") != NULL);
        ASSERT_TRUE(strstr(string_c_str(TeX), "n^{4}") != NULL);
        solutions = equ_derive_solutions(equation);
        ASSERT_NOT_NULL(solutions);
        ASSERT_EQ_INT((int)equ_solutions_count(solutions), 1);
        ASSERT_TRUE(test_equation_result_contains_number_text(solutions, "625/1611"));

        equ_solutions_free(solutions);
        string_free(TeX);
        equ_free(equation);
    }

    {
        equation_t *equation = equ_from_string(harmonic_input);
        equation_solutions_t *solutions;
        string_t *TeX;

        ASSERT_NOT_NULL(equation);
        TeX = equ_to_text(equation, style_LATEX);
        ASSERT_NOT_NULL(TeX);
        ASSERT_TRUE(strstr(string_c_str(TeX), "\\sum_{n=1}^{10}") != NULL);
        ASSERT_TRUE(strstr(string_c_str(TeX), "\\frac{x}{n}") != NULL);
        solutions = equ_derive_solutions(equation);
        ASSERT_NOT_NULL(solutions);
        ASSERT_EQ_INT((int)equ_solutions_count(solutions), 1);
        ASSERT_TRUE(test_equation_result_contains_number_text(solutions, "2520/7381"));

        equ_solutions_free(solutions);
        string_free(TeX);
        equ_free(equation);
    }

    {
        equation_t *equation = equ_from_string(geometric_input);
        equation_solutions_t *solutions;

        ASSERT_NOT_NULL(equation);
        solutions = equ_derive_solutions(equation);
        ASSERT_NOT_NULL(solutions);
        ASSERT_EQ_INT((int)equ_solutions_count(solutions), 1);
        ASSERT_TRUE(test_equation_result_contains_long(solutions, 1L));

        equ_solutions_free(solutions);
        equ_free(equation);
    }

    for (size_t i = 0u; i < sizeof(quadratic_inputs) / sizeof(quadratic_inputs[0]); ++i) {
        equation_t *equation = equ_from_string(quadratic_inputs[i]);
        equation_solutions_t *solutions;

        ASSERT_NOT_NULL(equation);
        solutions = equ_derive_solutions(equation);
        ASSERT_NOT_NULL(solutions);
        ASSERT_EQ_INT((int)equ_solutions_count(solutions), 1);
        ASSERT_TRUE(test_equation_result_contains_number_text(solutions, "4/77"));

        equ_solutions_free(solutions);
        equ_free(equation);
    }

    {
        equation_t *equation = equ_from_string(large_input);
        equation_solutions_t *solutions;

        ASSERT_NOT_NULL(equation);
        solutions = equ_derive_solutions(equation);
        ASSERT_NOT_NULL(solutions);
        ASSERT_EQ_INT((int)equ_solutions_count(solutions), 1);
        ASSERT_TRUE(test_equation_result_contains_number_text(solutions, "1/1000000000000000000375"));

        equ_solutions_free(solutions);
        equ_free(equation);
    }

    ASSERT_NULL(equ_from_string("x + 16x + 81x + 256x + ... + 10001x = 2000"));
    ASSERT_NULL(equ_from_string("x + 2x + 4x + ... + 67x = 1430"));
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
    ASSERT_TRUE(strstr(string_c_str(text), "return equation(x + y = a.z).") != NULL);
    ASSERT_TRUE(strstr(string_c_str(text), "return x + y - a.z.") == NULL);
    ASSERT_TRUE(strstr(string_c_str(text), "output(equ(x, y, z, a).solve()).") != NULL);
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
    ASSERT_TRUE(strstr(string_c_str(function), "(x + 10).") == NULL);
    ASSERT_TRUE(strstr(string_c_str(function), "return equation(") != NULL);
    ASSERT_TRUE(strstr(string_c_str(function), "x^10 - 73.x^8 + 280.x^7 - 25.x^6") != NULL);
    ASSERT_TRUE(strstr(string_c_str(function), "24.x^2") != NULL);
    ASSERT_TRUE(strstr(string_c_str(function), "1120.x - 1200 = 0") != NULL);

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
    ASSERT_TRUE(strstr(string_c_str(function), "x^9 + x^8 + 7.x^7 + 99.x^6") != NULL);
    ASSERT_TRUE(strstr(string_c_str(function), "884.x^2") != NULL);
    ASSERT_TRUE(strstr(string_c_str(function), "912.x") != NULL);
    ASSERT_TRUE(strstr(string_c_str(function), "1040 = 0") != NULL);
    ASSERT_TRUE(strstr(string_c_str(function), "`` x = ?\noutput(equ(x).solve()).") != NULL);
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
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(
        result, style_LATEX, "\\sqrt{b^{2} - 4\\mkern-2mu a\\mkern-2mu c}"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(
        result, style_LATEX, "\\frac{\\sqrt{b^{2} - 4\\mkern-2mu a\\mkern-2mu c} - b}{2\\mkern-2mu a}"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(
        result, style_LATEX, "\\frac{-\\sqrt{b^{2} - 4\\mkern-2mu a\\mkern-2mu c} - b}{2\\mkern-2mu a}"));

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

static void test_equation_renders_exact_complex_quadratic_roots_as_surds(void)
{
    equation_t *equation = equ_from_string("{ u^2 + 5*u + 21 = 0 | u = NAN }");
    expr_t *u;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    u = equ_binding(equation, "u");
    ASSERT_NOT_NULL(u);

    ASSERT_EQ_INT(equ_solve_for(equation, u, result), 0);
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 2);
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_LATEX, "\\sqrt{59}"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_LATEX, "\\frac{-5 + i\\mkern-2mu \\sqrt{59}}{2}"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_LATEX, "\\frac{-5 - i\\mkern-2mu \\sqrt{59}}{2}"));
    ASSERT_TRUE(test_equation_all_solutions_satisfy(equation, u, result, "1e-40"));

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
    ASSERT_TRUE(test_equation_rhs_text_contains(RESULT_SOLUTION(result, 0u), style_LATEX, "\\sqrt{17}"));
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
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "Wₙ(k, ln(-3) + 2iπn)"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "ln(-3) + 2iπn"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_repeated_exponential_power_polynomial(void)
{
    equation_t *equation = equ_from_string("2^m + 2^(2m) + 2^(3m) = 84");
    expr_bindings_t *bindings;
    expr_t *m;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    bindings = equ_bindings(equation);
    m = equ_binding(equation, "m");
    ASSERT_NOT_NULL(bindings);
    ASSERT_NOT_NULL(m);
    ASSERT_FALSE(expr_bindings_is_constant_at(bindings, 0u));

    ASSERT_EQ_INT(equ_solve_for(equation, m, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 3);
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_LATEX, "2 + i"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_LATEX, "\\sqrt{59}"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_LATEX, "\\pi"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "n"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_simplifies_real_repeated_power_roots(void)
{
    equation_t *equation = equ_from_string("3^(3u) - 4*3^(2u) + 3^u + 6 = 0");
    expr_t *u;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    u = equ_binding(equation, "u");
    ASSERT_NOT_NULL(u);

    ASSERT_EQ_INT(equ_solve_for(equation, u, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 3);
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_LATEX, "\\frac{\\ln(2)}{\\ln(3)}"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_LATEX, "2\\mkern-2mu n + 1"));
    ASSERT_FALSE(test_equation_result_has_rhs_text_containing(result, style_LATEX, "\\sqrt{-"));
    ASSERT_FALSE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "0 + i"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_solves_repeated_exp_polynomial(void)
{
    equation_t *equation = equ_from_string("e^(3u) - 4*e^(2u) + e^u + 6 = 0");
    expr_t *u;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    u = equ_binding(equation, "u");
    ASSERT_NOT_NULL(u);

    ASSERT_EQ_INT(equ_solve_for(equation, u, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 3);
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_LATEX, "\\ln(2)"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_LATEX, "\\ln(3)"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_LATEX, "2\\mkern-2mu n + 1"));
    ASSERT_FALSE(test_equation_result_has_rhs_text_containing(result, style_LATEX, "\\sqrt{-"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_simplifies_negative_real_quartic_power_root(void)
{
    equation_t *equation = equ_from_string("5^(4u) - 8*5^(3u) + 17*5^(2u) + 2*5^u = 24");
    expr_t *u;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    u = equ_binding(equation, "u");
    ASSERT_NOT_NULL(u);

    ASSERT_EQ_INT(equ_solve_for(equation, u, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 4);
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(
        result, style_LATEX,
        "i\\mkern-2mu \\frac{\\pi\\mkern-2mu \\left(2\\mkern-2mu n + 1\\right)}{\\ln(5)}"));
    ASSERT_FALSE(test_equation_result_has_rhs_text_containing(result, style_LATEX, "\\ln(-1)"));
    ASSERT_FALSE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "ln(-1)"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_normalises_shifted_repeated_power_exponents(void)
{
    equation_t *equation = equ_from_string("5^(4u) - 5^(3u+1) - 2*5^(2u+1) + 16*5^(u+1) = 96");
    expr_t *u;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    u = equ_binding(equation, "u");
    ASSERT_NOT_NULL(u);

    ASSERT_EQ_INT(equ_solve_for(equation, u, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 4);
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_LATEX, "\\frac{\\ln(2)}{\\ln(5)}"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_LATEX, "\\frac{\\ln(3)}{\\ln(5)}"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_LATEX, "\\frac{\\ln(4)}{\\ln(5)}"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(
        result, style_LATEX,
        "i\\mkern-2mu \\frac{\\pi\\mkern-2mu \\left(2\\mkern-2mu n + 1\\right)}{\\ln(5)}"));
    ASSERT_FALSE(test_equation_result_has_rhs_text_containing(result, style_LATEX, "\\ln(-4)"));

    equ_solve_result_free(result);
    equ_free(equation);
}

static void test_equation_avoids_existing_branch_parameter_name(void)
{
    equation_t *equation = equ_from_string("{ 5^(2u) - 10*5^u + 25 = 0 | u = NAN; n = 7 }");
    expr_t *u;
    ASSERT_NEW_RESULT(result);

    ASSERT_NOT_NULL(equation);
    u = equ_binding(equation, "u");
    ASSERT_NOT_NULL(u);

    ASSERT_EQ_INT(equ_solve_for(equation, u, result), 0);
    ASSERT_TRUE(RESULT_IS_SOLVED(result));
    ASSERT_EQ_INT((int)RESULT_COUNT(result), 1);
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "2πm"));
    ASSERT_FALSE(test_equation_result_has_rhs_text_containing(result, style_UNBOUND, "2πn"));

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
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_LATEX, "\\sqrt[3]{"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(result, style_LATEX, "\\sqrt{"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(
        result, style_LATEX, "3\\mkern-2mu a\\mkern-2mu c - b^{2}"));
    ASSERT_TRUE(test_equation_result_has_rhs_text_containing(
        result, style_LATEX, "27\\mkern-2mu a\\mkern-2mu d - 9\\mkern-2mu b\\mkern-2mu c"));

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

static void test_equation_zeta_nontrivial_search(void)
{
    size_t previous_digits = num_get_default_prec_digits();
    equation_t *equation;
    equation_solutions_t *solutions;
    expr_t *s;
    number_t binding_value;
    size_t positive_count = 0u;
    static const double heights[] = {
        14.134725141734694, 21.022039638771555, 25.010857580145689, 30.424876125859513,
        32.935061587739190, 37.586178158825671, 40.918719012147495, 43.327073280914999,
        48.005150881167159, 49.773832477672302, 52.970321477714461, 56.446247697063395,
        59.347044002602353, 60.831778524609810, 65.112544048081607, 67.079810529494174,
        69.546401711173979, 72.067157674481908, 75.704690699083933, 77.144840068874805
    };
    bool found[sizeof(heights) / sizeof(heights[0])] = {false};

    num_set_default_prec_digits(78u);
    equation = equ_from_string("0=zeta(s)");
    ASSERT_NOT_NULL(equation);
    s = equ_binding(equation, "s");
    ASSERT_NOT_NULL(s);
    ASSERT_NULL(equ_interpretation_note(equation));
    solutions = equ_derive_solutions(equation);
    ASSERT_NOT_NULL(solutions);
    ASSERT_EQ_INT(equ_solutions_count(solutions), 40);
    ASSERT_NOT_NULL(equ_solutions_search_note(solutions));
    ASSERT_NOT_NULL(equ_solutions_family_note(solutions));
    ASSERT_TRUE(test_equation_all_solutions_satisfy(equation, s, solutions, "1e-70"));
    ASSERT_TRUE(test_equation_nonreal_solutions_have_conjugates(solutions));
    ASSERT_FALSE(test_equation_result_contains_long(solutions, -2L));
    binding_value = expr_eval(s);
    ASSERT_TRUE(num_is_nan(binding_value));
    num_destroy(&binding_value);
    for (size_t i = 0u; i < equ_solutions_count(solutions); ++i) {
        number_t root = expr_eval(equ_rhs(equ_solutions_at(solutions, i)));
        number_t real = num_real_part(root);
        number_t imaginary = num_imag_part(root);
        double height = num_to_double(imaginary);

        ASSERT_TRUE(num_gt(real, NUM_ZERO) && num_lt(real, NUM_ONE));
        ASSERT_TRUE(height >= -80.0 && height <= 80.0);
        if (height > 0.0) {
            positive_count++;
            for (size_t j = 0u; j < sizeof(heights) / sizeof(heights[0]); ++j)
                if (height > heights[j] - 1e-12 && height < heights[j] + 1e-12)
                    found[j] = true;
        }
        num_destroy(&imaginary);
        num_destroy(&real);
        num_destroy(&root);
    }
    ASSERT_EQ_INT(positive_count, 20);
    for (size_t j = 0u; j < sizeof(found) / sizeof(found[0]); ++j)
        ASSERT_TRUE(found[j]);
    equ_solutions_free(solutions);
    equ_free(equation);
    num_set_default_prec_digits(previous_digits);
}

static void test_equation_infinite_series_domain(void)
{
    static const struct {
        const char *source;
        bool series_on_left;
    } cases[] = {
        {"0=1/1^s+1/2^s+1/3^s+1/4^s+1/5^s+1/6^s+1/7^s+1/8^s+...", false},
        {"0=sum(n,1,inf,1/n^s)", false},
        {"0=@Z_(n=1)^inf 1/n^s", false},
        {"0=Σ_(n=1)^∞ 1/n^s", false},
        {"0=sum(n,1,inf,1/n^z)", false},
        {"-1=sum(k,2,inf,1/k^[exponent])", false},
        {"{0=sum(n,1,inf,1/n^s) | s=-2}", false},
        {"{0=sum(n,1,inf,1/n^s) | s=0.5+14i}", false},
        {"1/1^s+1/2^s+1/3^s+...=0", true},
        {"sum(n,1,inf,1/n^s)=0", true},
        {"@Z_(n=1)^inf 1/n^s=0", true},
        {"Σ_(n=1)^∞ 1/n^s=0", true},
        {"sum(n,1,inf,1/n^z)=0", true},
        {"sum(k,2,inf,1/k^[exponent])=-1", true},
        {"{sum(n,1,inf,1/n^s)=0 | s=-2}", true},
        {"{sum(n,1,inf,1/n^s)=0 | s=0.5+14i}", true}
    };

    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        equation_t *equation = equ_from_string(cases[i].source);
        equation_solutions_t *solutions;
        string_t *function;
        string_t *original_function;
        expr_t *unknown;
        number_t original_value;
        number_t after_value;

        ASSERT_NOT_NULL(equation);
        unknown = expr_bindings_expr_at(equ_bindings(equation), 0u);
        original_value = expr_eval(unknown);
        original_function = equ_to_text(equation, style_FUNCTION);
        ASSERT_NOT_NULL(original_function);
        solutions = equ_derive_solutions(equation);
        ASSERT_NOT_NULL(solutions);
        ASSERT_EQ_INT(equ_solutions_count(solutions), 0);
        ASSERT_TRUE(equ_solutions_proven_empty(solutions));
        ASSERT_NULL(equ_solutions_family_note(solutions));
        function = equ_to_text(equation, style_FUNCTION);
        ASSERT_NOT_NULL(function);
        ASSERT_NOT_NULL(strstr(string_c_str(function), "sum("));
        ASSERT_NULL(strstr(string_c_str(function), "zeta("));
        ASSERT_TRUE(strcmp(string_c_str(original_function), string_c_str(function)) == 0);
        after_value = expr_eval(unknown);
        ASSERT_TRUE(num_eq(original_value, after_value) || (num_is_nan(original_value) && num_is_nan(after_value)));
        num_destroy(&after_value);
        num_destroy(&original_value);
        string_free(original_function);
        string_free(function);
        equ_solutions_free(solutions);
        equ_free(equation);
    }
}

static void test_equation_zeta_level_search(void)
{
    size_t previous_digits = num_get_default_prec_digits();
    equation_t *equation;
    equation_solutions_t *solutions;
    expr_t *s;
    number_t value;

    num_set_default_prec_digits(78u);
    equation = equ_from_string(
        "0.624329988543550870992936383100837244179642620180529286973551902495638088855113254462460276"
        "1955398688691404103940627431 = 1/2^s+1/3^s+1/4^s+1/5^s+1/6^s+1/7^s+1/8^s+...");
    ASSERT_NOT_NULL(equation);
    s = equ_binding(equation, "s");
    solutions = equ_derive_solutions(equation);
    ASSERT_NOT_NULL(solutions);
    ASSERT_EQ_INT(equ_solutions_count(solutions), 9);
    ASSERT_NULL(equ_solutions_family_note(solutions));
    ASSERT_NOT_NULL(equ_solutions_search_note(solutions));
    ASSERT_TRUE(test_equation_all_solutions_satisfy(equation, s, solutions, "1e-70"));
    ASSERT_TRUE(test_equation_nonreal_solutions_have_conjugates(solutions));
    for (size_t i = 0u; i < equ_solutions_count(solutions); ++i) {
        number_t root = expr_eval(equ_rhs(equ_solutions_at(solutions, i)));
        number_t real = num_real_part(root);

        ASSERT_TRUE(num_gt(real, NUM_ONE));
        num_destroy(&real);
        num_destroy(&root);
    }
    value = expr_eval(s);
    ASSERT_TRUE(num_is_nan(value));
    num_destroy(&value);
    equ_solutions_free(solutions);
    equ_free(equation);

    /* Explicit seeds still request a single root and are not replaced by a grid search. */
    equation = equ_from_string("{zeta(w)=0 | w=0.5+14i}");
    ASSERT_NOT_NULL(equation);
    ASSERT_NULL(equ_interpretation_note(equation));
    solutions = equ_derive_solutions(equation);
    ASSERT_NOT_NULL(solutions);
    ASSERT_EQ_INT(equ_solutions_count(solutions), 1);
    ASSERT_NULL(equ_solutions_search_note(solutions));
    ASSERT_TRUE(test_equation_all_solutions_satisfy(equation, equ_binding(equation, "w"), solutions, "1e-70"));
    equ_solutions_free(solutions);
    equ_free(equation);
    num_set_default_prec_digits(previous_digits);
}

static void test_equation_basics(void)
{
    TEST_RUN_SUBTEST(test_equation_from_string_shares_symbols_across_sides, NULL);
    TEST_RUN_SUBTEST(test_equation_from_string_accepts_bare_equation, NULL);
    TEST_RUN_SUBTEST(test_equation_expands_algebraic_sequence_ellipsis, NULL);
    TEST_RUN_SUBTEST(test_equation_zeta_nontrivial_search, NULL);
    TEST_RUN_SUBTEST(test_equation_infinite_series_domain, NULL);
    TEST_RUN_SUBTEST(test_equation_zeta_level_search, NULL);
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
    TEST_RUN_SUBTEST(test_equation_renders_exact_complex_quadratic_roots_as_surds, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_atan_sum_as_branch_valid_surd, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_self_power_with_lambert_w, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_repeated_exponential_power_polynomial, NULL);
    TEST_RUN_SUBTEST(test_equation_simplifies_real_repeated_power_roots, NULL);
    TEST_RUN_SUBTEST(test_equation_solves_repeated_exp_polynomial, NULL);
    TEST_RUN_SUBTEST(test_equation_simplifies_negative_real_quartic_power_root, NULL);
    TEST_RUN_SUBTEST(test_equation_normalises_shifted_repeated_power_exponents, NULL);
    TEST_RUN_SUBTEST(test_equation_avoids_existing_branch_parameter_name, NULL);
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
