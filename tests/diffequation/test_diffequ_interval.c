/**
 * @file test_diffequ_interval.c
 * @brief Native parsing, metadata and solution-domain checks for coordinate and parameter restrictions.
 *
 * Ensures open intervals remain separate from point conditions, round-trip in
 * native text, constrain complete solutions, and never silently discard invalid
 * inequalities. The final executable example mirrors docs/diffequation.md.
 */
#include <stdio.h>
#include <string.h>

#include "string-t.h"
#include "diffequation-t.h"
#include "equation-t.h"
#include "test_harness.h"
#include "test_diffequ_interval.h"

static void test_interval_parsing(void)
{
    static const struct { const char *source; size_t points; const char *lower, *upper; } cases[] = {
        {"y''+y=cosec(x); 0 < x < pi", 0u, "0", "pi"},
        {"y''+y=cosec(x); pi > x > 0", 0u, "0", "pi"},
        {"Dtt(u)+u=cosec(t); 0<t<pi", 0u, "0", "pi"},
        {"y'=y; x>0", 0u, "0", NULL},
        {"y'=y; x<2", 0u, NULL, "2"},
        {"y'=y; x>0; x>1; x>0; x<2", 0u, "1", "2"},
        {"y'=y; y(1)=2; 0<x<pi", 1u, "0", "pi"},
        {"{y'=y | y(1)=2, 0<x<pi}", 1u, "0", "pi"},
        {"{y'=y | x=?; ; 0<x<pi}", 0u, "0", "pi"},
    };
    bool ok = true;
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        diffequ_t *de = de_from_string(cases[i].source);
        string_t *text = de ? de_to_text(de, style_EXPRESSION) : NULL;
        diffequ_t *copy = text ? de_from_text(text) : NULL;
        bool valid = de && copy && de_condition_count(de) == cases[i].points &&
                     de_condition_count(copy) == cases[i].points && !strstr(string_c_str(text), "NAN");
        for (size_t edge = 0; valid && edge < 2u; ++edge) {
            const expr_t *original = edge ? de_independent_upper_bound(de, 0u) : de_independent_lower_bound(de, 0u);
            const expr_t *roundtrip = edge ? de_independent_upper_bound(copy, 0u) : de_independent_lower_bound(copy, 0u);
            const char *expected = edge ? cases[i].upper : cases[i].lower;
            if (!expected) {
                valid = !original && !roundtrip;
                continue;
            }
            expr_t *bound = expr_from_string(expected, NULL);
            number_t a = expr_eval(original), b = expr_eval(roundtrip), want = expr_eval(bound);
            valid = original && roundtrip && num_eq(a, want) && num_eq(b, want);
            num_destroy(&want); num_destroy(&b); num_destroy(&a); expr_free(bound);
        }
        if (!valid)
            string_printf("Interval input: %s; canonical: %s\n", cases[i].source, text ? string_c_str(text) : "NULL");
        ok = valid && ok;
        de_free(copy); string_free(text); de_free(de);
    }
    ok = ok && !de_independent_lower_bound(NULL, 0u) && !de_independent_upper_bound(NULL, 0u);
    TEST_ASSERT_TRUE(ok, "strict coordinate intervals round-trip separately from initial and boundary conditions");
}

static void test_interval_rejections(void)
{
    const char *sources[] = {"y'=y; 2<x<1", "y'=y; 1<x<1", "y'=y; 0<=x<pi", "y'=y; 0<x<=pi",
                             "y'=y; 0<x>pi", "y'=y; x<x+1", "y'=y; x<1i", "y'=y; 0<x<",
                             "y'=y; 0<x<pi<4", "y'=y; 0<y<pi", "y'=y; x~~=1"};
    bool ok = true;
    for (size_t i = 0; i < sizeof sources / sizeof *sources; ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        if (de)
            string_printf("Unexpected interval acceptance: %s\n", sources[i]);
        ok = !de && ok;
        de_free(de);
    }
    TEST_ASSERT_TRUE(ok, "empty, malformed, inclusive-coordinate and dependent-field inequalities are rejected");
}

static void test_parameter_restrictions(void)
{
    const char *sources[] = {
        "mx''+cx'+kx=0; c>=0; m>0; k>0",
        "mx''+cx'+kx=0; c≥0; m>0; k>0",
        "my''+cy'+ky=0; 0<=c; 0<m; 0<k",
        "my''+cy'+ky=0; 0≤c; 0<m; 0<k",
        "y''+a*y=0; 0<a<=2",
        "{mx''+cx'+kx=0 | t=?; ; c>=0,m>0,k>0}",
        "mx''+cx'+kx=0; c>=0; m>0; k>0; x(0)=1; x'(0)=0",
        "{mx''+cx'+kx=0 | t=?; c=0,m=1,k=2; c>=0,m>0,k>0}",
        "{mx''+cx'+kx=0 | t=?; c=?,m=?,k=?; c>=0; m>0; k>0}",
    };
    bool ok = true;
    for (size_t i = 0; i < sizeof sources / sizeof *sources; ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        string_t *source = de ? de_to_text(de, style_EXPRESSION) : NULL;
        string_t *problem = de ? de_problem_to_TeX(de) : NULL;
        diffequ_t *copy = source ? de_from_text(source) : NULL;
        diffequ_solve_result_t *result = de ? de_solve_with_context(de, DE_SOLVE_OPTION_NONE, NULL) : NULL;
        const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
        string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
        string_t *TeX = solution ? equ_to_text(solution, style_LATEX) : NULL;
        bool valid = de && copy && problem && text && TeX &&
                     de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
                     de_condition_count(de) == (i == 6u ? 2u : 0u) &&
                     de_condition_count(copy) == de_condition_count(de) &&
                     !strstr(string_c_str(problem), "NAN") && !strstr(string_c_str(text), "NAN") &&
                     !strstr(string_c_str(TeX), "real_at_least");
        if (valid && i < 6u)
            valid = strstr(string_c_str(text), "where") && strstr(string_c_str(TeX), "\\ge");
        if (!valid)
            string_printf("Parameter restriction: %s\nProblem: %S\nSolution: %S\nTeX: %S\n",
                           sources[i], source, text, TeX);
        ok = valid && ok;
        string_free(TeX); string_free(text); de_solve_result_free(result);
        de_free(copy); string_free(problem); string_free(source); de_free(de);
    }
    TEST_ASSERT_TRUE(ok, "strict and inclusive parameter restrictions solve and round-trip without fixing constants");
}

static void test_parameter_restriction_evaluation(void)
{
    diffequ_t *de = de_from_string("y'=a*y; y(0)=1; a>=0");
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    expr_t *parameter = expr_new_named_var(NUM_NAN, "a");
    bool ok = parameter && de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED;
    const char *values[] = {"0", "2", "-1", "1+i"};
    for (size_t i = 0; ok && i < sizeof values / sizeof *values; ++i) {
        expr_t *value = expr_from_string(values[i], NULL);
        expr_t *bound = expr_substitute(equ_rhs(solution), parameter, value);
        expr_t *zero = expr_new_const(NUM_ZERO);
        expr_t *at = bound ? expr_substitute(bound, de_independent_at(de, 0u), zero) : NULL;
        number_t number = at ? expr_eval(at) : num_clone(NUM_NAN);
        ok = i < 2u ? num_eq(number, NUM_ONE) : num_is_nan(number);
        if (!ok) string_printf("Restricted evaluation: a=%s; value=%g\n", values[i], num_to_double(number));
        num_destroy(&number); expr_free(at); expr_free(zero); expr_free(bound); expr_free(value);
    }
    expr_free(parameter); de_solve_result_free(result); de_free(de);
    const char *invalid[] = {
        "{y'=a*y | x=?; a=-1; a>=0}", "{y'=a*y | x=?; a=1+i; a>=0}",
        "y'=a*y; a>x", "y'=a*y; a>y", "y'=a*y; a~~=0",
    };
    for (size_t i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        de = de_from_string(invalid[i]);
        if (de) string_printf("Unexpected parameter restriction: %s\n", invalid[i]);
        ok = !de && ok;
        de_free(de);
    }
    TEST_ASSERT_TRUE(ok, "parameter restrictions accept zero inclusively and reject invalid live or supplied values");
}

static void test_parameter_expression_serialisation(void)
{
    const char *sources[] = {"x where (a>=0)", "x where (a≥0)", "x where (a>0)",
                             "x where (0<=a)", "x where (0≤a)", "x where (0<a)"};
    bool ok = true;
    for (size_t i = 0; i < sizeof sources / sizeof *sources; ++i) {
        expr_t *expression = expr_from_string(sources[i], NULL);
        string_t *native = expression ? expr_to_text(expression, style_EXPRESSION) : NULL;
        string_t *body = expression ? expr_to_function_body_text(expression) : NULL;
        string_t *function = expression ? expr_to_text(expression, style_FUNCTION) : NULL;
        string_t *TeX = expression ? expr_to_text(expression, style_LATEX) : NULL;
        expr_t *copy = native ? expr_from_text(native, NULL) : NULL;
        expr_t *from_body = body ? expr_from_function_body_text(body, NULL) : NULL;
        bool valid = expression && native && body && function && TeX && copy && from_body &&
                     strstr(string_c_str(function), "realpart(a)") && !strstr(string_c_str(TeX), "real_at_least");
        if (!valid)
            string_printf("Ordering source: %s\nNative: %S\nFunction: %S\n", sources[i], native, body);
        ok = valid && ok;
        expr_free(from_body); expr_free(copy); string_free(TeX); string_free(function);
        string_free(body); string_free(native); expr_free(expression);
    }
    TEST_ASSERT_TRUE(ok, "real parameter conditions round-trip in native and Function text with semantic TeX output");
}

static void test_interval_series_domain(void)
{
    diffequ_t *de = de_from_string("y''=y; y(1)=1; y'(1)=0; 0<x<2");
    diffequ_solve_result_t *result = de ? de_solve_series(de, 4u, DE_SOLVE_OPTION_NONE) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    const expr_t *coefficient = de_solve_result_series_coefficient(result, 0u);
    number_t value = coefficient ? expr_eval(coefficient) : num_clone(NUM_NAN);
    bool ok = solution && de_solve_result_status(result) == DE_SOLVE_STATUS_SERIES && text &&
              strstr(string_c_str(text), "where") && strstr(string_c_str(text), "x ∈ ℝ") &&
              strstr(string_c_str(text), "Re(x) > 0") && strstr(string_c_str(text), "> x") && num_eq(value, NUM_ONE);
    num_destroy(&value);
    string_free(text);
    de_solve_result_free(result); de_free(de);
    TEST_ASSERT_TRUE(ok, "explicit Taylor-series results retain their authored coordinate interval");
}

static void test_interval_solution_domain(void)
{
    diffequ_t *de = de_from_string("y'=y; y(1)=1; 0<x<pi");
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    bool ok = solution && de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED && de_condition_count(de) == 1u;
    const char *points[] = {"1", "0", "-1", "4", "1+i"};
    for (size_t i = 0; ok && i < sizeof points / sizeof *points; ++i) {
        expr_t *point = expr_from_string(points[i], NULL);
        expr_t *at = expr_substitute(equ_rhs(solution), de_independent_at(de, 0u), point);
        number_t value = expr_eval(at);
        ok = i == 0u ? num_eq(value, NUM_ONE) : num_is_nan(value);
        num_destroy(&value); expr_free(at); expr_free(point);
    }
    string_t *TeX = de ? de_problem_to_TeX(de) : NULL;
    ok = ok && TeX && strstr(string_c_str(TeX), "\\pi") && !strstr(string_c_str(TeX), "NAN");
    string_free(TeX); de_solve_result_free(result); de_free(de);
    TEST_ASSERT_TRUE(ok, "complete solution families retain their real interval and TeX retains the authored bounds");
}

/* Register ordinary interval checks with the owning suite. */
void test_diffequ_intervals(void)
{
    TEST_RUN_SUBTEST(test_interval_parsing, NULL);
    TEST_RUN_SUBTEST(test_interval_rejections, NULL);
    TEST_RUN_SUBTEST(test_interval_solution_domain, NULL);
    TEST_RUN_SUBTEST(test_interval_series_domain, NULL);
    TEST_RUN_SUBTEST(test_parameter_restrictions, NULL);
    TEST_RUN_SUBTEST(test_parameter_restriction_evaluation, NULL);
    TEST_RUN_SUBTEST(test_parameter_expression_serialisation, NULL);
}

/* README example from docs/diffequation.md: intervals are not point conditions. */
void example_diffequation_interval(void)
{
    diffequ_t *problem = de_from_string("y''+y=cosec(x); 0<x<pi");
    diffequ_solve_result_t *result = problem ? de_solve(problem) : NULL;
    const expr_t *lower = de_independent_lower_bound(problem, 0u);
    const expr_t *upper = de_independent_upper_bound(problem, 0u);
    string_t *a = lower ? expr_to_text(lower, style_UNBOUND) : NULL;
    string_t *b = upper ? expr_to_text(upper, style_UNBOUND) : NULL;
    bool ok = a && b && result && de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED;
    if (ok) {
        printf("Interval: %s < x < %s\n", string_c_str(a), string_c_str(b));
        printf("Point conditions: %zu\n", de_condition_count(problem));
        printf("Solution families: %zu\n", de_solve_result_count(result));
    }
    string_free(b); string_free(a); de_solve_result_free(result); de_free(problem);
    TEST_ASSERT_TRUE(ok, "README interval example solves without consuming integration constants");
}

/* README example from docs/diffequation.md: coefficient inequalities preserve integration constants. */
void example_diffequation_parameter_restrictions(void)
{
    diffequ_t *problem = de_from_string("mx''+cx'+kx=0; c>=0; m>0; k>0");
    diffequ_solve_result_t *result = problem ? de_solve(problem) : NULL;
    string_t *text = problem ? de_to_text(problem, style_EXPRESSION) : NULL;
    bool ok = text && result && de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED;
    if (ok) {
        puts(string_c_str(text));
        printf("Point conditions: %zu\n", de_condition_count(problem));
        puts("Symbolic solution: solved");
        TEST_ASSERT_TRUE(de_condition_count(problem) == 0u, "README parameter assumptions are not initial data");
    }
    string_free(text); de_solve_result_free(result); de_free(problem);
    TEST_ASSERT_TRUE(ok, "README coefficient assumptions retain the symbolic solution family");
}
