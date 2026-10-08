/**
 * @file checks_inline.c
 * @brief Physical-line inline scanner used by native source-policy audits.
 *
 * Masks comments, literals and directives without losing physical newlines.
 * Tracks top-level declarations and nested bodies across all conditional
 * branches, including GNU inline spellings and continued macro definitions.
 * It deliberately does not attempt to prove that a short function is trivial.
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "checks_policy.h"

static bool policy_word_byte(unsigned char c)
{
    return isalnum(c) || c == '_' || c >= 128;
}

static bool policy_inline_word(const char *word, size_t length)
{
    return (length == 6 && !memcmp(word, "inline", 6)) ||
           (length == 8 && !memcmp(word, "__inline", 8)) ||
           (length == 10 && !memcmp(word, "__inline__", 10));
}

static string_t *policy_mask_literals(const string_t *source)
{
    string_t *masked = checks_text("");
    const char *s = string_c_str(source);
    size_t length = string_byte_length(source);
    for (size_t i = 0; i < length;) {
        size_t end = i;
        if (s[i] == '/' && i + 1 < length && s[i + 1] == '*') {
            end = i + 2;
            while (end + 1 < length && !(s[end] == '*' && s[end + 1] == '/'))
                ++end;
            end = end + 1 < length ? end + 2 : length;
        } else if (s[i] == '/' && i + 1 < length && s[i + 1] == '/') {
            end = i + 2;
            while (end < length) {
                if (s[end] == '\\' && end + 1 < length && s[end + 1] == '\n')
                    end += 2;
                else if (s[end] == '\\' && end + 2 < length && s[end + 1] == '\r' && s[end + 2] == '\n')
                    end += 3;
                else if (s[end] == '\n')
                    break;
                else
                    ++end;
            }
        } else if (s[i] == '"' || s[i] == '\'') {
            char quote = s[i];
            end = i + 1;
            while (end < length) {
                if (s[end] == '\\' && end + 1 < length)
                    end += 2;
                else if (s[end++] == quote)
                    break;
            }
        }
        if (end == i) {
            size_t start = i++;
            while (i < length && s[i] != '/' && s[i] != '"' && s[i] != '\'')
                ++i;
            if (string_append_utf8_exact(masked, s + start, i - start))
                checks_fatal("masking inline source");
        } else {
            for (; i < end; ++i)
                string_append_char(masked, s[i] == '\n' ? '\n' : ' ');
        }
    }
    return masked;
}

/* Scan physical source without preprocessing away inactive branches. */
bool checks_policy_scan_inline(const string_t *source, checks_strings_t *spans, checks_strings_t *macros,
                               checks_strings_t *errors)
{
    string_t *masked = policy_mask_literals(source);
    const char *s = string_c_str(masked);
    size_t length = string_byte_length(masked), line = 1, declaration = 0, inline_start = 0;
    size_t braces = 0;
    int parentheses = 0;
    bool line_start = true, has_inline = false, has_parenthesis = false;
    for (size_t i = 0; i < length;) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\n') {
            ++line;
            line_start = true;
            ++i;
            continue;
        }
        if (isspace(c)) {
            ++i;
            continue;
        }
        if (line_start && c == '#') {
            size_t directive_line = line;
            bool define = false, first = true, concealed = false;
            ++i;
            while (i < length && s[i] != '\n') {
                if (s[i] == '\\' && (s[i + 1] == '\n' ||
                    (s[i + 1] == '\r' && i + 2 < length && s[i + 2] == '\n'))) {
                    i += s[i + 1] == '\r' ? 3 : 2;
                    ++line;
                } else if (isalpha((unsigned char)s[i]) || s[i] == '_') {
                    size_t start = i++;
                    while (i < length && policy_word_byte((unsigned char)s[i]))
                        ++i;
                    if (first)
                        define = i - start == 6 && !memcmp(s + start, "define", 6);
                    first = false;
                    concealed |= policy_inline_word(s + start, i - start);
                } else {
                    if (!isspace((unsigned char)s[i]))
                        first = false;
                    ++i;
                }
            }
            if (define && concealed)
                checks_strings_add(macros, string_sprintf("%zu", directive_line));
            continue;
        }
        line_start = false;
        if (braces) {
            if (c == '{')
                ++braces;
            else if (c == '}' && !--braces) {
                if (inline_start)
                    checks_strings_add(spans, string_sprintf("%zu:%zu", inline_start, line));
                declaration = inline_start = 0;
                has_inline = has_parenthesis = false;
            }
            ++i;
        } else if (c == '{') {
            inline_start = has_inline && has_parenthesis ? declaration : 0;
            braces = 1;
            parentheses = 0;
            ++i;
        } else if (c == ';' && !parentheses) {
            declaration = 0;
            has_inline = has_parenthesis = false;
            ++i;
        } else {
            if (!declaration)
                declaration = line;
            size_t start = i++;
            if (isalpha(c) || c == '_') {
                while (i < length && policy_word_byte((unsigned char)s[i]))
                    ++i;
                has_inline |= policy_inline_word(s + start, i - start);
            }
            if (c == '(') {
                ++parentheses;
                has_parenthesis = true;
            } else if (c == ')')
                --parentheses;
        }
    }
    if (inline_start)
        checks_strings_add(errors, string_sprintf("unterminated inline definition at line %zu", inline_start));
    string_free(masked);
    return !inline_start;
}

/* Convert scanner spans into the source policy's diagnostic wording. */
void checks_policy_inline(const string_t *source, checks_strings_t *errors)
{
    checks_strings_t *spans = checks_strings_new(), *macros = checks_strings_new();
    checks_policy_scan_inline(source, spans, macros, errors);
    for (size_t i = 0; i < checks_strings_count(spans); ++i) {
        size_t start = 0, end = 0;
        sscanf(string_c_str(checks_strings_get(spans, i)), "%zu:%zu", &start, &end);
        if (end - start + 1 > 3)
            checks_strings_add(errors, string_sprintf(
                "line %zu: inline definition occupies %zu physical lines (maximum 3)", start, end - start + 1));
    }
    for (size_t i = 0; i < checks_strings_count(macros); ++i)
        checks_strings_add(errors, string_sprintf("line %s: macro conceals inline code or its declaration",
                                                 string_c_str(checks_strings_get(macros, i))));
    checks_strings_free(spans);
    checks_strings_free(macros);
}
