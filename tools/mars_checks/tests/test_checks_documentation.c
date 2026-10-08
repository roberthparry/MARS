/**
 * @file test_checks_documentation.c
 * @brief Native port of the Python Markdown coverage and formula-layout tests.
 *
 * Checks the real module guides, preview stylesheet and tracked Markdown formulae.
 * The original extractor fixture is registered in test_checks_parsers.c. These
 * are read-only repository regressions controlled by the global test harness.
 */
#include <ctype.h>

#include "test_harness.h"
#include "checks_process.h"
#include "checks_controls.h"
#include "test_checks.h"

static string_t *test_checks_document(const char *relative)
{
    string_t *root = checks_text(MARS_CHECKS_ROOT_DIR);
    string_t *path = checks_path(root, relative);
    string_t *text = checks_read(path);
    string_free(path);
    string_free(root);
    return text;
}

static string_t *test_checks_css_rule(const string_t *css, const char *selector)
{
    size_t length = string_byte_length(css), start = 0;
    for (size_t i = 0; i < length; ++i) {
        if (checks_byte(css, i) != '{')
            continue;
        string_t *name = checks_slice(css, start, i - start);
        string_trim(name);
        size_t end = i + 1;
        while (end < length && checks_byte(css, end) != '}')
            ++end;
        bool found = checks_equal(name, selector);
        string_free(name);
        if (found)
            return checks_slice(css, i + 1, end - i - 1);
        start = end + 1;
        i = end;
    }
    return NULL;
}

static void test_checks_preview_formula_alignment_applies_to_every_guide(void)
{
    string_t *settings_text = test_checks_document(".vscode/settings.json");
    string_t *css_text = test_checks_document("docs/preview.css");
    json_t *settings = settings_text ? json_from_text(settings_text) : NULL;
    const json_t *styles = checks_member(settings, "markdown.styles");
    size_t count = styles ? json_array_size(styles) : 0;
    bool ok = count && checks_equal(json_string_value(json_array_get(styles, count - 1)), "docs/preview.css");
    string_t *css = css_text ? checks_strip_c(css_text, false) : NULL;
    const char *const selectors[] = {"body .katex-display", "body .katex-display > .katex", "body p:has(> .katex)"};
    for (size_t i = 0; i < 3 && css; ++i) {
        string_t *rule = test_checks_css_rule(css, selectors[i]);
        ok = rule && checks_match(rule, "text-align[[:space:]]*:[[:space:]]*left;", 0, NULL) && ok;
        if (rule && !i)
            ok = checks_match(rule, "padding-left[[:space:]]*:[[:space:]]*1em;", 0, NULL) &&
                 checks_match(rule, "overflow-x[[:space:]]*:[[:space:]]*auto;", 0, NULL) && ok;
        if (rule && i == 1)
            ok = checks_match(rule, "white-space[[:space:]]*:[[:space:]]*nowrap;", 0, NULL) && ok;
        if (rule && i == 2)
            ok = !checks_match(rule, "white-space[[:space:]]*:", 0, NULL) && ok;
        string_free(rule);
    }
    ok = css && ok;
    string_free(css);
    string_free(css_text);
    string_free(settings_text);
    json_free(settings);
    TEST_ASSERT_TRUE(
        ok, "ported preview regression applies left alignment to every guide without preventing prose wrapping");
}

static string_t *test_checks_unquote(const string_t *line)
{
    size_t start = 0;
    while (isspace(checks_byte(line, start)))
        ++start;
    if (checks_byte(line, start) == '>') {
        ++start;
        while (isspace(checks_byte(line, start)))
            ++start;
    }
    return checks_slice(line, start, string_byte_length(line) - start);
}

static bool test_checks_blank(const string_t *line)
{
    if (!line)
        return true;
    string_t *normal = test_checks_unquote(line);
    string_trim(normal);
    bool blank = !string_byte_length(normal);
    string_free(normal);
    return blank;
}

static size_t test_checks_dollars(const string_t *line)
{
    size_t dollars = 0;
    for (size_t i = 0; i < string_byte_length(line); ++i)
        dollars += checks_byte(line, i) == '$';
    return dollars;
}

static bool test_checks_indented_formulae(const string_t *text, size_t *formulae)
{
    checks_strings_t *lines = checks_split(text, '\n');
    unsigned char fence = 0;
    size_t fence_length = 0;
    bool ok = true;
    for (size_t i = 0; i < checks_strings_count(lines); ++i) {
        string_t *line = test_checks_unquote(checks_strings_get(lines, i));
        unsigned char c = checks_byte(line, 0);
        size_t marker = 0;
        if (c == '`' || c == '~')
            while (checks_byte(line, marker) == c)
                ++marker;
        if (marker >= 3) {
            if (!fence) {
                fence = c;
                fence_length = marker;
            } else if (c == fence && marker >= fence_length) {
                fence = 0;
            }
            string_free(line);
            continue;
        }
        if (!fence) {
            bool previous = !i || test_checks_blank(checks_strings_get(lines, i - 1));
            bool next = test_checks_blank(checks_strings_get(lines, i + 1));
            bool prefix = string_starts_with(line, "$\\quad\\begin{array}{l}\\displaystyle ");
            bool standalone = previous && next && checks_match(line, "^\\$[^$]+\\$[.,;:]?$", 0, NULL);
            ok = !string_starts_with(line, "$$") && ok;
            string_t *trimmed = string_clone(line);
            string_trim(trimmed);
            ok = !checks_equal(trimmed, "\\[") && !checks_equal(trimmed, "\\]") && ok;
            string_free(trimmed);
            if (prefix || standalone) {
                ok = prefix && string_ends_with(line, "\\end{array}$") && test_checks_dollars(line) == 2 &&
                     string_find(line, "\\tag{") < 0 && previous && next && ok;
                ++*formulae;
            }
        }
        string_free(line);
    }
    checks_strings_free(lines);
    return ok;
}

static void test_checks_guide_formulae_are_indented_unbroken_paragraphs(void)
{
    string_t *root = checks_text(MARS_CHECKS_ROOT_DIR);
    checks_strings_t *paths = checks_git_paths(root, false);
    bool ok = paths != NULL;
    size_t formulae = 0;
    for (size_t i = 0; paths && i < checks_strings_count(paths); ++i) {
        const string_t *relative = checks_strings_get(paths, i);
        if (!string_ends_with(relative, ".md"))
            continue;
        string_t *path = checks_path(root, string_c_str(relative));
        string_t *text = checks_read(path);
        ok = text && test_checks_indented_formulae(text, &formulae) && ok;
        string_free(text);
        string_free(path);
    }
    checks_strings_free(paths);
    string_free(root);
    TEST_ASSERT_TRUE(ok && formulae > 0, "ported tracked Markdown formulae remain indented unbroken paragraphs");
}

static void test_checks_laplace_documentation_uses_markdown_math_delimiters(void)
{
    const char *const paths[] = {"docs/expression.md", "docs/design-notes/integral-transforms.md"};
    bool ok = true;
    for (size_t p = 0; p < 2; ++p) {
        string_t *text = test_checks_document(paths[p]);
        if (!text) {
            ok = false;
            continue;
        }
        checks_strings_t *lines = checks_split(text, '\n');
        bool fenced = false;
        size_t rows = 0;
        for (size_t i = 0; i < checks_strings_count(lines); ++i) {
            const string_t *line = checks_strings_get(lines, i);
            string_t *trimmed = string_clone(line);
            string_trim(trimmed);
            if (string_starts_with(trimmed, "```") || string_starts_with(trimmed, "~~~"))
                fenced = !fenced;
            if (!fenced) {
                ok = !checks_equal(trimmed, "$") && string_find(line, "\\(") < 0 && string_find(line, "\\)") < 0 &&
                     string_find(line, "\\[") < 0 && string_find(line, "\\]") < 0 && ok;
                if (string_starts_with(line, "|") && test_checks_dollars(line)) {
                    ++rows;
                    ok = test_checks_dollars(line) % 2 == 0 && ok;
                }
            }
            string_free(trimmed);
        }
        ok = rows > 10 && ok;
        checks_strings_free(lines);
        string_free(text);
    }
    TEST_ASSERT_TRUE(ok, "ported transform-guide regression requires balanced Markdown math delimiters");
}

static void test_checks_every_public_function_is_covered_by_its_module_guide(void)
{
    string_t *root = checks_text(MARS_CHECKS_ROOT_DIR);
    int status = checks_markdown_api(root);
    string_free(root);
    TEST_ASSERT_INT_EQ(status, 0);
}

/* Register the migrated repository documentation regressions. */
void test_checks_documentation_cases(void)
{
    TEST_RUN_IN_GROUP(test_checks_preview_formula_alignment_applies_to_every_guide, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_guide_formulae_are_indented_unbroken_paragraphs, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_laplace_documentation_uses_markdown_math_delimiters, tests, NULL);
    TEST_RUN_IN_GROUP(test_checks_every_public_function_is_covered_by_its_module_guide, tests, NULL);
}
