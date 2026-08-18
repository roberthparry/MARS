#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "equation.h"
#define MARS_EQUATION_INTERNAL_ACCESS
#include "equation_internal.h"
#include "ustring.h"

typedef struct {
    string_view_t lhs;
    string_view_t rhs;
    string_view_t bindings;
    bool has_bindings;
} equation_parse_parts_t;

static bool equ_view_ascii_at(string_view_t view, size_t pos, char expected)
{
    unsigned char ch = 0u;

    return string_view_peek_ascii(view, pos, &ch) && ch == (unsigned char)expected;
}

static int equ_append_view(string_t *text, string_view_t view)
{
    string_t *copy = string_from_view(&view);
    int rc;

    if (!copy)
        return -1;
    rc = string_append_string(text, copy);
    string_free(copy);
    return rc;
}

static bool equ_scan_depths(string_view_t view, size_t pos, int *paren_depth, int *bracket_depth, int *brace_depth)
{
    unsigned char ch = 0u;

    if (!string_view_peek_ascii(view, pos, &ch))
        return true;

    switch (ch) {
        case '(':
            ++*paren_depth;
            break;
        case ')':
            if (*paren_depth > 0)
                --*paren_depth;
            break;
        case '[':
            ++*bracket_depth;
            break;
        case ']':
            if (*bracket_depth > 0)
                --*bracket_depth;
            break;
        case '{':
            ++*brace_depth;
            break;
        case '}':
            if (*brace_depth > 0)
                --*brace_depth;
            break;
        default:
            break;
    }

    return true;
}

static bool equ_find_top_level_char(string_view_t view, char target, size_t *pos_out)
{
    const size_t len = string_view_length(view);
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;

    for (size_t i = 0u; i < len; ++i) {
        if (paren_depth == 0 && bracket_depth == 0 && brace_depth == 0 && equ_view_ascii_at(view, i, target)) {
            *pos_out = i;
            return true;
        }

        equ_scan_depths(view, i, &paren_depth, &bracket_depth, &brace_depth);
    }

    return false;
}

static bool equ_strip_outer_braces(string_view_t *view)
{
    const size_t len = string_view_length(*view);
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;

    if (len < 2u || !equ_view_ascii_at(*view, 0u, '{'))
        return true;

    for (size_t i = 0u; i < len; ++i) {
        equ_scan_depths(*view, i, &paren_depth, &bracket_depth, &brace_depth);
        if (brace_depth == 0) {
            if (i != len - 1u) {
                fprintf(stderr, "equ_from_text: trailing text after equation wrapper\n");
                return false;
            }

            *view = string_view_trim(string_view_slice(*view, 1u, len - 2u));
            return true;
        }
    }

    fprintf(stderr, "equ_from_text: missing closing equation wrapper\n");
    return false;
}

static bool equ_parse_parts(const string_t *text, equation_parse_parts_t *parts)
{
    string_view_t view;
    size_t split_pos = 0u;
    string_view_t equation_view;

    if (!text || !parts)
        return false;

    view = string_view_trim(string_view_all(text));
    if (string_view_is_empty(view)) {
        fprintf(stderr, "equ_from_text: empty equation\n");
        return false;
    }

    if (!equ_strip_outer_braces(&view))
        return false;

    parts->bindings = string_view_empty();
    parts->has_bindings = equ_find_top_level_char(view, '|', &split_pos);
    if (parts->has_bindings) {
        equation_view = string_view_trim(string_view_slice(view, 0u, split_pos));
        parts->bindings =
            string_view_trim(string_view_slice(view, split_pos + 1u, string_view_length(view) - split_pos - 1u));
    } else {
        equation_view = view;
    }

    if (!equ_find_top_level_char(equation_view, '=', &split_pos)) {
        fprintf(stderr, "equ_from_text: expected top-level '='\n");
        return false;
    }

    parts->lhs = string_view_trim(string_view_slice(equation_view, 0u, split_pos));
    parts->rhs = string_view_trim(
        string_view_slice(equation_view, split_pos + 1u, string_view_length(equation_view) - split_pos - 1u));

    if (string_view_is_empty(parts->lhs) || string_view_is_empty(parts->rhs)) {
        fprintf(stderr, "equ_from_text: equation side is empty\n");
        return false;
    }

    return true;
}

static string_t *equ_make_binding_probe(const equation_parse_parts_t *parts)
{
    string_t *probe = string_new_with("{ (");

    if (!probe)
        return NULL;

    if (equ_append_view(probe, parts->lhs) != 0 || string_append_cstr(probe, ") + (") != 0 ||
        equ_append_view(probe, parts->rhs) != 0 || string_append_char(probe, ')') != 0) {
        string_free(probe);
        return NULL;
    }

    if (parts->has_bindings) {
        if (string_append_cstr(probe, " | ") != 0 || equ_append_view(probe, parts->bindings) != 0) {
            string_free(probe);
            return NULL;
        }
    }

    if (string_append_cstr(probe, " }") != 0) {
        string_free(probe);
        return NULL;
    }

    return probe;
}

static int equ_build_symbol_arrays(expr_bindings_t *bindings, const string_t ***names_out, expr_t ***symbols_out,
                                   size_t *count_out)
{
    const string_t **names = NULL;
    expr_t **symbols = NULL;
    size_t count = expr_bindings_count(bindings);

    *names_out = NULL;
    *symbols_out = NULL;
    *count_out = 0u;

    if (count == 0u)
        return 0;

    names = calloc(count, sizeof(*names));
    symbols = calloc(count, sizeof(*symbols));
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

static expr_t *equ_parse_side(string_view_t side, const string_t *const *names, expr_t *const *symbols,
                              size_t symbol_count)
{
    string_t *side_text = string_from_view(&side);
    expr_t *expr;

    if (!side_text)
        return NULL;

    expr = expr_from_expression_text(side_text, names, symbols, symbol_count);
    string_free(side_text);
    return expr;
}

equation_t *equ_from_text(const string_t *text)
{
    equation_parse_parts_t parts;
    string_t *expanded_lhs = NULL;
    string_t *expanded_rhs = NULL;
    string_t *probe = NULL;
    expr_t *probe_expr = NULL;
    expr_bindings_t *bindings = NULL;
    const string_t **names = NULL;
    expr_t **symbols = NULL;
    size_t symbol_count = 0u;
    expr_t *lhs = NULL;
    expr_t *rhs = NULL;
    equation_t *equation = NULL;

    if (!equ_parse_parts(text, &parts))
        return NULL;

    expanded_lhs = equ_expand_arithmetic_series_side(parts.lhs);
    expanded_rhs = equ_expand_arithmetic_series_side(parts.rhs);
    if (!expanded_lhs || !expanded_rhs)
        goto cleanup;
    parts.lhs = string_view_all(expanded_lhs);
    parts.rhs = string_view_all(expanded_rhs);

    probe = equ_make_binding_probe(&parts);
    if (!probe)
        goto cleanup;

    probe_expr = expr_from_text(probe, &bindings);
    string_free(probe);
    if (!probe_expr)
        goto cleanup;

    expr_free(probe_expr);
    probe_expr = NULL;

    if (equ_build_symbol_arrays(bindings, &names, &symbols, &symbol_count) != 0)
        goto cleanup;

    lhs = equ_parse_side(parts.lhs, names, symbols, symbol_count);
    rhs = equ_parse_side(parts.rhs, names, symbols, symbol_count);
    if (!lhs || !rhs)
        goto cleanup;

    equation = equ_new_with_owned_bindings(lhs, rhs, bindings);
    if (!equation)
        goto cleanup;
    bindings = NULL;

cleanup:
    expr_free(rhs);
    expr_free(lhs);
    free(symbols);
    free(names);
    expr_bindings_free(bindings);
    expr_free(probe_expr);
    string_free(expanded_rhs);
    string_free(expanded_lhs);
    return equation;
}

equation_t *equ_from_string(const char *s)
{
    string_t *text;
    equation_t *equation;

    if (!s)
        return NULL;

    text = string_new_with(s);
    if (!text)
        return NULL;

    equation = equ_from_text(text);
    string_free(text);
    return equation;
}
