/**
 * @file test_diffequ_emden.c
 * @brief Verify independent-coefficient quadratures and exceptional branches against the original equation.
 *
 * Differentiates the returned parametric coordinates using native automatic differentiation, rather than
 * fitting a curve or assuming point-linearisability. Parser association, fresh symbols and the existing
 * eight-symmetry special case have separate checks. The documented executable example runs last.
 */
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "string-t.h"
#include "diffequation-t.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#include "test_harness.h"
#include "test_diffequ_emden.h"

static void test_emden_trajectory_samples(void)
{
    const char *sources[] = {"y'=y", "y''+y=0", "x''+3*x*x'+x^3=0", "x''+x*x'+x^3=0"};
    for (size_t test = 0; test < 4u; ++test) {
        diffequ_t *de = de_from_string(sources[test]);
        sampler_options_t options = sampler_options_default();
        options.intervals = 128u; options.absolute_tolerance = 1e-9; options.relative_tolerance = 1e-8;
        number_t lo = num_create_from_long(-1), hi = num_create_from_long(1);
        sampler_t *samples = de_sample_trajectory(de, NUM_ZERO, test ? NUM_ZERO : NUM_ONE,
                                                  NUM_ONE, lo, hi, &options);
        bool ok = samples && sampler_count(samples) == 129u;
        for (size_t i = 0; ok && i < sampler_count(samples); ++i) {
            const sampler_point_t *point = sampler_point(samples, i);
            double t = num_to_double(point->x), value = num_to_double(point->y);
            double expected = test == 0 ? exp(t) : test == 1 ? sin(t) : t / (1 + t * t / 2);
            ok = point->valid && isfinite(value) && (test == 3u || fabs(value - expected) < 2e-7);
            if (ok && test == 3u && i > 1u && i + 2u < sampler_count(samples)) {
                double previous = num_to_double(sampler_point(samples, i - 1u)->y);
                double next = num_to_double(sampler_point(samples, i + 1u)->y);
                double h = 2.0 / options.intervals;
                double residual = (next - 2 * value + previous) / (h * h) +
                                  value * (next - previous) / (2 * h) + value * value * value;
                ok = fabs(residual) < 0.002;
            }
        }
        if (!ok) fprintf(stderr, "Trajectory failed for %s: errno %d\n", sources[test], errno);
        TEST_ASSERT_TRUE(ok, "native trajectory sampling matches analytic references or the independent-coefficient ODE");
        sampler_free(samples); num_destroy(&hi); num_destroy(&lo); de_free(de);
    }
    diffequ_t *pole = de_from_string("y'=y^2");
    number_t lo = num_create_from_long(-1), hi = num_create_from_long(2);
    sampler_options_t settings = sampler_options_default(); settings.intervals = 96u;
    sampler_t *samples = de_sample_trajectory(pole, NUM_ZERO, NUM_ONE, NUM_NAN, lo, hi, &settings);
    bool ok = samples != NULL, before = false, after = false;
    for (size_t i = 0; ok && i < sampler_count(samples); ++i) {
        const sampler_point_t *p = sampler_point(samples, i);
        double t = num_to_double(p->x);
        if (t < 0.5) before |= p->valid;
        if (t > 1.01) { after = true; ok = !p->valid; }
    }
    TEST_ASSERT_TRUE(ok && before && after, "trajectories stop at a pole and do not resume beyond it");
    sampler_free(samples);
    settings.max_evaluations = 100u;
    samples = de_sample_trajectory(pole, NUM_ZERO, NUM_ONE, NUM_NAN, lo, hi, &settings);
    TEST_ASSERT_TRUE(!samples && errno == E2BIG, "native trajectory work limits fail without publishing partial samples");
    sampler_free(samples); num_destroy(&hi); num_destroy(&lo); de_free(pole);
}

static void test_emden_independent_coefficients(void)
{
    const char *sources[] = {
        "x'' + alpha xx' + beta x^3 = 0", "y''+a*y*y'+b*y^3=0", "2*z''+a*z*z'+b*z^3=0",
        "{z''+p*z*z'+q*z^3=0 | s=?; p=?,q=?;}", "r''+a*r*r'+b*r^3=0",
        "x''+r*x*x'+r0*x^3=0",
        "{d²x/dt²+alpha*x*dx/dt+beta*x³=0 | t=?}",
        "{x''+alpha*x*x'+beta*x^3=0 | t=?; alpha=?,beta=?}",
        "{x''+alpha*x*x'+beta*x^3=0 | ; alpha=?,beta=?;}",
    };
    for (size_t i = 0; i < sizeof sources / sizeof *sources; ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve_with_context(de, DE_SOLVE_OPTION_STEPS, NULL) : NULL;
        const expr_t *parameter = de_solve_result_quadrature_parameter(result);
        const expr_t *reference = de_solve_result_quadrature_reference(result);
        bool ok = result && de_solve_result_status(result) == DE_SOLVE_STATUS_QUADRATURE &&
            de_solve_result_solver(result) == DE_SOLVER_EMDEN_QUADRATURE && de_solve_result_count(result) == 6u &&
            parameter && reference && de_solve_result_steps(result) &&
            strstr(de_solve_result_steps(result), "two integration constants") &&
            strstr(de_solve_result_steps(result), "it is not assumed");
        for (size_t j = 0; ok && j < de_solve_result_count(result); ++j) {
            string_t *text = equ_to_text(de_solve_result_at(result, j), style_UNBOUND);
            string_t *notation = equ_to_text(de_solve_result_at(result, j), style_LATEX);
            ok = text && notation && !strstr(string_c_str(text), "NAN") && !strstr(string_c_str(notation), "NAN") &&
                (j >= 2u || (!strstr(string_c_str(notation), "\\middle|") &&
                              !strstr(string_c_str(notation), "= ?")));
            string_free(notation); string_free(text);
        }
        if (!ok) fprintf(stderr, "Independent-coefficient quadrature failed: %s\n", sources[i]);
        TEST_ASSERT_TRUE(ok, "independent coefficients, product association and renamed coordinates retain exact quadratures");
        de_solve_result_free(result); de_free(de);
    }
}

static void test_emden_parametric_residual(void)
{
    size_t precision = num_get_default_prec_bits();
    num_set_default_prec_bits(96u);
    diffequ_t *de = de_from_string("x''+x*x'+x^3=0");
    diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
    const equation_t *position_equation = de_solve_result_at(result, 0u), *time_equation = de_solve_result_at(result, 1u);
    const expr_t *position = position_equation ? equ_rhs(position_equation) : NULL;
    const expr_t *clock = time_equation ? equ_rhs(time_equation) : NULL;
    expr_t *pair = position && clock ? expr_add(position, clock) : NULL;
    expr_bindings_t *bindings = pair ? expr_bindings_from_expr_internal(pair) : NULL;
    const expr_t *parameter = de_solve_result_quadrature_parameter(result);
    const expr_t *reference = de_solve_result_quadrature_reference(result);
    expr_t *amplitude = expr_bindings_get(bindings, "C1"), *origin = expr_bindings_get(bindings, "C2");
    bool ok = parameter && reference && amplitude && origin;
    expr_t *dx = ok ? expr_create_deriv(position, parameter) : NULL;
    expr_t *dt = ok ? expr_create_deriv(clock, parameter) : NULL;
    expr_t *velocity = dx && dt ? expr_div(dx, dt) : NULL;
    expr_t *dv = velocity ? expr_create_deriv(velocity, parameter) : NULL;
    expr_t *acceleration = dv && dt ? expr_div(dv, dt) : NULL;
    expr_t *mixed = velocity ? expr_mul(position, velocity) : NULL, *cubic = position ? expr_pow_long(position, 3) : NULL;
    expr_t *partial = acceleration && mixed ? expr_add(acceleration, mixed) : NULL;
    expr_t *residual = partial && cubic ? expr_add(partial, cubic) : NULL;
    /* Context snapshots intentionally leave unresolved calculus scopes untouched. Substitute fixed
     * parameters explicitly into the native graph, including their occurrences within integrals. */
    expr_t *zero = expr_const_zero(), *two = expr_const_long(2);
    /* Variable needles intentionally select every same-named cloned parameter leaf. */
    expr_t *ref_name = reference ? expr_new_named_var(NUM_NAN, expr_symbol_name(reference)) : NULL;
    expr_t *amplitude_name = expr_new_named_var(NUM_NAN, "C1"), *origin_name = expr_new_named_var(NUM_NAN, "C2");
    expr_t *fixed_reference = residual && ref_name ? expr_substitute(residual, ref_name, zero) : NULL;
    expr_t *fixed_amplitude = fixed_reference ? expr_substitute(fixed_reference, amplitude_name, two) : NULL;
    expr_t *fixed = fixed_amplitude ? expr_substitute(fixed_amplitude, origin_name, zero) : NULL;
    number_t tolerance = num_create_from_string("1e-18");
    const long numerators[] = {-1, 0, 1, 2};
    for (size_t i = 0; ok && fixed && i < 4u; ++i) {
        number_t point = num_create_from_frac(numerators[i], 4);
        expr_t *literal = expr_new_const(point), *sample = literal ? expr_substitute(fixed, parameter, literal) : NULL;
        number_t value = sample ? expr_eval(sample) : NUM_NAN, magnitude = num_abs(value);
        ok = ok && num_is_real(value) && num_is_finite(value) && num_lt(magnitude, tolerance);
        if (!ok) {
            fprintf(stderr, "Parametric ODE residual at r=%g: %g\n", num_to_double(point), num_to_double(value));
            string_t *debug = expr_to_text(sample, style_UNBOUND);
            fprintf(stderr, "%s\n", debug ? string_c_str(debug) : "no residual");
            string_free(debug);
        }
        expr_free(sample); expr_free(literal);
        num_destroy(&magnitude); num_destroy(&value); num_destroy(&point);
    }
    ok = ok && fixed;
    TEST_ASSERT_TRUE(ok, "automatic derivatives of both parametric quadratures satisfy x''+x*x'+x^3=0");
    num_destroy(&tolerance);
    expr_free(fixed); expr_free(fixed_amplitude); expr_free(fixed_reference); expr_free(two); expr_free(zero);
    expr_free(origin_name); expr_free(amplitude_name); expr_free(ref_name);
    expr_free(residual); expr_free(partial); expr_free(cubic); expr_free(mixed); expr_free(acceleration);
    expr_free(dv); expr_free(velocity); expr_free(dt); expr_free(dx);
    expr_bindings_free(bindings); expr_free(pair); de_solve_result_free(result); de_free(de);
    num_set_default_prec_bits(precision);
}

static void test_emden_exceptional_families(void)
{
    static const struct { const char *source; size_t count; } cases[] = {
        {"x''+x*x'+x^3=0", 3u}, {"x''+4*x*x'+2*x^3=0", 4u},
        {"x''+x*x'=0", 5u}, {"x''-4*x*x'+x^3=0", 5u},
    };
    for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
        diffequ_t *de = de_from_string(cases[i].source);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        bool ok = result && de_solve_result_status(result) == DE_SOLVE_STATUS_QUADRATURE &&
            de_solve_result_count(result) == cases[i].count;
        const equation_t *equilibrium = de_solve_result_at(result, 2u);
        number_t value = equilibrium ? expr_eval(equ_rhs(equilibrium)) : NUM_NAN;
        ok = ok && num_is_zero(value);
        if (!ok) fprintf(stderr, "Exceptional family count: %s -> %zu\n", cases[i].source, de_solve_result_count(result));
        TEST_ASSERT_TRUE(ok, "zero, repeated real ratios, non-real ratios and zero cubic coefficients "
                             "retain their proper branches");
        num_destroy(&value); de_solve_result_free(result); de_free(de);
    }
}

static void test_emden_point_case_and_scope(void)
{
    const char *sources[] = {
        "x''+a*x*x'+a^2/9*x^3=0", "y''+(a*y)*y'+(a*a/9)*y^3=0",
        "d²x/dt² + alpha x*dx/dt + alpha^2/9 x³ = 0",
        "x'' + alpha*x*x' + alpha^2*x^3/9 = 0",
        "d²x/dt² + α*x*dx/dt + α²*x³/9 = 0",
        "{d²x/dt² + alpha x*dx/dt + alpha^2/9 x³ = 0 | t = ?}",
    };
    for (size_t i = 0; i < sizeof sources / sizeof *sources; ++i) {
        diffequ_t *de = de_from_string(sources[i]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        bool ok = result && de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
            de_solve_result_solver(result) == DE_SOLVER_LINEAR_TRANSFORMATION &&
            !de_solve_result_quadrature_parameter(result) && !de_solve_result_quadrature_reference(result);
        TEST_ASSERT_TRUE(ok, "the symbolic eight-symmetry coefficient relation still selects the simpler rational solution");
        de_solve_result_free(result); de_free(de);
    }
    const char *outside[] = {"y''+a*y*y'+b*y^3+y=0", "y''+x*y*y'+y^3=0", "y''+y*y'+y^3=1"};
    for (size_t i = 0; i < 3u; ++i) {
        diffequ_t *de = de_from_string(outside[i]);
        diffequ_solve_result_t *result = de ? de_solve(de) : NULL;
        TEST_ASSERT_TRUE(!result || de_solve_result_solver(result) != DE_SOLVER_EMDEN_QUADRATURE,
                         "forcing, extra powers and coordinate-dependent coefficients are not misclassified");
        de_solve_result_free(result); de_free(de);
    }
    TEST_ASSERT_TRUE(!de_solve_result_quadrature_parameter(NULL) && !de_solve_result_quadrature_reference(NULL),
                     "quadrature metadata accessors accept NULL");
}

/* Register ordinary mathematical checks before the documentation example. */
void test_diffequ_emden_quadratures(void)
{
    TEST_RUN_SUBTEST(test_emden_trajectory_samples, NULL);
    TEST_RUN_SUBTEST(test_emden_independent_coefficients, NULL);
    TEST_RUN_SUBTEST(test_emden_parametric_residual, NULL);
    TEST_RUN_SUBTEST(test_emden_exceptional_families, NULL);
    TEST_RUN_SUBTEST(test_emden_point_case_and_scope, NULL);
}

/* README example from docs/diffequation.md: inspect the exact two-equation parametric family. */
void example_diffequation_emden_quadrature(void)
{
    diffequ_t *problem = de_from_string("x''+alpha*x*x'+beta*x^3=0");
    diffequ_solve_result_t *result = problem ? de_solve(problem) : NULL;
    const expr_t *parameter = de_solve_result_quadrature_parameter(result);
    const expr_t *reference = de_solve_result_quadrature_reference(result);
    bool ok = result && de_solve_result_status(result) == DE_SOLVE_STATUS_QUADRATURE && parameter && reference &&
              de_solve_result_count(result) == 6u;
    if (ok) {
        printf("Parameter: %s\nFixed reference: %s\n", expr_symbol_name(parameter), expr_symbol_name(reference));
        puts("Parametric coordinate equations: 2");
        printf("Exceptional families: %zu\n", de_solve_result_count(result) - 2u);
    }
    de_solve_result_free(result); de_free(problem);
    TEST_ASSERT_TRUE(ok, "README independent-coefficient example distinguishes the parameter, reference "
                         "and exceptional branches");
}

/* README example from docs/diffequation.md: the authored coefficient relation selects the rational family. */
void example_diffequation_symbolic_emden(void)
{
    const char *source = "d²x/dt² + alpha x*dx/dt + alpha^2/9 x³ = 0";
    diffequ_t *problem = de_from_string(source);
    diffequ_solve_result_t *result = problem ? de_solve(problem) : NULL;
    const equation_t *solution = de_solve_result_at(result, 0u);
    string_t *text = solution ? equ_to_text(solution, style_UNBOUND) : NULL;
    bool ok = text && de_solve_result_status(result) == DE_SOLVE_STATUS_SOLVED &&
        de_solve_result_solver(result) == DE_SOLVER_LINEAR_TRANSFORMATION &&
        !strcmp(string_c_str(text), "x = (2t + C₁)/(⅓α·(t² + C₁t + C₂))");
    if (ok) {
        printf("input = %s\nsolver = linear transformation\n", source);
        printf("solution = %s\n", string_c_str(text));
    }
    string_free(text); de_solve_result_free(result); de_free(problem);
    TEST_ASSERT_TRUE(ok, "README symbolic modified-Emden equation selects the rational solution directly");
}

/* README example from docs/diffequation.md: sample the original ODE from specified initial data. */
void example_diffequation_trajectory(void)
{
    diffequ_t *problem = de_from_string("x''+3*x*x'+x^3=0");
    sampler_options_t options = sampler_options_default();
    options.intervals = 128;
    options.absolute_tolerance = 1e-9;
    options.relative_tolerance = 1e-8;
    number_t lower = num_create_from_long(-1);
    sampler_t *samples = de_sample_trajectory(problem, NUM_ZERO, NUM_ZERO, NUM_ONE, lower, NUM_ONE, &options);
    const sampler_point_t *last = samples ? sampler_point(samples, sampler_count(samples) - 1) : NULL;
    if (last && last->valid) {
        printf("Samples: %zu\nx(1): %.6f\n", sampler_count(samples), num_to_double(last->y));
    }
    bool ok = last && last->valid && fabs(num_to_double(last->y) - 2.0 / 3) < 1e-7;
    sampler_free(samples);
    num_destroy(&lower);
    de_free(problem);
    TEST_ASSERT_TRUE(ok, "README trajectory has 129 samples and x(1) = 0.666667");
}
