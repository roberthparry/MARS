/**
 * @file test_lab_math_inverse_gaussian.c
 * @brief Gaussian and error-function inverse regressions from copied formulae.
 *
 * Ports every case of test_inverse_laplace_gaussian.py, retaining exact radical
 * mismatches, impulses, complex modulation, all binding guards and both scale
 * signs. Independent C real/complex references supplement direct source checks.
 * All worker execution and string parsing use the shared native fixture.
 */
#include <math.h>

#include "test_lab_math_support.h"

typedef double complex (*reference_t)(double);
static double scale;
static double offset;

static const json_t *math_inverse_fields(const char *source)
{
    return lab_math_fields(source, "t", "evaluate", 40);
}

static double complex math_inverse_value(const char *source, double t)
{
    return lab_math_number(math_inverse_fields(lab_math_format("{%s | t=%.17g}", source, t)), "value");
}

static const json_t *math_inverse_inverse(const char *spectrum)
{
    const json_t *result = math_inverse_fields(lab_math_format("InverseLaplace(%s,s,t)", spectrum));
    lab_math_contains(lab_math_text(result, "function"), "inverselaplace(", false);
    return result;
}

static const json_t *math_inverse_copied_inverse(const char *source)
{
    const json_t *result = lab_math_fields(lab_math_format("Laplace(%s,t,s)", source), "s", "evaluate", 40);
    return math_inverse_inverse(lab_math_algebra(result));
}

static void math_inverse_round_trip(const char *source, reference_t reference)
{
    const json_t *forward = lab_math_fields(lab_math_format("Laplace(%s,t,s)", source), "s", "evaluate", 40);
    lab_math_contains(lab_math_text(forward, "function"), "laplace(", false);
    const char *inverse_source = lab_math_format("InverseLaplace(%s,s,t)", lab_math_algebra(forward));
    const json_t *recovered = math_inverse_fields(inverse_source);
    lab_math_contains(lab_math_text(recovered, "function"), "inverselaplace(", false);
    const char *restored = lab_math_algebra(recovered);
    const double points[] = {0, 0.2, 0.75, 1.5};
    for (size_t i = 0; i < 4; ++i) {
        double complex wanted = reference(points[i]);
        lab_math_close(math_inverse_value(inverse_source, points[i]), wanted, 2e-12 * (1 + cabs(wanted)));
        lab_math_close(math_inverse_value(restored, points[i]), wanted, 2e-12 * (1 + cabs(wanted)));
    }
}

static void math_inverse_check_spectrum(const char *spectrum, reference_t reference, const double points[3])
{
    const char *restored = lab_math_algebra(math_inverse_inverse(spectrum));
    for (size_t i = 0; i < 3; ++i)
        lab_math_close(math_inverse_value(restored, points[i]), reference(points[i]), 2e-12);
}

static double complex math_inverse_gaussian(double t) { return exp(-t * t); }
static double complex math_inverse_affine_gaussian(double t) { return exp(-2 * t * t + t + 1); }
static double complex math_inverse_squared_affine(double t) { return exp(-(2 * t + 1) * (2 * t + 1)); }
static double complex math_inverse_scaled_gaussian(double t) { return 3 * exp(-t * t / 4 - 2 * t); }
static double complex math_inverse_complex_gaussian(double t) { return cexp(-t * t + I * t); }
static double complex math_inverse_complex_width(double t) { return cexp(-(1 + 0.5 * I) * t * t + I * t); }
static double complex math_inverse_error_function(double t) { return erf(scale * t + offset); }
static double complex math_inverse_error_complement(double t) { return erfc(scale * t + offset); }
static double complex math_inverse_normal_density(double t) { return exp(-pow(scale * t + offset, 2) / 2) / sqrt(2 * acos(-1)); }
static double complex math_inverse_normal_cumulative(double t) { return erfc(-(scale * t + offset) / sqrt(2)) / 2; }
static double complex math_inverse_radical_three(double t) { return erf(t / sqrt(3)); }
static double complex math_inverse_radical_five(double t) { return erf(2 * t / sqrt(5)); }
static double complex math_inverse_weighted_gaussian(double t) { return t * exp(-t * t); }
static double complex math_inverse_weighted_affine(double t) { return t * exp(-2 * t * t + t + 1); }
static double complex math_inverse_quadratic_gaussian(double t) { return t * t * exp(-t * t); }
static double complex math_inverse_weighted_complex(double t) { return t * cexp(-t * t + I * t); }
static double complex math_inverse_modulated_erf(double t) { return exp(-t) * erf(t); }
static double complex math_inverse_modulated_erfc(double t) { return exp(-2 * t) * erfc(2 * t - 1); }
static double complex math_inverse_growing_erf(double t) { return exp(t) * erf(-2 * t + 1); }
static double complex math_inverse_modulated_cdf(double t) { return cexp(I * t) * erfc((2 * t - 1) / sqrt(2)) / 2; }

static void test_copied_gaussian_formulae(void)
{
    const char *sources[] = {"exp(-t^2)", "exp(-2*t^2+t+1)", "exp(-(2*t+1)^2)", "3*exp(-t^2/4-2*t)",
                             "exp(-t^2+i*t)", "exp(-(1+i/2)*t^2+i*t)"};
    const reference_t references[] = {
        math_inverse_gaussian, math_inverse_affine_gaussian, math_inverse_squared_affine,
        math_inverse_scaled_gaussian, math_inverse_complex_gaussian, math_inverse_complex_width,
    };
    for (size_t i = 0; i < 6; ++i)
        math_inverse_round_trip(sources[i], references[i]);
}

static void math_inverse_affine_families(const char *names[2], const reference_t references[2])
{
    const double scales[] = {1, 2, -2, 0.5};
    const double offsets[] = {0, 1, 1, -1};
    for (size_t n = 0; n < 2; ++n)
        for (size_t i = 0; i < 4; ++i) {
            scale = scales[i];
            offset = offsets[i];
            math_inverse_round_trip(lab_math_format("%s(%.17g*t+(%.17g))", names[n], scale, offset), references[n]);
        }
}

static void test_copied_error_functions_with_both_scale_signs(void)
{
    const char *names[] = {"erf", "erfc"};
    const reference_t references[] = {math_inverse_error_function, math_inverse_error_complement};
    math_inverse_affine_families(names, references);
}

static void test_copied_normal_distributions_with_both_scale_signs(void)
{
    const char *names[] = {"normal_pdf", "normal_cdf"};
    const reference_t references[] = {math_inverse_normal_density, math_inverse_normal_cumulative};
    math_inverse_affine_families(names, references);
}

static void test_exact_nested_radical_scales_in_copied_normal_cdf(void)
{
    const char *spectra[] = {
        "(1+exp(s^2/(4*(-1/sqrt(2))^2))*erfc(s/(2*sqrt((-1/sqrt(2))^2))))/(2*s)",
        "exp(s^2/(4*(-1/sqrt(3))^2))*erfc(s/(2*sqrt((-1/sqrt(3))^2)))/s",
        "exp(s^2/(4*(-2/sqrt(5))^2))*erfc(s/(2*sqrt((-2/sqrt(5))^2)))/s",
    };
    const reference_t references[] = {math_inverse_normal_cumulative, math_inverse_radical_three, math_inverse_radical_five};
    const double points[] = {0, 0.3, 1.1};
    scale = 1;
    offset = 0;
    for (size_t i = 0; i < 3; ++i)
        math_inverse_check_spectrum(spectra[i], references[i], points);
    const char *different = "exp(s^2/(4*(-1/sqrt(2))^2)+s^2/100000000000000000000)"
                            "*erfc(s/(2*sqrt((-1/sqrt(2))^2)))/s";
    lab_math_contains(lab_math_text(math_inverse_fields(lab_math_format("InverseLaplace(%s,s,t)", different)), "function"),
                      "inverselaplace(", true);
}

static void test_copied_polynomial_weighted_gaussians(void)
{
    const char *sources[] = {"t*exp(-t^2)", "t*exp(-2*t^2+t+1)", "t^2*exp(-t^2)", "t*exp(-t^2+i*t)"};
    const reference_t references[] = {
        math_inverse_weighted_gaussian, math_inverse_weighted_affine,
        math_inverse_quadratic_gaussian, math_inverse_weighted_complex,
    };
    for (size_t i = 0; i < 4; ++i)
        math_inverse_round_trip(sources[i], references[i]);
}

static void test_copied_exponentially_modulated_error_functions(void)
{
    const char *sources[] = {"exp(-t)*erf(t)", "exp(-2*t)*erfc(2*t-1)", "exp(t)*erf(-2*t+1)",
                             "exp(i*t)*normal_cdf(-2*t+1)"};
    const reference_t references[] = {
        math_inverse_modulated_erf, math_inverse_modulated_erfc, math_inverse_growing_erf, math_inverse_modulated_cdf,
    };
    for (size_t i = 0; i < 4; ++i)
        math_inverse_round_trip(sources[i], references[i]);
}

static void test_independent_weighted_and_frequency_shifted_formulae(void)
{
    const char *spectra[] = {
        "1/2-sqrt(@pi)*s/4*exp(s^2/4)*erfc(s/2)",
        "sqrt(@pi)/4*(1+s^2/2)*exp(s^2/4)*erfc(s/2)-s/4",
        "exp((s+1)^2/4)*erfc((s+1)/2)/(s+1)",
        "2*exp((2*s+2)^2/16)*erfc((2*s+2)/4)/(2*s+2)",
        "-sqrt(2)*sqrt(@pi)/16*((s-1)*erfc((s-1)/(2*sqrt(2)))"
        "*exp(((s-1)/(2*sqrt(2)))^2+1)-2*sqrt(2)*exp(1)/sqrt(@pi))",
        "-¹⁄₁₆√2·√(π)·((s-1)·erfc((s-1)/(2·√(2)))"
        "·exp(((s-1)/(2·√(2)))²+1)-2/√π·exp(1)·√(2))",
    };
    const reference_t references[] = {math_inverse_weighted_gaussian, math_inverse_quadratic_gaussian, math_inverse_modulated_erf,
                                     math_inverse_modulated_erf, math_inverse_weighted_affine, math_inverse_weighted_affine};
    const double points[] = {0, 0.4, 1.25};
    for (size_t i = 0; i < 6; ++i)
        math_inverse_check_spectrum(spectra[i], references[i], points);
}

static void test_symbolic_frequency_shift_keeps_the_whole_spectrum_consistent(void)
{
    const char *source = "exp(-k*t)*erf(t)";
    const json_t *result = math_inverse_copied_inverse(source);
    const char *rates[] = {"2", "-1", "i/2"};
    const double complex numerical_rates[] = {2, -1, I / 2};
    for (size_t i = 0; i < 3; ++i) {
        const char *bindings = lab_math_format(" | t=0.4; k=%s", rates[i]);
        double complex expected = lab_math_number(math_inverse_fields(lab_math_format("{%s%s}", source, bindings)), "value");
        const char *bound = lab_math_format("{%s%s}", lab_math_algebra(result), bindings);
        double complex actual = lab_math_number(math_inverse_fields(bound), "value");
        lab_math_close(actual, expected, 2e-12);
        lab_math_close(actual, cexp(-numerical_rates[i] * 0.4) * erf(0.4), 2e-12);
    }
}

static void test_cartesian_exponential_spectrum_recovers_complex_gaussian(void)
{
    const double points[] = {0, 0.3, 1.1};
    math_inverse_check_spectrum("sqrt(@pi)/2*exp((s^2-1)/4)*(cos(s/2)-i*sin(s/2))*erfc((s-i)/2)",
                                math_inverse_complex_gaussian, points);
}

static void test_symbolic_error_kernel_cancels_reciprocal_radical_factors(void)
{
    const json_t *result = math_inverse_inverse("(erf(b)+a/sqrt(a^2)*exp(s^2/(4*a^2)+b*s/a)"
                                   "*erfc(s/(2*sqrt(a^2))+b*sqrt(a^2)/a))/s");
    const int scales[] = {-2, 2};
    for (size_t i = 0; i < 2; ++i) {
        const char *source = lab_math_format("{%s | t=0.3; a=%d; b=1}", lab_math_algebra(result), scales[i]);
        const json_t *bound = math_inverse_fields(source);
        lab_math_close(lab_math_number(bound, "value"), erf(scales[i] * 0.3 + 1), 2e-12);
    }
}

static void test_initial_value_impulses_are_not_silently_discarded(void)
{
    const char *kernel = "sqrt(@pi)*s/4*exp(s^2/4)*erfc(s/2)";
    const char *spectra[] = {kernel, lab_math_format("1/3-%s", kernel),
                             lab_math_format("1/2+1/100000000000000000000-%s", kernel)};
    for (size_t i = 0; i < 3; ++i)
        lab_math_contains(lab_math_text(math_inverse_fields(lab_math_format("InverseLaplace(%s,s,t)", spectra[i])), "function"),
                          "inverselaplace(", true);
}

static void test_independently_written_kernel_and_its_integral(void)
{
    const char *spectrum = "exp((s/4+1)^2)*erfc(s/4+1)";
    const char *restored = lab_math_algebra(math_inverse_inverse(spectrum));
    const char *integral = lab_math_algebra(math_inverse_inverse(lab_math_format("(%s)/s", spectrum)));
    const double points[] = {0.1, 0.5, 1};
    for (size_t i = 0; i < 3; ++i) {
        double t = points[i];
        lab_math_close(math_inverse_value(restored, t), 4 / sqrt(acos(-1)) * exp(-4 * t * t - 4 * t), 2e-12);
        lab_math_close(math_inverse_value(integral, t), exp(1) * (erf(2 * t + 1) - erf(1)), 2e-12);
    }
}

static void test_symbolic_gaussian_retains_parameter_restrictions(void)
{
    const char *source = "exp(-a*t^2+b*t+d)";
    const json_t *recovered = math_inverse_copied_inverse(source);
    lab_math_contains(lab_math_text(recovered, "function"), "realpart", true);
    const char *bindings[] = {" | t=0.4; a=2; b=3; d=-1", " | t=0.4; a=1+i/2; b=i; d=0"};
    const double complex references[] = {exp(-2 * 0.16 + 3 * 0.4 - 1), cexp(-(1 + I / 2) * 0.16 + I * 0.4)};
    for (size_t i = 0; i < 2; ++i) {
        double complex actual = lab_math_number(math_inverse_fields(lab_math_format("{%s%s}", lab_math_algebra(recovered),
                                                                       bindings[i])), "value");
        double complex expected = lab_math_number(math_inverse_fields(lab_math_format("{%s%s}", source, bindings[i])), "value");
        lab_math_close(actual, expected, 2e-12);
        lab_math_close(actual, references[i], 2e-12);
    }
    const char *copied = lab_math_text(recovered, "expression");
    const char *names[] = {"t", "a", "b", "d"};
    const char *values[] = {"0.4", "-1", "0", "0"};
    for (size_t i = 0; i < 4; ++i) {
        copied = lab_math_replace(copied, lab_math_format("%s = NAN", names[i]),
                                 lab_math_format("%s = %s", names[i], values[i]));
        copied = lab_math_replace(copied, lab_math_format("%s = ?", names[i]),
                                 lab_math_format("%s = %s", names[i], values[i]));
    }
    lab_math_equal(lab_math_text(math_inverse_fields(copied), "value"), "NAN");
}

static void test_symbolic_error_function_formulae(void)
{
    const char *names[] = {"erf", "erfc", "normal_pdf", "normal_cdf"};
    const reference_t references[] = {
        math_inverse_error_function, math_inverse_error_complement,
        math_inverse_normal_density, math_inverse_normal_cumulative,
    };
    const int scales[] = {2, -2};
    for (size_t n = 0; n < 4; ++n) {
        const char *source = lab_math_format("%s(a*t+b)", names[n]);
        const json_t *result = math_inverse_copied_inverse(source);
        for (size_t i = 0; i < 2; ++i) {
            scale = scales[i];
            offset = 1;
            const char *bindings = lab_math_format(" | t=0.4; a=%d; b=1", scales[i]);
            double complex actual = lab_math_number(math_inverse_fields(lab_math_format("{%s%s}", lab_math_algebra(result),
                                                                           bindings)), "value");
            double complex expected = lab_math_number(math_inverse_fields(lab_math_format("{%s%s}", source, bindings)), "value");
            lab_math_close(actual, expected, 2e-12);
            lab_math_close(actual, references[n](0.4), 2e-12);
        }
    }
}

static void test_nonmatching_exponents_and_wrong_root_branch_remain_symbolic(void)
{
    const char *spectra[] = {"exp(s^2)*erfc(-s)", "exp(-s^2)*erfc(i*s)", "exp(s^2/4+s)*erfc(s/2)",
                             "exp(s^2/3)*erfc(s/2)", "exp(s^2/4)*erfc(s/2)^2"};
    for (size_t i = 0; i < 5; ++i)
        lab_math_contains(lab_math_text(math_inverse_fields(lab_math_format("InverseLaplace(%s,s,t)", spectra[i])), "function"),
                          "inverselaplace(", true);
}

/* Register all 15 original Gaussian inverse regression methods. */
void test_lab_math_inverse_gaussian_cases(void)
{
    TEST_RUN_IN_GROUP(test_copied_gaussian_formulae, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_copied_error_functions_with_both_scale_signs, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_copied_normal_distributions_with_both_scale_signs, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_exact_nested_radical_scales_in_copied_normal_cdf, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_copied_polynomial_weighted_gaussians, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_copied_exponentially_modulated_error_functions, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_independent_weighted_and_frequency_shifted_formulae, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_symbolic_frequency_shift_keeps_the_whole_spectrum_consistent, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_cartesian_exponential_spectrum_recovers_complex_gaussian, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_symbolic_error_kernel_cancels_reciprocal_radical_factors, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_initial_value_impulses_are_not_silently_discarded, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_independently_written_kernel_and_its_integral, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_symbolic_gaussian_retains_parameter_restrictions, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_symbolic_error_function_formulae, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_nonmatching_exponents_and_wrong_root_branch_remain_symbolic, tests, NULL);
    lab_math_reset();
}
