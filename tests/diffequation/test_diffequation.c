#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diffequation.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static bool test_diffequ_expect_text(const char *label, const char *actual, const char *expected, const char *file,
                                     int line)
{
    printf("  %s\n"
           "    expected: %s\n"
           "    actual:   %s\n",
           label, expected ? expected : "NULL", actual ? actual : "NULL");
    return test_assert_cstr_eq(actual, expected, file, line);
}

static bool test_diffequ_expect_pointer(const char *label, const void *actual, bool expected_nonnull, const char *file,
                                        int line)
{
    return test_diffequ_expect_text(label, actual ? "non-NULL" : "NULL", expected_nonnull ? "non-NULL" : "NULL", file,
                                    line);
}

static bool test_diffequ_expect_long(const char *label, long actual, long expected, const char *file, int line)
{
    printf("  %s\n"
           "    expected: %ld\n"
           "    actual:   %ld\n",
           label, expected, actual);
    return test_assert_long_eq(actual, expected, file, line);
}

static bool test_diffequ_expect_number(const char *label, number_t actual, number_t expected, const char *file,
                                       int line)
{
    string_t *actual_text = num_to_string(actual);
    string_t *expected_text = num_to_string(expected);
    bool equal;

    printf("  %s\n"
           "    expected: %s\n"
           "    actual:   %s\n",
           label, expected_text ? string_c_str(expected_text) : "NULL",
           actual_text ? string_c_str(actual_text) : "NULL");
    equal = actual_text && expected_text && num_eq(actual, expected);
    string_free(expected_text);
    string_free(actual_text);
    return test_assert_true(equal, file, line, label);
}

#define EXPECT_TEXT(label, actual, expected)                                                                           \
    TEST_HARNESS_RETURN_UNLESS(test_diffequ_expect_text((label), (actual), (expected), __FILE__, __LINE__))

#define EXPECT_POINTER(label, actual, expected_nonnull)                                                                \
    TEST_HARNESS_RETURN_UNLESS(test_diffequ_expect_pointer((label), (actual), (expected_nonnull), __FILE__, __LINE__))

#define EXPECT_LONG(label, actual, expected)                                                                           \
    TEST_HARNESS_RETURN_UNLESS(test_diffequ_expect_long((label), (actual), (expected), __FILE__, __LINE__))

#define EXPECT_NUMBER(label, actual, expected)                                                                         \
    TEST_HARNESS_RETURN_UNLESS(test_diffequ_expect_number((label), (actual), (expected), __FILE__, __LINE__))

static void test_diffequ_lifecycle_null_safety(void)
{
    diffequ_solve_result_t *invalid_result;

    EXPECT_POINTER("de_new(NULL)", de_new(NULL), false);
    EXPECT_POINTER("de_from_string(NULL)", de_from_string(NULL), false);
    EXPECT_POINTER("de_from_text(NULL)", de_from_text(NULL), false);
    EXPECT_POINTER("de_equation(NULL)", de_equation(NULL), false);
    EXPECT_LONG("de_independent_count(NULL)", (long)de_independent_count(NULL), 0L);
    EXPECT_POINTER("de_independent_at(NULL, 0)", de_independent_at(NULL, 0u), false);
    EXPECT_POINTER("de_constants(NULL)", de_constants(NULL), false);
    EXPECT_POINTER("de_constant(NULL, \"a\")", de_constant(NULL, "a"), false);
    EXPECT_LONG("de_condition_count(NULL)", (long)de_condition_count(NULL), 0L);
    EXPECT_POINTER("de_condition_at(NULL, 0)", de_condition_at(NULL, 0u), false);
    EXPECT_LONG("de_condition_argument_count(NULL, 0)", (long)de_condition_argument_count(NULL, 0u), 0L);
    EXPECT_POINTER("de_condition_argument_at(NULL, 0, 0)", de_condition_argument_at(NULL, 0u, 0u), false);
    invalid_result = de_solve(NULL);
    EXPECT_POINTER("de_solve(NULL)", invalid_result, true);
    EXPECT_LONG("de_solve(NULL) status", (long)de_solve_result_status(invalid_result), (long)DE_SOLVE_STATUS_INVALID);
    EXPECT_LONG("de_solve_result_count(NULL)", (long)de_solve_result_count(NULL), 0L);
    EXPECT_POINTER("de_solve_result_at(NULL, 0)", de_solve_result_at(NULL, 0u), false);

    de_solve_result_free(invalid_result);
    de_solve_result_free(NULL);
    de_free(NULL);
}

static void test_diffequ_derivations_are_opt_in(void)
{
    static const char *sources[] = {"Dx(y) = x*y", "y'' + 3*y*y' + y^3 = 0", "z_y + 2*y*z = x*y^3",
                                    "phi_xx + phi_yy = 0"};

    for (size_t i = 0u; i < sizeof(sources) / sizeof(sources[0]); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;

        EXPECT_POINTER("default solve result", result, true);
        EXPECT_LONG("default solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
        EXPECT_POINTER("default solve omits plain-text derivation", de_solve_result_steps(result), false);
        EXPECT_POINTER("default solve omits TeX derivation", de_solve_result_steps_TeX(result), false);

        de_solve_result_free(result);
        de_free(de);
    }

    {
        diffequ_t *de = de_from_string("Dx(y) = x*y");
        diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;

        EXPECT_POINTER("solve result with derivation", result, true);
        EXPECT_POINTER("opt-in plain-text derivation", de_solve_result_steps(result), true);
        EXPECT_POINTER("opt-in TeX derivation", de_solve_result_steps_TeX(result), true);

        de_solve_result_free(result);
        de_free(de);
    }
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
    diffequ_t *de = de_from_string("{ Dx(y) = x*y | x = ?;; y(0) = 1 }");
    string_t *base_text;
    char *text;

    EXPECT_POINTER("parsed separable ODE", de, true);
    if (!de)
        return;

    EXPECT_POINTER("base equation", de_equation(de), true);
    EXPECT_LONG("independent-variable count", (long)de_independent_count(de), 1L);
    EXPECT_POINTER("first independent variable", de_independent_at(de, 0u), true);
    EXPECT_LONG("condition count", (long)de_condition_count(de), 1L);
    EXPECT_POINTER("first condition", de_condition_at(de, 0u), true);

    base_text = equ_to_text(de_equation(de), style_UNBOUND);
    EXPECT_POINTER("rendered base equation", base_text, true);
    if (base_text)
        EXPECT_TEXT("rendered base equation text", string_c_str(base_text), "Dx(y) = xy");

    text = de_to_string(de, style_EXPRESSION);
    EXPECT_POINTER("rendered differential equation", text, true);
    if (text)
        EXPECT_TEXT("rendered differential-equation text", text, "{ dy/dx = x*y | x = ?; ; y(0) = 1 }");

    free(text);
    string_free(base_text);
    de_free(de);
}

static void test_diffequ_parses_linear_ode_and_constant(void)
{
    diffequ_t *de = de_from_string("{ Dx(y) + a*y = x | x = ?; a = 2; y(0) = 1 }");
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
    diffequ_t *first = de_from_string("{ Dxx(y) + 3*Dx(y) + 2*y = 0 | x = ?;; y(0) = 1, Dx(y)(0) = 0 }");
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
    diffequ_t *de = de_from_string("Dxx(y) = y; y(0) = 1; y'(0) = 1");
    char *text;

    EXPECT_POINTER("parsed shorthand ODE", de, true);
    if (!de)
        return;

    EXPECT_LONG("inferred independent-variable count", (long)de_independent_count(de), 1L);
    EXPECT_LONG("shorthand condition count", (long)de_condition_count(de), 2L);

    text = de_to_string(de, style_EXPRESSION);
    EXPECT_POINTER("normalized shorthand text", text, true);
    if (text)
        EXPECT_TEXT("normalized shorthand", text, "{ d²y/dx² = y | x = ?; ; y(0) = 1, dy/dx(0) = 1 }");

    free(text);
    de_free(de);
}

static void test_diffequ_parses_greek_differential_forms(void)
{
    const char *alias_source = "(sin(@theta)-2r^2 cos^2(@theta))dr + "
                               "r cos(@theta)(2r sin(@theta)+1)d@theta = 0";
    const char *plain_source = "(sin(theta)-2r^2 cos^2(theta))dr + "
                               "r cos(theta)(2r sin(theta)+1)dtheta = 0";
    diffequ_t *alias = de_from_string(alias_source);
    diffequ_t *plain = de_from_string(plain_source);
    diffequ_t *alpha = de_from_string("(alpha+y)dy + y dalpha = 0");
    char *alias_text = alias ? de_to_string(alias, style_EXPRESSION) : NULL;
    char *plain_text = plain ? de_to_string(plain, style_EXPRESSION) : NULL;
    char *alias_TeX = alias ? de_to_string(alias, style_LATEX) : NULL;
    char *plain_TeX = plain ? de_to_string(plain, style_LATEX) : NULL;
    char *alpha_text = alpha ? de_to_string(alpha, style_EXPRESSION) : NULL;
    diffequ_t *round_trip = alias_text ? de_from_string(alias_text) : NULL;
    char *round_trip_TeX = round_trip ? de_to_string(round_trip, style_LATEX) : NULL;

    EXPECT_POINTER("parsed @theta differential form", alias, true);
    EXPECT_POINTER("parsed plain theta differential form", plain, true);
    EXPECT_POINTER("parsed another plain Greek differential", alpha, true);
    EXPECT_POINTER("Greek derivative text round-trips", round_trip, true);
    EXPECT_LONG("@theta differential-form independent count", (long)de_independent_count(alias), 1L);
    EXPECT_LONG("plain theta differential-form independent count", (long)de_independent_count(plain), 1L);
    EXPECT_TEXT("Greek differential forms agree", plain_TeX, alias_TeX);
    EXPECT_TEXT("Greek derivative round-trip agrees", round_trip_TeX, alias_TeX);
    EXPECT_POINTER("@theta differential form displays Greek theta", alias_text ? strstr(alias_text, "dθ") : NULL, true);
    EXPECT_POINTER("plain theta differential form displays Greek theta", plain_text ? strstr(plain_text, "dθ") : NULL,
                   true);
    EXPECT_POINTER("plain alpha differential form displays Greek alpha", alpha_text ? strstr(alpha_text, "dα") : NULL,
                   true);
    EXPECT_TEXT("Greek differential-form text agrees", plain_text, alias_text);

    free(alpha_text);
    free(round_trip_TeX);
    free(plain_TeX);
    free(alias_TeX);
    free(plain_text);
    free(alias_text);
    de_free(plain);
    de_free(alias);
    de_free(alpha);
    de_free(round_trip);
}

static void test_diffequ_solves_exact_differential_form(void)
{
    const char *source = "(sin(theta)-2r cos^2(theta))dr + "
                         "r cos(theta)(2r sin(theta)+1)dtheta = 0";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *positive_solution = result ? de_solve_result_at(result, 0u) : NULL;
    const equation_t *negative_solution = result ? de_solve_result_at(result, 1u) : NULL;
    string_t *positive_text = positive_solution ? equ_to_text(positive_solution, style_UNBOUND) : NULL;
    string_t *negative_text = negative_solution ? equ_to_text(negative_solution, style_UNBOUND) : NULL;

    EXPECT_POINTER("parsed exact differential form", de, true);
    EXPECT_POINTER("exact differential-form result", result, true);
    EXPECT_LONG("exact differential-form status", result ? (long)de_solve_result_status(result) : -1L,
                (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("exact differential-form solver", result ? (long)de_solve_result_solver(result) : -1L,
                (long)DE_SOLVER_EXACT_FIRST_ORDER);
    EXPECT_LONG("exact differential-form solution count", result ? (long)de_solve_result_count(result) : -1L, 2L);
    EXPECT_TEXT("exact differential-form positive branch", positive_text ? string_c_str(positive_text) : NULL,
                "r = (sin(θ) - √(sin²(θ) - C·cos²(θ)))/(2·cos²(θ))");
    EXPECT_TEXT("exact differential-form negative branch", negative_text ? string_c_str(negative_text) : NULL,
                "r = (sin(θ) + √(sin²(θ) - C·cos²(θ)))/(2·cos²(θ))");

    string_free(negative_text);
    string_free(positive_text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_applies_initial_condition_to_exact_differential_form(void)
{
    diffequ_t *de = de_from_string("(x^2+y^2)dx + 2xy dy = 0; y(2) = 1");
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    string_t *solution_text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;

    EXPECT_POINTER("parsed conditioned exact differential form", de, true);
    EXPECT_POINTER("conditioned exact differential-form result", result, true);
    EXPECT_LONG("conditioned exact differential-form status", result ? (long)de_solve_result_status(result) : -1L,
                (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("conditioned exact differential-form solver", result ? (long)de_solve_result_solver(result) : -1L,
                (long)DE_SOLVER_EXACT_FIRST_ORDER);
    EXPECT_LONG("conditioned exact differential-form solution count",
                result ? (long)de_solve_result_count(result) : -1L, 1L);
    EXPECT_TEXT("conditioned exact differential-form branch", solution_text ? string_c_str(solution_text) : NULL,
                "y = √(1/(3x)·(14 - x³))");

    string_free(solution_text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_parses_and_solves_prime_ode_shorthand(void)
{
    const char *source = "y'' + 4y = 0";
    diffequ_t *de = de_from_string(source);
    diffequ_t *explicit = de_from_string("{ y'' + 4y = 0 | t = ?;; }");
    diffequ_t *forced = de_from_string("y'' + 4y = e^x");
    diffequ_t *time_dependent = de_from_string("x'' + x = 0");
    diffequ_t *fraction = de_from_string("d²y/dx² + 4y = 0");
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    diffequ_solve_result_t *explicit_result = explicit ? de_solve(explicit) : NULL;
    diffequ_solve_result_t *forced_result = forced ? de_solve(forced) : NULL;
    diffequ_solve_result_t *time_dependent_result = time_dependent ? de_solve(time_dependent) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    const equation_t *explicit_solution = explicit_result ? de_solve_result_at(explicit_result, 0u) : NULL;
    const equation_t *forced_solution = forced_result ? de_solve_result_at(forced_result, 0u) : NULL;
    const equation_t *time_dependent_solution =
        time_dependent_result ? de_solve_result_at(time_dependent_result, 0u) : NULL;
    string_t *equation_text = de ? equ_to_text(de_equation(de), style_UNBOUND) : NULL;
    string_t *solution_text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    string_t *explicit_equation_text = explicit ? equ_to_text(de_equation(explicit), style_UNBOUND) : NULL;
    string_t *explicit_solution_text = explicit_solution ? equ_to_text(explicit_solution, style_UNBOUND) : NULL;
    string_t *forced_equation_text = forced ? equ_to_text(de_equation(forced), style_UNBOUND) : NULL;
    string_t *forced_solution_text = forced_solution ? equ_to_text(forced_solution, style_UNBOUND) : NULL;
    string_t *time_dependent_equation_text =
        time_dependent ? equ_to_text(de_equation(time_dependent), style_UNBOUND) : NULL;
    string_t *time_dependent_solution_text =
        time_dependent_solution ? equ_to_text(time_dependent_solution, style_UNBOUND) : NULL;
    string_t *fraction_equation_text = fraction ? equ_to_text(de_equation(fraction), style_UNBOUND) : NULL;
    char *fraction_TeX = fraction ? de_to_string(fraction, style_LATEX) : NULL;

    EXPECT_POINTER("parsed prime-notation ODE", de, true);
    EXPECT_TEXT("normalized prime-notation ODE", equation_text ? string_c_str(equation_text) : NULL, "Dxx(y) + 4y = 0");
    EXPECT_LONG("prime-notation solve status", result ? (long)de_solve_result_status(result) : -1L,
                (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_TEXT("prime-notation solution", solution_text ? string_c_str(solution_text) : NULL,
                "y = C₁·cos(2x) + C₂·sin(2x)");
    EXPECT_TEXT("prime notation uses the declared independent variable",
                explicit_equation_text ? string_c_str(explicit_equation_text) : NULL, "Dtt(y) + 4y = 0");
    EXPECT_TEXT("declared-variable prime-notation solution",
                explicit_solution_text ? string_c_str(explicit_solution_text) : NULL, "y = C₁·cos(2t) + C₂·sin(2t)");
    EXPECT_TEXT("prime notation accepts the standard constant e",
                forced_equation_text ? string_c_str(forced_equation_text) : NULL, "Dxx(y) + 4y = exp(x)");
    EXPECT_LONG("exponential-forcing solve status", forced_result ? (long)de_solve_result_status(forced_result) : -1L,
                (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_TEXT("compact exponential-forcing solution",
                forced_solution_text ? string_c_str(forced_solution_text) : NULL,
                "y = ⅕·exp(x) + C₁·cos(2x) + C₂·sin(2x)");
    EXPECT_TEXT("prime x defaults to differentiation with respect to t",
                time_dependent_equation_text ? string_c_str(time_dependent_equation_text) : NULL, "Dtt(x) + x = 0");
    EXPECT_TEXT("time-dependent prime-notation solution",
                time_dependent_solution_text ? string_c_str(time_dependent_solution_text) : NULL,
                "x = C₁·cos(t) + C₂·sin(t)");
    EXPECT_TEXT("ordinary derivative fraction input",
                fraction_equation_text ? string_c_str(fraction_equation_text) : NULL, "Dxx(y) + 4y = 0");
    EXPECT_TEXT("ordinary derivative fraction TeX", fraction_TeX, "\\frac{d^{2} y}{d x^{2}} + 4 y = 0");

    free(fraction_TeX);
    string_free(fraction_equation_text);
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
    de_free(fraction);
    de_free(forced);
    de_free(explicit);
    de_free(de);
}

static void test_diffequ_parses_subscript_partial_derivatives(void)
{
    const char *first_source = "u_x + u_y = 0";
    const char *mixed_source = "u_xy = 0";
    diffequ_t *first = de_from_string(first_source);
    diffequ_t *mixed = de_from_string(mixed_source);
    diffequ_t *nested = de_from_string("Dy(Dx(u)) = 0");
    diffequ_t *greek = de_from_string("phi_x + phi_y = 0");
    diffequ_t *unicode_first = de_from_string("∂u/∂x + ∂u/∂y = 0");
    diffequ_t *unicode_mixed = de_from_string("∂²u/∂y∂x = 0");
    diffequ_t *unicode_repeated = de_from_string("∂²u/∂x² + ∂²u/∂y² = 0");
    diffequ_t *compact = de_from_string("x*(y-z)*z_x + y*(z-x)*z_y = z*(x-y)");
    string_t *first_text = first ? equ_to_text(de_equation(first), style_UNBOUND) : NULL;
    string_t *mixed_text = mixed ? equ_to_text(de_equation(mixed), style_UNBOUND) : NULL;
    string_t *nested_text = nested ? equ_to_text(de_equation(nested), style_UNBOUND) : NULL;
    char *greek_TeX = greek ? de_to_string(greek, style_LATEX) : NULL;
    char *mixed_TeX = mixed ? de_to_string(mixed, style_LATEX) : NULL;
    string_t *unicode_first_text = unicode_first ? equ_to_text(de_equation(unicode_first), style_UNBOUND) : NULL;
    string_t *unicode_mixed_text = unicode_mixed ? equ_to_text(de_equation(unicode_mixed), style_UNBOUND) : NULL;
    string_t *unicode_repeated_text =
        unicode_repeated ? equ_to_text(de_equation(unicode_repeated), style_UNBOUND) : NULL;
    char *compact_TeX = compact ? de_to_string(compact, style_LATEX) : NULL;

    printf("  subscript partial-derivative shorthand\n"
           "    input:    %s\n"
           "    expected: %s\n"
           "    actual:   %s\n"
           "    input:    %s\n"
           "    expected: %s\n"
           "    actual:   %s\n",
           first_source, "Dx(u) + Dy(u) = 0", first_text ? string_c_str(first_text) : "NULL", mixed_source,
           "Dxy(u) = 0", mixed_text ? string_c_str(mixed_text) : "NULL");
    EXPECT_POINTER("parsed first partial derivatives", first, true);
    EXPECT_LONG("first-partial independent-variable count", (long)de_independent_count(first), 2L);
    EXPECT_TEXT("normalized first partial derivatives", first_text ? string_c_str(first_text) : NULL,
                "Dx(u) + Dy(u) = 0");
    EXPECT_POINTER("parsed mixed partial derivative", mixed, true);
    EXPECT_LONG("mixed-partial independent-variable count", (long)de_independent_count(mixed), 2L);
    EXPECT_TEXT("normalized mixed partial derivative", mixed_text ? string_c_str(mixed_text) : NULL, "Dxy(u) = 0");
    EXPECT_TEXT("u_xy agrees with Dy(Dx(u))", mixed_text ? string_c_str(mixed_text) : NULL,
                nested_text ? string_c_str(nested_text) : NULL);
    EXPECT_TEXT("mixed-partial TeX notation", mixed_TeX, "\\frac{\\partial^{2} u}{\\partial y\\,\\partial x} = 0");
    EXPECT_TEXT("Greek-name subscript derivative", greek_TeX,
                "\\frac{\\partial \\phi}{\\partial x} + "
                "\\frac{\\partial \\phi}{\\partial y} = 0");
    EXPECT_TEXT("Unicode first partial derivatives", unicode_first_text ? string_c_str(unicode_first_text) : NULL,
                "Dx(u) + Dy(u) = 0");
    EXPECT_TEXT("Unicode mixed partial derivative", unicode_mixed_text ? string_c_str(unicode_mixed_text) : NULL,
                "Dxy(u) = 0");
    EXPECT_TEXT("Unicode repeated partial derivatives",
                unicode_repeated_text ? string_c_str(unicode_repeated_text) : NULL, "Dxx(u) + Dyy(u) = 0");
    EXPECT_TEXT("visually short PDE stays on one line", compact_TeX,
                "x \\cdot \\left(y - z\\right) \\cdot "
                "\\frac{\\partial z}{\\partial x} + "
                "y \\cdot \\left(z - x\\right) \\cdot "
                "\\frac{\\partial z}{\\partial y} = "
                "z \\cdot \\left(x - y\\right)");

    free(compact_TeX);
    string_free(unicode_repeated_text);
    string_free(unicode_mixed_text);
    string_free(unicode_first_text);
    free(mixed_TeX);
    free(greek_TeX);
    string_free(nested_text);
    string_free(mixed_text);
    string_free(first_text);
    de_free(nested);
    de_free(greek);
    de_free(unicode_repeated);
    de_free(unicode_mixed);
    de_free(unicode_first);
    de_free(compact);
    de_free(mixed);
    de_free(first);
}

static void test_diffequ_rejects_noncanonical_text(void)
{
    diffequ_t *missing_derivative = de_from_string("y = 1");
    diffequ_t *missing_section = de_from_string("{ Dx(y) = y | x = ?; }");

    EXPECT_POINTER("shorthand without a derivative", missing_derivative, false);
    EXPECT_POINTER("explicit form missing a section", missing_section, false);
    de_free(missing_section);
    de_free(missing_derivative);
}

static void test_diffequ_solves_separable_initial_value_problem(void)
{
    diffequ_t *de = de_from_string("Dx(y) = x*y; y(0) = 1");
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

    EXPECT_LONG("separable solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_SEPARABLE);
    EXPECT_LONG("separable solution count", (long)de_solve_result_count(result), 1L);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("separable solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("separable solution text", text, true);
    if (text)
        EXPECT_TEXT("separable solution", string_c_str(text), "y = exp(½x²)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_linear_initial_value_problem(void)
{
    diffequ_t *de = de_from_string("Dx(y) + y = x; y(0) = 0");
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

    EXPECT_LONG("linear solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_LINEAR);
    EXPECT_LONG("linear solution count", (long)de_solve_result_count(result), 1L);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("linear solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("linear solution text", text, true);
    if (text)
        EXPECT_TEXT("linear solution", string_c_str(text), "y = ((x - 1)·exp(x) + 1)/exp(x)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_quadratic_separable_problem(void)
{
    diffequ_t *de = de_from_string("Dx(y) = x*y^2; y(0) = 1");
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

    EXPECT_LONG("quadratic separable solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_SEPARABLE);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("quadratic separable solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("quadratic separable solution text", text, true);
    if (text)
        EXPECT_TEXT("quadratic separable solution", string_c_str(text), "y = -1/(½x² - 1)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_variable_coefficient_linear_problem(void)
{
    diffequ_t *de = de_from_string("Dx(y) + 2*x*y = exp(-x^2); y(0) = 0");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER("parsed variable-coefficient linear problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("variable-coefficient linear solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG("variable-coefficient linear solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_LINEAR);
    EXPECT_TEXT("linear solver diagnostic", de_solve_result_diagnostic(result), "solved as a first-order linear ODE");
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("variable-coefficient linear solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("variable-coefficient linear solution text", text, true);
    if (text)
        EXPECT_TEXT("variable-coefficient linear solution", string_c_str(text), "y = x/exp(x²)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_rational_integrating_factor_problem(void)
{
    diffequ_t *de = de_from_string("Dx(y) + y/x = x^2; y(1) = 1");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER("parsed rational integrating-factor problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("rational integrating-factor solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG("rational integrating-factor solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_LINEAR);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("rational integrating-factor solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("rational integrating-factor solution text", text, true);
    if (text)
        EXPECT_TEXT("rational integrating-factor solution", string_c_str(text), "y = (x⁴ + 3)/(4x)");

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

    EXPECT_LONG("unconditioned linear solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_LINEAR);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("unconditioned linear solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("unconditioned linear solution text", text, true);
    if (text)
        EXPECT_TEXT("unconditioned linear solution", string_c_str(text), "y = (C + (x - 1)·exp(x))/exp(x)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_linear_solution_uses_special_function(void)
{
    diffequ_t *de = de_from_string("Dx(y) + 2*x*y = 1; y(0) = 0");
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

    EXPECT_LONG("non-elementary linear solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_LINEAR);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("special-function linear solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("special-function solution text", text, true);
    if (text)
        EXPECT_TEXT("special-function linear solution", string_c_str(text),
                    "y = ½·√(π)·(erf(x·√(-1)) - erf(0))/(√(-1)·exp(x²))");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_linear_solution_retains_formal_integral(void)
{
    diffequ_t *de = de_from_string("Dx(y) + y = exp(cosh(x)); y(0) = 0");
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

    EXPECT_LONG("formal-integral linear solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_LINEAR);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("formal-integral linear solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("formal-integral solution text", text, true);
    if (text)
        EXPECT_TEXT("formal-integral linear solution", string_c_str(text), "y = ∫^x exp(cosh(t) + t)·dt/exp(x)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_linear_solution_retains_formal_factor(void)
{
    diffequ_t *de = de_from_string("Dx(y) + exp(cosh(x))*y = 0; y(0) = 1");
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

    EXPECT_LONG("formal-factor linear solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_LINEAR);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("formal-factor linear solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("formal-factor solution text", text, true);
    if (text)
        EXPECT_TEXT("formal-factor linear solution", string_c_str(text), "y = 1/exp(∫^x exp(cosh(t))·dt)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_first_order_homogeneous_problem(void)
{
    diffequ_t *de = de_from_string("Dx(y) = y/x + x/y; y(1) = 1");
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

    EXPECT_LONG("homogeneous solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_HOMOGENEOUS);
    EXPECT_TEXT("homogeneous solver diagnostic", de_solve_result_diagnostic(result),
                "solved as a first-order homogeneous ODE");
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("homogeneous solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("homogeneous solution text", text, true);
    if (text)
        EXPECT_TEXT("homogeneous solution", string_c_str(text), "½·(y/x)² = ln(|x|) + ½");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_homogeneous_solution_retains_constant(void)
{
    diffequ_t *de = de_from_string("Dx(y) = y/x + x/y");
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

    EXPECT_LONG("unconditioned homogeneous solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_HOMOGENEOUS);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("unconditioned homogeneous solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("unconditioned homogeneous solution text", text, true);
    if (text)
        EXPECT_TEXT("unconditioned homogeneous solution", string_c_str(text), "½·(y/x)² = ln(|x|) + C");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_polynomial_homogeneous_initial_value_problem(void)
{
    diffequ_t *de = de_from_string("x*(x^3-x*y^2+2*y^3)*y' - y*(x^3+2*y^3) = 0; y(1) = 1");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER("parsed polynomial homogeneous ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("polynomial homogeneous solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG("polynomial homogeneous solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("polynomial homogeneous selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_HOMOGENEOUS);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("polynomial homogeneous solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("polynomial homogeneous solution text", text, true);
    if (text)
        EXPECT_TEXT("polynomial homogeneous initial-value solution", string_c_str(text),
                    "-½·1/(y/x)² - ln(y/x) + 2·y/x = ln(|x|) + ³⁄₂");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_integrates_rational_homogeneous_problem(void)
{
    diffequ_t *de = de_from_string("y*(8*x-9*y) + 2*x*(x-3*y)*y' = 0");
    diffequ_solve_result_t *result;
    const equation_t *first_solution;
    const equation_t *second_solution;
    string_t *first_text;
    string_t *second_text;

    EXPECT_POINTER("parsed rational homogeneous ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("rational homogeneous solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG("rational homogeneous solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("rational homogeneous selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_HOMOGENEOUS);
    EXPECT_LONG("rational homogeneous solution count", (long)de_solve_result_count(result), 2L);
    first_solution = de_solve_result_at(result, 0u);
    second_solution = de_solve_result_at(result, 1u);
    EXPECT_POINTER("first rational homogeneous solution", first_solution, true);
    EXPECT_POINTER("second rational homogeneous solution", second_solution, true);
    first_text = first_solution ? equ_to_text(first_solution, style_UNBOUND) : NULL;
    second_text = second_solution ? equ_to_text(second_solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("first rational homogeneous solution text", first_text, true);
    EXPECT_POINTER("second rational homogeneous solution text", second_text, true);
    if (first_text)
        EXPECT_TEXT("first explicit rational homogeneous solution", string_c_str(first_text),
                    "y = -⅓·(√(x² - 3C/x³) - x)");
    if (second_text)
        EXPECT_TEXT("second explicit rational homogeneous solution", string_c_str(second_text),
                    "y = ⅓·(√(x² - 3C/x³) + x)");

    string_free(second_text);
    string_free(first_text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_affine_combination_substitution(void)
{
    diffequ_t *de = de_from_string("Dx(y) = (x + y)^2; y(0) = 1");
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

    EXPECT_LONG("affine-substitution solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_LINEAR_SUBSTITUTION);
    EXPECT_TEXT("linear-substitution solver diagnostic", de_solve_result_diagnostic(result),
                "solved by a first-order linear substitution");
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("affine-substitution solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("affine-substitution solution text", text, true);
    if (text)
        EXPECT_TEXT("affine-substitution solution", string_c_str(text), "atan(x + y) = x + π/4");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_shifted_homogeneous_substitution(void)
{
    diffequ_t *de = de_from_string("Dx(y) = ((y - 1)/(x + 2))^2 + "
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

    EXPECT_LONG("shifted-homogeneous solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_LINEAR_SUBSTITUTION);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("shifted-homogeneous solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("shifted-homogeneous solution text", text, true);
    if (text)
        EXPECT_TEXT("shifted-homogeneous solution", string_c_str(text), "-(x + 2)/(y - 1) = ln(|x + 2|) - ln(2) - 1");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_linear_change_of_variables(void)
{
    diffequ_t *de = de_from_string("Dx(y) = (1 - (x + y)*exp(x - y))/"
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

    EXPECT_LONG("linear-transformation solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_LINEAR_TRANSFORMATION);
    EXPECT_TEXT("linear-transformation solver diagnostic", de_solve_result_diagnostic(result),
                "solved by a linear change of variables");
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("linear-transformation solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("linear-transformation solution text", text, true);
    if (text)
        EXPECT_TEXT("linear-transformation solution", string_c_str(text), "½·(x + y)² = 1 - exp(-(x - y))");

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

    EXPECT_LONG("unconditioned solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("unconditioned solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("unconditioned solution text", text, true);
    if (text)
        EXPECT_TEXT("arbitrary integration constant", string_c_str(text), "y = C·exp(½x²)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_preserves_zero_singular_solution(void)
{
    diffequ_t *de = de_from_string("Dx(y) = x*y^2; y(0) = 0");
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

    EXPECT_LONG("zero initial-value solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("zero singular solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("zero singular solution text", text, true);
    if (text)
        EXPECT_TEXT("zero singular solution", string_c_str(text), "y = 0");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_quadratic_bernoulli_problem(void)
{
    diffequ_t *de = de_from_string("Dx(y) + y = x*y^2; y(0) = 1");
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

    EXPECT_LONG("Bernoulli solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_BERNOULLI);
    EXPECT_LONG("Bernoulli solution count", (long)de_solve_result_count(result), 1L);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("Bernoulli solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("Bernoulli solution text", text, true);
    if (text)
        EXPECT_TEXT("Bernoulli solution", string_c_str(text), "y = 1/(x + 1)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_normalizes_bernoulli_arbitrary_constant(void)
{
    const char *source = "Dx(y) - 2*y = y^2";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER("parsed unconditioned Bernoulli problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("unconditioned Bernoulli result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG("unconditioned Bernoulli status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("unconditioned Bernoulli selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_BERNOULLI);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("unconditioned Bernoulli solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("unconditioned Bernoulli solution text", text, true);
    if (text)
        EXPECT_TEXT("normalized Bernoulli arbitrary constant", string_c_str(text), "y = 2·exp(2x)/(C - exp(2x))");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_derivative_quadratic_problem(void)
{
    static const char *expected[] = {"x = ½·(√(8y + 1) - "
                                     "ln(|½·(√(8y + 1) + 1)|) + 1) + C",
                                     "x = ½·(1 - √(8y + 1) - "
                                     "ln(|½·(1 - √(8y + 1))|)) + C",
                                     "y = 0"};
    diffequ_t *de = de_from_string("(y')^2 = y' + 2y");
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

    EXPECT_LONG("derivative-quadratic solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("derivative-quadratic selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_DERIVATIVE_QUADRATIC);
    EXPECT_LONG("derivative-quadratic solution count", (long)de_solve_result_count(result), 3L);

    for (size_t i = 0u; i < 3u; ++i) {
        const equation_t *solution = de_solve_result_at(result, i);
        string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;

        EXPECT_POINTER("derivative-quadratic solution", solution, true);
        EXPECT_TEXT("derivative-quadratic solution text", text ? string_c_str(text) : NULL, expected[i]);
        string_free(text);
    }

    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_linearizes_exact_third_order_problem(void)
{
    static const char *expected[] = {"y = 2·ln(|Σ_(n=0)^∞ c_(n)·x^n|)",
                                     "c_(0) = C₂",
                                     "c_(1) = C₃",
                                     "c_(-1) = 0",
                                     "c_(-2) = 0",
                                     "c_(-3) = 0",
                                     "c_(n + 2) = (C₁·c_(n) + c_(n - 3))/(2·(n + 2)·(n + 1))"};
    diffequ_t *de = de_from_string("y''' + y''*y' = 3x^2");
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

    EXPECT_LONG("exact third-order solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("exact third-order selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_EXACT_DERIVATIVE_LINEARIZATION);
    EXPECT_TEXT("exact third-order solver diagnostic", de_solve_result_diagnostic(result),
                "linearized exactly, then solved by a convergent "
                "power-series recurrence");
    EXPECT_LONG("exact third-order solution count", (long)de_solve_result_count(result), 7L);

    for (size_t i = 0u; i < 7u; ++i) {
        const equation_t *solution = de_solve_result_at(result, i);
        string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;

        EXPECT_POINTER("exact third-order solution", solution, true);
        EXPECT_TEXT("exact third-order solution text", text ? string_c_str(text) : NULL, expected[i]);
        string_free(text);
    }

    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_second_order_sturm_liouville_problem(void)
{
    diffequ_t *de = de_from_string("Dxx(y) = y; y(0) = 1; y'(0) = 1");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    EXPECT_POINTER("parsed second-order problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("Sturm-Liouville solve result", result, true);
    if (result) {
        EXPECT_LONG("Sturm-Liouville solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
        EXPECT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_STURM_LIOUVILLE);
        EXPECT_LONG("Sturm-Liouville solution count", (long)de_solve_result_count(result), 1L);
        EXPECT_TEXT("Sturm-Liouville diagnostic", de_solve_result_diagnostic(result),
                    "solved as a second-order linear Sturm-Liouville equation");
    }
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("Sturm-Liouville solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("Sturm-Liouville solution text", text, true);
    if (text)
        EXPECT_TEXT("Sturm-Liouville solution", string_c_str(text), "y = exp(x)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_linearizes_modified_emden_problem(void)
{
    const char *source = "y'' + 3yy' + y^3 = 0";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
    const char *expected = "y = (2x + C₁)/(x² + C₁x + C₂)";

    EXPECT_POINTER("parsed modified-Emden problem", de, true);
    EXPECT_POINTER("modified-Emden solve result", result, true);
    EXPECT_LONG("modified-Emden solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("modified-Emden selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_LINEAR_TRANSFORMATION);
    EXPECT_TEXT("modified-Emden diagnostic", de_solve_result_diagnostic(result),
                "linearized by y = u'/u, then solved as u''' = 0");
    EXPECT_POINTER("modified-Emden derivation", de_solve_result_steps(result), true);
    EXPECT_TEXT("modified-Emden symmetry", de_solve_result_symmetry(result), "SL(3, ℝ)");
    if (de_solve_result_steps(result))
        EXPECT_POINTER("modified-Emden derivation contains transformed ODE",
                       strstr(de_solve_result_steps(result), "d²Y/dX² = 0"), true);
    if (de_solve_result_steps_TeX(result)) {
        EXPECT_POINTER("modified-Emden TeX uses the dependent symbol directly",
                       strstr(de_solve_result_steps_TeX(result), "y''+3(1)yy'"), true);
        EXPECT_POINTER("modified-Emden TeX omits binding wrappers",
                       strstr(de_solve_result_steps_TeX(result), "\\middle|"), false);
        EXPECT_POINTER("modified-Emden TeX omits unbound sentinel values",
                       strstr(de_solve_result_steps_TeX(result), "NAN"), false);
    }
    EXPECT_LONG("modified-Emden solution count", (long)de_solve_result_count(result), 1L);
    {
        const equation_t *solution = de_solve_result_at(result, 0u);
        string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;

        EXPECT_POINTER("modified-Emden solution", solution, true);
        EXPECT_POINTER("modified-Emden solution text", text, true);
        if (text)
            EXPECT_TEXT("modified-Emden solution text", string_c_str(text), expected);
        string_free(text);
    }

    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_rejects_quartic_emden_point_linearization(void)
{
    diffequ_t *de = de_from_string("y'' + 3*y*y' + y^4 = 0");
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;

    EXPECT_POINTER("parsed quartic Emden problem", de, true);
    EXPECT_POINTER("quartic Emden solve result", result, true);
    EXPECT_LONG("quartic Emden solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_UNSUPPORTED);
    EXPECT_TEXT("quartic Emden point-linearization diagnostic", de_solve_result_diagnostic(result),
                "not point-linearizable: the Lie–Tressé invariant "
                "36y(1 − 2y) is not identically zero");

    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_linearizes_scaled_modified_emden_problem(void)
{
    diffequ_t *de = de_from_string("y'' + 6*y*y' + 4*y^3 = 0");
    diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;

    EXPECT_POINTER("scaled modified-Emden result", result, true);
    EXPECT_LONG("scaled modified-Emden status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_TEXT("scaled modified-Emden symmetry", de_solve_result_symmetry(result), "SL(3, ℝ)");
    EXPECT_POINTER("scaled modified-Emden X substitution", strstr(de_solve_result_steps(result), "X = x − 1/(2y)"),
                   true);
    if (text)
        EXPECT_TEXT("scaled modified-Emden solution", string_c_str(text), "y = (2x + C₁)/(2·(x² + C₁x + C₂))");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_hydrogen_ground_state(void)
{
    const char *source = "i*Dt(@psi) = -1/2*(Dxx(@psi) + Dyy(@psi) + Dzz(@psi)) "
                         "- @psi/sqrt(x^2+y^2+z^2); "
                         "@psi(x,y,z,0) = exp(-sqrt(x^2+y^2+z^2))/sqrt(pi)";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;

    EXPECT_POINTER("hydrogen ground-state result", result, true);
    EXPECT_LONG("hydrogen ground-state status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("stationary eigenfunction solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_STATIONARY_EIGENFUNCTION);
    EXPECT_POINTER("hydrogen eigenfunction steps", strstr(de_solve_result_steps(result), "derived constant rate"),
                   true);
    if (text)
        EXPECT_TEXT("hydrogen ground-state wavefunction", string_c_str(text), "ψ = exp(0.5it - √(x² + y² + z²))/√(π)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_modified_emden_uses_coefficient_rule(void)
{
    diffequ_t *de = de_from_string("y'' + 9*y*y' + 9*y^3 = 0");
    diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;

    EXPECT_LONG("coefficient-derived Emden status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_POINTER("coefficient-derived Emden scale", strstr(de_solve_result_steps(result), "3(3)"), true);
    EXPECT_TEXT("coefficient-derived Emden solution", text ? string_c_str(text) : NULL,
                "y = (2x + C₁)/(3·(x² + C₁x + C₂))");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_stationary_eigenfunction_uses_general_rule(void)
{
    const char *source = "Dt(u) = Dxx(u); u(x,0) = exp(2*x)";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;

    EXPECT_POINTER("stationary rule result", result, true);
    EXPECT_LONG("stationary rule status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("stationary rule solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_STATIONARY_EIGENFUNCTION);
    EXPECT_TEXT("stationary rule solution", text ? string_c_str(text) : NULL, "u = exp(2·(2t + x))");
    EXPECT_POINTER("stationary rule derived rate", strstr(de_solve_result_steps(result), "λ ="), true);

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_affine_factorized_second_order_problem(void)
{
    const char *source = "y'' - (x^2+1)*y = 0";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    printf("  differential equation\n    input:    %s\n", source);
    EXPECT_POINTER("parsed affine-factorized problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("affine-factorized solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG("affine-factorized solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("affine-factorized selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_STURM_LIOUVILLE);
    EXPECT_LONG("affine-factorized solution count", (long)de_solve_result_count(result), 1L);
    EXPECT_TEXT("affine-factorized diagnostic", de_solve_result_diagnostic(result),
                "solved as a second-order linear Sturm-Liouville equation");

    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("affine-factorized solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("affine-factorized solution text", text, true);
    if (text) {
        printf("    expected: y = exp(½x²)·(C₁ + C₂·erf(x))\n"
               "    actual:   %s\n",
               string_c_str(text));
        EXPECT_TEXT("affine-factorized solution text", string_c_str(text), "y = exp(½x²)·(C₁ + C₂·erf(x))");
    }

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_applies_affine_factorized_initial_conditions(void)
{
    const char *source = "y'' - (x^2+1)*y = 0; y(0) = 1; y'(0) = 0";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    printf("  differential equation\n    input:    %s\n", source);
    EXPECT_POINTER("parsed affine-factorized IVP", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("affine-factorized IVP solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    EXPECT_LONG("affine-factorized IVP solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("affine-factorized IVP solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("affine-factorized IVP solution text", text, true);
    if (text) {
        printf("    expected: y = exp(½x²)\n"
               "    actual:   %s\n",
               string_c_str(text));
        EXPECT_TEXT("affine-factorized IVP solution text", string_c_str(text), "y = exp(½x²)");
    }

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_does_not_invent_cubic_potential_functions(void)
{
    diffequ_t *de = de_from_string("y'' = 1/2*(x^3+a)*y; y(0) = 1; y'(0) = 0");
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

    EXPECT_LONG("cubic-potential solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_UNSUPPORTED);
    EXPECT_LONG("cubic-potential selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_NONE);
    EXPECT_LONG("cubic-potential solution count", (long)de_solve_result_count(result), 0L);

    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_repeated_characteristic_root(void)
{
    diffequ_t *de = de_from_string("Dxx(y) + 2*Dx(y) + y = 0; y(0) = 1; y'(0) = 0");
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
    EXPECT_LONG("repeated-root solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("repeated-root selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_STURM_LIOUVILLE);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("repeated-root solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("repeated-root solution text", text, true);
    if (text)
        EXPECT_TEXT("repeated-root solution", string_c_str(text), "y = (x + 1)·exp(-x)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_oscillatory_sturm_liouville_problem(void)
{
    diffequ_t *de = de_from_string("Dxx(y) + y = 0; y(0) = 0; y'(0) = 1");
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
    EXPECT_LONG("oscillatory solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("oscillatory selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_STURM_LIOUVILLE);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("oscillatory solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("oscillatory solution text", text, true);
    if (text)
        EXPECT_TEXT("oscillatory solution", string_c_str(text), "y = sin(x)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_normalizes_variable_coefficient_sturm_liouville(void)
{
    diffequ_t *de = de_from_string("x*Dxx(y) + Dx(y) + y = 0");
    diffequ_solve_result_t *result;

    EXPECT_POINTER("parsed variable-coefficient problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    EXPECT_POINTER("variable-coefficient solve result", result, true);
    if (result) {
        EXPECT_LONG("variable-coefficient solve status", (long)de_solve_result_status(result),
                    (long)DE_SOLVE_STATUS_UNSUPPORTED);
        EXPECT_LONG("variable-coefficient solution count", (long)de_solve_result_count(result), 0L);
        EXPECT_TEXT("variable-coefficient diagnostic", de_solve_result_diagnostic(result),
                    "the second-order linear equation has no supported "
                    "closed-form basis");
    }

    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_power_law_bessel_family(void)
{
    const char *sources[] = {"y'' + x^2*y = 0", "y'' + 9*x^4*y = 0"};
    const char *orders[] = {"¼", "⅙"};
    const char *arguments[] = {"½·x^2", "x^3"};

    for (size_t i = 0u; i < 2u; ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result;
        const equation_t *solution;
        string_t *text;

        EXPECT_POINTER("parsed power-law Bessel problem", de, true);
        if (!de)
            continue;

        result = de_solve_with_options(de, DE_SOLVE_OPTION_STEPS);
        EXPECT_POINTER("power-law Bessel solve result", result, true);
        if (!result) {
            de_free(de);
            continue;
        }

        EXPECT_LONG("power-law Bessel solve status", (long)de_solve_result_status(result),
                    (long)DE_SOLVE_STATUS_SOLVED);
        EXPECT_LONG("power-law Bessel selected solver", (long)de_solve_result_solver(result),
                    (long)DE_SOLVER_POWER_LAW_BESSEL);
        EXPECT_TEXT("power-law Bessel diagnostic", de_solve_result_diagnostic(result),
                    "solved by a power-law reduction to Bessel's equation");
        solution = de_solve_result_at(result, 0u);
        EXPECT_POINTER("power-law Bessel solution", solution, true);
        text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
        EXPECT_POINTER("power-law Bessel solution text", text, true);
        if (text) {
            EXPECT_POINTER("power-law Bessel solution has negative-order basis",
                           strstr(string_c_str(text), "BesselJ(-"), true);
            EXPECT_POINTER("power-law Bessel solution has positive-order basis", strstr(string_c_str(text), orders[i]),
                           true);
            EXPECT_POINTER("power-law Bessel solution has derived argument", strstr(string_c_str(text), arguments[i]),
                           true);
        }
        EXPECT_POINTER("power-law Bessel derivation names its substitution",
                       strstr(de_solve_result_steps(result), "y = sqrt(x)*u(z)"), true);
        EXPECT_POINTER("power-law Bessel TeX uses conventional Bessel notation",
                       strstr(de_solve_result_steps_TeX(result), "J_{-\\frac"), true);
        EXPECT_POINTER("power-law Bessel TeX omits binding wrappers",
                       strstr(de_solve_result_steps_TeX(result), "\\middle|"), false);
        EXPECT_POINTER("power-law Bessel TeX omits unbound sentinel values",
                       strstr(de_solve_result_steps_TeX(result), "NAN"), false);

        string_free(text);
        de_solve_result_free(result);
        de_free(de);
    }
}

static void test_diffequ_solves_forced_power_law_lommel_family(void)
{
    const char *sources[] = {"y'' + x^3*y = x", "y'' + 9*x*y = 6"};
    const char *orders[] = {"⅕", "⅓"};
    const char *arguments[] = {"⅖·x^⁵⁄₂", "2·x^³⁄₂"};
    const char *scales[] = {"⅖·LommelS(0, ⅕", "⁴⁄₃·LommelS(0, ⅓"};

    for (size_t i = 0u; i < 2u; ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result;
        const equation_t *solution;
        string_t *text;

        EXPECT_POINTER("parsed forced power-law problem", de, true);
        if (!de)
            continue;

        result = de_solve_with_options(de, DE_SOLVE_OPTION_STEPS);
        EXPECT_POINTER("forced power-law solve result", result, true);
        if (!result) {
            de_free(de);
            continue;
        }

        EXPECT_LONG("forced power-law solve status", (long)de_solve_result_status(result),
                    (long)DE_SOLVE_STATUS_SOLVED);
        EXPECT_LONG("forced power-law selected solver", (long)de_solve_result_solver(result),
                    (long)DE_SOLVER_POWER_LAW_BESSEL);
        solution = de_solve_result_at(result, 0u);
        EXPECT_POINTER("forced power-law solution", solution, true);
        text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
        EXPECT_POINTER("forced power-law solution text", text, true);
        if (text) {
            EXPECT_POINTER("forced power-law solution has homogeneous order", strstr(string_c_str(text), orders[i]),
                           true);
            EXPECT_POINTER("forced power-law solution has derived argument", strstr(string_c_str(text), arguments[i]),
                           true);
            EXPECT_POINTER("forced power-law solution has Lommel particular", strstr(string_c_str(text), scales[i]),
                           true);
        }
        EXPECT_POINTER("forced power-law derivation names monomial forcing",
                       strstr(de_solve_result_steps(result), "monomial forcing"), true);
        EXPECT_POINTER("forced power-law TeX uses Lommel notation", strstr(de_solve_result_steps_TeX(result), "s_{0,"),
                       true);

        string_free(text);
        de_solve_result_free(result);
        de_free(de);
    }
}

static void test_diffequ_solves_third_order_constant_coefficient_problem(void)
{
    const char *source = "Dxxx(y) - Dx(y) = 0; "
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
    EXPECT_LONG("third-order solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("third-order selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("third-order solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("third-order solution text", text, true);
    if (text)
        EXPECT_TEXT("third-order solution", string_c_str(text), "y = exp(x)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_high_order_repeated_root(void)
{
    const char *source = "Dxxx(y) - 3*Dxx(y) + 3*Dx(y) - y = 0; "
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
    EXPECT_LONG("high-order repeated-root status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("high-order repeated-root solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("high-order repeated-root solution text", text, true);
    if (text)
        EXPECT_TEXT("high-order repeated-root solution", string_c_str(text), "y = exp(x)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_nonhomogeneous_constant_coefficient_problem(void)
{
    const char *source = "Dxx(y) - y = exp(2*x); y(0) = 0; y'(0) = 0";
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
    EXPECT_LONG("nonhomogeneous solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("nonhomogeneous selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("nonhomogeneous solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("nonhomogeneous solution text", text, true);
    if (text)
        EXPECT_TEXT("nonhomogeneous solution", string_c_str(text), "y = ⅙·(2·exp(2x) - 3·exp(x) + exp(-x))");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_secant_cubed_forcing(void)
{
    const char *source = "Dxx(y) + y = sec(x)^3";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    printf("  differential equation\n    input:    %s\n", source);
    EXPECT_POINTER("parsed secant-cubed forcing problem", de, true);
    if (!de)
        return;
    result = de_solve(de);
    EXPECT_POINTER("secant-cubed forcing result", result, true);
    if (!result) {
        de_free(de);
        return;
    }
    EXPECT_LONG("secant-cubed forcing status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("secant-cubed forcing selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("secant-cubed forcing solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("secant-cubed forcing solution text", text, true);
    if (text)
        EXPECT_TEXT("secant-cubed forcing solution", string_c_str(text), "y = ½·sec(x) + C₁·cos(x) + C₂·sin(x)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_repeated_complex_roots(void)
{
    const char *source = "Dxxxx(y) + 2*Dxx(y) + y = 0; "
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
    EXPECT_LONG("repeated-complex-root status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    solution = de_solve_result_at(result, 0u);
    EXPECT_POINTER("repeated-complex-root solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    EXPECT_POINTER("repeated-complex-root solution text", text, true);
    if (text)
        EXPECT_TEXT("repeated-complex-root solution", string_c_str(text), "y = cos(x)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_degree_six_characteristic_polynomial(void)
{
    diffequ_t *de = de_from_string("Dxxxxxx(y) - 4*Dxxxx(y) - Dxx(y) + 4*y = 0");
    diffequ_solve_result_t *result;

    EXPECT_POINTER("parsed sixth-order problem", de, true);
    if (!de)
        return;
    result = de_solve(de);
    EXPECT_POINTER("sixth-order solve result", result, true);
    if (result) {
        EXPECT_LONG("sixth-order solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
        EXPECT_LONG("sixth-order selected solver", (long)de_solve_result_solver(result),
                    (long)DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR);
        EXPECT_LONG("sixth-order solution count", (long)de_solve_result_count(result), 1L);
    }

    de_solve_result_free(result);
    de_free(de);
}

static bool test_diffequ_expect_constant_linear_solution(const char *source, const char *expected, const char *file,
                                                         int line)
{
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    const char *actual = text ? string_c_str(text) : NULL;
    bool valid = de && result && de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
                 de_solve_result_solver(result) == DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR &&
                 de_solve_result_count(result) == 1u && actual && strcmp(actual, expected) == 0;

    printf("  differential equation\n"
           "    input:    %s\n"
           "    expected: %s\n"
           "    actual:   %s\n",
           source, expected, actual ? actual : "NULL");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
    return test_assert_true(valid, file, line, "constant-coefficient linear solution");
}

#define EXPECT_CONSTANT_LINEAR_SOLUTION(source, expected)                                                              \
    TEST_HARNESS_RETURN_UNLESS(test_diffequ_expect_constant_linear_solution((source), (expected), __FILE__, __LINE__))

static void test_diffequ_solves_logarithmic_forcing(void)
{
    EXPECT_CONSTANT_LINEAR_SOLUTION("y'' + y = ln(x)", "y = ln(x) - cos(x)·Ci(x) - sin(x)·Si(x) + "
                                                       "C₁·cos(x) + C₂·sin(x)");
}

static void test_diffequ_resolves_polynomial_differential_operator(void)
{
    const char *source = "(Dx^2 + 4Dx + 20)^2(y) = 0";
    const char *expected_problem = "{ d⁴y/dx⁴ + 8*d³y/dx³ + 56*d²y/dx² + "
                                   "160*dy/dx + 400*y = 0 | x = ?; ;  }";
    const char *expected_solution = "y = exp(-2x)·(C₁·cos(4x) + C₂·sin(4x) + "
                                    "C₃x·cos(4x) + C₄x·sin(4x))";
    diffequ_t *de = de_from_string(source);
    char *problem = de ? de_to_string(de, style_EXPRESSION) : NULL;
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    string_t *solution_text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;

    printf("  polynomial differential operator\n"
           "    input:    %s\n"
           "    resolves: %s\n"
           "    solution: %s\n",
           source, problem ? problem : "NULL", solution_text ? string_c_str(solution_text) : "NULL");
    EXPECT_TEXT("resolved differential equation", problem, expected_problem);
    EXPECT_POINTER("operator solve result", result, true);
    if (result) {
        EXPECT_LONG("operator solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
        EXPECT_LONG("operator selected solver", (long)de_solve_result_solver(result),
                    (long)DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR);
    }
    EXPECT_TEXT("operator solution", solution_text ? string_c_str(solution_text) : NULL, expected_solution);

    string_free(solution_text);
    de_solve_result_free(result);
    free(problem);
    de_free(de);
}

static void test_diffequ_defaults_bare_differential_operator(void)
{
    static const struct {
        const char *source;
        const char *problem;
        const char *solution;
    } cases[] = {
        {
            "(D^2 + 4D + 20)^2(y) = 0",
            "{ d⁴y/dx⁴ + 8*d³y/dx³ + 56*d²y/dx² + "
            "160*dy/dx + 400*y = 0 | x = ?; ;  }",
            "y = exp(-2x)·(C₁·cos(4x) + C₂·sin(4x) + "
            "C₃x·cos(4x) + C₄x·sin(4x))",
        },
        {
            "(D^2 + 4D + 20)^2(x) = 0",
            "{ d⁴x/dt⁴ + 8*d³x/dt³ + 56*d²x/dt² + "
            "160*dx/dt + 400*x = 0 | t = ?; ;  }",
            "x = exp(-2t)·(C₁·cos(4t) + C₂·sin(4t) + "
            "C₃t·cos(4t) + C₄t·sin(4t))",
        },
        {
            "D(y) = y",
            "{ dy/dx = y | x = ?; ;  }",
            "y = C·exp(x)",
        },
        {
            "D(x) = x",
            "{ dx/dt = x | t = ?; ;  }",
            "x = C·exp(t)",
        },
        {
            "D(z) = z",
            "{ dz/dx = z | x = ?; ;  }",
            "z = C·exp(x)",
        },
        {
            "D(q) = q",
            "{ dq/dx = q | x = ?; ;  }",
            "q = C·exp(x)",
        },
        {
            "D^2(y) + y = 0",
            "{ d²y/dx² + y = 0 | x = ?; ;  }",
            "y = C₁·cos(x) + C₂·sin(x)",
        },
        {
            "D^2(x) + x = 0",
            "{ d²x/dt² + x = 0 | t = ?; ;  }",
            "x = C₁·cos(t) + C₂·sin(t)",
        },
        {
            "(D^2 + @omega^2)x = 0",
            "{ d²x/dt² + ω²*x = 0 | t = ?; ;  }",
            "x = C₁·cos(ωt) + C₂·sin(ωt)",
        },
        {
            "(D^2 - @omega^2)x = 0",
            "{ d²x/dt² - ω²*x = 0 | t = ?; ;  }",
            "x = C₁·exp(ωt) + C₂·exp(-ωt)",
        },
        {
            "(D^2 - @omega^2)^2(x) = 0",
            "{ d⁴x/dt⁴ - 2ω²*d²x/dt² + ω⁴*x = 0 | t = ?; ;  }",
            "x = (C₁ + C₂t)·exp(ωt) + (C₃ + C₄t)·exp(-ωt)",
        },
        {
            "(D^2 + @omega^2)^2(x) = 0",
            "{ d⁴x/dt⁴ + 2ω²*d²x/dt² + ω⁴*x = 0 | t = ?; ;  }",
            "x = (C₁ + C₂t)·cos(ωt) + (C₃ + C₄t)·sin(ωt)",
        },
        {
            "(D^2 + @omega^2)^3x = 0",
            "{ d⁶x/dt⁶ + 3ω²*d⁴x/dt⁴ + 3ω⁴*d²x/dt² + "
            "ω⁶*x = 0 | t = ?; ;  }",
            "x = (C₁ + C₂t + C₃t²)·cos(ωt) + "
            "(C₄ + C₅t + C₆t²)·sin(ωt)",
        },
        {
            "(D^2 - @omega^2)^3x = 0",
            "{ d⁶x/dt⁶ - 3ω²*d⁴x/dt⁴ + 3ω⁴*d²x/dt² - "
            "ω⁶*x = 0 | t = ?; ;  }",
            "x = (C₁ + C₂t + C₃t²)·exp(ωt) + "
            "(C₄ + C₅t + C₆t²)·exp(-ωt)",
        },
        {
            "(D^2 + @omega^2)^4x = 0",
            "{ d⁸x/dt⁸ + 4ω²*d⁶x/dt⁶ + 6ω⁴*d⁴x/dt⁴ + "
            "4ω⁶*d²x/dt² + ω⁸*x = 0 | t = ?; ;  }",
            "x = (Σ_(k=0)^3 C_(k + 1)·t^k)·cos(ωt) + "
            "(Σ_(k=0)^3 C_(k + 5)·t^k)·sin(ωt)",
        },
        {
            "(D^2 + @omega^2)^4phi = 0",
            "{ d⁸φ/dx⁸ + 4ω²*d⁶φ/dx⁶ + 6ω⁴*d⁴φ/dx⁴ + "
            "4ω⁶*d²φ/dx² + ω⁸*φ = 0 | x = ?; ;  }",
            "φ = (Σ_(k=0)^3 C_(k + 1)·x^k)·cos(ωx) + "
            "(Σ_(k=0)^3 C_(k + 5)·x^k)·sin(ωx)",
        },
        {
            "(D^2 + @omega^2)^4@phi = 0",
            "{ d⁸φ/dx⁸ + 4ω²*d⁶φ/dx⁶ + 6ω⁴*d⁴φ/dx⁴ + "
            "4ω⁶*d²φ/dx² + ω⁸*φ = 0 | x = ?; ;  }",
            "φ = (Σ_(k=0)^3 C_(k + 1)·x^k)·cos(ωx) + "
            "(Σ_(k=0)^3 C_(k + 5)·x^k)·sin(ωx)",
        },
        {
            "(D^2 + @omega^2)^4φ = 0",
            "{ d⁸φ/dx⁸ + 4ω²*d⁶φ/dx⁶ + 6ω⁴*d⁴φ/dx⁴ + "
            "4ω⁶*d²φ/dx² + ω⁸*φ = 0 | x = ?; ;  }",
            "φ = (Σ_(k=0)^3 C_(k + 1)·x^k)·cos(ωx) + "
            "(Σ_(k=0)^3 C_(k + 5)·x^k)·sin(ωx)",
        },
    };
    bool valid = true;

    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        diffequ_t *de = de_from_string(cases[i].source);
        char *problem = de ? de_to_string(de, style_EXPRESSION) : NULL;
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
        string_t *solution_text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
        const char *actual_solution = solution_text ? string_c_str(solution_text) : NULL;
        bool case_valid = de && problem && strcmp(problem, cases[i].problem) == 0 && result &&
                          de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED && actual_solution &&
                          strcmp(actual_solution, cases[i].solution) == 0;

        printf("  bare differential operator\n"
               "    input:    %s\n"
               "    resolves: %s\n"
               "    solution: %s\n",
               cases[i].source, problem ? problem : "NULL", actual_solution ? actual_solution : "NULL");
        valid = valid && case_valid;

        string_free(solution_text);
        de_solve_result_free(result);
        free(problem);
        de_free(de);
    }
    ASSERT_TRUE(valid);
}

static void test_diffequ_solves_maximum_repeated_quadratic_power(void)
{
    diffequ_t *de = de_from_string("(D^2 + @omega^2)^64x = 0");
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    bool valid = result && de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED && text &&
                 strcmp(string_c_str(text), "x = (Σ_(k=0)^63 C_(k + 1)·t^k)·cos(ωt) + "
                                            "(Σ_(k=0)^63 C_(k + 65)·t^k)·sin(ωt)") == 0;

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
    ASSERT_TRUE(valid);
}

static void test_diffequ_general_solution_with_distinct_real_roots(void)
{
    EXPECT_CONSTANT_LINEAR_SOLUTION("Dxxx(y) - 6*Dxx(y) + 11*Dx(y) - 6*y = 0",
                                    "y = C₁·exp(3x) + C₂·exp(2x) + C₃·exp(x)");
}

static void test_diffequ_general_solution_with_repeated_real_root(void)
{
    EXPECT_CONSTANT_LINEAR_SOLUTION("Dxxx(y) - 3*Dxx(y) + 3*Dx(y) - y = 0", "y = (C₁ + C₂x + C₃x²)·exp(x)");
}

static void test_diffequ_general_solution_with_real_and_complex_roots(void)
{
    EXPECT_CONSTANT_LINEAR_SOLUTION("Dxxx(y) - Dxx(y) + Dx(y) - y = 0", "y = C₁·exp(x) + C₂·cos(x) + C₃·sin(x)");
}

static void test_diffequ_general_solution_with_repeated_complex_roots(void)
{
    EXPECT_CONSTANT_LINEAR_SOLUTION("Dxxxx(y) + 2*Dxx(y) + y = 0", "y = (C₁ + C₂x)·cos(x) + (C₃ + C₄x)·sin(x)");
}

static void test_diffequ_general_sixth_order_solution(void)
{
    EXPECT_CONSTANT_LINEAR_SOLUTION("Dxxxxxx(y) - 4*Dxxxx(y) - Dxx(y) + 4*y = 0",
                                    "y = C₁·exp(x) + C₂·exp(2x) + C₃·exp(-x) + C₄·exp(-2x) + "
                                    "C₅·cos(x) + C₆·sin(x)");
}

static void test_diffequ_general_nonhomogeneous_solution(void)
{
    EXPECT_CONSTANT_LINEAR_SOLUTION("Dxx(y) - y = exp(2*x)", "y = ⅓·exp(2x) + C₁·exp(x) + C₂·exp(-x)");
    EXPECT_CONSTANT_LINEAR_SOLUTION("y'' + 4y = e^x + x^3", "y = ⅕·exp(x) + ¼x³ - ⅜x + "
                                                            "C₁·cos(2x) + C₂·sin(2x)");
}

static void test_diffequ_general_trigonometric_forcing_solution(void)
{
    EXPECT_CONSTANT_LINEAR_SOLUTION("Dxx(y) + y = cos(2*x)", "y = -⅓·cos(2x) + C₁·cos(x) + C₂·sin(x)");
    EXPECT_CONSTANT_LINEAR_SOLUTION("Dxx(y) + 2*Dx(y) + 5*y = sin(3*x)", "y = ¹⁄₂₆·(-3·cos(3x) - 2·sin(3x)) + "
                                                                         "C₁·exp(-x)·cos(2x) + C₂·exp(-x)·sin(2x)");
}

static void test_diffequ_general_third_order_forced_solution(void)
{
    EXPECT_CONSTANT_LINEAR_SOLUTION("Dxxx(y) - Dx(y) = exp(2*x)", "y = ⅙·exp(2x) + C₁·exp(x) + C₂ + C₃·exp(-x)");
}

static bool test_diffequ_expect_pde_solution(const char *source, const char *expected, de_solver_t expected_solver,
                                             const char *file, int line)
{
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    const char *actual = text ? string_c_str(text) : NULL;
    bool valid = de && result && de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
                 de_solve_result_solver(result) == expected_solver && de_solve_result_count(result) == 1u && actual &&
                 strcmp(actual, expected) == 0;

    printf("  partial differential equation\n"
           "    input:    %s\n"
           "    expected: %s\n"
           "    actual:   %s\n",
           source, expected, actual ? actual : "NULL");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
    return test_assert_true(valid, file, line, "partial differential equation solution");
}

#define EXPECT_TRANSPORT_SOLUTION(source, expected)                                                                    \
    TEST_HARNESS_RETURN_UNLESS(test_diffequ_expect_pde_solution(                                                       \
        (source), (expected), DE_SOLVER_CONSTANT_COEFFICIENT_TRANSPORT, __FILE__, __LINE__))

#define EXPECT_CHARACTERISTIC_SOLUTION(source, expected)                                                               \
    TEST_HARNESS_RETURN_UNLESS(                                                                                        \
        test_diffequ_expect_pde_solution((source), (expected), DE_SOLVER_CHARACTERISTICS, __FILE__, __LINE__))

#define EXPECT_LAPLACE_SOLUTION(source, expected)                                                                      \
    TEST_HARNESS_RETURN_UNLESS(                                                                                        \
        test_diffequ_expect_pde_solution((source), (expected), DE_SOLVER_LAPLACE, __FILE__, __LINE__))

static void test_diffequ_solves_two_dimensional_laplace_equation(void)
{
    EXPECT_LAPLACE_SOLUTION("phi_xx + phi_yy = 0", "φ = F(x + iy) + G(x - iy)");
    EXPECT_LAPLACE_SOLUTION("3*phi_xx + 3*phi_yy = 0", "φ = F(x + iy) + G(x - iy)");
    EXPECT_LAPLACE_SOLUTION("u_ss + u_tt = 0", "u = F(it + s) + G(s - it)");
    EXPECT_LAPLACE_SOLUTION("phi_rr + phi_r/r + phi_thetatheta/r^2 = 0", "φ = F(r·exp(iθ)) + G(r·exp(-iθ))");
    EXPECT_LAPLACE_SOLUTION("phi_rr + 1/r phi_r + 1/r^2 phi_thetatheta = 0", "φ = F(r·exp(iθ)) + G(r·exp(-iθ))");
    EXPECT_LAPLACE_SOLUTION("2*phi_thetatheta/r^2 + 2*phi_rr + 2*phi_r/r = 0", "φ = F(r·exp(iθ)) + G(r·exp(-iθ))");
}

static void test_diffequ_parses_pde_boundary_arguments(void)
{
    const char *source = "{ 2*Dx(u) + Dy(u) = 0 | x = ?, y = ?;; "
                         "u(x, 0) = x^2 }";
    const char *expected_problem = "{ 2*∂u/∂x + ∂u/∂y = 0 | x = ?, y = ?; ; "
                                   "u(x, 0) = x^2 }";
    diffequ_t *de = de_from_string(source);
    char *problem = de ? de_to_string(de, style_EXPRESSION) : NULL;
    const expr_t *first = de ? de_condition_argument_at(de, 0u, 0u) : NULL;
    const expr_t *second = de ? de_condition_argument_at(de, 0u, 1u) : NULL;
    number_t second_value = second ? expr_eval(second) : num_new();
    number_t zero = num_create_from_long(0L);

    EXPECT_POINTER("parsed PDE", de, true);
    EXPECT_LONG("PDE independent-variable count", (long)de_independent_count(de), 2L);
    EXPECT_LONG("PDE boundary-argument count", (long)de_condition_argument_count(de, 0u), 2L);
    EXPECT_TEXT("PDE first boundary argument shares x", first == de_independent_at(de, 0u) ? "same" : "different",
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
    EXPECT_TRANSPORT_SOLUTION("{ 2*Dx(u) + Dy(u) = 0 | x = ?, y = ?;; "
                              "u(x, 0) = x^2 }",
                              "u = (x - 2y)²");
}

static void test_diffequ_solves_constant_transport_from_x_boundary(void)
{
    EXPECT_TRANSPORT_SOLUTION("{ Dx(u) + 3*Dy(u) = 0 | x = ?, y = ?;; "
                              "u(0, y) = exp(y) }",
                              "u = exp(y - 3x)");
}

static void test_diffequ_solves_variable_forcing_from_time_boundary(void)
{
    EXPECT_TRANSPORT_SOLUTION("Dx(z) + 1/2*Dt(z) = cos(x); z(x,0) = 0", "z = sin(x) - sin(x - 2t)");
}

static void test_diffequ_solves_parametric_characteristic_boundary(void)
{
    EXPECT_CHARACTERISTIC_SOLUTION("∂z/∂x + ∂z/∂y = 2*z*(x+y); z(x,1-x) = x^2",
                                   "z = ¼·(x - y + 1)²·exp(½·((x + y)² - 1))");
}

static void test_diffequ_solves_scaled_coordinate_characteristics(void)
{
    EXPECT_CHARACTERISTIC_SOLUTION("(x^2+1)*Dx(z) + 2*x*y*Dy(z) - x*y = 0", "z = ½·(2·F(y/(x² + 1)) + y)");
    EXPECT_CHARACTERISTIC_SOLUTION("(x^2+1)*Dx(z) + 2*x*y*Dy(z) - x*y = 0; "
                                   "z(x, 1) = (x^2+1)^2",
                                   "z = ½·(y + 2·((x² + 1)/y)² - 1)");
}

static void test_diffequ_solves_exponential_characteristics(void)
{
    EXPECT_CHARACTERISTIC_SOLUTION("exp(x)*Dx(z) + Dy(z) = 0", "z = F(exp(-x) + y)");
    EXPECT_CHARACTERISTIC_SOLUTION("exp(x)*Dx(z) + Dy(z) = 0; z(x, 0) = tanh(x)", "z = -tanh(ln(exp(-x) + y))");
}

static void test_diffequ_solves_unbounded_homogeneous_transport(void)
{
    EXPECT_TRANSPORT_SOLUTION("Dt(u) + c*Dx(u) = 0", "u = F(x - ct)");
}

static void test_diffequ_solves_unbounded_inhomogeneous_transport(void)
{
    EXPECT_TRANSPORT_SOLUTION("Dt(u) + c*Dx(u) = 1", "u = F(x - ct) + t");
}

static void test_diffequ_solves_transport_with_reaction_term(void)
{
    EXPECT_TRANSPORT_SOLUTION("Dx(z) + Dy(z) = z", "z = exp(x)·F(y - x)");
}

static void test_diffequ_solves_transport_with_variable_forcing(void)
{
    diffequ_t *de = de_from_string("Dx(z) + Dy(z) + z = x");
    char *tex = de ? de_to_string(de, style_LATEX) : NULL;

    EXPECT_TEXT("variable-forcing PDE TeX", tex,
                "\\frac{\\partial z}{\\partial x} + "
                "\\frac{\\partial z}{\\partial y} + z = x");
    EXPECT_TRANSPORT_SOLUTION("Dx(z) + Dy(z) + z = x", "z = exp(-x)·F(y - x) + x - 1");

    free(tex);
    de_free(de);
}

static void test_diffequ_solves_mixed_phase_trigonometric_forcing(void)
{
    EXPECT_TRANSPORT_SOLUTION("Dx(z) + Dy(z) = cos(x+y)", "z = F(y - x) + ½·sin(x + y)");
}

static void test_diffequ_solves_mixed_phase_unary_forcing(void)
{
    EXPECT_TRANSPORT_SOLUTION("Dx(z) + 2*Dy(z) = tanh(x+y)", "z = F(y - 2x) + ⅓·ln(cosh(x + y))");
}

static void test_diffequ_uses_builtin_alias_as_dependent_symbol(void)
{
    const char *source = "Dx(@phi) - Dy(@phi) = sin(x) + cos(y)";
    diffequ_t *de = de_from_string(source);
    char *tex = de ? de_to_string(de, style_LATEX) : NULL;

    EXPECT_TEXT("contextual dependent-symbol TeX", tex,
                "\\frac{\\partial \\phi}{\\partial x} - "
                "\\frac{\\partial \\phi}{\\partial y} = "
                "\\sin(x) + \\cos(y)");
    EXPECT_TRANSPORT_SOLUTION(source, "φ = F(x + y) - cos(x) - sin(y)");

    free(tex);
    de_free(de);
}

static void test_diffequ_solves_scaled_additive_transport_forcing(void)
{
    EXPECT_TRANSPORT_SOLUTION("2*Dx(u) + 3*Dy(u) = 4*x + 6*y", "u = F(½·(2y - 3x)) + x² + y²");
}

static void test_diffequ_solves_polynomial_reaction_forcing(void)
{
    EXPECT_TRANSPORT_SOLUTION("Dx(z) + 3*Dy(z) - 2*z + "
                              "4*y^2 - 22*y + 4*x + 13 = 0",
                              "z = exp(2x)·F(y - 3x) + 2x - 5y + 2y²");
}

static void test_diffequ_solves_mixed_polynomial_reaction_forcing(void)
{
    EXPECT_TRANSPORT_SOLUTION("2*Dx(u) - Dy(u) + 3*u = x*y", "u = exp(-³⁄₂x)·F(½·(x + 2y)) + "
                                                             "¹⁄₂₇·(9xy + 3x - 6y - 4)");
}

static void test_diffequ_solves_trigonometric_reaction_forcing(void)
{
    EXPECT_TRANSPORT_SOLUTION("2*Dx(@phi) + Dy(@phi) + 6*@phi = 37*sin(y)",
                              "φ = exp(-3x)·F(½·(2y - x)) + 6·sin(y) - cos(y)");
}

static void test_diffequ_solves_exponential_reaction_forcing(void)
{
    EXPECT_TRANSPORT_SOLUTION("3*Dx(u) + 2*Dy(u) + 5*u = 11*exp(y)", "u = exp(-⁵⁄₃x)·F(⅓·(3y - 2x)) + ¹¹⁄₇·exp(y)");
}

static void test_diffequ_solves_three_variable_transport(void)
{
    EXPECT_TRANSPORT_SOLUTION("Dx(@phi) + Dy(@phi) + Dz(@phi) = @phi", "φ = exp(x)·F(y - x, z - x)");
}

static void test_diffequ_solves_scaled_three_variable_transport(void)
{
    EXPECT_TRANSPORT_SOLUTION("2*Dx(u) - 3*Dy(u) + 4*Dz(u) + 5*u = 10", "u = exp(-⁵⁄₂x)·F(½·(3x + 2y), z - 2x) + 2");
}

static void test_diffequ_solves_nonlinear_characteristic_pde(void)
{
    diffequ_t *de = de_from_string("Dx(z) + Dy(z) = 6*(x+y)^2*z^2");
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *general = result ? de_solve_result_at(result, 0u) : NULL;
    const equation_t *singular = result ? de_solve_result_at(result, 1u) : NULL;
    string_t *general_text = general ? equ_to_text(general, style_UNBOUND) : NULL;
    string_t *singular_text = singular ? equ_to_text(singular, style_UNBOUND) : NULL;

    EXPECT_LONG("nonlinear characteristic status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("nonlinear characteristic solution count", (long)de_solve_result_count(result), 2L);
    EXPECT_TEXT("nonlinear characteristic general solution", general_text ? string_c_str(general_text) : NULL,
                "z = 1/(F(y - x) - (x + y)³)");
    EXPECT_TEXT("nonlinear characteristic singular solution", singular_text ? string_c_str(singular_text) : NULL,
                "z = 0");

    string_free(singular_text);
    string_free(general_text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_nonlinear_characteristic_uses_power_rule(void)
{
    diffequ_t *de = de_from_string("Dx(u) + Dy(u) = 4*(x+y)*u^3");
    diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
    const equation_t *general = result ? de_solve_result_at(result, 0u) : NULL;
    string_t *text = general ? equ_to_text(general, style_UNBOUND) : NULL;

    EXPECT_LONG("power characteristic status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_TEXT("power characteristic solution", text ? string_c_str(text) : NULL, "u = 1/√(F(y - x) - 8xy)");
    EXPECT_POINTER("power characteristic derivation", de_solve_result_steps(result), true);

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_spiral_characteristic_uses_linear_field_rule(void)
{
    const char *source = "(2*x+3*y)*Dx(u) + (-3*x+2*y)*Dy(u) = 0";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;

    EXPECT_LONG("spiral characteristic status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_POINTER("spiral characteristic arbitrary family", text ? strstr(string_c_str(text), "F(") : NULL, true);
    EXPECT_POINTER("spiral characteristic logarithmic invariant",
                   text ? strstr(string_c_str(text), "ln(x² + y²)") : NULL, true);
    EXPECT_POINTER("spiral characteristic derivation", de_solve_result_steps(result), true);

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_quadratic_characteristic_evolution(void)
{
    const char *source = "x^2*Dx(z) + y^2*Dy(z) = z^2";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *general = result ? de_solve_result_at(result, 0u) : NULL;
    const equation_t *singular = result ? de_solve_result_at(result, 1u) : NULL;
    string_t *general_text = general ? equ_to_text(general, style_UNBOUND) : NULL;
    string_t *singular_text = singular ? equ_to_text(singular, style_UNBOUND) : NULL;

    printf("  quadratic characteristic evolution\n"
           "    input: %s\n",
           source);
    EXPECT_LONG("quadratic characteristic status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("quadratic characteristic solution count", (long)de_solve_result_count(result), 2L);
    EXPECT_TEXT("quadratic characteristic general solution", general_text ? string_c_str(general_text) : NULL,
                "z = 1/(F(1/x - 1/y) + 1/x)");
    EXPECT_TEXT("quadratic characteristic singular solution", singular_text ? string_c_str(singular_text) : NULL,
                "z = 0");

    string_free(singular_text);
    string_free(general_text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_applies_quadratic_characteristic_boundary(void)
{
    const char *source = "x^2*Dx(z) + y^2*Dy(z) + z^2 = 0; "
                         "z(x, x/(x-1)) = 1";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    string_t *solution_text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;

    printf("  quadratic characteristic boundary problem\n"
           "    input:    %s\n"
           "    expected: z = 2/(3 - 1/x - 1/y)\n"
           "    actual:   %s\n",
           source, solution_text ? string_c_str(solution_text) : "NULL");
    EXPECT_LONG("quadratic boundary status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("quadratic boundary selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_CHARACTERISTICS);
    EXPECT_LONG("quadratic boundary solution count", (long)de_solve_result_count(result), 1L);
    EXPECT_TEXT("quadratic boundary solution", solution_text ? string_c_str(solution_text) : NULL,
                "z = 2/(3 - 1/x - 1/y)");

    string_free(solution_text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_dependent_square_characteristic_pde(void)
{
    const char *source = "xzz_x + yzz_y + x^2 + y^2 = 0";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *positive = result ? de_solve_result_at(result, 0u) : NULL;
    const equation_t *negative = result ? de_solve_result_at(result, 1u) : NULL;
    string_t *positive_text = positive ? equ_to_text(positive, style_UNBOUND) : NULL;
    string_t *negative_text = negative ? equ_to_text(negative, style_UNBOUND) : NULL;

    printf("  dependent-square characteristic reduction\n"
           "    input:    %s\n"
           "    expected: %s\n"
           "    actual:   %s\n"
           "    expected: %s\n"
           "    actual:   %s\n",
           source, "z = √(F(y/x) - x² - y²)", positive_text ? string_c_str(positive_text) : "NULL",
           "z = -√(F(y/x) - x² - y²)", negative_text ? string_c_str(negative_text) : "NULL");
    EXPECT_LONG("dependent-square characteristic status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("dependent-square selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_CHARACTERISTICS);
    EXPECT_LONG("dependent-square solution count", (long)de_solve_result_count(result), 2L);
    EXPECT_TEXT("dependent-square positive branch", positive_text ? string_c_str(positive_text) : NULL,
                "z = √(F(y/x) - x² - y²)");
    EXPECT_TEXT("dependent-square negative branch", negative_text ? string_c_str(negative_text) : NULL,
                "z = -√(F(y/x) - x² - y²)");

    string_free(negative_text);
    string_free(positive_text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_applies_dependent_square_boundary(void)
{
    const char *source = "x*z*z_x + y*z*z_y + x*y = 0; z(x, 1/x) = x";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    string_t *solution_text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;

    printf("  dependent-square characteristic boundary problem\n"
           "    input:    %s\n"
           "    expected: z = √(1 - xy + x/y)\n"
           "    actual:   %s\n",
           source, solution_text ? string_c_str(solution_text) : "NULL");
    EXPECT_LONG("dependent-square boundary status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("dependent-square boundary selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_CHARACTERISTICS);
    EXPECT_LONG("dependent-square boundary solution count", (long)de_solve_result_count(result), 1L);
    EXPECT_TEXT("dependent-square boundary solution", solution_text ? string_c_str(solution_text) : NULL,
                "z = √(1 - xy + x/y)");

    string_free(solution_text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_applies_signed_dependent_square_boundary(void)
{
    EXPECT_CHARACTERISTIC_SOLUTION("z*z_x - z*z_y = y-x; z(1, y) = y^2", "z = √(2xy - 2x - 2y + 2 + (x + y - 1)⁴)");
}

static void test_diffequ_solves_invariant_forced_square_pde(void)
{
    const char *source = "zz_x + zz_y = y - x";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *positive = result ? de_solve_result_at(result, 0u) : NULL;
    const equation_t *negative = result ? de_solve_result_at(result, 1u) : NULL;
    string_t *positive_text = positive ? equ_to_text(positive, style_UNBOUND) : NULL;
    string_t *negative_text = negative ? equ_to_text(negative, style_UNBOUND) : NULL;

    printf("  invariant-forced dependent-square PDE\n"
           "    input:    %s\n"
           "    expected: %s\n"
           "    actual:   %s\n"
           "    expected: %s\n"
           "    actual:   %s\n",
           source, "z = √(F(y - x) - x² + y²)", positive_text ? string_c_str(positive_text) : "NULL",
           "z = -√(F(y - x) - x² + y²)", negative_text ? string_c_str(negative_text) : "NULL");
    EXPECT_LONG("invariant-forced square status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("invariant-forced square solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_CHARACTERISTICS);
    EXPECT_LONG("invariant-forced square solution count", (long)de_solve_result_count(result), 2L);
    EXPECT_TEXT("invariant-forced positive branch", positive_text ? string_c_str(positive_text) : NULL,
                "z = √(F(y - x) - x² + y²)");
    EXPECT_TEXT("invariant-forced negative branch", negative_text ? string_c_str(negative_text) : NULL,
                "z = -√(F(y - x) - x² + y²)");

    string_free(negative_text);
    string_free(positive_text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_reciprocal_forced_square_pde(void)
{
    const char *source = "(y-x)∂z/∂x + (y+x)∂z/∂y = (x^2+y^2)/z";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *positive = result ? de_solve_result_at(result, 0u) : NULL;
    const equation_t *negative = result ? de_solve_result_at(result, 1u) : NULL;
    string_t *positive_text = positive ? equ_to_text(positive, style_UNBOUND) : NULL;
    string_t *negative_text = negative ? equ_to_text(negative, style_UNBOUND) : NULL;

    printf("  reciprocal-forced dependent-square PDE\n"
           "    input:    %s\n"
           "    expected: %s\n"
           "    actual:   %s\n"
           "    expected: %s\n"
           "    actual:   %s\n",
           source, "z = √(F(x² + 2xy - y²) + 2xy)", positive_text ? string_c_str(positive_text) : "NULL",
           "z = -√(F(x² + 2xy - y²) + 2xy)", negative_text ? string_c_str(negative_text) : "NULL");
    EXPECT_LONG("reciprocal-forced square status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("reciprocal-forced square solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_CHARACTERISTICS);
    EXPECT_LONG("reciprocal-forced square solution count", (long)de_solve_result_count(result), 2L);
    EXPECT_TEXT("reciprocal-forced positive branch", positive_text ? string_c_str(positive_text) : NULL,
                "z = √(F(x² + 2xy - y²) + 2xy)");
    EXPECT_TEXT("reciprocal-forced negative branch", negative_text ? string_c_str(negative_text) : NULL,
                "z = -√(F(x² + 2xy - y²) + 2xy)");

    string_free(negative_text);
    string_free(positive_text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_rotating_characteristic_pde(void)
{
    EXPECT_CHARACTERISTIC_SOLUTION("(x+y)*Dx(z) + (y-x)*Dy(z) = 0", "z = F(½·(ln(x² + y²) + 2·atan2(y, x)))");
}

static void test_diffequ_solves_cyclic_lagrange_pde(void)
{
    EXPECT_CHARACTERISTIC_SOLUTION("x*(y-z)*z_x + y*(z-x)*z_y = z*(x-y)", "F(x + y + z, xyz) = 0");
    EXPECT_CHARACTERISTIC_SOLUTION("x(y^2-z^2)∂z/∂x + y(z^2-x^2)∂z/∂y = "
                                   "z(x^2-y^2)",
                                   "F(x² + y² + z², xyz) = 0");
}

static void test_diffequ_solves_monomial_linear_characteristic_pde(void)
{
    const char *source = "x^2*Dx(@psi) - x*y*Dy(@psi) + y*@psi = 0";
    diffequ_t *de = de_from_string(source);
    char *tex = de ? de_to_string(de, style_LATEX) : NULL;

    EXPECT_TEXT("Greek dependent-symbol TeX", tex,
                "x^{2} \\cdot \\frac{\\partial \\psi}{\\partial x} - "
                "x y \\cdot \\frac{\\partial \\psi}{\\partial y} + "
                "\\psi y = 0");
    EXPECT_CHARACTERISTIC_SOLUTION(source, "ψ = F(xy)·exp(½·1/x·y)");

    free(tex);
    de_free(de);
}

static void test_diffequ_solves_forced_monomial_characteristic_pde(void)
{
    EXPECT_CHARACTERISTIC_SOLUTION("x*Dx(z) - 7*y*Dy(z) = 5*x^2*y", "z = F(x⁷y) - x²y");
    EXPECT_CHARACTERISTIC_SOLUTION("x*Dx(u) - 2*y*Dy(u) = 6*x*y", "u = F(x²y) - 6xy");
}

static void test_diffequ_solves_forced_radial_characteristic_pde(void)
{
    EXPECT_CHARACTERISTIC_SOLUTION("x*y*Dx(z) - x^2*Dy(z) + y*z = 3*x^2*y", "z = F(x² + y²)/x + x²");
}

static void test_diffequ_solves_separable_trigonometric_characteristic_pde(void)
{
    EXPECT_CHARACTERISTIC_SOLUTION("Dx(@phi)*sec(x) + Dy(@phi) = cot(y)", "φ = F(y - sin(x)) + ln(sin(y))");
}

static void test_diffequ_solves_cross_coordinate_characteristic_pde(void)
{
    EXPECT_CHARACTERISTIC_SOLUTION("3*y^2*Dx(u) + Dy(u) - x*y^2*u = 0", "u = exp(⅙x²)·F(x - y³)");
    EXPECT_CHARACTERISTIC_SOLUTION("3*y^2*Dx(u) + Dy(u) - x*y^2*u = 0; "
                                   "u(y+y^3, y) = (y+y^3)*exp((y+y^3)^2/6)",
                                   "u = exp(⅙x²)·(x - y³ + (x - y³)³)");
}

static void test_diffequ_solves_parameter_linear_pde(void)
{
    const char *source = "z_y + 2*y*z = x*y^3";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    char *problem = de ? de_to_string(de, style_EXPRESSION) : NULL;
    char *tex = de ? de_to_string(de, style_LATEX) : NULL;

    printf("  parameter-dependent linear PDE\n"
           "    input:    %s\n"
           "    expected: %s\n"
           "    actual:   %s\n",
           source, "z = ½x·(y² - 1) + F(x)·exp(-y²)", text ? string_c_str(text) : "NULL");
    EXPECT_LONG("parameter-linear PDE status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("parameter-linear PDE solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_PARAMETER_LINEAR_PDE);
    EXPECT_TEXT("single-coordinate subscript remains a partial derivative", problem,
                "{ ∂z/∂y + 2*y*z = x*y^3 | y = ?; ;  }");
    EXPECT_TEXT("single-coordinate subscript partial derivative TeX", tex,
                "\\frac{\\partial z}{\\partial y} + 2 y z = x y^{3}");
    EXPECT_TEXT("parameter-linear PDE solution", text ? string_c_str(text) : NULL, "z = ½x·(y² - 1) + F(x)·exp(-y²)");
    EXPECT_POINTER("parameter-linear PDE integrating-factor derivation", result ? de_solve_result_steps(result) : NULL,
                   true);
    if (result && de_solve_result_steps(result)) {
        EXPECT_POINTER("parameter-linear PDE derivation forms integrating factor",
                       strstr(de_solve_result_steps(result), "μ = exp(∫(2y)dy) = exp(y²)"), true);
        EXPECT_POINTER("parameter-linear PDE derivation reaches arbitrary function",
                       strstr(de_solve_result_steps(result), "μz = ½x·(y² - 1)·exp(y²) + F(x)"), true);
    }
    EXPECT_POINTER("parameter-linear PDE TeX derivation", result ? de_solve_result_steps_TeX(result) : NULL, true);

    free(tex);
    free(problem);
    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_parameter_linear_pde_uses_general_rule(void)
{
    const char *source = "u_t + 2*t*u = 2*x*t";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    const char *steps = result ? de_solve_result_steps(result) : NULL;

    printf("  parameter-dependent linear PDE general rule\n"
           "    input:    %s\n"
           "    expected: %s\n"
           "    actual:   %s\n",
           source, "u = x + exp(-t²)·F(x)", text ? string_c_str(text) : "NULL");
    EXPECT_LONG("general parameter-linear PDE status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("general parameter-linear PDE solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_PARAMETER_LINEAR_PDE);
    EXPECT_TEXT("general parameter-linear PDE solution", text ? string_c_str(text) : NULL, "u = x + exp(-t²)·F(x)");
    EXPECT_POINTER("general parameter-linear PDE derivation", steps, true);
    if (steps) {
        EXPECT_POINTER("general derivation uses parsed coordinate",
                       strstr(steps, "Treat x as parameter and solve in t."), true);
        EXPECT_POINTER("general derivation computes parsed integrating factor",
                       strstr(steps, "μ = exp(∫(2t)dt) = exp(t²)"), true);
    }

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_parameter_linear_pde_accepts_parameter_rate(void)
{
    const char *source = "z_y + 2*x*z = x*y^3";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    const char *steps = result ? de_solve_result_steps(result) : NULL;

    printf("  parameter-dependent PDE with parameter rate\n"
           "    input:    %s\n"
           "    expected: %s\n"
           "    actual:   %s\n",
           source, "z = ¾y/x² - ¾y²/x - ⅜/x³ + ½y³ + F(x)·exp(-2xy)", text ? string_c_str(text) : "NULL");
    EXPECT_LONG("parameter-rate PDE status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    EXPECT_LONG("parameter-rate PDE solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_PARAMETER_LINEAR_PDE);
    EXPECT_TEXT("parameter-rate PDE solution", text ? string_c_str(text) : NULL,
                "z = ¾y/x² - ¾y²/x - ⅜/x³ + ½y³ + F(x)·exp(-2xy)");
    EXPECT_POINTER("parameter-rate PDE derivation", steps, true);
    if (steps) {
        EXPECT_POINTER("parameter-rate derivation identifies parameter",
                       strstr(steps, "Treat x as parameter and solve in y."), true);
        EXPECT_POINTER("parameter-rate derivation computes integrating factor",
                       strstr(steps, "μ = exp(∫(2x)dy) = exp(2xy)"), true);
        EXPECT_POINTER("parameter-rate derivation integrates exponential polynomial",
                       strstr(steps, "∂(μz)/∂y = μxy³ = xy³·exp(2xy)"), true);
    }

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

/*
 * Keep this example synchronised with docs/diffequation.md.
 */
static void example_diffequation_solving_an_ode(void)
{
    const char *source = "Dx(y) = x*y; y(0) = 1";
    const char *expected_problem = "{ dy/dx = x*y | x = ?; ; y(0) = 1 }";
    const char *expected_solution = "y = exp(½x²)";
    diffequ_t *ode = de_from_string(source);
    diffequ_solve_result_t *result = de_solve(ode);
    const equation_t *solution = de_solve_result_at(result, 0u);
    char *problem_text = de_to_string(ode, style_EXPRESSION);
    string_t *solution_text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    bool valid = ode && problem_text && strcmp(problem_text, expected_problem) == 0 &&
                 de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
                 de_solve_result_solver(result) == DE_SOLVER_SEPARABLE && de_solve_result_count(result) == 1u &&
                 solution_text && strcmp(string_c_str(solution_text), expected_solution) == 0;

    printf("input = %s\n", source);
    printf("problem = %s\n", problem_text ? problem_text : "NULL");
    printf("solution = %s\n", solution_text ? string_c_str(solution_text) : "NULL");

    string_free(solution_text);
    free(problem_text);
    de_solve_result_free(result);
    de_free(ode);
    ASSERT_TRUE(valid);
}

/*
 * Keep this example synchronised with docs/diffequation.md.
 */
static void example_diffequation_linearising_a_lie_symmetric_ode(void)
{
    const char *source = "y'' + 3*y*y' + y^3 = 0";
    const char *expected_symmetry = "SL(3, ℝ)";
    const char *expected_steps = "Recognise the modified-Emden rule\n"
                                 "      y″ + 3(1)yy′ + (1)²y³ = 0\n"
                                 "Set y = u′/u. Then\n"
                                 "      y″ + 3(1)yy′ + (1)²y³ = u‴/u\n"
                                 "so u‴ = 0 and u is quadratic.\n"
                                 "Equivalently, the point transformation is\n"
                                 "      X = x − 1/y\n"
                                 "      Y = x/y − x²/2\n"
                                 "and d²Y/dX² = 0.";
    const char *expected_solution = "y = (2x + C₁)/(x² + C₁x + C₂)";
    diffequ_t *ode = de_from_string(source);
    diffequ_solve_result_t *result = ode ? de_solve_with_options(ode, DE_SOLVE_OPTION_STEPS) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    const char *symmetry = result ? de_solve_result_symmetry(result) : NULL;
    const char *steps = result ? de_solve_result_steps(result) : NULL;
    string_t *solution_text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    bool valid = ode && result && de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
                 de_solve_result_solver(result) == DE_SOLVER_LINEAR_TRANSFORMATION &&
                 de_solve_result_count(result) == 1u && symmetry && strcmp(symmetry, expected_symmetry) == 0 && steps &&
                 strcmp(steps, expected_steps) == 0 && solution_text &&
                 strcmp(string_c_str(solution_text), expected_solution) == 0;

    printf("input = %s\n", source);
    printf("symmetry = %s\n", symmetry ? symmetry : "NULL");
    printf("linearisation:\n%s\n", steps ? steps : "NULL");
    printf("solution = %s\n", solution_text ? string_c_str(solution_text) : "NULL");

    string_free(solution_text);
    de_solve_result_free(result);
    de_free(ode);
    ASSERT_TRUE(valid);
}

int tests_main(void)
{
    RUN_TEST_CASE(test_diffequ_lifecycle_null_safety);
    RUN_TEST_CASE(test_diffequ_derivations_are_opt_in);
    RUN_TEST_CASE(test_diffequ_constructs_from_equation);
    RUN_TEST_CASE(test_diffequ_parses_separable_ode);
    RUN_TEST_CASE(test_diffequ_parses_linear_ode_and_constant);
    RUN_TEST_CASE(test_diffequ_expression_text_round_trips);
    RUN_TEST_CASE(test_diffequ_parses_ode_shorthand);
    RUN_TEST_CASE(test_diffequ_parses_greek_differential_forms);
    RUN_TEST_CASE(test_diffequ_solves_exact_differential_form);
    RUN_TEST_CASE(test_diffequ_applies_initial_condition_to_exact_differential_form);
    RUN_TEST_CASE(test_diffequ_parses_and_solves_prime_ode_shorthand);
    RUN_TEST_CASE(test_diffequ_parses_subscript_partial_derivatives);
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
    RUN_TEST_CASE(test_diffequ_solves_polynomial_homogeneous_initial_value_problem);
    RUN_TEST_CASE(test_diffequ_integrates_rational_homogeneous_problem);
    RUN_TEST_CASE(test_diffequ_solves_affine_combination_substitution);
    RUN_TEST_CASE(test_diffequ_solves_shifted_homogeneous_substitution);
    RUN_TEST_CASE(test_diffequ_solves_linear_change_of_variables);
    RUN_TEST_CASE(test_diffequ_retains_arbitrary_constant);
    RUN_TEST_CASE(test_diffequ_preserves_zero_singular_solution);
    RUN_TEST_CASE(test_diffequ_solves_quadratic_bernoulli_problem);
    RUN_TEST_CASE(test_diffequ_normalizes_bernoulli_arbitrary_constant);
    RUN_TEST_CASE(test_diffequ_solves_derivative_quadratic_problem);
    RUN_TEST_CASE(test_diffequ_linearizes_exact_third_order_problem);
    RUN_TEST_CASE(test_diffequ_linearizes_modified_emden_problem);
    RUN_TEST_CASE(test_diffequ_linearizes_scaled_modified_emden_problem);
    RUN_TEST_CASE(test_diffequ_modified_emden_uses_coefficient_rule);
    RUN_TEST_CASE(test_diffequ_solves_hydrogen_ground_state);
    RUN_TEST_CASE(test_diffequ_stationary_eigenfunction_uses_general_rule);
    RUN_TEST_CASE(test_diffequ_rejects_quartic_emden_point_linearization);
    RUN_TEST_CASE(test_diffequ_solves_second_order_sturm_liouville_problem);
    RUN_TEST_CASE(test_diffequ_solves_affine_factorized_second_order_problem);
    RUN_TEST_CASE(test_diffequ_applies_affine_factorized_initial_conditions);
    RUN_TEST_CASE(test_diffequ_does_not_invent_cubic_potential_functions);
    RUN_TEST_CASE(test_diffequ_solves_repeated_characteristic_root);
    RUN_TEST_CASE(test_diffequ_solves_oscillatory_sturm_liouville_problem);
    RUN_TEST_CASE(test_diffequ_normalizes_variable_coefficient_sturm_liouville);
    RUN_TEST_CASE(test_diffequ_solves_power_law_bessel_family);
    RUN_TEST_CASE(test_diffequ_solves_forced_power_law_lommel_family);
    RUN_TEST_CASE(test_diffequ_solves_third_order_constant_coefficient_problem);
    RUN_TEST_CASE(test_diffequ_solves_high_order_repeated_root);
    RUN_TEST_CASE(test_diffequ_solves_nonhomogeneous_constant_coefficient_problem);
    RUN_TEST_CASE(test_diffequ_solves_secant_cubed_forcing);
    RUN_TEST_CASE(test_diffequ_solves_logarithmic_forcing);
    RUN_TEST_CASE(test_diffequ_solves_repeated_complex_roots);
    RUN_TEST_CASE(test_diffequ_solves_degree_six_characteristic_polynomial);
    RUN_TEST_CASE(test_diffequ_resolves_polynomial_differential_operator);
    RUN_TEST_CASE(test_diffequ_defaults_bare_differential_operator);
    RUN_TEST_CASE(test_diffequ_solves_maximum_repeated_quadratic_power);
    RUN_TEST_CASE(test_diffequ_general_solution_with_distinct_real_roots);
    RUN_TEST_CASE(test_diffequ_general_solution_with_repeated_real_root);
    RUN_TEST_CASE(test_diffequ_general_solution_with_real_and_complex_roots);
    RUN_TEST_CASE(test_diffequ_general_solution_with_repeated_complex_roots);
    RUN_TEST_CASE(test_diffequ_general_sixth_order_solution);
    RUN_TEST_CASE(test_diffequ_general_nonhomogeneous_solution);
    RUN_TEST_CASE(test_diffequ_general_trigonometric_forcing_solution);
    RUN_TEST_CASE(test_diffequ_general_third_order_forced_solution);
    RUN_TEST_CASE(test_diffequ_parses_pde_boundary_arguments);
    RUN_TEST_CASE(test_diffequ_solves_two_dimensional_laplace_equation);
    RUN_TEST_CASE(test_diffequ_solves_constant_transport_from_y_boundary);
    RUN_TEST_CASE(test_diffequ_solves_constant_transport_from_x_boundary);
    RUN_TEST_CASE(test_diffequ_solves_variable_forcing_from_time_boundary);
    RUN_TEST_CASE(test_diffequ_solves_parametric_characteristic_boundary);
    RUN_TEST_CASE(test_diffequ_solves_scaled_coordinate_characteristics);
    RUN_TEST_CASE(test_diffequ_solves_exponential_characteristics);
    RUN_TEST_CASE(test_diffequ_solves_unbounded_homogeneous_transport);
    RUN_TEST_CASE(test_diffequ_solves_unbounded_inhomogeneous_transport);
    RUN_TEST_CASE(test_diffequ_solves_transport_with_reaction_term);
    RUN_TEST_CASE(test_diffequ_solves_transport_with_variable_forcing);
    RUN_TEST_CASE(test_diffequ_solves_mixed_phase_trigonometric_forcing);
    RUN_TEST_CASE(test_diffequ_solves_mixed_phase_unary_forcing);
    RUN_TEST_CASE(test_diffequ_uses_builtin_alias_as_dependent_symbol);
    RUN_TEST_CASE(test_diffequ_solves_scaled_additive_transport_forcing);
    RUN_TEST_CASE(test_diffequ_solves_polynomial_reaction_forcing);
    RUN_TEST_CASE(test_diffequ_solves_mixed_polynomial_reaction_forcing);
    RUN_TEST_CASE(test_diffequ_solves_trigonometric_reaction_forcing);
    RUN_TEST_CASE(test_diffequ_solves_exponential_reaction_forcing);
    RUN_TEST_CASE(test_diffequ_solves_three_variable_transport);
    RUN_TEST_CASE(test_diffequ_solves_scaled_three_variable_transport);
    RUN_TEST_CASE(test_diffequ_solves_nonlinear_characteristic_pde);
    RUN_TEST_CASE(test_diffequ_nonlinear_characteristic_uses_power_rule);
    RUN_TEST_CASE(test_diffequ_spiral_characteristic_uses_linear_field_rule);
    RUN_TEST_CASE(test_diffequ_solves_quadratic_characteristic_evolution);
    RUN_TEST_CASE(test_diffequ_applies_quadratic_characteristic_boundary);
    RUN_TEST_CASE(test_diffequ_solves_dependent_square_characteristic_pde);
    RUN_TEST_CASE(test_diffequ_applies_dependent_square_boundary);
    RUN_TEST_CASE(test_diffequ_applies_signed_dependent_square_boundary);
    RUN_TEST_CASE(test_diffequ_solves_invariant_forced_square_pde);
    RUN_TEST_CASE(test_diffequ_solves_reciprocal_forced_square_pde);
    RUN_TEST_CASE(test_diffequ_solves_rotating_characteristic_pde);
    RUN_TEST_CASE(test_diffequ_solves_cyclic_lagrange_pde);
    RUN_TEST_CASE(test_diffequ_solves_monomial_linear_characteristic_pde);
    RUN_TEST_CASE(test_diffequ_solves_forced_monomial_characteristic_pde);
    RUN_TEST_CASE(test_diffequ_solves_forced_radial_characteristic_pde);
    RUN_TEST_CASE(test_diffequ_solves_separable_trigonometric_characteristic_pde);
    RUN_TEST_CASE(test_diffequ_solves_cross_coordinate_characteristic_pde);
    RUN_TEST_CASE(test_diffequ_solves_parameter_linear_pde);
    RUN_TEST_CASE(test_diffequ_parameter_linear_pde_uses_general_rule);
    RUN_TEST_CASE(test_diffequ_parameter_linear_pde_accepts_parameter_rate);

    TEST_SECTION("README Output Example");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_diffequation_solving_an_ode, readme_examples, "diffequation,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_diffequation_linearising_a_lie_symmetric_ode, readme_examples,
                                  "diffequation,readme,output,lie-symmetry");

    return TESTS_EXIT_CODE();
}
