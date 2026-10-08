/**
 * @file test_lab_math_fourier_branch.c
 * @brief Native boundary-value and gamma Fourier regression port.
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


static void test_copied_branch_round_trips(void)
{
    const char *functions[] = {"atanh", "asin", "acos", "acosh"};
    const char *arguments[] = {"x", "2*x+1", "-2*x+1", "x/2-1/3", "a*x+b"};
    const char *operators[] = {"Fourier", "InverseFourier"};
    const double points[] = {-2.3, -1, -0.2, 0, 0.7, 1, 2.3};
    for (size_t f = 0; f < 4; ++f)
        for (size_t a = 0; a < 5; ++a)
            for (size_t d = 0; d < 2; ++d) {
                const char *source = lab_math_format("%s(%s)", functions[f], arguments[a]);
                const json_t *s = math_fourier_fields(lab_math_format("%s(%s,x,k)", operators[d], source), "k");
                const char *copies[] = {lab_math_algebra(s), math_fourier_text(s, "expression")};
                for (size_t c = 0; c < 2; ++c) {
                    const json_t *r = math_fourier_fields(lab_math_format("%s(%s,k,x)", operators[1-d], copies[c]), "x");
                    lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
                    lab_math_contains(math_fourier_text(r, "expression"), "k =", false);
                    for (size_t p = 0; p < 7; ++p) {
                        const char *bound = lab_math_replace(math_fourier_text(r, "expression"), "x = NAN",
                                                            lab_math_format("x = %.17g", points[p]));
                        bound = lab_math_replace(bound, "a = NAN", "a = -2");
                        bound = lab_math_replace(bound, "b = NAN", "b = 0.3");
                        const json_t *expected = math_fourier_fields(lab_math_format("{%s | x=%.17g; a=-2; b=0.3}",
                            source, points[p]), "x");
                        const json_t *actual = math_fourier_fields(bound, "x");
                        if (!strcmp(math_fourier_text(expected, "value"), "NAN") || strstr(math_fourier_text(expected,
                            "value"), "∞"))
                            lab_math_equal(math_fourier_text(actual, "value"), "NAN");
                        else
                            lab_math_close(math_fourier_number(actual), math_fourier_number(expected), 2e-12);
                    }
                }
            }
}

static void test_full_distribution_and_mathematical_notation(void)
{
    const char *functions[] = {"atanh", "asin", "acos", "acosh"};
    const char *cards[] = {"expression", "tex", "function"};
    const char *forbidden[] = {": principal value", ": finite part", "PV(", "Fp("};
    for (size_t f = 0; f < 4; ++f) {
        const json_t *r = math_fourier_fields(lab_math_format("@F{%s(x)}", functions[f]), "k");
        lab_math_contains(math_fourier_text(r, "expression"), "δ(k)", true);
        lab_math_contains(math_fourier_text(r, "expression"), "k ≠ 0", false);
        for (size_t c = 0; c < 3; ++c)
            for (size_t i = 0; i < 4; ++i)
                lab_math_contains(math_fourier_text(r, cards[c]), forbidden[i], false);
        if (f) {
            lab_math_contains(math_fourier_text(r, "expression"), "step(k)/k", true);
            lab_math_contains(math_fourier_text(r, "expression"), "Dk(", false);
            lab_math_contains(math_fourier_text(r, "expression"), "ln(2)", true);
            lab_math_contains(math_fourier_text(r, "expression"), "γ", true);
            lab_math_contains(math_fourier_text(r, "function"), "@eulermascheroni", true);
            lab_math_contains(math_fourier_text(r, "tex"), "\\partial", false);
            lab_math_contains(math_fourier_text(r, "tex"), "\\operatorname{D}", false);
            lab_math_contains(math_fourier_text(r, "function"), "delta(k).ln", false);
        }
    }
}

static void test_direct_spectra_and_zero_frequency_constants(void)
{
    const char *spectra[] = {"i*@pi^2*delta(k)-2*i*@pi*step(k)*sinc(k/@pi)",
        "2*i*@pi*((ln(2)-@eulermascheroni)*delta(k)-besselj(0,k)*step(k)/k)",
        "@pi^2*delta(k)-2*i*@pi*((ln(2)-@eulermascheroni)*delta(k)-besselj(0,k)*step(k)/k)",
        "i*@pi^2*delta(k)+2*@pi*((ln(2)-@eulermascheroni)*delta(k)-besselj(0,k)*step(k)/k)"};
    const char *originals[] = {"atanh(x)", "asin(x)", "acos(x)", "acosh(x)"};
    for (size_t i = 0; i < 4; ++i) {
        const json_t *r = math_fourier_fields(lab_math_format("InverseFourier(%s,k,x)", spectra[i]), "x");
        lab_math_equal(lab_math_algebra(r), originals[i]);
        const json_t *altered = math_fourier_fields(lab_math_format("InverseFourier((%s)+delta(k),k,x)", spectra[i]), "x");
        const json_t *actual = math_fourier_fields(lab_math_replace(math_fourier_text(altered, "expression"), "x = NAN",
            "x = 0"), "x");
        double complex expected = math_fourier_number(math_fourier_fields(lab_math_format("{%s | x=0}", originals[i]),
            "x"))+1/(2*M_PI);
        lab_math_close(math_fourier_number(actual), expected, 2e-12);
    }
}

static void test_legacy_derivative_spectra(void)
{
    const int rates[] = {1, 2, -2};
    const double points[] = {-1.3, 0, 1.3};
    for (size_t a = 0; a < 3; ++a) {
        const char *spectrum = lab_math_format("2*i*@pi*((ln(2)-@eulermascheroni)*delta(k)"
            "-(%d/abs(%d))*besselj(0,k/(%d))*Dk(step(k/(%d))*ln(abs(k/(%d)))))",
            rates[a], rates[a], rates[a], rates[a], rates[a]);
        const json_t *r = math_fourier_fields(lab_math_format("InverseFourier(%s,k,x)", spectrum), "x");
        lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
        for (size_t p = 0; p < 3; ++p) {
            const char *bound = lab_math_replace(math_fourier_text(r, "expression"), "x = NAN", lab_math_format(
                "x = %.17g", points[p]));
            lab_math_close(math_fourier_number(math_fourier_fields(bound, "x")),
                           math_fourier_number(math_fourier_fields(lab_math_format("asin(%d*(%.17g))", rates[a],
                               points[p]), "x")), 2e-12);
        }
    }
}

static void test_acosh_boundary_values_and_constants(void)
{
    const double points[] = {-3, -1, -0.5, 0, 0.5, 1, 3};
    for (size_t p = 0; p < 7; ++p)
        lab_math_close(math_fourier_number(math_fourier_fields(lab_math_format("acosh(%.17g)", points[p]), "k")),
            cacosh(CMPLX(points[p], 0)), 2e-12);
    const char *functions[] = {"acosh", "acos", "asin", "atanh"};
    for (size_t f = 0; f < 4; ++f)
        for (int offset = 0; offset <= 2; offset += 2) {
            const json_t *s = math_fourier_fields(lab_math_format("Fourier(%s(0*x+%d),x,k)", functions[f], offset), "k");
            lab_math_contains(math_fourier_text(s, "function"), "fourier(", false);
            const json_t *r = math_fourier_fields(lab_math_format("{InverseFourier(%s,k,x) | x=0}", lab_math_algebra(s)), "x");
            lab_math_close(math_fourier_number(r), math_fourier_number(math_fourier_fields(lab_math_format("%s(%d)",
                functions[f], offset), "k")), 2e-12);
        }
    const char *sources[] = {"acosh(x+i)", "acosh(i*x)", "acosh(x^2)"};
    for (size_t i = 0; i < 3; ++i)
        lab_math_contains(math_fourier_text(math_fourier_fields(lab_math_format("Fourier(%s,x,k)", sources[i]), "k"),
            "function"), "fourier(", true);
}

static double math_fourier_j0_series(double x)
{
    double term = 1, total = 1;
    for (int n = 1; n < 60; ++n) {
        term *= -x*x/(4*n*n);
        total += term;
    }
    return total;
}

static double complex math_fourier_acosh_low(double k, void *context)
{
    double odd = *(double *)context;
    return k == 0 ? odd : (math_fourier_j0_series(k)*(1+odd*k)*exp(-k*k)-1)/k;
}

static double complex math_fourier_acosh_high(double k, void *context)
{
    double odd = *(double *)context;
    return math_fourier_j0_series(k)*(1+odd*k)*exp(-k*k)/k;
}

static double complex math_fourier_acosh_tails(double u, void *context)
{
    (void)context;
    return u*sinh(u)*exp(-pow(cosh(u), 2)/4);
}

static double complex math_fourier_acosh_interior(double u, void *context)
{
    (void)context;
    return sin(u)*cos(u)*(u-M_PI/2)*exp(-pow(cos(u), 2)/4);
}

static void test_acosh_distribution_against_gaussian_test_functions(void)
{
    double odds[] = {0, -0.3, 0.4};
    for (size_t i = 0; i < 3; ++i) {
        double complex low = lab_math_simpson(math_fourier_acosh_low, &odds[i], 0, 1, 12000);
        double complex high = lab_math_simpson(math_fourier_acosh_high, &odds[i], 1, 12, 12000);
        double complex spectral = 2*M_PI*(log(2)-0.5772156649015328606-low-high)+I*M_PI*M_PI;
        double complex tails = 2*lab_math_simpson(math_fourier_acosh_tails, NULL, 0, 6, 12000);
        double complex interior = lab_math_simpson(math_fourier_acosh_interior, NULL, 0, M_PI/2, 12000);
        double exterior = -M_PI*exp(-0.25);
        double complex original = sqrt(M_PI)*(tails+odds[i]*(interior+exterior))+I*M_PI*M_PI;
        lab_math_close(spectral, original, 2e-10);
    }
}

static double complex math_fourier_asin_low(double k, void *context)
{
    double rate = *(double *)context;
    return k == 0 ? 0 : (math_fourier_j0_series(k/rate)*exp(-k*k)-1)/k;
}

static double complex math_fourier_asin_high(double k, void *context)
{
    double rate = *(double *)context;
    return math_fourier_j0_series(k/rate)*exp(-k*k)/k;
}

static double complex math_fourier_asin_original(double u, void *context)
{
    double rate = *(double *)context;
    return u*sinh(u)/rate*exp(-pow(cosh(u)/rate, 2)/4);
}

static void test_asin_distribution_against_gaussian_test_function(void)
{
    double rates[] = {0.5, 1, 2};
    for (size_t i = 0; i < 3; ++i) {
        double complex low = lab_math_simpson(math_fourier_asin_low, &rates[i], 0, 1, 12000);
        double complex high = lab_math_simpson(math_fourier_asin_high, &rates[i], 1, 12, 12000);
        double complex spectral = 2*M_PI*(log(2*rates[i])-0.5772156649015328606-low-high);
        double complex original = 2*sqrt(M_PI)*lab_math_simpson(math_fourier_asin_original, &rates[i], 0, 6, 12000);
        lab_math_close(spectral, original, 2e-10);
    }
}

static void test_gamma_pairs_and_round_trips(void)
{
    const char *arguments[] = {"1+i*x", "2+2*i*x", "1-2*i*x", "a+i*x", "1+i/3+i*x"};
    const char *operators[] = {"Fourier", "InverseFourier"};
    const double points[] = {-0.7, 0, 1.2};
    for (size_t a = 0; a < 5; ++a)
        for (size_t d = 0; d < 2; ++d) {
            const json_t *s = math_fourier_fields(lab_math_format("%s(gamma(%s),x,k)", operators[d], arguments[a]), "k");
            lab_math_contains(math_fourier_text(s, "function"), "fourier(", false);
            const char *copies[] = {lab_math_algebra(s), math_fourier_text(s, "expression")};
            for (size_t c = 0; c < 2; ++c) {
                const json_t *r = math_fourier_fields(lab_math_format("%s(%s,k,x)", operators[1-d], copies[c]), "x");
                lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
                for (size_t p = 0; p < 3; ++p) {
                    const char *bound = lab_math_replace(math_fourier_text(r, "expression"), "x = NAN",
                                                        lab_math_format("x = %.17g", points[p]));
                    bound = lab_math_replace(bound, "a = NAN", "a = 2");
                    const json_t *expected = math_fourier_fields(lab_math_format("{gamma(%s) | x=%.17g; a=2}",
                        arguments[a], points[p]), "x");
                    lab_math_close(math_fourier_number(math_fourier_fields(bound, "x")), math_fourier_number(expected), 2e-12);
                }
            }
        }
}

static double complex math_fourier_gamma_inverse_integrand(double k, void *context)
{
    return cexp(k-exp(k)+I*k*(*(double *)context));
}

static void test_gamma_independent_inverse_quadrature(void)
{
    double points[] = {-0.7, 0, 1.2};
    for (size_t p = 0; p < 3; ++p) {
        double complex expected = lab_math_simpson(math_fourier_gamma_inverse_integrand, &points[p], -32, 5, 12000);
        lab_math_close(math_fourier_number(math_fourier_fields(lab_math_format("{@Finv{2*@pi*exp(k-exp(k))} | x=%.17g}",
            points[p]), "x")),
                       expected, 2e-10);
    }
}

static void test_gamma_existence_conditions(void)
{
    const char *sources[] = {"gamma(x)", "gamma(-2*x+1)"};
    for (size_t i = 0; i < 2; ++i) {
        const json_t *r = math_fourier_fields(lab_math_format("Fourier(%s,x,k)", sources[i]), "k");
        lab_math_equal(math_fourier_text(r, "value"), "NAN");
        lab_math_contains(math_fourier_text(r, "value_note"), "no ordinary or tempered-distribution", true);
        lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
    }
    const json_t *r = math_fourier_fields("@F{gamma(a+i*b*x)}", "k");
    lab_math_contains(math_fourier_text(r, "expression"), "Re(a) > 0", true);
    lab_math_contains(math_fourier_text(r, "expression"), "b ∈ ℝ", true);
    lab_math_contains(math_fourier_text(r, "expression"), "b ≠ 0", true);
    const char *rejected[] = {"@F{gamma(-1+i*x)}", "@F{asin(x+i)}", "@F{atanh(i*x)}", "@Finv{exp(k+exp(k))}"};
    for (size_t i = 0; i < 4; ++i)
        lab_math_contains(math_fourier_text(math_fourier_fields(rejected[i], "k"), "function"), "fourier(", true);
}

static void test_readme_gamma_fourier_pair(void)
{
    /* README examples: docs/expression.md, inverse-function and gamma pairs. */
    lab_math_equal(lab_math_algebra(math_fourier_fields("@F{gamma(1+i*x)}", "k")), "2π·exp(k - exp(k))");
    lab_math_equal(lab_math_algebra(math_fourier_fields("@Finv{2*@pi*exp(k-exp(k))}", "x")), "Γ(ix + 1)");
    const json_t *r = math_fourier_fields("Fourier(gamma(x),x,k)", "k");
    lab_math_equal(math_fourier_text(r, "value"), "NAN");
    lab_math_contains(math_fourier_text(r, "value_note"), "no ordinary or tempered-distribution", true);
}

static void test_readme_branch_fourier_round_trips(void)
{
    /* README examples: docs/expression.md, inverse-function table. */
    const char *functions[] = {"atanh", "asin", "acos", "acosh"};
    for (size_t f = 0; f < 4; ++f) {
        const json_t *s = math_fourier_fields(lab_math_format("@F{%s(x)}", functions[f]), "k");
        const json_t *r = math_fourier_fields(lab_math_format("InverseFourier(%s,k,x)", math_fourier_text(s, "expression")), "x");
        lab_math_equal(lab_math_algebra(r), lab_math_format("%s(x)", functions[f]));
    }
}

static void test_readme_acosh_spectrum(void)
{
    /* README examples: docs/expression.md, expanded acosh distribution. */
    const json_t *s = math_fourier_fields("@F{acosh(x)}", "k");
    lab_math_contains(math_fourier_text(s, "function"), "fourier(", false);
    lab_math_contains(math_fourier_text(s, "function"), "@eulermascheroni", true);
    lab_math_contains(math_fourier_text(s, "expression"), "δ(k)", true);
    const char *expected = "i*@pi^2*delta(k)+2*@pi*((ln(2)-@eulermascheroni)*delta(k)-besselj(0,k)*step(k)/k)";
    lab_math_equal(lab_math_algebra(math_fourier_fields(lab_math_format("(%s)-(%s)", lab_math_algebra(s), expected), "k")), "0");
}

/* Register ordinary regressions. */
void test_lab_math_fourier_branch_cases(void)
{
    TEST_RUN_IN_GROUP(test_copied_branch_round_trips, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_full_distribution_and_mathematical_notation, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_direct_spectra_and_zero_frequency_constants, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_legacy_derivative_spectra, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_acosh_boundary_values_and_constants, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_acosh_distribution_against_gaussian_test_functions, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_asin_distribution_against_gaussian_test_function, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_gamma_pairs_and_round_trips, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_gamma_independent_inverse_quadrature, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_gamma_existence_conditions, tests, NULL);
    lab_math_reset();
}

/* Register README examples last. */
void test_lab_math_fourier_branch_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_readme_gamma_fourier_pair, readme_examples, "math,readme,output");
    lab_math_reset();
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_readme_branch_fourier_round_trips, readme_examples, "math,readme,output");
    lab_math_reset();
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_readme_acosh_spectrum, readme_examples, "math,readme,output");
    lab_math_reset();
}
