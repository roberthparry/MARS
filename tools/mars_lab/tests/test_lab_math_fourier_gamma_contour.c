/**
 * @file test_lab_math_fourier_gamma_contour.c
 * @brief Complete native vertical-contour gamma Fourier regression port.
 *
 * Preserves Cartesian coordinates, affine round trips, independent complex
 * quadrature and the README examples without changing the shared helper.
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


static void test_default_coordinate_and_conditions(void)
{
    const json_t *r = math_fourier_fields("@F{gamma(x)}", "k");
    lab_math_equal(lab_math_algebra(r), "2π·exp(k·Re(x) - exp(k))");
    lab_math_contains(math_fourier_text(r, "transform_identity_TeX"), "\\operatorname{Im}(x)\\to k", true);
    lab_math_contains(math_fourier_text(r, "expression"), "Re(x) > 0", true);
    lab_math_contains(math_fourier_text(r, "expression"), "k ∈ ℝ", true);
    lab_math_contains(math_fourier_text(r, "function"), "realpart(x) > 0", true);
    lab_math_contains(math_fourier_text(r, "value_note"), "holding the real coordinate fixed", true);
    lab_math_equal(lab_math_algebra(math_fourier_fields("Fourier(gamma(x),Im(x),k)", "k")), lab_math_algebra(r));
}

static void test_copied_spectra_restore_complex_argument(void)
{
    const char *coordinates[][2] = {{"x", "k"}, {"t", "ω"}, {"y", "m"}, {"z", "n"}};
    const char *points[] = {"1+0.4i", "2-0.7i"};
    for (size_t i = 0; i < 4; ++i) {
        const char *x = coordinates[i][0];
        const json_t *s = math_fourier_fields(lab_math_format("@F{gamma(%s)}", x), coordinates[i][1]);
        const char *copies[] = {lab_math_algebra(s), math_fourier_text(s, "expression")};
        for (size_t c = 0; c < 2; ++c) {
            const json_t *r = math_fourier_fields(lab_math_format("@Finv{%s}", copies[c]), x);
            lab_math_equal(lab_math_algebra(r), lab_math_format("Γ(%s)", x));
            lab_math_contains(math_fourier_text(r, "expression"), lab_math_format("Re(%s) > 0", x), true);
            lab_math_contains(math_fourier_text(r, "expression"), lab_math_format("%s ∈ ℝ", x), false);
            for (size_t p = 0; p < 2; ++p) {
                const char *bound = lab_math_replace(math_fourier_text(r, "expression"), lab_math_format("%s = NAN", x),
                                                     lab_math_format("%s = %s", x, points[p]));
                lab_math_close(math_fourier_number(math_fourier_fields(bound, x)), math_fourier_number(
                    math_fourier_fields(lab_math_format("gamma(%s)", points[p]), x)), 1e-12);
            }
        }
    }
}

static void test_both_directions_and_affine_arguments(void)
{
    const char *arguments[] = {"x", "2*x+1", "-2*x+1", "2*x+1+i/3"};
    const char *operators[] = {"Fourier", "InverseFourier"};
    const char *points[] = {"0.2+0.7i", "0.1-0.4i"};
    for (size_t a = 0; a < 4; ++a)
        for (size_t d = 0; d < 2; ++d) {
            const json_t *s = math_fourier_fields(lab_math_format("%s(gamma(%s),Im(x),k)", operators[d], arguments[a]), "k");
            const json_t *r = math_fourier_fields(lab_math_format("%s(%s,k,Im(x))", operators[1-d], math_fourier_text(s,
                "expression")), "x");
            lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
            for (size_t p = 0; p < 2; ++p) {
                const char *bound = lab_math_replace(math_fourier_text(r, "expression"), "x = NAN", lab_math_format(
                    "x = %s", points[p]));
                lab_math_close(math_fourier_number(math_fourier_fields(bound, "x")),
                    math_fourier_number(math_fourier_fields(lab_math_format("{gamma(%s) | x=%s}", arguments[a],
                        points[p]), "x")), 1e-12);
            }
        }
}

static void test_numeric_spectrum_and_domain(void)
{
    const char *points[] = {"2+0.3i", "2-4i"};
    for (size_t p = 0; p < 2; ++p) {
        const json_t *r = math_fourier_fields(lab_math_format("{@F{gamma(x)} | x=%s; k=1}", points[p]), "k");
        lab_math_close(math_fourier_number(r), 2*M_PI*exp(2-exp(1)), 1e-12);
        lab_math_contains(math_fourier_text(r, "expression"), "Re(x)", true);
    }
    const char *invalid[][2] = {{"-1+i", "1"}, {"i", "1"}, {"2+i", "i"}};
    for (size_t p = 0; p < 3; ++p)
        lab_math_equal(math_fourier_text(math_fourier_fields(lab_math_format("{@F{gamma(x)} | x=%s; k=%s}",
            invalid[p][0], invalid[p][1]), "k"),
                            "value"), "NAN");
}

static double complex math_fourier_gamma_integrand(double k, void *context)
{
    double complex z = *(double complex *)context;
    return cexp(creal(z)*k-exp(k)+I*cimag(z)*k);
}

static void test_independent_inverse_integral(void)
{
    double complex points[] = {1+0.4*I, 2-0.7*I};
    const char *bindings[] = {"x = 1+0.4i", "x = 2-0.7i"};
    for (size_t p = 0; p < 2; ++p) {
        double complex integral = lab_math_simpson(math_fourier_gamma_integrand, &points[p], -40, 6, 12000);
        const json_t *r = math_fourier_fields("@Finv{2*@pi*exp(k*Re(x)-exp(k))}", "x");
        lab_math_close(math_fourier_number(math_fourier_fields(lab_math_replace(math_fourier_text(r, "expression"),
            "x = NAN", bindings[p]), "x")), integral, 2e-11);
    }
}

static void test_aliases_and_real_axis_are_not_reinterpreted(void)
{
    const char *real_aliases[] = {"Re", "realpart", "real_part", "ℜ"};
    const char *imag_aliases[] = {"Im", "imagpart", "imag_part", "ℑ"};
    for (size_t i = 0; i < 4; ++i) {
        lab_math_check(math_fourier_number(math_fourier_fields(lab_math_format("%s(2+3i)", real_aliases[i]), "k")) == 2,
            "real-part alias");
        lab_math_check(math_fourier_number(math_fourier_fields(lab_math_format("%s(2+3i)", imag_aliases[i]), "k")) == 3,
            "imaginary-part alias");
        lab_math_equal(lab_math_algebra(math_fourier_fields(lab_math_format("Fourier(gamma(x),%s(x),k)", imag_aliases[i]), "k")),
                       "2π·exp(k·Re(x) - exp(k))");
    }
    lab_math_equal(lab_math_algebra(math_fourier_fields("@F{gamma(a+x*i)}", "k")),
                   lab_math_algebra(math_fourier_fields("@F{gamma(a+ix)}", "k")));
    lab_math_contains(math_fourier_text(math_fourier_fields("a+xi", "k"), "expression"), "ξ", true);
    const json_t *r = math_fourier_fields("Fourier(gamma(x),x,k)", "k");
    lab_math_equal(math_fourier_text(r, "value"), "NAN");
    lab_math_contains(math_fourier_text(r, "value_note"), "no ordinary or tempered-distribution", true);
}

static void test_readme_vertical_line_gamma_pair(void)
{
    /* README examples: docs/expression.md, vertical-line gamma table and coordinate. */
    const char *sources[] = {"@F{gamma(x)}", "Fourier(gamma(x),Im(x),k)"};
    for (size_t i = 0; i < 2; ++i) {
        const json_t *r = math_fourier_fields(sources[i], "k");
        lab_math_equal(lab_math_algebra(r), "2π·exp(k·Re(x) - exp(k))");
        lab_math_contains(math_fourier_text(r, "expression"), "Re(x) > 0", true);
    }
    const json_t *r = math_fourier_fields("@Finv{2*@pi*exp(k*Re(x)-exp(k))}", "x");
    lab_math_equal(lab_math_algebra(r), "Γ(x)");
    lab_math_contains(math_fourier_text(r, "expression"), "Re(x) > 0", true);
}

/* Register ordinary regressions. */
void test_lab_math_fourier_gamma_contour_cases(void)
{
    TEST_RUN_IN_GROUP(test_default_coordinate_and_conditions, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_copied_spectra_restore_complex_argument, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_both_directions_and_affine_arguments, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_numeric_spectrum_and_domain, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_independent_inverse_integral, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_aliases_and_real_axis_are_not_reinterpreted, tests, NULL);
    lab_math_reset();
}

/* Register README examples last. */
void test_lab_math_fourier_gamma_contour_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_readme_vertical_line_gamma_pair, readme_examples, "math,readme,output");
    lab_math_reset();
}
