/**
 * @file test_lab_syntax.c
 * @brief Native Function and matrix-heading metadata with bounded fallback regressions.
 *
 * Exercises the public presentation adapter without rendering, workers or a
 * browser. Checks lossless combined programme/declaration text, Unicode tokens,
 * comments, array variables, display variants and the wire-safe span boundary.
 */
#include <string.h>

#include "lab_presentation.h"
#include "test_harness.h"
#include "test_lab_support.h"

static bool lab_syntax_test_set(json_t *fields, const char *name, const string_t *source)
{
    string_t *key = string_new_with(name);
    json_t *value = source ? json_new_string(source) : NULL;
    bool ok = key && value && json_object_set(fields, key, value);
    string_free(key);
    json_free(value);
    return ok;
}

static const json_t *lab_syntax_test_entry(const json_t *fields, size_t index)
{
    return json_array_get(test_lab_member(test_lab_member(fields, "presentation"), "function_syntax"), index);
}

static bool lab_syntax_test_lossless(const json_t *entry, const string_t *source)
{
    string_t *joined = string_new();
    const json_t *spans = test_lab_member(entry, "spans");
    bool ok = joined && !strcmp(test_lab_text(entry, "source"), string_c_str(source));
    for (size_t i = 0u; ok && i < json_array_size(spans); ++i)
        ok = !string_append_cstr(joined, test_lab_text(json_array_get(spans, i), "text"));
    ok = ok && !string_compare(joined, source);
    string_free(joined);
    return ok;
}

static bool lab_syntax_test_token(const json_t *entry, const char *text, const char *kind)
{
    const json_t *spans = test_lab_member(entry, "spans");
    /* Test fixtures contain at most 1024 spans; inspect their exact public wire representation. */
    for (size_t i = 0u; i < json_array_size(spans); ++i) {
        const json_t *span = json_array_get(spans, i);
        if (!strcmp(test_lab_text(span, "text"), text) && !strcmp(test_lab_text(span, "kind"), kind)) return true;
    }
    return false;
}

static void test_lab_syntax_combined(void)
{
    string_t *source = string_new_with(" \nexpression f(array α, x) { return sin(x)+α[0]+2.5e-3; }\n"
                                      "array const values = [1,2];\nconst c = @pi;\nx = 3;\n");
    json_t *fields = json_new_object();
    bool ok = source && fields && lab_syntax_test_set(fields, "function", source) && lab_presentation_adapt(fields);
    const json_t *entry = lab_syntax_test_entry(fields, 0u);
    bool function = false;
    ok = ok && json_bool_value(test_lab_member(entry, "is_function"), &function) && function &&
         lab_syntax_test_lossless(entry, source) && lab_syntax_test_token(entry, "return", "keyword") &&
         lab_syntax_test_token(entry, "α", "array") && lab_syntax_test_token(entry, "values", "array") &&
         lab_syntax_test_token(entry, "sin", "function") && lab_syntax_test_token(entry, "2.5e-3", "number") &&
         lab_syntax_test_token(entry, "@pi", "constant") && lab_syntax_test_token(entry, "(", "bracket");
    json_free(fields);
    string_free(source);
    TEST_ASSERT_TRUE(ok, "combined Function, declarations and initialisations retain exact native lexical spans");
}

static void test_lab_syntax_literals(void)
{
    string_t *source = string_new_with("matrix f(x) { `` return 123\n`comment` \"return 7\"; \"<img src=x>&\r\"; "
                                      "return $[a[b]]+.5+1e-3+@eulermascheroni+π; }\n`unfinished");
    json_t *fields = json_new_object();
    bool ok = source && fields && lab_syntax_test_set(fields, "function", source) && lab_presentation_adapt(fields);
    const json_t *entry = lab_syntax_test_entry(fields, 0u);
    ok = ok && lab_syntax_test_lossless(entry, source) &&
         lab_syntax_test_token(entry, "`` return 123", "comment") &&
         lab_syntax_test_token(entry, "`unfinished", "comment") &&
         lab_syntax_test_token(entry, "\"return 7\"", "") &&
         lab_syntax_test_token(entry, "$[a[b]]", "variable") && lab_syntax_test_token(entry, ".5", "number") &&
         lab_syntax_test_token(entry, "1e-3", "number") &&
         lab_syntax_test_token(entry, "@eulermascheroni", "constant") && lab_syntax_test_token(entry, "π", "") &&
         strstr(test_lab_text(entry, "html"), "&lt;img src=x&gt;&amp;&#13;") &&
         !strstr(test_lab_text(entry, "html"), "<img") &&
         strstr(test_lab_text(entry, "html"), "class=\"function-token-keyword\"");
    json_free(fields);
    string_free(source);
    TEST_ASSERT_TRUE(ok, "quoted and incomplete source stays lossless without highlighting words inside literals");
}

static void test_lab_syntax_variants(void)
{
    string_t *display = string_new_with("expression f(x) { return 1.23*x; }\nconst c = ?;\n");
    string_t *full = string_new_with("expression f(x) { return 1.234567*x; }\nconst c = ?;\n");
    json_t *fields = json_new_object();
    bool ok = display && full && fields && lab_syntax_test_set(fields, "display_function", display) &&
              lab_syntax_test_set(fields, "full_display_function", full) &&
              lab_syntax_test_set(fields, "function", full) &&
              lab_syntax_test_set(fields, "derivative_function", display) && lab_presentation_adapt(fields);
    const json_t *entries = test_lab_member(test_lab_member(fields, "presentation"), "function_syntax");
    ok = ok && json_array_size(entries) == 2u && lab_syntax_test_lossless(json_array_get(entries, 0u), display) &&
         lab_syntax_test_lossless(json_array_get(entries, 1u), full);
    json_free(fields);
    string_free(display);
    string_free(full);
    TEST_ASSERT_TRUE(ok, "display/full/calculus variants use exact whitespace and deduplicate identical sources");
}

static void test_lab_syntax_span_bound(void)
{
    string_t *source = string_new();
    bool ok = source != NULL;
    for (size_t i = 0u; ok && i < 512u; ++i) ok = !string_append_cstr(source, "a+");
    json_t *fields = json_new_object();
    ok = ok && fields && lab_syntax_test_set(fields, "function", source) && lab_presentation_adapt(fields);
    const json_t *entry = lab_syntax_test_entry(fields, 0u);
    ok = ok && json_array_size(test_lab_member(entry, "spans")) == 1024u && lab_syntax_test_lossless(entry, source);
    json_free(fields);
    ok = ok && !string_append_char(source, 'a');
    fields = json_new_object();
    ok = ok && fields && lab_syntax_test_set(fields, "function", source) && lab_presentation_adapt(fields);
    entry = lab_syntax_test_entry(fields, 0u);
    bool function = true;
    ok = ok && json_array_size(test_lab_member(entry, "spans")) == 0u &&
         !strcmp(test_lab_text(entry, "source"), string_c_str(source)) &&
         json_bool_value(test_lab_member(entry, "is_function"), &function) && !function &&
         !test_lab_member(entry, "html");
    json_free(fields);
    string_free(source);
    TEST_ASSERT_TRUE(ok, "1024 spans are allowed; the next span falls back to exact text without failing evaluation");
}

static void test_lab_syntax_matrix_headings(void)
{
    string_t *source = string_new_with("\tEigenVALUES \r\nλ = 1/3\n\neigenvectors\n(α, β)\n"
                                      "eigenvalues = 2\nnot eigenvectors\nEIGENVECTORS:");
    json_t *fields = json_new_object();
    bool ok = source && fields && lab_syntax_test_set(fields, "display_result", source) &&
              lab_syntax_test_set(fields, "result", source) && lab_presentation_adapt(fields);
    const json_t *entries = test_lab_member(test_lab_member(fields, "presentation"), "matrix_headings");
    const json_t *entry = json_array_get(entries, 0u);
    const json_t *spans = test_lab_member(entry, "spans");
    size_t headings = 0u;
    for (size_t i = 0u; i < json_array_size(spans); ++i) {
        const json_t *span = json_array_get(spans, i);
        if (!strcmp(test_lab_text(span, "kind"), "heading")) {
            ok = ok && !strcmp(test_lab_text(span, "display"), headings ? "eigenvectors" : "eigenvalues");
            ++headings;
        }
    }
    ok = ok && headings == 2u && json_array_size(entries) == 2u && lab_syntax_test_lossless(entry, source) &&
         !strcmp(test_lab_text(json_array_get(entries, 1u), "source"), string_c_str(source) + 1u) &&
         lab_syntax_test_token(entry, "EigenVALUES", "heading") &&
         lab_syntax_test_token(entry, "eigenvectors", "heading") &&
         strstr(test_lab_text(entry, "html"), "<span class=\"matrix-section-heading\">eigenvalues</span> &#13;\n");
    json_free(fields);
    string_free(source);
    TEST_ASSERT_TRUE(ok, "native matrix headings retain CRLF, Unicode and exact source; inline lookalikes remain plain");
}

static void test_lab_syntax_matrix_heading_bound(void)
{
    string_t *source = string_new();
    json_t *fields = json_new_object();
    bool ok = source && fields;
    for (size_t i = 0u; ok && i < 600u; ++i) ok = !string_append_cstr(source, "eigenvalues\n");
    ok = ok && lab_syntax_test_set(fields, "result", source) && lab_presentation_adapt(fields);
    const json_t *entries = test_lab_member(test_lab_member(fields, "presentation"), "matrix_headings");
    const json_t *entry = json_array_get(entries, 0u);
    ok = ok && json_array_size(test_lab_member(entry, "spans")) == 0u &&
         !strcmp(test_lab_text(entry, "source"), string_c_str(source)) && !test_lab_member(entry, "html");
    json_free(fields);
    string_free(source);
    TEST_ASSERT_TRUE(ok, "excessive matrix heading spans fall back to complete original text");
}

static void test_lab_syntax_number_display(void)
{
    static const struct { const char *source; const char *display; } cases[] = {
        {"1e-4", "0.0001"},
        {"-1.25E-005", "-1.25×10⁻⁵"},
        {"1e-12 + 2E-9i", "1×10⁻¹² + 2×10⁻⁹i"},
        {"1.234567890123456789012345678901234567890123456789e-78",
         "1.234567890123456789012345678901234567890123456789×10⁻⁷⁸"},
        {"1.25E+3", "1250"},
        {"1.25e0", "1.25"},
        {".125e1", "1.25"},
        {"1.25e5000", "1.25×10⁵⁰⁰⁰"},
        {"1e-999999999999999999999", "1×10⁻⁹⁹⁹⁹⁹⁹⁹⁹⁹⁹⁹⁹⁹⁹⁹⁹⁹⁹⁹⁹⁹"},
        {"1.234...e+78", "1.234...×10⁷⁸"},
        {"x1e5 + rate2E3 + e + E + exp(x)", "x1e5 + rate2E3 + e + E + exp(x)"},
        {"\"1e-5\" + '2E3' + `3e-5`", "\"1e-5\" + '2E3' + `3e-5`"},
        {"`` 1e-5\n1e-5", "`` 1e-5\n1×10⁻⁵"},
        {"1e+ + 1E- + 1.2.3", "1e+ + 1E- + 1.2.3"},
        {"1e-5 < 2e-5 & 3e-5", "1×10⁻⁵ &lt; 2×10⁻⁵ &amp; 3×10⁻⁵"},
    };
    bool ok = true;
    for (size_t i = 0u; i < sizeof(cases) / sizeof(*cases); ++i) {
        string_t *source = string_new_with(cases[i].source);
        json_t *fields = json_new_object();
        bool adapted = source && fields && lab_syntax_test_set(fields, "result", source) &&
                       lab_presentation_adapt(fields);
        const json_t *entries = test_lab_member(test_lab_member(fields, "presentation"), "matrix_headings");
        const json_t *entry = json_array_get(entries, 0u);
        bool match = adapted && !strcmp(test_lab_text(entry, "html"), cases[i].display) &&
                     lab_syntax_test_lossless(entry, source) &&
                     !strcmp(test_lab_text(fields, "result"), cases[i].source);
        if (!match) string_printf("number display: %s -> %s\n", cases[i].source, test_lab_text(entry, "html"));
        ok = ok && match;
        json_free(fields);
        string_free(source);
    }
    TEST_ASSERT_TRUE(ok, "native card typography preserves numeric digits and source while replacing exponent notation");
}

static void test_lab_syntax_mode_numbers(void)
{
    json_t *fields = test_lab_json(
        "{\"expression\":\"1e-5*x\",\"problem\":\"y'=1e-5*y\",\"solutions\":\"y=1e-5*exp(x)\","
        "\"function\":\"expression f() { return 1e-5. }\","
        "\"derivative\":\"d/dx = 1e-5\",\"derivative_value\":\"1e-5\","
        "\"integral\":\"∫dx = 1e-5*x\",\"integral_value\":\"1e-5\","
        "\"value\":\"1e-5\",\"error\":\"2e-20\",\"bound\":\"x in [0, 1e-5]\","
        "\"steps\":\"y=1e-5*exp(x)\"}");
    bool ok = fields && lab_presentation_adapt(fields);
    const json_t *metadata = test_lab_member(fields, "presentation");
    const json_t *headings = test_lab_member(metadata, "matrix_headings");
    const json_t *function = json_array_get(test_lab_member(metadata, "function_syntax"), 0);
    ok = ok && strstr(test_lab_text(function, "html"), ">1×10⁻⁵</span>") &&
         strstr(test_lab_text(function, "source"), "1e-5");
    unsigned matched = 0u;
    for (size_t i = 0u; i < json_array_size(headings); ++i) {
        const json_t *entry = json_array_get(headings, i);
        const char *source = test_lab_text(entry, "source"), *html = test_lab_text(entry, "html");
        if (!strstr(source, "1e-5")) continue;
        ok = ok && !strstr(html, "1e-5") && strstr(html, "1×10⁻⁵");
        if (strstr(source, "error ≈")) ok = ok && strstr(html, "2×10⁻²⁰");
        ++matched;
    }
    json_free(fields);
    TEST_ASSERT_TRUE(ok && matched >= 7u, "all numerical, symbolic, calculus and solver card sources receive native markup");
}

/* Register lexical metadata regressions before the suite's README examples. */
void test_lab_syntax_cases(void)
{
    TEST_RUN_IN_GROUP(test_lab_syntax_combined, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_syntax_literals, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_syntax_variants, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_syntax_span_bound, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_syntax_matrix_headings, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_syntax_matrix_heading_bound, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_syntax_number_display, tests, NULL);
    TEST_RUN_IN_GROUP(test_lab_syntax_mode_numbers, tests, NULL);
}
