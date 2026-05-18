#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "matrix_internal.h"
#include "internal/dval_internal.h"

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} mat_buf_t;

typedef struct {
    char *name;
    char *value;
} mt_binding_token_t;

static int mt_value_is_nan(const char *value)
{
    size_t len;
    char compact[256];
    size_t j = 0;

    if (!value)
        return 0;

    len = strlen(value);
    if (len >= sizeof(compact))
        return 0;

    for (size_t i = 0; i < len; ++i) {
        if (value[i] == ' ' || value[i] == '\t')
            continue;
        compact[j++] = (char)tolower((unsigned char)value[i]);
    }
    compact[j] = '\0';

    if (strcmp(compact, "nan") == 0)
        return 1;
    if (strcmp(compact, "nan+0i") == 0 || strcmp(compact, "nan-0i") == 0)
        return 1;
    if (strcmp(compact, "nan+0.0i") == 0 || strcmp(compact, "nan-0.0i") == 0)
        return 1;
    return 0;
}

static int mb_reserve(mat_buf_t *b, size_t extra)
{
    if (b->len + extra + 1 <= b->cap)
        return 0;

    size_t new_cap = b->cap ? b->cap * 2 : 128;
    while (new_cap < b->len + extra + 1)
        new_cap *= 2;

    char *grown = realloc(b->data, new_cap);
    if (!grown)
        return -1;
    b->data = grown;
    b->cap = new_cap;
    return 0;
}

static int mb_puts(mat_buf_t *b, const char *s)
{
    size_t n = strlen(s);
    if (mb_reserve(b, n) != 0)
        return -1;
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
    return 0;
}

static int mb_putc(mat_buf_t *b, char c)
{
    if (mb_reserve(b, 1) != 0)
        return -1;
    b->data[b->len++] = c;
    b->data[b->len] = '\0';
    return 0;
}

static char *mb_take(mat_buf_t *b)
{
    if (!b->data) {
        b->data = malloc(1);
        if (!b->data)
            return NULL;
        b->data[0] = '\0';
    }
    return b->data;
}

static char *mt_dup_trimmed_token(const char *start, size_t len)
{
    while (len > 0 && (*start == ' ' || *start == '\t')) {
        start++;
        len--;
    }
    while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '\t'))
        len--;

    char *out = malloc(len + 1);
    if (!out)
        return NULL;
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

static void mt_free_binding_token(mt_binding_token_t *token)
{
    if (!token)
        return;
    free(token->name);
    free(token->value);
    token->name = NULL;
    token->value = NULL;
}

static int mt_binding_contains(char **bindings, size_t nb, const char *token)
{
    for (size_t i = 0; i < nb; ++i) {
        if (strcmp(bindings[i], token) == 0)
            return 1;
    }
    return 0;
}

static void mt_append_binding(char ***bindings,
                              size_t *nbindings,
                              size_t *capbindings,
                              char *token)
{
    if (!token || !*token) {
        free(token);
        return;
    }

    if (mt_binding_contains(*bindings, *nbindings, token)) {
        free(token);
        return;
    }

    if (*nbindings == *capbindings) {
        size_t new_cap = *capbindings ? (*capbindings * 2) : 8;
        char **grown = realloc(*bindings, new_cap * sizeof(**bindings));
        if (!grown) {
            free(token);
            return;
        }
        *bindings = grown;
        *capbindings = new_cap;
    }

    (*bindings)[(*nbindings)++] = token;
}

static void mt_collect_bindings(char ***var_bindings,
                                size_t *nvar_bindings,
                                size_t *capvar_bindings,
                                char ***const_bindings,
                                size_t *nconst_bindings,
                                size_t *capconst_bindings,
                                const char *binding_text)
{
    const char *p = binding_text;
    int in_constants = 0;

    while (p && *p) {
        while (*p == ' ' || *p == '\t' || *p == ',')
            p++;
        if (*p == ';') {
            in_constants = 1;
            p++;
            continue;
        }
        if (!*p)
            break;

        const char *start = p;
        while (*p && *p != ',' && *p != ';')
            p++;

        char *token = mt_dup_trimmed_token(start, (size_t)(p - start));
        if (in_constants)
            mt_append_binding(const_bindings, nconst_bindings, capconst_bindings, token);
        else
            mt_append_binding(var_bindings, nvar_bindings, capvar_bindings, token);
    }
}

static void mt_collect_dval_bindings(const dval_t *dv,
                                     char ***var_bindings,
                                     size_t *nvar_bindings,
                                     size_t *capvar_bindings,
                                     char ***const_bindings,
                                     size_t *nconst_bindings,
                                     size_t *capconst_bindings,
                                     const char *binding_text)
{
    if (dv && dv_is_named_const(dv) &&
        binding_text && *binding_text && !strchr(binding_text, ';')) {
        mt_append_binding(const_bindings, nconst_bindings, capconst_bindings,
                          strdup(binding_text));
        return;
    }

    mt_collect_bindings(var_bindings, nvar_bindings, capvar_bindings,
                        const_bindings, nconst_bindings, capconst_bindings,
                        binding_text);
}

static char *mt_join_binding_list(char **bindings, size_t nbindings)
{
    size_t total = 1;
    char *out;

    for (size_t i = 0; i < nbindings; ++i)
        total += strlen(bindings[i]) + (i ? 2 : 0);

    out = malloc(total);
    if (!out)
        return NULL;
    out[0] = '\0';

    for (size_t i = 0; i < nbindings; ++i) {
        if (i)
            strcat(out, ", ");
        strcat(out, bindings[i]);
    }

    return out;
}

static int mt_parse_binding_token(const char *binding, mt_binding_token_t *out)
{
    const char *eq = strstr(binding, "=");

    out->name = NULL;
    out->value = NULL;
    if (!eq)
        return -1;

    out->name = mt_dup_trimmed_token(binding, (size_t)(eq - binding));
    out->value = mt_dup_trimmed_token(eq + 1, strlen(eq + 1));
    if (!out->name || !out->value) {
        mt_free_binding_token(out);
        return -1;
    }

    return 0;
}

static int mt_has_long_binding(char **bindings, size_t nbindings, size_t threshold)
{
    for (size_t i = 0; i < nbindings; ++i) {
        if (strlen(bindings[i]) > threshold)
            return 1;
    }
    return 0;
}

static int mt_all_bindings_are_nan(char **var_bindings,
                                   size_t nvar_bindings,
                                   char **const_bindings,
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

static char *mt_join_bindings(char **var_bindings,
                              size_t nvar_bindings,
                              char **const_bindings,
                              size_t nconst_bindings,
                              int scientific)
{
    char *vars = NULL;
    char *consts = NULL;
    char *out;
    mat_buf_t b = {0};

    if (nvar_bindings > 0)
        vars = mt_join_binding_list(var_bindings, nvar_bindings);

    if (nconst_bindings > 0) {
        if (scientific) {
            for (size_t i = 0; i < nconst_bindings; ++i) {
                mt_binding_token_t token = {0};
                char value_buf[256];
                number_t value = num_new();

                if (mt_parse_binding_token(const_bindings[i], &token) != 0)
                    continue;
                if (num_set_from_string(&value, token.value) != 0) {
                    snprintf(value_buf, sizeof(value_buf), "%s", token.value);
                } else {
                    num_sprintf(value_buf, sizeof(value_buf), "%N", value);
                }
                if (i > 0)
                    mb_puts(&b, ", ");
                mb_puts(&b, token.name);
                mb_puts(&b, " = ");
                mb_puts(&b, value_buf);
                num_destroy(&value);
                mt_free_binding_token(&token);
            }
            consts = mb_take(&b);
        } else if (!mt_has_long_binding(const_bindings, nconst_bindings, 16) && nvar_bindings == 0) {
            consts = strdup("");
        } else {
            consts = mt_join_binding_list(const_bindings, nconst_bindings);
        }
    } else {
        consts = strdup("");
    }

    if (!vars)
        vars = strdup("");
    if (!vars || !consts) {
        free(vars);
        free(consts);
        free(b.data);
        return NULL;
    }

    if (!*vars && !*consts) {
        free(vars);
        return consts;
    }

    out = malloc(strlen(vars) + strlen(consts) + 4);
    if (!out) {
        free(vars);
        free(consts);
        return NULL;
    }

    out[0] = '\0';
    if (*vars)
        strcat(out, vars);
    if (*consts) {
        if (*vars)
            strcat(out, "; ");
        strcat(out, consts);
    }

    free(vars);
    free(consts);
    return out;
}

static void mt_emit_cells(mat_buf_t *out,
                          char **cells,
                          size_t rows,
                          size_t cols,
                          const size_t *widths,
                          int layout)
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
                for (size_t pad = strlen(cells[idx]); pad < widths[j]; ++pad)
                    mb_putc(out, ' ');
            }
            mb_puts(out, cells[idx] ? cells[idx] : "");
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

static void mt_emit_cells_tex(mat_buf_t *out,
                              char **cells,
                              size_t rows,
                              size_t cols)
{
    mb_puts(out, "\\begin{bmatrix}");
    for (size_t i = 0; i < rows; ++i) {
        if (i > 0)
            mb_puts(out, " \\\\ ");
        for (size_t j = 0; j < cols; ++j) {
            size_t idx = i * cols + j;

            if (j > 0)
                mb_puts(out, " & ");
            mb_puts(out, cells[idx] ? cells[idx] : "");
        }
    }
    mb_puts(out, "\\end{bmatrix}");
}

static int mt_split_dval_repr(const dval_t *dv, char **expr_out, char **bindings_out)
{
    char *tmp;
    char *body;
    char *sep;
    size_t len;

    *expr_out = NULL;
    *bindings_out = NULL;

    if (!dv) {
        *expr_out = strdup("NULL");
        *bindings_out = strdup("");
        return (*expr_out && *bindings_out) ? 0 : -1;
    }

    tmp = dv_to_string(dv, style_EXPRESSION);
    if (!tmp)
        return -1;

    body = tmp;
    len = strlen(tmp);
    if (len >= 4 && tmp[0] == '{' && tmp[1] == ' ' &&
        tmp[len - 2] == ' ' && tmp[len - 1] == '}') {
        body = tmp + 2;
        tmp[len - 2] = '\0';
    }

    sep = strstr(body, " | ");
    if (sep) {
        *sep = '\0';
        *expr_out = strdup(body);
        *bindings_out = strdup(sep + 3);
    } else {
        *expr_out = strdup(body);
        *bindings_out = strdup("");
    }

    free(tmp);
    return (*expr_out && *bindings_out) ? 0 : -1;
}

static void mt_pretty_dval_expr(char **expr_io,
                                char **const_bindings,
                                size_t nconst_bindings)
{
    char *expr;

    if (!expr_io || !*expr_io)
        return;

    expr = *expr_io;
    for (size_t i = 0; i < nconst_bindings; ++i) {
        mt_binding_token_t token = {0};

        if (mt_parse_binding_token(const_bindings[i], &token) != 0)
            continue;

        if (strlen(const_bindings[i]) > 16 && strcmp(expr, token.value) == 0) {
            char *replacement = strdup(token.name);
            if (replacement) {
                free(*expr_io);
                *expr_io = replacement;
            }
            mt_free_binding_token(&token);
            return;
        }

        mt_free_binding_token(&token);
    }
}

static int mt_format_scalar(const matrix_t *A,
                            size_t i,
                            size_t j,
                            int scientific,
                            char *buf,
                            size_t buf_size)
{
    unsigned char *raw;
    int rc;

    raw = calloc(1u, A->elem->size ? A->elem->size : 1u);
    if (!raw)
        return -1;

    mat_get_owned(A, i, j, raw);

    if (!A->elem->format_scalar) {
        mat_value_destroy(A, raw);
        free(raw);
        return -1;
    }

    rc = A->elem->format_scalar(raw, scientific, buf, buf_size);
    mat_value_destroy(A, raw);
    free(raw);
    return rc;
}

static char *mt_texify_binding_list(char **bindings, size_t nbindings)
{
    mat_buf_t out = {0};

    for (size_t i = 0; i < nbindings; ++i) {
        char *tex = dv_tostring_texify(bindings[i]);

        if (i > 0)
            mb_puts(&out, ", ");
        if (tex) {
            mb_puts(&out, tex);
            free(tex);
        } else {
            mb_puts(&out, bindings[i]);
        }
    }

    return mb_take(&out);
}

static char *mt_join_bindings_tex(char **var_bindings,
                                  size_t nvar_bindings,
                                  char **const_bindings,
                                  size_t nconst_bindings)
{
    char *vars = nvar_bindings ? mt_texify_binding_list(var_bindings, nvar_bindings) : strdup("");
    char *consts = nconst_bindings ? mt_texify_binding_list(const_bindings, nconst_bindings) : strdup("");
    char *out;

    if (!vars || !consts) {
        free(vars);
        free(consts);
        return NULL;
    }

    if (!*vars && !*consts) {
        free(vars);
        return consts;
    }

    out = malloc(strlen(vars) + strlen(consts) + 4u);
    if (!out) {
        free(vars);
        free(consts);
        return NULL;
    }

    out[0] = '\0';
    if (*vars)
        strcat(out, vars);
    if (*consts) {
        if (*vars)
            strcat(out, "; ");
        strcat(out, consts);
    }

    free(vars);
    free(consts);
    return out;
}

static char *mat_to_string_numeric(const matrix_t *A, mat_string_style_t style)
{
    mat_buf_t out = {0};
    char **cells = NULL;
    size_t *widths = NULL;
    int tex = (style == MAT_STRING_TEX);
    int layout = (style == MAT_STRING_LAYOUT_SCIENTIFIC ||
                  style == MAT_STRING_LAYOUT_PRETTY);
    int scientific = (style == MAT_STRING_INLINE_SCIENTIFIC ||
                      style == MAT_STRING_LAYOUT_SCIENTIFIC);
    int ok = 1;

    size_t ncell = A->rows * A->cols;

    cells = calloc(ncell ? ncell : 1, sizeof(*cells));
    widths = calloc(A->cols ? A->cols : 1, sizeof(*widths));
    ok = cells && widths;

    for (size_t i = 0; ok && i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            char tmp[1024];
            size_t idx = i * A->cols + j;

            if (mt_format_scalar(A, i, j, scientific, tmp, sizeof(tmp)) != 0) {
                ok = 0;
                break;
            }
            cells[idx] = tex ? dv_tostring_texify(tmp) : strdup(tmp);
            if (!cells[idx]) {
                ok = 0;
                break;
            }
            if (strlen(cells[idx]) > widths[j])
                widths[j] = strlen(cells[idx]);
        }
    }

    if (!ok) {
        free(out.data);
        out.data = NULL;
        goto cleanup;
    }

    if (tex)
        mt_emit_cells_tex(&out, cells, A->rows, A->cols);
    else
        mt_emit_cells(&out, cells, A->rows, A->cols, widths, layout);

cleanup:
    if (cells) {
        for (size_t i = 0; i < A->rows * A->cols; ++i)
            free(cells[i]);
    }
    free(cells);
    free(widths);
    return mb_take(&out);
}

static char *mat_to_string_dval(const matrix_t *A, mat_string_style_t style)
{
    size_t n = A->rows * A->cols;
    char **exprs = calloc(n ? n : 1, sizeof(*exprs));
    char **var_bindings = NULL;
    char **const_bindings = NULL;
    size_t nvar_bindings = 0, capvar_bindings = 0;
    size_t nconst_bindings = 0, capconst_bindings = 0;
    size_t *widths = calloc(A->cols ? A->cols : 1, sizeof(*widths));
    mat_buf_t out = {0};
    int ok = exprs && widths;
    int tex = (style == MAT_STRING_TEX);
    int layout = (style == MAT_STRING_LAYOUT_SCIENTIFIC ||
                  style == MAT_STRING_LAYOUT_PRETTY);
    int scientific = (style == MAT_STRING_INLINE_SCIENTIFIC ||
                      style == MAT_STRING_LAYOUT_SCIENTIFIC);
    int omit_wrapper = 0;

    for (size_t i = 0; ok && i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            char *expr = NULL;
            char *binding_text = NULL;
            dval_t *dv = NULL;
            size_t idx = i * A->cols + j;

            mat_get(A, i, j, &dv);
            if ((tex && dv_to_tex_parts(dv, &expr, &binding_text) != 0) ||
                (!tex && mt_split_dval_repr(dv, &expr, &binding_text) != 0)) {
                free(expr);
                free(binding_text);
                ok = 0;
                break;
            }
            exprs[idx] = expr;
            mt_collect_dval_bindings(dv,
                                     &var_bindings, &nvar_bindings, &capvar_bindings,
                                     &const_bindings, &nconst_bindings, &capconst_bindings,
                                     binding_text);
            if (!tex) {
                mt_pretty_dval_expr(&exprs[idx], const_bindings, nconst_bindings);
            }
            if (strlen(exprs[idx]) > widths[j])
                widths[j] = strlen(exprs[idx]);
            free(binding_text);
        }
    }

    if (!ok) {
        free(out.data);
        out.data = strdup("<dval matrix>");
    } else if (tex) {
        omit_wrapper = mt_all_bindings_are_nan(var_bindings, nvar_bindings,
                                               const_bindings, nconst_bindings);
        char *joined = mt_join_bindings_tex(var_bindings, nvar_bindings,
                                            const_bindings, nconst_bindings);
        if (!omit_wrapper)
            mb_puts(&out, "\\left\\{ ");
        mt_emit_cells_tex(&out, exprs, A->rows, A->cols);
        if (!omit_wrapper && joined && *joined) {
            mb_puts(&out, " \\;\\middle|\\; ");
            mb_puts(&out, joined);
        }
        if (!omit_wrapper)
            mb_puts(&out, " \\right\\}");
        free(joined);
    } else if (!layout) {
        omit_wrapper = mt_all_bindings_are_nan(var_bindings, nvar_bindings,
                                               const_bindings, nconst_bindings);
        char *joined = mt_join_bindings(var_bindings, nvar_bindings,
                                        const_bindings, nconst_bindings,
                                        scientific);
        if (!omit_wrapper)
            mb_puts(&out, "{ ");
        mt_emit_cells(&out, exprs, A->rows, A->cols, widths, 0);
        if (!omit_wrapper && joined && *joined) {
            mb_puts(&out, " | ");
            mb_puts(&out, joined);
        }
        if (!omit_wrapper)
            mb_puts(&out, " }");
        free(joined);
    } else {
        omit_wrapper = mt_all_bindings_are_nan(var_bindings, nvar_bindings,
                                               const_bindings, nconst_bindings);
        char *joined = mt_join_bindings(var_bindings, nvar_bindings,
                                        const_bindings, nconst_bindings,
                                        scientific);
        if (!omit_wrapper)
            mb_puts(&out, "{ ");
        mt_emit_cells(&out, exprs, A->rows, A->cols, widths, 1);
        if (!omit_wrapper && joined && *joined) {
            mb_puts(&out, " | ");
            mb_puts(&out, joined);
        }
        if (!omit_wrapper)
            mb_puts(&out, " }");
        free(joined);
    }

    for (size_t i = 0; i < n; ++i)
        free(exprs[i]);
    for (size_t i = 0; i < nvar_bindings; ++i)
        free(var_bindings[i]);
    for (size_t i = 0; i < nconst_bindings; ++i)
        free(const_bindings[i]);
    free(var_bindings);
    free(const_bindings);
    free(exprs);
    free(widths);
    return mb_take(&out);
}

char *mat_to_string(const matrix_t *A, mat_string_style_t style)
{
    if (!A)
        return strdup("(null)");

    if (matrix_is_symbolic(A))
        return mat_to_string_dval(A, style);
    return mat_to_string_numeric(A, style);
}
