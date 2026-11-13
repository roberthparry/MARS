/**
 * @file test_diffequ_riccati.c
 * @brief Verify Riccati linearisations through the original differential equation.
 *
 * Checks analytic continuation across zero, independently evaluates numerical differential residuals,
 * protects the exceptional projective solution and exercises initial values and scope limits.
 * The final runnable example is synchronised with docs/diffequation.md.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "string-t.h"
#include "diffequation-t.h"
#include "expression-t.h"
#include "test_harness.h"
#include "test_diffequ_riccati.h"

static double riccati_value(const expr_t *expression, const expr_t *coordinate, const char *point)
{
    expr_t *at = expr_from_string(point, NULL);
    expr_t *substituted = at ? expr_substitute(expression, coordinate, at) : NULL;
    number_t value = substituted ? expr_eval(substituted) : num_clone(NUM_NAN);
    double result = num_to_double(value);
    num_destroy(&value);
    expr_free(substituted);
    expr_free(at);
    return result;
}

/* Compare the displayed Bessel formula with the independent analytic evaluation graph on both half-axes. */
static bool riccati_bessel_display(const diffequ_t *de, const equation_t *solution, const char *axis)
{
    string_t *text = equ_to_text(solution, style_UNBOUND);
    string_t *TeX = equ_to_text(solution, style_LATEX);
    const char *source = text ? strstr(string_c_str(text), " = ") : NULL;
    if (source)
        source += 3;
    const char *end = source ? strchr(source, ';') : NULL;
    string_t *body = source && end ? string_new() : NULL;
    if (body && string_append_chars(body, source, (size_t)(end - source)) != 0) {
        string_free(body); body = NULL;
    }
    expr_bindings_t *bindings = NULL;
    expr_t *shown = body ? expr_from_text(body, &bindings) : NULL;
    expr_t *c = expr_new_named_var(NUM_NAN, "C"), *one = expr_new_const(NUM_ONE);
    expr_t *actual = expr_substitute(equ_rhs(solution), c, one);
    expr_t *coordinate = bindings ? expr_bindings_get(bindings, axis) : NULL;
    if (bindings && expr_bindings_get(bindings, "C"))
        expr_set_val(expr_bindings_get(bindings, "C"), NUM_ONE);
    const char *points[] = {"-0.5", "0.75"};
    bool valid = shown && coordinate && strstr(string_c_str(text), "J") &&
                 !strstr(string_c_str(text), "Hypergeometric") && !strstr(string_c_str(text), "!=") &&
                 strstr(string_c_str(text), "≠ 0") && source && *source != '{' && TeX &&
                 strstr(string_c_str(TeX), "\\begin{cases}") && !strstr(string_c_str(TeX), "NAN");
    for (size_t i = 0u; valid && i < sizeof points / sizeof *points; ++i) {
        double first = riccati_value(actual, de_independent_at(de, 0u), points[i]);
        double second = riccati_value(shown, coordinate, points[i]);
        printf("  Bessel display at %s: %.12g; analytic value: %.12g\n", points[i], second, first);
        valid = isfinite(first) && isfinite(second) && fabs(first - second) < 1e-8 * (1 + fabs(first));
    }
    const char *origin_source = source ? strstr(source, "(0) = ") : NULL;
    if (valid && origin_source) {
        expr_bindings_t *origin_bindings = NULL;
        expr_t *origin = expr_from_string(origin_source + strlen("(0) = "), &origin_bindings);
        expr_t *parameter = origin_bindings ? expr_bindings_get(origin_bindings, "C") : NULL;
        if (parameter)
            expr_set_val(parameter, NUM_ONE);
        number_t origin_value = origin ? expr_eval(origin) : num_clone(NUM_NAN);
        double shown_origin = num_to_double(origin_value);
        double actual_origin = riccati_value(actual, de_independent_at(de, 0u), "0");
        valid = isfinite(shown_origin) && isfinite(actual_origin) &&
                fabs(shown_origin - actual_origin) < 1e-8 * (1 + fabs(actual_origin));
        num_destroy(&origin_value);
        expr_free(origin); expr_bindings_free(origin_bindings);
    }
    if (!valid)
        printf("  Bessel display: %s\n", text ? string_c_str(text) : "NULL");
    expr_free(actual); expr_free(one); expr_free(c);
    expr_free(shown); expr_bindings_free(bindings);
    string_free(body); string_free(TeX); string_free(text);
    return valid;
}

/* Substitute into the authored right side, rather than reusing the solver's transformed potential. */
static bool riccati_residual(const diffequ_t *de, const equation_t *solution, const char *source_right, const char *coordinate,
                             const char *dependent, const char *point, bool family)
{
    const expr_t *x = de_independent_at(de, 0u), *y = equ_lhs(solution);
    const char *names[] = {coordinate, dependent};
    expr_t *symbols[] = {(expr_t *)x, (expr_t *)y};
    expr_t *right = expr_from_function_body_with_symbols(source_right, names, symbols, 2u);
    expr_t *expected = right ? expr_substitute(right, y, equ_rhs(solution)) : NULL;
    expr_t *derivative = expr_create_deriv(equ_rhs(solution), x);
    expr_t *constant = expr_new_named_var(NUM_NAN, "C");
    expr_t *constant_value = expr_new_const(NUM_ONE);
    expr_t *actual = family ? expr_substitute(derivative, constant, constant_value) : expr_clone(derivative);
    expr_t *wanted = family ? expr_substitute(expected, constant, constant_value) : expr_clone(expected);
    double got = riccati_value(actual, x, point), want = riccati_value(wanted, x, point);
    bool valid = isfinite(got) && isfinite(want) && fabs(got - want) < 1e-8 * (1.0 + fabs(want));
    printf("  residual at %s: %.12g (derivative %.12g, right side %.12g)\n", point, got - want, got, want);
    if (!valid) {
        char *debug = wanted ? expr_to_string(wanted, style_FUNCTION) : NULL;
        printf("  residual right expression: %s\n", debug ? debug : "NULL");
        free(debug);
    }
    expr_free(wanted); expr_free(actual);
    expr_free(constant_value);
    expr_free(constant);
    expr_free(derivative); expr_free(expected); expr_free(right);
    return valid;
}

void test_diffequ_riccati_families(void)
{
    static const struct {
        const char *source, *right, *x, *y;
    } cases[] = {
        {"y' - y^2 = x^2", "y^2+x^2", "x", "y"},
        {"2*y' = 2*y*y + 2*x*x", "y^2+x^2", "x", "y"},
        {"Dt(u) = u^2 + 9*t^4", "u^2+9*t^4", "t", "u"},
        {"y' = y^2 - x^3", "y^2-x^3", "x", "y"},
        {"y' = 2*y^2 + 3*y + (x^2+9/4)/2", "2*y^2+3*y+(x^2+9/4)/2", "x", "y"},
        {"y' = x*y^2-y/x+x", "x*y^2-y/x+x", "x", "y"},
        {"y' = -y^2-x^2", "-y^2-x^2", "x", "y"},
        {"y' = 2*y^2-3*y+(x^2+9/4)/2", "2*y^2-3*y+(x^2+9/4)/2", "x", "y"}
    };
    for (size_t i = 0u; i < sizeof cases / sizeof *cases; ++i) {
        printf("  %s\n", cases[i].source);
        diffequ_t *de = de_from_string(cases[i].source);
        diffequ_solve_result_t *result = de_solve_with_options(de, DE_SOLVE_OPTION_STEPS);
        bool solved = result && de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
                      de_solve_result_solver(result) == DE_SOLVER_RICCATI && de_solve_result_count(result) == 2u;
        printf("  status %d, solver %d, count %zu: %s\n", de_solve_result_status(result),
               de_solve_result_solver(result), de_solve_result_count(result), de_solve_result_diagnostic(result));
        bool verified = solved;
        for (size_t branch = 0u; verified && branch < 2u; ++branch) {
            const equation_t *solution = de_solve_result_at(result, branch);
            verified = riccati_residual(de, solution, cases[i].right, cases[i].x, cases[i].y, "-0.5", branch == 0u) &&
                       riccati_residual(de, solution, cases[i].right, cases[i].x, cases[i].y, "0.75", branch == 0u);
            if (i != 3u)
                verified = verified && riccati_bessel_display(de, solution, cases[i].x);
        }
        const char *steps = de_solve_result_steps(result), *TeX = de_solve_result_steps_TeX(result);
        bool explained = steps && strstr(steps, "auxiliary linear equation") && strstr(steps, "poles") &&
                         !strstr(steps, "1/(1)") && !strstr(steps, "(0)/(1)") && !strchr(steps, '^') &&
                         !strchr(steps, '*') && !strstr(steps, "(1) and v") &&
                         TeX && strstr(TeX, "SL(2,") && !strstr(TeX, "NAN");
        if (i == 0u) {
            printf("%s\n", steps ? steps : "NULL derivation");
            explained = explained && strstr(steps, "Set y = -v′/v.") && strstr(steps, "x²");
            string_t *display = equ_to_text(de_solve_result_at(result, 0u), style_UNBOUND);
            const char *native = display ? string_c_str(display) : NULL;
            const char *slash = native ? strchr(native, '/') : NULL;
            const char *numerator_end = native ? strstr(native, "J_{-¾}") : NULL;
            printf("One-constant quotient: %s\n", native ? native : "NULL");
            explained = explained && slash && numerator_end && numerator_end < slash &&
                        strstr(native, "; y(0) = ") && strstr(native, "Γ(¼)²") &&
                        strstr(native, "π") && strstr(native, "√") &&
                        !strstr(native, "Γ(¾)") && !strstr(native, "cosec");
            string_free(display);
        }
        if (i == 5u)
            explained = explained && strstr(steps, "x ≠ 0 and v ≠ 0");
        if (i == 6u)
            explained = explained && strstr(steps, "Set y = v′/v.");
        if (!explained)
            printf("Derivation requiring review:\n%s\n", steps ? steps : "NULL");
        de_solve_result_free(result); de_free(de);
        ASSERT_TRUE(solved);
        ASSERT_TRUE(verified);
        ASSERT_TRUE(explained);
    }
}

void test_diffequ_riccati_initial_values(void)
{
    static const struct {
        const char *source, *right, *point;
        double value;
    } cases[] = {
        {"y'=y^2+x^2; y(0)=0", "y^2+x^2", "0", 0.0},
        {"y'=y^2+x^2; y(0)=1", "y^2+x^2", "0", 1.0},
        {"y'=y^2+x^2; y(1)=2", "y^2+x^2", "1", 2.0},
        {"y'=y^2+x^2; y(-1)=2", "y^2+x^2", "-1", 2.0},
        {"y'=x*y^2-y/x+x; y(1)=2", "x*y^2-y/x+x", "1", 2.0},
        {"y'=2*y^2+3*y+(x^2+9/4)/2; y(0)=1", "2*y^2+3*y+(x^2+9/4)/2", "0", 1.0}
    };
    for (size_t i = 0u; i < sizeof cases / sizeof *cases; ++i) {
        printf("  %s\n", cases[i].source);
        diffequ_t *de = de_from_string(cases[i].source);
        diffequ_solve_result_t *result = de_solve(de);
        const equation_t *solution = de_solve_result_at(result, 0u);
        const expr_t *x = de_independent_at(de, 0u);
        double value = solution ? riccati_value(equ_rhs(solution), x, cases[i].point) : NAN;
        printf("  initial value %.12g\n", value);
        bool solved = de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
                      de_solve_result_solver(result) == DE_SOLVER_RICCATI && de_solve_result_count(result) == 1u;
        bool verified = solved && fabs(value - cases[i].value) < 1e-9 &&
                        riccati_residual(de, solution, cases[i].right, "x", "y", cases[i].point, false) &&
                        riccati_residual(de, solution, cases[i].right, "x", "y", "0.25", false) &&
                        riccati_bessel_display(de, solution, "x");
        bool across_zero = i != 0u || (isfinite(riccati_value(equ_rhs(solution), x, "-0.5")) &&
                                       isfinite(riccati_value(equ_rhs(solution), x, "0.5")));
        de_solve_result_free(result); de_free(de);
        ASSERT_TRUE(solved);
        ASSERT_TRUE(verified);
        ASSERT_TRUE(across_zero);
    }
}

void test_diffequ_riccati_scope(void)
{
    static const char *const sources[] = {
        "y'=y^2+x^2+1",
        "y'=y^2+x^2; y(0)=0; y(1)=1",
        "y'=x*y^2-y/x+x; y(0)=1",
        "y'=y^2+x^2; y(@inf)=0",
        "y'=y^2+x^2; y(0)=@inf",
        "y'=y^2/(x-1)+y/(x-1)+(x-1)*x^2; y(1)=0"
    };
    for (size_t i = 0u; i < sizeof sources / sizeof *sources; ++i) {
        printf("  %s\n", sources[i]);
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de_solve_with_options(de, DE_SOLVE_OPTION_STEPS);
        printf("  status %d, solver %d: %s\n", de_solve_result_status(result), de_solve_result_solver(result),
               de_solve_result_diagnostic(result));
        bool honest = de_solve_result_status(result) == DE_SOLVE_STATUS_UNSUPPORTED &&
                      de_solve_result_solver(result) == DE_SOLVER_RICCATI && de_solve_result_count(result) == 0u &&
                      de_solve_result_steps(result) && strstr(de_solve_result_steps(result), "auxiliary linear equation");
        de_solve_result_free(result); de_free(de);
        ASSERT_TRUE(honest);
    }
    diffequ_t *de = de_from_string("y'=y^3+x^2");
    diffequ_solve_result_t *result = de_solve(de);
    bool declined = de_solve_result_solver(result) != DE_SOLVER_RICCATI;
    de_solve_result_free(result); de_free(de);
    ASSERT_TRUE(declined);
}

/* README example: keep the input, public API calls and output synchronised with docs/diffequation.md. */
void example_diffequation_riccati(void)
{
    diffequ_t *problem = de_from_string("y' - y^2 = x^2; y(0)=0");
    diffequ_solve_result_t *result = problem ? de_solve_with_options(problem, DE_SOLVE_OPTION_STEPS) : NULL;
    bool solved = result && de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED;
    printf("Solved: %s\n", solved ? "yes" : "no");
    printf("Solutions: %zu\n", de_solve_result_count(result));
    printf("Lie system: %s\n", result ? de_solve_result_symmetry(result) : "unavailable");
    const equation_t *solution = de_solve_result_at(result, 0u);
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    printf("Solution: %s\n", text ? string_c_str(text) : "unavailable");
    const char *expected = "y = x·J_{¾}(½·x²)/J_{-¼}(½·x²); x ≠ 0; y(0) = 0";
    bool valid = solved && de_solve_result_count(result) == 1u && text && !strcmp(string_c_str(text), expected);
    string_free(text); de_solve_result_free(result); de_free(problem);
    ASSERT_TRUE(valid);
}
