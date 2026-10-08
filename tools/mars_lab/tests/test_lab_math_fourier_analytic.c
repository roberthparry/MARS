/**
 * @file test_lab_math_fourier_analytic.c
 * @brief Native analytic-functional Fourier regression port.
 *
 * Preserves every original case, parameter grid, reference calculation and
 * tolerance. README examples are registered separately after ordinary tests.
 */
#include <complex.h>
#include <math.h>
#include <string.h>

#include "test_lab_math_support.h"

static const json_t *math_fourier_fields(const char *source, const char *variable)
{
    return lab_math_fields(source, variable, "evaluate", 40);
}

static const char *math_fourier_text(const json_t *result, const char *key)
{
    return lab_math_text(result, key);
}

static double complex math_fourier_number(const json_t *result)
{
    return lab_math_number(result, "value");
}


static const char *math_fourier_spectrum(const char *body, bool inverse)
{
    return lab_math_algebra(math_fourier_fields(lab_math_format("%s{%s}", inverse ? "@Finv" : "@F", body), inverse ? "t" : "ω"));
}

static void test_requested_sinh_pair_is_visible_and_copies(void)
{
    const json_t *r = math_fourier_fields("@F{sinh(t)}", "ω");
    lab_math_contains(math_fourier_text(r, "tex"), "\\delta(", true);
    lab_math_contains(math_fourier_text(r, "tex"), "\\delta_", false);
    lab_math_contains(math_fourier_text(r, "expression"), "analytic_delta(", true);
    lab_math_contains(math_fourier_text(r, "function"), "analytic_delta(", true);
    lab_math_contains(math_fourier_text(r, "value_note"), "Extended Fourier transform", true);
    lab_math_equal(math_fourier_text(math_fourier_fields(math_fourier_text(r, "expression"), "ω"), "tex"),
        math_fourier_text(r, "tex"));
    lab_math_contains(math_fourier_text(r, "tex"), "\\left(i\\right)", false);
    lab_math_contains(math_fourier_text(r, "expression"), "(i)", false);
    r = math_fourier_fields(lab_math_format("@Finv{%s}", math_fourier_spectrum("sinh(t)", false)), "t");
    lab_math_contains(math_fourier_text(r, "expression"), "sinh(t)", true);
    lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
}

static void test_finite_exponential_families_round_trip_both_directions(void)
{
    const char *cases[] = {"sinh(z)", "cosh(z)", "sinh(2*z+1)", "cosh(-2*z+1)", "sinh(z)^2",
        "cosh(z)^3", "sech(z)^(-2)", "cosech(z)^(-2)", "exp(z)", "exp(-2*z+1)", "exp((1+i)*z)", "3*sinh(z)"};
    const double points[] = {-0.7, 0.4};
    for (size_t d = 0; d < 2; ++d)
        for (size_t c = 0; c < 12; ++c) {
            const char *source = d ? "ω" : "t";
            const char *body = lab_math_replace(cases[c], "z", source);
            const char *copy = math_fourier_spectrum(body, d);
            lab_math_contains(copy, "analytic_delta", true);
            for (size_t p = 0; p < 2; ++p) {
                const json_t *r = math_fourier_fields(lab_math_format("{%s{%s} | %s=%.17g}",
                    d ? "@F" : "@Finv", copy, source, points[p]), source);
                const json_t *expected = math_fourier_fields(lab_math_format("{%s | %s=%.17g}", body, source, points[p]), source);
                lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
                lab_math_close(math_fourier_number(r), math_fourier_number(expected), 1e-11);
            }
        }
}

static void test_evaluation_action_and_duality(void)
{
    const char *aliases[] = {"analytic_delta", "AnalyticDelta", "δℂ"};
    const double points[] = {-0.6, 0, 0.7};
    for (size_t a = 0; a < 3; ++a)
        for (size_t p = 0; p < 3; ++p) {
            const json_t *forward = math_fourier_fields(lab_math_format("{@F{%s(t-i)} | ω=%.17g}", aliases[a], points[p]), "ω");
            const json_t *inverse = math_fourier_fields(lab_math_format("{@Finv{%s(ω-i)} | t=%.17g}", aliases[a],
                points[p]), "t");
            lab_math_places(creal(math_fourier_number(forward)), exp(points[p]), 7);
            lab_math_places(creal(math_fourier_number(inverse)), exp(-points[p])/(2*M_PI), 7);
        }
    const json_t *r = math_fourier_fields(lab_math_format("{@F{%s,ω,t} | t=0.4}", math_fourier_spectrum("sinh(t)", false)), "t");
    lab_math_places(creal(math_fourier_number(r)), -2*M_PI*sinh(0.4), 7);
}

static void test_no_pointwise_value_even_at_real_regular_points(void)
{
    const char *bodies[] = {"@F{sinh(t)}", "analytic_delta(ω+i)", "analytic_delta(ω)", "analytic_delta(ω-i)",
        "Derivative(analytic_delta(ω+i),ω,1)"};
    const int points[] = {0, 1, -1};
    for (size_t b = 0; b < 5; ++b)
        for (size_t p = 0; p < 3; ++p) {
            const json_t *r = math_fourier_fields(lab_math_format("{%s | ω=%d}", bodies[b], points[p]), "ω");
            lab_math_equal(math_fourier_text(r, "value"), "NAN");
            lab_math_contains(math_fourier_text(r, "value_note"), "analytic", true);
        }
    lab_math_check(math_fourier_number(math_fourier_fields("{delta(ω) | ω=1}", "ω")) == 0, "ordinary off-support impulse");
}

static void test_no_unsupported_delta_scaling_or_non_entire_extension(void)
{
    const char *bodies[] = {"analytic_delta(2*ω+i)", "analytic_delta(ω^2+i)", "delta(ω+i)"};
    for (size_t b = 0; b < 3; ++b) {
        const json_t *r = math_fourier_fields(lab_math_format("@Finv{%s}", bodies[b]), "t");
        lab_math_check(strstr(math_fourier_text(r, "function"), "fourier(") || !strcmp(math_fourier_text(r, "value"),
            "NAN"), bodies[b]);
    }
    const char *growth[] = {"sinh(t)^(1/2)", "sinh(t)^(1+i)", "exp(t^2)"};
    for (size_t b = 0; b < 3; ++b)
        lab_math_contains(math_fourier_text(math_fourier_fields(lab_math_format("@F{%s}", growth[b]), "ω"),
            "expression"), "analytic_delta", false);
}

static void test_symbolic_affine_parameters_and_cancellation(void)
{
    const char *copy = math_fourier_spectrum("sinh(a*t+b)", false);
    const int parameters[][2] = {{2, 1}, {-2, -1}, {0, 1}};
    for (size_t p = 0; p < 3; ++p) {
        const json_t *r = math_fourier_fields(lab_math_format("{@Finv{%s} | t=0.4; a=%d; b=%d}",
                                                copy, parameters[p][0], parameters[p][1]), "t");
        lab_math_places(creal(math_fourier_number(r)), sinh(parameters[p][0]*0.4+parameters[p][1]), 7);
    }
    lab_math_check(math_fourier_number(math_fourier_fields("{Fourier(c*sinh(t),t,ω) | ω=1; c=0}", "ω")) == 0, "zero multiplier");
}

static void test_polynomial_multiplier_and_derivative_round_trip(void)
{
    const json_t *r = math_fourier_fields(lab_math_format("{@Finv{%s} | t=0.4}", math_fourier_spectrum("t*sinh(t)", false)), "t");
    lab_math_places(creal(math_fourier_number(r)), 0.4*sinh(0.4), 7);
}

static void test_imaginary_shifts_do_not_keep_factor_parentheses(void)
{
    const char *bodies[] = {"sinh(t)", "sinh(2*t)", "cosh(3*t)", "exp(-2*t)"};
    for (size_t i = 0; i < 4; ++i) {
        const json_t *r = math_fourier_fields(lab_math_format("@F{%s}", bodies[i]), "ω");
        lab_math_regex(math_fourier_text(r, "tex"), "\\\\left\\([123]?i\\\\right\\)", false);
        lab_math_regex(math_fourier_text(r, "expression"), "\\([123]?i\\)", false);
        lab_math_equal(math_fourier_text(math_fourier_fields(math_fourier_text(r, "expression"), "ω"), "tex"),
            math_fourier_text(r, "tex"));
    }
}

static void test_required_complex_grouping_preserves_values(void)
{
    const char *bodies[] = {"x-(2+3*i)", "x*(-2*i)", "x/(2*i)", "(-2*i)^2*x", "x+(-2*i)", "x-(-2*i)"};
    const double complex expected[] = {-1-3*I, -2*I, -0.5*I, -4, 1-2*I, 1+2*I};
    for (size_t i = 0; i < 6; ++i) {
        const json_t *r = math_fourier_fields(lab_math_format("{%s | x=1}", bodies[i]), "x");
        lab_math_close(math_fourier_number(r), expected[i], 1e-14);
        lab_math_close(math_fourier_number(math_fourier_fields(math_fourier_text(r, "expression"), "x")), expected[i], 1e-14);
    }
}

static void test_readme_analytic_sinh(void)
{
    /* README examples: docs/expression.md, analytic-functional sinh pair. */
    lab_math_contains(math_fourier_text(math_fourier_fields("@F{sinh(t)}", "ω"), "expression"), "analytic_delta", true);
    const json_t *r = math_fourier_fields("@Finv{@pi*(analytic_delta(ω+i)-analytic_delta(ω-i))}", "t");
    lab_math_contains(math_fourier_text(r, "expression"), "sinh(t)", true);
    lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
}

/* Register ordinary regressions. */
void test_lab_math_fourier_analytic_cases(void)
{
    TEST_RUN_IN_GROUP(test_requested_sinh_pair_is_visible_and_copies, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_finite_exponential_families_round_trip_both_directions, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_evaluation_action_and_duality, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_no_pointwise_value_even_at_real_regular_points, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_no_unsupported_delta_scaling_or_non_entire_extension, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_symbolic_affine_parameters_and_cancellation, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_polynomial_multiplier_and_derivative_round_trip, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_imaginary_shifts_do_not_keep_factor_parentheses, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_required_complex_grouping_preserves_values, tests, NULL);
    lab_math_reset();
}

/* Register README examples last. */
void test_lab_math_fourier_analytic_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_readme_analytic_sinh, readme_examples, "math,readme,output");
    lab_math_reset();
}
