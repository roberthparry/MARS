/**
 * @file test_lab_math_fourier_asinh.c
 * @brief Complete native asinh Fourier and modified-Bessel K regression port.
 *
 * Preserves all nine original cases, their quadratures, 80-digit independent
 * harmonic series and README example. Uses only the shared native worker arena.
 */
#include <complex.h>
#include <math.h>
#include <mpfr.h>
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


static double complex math_fourier_k_integrand(double t, void *context)
{
    const double complex *parameters = context;
    return cexp(-parameters[1] * cosh(t)) * ccosh(parameters[0] * t);
}

static double complex math_fourier_k_integral(double complex order, double complex argument)
{
    double complex parameters[] = {order, argument};
    return lab_math_simpson(math_fourier_k_integrand, parameters, 0, 8, 12000);
}

static void test_basic_pair_and_numeric_domain(void)
{
    const json_t *result = math_fourier_fields("@F{asinh(x)}", "k");
    const char *keys[] = {"function", "expression", "expression", "function", "function", "tex", "value_note"};
    const char *fragments[] = {"fourier(", "k ≠ 0", "k ∈ ℝ", "k != 0", "besselk(0,", "K_{0}", "distributional"};
    for (size_t i = 0; i < 7; ++i)
        lab_math_contains(math_fourier_text(result, keys[i]), fragments[i], i != 0);
    const char *cards[] = {"expression", "function", "tex"};
    for (size_t i = 0; i < 3; ++i) {
        lab_math_contains(math_fourier_text(result, cards[i]), "principal value", false);
        lab_math_contains(math_fourier_text(result, cards[i]), "PV(", false);
    }
    const char *operators[] = {"Fourier", "InverseFourier"};
    const double points[] = {-1.3, -0.4, 0.7, 2};
    for (size_t d = 0; d < 2; ++d)
        for (size_t p = 0; p < 4; ++p) {
            double k = points[p];
            result = math_fourier_fields(lab_math_format("{%s(asinh(x),x,k) | k=%.17g}", operators[d], k), "k");
            lab_math_close(math_fourier_number(result), (d ? I / M_PI : -2 * I) * math_fourier_k_integral(0, fabs(k)) / k, 2e-12);
        }
    lab_math_equal(math_fourier_text(math_fourier_fields("{@F{asinh(x)} | k=0}", "k"), "value"), "NAN");
    lab_math_equal(math_fourier_text(math_fourier_fields("{@F{asinh(x)} | k=i}", "k"), "value"), "NAN");
}

static void test_copied_spectra_both_directions(void)
{
    const char *arguments[] = {"x", "2*x+1", "-2*x+1", "x/2-1/3"};
    const char *operators[] = {"Fourier", "InverseFourier"};
    const double points[] = {-0.4, 0, 0.7};
    for (size_t a = 0; a < 4; ++a)
        for (size_t d = 0; d < 2; ++d) {
            const json_t *spectrum = math_fourier_fields(lab_math_format("%s(asinh(%s),x,k)", operators[d], arguments[a]), "k");
            const char *copies[] = {lab_math_algebra(spectrum), math_fourier_text(spectrum, "expression")};
            for (size_t c = 0; c < 2; ++c) {
                const json_t *result = math_fourier_fields(lab_math_format("%s(%s,k,x)", operators[1-d], copies[c]), "x");
                lab_math_contains(math_fourier_text(result, "function"), "fourier(", false);
                lab_math_contains(math_fourier_text(result, "function"), "x != 0", false);
                for (size_t p = 0; p < 3; ++p) {
                    const char *bound = lab_math_replace(math_fourier_text(result, "expression"), "x = NAN",
                                                        lab_math_format("x = %.17g", points[p]));
                    const json_t *expected = math_fourier_fields(lab_math_format("{asinh(%s) | x=%.17g}", arguments[a],
                        points[p]), "x");
                    lab_math_close(math_fourier_number(math_fourier_fields(bound, "x")), math_fourier_number(expected), 2e-12);
                }
            }
        }
}

static void test_symbolic_affine_pair(void)
{
    const json_t *spectrum = math_fourier_fields("Fourier(asinh(a*x+b),x,k)", "k");
    const char *conditions[] = {"a ∈ ℝ", "b ∈ ℝ", "a ≠ 0", "k ≠ 0"};
    for (size_t i = 0; i < 4; ++i)
        lab_math_contains(math_fourier_text(spectrum, "expression"), conditions[i], true);
    const json_t *restored = math_fourier_fields(lab_math_format("InverseFourier(%s,k,x)", math_fourier_text(spectrum,
        "expression")), "x");
    lab_math_contains(math_fourier_text(restored, "function"), "fourier(", false);
    const double rates[] = {-2, 0.5, 2};
    for (size_t i = 0; i < 3; ++i) {
        const char *copy = lab_math_replace(math_fourier_text(restored, "expression"), "x = NAN", "x = 0.3");
        copy = lab_math_replace(copy, "a = NAN", lab_math_format("a = %.17g", rates[i]));
        copy = lab_math_replace(copy, "b = NAN", "b = 0.2");
        lab_math_close(math_fourier_number(math_fourier_fields(copy, "x")), asinh(rates[i]*0.3+0.2), 2e-12);
    }
}

static void test_direct_formula_modulation_and_rejections(void)
{
    const char *spectra[] = {"-2i*K0(abs(k))/k", "-2*i*besselk(0,abs(k))*k^(-1)", "-2i*K_0(abs(k))/k"};
    for (size_t i = 0; i < 3; ++i)
        lab_math_equal(lab_math_algebra(math_fourier_fields(lab_math_format("@Finv{%s}", spectra[i]), "x")), "asinh(x)");
    const double points[] = {-0.5, 0, 0.7};
    for (size_t i = 0; i < 3; ++i)
        lab_math_close(math_fourier_number(math_fourier_fields(lab_math_format(
            "{InverseFourier(exp(3*i*k)*K0(2*abs(k))/k,k,x) | x=%.17g}", points[i]), "x")),
            0.5*I*asinh((points[i]+3)/2), 2e-12);
    const char *rejected[] = {"@F{asinh(i*x)}", "@F{asinh(x+i)}", "@Finv{K0(abs(k))/k^2}",
        "@Finv{K0(-abs(k))/k}", "@Finv{K_1(abs(k))/k}",
        "InverseFourier(K0(abs(k))/k where (k-1 != 0),k,x)"};
    for (size_t i = 0; i < 6; ++i)
        lab_math_contains(math_fourier_text(math_fourier_fields(rejected[i], "x"), "function"), "fourier(", true);
    lab_math_check(math_fourier_number(math_fourier_fields("@F{asinh(0*x)}", "k")) == 0, "zero affine rate");
}

static double complex math_fourier_asinh_integrand(double t, void *context)
{
    return atan(*(double *)context / cosh(t));
}

static void test_inverse_by_independent_integral(void)
{
    double points[] = {-1.3, 0, 0.4, 1.2};
    for (size_t i = 0; i < 4; ++i) {
        double complex expected = 2/M_PI * lab_math_simpson(math_fourier_asinh_integrand, &points[i], 0, 32, 12000);
        lab_math_close(math_fourier_number(math_fourier_fields(lab_math_format("{@Finv{-2i*K0(abs(k))/k} | x=%.17g}",
            points[i]), "x")),
                       expected, 2e-12);
    }
}

static void test_requested_precision_is_preserved(void)
{
    /* Independent 139-term harmonic/I0 series, evaluated beyond the original 100 decimal digits. */
    mpfr_t gamma, term, i0, harmonic, weighted, temporary, expected, actual, tolerance;
    mpfr_inits2(384, gamma, term, i0, harmonic, weighted, temporary, expected, actual, tolerance, (mpfr_ptr)0);
    mpfr_set_str(gamma, "0.5772156649015328606065120900824024310421593359399235988057672348848677267776646709369470632917467495",
                 10, MPFR_RNDN);
    mpfr_set_ui(term, 1, MPFR_RNDN);
    mpfr_set_ui(i0, 1, MPFR_RNDN);
    mpfr_set_zero(harmonic, 0);
    mpfr_set_zero(weighted, 0);
    for (unsigned n = 1; n < 140; ++n) {
        mpfr_div_ui(term, term, 4*n*n, MPFR_RNDN);
        mpfr_set_ui(temporary, n, MPFR_RNDN);
        mpfr_ui_div(temporary, 1, temporary, MPFR_RNDN);
        mpfr_add(harmonic, harmonic, temporary, MPFR_RNDN);
        mpfr_add(i0, i0, term, MPFR_RNDN);
        mpfr_mul(temporary, harmonic, term, MPFR_RNDN);
        mpfr_add(weighted, weighted, temporary, MPFR_RNDN);
    }
    mpfr_set_d(temporary, 0.5, MPFR_RNDN);
    mpfr_log(temporary, temporary, MPFR_RNDN);
    mpfr_add(temporary, temporary, gamma, MPFR_RNDN);
    mpfr_mul(temporary, temporary, i0, MPFR_RNDN);
    mpfr_sub(expected, weighted, temporary, MPFR_RNDN);
    const json_t *result = lab_math_fields("K0(1)", "x", "evaluate", 80);
    bool parsed = mpfr_set_str(actual, math_fourier_text(result, "value"), 10, MPFR_RNDN) == 0;
    mpfr_sub(actual, actual, expected, MPFR_RNDN);
    mpfr_abs(actual, actual, MPFR_RNDN);
    mpfr_set_str(tolerance, "1e-77", 10, MPFR_RNDN);
    lab_math_check(parsed && mpfr_less_p(actual, tolerance), "80-digit K0 agrees with independent harmonic series");
    mpfr_clears(gamma, term, i0, harmonic, weighted, temporary, expected, actual, tolerance, (mpfr_ptr)0);
    const char *arguments[] = {"0.2", "2", "30", "1+i"};
    for (size_t i = 0; i < 4; ++i) {
        result = lab_math_fields(lab_math_format("besselk(1/2,%s)/(sqrt(@pi/(2*(%s)))*exp(-(%s)))-1",
                                                arguments[i], arguments[i], arguments[i]), "x", "evaluate", 80);
        lab_math_close(math_fourier_number(result), 0, 1e-70);
    }
}

static void test_numeric_orders_and_complex_arguments(void)
{
    const double complex orders[] = {0, 1, 2, -3, 0.5, 0.3, 1.0000001, 0.3+0.2*I};
    const char *order_text[] = {"0", "1", "2", "-3", "0.5", "0.3", "1.0000001", "0.3+0.2i"};
    const double complex arguments[] = {0.4, 2, 1+0.5*I};
    const char *argument_text[] = {"0.4", "2", "1+0.5i"};
    for (size_t n = 0; n < 8; ++n)
        for (size_t z = 0; z < 3; ++z) {
            double complex expected = math_fourier_k_integral(orders[n], arguments[z]);
            lab_math_close(math_fourier_number(math_fourier_fields(lab_math_format("besselk(%s,%s)", order_text[n],
                argument_text[z]), "k")),
                           expected, 2e-11*fmax(1, cabs(expected)));
        }
}

static void test_aliases_derivative_integral_and_sum(void)
{
    const char *unary[] = {"K0", "K_0", "K₀"};
    const char *binary[] = {"besselk", "bessel_k", "BesselK"};
    for (size_t i = 0; i < 3; ++i) {
        lab_math_places(creal(math_fourier_number(math_fourier_fields(lab_math_format("%s(1)", unary[i]), "k"))),
            0.42102443824070833, 14);
        lab_math_places(creal(math_fourier_number(math_fourier_fields(lab_math_format("%s(0,1)", binary[i]), "k"))),
            0.42102443824070833, 14);
    }
    lab_math_close(math_fourier_number(math_fourier_fields("{Dx(K0(x)) | x=1}", "x")), -math_fourier_k_integral(1, 1), 2e-12);
    for (int n = 0; n < 4; ++n) {
        const json_t *primitive = lab_math_fields(lab_math_format("K_%d(x)", n), "x", "integral", 40);
        lab_math_contains(math_fourier_text(primitive, "integral_function"), "return integral(besselk(", true);
        const char *expression = lab_math_after(math_fourier_text(primitive, "integral"), " = ");
        lab_math_contains(expression, "∫", false);
        const json_t *result = lab_math_fields(lab_math_replace(expression, "x = NAN", "x = 1"),
                                               "x", "derivative", 40);
        lab_math_close(lab_math_number(result, "derivative_value"), math_fourier_k_integral(n, 1), 2e-11);
    }
    lab_math_close(math_fourier_number(math_fourier_fields("sum(n,0,2,besselk(n,1))", "k")),
                   math_fourier_k_integral(0, 1)+math_fourier_k_integral(1, 1)+math_fourier_k_integral(2, 1), 2e-11);
}

static void test_readme_asinh_pair(void)
{
    /* README examples: docs/expression.md, inverse hyperbolic sine Fourier pair. */
    const json_t *result = math_fourier_fields("@F{asinh(x)}", "k");
    lab_math_contains(math_fourier_text(result, "expression"), "k ≠ 0", true);
    lab_math_contains(math_fourier_text(result, "tex"), "K_{0}", true);
    lab_math_equal(lab_math_algebra(math_fourier_fields("@Finv{-2i*K0(abs(k))/k}", "x")), "asinh(x)");
}

/* Register the complete original regression cases. */
void test_lab_math_fourier_asinh_cases(void)
{
    TEST_RUN_IN_GROUP(test_basic_pair_and_numeric_domain, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_copied_spectra_both_directions, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_symbolic_affine_pair, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_direct_formula_modulation_and_rejections, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_inverse_by_independent_integral, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_requested_precision_is_preserved, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_numeric_orders_and_complex_arguments, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_aliases_derivative_integral_and_sum, tests, NULL);
    lab_math_reset();
}

/* Register README examples after ordinary cases. */
void test_lab_math_fourier_asinh_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_readme_asinh_pair, readme_examples, "math,readme,output");
    lab_math_reset();
}
