#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diffequation.h"
#define MARS_DIFFEQUATION_INTERNAL_ACCESS
#include "diffequ_internal.h"
#define MARS_EQUATION_INTERNAL_ACCESS
#include "equation/equation_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

typedef struct {
    char *equation;
    char *independent;
    char *constants;
    char *conditions;
} de_parse_parts_t;

typedef struct {
    size_t start;
    size_t end;
    char *name;
} de_differential_marker_t;

static int de_append_next_utf8_character(string_t *out,
                                         const char *text,
                                         size_t *position);

static char *de_trimmed_copy(const char *start, size_t length)
{
    char *copy;

    while (length > 0u && isspace((unsigned char)*start)) {
        start++;
        length--;
    }
    while (length > 0u && isspace((unsigned char)start[length - 1u]))
        length--;

    copy = malloc(length + 1u);
    if (!copy)
        return NULL;
    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
}

static size_t de_utf8_character_length(const char *text)
{
    unsigned char lead;
    size_t length = 1u;

    if (!text || !*text)
        return 0u;
    lead = (unsigned char)*text;
    if ((lead & 0xe0u) == 0xc0u)
        length = 2u;
    else if ((lead & 0xf0u) == 0xe0u)
        length = 3u;
    else if ((lead & 0xf8u) == 0xf0u)
        length = 4u;
    else if (lead >= 0x80u)
        return 0u;
    for (size_t i = 1u; i < length; ++i) {
        if (!text[i] ||
            ((unsigned char)text[i] & 0xc0u) != 0x80u)
            return 0u;
    }
    return length;
}

static char *de_normalise_greek_alias(const char *text, size_t length)
{
    string_t *alias = string_new();
    string_t *normalised;
    char *result;

    if (!alias || string_append_chars(alias, text, length) != 0) {
        string_free(alias);
        return NULL;
    }
    normalised = expr_normalise_greek_alias_text(alias);
    string_free(alias);
    if (!normalised)
        return NULL;
    result = strdup(string_c_str(normalised));
    string_free(normalised);
    return result;
}

static bool de_differential_name(const char *text,
                                 size_t start,
                                 size_t limit,
                                 size_t *end_out,
                                 char **name_out)
{
    size_t cursor = start;
    char *name = NULL;

    if (!text || cursor >= limit || text[cursor] != 'd')
        return false;
    cursor++;
    if (cursor >= limit)
        return false;

    if (text[cursor] == '@') {
        size_t alias_start = ++cursor;

        while (cursor < limit &&
               isalpha((unsigned char)text[cursor]))
            cursor++;
        if (cursor == alias_start)
            return false;
        name = de_normalise_greek_alias(
            text + alias_start, cursor - alias_start);
    } else if (isalpha((unsigned char)text[cursor])) {
        size_t name_start = cursor;

        while (cursor < limit &&
               isalpha((unsigned char)text[cursor]))
            cursor++;
        if (cursor - name_start == 1u)
            name = de_trimmed_copy(text + name_start, 1u);
        else
            name = de_normalise_greek_alias(
                text + name_start, cursor - name_start);
    } else if ((unsigned char)text[cursor] >= 0x80u) {
        size_t length = de_utf8_character_length(text + cursor);

        if (length == 0u || cursor + length > limit)
            return false;
        name = de_trimmed_copy(text + cursor, length);
        cursor += length;
    }

    if (!name)
        return false;
    if (cursor < limit &&
        (isalnum((unsigned char)text[cursor]) ||
         text[cursor] == '_' || text[cursor] == '@' ||
         (unsigned char)text[cursor] >= 0x80u)) {
        free(name);
        return false;
    }
    if (end_out)
        *end_out = cursor;
    if (name_out)
        *name_out = name;
    else
        free(name);
    return true;
}

static void de_equation_span(const char *text,
                             size_t *start_out,
                             size_t *end_out)
{
    size_t start = 0u;
    size_t length = strlen(text);
    size_t end = length;
    int parens = 0;
    int brackets = 0;

    while (start < length && isspace((unsigned char)text[start]))
        start++;
    if (start < length && text[start] == '{')
        start++;

    for (size_t i = start; i < length; ++i) {
        char ch = text[i];

        if (ch == '(')
            parens++;
        else if (ch == ')' && parens > 0)
            parens--;
        else if (ch == '[')
            brackets++;
        else if (ch == ']' && brackets > 0)
            brackets--;
        else if (parens == 0 && brackets == 0 &&
                 (ch == '|' || ch == ';')) {
            end = i;
            break;
        }
    }
    *start_out = start;
    *end_out = end;
}

static bool de_name_is_one_of(const char *name,
                              char *const *variables,
                              size_t variable_count)
{
    for (size_t i = 0u; i < variable_count; ++i) {
        if (strcmp(name, variables[i]) == 0)
            return true;
    }
    return false;
}

static char *de_normalize_differential_variable_names(
    const char *text,
    char *const *variables,
    size_t variable_count)
{
    string_t *out = string_new();
    size_t cursor = 0u;
    char *result;

    if (!out)
        return NULL;
    while (text[cursor]) {
        size_t token_start = cursor;
        size_t alias_start;
        char *normalised;

        if (text[cursor] == '@') {
            alias_start = ++cursor;
            while (isalpha((unsigned char)text[cursor]))
                cursor++;
            if (cursor == alias_start) {
                if (string_append_char(out, '@') != 0)
                    goto fail;
                continue;
            }
        } else if (isalpha((unsigned char)text[cursor])) {
            alias_start = cursor++;
            while (isalpha((unsigned char)text[cursor]))
                cursor++;
        } else {
            if (de_append_next_utf8_character(out, text, &cursor) != 0)
                goto fail;
            continue;
        }

        normalised = de_normalise_greek_alias(
            text + alias_start, cursor - alias_start);
        if (normalised &&
            de_name_is_one_of(normalised, variables, variable_count)) {
            if (string_append_cstr(out, normalised) != 0) {
                free(normalised);
                goto fail;
            }
        } else if (string_append_chars(
                       out, text + token_start, cursor - token_start) != 0) {
            free(normalised);
            goto fail;
        }
        free(normalised);
    }
    result = strdup(string_c_str(out));
    string_free(out);
    return result;

fail:
    string_free(out);
    return NULL;
}

static int de_independent_variable_score(const char *name)
{
    if (strcmp(name, "x") == 0)
        return 4;
    if (strcmp(name, "t") == 0)
        return 3;
    if (strcmp(name, "θ") == 0)
        return 2;
    if (strcmp(name, "φ") == 0)
        return 1;
    if ((unsigned char)name[0] >= 0x80u)
        return 1;
    return 0;
}

static bool de_character_ends_factor(unsigned char ch)
{
    return isalnum(ch) || ch == ')' || ch == ']' || ch == '_' ||
           ch >= 0x80u;
}

static bool de_character_starts_factor(unsigned char ch)
{
    return isalnum(ch) || ch == '@' || ch == '_' || ch >= 0x80u;
}

static char *de_normalize_spaced_products(const char *text)
{
    string_t *out = string_new();
    size_t cursor = 0u;
    char *result;

    if (!out)
        return NULL;
    while (text[cursor]) {
        size_t space_start;
        unsigned char previous;
        unsigned char next;

        if (!isspace((unsigned char)text[cursor])) {
            if (de_append_next_utf8_character(out, text, &cursor) != 0)
                goto fail;
            continue;
        }
        space_start = cursor;
        while (isspace((unsigned char)text[cursor]))
            cursor++;
        if (string_append_chars(
                out, text + space_start, cursor - space_start) != 0)
            goto fail;
        if (space_start == 0u || !text[cursor])
            continue;
        previous = (unsigned char)text[space_start - 1u];
        next = (unsigned char)text[cursor];
        if (de_character_ends_factor(previous) &&
            de_character_starts_factor(next) &&
            string_append_char(out, '*') != 0)
            goto fail;
    }
    result = strdup(string_c_str(out));
    string_free(out);
    return result;

fail:
    string_free(out);
    return NULL;
}

static bool de_previous_character_ends_factor(const char *text,
                                              size_t start,
                                              size_t lower_bound)
{
    size_t cursor = start;

    while (cursor > lower_bound &&
           isspace((unsigned char)text[cursor - 1u]))
        cursor--;
    if (cursor == lower_bound)
        return false;
    switch (text[cursor - 1u]) {
        case '+':
        case '-':
        case '*':
        case '/':
        case '^':
        case '=':
        case '(':
        case ',':
            return false;

        default:
            return true;
    }
}

static char *de_normalize_differential_form(const char *source,
                                            bool *converted_out)
{
    de_differential_marker_t *markers = NULL;
    size_t marker_count = 0u;
    size_t marker_capacity = 0u;
    char *variables[2] = { NULL, NULL };
    size_t variable_count = 0u;
    size_t equation_start;
    size_t equation_end;
    size_t independent = 0u;
    size_t dependent;
    string_t *out = NULL;
    char *converted = NULL;
    char *named = NULL;
    char *result = NULL;

    if (!source)
        return NULL;
    if (converted_out)
        *converted_out = false;
    de_equation_span(source, &equation_start, &equation_end);
    for (size_t i = equation_start; i < equation_end; ++i) {
        size_t end;
        char *name = NULL;
        size_t variable_index = SIZE_MAX;
        size_t before = i;

        while (before > equation_start &&
               isspace((unsigned char)source[before - 1u]))
            before--;
        if (before > equation_start && source[before - 1u] == '/')
            continue;

        if (!de_differential_name(
                source, i, equation_end, &end, &name))
            continue;
        for (size_t j = 0u; j < variable_count; ++j) {
            if (strcmp(name, variables[j]) == 0) {
                variable_index = j;
                break;
            }
        }
        if (variable_index == SIZE_MAX) {
            if (variable_count == 2u) {
                free(name);
                goto unchanged;
            }
            variables[variable_count++] = strdup(name);
            if (!variables[variable_count - 1u]) {
                free(name);
                goto cleanup;
            }
        }
        if (marker_count == marker_capacity) {
            size_t capacity = marker_capacity ? marker_capacity * 2u : 4u;
            de_differential_marker_t *items = realloc(
                markers, capacity * sizeof(*markers));

            if (!items) {
                free(name);
                goto cleanup;
            }
            markers = items;
            marker_capacity = capacity;
        }
        markers[marker_count].start = i;
        markers[marker_count].end = end;
        markers[marker_count].name = name;
        marker_count++;
        i = end - 1u;
    }
    if (variable_count != 2u)
        goto unchanged;

    if (de_independent_variable_score(variables[1]) >
        de_independent_variable_score(variables[0]))
        independent = 1u;
    dependent = 1u - independent;
    out = string_new();
    if (!out)
        goto cleanup;
    {
        size_t copied = 0u;

        for (size_t i = 0u; i < marker_count; ++i) {
            bool is_independent =
                strcmp(markers[i].name, variables[independent]) == 0;
            bool needs_multiply = de_previous_character_ends_factor(
                source, markers[i].start, equation_start);

            if (string_append_chars(
                    out, source + copied, markers[i].start - copied) != 0)
                goto cleanup;
            if (is_independent) {
                if (!needs_multiply && string_append_char(out, '1') != 0)
                    goto cleanup;
            } else {
                if ((needs_multiply && string_append_char(out, '*') != 0) ||
                    string_append_format(
                        out,
                        "D%s(%s)",
                        variables[independent],
                        variables[dependent]) < 0)
                    goto cleanup;
            }
            copied = markers[i].end;
        }
        if (string_append_cstr(out, source + copied) != 0)
            goto cleanup;
    }
    converted = strdup(string_c_str(out));
    if (converted)
        named = de_normalize_differential_variable_names(
            converted, variables, variable_count);
    if (named)
        result = de_normalize_spaced_products(named);
    if (result && converted_out)
        *converted_out = true;
    goto cleanup;

unchanged:
    result = strdup(source);

cleanup:
    free(named);
    free(converted);
    string_free(out);
    for (size_t i = 0u; i < marker_count; ++i)
        free(markers[i].name);
    free(markers);
    free(variables[1]);
    free(variables[0]);
    return result;
}

static bool de_find_top_level_char(const char *text,
                                   char target,
                                   size_t *position_out)
{
    int parens = 0;
    int brackets = 0;
    int braces = 0;

    if (!text)
        return false;

    for (size_t i = 0u; text[i]; ++i) {
        char ch = text[i];

        if (ch == '(')
            parens++;
        else if (ch == ')' && parens > 0)
            parens--;
        else if (ch == '[')
            brackets++;
        else if (ch == ']' && brackets > 0)
            brackets--;
        else if (ch == '{')
            braces++;
        else if (ch == '}' && braces > 0)
            braces--;
        else if (ch == target &&
                 parens == 0 && brackets == 0 && braces == 0) {
            if (position_out)
                *position_out = i;
            return true;
        }
    }
    return false;
}

static bool de_split_three_sections(const char *text,
                                    char **first,
                                    char **second,
                                    char **third)
{
    size_t semicolons[2];
    size_t count = 0u;
    int parens = 0;
    int brackets = 0;
    int braces = 0;
    size_t length = strlen(text);

    for (size_t i = 0u; i < length; ++i) {
        char ch = text[i];

        if (ch == '(')
            parens++;
        else if (ch == ')' && parens > 0)
            parens--;
        else if (ch == '[')
            brackets++;
        else if (ch == ']' && brackets > 0)
            brackets--;
        else if (ch == '{')
            braces++;
        else if (ch == '}' && braces > 0)
            braces--;
        else if (ch == ';' &&
                 parens == 0 && brackets == 0 && braces == 0) {
            if (count >= 2u)
                return false;
            semicolons[count++] = i;
        }
    }

    if (count != 2u)
        return false;

    *first = de_trimmed_copy(text, semicolons[0]);
    *second = de_trimmed_copy(
        text + semicolons[0] + 1u,
        semicolons[1] - semicolons[0] - 1u);
    *third = de_trimmed_copy(
        text + semicolons[1] + 1u,
        length - semicolons[1] - 1u);
    return *first && *second && *third;
}

static void de_parse_parts_dispose(de_parse_parts_t *parts)
{
    if (!parts)
        return;
    free(parts->conditions);
    free(parts->constants);
    free(parts->independent);
    free(parts->equation);
    memset(parts, 0, sizeof(*parts));
}

static bool de_parse_parts(const char *source, de_parse_parts_t *parts)
{
    char *trimmed;
    char *inner = NULL;
    char *sections = NULL;
    size_t pipe;
    size_t length;

    memset(parts, 0, sizeof(*parts));
    if (!source)
        return false;

    trimmed = de_trimmed_copy(source, strlen(source));
    if (!trimmed)
        return false;
    length = strlen(trimmed);
    if (length < 2u || trimmed[0] != '{' || trimmed[length - 1u] != '}') {
        fprintf(stderr, "de_from_string: expected '{ ... }'\n");
        free(trimmed);
        return false;
    }

    inner = de_trimmed_copy(trimmed + 1u, length - 2u);
    free(trimmed);
    if (!inner)
        return false;
    if (!de_find_top_level_char(inner, '|', &pipe)) {
        fprintf(stderr, "de_from_string: expected '|'\n");
        free(inner);
        return false;
    }

    parts->equation = de_trimmed_copy(inner, pipe);
    sections = de_trimmed_copy(
        inner + pipe + 1u, strlen(inner) - pipe - 1u);
    free(inner);
    if (!parts->equation || !sections ||
        !de_split_three_sections(
            sections,
            &parts->independent,
            &parts->constants,
            &parts->conditions)) {
        fprintf(stderr,
                "de_from_string: expected independent; constants; conditions\n");
        free(sections);
        de_parse_parts_dispose(parts);
        return false;
    }
    free(sections);
    return true;
}

static bool de_derivative_prefix(const char *text,
                                 size_t position,
                                 size_t *open_out)
{
    size_t p = position;

    if (text[p] != 'D')
        return false;
    p++;
    if (text[p] == '[') {
        p++;
        while (text[p] && text[p] != ']')
            p++;
        if (text[p] != ']')
            return false;
        p++;
    } else {
        size_t letters = 0u;

        while (text[p] && text[p] != '(') {
            size_t length;

            if (isalpha((unsigned char)text[p]))
                length = 1u;
            else if ((unsigned char)text[p] >= 0x80u)
                length = de_utf8_character_length(text + p);
            else
                break;
            if (length == 0u)
                break;
            p += length;
            letters++;
        }
        if (letters == 0u)
            return false;
    }

    if (text[p] != '(')
        return false;
    *open_out = p;
    return true;
}

static size_t de_matching_paren(const char *text, size_t open);

static bool de_name_list_contains(const string_t *names,
                                  const char *name,
                                  size_t length)
{
    const char *text = string_c_str(names);
    size_t start = 0u;
    size_t text_length = strlen(text);

    for (size_t i = 0u; i <= text_length; ++i) {
        if (i < text_length && text[i] != ',')
            continue;
        if (i - start == length &&
            strncmp(text + start, name, length) == 0)
            return true;
        start = i + 1u;
    }
    return false;
}

static int de_append_independent_name(string_t *names,
                                      const char *name,
                                      size_t length)
{
    while (length > 0u && isspace((unsigned char)*name)) {
        name++;
        length--;
    }
    while (length > 0u && isspace((unsigned char)name[length - 1u]))
        length--;
    if (length == 0u || de_name_list_contains(names, name, length))
        return 0;
    if (string_length(names) > 0u && string_append_char(names, ',') != 0)
        return -1;
    return string_append_chars(names, name, length);
}

static string_t *de_infer_independent_names(const char *equation)
{
    string_t *names = string_new();

    if (!names)
        return NULL;

    for (size_t i = 0u; equation[i]; ++i) {
        size_t open;

        if (!de_derivative_prefix(equation, i, &open))
            continue;

        if (equation[i + 1u] == '[') {
            size_t start = i + 2u;
            size_t close = start;

            while (equation[close] && equation[close] != ']')
                close++;
            for (size_t p = start; p <= close; ++p) {
                if (p < close && equation[p] != ',')
                    continue;
                if (de_append_independent_name(
                        names, equation + start, p - start) != 0)
                    goto fail;
                start = p + 1u;
            }
        } else {
            for (size_t p = i + 1u; p < open;) {
                size_t length = de_utf8_character_length(equation + p);

                if (length == 0u || p + length > open)
                    goto fail;
                if (de_append_independent_name(
                        names, equation + p, length) != 0)
                    goto fail;
                p += length;
            }
        }
        i = open;
    }

    if (string_length(names) == 0u)
        goto fail;
    return names;

fail:
    string_free(names);
    return NULL;
}

static char *de_first_independent_name(const string_t *names)
{
    const char *text = string_c_str(names);
    const char *comma = strchr(text, ',');
    size_t length = comma ? (size_t)(comma - text) : strlen(text);

    return de_trimmed_copy(text, length);
}

static char *de_first_declared_independent(const char *declarations)
{
    const char *equals;
    const char *comma;
    size_t length;

    if (!declarations)
        return NULL;
    equals = strchr(declarations, '=');
    comma = strchr(declarations, ',');
    if (!equals || (comma && comma < equals))
        return NULL;
    length = (size_t)(equals - declarations);
    return de_trimmed_copy(declarations, length);
}

static bool de_declarations_contain_name(
    const char *declarations,
    const char *name)
{
    size_t start = 0u;
    size_t length;
    string_t *target;
    string_t *normalised_target;
    bool found = false;

    if (!declarations || !name)
        return false;
    target = string_new_with(name);
    normalised_target =
        target ? expr_normalise_name_text(target) : NULL;
    string_free(target);
    if (!normalised_target)
        return false;
    length = strlen(declarations);
    for (size_t i = 0u; i <= length; ++i) {
        char *entry;
        size_t equals;
        char *declared_name;
        string_t *declared_text;
        string_t *normalised_declared;

        if (i < length && declarations[i] != ',')
            continue;
        entry = de_trimmed_copy(declarations + start, i - start);
        if (!entry ||
            !de_find_top_level_char(entry, '=', &equals)) {
            free(entry);
            break;
        }
        declared_name = de_trimmed_copy(entry, equals);
        declared_text = declared_name
            ? string_new_with(declared_name)
            : NULL;
        normalised_declared = declared_text
            ? expr_normalise_name_text(declared_text)
            : NULL;
        found = normalised_declared &&
            string_compare(
                normalised_declared, normalised_target) == 0;
        string_free(normalised_declared);
        string_free(declared_text);
        free(declared_name);
        free(entry);
        if (found)
            break;
        start = i + 1u;
    }
    string_free(normalised_target);
    return found;
}

static char *de_probe_independent_declarations(
    const char *equation,
    const char *independent)
{
    string_t *out = string_new_with(independent);

    if (!out)
        return NULL;

    for (size_t i = 0u; equation && equation[i]; ++i) {
        size_t open;
        size_t close;
        char *argument;
        string_t *argument_text;
        number_t value;

        if (!de_derivative_prefix(equation, i, &open))
            continue;
        close = de_matching_paren(equation, open);
        if (close == SIZE_MAX)
            goto fail;
        argument =
            de_trimmed_copy(equation + open + 1u, close - open - 1u);
        if (!argument)
            goto fail;
        argument_text = string_new_with(argument);
        if (!argument_text) {
            free(argument);
            goto fail;
        }
        if (expr_get_default_constant_num_text(argument_text, &value)) {
            if (!de_declarations_contain_name(
                    string_c_str(out), argument)) {
                if (string_length(out) > 0u &&
                    string_append_cstr(out, ", ") != 0) {
                    num_destroy(&value);
                    string_free(argument_text);
                    free(argument);
                    goto fail;
                }
                if (string_append_format(
                        out, "%s = ?", argument) < 0) {
                    num_destroy(&value);
                    string_free(argument_text);
                    free(argument);
                    goto fail;
                }
            }
            num_destroy(&value);
        }
        string_free(argument_text);
        free(argument);
        i = close;
    }

    {
        char *copy = strdup(string_c_str(out));

        string_free(out);
        return copy;
    }

fail:
    string_free(out);
    return NULL;
}

static void de_name_contextual_dependents(
    const char *equation,
    expr_bindings_t *bindings)
{
    for (size_t i = 0u; equation && equation[i]; ++i) {
        size_t open;
        size_t close;
        char *argument;
        string_t *argument_text;
        number_t value;

        if (!de_derivative_prefix(equation, i, &open))
            continue;
        close = de_matching_paren(equation, open);
        if (close == SIZE_MAX)
            return;
        argument =
            de_trimmed_copy(equation + open + 1u, close - open - 1u);
        argument_text = argument ? string_new_with(argument) : NULL;
        if (argument_text &&
            expr_get_default_constant_num_text(argument_text, &value)) {
            expr_t *dependent =
                expr_bindings_get(bindings, argument);

            if (dependent)
                expr_set_name(dependent, argument);
            num_destroy(&value);
        }
        string_free(argument_text);
        free(argument);
        i = close;
    }
}

static char *de_contextual_dependent_name(const char *equation)
{
    char *found = NULL;

    for (size_t i = 0u; equation && equation[i]; ++i) {
        size_t open;
        size_t close;
        char *argument;
        string_t *argument_text;
        number_t value;

        if (!de_derivative_prefix(equation, i, &open))
            continue;
        close = de_matching_paren(equation, open);
        if (close == SIZE_MAX)
            goto fail;
        argument =
            de_trimmed_copy(equation + open + 1u, close - open - 1u);
        argument_text = argument ? string_new_with(argument) : NULL;
        if (!argument_text) {
            free(argument);
            goto fail;
        }
        if (expr_get_default_constant_num_text(argument_text, &value)) {
            num_destroy(&value);
            if (found && strcmp(found, argument) != 0) {
                string_free(argument_text);
                free(argument);
                goto fail;
            }
            if (!found)
                found = strdup(argument);
        }
        string_free(argument_text);
        free(argument);
        i = close;
    }
    return found;

fail:
    free(found);
    return NULL;
}

static void de_name_formal_dependent(
    const expr_t *expr,
    const char *name)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;

    if (!expr || !name)
        return;
    if (expr_is_formal_derivative(expr)) {
        const expr_t *dependent =
            expr_formal_derivative_dependent(expr);

        if (dependent)
            expr_set_name((expr_t *)dependent, name);
    }
    if (expr_child_exprs(expr, &left, &right)) {
        de_name_formal_dependent(left, name);
        de_name_formal_dependent(right, name);
    }
}

static const char *de_prime_shorthand_independent(const char *equation)
{
    size_t i = 0u;

    if (!equation)
        return "x";

    while (equation[i]) {
        size_t name_start;
        size_t name_end;

        if (!(isalpha((unsigned char)equation[i]) ||
              equation[i] == '_')) {
            i++;
            continue;
        }
        name_start = i++;
        while (isalnum((unsigned char)equation[i]) ||
               equation[i] == '_')
            i++;
        name_end = i;
        if (equation[i] != '\'')
            continue;
        if (name_end - name_start == 1u &&
            equation[name_start] == 'x')
            return "t";
        return "x";
    }
    return "x";
}

static char *de_prime_shorthand_dependent(const char *text)
{
    const char *best = NULL;
    size_t best_length = SIZE_MAX;
    size_t i = 0u;

    if (!text)
        return NULL;

    while (text[i]) {
        size_t start;
        size_t length;

        if (!(isalpha((unsigned char)text[i]) || text[i] == '_')) {
            i++;
            continue;
        }
        start = i++;
        while (isalnum((unsigned char)text[i]) || text[i] == '_')
            i++;
        length = i - start;
        if (text[i] == '\'' && length < best_length) {
            best = text + start;
            best_length = length;
        }
        while (text[i] == '\'')
            i++;
    }

    return best ? de_trimmed_copy(best, best_length) : NULL;
}

static char *de_normalize_prime_derivatives(const char *text,
                                            const char *wrt)
{
    string_t *out;
    char *dependent;
    char *result;
    size_t i = 0u;

    if (!text || !wrt || !*wrt)
        return NULL;

    out = string_new();
    if (!out)
        return NULL;
    dependent = de_prime_shorthand_dependent(text);

    while (text[i]) {
        size_t name_start;
        size_t name_end;
        size_t order_end;

        if (!(isalpha((unsigned char)text[i]) || text[i] == '_')) {
            if ((unsigned char)text[i] >= 0x80u) {
                if (de_append_next_utf8_character(out, text, &i) != 0)
                    goto fail;
            } else if (string_append_char(out, text[i++]) != 0) {
                goto fail;
            }
            continue;
        }

        name_start = i++;
        while (isalnum((unsigned char)text[i]) || text[i] == '_')
            i++;
        name_end = i;
        while (text[i] == '\'')
            i++;
        order_end = i;

        if (order_end == name_end) {
            if (string_append_chars(
                    out, text + name_start, name_end - name_start) != 0)
                goto fail;
            continue;
        }

        if (dependent) {
            size_t dependent_length = strlen(dependent);
            size_t name_length = name_end - name_start;

            if (name_length > dependent_length &&
                strncmp(
                    text + name_end - dependent_length,
                    dependent,
                    dependent_length) == 0) {
                size_t coefficient_length = name_length - dependent_length;

                if (string_append_chars(
                        out, text + name_start, coefficient_length) != 0 ||
                    string_append_char(out, '*') != 0)
                    goto fail;
                name_start += coefficient_length;
            }
        }

        if (strlen(wrt) == 1u && isalpha((unsigned char)wrt[0])) {
            if (string_append_char(out, 'D') != 0)
                goto fail;
            for (size_t order = name_end; order < order_end; ++order) {
                if (string_append_char(out, wrt[0]) != 0)
                    goto fail;
            }
        } else {
            if (string_append_cstr(out, "D[") != 0)
                goto fail;
            for (size_t order = name_end; order < order_end; ++order) {
                if ((order > name_end &&
                     string_append_char(out, ',') != 0) ||
                    string_append_cstr(out, wrt) != 0)
                    goto fail;
            }
            if (string_append_char(out, ']') != 0)
                goto fail;
        }

        if (string_append_char(out, '(') != 0 ||
            string_append_chars(
                out, text + name_start, name_end - name_start) != 0 ||
            string_append_char(out, ')') != 0)
            goto fail;
    }

    result = strdup(string_c_str(out));
    string_free(out);
    free(dependent);
    return result;

fail:
    string_free(out);
    free(dependent);
    return NULL;
}

static bool de_subscript_derivative_suffix(
    const char *suffix,
    size_t length,
    const char *independent_declarations)
{
    if (!suffix || length == 0u)
        return false;
    for (size_t i = 0u; i < length; ++i) {
        char coordinate[2];

        if (!islower((unsigned char)suffix[i]))
            return false;
        if (!independent_declarations)
            continue;
        coordinate[0] = suffix[i];
        coordinate[1] = '\0';
        if (!de_declarations_contain_name(
                independent_declarations, coordinate))
            return false;
    }
    return true;
}

static bool de_subscript_coefficient_prefix(
    const char *base,
    size_t length,
    const char *independent_declarations)
{
    char coordinate[2];

    if (!base || length < 2u ||
        !islower((unsigned char)base[0]) ||
        !islower((unsigned char)base[length - 1u]))
        return false;
    if (!independent_declarations)
        return base[0] == 'x' ||
            base[0] == 'y' ||
            base[0] == 'z' ||
            base[0] == 't';
    coordinate[0] = base[0];
    coordinate[1] = '\0';
    return de_declarations_contain_name(
        independent_declarations, coordinate);
}

static int de_append_subscript_dependent(
    string_t *out,
    const char *name,
    size_t length,
    bool alias)
{
    string_t *raw;
    string_t *normalised;
    const char *canonical;
    int result;

    if (alias)
        return string_append_char(out, '@') == 0
            ? string_append_chars(out, name, length)
            : -1;
    raw = string_new();
    if (!raw || string_append_chars(raw, name, length) != 0) {
        string_free(raw);
        return -1;
    }
    normalised = expr_default_constant_canonical_name_text(raw);
    canonical = normalised ? string_c_str(normalised) : NULL;
    result = canonical && canonical[0] == '@'
        ? string_append_cstr(out, canonical)
        : string_append_chars(out, name, length);
    string_free(normalised);
    string_free(raw);
    return result;
}

static bool de_utf8_superscript_digit(const char *text,
                                      size_t *length_out,
                                      unsigned int *digit_out)
{
    static const struct {
        const char *text;
        unsigned int digit;
    } digits[] = {
        {"⁰", 0u}, {"¹", 1u}, {"²", 2u}, {"³", 3u},
        {"⁴", 4u}, {"⁵", 5u}, {"⁶", 6u}, {"⁷", 7u},
        {"⁸", 8u}, {"⁹", 9u},
    };

    if (!text)
        return false;
    for (size_t i = 0u; i < sizeof(digits) / sizeof(digits[0]); ++i) {
        size_t length = strlen(digits[i].text);

        if (strncmp(text, digits[i].text, length) != 0)
            continue;
        if (length_out)
            *length_out = length;
        if (digit_out)
            *digit_out = digits[i].digit;
        return true;
    }
    return false;
}

static bool de_parse_utf8_superscript(const char *text,
                                      size_t *position,
                                      size_t *value_out)
{
    size_t cursor;
    size_t value = 0u;
    bool found = false;

    if (!text || !position)
        return false;
    cursor = *position;
    while (text[cursor]) {
        size_t length = 0u;
        unsigned int digit = 0u;

        if (!de_utf8_superscript_digit(
                text + cursor, &length, &digit))
            break;
        if (value > (SIZE_MAX - digit) / 10u)
            return false;
        value = value * 10u + digit;
        cursor += length;
        found = true;
    }
    if (!found)
        return false;
    *position = cursor;
    if (value_out)
        *value_out = value;
    return true;
}

static void de_skip_ascii_space(const char *text, size_t *position)
{
    while (text[*position] &&
           isspace((unsigned char)text[*position]))
        (*position)++;
}

static int de_append_next_utf8_character(string_t *out,
                                         const char *text,
                                         size_t *position)
{
    unsigned char lead;
    size_t length = 1u;

    if (!out || !text || !position || !text[*position])
        return -1;
    lead = (unsigned char)text[*position];
    if ((lead & 0xe0u) == 0xc0u)
        length = 2u;
    else if ((lead & 0xf0u) == 0xe0u)
        length = 3u;
    else if ((lead & 0xf8u) == 0xf0u)
        length = 4u;
    for (size_t i = 1u; i < length; ++i) {
        if (!text[*position + i] ||
            ((unsigned char)text[*position + i] & 0xc0u) != 0x80u) {
            length = 1u;
            break;
        }
    }
    if (string_append_chars(out, text + *position, length) != 0)
        return -1;
    *position += length;
    return 0;
}

static bool de_try_unicode_partial_derivative(const char *text,
                                              size_t start,
                                              size_t *end_out,
                                              string_t *out)
{
    static const char partial[] = "∂";
    char suffix[128];
    size_t cursor = start;
    size_t numerator_order = 0u;
    size_t dependent_start;
    size_t dependent_end;
    size_t suffix_length = 0u;
    size_t parsed_order;

    if (!text || !out ||
        strncmp(text + cursor, partial, sizeof(partial) - 1u) != 0)
        return false;
    cursor += sizeof(partial) - 1u;
    (void)de_parse_utf8_superscript(
        text, &cursor, &numerator_order);
    de_skip_ascii_space(text, &cursor);

    dependent_start = cursor;
    if (text[cursor] == '@')
        cursor++;
    if (!(isalpha((unsigned char)text[cursor]) ||
          text[cursor] == '_'))
        return false;
    cursor++;
    while (isalnum((unsigned char)text[cursor]) ||
           text[cursor] == '_')
        cursor++;
    dependent_end = cursor;
    de_skip_ascii_space(text, &cursor);
    if (text[cursor] != '/')
        return false;
    cursor++;

    while (true) {
        size_t multiplicity = 1u;
        size_t lookahead;
        char variable;

        de_skip_ascii_space(text, &cursor);
        if (strncmp(
                text + cursor, partial, sizeof(partial) - 1u) != 0)
            return false;
        cursor += sizeof(partial) - 1u;
        de_skip_ascii_space(text, &cursor);
        if (!islower((unsigned char)text[cursor]))
            return false;
        variable = text[cursor++];
        if (de_parse_utf8_superscript(
                text, &cursor, &parsed_order)) {
            if (parsed_order == 0u)
                return false;
            multiplicity = parsed_order;
        }
        if (multiplicity > sizeof(suffix) - suffix_length)
            return false;
        for (size_t i = 0u; i < multiplicity; ++i)
            suffix[suffix_length++] = variable;

        lookahead = cursor;
        de_skip_ascii_space(text, &lookahead);
        if (strncmp(
                text + lookahead,
                partial,
                sizeof(partial) - 1u) != 0)
            break;
        cursor = lookahead;
    }

    if (suffix_length == 0u ||
        (numerator_order != 0u &&
         numerator_order != suffix_length))
        return false;
    if (string_append_char(out, 'D') != 0)
        return false;
    for (size_t i = suffix_length; i > 0u; --i) {
        if (string_append_char(out, suffix[i - 1u]) != 0)
            return false;
    }
    if (string_append_char(out, '(') != 0 ||
        string_append_chars(
            out,
            text + dependent_start,
            dependent_end - dependent_start) != 0 ||
        string_append_char(out, ')') != 0)
        return false;
    if (end_out)
        *end_out = cursor;
    return true;
}

static char *de_normalize_unicode_partial_derivatives(const char *text)
{
    static const char partial[] = "∂";
    string_t *out;
    size_t cursor = 0u;
    char *result;

    if (!text)
        return NULL;
    out = string_new();
    if (!out)
        return NULL;

    while (text[cursor]) {
        size_t end = cursor;

        if (strncmp(
                text + cursor, partial, sizeof(partial) - 1u) == 0 &&
            de_try_unicode_partial_derivative(
                text, cursor, &end, out)) {
            cursor = end;
            continue;
        }
        if (de_append_next_utf8_character(
                out, text, &cursor) != 0)
            goto fail;
    }

    result = strdup(string_c_str(out));
    string_free(out);
    return result;

fail:
    string_free(out);
    return NULL;
}

static bool de_try_unicode_total_derivative(const char *text,
                                            size_t start,
                                            size_t *end_out,
                                            string_t *out)
{
    size_t cursor = start;
    size_t numerator_order = 1u;
    size_t denominator_order = 1u;
    size_t dependent_start;
    size_t dependent_end;
    size_t variable_start;
    size_t variable_length;

    if (!text || !out || text[cursor] != 'd')
        return false;
    cursor++;
    (void)de_parse_utf8_superscript(
        text, &cursor, &numerator_order);
    if (numerator_order == 0u)
        return false;
    de_skip_ascii_space(text, &cursor);

    dependent_start = cursor;
    if (text[cursor] == '@') {
        cursor++;
        if (!(isalpha((unsigned char)text[cursor]) ||
              text[cursor] == '_'))
            return false;
        cursor++;
        while (isalnum((unsigned char)text[cursor]) ||
               text[cursor] == '_')
            cursor++;
    } else if (isalpha((unsigned char)text[cursor]) ||
               text[cursor] == '_') {
        cursor++;
        while (isalnum((unsigned char)text[cursor]) ||
               text[cursor] == '_')
            cursor++;
    } else {
        size_t length = de_utf8_character_length(text + cursor);

        if (length == 0u)
            return false;
        cursor += length;
    }
    dependent_end = cursor;
    de_skip_ascii_space(text, &cursor);
    if (text[cursor] != '/')
        return false;
    cursor++;
    de_skip_ascii_space(text, &cursor);
    if (text[cursor++] != 'd')
        return false;
    de_skip_ascii_space(text, &cursor);
    variable_start = cursor;
    if (islower((unsigned char)text[cursor]))
        variable_length = 1u;
    else
        variable_length = de_utf8_character_length(text + cursor);
    if (variable_length == 0u)
        return false;
    cursor += variable_length;
    (void)de_parse_utf8_superscript(
        text, &cursor, &denominator_order);
    if (denominator_order == 0u ||
        numerator_order != denominator_order)
        return false;

    if (string_append_char(out, 'D') != 0)
        return false;
    for (size_t i = 0u; i < numerator_order; ++i) {
        if (string_append_chars(
                out, text + variable_start, variable_length) != 0)
            return false;
    }
    if (string_append_char(out, '(') != 0 ||
        string_append_chars(
            out,
            text + dependent_start,
            dependent_end - dependent_start) != 0 ||
        string_append_char(out, ')') != 0)
        return false;
    if (end_out)
        *end_out = cursor;
    return true;
}

static char *de_normalize_unicode_total_derivatives(const char *text)
{
    string_t *out;
    size_t cursor = 0u;
    char *result;

    if (!text)
        return NULL;
    out = string_new();
    if (!out)
        return NULL;

    while (text[cursor]) {
        size_t end = cursor;

        if (text[cursor] == 'd' &&
            de_try_unicode_total_derivative(
                text, cursor, &end, out)) {
            cursor = end;
            continue;
        }
        if (de_append_next_utf8_character(
                out, text, &cursor) != 0)
            goto fail;
    }

    result = strdup(string_c_str(out));
    string_free(out);
    return result;

fail:
    string_free(out);
    return NULL;
}

static char *de_normalize_subscript_derivatives(
    const char *text,
    const char *independent_declarations)
{
    string_t *out;
    size_t i = 0u;
    char *result;

    if (!text)
        return NULL;
    out = string_new();
    if (!out)
        return NULL;

    while (text[i]) {
        size_t token_start = i;
        size_t name_start = i;
        size_t name_end;
        size_t underscore = SIZE_MAX;
        size_t dependent_start;
        bool alias = false;

        if (text[i] == '[') {
            while (text[i]) {
                char ch = text[i++];

                if (string_append_char(out, ch) != 0)
                    goto fail;
                if (ch == ']')
                    break;
            }
            continue;
        }

        if (text[i] == '@' &&
            (isalpha((unsigned char)text[i + 1u]) ||
             text[i + 1u] == '_')) {
            alias = true;
            name_start = ++i;
        } else if (!(isalpha((unsigned char)text[i]) ||
                     text[i] == '_')) {
            if ((unsigned char)text[i] >= 0x80u) {
                if (de_append_next_utf8_character(out, text, &i) != 0)
                    goto fail;
            } else if (string_append_char(out, text[i++]) != 0) {
                goto fail;
            }
            continue;
        }
        i++;
        while (isalnum((unsigned char)text[i]) || text[i] == '_')
            i++;
        name_end = i;
        for (size_t p = name_start; p < name_end; ++p) {
            if (text[p] != '_')
                continue;
            if (underscore != SIZE_MAX) {
                underscore = SIZE_MAX;
                break;
            }
            underscore = p;
        }

        if (underscore == SIZE_MAX ||
            underscore == name_start ||
            !de_subscript_derivative_suffix(
                text + underscore + 1u,
                name_end - underscore - 1u,
                independent_declarations)) {
            if (string_append_chars(
                    out, text + token_start, name_end - token_start) != 0)
                goto fail;
            continue;
        }

        dependent_start = name_start;
        if (!alias && de_subscript_coefficient_prefix(
                text + name_start,
                underscore - name_start,
                independent_declarations)) {
            dependent_start = underscore - 1u;
            if (string_append_chars(
                    out,
                    text + name_start,
                    dependent_start - name_start) != 0)
                goto fail;
        }
        if (string_append_char(out, 'D') != 0 ||
            string_append_chars(
                out,
                text + underscore + 1u,
                name_end - underscore - 1u) != 0 ||
            string_append_char(out, '(') != 0 ||
            de_append_subscript_dependent(
                out,
                text + dependent_start,
                underscore - dependent_start,
                alias) != 0 ||
            string_append_char(out, ')') != 0)
            goto fail;
    }

    result = strdup(string_c_str(out));
    string_free(out);
    return result;

fail:
    string_free(out);
    return NULL;
}

static char *de_normalize_prime_condition(const char *condition,
                                          const char *wrt)
{
    char *trimmed = de_trimmed_copy(condition, strlen(condition));
    const char *cursor;
    const char *dependent_start;
    size_t dependent_length;
    size_t order = 0u;
    size_t point_open;
    size_t point_close;
    string_t *out;
    char *result;

    if (!trimmed)
        return NULL;

    cursor = trimmed;
    if (!(isalpha((unsigned char)*cursor) || *cursor == '_'))
        return trimmed;
    dependent_start = cursor++;
    while (isalnum((unsigned char)*cursor) || *cursor == '_')
        cursor++;
    dependent_length = (size_t)(cursor - dependent_start);
    while (*cursor == '\'') {
        order++;
        cursor++;
    }
    if (order == 0u || *cursor != '(')
        return trimmed;

    point_open = (size_t)(cursor - trimmed);
    point_close = de_matching_paren(trimmed, point_open);
    if (point_close == SIZE_MAX)
        return trimmed;
    cursor = trimmed + point_close + 1u;
    while (isspace((unsigned char)*cursor))
        cursor++;
    if (*cursor != '=')
        return trimmed;
    cursor++;
    while (isspace((unsigned char)*cursor))
        cursor++;

    out = string_new();
    if (!out)
        goto fail;

    if (strlen(wrt) == 1u && isalpha((unsigned char)wrt[0])) {
        if (string_append_char(out, 'D') != 0)
            goto output_fail;
        for (size_t i = 0u; i < order; ++i) {
            if (string_append_char(out, wrt[0]) != 0)
                goto output_fail;
        }
    } else {
        if (string_append_cstr(out, "D[") != 0)
            goto output_fail;
        for (size_t i = 0u; i < order; ++i) {
            if ((i > 0u && string_append_char(out, ',') != 0) ||
                string_append_cstr(out, wrt) != 0)
                goto output_fail;
        }
        if (string_append_char(out, ']') != 0)
            goto output_fail;
    }

    if (string_append_char(out, '(') != 0 ||
        string_append_chars(out, dependent_start, dependent_length) != 0 ||
        string_append_char(out, ')') != 0 ||
        string_append_chars(
            out,
            trimmed + point_open,
            point_close - point_open + 1u) != 0 ||
        string_append_cstr(out, " = ") != 0 ||
        string_append_cstr(out, cursor) != 0)
        goto output_fail;

    result = strdup(string_c_str(out));
    string_free(out);
    free(trimmed);
    return result;

output_fail:
    string_free(out);
fail:
    free(trimmed);
    return NULL;
}

static char *de_expand_shorthand(const char *source)
{
    char *trimmed = NULL;
    char *equation = NULL;
    char *first_wrt = NULL;
    string_t *names = NULL;
    string_t *declarations = NULL;
    string_t *conditions = NULL;
    string_t *out = NULL;
    size_t first_separator = SIZE_MAX;
    size_t length;
    int parens = 0;
    int brackets = 0;
    char *result = NULL;

    if (!source)
        return NULL;
    trimmed = de_trimmed_copy(source, strlen(source));
    if (!trimmed)
        return NULL;
    length = strlen(trimmed);
    if (length > 0u && trimmed[0] == '{')
        return trimmed;

    for (size_t i = 0u; i < length; ++i) {
        if (trimmed[i] == '(')
            parens++;
        else if (trimmed[i] == ')' && parens > 0)
            parens--;
        else if (trimmed[i] == '[')
            brackets++;
        else if (trimmed[i] == ']' && brackets > 0)
            brackets--;
        else if (trimmed[i] == ';' && parens == 0 && brackets == 0) {
            first_separator = i;
            break;
        }
    }

    equation = de_trimmed_copy(
        trimmed,
        first_separator == SIZE_MAX ? length : first_separator);
    if (equation) {
        const char *wrt = de_prime_shorthand_independent(equation);
        char *subscript_normalized =
            de_normalize_subscript_derivatives(equation, NULL);
        char *normalized = subscript_normalized
            ? de_normalize_prime_derivatives(subscript_normalized, wrt)
            : NULL;

        free(subscript_normalized);
        free(equation);
        equation = normalized;
    }
    names = equation ? de_infer_independent_names(equation) : NULL;
    declarations = string_new();
    conditions = string_new();
    if (!equation || !*equation || !names || !declarations || !conditions)
        goto cleanup;

    {
        const char *name_text = string_c_str(names);
        size_t start = 0u;
        size_t name_length = strlen(name_text);

        for (size_t i = 0u; i <= name_length; ++i) {
            if (i < name_length && name_text[i] != ',')
                continue;
            if ((start > 0u &&
                 string_append_cstr(declarations, ", ") != 0) ||
                string_append_chars(
                    declarations, name_text + start, i - start) != 0 ||
                string_append_cstr(declarations, " = ?") != 0)
                goto cleanup;
            start = i + 1u;
        }
    }

    first_wrt = de_first_independent_name(names);
    if (!first_wrt)
        goto cleanup;

    if (first_separator != SIZE_MAX) {
        size_t start = first_separator + 1u;

        parens = 0;
        brackets = 0;
        for (size_t i = start; i <= length; ++i) {
            char ch = i < length ? trimmed[i] : ';';

            if (ch == '(')
                parens++;
            else if (ch == ')' && parens > 0)
                parens--;
            else if (ch == '[')
                brackets++;
            else if (ch == ']' && brackets > 0)
                brackets--;
            if (ch != ';' || parens != 0 || brackets != 0)
                continue;
            {
                char *condition =
                    de_trimmed_copy(trimmed + start, i - start);
                char *normalized = condition && *condition
                    ? de_normalize_prime_condition(condition, first_wrt)
                    : NULL;

                free(condition);
                if (!normalized)
                    goto cleanup;
                if ((string_length(conditions) > 0u &&
                     string_append_cstr(conditions, ", ") != 0) ||
                    string_append_cstr(conditions, normalized) != 0) {
                    free(normalized);
                    goto cleanup;
                }
                free(normalized);
            }
            start = i + 1u;
        }
    }

    out = string_sprintf(
        "{ %s | %S;; %S }", equation, declarations, conditions);
    if (out)
        result = strdup(string_c_str(out));

cleanup:
    string_free(out);
    string_free(conditions);
    string_free(declarations);
    string_free(names);
    free(first_wrt);
    free(equation);
    free(trimmed);
    return result;
}

static size_t de_matching_paren(const char *text, size_t open)
{
    int depth = 0;

    for (size_t i = open; text[i]; ++i) {
        if (text[i] == '(')
            depth++;
        else if (text[i] == ')' && --depth == 0)
            return i;
    }
    return SIZE_MAX;
}

static char *de_mask_formal_derivatives(const char *text)
{
    string_t *out = string_new();

    if (!out)
        return NULL;

    for (size_t i = 0u; text[i];) {
        size_t open;
        size_t close;

        if (!de_derivative_prefix(text, i, &open)) {
            if ((unsigned char)text[i] >= 0x80u) {
                if (de_append_next_utf8_character(out, text, &i) != 0)
                    goto fail;
            } else if (string_append_char(out, text[i++]) != 0) {
                goto fail;
            }
            continue;
        }

        close = de_matching_paren(text, open);
        if (close == SIZE_MAX)
            goto fail;
        {
            char *argument =
                de_trimmed_copy(text + open + 1u, close - open - 1u);
            char *masked = argument
                ? de_mask_formal_derivatives(argument)
                : NULL;

            free(argument);
            if (!masked || string_append_cstr(out, masked) != 0) {
                free(masked);
                goto fail;
            }
            free(masked);
        }
        i = close + 1u;
    }

    {
        char *copy = strdup(string_c_str(out));

        string_free(out);
        return copy;
    }

fail:
    string_free(out);
    return NULL;
}

static int de_build_symbol_arrays(expr_bindings_t *bindings,
                                  const string_t ***names_out,
                                  expr_t ***symbols_out,
                                  size_t *count_out)
{
    size_t count = expr_bindings_count(bindings);
    const string_t **names = calloc(count ? count : 1u, sizeof(*names));
    expr_t **symbols = calloc(count ? count : 1u, sizeof(*symbols));

    if (!names || !symbols) {
        free(symbols);
        free(names);
        return -1;
    }

    for (size_t i = 0u; i < count; ++i) {
        names[i] = expr_bindings_name_text_at(bindings, i);
        symbols[i] = expr_bindings_expr_at(bindings, i);
    }

    *names_out = names;
    *symbols_out = symbols;
    *count_out = count;
    return 0;
}

static expr_t *de_parse_expr(const char *text,
                             const string_t *const *names,
                             expr_t *const *symbols,
                             size_t symbol_count)
{
    string_t *source = string_new_with(text);
    expr_t *expr = source
        ? expr_from_expression_text_formal(
              source, names, symbols, symbol_count)
        : NULL;

    string_free(source);
    return expr;
}

static bool de_split_equation(const char *text, char **lhs, char **rhs)
{
    size_t equals;

    if (!de_find_top_level_char(text, '=', &equals))
        return false;

    *lhs = de_trimmed_copy(text, equals);
    *rhs = de_trimmed_copy(
        text + equals + 1u, strlen(text) - equals - 1u);
    return *lhs && *rhs && **lhs && **rhs;
}

static equation_t *de_parse_equation_with_symbols(
    const char *text,
    const string_t *const *names,
    expr_t *const *symbols,
    size_t symbol_count,
    expr_bindings_t *owned_bindings)
{
    char *lhs_text = NULL;
    char *rhs_text = NULL;
    expr_t *lhs = NULL;
    expr_t *rhs = NULL;
    equation_t *equation = NULL;

    if (!de_split_equation(text, &lhs_text, &rhs_text))
        goto cleanup;

    lhs = de_parse_expr(lhs_text, names, symbols, symbol_count);
    rhs = de_parse_expr(rhs_text, names, symbols, symbol_count);
    if (!lhs || !rhs)
        goto cleanup;

    equation = equ_new_with_owned_bindings(lhs, rhs, owned_bindings);
    owned_bindings = NULL;

cleanup:
    expr_bindings_free(owned_bindings);
    expr_free(rhs);
    expr_free(lhs);
    free(rhs_text);
    free(lhs_text);
    return equation;
}

static int de_append_independent(diffequ_t *de, expr_t *variable)
{
    expr_t **items = realloc(
        de->independent_vars,
        (de->independent_count + 1u) * sizeof(*items));

    if (!items)
        return -1;
    de->independent_vars = items;
    de->independent_vars[de->independent_count++] = variable;
    expr_retain(variable);
    return 0;
}

static bool de_parse_independents(diffequ_t *de,
                                  const char *text,
                                  expr_bindings_t *bindings)
{
    size_t start = 0u;
    size_t length = strlen(text);

    if (length == 0u)
        return true;

    for (size_t i = 0u; i <= length; ++i) {
        if (i < length && text[i] != ',')
            continue;
        {
            char *entry = de_trimmed_copy(text + start, i - start);
            size_t equals;
            char *name = NULL;
            char *value = NULL;
            expr_t *variable;

            if (!entry || !de_find_top_level_char(entry, '=', &equals))
                goto fail;
            name = de_trimmed_copy(entry, equals);
            value = de_trimmed_copy(
                entry + equals + 1u, strlen(entry) - equals - 1u);
            variable = name ? expr_bindings_get(bindings, name) : NULL;
            if (!name || !value || strcmp(value, "?") != 0 || !variable ||
                de_append_independent(de, variable) != 0) {
                free(value);
                free(name);
                free(entry);
                return false;
            }
            free(value);
            free(name);
            free(entry);
        }
        start = i + 1u;
    }
    return true;

fail:
    fprintf(stderr, "de_from_string: invalid independent declaration\n");
    return false;
}

static bool de_extract_condition_point(const char *lhs,
                                       char **core_out,
                                       char **point_out)
{
    size_t length = strlen(lhs);
    size_t close;
    size_t open;
    int depth = 0;

    *core_out = NULL;
    *point_out = NULL;
    while (length > 0u && isspace((unsigned char)lhs[length - 1u]))
        length--;
    if (length == 0u || lhs[length - 1u] != ')') {
        *core_out = de_trimmed_copy(lhs, length);
        return *core_out != NULL;
    }

    close = length - 1u;
    open = close;
    while (open > 0u) {
        char ch = lhs[open];

        if (ch == ')')
            depth++;
        else if (ch == '(' && --depth == 0)
            break;
        open--;
    }
    if (depth != 0 || open == 0u) {
        *core_out = de_trimmed_copy(lhs, length);
        return *core_out != NULL;
    }

    *core_out = de_trimmed_copy(lhs, open);
    *point_out = de_trimmed_copy(lhs + open + 1u, close - open - 1u);
    return *core_out && *point_out;
}

static int de_append_condition(diffequ_t *de,
                               equation_t *condition,
                               string_t *text,
                               expr_t **points,
                               size_t point_count)
{
    size_t count = de->condition_count + 1u;
    equation_t **conditions = malloc(count * sizeof(*conditions));
    string_t **texts = malloc(count * sizeof(*texts));
    expr_t ***condition_points =
        malloc(count * sizeof(*condition_points));
    size_t *point_counts = malloc(count * sizeof(*point_counts));

    if (!conditions || !texts || !condition_points || !point_counts) {
        free(conditions);
        free(texts);
        free(condition_points);
        free(point_counts);
        return -1;
    }

    if (de->condition_count > 0u) {
        memcpy(conditions,
               de->conditions,
               de->condition_count * sizeof(*conditions));
        memcpy(texts,
               de->condition_texts,
               de->condition_count * sizeof(*texts));
        memcpy(condition_points,
               de->condition_points,
               de->condition_count * sizeof(*condition_points));
        memcpy(point_counts,
               de->condition_point_counts,
               de->condition_count * sizeof(*point_counts));
    }
    free(de->conditions);
    free(de->condition_texts);
    free(de->condition_points);
    free(de->condition_point_counts);
    de->conditions = conditions;
    de->condition_texts = texts;
    de->condition_points = condition_points;
    de->condition_point_counts = point_counts;
    de->conditions[de->condition_count] = condition;
    de->condition_texts[de->condition_count] = text;
    de->condition_points[de->condition_count] = points;
    de->condition_point_counts[de->condition_count] = point_count;
    de->condition_count = count;
    return 0;
}

static void de_free_condition_points(expr_t **points, size_t count)
{
    for (size_t i = 0u; i < count; ++i)
        expr_free(points[i]);
    free(points);
}

static bool de_parse_condition_points(
    const char *text,
    const string_t *const *names,
    expr_t *const *symbols,
    size_t symbol_count,
    expr_t ***points_out,
    size_t *count_out)
{
    size_t start = 0u;
    size_t length;
    int parens = 0;
    int brackets = 0;
    expr_t **points = NULL;
    size_t count = 0u;

    *points_out = NULL;
    *count_out = 0u;
    if (!text)
        return true;

    length = strlen(text);
    if (length == 0u)
        return false;

    for (size_t i = 0u; i <= length; ++i) {
        char ch = i < length ? text[i] : ',';

        if (ch == '(')
            parens++;
        else if (ch == ')' && parens > 0)
            parens--;
        else if (ch == '[')
            brackets++;
        else if (ch == ']' && brackets > 0)
            brackets--;
        if (ch != ',' || parens != 0 || brackets != 0)
            continue;
        {
            char *argument =
                de_trimmed_copy(text + start, i - start);
            expr_t *point = argument && *argument
                ? de_parse_expr(
                      argument, names, symbols, symbol_count)
                : NULL;
            expr_t **new_points;

            free(argument);
            if (!point)
                goto fail;
            new_points = realloc(
                points, (count + 1u) * sizeof(*new_points));
            if (!new_points) {
                expr_free(point);
                goto fail;
            }
            points = new_points;
            points[count++] = point;
        }
        start = i + 1u;
    }

    *points_out = points;
    *count_out = count;
    return true;

fail:
    de_free_condition_points(points, count);
    return false;
}

static bool de_parse_one_condition(
    diffequ_t *de,
    const char *text,
    const string_t *const *names,
    expr_t *const *symbols,
    size_t symbol_count)
{
    char *lhs = NULL;
    char *rhs = NULL;
    char *core = NULL;
    char *point_text = NULL;
    string_t *canonical = NULL;
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t **points = NULL;
    size_t point_count = 0u;
    equation_t *condition = NULL;
    bool ok = false;

    if (!de_split_equation(text, &lhs, &rhs) ||
        !de_extract_condition_point(lhs, &core, &point_text))
        goto cleanup;

    left = de_parse_expr(core, names, symbols, symbol_count);
    right = de_parse_expr(rhs, names, symbols, symbol_count);
    if (!left ||
        !right ||
        !de_parse_condition_points(
            point_text,
            names,
            symbols,
            symbol_count,
            &points,
            &point_count))
        goto cleanup;

    condition = equ_new(left, right);
    canonical = string_new_with(text);
    if (!condition || !canonical ||
        de_append_condition(
            de,
            condition,
            canonical,
            points,
            point_count) != 0)
        goto cleanup;

    condition = NULL;
    canonical = NULL;
    points = NULL;
    point_count = 0u;
    ok = true;

cleanup:
    equ_free(condition);
    de_free_condition_points(points, point_count);
    string_free(canonical);
    expr_free(right);
    expr_free(left);
    free(point_text);
    free(core);
    free(rhs);
    free(lhs);
    return ok;
}

static bool de_parse_conditions(
    diffequ_t *de,
    const char *text,
    const string_t *const *names,
    expr_t *const *symbols,
    size_t symbol_count)
{
    size_t start = 0u;
    size_t length = strlen(text);
    int parens = 0;

    if (length == 0u)
        return true;

    for (size_t i = 0u; i <= length; ++i) {
        char ch = i < length ? text[i] : ',';

        if (ch == '(')
            parens++;
        else if (ch == ')' && parens > 0)
            parens--;
        if (ch != ',' || parens != 0)
            continue;
        {
            char *condition = de_trimmed_copy(text + start, i - start);
            bool ok = condition && *condition &&
                de_parse_one_condition(
                    de, condition, names, symbols, symbol_count);

            free(condition);
            if (!ok)
                return false;
        }
        start = i + 1u;
    }
    return true;
}

diffequ_t *de_from_string(const char *text)
{
    de_parse_parts_t parts;
    char *normalized_form = NULL;
    char *normalized_totals = NULL;
    char *normalized_partials = NULL;
    char *expanded = NULL;
    char *first_wrt = NULL;
    char *normalized_subscripts = NULL;
    char *normalized_conditions = NULL;
    char *normalized_equation = NULL;
    char *masked_lhs = NULL;
    char *masked_rhs = NULL;
    char *probe_independent = NULL;
    char *probe_text = NULL;
    char *base_lhs = NULL;
    char *base_rhs = NULL;
    equation_t *probe = NULL;
    equation_t *base = NULL;
    expr_bindings_t *base_bindings = NULL;
    const string_t **names = NULL;
    expr_t **symbols = NULL;
    size_t symbol_count = 0u;
    size_t source_equation_start = 0u;
    size_t source_equation_end = 0u;
    char *source_form_equation = NULL;
    bool differential_form_input = false;
    diffequ_t *de = NULL;

    if (!text)
        return NULL;
    de_equation_span(text, &source_equation_start, &source_equation_end);
    source_form_equation = de_trimmed_copy(
        text + source_equation_start,
        source_equation_end - source_equation_start);
    normalized_form = de_normalize_differential_form(
        text, &differential_form_input);
    normalized_totals =
        de_normalize_unicode_total_derivatives(normalized_form);
    free(normalized_form);
    normalized_form = NULL;
    normalized_partials =
        de_normalize_unicode_partial_derivatives(normalized_totals);
    free(normalized_totals);
    normalized_totals = NULL;
    expanded = de_expand_shorthand(normalized_partials);
    free(normalized_partials);
    normalized_partials = NULL;
    if (!expanded) {
        free(source_form_equation);
        return NULL;
    }
    if (!de_parse_parts(expanded, &parts)) {
        free(expanded);
        free(source_form_equation);
        return NULL;
    }
    free(expanded);
    normalized_subscripts = de_normalize_subscript_derivatives(
        parts.equation, parts.independent);
    normalized_conditions = de_normalize_subscript_derivatives(
        parts.conditions, parts.independent);
    if (!normalized_subscripts || !normalized_conditions)
        goto cleanup;
    free(parts.equation);
    parts.equation = normalized_subscripts;
    normalized_subscripts = NULL;
    free(parts.conditions);
    parts.conditions = normalized_conditions;
    normalized_conditions = NULL;
    first_wrt = de_first_declared_independent(parts.independent);
    normalized_equation = first_wrt
        ? de_normalize_prime_derivatives(parts.equation, first_wrt)
        : NULL;
    if (!normalized_equation)
        goto cleanup;
    free(parts.equation);
    parts.equation = normalized_equation;
    normalized_equation = NULL;
    if (!de_split_equation(parts.equation, &base_lhs, &base_rhs))
        goto cleanup;

    masked_lhs = de_mask_formal_derivatives(base_lhs);
    masked_rhs = de_mask_formal_derivatives(base_rhs);
    probe_independent = de_probe_independent_declarations(
        parts.equation, parts.independent);
    if (!masked_lhs || !masked_rhs || !probe_independent)
        goto cleanup;

    if (*parts.constants) {
        if (asprintf(&probe_text,
                     "{ %s = %s | %s; %s }",
                     masked_lhs,
                     masked_rhs,
                     probe_independent,
                     parts.constants) < 0)
            probe_text = NULL;
    } else {
        if (asprintf(&probe_text,
                     "{ %s = %s | %s; }",
                     masked_lhs,
                     masked_rhs,
                     probe_independent) < 0)
            probe_text = NULL;
    }
    probe = probe_text ? equ_from_string(probe_text) : NULL;
    if (!probe)
        goto cleanup;
    de_name_contextual_dependents(
        parts.equation, equ_bindings(probe));

    if (de_build_symbol_arrays(
            equ_bindings(probe), &names, &symbols, &symbol_count) != 0)
        goto cleanup;

    base_bindings =
        expr_bindings_clone_internal(equ_bindings(probe), false);
    base = de_parse_equation_with_symbols(
        parts.equation,
        names,
        symbols,
        symbol_count,
        base_bindings);
    base_bindings = NULL;
    if (!base)
        goto cleanup;
    {
        char *dependent_name =
            de_contextual_dependent_name(parts.equation);

        if (dependent_name) {
            de_name_formal_dependent(
                equ_lhs(base), dependent_name);
            de_name_formal_dependent(
                equ_rhs(base), dependent_name);
        }
        free(dependent_name);
    }

    de = de_new_owned(base);
    if (!de)
        goto cleanup;
    base = NULL;

    de->constants =
        expr_bindings_clone_internal(equ_bindings(probe), true);
    string_free(de->equation_text);
    string_free(de->independent_text);
    string_free(de->constant_text);
    de->equation_text = string_new_with(parts.equation);
    if (differential_form_input) {
        string_free(de->differential_form_text);
        de->differential_form_text = string_new_with(source_form_equation);
        de->differential_form_input = true;
    }
    de->independent_text = string_new_with(parts.independent);
    de->constant_text = string_new_with(parts.constants);
    if (!de->equation_text ||
        !de->differential_form_text ||
        !de->independent_text ||
        !de->constant_text ||
        !de_parse_independents(de, parts.independent, equ_bindings(probe)) ||
        !de_parse_conditions(
            de,
            parts.conditions,
            names,
            symbols,
            symbol_count)) {
        de_free(de);
        de = NULL;
        goto cleanup;
    }

cleanup:
    free(source_form_equation);
    free(normalized_conditions);
    free(normalized_subscripts);
    free(normalized_equation);
    free(first_wrt);
    expr_bindings_free(base_bindings);
    equ_free(base);
    equ_free(probe);
    free(symbols);
    free(names);
    free(probe_independent);
    free(probe_text);
    free(masked_rhs);
    free(masked_lhs);
    free(base_rhs);
    free(base_lhs);
    de_parse_parts_dispose(&parts);
    return de;
}

diffequ_t *de_from_text(const string_t *text)
{
    return text ? de_from_string(string_c_str(text)) : NULL;
}
