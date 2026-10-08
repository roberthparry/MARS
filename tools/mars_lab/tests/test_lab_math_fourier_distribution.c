/**
 * @file test_lab_math_fourier_distribution.c
 * @brief Native port of all distribution-qualifier regression cases.
 *
 * Preserves occurrence scope, complete copied cards, invalid qualifications,
 * regular values and singular support, with README cases registered separately.
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

static bool math_fourier_has_field(const json_t *result, const char *name)
{
    string_t *key = string_new();
    string_append_utf8_exact(key, name, strlen(name));
    bool present = json_object_get(result, key) != NULL;
    string_free(key);
    return present;
}

static const json_t *math_fourier_round_trip(const char *source)
{
    const json_t *original = math_fourier_fields(source, "x");
    const json_t *copied = math_fourier_fields(math_fourier_text(original, "expression"), "x");
    const json_t *local = math_fourier_fields(math_fourier_text(original, "unbound"), "x");
    lab_math_equal(math_fourier_text(copied, "function"), math_fourier_text(original, "function"));
    const json_t *candidates[] = {copied, local};
    for (size_t i = 0; i < 2; ++i) {
        lab_math_equal(math_fourier_text(candidates[i], "tex"), math_fourier_text(original, "tex"));
        lab_math_equal(math_fourier_text(candidates[i], "unbound"), math_fourier_text(original, "unbound"));
    }
    const char *names[] = {"principal_value(", "finite_part(", "PV(", "Fp("};
    for (size_t i = 0; i < 4; ++i) {
        lab_math_contains(math_fourier_text(original, "expression"), names[i], false);
        lab_math_contains(math_fourier_text(original, "function"), names[i], false);
    }
    lab_math_contains(math_fourier_text(original, "tex"), "\\operatorname{PV}", false);
    lab_math_contains(math_fourier_text(original, "tex"), "\\operatorname{Fp}", false);
    return original;
}

static void test_single_qualification_shares_domain_area(void)
{
    const char *sources[] = {"PV(1/x)", "Fp(1/abs(x))"};
    const char *labels[] = {"principal value", "finite part"};
    for (size_t i = 0; i < 2; ++i) {
        const json_t *r = math_fourier_round_trip(lab_math_format("%s where (x ∈ ℝ)", sources[i]));
        lab_math_contains(math_fourier_before(math_fourier_text(r, "expression"), " | "), labels[i], false);
        const char *qualifications = lab_math_after(math_fourier_text(r, "expression"), " | ");
        lab_math_contains(qualifications, "x ∈ ℝ", true);
        lab_math_contains(qualifications, lab_math_format(" : %s", labels[i]), true);
        lab_math_contains(math_fourier_text(r, "function"), lab_math_format(" : %s", labels[i]), true);
        lab_math_contains(math_fourier_text(r, "tex"), lab_math_format("\\text{%s}", labels[i]), true);
    }
}

static void test_aliases(void)
{
    const char *pairs[][2] = {{"PV", "principal_value"}, {"Fp", "finite_part"}};
    for (size_t i = 0; i < 2; ++i)
        lab_math_equal(math_fourier_text(math_fourier_fields(lab_math_format("%s(1/x)", pairs[i][0]), "x"), "expression"),
                       math_fourier_text(math_fourier_fields(lab_math_format("%s(1/x)", pairs[i][1]), "x"), "expression"));
}

static void test_duplicates_mixed_and_nested_operators(void)
{
    const char *cases[] = {"PV(1/x)+1/x", "PV(1/x)-1/x", "PV(1/x)+PV(1/x)", "PV(1/x)+finite_part(1/x)",
        "PV(1/x)*finite_part(1/x)", "PV(finite_part(1/x))", "finite_part(PV(1/x))", "PV(PV(1/x))",
        "PV(1/x+1/(x+1))", "PV(1/x)^2", "1/PV(1/x)", "sin(PV(1/x))", "PV(sin(x))+sin(x)", "PV(x*y)+y*x",
        "PV(1/x)+finite_part(1/abs(x))+1/x", "finite_part(1/abs(x))-1/abs(x)"};
    for (size_t i = 0; i < 16; ++i) {
        const char *function = math_fourier_text(math_fourier_round_trip(cases[i]), "function");
        lab_math_check(strstr(function, " : principal value") || strstr(function, " : finite part"), cases[i]);
    }
}

static void test_unwrapped_duplicate_remains_unwrapped(void)
{
    const json_t *r = math_fourier_round_trip("PV(1/x)-1/x");
    const char *first = strstr(math_fourier_text(r, "function"), " : principal value");
    lab_math_check(first && !strstr(first+1, " : principal value"), "exactly one occurrence is qualified");
    lab_math_contains(math_fourier_before(math_fourier_text(r, "expression"), " | "), " : principal value", true);
    lab_math_contains(math_fourier_text(r, "tex"), "\\underbrace{", true);
    lab_math_check(strcmp(math_fourier_text(r, "unbound"), "0") != 0, "qualified subtraction is not zero");
}

static void test_nested_order_is_preserved(void)
{
    const json_t *first = math_fourier_round_trip("PV(finite_part(1/x))");
    const json_t *second = math_fourier_round_trip("finite_part(PV(1/x))");
    lab_math_check(strcmp(math_fourier_text(first, "function"), math_fourier_text(second, "function")) != 0,
        "nesting order differs");
    lab_math_contains(math_fourier_text(first, "function"), "((1/x : finite part) : principal value)", true);
    lab_math_contains(math_fourier_text(second, "function"), "((1/x : principal value) : finite part)", true);
}

static void test_grouping_preserves_scope_before_simplification(void)
{
    const char *cases[] = {"{ (1/x)-1/x | x=?; 1/x : principal value }",
        "{ (1/x)^2 | x=?; 1/x : principal value }", "{ 1/(1/x) | x=?; 1/x : finite part }"};
    for (size_t i = 0; i < 3; ++i) {
        const char *function = math_fourier_text(math_fourier_round_trip(cases[i]), "function");
        lab_math_check(strstr(function, " : principal value") || strstr(function, " : finite part"), cases[i]);
    }
}

static void test_invalid_or_ambiguous_qualifications_are_rejected(void)
{
    const char *cases[] = {"{ (1/x)+(1/x) | x=?; 1/x : principal value }",
        "{ 1/x | x=?; 1/x : principal value }", "{ (1/x) | x=?; 1/x : unknown }", "(1/x : principal value extra)"};
    for (size_t i = 0; i < 4; ++i) {
        int status = 0;
        const char *raw = NULL;
        lab_math_worker("mars_lab", cases[i], "x", "evaluate", 40, &status, &raw);
        lab_math_check(status != 0, raw);
    }
}

static void test_distribution_regular_values_preserve_symbolic_nodes(void)
{
    const char *operators[] = {"PV", "finite_part"};
    const char *operands[] = {"1", "1/x", "i/x"};
    const double complex expected[] = {1, 0.5, 0.5*I};
    const char *points[] = {"0", "?", "NAN", "inf"};
    for (size_t o = 0; o < 2; ++o) {
        for (size_t p = 0; p < 3; ++p) {
            const json_t *r = math_fourier_fields(lab_math_format("{%s(%s) | x=2}", operators[o], operands[p]), "x");
            if (p)
                lab_math_contains(math_fourier_text(r, "function"), o ? " : finite part" : " : principal value", true);
            lab_math_check(math_fourier_number(r) == expected[p], "regular distribution value");
            lab_math_check(!math_fourier_has_field(r, "value_note"), "regular values have no distribution warning");
        }
        for (size_t p = 0; p < 4; ++p) {
            const char *source = p == 3 ? "delta(x)" : lab_math_format("%s(1/x)", operators[o]);
            lab_math_equal(math_fourier_text(math_fourier_fields(lab_math_format("{%s | x=%s}", source, points[p]),
                "x"), "value"), "NAN");
        }
    }
}

static void test_impulse_support_and_derivatives(void)
{
    const char *sources[] = {"delta(x-2)", "delta(3*x-6)", "Derivative(delta(x-2),3)",
        "Derivative(delta(x-2),x,1)", "Derivative(delta(x-2),x,2)", "Derivative(delta(x-2),n)"};
    const int points[] = {-1, 0, 2, 3};
    for (size_t s = 0; s < 6; ++s)
        for (size_t p = 0; p < 4; ++p) {
            const json_t *r = math_fourier_fields(lab_math_format("{%s | x=%d; n=3}", sources[s], points[p]), "x");
            if (points[p] == 2)
                lab_math_equal(math_fourier_text(r, "value"), "NAN");
            else
                lab_math_check(math_fourier_number(r) == 0, "off-support impulse derivative");
            lab_math_contains(math_fourier_text(r, "expression"), "δ(", true);
        }
    const char *invalid_points[] = {"?", "i", "inf"};
    for (size_t p = 0; p < 3; ++p)
        lab_math_equal(math_fourier_text(math_fourier_fields(lab_math_format("{delta(x) | x=%s}", invalid_points[p]),
            "x"), "value"), "NAN");
    const char *orders[] = {"?", "-1", "1/2", "i"};
    for (size_t n = 0; n < 4; ++n)
        lab_math_equal(math_fourier_text(math_fourier_fields(lab_math_format("{Derivative(delta(x),n) | x=2; n=%s}",
            orders[n]), "x"), "value"), "NAN");
    lab_math_check(math_fourier_number(math_fourier_fields("delta(1)", "x")) == 0, "constant impulse away from support");
}

static void test_regularised_derivatives_and_finite_sums(void)
{
    const char *sources[] = {"Derivative(PV(1/x),x,1)", "Derivative(finite_part(1/abs(x)),x,1)",
        "Derivative(PV(1/x),x,2)", "sum(n,1,2,finite_part(n/abs(x)))"};
    const double expected[] = {-0.25, -0.25, 0.25, 1.5};
    for (size_t i = 0; i < 4; ++i) {
        lab_math_check(math_fourier_number(math_fourier_fields(lab_math_format("{%s | x=2}", sources[i]),
            "x")) == expected[i], sources[i]);
        lab_math_equal(math_fourier_text(math_fourier_fields(lab_math_format("{%s | x=0}", sources[i]), "x"), "value"), "NAN");
    }
    lab_math_equal(math_fourier_text(math_fourier_fields("{Derivative(PV(1/x),x,1) | x=?}", "x"), "value"), "NAN");
}

static void test_transform_values_off_singular_support(void)
{
    const char *sources[] = {"@F{acosh(x)}", "@F{acosh(x)}", "@F{asin(x)}", "@F{acos(x)}", "@F{atanh(x)}",
        "@F{ln(abs(x))}", "@F{step(x)}", "Fourier(1,x,k)", "@F{x^3}", "@F{sin(x)}"};
    const int points[] = {1, -1, 1, 1, 1, 2, 2, 2, 2, 2};
    const double complex expected[] = {-4.807878861268826, 0, -4.807878861268826*I, 4.807878861268826*I,
        -2*I*M_PI*sin(1), -M_PI/2, -0.5*I, 0, 0, 0};
    for (size_t i = 0; i < 10; ++i) {
        const json_t *unbound = math_fourier_fields(sources[i], "k");
        const json_t *bound = math_fourier_fields(lab_math_format("{%s | k=%d}", sources[i], points[i]), "k");
        lab_math_close(math_fourier_number(bound), expected[i], 2e-12);
        lab_math_check(!math_fourier_has_field(bound, "value_note"), "regular transform value has no warning");
        lab_math_equal(math_fourier_text(bound, "tex"), math_fourier_text(unbound, "tex"));
        lab_math_equal(math_fourier_text(bound, "unbound"), math_fourier_text(unbound, "unbound"));
        lab_math_close(math_fourier_number(math_fourier_fields(math_fourier_text(bound, "expression"), "k")), expected[i], 2e-12);
    }
    const char *singular[] = {"@F{acosh(x)}", "@F{ln(abs(x))}", "@F{step(x)}", "@F{sin(x)}", "@F{cos(2*x)}"};
    const int support[] = {0, 0, 0, 1, -2};
    for (size_t i = 0; i < 5; ++i) {
        const json_t *r = math_fourier_fields(lab_math_format("{%s | k=%d}", singular[i], support[i]), "k");
        lab_math_equal(math_fourier_text(r, "value"), "NAN");
        lab_math_contains(math_fourier_text(r, "value_note"), "singular", true);
    }
}

static void test_bound_spectrum_retains_inverse_information(void)
{
    const json_t *s = math_fourier_fields("{@F{acosh(x)} | k=1}", "k");
    const json_t *r = math_fourier_fields(lab_math_format("InverseFourier(%s,k,x)", math_fourier_text(s, "expression")), "x");
    lab_math_contains(math_fourier_text(r, "expression"), "acosh(x)", true);
    lab_math_contains(math_fourier_text(r, "function"), "fourier(", false);
    const int points[] = {1, 0, -1};
    const double complex expected[] = {0, 0.5*I*M_PI, I*M_PI};
    for (size_t p = 0; p < 3; ++p)
        lab_math_close(math_fourier_number(math_fourier_fields(lab_math_replace(math_fourier_text(r, "expression"), "x = NAN",
            lab_math_format("x = %d", points[p])), "x")), expected[p], 2e-12);
}

static void test_complete_copied_expression_inside_transform(void)
{
    const char *sources[] = {"finite_part(1/abs(x))", "PV(tan(x))"};
    for (size_t i = 0; i < 2; ++i) {
        const char *copy = math_fourier_text(math_fourier_fields(sources[i], "x"), "expression");
        const json_t *direct = math_fourier_fields(lab_math_format("Fourier(%s,x,ω)", sources[i]), "ω");
        const json_t *nested = math_fourier_fields(lab_math_format("Fourier(%s,x,ω)", copy), "ω");
        lab_math_equal(math_fourier_text(nested, "unbound"), math_fourier_text(direct, "unbound"));
        lab_math_equal(math_fourier_text(nested, "tex"), math_fourier_text(direct, "tex"));
    }
}

static void test_bound_symbols_and_conditions_survive(void)
{
    const json_t *r = math_fourier_round_trip("{PV(1/(x+a)) | x=2; a=1/3; x ∈ ℝ}");
    lab_math_contains(math_fourier_text(r, "expression"), "x = 2", true);
    lab_math_contains(math_fourier_text(r, "expression"), "⅓", true);
    lab_math_contains(math_fourier_text(r, "expression"), "x ∈ ℝ", true);
}

static void test_transform_shorthand_braces_keep_free_symbols(void)
{
    const char *cases[][3] = {{"@F{step(t)}", "Fourier(step(t),t,ω)", "ω"},
        {"@Finv{ln(abs(ω))}", "InverseFourier(ln(abs(ω)),ω,t)", "t"}};
    for (size_t i = 0; i < 2; ++i)
        lab_math_equal(math_fourier_text(math_fourier_fields(cases[i][0], cases[i][2]), "unbound"),
                       math_fourier_text(math_fourier_fields(cases[i][1], cases[i][2]), "unbound"));
}

static void test_readme_distribution_regular_values(void)
{
    /* README examples: docs/expression.md, numerical evaluation of distributions. */
    const char *sources[] = {"{@F{acosh(x)} | k=1}", "{@F{acosh(x)} | k=-1}", "{@F{ln(abs(x))} | k=2}"};
    const double expected[] = {-4.807878861268826, 0, -M_PI/2};
    for (size_t i = 0; i < 3; ++i)
        lab_math_close(math_fourier_number(math_fourier_fields(sources[i], "k")), expected[i], 2e-12);
}

static void test_readme_distribution_notation(void)
{
    /* README examples: docs/expression.md, occurrence-specific notation. */
    lab_math_equal(math_fourier_text(math_fourier_fields("PV(1/x)", "x"), "unbound"), "(1/x : principal value)");
    lab_math_equal(math_fourier_text(math_fourier_fields("Fp(1/abs(x))", "x"), "unbound"), "(1/|x| : finite part)");
}

/* Register ordinary regressions. */
void test_lab_math_fourier_distribution_cases(void)
{
    TEST_RUN_IN_GROUP(test_single_qualification_shares_domain_area, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_aliases, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_duplicates_mixed_and_nested_operators, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_unwrapped_duplicate_remains_unwrapped, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_nested_order_is_preserved, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_grouping_preserves_scope_before_simplification, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_invalid_or_ambiguous_qualifications_are_rejected, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_distribution_regular_values_preserve_symbolic_nodes, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_impulse_support_and_derivatives, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_regularised_derivatives_and_finite_sums, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_transform_values_off_singular_support, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_bound_spectrum_retains_inverse_information, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_complete_copied_expression_inside_transform, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_bound_symbols_and_conditions_survive, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_transform_shorthand_braces_keep_free_symbols, tests, NULL);
    lab_math_reset();
}

/* Register README examples last. */
void test_lab_math_fourier_distribution_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_readme_distribution_regular_values, readme_examples, "math,readme,output");
    lab_math_reset();
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_readme_distribution_notation, readme_examples, "math,readme,output");
    lab_math_reset();
}
