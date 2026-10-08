/**
 * @file test_lab_math_inverse_general.c
 * @brief General unilateral inverse Laplace regressions and documented logarithm pair.
 *
 * Preserves all fractional, rational, finite-sum, hyperbolic and causal-delay
 * cases from test_inverse_laplace_general.py. Independent C references check
 * freshly parsed output at every original time. The README example has its
 * own registration entry so the parent can run it after all ordinary tests.
 */
#include <math.h>

#include "test_lab_math_support.h"

typedef double (*reference_t)(double);
static double power;
static double scale;
static double offset;
static int order;
static double (*hyperbolic)(double);

static const json_t *math_inverse_fields(const char *source)
{
    return lab_math_fields(source, "t", "evaluate", 40);
}

static const json_t *math_inverse_forward(const char *source)
{
    const json_t *result = lab_math_fields(lab_math_format("Laplace(%s,t,s)", source), "s", "evaluate", 40);
    lab_math_contains(lab_math_text(result, "function"), "laplace(", false);
    return result;
}

static void math_inverse_check_inverse(const char *spectrum, reference_t reference, const double *points)
{
    const double defaults[] = {0.2, 0.7, 1.6};
    if (!points)
        points = defaults;
    const json_t *inverse = math_inverse_fields(lab_math_format("InverseLaplace(%s,s,t)", spectrum));
    lab_math_contains(lab_math_text(inverse, "function"), "inverselaplace(", false);
    const char *restored = lab_math_algebra(inverse);
    for (size_t i = 0; i < 3; ++i) {
        double expected = reference(points[i]);
        const json_t *bound = math_inverse_fields(lab_math_format("{%s | t=%.17g}", restored, points[i]));
        lab_math_close(lab_math_number(bound, "value"), expected, 2e-11 * (1 + fabs(expected)));
    }
}

static double math_inverse_cube_root(double t) { return pow(t, 1.0 / 3); }
static double math_inverse_two_thirds(double t) { return pow(t, 2.0 / 3); }
static double math_inverse_minus_third(double t) { return pow(t, -1.0 / 3); }
static double math_inverse_triple_root(double t) { return 3 * sqrt(t); }
static double math_inverse_absolute_affine(double t) { return fabs(2 * t - 1); }
static double math_inverse_scaled_root(double t) { return 3 * sqrt(t / acos(-1)); }
static double math_inverse_cubic(double t) { return t * t * t / 6; }
static double math_inverse_shifted_quadratic(double t) { return t * t * exp(-2 * t) / 2; }
static double math_inverse_polynomial(double t) { return t + t * t + t * t * t / 6; }
static double math_inverse_shifted_cubic(double t) { return t * t * t * exp(-t) / 6; }
static double math_inverse_sinh_cube(double t) { return pow(sinh(t), 3); }
static double math_inverse_cosh_cube(double t) { return pow(cosh(t), 3); }
static double math_inverse_finite_sum(double t) { return exp(-t) + 2 * exp(-2 * t) + 3 * exp(-3 * t); }
static double math_inverse_delayed_ramp(double t) { return fmax(0, t - 0.5); }
static double math_inverse_delayed_decay(double t) { return t > 2 ? exp(-(t - 2)) : 0; }
static double math_inverse_delayed_root(double t) { return t > 0.5 ? 2 * sqrt((t - 0.5) / acos(-1)) : 0; }
static double math_inverse_delayed_step(double t) { return 1 + (t > 1); }
static double math_inverse_hyperbolic_power(double t) { return pow(hyperbolic(scale * t + offset), order); }

static double math_inverse_fractional(double t)
{
    return pow(t, power - 1) * exp(-offset * t / scale) / (pow(scale, power) * tgamma(power));
}

static void test_fractional_source_functions_round_trip(void)
{
    const char *sources[] = {"sqrt(t)", "cubrt(t)", "t^(2/3)", "t^(-1/3)", "3*sqrt(t)", "log10(t)", "abs(2*t-1)"};
    const reference_t references[] = {
        sqrt, math_inverse_cube_root, math_inverse_two_thirds, math_inverse_minus_third,
        math_inverse_triple_root, log10, math_inverse_absolute_affine,
    };
    for (size_t i = 0; i < 7; ++i)
        math_inverse_check_inverse(lab_math_algebra(math_inverse_forward(sources[i])), references[i], NULL);
}

static void test_independent_fractional_and_shifted_spectra(void)
{
    const struct { const char *spectrum; double power, scale, offset; } cases[] = {
        {"1/s^(3/2)", 1.5, 1, 0}, {"s^(-4/3)", 4.0 / 3, 1, 0}, {"1/sqrt(s)", 0.5, 1, 0},
        {"1/cubrt(s)", 1.0 / 3, 1, 0}, {"(s+2)^(-3/2)", 1.5, 1, 2}, {"1/(2*s+3)^(5/4)", 1.25, 2, 3},
    };
    for (size_t i = 0; i < 6; ++i) {
        power = cases[i].power;
        scale = cases[i].scale;
        offset = cases[i].offset;
        math_inverse_check_inverse(cases[i].spectrum, math_inverse_fractional, NULL);
    }
}

static void test_scalar_association_and_decimal_logarithm(void)
{
    const char *logarithms[] = {"-(ln(s)+@gamma)/(ln(10)*s)", "(-ln(s)-@gamma)/s/ln(10)",
                               "-(ln(s)+@gamma)*(1/(s*ln(10)))"};
    const char *roots[] = {"3/(2*s^(3/2))", "(3/2)*s^(-3/2)", "3/s^(3/2)/2"};
    for (size_t i = 0; i < 3; ++i)
        math_inverse_check_inverse(logarithms[i], log10, NULL);
    for (size_t i = 0; i < 3; ++i)
        math_inverse_check_inverse(roots[i], math_inverse_scaled_root, NULL);
}

static void test_nested_integer_powers_of_rational_spectra(void)
{
    const char *spectra[] = {"(1/s^2)^2", "(s^(-2))^2", "1/(s^2)^2"};
    for (size_t i = 0; i < 3; ++i)
        math_inverse_check_inverse(spectra[i], math_inverse_cubic, NULL);
    math_inverse_check_inverse("(1/(s+2))^3", math_inverse_shifted_quadratic, NULL);
    math_inverse_check_inverse("((s+1)/s^2)^2", math_inverse_polynomial, NULL);
    math_inverse_check_inverse("((s+1)^2)^(-2)", math_inverse_shifted_cubic, NULL);
    math_inverse_check_inverse(lab_math_algebra(math_inverse_forward("causal_convolve(t,t,t)")), math_inverse_cubic, NULL);
}

static void test_unbound_hyperbolic_powers_round_trip_through_every_supported_order(void)
{
    const char *names[] = {"sinh", "cosh"};
    double (*functions[])(double) = {sinh, cosh};
    for (size_t n = 0; n < 2; ++n)
        for (order = 0; order <= 16; ++order) {
            hyperbolic = functions[n];
            scale = 1;
            offset = 0;
            const json_t *result = math_inverse_forward(lab_math_format("%s(t)^%d", names[n], order));
            if (order > 1)
                lab_math_contains(lab_math_text(result, "function"), "sum(", true);
            math_inverse_check_inverse(lab_math_algebra(result), math_inverse_hyperbolic_power, NULL);
        }
}

static void test_unbound_affine_hyperbolic_powers_and_half_planes(void)
{
    const char *names[] = {"sinh", "cosh"};
    double (*functions[])(double) = {sinh, cosh};
    const int orders[] = {3, 4, 16};
    const double rates[] = {-2, 0.5, 1};
    const double offsets[] = {0.25, -0.25, 0.25};
    for (size_t n = 0; n < 2; ++n)
        for (size_t i = 0; i < 3; ++i) {
            hyperbolic = functions[n];
            order = orders[i];
            scale = rates[i];
            offset = offsets[i];
            const char *source = lab_math_format("%s(%.17g*t+(%.17g))^%d", names[n], scale, offset, order);
            const json_t *result = math_inverse_forward(source);
            math_inverse_check_inverse(lab_math_algebra(result), math_inverse_hyperbolic_power, NULL);
            const char *invalid = lab_math_format("{%s | s=%.17g}", lab_math_text(result, "unbound"),
                                                   order * fabs(scale) - 0.5);
            lab_math_equal(lab_math_text(lab_math_fields(invalid, "s", "evaluate", 40), "value"), "NAN");
        }
}

static void test_independent_finite_sum_inverse_preserves_index_scope(void)
{
    math_inverse_check_inverse("sum(k,0,3,(-1)^k*binomial(3,k)/(s-(3-2*k)))/8", math_inverse_sinh_cube, NULL);
    math_inverse_check_inverse("sum(k,0,3,binomial(3,k)/(s-(3-2*k)))/8", math_inverse_cosh_cube, NULL);
    math_inverse_check_inverse("sum(k,1,3,k/(s+k))", math_inverse_finite_sum, NULL);
}

static void test_causal_delays_and_reciprocal_exponentials(void)
{
    const char *spectra[] = {"exp(-s/2)/s^2", "1/(exp(s/2)*s^2)", "exp(1-s/2)/(exp(1)*s^2)"};
    for (size_t i = 0; i < 3; ++i)
        math_inverse_check_inverse(spectra[i], math_inverse_delayed_ramp, NULL);
    const double points[] = {0.2, 1.6, 2.4};
    math_inverse_check_inverse("exp(-2*s)/(s+1)", math_inverse_delayed_decay, points);
    math_inverse_check_inverse("exp(-s/2)/s^(3/2)", math_inverse_delayed_root, NULL);
}

static void test_linearity_through_a_common_denominator(void)
{
    math_inverse_check_inverse("(s-2+4*exp(-s/2))/s^2", math_inverse_absolute_affine, NULL);
    math_inverse_check_inverse("(1+exp(-s))/s", math_inverse_delayed_step, NULL);
}

static void test_symbolic_power_and_delay_keep_their_domains(void)
{
    const json_t *result = math_inverse_fields("@Linv{s^(-p)}");
    lab_math_contains(lab_math_text(result, "function"), "inverselaplace(", false);
    lab_math_contains(lab_math_text(result, "expression"), "Re(p) > 0", true);
    result = math_inverse_fields("@Linv{exp(-c*s)/s^2}");
    lab_math_contains(lab_math_text(result, "function"), "inverselaplace(", false);
    lab_math_contains(lab_math_text(result, "expression"), "Re(c) > 0", true);
    lab_math_contains(lab_math_text(result, "expression"), "c ∈ ℝ", true);
    const double points[] = {0.2, 0.8};
    for (size_t i = 0; i < 2; ++i) {
        result = math_inverse_fields(lab_math_format("{@Linv{exp(-c*s)/s^2} | t=%.17g; c=1/2}", points[i]));
        lab_math_close(lab_math_number(result, "value"), math_inverse_delayed_ramp(points[i]), 1e-12);
    }
}

static void test_conditioned_spectrum_preserves_parameter_guards(void)
{
    const json_t *result = math_inverse_fields("InverseLaplace((s^(-p) where (Re(s)>0; Re(p)>0)),s,t)");
    lab_math_contains(lab_math_text(result, "function"), "inverselaplace(", false);
    lab_math_contains(lab_math_text(result, "expression"), "Re(p) > 0", true);
    lab_math_contains(lab_math_text(result, "expression"), "Re(s)", false);
}

static void test_unsupported_branch_choices_are_not_assumed(void)
{
    const char *spectra[] = {"s^(1/2)", "1/(-s+2)^(1/2)", "exp(s)/s", "exp(-i*s)/s"};
    for (size_t i = 0; i < 4; ++i)
        lab_math_contains(lab_math_text(math_inverse_fields(lab_math_format("@Linv{%s}", spectra[i])), "function"),
                          "inverselaplace(", true);
}

static void test_inverse_general_readme(void)
{
    /* README example: docs/expression.md, logarithmic inverse on positive time. */
    const char *guide = lab_math_read("docs/expression.md");
    lab_math_contains(guide, "@Linv{-(ln(s)+γ)/s}", true);
    lab_math_contains(guide, "returns `ln(t)`", true);
    const json_t *result = math_inverse_fields("@Linv{-(ln(s)+γ)/s}");
    lab_math_equal(lab_math_algebra(result), "ln(t)");
}

/* Register all 12 original general inverse regression methods. */
void test_lab_math_inverse_general_cases(void)
{
    TEST_RUN_IN_GROUP(test_fractional_source_functions_round_trip, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_independent_fractional_and_shifted_spectra, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_scalar_association_and_decimal_logarithm, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_nested_integer_powers_of_rational_spectra, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_unbound_hyperbolic_powers_round_trip_through_every_supported_order, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_unbound_affine_hyperbolic_powers_and_half_planes, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_independent_finite_sum_inverse_preserves_index_scope, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_causal_delays_and_reciprocal_exponentials, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_linearity_through_a_common_denominator, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_symbolic_power_and_delay_keep_their_domains, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_conditioned_spectrum_preserves_parameter_guards, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_unsupported_branch_choices_are_not_assumed, tests, NULL);
    lab_math_reset();
}

/* Register the documented inverse pair after every ordinary regression suite. */
void test_lab_math_inverse_general_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_inverse_general_readme, readme_examples, "mars_lab,math,readme,output");
    lab_math_reset();
}
