#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diffequation.h"
#include "test_harness.h"

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static bool test_diffequ_want_text(const char *label, const char *got, const char *want, const char *file,
                                     int line)
{
    printf("  %s\n"
           "    want: %s\n"
           "    got:   %s\n",
           label, want ? want : "NULL", got ? got : "NULL");
    return test_assert_cstr_eq(got, want, file, line);
}

static bool test_diffequ_want_pointer(const char *label, const void *got, bool want_nonnull, const char *file,
                                        int line)
{
    return test_diffequ_want_text(label, got ? "non-NULL" : "NULL", want_nonnull ? "non-NULL" : "NULL", file,
                                    line);
}

static bool test_diffequ_want_long(const char *label, long got, long want, const char *file, int line)
{
    printf("  %s\n"
           "    want: %ld\n"
           "    got:   %ld\n",
           label, want, got);
    return test_assert_long_eq(got, want, file, line);
}

static bool test_diffequ_want_number(const char *label, number_t got, number_t want, const char *file,
                                       int line)
{
    string_t *got_text = num_to_string(got);
    string_t *want_text = num_to_string(want);
    bool equal;

    printf("  %s\n"
           "    want: %s\n"
           "    got:   %s\n",
           label, want_text ? string_c_str(want_text) : "NULL",
           got_text ? string_c_str(got_text) : "NULL");
    equal = got_text && want_text && num_eq(got, want);
    string_free(want_text);
    string_free(got_text);
    return test_assert_true(equal, file, line, label);
}

#define WANT_TEXT(label, got, want)                                                                           \
    TEST_HARNESS_RETURN_UNLESS(test_diffequ_want_text((label), (got), (want), __FILE__, __LINE__))

#define WANT_POINTER(label, got, want_nonnull)                                                                \
    TEST_HARNESS_RETURN_UNLESS(test_diffequ_want_pointer((label), (got), (want_nonnull), __FILE__, __LINE__))

#define WANT_LONG(label, got, want)                                                                           \
    TEST_HARNESS_RETURN_UNLESS(test_diffequ_want_long((label), (got), (want), __FILE__, __LINE__))

#define WANT_NUMBER(label, got, want)                                                                         \
    TEST_HARNESS_RETURN_UNLESS(test_diffequ_want_number((label), (got), (want), __FILE__, __LINE__))

static void test_diffequ_lie_free_particle(void)
{
    diffequ_t *de = de_from_string("y'' = 0");
    de_lie_t *lie = de_lie_new(de);
    WANT_POINTER("free-particle normal form", lie, true);
    WANT_LONG("free-particle rhs", expr_is_exact_zero(de_lie_rhs(lie)), 1L);
    for (size_t k = 0u; k < 2u; ++k) {
        expr_t *invariant = de_lie_invariant(lie, k);
        WANT_LONG("free-particle relative invariant", expr_is_exact_zero(invariant), 1L);
        expr_free(invariant);
    }
    equation_t *determining = de_lie_determining_equation(lie);
    WANT_POINTER("general determining equation", determining, true);
    string_t *text = equ_to_text(determining, style_LATEX);
    WANT_POINTER("determining equation TeX", text, true);
    printf("  determining PDE: %s\n", string_c_str(text));
    string_free(text);
    equ_free(determining);
    const long dimensions[3] = {2L, 6L, 8L};
    for (size_t degree = 0u; degree <= 2u; ++degree) {
        matrix_t *generators = de_lie_polynomial_generators(lie, degree);
        WANT_POINTER("polynomial generators", generators, true);
        WANT_LONG("polynomial symmetry dimension", (long)mat_get_col_count(generators), dimensions[degree]);
        if (degree == 2u) {
            matrix_t *constants = de_lie_structure_constants(lie, generators);
            WANT_POINTER("free-particle algebra closes", constants, true);
            WANT_LONG("structure-constant rows", (long)mat_get_row_count(constants), 64L);
            WANT_LONG("structure-constant columns", (long)mat_get_col_count(constants), 8L);
            mat_free(constants);
        }
        mat_free(generators);
    }
    de_lie_free(lie);
    de_free(de);
}

static void test_diffequ_lie_invariants_and_normalisation(void)
{
    static const struct { const char *equation; const char *invariant; long dimension; } cases[] = {
        {"y'' + 3*y*y' + y^4 = 0", "36*y-72*y^2", 1L},
        {"y'' + 6*y*y' + 4*y^3 = 0", "0", 3L},
        {"2*Dtt(u) + 6*u*Dt(u) + 2*u^4 = 0", "36*u-72*u^2", 1L}
    };
    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        diffequ_t *de = de_from_string(cases[i].equation);
        de_lie_t *lie = de_lie_new(de);
        WANT_POINTER("normalised second-order equation", lie, true);
        expr_t *invariant = de_lie_invariant(lie, 1u);
        expr_t *expected = expr_from_string(cases[i].invariant, NULL);
        expr_t *symbol = expr_new_named_var(NUM_NAN, i == 2u ? "u" : "y");
        expr_t *linked = expr_substitute(expected, symbol, de_lie_coordinate(lie, 1u));
        expr_free(symbol);
        expr_free(expected);
        expected = linked;
        expr_t *difference = expr_sub_simplify_owned(expr_clone(invariant), expected);
        string_t *text = expr_to_text(invariant, style_UNBOUND);
        printf("  computed invariant: %s\n", string_c_str(text));
        WANT_LONG("computed Lie-Tresse invariant", expr_is_exact_zero(difference), 1L);
        matrix_t *generators = de_lie_polynomial_generators(lie, 2u);
        WANT_POINTER("derived generators", generators, true);
        WANT_LONG("degree-two search dimension", (long)mat_get_col_count(generators), cases[i].dimension);
        mat_free(generators);
        string_free(text);
        expr_free(difference);
        expr_free(invariant);
        de_lie_free(lie);
        de_free(de);
    }
}

static void test_diffequ_lie_nonpolynomial_and_validation(void)
{
    diffequ_t *de = de_from_string("y'' + y = 0; y(0) = 1");
    de_lie_t *lie = de_lie_new(de);
    expr_t *zero = expr_const_zero();
    expr_t *eta = expr_sin(de_lie_coordinate(lie, 0u));
    expr_t *residual = de_lie_residual(lie, zero, eta);
    WANT_LONG("non-polynomial sine generator is verified", expr_is_exact_zero(residual), 1L);
    WANT_POINTER("velocity-dependent point generator rejected",
                  de_lie_residual(lie, de_lie_coordinate(lie, 2u), eta), false);
    WANT_POINTER("invalid prolongation order", de_lie_prolongation(lie, zero, eta, 3u), false);
    WANT_POINTER("invalid polynomial degree", de_lie_polynomial_generators(lie, 5u), false);
    WANT_POINTER("invalid invariant index", de_lie_invariant(lie, 2u), false);
    WANT_POINTER("invalid coordinate index", de_lie_coordinate(lie, 3u), false);
    expr_free(residual);
    expr_free(eta);
    expr_free(zero);
    de_lie_free(lie);
    de_free(de);
    de = de_from_string("y'' = sin(y)");
    lie = de_lie_new(de);
    WANT_POINTER("non-polynomial normal form is supported", lie, true);
    WANT_POINTER("unsupported polynomial coefficient search is explicit", de_lie_polynomial_generators(lie, 2u), false);
    de_lie_free(lie);
    de_free(de);
    de = de_from_string("y'' = (y')^4");
    lie = de_lie_new(de);
    expr_t *invariant = de_lie_invariant(lie, 0u);
    expr_t *twenty_four = expr_const_long(24L);
    WANT_LONG("first Lie-Tresse invariant detects quartic velocity dependence",
                expr_struct_eq(invariant, twenty_four), 1L);
    expr_free(twenty_four);
    expr_free(invariant);
    de_lie_free(lie);
    de_free(de);
    static const char *invalid[] = {"y' = y", "(y'')^2 = y", "u_xx + u_yy = 0"};
    for (size_t i = 0u; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        de = de_from_string(invalid[i]);
        WANT_POINTER("unsupported normal form", de_lie_new(de), false);
        de_free(de);
    }
    WANT_POINTER("null analysis input", de_lie_new(NULL), false);
    WANT_POINTER("null rhs", de_lie_rhs(NULL), false);
    WANT_POINTER("null determining equation", de_lie_determining_equation(NULL), false);
    WANT_POINTER("null polynomial search", de_lie_polynomial_generators(NULL, 2u), false);
    WANT_POINTER("null bracket", de_lie_bracket(NULL, NULL, 0u, 0u), false);
    WANT_POINTER("null structure constants", de_lie_structure_constants(NULL, NULL), false);
    de_lie_free(NULL);
}

static void test_diffequ_lie_brackets_and_search_limits(void)
{
    diffequ_t *de = de_from_string("y'' = 0");
    de_lie_t *lie = de_lie_new(de);
    expr_t *zero = expr_const_zero(), *one = expr_const_one();
    expr_t *x = expr_clone(de_lie_coordinate(lie, 0u));
    expr_t *y = expr_clone(de_lie_coordinate(lie, 1u));
    matrix_t *basis = mat_new_sparse_expr(2u, 3u);
    mat_set(basis, 0u, 0u, &one);
    mat_set(basis, 0u, 1u, &x);
    mat_set(basis, 1u, 2u, &y);
    matrix_t *bracket = de_lie_bracket(lie, basis, 0u, 1u);
    expr_t *component = NULL;
    WANT_POINTER("translation-dilation bracket", bracket, true);
    mat_get(bracket, 0u, 0u, &component);
    WANT_LONG("[d/dx,x*d/dx] = d/dx", expr_struct_eq(component, one), 1L);
    matrix_t *constants = de_lie_structure_constants(lie, basis);
    WANT_POINTER("three-generator subalgebra closes", constants, true);
    mat_get(constants, 1u, 0u, &component);
    WANT_LONG("C_01^0 = 1", expr_struct_eq(component, one), 1L);
    mat_get(constants, 3u, 0u, &component);
    expr_t *negative = expr_const_long(-1L);
    WANT_LONG("C_10^0 = -1", expr_struct_eq(component, negative), 1L);
    expr_free(negative);
    mat_free(constants);
    mat_free(bracket);
    mat_free(basis);
    expr_t *square = expr_mul(x, x);
    expr_t *xy = expr_mul(x, y);
    expr_t *nonclosed[4] = {one, square, zero, xy};
    basis = mat_create_expr(2u, 2u, nonclosed);
    WANT_POINTER("non-closed polynomial symmetry basis rejected", de_lie_structure_constants(lie, basis), false);
    mat_set(basis, 0u, 1u, &one);
    mat_set(basis, 1u, 1u, &zero);
    WANT_POINTER("dependent basis rejected", de_lie_structure_constants(lie, basis), false);
    mat_free(basis);
    basis = de_lie_polynomial_generators(lie, 4u);
    WANT_POINTER("degree-four search", basis, true);
    WANT_LONG("higher degree does not invent free-particle symmetries", (long)mat_get_col_count(basis), 8L);
    mat_free(basis);
    expr_free(xy);
    expr_free(square);
    expr_free(y);
    expr_free(x);
    expr_free(one);
    expr_free(zero);
    de_lie_free(lie);
    de_free(de);

    de = de_from_string("y'' = x + y^2");
    lie = de_lie_new(de);
    basis = de_lie_polynomial_generators(lie, 2u);
    WANT_POINTER("empty search is distinct from unsupported", basis, true);
    WANT_LONG("no degree-two generators", (long)mat_get_col_count(basis), 0L);
    constants = de_lie_structure_constants(lie, basis);
    WANT_POINTER("empty algebra has an empty structure-constant matrix", constants, true);
    WANT_LONG("empty structure-constant rows", (long)mat_get_row_count(constants), 0L);
    WANT_LONG("empty structure-constant columns", (long)mat_get_col_count(constants), 0L);
    mat_free(constants);
    WANT_POINTER("non-autonomous reduction rejected", de_lie_autonomous_reduction(lie), false);
    mat_free(basis);
    de_lie_free(lie);
    de_free(de);
}

static void test_diffequ_lie_velocity_symbol_and_reduction(void)
{
    diffequ_t *de = de_from_string("y'' + p*y' + p_1*y = 0");
    de_lie_t *lie = de_lie_new(de);
    WANT_POINTER("parameter-dependent normal form", lie, true);
    WANT_TEXT("velocity does not capture either parameter", expr_symbol_name(de_lie_coordinate(lie, 2u)), "p₂");
    WANT_POINTER("unresolved parameter coefficients are not treated as numeric",
                  de_lie_polynomial_generators(lie, 1u), false);
    de_lie_free(lie);
    de_free(de);
    de = de_from_string("y'' + 3*y*y' + y^4 = 0");
    lie = de_lie_new(de);
    equation_t *reduced = de_lie_autonomous_reduction(lie);
    WANT_POINTER("autonomous order reduction", reduced, true);
    string_t *text = equ_to_text(reduced, style_UNBOUND);
    printf("  reduced equation: %s\n", string_c_str(text));
    const expr_t *left = NULL, *right = NULL;
    WANT_LONG("reduced left-hand side is a product", expr_match_mul_expr(equ_lhs(reduced), &left, &right), 1L);
    const expr_t *derivative = expr_is_formal_derivative(left) ? left : right;
    WANT_LONG("reduced equation has first derivative", (long)expr_formal_derivative_order(derivative), 1L);
    WANT_LONG("new independent coordinate is y",
                expr_struct_eq(expr_formal_derivative_wrt_at(derivative, 0u), de_lie_coordinate(lie, 1u)), 1L);
    string_free(text);
    equ_free(reduced);
    de_lie_free(lie);
    de_free(de);
}

static void test_diffequ_lifecycle_null_safety(void)
{
    diffequ_solve_result_t *invalid_result;

    WANT_POINTER("de_new(NULL)", de_new(NULL), false);
    WANT_POINTER("de_from_string(NULL)", de_from_string(NULL), false);
    WANT_POINTER("de_from_text(NULL)", de_from_text(NULL), false);
    WANT_POINTER("de_equation(NULL)", de_equation(NULL), false);
    WANT_LONG("de_independent_count(NULL)", (long)de_independent_count(NULL), 0L);
    WANT_POINTER("de_independent_at(NULL, 0)", de_independent_at(NULL, 0u), false);
    WANT_POINTER("de_constants(NULL)", de_constants(NULL), false);
    WANT_POINTER("de_constant(NULL, \"a\")", de_constant(NULL, "a"), false);
    WANT_LONG("de_condition_count(NULL)", (long)de_condition_count(NULL), 0L);
    WANT_POINTER("de_condition_at(NULL, 0)", de_condition_at(NULL, 0u), false);
    WANT_LONG("de_condition_argument_count(NULL, 0)", (long)de_condition_argument_count(NULL, 0u), 0L);
    WANT_POINTER("de_condition_argument_at(NULL, 0, 0)", de_condition_argument_at(NULL, 0u, 0u), false);
    invalid_result = de_solve(NULL);
    WANT_POINTER("de_solve(NULL)", invalid_result, true);
    WANT_LONG("de_solve(NULL) status", (long)de_solve_result_status(invalid_result), (long)DE_SOLVE_STATUS_INVALID);
    WANT_LONG("de_solve_result_count(NULL)", (long)de_solve_result_count(NULL), 0L);
    WANT_POINTER("de_solve_result_at(NULL, 0)", de_solve_result_at(NULL, 0u), false);

    de_solve_result_free(invalid_result);
    de_solve_result_free(NULL);
    de_free(NULL);
}

static void test_diffequ_derivations_are_opt_in(void)
{
    static const char *sources[] = {"Dx(y) = x*y", "y'' + 3*y*y' + y^3 = 0", "z_y + 2*y*z = x*y^3",
                                    "phi_xx + phi_yy = 0", "z_xx - 3z_yx + 2z_yy = 0"};

    for (size_t i = 0u; i < sizeof(sources) / sizeof(sources[0]); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;

        WANT_POINTER("default solve result", result, true);
        WANT_LONG("default solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
        WANT_POINTER("default solve omits plain-text derivation", de_solve_result_steps(result), false);
        WANT_POINTER("default solve omits TeX derivation", de_solve_result_steps_TeX(result), false);

        de_solve_result_free(result);
        de_free(de);
    }

    {
        diffequ_t *de = de_from_string("Dx(y) = x*y");
        diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;

        WANT_POINTER("solve result with derivation", result, true);
        WANT_POINTER("opt-in plain-text derivation", de_solve_result_steps(result), true);
        WANT_POINTER("opt-in TeX derivation", de_solve_result_steps_TeX(result), true);

        de_solve_result_free(result);
        de_free(de);
    }
}

static void test_diffequ_constructs_from_equation(void)
{
    equation_t *equation = equ_from_string("x = 1");
    diffequ_t *de;
    string_t *text;

    WANT_POINTER("source equation", equation, true);
    if (!equation)
        return;

    de = de_new(equation);
    WANT_POINTER("constructed differential equation", de, true);
    equ_free(equation);
    if (!de)
        return;

    WANT_POINTER("retained base equation", de_equation(de), true);
    text = de_to_text(de, style_UNBOUND);
    WANT_POINTER("rendered base equation", text, true);
    if (text)
        WANT_TEXT("rendered base equation text", string_c_str(text), "x = 1");

    string_free(text);
    de_free(de);
}

static void test_diffequ_parses_separable_ode(void)
{
    diffequ_t *de = de_from_string("{ Dx(y) = x*y | x = ?;; y(0) = 1 }");
    string_t *base_text;
    char *text;

    WANT_POINTER("parsed separable ODE", de, true);
    if (!de)
        return;

    WANT_POINTER("base equation", de_equation(de), true);
    WANT_LONG("independent-variable count", (long)de_independent_count(de), 1L);
    WANT_POINTER("first independent variable", de_independent_at(de, 0u), true);
    WANT_LONG("condition count", (long)de_condition_count(de), 1L);
    WANT_POINTER("first condition", de_condition_at(de, 0u), true);

    base_text = equ_to_text(de_equation(de), style_UNBOUND);
    WANT_POINTER("rendered base equation", base_text, true);
    if (base_text)
        WANT_TEXT("rendered base equation text", string_c_str(base_text), "Dx(y) = xy");

    text = de_to_string(de, style_EXPRESSION);
    WANT_POINTER("rendered differential equation", text, true);
    if (text)
        WANT_TEXT("rendered differential-equation text", text, "{ dy/dx = x*y | x = ?; ; y(0) = 1 }");

    free(text);
    string_free(base_text);
    de_free(de);
}

static void test_diffequ_parses_linear_ode_and_constant(void)
{
    diffequ_t *de = de_from_string("{ Dx(y) + a*y = x | x = ?; a = 2; y(0) = 1 }");
    expr_t *constant;
    number_t value;

    WANT_POINTER("parsed linear ODE", de, true);
    if (!de)
        return;

    constant = de_constant(de, "a");
    WANT_POINTER("constant a", constant, true);
    if (constant) {
        number_t want = num_create_from_long(2L);

        value = expr_eval(constant);
        WANT_NUMBER("constant a value", value, want);
        num_destroy(&value);
        num_destroy(&want);
    }

    de_free(de);
}

static void test_diffequ_expression_text_round_trips(void)
{
    diffequ_t *first = de_from_string("{ Dxx(y) + 3*Dx(y) + 2*y = 0 | x = ?;; y(0) = 1, Dx(y)(0) = 0 }");
    diffequ_t *second = NULL;
    char *first_text = NULL;
    char *second_text = NULL;

    WANT_POINTER("first parse", first, true);
    if (!first)
        return;

    first_text = de_to_string(first, style_EXPRESSION);
    WANT_POINTER("first rendered text", first_text, true);
    if (first_text)
        second = de_from_string(first_text);
    WANT_POINTER("second parse", second, true);
    if (second)
        second_text = de_to_string(second, style_EXPRESSION);
    WANT_POINTER("second rendered text", second_text, true);
    if (first_text && second_text)
        WANT_TEXT("round-trip text", second_text, first_text);

    free(second_text);
    free(first_text);
    de_free(second);
    de_free(first);
}

static void test_diffequ_parses_ode_shorthand(void)
{
    diffequ_t *de = de_from_string("Dxx(y) = y; y(0) = 1; y'(0) = 1");
    char *text;

    WANT_POINTER("parsed shorthand ODE", de, true);
    if (!de)
        return;

    WANT_LONG("inferred independent-variable count", (long)de_independent_count(de), 1L);
    WANT_LONG("shorthand condition count", (long)de_condition_count(de), 2L);

    text = de_to_string(de, style_EXPRESSION);
    WANT_POINTER("normalized shorthand text", text, true);
    if (text)
        WANT_TEXT("normalized shorthand", text, "{ d²y/dx² = y | x = ?; ; y(0) = 1, dy/dx(0) = 1 }");

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

    WANT_POINTER("parsed @theta differential form", alias, true);
    WANT_POINTER("parsed plain theta differential form", plain, true);
    WANT_POINTER("parsed another plain Greek differential", alpha, true);
    WANT_POINTER("Greek derivative text round-trips", round_trip, true);
    WANT_LONG("@theta differential-form independent count", (long)de_independent_count(alias), 1L);
    WANT_LONG("plain theta differential-form independent count", (long)de_independent_count(plain), 1L);
    WANT_TEXT("Greek differential forms agree", plain_TeX, alias_TeX);
    WANT_TEXT("Greek derivative round-trip agrees", round_trip_TeX, alias_TeX);
    WANT_POINTER("@theta differential form displays Greek theta", alias_text ? strstr(alias_text, "dθ") : NULL, true);
    WANT_POINTER("plain theta differential form displays Greek theta", plain_text ? strstr(plain_text, "dθ") : NULL,
                   true);
    WANT_POINTER("plain alpha differential form displays Greek alpha", alpha_text ? strstr(alpha_text, "dα") : NULL,
                   true);
    WANT_TEXT("Greek differential-form text agrees", plain_text, alias_text);

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

    WANT_POINTER("parsed exact differential form", de, true);
    WANT_POINTER("exact differential-form result", result, true);
    WANT_LONG("exact differential-form status", result ? (long)de_solve_result_status(result) : -1L,
                (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("exact differential-form solver", result ? (long)de_solve_result_solver(result) : -1L,
                (long)DE_SOLVER_EXACT_FIRST_ORDER);
    WANT_LONG("exact differential-form solution count", result ? (long)de_solve_result_count(result) : -1L, 2L);
    WANT_TEXT("exact differential-form positive branch", positive_text ? string_c_str(positive_text) : NULL,
                "r = 1/(2·cos²(θ))·(sin(θ) - √(sin²(θ) - C·cos²(θ)))");
    WANT_TEXT("exact differential-form negative branch", negative_text ? string_c_str(negative_text) : NULL,
                "r = 1/(2·cos²(θ))·(sin(θ) + √(sin²(θ) - C·cos²(θ)))");

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

    WANT_POINTER("parsed conditioned exact differential form", de, true);
    WANT_POINTER("conditioned exact differential-form result", result, true);
    WANT_LONG("conditioned exact differential-form status", result ? (long)de_solve_result_status(result) : -1L,
                (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("conditioned exact differential-form solver", result ? (long)de_solve_result_solver(result) : -1L,
                (long)DE_SOLVER_EXACT_FIRST_ORDER);
    WANT_LONG("conditioned exact differential-form solution count",
                result ? (long)de_solve_result_count(result) : -1L, 1L);
    WANT_TEXT("conditioned exact differential-form branch", solution_text ? string_c_str(solution_text) : NULL,
                "y = √(1/(3x)·(14 - x³))");

    string_free(solution_text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_divided_differential_form(void)
{
    const char *source = "dx/sqrt(x^2+y^2) + (1/y - x/(y*sqrt(x^2+y^2)))dy = 0; y(2) = 1";
    const char *family_source = "dx/sqrt(x^2+y^2) + (1/y - x/(y*sqrt(x^2+y^2)))dy = 0";
    diffequ_t *de = de_from_string(source);
    diffequ_t *family_de = de_from_string(family_source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    diffequ_solve_result_t *family_result = family_de ? de_solve(family_de) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    const equation_t *family_solution = family_result ? de_solve_result_at(family_result, 0u) : NULL;
    string_t *solution_text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    string_t *family_solution_text = family_solution ? equ_to_text(family_solution, style_UNBOUND) : NULL;

    WANT_POINTER("parsed divided differential form", de, true);
    WANT_POINTER("parsed unconditioned divided differential form", family_de, true);
    WANT_POINTER("divided differential-form result", result, true);
    WANT_POINTER("unconditioned divided differential-form result", family_result, true);
    WANT_LONG("divided differential-form status", result ? (long)de_solve_result_status(result) : -1L,
                (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("divided differential-form solver", result ? (long)de_solve_result_solver(result) : -1L,
                (long)DE_SOLVER_EXACT_FIRST_ORDER);
    WANT_LONG("divided differential-form solution count", result ? (long)de_solve_result_count(result) : -1L, 1L);
    WANT_TEXT("divided differential-form branch", solution_text ? string_c_str(solution_text) : NULL,
                "y = √((√(5) + 2)·(√(5) - 2x + 2))");
    WANT_LONG("unconditioned divided differential-form status",
                family_result ? (long)de_solve_result_status(family_result) : -1L, (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("unconditioned divided differential-form solver",
                family_result ? (long)de_solve_result_solver(family_result) : -1L,
                (long)DE_SOLVER_EXACT_FIRST_ORDER);
    WANT_LONG("unconditioned divided differential-form solution count",
                family_result ? (long)de_solve_result_count(family_result) : -1L, 1L);
    WANT_TEXT("unconditioned divided differential-form family",
                family_solution_text ? string_c_str(family_solution_text) : NULL, "y = ±C·√(1 - 2x/C)");

    string_free(family_solution_text);
    string_free(solution_text);
    de_solve_result_free(family_result);
    de_solve_result_free(result);
    de_free(family_de);
    de_free(de);
}

static void test_diffequ_derivative_quotient_TeX(void)
{
    static const struct { const char *source; const char *TeX; } cases[] = {
        {"u_tt/v^2 = 0", "\\frac{1}{v^{2}}\\,\\frac{\\partial^{2} u}{\\partial t^{2}} = 0"},
        {"2u_tt/v^2 = 0", "\\frac{2}{v^{2}}\\,\\frac{\\partial^{2} u}{\\partial t^{2}} = 0"},
        {"-2u_tt/v^2 = 0", "-\\frac{2}{v^{2}}\\,\\frac{\\partial^{2} u}{\\partial t^{2}} = 0"},
        {"u_tt/(-v^2) = 0", "-\\frac{1}{v^{2}}\\,\\frac{\\partial^{2} u}{\\partial t^{2}} = 0"},
        {"u - 2u_tt/v^2 = 0", "u - \\frac{2}{v^{2}}\\,\\frac{\\partial^{2} u}{\\partial t^{2}} = 0"},
        {"u_xy/(1+x) = 0", "\\frac{1}{x + 1}\\,\\frac{\\partial^{2} u}{\\partial y\\,\\partial x} = 0"},
        {"y''/v^2 = 0", "\\frac{1}{v^{2}}\\,\\frac{d^{2} y}{d x^{2}} = 0"},
        {"a*u_tt*u_x/v^2 = 0",
         "\\frac{a}{v^{2}}\\,\\frac{\\partial^{2} u}{\\partial t^{2}}\\,\\frac{\\partial u}{\\partial x} = 0"}
    };
    for (size_t i = 0u; i < sizeof(cases) / sizeof(*cases); ++i) {
        diffequ_t *de = de_from_string(cases[i].source);
        char *TeX = de ? de_to_string(de, style_LATEX) : NULL;
        WANT_TEXT(cases[i].source, TeX, cases[i].TeX);
        free(TeX);
        de_free(de);
    }
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

    WANT_POINTER("parsed prime-notation ODE", de, true);
    WANT_TEXT("normalized prime-notation ODE", equation_text ? string_c_str(equation_text) : NULL, "Dxx(y) + 4y = 0");
    WANT_LONG("prime-notation solve status", result ? (long)de_solve_result_status(result) : -1L,
                (long)DE_SOLVE_STATUS_SOLVED);
    WANT_TEXT("prime-notation solution", solution_text ? string_c_str(solution_text) : NULL,
                "y = C₁·cos(2x) + C₂·sin(2x)");
    WANT_TEXT("prime notation uses the declared independent variable",
                explicit_equation_text ? string_c_str(explicit_equation_text) : NULL, "Dtt(y) + 4y = 0");
    WANT_TEXT("declared-variable prime-notation solution",
                explicit_solution_text ? string_c_str(explicit_solution_text) : NULL, "y = C₁·cos(2t) + C₂·sin(2t)");
    WANT_TEXT("prime notation accepts the standard constant e",
                forced_equation_text ? string_c_str(forced_equation_text) : NULL, "Dxx(y) + 4y = exp(x)");
    WANT_LONG("exponential-forcing solve status", forced_result ? (long)de_solve_result_status(forced_result) : -1L,
                (long)DE_SOLVE_STATUS_SOLVED);
    WANT_TEXT("compact exponential-forcing solution",
                forced_solution_text ? string_c_str(forced_solution_text) : NULL,
                "y = ⅕·exp(x) + C₁·cos(2x) + C₂·sin(2x)");
    WANT_TEXT("prime x defaults to differentiation with respect to t",
                time_dependent_equation_text ? string_c_str(time_dependent_equation_text) : NULL, "Dtt(x) + x = 0");
    WANT_TEXT("time-dependent prime-notation solution",
                time_dependent_solution_text ? string_c_str(time_dependent_solution_text) : NULL,
                "x = C₁·cos(t) + C₂·sin(t)");
    WANT_TEXT("ordinary derivative fraction input",
                fraction_equation_text ? string_c_str(fraction_equation_text) : NULL, "Dxx(y) + 4y = 0");
    WANT_TEXT("ordinary derivative fraction TeX", fraction_TeX,
                "\\frac{d^{2} y}{d x^{2}} + 4\\mkern-2mu y = 0");

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
           "    want: %s\n"
           "    got:   %s\n"
           "    input:    %s\n"
           "    want: %s\n"
           "    got:   %s\n",
           first_source, "Dx(u) + Dy(u) = 0", first_text ? string_c_str(first_text) : "NULL", mixed_source,
           "Dxy(u) = 0", mixed_text ? string_c_str(mixed_text) : "NULL");
    WANT_POINTER("parsed first partial derivatives", first, true);
    WANT_LONG("first-partial independent-variable count", (long)de_independent_count(first), 2L);
    WANT_TEXT("normalized first partial derivatives", first_text ? string_c_str(first_text) : NULL,
                "Dx(u) + Dy(u) = 0");
    WANT_POINTER("parsed mixed partial derivative", mixed, true);
    WANT_LONG("mixed-partial independent-variable count", (long)de_independent_count(mixed), 2L);
    WANT_TEXT("normalized mixed partial derivative", mixed_text ? string_c_str(mixed_text) : NULL, "Dxy(u) = 0");
    WANT_TEXT("u_xy agrees with Dy(Dx(u))", mixed_text ? string_c_str(mixed_text) : NULL,
                nested_text ? string_c_str(nested_text) : NULL);
    WANT_TEXT("mixed-partial TeX notation", mixed_TeX, "\\frac{\\partial^{2} u}{\\partial y\\,\\partial x} = 0");
    WANT_TEXT("Greek-name subscript derivative", greek_TeX,
                "\\frac{\\partial \\phi}{\\partial x} + "
                "\\frac{\\partial \\phi}{\\partial y} = 0");
    WANT_TEXT("Unicode first partial derivatives", unicode_first_text ? string_c_str(unicode_first_text) : NULL,
                "Dx(u) + Dy(u) = 0");
    WANT_TEXT("Unicode mixed partial derivative", unicode_mixed_text ? string_c_str(unicode_mixed_text) : NULL,
                "Dxy(u) = 0");
    WANT_TEXT("Unicode repeated partial derivatives",
                unicode_repeated_text ? string_c_str(unicode_repeated_text) : NULL, "Dxx(u) + Dyy(u) = 0");
    WANT_TEXT("visually short PDE stays on one line", compact_TeX,
                "x\\mkern-2mu \\left(y - z\\right)\\mkern-2mu "
                "\\frac{\\partial z}{\\partial x} + "
                "y\\mkern-2mu \\left(z - x\\right)\\mkern-2mu "
                "\\frac{\\partial z}{\\partial y} = "
                "z\\mkern-2mu \\left(x - y\\right)");

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

static void test_diffequ_compact_parameter_derivatives(void)
{
    static const struct { const char *compact; const char *explicit_form; } cases[] = {
        {"u_y + au_xx + bu_yy = 0", "u_y + a*u_xx + b*u_yy = 0"},
        {"au_xx + bu_yy + u_y = 0", "a*u_xx + b*u_yy + u_y = 0"},
        {"u_y + 2au_xx + 3bu_yy = 0", "u_y + 2*a*u_xx + 3*b*u_yy = 0"},
        {"u_y + aau_xx = 0", "u_y + a*a*u_xx = 0"},
        {"phi_y + aphi_xx = 0", "phi_y + a*phi_xx = 0"},
        {"xzz_x + z_y = 0", "x*z*z_x + z_y = 0"},
        {"temperature_x + temperature_y = 0", "Dx([temperature]) + Dy([temperature]) = 0"},
        {"u_y + velocity_xx = 0", "u_y + Dxx([velocity]) = 0"},
        {"u_y + mu_xx = 0", "u_y + @mu_xx = 0"},
        {"au_xx + bu_yy = 0", "Dxx([au]) + Dyy([bu]) = 0"}
    };
    for (size_t i = 0u; i < sizeof(cases) / sizeof(*cases); ++i) {
        diffequ_t *compact = de_from_string(cases[i].compact);
        diffequ_t *explicit_form = de_from_string(cases[i].explicit_form);
        char *actual = compact ? de_to_string(compact, style_LATEX) : NULL;
        char *expected = explicit_form ? de_to_string(explicit_form, style_LATEX) : NULL;
        WANT_POINTER("explicit comparison equation", expected, true);
        WANT_TEXT(cases[i].compact, actual, expected);
        free(expected);
        free(actual);
        de_free(explicit_form);
        de_free(compact);
    }
}

static void test_diffequ_rejects_noncanonical_text(void)
{
    diffequ_t *missing_derivative = de_from_string("y = 1");
    diffequ_t *missing_section = de_from_string("{ Dx(y) = y | x = ?; }");

    WANT_POINTER("shorthand without a derivative", missing_derivative, false);
    WANT_POINTER("explicit form missing a section", missing_section, false);
    de_free(missing_section);
    de_free(missing_derivative);
}

static void test_diffequ_solves_separable_initial_value_problem(void)
{
    diffequ_t *de = de_from_string("Dx(y) = x*y; y(0) = 1");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    WANT_POINTER("parsed separable initial-value problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("separable solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("separable solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_SEPARABLE);
    WANT_LONG("separable solution count", (long)de_solve_result_count(result), 1L);
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("separable solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("separable solution text", text, true);
    if (text)
        WANT_TEXT("separable solution", string_c_str(text), "y = exp(½x²)");

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

    WANT_POINTER("parsed linear initial-value problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("linear solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("linear solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_LINEAR);
    WANT_LONG("linear solution count", (long)de_solve_result_count(result), 1L);
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("linear solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("linear solution text", text, true);
    if (text)
        WANT_TEXT("linear solution", string_c_str(text), "y = 1/exp(x)·((x - 1)·exp(x) + 1)");

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

    WANT_POINTER("parsed quadratic separable problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("quadratic separable solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("quadratic separable solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_SEPARABLE);
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("quadratic separable solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("quadratic separable solution text", text, true);
    if (text)
        WANT_TEXT("quadratic separable solution", string_c_str(text), "y = -1/(½·(x² - 2))");

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

    WANT_POINTER("parsed variable-coefficient linear problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("variable-coefficient linear solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("variable-coefficient linear solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_LINEAR);
    WANT_TEXT("linear solver diagnostic", de_solve_result_diagnostic(result), "solved as a first-order linear ODE");
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("variable-coefficient linear solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("variable-coefficient linear solution text", text, true);
    if (text)
        WANT_TEXT("variable-coefficient linear solution", string_c_str(text), "y = x/exp(x²)");

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

    WANT_POINTER("parsed rational integrating-factor problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("rational integrating-factor solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("rational integrating-factor solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_LINEAR);
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("rational integrating-factor solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("rational integrating-factor solution text", text, true);
    if (text)
        WANT_TEXT("rational integrating-factor solution", string_c_str(text), "y = ¼/x·(x⁴ + 3)");

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

    WANT_POINTER("parsed unconditioned linear ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("unconditioned linear solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("unconditioned linear solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_LINEAR);
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("unconditioned linear solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("unconditioned linear solution text", text, true);
    if (text)
        WANT_TEXT("unconditioned linear solution", string_c_str(text), "y = 1/exp(x)·(C + (x - 1)·exp(x))");

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

    WANT_POINTER("parsed non-elementary linear ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("non-elementary linear solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("non-elementary linear solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_LINEAR);
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("special-function linear solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("special-function solution text", text, true);
    if (text)
        WANT_TEXT("special-function linear solution", string_c_str(text),
                    "y = √(π)/(2·√(-1)·exp(x²))·(erf(x·√(-1)) - erf(0))");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

/* Also exercised last as the README integrating-factor example from docs/diffequation.md. */
static void test_diffequ_linear_solution_retains_formal_integral(void)
{
    diffequ_t *de = de_from_string("Dx(y) + y = exp(cosh(x)); y(0) = 0");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    WANT_POINTER("parsed formal-integral linear ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("formal-integral linear solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("formal-integral linear solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_LINEAR);
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("formal-integral linear solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("formal-integral solution text", text, true);
    if (text)
        WANT_TEXT("formal-integral linear solution", string_c_str(text), "y = ∫^x exp(cosh(t) + t)·dt/exp(x)");

    char *TeX = solution ? equ_to_TeX_body_wrapped(solution, SIZE_MAX) : NULL;
    ASSERT_TRUE(TeX && strstr(TeX, "e^{-x}\\,\\int^{x} e^{\\cosh(t) + t}"));
    ASSERT_TRUE(!strstr(TeX, "\\frac"));
    printf("  %s\n", TeX ? TeX : "NULL");
    free(TeX);

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

    WANT_POINTER("parsed formal-factor linear ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("formal-factor linear solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("formal-factor linear solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_LINEAR);
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("formal-factor linear solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("formal-factor solution text", text, true);
    if (text)
        WANT_TEXT("formal-factor linear solution", string_c_str(text), "y = 1/exp(∫^x exp(cosh(t))·dt)");

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

    WANT_POINTER("parsed homogeneous ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("homogeneous solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("homogeneous solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_HOMOGENEOUS);
    WANT_TEXT("homogeneous solver diagnostic", de_solve_result_diagnostic(result),
                "solved as a first-order homogeneous ODE");
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("homogeneous solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("homogeneous solution text", text, true);
    if (text)
        WANT_TEXT("homogeneous solution", string_c_str(text), "½·(y/x)² = ln(|x|) + ½");

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

    WANT_POINTER("parsed unconditioned homogeneous ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("unconditioned homogeneous solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("unconditioned homogeneous solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_HOMOGENEOUS);
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("unconditioned homogeneous solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("unconditioned homogeneous solution text", text, true);
    if (text)
        WANT_TEXT("unconditioned homogeneous solution", string_c_str(text), "½·(y/x)² = ln(|x|) + C");

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

    WANT_POINTER("parsed polynomial homogeneous ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("polynomial homogeneous solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("polynomial homogeneous solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("polynomial homogeneous selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_HOMOGENEOUS);
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("polynomial homogeneous solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("polynomial homogeneous solution text", text, true);
    if (text)
        WANT_TEXT("polynomial homogeneous initial-value solution", string_c_str(text),
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

    WANT_POINTER("parsed rational homogeneous ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("rational homogeneous solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("rational homogeneous solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("rational homogeneous selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_HOMOGENEOUS);
    WANT_LONG("rational homogeneous solution count", (long)de_solve_result_count(result), 2L);
    first_solution = de_solve_result_at(result, 0u);
    second_solution = de_solve_result_at(result, 1u);
    WANT_POINTER("first rational homogeneous solution", first_solution, true);
    WANT_POINTER("second rational homogeneous solution", second_solution, true);
    first_text = first_solution ? equ_to_text(first_solution, style_UNBOUND) : NULL;
    second_text = second_solution ? equ_to_text(second_solution, style_UNBOUND) : NULL;
    WANT_POINTER("first rational homogeneous solution text", first_text, true);
    WANT_POINTER("second rational homogeneous solution text", second_text, true);
    if (first_text)
        WANT_TEXT("first explicit rational homogeneous solution", string_c_str(first_text),
                    "y = -⅓·(√(x² - 3C/x³) - x)");
    if (second_text)
        WANT_TEXT("second explicit rational homogeneous solution", string_c_str(second_text),
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

    WANT_POINTER("parsed affine-substitution ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("affine-substitution solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("affine-substitution solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_LINEAR_SUBSTITUTION);
    WANT_TEXT("linear-substitution solver diagnostic", de_solve_result_diagnostic(result),
                "solved by a first-order linear substitution");
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("affine-substitution solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("affine-substitution solution text", text, true);
    if (text)
        WANT_TEXT("affine-substitution solution", string_c_str(text), "atan(x + y) = x + π/4");

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

    WANT_POINTER("parsed shifted-homogeneous ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("shifted-homogeneous solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("shifted-homogeneous solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_LINEAR_SUBSTITUTION);
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("shifted-homogeneous solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("shifted-homogeneous solution text", text, true);
    if (text)
        WANT_TEXT("shifted-homogeneous solution", string_c_str(text), "-(x + 2)/(y - 1) = ln(|x + 2|) - ln(2) - 1");

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

    WANT_POINTER("parsed linear-transformation ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("linear-transformation solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("linear-transformation solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_LINEAR_TRANSFORMATION);
    WANT_TEXT("linear-transformation solver diagnostic", de_solve_result_diagnostic(result),
                "solved by a linear change of variables");
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("linear-transformation solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("linear-transformation solution text", text, true);
    if (text)
        WANT_TEXT("linear-transformation solution", string_c_str(text), "½·(x + y)² = 1 - exp(y - x)");

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

    WANT_POINTER("parsed unconditioned ODE", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("unconditioned solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("unconditioned solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("unconditioned solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("unconditioned solution text", text, true);
    if (text)
        WANT_TEXT("arbitrary integration constant", string_c_str(text), "y = C·exp(½x²)");

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

    WANT_POINTER("parsed zero initial-value problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("zero initial-value solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("zero initial-value solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("zero singular solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("zero singular solution text", text, true);
    if (text)
        WANT_TEXT("zero singular solution", string_c_str(text), "y = 0");

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

    WANT_POINTER("parsed Bernoulli initial-value problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("Bernoulli solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("Bernoulli solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_BERNOULLI);
    WANT_LONG("Bernoulli solution count", (long)de_solve_result_count(result), 1L);
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("Bernoulli solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("Bernoulli solution text", text, true);
    if (text)
        WANT_TEXT("Bernoulli solution", string_c_str(text), "y = 1/(x + 1)");

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

    WANT_POINTER("parsed unconditioned Bernoulli problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("unconditioned Bernoulli result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("unconditioned Bernoulli status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("unconditioned Bernoulli selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_BERNOULLI);
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("unconditioned Bernoulli solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("unconditioned Bernoulli solution text", text, true);
    if (text)
        WANT_TEXT("normalized Bernoulli arbitrary constant", string_c_str(text), "y = 2·exp(2x)/(C - exp(2x))");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_derivative_quadratic_problem(void)
{
    static const char *want[] = {"x = ½·(√(8y + 1) - "
                                     "ln(|½·(√(8y + 1) + 1)|) + 1) + C",
                                     "x = ½·(1 - √(8y + 1) - "
                                     "ln(|½·(1 - √(8y + 1))|)) + C",
                                     "y = 0"};
    diffequ_t *de = de_from_string("(y')^2 = y' + 2y");
    diffequ_solve_result_t *result;

    WANT_POINTER("parsed derivative-quadratic problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("derivative-quadratic solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("derivative-quadratic solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("derivative-quadratic selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_DERIVATIVE_QUADRATIC);
    WANT_LONG("derivative-quadratic solution count", (long)de_solve_result_count(result), 3L);

    for (size_t i = 0u; i < 3u; ++i) {
        const equation_t *solution = de_solve_result_at(result, i);
        string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;

        WANT_POINTER("derivative-quadratic solution", solution, true);
        WANT_TEXT("derivative-quadratic solution text", text ? string_c_str(text) : NULL, want[i]);
        string_free(text);
    }

    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_linearizes_exact_third_order_problem(void)
{
    static const char *want[] = {"y = 2·ln(|Σ_(n=0)^∞ c_(n)·x^n|)",
                                     "c_(0) = C₂",
                                     "c_(1) = C₃",
                                     "c_(-1) = 0",
                                     "c_(-2) = 0",
                                     "c_(-3) = 0",
                                     "c_(n + 2) = 1/(2·(n + 2)·(n + 1))·(C₁·c_(n) + c_(n - 3))"};
    diffequ_t *de = de_from_string("y''' + y''*y' = 3x^2");
    diffequ_solve_result_t *result;

    WANT_POINTER("parsed exact third-order problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("exact third-order solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("exact third-order solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("exact third-order selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_EXACT_DERIVATIVE_LINEARIZATION);
    WANT_TEXT("exact third-order solver diagnostic", de_solve_result_diagnostic(result),
                "linearized exactly, then solved by a convergent "
                "power-series recurrence");
    WANT_LONG("exact third-order solution count", (long)de_solve_result_count(result), 7L);

    for (size_t i = 0u; i < 7u; ++i) {
        const equation_t *solution = de_solve_result_at(result, i);
        string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;

        WANT_POINTER("exact third-order solution", solution, true);
        WANT_TEXT("exact third-order solution text", text ? string_c_str(text) : NULL, want[i]);
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

    WANT_POINTER("parsed second-order problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("Sturm-Liouville solve result", result, true);
    if (result) {
        WANT_LONG("Sturm-Liouville solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
        WANT_LONG("selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_STURM_LIOUVILLE);
        WANT_LONG("Sturm-Liouville solution count", (long)de_solve_result_count(result), 1L);
        WANT_TEXT("Sturm-Liouville diagnostic", de_solve_result_diagnostic(result),
                    "solved as a second-order linear Sturm-Liouville equation");
    }
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("Sturm-Liouville solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("Sturm-Liouville solution text", text, true);
    if (text)
        WANT_TEXT("Sturm-Liouville solution", string_c_str(text), "y = exp(x)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_linearizes_modified_emden_problem(void)
{
    const char *source = "y'' + 3yy' + y^3 = 0";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
    const char *want = "y = (2x + C₁)/(x² + C₁x + C₂)";

    WANT_POINTER("parsed modified-Emden problem", de, true);
    WANT_POINTER("modified-Emden solve result", result, true);
    WANT_LONG("modified-Emden solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("modified-Emden selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_LINEAR_TRANSFORMATION);
    WANT_TEXT("modified-Emden diagnostic", de_solve_result_diagnostic(result),
                "linearized by y = u'/u, then solved as u''' = 0");
    WANT_POINTER("modified-Emden derivation", de_solve_result_steps(result), true);
    WANT_TEXT("modified-Emden symmetry", de_solve_result_symmetry(result), "SL(3, ℝ)");
    if (de_solve_result_steps(result))
        WANT_POINTER("modified-Emden derivation contains transformed ODE",
                       strstr(de_solve_result_steps(result), "d²Y/dX² = 0"), true);
    if (de_solve_result_steps_TeX(result)) {
        WANT_POINTER("modified-Emden TeX uses the dependent symbol directly",
                       strstr(de_solve_result_steps_TeX(result), "y=\\frac{1}{u}u'"), true);
        WANT_POINTER("modified-Emden TeX omits binding wrappers",
                       strstr(de_solve_result_steps_TeX(result), "\\middle|"), false);
        WANT_POINTER("modified-Emden TeX omits unbound sentinel values",
                       strstr(de_solve_result_steps_TeX(result), "NAN"), false);
        WANT_POINTER("modified-Emden TeX keeps derivatives outside algebraic fractions",
                       strstr(de_solve_result_steps_TeX(result), "\\frac{u'"), false);
        WANT_POINTER("modified-Emden TeX explains the auxiliary polynomial",
                       strstr(de_solve_result_steps_TeX(result), "u=Ax^2+Bx+C"), true);
        WANT_POINTER("modified-Emden TeX omits redundant unit coefficients",
                       strstr(de_solve_result_steps_TeX(result), "3(1)"), false);
    }
    WANT_LONG("modified-Emden solution count", (long)de_solve_result_count(result), 1L);
    {
        const equation_t *solution = de_solve_result_at(result, 0u);
        string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;

        WANT_POINTER("modified-Emden solution", solution, true);
        WANT_POINTER("modified-Emden solution text", text, true);
        if (text)
            WANT_TEXT("modified-Emden solution text", string_c_str(text), want);
        string_free(text);
    }

    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_rejects_quartic_emden_point_linearization(void)
{
    diffequ_t *de = de_from_string("y'' + 3*y*y' + y^4 = 0");
    diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;

    WANT_POINTER("parsed quartic Emden problem", de, true);
    WANT_POINTER("quartic Emden solve result", result, true);
    WANT_LONG("quartic Emden solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SERIES);
    WANT_POINTER("computed invariant diagnostic", strstr(de_solve_result_steps(result), "Computed Lie–Tressé"), true);
    WANT_POINTER("translation symmetry is found despite non-linearisation",
                  strstr(de_solve_result_steps(result), "total degree <= 2): 1 verified"), true);
    WANT_POINTER("local order reduction is reported", strstr(de_solve_result_steps(result), "local order reduction"), true);
    WANT_POINTER("native TeX analysis", de_solve_result_steps_TeX(result), true);
    const char *explanations[] = {de_solve_result_steps(result), de_solve_result_steps_TeX(result)};
    for (size_t i = 0u; i < 2u; ++i) {
        WANT_POINTER("slope substitution is defined", strstr(explanations[i], "p=y'") ?
                     strstr(explanations[i], "p=y'") : strstr(explanations[i], "p = y'"), true);
        WANT_POINTER("total derivative is explained", strstr(explanations[i], "total differentiation along"), true);
        WANT_POINTER("chain rule is named", strstr(explanations[i], "chain rule"), true);
        WANT_POINTER("partial derivatives are explained", strstr(explanations[i], "other coordinates fixed"), true);
        WANT_POINTER("Taylor factorials are explained", strstr(explanations[i], "divided by factorials"), true);
        WANT_POINTER("actual coefficient algorithm is identified", strstr(explanations[i], "matching powers"), true);
        WANT_POINTER("remainder is explained", strstr(explanations[i], "omitted higher powers"), true);
    }

    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_series_quartic_coefficients(void)
{
    static const char *const expected[] = {"0", "1", "0", "-1/2", "0", "3/10", "-1/30", "-51/280", "27/560"};
    diffequ_t *de = de_from_string("y'' + 3*y*y' + y^4 = 0; y(0)=0; y'(0)=1");
    diffequ_solve_result_t *result = de_solve_series(de, 8u, DE_SOLVE_OPTION_NONE);
    WANT_LONG("series status", de_solve_result_status(result), DE_SOLVE_STATUS_SERIES);
    WANT_LONG("retained degree", de_solve_result_series_degree(result), 8L);
    WANT_LONG("zero centre", expr_is_exact_zero(de_solve_result_series_centre(result)), 1L);
    WANT_POINTER("steps remain opt-in", de_solve_result_steps(result), false);
    for (size_t n = 0u; n < 9u; ++n) {
        expr_t *want = expr_from_string(expected[n], NULL);
        expr_t *difference = expr_sub_simplify_owned(expr_clone(de_solve_result_series_coefficient(result, n)), want);
        WANT_LONG("exact Taylor coefficient", expr_is_exact_zero(difference), 1L);
        expr_free(difference);
    }
    WANT_POINTER("coefficient out of range", de_solve_result_series_coefficient(result, 9u), false);
    string_t *text = equ_to_text(de_solve_result_at(result, 0u), style_LATEX);
    WANT_POINTER("explicit remainder in native TeX", strstr(string_c_str(text), "O"), true);
    printf("  local series: %s\n", string_c_str(text));
    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void assert_series_numbers_exact(const expr_t *expr)
{
    if (!expr)
        return;
    number_t value = num_new();
    if (!expr_is_named_const(expr) && expr_match_const_value(expr, &value))
        WANT_LONG("Taylor coefficient has an exact numeric leaf", num_is_exact(value), 1L);
    num_destroy(&value);
    const expr_t *left = NULL, *right = NULL;
    if (expr_child_exprs(expr, &left, &right)) {
        assert_series_numbers_exact(left);
        assert_series_numbers_exact(right);
    }
}

static void test_diffequ_series_general_data(void)
{
    diffequ_t *de = de_from_string("y'' + 3*y*y' + y^4 = 0");
    diffequ_solve_result_t *result = de_solve(de);
    WANT_LONG("automatic series fallback", de_solve_result_status(result), DE_SOLVE_STATUS_SERIES);
    const expr_t *a = de_solve_result_series_coefficient(result, 0u);
    const expr_t *b = de_solve_result_series_coefficient(result, 1u);
    WANT_LONG("arbitrary initial value", expr_is_named_const(a), 1L);
    WANT_LONG("arbitrary initial slope", expr_is_named_const(b), 1L);
    WANT_LONG("independent initial constants", expr_struct_eq(a, b), 0L);
    /* Presentation regression: collect the recurrence, rather than printing its nested construction tree. */
    for (size_t n = 2u; n <= de_solve_result_series_degree(result); ++n) {
        assert_series_numbers_exact(de_solve_result_series_coefficient(result, n));
        char *coefficient = expr_to_string(de_solve_result_series_coefficient(result, n), style_UNBOUND);
        WANT_LONG("compact symbolic Taylor coefficient", coefficient && strlen(coefficient) < 400u, 1L);
        WANT_LONG("no decimal Taylor coefficients", coefficient && !strchr(coefficient, '.'), 1L);
        free(coefficient);
        char *coefficient_TeX = expr_to_string(de_solve_result_series_coefficient(result, n), style_LATEX);
        WANT_LONG("TeX retains fractional Taylor coefficients",
                  coefficient_TeX && !strchr(coefficient_TeX, '.') && strstr(coefficient_TeX, "\\frac"), 1L);
        free(coefficient_TeX);
    }
    string_t *series_text = equ_to_text(de_solve_result_at(result, 0u), style_UNBOUND);
    WANT_LONG("readable Taylor series", series_text && strlen(string_c_str(series_text)) < 1500u, 1L);
    string_free(series_text);
    expr_t *square = expr_mul(a, a);
    expr_t *fourth = expr_mul(square, square);
    expr_t *product = expr_mul(a, b);
    expr_t *sum = expr_add_simplify_owned(expr_mul_long(product, 3L), fourth);
    expr_t *expected = expr_div_long(sum, -2L);
    expr_t *difference = expr_sub_simplify_owned(expr_clone(de_solve_result_series_coefficient(result, 2u)), expected);
    expr_t *a_symbol = expr_new_named_var(NUM_NAN, expr_symbol_name(a));
    expr_t *b_symbol = expr_new_named_var(NUM_NAN, expr_symbol_name(b));
    /* Both sides have degree <= 4 in a and <= 1 in b: this exact grid determines their difference. */
    for (long av = -2L; av <= 2L; ++av) {
        for (long bv = -1L; bv <= 1L; ++bv) {
            expr_t *a_value = expr_const_long(av), *b_value = expr_const_long(bv);
            expr_t *at_a = expr_substitute(difference, a_symbol, a_value);
            expr_t *at_b = expr_substitute(at_a, b_symbol, b_value);
            expr_t *simplified = expr_simplify(at_b);
            WANT_LONG("coefficient for arbitrary initial data", expr_is_exact_zero(simplified), 1L);
            expr_free(simplified);
            expr_free(at_b);
            expr_free(at_a);
            expr_free(b_value);
            expr_free(a_value);
        }
    }
    /* Collecting symbolic coefficients must agree with all orders computed directly from numeric data. */
    static const long initial_data[][2] = {{-1L, 2L}, {1L, 0L}, {2L, -1L}, {0L, 1L}};
    for (size_t i = 0u; i < sizeof(initial_data) / sizeof(*initial_data); ++i) {
        char source[128];
        snprintf(source, sizeof(source), "y'' + 3*y*y' + y^4 = 0; y(0)=%ld; y'(0)=%ld",
                 initial_data[i][0], initial_data[i][1]);
        diffequ_t *numeric_de = de_from_string(source);
        diffequ_solve_result_t *numeric = de_solve_series(numeric_de, 6u, DE_SOLVE_OPTION_NONE);
        WANT_LONG("numeric comparison series", de_solve_result_status(numeric), DE_SOLVE_STATUS_SERIES);
        expr_t *a_value = expr_const_long(initial_data[i][0]);
        expr_t *b_value = expr_const_long(initial_data[i][1]);
        for (size_t n = 0u; n <= 6u; ++n) {
            expr_t *at_a = expr_substitute(de_solve_result_series_coefficient(result, n), a_symbol, a_value);
            expr_t *at_b = expr_substitute(at_a, b_symbol, b_value);
            expr_t *error = expr_sub_simplify_owned(at_b, expr_clone(de_solve_result_series_coefficient(numeric, n)));
            WANT_LONG("collected coefficient retains exact value", expr_is_exact_zero(error), 1L);
            expr_free(error);
            expr_free(at_a);
        }
        expr_free(b_value);
        expr_free(a_value);
        de_solve_result_free(numeric);
        de_free(numeric_de);
    }
    expr_free(b_symbol);
    expr_free(a_symbol);
    expr_free(difference);
    expr_free(sum);
    expr_free(product);
    expr_free(square);
    de_solve_result_free(result);
    de_free(de);
}

/* Verify every determined residual coefficient, independently of the total-derivative recurrence. */
static void test_diffequ_series_shifted_residuals(void)
{
    static const char *const sources[] = {
        "2*Dtt(u)=2*t+2*u^2+2*Dt(u)^2; u(2)=1; u'(2)=-1",
        "y'' + 5*y*y' + 2*y^5 = 0; y(1)=2; y'(1)=0",
        "y'' + 3*y*y' + y^4 = 0; y(0)=0; y'(0)=0",
    };
    for (size_t c = 0u; c < sizeof(sources) / sizeof(*sources); ++c) {
        diffequ_t *de = de_from_string(sources[c]);
        de_lie_t *normal = de_lie_new(de);
        diffequ_solve_result_t *result = de_solve_series(de, 6u, DE_SOLVE_OPTION_NONE);
        WANT_LONG("polynomial normal form series", de_solve_result_status(result), DE_SOLVE_STATUS_SERIES);
        const expr_t *x = de_lie_coordinate(normal, 0u);
        const expr_t *centre = de_solve_result_series_centre(result);
        expr_t *shift = expr_sub(x, centre);
        expr_t *power = expr_const_one();
        expr_t *polynomial = expr_const_zero();
        for (size_t n = 0u; n <= 6u; ++n) {
            polynomial = expr_add_simplify_owned(polynomial, expr_mul(de_solve_result_series_coefficient(result, n), power));
            power = expr_mul_simplify_owned(power, expr_clone(shift));
        }
        expr_t *first = expr_create_deriv(polynomial, x);
        expr_t *second = expr_create_deriv(first, x);
        expr_t *rhs_y = expr_substitute(de_lie_rhs(normal), de_lie_coordinate(normal, 1u), polynomial);
        expr_t *rhs = expr_substitute(rhs_y, de_lie_coordinate(normal, 2u), first);
        expr_t *residual = expr_sub(second, rhs);
        /* Evaluate at the centre before expansion to keep the check small. */
        for (size_t n = 0u; n <= 4u; ++n) {
            expr_t *at = expr_substitute(residual, x, centre);
            expr_t *simplified = expr_simplify(at);
            WANT_LONG("residual derivative at expansion point", expr_is_exact_zero(simplified), 1L);
            expr_free(simplified);
            expr_free(at);
            expr_t *next = n < 4u ? expr_create_deriv(residual, x) : NULL;
            expr_free(residual);
            residual = next;
        }
        expr_free(rhs);
        expr_free(rhs_y);
        expr_free(second);
        expr_free(first);
        expr_free(polynomial);
        expr_free(power);
        expr_free(shift);
        de_solve_result_free(result);
        de_lie_free(normal);
        de_free(de);
    }
}

static void test_diffequ_series_limits_and_initial_conditions(void)
{
    static const char *const rejected[] = {
        "y''=abs(y)", "x*y''=y^4", "y''=1/y", "y''^2=y", "y'=y^4", "u_xx=u^4",
        "y''=y^4; y(0)=1; y'(1)=2", "y''=y^4; y(0)=1; y(0)=2",
        "y''=y^4; y''(0)=1", "y''=y^4; y(0)=x", "y''=y^4; y(0)=inf",
    };
    for (size_t i = 0u; i < sizeof(rejected) / sizeof(*rejected); ++i) {
        diffequ_t *de = de_from_string(rejected[i]);
        WANT_POINTER("well-formed but unsuitable for Taylor fallback", de, true);
        diffequ_solve_result_t *result = de_solve_series(de, 6u, DE_SOLVE_OPTION_NONE);
        WANT_LONG("unsupported series case", de_solve_result_status(result), DE_SOLVE_STATUS_UNSUPPORTED);
        WANT_LONG("no misleading finite solution", de_solve_result_count(result), 0L);
        de_solve_result_free(result);
        de_free(de);
    }
    diffequ_t *de = de_from_string("y''=-3*y*y'-y^4; y(2)=0");
    diffequ_solve_result_t *result = de_solve_series(de, 8u, DE_SOLVE_OPTION_NONE);
    WANT_LONG("partial initial data", de_solve_result_status(result), DE_SOLVE_STATUS_SERIES);
    WANT_LONG("supplied value", expr_is_exact_zero(de_solve_result_series_coefficient(result, 0u)), 1L);
    WANT_LONG("unspecified slope remains arbitrary", expr_is_named_const(de_solve_result_series_coefficient(result, 1u)), 1L);
    de_solve_result_free(result);
    result = de_solve_series(de, 9u, DE_SOLVE_OPTION_NONE);
    WANT_LONG("degree limit", de_solve_result_status(result), DE_SOLVE_STATUS_INVALID);
    de_solve_result_free(result);
    de_free(de);
    WANT_LONG("null series degree", de_solve_result_series_degree(NULL), 0L);
    WANT_POINTER("null series centre", de_solve_result_series_centre(NULL), false);
    WANT_POINTER("null series coefficient", de_solve_result_series_coefficient(NULL, 0u), false);
}

static void test_diffequ_linearizes_scaled_modified_emden_problem(void)
{
    diffequ_t *de = de_from_string("y'' + 6*y*y' + 4*y^3 = 0");
    diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;

    WANT_POINTER("scaled modified-Emden result", result, true);
    WANT_LONG("scaled modified-Emden status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_TEXT("scaled modified-Emden symmetry", de_solve_result_symmetry(result), "SL(3, ℝ)");
    WANT_POINTER("scaled modified-Emden X substitution", strstr(de_solve_result_steps(result), "X = x − 1/(2y)"),
                   true);
    if (text)
        WANT_TEXT("scaled modified-Emden solution", string_c_str(text),
                    "y = (2x + C₁)/(2·(x² + C₁x + C₂))");

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

    WANT_POINTER("hydrogen ground-state result", result, true);
    WANT_LONG("hydrogen ground-state status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("stationary eigenfunction solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_STATIONARY_EIGENFUNCTION);
    WANT_POINTER("hydrogen eigenfunction steps", strstr(de_solve_result_steps(result), "derived constant rate"),
                   true);
    if (text)
        WANT_TEXT("hydrogen ground-state wavefunction", string_c_str(text), "ψ = exp(0.5it - √(x² + y² + z²))/√(π)");

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

    WANT_LONG("coefficient-derived Emden status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_POINTER("coefficient-derived Emden scale", strstr(de_solve_result_steps(result), "Set y = (1/(3u))u′"), true);
    WANT_TEXT("coefficient-derived Emden solution", text ? string_c_str(text) : NULL,
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

    WANT_POINTER("stationary rule result", result, true);
    WANT_LONG("stationary rule status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("stationary rule solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_STATIONARY_EIGENFUNCTION);
    WANT_TEXT("stationary rule solution", text ? string_c_str(text) : NULL, "u = exp(2·(2t + x))");
    WANT_POINTER("stationary rule derived rate", strstr(de_solve_result_steps(result), "λ ="), true);

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
    WANT_POINTER("parsed affine-factorized problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("affine-factorized solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("affine-factorized solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("affine-factorized selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_STURM_LIOUVILLE);
    WANT_LONG("affine-factorized solution count", (long)de_solve_result_count(result), 1L);
    WANT_TEXT("affine-factorized diagnostic", de_solve_result_diagnostic(result),
                "solved as a second-order linear Sturm-Liouville equation");

    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("affine-factorized solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("affine-factorized solution text", text, true);
    if (text) {
        printf("    want: y = exp(½x²)·(C₁ + C₂·erf(x))\n"
               "    got:   %s\n",
               string_c_str(text));
        WANT_TEXT("affine-factorized solution text", string_c_str(text), "y = exp(½x²)·(C₁ + C₂·erf(x))");
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
    WANT_POINTER("parsed affine-factorized IVP", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("affine-factorized IVP solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("affine-factorized IVP solve status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("affine-factorized IVP solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("affine-factorized IVP solution text", text, true);
    if (text) {
        printf("    want: y = exp(½x²)\n"
               "    got:   %s\n",
               string_c_str(text));
        WANT_TEXT("affine-factorized IVP solution text", string_c_str(text), "y = exp(½x²)");
    }

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_does_not_invent_cubic_potential_functions(void)
{
    diffequ_t *de = de_from_string("y'' = 1/2*(x^3+a)*y; y(0) = 1; y'(0) = 0");
    diffequ_solve_result_t *result;

    WANT_POINTER("parsed cubic-potential IVP", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("cubic-potential solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }

    WANT_LONG("cubic-potential solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SERIES);
    WANT_LONG("cubic-potential selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_TAYLOR_SERIES);
    WANT_LONG("cubic-potential series count", (long)de_solve_result_count(result), 1L);

    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_repeated_characteristic_root(void)
{
    diffequ_t *de = de_from_string("Dxx(y) + 2*Dx(y) + y = 0; y(0) = 1; y'(0) = 0");
    diffequ_solve_result_t *result;
    const equation_t *solution;
    string_t *text;

    WANT_POINTER("parsed repeated-root problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("repeated-root solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }
    WANT_LONG("repeated-root solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("repeated-root selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_STURM_LIOUVILLE);
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("repeated-root solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("repeated-root solution text", text, true);
    if (text)
        WANT_TEXT("repeated-root solution", string_c_str(text), "y = (x + 1)·exp(-x)");

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

    WANT_POINTER("parsed oscillatory problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("oscillatory solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }
    WANT_LONG("oscillatory solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("oscillatory selected solver", (long)de_solve_result_solver(result), (long)DE_SOLVER_STURM_LIOUVILLE);
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("oscillatory solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("oscillatory solution text", text, true);
    if (text)
        WANT_TEXT("oscillatory solution", string_c_str(text), "y = sin(x)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_normalizes_variable_coefficient_sturm_liouville(void)
{
    diffequ_t *de = de_from_string("x*Dxx(y) + Dx(y) + y = 0");
    diffequ_solve_result_t *result;

    WANT_POINTER("parsed variable-coefficient problem", de, true);
    if (!de)
        return;

    result = de_solve(de);
    WANT_POINTER("variable-coefficient solve result", result, true);
    if (result) {
        WANT_LONG("variable-coefficient solve status", (long)de_solve_result_status(result),
                    (long)DE_SOLVE_STATUS_UNSUPPORTED);
        WANT_LONG("variable-coefficient solution count", (long)de_solve_result_count(result), 0L);
        WANT_POINTER("variable-coefficient Lie diagnostic",
                      strstr(de_solve_result_diagnostic(result), "computed Lie–Tressé invariants"), true);
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

        WANT_POINTER("parsed power-law Bessel problem", de, true);
        if (!de)
            continue;

        result = de_solve_with_options(de, DE_SOLVE_OPTION_STEPS);
        WANT_POINTER("power-law Bessel solve result", result, true);
        if (!result) {
            de_free(de);
            continue;
        }

        WANT_LONG("power-law Bessel solve status", (long)de_solve_result_status(result),
                    (long)DE_SOLVE_STATUS_SOLVED);
        WANT_LONG("power-law Bessel selected solver", (long)de_solve_result_solver(result),
                    (long)DE_SOLVER_POWER_LAW_BESSEL);
        WANT_TEXT("power-law Bessel diagnostic", de_solve_result_diagnostic(result),
                    "solved by a power-law reduction to Bessel's equation");
        solution = de_solve_result_at(result, 0u);
        WANT_POINTER("power-law Bessel solution", solution, true);
        text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
        WANT_POINTER("power-law Bessel solution text", text, true);
        if (text) {
            WANT_POINTER("power-law Bessel solution has negative-order basis",
                           strstr(string_c_str(text), "BesselJ(-"), true);
            WANT_POINTER("power-law Bessel solution has positive-order basis", strstr(string_c_str(text), orders[i]),
                           true);
            WANT_POINTER("power-law Bessel solution has derived argument", strstr(string_c_str(text), arguments[i]),
                           true);
        }
        WANT_POINTER("power-law Bessel derivation names its substitution",
                       strstr(de_solve_result_steps(result), "y = sqrt(x)*u(z)"), true);
        WANT_POINTER("power-law Bessel TeX uses conventional Bessel notation",
                       strstr(de_solve_result_steps_TeX(result), "J_{-\\frac"), true);
        WANT_POINTER("power-law Bessel TeX omits binding wrappers",
                       strstr(de_solve_result_steps_TeX(result), "\\middle|"), false);
        WANT_POINTER("power-law Bessel TeX omits unbound sentinel values",
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

        WANT_POINTER("parsed forced power-law problem", de, true);
        if (!de)
            continue;

        result = de_solve_with_options(de, DE_SOLVE_OPTION_STEPS);
        WANT_POINTER("forced power-law solve result", result, true);
        if (!result) {
            de_free(de);
            continue;
        }

        WANT_LONG("forced power-law solve status", (long)de_solve_result_status(result),
                    (long)DE_SOLVE_STATUS_SOLVED);
        WANT_LONG("forced power-law selected solver", (long)de_solve_result_solver(result),
                    (long)DE_SOLVER_POWER_LAW_BESSEL);
        solution = de_solve_result_at(result, 0u);
        WANT_POINTER("forced power-law solution", solution, true);
        text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
        WANT_POINTER("forced power-law solution text", text, true);
        if (text) {
            WANT_POINTER("forced power-law solution has homogeneous order", strstr(string_c_str(text), orders[i]),
                           true);
            WANT_POINTER("forced power-law solution has derived argument", strstr(string_c_str(text), arguments[i]),
                           true);
            WANT_POINTER("forced power-law solution has Lommel particular", strstr(string_c_str(text), scales[i]),
                           true);
        }
        WANT_POINTER("forced power-law derivation names monomial forcing",
                       strstr(de_solve_result_steps(result), "monomial forcing"), true);
        WANT_POINTER("forced power-law TeX uses Lommel notation", strstr(de_solve_result_steps_TeX(result), "s_{0,"),
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
    WANT_POINTER("parsed third-order problem", de, true);
    if (!de)
        return;
    result = de_solve(de);
    WANT_POINTER("third-order solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }
    WANT_LONG("third-order solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("third-order selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR);
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("third-order solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("third-order solution text", text, true);
    if (text)
        WANT_TEXT("third-order solution", string_c_str(text), "y = exp(x)");

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
    WANT_POINTER("parsed high-order repeated-root problem", de, true);
    if (!de)
        return;
    result = de_solve(de);
    WANT_POINTER("high-order repeated-root result", result, true);
    if (!result) {
        de_free(de);
        return;
    }
    WANT_LONG("high-order repeated-root status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("high-order repeated-root solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("high-order repeated-root solution text", text, true);
    if (text)
        WANT_TEXT("high-order repeated-root solution", string_c_str(text), "y = exp(x)");

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
    WANT_POINTER("parsed nonhomogeneous second-order problem", de, true);
    if (!de)
        return;
    result = de_solve(de);
    WANT_POINTER("nonhomogeneous solve result", result, true);
    if (!result) {
        de_free(de);
        return;
    }
    WANT_LONG("nonhomogeneous solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("nonhomogeneous selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR);
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("nonhomogeneous solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("nonhomogeneous solution text", text, true);
    if (text)
        WANT_TEXT("nonhomogeneous solution", string_c_str(text), "y = ⅙·(2·exp(2x) - 3·exp(x) + exp(-x))");

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
    WANT_POINTER("parsed secant-cubed forcing problem", de, true);
    if (!de)
        return;
    result = de_solve(de);
    WANT_POINTER("secant-cubed forcing result", result, true);
    if (!result) {
        de_free(de);
        return;
    }
    WANT_LONG("secant-cubed forcing status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("secant-cubed forcing selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR);
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("secant-cubed forcing solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("secant-cubed forcing solution text", text, true);
    if (text)
        WANT_TEXT("secant-cubed forcing solution", string_c_str(text), "y = ½·sec(x) + C₁·cos(x) + C₂·sin(x)");

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
    WANT_POINTER("parsed repeated-complex-root problem", de, true);
    if (!de)
        return;
    result = de_solve(de);
    WANT_POINTER("repeated-complex-root result", result, true);
    if (!result) {
        de_free(de);
        return;
    }
    WANT_LONG("repeated-complex-root status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    solution = de_solve_result_at(result, 0u);
    WANT_POINTER("repeated-complex-root solution", solution, true);
    text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    WANT_POINTER("repeated-complex-root solution text", text, true);
    if (text)
        WANT_TEXT("repeated-complex-root solution", string_c_str(text), "y = cos(x)");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_degree_six_characteristic_polynomial(void)
{
    diffequ_t *de = de_from_string("Dxxxxxx(y) - 4*Dxxxx(y) - Dxx(y) + 4*y = 0");
    diffequ_solve_result_t *result;

    WANT_POINTER("parsed sixth-order problem", de, true);
    if (!de)
        return;
    result = de_solve(de);
    WANT_POINTER("sixth-order solve result", result, true);
    if (result) {
        WANT_LONG("sixth-order solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
        WANT_LONG("sixth-order selected solver", (long)de_solve_result_solver(result),
                    (long)DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR);
        WANT_LONG("sixth-order solution count", (long)de_solve_result_count(result), 1L);
    }

    de_solve_result_free(result);
    de_free(de);
}

static bool test_diffequ_want_constant_linear_solution(const char *source, const char *want, const char *file,
                                                         int line)
{
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    const char *got = text ? string_c_str(text) : NULL;
    bool valid = de && result && de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
                 de_solve_result_solver(result) == DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR &&
                 de_solve_result_count(result) == 1u && got && strcmp(got, want) == 0;

    printf("  differential equation\n"
           "    input:    %s\n"
           "    want: %s\n"
           "    got:   %s\n",
           source, want, got ? got : "NULL");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
    return test_assert_true(valid, file, line, "constant-coefficient linear solution");
}

#define WANT_CONSTANT_LINEAR_SOLUTION(source, want)                                                              \
    TEST_HARNESS_RETURN_UNLESS(test_diffequ_want_constant_linear_solution((source), (want), __FILE__, __LINE__))

static void test_diffequ_solves_logarithmic_forcing(void)
{
    WANT_CONSTANT_LINEAR_SOLUTION("y'' + y = ln(x)", "y = ln(x) - cos(x)·Ci(x) - sin(x)·Si(x) + "
                                                       "C₁·cos(x) + C₂·sin(x)");
}

static void test_diffequ_resolves_polynomial_differential_operator(void)
{
    const char *source = "(Dx^2 + 4Dx + 20)^2(y) = 0";
    const char *want_problem = "{ d⁴y/dx⁴ + 8*d³y/dx³ + 56*d²y/dx² + "
                                   "160*dy/dx + 400*y = 0 | x = ?; ;  }";
    const char *want_solution = "y = exp(-2x)·(C₁·cos(4x) + C₂·sin(4x) + "
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
    WANT_TEXT("resolved differential equation", problem, want_problem);
    WANT_POINTER("operator solve result", result, true);
    if (result) {
        WANT_LONG("operator solve status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
        WANT_LONG("operator selected solver", (long)de_solve_result_solver(result),
                    (long)DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR);
    }
    WANT_TEXT("operator solution", solution_text ? string_c_str(solution_text) : NULL, want_solution);

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
            "x = Σ_(k=0)^3 C_(k + 1)·t^k·cos(ωt) + "
            "Σ_(k=0)^3 C_(k + 5)·t^k·sin(ωt)",
        },
        {
            "(D^2 + @omega^2)^4phi = 0",
            "{ d⁸φ/dx⁸ + 4ω²*d⁶φ/dx⁶ + 6ω⁴*d⁴φ/dx⁴ + "
            "4ω⁶*d²φ/dx² + ω⁸*φ = 0 | x = ?; ;  }",
            "φ = Σ_(k=0)^3 C_(k + 1)·x^k·cos(ωx) + "
            "Σ_(k=0)^3 C_(k + 5)·x^k·sin(ωx)",
        },
        {
            "(D^2 + @omega^2)^4@phi = 0",
            "{ d⁸φ/dx⁸ + 4ω²*d⁶φ/dx⁶ + 6ω⁴*d⁴φ/dx⁴ + "
            "4ω⁶*d²φ/dx² + ω⁸*φ = 0 | x = ?; ;  }",
            "φ = Σ_(k=0)^3 C_(k + 1)·x^k·cos(ωx) + "
            "Σ_(k=0)^3 C_(k + 5)·x^k·sin(ωx)",
        },
        {
            "(D^2 + @omega^2)^4φ = 0",
            "{ d⁸φ/dx⁸ + 4ω²*d⁶φ/dx⁶ + 6ω⁴*d⁴φ/dx⁴ + "
            "4ω⁶*d²φ/dx² + ω⁸*φ = 0 | x = ?; ;  }",
            "φ = Σ_(k=0)^3 C_(k + 1)·x^k·cos(ωx) + "
            "Σ_(k=0)^3 C_(k + 5)·x^k·sin(ωx)",
        },
    };
    bool valid = true;

    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        diffequ_t *de = de_from_string(cases[i].source);
        char *problem = de ? de_to_string(de, style_EXPRESSION) : NULL;
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
        string_t *solution_text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
        const char *got_solution = solution_text ? string_c_str(solution_text) : NULL;
        bool case_valid = de && problem && strcmp(problem, cases[i].problem) == 0 && result &&
                          de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED && got_solution &&
                          strcmp(got_solution, cases[i].solution) == 0;

        printf("  bare differential operator\n"
               "    input:    %s\n"
               "    resolves: %s\n"
               "    solution: %s\n",
               cases[i].source, problem ? problem : "NULL", got_solution ? got_solution : "NULL");
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
                 strcmp(string_c_str(text), "x = Σ_(k=0)^63 C_(k + 1)·t^k·cos(ωt) + "
                                            "Σ_(k=0)^63 C_(k + 65)·t^k·sin(ωt)") == 0;

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
    ASSERT_TRUE(valid);
}

static void test_diffequ_general_solution_with_distinct_real_roots(void)
{
    WANT_CONSTANT_LINEAR_SOLUTION("Dxxx(y) - 6*Dxx(y) + 11*Dx(y) - 6*y = 0",
                                    "y = C₁·exp(3x) + C₂·exp(2x) + C₃·exp(x)");
}

static void test_diffequ_general_solution_with_repeated_real_root(void)
{
    WANT_CONSTANT_LINEAR_SOLUTION("Dxxx(y) - 3*Dxx(y) + 3*Dx(y) - y = 0", "y = (C₁ + C₂x + C₃x²)·exp(x)");
}

static void test_diffequ_general_solution_with_real_and_complex_roots(void)
{
    WANT_CONSTANT_LINEAR_SOLUTION("Dxxx(y) - Dxx(y) + Dx(y) - y = 0", "y = C₁·exp(x) + C₂·cos(x) + C₃·sin(x)");
}

static void test_diffequ_general_solution_with_repeated_complex_roots(void)
{
    WANT_CONSTANT_LINEAR_SOLUTION("Dxxxx(y) + 2*Dxx(y) + y = 0", "y = (C₁ + C₂x)·cos(x) + (C₃ + C₄x)·sin(x)");
}

static void test_diffequ_general_sixth_order_solution(void)
{
    WANT_CONSTANT_LINEAR_SOLUTION("Dxxxxxx(y) - 4*Dxxxx(y) - Dxx(y) + 4*y = 0",
                                    "y = C₁·exp(x) + C₂·exp(2x) + C₃·exp(-x) + C₄·exp(-2x) + "
                                    "C₅·cos(x) + C₆·sin(x)");
}

static void test_diffequ_general_nonhomogeneous_solution(void)
{
    WANT_CONSTANT_LINEAR_SOLUTION("Dxx(y) - y = exp(2*x)", "y = ⅓·exp(2x) + C₁·exp(x) + C₂·exp(-x)");
    WANT_CONSTANT_LINEAR_SOLUTION("y'' + 4y = e^x + x^3", "y = ⅕·exp(x) + ¼x³ - ⅜x + "
                                                            "C₁·cos(2x) + C₂·sin(2x)");
}

static void test_diffequ_general_trigonometric_forcing_solution(void)
{
    WANT_CONSTANT_LINEAR_SOLUTION("Dxx(y) + y = cos(2*x)", "y = -⅓·cos(2x) + C₁·cos(x) + C₂·sin(x)");
    WANT_CONSTANT_LINEAR_SOLUTION("Dxx(y) + 2*Dx(y) + 5*y = sin(3*x)", "y = ¹⁄₂₆·(-3·cos(3x) - 2·sin(3x)) + "
                                                                         "C₁·exp(-x)·cos(2x) + C₂·exp(-x)·sin(2x)");
}

static void test_diffequ_general_third_order_forced_solution(void)
{
    WANT_CONSTANT_LINEAR_SOLUTION("Dxxx(y) - Dx(y) = exp(2*x)", "y = ⅙·exp(2x) + C₁·exp(x) + C₂ + C₃·exp(-x)");
}

static bool test_diffequ_want_pde_solution(const char *source, const char *want, de_solver_t want_solver,
                                             const char *file, int line)
{
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    const char *got = text ? string_c_str(text) : NULL;
    bool valid = de && result && de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
                 de_solve_result_solver(result) == want_solver && de_solve_result_count(result) == 1u && got &&
                 strcmp(got, want) == 0;

    printf("  partial differential equation\n"
           "    input:    %s\n"
           "    want: %s\n"
           "    got:   %s\n",
           source, want, got ? got : "NULL");

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
    return test_assert_true(valid, file, line, "partial differential equation solution");
}

#define WANT_TRANSPORT_SOLUTION(source, want)                                                                    \
    TEST_HARNESS_RETURN_UNLESS(test_diffequ_want_pde_solution(                                                       \
        (source), (want), DE_SOLVER_CONSTANT_COEFFICIENT_TRANSPORT, __FILE__, __LINE__))

#define WANT_CHARACTERISTIC_SOLUTION(source, want)                                                               \
    TEST_HARNESS_RETURN_UNLESS(                                                                                        \
        test_diffequ_want_pde_solution((source), (want), DE_SOLVER_CHARACTERISTICS, __FILE__, __LINE__))

#define WANT_LAPLACE_SOLUTION(source, want)                                                                      \
    TEST_HARNESS_RETURN_UNLESS(                                                                                        \
        test_diffequ_want_pde_solution((source), (want), DE_SOLVER_LAPLACE, __FILE__, __LINE__))

#define WANT_SECOND_ORDER_PDE_SOLUTION(source, want)                                                               \
    TEST_HARNESS_RETURN_UNLESS(test_diffequ_want_pde_solution(                                                       \
        (source), (want), DE_SOLVER_CONSTANT_COEFFICIENT_LINEAR, __FILE__, __LINE__))

static void test_diffequ_solves_second_order_pde_distinct_roots(void)
{
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx - 3z_yx + 2z_yy = 0", "z = F(x + y) + G(2x + y)");
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx - 3z_xy + 2z_yy = 0", "z = F(x + y) + G(2x + y)");
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx - z_xy - 2z_yx + 2z_yy = 0", "z = F(x + y) + G(2x + y)");
    WANT_SECOND_ORDER_PDE_SOLUTION("6u_ss - 9u_st + 3u_tt = 0", "u = F(½·(2t + s)) + G(t + s)");
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + z_xy = 0", "z = F(y - x) + G(y)");
}

static void test_diffequ_solves_second_order_pde_repeated_roots(void)
{
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx - 2z_xy + z_yy = 0", "z = F(x + y) + x·G(x + y)");
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 2z_xy + z_yy = 0", "z = F(y - x) + x·G(y - x)");
}

static void test_diffequ_solves_second_order_pde_degenerate_operator(void)
{
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xy = 0", "z = F(x) + G(y)");
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xy + 2z_yy = 0", "z = F(½·(2x - y)) + G(x)");
}

static void test_diffequ_solves_second_order_pde_surd_and_complex_roots(void)
{
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx - 2z_yy = 0",
                                   "z = F(y - ½x·√(8)) + G(½x·√(8) + y)");
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 2z_yy = 0",
                                   "z = F(y - 0.5ix·√(8)) + G(0.5ix·√(8) + y)");
    WANT_SECOND_ORDER_PDE_SOLUTION("-z_xx - 2z_yy = 0",
                                   "z = F(y - 0.5ix·√(8)) + G(0.5ix·√(8) + y)");
    WANT_SECOND_ORDER_PDE_SOLUTION("2z_xx + 2z_xy + z_yy = 0",
                                   "z = F((-0.5 - 0.5i)x + y) + G((-0.5 + 0.5i)x + y)");
}

static void test_diffequ_second_order_pde_rejects_outside_family(void)
{
    static const char *sources[] = {
        "z_xx - 3z_xy + 2z_yy = exp(x*x-y)",
        "x*z_xx - 3z_xy + 2z_yy = 0",
        "z_xx - 3z_xy + 2z_yy + z = 0",
        "z_xx - 3z_xy + 2z_yy + z_x = 0",
        "z_xx*z_yy - 3z_xy = 0",
        "z_xx - 3z_xy + 2z_yy = 0; z(0,y)=y",
    };

    for (size_t i = 0u; i < sizeof(sources) / sizeof(sources[0]); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        bool rejected = result && de_solve_result_status(result) == DE_SOLVE_STATUS_UNSUPPORTED &&
                        de_solve_result_count(result) == 0u;

        printf("  unsupported second-order PDE: %s\n", sources[i]);
        de_solve_result_free(result);
        de_free(de);
        ASSERT_TRUE(rejected);
    }
}

static void test_diffequ_second_order_pde_exponential_forcing(void)
{
    const char *want = "z = F(y - 3x) + G(y - 2x) + exp(x - y)";

    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 5z_yx + 6z_yy = 2e^(x-y)", want);
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 5z_xy + 6z_yy = 2exp(x-y)", want);
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 5z_xy + 6z_yy - 2exp(x-y) = 0", want);
    WANT_SECOND_ORDER_PDE_SOLUTION("2u_ss + 10u_ts + 12u_tt = 4exp(s-t)",
                                   "u = F(t - 3s) + G(t - 2s) + exp(s - t)");
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 5z_xy + 6z_yy = 2e^(x-y+3)",
                                   "z = F(y - 3x) + G(y - 2x) + exp(x - y + 3)");
}

static void test_diffequ_second_order_pde_resonant_forcing(void)
{
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 5z_xy + 6z_yy = exp(y-2x)",
                                   "z = F(y - 3x) + G(y - 2x) + x·exp(y - 2x)");
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 2z_xy + z_yy = exp(x-y)",
                                   "z = F(y - x) + x·G(y - x) + ½x²·exp(x - y)");
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xy = exp(x)", "z = F(x) + G(y) + y·exp(x)");
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xy = 1", "z = F(x) + G(y) + xy");
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 5z_xy + 6z_yy = 2", "z = F(y - 3x) + G(y - 2x) + x²");
}

static void test_diffequ_second_order_pde_forcing_superposition(void)
{
    const char *polynomial = "z = F(2x + y) + x·G(2x + y) + 4x⁴ + y⁴";
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx - 4z_yx + 4z_yy = 48(x^2 + y^2)", polynomial);
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx - 4z_xy + 4z_yy = 48x^2 + 48y^2", polynomial);
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx - 4z_xy + 4z_yy - 48(x^2+y^2) = 0", polynomial);
    WANT_SECOND_ORDER_PDE_SOLUTION("-2z_xx + 8z_xy - 8z_yy = -96(x^2+y^2)", polynomial);
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 5z_xy + 6z_yy = exp(x-y) + exp(x+y)",
                                   "z = F(y - 3x) + G(y - 2x) + ½·exp(x - y) + ¹⁄₁₂·exp(x + y)");
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 5z_xy + 6z_yy = exp(x-y) - cos(x-y)",
                                   "z = F(y - 3x) + G(y - 2x) + ½·exp(x - y) + ½·cos(x - y)");
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 5z_xy + 6z_yy = 2sin(x-y)",
                                   "z = F(y - 3x) + G(y - 2x) - sin(x - y)");
}

static void test_diffequ_second_order_pde_single_phase_integrals(void)
{
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 5z_yx + 6z_yy = 2tan(x-y)",
                                   "z = F(y - 3x) + G(y - 2x) + ½·Cl₂(2x - 2y + π) + ln(2)·(x - y)");
    WANT_SECOND_ORDER_PDE_SOLUTION("2u_ss + 10u_ts + 12u_tt = 4tan(s-t)",
                                   "u = F(t - 3s) + G(t - 2s) + ½·Cl₂(2s - 2t + π) + ln(2)·(s - t)");
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 5z_yx + 6z_yy = 2tanh(x-y)",
                                   "z = F(y - 3x) + G(y - 2x) + ½·Li₂(-exp(2·(y - x))) - "
                                   "ln(2)·(x - y) + ½·(x - y)²");
    WANT_SECOND_ORDER_PDE_SOLUTION("2u_ss + 10u_ts + 12u_tt = 4tanh(s-t)",
                                   "u = F(t - 3s) + G(t - 2s) + ½·Li₂(-exp(2·(t - s))) - "
                                   "ln(2)·(s - t) + ½·(s - t)²");
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 5z_xy + 6z_yy = 8tanh(2x-2y+3)",
                                   "z = F(y - 3x) + G(y - 2x) + ½·Li₂(-exp(-2·(2x - 2y + 3))) - "
                                   "ln(2)·(2x - 2y + 3) + ½·(2x - 2y + 3)²");
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 5z_xy + 6z_yy = 2tanh(x-y+C)",
                                   "z = F(y - 3x) + G(y - 2x) + ½·Li₂(-exp(-2·(x - y + C))) - "
                                   "ln(2)·(x - y + C) + ½·(x - y + C)²");
}

static void test_diffequ_second_order_pde_single_phase_resonance(void)
{
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 5z_xy + 6z_yy = tanh(y-2x)",
                                   "z = F(y - 3x) + G(y - 2x) + x·ln(cosh(y - 2x))");
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xy = tanh(x)", "z = F(x) + G(y) + y·ln(cosh(x))");
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 2z_xy + z_yy = 2tanh(x-y)",
                                   "z = F(y - x) + x·G(y - x) + x²·tanh(x - y)");
}

static void test_diffequ_second_order_pde_single_phase_compositions(void)
{
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 5z_yx + 6z_yy = 2atanh(x-y)",
                                   "z = F(y - 3x) + G(y - 2x) - ½x + ½y + ½·(x - y)·ln(1 - (x - y)²) + "
                                   "½·((x - y)² + 1)·atanh(x - y)");
    WANT_SECOND_ORDER_PDE_SOLUTION("2u_ss + 10u_ts + 12u_tt = 4atanh(s-t)",
                                   "u = F(t - 3s) + G(t - 2s) - ½s + ½t + ½·(s - t)·ln(1 - (s - t)²) + "
                                   "½·((s - t)² + 1)·atanh(s - t)");
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 5z_xy + 6z_yy = 2atan(x-y)",
                                   "z = F(y - 3x) + G(y - 2x) + ½x - ½y - ½·(x - y)·ln((x - y)² + 1) + "
                                   "½·((x - y)² - 1)·atan(x - y)");
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 5z_xy + 6z_yy = 2tanh(x-y)*sin(2x-2y)",
                                   "z = F(y - 3x) + G(y - 2x) + ∫^(x - y) ∫^u tanh(t)·sin(2t)·dt·du");
}

/* Remove the arbitrary homogeneous data to independently test the computed particular solution. */
static expr_t *test_diffequ_zero_arbitrary_functions(const expr_t *expr)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool is_sub = false;

    if (expr_is_arbitrary_function(expr))
        return expr_const_zero();
    if (expr_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        expr_t *first = test_diffequ_zero_arbitrary_functions(left);
        expr_t *second = test_diffequ_zero_arbitrary_functions(right);

        return is_sub ? expr_sub_simplify_owned(first, second) : expr_add_simplify_owned(first, second);
    }
    if (expr_match_mul_expr(expr, &left, &right)) {
        expr_t *first = test_diffequ_zero_arbitrary_functions(left);
        expr_t *second = test_diffequ_zero_arbitrary_functions(right);

        return expr_mul_simplify_owned(first, second);
    }
    return expr_clone(expr);
}

static void test_diffequ_second_order_pde_particular_satisfies_equation(void)
{
    static const char *sources[] = {
        "z_xx + 5z_yx + 6z_yy = 2e^(x-y)",
        "z_xx + 5z_xy + 6z_yy = exp(y-2x)",
        "z_xx + 2z_xy + z_yy = exp(x-y)",
        "z_xy = exp(x)",
        "z_xy = 1",
        "z_xx + 5z_xy + 6z_yy = exp(x-y) + exp(x+y)",
        "z_xx + 5z_xy + 6z_yy = 2sin(x-y)",
        "z_xx + 5z_yx + 6z_yy = 2tanh(x-y)",
        "z_xx + 5z_yx + 6z_yy = 2tan(x-y)",
        "2u_ss + 10u_ts + 12u_tt = 4tan(s-t)",
        "z_xx + 5z_xy + 6z_yy = 8tan(2x-2y+3)",
        "2u_ss + 10u_ts + 12u_tt = 4tanh(s-t)",
        "z_xx + 5z_xy + 6z_yy = tanh(y-2x)",
        "z_xx + 5z_xy + 6z_yy = 2atan(x-y)",
        "z_xx + 5z_xy + 6z_yy = 2tanh(x-y)*sin(2x-2y)",
    };

    for (size_t i = 0u; i < sizeof(sources) / sizeof(sources[0]); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
        expr_t *particular = solution ? test_diffequ_zero_arbitrary_functions(equ_rhs(solution)) : NULL;
        expr_t *residual = de ? equ_residual(de_equation(de)) : NULL;
        expr_t *applied = particular && residual ? expr_substitute(residual, equ_lhs(solution), particular) : NULL;
        expr_t *normalised = applied ? expr_simplify(applied) : NULL;
        bool valid = normalised != NULL;

        for (size_t sample = 0u; valid && sample < 3u; ++sample) {
            expr_t *evaluated = expr_clone(normalised);

            for (size_t j = 0u; evaluated && j < 2u; ++j) {
                expr_t *point = expr_const_long(j == 0u ? (long)sample - 1L : 2L - (long)sample);
                expr_t *next = expr_substitute(evaluated, de_independent_at(de, j), point);

                expr_free(point);
                expr_free(evaluated);
                evaluated = next;
            }
            number_t value = evaluated ? expr_eval(evaluated) : num_new();
            number_t magnitude = num_abs(value);
            number_t tolerance = num_create_from_string("1e-24");

            valid = num_is_finite(magnitude) && num_lt(magnitude, tolerance);
            num_destroy(&tolerance);
            num_destroy(&magnitude);
            num_destroy(&value);
            expr_free(evaluated);
        }
        printf("  particular solution satisfies: %s\n", sources[i]);
        expr_free(normalised);
        expr_free(applied);
        expr_free(residual);
        expr_free(particular);
        de_solve_result_free(result);
        de_free(de);
        ASSERT_TRUE(valid);
    }
}

static void test_diffequ_second_order_pde_polynomial_superposition(void)
{
    static const char *const sources[] = {
        "z_xx - 4z_yx + 4z_yy = 48(x^2+y^2)",
        "z_xx - 4z_xy + 4z_yy = -48(x^2+y^2)",
        "z_xx - 4z_xy + 4z_yy = (x^2+y^2)/2",
        "2u_ss - 8u_ts + 8u_tt = 96(s^2+t^2)",
        "z_xx - 3z_xy + 2z_yy = 24(x^2+y^2)",
        "z_xx - 4z_xy + 4z_yy = 48(x+y)*(x-y)",
        "z_xx - 4z_xy + 4z_yy = 48(x^2-y^2+1)",
        "z_xy = 6(x^2+y^2)"
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        WANT_LONG(sources[i], de_solve_result_status(result), DE_SOLVE_STATUS_SOLVED);
        const equation_t *solution = de_solve_result_at(result, 0u);
        expr_t *particular = solution ? test_diffequ_zero_arbitrary_functions(equ_rhs(solution)) : NULL;
        expr_t *residual = de ? equ_residual(de_equation(de)) : NULL;
        expr_t *applied = particular && residual ? expr_substitute(residual, equ_lhs(solution), particular) : NULL;
        expr_t *expanded = applied ? expr_expand_products_internal(applied) : NULL;
        expr_t *normalised = expanded ? expr_simplify(expanded) : NULL;
        WANT_LONG("polynomial particular solution has identically zero residual",
                  normalised && expr_is_exact_zero(normalised), 1L);
        expr_free(normalised);
        expr_free(expanded);
        expr_free(applied);
        expr_free(residual);
        expr_free(particular);
        de_solve_result_free(result);
        de_free(de);
    }
}

/* Borrow an actual function node: expression substitution matches non-leaf nodes by identity. */
static const expr_t *test_diffequ_find_arbitrary_function(const expr_t *expr)
{
    const expr_t *left = NULL, *right = NULL;
    if (expr_is_arbitrary_function(expr))
        return expr;
    if (!expr_child_exprs(expr, &left, &right))
        return NULL;
    const expr_t *found = left ? test_diffequ_find_arbitrary_function(left) : NULL;
    return found ? found : (right ? test_diffequ_find_arbitrary_function(right) : NULL);
}

static bool test_diffequ_wave_zero_at_samples(const expr_t *expr, const diffequ_t *de)
{
    for (long sample = 0L; sample < 3L; ++sample) {
        expr_t *at = expr_clone(expr);
        for (size_t i = 0u; at && i < de_independent_count(de); ++i) {
            number_t coordinate = num_create_from_frac(sample + (long)i - 1L, 4L);
            expr_t *point = expr_new_const(coordinate);
            num_destroy(&coordinate);
            expr_t *next = expr_substitute(at, de_independent_at(de, i), point);
            expr_free(point);
            expr_free(at);
            at = next;
        }
        number_t value = at ? expr_eval(at) : num_clone(NUM_NAN);
        number_t magnitude = num_abs(value), tolerance = num_create_from_string("1e-24");
        bool zero = num_is_finite(value) && num_lt(magnitude, tolerance);
        num_destroy(&tolerance);
        num_destroy(&magnitude);
        num_destroy(&value);
        expr_free(at);
        if (!zero)
            return false;
    }
    return true;
}

static void test_diffequ_symbolic_function_input(void)
{
    static const char *const sources[] = {
        "u_tt - c^2u_xx = f(x,t); u(x, 0) = g(x); u_t(x,0) = h(x)",
        "u_tt - c^2u_xx = f (x, t); u(x,0) = g (x); u_t(x,0) = h (x)",
        "w_ss - 4w_rr = F(r,s); w(r,2) = G(r); w_s(r,2) = H(r)",
        "u_tt - u_xx = [source](x,t); u(x,0) = [displacement](x); u_t(x,0) = [velocity](x)"
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        ASSERT_TRUE(de != NULL);
        ASSERT_TRUE(expr_is_arbitrary_function(equ_rhs(de_equation(de))));
        for (size_t j = 0u; j < 2u; ++j)
            ASSERT_TRUE(expr_is_arbitrary_function(equ_rhs(de_condition_at(de, j))));
        char *problem_TeX = de_to_string(de, style_LATEX);
        ASSERT_TRUE(problem_TeX && strstr(problem_TeX, "\\left(") && !strstr(problem_TeX, "NAN"));
        diffequ_solve_result_t *result = de_solve_with_options(de, DE_SOLVE_OPTION_STEPS);
        WANT_LONG(sources[i], de_solve_result_status(result), DE_SOLVE_STATUS_SOLVED);
        WANT_LONG("symbolic data use the wave IVP rule", de_solve_result_solver(result), DE_SOLVER_DALEMBERT_DUHAMEL);
        const equation_t *solution = de_solve_result_at(result, 0u);
        char *TeX = solution ? equ_to_TeX_body_wrapped(solution, SIZE_MAX) : NULL;
        ASSERT_TRUE(TeX && strstr(TeX, "\\int") && !strstr(TeX, "NAN"));
        if (i < 2u) {
            ASSERT_TRUE(strstr(problem_TeX, "f\\left(x, t\\right)"));
            ASSERT_TRUE(strstr(TeX, "f\\left(\\xi, s\\right)"));
            ASSERT_TRUE(strstr(TeX, "h\\left(\\xi\\right)"));
            ASSERT_TRUE(strstr(TeX, "g\\left(x + c\\mkern-2mu t\\right)"));
            ASSERT_TRUE(!de_constant(de, "f") && !de_constant(de, "g") && !de_constant(de, "h"));
            char *canonical = de_to_string(de, style_EXPRESSION);
            diffequ_t *roundtrip = de_from_string(canonical);
            char *roundtrip_TeX = roundtrip ? de_to_string(roundtrip, style_LATEX) : NULL;
            WANT_TEXT("symbolic-call round trip", roundtrip_TeX, problem_TeX);
            free(roundtrip_TeX);
            de_free(roundtrip);
            free(canonical);
        }
        if (i == 0u) {
            /* Substitute f(a,b)=a*b, g(a)=a^2 and h(a)=1 into the actual returned integral tree. */
            expr_t *candidate = expr_clone(equ_rhs(solution));
            const expr_t *call = NULL;
            size_t replacements = 0u;
            while ((call = test_diffequ_find_arbitrary_function(candidate)) != NULL && replacements++ < 8u) {
                const char *name = expr_symbol_name(call);
                const expr_t *args = NULL, *first = NULL, *second = NULL;
                ASSERT_TRUE(expr_child_exprs(call, &args, NULL));
                expr_t *replacement = NULL;
                if (strcmp(name, "f") == 0) {
                    ASSERT_TRUE(expr_child_exprs(args, &first, &second));
                    replacement = expr_mul(first, second);
                } else if (strcmp(name, "g") == 0) {
                    replacement = expr_mul(args, args);
                } else if (strcmp(name, "h") == 0) {
                    replacement = expr_const_one();
                }
                ASSERT_TRUE(replacement != NULL);
                expr_t *next = expr_substitute(candidate, call, replacement);
                expr_free(replacement);
                expr_free(candidate);
                candidate = next;
            }
            ASSERT_TRUE(candidate && !test_diffequ_find_arbitrary_function(candidate));
            expr_t *c = expr_new_named_var(NUM_NAN, "c"), *two = expr_const_long(2L);
            expr_t *at_speed = expr_substitute(candidate, c, two);
            diffequ_t *concrete = de_from_string("u_tt - 4u_xx = x*t; u(x,0) = x^2; u_t(x,0) = 1");
            expr_t *original_residual = equ_residual(de_equation(de));
            const expr_t *forcing_call = test_diffequ_find_arbitrary_function(original_residual);
            expr_t *forcing = expr_mul(de_independent_at(de, 0u), de_independent_at(de, 1u));
            expr_t *with_forcing = expr_substitute(original_residual, forcing_call, forcing);
            expr_t *residual = expr_substitute(with_forcing, c, two);
            expr_t *applied = expr_substitute(residual, equ_lhs(solution), at_speed);
            expr_t *error = expr_simplify(applied);
            char *candidate_text = expr_to_string(at_speed, style_UNBOUND);
            char *error_text = expr_to_string(error, style_UNBOUND);
            printf("  substituted symbolic solution: %s\n  residual: %s\n", candidate_text, error_text);
            free(error_text);
            free(candidate_text);
            ASSERT_TRUE(error && test_diffequ_wave_zero_at_samples(error, de));
            expr_t *time = expr_new_named_var(NUM_NAN, "t"), *zero = expr_const_zero();
            for (size_t j = 0u; j < 2u; ++j) {
                const equation_t *condition = de_condition_at(de, j);
                expr_t *lhs = expr_substitute(equ_lhs(condition), equ_lhs(solution), at_speed);
                expr_t *at = expr_substitute(lhs, time, zero);
                expr_t *initial_error = expr_sub(at, equ_rhs(de_condition_at(concrete, j)));
                ASSERT_TRUE(initial_error && test_diffequ_wave_zero_at_samples(initial_error, de));
                expr_free(initial_error);
                expr_free(at);
                expr_free(lhs);
            }
            expr_free(zero);
            expr_free(time);
            expr_free(error);
            expr_free(applied);
            expr_free(residual);
            expr_free(with_forcing);
            expr_free(forcing);
            expr_free(original_residual);
            de_free(concrete);
            expr_free(at_speed);
            expr_free(two);
            expr_free(c);
            expr_free(candidate);
        }
        printf("  %s\n", TeX);
        free(TeX);
        de_solve_result_free(result);
        free(problem_TeX);
        de_free(de);
    }
    /* Calls generalise across the DE parser; this is not a wave-specific f/g/h substitution. */
    diffequ_t *ode = de_from_string("y' = f(x)");
    ASSERT_TRUE(ode && expr_is_arbitrary_function(equ_rhs(de_equation(ode))));
    de_free(ode);
    ode = de_from_string("y' = f(g(x))");
    ASSERT_TRUE(ode && expr_is_arbitrary_function(equ_rhs(de_equation(ode))));
    de_free(ode);
    ASSERT_TRUE(test_diffequ_want_pde_solution(
        "{ u_tt - u_xx = 0 | t = ?, x = ?; h = 2; u(x,0) = 0, u_t(x,0) = h(x) }",
        "u = htx", DE_SOLVER_DALEMBERT_DUHAMEL, __FILE__, __LINE__));
}

static void test_diffequ_wave_ivp_polynomial_data(void)
{
    static const struct { const char *source; const char *expected; const char *time; long initial_time; } cases[] = {
        {"u_tt - 4u_xx = x*t; u(x,0) = x^2; u_t(x,0) = 1", "x^2+4t^2+t+x*t^3/6", "t", 0L},
        {"u_tt - 9u_xx = 1; u(x,0) = 0; u_t(x,0) = 0", "t^2/2", "t", 0L},
        {"8u_xx - 2u_tt = -2x*t; u_t(x,0) = 1; u(x,0) = x^2", "x^2+4t^2+t+x*t^3/6", "t", 0L},
        {"u_tt - 4u_xx = 0; u(x,2) = x^2; u_t(x,2) = x", "x^2+x*(t-2)+4*(t-2)^2", "t", 2L},
        {"w_ss - 9w_rr = r*s; w(r,0) = r^2; w_s(r,0) = 1", "r^2+9s^2+s+r*s^3/6", "s", 0L},
        {"u_tt - 4u_xx = x^2+t^2; u(x,0) = 0; u_t(x,0) = 0", "t^2*x^2/2+5t^4/12", "t", 0L},
        {"u_xx - u_tt = 0; u(x,0) = sin(x); u_t(x,0) = cos(x)", "sin(x)*cos(t)+cos(x)*sin(t)", "t", 0L}
    };
    for (size_t i = 0u; i < sizeof(cases) / sizeof(*cases); ++i) {
        diffequ_t *de = de_from_string(cases[i].source);
        diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
        WANT_LONG(cases[i].source, de_solve_result_status(result), DE_SOLVE_STATUS_SOLVED);
        WANT_LONG("one-dimensional wave IVP", de_solve_result_solver(result), DE_SOLVER_DALEMBERT_DUHAMEL);
        const equation_t *solution = de_solve_result_at(result, 0u);
        expr_t *expected = expr_from_string(cases[i].expected, NULL);
        expr_t *difference = solution ? expr_sub(equ_rhs(solution), expected) : NULL;
        ASSERT_TRUE(difference && test_diffequ_wave_zero_at_samples(difference, de));
        string_t *text = equ_to_text(solution, style_UNBOUND);
        ASSERT_TRUE(text && !strstr(string_c_str(text), "∫"));
        printf("  %s\n", string_c_str(text));
        expr_t *residual = equ_residual(de_equation(de));
        expr_t *applied = expr_substitute(residual, equ_lhs(solution), equ_rhs(solution));
        ASSERT_TRUE(applied && test_diffequ_wave_zero_at_samples(applied, de));
        expr_t *time = expr_new_named_var(NUM_NAN, cases[i].time);
        expr_t *point = expr_const_long(cases[i].initial_time);
        for (size_t j = 0u; j < 2u; ++j) {
            const equation_t *condition = de_condition_at(de, j);
            expr_t *lhs = expr_substitute(equ_lhs(condition), equ_lhs(solution), equ_rhs(solution));
            expr_t *at = expr_substitute(lhs, time, point);
            expr_t *error = expr_sub(at, equ_rhs(condition));
            ASSERT_TRUE(error && test_diffequ_wave_zero_at_samples(error, de));
            expr_free(error);
            expr_free(at);
            expr_free(lhs);
        }
        ASSERT_TRUE(strstr(de_solve_result_steps(result), "Duhamel"));
        ASSERT_TRUE(!strstr(de_solve_result_steps_TeX(result), "NAN"));
        expr_free(point);
        expr_free(time);
        expr_free(applied);
        expr_free(residual);
        string_free(text);
        expr_free(difference);
        expr_free(expected);
        de_solve_result_free(result);
        de_free(de);
    }
}

static void test_diffequ_wave_ivp_integral_data(void)
{
    const char *source = "u_tt - u_xx = exp(cosh(x)+t^2); u(x,0) = sin(x); u_t(x,0) = exp(cosh(x))";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    WANT_LONG(source, de_solve_result_status(result), DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("exact bounded-integral solution", de_solve_result_solver(result), DE_SOLVER_DALEMBERT_DUHAMEL);
    const equation_t *solution = de_solve_result_at(result, 0u);
    char *TeX = solution ? equ_to_TeX_body_wrapped(solution, SIZE_MAX) : NULL;
    ASSERT_TRUE(TeX && strstr(TeX, "\\int_{0}^{t}") && strstr(TeX, "d\\xi\\, ds"));
    ASSERT_TRUE(!strstr(TeX, "\\frac{\\int") && !strstr(TeX, "NAN"));
    ASSERT_TRUE(de_solve_result_steps(result) == NULL);
    printf("  %s\n", TeX);
    expr_t *residual = equ_residual(de_equation(de));
    expr_t *applied = expr_substitute(residual, equ_lhs(solution), equ_rhs(solution));
    expr_t *simplified = applied ? expr_simplify(applied) : NULL;
    ASSERT_TRUE(simplified && test_diffequ_wave_zero_at_samples(simplified, de));
    expr_t *time = expr_new_named_var(NUM_NAN, "t"), *zero = expr_const_zero();
    for (size_t j = 0u; j < 2u; ++j) {
        const equation_t *condition = de_condition_at(de, j);
        expr_t *lhs = expr_substitute(equ_lhs(condition), equ_lhs(solution), equ_rhs(solution));
        expr_t *at = expr_substitute(lhs, time, zero);
        expr_t *error = expr_sub(at, equ_rhs(condition));
        ASSERT_TRUE(error && test_diffequ_wave_zero_at_samples(error, de));
        expr_free(error);
        expr_free(at);
        expr_free(lhs);
    }
    expr_free(zero);
    expr_free(time);
    expr_free(applied);
    expr_free(simplified);
    expr_free(residual);
    free(TeX);
    de_solve_result_free(result);
    de_free(de);
    source = "{ u_tt - u_xx = exp(cosh(x)+t^2) | t = ?, x = ?; s = ?, @xi = ?; "
             "u(x,0) = s, u_t(x,0) = @xi }";
    de = de_from_string(source);
    result = de ? de_solve(de) : NULL;
    WANT_LONG("fresh dummy variables include initial-data names", de_solve_result_status(result), DE_SOLVE_STATUS_SOLVED);
    solution = de_solve_result_at(result, 0u);
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    ASSERT_TRUE(text && strstr(string_c_str(text), "s₁") && strstr(string_c_str(text), "ξ₁"));
    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_wave_ivp_rejects_invalid_data(void)
{
    static const char *const sources[] = {
        "u_tt + u_xx = 0; u(x,0) = 0; u_t(x,0) = 1",
        "u_tt - x*u_xx = 0; u(x,0) = 0; u_t(x,0) = 1",
        "u_tt - u_xx + u_t = 0; u(x,0) = 0; u_t(x,0) = 1",
        "u_tt - u_xx + u = 0; u(x,0) = 0; u_t(x,0) = 1",
        "u_tt - u_xx = u^2; u(x,0) = 0; u_t(x,0) = 1",
        "u_tt - u_xx + u_xt = 0; u(x,0) = 0; u_t(x,0) = 1",
        "u_tt - u_xx = 0; u(x,0) = 0; u_t(x,1) = 1",
        "u_tt - u_xx = 0; u(x,0) = 0; u(x,0) = 1",
        "u_tt - u_xx = 0; u(x,x) = 0; u_t(x,x) = 1",
        "u_tt - u_xx = 0; u(x,0) = t; u_t(x,0) = 1",
        "{ u_tt - c^2u_xx = 0 | t = ?, x = ?; c = 0; u(x,0) = 0, u_t(x,0) = 1 }"
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        ASSERT_TRUE(de != NULL);
        diffequ_solve_result_t *result = de_solve(de);
        printf("  rejected wave IVP: %s\n", sources[i]);
        ASSERT_TRUE(de_solve_result_status(result) != DE_SOLVE_STATUS_SOLVED);
        de_solve_result_free(result);
        de_free(de);
    }
}

static const expr_t *test_diffequ_outermost_integral(const expr_t *expr);

static void test_diffequ_wave_ivp_symbolic_speed(void)
{
    const char *source = "u_tt - c^2u_xx = 0; u(x,0) = sin(x); u_t(x,0) = exp(cosh(x))";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
    WANT_LONG(source, de_solve_result_status(result), DE_SOLVE_STATUS_SOLVED);
    const equation_t *solution = de_solve_result_at(result, 0u);
    char *TeX = solution ? equ_to_TeX_body_wrapped(solution, SIZE_MAX) : NULL;
    ASSERT_TRUE(TeX && !strstr(TeX, "\\sqrt") && strstr(TeX, "\\int_{"));
    ASSERT_TRUE(strstr(de_solve_result_steps(result), "real and non-zero (either sign)"));
    expr_t *c = expr_new_named_var(NUM_NAN, "c"), *one = expr_const_one(), *minus = expr_const_long(-1L);
    expr_t *positive = expr_substitute(equ_rhs(solution), c, one);
    expr_t *negative = expr_substitute(equ_rhs(solution), c, minus);
    const expr_t *forward = test_diffequ_outermost_integral(positive);
    const expr_t *backward = test_diffequ_outermost_integral(negative);
    ASSERT_TRUE(forward && backward);
    const expr_t *forward_integrand = NULL, *backward_integrand = NULL;
    ASSERT_TRUE(expr_match_integral_expr(forward, &forward_integrand, NULL));
    ASSERT_TRUE(expr_match_integral_expr(backward, &backward_integrand, NULL));
    ASSERT_TRUE(expr_struct_eq(forward_integrand, backward_integrand));
    expr_t *lower_error = expr_sub(expr_integral_lower_bound_expr(forward), expr_integral_upper_bound_expr(backward));
    expr_t *upper_error = expr_sub(expr_integral_upper_bound_expr(forward), expr_integral_lower_bound_expr(backward));
    ASSERT_TRUE(test_diffequ_wave_zero_at_samples(lower_error, de));
    ASSERT_TRUE(test_diffequ_wave_zero_at_samples(upper_error, de));
    /* Reverse the limits and hence the sign; then check the surrounding coefficients independently. */
    expr_t *forward_value = expr_substitute(positive, forward, one);
    expr_t *backward_value = expr_substitute(negative, backward, minus);
    expr_t *difference = expr_sub(forward_value, backward_value);
    ASSERT_TRUE(difference && test_diffequ_wave_zero_at_samples(difference, de));
    expr_free(difference);
    expr_free(backward_value);
    expr_free(forward_value);
    expr_free(upper_error);
    expr_free(lower_error);
    expr_free(negative);
    expr_free(positive);
    expr_free(minus);
    expr_free(one);
    expr_free(c);
    free(TeX);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_wave_kirchhoff_family(void)
{
    static const char *const sources[] = {
        "psi_xx + psi_yy + psi_zz = 1/v^2psi_tt",
        "u_tt = 9(u_xx + u_yy + u_zz)",
        "-2u_xx - 2u_yy - 2u_zz + u_tt/2 = 0",
        "w_aa + w_bb + w_cc = w_ss/4",
        "u_xx + u_yy + u_zz = F*u_tt",
        "u_thetatheta + u_phiphi + u_xx = u_tt",
        "{ u_xx + u_yy + u_zz = 1/v^2u_tt | x = ?, y = ?, z = ?, t = ?; v = -2; }",
        "u_tt = k^2*(u_xx + u_yy + u_zz)",
        "{ u_xx + u_yy + u_zz = 1/v^2u_tt | x = ?, y = ?, z = ?, t = ?; v = 2; }"
    };
    static const char *const time_names[] = {"t", "t", "t", "s", "t", "t", "t", "t", "t"};
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
        WANT_LONG(sources[i], de_solve_result_status(result), DE_SOLVE_STATUS_SOLVED);
        WANT_LONG("Kirchhoff solver", de_solve_result_solver(result), DE_SOLVER_KIRCHHOFF);
        const equation_t *solution = de_solve_result_at(result, 0u);
        string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
        char *TeX = solution ? equ_to_TeX_body_wrapped(solution, 1u) : NULL;
        ASSERT_TRUE(text && strstr(string_c_str(text), "M_(") && !strstr(string_c_str(text), "∫"));
        ASSERT_TRUE(strlen(string_c_str(text)) < 400u);
        ASSERT_TRUE(strstr(string_c_str(text), "sphere centred at r, radius |a|"));
        string_t *expression_text = equ_to_text(solution, style_EXPRESSION);
        ASSERT_TRUE(expression_text && !strstr(string_c_str(expression_text), "M_("));
        ASSERT_TRUE(strstr(string_c_str(expression_text), "∫"));
        string_free(expression_text);
        equation_t *expanded = equ_display_expanded(solution, NULL);
        string_t *expanded_text = expanded ? equ_to_text(expanded, style_UNBOUND) : NULL;
        ASSERT_TRUE(expanded_text && strcmp(string_c_str(expanded_text), string_c_str(text)) == 0);
        string_free(expanded_text);
        equ_free(expanded);
        ASSERT_TRUE(TeX && strstr(TeX, "\\mathcal M_") && !strstr(TeX, "NAN"));
        ASSERT_TRUE(strstr(TeX, "arbitrary smooth spatial functions") && !strstr(TeX, "\\int"));
        if (i == 0u || i >= 6u) {
            const char *parameter = i == 7u ? "k" : "v";
            char condition[64], absolute[16];
            snprintf(condition, sizeof(condition), "%s\\in\\mathbb R,\\quad %s\\ne0", parameter, parameter);
            snprintf(absolute, sizeof(absolute), "|%s|", parameter);
            ASSERT_TRUE(strstr(TeX, condition) && !strstr(TeX, "\\sqrt"));
            ASSERT_TRUE(strstr(string_c_str(text), absolute));
            ASSERT_TRUE(strstr(string_c_str(text), "real and non-zero (either sign)"));
        }
        ASSERT_TRUE(strstr(de_solve_result_steps_TeX(result), "\\int_{|\\boldsymbol\\omega|=1}"));
        ASSERT_TRUE(strstr(de_solve_result_steps_TeX(result), "no initial data imposed"));
        ASSERT_TRUE(strstr(de_solve_result_steps(result), "general smooth whole-space family"));
        ASSERT_TRUE(strstr(de_solve_result_steps(result), "real positive squared speed"));
        ASSERT_TRUE(!strstr(de_solve_result_steps_TeX(result), "NAN"));
        char time_line[64];
        snprintf(time_line, sizeof(time_line), "Time coordinate: %s;", time_names[i]);
        ASSERT_TRUE(strstr(de_solve_result_steps(result), time_line));
        if (i == 4u)
            ASSERT_TRUE(strstr(string_c_str(text), "F_1(") || strstr(string_c_str(text), "F₁("));
        if (i == 5u) {
            string_t *raw = expr_to_text(equ_rhs(solution), style_UNBOUND);
            ASSERT_TRUE(raw && (strstr(string_c_str(raw), "θ₁") || strstr(string_c_str(raw), "θ_1")));
            ASSERT_TRUE(strstr(string_c_str(raw), "φ₁") || strstr(string_c_str(raw), "φ_1"));
            string_free(raw);
        }
        if (i == 3u)
            ASSERT_TRUE(strstr(string_c_str(text), "M_(c₁*s)"));
        diffequ_solve_result_t *plain = de_solve(de);
        WANT_LONG("solve without derivations", de_solve_result_status(plain), DE_SOLVE_STATUS_SOLVED);
        ASSERT_TRUE(de_solve_result_steps(plain) == NULL && de_solve_result_steps_TeX(plain) == NULL);
        string_t *standalone = equ_to_text(de_solve_result_at(plain, 0u), style_UNBOUND);
        ASSERT_TRUE(standalone && strcmp(string_c_str(text), string_c_str(standalone)) == 0);
        string_free(standalone);
        de_solve_result_free(plain);
        free(TeX);
        string_free(text);
        de_solve_result_free(result);
        de_free(de);
    }
}

static void test_diffequ_wave_rejects_outside_family(void)
{
    static const char *const sources[] = {
        "u_xx + u_yy + u_zz + u_tt = 0",
        "u_xx + 2u_yy + u_zz = u_tt",
        "u_xx + u_yy + u_zz = x*u_tt",
        "u_xx + u_yy + u_zz + u = u_tt",
        "u_xx + u_yy + u_zz + u_x = u_tt",
        "u_xx + u_yy + u_zz = u_tt + 1",
        "u_xx + u_yy + u_zz = u*u_tt",
        "u_xx + u_yy + u_zz + u_xt = u_tt",
        "u_xx + u_yy + u_zz = u_tt; u(x,y,z,0)=1",
        "{ u_xx + u_yy + u_zz = 1/v^2u_tt | x = ?, y = ?, z = ?, t = ?; v = 0; }",
        "{ u_xx + u_yy + u_zz = a*u_tt | x = ?, y = ?, z = ?, t = ?; a = -1; }",
        "{ u_xx + u_yy + u_zz = a*u_tt | x = ?, y = ?, z = ?, t = ?; a = 0; }"
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        WANT_LONG(sources[i], de_solve_result_status(result), DE_SOLVE_STATUS_UNSUPPORTED);
        de_solve_result_free(result);
        de_free(de);
    }
}

/* Borrow the outermost integral; its bound coordinates must be supplied before evaluating nested integrals. */
static const expr_t *test_diffequ_outermost_integral(const expr_t *expr)
{
    const expr_t *left = NULL, *right = NULL;
    if (expr_match_integral_expr(expr, NULL, NULL))
        return expr;
    if (!expr_child_exprs(expr, &left, &right))
        return NULL;
    const expr_t *found = left ? test_diffequ_outermost_integral(left) : NULL;
    return found ? found : (right ? test_diffequ_outermost_integral(right) : NULL);
}

/* Independent spherical quadrature, exact for the quadratic data below:
 * four equally spaced azimuths and two Gauss-Legendre nodes in mu=cos(theta).
 * Division by sin(theta) changes the polar measure, so the stored Jacobian is tested too. */
static number_t test_diffequ_wave_quadrature(const expr_t *expr)
{
    const expr_t *body = NULL;
    if (expr_match_integral_expr(expr, &body, NULL)) {
        const expr_t *dummy = expr_integral_dummy_expr(expr);
        const expr_t *lower = expr_integral_lower_bound_expr(expr);
        const expr_t *upper = expr_integral_upper_bound_expr(expr);
        if (!dummy || !lower || !expr_is_exact_zero(lower) || !upper)
            return num_clone(NUM_NAN);
        number_t limit = expr_eval(upper), two_pi = num_mul_long(NUM_PI, 2L);
        bool azimuth = num_eq(limit, two_pi), polar = num_eq(limit, NUM_PI);
        num_destroy(&two_pi);
        num_destroy(&limit);
        if (!azimuth && !polar)
            return num_clone(NUM_NAN);
        number_t sum = num_clone(NUM_ZERO);
        for (long i = 0L; i < (azimuth ? 4L : 2L); ++i) {
            number_t weight = num_new(), point = num_new();
            if (azimuth) {
                weight = num_div(NUM_PI, NUM_TWO);
                point = num_mul_long(weight, i);
            } else {
                number_t third = num_create_from_frac(1L, 3L), root = num_sqrt(third);
                number_t mu = num_mul_long(root, i == 0L ? -1L : 1L);
                point = num_acos(mu);
                number_t sine = num_sin(point);
                weight = num_div(NUM_ONE, sine);
                num_destroy(&sine);
                num_destroy(&mu);
                num_destroy(&root);
                num_destroy(&third);
            }
            expr_t *at = expr_new_const(point);
            expr_t *sample = expr_substitute(body, dummy, at);
            number_t value = test_diffequ_wave_quadrature(sample);
            number_t term = num_mul(weight, value), next = num_add(sum, term);
            num_destroy(&sum);
            sum = next;
            num_destroy(&term);
            num_destroy(&value);
            expr_free(sample);
            expr_free(at);
            num_destroy(&point);
            num_destroy(&weight);
        }
        return sum;
    }
    const expr_t *integral = test_diffequ_outermost_integral(expr);
    if (!integral)
        return expr ? expr_eval(expr) : num_clone(NUM_NAN);
    number_t value = test_diffequ_wave_quadrature(integral);
    expr_t *constant = expr_new_const(value);
    expr_t *reduced = expr_substitute(expr, integral, constant);
    number_t result = test_diffequ_wave_quadrature(reduced);
    expr_free(reduced);
    expr_free(constant);
    num_destroy(&value);
    return result;
}

static void test_diffequ_wave_polynomial_data(void)
{
    diffequ_t *de = de_from_string("u_xx + u_yy + u_zz = u_tt/4");
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    WANT_LONG("wave family status", de_solve_result_status(result), DE_SOLVE_STATUS_SOLVED);
    const equation_t *solution = de_solve_result_at(result, 0u);
    expr_t *candidate = solution ? expr_clone(equ_rhs(solution)) : NULL;
    const expr_t *function = NULL;
    size_t replacements = 0u;
    /* F(r)=|r|², G(r)=|r|² exercises curvature in all three directions and both arbitrary functions. */
    while ((function = test_diffequ_find_arbitrary_function(candidate)) != NULL && replacements++ < 16u) {
        const expr_t *args = NULL, *pair = NULL, *x = NULL, *y = NULL, *z = NULL;
        ASSERT_TRUE(expr_child_exprs(function, &args, NULL));
        ASSERT_TRUE(expr_child_exprs(args, &pair, &z));
        ASSERT_TRUE(expr_child_exprs(pair, &x, &y));
        expr_t *polynomial = expr_add_simplify_owned(expr_mul(x, x), expr_mul(y, y));
        polynomial = expr_add_simplify_owned(polynomial, expr_mul(z, z));
        expr_t *next = expr_substitute(candidate, function, polynomial);
        expr_free(polynomial);
        expr_free(candidate);
        candidate = next;
    }
    ASSERT_TRUE(candidate && !test_diffequ_find_arbitrary_function(candidate));
    const expr_t *x = de_independent_at(de, 0u), *y = de_independent_at(de, 1u);
    const expr_t *z = de_independent_at(de, 2u), *t = de_independent_at(de, 3u);
    expr_t *r2 = expr_add_simplify_owned(expr_mul(x, x), expr_mul(y, y));
    r2 = expr_add_simplify_owned(r2, expr_mul(z, z));
    expr_t *t2 = expr_mul(t, t);
    expr_t *expected = expr_add_simplify_owned(expr_clone(r2), expr_mul(t, r2));
    expected = expr_add_simplify_owned(expected, expr_mul_simplify_owned(expr_const_long(12L), expr_clone(t2)));
    expected = expr_add_simplify_owned(expected, expr_mul_simplify_owned(expr_const_long(4L), expr_mul(t, t2)));
    expr_t *difference = expr_sub(candidate, expected);
    expr_t *residual = equ_residual(de_equation(de));
    /* Check the independent polynomial against the PDE; the quadrature below checks that the actual integrals equal it. */
    expr_t *applied = expr_substitute(residual, equ_lhs(solution), expected);
    const expr_t *checks[2] = {difference, applied};
    for (size_t check = 0u; check < 2u; ++check) {
        for (long sample = -2L; sample <= 2L; ++sample) {
            expr_t *at = expr_clone(checks[check]);
            for (size_t i = 0u; i < 4u; ++i) {
                expr_t *point = expr_const_long(i == 3u ? sample : (long)i + 1L);
                expr_t *next = expr_substitute(at, de_independent_at(de, i), point);
                expr_free(point);
                expr_free(at);
                at = next;
            }
            number_t value = test_diffequ_wave_quadrature(at);
            number_t magnitude = num_abs(value), tolerance = num_create_from_string("1e-20");
            bool valid = num_is_finite(magnitude) && num_lt(magnitude, tolerance);
            num_destroy(&tolerance);
            num_destroy(&magnitude);
            num_destroy(&value);
            expr_free(at);
            WANT_LONG(check == 0u ? "spherical integrals give r² + t*r² + 12t² + 4t³"
                                  : "independent polynomial satisfies the original wave equation", valid, 1L);
        }
    }
    expr_free(applied);
    expr_free(residual);
    expr_free(difference);
    expr_free(expected);
    expr_free(t2);
    expr_free(r2);
    expr_free(candidate);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_radial_euler_pde(void)
{
    static const char *const sources[] = {
        "x^2z_xx + 2xyz_yx + y^2z_yy = 0",
        "-3x^2z_xx - 6xyz_xy - 3y^2z_yy = 0",
        "s^2u_ss + st*u_st + st*u_ts + t^2u_tt = 0",
        "x^2z_xx + 2xyz_xy + y^2z_yy + xz_x + yz_y = 0",
        "x^2z_xx + 2xyz_xy + y^2z_yy - 2xz_x - 2yz_y + 2z = 0",
        "x^2z_xx + 2xyz_xy + y^2z_yy + xz_x + yz_y + z = 0",
        "x^2z_xx + 2xyz_xy + y^2z_yy - xz_x - yz_y + z = 0",
        "x^2z_xx + 2xyz_xy + y^2z_yy - 2z = 0"
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
        WANT_LONG(sources[i], de_solve_result_status(result), DE_SOLVE_STATUS_SOLVED);
        WANT_LONG("radial characteristic solver", de_solve_result_solver(result), DE_SOLVER_CHARACTERISTICS);
        const equation_t *solution = de_solve_result_at(result, 0u);
        string_t *solution_text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
        const char *text = solution_text ? string_c_str(solution_text) : NULL;
        if (i < 2u)
            WANT_TEXT("radial Euler solution", text, "z = F(y/x) + x·G(y/x)");
        ASSERT_TRUE(text && strstr(text, "F(") && strstr(text, "G("));
        if (i == 3u || i == 6u)
            ASSERT_TRUE(strstr(text, "ln(x)"));
        char *TeX = de ? de_to_string(de, style_LATEX) : NULL;
        ASSERT_TRUE(TeX && !strstr(TeX, "\\\\") && !strstr(TeX, "aligned"));
        ASSERT_TRUE(strstr(TeX, " = 0") && strcmp(strstr(TeX, " = 0"), " = 0") == 0);
        ASSERT_TRUE(strstr(de_solve_result_steps(result), "radial scaling"));
        ASSERT_TRUE(strstr(de_solve_result_steps_TeX(result), "Local chart"));
        ASSERT_TRUE(!strstr(de_solve_result_steps_TeX(result), "NAN"));
        ASSERT_TRUE(strstr(de_solve_result_steps_TeX(result), i == 2u ? "s>0" : "x>0"));

        /* Exercise both arbitrary functions using 1+eta² and eta³-eta. */
        const expr_t *x = de_independent_at(de, 0u), *y = de_independent_at(de, 1u);
        expr_t *eta = expr_div_simplify_owned(expr_clone(y), expr_clone(x));
        expr_t *square = expr_mul(eta, eta);
        expr_t *first = expr_add_simplify_owned(expr_const_one(), expr_clone(square));
        expr_t *second = expr_sub_simplify_owned(expr_mul(square, eta), expr_clone(eta));
        const expr_t *f = solution ? test_diffequ_find_arbitrary_function(equ_rhs(solution)) : NULL;
        expr_t *with_f = solution ? expr_substitute(equ_rhs(solution), f, first) : NULL;
        const expr_t *g = with_f ? test_diffequ_find_arbitrary_function(with_f) : NULL;
        expr_t *candidate = with_f ? expr_substitute(with_f, g, second) : NULL;
        ASSERT_TRUE(f && g && candidate && !test_diffequ_find_arbitrary_function(candidate));
        expr_t *residual = de ? equ_residual(de_equation(de)) : NULL;
        expr_t *applied = candidate ? expr_substitute(residual, equ_lhs(solution), candidate) : NULL;
        ASSERT_TRUE(applied);
        for (long sample = 1L; sample <= 3L; ++sample) {
            expr_t *px = expr_const_long(sample), *py = expr_const_long(sample - 2L);
            expr_t *at_x = expr_substitute(applied, x, px);
            expr_t *at_xy = at_x ? expr_substitute(at_x, y, py) : NULL;
            number_t value = at_xy ? expr_eval(at_xy) : num_new();
            number_t magnitude = num_abs(value), tolerance = num_create_from_string("1e-24");
            WANT_LONG("non-trivial radial solution satisfies the original PDE",
                      num_is_finite(magnitude) && num_lt(magnitude, tolerance), 1L);
            num_destroy(&tolerance);
            num_destroy(&magnitude);
            num_destroy(&value);
            expr_free(at_xy);
            expr_free(at_x);
            expr_free(py);
            expr_free(px);
        }
        expr_free(applied);
        expr_free(residual);
        expr_free(candidate);
        expr_free(with_f);
        expr_free(second);
        expr_free(first);
        expr_free(square);
        expr_free(eta);
        free(TeX);
        string_free(solution_text);
        de_solve_result_free(result);
        de_free(de);
    }
}

static void test_diffequ_radial_euler_rejects_nonmatching_pde(void)
{
    static const char *const sources[] = {
        "x^2z_xx + 3xyz_xy + y^2z_yy = 0",
        "x^2z_xx + 2xyz_xy + 2y^2z_yy = 0",
        "x^2z_xx + 2xyz_xy + y^2z_yy + xz_x + 2yz_y = 0",
        "x^2z_xx + 2xyz_xy + y^2z_yy + xz = 0",
        "x^2z_xx + 2xyz_xy + y^2z_yy + z^2 = 0",
        "x^2z_xx + 2xyz_xy + y^2z_yy = 1",
        "x^2z_xx + 2xyz_xy + y^2z_yy = 0; z(1,y)=y"
    };
    for (size_t i = 0u; i < sizeof(sources) / sizeof(*sources); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        WANT_LONG(sources[i], de_solve_result_status(result), DE_SOLVE_STATUS_UNSUPPORTED);
        de_solve_result_free(result);
        de_free(de);
    }
}

static void test_diffequ_abs_pde_real_residual(void)
{
    static const char *const sources[] = {
        "z_xx + 5z_yx + 6z_yy = 2abs(x-y)",
        "2u_ss + 10u_ts + 12u_tt = 4abs(s-t)",
        "z_xx + 5z_xy + 6z_yy = 8abs(1-2x+2y)"
    };

    for (size_t i = 0u; i < sizeof(sources) / sizeof(sources[0]); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
        expr_t *particular = solution ? test_diffequ_zero_arbitrary_functions(equ_rhs(solution)) : NULL;
        expr_t *residual = de ? equ_residual(de_equation(de)) : NULL;
        expr_t *applied = particular && residual ? expr_substitute(residual, equ_lhs(solution), particular) : NULL;
        expr_t *expanded = applied ? expr_display_expanded(applied) : NULL;
        expr_t *normalised = expanded ? expr_simplify(expanded) : NULL;
        char *text = particular ? expr_to_string(particular, style_UNBOUND) : NULL;

        ASSERT_TRUE(text && !strstr(text, "∫"));
        for (long sample = -2L; sample <= 2L; ++sample) {
            number_t coordinate = num_create_from_frac(sample, 2L);
            expr_t *point = expr_new_const(coordinate);
            expr_t *zero = expr_const_zero();
            expr_t *at_x = normalised ? expr_substitute(normalised, de_independent_at(de, 0u), point) : NULL;
            expr_t *at_xy = at_x ? expr_substitute(at_x, de_independent_at(de, 1u), zero) : NULL;
            number_t value = at_xy ? expr_eval(at_xy) : num_clone(NUM_NAN);
            number_t magnitude = num_abs(value);
            number_t tolerance = num_create_from_string("1e-24");

            /* Include both sides of each cusp and the cusp itself. */
            ASSERT_TRUE(num_is_real(value) && num_is_finite(value) && num_lt(magnitude, tolerance));
            num_destroy(&tolerance);
            num_destroy(&magnitude);
            num_destroy(&value);
            expr_free(at_xy);
            expr_free(at_x);
            expr_free(zero);
            expr_free(point);
            num_destroy(&coordinate);
        }
        free(text);
        expr_free(normalised);
        expr_free(expanded);
        expr_free(applied);
        expr_free(residual);
        expr_free(particular);
        de_solve_result_free(result);
        de_free(de);
    }
}

static void test_diffequ_inverse_pde_real_residual(void)
{
    static const char *const sources[] = {
        "z_xx + 5z_yx + 6z_yy = 2atanh(x-y)",
        "2u_ss + 10u_ts + 12u_tt = 4atanh(s-t)",
        "z_xx + 5z_xy + 6z_yy = 8atanh(2x-2y+1/10)",
        "z_xx + 5z_yx + 6z_yy = 2acos(x-y)",
        "2u_ss + 10u_ts + 12u_tt = 4acos(s-t)",
        "z_xx + 5z_xy + 6z_yy = 8acos(1/10-2x+2y)",
        "z_xx + 5z_yx + 6z_yy = 2asin(x-y)"
    };

    for (size_t i = 0u; i < sizeof(sources) / sizeof(sources[0]); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
        expr_t *particular = solution ? test_diffequ_zero_arbitrary_functions(equ_rhs(solution)) : NULL;
        expr_t *residual = de ? equ_residual(de_equation(de)) : NULL;
        expr_t *applied = particular && residual ? expr_substitute(residual, equ_lhs(solution), particular) : NULL;
        char *text = particular ? expr_to_string(particular, style_UNBOUND) : NULL;

        ASSERT_TRUE(text && !strstr(text, "∫"));
        for (long sample = -3L; sample <= 3L; sample += 2L) {
            number_t coordinate = num_create_from_frac(sample, 10L);
            expr_t *point = expr_new_const(coordinate);
            expr_t *zero = expr_const_zero();
            expr_t *at_x = applied ? expr_substitute(applied, de_independent_at(de, 0u), point) : NULL;
            expr_t *at_xy = at_x ? expr_substitute(at_x, de_independent_at(de, 1u), zero) : NULL;
            number_t value = at_xy ? expr_eval(at_xy) : num_clone(NUM_NAN);
            number_t magnitude = num_abs(value);
            number_t tolerance = num_create_from_string("1e-24");

            ASSERT_TRUE(num_is_real(value) && num_is_finite(value) && num_lt(magnitude, tolerance));
            num_destroy(&tolerance);
            num_destroy(&magnitude);
            num_destroy(&value);
            expr_free(at_xy);
            expr_free(at_x);
            expr_free(zero);
            expr_free(point);
            num_destroy(&coordinate);
        }
        free(text);
        expr_free(applied);
        expr_free(residual);
        expr_free(particular);
        de_solve_result_free(result);
        de_free(de);
    }
}

static void test_diffequ_second_order_pde_integral_evaluates(void)
{
    diffequ_t *de = de_from_string("z_xx + 5z_yx + 6z_yy = 2tanh(x-y)");
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    expr_t *particular = solution ? test_diffequ_zero_arbitrary_functions(equ_rhs(solution)) : NULL;
    expr_t *zero = expr_const_zero();
    expr_t *at_y_zero = particular ? expr_substitute(particular, de_independent_at(de, 1u), zero) : NULL;
    number_t values[3] = {num_new(), num_new(), num_new()};
    bool valid = at_y_zero != NULL;

    for (size_t i = 0u; valid && i < 3u; ++i) {
        expr_t *point = expr_const_long((long)i - 1L);
        expr_t *at_point = expr_substitute(at_y_zero, de_independent_at(de, 0u), point);

        num_destroy(&values[i]);
        values[i] = at_point ? expr_eval(at_point) : num_new();
        valid = num_is_real(values[i]) && num_is_finite(values[i]);
        expr_free(at_point);
        expr_free(point);
    }
    /* The arbitrary functions absorb the constant; normalise only for this zero-based integral check. */
    number_t pi_square = num_sqr(NUM_PI);
    number_t divisor = num_create_from_long(24);
    number_t offset = num_div(pi_square, divisor);
    number_t at_zero = num_add(values[1], offset);
    number_t at_zero_magnitude = num_abs(at_zero);
    number_t positive_value = num_sub(values[2], values[1]);
    number_t raw_sum = num_add(values[0], values[2]);
    number_t twice_origin = num_add(values[1], values[1]);
    number_t sum = num_sub(raw_sum, twice_origin);
    number_t magnitude = num_abs(sum);
    number_t tolerance = num_create_from_string("1e-18");

    valid = valid && num_lt(at_zero_magnitude, tolerance) &&
            num_gt(positive_value, NUM_ZERO) && num_lt(positive_value, NUM_ONE) &&
            num_lt(magnitude, tolerance);
    num_destroy(&twice_origin);
    num_destroy(&raw_sum);
    num_destroy(&positive_value);
    num_destroy(&at_zero_magnitude);
    num_destroy(&at_zero);
    num_destroy(&offset);
    num_destroy(&divisor);
    num_destroy(&pi_square);
    num_destroy(&tolerance);
    num_destroy(&magnitude);
    num_destroy(&sum);
    for (size_t i = 0u; i < 3u; ++i)
        num_destroy(&values[i]);
    expr_free(at_y_zero);
    expr_free(zero);
    expr_free(particular);
    de_solve_result_free(result);
    de_free(de);
    ASSERT_TRUE(valid);
}

/* Check the characteristic identities directly, without expanding arbitrary functions. */
static bool test_diffequ_collect_pde_slopes(const expr_t *expr, const diffequ_t *de, number_t *slopes, size_t *count)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    expr_child_exprs(expr, &left, &right);
    if (expr_is_arbitrary_function(expr)) {
        if (*count >= 2u)
            return false;
        for (size_t i = 0u; i < 2u; ++i) {
            expr_t *derivative = expr_create_deriv(left, de_independent_at(de, i));
            size_t slot = 2u * *count + i;
            bool linear = derivative != NULL;

            num_destroy(&slopes[slot]);
            slopes[slot] = derivative ? expr_eval(derivative) : num_new();
            for (size_t j = 0u; linear && j < 2u; ++j) {
                expr_t *second = expr_create_deriv(derivative, de_independent_at(de, j));
                number_t value = second ? expr_eval(second) : num_new();

                linear = num_is_zero(value);
                num_destroy(&value);
                expr_free(second);
            }
            expr_free(derivative);
            if (!linear || !num_is_finite(slopes[slot]))
                return false;
        }
        ++*count;
        return true;
    }
    return (!left || test_diffequ_collect_pde_slopes(left, de, slopes, count)) &&
           (!right || test_diffequ_collect_pde_slopes(right, de, slopes, count));
}

static void test_diffequ_second_order_pde_solutions_satisfy_operator(void)
{
    static const struct {
        const char *source;
        long a, b, c;
        bool repeated;
    } cases[] = {
        {"z_xx - 3z_yx + 2z_yy = 0", 1, -3, 2, false},
        {"z_xx - 2z_xy + z_yy = 0", 1, -2, 1, true},
        {"z_xy = 0", 0, 1, 0, false},
        {"z_xy + 2z_yy = 0", 0, 1, 2, false},
        {"z_xx - 2z_yy = 0", 1, 0, -2, false},
        {"2z_xx + 2z_xy + z_yy = 0", 2, 2, 1, false},
        {"z_xx + 2z_yy = 0", 1, 0, 2, false},
    };

    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        diffequ_t *de = de_from_string(cases[i].source);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
        number_t slopes[4] = {num_new(), num_new(), num_new(), num_new()};
        number_t tolerance = num_create_from_string("1e-24");
        size_t count = 0u;
        bool valid = solution && test_diffequ_collect_pde_slopes(equ_rhs(solution), de, slopes, &count) && count == 2u;

        for (size_t j = 0u; valid && j < 2u; ++j) {
            number_t xx = num_sqr(slopes[2u * j]);
            number_t xy = num_mul(slopes[2u * j], slopes[2u * j + 1u]);
            number_t yy = num_sqr(slopes[2u * j + 1u]);
            number_t first = num_mul_long(xx, cases[i].a);
            number_t mixed = num_mul_long(xy, cases[i].b);
            number_t last = num_mul_long(yy, cases[i].c);
            number_t sum = num_add(first, mixed);
            number_t residual = num_add(sum, last);
            number_t magnitude = num_abs(residual);

            valid = num_is_finite(magnitude) && num_lt(magnitude, tolerance);
            num_destroy(&magnitude);
            num_destroy(&residual);
            num_destroy(&sum);
            num_destroy(&last);
            num_destroy(&mixed);
            num_destroy(&first);
            num_destroy(&yy);
            num_destroy(&xy);
            num_destroy(&xx);
        }
        number_t diagonal = num_mul(slopes[0], slopes[3]);
        number_t off_diagonal = num_mul(slopes[1], slopes[2]);
        number_t determinant = num_sub(diagonal, off_diagonal);

        valid = valid && num_is_finite(determinant) && (num_is_zero(determinant) == cases[i].repeated);
        printf("  characteristic identities and independence: %s\n", cases[i].source);
        num_destroy(&determinant);
        num_destroy(&off_diagonal);
        num_destroy(&diagonal);
        num_destroy(&tolerance);
        for (size_t j = 0u; j < 4u; ++j)
            num_destroy(&slopes[j]);
        de_solve_result_free(result);
        de_free(de);
        ASSERT_TRUE(valid);
    }
}

static void test_diffequ_solves_two_dimensional_laplace_equation(void)
{
    WANT_LAPLACE_SOLUTION("phi_xx + phi_yy = 0", "φ = F(x + iy) + G(x - iy)");
    WANT_LAPLACE_SOLUTION("3*phi_xx + 3*phi_yy = 0", "φ = F(x + iy) + G(x - iy)");
    WANT_LAPLACE_SOLUTION("u_ss + u_tt = 0", "u = F(it + s) + G(s - it)");
    WANT_LAPLACE_SOLUTION("phi_rr + phi_r/r + phi_thetatheta/r^2 = 0", "φ = F(r·exp(iθ)) + G(r·exp(-iθ))");
    WANT_LAPLACE_SOLUTION("phi_rr + 1/r phi_r + 1/r^2 phi_thetatheta = 0", "φ = F(r·exp(iθ)) + G(r·exp(-iθ))");
    WANT_LAPLACE_SOLUTION("2*phi_thetatheta/r^2 + 2*phi_rr + 2*phi_r/r = 0", "φ = F(r·exp(iθ)) + G(r·exp(-iθ))");
}

static void test_diffequ_parses_pde_boundary_arguments(void)
{
    const char *source = "{ 2*Dx(u) + Dy(u) = 0 | x = ?, y = ?;; "
                         "u(x, 0) = x^2 }";
    const char *want_problem = "{ 2*∂u/∂x + ∂u/∂y = 0 | x = ?, y = ?; ; "
                                   "u(x, 0) = x^2 }";
    diffequ_t *de = de_from_string(source);
    char *problem = de ? de_to_string(de, style_EXPRESSION) : NULL;
    const expr_t *first = de ? de_condition_argument_at(de, 0u, 0u) : NULL;
    const expr_t *second = de ? de_condition_argument_at(de, 0u, 1u) : NULL;
    number_t second_value = second ? expr_eval(second) : num_new();
    number_t zero = num_create_from_long(0L);

    WANT_POINTER("parsed PDE", de, true);
    WANT_LONG("PDE independent-variable count", (long)de_independent_count(de), 2L);
    WANT_LONG("PDE boundary-argument count", (long)de_condition_argument_count(de, 0u), 2L);
    WANT_TEXT("PDE first boundary argument shares x", first == de_independent_at(de, 0u) ? "same" : "different",
                "same");
    WANT_NUMBER("PDE second boundary argument", second_value, zero);
    WANT_TEXT("PDE canonical problem", problem, want_problem);

    num_destroy(&zero);
    num_destroy(&second_value);
    free(problem);
    de_free(de);
}

static void test_diffequ_solves_constant_transport_from_y_boundary(void)
{
    WANT_TRANSPORT_SOLUTION("{ 2*Dx(u) + Dy(u) = 0 | x = ?, y = ?;; "
                              "u(x, 0) = x^2 }",
                              "u = (x - 2y)²");
}

static void test_diffequ_solves_constant_transport_from_x_boundary(void)
{
    WANT_TRANSPORT_SOLUTION("{ Dx(u) + 3*Dy(u) = 0 | x = ?, y = ?;; "
                              "u(0, y) = exp(y) }",
                              "u = exp(y - 3x)");
}

static void test_diffequ_solves_variable_forcing_from_time_boundary(void)
{
    WANT_TRANSPORT_SOLUTION("Dx(z) + 1/2*Dt(z) = cos(x); z(x,0) = 0", "z = sin(x) - sin(x - 2t)");
}

static void test_diffequ_solves_parametric_characteristic_boundary(void)
{
    WANT_CHARACTERISTIC_SOLUTION("∂z/∂x + ∂z/∂y = 2*z*(x+y); z(x,1-x) = x^2",
                                   "z = ¼·(x - y + 1)²·exp(½·((x + y)² - 1))");
}

static void test_diffequ_solves_scaled_coordinate_characteristics(void)
{
    WANT_CHARACTERISTIC_SOLUTION("(x^2+1)*Dx(z) + 2*x*y*Dy(z) - x*y = 0", "z = F(y/(x² + 1)) + ½y");
    WANT_CHARACTERISTIC_SOLUTION("(x^2+1)*Dx(z) + 2*x*y*Dy(z) - x*y = 0; "
                                   "z(x, 1) = (x^2+1)^2",
                                   "z = ½·(y + 2·(1/y·(x² + 1))² - 1)");
}

static void test_diffequ_solves_exponential_characteristics(void)
{
    WANT_CHARACTERISTIC_SOLUTION("exp(x)*Dx(z) + Dy(z) = 0", "z = F(exp(-x) + y)");
    WANT_CHARACTERISTIC_SOLUTION("exp(x)*Dx(z) + Dy(z) = 0; z(x, 0) = tanh(x)", "z = -tanh(ln(exp(-x) + y))");
}

static void test_diffequ_solves_unbounded_homogeneous_transport(void)
{
    WANT_TRANSPORT_SOLUTION("Dt(u) + c*Dx(u) = 0", "u = F(x - ct)");
}

static void test_diffequ_solves_unbounded_inhomogeneous_transport(void)
{
    WANT_TRANSPORT_SOLUTION("Dt(u) + c*Dx(u) = 1", "u = F(x - ct) + t");
}

static void test_diffequ_solves_transport_with_reaction_term(void)
{
    WANT_TRANSPORT_SOLUTION("Dx(z) + Dy(z) = z", "z = exp(x)·F(y - x)");
}

static void test_diffequ_solves_transport_with_variable_forcing(void)
{
    diffequ_t *de = de_from_string("Dx(z) + Dy(z) + z = x");
    char *tex = de ? de_to_string(de, style_LATEX) : NULL;

    WANT_TEXT("variable-forcing PDE TeX", tex,
                "\\frac{\\partial z}{\\partial x} + "
                "\\frac{\\partial z}{\\partial y} + z = x");
    WANT_TRANSPORT_SOLUTION("Dx(z) + Dy(z) + z = x", "z = exp(-x)·F(y - x) + x - 1");

    free(tex);
    de_free(de);
}

static void test_diffequ_solves_mixed_phase_trigonometric_forcing(void)
{
    WANT_TRANSPORT_SOLUTION("Dx(z) + Dy(z) = cos(x+y)", "z = F(y - x) + ½·sin(x + y)");
}

static void test_diffequ_solves_mixed_phase_unary_forcing(void)
{
    WANT_TRANSPORT_SOLUTION("Dx(z) + 2*Dy(z) = tanh(x+y)", "z = F(y - 2x) + ⅓·ln(cosh(x + y))");
}

static void test_diffequ_uses_builtin_alias_as_dependent_symbol(void)
{
    const char *source = "Dx(@phi) - Dy(@phi) = sin(x) + cos(y)";
    diffequ_t *de = de_from_string(source);
    char *tex = de ? de_to_string(de, style_LATEX) : NULL;

    WANT_TEXT("contextual dependent-symbol TeX", tex,
                "\\frac{\\partial \\phi}{\\partial x} - "
                "\\frac{\\partial \\phi}{\\partial y} = "
                "\\sin(x) + \\cos(y)");
    WANT_TRANSPORT_SOLUTION(source, "φ = F(x + y) - cos(x) - sin(y)");

    free(tex);
    de_free(de);
}

static void test_diffequ_solves_scaled_additive_transport_forcing(void)
{
    WANT_TRANSPORT_SOLUTION("2*Dx(u) + 3*Dy(u) = 4*x + 6*y", "u = F(½·(2y - 3x)) + x² + y²");
}

static void test_diffequ_solves_polynomial_reaction_forcing(void)
{
    WANT_TRANSPORT_SOLUTION("Dx(z) + 3*Dy(z) - 2*z + "
                              "4*y^2 - 22*y + 4*x + 13 = 0",
                              "z = exp(2x)·F(y - 3x) + 2x - 5y + 2y²");
}

static void test_diffequ_solves_mixed_polynomial_reaction_forcing(void)
{
    WANT_TRANSPORT_SOLUTION("2*Dx(u) - Dy(u) + 3*u = x*y", "u = exp(-³⁄₂x)·F(½·(x + 2y)) + "
                                                             "¹⁄₂₇·(9xy + 3x - 6y - 4)");
}

static void test_diffequ_solves_trigonometric_reaction_forcing(void)
{
    WANT_TRANSPORT_SOLUTION("2*Dx(@phi) + Dy(@phi) + 6*@phi = 37*sin(y)",
                              "φ = exp(-3x)·F(½·(2y - x)) + 6·sin(y) - cos(y)");
}

static void test_diffequ_solves_exponential_reaction_forcing(void)
{
    WANT_TRANSPORT_SOLUTION("3*Dx(u) + 2*Dy(u) + 5*u = 11*exp(y)", "u = exp(-⁵⁄₃x)·F(⅓·(3y - 2x)) + ¹¹⁄₇·exp(y)");
}

static void test_diffequ_solves_three_variable_transport(void)
{
    WANT_TRANSPORT_SOLUTION("Dx(@phi) + Dy(@phi) + Dz(@phi) = @phi", "φ = exp(x)·F(y - x, z - x)");
}

static void test_diffequ_solves_scaled_three_variable_transport(void)
{
    WANT_TRANSPORT_SOLUTION("2*Dx(u) - 3*Dy(u) + 4*Dz(u) + 5*u = 10", "u = exp(-⁵⁄₂x)·F(½·(3x + 2y), z - 2x) + 2");
}

static void test_diffequ_solves_nonlinear_characteristic_pde(void)
{
    diffequ_t *de = de_from_string("Dx(z) + Dy(z) = 6*(x+y)^2*z^2");
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *general = result ? de_solve_result_at(result, 0u) : NULL;
    const equation_t *singular = result ? de_solve_result_at(result, 1u) : NULL;
    string_t *general_text = general ? equ_to_text(general, style_UNBOUND) : NULL;
    string_t *singular_text = singular ? equ_to_text(singular, style_UNBOUND) : NULL;

    WANT_LONG("nonlinear characteristic status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("nonlinear characteristic solution count", (long)de_solve_result_count(result), 2L);
    WANT_TEXT("nonlinear characteristic general solution", general_text ? string_c_str(general_text) : NULL,
                "z = 1/(F(y - x) - (x + y)³)");
    WANT_TEXT("nonlinear characteristic singular solution", singular_text ? string_c_str(singular_text) : NULL,
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

    WANT_LONG("power characteristic status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_TEXT("power characteristic solution", text ? string_c_str(text) : NULL, "u = 1/√(F(y - x) - 8xy)");
    WANT_POINTER("power characteristic derivation", de_solve_result_steps(result), true);

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

    WANT_LONG("spiral characteristic status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_POINTER("spiral characteristic arbitrary family", text ? strstr(string_c_str(text), "F(") : NULL, true);
    WANT_POINTER("spiral characteristic logarithmic invariant",
                   text ? strstr(string_c_str(text), "ln(x² + y²)") : NULL, true);
    WANT_POINTER("spiral characteristic derivation", de_solve_result_steps(result), true);

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
    WANT_LONG("quadratic characteristic status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("quadratic characteristic solution count", (long)de_solve_result_count(result), 2L);
    WANT_TEXT("quadratic characteristic general solution", general_text ? string_c_str(general_text) : NULL,
                "z = 1/(F(1·(1/x - 1/y)) + 1/x)");
    WANT_TEXT("quadratic characteristic singular solution", singular_text ? string_c_str(singular_text) : NULL,
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
           "    want: z = 2/(3 - 1/x - 1/y)\n"
           "    got:   %s\n",
           source, solution_text ? string_c_str(solution_text) : "NULL");
    WANT_LONG("quadratic boundary status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("quadratic boundary selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_CHARACTERISTICS);
    WANT_LONG("quadratic boundary solution count", (long)de_solve_result_count(result), 1L);
    WANT_TEXT("quadratic boundary solution", solution_text ? string_c_str(solution_text) : NULL,
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
           "    want: %s\n"
           "    got:   %s\n"
           "    want: %s\n"
           "    got:   %s\n",
           source, "z = √(F(y/x) - x² - y²)", positive_text ? string_c_str(positive_text) : "NULL",
           "z = -√(F(y/x) - x² - y²)", negative_text ? string_c_str(negative_text) : "NULL");
    WANT_LONG("dependent-square characteristic status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("dependent-square selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_CHARACTERISTICS);
    WANT_LONG("dependent-square solution count", (long)de_solve_result_count(result), 2L);
    WANT_TEXT("dependent-square positive branch", positive_text ? string_c_str(positive_text) : NULL,
                "z = √(F(y/x) - x² - y²)");
    WANT_TEXT("dependent-square negative branch", negative_text ? string_c_str(negative_text) : NULL,
                "z = -√(F(y/x) - x² - y²)");

    string_free(negative_text);
    string_free(positive_text);
    de_solve_result_free(result);
    de_free(de);
}

static bool test_diffequ_square_branches_match(const char *source, const char *positive, const char *negative)
{
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    bool solved = result && de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
                  de_solve_result_solver(result) == DE_SOLVER_CHARACTERISTICS && de_solve_result_count(result) == 2u;
    const char *expected[2] = {positive, negative};

    printf("%s\n", source);
    for (size_t i = 0u; i < 2u; ++i) {
        const equation_t *solution = de_solve_result_at(result, i);
        string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;

        printf("%s\n", text ? string_c_str(text) : "NULL");
        solved = solved && text && strcmp(string_c_str(text), expected[i]) == 0;
        string_free(text);
    }

    de_solve_result_free(result);
    de_free(de);
    return solved;
}

static void test_diffequ_solves_parameter_forced_square_pde(void)
{
    ASSERT_TRUE(test_diffequ_square_branches_match("zz_x - zz_t = y-x", "z = √(F(-t - x) + 2xy - x²)",
                                                  "z = -√(F(-t - x) + 2xy - x²)"));
    ASSERT_TRUE(test_diffequ_square_branches_match("zz_x - zz_t = 2-x", "z = √(F(-t - x) + 4x - x²)",
                                                  "z = -√(F(-t - x) + 4x - x²)"));
    ASSERT_TRUE(test_diffequ_square_branches_match("u*u_a - u*u_b = c-a", "u = √(F(-a - b) + 2ca - a²)",
                                                  "u = -√(F(-a - b) + 2ca - a²)"));
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
           "    want: z = √(1 - xy + x/y)\n"
           "    got:   %s\n",
           source, solution_text ? string_c_str(solution_text) : "NULL");
    WANT_LONG("dependent-square boundary status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("dependent-square boundary selected solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_CHARACTERISTICS);
    WANT_LONG("dependent-square boundary solution count", (long)de_solve_result_count(result), 1L);
    WANT_TEXT("dependent-square boundary solution", solution_text ? string_c_str(solution_text) : NULL,
                "z = √(1 - xy + x/y)");

    string_free(solution_text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_applies_signed_dependent_square_boundary(void)
{
    WANT_CHARACTERISTIC_SOLUTION("z*z_x - z*z_y = y-x; z(1, y) = y^2", "z = √(2xy - 2x - 2y + 2 + (x + y - 1)⁴)");
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
           "    want: %s\n"
           "    got:   %s\n"
           "    want: %s\n"
           "    got:   %s\n",
           source, "z = √(F(y - x) - x² + y²)", positive_text ? string_c_str(positive_text) : "NULL",
           "z = -√(F(y - x) - x² + y²)", negative_text ? string_c_str(negative_text) : "NULL");
    WANT_LONG("invariant-forced square status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("invariant-forced square solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_CHARACTERISTICS);
    WANT_LONG("invariant-forced square solution count", (long)de_solve_result_count(result), 2L);
    WANT_TEXT("invariant-forced positive branch", positive_text ? string_c_str(positive_text) : NULL,
                "z = √(F(y - x) - x² + y²)");
    WANT_TEXT("invariant-forced negative branch", negative_text ? string_c_str(negative_text) : NULL,
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
           "    want: %s\n"
           "    got:   %s\n"
           "    want: %s\n"
           "    got:   %s\n",
           source, "z = √(F(x² + 2xy - y²) + 2xy)", positive_text ? string_c_str(positive_text) : "NULL",
           "z = -√(F(x² + 2xy - y²) + 2xy)", negative_text ? string_c_str(negative_text) : "NULL");
    WANT_LONG("reciprocal-forced square status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("reciprocal-forced square solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_CHARACTERISTICS);
    WANT_LONG("reciprocal-forced square solution count", (long)de_solve_result_count(result), 2L);
    WANT_TEXT("reciprocal-forced positive branch", positive_text ? string_c_str(positive_text) : NULL,
                "z = √(F(x² + 2xy - y²) + 2xy)");
    WANT_TEXT("reciprocal-forced negative branch", negative_text ? string_c_str(negative_text) : NULL,
                "z = -√(F(x² + 2xy - y²) + 2xy)");

    string_free(negative_text);
    string_free(positive_text);
    de_solve_result_free(result);
    de_free(de);
}

static void test_diffequ_solves_rotating_characteristic_pde(void)
{
    WANT_CHARACTERISTIC_SOLUTION("(x+y)*Dx(z) + (y-x)*Dy(z) = 0", "z = F(½·(ln(x² + y²) + 2·atan2(y, x)))");
}

static void test_diffequ_solves_cyclic_lagrange_pde(void)
{
    WANT_CHARACTERISTIC_SOLUTION("x*(y-z)*z_x + y*(z-x)*z_y = z*(x-y)", "F(x + y + z, xyz) = 0");
    WANT_CHARACTERISTIC_SOLUTION("x(y^2-z^2)∂z/∂x + y(z^2-x^2)∂z/∂y = "
                                   "z(x^2-y^2)",
                                   "F(x² + y² + z², xyz) = 0");
}

static void test_diffequ_solves_weighted_cyclic_lagrange_pde(void)
{
    WANT_CHARACTERISTIC_SOLUTION("(y-z)z_x - (z-x)z_y = x-y", "F(x - y + z, x² - y² + z²) = 0");
    WANT_CHARACTERISTIC_SOLUTION("(y-z)*Dx(z) + (x-z)*Dy(z) = x-y", "F(x - y + z, x² - y² + z²) = 0");
    WANT_CHARACTERISTIC_SOLUTION("-3(y-z)z_x + 3(z-x)z_y = -3(x-y)", "F(x - y + z, x² - y² + z²) = 0");
    WANT_CHARACTERISTIC_SOLUTION("(b-u)u_a - (u-a)u_b = a-b", "F(a - b + u, a² - b² + u²) = 0");
    WANT_CHARACTERISTIC_SOLUTION("(y-z)z_x + (z-x)z_y = x-y", "F(x + y + z, x² + y² + z²) = 0");
    WANT_CHARACTERISTIC_SOLUTION("6(y-z)z_x - 3(z-x)z_y = 2(x-y)", "F(x - 2y + 3z, x² - 2y² + 3z²) = 0");
}

static void test_diffequ_weighted_cyclic_rule_rejects_unverified_solutions(void)
{
    const char *sources[] = {
        "(y-z)z_x - (z-x)z_y = x-y+1",
        "(y-z)z_x - (z-x)z_y = x-y; z(x,0) = x",
    };

    for (size_t i = 0u; i < sizeof(sources) / sizeof(sources[0]); ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        bool rejected = result && de_solve_result_status(result) == DE_SOLVE_STATUS_UNSUPPORTED &&
                        de_solve_result_count(result) == 0u;

        de_solve_result_free(result);
        de_free(de);
        ASSERT_TRUE(rejected);
    }
}

static void test_diffequ_solves_monomial_linear_characteristic_pde(void)
{
    const char *source = "x^2*Dx(@psi) - x*y*Dy(@psi) + y*@psi = 0";
    diffequ_t *de = de_from_string(source);
    char *tex = de ? de_to_string(de, style_LATEX) : NULL;

    WANT_TEXT("Greek dependent-symbol TeX", tex,
                "x^{2}\\mkern-2mu \\frac{\\partial \\psi}{\\partial x} - "
                "x\\mkern-2mu y\\mkern-2mu \\frac{\\partial \\psi}{\\partial y} + "
                "\\psi\\mkern-2mu y = 0");
    WANT_CHARACTERISTIC_SOLUTION(source, "ψ = F(xy)·exp(½·1/x·y)");

    free(tex);
    de_free(de);
}

static void test_diffequ_solves_forced_monomial_characteristic_pde(void)
{
    WANT_CHARACTERISTIC_SOLUTION("x*Dx(z) - 7*y*Dy(z) = 5*x^2*y", "z = F(x⁷y) - x²y");
    WANT_CHARACTERISTIC_SOLUTION("x*Dx(u) - 2*y*Dy(u) = 6*x*y", "u = F(x²y) - 6xy");
}

static void test_diffequ_solves_forced_radial_characteristic_pde(void)
{
    WANT_CHARACTERISTIC_SOLUTION("x*y*Dx(z) - x^2*Dy(z) + y*z = 3*x^2*y", "z = F(x² + y²)/x + x²");
}

static void test_diffequ_solves_separable_trigonometric_characteristic_pde(void)
{
    WANT_CHARACTERISTIC_SOLUTION("Dx(@phi)*sec(x) + Dy(@phi) = cot(y)", "φ = F(y - sin(x)) + ln(sin(y))");
}

static void test_diffequ_solves_cross_coordinate_characteristic_pde(void)
{
    WANT_CHARACTERISTIC_SOLUTION("3*y^2*Dx(u) + Dy(u) - x*y^2*u = 0", "u = exp(⅙x²)·F(x - y³)");
    WANT_CHARACTERISTIC_SOLUTION("3*y^2*Dx(u) + Dy(u) - x*y^2*u = 0; "
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
           "    want: %s\n"
           "    got:   %s\n",
           source, "z = ½x·(y² - 1) + F(x)·exp(-y²)", text ? string_c_str(text) : "NULL");
    WANT_LONG("parameter-linear PDE status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("parameter-linear PDE solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_PARAMETER_LINEAR_PDE);
    WANT_TEXT("single-coordinate subscript remains a partial derivative", problem,
                "{ ∂z/∂y + 2*y*z = x*y^3 | y = ?; ;  }");
    WANT_TEXT("single-coordinate subscript partial derivative TeX", tex,
                "\\frac{\\partial z}{\\partial y} + 2\\mkern-2mu y\\mkern-2mu z = x\\mkern-2mu y^{3}");
    WANT_TEXT("parameter-linear PDE solution", text ? string_c_str(text) : NULL, "z = ½x·(y² - 1) + F(x)·exp(-y²)");
    WANT_POINTER("parameter-linear PDE integrating-factor derivation", result ? de_solve_result_steps(result) : NULL,
                   true);
    if (result && de_solve_result_steps(result)) {
        WANT_POINTER("parameter-linear PDE derivation forms integrating factor",
                       strstr(de_solve_result_steps(result), "μ = exp(∫(2y)dy) = exp(y²)"), true);
        WANT_POINTER("parameter-linear PDE derivation reaches arbitrary function",
                       strstr(de_solve_result_steps(result), "μz = ½x·(y² - 1)·exp(y²) + F(x)"), true);
    }
    WANT_POINTER("parameter-linear PDE TeX derivation", result ? de_solve_result_steps_TeX(result) : NULL, true);

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
           "    want: %s\n"
           "    got:   %s\n",
           source, "u = x + exp(-t²)·F(x)", text ? string_c_str(text) : "NULL");
    WANT_LONG("general parameter-linear PDE status", (long)de_solve_result_status(result),
                (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("general parameter-linear PDE solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_PARAMETER_LINEAR_PDE);
    WANT_TEXT("general parameter-linear PDE solution", text ? string_c_str(text) : NULL, "u = x + exp(-t²)·F(x)");
    WANT_POINTER("general parameter-linear PDE derivation", steps, true);
    if (steps) {
        WANT_POINTER("general derivation uses parsed coordinate",
                       strstr(steps, "Treat x as parameter and solve in t."), true);
        WANT_POINTER("general derivation computes parsed integrating factor",
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
           "    want: %s\n"
           "    got:   %s\n",
           source, "z = ¾y/x² - ¾y²/x + ½y³ - ⅜/x³ + F(x)·exp(-2xy)", text ? string_c_str(text) : "NULL");
    WANT_LONG("parameter-rate PDE status", (long)de_solve_result_status(result), (long)DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("parameter-rate PDE solver", (long)de_solve_result_solver(result),
                (long)DE_SOLVER_PARAMETER_LINEAR_PDE);
    WANT_TEXT("parameter-rate PDE solution", text ? string_c_str(text) : NULL,
                "z = ¾y/x² - ¾y²/x + ½y³ - ⅜/x³ + F(x)·exp(-2xy)");
    WANT_POINTER("parameter-rate PDE derivation", steps, true);
    if (steps) {
        WANT_POINTER("parameter-rate derivation identifies parameter",
                       strstr(steps, "Treat x as parameter and solve in y."), true);
        WANT_POINTER("parameter-rate derivation computes integrating factor",
                       strstr(steps, "μ = exp(∫(2x)dy) = exp(2xy)"), true);
        WANT_POINTER("parameter-rate derivation integrates exponential polynomial",
                       strstr(steps, "∂(μz)/∂y = μxy³ = xy³·exp(2xy)"), true);
    }

    string_free(text);
    de_solve_result_free(result);
    de_free(de);
}

/*
 * Keep this example synchronised with docs/diffequation.md.
 */
/* README example from docs/diffequation.md: compute the quartic diagnostic and symmetry. */
static void example_diffequation_quartic_lie_analysis(void)
{
    const char *source = "y'' + 3*y*y' + y^4 = 0";
    diffequ_t *ode = de_from_string(source);
    de_lie_t *lie = de_lie_new(ode);
    diffequ_solve_result_t *result = de_solve(ode);
    expr_t *first = de_lie_invariant(lie, 0u);
    expr_t *second = de_lie_invariant(lie, 1u);
    char *first_text = expr_to_string(first, style_UNBOUND);
    char *second_text = expr_to_string(second, style_UNBOUND);
    matrix_t *generators = de_lie_polynomial_generators(lie, 2u);
    expr_t *xi = NULL, *eta = NULL;
    bool valid = result && generators && mat_get_col_count(generators) == 1u &&
                 de_solve_result_status(result) == DE_SOLVE_STATUS_SERIES && first_text && second_text &&
                 strcmp(first_text, "0") == 0 && strcmp(second_text, "36·(y - 2y²)") == 0;
    if (valid) {
        mat_get(generators, 0u, 0u, &xi);
        mat_get(generators, 1u, 0u, &eta);
        char *xi_text = expr_to_string(xi, style_UNBOUND);
        char *eta_text = expr_to_string(eta, style_UNBOUND);
        valid = xi_text && eta_text && strcmp(xi_text, "1") == 0 && strcmp(eta_text, "0") == 0;
        if (valid) {
            printf("input = %s\nsolution status = local series (not a closed form)\n", source);
            printf("I₁ = %s\nI₂ = %s\n", first_text, second_text);
            printf("degree-two generators = (ξ, η) = (%s, %s)\n", xi_text, eta_text);
        }
        free(eta_text);
        free(xi_text);
    }
    mat_free(generators);
    free(second_text);
    free(first_text);
    expr_free(second);
    expr_free(first);
    de_solve_result_free(result);
    de_lie_free(lie);
    de_free(ode);
    ASSERT_TRUE(valid);
}

/* README example from docs/diffequation.md: derive the free-particle Lie algebra. */
static void example_diffequation_deriving_a_lie_algebra(void)
{
    diffequ_t *ode = de_from_string("y'' = 0");
    de_lie_t *lie = de_lie_new(ode);
    matrix_t *generators = de_lie_polynomial_generators(lie, 2);
    matrix_t *constants = de_lie_structure_constants(lie, generators);
    bool valid = generators && constants && mat_get_col_count(generators) == 8u &&
                 mat_get_row_count(constants) == 64u && mat_get_col_count(constants) == 8u;

    if (generators && constants) {
        printf("polynomial generators = %zu\n", mat_get_col_count(generators));
        printf("structure constants = %zu x %zu\n",
               mat_get_row_count(constants), mat_get_col_count(constants));
    }

    mat_free(constants);
    mat_free(generators);
    de_lie_free(lie);
    de_free(ode);
    ASSERT_TRUE(valid);
}

static void example_diffequation_solving_an_ode(void)
{
    const char *source = "Dx(y) = x*y; y(0) = 1";
    const char *want_problem = "{ dy/dx = x*y | x = ?; ; y(0) = 1 }";
    const char *want_solution = "y = exp(½x²)";
    diffequ_t *ode = de_from_string(source);
    diffequ_solve_result_t *result = de_solve(ode);
    const equation_t *solution = de_solve_result_at(result, 0u);
    char *problem_text = de_to_string(ode, style_EXPRESSION);
    string_t *solution_text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    bool valid = ode && problem_text && strcmp(problem_text, want_problem) == 0 &&
                 de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
                 de_solve_result_solver(result) == DE_SOLVER_SEPARABLE && de_solve_result_count(result) == 1u &&
                 solution_text && strcmp(string_c_str(solution_text), want_solution) == 0;

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
    const char *want_symmetry = "SL(3, ℝ)";
    const char *want_solution = "y = (2x + C₁)/(x² + C₁x + C₂)";
    diffequ_t *ode = de_from_string(source);
    diffequ_solve_result_t *result = ode ? de_solve(ode) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    const char *symmetry = result ? de_solve_result_symmetry(result) : NULL;
    string_t *solution_text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    bool valid = ode && result && de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
                 de_solve_result_solver(result) == DE_SOLVER_LINEAR_TRANSFORMATION &&
                 de_solve_result_count(result) == 1u && symmetry && strcmp(symmetry, want_symmetry) == 0 && solution_text &&
                 strcmp(string_c_str(solution_text), want_solution) == 0;

    printf("input = %s\n", source);
    printf("symmetry = %s\n", symmetry ? symmetry : "NULL");
    printf("solution = %s\n", solution_text ? string_c_str(solution_text) : "NULL");

    string_free(solution_text);
    de_solve_result_free(result);
    de_free(ode);
    ASSERT_TRUE(valid);
}

/* README example from docs/diffequation.md: weighted cyclic characteristics. */
static void example_diffequation_weighted_cyclic_pde(void)
{
    const char *source = "(y-z)z_x - (z-x)z_y = x-y";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    bool valid = result && de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED && text &&
                 strcmp(string_c_str(text), "F(x - y + z, x² - y² + z²) = 0") == 0;

    printf("input = %s\n%s\n", source, text ? string_c_str(text) : "NULL");
    string_free(text);
    de_solve_result_free(result);
    de_free(de);
    ASSERT_TRUE(valid);
}

/* README example from docs/diffequation.md: an undifferentiated parameter in the forcing. */
static void example_diffequation_parameter_forced_pde(void)
{
    ASSERT_TRUE(test_diffequ_square_branches_match("zz_x - zz_t = y-x", "z = √(F(-t - x) + 2xy - x²)",
                                                  "z = -√(F(-t - x) + 2xy - x²)"));
}

/* README example from docs/diffequation.md: a factored second-order PDE operator. */
static void example_diffequation_second_order_pde(void)
{
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx - 3z_yx + 2z_yy = 0", "z = F(x + y) + G(2x + y)");
    /* README example from docs/diffequation.md: variable-coefficient radial Euler PDE. */
    WANT_CHARACTERISTIC_SOLUTION("x^2z_xx + 2xyz_yx + y^2z_yy = 0", "z = F(y/x) + x·G(y/x)");
}

/* README example from docs/diffequation.md: parameter prefixes retain all derivative terms. */
static void example_diffequation_compact_parameter_derivatives(void)
{
    const char *source = "u_y + au_xx + bu_yy = 0";
    diffequ_t *de = de_from_string(source);
    char *TeX = de ? de_to_string(de, style_LATEX) : NULL;
    WANT_TEXT("README compact parameter coefficients", TeX,
        "\\frac{\\partial u}{\\partial y} + a\\mkern-2mu \\frac{\\partial^{2} u}{\\partial x^{2}} + "
        "b\\mkern-2mu \\frac{\\partial^{2} u}{\\partial y^{2}} = 0");
    printf("  %s\n  %s\n", source, TeX ? TeX : "NULL");
    free(TeX);
    de_free(de);
}

/* README examples from docs/diffequation.md: forced wave IVPs with evaluated and retained integrals. */
static void example_diffequation_wave_ivp(void)
{
    ASSERT_TRUE(test_diffequ_want_pde_solution("u_tt - 4u_xx = x*t; u(x,0) = x^2; u_t(x,0) = 1",
        "u = ⅙·(t³x + 24t² + 6t + 6x²)", DE_SOLVER_DALEMBERT_DUHAMEL, __FILE__, __LINE__));
    const char *source = "u_tt - u_xx = exp(cosh(x)+t^2); u(x,0) = sin(x); u_t(x,0) = exp(cosh(x))";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    WANT_LONG("README wave integral IVP", de_solve_result_solver(result), DE_SOLVER_DALEMBERT_DUHAMEL);
    const equation_t *solution = de_solve_result_at(result, 0u);
    char *TeX = solution ? equ_to_TeX_body_wrapped(solution, SIZE_MAX) : NULL;
    ASSERT_TRUE(TeX && strstr(TeX, "\\int_{0}^{t}") && strstr(TeX, "d\\xi\\, ds"));
    printf("  %s\n  %s\n", source, TeX);
    free(TeX);
    de_solve_result_free(result);
    de_free(de);
}

/* README example from docs/diffequation.md: unspecified forcing and initial profiles remain symbolic calls. */
static void example_diffequation_symbolic_wave_ivp(void)
{
    const char *source = "u_tt - c^2u_xx = f(x,t); u(x, 0) = g(x); u_t(x,0) = h(x)";
    diffequ_t *de = de_from_string(source);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    WANT_LONG("README symbolic wave IVP", de_solve_result_status(result), DE_SOLVE_STATUS_SOLVED);
    char *problem_TeX = de ? de_to_string(de, style_LATEX) : NULL;
    ASSERT_TRUE(problem_TeX && strstr(problem_TeX, "f\\left(x, t\\right)"));
    const equation_t *solution = de_solve_result_at(result, 0u);
    char *TeX = solution ? equ_to_TeX_body_wrapped(solution, SIZE_MAX) : NULL;
    ASSERT_TRUE(TeX && strstr(TeX, "f\\left(\\xi, s\\right)") && strstr(TeX, "h\\left(\\xi\\right)"));
    ASSERT_TRUE(strstr(TeX, "g\\left(x + c\\mkern-2mu t\\right)"));
    ASSERT_TRUE(strstr(TeX, "^{x + c\\mkern-2mu t}"));
    ASSERT_TRUE(!strstr(TeX, "c\\mkern-2mu t + x") && !strstr(TeX, "\\\\"));
    printf("  %s\n  %s\n  %s\n", source, problem_TeX, TeX);
    free(TeX);
    free(problem_TeX);
    de_solve_result_free(result);
    de_free(de);
}

/* README example from docs/diffequation.md: Kirchhoff's formula without prescribed initial data. */
static void example_diffequation_wave_kirchhoff(void)
{
    const char *source = "psi_xx + psi_yy + psi_zz = 1/v^2psi_tt";
    diffequ_t *de = de_from_string(source);
    char *problem_TeX = de ? de_to_string(de, style_LATEX) : NULL;
    ASSERT_TRUE(problem_TeX && strstr(problem_TeX,
        "\\frac{1}{v^{2}}\\,\\frac{\\partial^{2} \\psi}{\\partial t^{2}}"));
    printf("  %s\n", problem_TeX ? problem_TeX : "NULL");
    free(problem_TeX);
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    WANT_LONG(source, de_solve_result_status(result), DE_SOLVE_STATUS_SOLVED);
    WANT_LONG("wave solver", de_solve_result_solver(result), DE_SOLVER_KIRCHHOFF);
    const equation_t *solution = de_solve_result_at(result, 0u);
    char *TeX = solution ? equ_to_TeX_body_wrapped(solution, SIZE_MAX) : NULL;
    ASSERT_TRUE(TeX && strstr(TeX, "\\mathcal M_") && strstr(TeX, "arbitrary smooth spatial functions"));
    ASSERT_TRUE(strstr(TeX, "F(\\mathbf r)") && strstr(TeX, "G(\\mathbf r)"));
    ASSERT_TRUE(strstr(TeX, "v\\in\\mathbb R,\\quad v\\ne0") && !strstr(TeX, "\\sqrt"));
    ASSERT_TRUE(strstr(TeX, "\\mathbf r=(x,y,z)"));
    printf("  %s\n  %s\n", source, TeX);
    free(TeX);
    de_solve_result_free(result);
    de_free(de);
}

/* README example from docs/diffequation.md: an exponentially forced second-order PDE. */
static void example_diffequation_forced_second_order_pde(void)
{
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 5z_yx + 6z_yy = 2e^(x-y)",
                                   "z = F(y - 3x) + G(y - 2x) + exp(x - y)");
    /* README example: polynomial forcing with a repeated characteristic root. */
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx - 4z_yx + 4z_yy = 48(x^2+y^2)",
                                   "z = F(2x + y) + x·G(2x + y) + 4x⁴ + y⁴");
}

/* README example from docs/diffequation.md: single-phase forcing integrated using the dilogarithm. */
static void example_diffequation_single_phase_integral_pde(void)
{
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 5z_yx + 6z_yy = 2tanh(x-y)",
                                   "z = F(y - 3x) + G(y - 2x) + ½·Li₂(-exp(2·(y - x))) - "
                                   "ln(2)·(x - y) + ½·(x - y)²");
}

/* README example from docs/diffequation.md: tangent forcing integrated using the real Clausen function. */
static void example_diffequation_clausen_pde(void)
{
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 5z_yx + 6z_yy = 2tan(x-y)",
                                   "z = F(y - 3x) + G(y - 2x) + ½·Cl₂(2x - 2y + π) + ln(2)·(x - y)");
}

/* README example from docs/diffequation.md: an elementary inverse-hyperbolic-tangent forcing integral. */
static void example_diffequation_atanh_pde(void)
{
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 5z_yx + 6z_yy = 2atanh(x-y)",
                                   "z = F(y - 3x) + G(y - 2x) - ½x + ½y + ½·(x - y)·ln(1 - (x - y)²) + "
                                   "½·((x - y)² + 1)·atanh(x - y)");
}

/* README example from docs/diffequation.md: absolute-value forcing has a twice-differentiable primitive. */
static void example_diffequation_abs_pde(void)
{
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 5z_yx + 6z_yy = 2abs(x-y)",
                                   "z = F(y - 3x) + G(y - 2x) + ⅙·(x - y)²·|x - y|");
}

/* README example from docs/diffequation.md: inverse-cosine forcing uses the shared elementary integrator. */
static void example_diffequation_acos_pde(void)
{
    WANT_SECOND_ORDER_PDE_SOLUTION("z_xx + 5z_yx + 6z_yy = 2acos(x-y)",
                                   "z = F(y - 3x) + G(y - 2x) - ½·asin(x - y) + "
                                   "¼·acos(x - y)·(2·(x - y)² - 1) - ¾·(x - y)·√(1 - (x - y)²)");
}

int tests_main(void)
{
    RUN_TEST_CASE(test_diffequ_lie_free_particle);
    RUN_TEST_CASE(test_diffequ_series_quartic_coefficients);
    RUN_TEST_CASE(test_diffequ_series_general_data);
    RUN_TEST_CASE(test_diffequ_series_shifted_residuals);
    RUN_TEST_CASE(test_diffequ_series_limits_and_initial_conditions);
    RUN_TEST_CASE(test_diffequ_lie_invariants_and_normalisation);
    RUN_TEST_CASE(test_diffequ_lie_nonpolynomial_and_validation);
    RUN_TEST_CASE(test_diffequ_lie_brackets_and_search_limits);
    RUN_TEST_CASE(test_diffequ_lie_velocity_symbol_and_reduction);
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
    RUN_TEST_CASE(test_diffequ_solves_divided_differential_form);
    RUN_TEST_CASE(test_diffequ_parses_and_solves_prime_ode_shorthand);
    RUN_TEST_CASE(test_diffequ_derivative_quotient_TeX);
    RUN_TEST_CASE(test_diffequ_parses_subscript_partial_derivatives);
    RUN_TEST_CASE(test_diffequ_compact_parameter_derivatives);
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
    RUN_TEST_CASE(test_diffequ_solves_parameter_forced_square_pde);
    RUN_TEST_CASE(test_diffequ_applies_dependent_square_boundary);
    RUN_TEST_CASE(test_diffequ_applies_signed_dependent_square_boundary);
    RUN_TEST_CASE(test_diffequ_solves_invariant_forced_square_pde);
    RUN_TEST_CASE(test_diffequ_solves_reciprocal_forced_square_pde);
    RUN_TEST_CASE(test_diffequ_solves_rotating_characteristic_pde);
    RUN_TEST_CASE(test_diffequ_solves_cyclic_lagrange_pde);
    RUN_TEST_CASE(test_diffequ_solves_weighted_cyclic_lagrange_pde);
    RUN_TEST_CASE(test_diffequ_weighted_cyclic_rule_rejects_unverified_solutions);
    RUN_TEST_CASE(test_diffequ_solves_monomial_linear_characteristic_pde);
    RUN_TEST_CASE(test_diffequ_solves_forced_monomial_characteristic_pde);
    RUN_TEST_CASE(test_diffequ_solves_forced_radial_characteristic_pde);
    RUN_TEST_CASE(test_diffequ_solves_separable_trigonometric_characteristic_pde);
    RUN_TEST_CASE(test_diffequ_solves_cross_coordinate_characteristic_pde);
    RUN_TEST_CASE(test_diffequ_solves_parameter_linear_pde);
    RUN_TEST_CASE(test_diffequ_parameter_linear_pde_uses_general_rule);
    RUN_TEST_CASE(test_diffequ_parameter_linear_pde_accepts_parameter_rate);
    RUN_TEST_CASE(test_diffequ_solves_second_order_pde_distinct_roots);
    RUN_TEST_CASE(test_diffequ_solves_second_order_pde_repeated_roots);
    RUN_TEST_CASE(test_diffequ_solves_second_order_pde_degenerate_operator);
    RUN_TEST_CASE(test_diffequ_solves_second_order_pde_surd_and_complex_roots);
    RUN_TEST_CASE(test_diffequ_second_order_pde_rejects_outside_family);
    RUN_TEST_CASE(test_diffequ_second_order_pde_solutions_satisfy_operator);
    RUN_TEST_CASE(test_diffequ_second_order_pde_exponential_forcing);
    RUN_TEST_CASE(test_diffequ_second_order_pde_resonant_forcing);
    RUN_TEST_CASE(test_diffequ_second_order_pde_forcing_superposition);
    RUN_TEST_CASE(test_diffequ_second_order_pde_particular_satisfies_equation);
    RUN_TEST_CASE(test_diffequ_second_order_pde_polynomial_superposition);
    RUN_TEST_CASE(test_diffequ_radial_euler_pde);
    RUN_TEST_CASE(test_diffequ_radial_euler_rejects_nonmatching_pde);
    RUN_TEST_CASE(test_diffequ_wave_kirchhoff_family);
    RUN_TEST_CASE(test_diffequ_wave_ivp_polynomial_data);
    RUN_TEST_CASE(test_diffequ_symbolic_function_input);
    RUN_TEST_CASE(test_diffequ_wave_ivp_integral_data);
    RUN_TEST_CASE(test_diffequ_wave_ivp_symbolic_speed);
    RUN_TEST_CASE(test_diffequ_wave_ivp_rejects_invalid_data);
    RUN_TEST_CASE(test_diffequ_wave_rejects_outside_family);
    RUN_TEST_CASE(test_diffequ_wave_polynomial_data);
    RUN_TEST_CASE(test_diffequ_inverse_pde_real_residual);
    RUN_TEST_CASE(test_diffequ_abs_pde_real_residual);
    RUN_TEST_CASE(test_diffequ_second_order_pde_single_phase_integrals);
    RUN_TEST_CASE(test_diffequ_second_order_pde_single_phase_resonance);
    RUN_TEST_CASE(test_diffequ_second_order_pde_single_phase_compositions);
    RUN_TEST_CASE(test_diffequ_second_order_pde_integral_evaluates);

    TEST_SECTION("README Output Example");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_diffequation_solving_an_ode, readme_examples, "diffequation,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_diffequation_linearising_a_lie_symmetric_ode, readme_examples,
                                  "diffequation,readme,output,lie-symmetry");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_diffequation_weighted_cyclic_pde, readme_examples,
                                  "diffequation,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_diffequation_parameter_forced_pde, readme_examples,
                                  "diffequation,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_diffequation_second_order_pde, readme_examples,
                                  "diffequation,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_diffequation_forced_second_order_pde, readme_examples,
                                  "diffequation,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_diffequation_wave_kirchhoff, readme_examples,
                                  "diffequation,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_diffequation_single_phase_integral_pde, readme_examples,
                                  "diffequation,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_diffequation_clausen_pde, readme_examples,
                                  "diffequation,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_diffequation_atanh_pde, readme_examples,
                                  "diffequation,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_diffequation_abs_pde, readme_examples,
                                  "diffequation,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_diffequation_acos_pde, readme_examples,
                                  "diffequation,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_diffequation_deriving_a_lie_algebra, readme_examples,
                                  "diffequation,readme,output,lie-symmetry");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_diffequation_quartic_lie_analysis, readme_examples,
                                  "diffequation,readme,output,lie-symmetry");
    /* README example from docs/diffequation.md: the degree-eight quartic IVP series. */
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_diffequ_series_quartic_coefficients, readme_examples,
                                  "diffequation,readme,output,series");
    /* README example from docs/diffequation.md: preserve both arbitrary initial constants. */
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_diffequ_series_general_data, readme_examples,
                                  "diffequation,readme,output,series");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_diffequ_linear_solution_retains_formal_integral, readme_examples,
                                  "diffequation,readme,output,linear");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_diffequation_compact_parameter_derivatives, readme_examples,
                                  "diffequation,readme,output");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_diffequation_wave_ivp, readme_examples,
                                  "diffequation,readme,output,wave");
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(example_diffequation_symbolic_wave_ivp, readme_examples,
                                  "diffequation,readme,output,wave");

    return TESTS_EXIT_CODE();
}
