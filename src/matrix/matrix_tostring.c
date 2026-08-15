#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARS_MATRIX_INTERNAL_ACCESS
#include "matrix_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"
#include "ustring.h"

typedef struct {
    string_t *text;
} mat_buf_t;

typedef struct {
    string_t *name;
    string_t *value;
} mt_binding_token_t;

static char mt_ascii_lower(char ch)
{
    return (ch >= 'A' && ch <= 'Z') ? (char)(ch - 'A' + 'a') : ch;
}

static int mt_rune_is_token_space(rune_t rune)
{
    return rune_is_equal(rune, ' ') || rune_is_equal(rune, '\t');
}

static size_t mt_text_display_length(const string_t *text)
{
    return text ? string_length(text) : 0u;
}

static string_t *mt_string_from_owned_cstr(char *raw)
{
    string_t *text;

    if (!raw)
        return NULL;

    text = string_new_with(raw);
    free(raw);
    return text;
}

static int mt_text_equal(const string_t *a, const string_t *b)
{
    return a && b && string_compare(a, b) == 0;
}

static int mt_value_is_nan(const string_t *value)
{
    string_t *compact;
    string_cursor_t *cursor;
    int result = 0;

    if (!value)
        return 0;

    compact = string_new();
    cursor = string_cursor_new(value);
    if (!compact || !cursor)
        goto done;

    while (!string_cursor_done(cursor)) {
        char ascii = '\0';
        rune_t rune = string_cursor_peek(cursor);

        if (mt_rune_is_token_space(rune)) {
            if (string_cursor_next(cursor) != 0)
                goto done;
            continue;
        }
        if (rune_to_ascii(rune, &ascii)) {
            if (string_append_char(compact, mt_ascii_lower(ascii)) != 0)
                goto done;
        } else if (string_append_rune(compact, rune) != 0) {
            goto done;
        }
        if (string_cursor_next(cursor) != 0)
            goto done;
    }

    result = string_view_equals_literal(string_view_all(compact), "nan") ||
             string_view_equals_literal(string_view_all(compact), "nan+0i") ||
             string_view_equals_literal(string_view_all(compact), "nan-0i") ||
             string_view_equals_literal(string_view_all(compact), "nan+0.0i") ||
             string_view_equals_literal(string_view_all(compact), "nan-0.0i");

done:
    string_cursor_free(cursor);
    string_free(compact);
    return result;
}

static int mb_ensure(mat_buf_t *b)
{
    if (b->text)
        return 0;
    b->text = string_new();
    return b->text ? 0 : -1;
}

static int mb_puts(mat_buf_t *b, const char *s)
{
    if (mb_ensure(b) != 0)
        return -1;
    return string_append_cstr(b->text, s);
}

static int mb_put_text(mat_buf_t *b, const string_t *text)
{
    if (mb_ensure(b) != 0)
        return -1;
    return string_append_string(b->text, text);
}

static int mb_putc(mat_buf_t *b, char c)
{
    if (mb_ensure(b) != 0)
        return -1;
    return string_append_char(b->text, c);
}

static string_t *mb_take(mat_buf_t *b)
{
    string_t *text;

    if (!b->text)
        b->text = string_new();
    text = b->text;
    b->text = NULL;
    return text;
}

static string_t *mt_cursor_slice_trimmed_text(const string_cursor_t *cursor, string_pos_t start, string_pos_t end)
{
    string_cursor_t *scan;
    string_t *text = NULL;
    string_pos_t trimmed_start = start;
    string_pos_t trimmed_end = start;
    int seen = 0;

    if (!cursor || start > end)
        return NULL;

    scan = string_cursor_clone(cursor);
    if (!scan || string_cursor_seek(scan, start) != 0)
        goto done;

    while (string_cursor_position(scan) < end && !string_cursor_done(scan)) {
        string_pos_t rune_start = string_cursor_position(scan);
        rune_t rune = string_cursor_peek(scan);

        if (string_cursor_next(scan) != 0)
            goto done;
        if (!mt_rune_is_token_space(rune)) {
            if (!seen)
                trimmed_start = rune_start;
            trimmed_end = string_cursor_position(scan);
            seen = 1;
        }
    }

    text = seen ? string_cursor_slice_between(trimmed_start, trimmed_end, cursor) : string_new_with("");

done:
    string_cursor_free(scan);
    return text;
}

static void mt_free_binding_token(mt_binding_token_t *token)
{
    if (!token)
        return;
    string_free(token->name);
    string_free(token->value);
    token->name = NULL;
    token->value = NULL;
}

static int mt_binding_contains(string_t **bindings, size_t nb, const string_t *token)
{
    for (size_t i = 0; i < nb; ++i) {
        if (mt_text_equal(bindings[i], token))
            return 1;
    }
    return 0;
}

static void mt_append_binding(string_t ***bindings, size_t *nbindings, size_t *capbindings, string_t *token)
{
    if (!token || string_length(token) == 0u) {
        string_free(token);
        return;
    }

    if (mt_binding_contains(*bindings, *nbindings, token)) {
        string_free(token);
        return;
    }

    if (*nbindings == *capbindings) {
        size_t new_cap = *capbindings ? (*capbindings * 2) : 8;
        string_t **grown = realloc(*bindings, new_cap * sizeof(**bindings));
        if (!grown) {
            string_free(token);
            return;
        }
        *bindings = grown;
        *capbindings = new_cap;
    }

    (*bindings)[(*nbindings)++] = token;
}

static void mt_collect_bindings(string_t ***var_bindings, size_t *nvar_bindings, size_t *capvar_bindings,
                                string_t ***const_bindings, size_t *nconst_bindings, size_t *capconst_bindings,
                                const string_t *binding_text)
{
    string_cursor_t *cursor;
    int in_constants = 0;

    if (!binding_text)
        return;

    cursor = string_cursor_new(binding_text);
    if (!cursor)
        goto done;

    while (!string_cursor_done(cursor)) {
        while (mt_rune_is_token_space(string_cursor_peek(cursor)) || rune_is_equal(string_cursor_peek(cursor), ',')) {
            if (string_cursor_next(cursor) != 0)
                goto done;
        }
        if (rune_is_equal(string_cursor_peek(cursor), ';')) {
            in_constants = 1;
            if (string_cursor_next(cursor) != 0)
                goto done;
            continue;
        }
        if (string_cursor_done(cursor))
            break;

        string_pos_t start = string_cursor_position(cursor);
        while (!string_cursor_done(cursor) && !rune_is_equal(string_cursor_peek(cursor), ',') &&
               !rune_is_equal(string_cursor_peek(cursor), ';')) {
            if (string_cursor_next(cursor) != 0)
                goto done;
        }

        string_t *token = mt_cursor_slice_trimmed_text(cursor, start, string_cursor_position(cursor));
        if (in_constants)
            mt_append_binding(const_bindings, nconst_bindings, capconst_bindings, token);
        else
            mt_append_binding(var_bindings, nvar_bindings, capvar_bindings, token);
    }

done:
    string_cursor_free(cursor);
}

static int mt_text_is_nonempty_without_semicolon(const string_t *text)
{
    string_cursor_t *cursor = text ? string_cursor_new(text) : NULL;
    int ok = 0;

    if (!text || !cursor)
        goto done;
    if (string_length(text) == 0u)
        goto done;

    while (!string_cursor_done(cursor)) {
        if (rune_is_equal(string_cursor_peek(cursor), ';'))
            goto done;
        if (string_cursor_next(cursor) != 0)
            goto done;
    }
    ok = 1;

done:
    string_cursor_free(cursor);
    return ok;
}

static void mt_collect_expr_bindings(const expr_t *dv, string_t ***var_bindings, size_t *nvar_bindings,
                                     size_t *capvar_bindings, string_t ***const_bindings, size_t *nconst_bindings,
                                     size_t *capconst_bindings, const string_t *binding_text)
{
    if (dv && expr_is_named_const(dv) && mt_text_is_nonempty_without_semicolon(binding_text)) {
        mt_append_binding(const_bindings, nconst_bindings, capconst_bindings, string_clone(binding_text));
        return;
    }

    mt_collect_bindings(var_bindings, nvar_bindings, capvar_bindings, const_bindings, nconst_bindings,
                        capconst_bindings, binding_text);
}

static string_t *mt_join_binding_list(string_t **bindings, size_t nbindings)
{
    string_t *out = string_new();

    if (!out)
        return NULL;

    for (size_t i = 0; i < nbindings; ++i) {
        if (i && string_append_cstr(out, ", ") != 0)
            goto fail;
        if (string_append_string(out, bindings[i]) != 0)
            goto fail;
    }

    return out;

fail:
    string_free(out);
    return NULL;
}

static int mt_parse_binding_token(const string_t *binding, mt_binding_token_t *out)
{
    string_cursor_t *cursor;
    string_pos_t eq_pos = 0u;
    int found_eq = 0;
    int ok = 0;

    out->name = NULL;
    out->value = NULL;

    cursor = binding ? string_cursor_new(binding) : NULL;
    if (!cursor)
        goto done;

    while (!string_cursor_done(cursor)) {
        if (rune_is_equal(string_cursor_peek(cursor), '=')) {
            eq_pos = string_cursor_position(cursor);
            found_eq = 1;
            break;
        }
        if (string_cursor_next(cursor) != 0)
            goto done;
    }
    if (!found_eq)
        goto done;

    out->name = mt_cursor_slice_trimmed_text(cursor, 0u, eq_pos);
    if (string_cursor_next(cursor) != 0)
        goto done;
    out->value =
        mt_cursor_slice_trimmed_text(cursor, string_cursor_position(cursor), string_cursor_end_position(cursor));
    if (!out->name || !out->value) {
        mt_free_binding_token(out);
        goto done;
    }
    ok = 1;

done:
    string_cursor_free(cursor);
    return ok ? 0 : -1;
}

static int mt_has_long_binding(string_t **bindings, size_t nbindings, size_t threshold)
{
    for (size_t i = 0; i < nbindings; ++i) {
        if (mt_text_display_length(bindings[i]) > threshold)
            return 1;
    }
    return 0;
}

static int mt_all_bindings_are_nan(string_t **var_bindings, size_t nvar_bindings, string_t **const_bindings,
                                   size_t nconst_bindings)
{
    for (size_t i = 0; i < nvar_bindings; ++i) {
        mt_binding_token_t token = {0};

        if (mt_parse_binding_token(var_bindings[i], &token) != 0)
            return 0;
        if (!mt_value_is_nan(token.value)) {
            mt_free_binding_token(&token);
            return 0;
        }
        mt_free_binding_token(&token);
    }

    for (size_t i = 0; i < nconst_bindings; ++i) {
        mt_binding_token_t token = {0};

        if (mt_parse_binding_token(const_bindings[i], &token) != 0)
            return 0;
        if (!mt_value_is_nan(token.value)) {
            mt_free_binding_token(&token);
            return 0;
        }
        mt_free_binding_token(&token);
    }

    return (nvar_bindings + nconst_bindings) > 0;
}

static string_t *mt_join_bindings(string_t **var_bindings, size_t nvar_bindings, string_t **const_bindings,
                                  size_t nconst_bindings, int scientific)
{
    string_t *vars = NULL;
    string_t *consts = NULL;
    string_t *out = NULL;

    if (nvar_bindings > 0)
        vars = mt_join_binding_list(var_bindings, nvar_bindings);
    else
        vars = string_new();

    if (nconst_bindings > 0) {
        if (scientific) {
            size_t emitted = 0;

            consts = string_new();
            if (!consts)
                goto fail;

            for (size_t i = 0; i < nconst_bindings; ++i) {
                mt_binding_token_t token = {0};
                string_t *value_text = NULL;
                number_t value = NUM_ZERO;

                if (mt_parse_binding_token(const_bindings[i], &token) != 0) {
                    num_destroy(&value);
                    continue;
                }
                if (num_set_from_text(&value, token.value) != 0) {
                    value_text = string_clone(token.value);
                } else {
                    value_text = num_sprintf_text("%N", value);
                }
                if (!value_text)
                    goto scientific_fail;
                if (emitted > 0 && string_append_cstr(consts, ", ") != 0)
                    goto scientific_fail;
                if (string_append_string(consts, token.name) != 0 || string_append_cstr(consts, " = ") != 0 ||
                    string_append_string(consts, value_text) != 0)
                    goto scientific_fail;
                emitted++;
                string_free(value_text);
                num_destroy(&value);
                mt_free_binding_token(&token);
                continue;

            scientific_fail:
                string_free(value_text);
                num_destroy(&value);
                mt_free_binding_token(&token);
                goto fail;
            }
        } else if (!mt_has_long_binding(const_bindings, nconst_bindings, 16) && nvar_bindings == 0) {
            consts = string_new();
        } else {
            consts = mt_join_binding_list(const_bindings, nconst_bindings);
        }
    } else {
        consts = string_new();
    }

    if (!vars || !consts)
        goto fail;

    if (string_length(vars) == 0u && string_length(consts) == 0u) {
        string_free(vars);
        return consts;
    }

    out = string_new();
    if (!out)
        goto fail;

    if (string_length(vars) > 0u && string_append_string(out, vars) != 0)
        goto fail;
    if (string_length(consts) > 0u) {
        if (string_length(vars) > 0u && string_append_cstr(out, "; ") != 0)
            goto fail;
        if (string_append_string(out, consts) != 0)
            goto fail;
    }

    string_free(vars);
    string_free(consts);
    return out;

fail:
    string_free(vars);
    string_free(consts);
    string_free(out);
    return NULL;
}

static void mt_emit_cells(mat_buf_t *out, string_t **cells, size_t rows, size_t cols, const size_t *widths, int layout)
{
    if (!layout)
        mb_putc(out, '(');

    for (size_t i = 0; i < rows; ++i) {
        if (layout)
            mb_puts(out, (i == 0) ? "(\n  " : "\n  ");

        for (size_t j = 0; j < cols; ++j) {
            size_t idx = i * cols + j;

            if (j > 0)
                mb_puts(out, layout ? " " : ", ");
            if (layout) {
                for (size_t pad = mt_text_display_length(cells[idx]); pad < widths[j]; ++pad)
                    mb_putc(out, ' ');
            }
            if (cells[idx])
                mb_put_text(out, cells[idx]);
        }

        if (layout) {
            if (i + 1 == rows)
                mb_puts(out, "\n)");
        } else if (i + 1 < rows) {
            mb_puts(out, "; ");
        }
    }

    if (!layout)
        mb_putc(out, ')');
}

static void mt_emit_cells_TeX(mat_buf_t *out, string_t **cells, size_t rows, size_t cols)
{
    mb_puts(out, "\\begin{bmatrix}");
    for (size_t i = 0; i < rows; ++i) {
        if (i > 0)
            mb_puts(out, " \\\\[10pt] ");
        for (size_t j = 0; j < cols; ++j) {
            size_t idx = i * cols + j;

            if (j > 0)
                mb_puts(out, " & ");
            if (cells[idx])
                mb_put_text(out, cells[idx]);
        }
    }
    mb_puts(out, "\\end{bmatrix}");
}

static int mt_split_expr_repr(const expr_t *dv, string_t **expr_out, string_t **bindings_out)
{
    string_t *tmp_text;
    string_t *expr_text = NULL;
    string_t *bindings_text = NULL;
    string_cursor_t *cursor = NULL;
    string_cursor_t *scan = NULL;
    string_pos_t body_start = 0u;
    string_pos_t body_end;
    string_pos_t sep_pos = 0u;
    int found_sep = 0;
    int ok = 0;

    *expr_out = NULL;
    *bindings_out = NULL;

    if (!dv) {
        *expr_out = string_new_with("NULL");
        *bindings_out = string_new_with("");
        return (*expr_out && *bindings_out) ? 0 : -1;
    }

    tmp_text = expr_to_text(dv, style_EXPRESSION);
    if (!tmp_text)
        return -1;

    cursor = string_cursor_new(tmp_text);
    if (!cursor)
        goto done;

    body_end = string_cursor_end_position(cursor);
    if (body_end >= 4u && string_cursor_match_at(cursor, 0u, "{ ") &&
        string_cursor_match_at(cursor, body_end - 2u, " }")) {
        body_start = 2u;
        body_end -= 2u;
    }

    scan = string_cursor_clone(cursor);
    if (!scan || string_cursor_seek(scan, body_start) != 0)
        goto done;

    while (string_cursor_position(scan) < body_end) {
        string_pos_t pos = string_cursor_position(scan);

        if (pos + 3u <= body_end && string_cursor_match_at(scan, pos, " | ")) {
            sep_pos = pos;
            found_sep = 1;
            break;
        }
        if (string_cursor_next(scan) != 0)
            break;
    }

    if (found_sep) {
        expr_text = string_cursor_slice_between(body_start, sep_pos, cursor);
        bindings_text = string_cursor_slice_between(sep_pos + 3u, body_end, cursor);
    } else {
        expr_text = string_cursor_slice_between(body_start, body_end, cursor);
        bindings_text = string_new_with("");
    }
    if (!expr_text || !bindings_text)
        goto done;

    *expr_out = expr_text;
    *bindings_out = bindings_text;
    expr_text = NULL;
    bindings_text = NULL;
    ok = (*expr_out && *bindings_out);

done:
    if (!ok) {
        string_free(*expr_out);
        string_free(*bindings_out);
        *expr_out = NULL;
        *bindings_out = NULL;
    }
    string_cursor_free(scan);
    string_cursor_free(cursor);
    string_free(expr_text);
    string_free(bindings_text);
    string_free(tmp_text);
    return ok ? 0 : -1;
}

static int mt_expr_TeX_parts_text(const expr_t *dv, string_t **expr_out, string_t **bindings_out)
{
    char *expr = NULL;
    char *bindings = NULL;
    int ok;

    *expr_out = NULL;
    *bindings_out = NULL;

    if (expr_to_TeX_parts(dv, &expr, &bindings) != 0)
        return -1;

    *expr_out = mt_string_from_owned_cstr(expr);
    *bindings_out = mt_string_from_owned_cstr(bindings);
    ok = *expr_out && *bindings_out;
    if (!ok) {
        string_free(*expr_out);
        string_free(*bindings_out);
        *expr_out = NULL;
        *bindings_out = NULL;
    }
    return ok ? 0 : -1;
}

static void mt_pretty_expr_expr(string_t **expr_io, string_t **const_bindings, size_t nconst_bindings)
{
    string_t *expr;

    if (!expr_io || !*expr_io)
        return;

    expr = *expr_io;
    for (size_t i = 0; i < nconst_bindings; ++i) {
        mt_binding_token_t token = {0};

        if (mt_parse_binding_token(const_bindings[i], &token) != 0)
            continue;

        if (mt_text_display_length(const_bindings[i]) > 16 && mt_text_equal(expr, token.value)) {
            string_t *replacement = string_clone(token.name);
            if (replacement) {
                string_free(*expr_io);
                *expr_io = replacement;
            }
            mt_free_binding_token(&token);
            return;
        }

        mt_free_binding_token(&token);
    }
}

static string_t *mt_format_scalar(const matrix_t *A, size_t i, size_t j, int scientific)
{
    unsigned char *raw;
    string_t *text = NULL;

    raw = calloc(1u, A->elem->size ? A->elem->size : 1u);
    if (!raw)
        return NULL;

    mat_get_owned(A, i, j, raw);

    if (!A->elem->format_scalar_text) {
        mat_value_destroy(A, raw);
        free(raw);
        return NULL;
    }

    text = A->elem->format_scalar_text(raw, scientific);
    mat_value_destroy(A, raw);
    free(raw);
    return text;
}

static string_t *mt_texify_binding_list(string_t **bindings, size_t nbindings)
{
    string_t *out = string_new();

    if (!out)
        return NULL;

    for (size_t i = 0; i < nbindings; ++i) {
        char *tex = expr_tostring_texify(string_c_str(bindings[i]));

        if (i > 0 && string_append_cstr(out, ", ") != 0) {
            free(tex);
            goto fail;
        }
        if (tex) {
            if (string_append_cstr(out, tex) != 0) {
                free(tex);
                goto fail;
            }
            free(tex);
        } else {
            if (string_append_string(out, bindings[i]) != 0)
                goto fail;
        }
    }

    return out;

fail:
    string_free(out);
    return NULL;
}

static string_t *mt_join_bindings_TeX(string_t **var_bindings, size_t nvar_bindings, string_t **const_bindings,
                                      size_t nconst_bindings)
{
    string_t *vars = nvar_bindings ? mt_texify_binding_list(var_bindings, nvar_bindings) : string_new();
    string_t *consts = nconst_bindings ? mt_texify_binding_list(const_bindings, nconst_bindings) : string_new();
    string_t *out = NULL;

    if (!vars || !consts)
        goto fail;

    if (string_length(vars) == 0u && string_length(consts) == 0u) {
        string_free(vars);
        return consts;
    }

    out = string_new();
    if (!out)
        goto fail;

    if (string_length(vars) > 0u && string_append_string(out, vars) != 0)
        goto fail;
    if (string_length(consts) > 0u) {
        if (string_length(vars) > 0u && string_append_cstr(out, "; ") != 0)
            goto fail;
        if (string_append_string(out, consts) != 0)
            goto fail;
    }

    string_free(vars);
    string_free(consts);
    return out;

fail:
    string_free(vars);
    string_free(consts);
    string_free(out);
    return NULL;
}

static string_t *mat_to_string_numeric(const matrix_t *A, mat_string_style_t style)
{
    mat_buf_t out = {0};
    string_t **cells = NULL;
    size_t *widths = NULL;
    int tex = (style == MAT_STRING_LATEX);
    int layout = (style == MAT_STRING_LAYOUT_SCIENTIFIC || style == MAT_STRING_LAYOUT_PRETTY);
    int scientific = (style == MAT_STRING_INLINE_SCIENTIFIC || style == MAT_STRING_LAYOUT_SCIENTIFIC);
    int ok = 1;

    size_t ncell = A->rows * A->cols;

    cells = calloc(ncell ? ncell : 1, sizeof(*cells));
    widths = calloc(A->cols ? A->cols : 1, sizeof(*widths));
    ok = cells && widths;

    for (size_t i = 0; ok && i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            size_t idx = i * A->cols + j;
            string_t *scalar = mt_format_scalar(A, i, j, scientific);

            if (!scalar) {
                ok = 0;
                break;
            }
            cells[idx] =
                tex ? mt_string_from_owned_cstr(expr_tostring_texify(string_c_str(scalar))) : string_clone(scalar);
            string_free(scalar);
            if (!cells[idx]) {
                ok = 0;
                break;
            }
            if (mt_text_display_length(cells[idx]) > widths[j])
                widths[j] = mt_text_display_length(cells[idx]);
        }
    }

    if (!ok) {
        string_free(out.text);
        out.text = NULL;
        goto cleanup;
    }

    if (tex)
        mt_emit_cells_TeX(&out, cells, A->rows, A->cols);
    else
        mt_emit_cells(&out, cells, A->rows, A->cols, widths, layout);

cleanup:
    if (cells) {
        for (size_t i = 0; i < A->rows * A->cols; ++i)
            string_free(cells[i]);
    }
    free(cells);
    free(widths);
    return mb_take(&out);
}

static string_t *mat_to_string_expr(const matrix_t *A, mat_string_style_t style, int include_bindings)
{
    size_t n = A->rows * A->cols;
    string_t **exprs = calloc(n ? n : 1, sizeof(*exprs));
    string_t **var_bindings = NULL;
    string_t **const_bindings = NULL;
    size_t nvar_bindings = 0, capvar_bindings = 0;
    size_t nconst_bindings = 0, capconst_bindings = 0;
    size_t *widths = calloc(A->cols ? A->cols : 1, sizeof(*widths));
    mat_buf_t out = {0};
    int ok = exprs && widths;
    int tex = (style == MAT_STRING_LATEX);
    int layout = (style == MAT_STRING_LAYOUT_SCIENTIFIC || style == MAT_STRING_LAYOUT_PRETTY);
    int scientific = (style == MAT_STRING_INLINE_SCIENTIFIC || style == MAT_STRING_LAYOUT_SCIENTIFIC);
    int omit_wrapper = 0;

    for (size_t i = 0; ok && i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            string_t *expr = NULL;
            string_t *binding_text = NULL;
            expr_t *dv = NULL;
            expr_t *display_expr = NULL;
            size_t idx = i * A->cols + j;

            mat_get(A, i, j, &dv);
            if (expr_contains_half_scaled_symbolic_power(dv))
                display_expr = expr_factor_common_post_calculus(dv);
            if (!display_expr && dv) {
                expr_retain(dv);
                display_expr = dv;
            }
            if ((tex && mt_expr_TeX_parts_text(display_expr, &expr, &binding_text) != 0) ||
                (!tex && mt_split_expr_repr(display_expr, &expr, &binding_text) != 0)) {
                string_free(expr);
                string_free(binding_text);
                expr_free(display_expr);
                ok = 0;
                break;
            }
            exprs[idx] = expr;
            mt_collect_expr_bindings(display_expr, &var_bindings, &nvar_bindings, &capvar_bindings, &const_bindings,
                                     &nconst_bindings, &capconst_bindings, binding_text);
            if (!tex) {
                mt_pretty_expr_expr(&exprs[idx], const_bindings, nconst_bindings);
            }
            if (mt_text_display_length(exprs[idx]) > widths[j])
                widths[j] = mt_text_display_length(exprs[idx]);
            string_free(binding_text);
            expr_free(display_expr);
        }
    }

    if (!ok) {
        string_free(out.text);
        out.text = string_new_with("<expr matrix>");
    } else if (tex) {
        omit_wrapper = !include_bindings || mt_all_bindings_are_nan(var_bindings, nvar_bindings, const_bindings, nconst_bindings);
        string_t *joined = mt_join_bindings_TeX(var_bindings, nvar_bindings, const_bindings, nconst_bindings);
        if (!omit_wrapper)
            mb_puts(&out, "\\left\\{ ");
        mt_emit_cells_TeX(&out, exprs, A->rows, A->cols);
        if (!omit_wrapper && joined && string_length(joined) > 0u) {
            mb_puts(&out, " \\;\\middle|\\; ");
            mb_put_text(&out, joined);
        }
        if (!omit_wrapper)
            mb_puts(&out, " \\right\\}");
        string_free(joined);
    } else if (!layout) {
        omit_wrapper = !include_bindings || mt_all_bindings_are_nan(var_bindings, nvar_bindings, const_bindings, nconst_bindings);
        string_t *joined = mt_join_bindings(var_bindings, nvar_bindings, const_bindings, nconst_bindings, scientific);
        if (!omit_wrapper)
            mb_puts(&out, "{ ");
        mt_emit_cells(&out, exprs, A->rows, A->cols, widths, 0);
        if (!omit_wrapper && joined && string_length(joined) > 0u) {
            mb_puts(&out, " | ");
            mb_put_text(&out, joined);
        }
        if (!omit_wrapper)
            mb_puts(&out, " }");
        string_free(joined);
    } else {
        omit_wrapper = !include_bindings || mt_all_bindings_are_nan(var_bindings, nvar_bindings, const_bindings, nconst_bindings);
        string_t *joined = mt_join_bindings(var_bindings, nvar_bindings, const_bindings, nconst_bindings, scientific);
        if (!omit_wrapper)
            mb_puts(&out, "{ ");
        mt_emit_cells(&out, exprs, A->rows, A->cols, widths, 1);
        if (!omit_wrapper && joined && string_length(joined) > 0u) {
            mb_puts(&out, " | ");
            mb_put_text(&out, joined);
        }
        if (!omit_wrapper)
            mb_puts(&out, " }");
        string_free(joined);
    }

    for (size_t i = 0; i < n; ++i)
        string_free(exprs[i]);
    for (size_t i = 0; i < nvar_bindings; ++i)
        string_free(var_bindings[i]);
    for (size_t i = 0; i < nconst_bindings; ++i)
        string_free(const_bindings[i]);
    free(var_bindings);
    free(const_bindings);
    free(exprs);
    free(widths);
    return mb_take(&out);
}

string_t *mat_to_text(const matrix_t *A, mat_string_style_t style)
{
    if (!A)
        return string_new_with("(null)");

    if (matrix_is_symbolic(A))
        return mat_to_string_expr(A, style, 1);
    return mat_to_string_numeric(A, style);
}

char *mat_to_string(const matrix_t *A, mat_string_style_t style)
{
    string_t *text = mat_to_text(A, style);
    char *out;

    if (!text)
        return NULL;

    out = strdup(string_c_str(text));
    string_free(text);
    return out;
}

/* Convert a matrix body to owned text without serialising its bindings. */
string_t *mat_body_to_text(const matrix_t *A, mat_string_style_t style)
{
    if (!A)
        return string_new_with("(null)");

    if (matrix_is_symbolic(A))
        return mat_to_string_expr(A, style, 0);
    return mat_to_string_numeric(A, style);
}

/* Convert a matrix body to an owned C string without serialising its bindings. */
char *mat_body_to_string(const matrix_t *A, mat_string_style_t style)
{
    string_t *text = mat_body_to_text(A, style);
    char *out;

    if (!text)
        return NULL;

    out = strdup(string_c_str(text));
    string_free(text);
    return out;
}

bool mat_serialize(const matrix_t *A, string_t **out_type, string_t **out_encoding, void **out_data, size_t *out_len)
{
    string_t *type = NULL;
    string_t *encoding = NULL;
    string_t *text = NULL;
    void *payload = NULL;

    if (!A || !out_type || !out_encoding || !out_data || !out_len)
        return false;

    text = mat_to_text(A, MAT_STRING_INLINE_PRETTY);
    if (!text)
        return false;

    payload = malloc(string_byte_length(text));
    if (!payload) {
        string_free(text);
        return false;
    }
    memcpy(payload, string_c_str(text), string_byte_length(text));

    type = string_new_with("matrix_t");
    encoding = string_new_with("mars/matrix-text");
    if (!type || !encoding) {
        free(payload);
        string_free(text);
        string_free(type);
        string_free(encoding);
        return false;
    }

    *out_type = type;
    *out_encoding = encoding;
    *out_data = payload;
    *out_len = string_byte_length(text);
    string_free(text);
    return true;
}

matrix_t *mat_deserialise(const void *data, size_t len, const string_t *type, const string_t *encoding)
{
    string_t *text;
    matrix_t *matrix;

    if (!data || !type || !encoding)
        return NULL;
    if (strcmp(string_c_str(type), "matrix_t") != 0 || strcmp(string_c_str(encoding), "mars/matrix-text") != 0)
        return NULL;

    text = string_new();
    if (!text)
        return NULL;
    if (string_append_chars(text, (const char *)data, len) != 0) {
        string_free(text);
        return NULL;
    }
    matrix = mat_from_text_expr(text, NULL);
    string_free(text);
    return matrix;
}
