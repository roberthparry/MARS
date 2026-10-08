/**
 * @file test_lab_math_fourier_tanh.c
 * @brief Complete native odd-hyperbolic Fourier regression port.
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


static void test_basic_pairs_both_directions(void)
{
    const char *operators[] = {"Fourier", "InverseFourier"};
    const char *functions[] = {"tanh", "csch", "coth"};
    const double points[] = {-1.1, 0.4, 1.3};
    for (size_t d = 0; d < 2; ++d)
        for (size_t f = 0; f < 3; ++f)
            for (size_t p = 0; p < 3; ++p) {
                double z = M_PI*points[p]/2;
                double dual = f == 0 ? 1/sinh(z) : f == 1 ? tanh(z) : 1/tanh(z);
                const json_t *r = math_fourier_fields(lab_math_format("{%s(%s(x),x,k) | k=%.17g}",
                    operators[d], functions[f], points[p]), "k");
                lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
                lab_math_close(math_fourier_number(r), (d ? 0.5*I : -I*M_PI)*dual, 1e-12);
            }
}

static void test_numerical_domain_and_conditional(void)
{
    const json_t *r = math_fourier_fields("@F{tanh(x)}", "k");
    const char *keys[] = {"expression", "expression", "function", "function", "function"};
    const char *fragments[] = {"k ∈ ℝ", "k ≠ 0", "k != 0", "else", "return @nan."};
    for (size_t i = 0; i < 5; ++i)
        lab_math_contains(math_fourier_text(r, keys[i]), fragments[i], true);
    lab_math_contains(math_fourier_text(r, "tex"), "\\left(-i\\right)", false);
    lab_math_contains(math_fourier_text(r, "expression"), "(-i)", false);
    const char *cards[] = {"expression", "tex", "function"};
    for (size_t i = 0; i < 3; ++i) {
        lab_math_contains(math_fourier_text(r, cards[i]), "principal value", false);
        lab_math_contains(math_fourier_text(r, cards[i]), "PV(", false);
    }
    lab_math_equal(math_fourier_text(math_fourier_fields("{@F{tanh(x)} | k=0}", "k"), "value"), "NAN");
    lab_math_equal(math_fourier_text(math_fourier_fields("{@F{tanh(x)} | k=i}", "k"), "value"), "NAN");
    lab_math_check(math_fourier_number(math_fourier_fields("{@Finv{-i*@pi*csch(@pi*k/2)} | x=0}", "x")) == 0, "inverse at zero");
}

static void test_copied_formulas_round_trip(void)
{
    const char *functions[] = {"tanh", "csch", "coth"};
    const char *arguments[] = {"x", "2*x+1", "-2*x+1", "x/2-1/3"};
    const char *operators[] = {"Fourier", "InverseFourier"};
    const double points[] = {-0.3, 0.7};
    for (size_t f = 0; f < 3; ++f)
        for (size_t a = 0; a < 4; ++a)
            for (size_t d = 0; d < 2; ++d) {
                const json_t *s = math_fourier_fields(lab_math_format("%s(%s(%s),x,k)", operators[d], functions[f],
                    arguments[a]), "k");
                const char *copies[] = {lab_math_algebra(s), math_fourier_text(s, "expression")};
                const json_t *r = NULL;
                for (size_t c = 0; c < 2; ++c) {
                    r = math_fourier_fields(lab_math_format("%s(%s,k,x)", operators[1-d], copies[c]), "x");
                    lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
                    lab_math_contains(math_fourier_text(r, "transform_identity_TeX"), "k\\to x", true);
                    lab_math_contains(math_fourier_text(r, "transform_identity_TeX"), "NAN", false);
                    for (size_t p = 0; p < 2; ++p) {
                        const char *bound = lab_math_replace(math_fourier_text(r, "expression"), "x = NAN",
                                                            lab_math_format("x = %.17g", points[p]));
                        const json_t *expected = math_fourier_fields(lab_math_format("{%s(%s) | x=%.17g}",
                                                                        functions[f], arguments[a], points[p]), "x");
                        lab_math_close(math_fourier_number(math_fourier_fields(bound, "x")), math_fourier_number(
                            expected), 2e-12);
                    }
                }
                if (!a && f != 1)
                    lab_math_equal(lab_math_algebra(r), f == 0 ? "tanh(x)" : "coth(x)");
                if (!a && f == 2)
                    lab_math_contains(math_fourier_text(r, "expression"), "x ≠ 0", true);
            }
}

static void test_direct_inverse_and_reciprocal_aliases(void)
{
    const json_t *expected = math_fourier_fields("@Finv{-i*@pi*csch(@pi*k/2)}", "x");
    lab_math_equal(math_fourier_text(expected, "unbound"), "tanh(x) where (x ∈ ℝ)");
    const char *spectra[] = {"-i*@pi/sinh(@pi*k/2)", "-i*@pi*sinh(@pi*k/2)^(-1)", "-i*@pi*cosech(@pi*k/2)"};
    for (size_t i = 0; i < 3; ++i)
        lab_math_equal(math_fourier_text(math_fourier_fields(lab_math_format("@Finv{%s}", spectra[i]), "x"), "tex"),
            math_fourier_text(expected, "tex"));
    const char *sources[] = {"csch(x)", "cosech(x)", "1/sinh(x)", "sinh(x)^(-1)"};
    for (size_t i = 0; i < 4; ++i) {
        const json_t *r = math_fourier_fields(lab_math_format("@F{%s}", sources[i]), "k");
        lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
        lab_math_contains(math_fourier_text(r, "expression"), "k ≠ 0", false);
    }
}

static void test_symbolic_scale_shift_and_copied_inverse(void)
{
    const char *functions[] = {"tanh", "csch", "coth"};
    const char *conditions[] = {"a ∈ ℝ", "b ∈ ℝ", "a ≠ 0"};
    const double parameters[][2] = {{2, 0.3}, {-2, 0.3}, {0.5, -0.2}};
    for (size_t f = 0; f < 3; ++f) {
        const json_t *s = math_fourier_fields(lab_math_format("Fourier(%s(a*x+b),x,k)", functions[f]), "k");
        for (size_t i = 0; i < 3; ++i)
            lab_math_contains(math_fourier_text(s, "expression"), conditions[i], true);
        const json_t *r = math_fourier_fields(lab_math_format("InverseFourier(%s,k,x)", math_fourier_text(s, "expression")), "x");
        lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
        for (size_t p = 0; p < 3; ++p) {
            double a = parameters[p][0], b = parameters[p][1], z = M_PI*0.7/(2*a);
            double dual = f == 0 ? 1/sinh(z) : f == 1 ? tanh(z) : 1/tanh(z);
            const json_t *value = math_fourier_fields(lab_math_format("{Fourier(%s(a*x+b),x,k) | k=0.7; a=%.17g; b=%.17g}",
                                                        functions[f], a, b), "k");
            lab_math_close(math_fourier_number(value), -I*M_PI/fabs(a)*cexp(I*0.7*b/a)*dual, 1e-12);
            const char *copy = lab_math_replace(math_fourier_text(r, "expression"), "x = NAN", "x = 0.7");
            copy = lab_math_replace(copy, "a = NAN", lab_math_format("a = %.17g", a));
            copy = lab_math_replace(copy, "b = NAN", lab_math_format("b = %.17g", b));
            z = a*0.7+b;
            double original = f == 0 ? tanh(z) : f == 1 ? 1/sinh(z) : 1/tanh(z);
            lab_math_close(math_fourier_number(math_fourier_fields(copy, "x")), original, 1e-12);
        }
    }
}

static double complex math_fourier_tanh_derivative_integrand(double x, void *context)
{
    double point = *(double *)context;
    return cos(point*x)/pow(cosh(x), 2);
}

static double complex math_fourier_csch_integrand(double x, void *context)
{
    double point = *(double *)context;
    return x == 0 ? point : sin(point*x)/sinh(x);
}

static double complex math_fourier_coth_integrand(double x, void *context)
{
    double point = *(double *)context;
    return x == 0 ? point/2 : sin(point*x)/expm1(2*x);
}

static void test_quadrature_independent_of_the_transform_formula(void)
{
    double points[] = {-1.3, 0.3, 1.2};
    for (size_t p = 0; p < 3; ++p) {
        double k = points[p];
        double complex derivative = lab_math_simpson(math_fourier_tanh_derivative_integrand, &points[p], -24, 24, 12000);
        lab_math_close(math_fourier_number(math_fourier_fields(lab_math_format("{@F{tanh(x)} | k=%.17g}", k), "k")),
            derivative/(I*k), 3e-10);
        double complex integral = lab_math_simpson(math_fourier_csch_integrand, &points[p], 0, 32, 12000);
        lab_math_close(math_fourier_number(math_fourier_fields(lab_math_format("{@F{csch(x)} | k=%.17g}", k), "k")),
            -2*I*integral, 3e-10);
        double complex correction = lab_math_simpson(math_fourier_coth_integrand, &points[p], 0, 32, 12000);
        lab_math_close(math_fourier_number(math_fourier_fields(lab_math_format("{@F{coth(x)} | k=%.17g}", k), "k")),
            -2*I/k-4*I*correction, 3e-10);
    }
}

static void test_coth_aliases_poles_and_conditional(void)
{
    const json_t *expected = math_fourier_fields("@F{coth(x)}", "k");
    const char *sources[] = {"1/tanh(x)", "tanh(x)^(-1)", "cosh(x)/sinh(x)"};
    for (size_t i = 0; i < 3; ++i)
        lab_math_equal(math_fourier_text(math_fourier_fields(lab_math_format("@F{%s}", sources[i]), "k"), "tex"),
            math_fourier_text(expected, "tex"));
    const char *cards[] = {"expression", "tex", "function"};
    for (size_t i = 0; i < 3; ++i) {
        lab_math_contains(math_fourier_text(expected, cards[i]), "principal value", false);
        lab_math_contains(math_fourier_text(expected, cards[i]), "PV(", false);
    }
    lab_math_contains(math_fourier_text(expected, "function"), "k != 0", true);
    const json_t *inverse = math_fourier_fields("@Finv{-i*@pi*coth(@pi*k/2)}", "x");
    lab_math_equal(math_fourier_text(inverse, "unbound"), "coth(x) where (x ∈ ℝ; x ≠ 0)");
    lab_math_contains(math_fourier_text(inverse, "function"), "x != 0", true);
    const char *points[] = {"0", "i"};
    for (size_t i = 0; i < 2; ++i) {
        lab_math_equal(math_fourier_text(math_fourier_fields(lab_math_format("{@F{coth(x)} | k=%s}", points[i]), "k"),
            "value"), "NAN");
        lab_math_equal(math_fourier_text(math_fourier_fields(lab_math_format("{@Finv{-i*@pi*coth(@pi*k/2)} | x=%s}",
            points[i]), "x"), "value"), "NAN");
    }
    const json_t *shifted = math_fourier_fields("Fourier(coth(2*x+1),x,k)", "k");
    const json_t *restored = math_fourier_fields(lab_math_format("InverseFourier(%s,k,x)", math_fourier_text(shifted,
        "expression")), "x");
    lab_math_equal(math_fourier_text(math_fourier_fields(lab_math_replace(math_fourier_text(restored, "expression"),
        "x = NAN", "x = -1/2"), "x"), "value"), "NAN");
    const char *reciprocals[] = {"1/coth(x)", "coth(x)^(-1)", "sinh(x)/cosh(x)"};
    for (size_t i = 0; i < 3; ++i)
        lab_math_equal(math_fourier_text(math_fourier_fields(lab_math_format("@F{%s}", reciprocals[i]), "k"), "tex"),
                       math_fourier_text(math_fourier_fields("@F{tanh(x)}", "k"), "tex"));
}

static void test_unrelated_domains_and_nonreal_rates_are_not_accepted(void)
{
    const char *sources[] = {"Fourier(csch(x) where (x-1 ≠ 0),x,k)", "@F{tanh(i*x)}", "@F{csch(x+i)}",
        "Fourier(coth(x) where (x-1 ≠ 0),x,k)", "@F{coth(i*x)}", "@F{coth(x+i)}"};
    for (size_t i = 0; i < 6; ++i)
        lab_math_contains(math_fourier_text(math_fourier_fields(sources[i], "k"), "function"), "fourier(", true);
}

static void test_zero_rate_is_a_constant_transform(void)
{
    const char *sources[] = {"@F{tanh(0*x+1)}", "@F{coth(0*x+1)}"};
    for (size_t i = 0; i < 2; ++i) {
        const json_t *r = math_fourier_fields(sources[i], "k");
        lab_math_contains(math_fourier_text(r, "expression"), "δ", true);
        lab_math_contains(math_fourier_text(r, "expression"), "k ≠ 0", false);
    }
    lab_math_check(math_fourier_number(math_fourier_fields("@F{tanh(0*x)}", "k")) == 0, "zero constant tanh");
}

static void test_readme_coth_fourier_pair(void)
{
    /* README examples: docs/expression.md, odd hyperbolic pairs. */
    const json_t *s = math_fourier_fields("@F{coth(x)}", "k");
    lab_math_contains(math_fourier_text(s, "expression"), "coth(½πk)", true);
    lab_math_contains(math_fourier_text(s, "expression"), "k ∈ ℝ", true);
    lab_math_contains(math_fourier_text(s, "expression"), "k ≠ 0", true);
    lab_math_equal(math_fourier_text(math_fourier_fields("@Finv{-i*@pi*coth(@pi*k/2)}", "x"), "unbound"),
        "coth(x) where (x ∈ ℝ; x ≠ 0)");
}

static void test_readme_tanh_fourier_pair(void)
{
    /* README examples: docs/expression.md, odd hyperbolic pairs. */
    const json_t *s = math_fourier_fields("@F{tanh(x)}", "k");
    lab_math_contains(math_fourier_text(s, "expression"), "cosech(½πk)", true);
    lab_math_contains(math_fourier_text(s, "expression"), "k ∈ ℝ", true);
    lab_math_contains(math_fourier_text(s, "expression"), "k ≠ 0", true);
    const char *sources[] = {"@Finv{-i*@pi*csch(@pi*k/2)}", "@Finv{-i*@pi/sinh(@pi*k/2)}"};
    for (size_t i = 0; i < 2; ++i)
        lab_math_equal(math_fourier_text(math_fourier_fields(sources[i], "x"), "unbound"), "tanh(x) where (x ∈ ℝ)");
}

/* Register ordinary regressions. */
void test_lab_math_fourier_tanh_cases(void)
{
    TEST_RUN_IN_GROUP(test_basic_pairs_both_directions, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_numerical_domain_and_conditional, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_copied_formulas_round_trip, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_direct_inverse_and_reciprocal_aliases, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_symbolic_scale_shift_and_copied_inverse, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_quadrature_independent_of_the_transform_formula, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_coth_aliases_poles_and_conditional, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_unrelated_domains_and_nonreal_rates_are_not_accepted, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_zero_rate_is_a_constant_transform, tests, NULL);
    lab_math_reset();
}

/* Register README examples last. */
void test_lab_math_fourier_tanh_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_readme_coth_fourier_pair, readme_examples, "math,readme,output");
    lab_math_reset();
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_readme_tanh_fourier_pair, readme_examples, "math,readme,output");
    lab_math_reset();
}
