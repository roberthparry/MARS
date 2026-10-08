/**
 * @file test_lab_math_inverse_elementary.c
 * @brief Elementary inverse Laplace regressions from freshly serialised spectra.
 *
 * Ports every elementary Python regression, including staircase signs, both
 * exterior atanh cuts, complete branch terms and Clausen dummy-index scope.
 * References use C mathematics or the independently evaluated source function.
 * Worker strings are owned by the shared sequential fixture arena.
 */
#include <math.h>
#include <string.h>

#include "test_lab_math_support.h"

typedef double complex (*reference_t)(double);
static double scale;
static int order;
static int coefficient;
static const char *reference_source;

static const json_t *math_inverse_fields(const char *source)
{
    return lab_math_fields(source, "t", "evaluate", 40);
}

static double complex math_inverse_value(const char *source, double time)
{
    return lab_math_number(math_inverse_fields(lab_math_format("{%s | t=%.17g}", source, time)), "value");
}

static const json_t *math_inverse_check_inverse(const char *spectrum, reference_t reference, const double *points)
{
    const double defaults[] = {0.19, 0.71, 1.31};
    if (!points)
        points = defaults;
    const char *source = lab_math_format("InverseLaplace(%s,s,t)", spectrum);
    const json_t *inverse = math_inverse_fields(source);
    const char *function = lab_math_text(inverse, "function");
    if (!lab_math_check(*function != '\0', "inverse Function field must not be empty"))
        return inverse;
    lab_math_contains(function, "inverselaplace(", false);
    const char *restored = lab_math_algebra(inverse);
    if (!lab_math_check(*restored != '\0', "inverse Expression algebra must not be empty"))
        return inverse;
    for (size_t i = 0; i < 3; ++i) {
        double complex wanted = reference(points[i]);
        lab_math_close(math_inverse_value(source, points[i]), wanted, 2e-11 * (1 + cabs(wanted)));
        lab_math_close(math_inverse_value(restored, points[i]), wanted, 2e-11 * (1 + cabs(wanted)));
    }
    return inverse;
}

static void math_inverse_round_trip(const char *source, reference_t reference, const double *points)
{
    const json_t *forward = lab_math_fields(lab_math_format("Laplace(%s,t,s)", source), "s", "evaluate", 40);
    lab_math_contains(lab_math_text(forward, "function"), "laplace(", false);
    math_inverse_check_inverse(lab_math_algebra(forward), reference, points);
}

static double complex math_inverse_floor_reference(double t) { return floor(scale * t); }
static double complex math_inverse_ceil_reference(double t) { return ceil(scale * t); }
static double complex math_inverse_tanh_reference(double t) { return tanh(scale * t); }
static double complex math_inverse_sech_reference(double t) { return 1 / cosh(scale * t); }
static double complex math_inverse_atan_reference(double t) { return atan(scale * t); }
static double complex math_inverse_acot_reference(double t) { return acos(-1) / 2 - atan(scale * t); }
static double complex math_inverse_asinh_reference(double t) { return asinh(scale * t); }

static double complex math_inverse_atanh_reference(double t)
{
    double x = scale * t;
    if (fabs(x) < 1)
        return atanh(x);
    return copysign(log((fabs(x) + 1) / (fabs(x) - 1)) / 2, x) + I * acos(-1) / 2;
}

static double complex math_inverse_changed_branch_reference(double t)
{
    return creal(math_inverse_atanh_reference(t)) + (t > 0.5 ? coefficient * I * acos(-1) / 2 : 0);
}

static double complex math_inverse_clausen_one_reference(double t) { return -log(2 * fabs(sin(t))); }
static double complex math_inverse_source_reference(double t) { return math_inverse_value(reference_source, t); }
static double complex math_inverse_affine_atan_reference(double t) { return 3 * atan(2 * t) + 2; }
static double complex math_inverse_affine_asinh_reference(double t) { return 2 - 3 * asinh(t / 2); }
static double complex math_inverse_affine_tanh_reference(double t) { return 3 * tanh(2 * t) - 2; }

static void test_real_scaled_staircases(void)
{
    const double scales[] = {2, -2, 0.5, -0.5};
    const char *names[] = {"floor", "ceil"};
    const reference_t references[] = {math_inverse_floor_reference, math_inverse_ceil_reference};
    /* All points avoid staircase jumps, whose values a unilateral inverse cannot fix. */
    for (size_t n = 0; n < 2; ++n)
        for (size_t i = 0; i < 4; ++i) {
            scale = scales[i];
            math_inverse_round_trip(lab_math_format("%s(%.17g*t)", names[n], scale), references[n], NULL);
        }
}

static void test_independent_geometric_spectra(void)
{
    scale = 2;
    math_inverse_check_inverse("1/(s*(exp(s/2)-1))", math_inverse_floor_reference, NULL);
    math_inverse_check_inverse("exp(s/2)/(s*(exp(s/2)-1))", math_inverse_ceil_reference, NULL);
    scale = -0.5;
    math_inverse_check_inverse("-exp(2*s)/(s*(exp(2*s)-1))", math_inverse_floor_reference, NULL);
    math_inverse_check_inverse("exp(2*s)/(s*(1-exp(2*s)))", math_inverse_floor_reference, NULL);
}

static void test_digamma_hyperbolic_pairs(void)
{
    const double scales[] = {2, -2, 0.5};
    const char *names[] = {"tanh", "sech"};
    const reference_t references[] = {math_inverse_tanh_reference, math_inverse_sech_reference};
    for (size_t i = 0; i < 3; ++i)
        for (size_t n = 0; n < 2; ++n) {
            scale = scales[i];
            math_inverse_round_trip(lab_math_format("%s(%.17g*t)", names[n], scale), references[n], NULL);
        }
}

static void test_independent_digamma_spectra(void)
{
    scale = 2;
    math_inverse_check_inverse("(2*digamma(s/8+1/2)-digamma(s/8)-digamma(s/8+1))/8", math_inverse_tanh_reference, NULL);
    math_inverse_check_inverse("(digamma(s/8+3/4)-digamma(s/8+1/4))/4", math_inverse_sech_reference, NULL);
}

static void test_inverse_circular_and_asinh_pairs(void)
{
    const double scales[] = {2, -2, 0.5};
    const char *names[] = {"atan", "acot", "asinh"};
    const reference_t references[] = {math_inverse_atan_reference, math_inverse_acot_reference, math_inverse_asinh_reference};
    for (size_t i = 0; i < 3; ++i)
        for (size_t n = 0; n < 3; ++n) {
            scale = scales[i];
            math_inverse_round_trip(lab_math_format("%s(%.17g*t)", names[n], scale), references[n], NULL);
        }
}

static void test_cancelled_imaginary_units_produce_real_rates(void)
{
    const char *sources[] = {"atan(2*t)", "acot(-2*t)", "Cl(3,2*t)"};
    for (size_t i = 0; i < 3; ++i) {
        const json_t *forward = lab_math_fields(lab_math_format("Laplace(%s,t,s)", sources[i]), "s", "evaluate", 40);
        const json_t *inverse = math_inverse_fields(lab_math_format("InverseLaplace(%s,s,t)", lab_math_algebra(forward)));
        lab_math_contains(lab_math_text(inverse, "function"), "inverselaplace(", false);
        lab_math_regex(lab_math_algebra(inverse), "[+-][[:space:]]*0[[:space:]]*i", false);
        lab_math_regex(lab_math_text(inverse, "tex"), "[+-][[:space:]]*0[[:space:]]*i", false);
    }
}

static void test_atanh_retains_both_exterior_cut_values(void)
{
    const double scales[] = {2, -2, 0.5, -0.5};
    const double points[] = {0.19, 0.71, 2.31};
    for (size_t i = 0; i < 4; ++i) {
        scale = scales[i];
        math_inverse_round_trip(lab_math_format("atanh(%.17g*t)", scale), math_inverse_atanh_reference, points);
    }
}

static void test_independent_atanh_branch_formula(void)
{
    const int signs[] = {1, -1};
    for (size_t i = 0; i < 2; ++i) {
        scale = 2 * signs[i];
        math_inverse_check_inverse(lab_math_format("((%d)*(exp(-s/2)*Ei(s/2)+exp(s/2)*E1(s/2))"
                                      "+i*@pi*exp(-s/2))/(2*s)", signs[i]), math_inverse_atanh_reference, NULL);
    }
}

static void test_complete_formula_verification_preserves_changed_branch_terms(void)
{
    for (coefficient = 0; coefficient <= 2; coefficient += 2) {
        const char *spectrum = lab_math_format("(exp(-s/2)*Ei(s/2)+exp(s/2)*E1(s/2)"
                                               "+%d*i*@pi*exp(-s/2))/(2*s)", coefficient);
        const json_t *result = math_inverse_fields(lab_math_format("InverseLaplace(%s,s,t)", spectrum));
        const char *function = lab_math_text(result, "function");
        if (!lab_math_check(*function != '\0', "branch-verification Function field must not be empty"))
            continue;
        if (!strstr(function, "inverselaplace(")) {
            scale = 2;
            math_inverse_check_inverse(spectrum, math_inverse_changed_branch_reference, NULL);
        }
    }
}

static void test_clausen_orders_inferred_from_spectral_degree(void)
{
    math_inverse_round_trip("Cl(1,2*t)", math_inverse_clausen_one_reference, NULL);
    const int orders[] = {2, 2, 3, 6, 32};
    const double scales[] = {1, -2, 2, -0.5, 1};
    for (size_t i = 0; i < 5; ++i) {
        order = orders[i];
        reference_source = lab_math_format("Cl(%d,%.17g*t)", order, scales[i]);
        /* Preserve the original independent special-function evaluator reference. */
        math_inverse_round_trip(reference_source, math_inverse_source_reference, NULL);
    }
}

static void test_scalar_multiples_and_constant_offsets(void)
{
    math_inverse_round_trip("3*atan(2*t)+2", math_inverse_affine_atan_reference, NULL);
    math_inverse_round_trip("2-3*asinh(t/2)", math_inverse_affine_asinh_reference, NULL);
    math_inverse_round_trip("3*tanh(2*t)-2", math_inverse_affine_tanh_reference, NULL);
}

static void test_asinh_offset_in_independent_common_denominator_spectra(void)
{
    const char *kernel = "4*s/@pi*hypergeometricpfq(1,2,1,3/2,3/2,-s^2)-bessely(0,2*s)";
    math_inverse_check_inverse(lab_math_format("(2-3*@pi/2*(%s))/s", kernel), math_inverse_affine_asinh_reference, NULL);
    math_inverse_check_inverse(lab_math_format("2/s-3*@pi*(%s)/(2*s)", kernel), math_inverse_affine_asinh_reference, NULL);
}

static double complex math_inverse_clausen_three_reference(double t)
{
    return lab_math_number(math_inverse_fields(lab_math_format("Cl(3,%.17g)", 2 * t)), "value");
}

static void test_clausen_finite_recurrence_is_independent_of_dummy_name(void)
{
    const char *indices[] = {"j", "k"};
    for (size_t i = 0; i < 2; ++i) {
        const char *index = indices[i];
        const char *spectrum = lab_math_format("(2*sum(%s,1,1,(-1)^(%s-1)*zeta(5-2*%s)"
                                               "*(s/2)^(4-2*%s))-digamma(1+i*s/2)-digamma(1-i*s/2)"
                                               "-2*@gamma)/(4*(s/2)^3)", index, index, index, index);
        math_inverse_check_inverse(spectrum, math_inverse_clausen_three_reference, NULL);
    }
}

/* Register all 13 original elementary regression methods in the ordinary phase. */
void test_lab_math_inverse_elementary_cases(void)
{
    TEST_RUN_IN_GROUP(test_real_scaled_staircases, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_independent_geometric_spectra, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_digamma_hyperbolic_pairs, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_independent_digamma_spectra, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_inverse_circular_and_asinh_pairs, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_cancelled_imaginary_units_produce_real_rates, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_atanh_retains_both_exterior_cut_values, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_independent_atanh_branch_formula, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_complete_formula_verification_preserves_changed_branch_terms, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_clausen_orders_inferred_from_spectral_degree, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_scalar_multiples_and_constant_offsets, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_asinh_offset_in_independent_common_denominator_spectra, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_clausen_finite_recurrence_is_independent_of_dummy_name, tests, NULL);
    lab_math_reset();
}
