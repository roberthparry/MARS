/**
 * @file test_diffequ_format.c
 * @brief Differential-equation power and explicit-solution formatting regressions.
 *
 * Checks integer and Unicode power notation in expression and differential-equation output. The associated example
 * verifies that documented notation remains parseable and consistent.
 *
 * Used by the project test harness for regression verification. Select cases through tests/test_config.json and
 * run suites sequentially; this source is not part of the installed library.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "format-t.h"
#include "string-t.h"
#include "diffequation-t.h"
#include "test_harness.h"
#include "test_database.h"
#include "test_diffequ_format.h"

static void solution_notation_row(const void *data)
{
    const test_database_case_t *row = data;
    diffequ_t *problem = de_from_text(row->input);
    diffequ_solve_result_t *result = problem ? de_solve(problem) : NULL;
    const equation_t *solution = result ? de_solve_result_at(result, 0u) : NULL;
    char *TeX = solution ? equ_to_TeX_body_wrapped(solution, SIZE_MAX) : NULL;
    bool ok = TeX && strstr(TeX, string_c_str(row->expected)) &&
              expr_is_variable(equ_lhs(solution));
    if (!ok)
        fmt_print("Solution notation: {:s}\nExpected notation: {:s}\nActual: {:s}\n",
            FMT(row->input),
            FMT(row->expected),
            FMT(TeX ? TeX : "(none)"));
    free(TeX); de_solve_result_free(result); de_free(problem);
    TEST_ASSERT_TRUE(ok, "explicit ODE notation labels the native solution by its declared coordinate");
}

/* Check presentation while retaining the variable left side required by numerical callers. */
void test_diffequ_solution_notation(void)
{
    test_run_database_cases(__FILE__, __LINE__, "diffequation.solution_notation", "test_diffequ_solution_notation",
                              solution_notation_row);
}

static void check_power_display(const char *source, const char *expected)
{
    diffequ_t *de = de_from_string(source);
    char *text = de ? de_to_string(de, style_EXPRESSION) : NULL;
    diffequ_t *again = text ? de_from_string(text) : NULL;
    char *round_trip = again ? de_to_string(again, style_EXPRESSION) : NULL;
    char *base = de ? de_to_string(de, style_UNBOUND) : NULL;
    char *round_trip_base = again ? de_to_string(again, style_UNBOUND) : NULL;
    bool matched = text && strstr(text, expected);
    bool stable = text && round_trip && strcmp(text, round_trip) == 0;
    bool equivalent = base && round_trip_base && strcmp(base, round_trip_base) == 0;

    fmt_print("  {source}\n  {value_2}\n", FMT(source), FMT(value_2, text ? text : "NULL"));
    if (!stable)
        fmt_print("  round trip: {round_trip}\n", FMT(round_trip, round_trip ? round_trip : "NULL"));
    free(round_trip_base);
    free(base);
    free(round_trip);
    free(text);
    de_free(again);
    de_free(de);
    ASSERT_TRUE(matched);
    ASSERT_TRUE(stable);
    ASSERT_TRUE(equivalent);
}

/* Keep polynomial powers consistent across equations, conditions and bindings. */
void test_diffequ_expression_integer_powers(void)
{
    static const struct { const char *source; const char *expected; } cases[] = {
        {"u_tt - v^2u_xx = f(x,t); u(x,0) = g(x); u_t(x,0) = h(x)", "v²∂²u/∂x²"},
        {"y' = x^12 + x^0 + x^(3)", "x¹² + x⁰ + x³"},
        {"y' = x^2; y(0) = 2^3", "y(0) = 2³"},
        {"{ y' = a*x^2 | x = ?; a = 2^3; y(0) = 1 }", "a = 2³"},
        {"(x^2+y^2)dx + 2xy dy = 0", "x²+y²"},
        {"y' = x^2.5", "x^2.5"},
        {"y' = x^2e1", "x^2e1"},
        {"y' = x^1/2", "x^1/2"},
        {"y' = x^(2/3)", "x^(2/3)"},
        {"y' = x^-2", "x^-2"},
        {"y' = x^n", "x^n"},
        {"y' = x^2^3", "x^2^3"},
        {"y' = x²^3", "x²^3"},
        {"y' = x^2!", "x^2!"},
        {"y' = x^(2+3)", "x^(2+3)"},
        {"y' = x^2147483648", "x^2147483648"},
    };
    for (size_t i = 0u; i < sizeof(cases) / sizeof(*cases); ++i)
        check_power_display(cases[i].source, cases[i].expected);
}

/* README example from docs/diffequation.md: native Unicode problem-card powers. */
void example_diffequation_unicode_powers(void)
{
    check_power_display("u_tt - v^2u_xx = f(x,t); u(x,0) = g(x); u_t(x,0) = h(x)",
                        "{ ∂²u/∂t² - v²∂²u/∂x² = f(x,t) | t = ?, x = ?; ; u(x,0) = g(x), ∂u/∂t(x,0) = h(x) }");
}
