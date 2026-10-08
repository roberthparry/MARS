/**
 * @file checks_markdown.c
 * @brief Extract public header functions and verify their module-guide coverage.
 *
 * Mirrors the original lightweight top-level C scanner, including inline
 * definitions, first declaration wins and excluded keyword spellings. This is
 * a documentation control rather than a C compiler. Names are sorted and
 * deduplicated; module dispatch uses a sorted fixed table with binary search.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "checks_controls.h"

static bool checks_markdown_identifier(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static string_t *checks_markdown_function_name(const string_t *candidate)
{
    string_t *normal = checks_normalise_space(candidate);
    if (checks_match(normal, "^(typedef|_Static_assert)([^A-Za-z0-9_]|$)", 0, NULL)) {
        string_free(normal);
        return NULL;
    }
    string_t *result = NULL;
    size_t length = string_byte_length(normal);
    for (size_t i = 0; i < length;) {
        unsigned char c = checks_byte(normal, i);
        if (!checks_markdown_identifier(c) || isdigit(c)) {
            ++i;
            continue;
        }
        size_t start = i++;
        while (checks_markdown_identifier(checks_byte(normal, i)))
            ++i;
        size_t end = i;
        while (isspace(checks_byte(normal, i)))
            ++i;
        if (checks_byte(normal, i) != '(')
            continue;
        string_t *name = checks_slice(normal, start, end - start);
        bool excluded = checks_equal(name, "if") || checks_equal(name, "for") || checks_equal(name, "while") ||
                        checks_equal(name, "switch") || checks_equal(name, "sizeof") ||
                        checks_equal(name, "_Alignof") || checks_equal(name, "__attribute__");
        if (!excluded) {
            result = name;
            break;
        }
        string_free(name);
    }
    string_free(normal);
    return result;
}

/* Extract unique public names from top-level declarations and inline bodies. */
checks_strings_t *checks_header_functions(const string_t *source)
{
    string_t *clean = checks_strip_c(source, true);
    checks_strings_t *names = checks_strings_new();
    size_t depth = 0, start = 0, length = string_byte_length(clean);
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = checks_byte(clean, i);
        if (depth) {
            if (c == '{')
                ++depth;
            else if (c == '}' && !--depth)
                start = i + 1;
            continue;
        }
        if (c != '{' && c != ';')
            continue;
        string_t *candidate = checks_slice(clean, start, i - start + (c == ';'));
        if (c == ';' || (string_find(candidate, ")") >= 0 &&
                         !checks_match(candidate, "^[[:space:]]*typedef([^A-Za-z0-9_]|$)", 0, NULL))) {
            string_t *name = checks_markdown_function_name(candidate);
            if (name)
                checks_strings_add(names, name);
        }
        string_free(candidate);
        start = i + 1;
        depth = c == '{';
    }
    checks_strings_sort(names);
    checks_strings_t *unique = checks_strings_new();
    for (size_t i = 0; i < checks_strings_count(names); ++i)
        if (!i || string_compare(checks_strings_get(names, i), checks_strings_get(names, i - 1)))
            checks_strings_add(unique, string_clone(checks_strings_get(names, i)));
    checks_strings_free(names);
    string_free(clean);
    return unique;
}

/* Require complete identifier boundaries around a documented name. */
bool checks_function_mentioned(const string_t *name, const string_t *text)
{
    string_t *pattern = string_sprintf("(^|[^A-Za-z0-9_])%s([^A-Za-z0-9_]|$)", string_c_str(name));
    bool found = checks_match(text, string_c_str(pattern), 0, NULL);
    string_free(pattern);
    return found;
}

struct module_guide {
    const char *header;
    const char *guide;
};

static int checks_markdown_compare_guide(const void *key, const void *entry)
{
    const struct module_guide *guide = entry;
    return strcmp(key, guide->header);
}

static checks_strings_t *checks_markdown_identifiers(const string_t *text)
{
    checks_strings_t *tokens = checks_strings_new();
    size_t length = string_byte_length(text);
    for (size_t i = 0; i < length;) {
        if (!checks_markdown_identifier(checks_byte(text, i))) {
            ++i;
            continue;
        }
        size_t start = i++;
        while (checks_markdown_identifier(checks_byte(text, i)))
            ++i;
        checks_strings_add(tokens, checks_slice(text, start, i - start));
    }
    checks_strings_sort(tokens);
    return tokens;
}

/* Check installed headers against their existing Markdown module guides. */
int checks_markdown_api(const string_t *root)
{
    static const struct module_guide guides[] = {{"almanac.h", "almanac.md"},
                                                 {"array.h", "array.md"},
                                                 {"bitset.h", "bitset.md"},
                                                 {"datetime.h", "datetime.md"},
                                                 {"dictionary.h", "dictionary.md"},
                                                 {"diffequation.h", "diffequation.md"},
                                                 {"equation.h", "equation.md"},
                                                 {"expression.h", "expression.md"},
                                                 {"file.h", "file.md"},
                                                 {"http.h", "http.md"},
                                                 {"integrator.h", "integrator.md"},
                                                 {"json.h", "json.md"},
                                                 {"jurisdiction.h", "jurisdiction.md"},
                                                 {"matrix.h", "matrix.md"},
                                                 {"number.h", "number.md"},
                                                 {"protobuf.h", "protobuf.md"},
                                                 {"qcomplex.h", "qcomplex.md"},
                                                 {"qfloat.h", "qfloat.md"},
                                                 {"set.h", "set.md"},
                                                 {"sqlite.h", "sqlite.md"},
                                                 {"timeseries.h", "timeseries.md"},
                                                 {"ustring.h", "string.md"},
                                                 {"webserver.h", "webserver.md"},
                                                 {"xml.h", "xml.md"}};
    checks_strings_t *paths = checks_files(root, "include", ".h", false), *missing = checks_strings_new();
    size_t total = 0;
    int result = 0;
    for (size_t i = 0; i < checks_strings_count(paths); ++i) {
        const string_t *relative = checks_strings_get(paths, i);
        string_t *path = checks_path(root, string_c_str(relative));
        string_t *source = checks_read(path);
        string_free(path);
        if (!source) {
            result = 2;
            break;
        }
        checks_strings_t *names = checks_header_functions(source);
        string_free(source);
        string_t *header =
            checks_slice(relative, sizeof("include/") - 1, string_byte_length(relative) - sizeof("include/") + 1);
        const struct module_guide *guide = bsearch(string_c_str(header), guides, sizeof(guides) / sizeof(*guides),
                                                   sizeof(*guides), checks_markdown_compare_guide);
        if (checks_strings_count(names) && !guide) {
            string_fprintf(stderr, "no Markdown module guide is assigned to %s\n", string_c_str(header));
            string_free(header);
            checks_strings_free(names);
            result = 1;
            break;
        }
        string_t *text = NULL;
        if (checks_strings_count(names)) {
            string_t *relative_guide = string_sprintf("docs/%s", guide->guide);
            path = checks_path(root, string_c_str(relative_guide));
            text = checks_read(path);
            string_free(path);
            string_free(relative_guide);
            if (!text) {
                string_free(header);
                checks_strings_free(names);
                result = 2;
                break;
            }
        }
        total += checks_strings_count(names);
        checks_strings_t *mentioned = text ? checks_markdown_identifiers(text) : checks_strings_new();
        for (size_t j = 0; j < checks_strings_count(names); ++j) {
            const string_t *name = checks_strings_get(names, j);
            if (!checks_strings_has(mentioned, string_c_str(name)))
                checks_strings_add(missing, string_sprintf("  %s: %s()", string_c_str(header), string_c_str(name)));
        }
        checks_strings_free(mentioned);
        string_free(header);
        string_free(text);
        checks_strings_free(names);
    }
    if (!result && checks_strings_count(missing)) {
        fprintf(stderr, "Markdown public API coverage is incomplete:\n");
        for (size_t i = 0; i < checks_strings_count(missing); ++i)
            string_fprintf(stderr, "%s\n", string_c_str(checks_strings_get(missing, i)));
        result = 1;
    }
    if (!result)
        printf("Markdown module guides cover %zu public functions\n", total);
    if (result == 2)
        fprintf(stderr, "Markdown public API check could not inspect the repository\n");
    checks_strings_free(missing);
    checks_strings_free(paths);
    return result;
}
