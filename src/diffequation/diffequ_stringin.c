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

        while (text[p] && isalpha((unsigned char)text[p])) {
            p++;
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
            for (size_t p = i + 1u; p < open; ++p) {
                if (de_append_independent_name(
                        names, equation + p, 1u) != 0)
                    goto fail;
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
            if (string_append_char(out, text[i++]) != 0)
                goto fail;
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
                               expr_t *point)
{
    size_t count = de->condition_count + 1u;
    equation_t **conditions = malloc(count * sizeof(*conditions));
    string_t **texts = malloc(count * sizeof(*texts));
    expr_t **points = malloc(count * sizeof(*points));

    if (!conditions || !texts || !points) {
        free(conditions);
        free(texts);
        free(points);
        return -1;
    }

    if (de->condition_count > 0u) {
        memcpy(conditions,
               de->conditions,
               de->condition_count * sizeof(*conditions));
        memcpy(texts,
               de->condition_texts,
               de->condition_count * sizeof(*texts));
        memcpy(points,
               de->condition_points,
               de->condition_count * sizeof(*points));
    }
    free(de->conditions);
    free(de->condition_texts);
    free(de->condition_points);
    de->conditions = conditions;
    de->condition_texts = texts;
    de->condition_points = points;
    de->conditions[de->condition_count] = condition;
    de->condition_texts[de->condition_count] = text;
    de->condition_points[de->condition_count] = point;
    de->condition_count = count;
    return 0;
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
    expr_t *point = NULL;
    equation_t *condition = NULL;
    bool ok = false;

    if (!de_split_equation(text, &lhs, &rhs) ||
        !de_extract_condition_point(lhs, &core, &point_text))
        goto cleanup;

    left = de_parse_expr(core, names, symbols, symbol_count);
    right = de_parse_expr(rhs, names, symbols, symbol_count);
    if (point_text)
        point = de_parse_expr(point_text, names, symbols, symbol_count);
    if (!left || !right || (point_text && !point))
        goto cleanup;

    condition = equ_new(left, right);
    canonical = string_new_with(text);
    if (!condition || !canonical ||
        de_append_condition(de, condition, canonical, point) != 0)
        goto cleanup;

    condition = NULL;
    canonical = NULL;
    point = NULL;
    ok = true;

cleanup:
    equ_free(condition);
    expr_free(point);
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
    char *expanded = NULL;
    char *masked_lhs = NULL;
    char *masked_rhs = NULL;
    char *probe_text = NULL;
    char *base_lhs = NULL;
    char *base_rhs = NULL;
    equation_t *probe = NULL;
    equation_t *base = NULL;
    expr_bindings_t *base_bindings = NULL;
    const string_t **names = NULL;
    expr_t **symbols = NULL;
    size_t symbol_count = 0u;
    diffequ_t *de = NULL;

    expanded = de_expand_shorthand(text);
    if (!expanded)
        return NULL;
    if (!de_parse_parts(expanded, &parts)) {
        free(expanded);
        return NULL;
    }
    free(expanded);
    if (!de_split_equation(parts.equation, &base_lhs, &base_rhs))
        goto cleanup;

    masked_lhs = de_mask_formal_derivatives(base_lhs);
    masked_rhs = de_mask_formal_derivatives(base_rhs);
    if (!masked_lhs || !masked_rhs)
        goto cleanup;

    if (*parts.constants) {
        if (asprintf(&probe_text,
                     "{ %s = %s | %s; %s }",
                     masked_lhs,
                     masked_rhs,
                     parts.independent,
                     parts.constants) < 0)
            probe_text = NULL;
    } else {
        if (asprintf(&probe_text,
                     "{ %s = %s | %s; }",
                     masked_lhs,
                     masked_rhs,
                     parts.independent) < 0)
            probe_text = NULL;
    }
    probe = probe_text ? equ_from_string(probe_text) : NULL;
    if (!probe)
        goto cleanup;

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
    de->independent_text = string_new_with(parts.independent);
    de->constant_text = string_new_with(parts.constants);
    if (!de->equation_text ||
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
    expr_bindings_free(base_bindings);
    equ_free(base);
    equ_free(probe);
    free(symbols);
    free(names);
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
