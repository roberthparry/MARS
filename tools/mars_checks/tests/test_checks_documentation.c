/**
 * @file test_checks_documentation.c
 * @brief Native port of the Python Markdown coverage and formula-layout tests.
 *
 * Checks the real module guides, embedded preview styles and tracked Markdown formulae.
 * The original extractor fixture is registered in test_checks_parsers.c. These
 * are read-only repository regressions controlled by the global test harness.
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "checks_process.h"
#include "checks_controls.h"
#include "test_harness.h"
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

static bool test_checks_document_has_math(const string_t *text)
{
    const char *p = string_c_str(text);
    unsigned fence = 0, inline_ticks = 0, dollars = 0;
    char fence_char = 0;
    while (*p) {
        const char *end = strchr(p, '\n');
        if (!end)
            end = p + strlen(p);
        const char *line = p;
        while (line < end && (*line == ' ' || *line == '\t' || *line == '>'))
            ++line;
        unsigned count = 0;
        if (line < end && (*line == '`' || *line == '~'))
            while (line + count < end && line[count] == *line)
                ++count;
        if (count >= 3 && (!fence || (*line == fence_char && count >= fence))) {
            if (fence) {
                fence = 0;
            } else {
                fence = count;
                fence_char = *line;
            }
        } else if (!fence) {
            for (const char *at = line; at < end; ++at) {
                if (*at == '\\' && at + 1 < end) {
                    if (!inline_ticks && (at[1] == '[' || at[1] == '('))
                        return true;
                    ++at;
                } else if (*at == '`') {
                    unsigned ticks = 1;
                    while (at + 1 < end && at[1] == '`') {
                        ++ticks;
                        ++at;
                    }
                    if (!inline_ticks)
                        inline_ticks = ticks;
                    else if (inline_ticks == ticks)
                        inline_ticks = 0;
                } else if (!inline_ticks && *at == '$' && ++dollars >= 2) {
                    return true;
                }
            }
        }
        p = *end ? end + 1 : end;
    }
    return false;
}

static bool test_checks_formula_styles(const string_t *text)
{
    const char *marker = "<style data-mars-formula-preview>";
    string_offset_t start = string_find(text, marker);
    size_t content_start = start >= 0 ? (size_t)start + strlen(marker) : 0;
    string_t *tail = start >= 0 ? checks_slice(text, content_start, string_byte_length(text) - content_start) : NULL;
    string_offset_t end = tail ? string_find(tail, "</style>") : -1;
    string_t *css_text = end >= 0 ? checks_slice(tail, 0, (size_t)end) : NULL;
    string_t *css = css_text ? checks_strip_c(css_text, false) : NULL;
    bool ok = css && string_find(tail, marker) < 0;
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
    string_free(css);
    string_free(css_text);
    string_free(tail);
    return ok;
}

static void test_checks_preview_formula_alignment_applies_to_every_guide(void)
{
    string_t *root = checks_text(MARS_CHECKS_ROOT_DIR);
    checks_strings_t *paths = checks_git_paths(root, false);
    bool ok = paths != NULL;
    size_t styled = 0;
    for (size_t i = 0; paths && i < checks_strings_count(paths); ++i) {
        const string_t *relative = checks_strings_get(paths, i);
        if (!string_ends_with(relative, ".md"))
            continue;
        string_t *text = test_checks_document(string_c_str(relative));
        if (text && test_checks_document_has_math(text)) {
            bool valid = test_checks_formula_styles(text);
            if (!valid)
                fprintf(stderr, "%s: missing or invalid embedded formula styles\n", string_c_str(relative));
            ok = valid && ok;
            ++styled;
        }
        ok = text && ok;
        string_free(text);
    }
    checks_strings_free(paths);
    string_free(root);
    TEST_ASSERT_TRUE(
        ok && styled, "mathematical guides embed left alignment and scrolling without preventing prose wrapping");
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

static bool test_checks_indented_formulae(const string_t *text, const string_t *path, size_t *formulae)
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
            bool valid = !string_starts_with(line, "$$");
            string_t *trimmed = string_clone(line);
            string_trim(trimmed);
            valid = !checks_equal(trimmed, "\\[") && !checks_equal(trimmed, "\\]") && valid;
            string_free(trimmed);
            if (prefix || standalone) {
                valid = prefix && string_ends_with(line, "\\end{array}$") && test_checks_dollars(line) == 2 &&
                        string_find(line, "\\tag{") < 0 && previous && next && valid;
                ++*formulae;
            }
            if (!valid)
                fprintf(stderr, "%s:%zu: formula must be an indented, unbroken Markdown paragraph\n",
                        string_c_str(path), i + 1);
            ok = valid && ok;
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
        if (!text)
            fprintf(stderr, "%s: could not read guide\n", string_c_str(relative));
        ok = text && test_checks_indented_formulae(text, relative, &formulae) && ok;
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
                bool valid = !checks_equal(trimmed, "$") && string_find(line, "\\(") < 0 &&
                             string_find(line, "\\)") < 0 && string_find(line, "\\[") < 0 &&
                             string_find(line, "\\]") < 0;
                if (string_starts_with(line, "|") && test_checks_dollars(line)) {
                    ++rows;
                    valid = test_checks_dollars(line) % 2 == 0 && valid;
                }
                if (!valid)
                    fprintf(stderr, "%s:%zu: use balanced Markdown math delimiters\n", paths[p], i + 1);
                ok = valid && ok;
            }
            string_free(trimmed);
        }
        if (rows <= 10)
            fprintf(stderr, "%s: expected more than ten mathematical table rows, found %zu\n", paths[p], rows);
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
