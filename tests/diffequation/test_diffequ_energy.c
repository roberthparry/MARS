/**
 * @file test_diffequ_energy.c
 * @brief Verify autonomous exponential-force solutions against the original ODE and initial data.
 *
 * Exercises real energy branches, the zero-energy limit, signs, affine scales,
 * coordinate renaming and pole restrictions. Formula derivatives come from the
 * expression library. The final example mirrors docs/diffequation.md.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "string-t.h"
#include "diffequation-t.h"
#include "test_harness.h"
#include "test_diffequ_energy.h"

static bool check_formula(const equation_t *solution, const char *coordinate, double amplitude,
                           double slope, double start, bool initial, double value, double velocity)
{
    string_t *source = solution ? expr_to_text(equ_rhs(solution), style_EXPRESSION) : NULL;
    expr_bindings_t *bindings = NULL;
    expr_t *formula = source ? expr_from_text(source, &bindings) : NULL;
    expr_t *x = bindings ? expr_bindings_get(bindings, coordinate) : NULL;
    /* Enumerate the native catalogue once; direct access sets representative integration constants only. */
    for (size_t i = 0; formula && i < expr_bindings_count(bindings); ++i) {
        const char *name = expr_bindings_name_at(bindings, i);
        expr_t *binding = expr_bindings_expr_at(bindings, i);
        number_t value = expr_eval(binding);
        if (strcmp(name, coordinate) && num_is_nan(value))
            expr_set_val(binding, NUM_ONE);
        num_destroy(&value);
    }
    expr_t *first = x ? expr_create_deriv(formula, x) : NULL;
    expr_t *second = first ? expr_create_deriv(first, x) : NULL;
    bool valid = x && formula && first && second;
    const double shifts[] = {0, 0.125, -0.25};
    for (size_t i = 0; valid && i < sizeof shifts / sizeof *shifts; ++i) {
        number_t point = num_create_from_double(start + shifts[i]);
        expr_set_val(x, point); num_destroy(&point);
        number_t y = expr_eval(formula), dy = expr_eval(first), ddy = expr_eval(second);
        double actual = num_to_double(ddy), expected = amplitude * exp(slope * num_to_double(y));
        valid = num_is_real(y) && num_is_real(ddy) && num_is_finite(y) && num_is_finite(ddy) &&
                fabs(actual - expected) < 1e-8 * fmax(1, fabs(expected));
        if (initial && i == 0u)
            valid = valid && fabs(num_to_double(y) - value) < 1e-8 && fabs(num_to_double(dy) - velocity) < 1e-8;
        if (!valid)
            string_printf("Energy solution: %S; at %g: y=%g y'=%g y''=%g expected=%g\n", source,
                start + shifts[i], num_to_double(y), num_to_double(dy), actual, expected);
        num_destroy(&ddy); num_destroy(&dy); num_destroy(&y);
    }
    expr_free(second); expr_free(first); expr_free(formula); expr_bindings_free(bindings); string_free(source);
    return valid;
}

static void test_energy_real_families(void)
{
    static const struct { const char *source, *coordinate; double amplitude, slope; size_t count; } cases[] = {
        {"y''=e^y", "x", 1, 1, 3},
        {"y''=exp(2y)", "x", 1, 2, 3},
        {"y''=2exp(3y)", "x", 2, 3, 3},
        {"y''=-exp(y)", "x", -1, 1, 1},
        {"y''=-2exp(-3y)", "x", -2, -3, 3},
        {"y''=2^y", "x", 1, 0.6931471805599453094, 3},
        {"Dtt(u)=3exp(2u)", "t", 3, 2, 3},
        {"2Dtt(u)=4exp(2u)", "t", 2, 2, 3},
        {"{Dtt(u)=a*exp(b*u) | t=?; a=3,b=-2;}", "t", 3, -2, 1},
    };
    bool ok = true;
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        diffequ_t *de = de_from_string(cases[i].source);
        diffequ_solve_result_t *result = de ? de_solve_with_options(de, DE_SOLVE_OPTION_STEPS) : NULL;
        bool valid = result && de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
                     de_solve_result_solver(result) == DE_SOLVER_AUTONOMOUS_ENERGY &&
                     de_solve_result_count(result) == cases[i].count &&
                     de_solve_result_steps(result) && de_solve_result_steps_TeX(result) &&
                     !strstr(de_solve_result_steps_TeX(result), "\\partial");
        for (size_t branch = 0; valid && branch < cases[i].count; ++branch)
            valid = check_formula(de_solve_result_at(result, branch), cases[i].coordinate,
                                   cases[i].amplitude, cases[i].slope, 0, false, 0, 0);
        if (!valid) string_printf("Autonomous force input: %s\n", cases[i].source);
        ok = valid && ok;
        de_solve_result_free(result); de_free(de);
    }
    TEST_ASSERT_TRUE(ok, "all real energy branches differentiate to the original autonomous exponential force");
}

static void test_energy_initial_values(void)
{
    const struct { const char *source; double amplitude, slope, point, value, velocity; } cases[] = {
        {"y''=exp(y); y(0)=0; y'(0)=0", 1, 1, 0, 0, 0},
        {"y''=exp(y); y(0)=0; y'(0)=2", 1, 1, 0, 0, 2},
        {"y''=exp(y); y(0)=0; y'(0)=sqrt(2)", 1, 1, 0, 0, sqrt(2)},
        {"y''=exp(y); y(2)=0; y'(2)=-sqrt(2)", 1, 1, 2, 0, -sqrt(2)},
        {"y''=-exp(y); y(0)=0; y'(0)=0", -1, 1, 0, 0, 0},
        {"u''=3exp(-2u); u(1)=1; u'(1)=1", 3, -2, 1, 1, 1},
    };
    bool ok = true;
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        diffequ_t *de = de_from_string(cases[i].source);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        bool valid = result && de_solve_result_solver(result) == DE_SOLVER_AUTONOMOUS_ENERGY &&
                     de_solve_result_count(result) == 1u && check_formula(de_solve_result_at(result, 0u), "x",
                        cases[i].amplitude, cases[i].slope, cases[i].point, true, cases[i].value, cases[i].velocity);
        if (!valid) string_printf("Autonomous initial data: %s\n", cases[i].source);
        ok = valid && ok;
        de_solve_result_free(result); de_free(de);
    }
    TEST_ASSERT_TRUE(ok, "initial value and slope select the positive, negative or zero-energy local solution");
}

static void test_energy_rejections_and_poles(void)
{
    const char *sources[] = {"y''=exp(y)+y", "y''=exp(x)", "y''+y'=exp(y)",
                             "y''=exp(y); y(0)=0; y'(1)=1", "y''=exp(a*y)"};
    bool ok = true;
    for (size_t i = 0; i < sizeof sources / sizeof *sources; ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        ok = result && de_solve_result_solver(result) != DE_SOLVER_AUTONOMOUS_ENERGY && ok;
        de_solve_result_free(result); de_free(de);
    }
    diffequ_t *de = de_from_string("y''=exp(y)");
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    for (size_t branch = 0; ok && branch < 3u; branch += 2u) {
        const equation_t *solution = de_solve_result_at(result, branch);
        string_t *text = solution ? expr_to_text(equ_rhs(solution), style_EXPRESSION) : NULL;
        expr_bindings_t *bindings = NULL;
        expr_t *formula = text ? expr_from_text(text, &bindings) : NULL;
        for (size_t i = 0; formula && i < expr_bindings_count(bindings); ++i)
            expr_set_val(expr_bindings_expr_at(bindings, i), NUM_ONE);
        number_t value = formula ? expr_eval(formula) : num_clone(NUM_NAN);
        ok = formula && num_is_nan(value);
        num_destroy(&value); expr_free(formula); expr_bindings_free(bindings); string_free(text);
    }
    de_solve_result_free(result); de_free(de);
    TEST_ASSERT_TRUE(ok, "autonomous energy solving declines unmatched equations and retains logarithmic poles");
}

/* Register the ordinary checks with the owning sequential suite. */
void test_diffequ_autonomous_energy(void)
{
    TEST_RUN_SUBTEST(test_energy_real_families, NULL);
    TEST_RUN_SUBTEST(test_energy_initial_values, NULL);
    TEST_RUN_SUBTEST(test_energy_rejections_and_poles, NULL);
}

/* README example from docs/diffequation.md: complete y''=exp(y), including the zero-energy branch. */
void example_diffequation_autonomous_energy(void)
{
    static const char *expected[] = {
        "y = ln(2·C₁²/sinh²(C₁·(x - C₂))) where (x ∈ ℝ; C₂ ∈ ℝ; C₁ > 0; sinh(C₁·(x - C₂)) ≠ 0)",
        "y = ln(2·C₁²/cos²(C₁·(x - C₂))) where (x ∈ ℝ; C₂ ∈ ℝ; C₁ > 0; cos(C₁·(x - C₂)) ≠ 0)",
        "y = ln(2/(x - C₂)²) where (x ∈ ℝ; C₂ ∈ ℝ; x - C₂ ≠ 0)",
    };
    diffequ_t *problem = de_from_string("y''=exp(y)");
    diffequ_solve_result_t *result = problem ? de_solve(problem) : NULL;
    bool ok = result && de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
              de_solve_result_count(result) == 3u;
    for (size_t i = 0; ok && i < de_solve_result_count(result); ++i) {
        string_t *text = equ_to_text(de_solve_result_at(result, i), style_UNBOUND);
        ok = text && strcmp(string_c_str(text), expected[i]) == 0 && puts(string_c_str(text)) != EOF;
        string_free(text);
    }
    de_solve_result_free(result); de_free(problem);
    TEST_ASSERT_TRUE(ok, "README autonomous-force example prints all three real energy families");
}
