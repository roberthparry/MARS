/**
 * @file test_lab_math_fourier_periodic.c
 * @brief Native periodic principal-value Fourier regression port.
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


static const json_t *math_fourier_copied_inverse(const char *source, const char *forward, const char *inverse,
    const json_t **spectrum)
{
    *spectrum = math_fourier_fields(lab_math_format("%s(%s,t,ω)", forward, source), "ω");
    lab_math_contains(math_fourier_text(*spectrum, "function"), "fourier(", false);
    lab_math_contains(math_fourier_text(*spectrum, "tex"), "\\sum", true);
    const json_t *r = math_fourier_fields(lab_math_format("%s(%s,ω,t)", inverse, lab_math_algebra(*spectrum)), "t");
    lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
    lab_math_contains(math_fourier_text(r, "tex"), "principal_value(", false);
    lab_math_contains(math_fourier_text(r, "expression"), "≠ 0", true);
    const char *cards[] = {"expression", "tex", "function"};
    for (size_t i = 0; i < 3; ++i)
        lab_math_contains(math_fourier_text(r, cards[i]), "principal value", false);
    return r;
}

static double complex math_fourier_numerical(const char *source, double point, const char *bindings)
{
    return math_fourier_number(math_fourier_fields(lab_math_format("{%s | t=%.17g%s}", source, point, bindings), "t"));
}

static void test_copied_periodic_pairs_both_directions(void)
{
    const char *functions[] = {"tan", "cot"}, *operators[] = {"Fourier", "InverseFourier"};
    const char *arguments[] = {"t", "2*t+1", "-2*t+1", "t/2-1/3"};
    const double points[] = {-0.3, 0.7};
    for (size_t f = 0; f < 2; ++f)
        for (size_t d = 0; d < 2; ++d)
            for (size_t a = 0; a < 4; ++a) {
                const json_t *s;
                const char *source = lab_math_format("%s(%s)", functions[f], arguments[a]);
                const json_t *r = math_fourier_copied_inverse(source, operators[d], operators[1-d], &s);
                lab_math_contains(math_fourier_text(r, "expression"), lab_math_format("%s(", functions[f]), true);
                lab_math_equal(math_fourier_text(r, "tex"), math_fourier_text(math_fourier_fields(math_fourier_text(r,
                    "expression"), "t"), "tex"));
                for (size_t p = 0; p < 2; ++p) {
                    double complex expected = math_fourier_numerical(source, points[p], "");
                    lab_math_close(math_fourier_numerical(lab_math_algebra(r), points[p], ""), expected, 1e-12*(1+cabs(
                        expected)));
                }
                const json_t *repeated = math_fourier_fields(lab_math_format("%s(%s,t,ω)", operators[d],
                    math_fourier_text(r, "expression")), "ω");
                lab_math_contains(math_fourier_text(repeated, "function"), "fourier(", false);
            }
}

static void test_symbolic_real_scale_and_shift(void)
{
    const char *functions[] = {"tan", "cot"};
    const double parameters[][2] = {{2, 0.3}, {-2, 0.3}, {0.5, -0.2}};
    for (size_t f = 0; f < 2; ++f) {
        const json_t *s;
        const char *source = lab_math_format("%s(a*t+b)", functions[f]);
        const json_t *r = math_fourier_copied_inverse(source, "Fourier", "InverseFourier", &s);
        lab_math_contains(math_fourier_text(s, "expression"), "a ∈ ℝ", true);
        lab_math_contains(math_fourier_text(s, "expression"), "b ∈ ℝ", true);
        lab_math_contains(math_fourier_text(r, "expression"), "n =", false);
        for (size_t p = 0; p < 3; ++p) {
            const char *bindings = lab_math_format("; a=%.17g; b=%.17g", parameters[p][0], parameters[p][1]);
            lab_math_close(math_fourier_numerical(lab_math_algebra(r), 0.7, bindings), math_fourier_numerical(source,
                0.7, bindings), 1e-12);
        }
    }
}

static void test_spatial_coordinates_round_trip_with_inferred_and_explicit_inverse(void)
{
    const char *pairs[][2] = {{"x", "k"}, {"y", "m"}, {"z", "n"}};
    for (size_t i = 0; i < 3; ++i) {
        const char *x = pairs[i][0], *k = pairs[i][1];
        const json_t *s = math_fourier_fields(lab_math_format("@F{tan(%s)}", x), k);
        const char *inverses[] = {lab_math_format("@Finv{%s}", lab_math_algebra(s)),
                                 lab_math_format("InverseFourier(%s,%s,%s)", lab_math_algebra(s), k, x)};
        for (size_t j = 0; j < 2; ++j) {
            const json_t *r = math_fourier_fields(inverses[j], x);
            lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
            lab_math_contains(math_fourier_text(r, "expression"), lab_math_format("tan(%s)", x), true);
            lab_math_contains(math_fourier_text(r, "expression"), lab_math_format("cos(%s) ≠ 0", x), true);
            lab_math_contains(math_fourier_text(r, "function"), "principal value", false);
        }
    }
}

static void test_native_pole_conditions_and_values(void)
{
    const char *functions[] = {"tan", "cot"}, *denominators[] = {"cos", "sin"};
    const double points[] = {-0.3, 0.7};
    for (size_t f = 0; f < 2; ++f) {
        const json_t *s;
        const json_t *r = math_fourier_copied_inverse(lab_math_format("%s(t)", functions[f]), "Fourier", "InverseFourier", &s);
        lab_math_contains(math_fourier_text(r, "expression"), lab_math_format("%s(t) ≠ 0", denominators[f]), true);
        lab_math_contains(math_fourier_text(r, "function"), lab_math_format("%s(t) != 0", denominators[f]), true);
        lab_math_contains(math_fourier_text(r, "function"), lab_math_format("return %s(t).", functions[f]), true);
        lab_math_contains(math_fourier_text(r, "tex"), "\\ne 0", true);
        for (size_t p = 0; p < 2; ++p) {
            const char *bound = lab_math_replace(math_fourier_text(r, "expression"), "t = NAN", lab_math_format(
                "t = %.17g", points[p]));
            lab_math_places(math_fourier_number(math_fourier_fields(bound, "t")), f ? 1/tan(points[p]) : tan(points[p]), 12);
        }
        if (f)
            lab_math_equal(math_fourier_text(math_fourier_fields(lab_math_replace(math_fourier_text(r, "expression"),
                "t = NAN", "t = 0"), "t"), "value"), "NAN");
    }
}

static void test_nonzero_condition_syntax_round_trips(void)
{
    const char *conditions[] = {"x ≠ 0", "x != 0", "Re(abs(x)) > 0"};
    for (size_t i = 0; i < 3; ++i) {
        const json_t *r = math_fourier_fields(lab_math_format("{1/x | x=2; %s}", conditions[i]), "x");
        lab_math_contains(math_fourier_text(r, "expression"), "x ≠ 0", true);
        lab_math_equal(math_fourier_text(r, "value"), "0.5");
        lab_math_equal(math_fourier_text(math_fourier_fields(math_fourier_text(r, "expression"), "x"), "tex"),
            math_fourier_text(r, "tex"));
    }
}

static void test_arbitrary_domain_restrictions_are_not_discarded(void)
{
    lab_math_contains(math_fourier_text(math_fourier_fields("Fourier(tan(t) where (Re(t)>0),t,ω)", "ω"), "function"),
        "fourier(", true);
}

static void test_parameter_pole_conditions_are_retained(void)
{
    const json_t *r = math_fourier_fields("Fourier(tan(a) where (a ∈ ℝ; cos(a) ≠ 0),t,ω)", "ω");
    lab_math_contains(math_fourier_text(r, "expression"), "cos(a) ≠ 0", true);
    lab_math_contains(math_fourier_text(r, "function"), "cos(a) != 0", true);
}

static void test_symbolic_free_binding_is_not_substituted(void)
{
    const json_t *r = math_fourier_fields("{Fourier(tan(a*t),t,ω) | a=@pi/3}", "ω");
    lab_math_contains(lab_math_algebra(r), "ω/a", true);
    const char *bindings = lab_math_after(math_fourier_text(r, "expression"), " | ");
    lab_math_contains(bindings, "a = π/3", true);
    lab_math_contains(bindings, "a ∈ ℝ", true);
}

static void test_symbolic_numeric_quotient_keeps_denominator_grouping(void)
{
    const json_t *r = math_fourier_fields("{Fourier(tan(a*t),t,ω) | ; a=@pi/3}", "ω");
    lab_math_contains(math_fourier_text(r, "expression"), "ω/(π/3)", true);
    lab_math_equal(math_fourier_text(math_fourier_fields(math_fourier_text(r, "expression"), "ω"), "tex"),
        math_fourier_text(r, "tex"));
}

static void test_independent_handwritten_spectra(void)
{
    const char *functions[] = {"tan", "cot"}, *coefficients[] = {"2*@pi*i", "-2*@pi*i"}, *weights[] = {"(-1)^j", "1"};
    for (size_t i = 0; i < 2; ++i) {
        const json_t *r = math_fourier_fields(lab_math_format(
            "InverseFourier(%s*sum(j,1,@inf,%s*(delta(ω-2*j)-delta(ω+2*j))),ω,t)", coefficients[i], weights[i]), "t");
        lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
        lab_math_contains(math_fourier_text(r, "expression"), lab_math_format("%s(t)", functions[i]), true);
    }
}

static void test_fourier_coefficients_by_independent_quadrature(void)
{
    const char *functions[] = {"tan", "cot"};
    for (size_t f = 0; f < 2; ++f) {
        const json_t *s = math_fourier_fields(lab_math_format("Fourier(%s(t),t,ω)", functions[f]), "ω");
        const char *prefix = f ? "{ -2iπ" : "{ 2iπ";
        lab_math_check(strncmp(math_fourier_text(s, "expression"), prefix, strlen(prefix)) == 0, "Fourier coefficient prefix");
        lab_math_contains(math_fourier_text(s, "expression"), "δ(ω - 2n)", true);
        lab_math_contains(math_fourier_text(s, "expression"), "δ(ω + 2n)", true);
        lab_math_contains(math_fourier_text(s, "expression"), "(-1)^n", !f);
        for (int order = 1; order <= 6; ++order) {
            double start = f ? 0 : -M_PI/2, step = M_PI/4096;
            double endpoint = f ? 2*order : -2*order*(order % 2 ? -1 : 1);
            double integral = 2*endpoint;
            for (int j = 1; j < 4096; ++j) {
                double x = start+j*step;
                integral += (j % 2 ? 4 : 2)*(f ? 1/tan(x) : tan(x))*sin(2*order*x);
            }
            double complex expected = f ? -2*I*M_PI : 2*I*M_PI*(order % 2 ? -1 : 1);
            lab_math_close(-2*I*integral*step/3, expected, 1e-10);
        }
    }
}

static void test_finite_or_altered_series_are_not_matched(void)
{
    const char *cases[] = {"sum(j,1,4,(-1)^j*(delta(ω-2*j)-delta(ω+2*j)))",
        "sum(j,1,@inf,(-1)^j*(delta(ω-2*j)+delta(ω+2*j)))",
        "sum(j,1,@inf,(-1)^j*(delta(ω-2*j)-delta(ω+3*j)))",
        "sum(j,2,@inf,(-1)^j*(delta(ω-2*j)-delta(ω+2*j)))"};
    for (size_t i = 0; i < 4; ++i) {
        const json_t *r = math_fourier_fields(lab_math_format("InverseFourier(%s,ω,t)", cases[i]), "t");
        lab_math_contains(math_fourier_text(r, "function"), "tan(", false);
        lab_math_contains(math_fourier_text(r, "function"), "cot(", false);
    }
}

static void test_series_dummy_does_not_capture_a_parameter(void)
{
    const json_t *s = math_fourier_fields("Fourier(tan(t+n),t,ω)", "ω");
    lab_math_contains(math_fourier_text(s, "expression"), "Σ_(n=", false);
    const json_t *r = math_fourier_fields(lab_math_format("InverseFourier(%s,ω,t)", lab_math_algebra(s)), "t");
    lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
    lab_math_contains(math_fourier_text(r, "expression"), "n", true);
}

static void test_principal_value_diagnostic(void)
{
    const char *functions[] = {"tan", "cot"};
    for (size_t i = 0; i < 2; ++i) {
        const json_t *r = math_fourier_fields(lab_math_format("@F{%s(x)}", functions[i]), "k");
        lab_math_contains(math_fourier_text(r, "value_note"), "principal values", true);
        lab_math_contains(math_fourier_text(r, "value_note"), "distributional", true);
    }
}

static void test_nonreal_rates_do_not_use_real_pole_prescription(void)
{
    const char *sources[] = {"tan(i*t)", "cot(t+i)"};
    for (size_t i = 0; i < 2; ++i) {
        const json_t *r = math_fourier_fields(lab_math_format("Fourier(%s,t,ω)", sources[i]), "ω");
        lab_math_contains(math_fourier_text(r, "function"), "fourier(", true);
        lab_math_contains(math_fourier_text(r, "tex"), "\\sum", false);
    }
}

static void test_readme_tangent_pair(void)
{
    /* README example: docs/expression.md, periodic principal-value transforms. */
    const json_t *s = math_fourier_fields("@F{tan(x)}", "k");
    lab_math_contains(math_fourier_text(s, "expression"), "δ(k - 2n)", true);
    lab_math_contains(math_fourier_text(s, "expression"), "(-1)^n", true);
    const json_t *r = math_fourier_fields(lab_math_format("InverseFourier(%s,k,x)", lab_math_algebra(s)), "x");
    lab_math_contains(math_fourier_text(r, "expression"), "tan(x)", true);
    lab_math_contains(math_fourier_text(r, "expression"), "cos(x) ≠ 0", true);
    lab_math_contains(math_fourier_text(r, "expression"), "principal value", false);
    lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
}

/* Register ordinary regressions. */
void test_lab_math_fourier_periodic_cases(void)
{
    TEST_RUN_IN_GROUP(test_copied_periodic_pairs_both_directions, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_symbolic_real_scale_and_shift, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_spatial_coordinates_round_trip_with_inferred_and_explicit_inverse, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_native_pole_conditions_and_values, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_nonzero_condition_syntax_round_trips, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_arbitrary_domain_restrictions_are_not_discarded, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_parameter_pole_conditions_are_retained, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_symbolic_free_binding_is_not_substituted, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_symbolic_numeric_quotient_keeps_denominator_grouping, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_independent_handwritten_spectra, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_fourier_coefficients_by_independent_quadrature, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_finite_or_altered_series_are_not_matched, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_series_dummy_does_not_capture_a_parameter, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_principal_value_diagnostic, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_nonreal_rates_do_not_use_real_pole_prescription, tests, NULL);
    lab_math_reset();
}

/* Register README examples last. */
void test_lab_math_fourier_periodic_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_readme_tangent_pair, readme_examples, "math,readme,output");
    lab_math_reset();
}
