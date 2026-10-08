/**
 * @file test_lab_math_laplace_gaussian.c
 * @brief Gaussian Laplace formulas, coverage audits and domain regression port.
 *
 * Preserves test_laplace_gaussian.py, including documentation inventory audits,
 * antiderivative domains, independent elementary values and README examples.
 */
#include <math.h>

#include "test_lab_math_support.h"

static const json_t *lab_math_gaussian_fields(const char *source, const char *action)
{
    return lab_math_fields(source, "s", action, 40);
}

static void test_laplace_gaussian_antiderivatives(void)
{
    const char *sources[] = {"@L{sin^2(1/2@pit)}", "@L{sin^2((@pi*t)/2)}", "@L{exp(-t)}", "@L{sin(t)}"};
    for (size_t i = 0; i < 4; ++i) {
        const json_t *result = lab_math_gaussian_fields(sources[i], "integral");
        string_t *integral = string_new_with(lab_math_text(result, "integral"));
        lab_math_check(string_starts_with(integral, "∫ds = "), "integral prefix");
        string_free(integral);
        lab_math_contains(lab_math_text(result, "integral"), "Re(s)", true);
        lab_math_contains(lab_math_text(result, "integral"), "C", true);
        const char *copy = lab_math_replace(lab_math_after(lab_math_text(result, "integral"), " = "), "s = NAN", "s = 2");
        const json_t *derivative = lab_math_gaussian_fields(copy, "derivative");
        const json_t *expected = lab_math_gaussian_fields(lab_math_format("{%s | s=2}", sources[i]), "evaluate");
        lab_math_places(lab_math_number(derivative, "derivative_value"), lab_math_number(expected, "value"), 13);
        copy = lab_math_replace(lab_math_replace(copy, "s = 2", "s = -2"), "C = NAN", "C = 0");
        lab_math_equal(lab_math_text(lab_math_gaussian_fields(copy, "evaluate"), "value"), "NAN");
    }
}

static void test_laplace_gaussian_exact_rates(void)
{
    const char *rates[] = {"@pi/2", "sqrt(2)/3"}, *functions[] = {"sin", "cos"};
    const double numeric[] = {acos(-1) / 2, sqrt(2) / 3};
    for (size_t r = 0; r < 2; ++r) {
        for (size_t f = 0; f < 2; ++f) {
            const char *source = lab_math_format("{@L{%s^2((%s)*t)} | s=2}", functions[f], rates[r]);
            const json_t *result = lab_math_gaussian_fields(source, "evaluate");
            double expected = (2 * numeric[r] * numeric[r] + (f ? 4 : 0)) / (2 * (4 + 4 * numeric[r] * numeric[r]));
            lab_math_contains(lab_math_text(result, "function"), "laplace(", false);
            lab_math_places(lab_math_number(result, "value"), expected, 13);
            const json_t *copied = lab_math_gaussian_fields(lab_math_format("{%s | s=2}", lab_math_text(result,
                "unbound")), "evaluate");
            lab_math_places(lab_math_number(copied, "value"), expected, 13);
        }
    }
    const json_t *result = lab_math_gaussian_fields("@L{sin^2((@pi*t)/2)}", "evaluate");
    lab_math_contains(lab_math_text(result, "tex"), "\\pi", true);
    lab_math_contains(lab_math_text(result, "function"), "3.14159", false);
    result = lab_math_gaussian_fields("@L{sin((@pi*t)/2)^n}", "evaluate");
    lab_math_contains(lab_math_text(result, "function"), "sum(", true);
    lab_math_contains(lab_math_text(result, "function"), "@pi", true);
    result = lab_math_gaussian_fields("@L{sin((@pi*t^2)/2)^2}", "evaluate");
    lab_math_contains(lab_math_text(result, "function"), "laplace(", true);
}

static const char *lab_math_gaussian_section(const char *input, const char *begin, const char *end)
{
    const char *tail = lab_math_after(input, begin);
    string_t *text = string_new_with(tail);
    string_offset_t boundary = string_find(text, end);
    lab_math_check(boundary >= 0, end);
    string_t *section = boundary >= 0 ? string_substr(text, 0, (size_t)boundary) : string_new();
    const char *result = lab_math_format("%s", string_c_str(section));
    string_free(section);
    string_free(text);
    return result;
}

static void test_laplace_gaussian_inventory(void)
{
    const char *header = lab_math_read("include/expression.h");
    const char *guide = lab_math_read("docs/expression.md");
    const char *inventory = lab_math_gaussian_section(guide, "### Laplace function coverage", "## Numeric representation");
    string_t *section = string_new_with(lab_math_gaussian_section(header, "expr_t *expr_sin(", "expr_t *expr_E1("));
    string_cursor_t *cursor = string_cursor_new(section);
    lab_math_contains(inventory, "`sin`", true);
    lab_math_contains(inventory, "`E1`", true);
    while (cursor && !string_cursor_done(cursor)) {
        if (string_cursor_consume(cursor, "expr_t *expr_")) {
            string_pos_t start = string_cursor_position(cursor);
            unsigned char byte = 0;
            while (string_cursor_peek_ascii(cursor, &byte) &&
                   ((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
                    (byte >= '0' && byte <= '9') || byte == '_'))
                string_cursor_next(cursor);
            string_t *name = string_cursor_extract(start, cursor);
            if (string_cursor_match(cursor, "("))
                lab_math_contains(inventory, lab_math_format("`%s`", string_c_str(name)), true);
            string_free(name);
        } else {
            string_cursor_next(cursor);
        }
    }
    string_cursor_free(cursor);
    string_free(section);
    const char *families = lab_math_gaussian_section(guide, "#### Supported families", "Linearity,");
    const char *tables = lab_math_gaussian_section(lab_math_read("docs/design-notes/integral-transforms.md"),
                                                   "## Implemented forward Laplace transforms", "## Expression Rendering");
    section = string_new_with(families);
    size_t count = 0;
    string_t **lines = string_split(section, "\n", &count);
    for (size_t i = 0; i < count; ++i) {
        if (!string_starts_with(lines[i], "| `"))
            continue;
        cursor = string_cursor_new(lines[i]);
        string_cursor_consume(cursor, "| ");
        while (!string_cursor_done(cursor) && !string_cursor_match(cursor, "|")) {
            if (string_cursor_consume(cursor, "`")) {
                string_pos_t start = string_cursor_position(cursor);
                while (!string_cursor_done(cursor) && !string_cursor_match(cursor, "`"))
                    string_cursor_next(cursor);
                string_t *name = string_cursor_extract(start, cursor);
                if (string_cursor_consume(cursor, "`"))
                    lab_math_contains(tables, lab_math_format("`%s`", string_c_str(name)), true);
                string_free(name);
            } else {
                string_cursor_next(cursor);
            }
        }
        string_cursor_free(cursor);
    }
    string_split_free(lines, count);
    string_free(section);
    section = string_new_with(tables);
    lines = string_split(section, "\n", &count);
    for (size_t i = 0; i < count; ++i) {
        if (!string_starts_with(lines[i], "|"))
            continue;
        cursor = string_cursor_new(lines[i]);
        unsigned bars = 0;
        while (!string_cursor_done(cursor)) {
            if (string_cursor_consume(cursor, "|"))
                ++bars;
            else
                string_cursor_next(cursor);
        }
        lab_math_check(bars == 4, string_c_str(lines[i]));
        string_cursor_free(cursor);
    }
    string_split_free(lines, count);
    string_free(section);
}

static void test_laplace_gaussian_values(void)
{
    double pi = acos(-1);
    const int targets[] = {0, 1, -2};
    const char *sources[] = {"exp(-t^2)", "e^(-t^2)"};
    for (size_t s = 0; s < 3; ++s) {
        double target = targets[s], expected = sqrt(pi) / 2 * exp(target * target / 4) * erfc(target / 2);
        for (size_t i = 0; i < 2; ++i) {
            const json_t *result = lab_math_gaussian_fields(lab_math_format("{@L(%s) | s=%d}", sources[i], targets[s]),
                "evaluate");
            lab_math_places(lab_math_number(result, "value"), expected, 13);
            lab_math_contains(lab_math_text(result, "function"), "laplace(", false);
            lab_math_contains(lab_math_text(result, "expression"), "Re(s)", false);
        }
    }
    lab_math_places(lab_math_number(lab_math_gaussian_fields("{@L(exp(-(2*t+1)^2)) | s=1}", "evaluate"), "value"),
                    sqrt(pi) / 4 * exp(1.25 * 1.25 - 1) * erfc(1.25), 13);
    const char *operands[] = {"normal_pdf(t)", "pdf(t)", "normal_pdf(2*t+1)", "normal_cdf(t)", "cdf(-t)",
                              "normal_logpdf(t)", "logpdf(2*t+1)"};
    const int points[] = {0, -1, 0, 1, 1, 2, 2};
    const double values[] = {0.5, exp(0.5) * erfc(-1 / sqrt(2)) / 2, erfc(1 / sqrt(2)) / 4,
                             (1 + exp(0.5) * erfc(1 / sqrt(2))) / 2, (1 - exp(0.5) * erfc(1 / sqrt(2))) / 2,
                             -1.0 / 8 - log(2 * pi) / 4, -1 - (1 + log(2 * pi)) / 4};
    for (size_t i = 0; i < 7; ++i) {
        const json_t *result = lab_math_gaussian_fields(lab_math_format("{@L(%s) | s=%d}", operands[i], points[i]), "evaluate");
        lab_math_contains(lab_math_text(result, "function"), "laplace(", false);
        lab_math_places(lab_math_number(result, "value"), values[i], 13);
    }
    const json_t *result = lab_math_gaussian_fields("@L(exp(-a*t^2))", "evaluate");
    lab_math_contains(lab_math_text(result, "function"), "laplace(", false);
    lab_math_contains(lab_math_text(result, "function"), "realpart(a) > 0", true);
    lab_math_contains(lab_math_text(result, "function"), "realpart(s)", false);
    lab_math_equal(lab_math_text(lab_math_gaussian_fields("{@L(exp(-a*t^2)) | s=1; a=-1}", "evaluate"), "value"), "NAN");
    lab_math_places(lab_math_number(lab_math_gaussian_fields("{@L(exp(-t^2)) | s=0}", "derivative"),
        "derivative_value"), -0.5, 13);
    const char *divergent[] = {"sec(t)", "cosec(t)", "cot(t)", "cosech(t)", "coth(t)", "gamma(t)",
                               "digamma(t)", "trigamma(t)", "zeta(t)", "exp(t^2)", "e^(t^2)"};
    for (size_t i = 0; i < 11; ++i)
        lab_math_contains(lab_math_text(lab_math_gaussian_fields(lab_math_format("@L(%s)", divergent[i]), "evaluate"),
                                        "value_note"), "No ordinary Laplace transform", true);
    lab_math_contains(lab_math_text(lab_math_gaussian_fields("@L(lgamma(t))", "evaluate"), "value_note"), "does not imply", true);
    const char *derivatives[] = {"E1(t)", "bessel_j(1,t)", "floor(t)"};
    const double expected[] = {0.5 - log(2), -1 / (2 * sqrt(2)), -1 / (exp(1) - 1) - exp(1) / pow(exp(1) - 1, 2)};
    for (size_t i = 0; i < 3; ++i)
        lab_math_places(lab_math_number(lab_math_gaussian_fields(lab_math_format("{@L(%s) | s=1}", derivatives[i]),
                                                                  "derivative"), "derivative_value"), expected[i], 12);
}

static void test_laplace_gaussian_readme(void)
{
    /* README examples: docs/expression.md, Laplace function coverage. */
    const char *sources[] = {"{@L(exp(-t^2)) | s=0}", "{@L(normal_pdf(t)) | s=0}", "{@L(floor(t)) | s=1}",
                             "{@L(sech(t)) | s=1}", "{@L(E1(t)) | s=1}", "{@L(bessel_j(1,t)) | s=1}",
                             "{@L(atanh(t)) | s=1}"};
    const double complex expected[] = {0.886226925452758, 0.5, 0.581976706869326, 0.693147180559945,
                                       0.693147180559945, 0.292893218813452, 0.646761122779130 + 0.577863674895461 * I};
    for (size_t i = 0; i < 7; ++i)
        lab_math_places(lab_math_number(lab_math_gaussian_fields(sources[i], "evaluate"), "value"), expected[i], 14);
}

/* Register Gaussian formulas, native calculus and documentation coverage audits. */
void test_lab_math_laplace_gaussian_cases(void)
{
    TEST_RUN_IN_GROUP(test_laplace_gaussian_antiderivatives, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_gaussian_exact_rates, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_gaussian_inventory, tests, NULL);
    lab_math_reset();
    TEST_RUN_IN_GROUP(test_laplace_gaussian_values, tests, NULL);
    lab_math_reset();
}

/* Run documented Gaussian and coverage examples after all ordinary tests. */
void test_lab_math_laplace_gaussian_readme_cases(void)
{
    TEST_RUN_OUTPUT_IN_GROUP_TAGS(test_laplace_gaussian_readme, readme_examples, "math,readme,output");
    lab_math_reset();
}
