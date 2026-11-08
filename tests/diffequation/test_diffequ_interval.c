/**
 * @file test_diffequ_interval.c
 * @brief Native parsing, metadata and solution-domain checks for coordinate intervals.
 *
 * Ensures open intervals remain separate from point conditions, round-trip in
 * native text, constrain complete solutions, and never silently discard invalid
 * inequalities. The final executable example mirrors docs/diffequation.md.
 */
#include <stdio.h>
#include <string.h>

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
    TEST_ASSERT_TRUE(ok, "empty, malformed, inclusive and non-coordinate inequalities are rejected");
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
