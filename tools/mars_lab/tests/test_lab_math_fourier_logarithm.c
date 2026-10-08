/**
 * @file test_lab_math_fourier_logarithm.c
 * @brief Logarithmic subset of the complete general Fourier regression port.
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


static const char *math_fourier_before(const char *source, const char *delimiter)
{
    string_t *input = string_new_with(source);
    string_offset_t end = string_find(input, delimiter);
    string_t *part = string_substr(input, 0, end < 0 ? string_byte_length(input) : (size_t)end);
    const char *result = lab_math_format("%s", string_c_str(part));
    string_free(part);
    string_free(input);
    return result;
}

static void test_log_absolute_spellings_and_cards(void)
{
    const json_t *expected = math_fourier_fields("-@pi*(finite_part(1/abs(k))+2*@eulermascheroni*delta(k))", "k");
    const char *sources[] = {"@F{ln|x|}", "@F{ln(|x|)}", "@F{ln(abs(x))}"};
    for (size_t i = 0; i < 3; ++i) {
        const json_t *r = math_fourier_fields(sources[i], "k");
        lab_math_equal(math_fourier_before(math_fourier_text(r, "tex"), "\\quad"), math_fourier_before(
            math_fourier_text(expected, "tex"), "\\quad"));
        lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
        lab_math_contains(math_fourier_text(r, "expression"), "finite part", true);
        lab_math_contains(math_fourier_text(r, "tex"), "\\text{finite part}", true);
        lab_math_contains(math_fourier_text(r, "value_note"), "distribution", true);
        lab_math_equal(math_fourier_text(math_fourier_fields(math_fourier_text(r, "expression"), "k"), "tex"),
            math_fourier_text(r, "tex"));
        lab_math_contains(math_fourier_text(r, "function"), "const x", false);
    }
    const char *functions[] = {"ln", "sin", "cos", "exp", "sqrt"};
    for (size_t i = 0; i < 5; ++i)
        lab_math_equal(math_fourier_text(math_fourier_fields(lab_math_format("%s|x|", functions[i]), "x"), "tex"),
                       math_fourier_text(math_fourier_fields(lab_math_format("%s(abs(x))", functions[i]), "x"), "tex"));
    lab_math_contains(math_fourier_text(math_fourier_fields("@F{ln(x)}", "k"), "function"), "fourier(", true);
}

static void test_finite_part_fourier_pair_and_inverse(void)
{
    const char *aliases[] = {"finite_part", "Fp"};
    const double forward_points[] = {-2, -0.5, 0.3, 1.25}, inverse_points[] = {-2, 0.3, 1.25};
    const double gamma = 0.5772156649015328606;
    for (size_t a = 0; a < 2; ++a) {
        const char *forward = lab_math_format("@F{%s(1/abs(t))}", aliases[a]);
        const char *inverse = lab_math_format("@Finv{%s(1/abs(ω))}", aliases[a]);
        lab_math_contains(math_fourier_text(math_fourier_fields(forward, "ω"), "function"), "fourier(", false);
        lab_math_contains(math_fourier_text(math_fourier_fields(inverse, "t"), "function"), "fourier(", false);
        for (size_t p = 0; p < 4; ++p)
            lab_math_places(math_fourier_number(math_fourier_fields(lab_math_format("{%s | ω=%.17g}", forward,
                forward_points[p]), "ω")),
                            -2*(log(fabs(forward_points[p]))+gamma), 12);
        for (size_t p = 0; p < 3; ++p)
            lab_math_places(math_fourier_number(math_fourier_fields(lab_math_format("{%s | t=%.17g}", inverse,
                inverse_points[p]), "t")),
                            -(log(fabs(inverse_points[p]))+gamma)/M_PI, 12);
    }
    const char *source = "@Finv{-@pi*finite_part(1/abs(ω))-2*@pi*@eulermascheroni*delta(ω)}";
    lab_math_contains(math_fourier_text(math_fourier_fields(source, "t"), "function"), "fourier(", false);
    for (size_t p = 0; p < 4; ++p)
        lab_math_places(math_fourier_number(math_fourier_fields(lab_math_format("{%s | t=%.17g}", source,
            forward_points[p]), "t")),
                        log(fabs(forward_points[p])), 12);
    const json_t *inverse = math_fourier_fields("@Finv{ln(abs(ω))}", "t");
    lab_math_equal(math_fourier_before(math_fourier_text(inverse, "tex"), "\\quad"),
        "-\\frac{1}{2}\\mkern-2mu \\left(\\frac{1}{\\left|t\\right|} + 2\\mkern-2mu \\gamma\\mkern-2mu \\delta(t)\\right)");
    for (size_t a = 0; a < 2; ++a) {
        const char *body = lab_math_format("%s(1/abs(x))", aliases[a]);
        lab_math_contains(math_fourier_text(math_fourier_fields(body, "x"), "value_note"), "distribution", true);
        lab_math_contains(math_fourier_text(lab_math_fields(body, "x", "derivative", 40), "function"), " : finite part", true);
        for (int point = 0; point < 2; ++point) {
            const json_t *r = math_fourier_fields(lab_math_format("{%s | x=%d}", body, point), "x");
            if (!point) {
                string_t *key = string_new();
                string_append_cstr(key, "value");
                const string_t *value = json_string_value(json_object_get(r, key));
                lab_math_check(!value || isnan(creal(lab_math_parse_number(string_c_str(value)))), "finite-part pole");
                string_free(key);
            } else {
                lab_math_check(math_fourier_number(r) == 1, "finite-part regular value");
            }
        }
    }
}

static double complex math_fourier_log_low(double u, void *context)
{
    return expm1(-(*(double *)context)*exp(2*u));
}

static double complex math_fourier_log_high(double u, void *context)
{
    return exp(-(*(double *)context)*exp(2*u));
}

static double complex math_fourier_log_original(double u, void *context)
{
    const double *p = context;
    return 2*sqrt(M_PI/p[0])*(u+log(fabs(p[1])))*exp(u-exp(2*u)/(4*p[0]));
}

static void test_log_fourier_action_on_gaussian_test_functions(void)
{
    const int scales[] = {1, 2, -3};
    double widths[] = {0.5, 1, 3};
    for (size_t s = 0; s < 3; ++s) {
        const json_t *r = math_fourier_fields(lab_math_format("@F{ln(abs(%d*x))}", scales[s]), "k");
        const json_t *inverse = math_fourier_fields(lab_math_format("@Finv{ln(abs(%d*ω))}", scales[s]), "t");
        for (size_t w = 0; w < 3; ++w) {
            double fp = creal(2*(lab_math_simpson(math_fourier_log_low, &widths[w], -32, 0, 8000)+
                                lab_math_simpson(math_fourier_log_high, &widths[w], 0, 6, 8000)));
            const char *action = math_fourier_before(math_fourier_text(r, "unbound"), " where ");
            action = lab_math_replace(action, "(1/|k| : finite part)", lab_math_format("(%.17g)", fp));
            action = lab_math_replace(action, "δ(k)", "1");
            double parameters[] = {widths[w], scales[s]};
            double complex expected = lab_math_simpson(math_fourier_log_original, parameters, -32, 6, 8000);
            lab_math_places(math_fourier_number(math_fourier_fields(action, "k")), expected, 8);
            action = math_fourier_before(math_fourier_text(inverse, "unbound"), " where ");
            action = lab_math_replace(action, "(1/|t| : finite part)", lab_math_format("(%.17g)", fp));
            action = lab_math_replace(action, "δ(t)", "1");
            lab_math_places(math_fourier_number(math_fourier_fields(action, "t")), expected/(2*M_PI), 8);
        }
    }
}

static void test_log_affine_domains_and_finite_part_calculus(void)
{
    const json_t *r = math_fourier_fields("@F{ln(abs(x-2))}", "k");
    lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
    lab_math_contains(math_fourier_text(r, "unbound"), "exp(-2ik)", true);
    r = math_fourier_fields("@F{ln(abs(a*x+b))}", "k");
    lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
    lab_math_contains(math_fourier_text(r, "tex"), "a", true);
    lab_math_contains(math_fourier_text(r, "tex"), "b", true);
    lab_math_contains(math_fourier_text(math_fourier_fields("@F{ln(abs(i*x))}", "k"), "function"), "fourier(", true);
    lab_math_contains(math_fourier_text(math_fourier_fields("@F{finite_part(1/abs(x)^2)}", "k"), "function"), "fourier(", true);
    r = math_fourier_fields("sum(n,1,2,finite_part(n/abs(x)))", "x");
    lab_math_contains(math_fourier_text(r, "function"), " : finite part", true);
    lab_math_contains(math_fourier_text(r, "value_note"), "distribution", true);
    lab_math_contains(math_fourier_text(math_fourier_fields("@S finite_part(1/abs(x)) dx", "x"), "function"),
        " : finite part", true);
}

static void test_readme_logarithmic_fourier_examples(void)
{
    /* README examples: docs/expression.md, logarithmic Fourier pairs. */
    const char *sources[] = {"@F{ln|x|}", "@F{ln(|x|)}", "@F{ln(abs(x))}"};
    for (size_t i = 0; i < 3; ++i)
        lab_math_equal(math_fourier_text(math_fourier_fields(sources[i], "k"), "unbound"),
            "-π·((1/|k| : finite part) + 2γ·δ(k)) where (k ∈ ℝ)");
    lab_math_equal(math_fourier_text(math_fourier_fields(
        "@Finv{-@pi*finite_part(1/abs(ω))-2*@pi*@eulermascheroni*delta(ω)}", "t"), "unbound"),
                   "ln(|t|) where (t ∈ ℝ)");
}

/* Register ordinary regressions. */
void test_lab_math_fourier_logarithm_cases(void)
{
    TEST_RUN_IN_GROUP(test_log_absolute_spellings_and_cards, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_finite_part_fourier_pair_and_inverse, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_log_fourier_action_on_gaussian_test_functions, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_log_affine_domains_and_finite_part_calculus, tests, NULL);
    lab_math_reset();
}

/* Register README examples last. */
void test_lab_math_fourier_logarithm_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_readme_logarithmic_fourier_examples, readme_examples, "math,readme,output");
    lab_math_reset();
}
