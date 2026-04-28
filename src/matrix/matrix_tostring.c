#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "matrix_internal.h"
#include "dval_pattern.h"

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} mat_buf_t;

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

static int mt_qf_to_pretty(char *buf, size_t buf_size, qfloat_t x)
{
    qf_sprintf(buf, buf_size, "%q", x);
    return 0;
}

static int mt_qc_to_pretty(char *buf, size_t buf_size, qcomplex_t z)
{
    char re_buf[512];
    char im_buf[512];
    qfloat_t im_abs = qf_signbit(z.im) ? qf_neg(z.im) : z.im;
    int re_zero = qf_eq(z.re, QF_ZERO);
    int im_zero = qf_eq(z.im, QF_ZERO);
    int im_one = qf_eq(im_abs, QF_ONE);

    if (re_zero && im_zero) {
        snprintf(buf, buf_size, "0");
        return 0;
    }

    if (im_zero) {
        mt_qf_to_pretty(buf, buf_size, z.re);
        return 0;
    }

    if (re_zero) {
        if (im_one)
            snprintf(buf, buf_size, "%si", qf_signbit(z.im) ? "-" : "");
        else {
            mt_qf_to_pretty(im_buf, sizeof(im_buf), im_abs);
            snprintf(buf, buf_size, "%s%si", qf_signbit(z.im) ? "-" : "", im_buf);
        }
        return 0;
    }

    mt_qf_to_pretty(re_buf, sizeof(re_buf), z.re);
    if (buf_size == 0)
        return 0;

    buf[0] = '\0';
    strncat(buf, re_buf, buf_size - 1);
    strncat(buf, qf_signbit(z.im) ? "-" : "+", buf_size - strlen(buf) - 1);
    if (im_one) {
        strncat(buf, "i", buf_size - strlen(buf) - 1);
    } else {
        mt_qf_to_pretty(im_buf, sizeof(im_buf), im_abs);
        strncat(buf, im_buf, buf_size - strlen(buf) - 1);
        strncat(buf, "i", buf_size - strlen(buf) - 1);
    }
    return 0;
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

static int mt_split_binding_token(const char *binding, char **name_out, char **value_out)
{
    const char *eq = strstr(binding, "=");
    char *name;
    char *value;

    *name_out = NULL;
    *value_out = NULL;
    if (!eq)
        return -1;

    name = strndup(binding, (size_t)(eq - binding));
    value = strdup(eq + 1);
    if (!name || !value) {
        free(name);
        free(value);
        return -1;
    }

    while (*name == ' ' || *name == '\t')
        memmove(name, name + 1, strlen(name));
    while (*value == ' ' || *value == '\t')
        memmove(value, value + 1, strlen(value));

    for (size_t len = strlen(name); len > 0; --len) {
        if (name[len - 1] == ' ' || name[len - 1] == '\t')
            name[len - 1] = '\0';
        else
            break;
    }
    for (size_t len = strlen(value); len > 0; --len) {
        if (value[len - 1] == ' ' || value[len - 1] == '\t')
            value[len - 1] = '\0';
        else
            break;
    }

    *name_out = name;
    *value_out = value;
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
                char *name = NULL;
                char *value = NULL;
                char value_buf[256];
                qcomplex_t z;

                if (mt_split_binding_token(const_bindings[i], &name, &value) != 0)
                    continue;
                z = qc_from_string(value);
                if (qc_isnan(z))
                    snprintf(value_buf, sizeof(value_buf), "%s", value);
                else
                    qc_sprintf(value_buf, sizeof(value_buf), "%Z", z);
                if (i > 0)
                    mb_puts(&b, ", ");
                mb_puts(&b, name);
                mb_puts(&b, " = ");
                mb_puts(&b, value_buf);
                free(name);
                free(value);
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
        char *name = NULL;
        char *value = NULL;

        if (mt_split_binding_token(const_bindings[i], &name, &value) != 0)
            continue;

        if (strlen(const_bindings[i]) > 16 && strcmp(expr, value) == 0) {
            char *replacement = strdup(name);
            if (replacement) {
                free(*expr_io);
                *expr_io = replacement;
            }
            free(name);
            free(value);
            return;
        }

        free(name);
        free(value);
    }
}

static int mt_format_scalar(const matrix_t *A,
                            size_t i,
                            size_t j,
                            int scientific,
                            char *buf,
                            size_t buf_size)
{
    unsigned char raw[64] = {0};

    mat_get(A, i, j, raw);
    switch (A->elem->kind) {
    case ELEM_DOUBLE: {
        double x = *(double *)raw;
        if (scientific)
            snprintf(buf, buf_size, "%.16E", x);
        else
            snprintf(buf, buf_size, "%.16g", x);
        return 0;
    }
    case ELEM_QFLOAT: {
        qfloat_t x = *(qfloat_t *)raw;
        qf_sprintf(buf, buf_size, scientific ? "%Q" : "%q", x);
        return 0;
    }
    case ELEM_QCOMPLEX: {
        qcomplex_t z = *(qcomplex_t *)raw;
        if (scientific)
            qc_sprintf(buf, buf_size, "%Z", z);
        else
            mt_qc_to_pretty(buf, buf_size, z);
        return 0;
    }
    default:
        return -1;
    }
}

static char *mat_to_string_numeric(const matrix_t *A, mat_string_style_t style)
{
    mat_buf_t out = {0};
    char **cells = NULL;
    size_t *widths = NULL;
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
            cells[idx] = strdup(tmp);
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

    if (!layout)
        mb_puts(&out, "[");

    for (size_t i = 0; i < A->rows; ++i) {
        if (layout) {
            if (i == 0)
                mb_puts(&out, "[ ");
            else
                mb_puts(&out, "\n  ");
        } else {
            mb_putc(&out, '[');
        }

        for (size_t j = 0; j < A->cols; ++j) {
            size_t idx = i * A->cols + j;
            if (j > 0)
                mb_putc(&out, ' ');
            if (layout) {
                for (size_t pad = strlen(cells[idx]); pad < widths[j]; ++pad)
                    mb_putc(&out, ' ');
            }
            mb_puts(&out, cells[idx]);
        }

        if (layout) {
            if (i + 1 == A->rows)
                mb_puts(&out, " ]");
        } else {
            mb_putc(&out, ']');
        }
    }

    if (!layout)
        mb_putc(&out, ']');

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
    int layout = (style == MAT_STRING_LAYOUT_SCIENTIFIC ||
                  style == MAT_STRING_LAYOUT_PRETTY);
    int scientific = (style == MAT_STRING_INLINE_SCIENTIFIC ||
                      style == MAT_STRING_LAYOUT_SCIENTIFIC);

    for (size_t i = 0; ok && i < A->rows; ++i) {
        for (size_t j = 0; j < A->cols; ++j) {
            char *expr = NULL;
            char *binding_text = NULL;
            dval_t *dv = NULL;
            size_t idx = i * A->cols + j;

            mat_get(A, i, j, &dv);
            if (mt_split_dval_repr(dv, &expr, &binding_text) != 0) {
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
            mt_pretty_dval_expr(&exprs[idx], const_bindings, nconst_bindings);
            if (strlen(exprs[idx]) > widths[j])
                widths[j] = strlen(exprs[idx]);
            free(binding_text);
        }
    }

    if (!ok) {
        free(out.data);
        out.data = strdup("<dval matrix>");
    } else if (!layout) {
        char *joined = mt_join_bindings(var_bindings, nvar_bindings,
                                        const_bindings, nconst_bindings,
                                        scientific);
        mb_puts(&out, "{ [");
        for (size_t i = 0; i < A->rows; ++i) {
            mb_putc(&out, '[');
            for (size_t j = 0; j < A->cols; ++j) {
                size_t idx = i * A->cols + j;
                if (j > 0)
                    mb_putc(&out, ' ');
                mb_puts(&out, exprs[idx] ? exprs[idx] : "");
            }
            mb_putc(&out, ']');
        }
        mb_puts(&out, "]"); 
        if (joined && *joined) {
            mb_puts(&out, " | ");
            mb_puts(&out, joined);
        }
        mb_puts(&out, " }");
        free(joined);
    } else {
        char *joined = mt_join_bindings(var_bindings, nvar_bindings,
                                        const_bindings, nconst_bindings,
                                        scientific);
        mb_puts(&out, "{ [\n");
        for (size_t i = 0; i < A->rows; ++i) {
            mb_puts(&out, "  ");
            for (size_t j = 0; j < A->cols; ++j) {
                size_t idx = i * A->cols + j;
                if (j > 0)
                    mb_putc(&out, ' ');
                for (size_t pad = strlen(exprs[idx]); pad < widths[j]; ++pad)
                    mb_putc(&out, ' ');
                mb_puts(&out, exprs[idx] ? exprs[idx] : "");
            }
            mb_putc(&out, '\n');
        }
        mb_puts(&out, "]");
        if (joined && *joined) {
            mb_puts(&out, " | ");
            mb_puts(&out, joined);
        }
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

    if (A->elem->kind == ELEM_DVAL)
        return mat_to_string_dval(A, style);
    return mat_to_string_numeric(A, style);
}
