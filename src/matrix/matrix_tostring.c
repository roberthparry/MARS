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
} mat_binding_token_t;

static int mat_parse_binding_token(const string_t *binding, mat_binding_token_t *out);

static char mat_ascii_lower(char ch)
{
    return (ch >= 'A' && ch <= 'Z') ? (char)(ch - 'A' + 'a') : ch;
}

static int mat_rune_is_token_space(rune_t rune)
{
    return rune_is_equal(rune, ' ') || rune_is_equal(rune, '\t');
}

static size_t mat_text_display_length(const string_t *text)
{
    return text ? string_length(text) : 0u;
}

static string_t *mat_string_from_owned_cstr(char *raw)
{
    string_t *text;

    if (!raw)
        return NULL;

    text = string_new_with(raw);
    free(raw);
    return text;
}

static int mat_text_equal(const string_t *a, const string_t *b)
{
    return a && b && string_compare(a, b) == 0;
}

static int mat_value_is_nan(const string_t *value)
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

        if (mat_rune_is_token_space(rune)) {
            if (string_cursor_next(cursor) != 0)
                goto done;
            continue;
        }
        if (rune_to_ascii(rune, &ascii)) {
            if (string_append_char(compact, mat_ascii_lower(ascii)) != 0)
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

static int mb_putn(mat_buf_t *b, const char *text, size_t length)
{
    if (mb_ensure(b) != 0)
        return -1;
    return string_append_chars(b->text, text, length);
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

static string_t *mat_cursor_slice_trimmed_text(const string_cursor_t *cursor, string_pos_t start, string_pos_t end)
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
        if (!mat_rune_is_token_space(rune)) {
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

static void mat_free_binding_token(mat_binding_token_t *token)
{
    if (!token)
        return;
    string_free(token->name);
    string_free(token->value);
    token->name = NULL;
    token->value = NULL;
}

static int mat_binding_contains(string_t **bindings, size_t nb, const string_t *token)
{
    for (size_t i = 0; i < nb; ++i) {
        if (mat_text_equal(bindings[i], token))
            return 1;
    }
    return 0;
}

static void mat_append_binding(string_t ***bindings, size_t *nbindings, size_t *capbindings, string_t *token)
{
    mat_binding_token_t incoming = {0};

    if (!token || string_length(token) == 0u) {
        string_free(token);
        return;
    }

    if (mat_parse_binding_token(token, &incoming) == 0) {
        for (size_t i = 0u; i < *nbindings; ++i) {
            mat_binding_token_t existing = {0};

            if (mat_parse_binding_token((*bindings)[i], &existing) != 0)
                continue;
            if (mat_text_equal(existing.name, incoming.name)) {
                if (mat_value_is_nan(existing.value) && !mat_value_is_nan(incoming.value)) {
                    string_free((*bindings)[i]);
                    (*bindings)[i] = token;
                    token = NULL;
                }
                mat_free_binding_token(&existing);
                mat_free_binding_token(&incoming);
                string_free(token);
                return;
            }
            mat_free_binding_token(&existing);
        }
        mat_free_binding_token(&incoming);
    } else if (mat_binding_contains(*bindings, *nbindings, token)) {
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

static void mat_collect_bindings(string_t ***var_bindings, size_t *nvar_bindings, size_t *capvar_bindings,
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
        while (mat_rune_is_token_space(string_cursor_peek(cursor)) || rune_is_equal(string_cursor_peek(cursor), ',')) {
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

        string_t *token = mat_cursor_slice_trimmed_text(cursor, start, string_cursor_position(cursor));
        if (in_constants)
            mat_append_binding(const_bindings, nconst_bindings, capconst_bindings, token);
        else
            mat_append_binding(var_bindings, nvar_bindings, capvar_bindings, token);
    }

done:
    string_cursor_free(cursor);
}

static int mat_text_is_nonempty_without_semicolon(const string_t *text)
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

static void mat_collect_expr_bindings(const expr_t *dv, string_t ***var_bindings, size_t *nvar_bindings,
                                     size_t *capvar_bindings, string_t ***const_bindings, size_t *nconst_bindings,
                                     size_t *capconst_bindings, const string_t *binding_text)
{
    if (dv && expr_is_named_const(dv) && mat_text_is_nonempty_without_semicolon(binding_text)) {
        mat_append_binding(const_bindings, nconst_bindings, capconst_bindings, string_clone(binding_text));
        return;
    }

    mat_collect_bindings(var_bindings, nvar_bindings, capvar_bindings, const_bindings, nconst_bindings,
                        capconst_bindings, binding_text);
}

static string_t *mat_join_binding_list(string_t **bindings, size_t nbindings)
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

static string_t *mat_join_binding_list_for_card(string_t **bindings, size_t nbindings, int plain_expression_syntax)
{
    string_t *out = string_new();
    mat_binding_token_t token = {0};

    if (!out)
        return NULL;

    for (size_t i = 0u; i < nbindings; ++i) {
        if (mat_parse_binding_token(bindings[i], &token) != 0)
            goto fail;
        if (i > 0u && string_append_cstr(out, ", ") != 0)
            goto token_fail;
        if (string_append_string(out, token.name) != 0 || string_append_cstr(out, " = ") != 0)
            goto token_fail;
        if (mat_value_is_nan(token.value)) {
            if (string_append_char(out, '?') != 0)
                goto token_fail;
        } else if (plain_expression_syntax) {
            expr_t *value_expr = expr_from_string(string_c_str(token.value), NULL);
            string_t *value_text = value_expr ? expr_to_function_body_text(value_expr) : NULL;

            if (string_append_string(out, value_text ? value_text : token.value) != 0) {
                string_free(value_text);
                expr_free(value_expr);
                goto token_fail;
            }
            string_free(value_text);
            expr_free(value_expr);
        } else if (string_append_string(out, token.value) != 0) {
            goto token_fail;
        }
        mat_free_binding_token(&token);
    }
    return out;

token_fail:
    mat_free_binding_token(&token);
fail:
    string_free(out);
    return NULL;
}

static int mat_parse_binding_token(const string_t *binding, mat_binding_token_t *out)
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

    out->name = mat_cursor_slice_trimmed_text(cursor, 0u, eq_pos);
    if (string_cursor_next(cursor) != 0)
        goto done;
    out->value =
        mat_cursor_slice_trimmed_text(cursor, string_cursor_position(cursor), string_cursor_end_position(cursor));
    if (!out->name || !out->value) {
        mat_free_binding_token(out);
        goto done;
    }
    ok = 1;

done:
    string_cursor_free(cursor);
    return ok ? 0 : -1;
}

static int mat_has_long_binding(string_t **bindings, size_t nbindings, size_t threshold)
{
    for (size_t i = 0; i < nbindings; ++i) {
        if (mat_text_display_length(bindings[i]) > threshold)
            return 1;
    }
    return 0;
}

static int mat_all_bindings_are_nan(string_t **var_bindings, size_t nvar_bindings, string_t **const_bindings,
                                   size_t nconst_bindings)
{
    for (size_t i = 0; i < nvar_bindings; ++i) {
        mat_binding_token_t token = {0};

        if (mat_parse_binding_token(var_bindings[i], &token) != 0)
            return 0;
        if (!mat_value_is_nan(token.value)) {
            mat_free_binding_token(&token);
            return 0;
        }
        mat_free_binding_token(&token);
    }

    for (size_t i = 0; i < nconst_bindings; ++i) {
        mat_binding_token_t token = {0};

        if (mat_parse_binding_token(const_bindings[i], &token) != 0)
            return 0;
        if (!mat_value_is_nan(token.value)) {
            mat_free_binding_token(&token);
            return 0;
        }
        mat_free_binding_token(&token);
    }

    return (nvar_bindings + nconst_bindings) > 0;
}

static string_t *mat_join_bindings(string_t **var_bindings, size_t nvar_bindings, string_t **const_bindings,
                                  size_t nconst_bindings, int scientific)
{
    string_t *vars = NULL;
    string_t *consts = NULL;
    string_t *out = NULL;

    if (nvar_bindings > 0)
        vars = mat_join_binding_list(var_bindings, nvar_bindings);
    else
        vars = string_new();

    if (nconst_bindings > 0) {
        if (scientific) {
            size_t emitted = 0;

            consts = string_new();
            if (!consts)
                goto fail;

            for (size_t i = 0; i < nconst_bindings; ++i) {
                mat_binding_token_t token = {0};
                string_t *value_text = NULL;
                number_t value = NUM_ZERO;

                if (mat_parse_binding_token(const_bindings[i], &token) != 0) {
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
                mat_free_binding_token(&token);
                continue;

            scientific_fail:
                string_free(value_text);
                num_destroy(&value);
                mat_free_binding_token(&token);
                goto fail;
            }
        } else if (!mat_has_long_binding(const_bindings, nconst_bindings, 16) && nvar_bindings == 0) {
            consts = string_new();
        } else {
            consts = mat_join_binding_list(const_bindings, nconst_bindings);
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

static void mat_emit_cells(mat_buf_t *out, string_t **cells, size_t rows, size_t cols, const size_t *widths, int layout)
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
                for (size_t pad = mat_text_display_length(cells[idx]); pad < widths[j]; ++pad)
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

static void mat_emit_cells_expression_layout(mat_buf_t *out, string_t **cells, size_t rows, size_t cols)
{
    size_t widest_row = 0u;

    for (size_t i = 0u; i < rows; ++i) {
        size_t row_width = cols > 0u ? cols - 1u : 0u;

        for (size_t j = 0u; j < cols; ++j)
            row_width += mat_text_display_length(cells[i * cols + j]);
        if (row_width > widest_row)
            widest_row = row_width;
    }

    mb_puts(out, "(\n");
    if (widest_row > 90u) {
        for (size_t i = 0u; i < rows; ++i) {
            for (size_t j = 0u; j < cols; ++j) {
                size_t idx = i * cols + j;

                mb_putc(out, '\t');
                if (cells[idx])
                    mb_put_text(out, cells[idx]);
                if (j + 1u < cols)
                    mb_putc(out, ',');
                else if (i + 1u < rows)
                    mb_putc(out, ';');
                mb_putc(out, '\n');
            }
        }
        mb_putc(out, ')');
        return;
    }

    for (size_t i = 0; i < rows; ++i) {
        mb_putc(out, '\t');
        for (size_t j = 0; j < cols; ++j) {
            size_t idx = i * cols + j;

            if (j > 0)
                mb_putc(out, '\t');
            if (cells[idx])
                mb_put_text(out, cells[idx]);
            if (j + 1 < cols)
                mb_putc(out, ',');
        }
        if (i + 1 < rows)
            mb_putc(out, ';');
        mb_putc(out, '\n');
    }
    mb_putc(out, ')');
}

static void mat_emit_cells_TeX(mat_buf_t *out, string_t **cells, size_t rows, size_t cols)
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

static int mat_numeric_cells_need_multiline_TeX(string_t **cells, size_t rows, size_t cols)
{
    for (size_t i = 0u; i < rows; ++i) {
        size_t row_width = cols > 0u ? 3u * (cols - 1u) : 0u;

        for (size_t j = 0u; j < cols; ++j) {
            size_t cell_width = mat_text_display_length(cells[i * cols + j]);

            if (cell_width > 64u)
                return 1;
            row_width += cell_width;
        }
        if (row_width > 90u)
            return 1;
    }
    return 0;
}

static void mat_numeric_cell_TeX_parts(const string_t *cell, const char **real, size_t *real_length,
                                      const char **imaginary, size_t *imaginary_length, int *imaginary_sign)
{
    static const char zero[] = "0";
    static const char one[] = "1";
    const char *text = cell ? string_c_str(cell) : zero;
    size_t length = strlen(text);
    const char *separator = strstr(text, " + ");

    if (!separator && text[0] != '\0')
        separator = strstr(text + 1, " - ");

    if (separator) {
        *real = text;
        *real_length = (size_t)(separator - text);
        *imaginary = separator + 3;
        *imaginary_length = length - (size_t)(*imaginary - text);
        *imaginary_sign = separator[1] == '-' ? -1 : 1;
        if (*imaginary_length > 0u && (*imaginary)[*imaginary_length - 1u] == 'i')
            (*imaginary_length)--;
        if (*imaginary_length == 0u) {
            *imaginary = one;
            *imaginary_length = 1u;
        }
        return;
    }

    if (length > 0u && text[length - 1u] == 'i') {
        *real = zero;
        *real_length = 1u;
        *imaginary = text;
        *imaginary_length = length - 1u;
        *imaginary_sign = 1;
        if (*imaginary_length > 0u && ((*imaginary)[0] == '-' || (*imaginary)[0] == '+')) {
            *imaginary_sign = (*imaginary)[0] == '-' ? -1 : 1;
            (*imaginary)++;
            (*imaginary_length)--;
        }
        if (*imaginary_length == 0u) {
            *imaginary = one;
            *imaginary_length = 1u;
        }
        return;
    }

    *real = text;
    *real_length = length;
    *imaginary = zero;
    *imaginary_length = 1u;
    *imaginary_sign = 1;
}

static int mat_numeric_cells_have_imaginary_TeX(string_t **cells, size_t rows, size_t cols)
{
    for (size_t i = 0u; i < rows * cols; ++i) {
        const char *real;
        const char *imaginary;
        size_t real_length;
        size_t imaginary_length;
        int imaginary_sign;

        mat_numeric_cell_TeX_parts(cells[i], &real, &real_length, &imaginary, &imaginary_length, &imaginary_sign);
        if (!(imaginary_length == 1u && imaginary[0] == '0'))
            return 1;
    }
    return 0;
}

static void mat_emit_numeric_component_TeX(mat_buf_t *out, string_t **cells, size_t rows, size_t cols, int imaginary_part)
{
    mb_puts(out, "\\begin{pmatrix}");
    for (size_t i = 0u; i < rows; ++i) {
        if (i > 0u)
            mb_puts(out, " \\\\[10pt] ");
        for (size_t j = 0u; j < cols; ++j) {
            size_t idx = i * cols + j;
            const char *real;
            const char *imaginary;
            size_t real_length;
            size_t imaginary_length;
            int imaginary_sign;

            mat_numeric_cell_TeX_parts(cells[idx], &real, &real_length, &imaginary, &imaginary_length,
                                       &imaginary_sign);

            if (j > 0u)
                mb_puts(out, " & ");
            if (imaginary_part) {
                if (imaginary_sign < 0)
                    mb_putc(out, '-');
                mb_putn(out, imaginary, imaginary_length);
            } else {
                mb_putn(out, real, real_length);
            }
        }
    }
    mb_puts(out, "\\end{pmatrix}");
}

static void mat_emit_numeric_cells_real_imaginary_TeX(mat_buf_t *out, string_t **cells, size_t rows, size_t cols)
{
    mb_puts(out, "\\begin{aligned}&");
    mat_emit_numeric_component_TeX(out, cells, rows, cols, 0);
    mb_puts(out, " \\\\[10pt] &{}+ i\\mkern-2mu ");
    mat_emit_numeric_component_TeX(out, cells, rows, cols, 1);
    mb_puts(out, "\\end{aligned}");
}

static int mat_split_expr_repr(const expr_t *dv, string_t **expr_out, string_t **bindings_out)
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

static int mat_expr_TeX_parts_text(const expr_t *dv, string_t **expr_out, string_t **bindings_out)
{
    char *expr = NULL;
    char *bindings = NULL;
    int ok;

    *expr_out = NULL;
    *bindings_out = NULL;

    if (expr_to_TeX_parts(dv, &expr, &bindings) != 0)
        return -1;

    *expr_out = mat_string_from_owned_cstr(expr);
    *bindings_out = mat_string_from_owned_cstr(bindings);
    ok = *expr_out && *bindings_out;
    if (!ok) {
        string_free(*expr_out);
        string_free(*bindings_out);
        *expr_out = NULL;
        *bindings_out = NULL;
    }
    return ok ? 0 : -1;
}

static string_t *mat_function_expression_text(const expr_function_temporaries_t *temporaries, const expr_t *expr)
{
    if (temporaries)
        return expr_function_temporaries_expression_text(temporaries, expr);
    return expr_to_function_body_text(expr);
}

static string_t *mat_matrix_factor_text(const expr_t *factor, mat_string_style_t style,
                                       const expr_function_temporaries_t *temporaries)
{
    string_t *expression = NULL;
    string_t *bindings = NULL;

    if (!factor)
        return NULL;
    if (style == MAT_STRING_FUNCTION || style == MAT_STRING_EXPRESSION_LAYOUT) {
        expression = mat_function_expression_text(temporaries, factor);
        if (!expression)
            return NULL;
    } else if (style == MAT_STRING_LATEX) {
        if (mat_expr_TeX_parts_text(factor, &expression, &bindings) != 0)
            return NULL;
    } else if (mat_split_expr_repr(factor, &expression, &bindings) != 0) {
        return NULL;
    }
    string_free(bindings);
    return expression;
}

static void mat_pretty_expr_expr(string_t **expr_io, string_t **const_bindings, size_t nconst_bindings)
{
    string_t *expr;

    if (!expr_io || !*expr_io)
        return;

    expr = *expr_io;
    for (size_t i = 0; i < nconst_bindings; ++i) {
        mat_binding_token_t token = {0};

        if (mat_parse_binding_token(const_bindings[i], &token) != 0)
            continue;

        if (mat_text_display_length(const_bindings[i]) > 16 && mat_text_equal(expr, token.value)) {
            string_t *replacement = string_clone(token.name);
            if (replacement) {
                string_free(*expr_io);
                *expr_io = replacement;
            }
            mat_free_binding_token(&token);
            return;
        }

        mat_free_binding_token(&token);
    }
}

static string_t *mat_format_scalar(const matrix_t *A, size_t i, size_t j, int scientific)
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

static string_t *mat_texify_binding_list(string_t **bindings, size_t nbindings)
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

static string_t *mat_join_bindings_TeX(string_t **var_bindings, size_t nvar_bindings, string_t **const_bindings,
                                      size_t nconst_bindings)
{
    string_t *vars = nvar_bindings ? mat_texify_binding_list(var_bindings, nvar_bindings) : string_new();
    string_t *consts = nconst_bindings ? mat_texify_binding_list(const_bindings, nconst_bindings) : string_new();
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
    int expression_layout = (style == MAT_STRING_EXPRESSION_LAYOUT);
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
            string_t *scalar = mat_format_scalar(A, i, j, scientific);

            if (!scalar) {
                ok = 0;
                break;
            }
            cells[idx] =
                tex ? mat_string_from_owned_cstr(expr_tostring_texify(string_c_str(scalar))) : string_clone(scalar);
            string_free(scalar);
            if (!cells[idx]) {
                ok = 0;
                break;
            }
            if (mat_text_display_length(cells[idx]) > widths[j])
                widths[j] = mat_text_display_length(cells[idx]);
        }
    }

    if (!ok) {
        string_free(out.text);
        out.text = NULL;
        goto cleanup;
    }

    if (tex) {
        if (mat_numeric_cells_need_multiline_TeX(cells, A->rows, A->cols) &&
            mat_numeric_cells_have_imaginary_TeX(cells, A->rows, A->cols))
            mat_emit_numeric_cells_real_imaginary_TeX(&out, cells, A->rows, A->cols);
        else
            mat_emit_cells_TeX(&out, cells, A->rows, A->cols);
    }
    else if (expression_layout)
        mat_emit_cells_expression_layout(&out, cells, A->rows, A->cols);
    else
        mat_emit_cells(&out, cells, A->rows, A->cols, widths, layout);

cleanup:
    if (cells) {
        for (size_t i = 0; i < A->rows * A->cols; ++i)
            string_free(cells[i]);
    }
    free(cells);
    free(widths);
    return mb_take(&out);
}

static string_t *mat_to_string_expr(const matrix_t *A, mat_string_style_t style, int include_bindings,
                                    string_t **function_declarations_out)
{
    size_t n = A->rows * A->cols;
    string_t **exprs = calloc(n ? n : 1, sizeof(*exprs));
    string_t **additive_constants = calloc(n ? n : 1, sizeof(*additive_constants));
    mat_expr_beautification_t beautification = {0};
    string_t **var_bindings = NULL;
    string_t **const_bindings = NULL;
    size_t nvar_bindings = 0, capvar_bindings = 0;
    size_t nconst_bindings = 0, capconst_bindings = 0;
    size_t *widths = calloc(A->cols ? A->cols : 1, sizeof(*widths));
    size_t *constant_widths = calloc(A->cols ? A->cols : 1, sizeof(*constant_widths));
    mat_buf_t out = {0};
    int ok = exprs && additive_constants && widths && constant_widths;
    int tex = (style == MAT_STRING_LATEX);
    int expression_style = (style == MAT_STRING_EXPRESSION || style == MAT_STRING_EXPRESSION_LAYOUT);
    int expression_layout = (style == MAT_STRING_EXPRESSION_LAYOUT);
    int function = (style == MAT_STRING_FUNCTION);
    int layout = (style == MAT_STRING_LAYOUT_SCIENTIFIC || style == MAT_STRING_LAYOUT_PRETTY);
    int scientific = (style == MAT_STRING_INLINE_SCIENTIFIC || style == MAT_STRING_LAYOUT_SCIENTIFIC);
    int omit_wrapper = 0;
    string_t *common_factor_text = NULL;
    expr_function_temporaries_t *function_temporaries = NULL;
    const expr_t **function_roots = NULL;

    if (function_declarations_out)
        *function_declarations_out = NULL;

    if (ok && mat_beautify_expression_matrix(A, &beautification) != 0)
        ok = 0;
    if (ok && function) {
        size_t root_count = n + (beautification.additive_constants ? n : 0u) +
                            (beautification.common_factor ? 1u : 0u);
        size_t root_index = 0u;

        function_roots = calloc(root_count ? root_count : 1u, sizeof(*function_roots));
        if (!function_roots) {
            ok = 0;
        } else {
            if (beautification.common_factor)
                function_roots[root_index++] = beautification.common_factor;
            for (size_t index = 0u; index < n; ++index)
                function_roots[root_index++] = beautification.entries[index];
            if (beautification.additive_constants) {
                for (size_t index = 0u; index < n; ++index)
                    function_roots[root_index++] = beautification.additive_constants[index];
            }
            function_temporaries = expr_function_temporaries_new(function_roots, root_index);
            if (!function_temporaries)
                ok = 0;
        }
    }
    common_factor_text = mat_matrix_factor_text(beautification.common_factor, style, function_temporaries);
    if (beautification.common_factor && !common_factor_text)
        ok = 0;

    for (size_t i = 0; ok && i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            string_t *expr = NULL;
            string_t *binding_text = NULL;
            string_t *constant_binding_text = NULL;
            size_t idx = i * A->cols + j;
            expr_t *display_expr = beautification.entries[idx];

            if (function || expression_layout) {
                expr = mat_function_expression_text(function_temporaries, display_expr);
                if (mat_split_expr_repr(display_expr, &constant_binding_text, &binding_text) != 0) {
                    string_free(expr);
                    expr = NULL;
                }
                string_free(constant_binding_text);
                constant_binding_text = NULL;
            } else if ((tex && mat_expr_TeX_parts_text(display_expr, &expr, &binding_text) != 0) ||
                       (!tex && mat_split_expr_repr(display_expr, &expr, &binding_text) != 0)) {
                string_free(expr);
                string_free(binding_text);
                ok = 0;
                break;
            }
            if (!expr || !binding_text) {
                string_free(expr);
                string_free(binding_text);
                ok = 0;
                break;
            }
            exprs[idx] = expr;
            mat_collect_expr_bindings(display_expr, &var_bindings, &nvar_bindings, &capvar_bindings, &const_bindings,
                                     &nconst_bindings, &capconst_bindings, binding_text);
            if (!tex && !expression_layout) {
                mat_pretty_expr_expr(&exprs[idx], const_bindings, nconst_bindings);
            }
            if (mat_text_display_length(exprs[idx]) > widths[j])
                widths[j] = mat_text_display_length(exprs[idx]);
            if (beautification.additive_constants) {
                if (function || expression_layout) {
                    additive_constants[idx] =
                        mat_function_expression_text(function_temporaries, beautification.additive_constants[idx]);
                    if (mat_split_expr_repr(beautification.additive_constants[idx], &expr, &constant_binding_text) != 0) {
                        string_free(additive_constants[idx]);
                        additive_constants[idx] = NULL;
                    }
                    string_free(expr);
                    expr = NULL;
                } else if ((tex && mat_expr_TeX_parts_text(beautification.additive_constants[idx],
                                                          &additive_constants[idx], &constant_binding_text) != 0) ||
                           (!tex && mat_split_expr_repr(beautification.additive_constants[idx], &additive_constants[idx],
                                                       &constant_binding_text) != 0)) {
                    string_free(constant_binding_text);
                    ok = 0;
                    break;
                }
                if (!additive_constants[idx] || !constant_binding_text) {
                    string_free(constant_binding_text);
                    ok = 0;
                    break;
                }
                mat_collect_expr_bindings(beautification.additive_constants[idx], &var_bindings, &nvar_bindings,
                                         &capvar_bindings, &const_bindings, &nconst_bindings, &capconst_bindings,
                                         constant_binding_text);
                if (!tex && !expression_layout)
                    mat_pretty_expr_expr(&additive_constants[idx], const_bindings, nconst_bindings);
                if (mat_text_display_length(additive_constants[idx]) > constant_widths[j])
                    constant_widths[j] = mat_text_display_length(additive_constants[idx]);
                string_free(constant_binding_text);
            }
            string_free(binding_text);
        }
    }

    if (!ok) {
        string_free(out.text);
        out.text = string_new_with("<expr matrix>");
    } else if (tex) {
        omit_wrapper = !include_bindings || mat_all_bindings_are_nan(var_bindings, nvar_bindings, const_bindings, nconst_bindings);
        string_t *joined = mat_join_bindings_TeX(var_bindings, nvar_bindings, const_bindings, nconst_bindings);
        if (!omit_wrapper)
            mb_puts(&out, "\\left\\{ ");
        if (common_factor_text) {
            mb_put_text(&out, common_factor_text);
            mb_puts(&out, "\\mkern-5mu ");
        }
        mat_emit_cells_TeX(&out, exprs, A->rows, A->cols);
        if (beautification.additive_constants) {
            mb_puts(&out, " + ");
            mat_emit_cells_TeX(&out, additive_constants, A->rows, A->cols);
        }
        if (!omit_wrapper && joined && string_length(joined) > 0u) {
            mb_puts(&out, " \\;\\middle|\\; ");
            mb_put_text(&out, joined);
        }
        if (!omit_wrapper)
            mb_puts(&out, " \\right\\}");
        string_free(joined);
    } else if (!layout) {
        string_t *joined;

        omit_wrapper = !include_bindings ||
                       (expression_style ? nvar_bindings + nconst_bindings == 0u
                                         : mat_all_bindings_are_nan(var_bindings, nvar_bindings, const_bindings,
                                                                   nconst_bindings));
        if (expression_style) {
            string_t *vars = mat_join_binding_list_for_card(var_bindings, nvar_bindings, expression_layout);
            string_t *consts = mat_join_binding_list_for_card(const_bindings, nconst_bindings, expression_layout);

            joined = string_new();
            if (joined && vars && string_length(vars) > 0u)
                string_append_string(joined, vars);
            if (joined && consts && string_length(consts) > 0u) {
                string_append_cstr(joined, "; ");
                string_append_string(joined, consts);
            }
            string_free(vars);
            string_free(consts);
        } else {
            joined = mat_join_bindings(var_bindings, nvar_bindings, const_bindings, nconst_bindings, scientific);
        }
        if (!omit_wrapper)
            mb_puts(&out, "{ ");
        if (common_factor_text) {
            mb_put_text(&out, common_factor_text);
            mb_puts(&out, ".");
        }
        if (expression_layout)
            mat_emit_cells_expression_layout(&out, exprs, A->rows, A->cols);
        else
            mat_emit_cells(&out, exprs, A->rows, A->cols, widths, 0);
        if (beautification.additive_constants) {
            mb_puts(&out, " + ");
            if (expression_layout)
                mat_emit_cells_expression_layout(&out, additive_constants, A->rows, A->cols);
            else
                mat_emit_cells(&out, additive_constants, A->rows, A->cols, constant_widths, 0);
        }
        if (!omit_wrapper && joined && string_length(joined) > 0u) {
            mb_puts(&out, " | ");
            mb_put_text(&out, joined);
        }
        if (!omit_wrapper)
            mb_puts(&out, " }");
        string_free(joined);
    } else {
        omit_wrapper = !include_bindings || mat_all_bindings_are_nan(var_bindings, nvar_bindings, const_bindings, nconst_bindings);
        string_t *joined = mat_join_bindings(var_bindings, nvar_bindings, const_bindings, nconst_bindings, scientific);
        if (!omit_wrapper)
            mb_puts(&out, "{ ");
        if (common_factor_text) {
            mb_put_text(&out, common_factor_text);
            mb_puts(&out, ".");
        }
        mat_emit_cells(&out, exprs, A->rows, A->cols, widths, 1);
        if (beautification.additive_constants) {
            mb_puts(&out, " + ");
            mat_emit_cells(&out, additive_constants, A->rows, A->cols, constant_widths, 1);
        }
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
    for (size_t i = 0; i < n; ++i)
        string_free(additive_constants[i]);
    for (size_t i = 0; i < nvar_bindings; ++i)
        string_free(var_bindings[i]);
    for (size_t i = 0; i < nconst_bindings; ++i)
        string_free(const_bindings[i]);
    free(var_bindings);
    free(const_bindings);
    string_free(common_factor_text);
    if (function_declarations_out && ok)
        *function_declarations_out = expr_function_temporaries_declarations_text(function_temporaries);
    expr_function_temporaries_free(function_temporaries);
    mat_expr_beautification_clear(&beautification);
    free(exprs);
    free(additive_constants);
    free(widths);
    free(constant_widths);
    free(function_roots);
    return mb_take(&out);
}

static int mat_append_function_binding_name(string_t *out, const mat_bindings_t *bindings, size_t index,
                                           int with_qualifier)
{
    const char *name = mat_bindings_name_at(bindings, index);

    if (!name)
        return -1;
    if (with_qualifier && mat_bindings_is_constant_at(bindings, index) && string_append_cstr(out, "const ") != 0)
        return -1;
    return string_append_cstr(out, name);
}

static int mat_append_function_binding_value(string_t *out, mat_bindings_t *bindings, size_t index)
{
    expr_t *binding = mat_bindings_expr_at(bindings, index);
    expr_t *value_expr = NULL;
    number_t value;
    string_t *text;
    string_t *function_text = NULL;
    int rc;

    if (!binding)
        return -1;
    value = expr_get_val(binding);
    if (num_is_nan(value)) {
        num_destroy(&value);
        return string_append_char(out, '?');
    }
    text = num_to_string(value);
    num_destroy(&value);
    if (!text)
        return -1;
    value_expr = expr_from_string(string_c_str(text), NULL);
    function_text = value_expr ? expr_to_function_body_text(value_expr) : NULL;
    rc = string_append_string(out, function_text ? function_text : text);
    string_free(function_text);
    expr_free(value_expr);
    string_free(text);
    return rc;
}

static int mat_append_function_binding_group(string_t *out, mat_bindings_t *bindings, int constants, int parameters)
{
    size_t emitted = 0u;

    for (size_t index = 0u; index < mat_bindings_count(bindings); ++index) {
        if ((int)mat_bindings_is_constant_at(bindings, index) != constants)
            continue;
        if (parameters) {
            if (string_length(out) > strlen("matrix mat(") && string_append_cstr(out, ", ") != 0)
                return -1;
            if (mat_append_function_binding_name(out, bindings, index, 1) != 0)
                return -1;
        } else {
            if (constants && string_append_cstr(out, "const ") != 0)
                return -1;
            if (mat_append_function_binding_name(out, bindings, index, 0) != 0 ||
                string_append_cstr(out, " = ") != 0 || mat_append_function_binding_value(out, bindings, index) != 0 ||
                string_append_cstr(out, ".\n") != 0)
                return -1;
        }
        emitted++;
    }
    return (int)emitted;
}

static string_t *mat_to_text_function(const matrix_t *A)
{
    string_t *declarations = NULL;
    string_t *body = matrix_is_symbolic(A) ? mat_to_string_expr(A, MAT_STRING_FUNCTION, 0, &declarations)
                                          : mat_to_string_numeric(A, MAT_STRING_FUNCTION);
    mat_bindings_t *bindings = mat_bindings_from_matrix(A);
    string_t *out = string_new_with("matrix mat(");
    int variable_count;
    int constant_count;

    if (!body || !out || (matrix_is_symbolic(A) && !declarations))
        goto fail;
    variable_count = mat_append_function_binding_group(out, bindings, 0, 1);
    constant_count = mat_append_function_binding_group(out, bindings, 1, 1);
    if (variable_count < 0 || constant_count < 0 || string_append_cstr(out, ") {\n") != 0 ||
        (declarations && string_append_string(out, declarations) != 0) || string_append_cstr(out, "    return ") != 0 ||
        string_append_string(out, body) != 0 || string_append_cstr(out, ".\n}\n\n") != 0)
        goto fail;
    if (mat_append_function_binding_group(out, bindings, 0, 0) < 0 ||
        mat_append_function_binding_group(out, bindings, 1, 0) < 0 || string_append_cstr(out, "output(mat(") != 0)
        goto fail;
    {
        size_t emitted = 0u;

        for (int constants = 0; constants <= 1; ++constants) {
            for (size_t index = 0u; index < mat_bindings_count(bindings); ++index) {
                if ((int)mat_bindings_is_constant_at(bindings, index) != constants)
                    continue;
                if (emitted++ > 0u && string_append_cstr(out, ", ") != 0)
                    goto fail;
                if (mat_append_function_binding_name(out, bindings, index, 0) != 0)
                    goto fail;
            }
        }
    }
    if (string_append_cstr(out, ")).") != 0)
        goto fail;

    string_free(body);
    string_free(declarations);
    mat_bindings_free(bindings);
    return out;

fail:
    string_free(body);
    string_free(declarations);
    string_free(out);
    mat_bindings_free(bindings);
    return NULL;
}

string_t *mat_to_text(const matrix_t *A, mat_string_style_t style)
{
    if (!A)
        return string_new_with("(null)");

    if (style == MAT_STRING_FUNCTION)
        return mat_to_text_function(A);

    if (matrix_is_symbolic(A))
        return mat_to_string_expr(A, style, 1, NULL);
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
        return mat_to_string_expr(A, style, 0, NULL);
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
