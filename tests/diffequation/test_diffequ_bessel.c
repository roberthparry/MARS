/**
 * @file test_diffequ_bessel.c
 * @brief Verify scaled Bessel solutions against authored differential equations.
 *
 * Database cases exercise ordinary and modified bases, half-order elementary
 * reductions, Euler limits, exact coefficients, point data and excluded domains.
 * Each independent basis is differentiated and substituted into the original
 * equation on both real half-axes. The final example mirrors docs/diffequation.md.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "string-t.h"
#include "diffequation-t.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

#include "test_harness.h"
#include "test_database.h"
#include "test_diffequ_bessel.h"

static bool bessel_near_zero(const expr_t *expression)
{
    number_t value = expression ? expr_eval(expression) : num_clone(NUM_NAN);
    bool ok = num_is_finite(value) && num_is_real(value) && fabs(num_to_double(value)) < 1e-12;
    if (!ok) {
        expr_printf("Residual: %nu\n", expression);
        printf("Residual value: %.17g\n", num_to_double(value));
    }
    num_destroy(&value);
    return ok;
}

static bool bessel_check_basis(const diffequ_t *problem, const equation_t *solution, size_t free_constants)
{
    const expr_t *coordinate = de_independent_at(problem, 0u);
    expr_t *residual = equ_residual(de_equation(problem));
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(equ_rhs(solution));
    expr_t *first = bindings ? expr_bindings_get(bindings, "C1") : NULL;
    expr_t *second = bindings ? expr_bindings_get(bindings, "C2") : NULL;
    bool ok = residual && bindings && (first != NULL) == (free_constants >= 1u) &&
              (second != NULL) == (free_constants == 2u);
    if (!ok) {
        printf("Bindings: expected %zu constants; C1=%p C2=%p\n", free_constants, (void *)first, (void *)second);
        for (size_t i = 0u; i < expr_bindings_count(bindings); ++i)
            printf("  %s\n", expr_bindings_name_at(bindings, i));
    }
    expr_t *first_needle = first ? expr_new_named_var(NUM_NAN, expr_name_or_default(first, "C1")) : NULL;
    expr_t *second_needle = second ? expr_new_named_var(NUM_NAN, expr_name_or_default(second, "C2")) : NULL;
    const double points[] = {-2.25, -0.75, 0.75, 2.25};
    int side = 0;
    if (de_condition_count(problem)) {
        number_t point = expr_eval(de_condition_argument_at(problem, 0u, 0u));
        side = num_get_sign(point);
        num_destroy(&point);
    }
    for (size_t basis = 0u; ok && basis < (free_constants == 2u ? 2u : 1u); ++basis) {
        expr_t *one = expr_const_one(), *zero = expr_const_zero();
        expr_t *formula = first ? expr_substitute(equ_rhs(solution), first_needle, basis == 0u ? one : zero)
                                : expr_clone(equ_rhs(solution));
        if (second) {
            expr_t *next = expr_substitute(formula, second_needle, basis == 1u ? one : zero);
            expr_free(formula);
            formula = next;
        }
        expr_t *substituted = formula ? expr_substitute(residual, equ_lhs(solution), formula) : NULL;
        for (size_t i = 0u; ok && i < sizeof points / sizeof *points; ++i) {
            if (side && side * points[i] < 0.0)
                continue;
            number_t value = num_create_from_double(points[i]);
            expr_t *point = expr_new_const(value);
            expr_t *at = point ? expr_substitute(substituted, coordinate, point) : NULL;
            expr_t *value_at = point ? expr_substitute(formula, coordinate, point) : NULL;
            number_t evaluated = value_at ? expr_eval(value_at) : num_clone(NUM_NAN);
            ok = num_is_finite(evaluated) && num_is_real(evaluated) && bessel_near_zero(at);
            if (!ok) expr_printf("Formula at point: %nu\n", value_at);
            num_destroy(&evaluated); num_destroy(&value);
            expr_free(value_at); expr_free(at); expr_free(point);
        }
        for (size_t i = 0u; ok && i < de_condition_count(problem); ++i) {
            const equation_t *condition = de_condition_at(problem, i);
            expr_t *left = expr_substitute(equ_lhs(condition), equ_lhs(solution), formula);
            expr_t *at = left ? expr_substitute(left, coordinate, de_condition_argument_at(problem, i, 0u)) : NULL;
            expr_t *difference = at ? expr_sub(at, equ_rhs(condition)) : NULL;
            ok = bessel_near_zero(difference);
            expr_free(difference); expr_free(at); expr_free(left);
        }
        expr_t *origin = formula ? expr_substitute(formula, coordinate, zero) : NULL;
        number_t at_origin = origin ? expr_eval(origin) : num_clone(NUM_NAN);
        ok = ok && num_is_nan(at_origin);
        num_destroy(&at_origin); expr_free(origin);
        if (side) {
            expr_t *opposite = expr_const_long(side > 0 ? -1L : 1L);
            expr_t *outside = formula ? expr_substitute(formula, coordinate, opposite) : NULL;
            number_t value = outside ? expr_eval(outside) : num_clone(NUM_NAN);
            ok = ok && num_is_nan(value);
            num_destroy(&value); expr_free(outside); expr_free(opposite);
        }
        expr_free(substituted); expr_free(formula); expr_free(zero); expr_free(one);
    }
    expr_free(second_needle); expr_free(first_needle); expr_bindings_free(bindings); expr_free(residual);
    return ok;
}

static void bessel_case(const void *data)
{
    const test_database_case_t *row = data;
    size_t saved_precision = num_get_default_prec_bits();
    num_set_default_prec_digits(row->precision);
    diffequ_t *problem = de_from_string(string_c_str(row->input));
    diffequ_solve_result_t *result = problem ? de_solve_with_options(problem, DE_SOLVE_OPTION_STEPS) : NULL;
    bool rejection = !strcmp(string_c_str(row->expected), "reject");
    bool ok = result && (rejection ? de_solve_result_solver(result) != DE_SOLVER_BESSEL :
        de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
        de_solve_result_solver(result) == DE_SOLVER_BESSEL && de_solve_result_count(result) == 1u);
    if (ok && !rejection) {
        size_t constants = 0u;
        char first[64] = {0}, second[64] = {0};
        ok = sscanf(string_c_str(row->expected), "%zu|%63[^|]|%63[^|]", &constants, first, second) == 3;
        const equation_t *solution = de_solve_result_at(result, 0u);
        string_t *text = equ_to_text(solution, style_UNBOUND);
        const char *steps = de_solve_result_steps_TeX(result);
        ok = ok && text && strstr(string_c_str(text), first) && strstr(string_c_str(text), second) &&
             steps && strstr(steps, "nu") && !strstr(steps, "NAN") &&
             bessel_check_basis(problem, solution, constants);
        if (!ok) printf("Solution: %s\n", text ? string_c_str(text) : "(none)");
        string_free(text);
    }
    if (!ok) printf("Bessel input: %s\nDiagnostic: %s\n", string_c_str(row->input),
                    result ? de_solve_result_diagnostic(result) : "(none)");
    de_solve_result_free(result); de_free(problem);
    num_set_default_prec_bits(saved_precision);
    TEST_ASSERT_TRUE(ok, "native Bessel bases satisfy the original ODE, point data and solver scope");
}

/* Register the stored ordinary cases with the owning sequential suite. */
void test_diffequ_bessel(void)
{
    test_run_database_cases(__FILE__, __LINE__, "diffequation.bessel", "test_diffequ_bessel", bessel_case);
}

/* README example from docs/diffequation.md: the half-order equation has an elementary real basis. */
void example_diffequation_bessel(void)
{
    diffequ_t *problem = de_from_string("x^2y'' + xy' + (x^2 - 1/4)y = 0");
    diffequ_solve_result_t *result = problem ? de_solve(problem) : NULL;
    string_t *text = result ? equ_to_text(de_solve_result_at(result, 0u), style_UNBOUND) : NULL;
    int status = EXIT_FAILURE;
    if (text && puts(string_c_str(text)) != EOF)
        status = EXIT_SUCCESS;
    bool ok = status == EXIT_SUCCESS && !strcmp(string_c_str(text),
        "y = (C₁·cos(x) + C₂·sin(x))·1/√(|x|) where (x ∈ ℝ; x ≠ 0)");
    string_free(text);
    de_solve_result_free(result);
    de_free(problem);
    TEST_ASSERT_TRUE(ok, "README Bessel example prints the native elementary solution");
}
