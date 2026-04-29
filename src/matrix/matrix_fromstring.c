#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "matrix_internal.h"

typedef struct {
    char *name;
    bool is_constant;
    bool has_value;
    qcomplex_t value;
    dval_t *symbol;
} matrix_symbol_t;

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} string_vec_t;

typedef struct {
    matrix_symbol_t *items;
    size_t count;
    size_t cap;
} symbol_vec_t;

static void *mf_xmalloc(size_t n)
{
    void *p = malloc(n);
    if (!p) {
        fprintf(stderr, "mat_from_string: out of memory\n");
        abort();
    }
    return p;
}

static char *mf_strndup(const char *s, size_t n)
{
    char *out = mf_xmalloc(n + 1);
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static char *mf_strdup(const char *s)
{
    return mf_strndup(s, strlen(s));
}

static void mf_report_error(const char *msg)
{
    fprintf(stderr, "mat_from_string: %s\n", msg);
}

static char *mf_trim_copy(const char *s, size_t n)
{
    while (n > 0 && isspace((unsigned char)*s)) {
        s++;
        n--;
    }
    while (n > 0 && isspace((unsigned char)s[n - 1]))
        n--;
    return mf_strndup(s, n);
}

static int mf_utf8_decode(const char *s, unsigned int *out)
{
    const unsigned char *p = (const unsigned char *)s;

    if (p[0] < 0x80) {
        *out = p[0];
        return 1;
    }
    if ((p[0] & 0xE0) == 0xC0) {
        *out = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
        return 2;
    }
    if ((p[0] & 0xF0) == 0xE0) {
        *out = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        return 3;
    }
    return -1;
}

static int mf_is_letter(unsigned int c)
{
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
        return 1;
    if (c >= 0x0391 && c <= 0x03A9)
        return 1;
    if (c >= 0x03B1 && c <= 0x03C9)
        return 1;
    return 0;
}

static char *mf_read_simple_name(const char **pp)
{
    const char *p = *pp;
    unsigned int c;
    int len = mf_utf8_decode(p, &c);
    char buf[256];
    int blen = 0;

    if (len <= 0 || !mf_is_letter(c))
        return NULL;

    memcpy(buf, p, (size_t)len);
    blen = len;
    p += len;

    for (;;) {
        unsigned int sc;
        int sl = mf_utf8_decode(p, &sc);

        if (sl > 0 && sc >= 0x2080 && sc <= 0x2089) {
            if (blen + sl >= (int)sizeof(buf) - 1)
                break;
            memcpy(buf + blen, p, (size_t)sl);
            blen += sl;
            p += sl;
            continue;
        }
        if ((*p == '_' && p[1] >= '0' && p[1] <= '9') ||
            (*p >= '0' && *p <= '9')) {
            int d;

            if (blen + 3 >= (int)sizeof(buf) - 1)
                break;
            if (*p == '_') {
                d = p[1] - '0';
                p += 2;
            } else {
                d = *p - '0';
                p++;
            }
            buf[blen++] = (char)0xE2;
            buf[blen++] = (char)0x82;
            buf[blen++] = (char)(0x80 + d);
            continue;
        }
        break;
    }

    buf[blen] = '\0';
    *pp = p;
    return mf_strdup(buf);
}

static int string_vec_push(string_vec_t *v, char *item)
{
    if (v->count == v->cap) {
        size_t new_cap = v->cap ? v->cap * 2 : 8;
        char **grown = realloc(v->items, new_cap * sizeof(*grown));

        if (!grown)
            return -1;
        v->items = grown;
        v->cap = new_cap;
    }
    v->items[v->count++] = item;
    return 0;
}

static void string_vec_free(string_vec_t *v)
{
    for (size_t i = 0; i < v->count; ++i)
        free(v->items[i]);
    free(v->items);
    v->items = NULL;
    v->count = 0;
    v->cap = 0;
}

static ssize_t symbol_vec_find(const symbol_vec_t *v, const char *name)
{
    for (size_t i = 0; i < v->count; ++i) {
        if (strcmp(v->items[i].name, name) == 0)
            return (ssize_t)i;
    }
    return -1;
}

static int symbol_vec_add(symbol_vec_t *v,
                          char *name,
                          bool is_constant,
                          bool has_value,
                          qcomplex_t value)
{
    matrix_symbol_t *grown;

    if (symbol_vec_find(v, name) >= 0)
        return -1;

    if (v->count == v->cap) {
        size_t new_cap = v->cap ? v->cap * 2 : 8;

        grown = realloc(v->items, new_cap * sizeof(*grown));
        if (!grown)
            return -1;
        v->items = grown;
        v->cap = new_cap;
    }

    v->items[v->count].name = name;
    v->items[v->count].is_constant = is_constant;
    v->items[v->count].has_value = has_value;
    v->items[v->count].value = value;
    v->items[v->count].symbol = NULL;
    v->count++;
    return 0;
}

static void symbol_vec_free(symbol_vec_t *v)
{
    for (size_t i = 0; i < v->count; ++i) {
        free(v->items[i].name);
        if (v->items[i].symbol)
            dv_free(v->items[i].symbol);
    }
    free(v->items);
    v->items = NULL;
    v->count = 0;
    v->cap = 0;
}

static bool mf_is_default_constant_name(const char *name)
{
    const char *p = name;
    unsigned int c;
    int len;

    if (!name || *name != 'c')
        return false;
    p++;

    len = mf_utf8_decode(p, &c);
    if (len > 0 && c >= 0x2080 && c <= 0x2089)
        return true;
    if (*p == '_' && p[1] >= '0' && p[1] <= '9')
        return true;
    if (*p >= '0' && *p <= '9')
        return true;
    return false;
}

static int mf_is_subscript_utf8(const char *p, int *len_out)
{
    unsigned int c;
    int len = mf_utf8_decode(p, &c);

    if (len > 0 && c >= 0x2080 && c <= 0x2089) {
        if (len_out)
            *len_out = len;
        return 1;
    }
    return 0;
}

static char *mf_normalise_expression_subscripts(const char *expr)
{
    size_t cap = strlen(expr) * 4 + 1;
    char *out = malloc(cap);
    size_t out_len = 0;
    const char *p = expr;

    if (!out)
        return NULL;

    while (*p) {
        unsigned int c;
        int len = mf_utf8_decode(p, &c);

        if (len > 0 && mf_is_letter(c)) {
            const char *q = p + len;
            int prev_is_name = 0;

            if (p > expr) {
                int prev_len = 0;
                prev_is_name = isalnum((unsigned char)p[-1]) || p[-1] == '_'
                            || mf_is_subscript_utf8(p - 3 >= expr ? p - 3 : p - 1,
                                                    &prev_len);
            }

            if (!prev_is_name && q[0] >= '0' && q[0] <= '9') {
                memcpy(out + out_len, p, (size_t)len);
                out_len += (size_t)len;
                while (*q >= '0' && *q <= '9') {
                    int d = *q - '0';
                    out[out_len++] = (char)0xE2;
                    out[out_len++] = (char)0x82;
                    out[out_len++] = (char)(0x80 + d);
                    q++;
                }
                p = q;
                continue;
            }
        }

        if (len > 0) {
            memcpy(out + out_len, p, (size_t)len);
            out_len += (size_t)len;
            p += len;
        } else {
            out[out_len++] = *p++;
        }
    }

    out[out_len] = '\0';
    return out;
}

static int mf_is_function_name(const char *p);

static int mf_entry_requires_symbolic(const char *entry)
{
    const char *p = entry;

    while (*p) {
        char *name = mf_read_simple_name(&p);

        if (!name) {
            p++;
            continue;
        }

        if (!mf_is_function_name(p) &&
            strcmp(name, "i") != 0 &&
            strcmp(name, "j") != 0) {
            free(name);
            return 1;
        }

        free(name);
    }

    return 0;
}

static int mf_is_function_name(const char *p)
{
    while (*p && isspace((unsigned char)*p))
        p++;
    return *p == '(';
}

static int mf_collect_expression_names(const char *expr, symbol_vec_t *symbols)
{
    const char *p = expr;

    while (*p) {
        char *name;

        if (*p == '[') {
            while (*p && *p != ']')
                p++;
            if (*p == ']')
                p++;
            continue;
        }

        name = mf_read_simple_name(&p);
        if (!name) {
            p++;
            continue;
        }

        if (!mf_is_function_name(p) && symbol_vec_find(symbols, name) < 0) {
            if (symbol_vec_add(symbols,
                               name,
                               mf_is_default_constant_name(name),
                               false,
                               qc_make(QF_NAN, QF_NAN)) != 0) {
                free(name);
                return -1;
            }
        } else {
            free(name);
        }
    }

    return 0;
}

static int mf_push_token_from_range(string_vec_t *row, const char *start, const char *end)
{
    char *token = mf_trim_copy(start, (size_t)(end - start));

    if (!token || !*token) {
        free(token);
        return 0;
    }
    return string_vec_push(row, token);
}

static int mf_parse_row(const char **pp, string_vec_t *cells)
{
    const char *p = *pp;
    const char *token_start;
    int paren_depth = 0;

    if (*p != '[')
        return -1;
    p++;
    token_start = p;

    while (*p) {
        if (*p == '(') {
            paren_depth++;
            p++;
            continue;
        }
        if (*p == ')') {
            if (paren_depth > 0)
                paren_depth--;
            p++;
            continue;
        }

        if (paren_depth == 0 && *p == ']') {
            if (mf_push_token_from_range(cells, token_start, p) != 0)
                return -1;
            *pp = p + 1;
            return 0;
        }

        if (paren_depth == 0 && isspace((unsigned char)*p)) {
            if (mf_push_token_from_range(cells, token_start, p) != 0)
                return -1;
            while (*p && isspace((unsigned char)*p))
                p++;
            token_start = p;
            continue;
        }

        p++;
    }

    return -1;
}

static int mf_parse_matrix_body(const char *body,
                                char ***entries_out,
                                size_t *rows_out,
                                size_t *cols_out)
{
    const char *p = body;
    size_t rows = 0;
    size_t cols = 0;
    string_vec_t entries = {0};
    string_vec_t row = {0};

    while (*p && isspace((unsigned char)*p))
        p++;
    if (*p != '[' || p[1] != '[')
        goto fail;
    p++;

    for (;;) {
        row.items = NULL;
        row.count = 0;
        row.cap = 0;

        while (*p && isspace((unsigned char)*p))
            p++;
        if (*p != '[')
            goto fail_row;
        if (mf_parse_row(&p, &row) != 0)
            goto fail_row;
        if (row.count == 0)
            goto fail_row;

        if (rows == 0)
            cols = row.count;
        else if (row.count != cols)
            goto fail_row;

        for (size_t i = 0; i < row.count; ++i) {
            if (string_vec_push(&entries, row.items[i]) != 0) {
                row.items[i] = NULL;
                goto fail_row;
            }
            row.items[i] = NULL;
        }

        free(row.items);
        rows++;

        while (*p && isspace((unsigned char)*p))
            p++;
        if (*p == ']') {
            p++;
            break;
        }
        if (*p != '[')
            goto fail;
    }

    while (*p && isspace((unsigned char)*p))
        p++;
    if (*p != '\0')
        goto fail;

    *entries_out = entries.items;
    *rows_out = rows;
    *cols_out = cols;
    return 0;

fail_row:
    string_vec_free(&row);
fail:
    string_vec_free(&entries);
    return -1;
}

static int mf_parse_binding_section(const char *text, symbol_vec_t *symbols)
{
    const char *p = text;
    bool in_constants = false;

    while (*p) {
        const char *value_start;
        const char *value_end;
        char *name;
        char *value_text;
        ssize_t found;
        int paren_depth = 0;
        qcomplex_t value;

        while (*p && (isspace((unsigned char)*p) || *p == ','))
            p++;
        if (!*p)
            break;
        if (*p == ';') {
            in_constants = true;
            p++;
            continue;
        }

        name = mf_read_simple_name(&p);
        if (!name)
            return -1;

        while (*p && isspace((unsigned char)*p))
            p++;
        if (*p != '=') {
            free(name);
            return -1;
        }
        p++;
        while (*p && isspace((unsigned char)*p))
            p++;

        value_start = p;
        while (*p) {
            if (*p == '(')
                paren_depth++;
            else if (*p == ')' && paren_depth > 0)
                paren_depth--;
            else if (paren_depth == 0 && (*p == ',' || *p == ';'))
                break;
            p++;
        }
        value_end = p;
        value_text = mf_trim_copy(value_start, (size_t)(value_end - value_start));
        if (!value_text || !*value_text) {
            free(name);
            free(value_text);
            return -1;
        }

        value = qc_from_string(value_text);
        free(value_text);
        if (qc_isnan(value)) {
            free(name);
            return -1;
        }

        found = symbol_vec_find(symbols, name);
        if (found >= 0) {
            if (symbols->items[found].has_value) {
                free(name);
                return -1;
            }
            symbols->items[found].is_constant = in_constants;
            symbols->items[found].has_value = true;
            symbols->items[found].value = value;
            free(name);
        } else {
            if (symbol_vec_add(symbols, name, in_constants, true, value) != 0) {
                free(name);
                return -1;
            }
        }
    }

    return 0;
}

static int mf_try_parse_numeric_matrix(char **entries,
                                       size_t rows,
                                       size_t cols,
                                       matrix_t **A_out)
{
    size_t n = rows * cols;
    qcomplex_t *zvals = mf_xmalloc(n * sizeof(*zvals));
    qfloat_t *rvals = mf_xmalloc(n * sizeof(*rvals));
    bool any_complex = false;
    matrix_t *A = NULL;

    for (size_t i = 0; i < n; ++i) {
        if (mf_entry_requires_symbolic(entries[i])) {
            free(zvals);
            free(rvals);
            return -1;
        }
        zvals[i] = qc_from_string(entries[i]);
        if (qc_isnan(zvals[i])) {
            free(zvals);
            free(rvals);
            return -1;
        }
        if (!qf_eq(zvals[i].im, QF_ZERO))
            any_complex = true;
        rvals[i] = zvals[i].re;
    }

    if (any_complex)
        A = mat_create_qc(rows, cols, zvals);
    else
        A = mat_create_qf(rows, cols, rvals);

    free(zvals);
    free(rvals);
    *A_out = A;
    return A ? 0 : -1;
}

static int mf_build_symbolic_matrix(char **entries,
                                    size_t rows,
                                    size_t cols,
                                    symbol_vec_t *symbols,
                                    binding_t **bindings_out,
                                    size_t *number_out,
                                    matrix_t **A_out)
{
    size_t n = rows * cols;
    dval_t **nodes = calloc(n, sizeof(*nodes));
    const char **names = calloc(symbols->count ? symbols->count : 1, sizeof(*names));
    dval_t **refs = calloc(symbols->count ? symbols->count : 1, sizeof(*refs));
    matrix_t *A = NULL;
    binding_t *bindings = NULL;
    int ok = nodes && names && refs;

    for (size_t i = 0; ok && i < symbols->count; ++i) {
        qcomplex_t init = symbols->items[i].has_value
                        ? symbols->items[i].value
                        : qc_make(QF_NAN, QF_NAN);

        symbols->items[i].symbol = symbols->items[i].is_constant
                                 ? dv_new_named_const_qc(init, symbols->items[i].name)
                                 : dv_new_named_var_qc(init, symbols->items[i].name);
        if (!symbols->items[i].symbol)
            ok = 0;
        names[i] = symbols->items[i].name;
        refs[i] = symbols->items[i].symbol;
    }

    for (size_t i = 0; ok && i < n; ++i) {
        char *normalised = mf_normalise_expression_subscripts(entries[i]);

        if (!normalised) {
            ok = 0;
            continue;
        }
        nodes[i] = dval_from_expression_string(normalised, names, refs, symbols->count);
        free(normalised);
        if (!nodes[i])
            ok = 0;
    }

    if (ok)
        A = mat_create_dv(rows, cols, nodes);
    ok = ok && A;

    if (ok && bindings_out) {
        size_t total_names = 0;

        for (size_t i = 0; i < symbols->count; ++i)
            total_names += strlen(symbols->items[i].name) + 1;

        bindings = calloc(1, sizeof(*bindings) * (symbols->count ? symbols->count : 1)
                             + total_names);
        if (!bindings)
            ok = 0;
        if (ok) {
            char *name_store = (char *)(bindings + symbols->count);

            for (size_t i = 0; i < symbols->count; ++i) {
                size_t name_len = strlen(symbols->items[i].name) + 1;

                memcpy(name_store, symbols->items[i].name, name_len);
                bindings[i].name = name_store;
                bindings[i].symbol = symbols->items[i].symbol;
                bindings[i].is_constant = symbols->items[i].is_constant;
                name_store += name_len;
            }
        }
    }

    if (!ok) {
        free(bindings);
        mat_free(A);
        A = NULL;
    }

    for (size_t i = 0; i < n; ++i) {
        if (nodes && nodes[i])
            dv_free(nodes[i]);
    }
    free(nodes);
    free(names);
    free(refs);

    if (!A)
        return -1;

    if (bindings_out)
        *bindings_out = bindings;
    if (number_out)
        *number_out = symbols->count;
    *A_out = A;
    return 0;
}

matrix_t *mat_from_string(const char *s, binding_t **bindings_out, size_t *number_out)
{
    const char *body_start;
    const char *body_end;
    const char *binding_start = NULL;
    const char *binding_end = NULL;
    char *body = NULL;
    char *bindings = NULL;
    char **entries = NULL;
    size_t rows = 0;
    size_t cols = 0;
    size_t nentries = 0;
    symbol_vec_t symbols = {0};
    matrix_t *A = NULL;
    bool wrapped = false;
    const char *error_msg = "invalid matrix string";

    if (bindings_out)
        *bindings_out = NULL;
    if (number_out)
        *number_out = 0;
    if (!s) {
        mf_report_error("NULL input");
        return NULL;
    }

    while (*s && isspace((unsigned char)*s))
        s++;

    if (*s == '{') {
        const char *close = strrchr(s, '}');
        const char *pipe = NULL;
        int depth = 0;
        const char *p = s + 1;

        if (!close)
        {
            mf_report_error("missing closing '}'");
            return NULL;
        }
        wrapped = true;
        while (p < close) {
            if (*p == '(')
                depth++;
            else if (*p == ')' && depth > 0)
                depth--;
            else if (depth == 0 && *p == '|') {
                pipe = p;
                break;
            }
            p++;
        }

        body_start = s + 1;
        body_end = pipe ? pipe : close;
        if (pipe) {
            binding_start = pipe + 1;
            binding_end = close;
        }
    } else {
        body_start = s;
        body_end = s + strlen(s);
    }

    body = mf_trim_copy(body_start, (size_t)(body_end - body_start));
    if (!body)
        goto cleanup;
    if (binding_start && binding_end) {
        bindings = mf_trim_copy(binding_start, (size_t)(binding_end - binding_start));
        if (!bindings)
            goto cleanup;
    }

    if (mf_parse_matrix_body(body, &entries, &rows, &cols) != 0) {
        error_msg = "invalid matrix body syntax";
        goto cleanup;
    }
    nentries = rows * cols;

    if (!wrapped && mf_try_parse_numeric_matrix(entries, rows, cols, &A) == 0)
        goto cleanup_success;

    if (bindings && *bindings) {
        if (mf_parse_binding_section(bindings, &symbols) != 0) {
            error_msg = "invalid binding syntax";
            goto cleanup;
        }
    }

    for (size_t i = 0; i < nentries; ++i) {
        if (mf_collect_expression_names(entries[i], &symbols) != 0) {
            error_msg = "invalid symbolic name usage";
            goto cleanup;
        }
    }

    if (mf_build_symbolic_matrix(entries, rows, cols, &symbols,
                                 bindings_out, number_out, &A) != 0) {
        error_msg = "invalid symbolic expression";
        goto cleanup;
    }

cleanup_success:
    free(body);
    free(bindings);
    if (entries) {
        for (size_t i = 0; i < nentries; ++i)
            free(entries[i]);
    }
    free(entries);
    symbol_vec_free(&symbols);
    return A;

cleanup:
    mf_report_error(error_msg);
    if (bindings_out)
        *bindings_out = NULL;
    if (number_out)
        *number_out = 0;
    mat_free(A);
    A = NULL;
    goto cleanup_success;
}
