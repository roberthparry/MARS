/**
 * @file diffequ_stringout.c
 * @brief Differential-equation native and TeX rendering.
 *
 * Formats problems, differential forms and derivative notation from their native representation. Display rules
 * preserve the equation and condition structure independently of the solving strategy.
 *
 * This is an implementation unit for diffequ_t. Specialised solving paths apply only to recognised
 * families; callers must inspect result status and any retained conditions.
 */

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "TeX-t.h"
#include "diffequation-t.h"

#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_notation_internal.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"

static int de_append_superscript(string_t *out, size_t value)
{
    static const char *const digits[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
    char text[3u * sizeof(size_t) + 1u];

    if (snprintf(text, sizeof(text), "%zu", value) < 0)
        return -1;
    for (const char *cursor = text; *cursor; ++cursor) {
        if (string_append_cstr(out, digits[(size_t)(*cursor - '0')]) != 0)
            return -1;
    }
    return 0;
}

static int de_is_derivative_letter(char value)
{
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

static int de_append_next_character(string_t *out, const char *text, size_t *position);

/* Preserve source order while displaying simple integer powers consistently with derivative orders. */
static string_t *de_powers_to_superscripts(const string_t *source)
{
    const char *text = source ? string_c_str(source) : "";
    string_t *out = string_new();
    bool chained = false;

    if (!out)
        return NULL;
    for (size_t i = 0u; text[i];) {
        if (text[i] != '^') {
            if (!isspace((unsigned char)text[i]))
                chained = false;
            if (de_append_next_character(out, text, &i) != 0)
                goto fail;
            continue;
        }
        size_t start = i + 1u;
        while (isspace((unsigned char)text[start]))
            ++start;
        bool enclosed = text[start] == '(';
        if (enclosed) {
            ++start;
            while (isspace((unsigned char)text[start]))
                ++start;
        }
        size_t end = start, value = 0u;
        bool overflow = false;
        while (isdigit((unsigned char)text[end])) {
            unsigned int digit = (unsigned int)(text[end++] - '0');
            if (value > ((size_t)INT_MAX - digit) / 10u)
                overflow = true;
            if (!overflow)
                value = 10u * value + digit;
        }
        if (end == start) {
            if (string_append_char(out, text[i++]) != 0)
                goto fail;
            chained = false;
            continue;
        }
        size_t next = end;
        while (isspace((unsigned char)text[next]))
            ++next;
        bool integer = !overflow;
        if (enclosed) {
            integer = integer && text[next] == ')';
            if (text[next] == ')') {
                end = ++next;
                while (isspace((unsigned char)text[next]))
                    ++next;
            }
        } else {
            /* Do not truncate decimal/scientific exponents or the parser's ^1/n root shorthand. */
            integer = integer && !(text[end] == '.' && isdigit((unsigned char)text[end + 1u])) &&
                      text[end] != 'e' && text[end] != 'E' && text[next] != '/';
        }
        /* Adjacent superscripts would merge a power chain into one exponent; retain its original syntax. */
        bool continues = text[next] == '^';
        unsigned char lead = (unsigned char)text[next];
        bool superscript = lead == 0xc2u || (lead == 0xe2u && (unsigned char)text[next + 1u] == 0x81u);
        bool previous_superscript = (i >= 2u && (unsigned char)text[i - 2u] == 0xc2u) ||
                                    (i >= 3u && (unsigned char)text[i - 3u] == 0xe2u &&
                                     (unsigned char)text[i - 2u] == 0x81u);
        if (integer && !chained && !continues && !superscript && !previous_superscript && text[next] != '!') {
            if (de_append_superscript(out, value) != 0)
                goto fail;
        } else if (string_append_chars(out, text + i, end - i) != 0) {
            goto fail;
        }
        chained = continues;
        i = end;
    }
    return out;

fail:
    string_free(out);
    return NULL;
}

static size_t de_derivative_letter_length(const char *text)
{
    unsigned char lead;
    size_t length;

    if (!text || !*text)
        return 0u;
    if (de_is_derivative_letter(*text))
        return 1u;
    lead = (unsigned char)*text;
    if ((lead & 0xe0u) == 0xc0u)
        length = 2u;
    else if ((lead & 0xf0u) == 0xe0u)
        length = 3u;
    else if ((lead & 0xf8u) == 0xf0u)
        length = 4u;
    else
        return 0u;
    for (size_t i = 1u; i < length; ++i) {
        if (!text[i] || ((unsigned char)text[i] & 0xc0u) != 0x80u)
            return 0u;
    }
    return length;
}

static int de_append_next_character(string_t *out, const char *text, size_t *position)
{
    size_t length = de_derivative_letter_length(text + *position);

    if (length == 0u)
        length = 1u;
    if (string_append_chars(out, text + *position, length) != 0)
        return -1;
    *position += length;
    return 0;
}

static string_t *de_normalise_greek_slice(const char *text, size_t start, size_t end)
{
    size_t length = end - start;
    char *alias;
    string_t *alias_text;
    string_t *normalized;

    alias = malloc(length + 1u);
    if (!alias)
        return NULL;
    memcpy(alias, text + start, length);
    alias[length] = '\0';
    alias_text = string_new_with(alias);
    free(alias);
    if (!alias_text)
        return NULL;
    normalized = expr_normalise_greek_alias_text(alias_text);
    string_free(alias_text);
    return normalized;
}

static string_t *de_greek_aliases_to_symbols(const string_t *source)
{
    const char *text = source ? string_c_str(source) : "";
    string_t *out = string_new();
    size_t index = 0u;

    if (!out)
        return NULL;
    while (text[index]) {
        size_t start = index;
        size_t end;
        string_t *normalized = NULL;

        if (text[index] == '@' && isalpha((unsigned char)text[index + 1u])) {
            start = ++index;
            while (isalpha((unsigned char)text[index]))
                ++index;
            normalized = de_normalise_greek_slice(text, start, index);
            if (normalized) {
                if (string_append_string(out, normalized) != 0)
                    goto fail_normalized;
                string_free(normalized);
                continue;
            }
            index = start - 1u;
        } else if (isalpha((unsigned char)text[index])) {
            while (isalpha((unsigned char)text[index]))
                ++index;
            end = index;
            normalized = de_normalise_greek_slice(text, start, end);
            if (!normalized && end - start > 1u && text[start] == 'd') {
                normalized = de_normalise_greek_slice(text, start + 1u, end);
                if (normalized && string_append_char(out, 'd') != 0)
                    goto fail_normalized;
            }
            if (normalized) {
                if (string_append_string(out, normalized) != 0)
                    goto fail_normalized;
                string_free(normalized);
            } else if (string_append_chars(out, text + start, end - start) != 0) {
                goto fail;
            }
            continue;
        }

        if (de_append_next_character(out, text, &index) != 0)
            goto fail;
        continue;

    fail_normalized:
        string_free(normalized);
        goto fail;
    }
    return out;

fail:
    string_free(out);
    return NULL;
}

static int de_append_standard_derivative(string_t *out, const char *suffix, size_t suffix_length, size_t order,
                                         const char *dependent, size_t dependent_length, bool partial)
{
    if (string_append_cstr(out, partial ? "∂" : "d") != 0)
        return -1;
    if (order > 1u && de_append_superscript(out, order) != 0)
        return -1;
    if (string_append_chars(out, dependent, dependent_length) != 0 || string_append_char(out, '/') != 0)
        return -1;

    if (!partial) {
        size_t variable_length = de_derivative_letter_length(suffix);

        if (string_append_char(out, 'd') != 0 || variable_length == 0u ||
            string_append_chars(out, suffix, variable_length) != 0)
            return -1;
        return order > 1u ? de_append_superscript(out, order) : 0;
    }

    {
        size_t *offsets = calloc(order ? order : 1u, sizeof(*offsets));
        size_t *lengths = calloc(order ? order : 1u, sizeof(*lengths));
        size_t cursor = 0u;
        int status = 0;

        if (!offsets || !lengths) {
            free(lengths);
            free(offsets);
            return -1;
        }
        for (size_t i = 0u; i < order; ++i) {
            offsets[i] = cursor;
            lengths[i] = de_derivative_letter_length(suffix + cursor);
            if (lengths[i] == 0u || cursor + lengths[i] > suffix_length) {
                status = -1;
                goto partial_cleanup;
            }
            cursor += lengths[i];
        }
        if (cursor != suffix_length) {
            status = -1;
            goto partial_cleanup;
        }
        for (size_t index = order; index > 0u;) {
            size_t variable = index - 1u;
            size_t multiplicity = 1u;

            while (index > multiplicity) {
                size_t previous = index - multiplicity - 1u;

                if (lengths[previous] != lengths[variable] ||
                    memcmp(suffix + offsets[previous], suffix + offsets[variable], lengths[variable]) != 0)
                    break;
                ++multiplicity;
            }
            if (string_append_cstr(out, "∂") != 0 ||
                string_append_chars(out, suffix + offsets[variable], lengths[variable]) != 0 ||
                (multiplicity > 1u && de_append_superscript(out, multiplicity) != 0)) {
                status = -1;
                goto partial_cleanup;
            }
            index -= multiplicity;
        }

    partial_cleanup:
        free(lengths);
        free(offsets);
        return status;
    }
}

static string_t *de_derivatives_to_standard_text(const string_t *source, bool partial)
{
    const char *text = source ? string_c_str(source) : "";
    string_t *out = string_new();
    size_t index = 0u;

    if (!out)
        return NULL;
    while (text[index]) {
        size_t suffix_end;
        size_t order = 0u;
        size_t dependent_start;
        size_t dependent_end;

        if (text[index] != 'D' || de_derivative_letter_length(text + index + 1u) == 0u) {
            if (de_append_next_character(out, text, &index) != 0)
                goto fail;
            continue;
        }

        suffix_end = index + 1u;
        {
            size_t length;

            while ((length = de_derivative_letter_length(text + suffix_end)) > 0u) {
                suffix_end += length;
                order++;
            }
        }
        if (text[suffix_end] != '(') {
            if (de_append_next_character(out, text, &index) != 0)
                goto fail;
            continue;
        }

        dependent_start = suffix_end + 1u;
        dependent_end = dependent_start;
        while (text[dependent_end] && text[dependent_end] != ')' && text[dependent_end] != '(' &&
               text[dependent_end] != ' ' && text[dependent_end] != '\t' && text[dependent_end] != '\r' &&
               text[dependent_end] != '\n')
            ++dependent_end;
        if (dependent_end == dependent_start || text[dependent_end] != ')') {
            if (de_append_next_character(out, text, &index) != 0)
                goto fail;
            continue;
        }

        if (de_append_standard_derivative(out, text + index + 1u, suffix_end - index - 1u, order,
                                          text + dependent_start, dependent_end - dependent_start, partial) != 0)
            goto fail;
        index = dependent_end + 1u;
    }
    return out;

fail:
    string_free(out);
    return NULL;
}

static string_t *de_conditions_to_text(const diffequ_t *de, bool partial_derivatives)
{
    string_t *out = string_new();

    if (!out)
        return NULL;

    for (size_t i = 0u; i < de->condition_count; ++i) {
        string_t *condition = de_derivatives_to_standard_text(de->condition_texts[i], partial_derivatives);

        if (!condition)
            goto fail;
        if (i > 0u && string_append_cstr(out, ", ") != 0)
            goto condition_fail;
        if (string_append_string(out, condition) != 0)
            goto condition_fail;
        string_free(condition);
        continue;

    condition_fail:
        string_free(condition);
        goto fail;
    }
    return out;

fail:
    string_free(out);
    return NULL;
}

static string_t *de_to_expression_text(const diffequ_t *de)
{
    string_t *canonical_equation;
    string_t *display_equation;
    string_t *equation;
    string_t *conditions;
    string_t *out;
    bool partial_derivatives;

    if (!de || !de->equation)
        return string_new_with("NULL");

    partial_derivatives = de->independent_count > 1u || de->partial_derivative_input;
    canonical_equation = de->differential_form_input && string_length(de->differential_form_text) > 0u
                             ? string_new_with(string_c_str(de->differential_form_text))
                         : string_length(de->equation_text) > 0u ? string_new_with(string_c_str(de->equation_text))
                                                                 : equ_to_text(de->equation, style_UNBOUND);
    display_equation = de->differential_form_input ? de_greek_aliases_to_symbols(canonical_equation)
                                                   : string_clone(canonical_equation);
    string_free(canonical_equation);
    equation = de_derivatives_to_standard_text(display_equation, partial_derivatives);
    string_free(display_equation);
    conditions = de_conditions_to_text(de, partial_derivatives);
    if (!equation || !conditions) {
        string_free(conditions);
        string_free(equation);
        return NULL;
    }

    out = string_sprintf("{ %S | %S; %S; %S }", equation, de->independent_text, de->constant_text, conditions);

    string_free(conditions);
    string_free(equation);
    string_t *display = out ? de_powers_to_superscripts(out) : NULL;
    string_free(out);
    return display;
}

/* Select native text or semantic notation without invoking a solution path. */
string_t *de_to_text(const diffequ_t *de, style_t style)
{
    if (!de)
        return string_new_with("NULL");
    if (style == style_UNBOUND)
        return equ_to_text(de->equation, style_UNBOUND);
    if (style == style_LATEX)
        return de_to_TeX(de);
    return de_to_expression_text(de);
}

char *de_to_string(const diffequ_t *de, style_t style)
{
    string_t *text = de_to_text(de, style);
    char *out = text ? strdup(string_c_str(text)) : NULL;

    string_free(text);
    return out;
}
