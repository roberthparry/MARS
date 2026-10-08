/**
 * @file test_lab_math_fourier_atan.c
 * @brief Complete native arctangent Fourier regression port.
 *
 * Retains all original parameter grids, copied spectra, independent quadrature
 * and README cases, using the shared native worker fixtures.
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


static void test_basic_pair_both_directions(void)
{
    const char *operators[] = {"Fourier", "InverseFourier"};
    const double points[] = {-1.3, -0.2, 0.4, 2};
    for (size_t d = 0; d < 2; ++d)
        for (size_t p = 0; p < 4; ++p) {
            double x = points[p];
            const json_t *r = math_fourier_fields(lab_math_format("{%s(atan(x),x,k) | k=%.17g}", operators[d], x), "k");
            lab_math_close(math_fourier_number(r), (d ? 0.5*I : -I*M_PI)*exp(-fabs(x))/x, 1e-12);
            lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
        }
}

static void test_copied_formulas_round_trip(void)
{
    const char *arguments[] = {"x", "2*x+1", "-2*x+1", "x/2-1/3"};
    const char *operators[] = {"Fourier", "InverseFourier"};
    const double points[] = {-0.3, 0, 0.7};
    for (size_t a = 0; a < 4; ++a)
        for (size_t d = 0; d < 2; ++d) {
            const json_t *s = math_fourier_fields(lab_math_format("%s(atan(%s),x,k)", operators[d], arguments[a]), "k");
            const char *copies[] = {lab_math_algebra(s), math_fourier_text(s, "expression")};
            for (size_t c = 0; c < 2; ++c) {
                const json_t *r = math_fourier_fields(lab_math_format("%s(%s,k,x)", operators[1-d], copies[c]), "x");
                lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
                lab_math_contains(math_fourier_text(r, "expression"), "k =", false);
                for (size_t p = 0; p < 3; ++p) {
                    const char *bound = lab_math_replace(math_fourier_text(r, "expression"), "x = NAN",
                                                        lab_math_format("x = %.17g", points[p]));
                    lab_math_close(math_fourier_number(math_fourier_fields(bound, "x")),
                        math_fourier_number(math_fourier_fields(lab_math_format("{atan(%s) | x=%.17g}", arguments[a],
                            points[p]), "x")), 2e-12);
                }
                if (!a)
                    lab_math_equal(math_fourier_text(r, "unbound"), "atan(x) where (x ∈ ℝ)");
            }
        }
}

static void test_symbolic_real_scale_and_shift(void)
{
    const json_t *s = math_fourier_fields("Fourier(atan(a*x+b),x,k)", "k");
    const char *conditions[] = {"a ∈ ℝ", "b ∈ ℝ", "a ≠ 0", "k ≠ 0"};
    for (size_t i = 0; i < 4; ++i)
        lab_math_contains(math_fourier_text(s, "expression"), conditions[i], true);
    const json_t *r = math_fourier_fields(lab_math_format("InverseFourier(%s,k,x)", math_fourier_text(s, "expression")), "x");
    lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
    const double parameters[][2] = {{2, 0.3}, {-2, 0.3}, {0.5, -0.2}};
    for (size_t i = 0; i < 3; ++i) {
        double a = parameters[i][0], b = parameters[i][1];
        const json_t *value = math_fourier_fields(lab_math_format(
            "{Fourier(atan(a*x+b),x,k) | k=0.7; a=%.17g; b=%.17g}", a, b), "k");
        lab_math_close(math_fourier_number(value), -I*M_PI*copysign(1, a)*cexp(-0.7/fabs(a)+I*0.7*b/a)/0.7, 1e-12);
        const char *copy = lab_math_replace(math_fourier_text(r, "expression"), "x = NAN", "x = 0.7");
        copy = lab_math_replace(copy, "a = NAN", lab_math_format("a = %.17g", a));
        copy = lab_math_replace(copy, "b = NAN", lab_math_format("b = %.17g", b));
        lab_math_close(math_fourier_number(math_fourier_fields(copy, "x")), atan(a*0.7+b), 1e-12);
    }
}

static void test_direct_inverse_equivalent_spectra(void)
{
    const char *spectra[] = {"-i*@pi*exp(-abs(k))/k", "-i*@pi/(k*exp(abs(k)))",
        "-i*@pi*exp(-abs(k))*k^(-1)", "-i*@pi*exp(-abs(k)/2)*exp(-abs(k)/2)/k"};
    for (size_t i = 0; i < 4; ++i)
        lab_math_equal(math_fourier_text(math_fourier_fields(lab_math_format("@Finv{%s}", spectra[i]), "x"), "unbound"),
            "atan(x) where (x ∈ ℝ)");
}

static void test_domains_and_generated_conditional(void)
{
    const json_t *r = math_fourier_fields("@F{atan(x)}", "k");
    lab_math_contains(math_fourier_text(r, "expression"), "k ∈ ℝ", true);
    lab_math_contains(math_fourier_text(r, "expression"), "k ≠ 0", true);
    lab_math_contains(math_fourier_text(r, "function"), "k != 0", true);
    lab_math_contains(math_fourier_text(r, "function"), "return @nan.", true);
    const char *cards[] = {"expression", "tex", "function"};
    for (size_t i = 0; i < 3; ++i) {
        lab_math_contains(math_fourier_text(r, cards[i]), "principal value", false);
        lab_math_contains(math_fourier_text(r, cards[i]), "PV(", false);
    }
    lab_math_contains(math_fourier_text(r, "value_note"), "distributional", true);
    lab_math_equal(math_fourier_text(math_fourier_fields("{@F{atan(x)} | k=0}", "k"), "value"), "NAN");
    lab_math_equal(math_fourier_text(math_fourier_fields("{@F{atan(x)} | k=i}", "k"), "value"), "NAN");
    r = math_fourier_fields("@Finv{-i*@pi*exp(-abs(k))/k}", "x");
    lab_math_contains(math_fourier_text(r, "function"), "x != 0", false);
    lab_math_check(math_fourier_number(math_fourier_fields("{@Finv{-i*@pi*exp(-abs(k))/k} | x=0}", "x")) == 0, "inverse at zero");
}

static void test_damped_reciprocal_with_modulation(void)
{
    const char *operators[] = {"Fourier", "InverseFourier"};
    const double points[] = {-0.3, 0, 0.7};
    for (size_t d = 0; d < 2; ++d)
        for (size_t p = 0; p < 3; ++p)
            lab_math_close(math_fourier_number(math_fourier_fields(lab_math_format("{%s(exp(-2*abs(k)+3*i*k)/k,k,x) | x=%.17g}",
                operators[d], points[p]), "x")), (d ? I/M_PI : -2*I)*atan((points[p]+(d ? 3 : -3))/2), 1e-12);
}

static double complex math_fourier_atan_integrand(double k, void *context)
{
    double x = *(double *)context;
    return k == 0 ? x : exp(-k)*sin(k*x)/k;
}

static void test_quadrature_without_the_transform_formula(void)
{
    double points[] = {-1.3, 0, 0.4, 1.2};
    for (size_t p = 0; p < 4; ++p)
        lab_math_close(math_fourier_number(math_fourier_fields(lab_math_format(
            "{@Finv{-i*@pi*exp(-abs(k))/k} | x=%.17g}", points[p]), "x")),
            lab_math_simpson(math_fourier_atan_integrand, &points[p], 0, 32, 12000), 3e-10);
    for (int sign = -1; sign <= 1; sign += 2) {
        double point = sign*0.7;
        const json_t *rational = math_fourier_fields(lab_math_format("{Fourier(1/(1+x^2),x,k) | k=%.17g}", point), "k");
        const json_t *transformed = math_fourier_fields(lab_math_format("{@F{atan(x)} | k=%.17g}", point), "k");
        lab_math_close(I*point*math_fourier_number(transformed), math_fourier_number(rational), 1e-12);
    }
}

static void test_zero_rate_and_rejected_cases(void)
{
    lab_math_check(math_fourier_number(math_fourier_fields("@F{atan(0*x)}", "k")) == 0, "zero rate");
    const json_t *r = math_fourier_fields("@F{atan(0*x+1)}", "k");
    lab_math_contains(math_fourier_text(r, "expression"), "δ", true);
    lab_math_contains(math_fourier_text(r, "expression"), "k ≠ 0", false);
    const char *sources[] = {"@F{atan(i*x)}", "@F{atan(x+i)}", "@Finv{exp(abs(k))/k}",
        "@Finv{exp(-abs(k))/k^2}", "InverseFourier(exp(-abs(k))/k where (k-1 ≠ 0),k,x)"};
    for (size_t i = 0; i < 5; ++i)
        lab_math_contains(math_fourier_text(math_fourier_fields(sources[i], "x"), "function"), "fourier(", true);
}

static void test_readme_atan_fourier_pair(void)
{
    /* README examples: docs/expression.md, arctangent Fourier pair. */
    const json_t *r = math_fourier_fields("@F{atan(x)}", "k");
    lab_math_equal(lab_math_algebra(r), "-iπ·exp(-|k|)/k");
    lab_math_contains(math_fourier_text(r, "expression"), "k ∈ ℝ", true);
    lab_math_contains(math_fourier_text(r, "expression"), "k ≠ 0", true);
    lab_math_equal(math_fourier_text(math_fourier_fields("@Finv{-i*@pi*exp(-abs(k))/k}", "x"), "unbound"),
        "atan(x) where (x ∈ ℝ)");
}

/* Register ordinary regressions. */
void test_lab_math_fourier_atan_cases(void)
{
    TEST_RUN_IN_GROUP(test_basic_pair_both_directions, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_copied_formulas_round_trip, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_symbolic_real_scale_and_shift, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_direct_inverse_equivalent_spectra, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_domains_and_generated_conditional, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_damped_reciprocal_with_modulation, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_quadrature_without_the_transform_formula, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_zero_rate_and_rejected_cases, tests, NULL);
    lab_math_reset();
}

/* Register README examples last. */
void test_lab_math_fourier_atan_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_readme_atan_fourier_pair, readme_examples, "math,readme,output");
    lab_math_reset();
}
