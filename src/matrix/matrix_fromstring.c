#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "matrix_internal.h"
#include "dictionary.h"
#include "internal/dval_internal.h"

typedef struct {
    const char *name;
    dval_t *dval;
} mat_binding_entry_t;

struct mat_bindings_t {
    size_t count;
    mat_binding_entry_t *entries;
    dictionary_t *index;
    void *storage;
};

typedef struct {
    char *name;
    bool is_constant;
    bool has_value;
    bool used_in_expr;
    bool owns_symbol;
    number_t value;
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

static size_t mf_binding_name_hash(const void *key)
{
    const unsigned char *s = (const unsigned char *)*(const char * const *)key;
    size_t hash = 1469598103934665603ull;

    while (*s) {
        hash ^= (size_t)*s++;
        hash *= 1099511628211ull;
    }
    return hash;
}

static int mf_binding_name_cmp(const void *a, const void *b)
{
    const char *ka = *(const char * const *)a;
    const char *kb = *(const char * const *)b;

    return strcmp(ka, kb);
}

static dictionary_t *mf_binding_index_create(void)
{
    return dictionary_create(sizeof(char *),
                             sizeof(mat_binding_entry_t *),
                             mf_binding_name_hash,
                             mf_binding_name_cmp,
                             NULL,
                             NULL,
                             NULL,
                             NULL,
                             NULL);
}

static void mf_bindings_destroy_partial(mat_bindings_t *bindings)
{
    if (!bindings)
        return;
    dictionary_destroy(bindings->index);
    free(bindings->storage);
    free(bindings);
}

static mat_bindings_t *mf_bindings_create(size_t count, size_t total_name_bytes)
{
    mat_bindings_t *bindings = calloc(1, sizeof(*bindings));

    if (!bindings)
        return NULL;

    bindings->storage = calloc(1, sizeof(bindings->entries[0]) * count +
                                  total_name_bytes);
    bindings->index = mf_binding_index_create();
    if (!bindings->storage || !bindings->index) {
        mf_bindings_destroy_partial(bindings);
        return NULL;
    }

    bindings->count = count;
    bindings->entries = (mat_binding_entry_t *)bindings->storage;
    return bindings;
}

static int mf_bindings_index_entry(mat_bindings_t *bindings,
                                   mat_binding_entry_t *entry)
{
    return dictionary_set(bindings->index, &entry->name, &entry) ? 0 : -1;
}

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

static char *mf_read_bracketed_name(const char **pp)
{
    const char *p = *pp;
    const char *start;

    if (*p != '[')
        return NULL;
    p++;
    start = p;
    while (*p && *p != ']')
        p++;
    if (*p != ']')
        return NULL;

    *pp = p + 1;
    return mf_strndup(start, (size_t)(p - start));
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

static void mf_skip_spaces(const char **pp)
{
    while (**pp && isspace((unsigned char)**pp))
        (*pp)++;
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
    int had_at = 0;
    int len;
    char buf[256];
    int blen = 0;

    if (strncmp(p, "pi", 2) == 0 &&
        !isalnum((unsigned char)p[2]) && p[2] != '_') {
        *pp = p + 2;
        return mf_strndup("\xcf\x80", 2);
    }

    if (*p == '@') {
        had_at = 1;
        buf[blen++] = *p++;
    }

    len = mf_utf8_decode(p, &c);
    if (len <= 0 || !mf_is_letter(c))
        return NULL;

    memcpy(buf + blen, p, (size_t)len);
    blen += len;
    p += len;

    if (had_at) {
        for (;;) {
            int sl = mf_utf8_decode(p, &c);

            if (sl <= 0 || !mf_is_letter(c))
                break;
            if (blen + sl >= (int)sizeof(buf) - 1)
                break;
            memcpy(buf + blen, p, (size_t)sl);
            blen += sl;
            p += sl;
        }
    }

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
    if (!had_at)
        return mf_strdup(buf);
    return dv_normalize_name(buf);
}

static char *mf_read_any_name(const char **pp)
{
    if (**pp == '[')
        return mf_read_bracketed_name(pp);
    return mf_read_simple_name(pp);
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
                          number_t value)
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
    v->items[v->count].used_in_expr = false;
    v->items[v->count].owns_symbol = false;
    v->items[v->count].value = value;
    v->items[v->count].symbol = NULL;
    v->count++;
    return 0;
}

static void symbol_vec_free(symbol_vec_t *v)
{
    for (size_t i = 0; i < v->count; ++i) {
        free(v->items[i].name);
        num_destroy(&v->items[i].value);
        if (v->items[i].owns_symbol && v->items[i].symbol)
            dv_free(v->items[i].symbol);
    }
    free(v->items);
    v->items = NULL;
    v->count = 0;
    v->cap = 0;
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
        if (strncmp(p, "pi", 2) == 0 &&
            (p == expr || (!isalnum((unsigned char)p[-1]) &&
                           p[-1] != '_' &&
                           p[-1] != '[')) &&
            !isalnum((unsigned char)p[2]) &&
            p[2] != '_') {
            memcpy(out + out_len, "\xcf\x80", 2);
            out_len += 2;
            p += 2;
            continue;
        }

        if (*p == '@') {
            const char *q = p;
            char *name = mf_read_simple_name(&q);

            if (name) {
                size_t n = strlen(name);

                memcpy(out + out_len, name, n);
                out_len += n;
                free(name);
                p = q;
                continue;
            }
        }

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
        char *name = mf_read_any_name(&p);

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

static int mf_parse_number_literal(const char *text, number_t *out)
{
    number_t value;
    qcomplex_t legacy;

    if (!text || !out)
        return -1;

    value = num_new();
    if (num_set_from_string(&value, text) == 0) {
        *out = value;
        return 0;
    }
    num_destroy(&value);

    legacy = qc_from_string(text);
    if (qc_isnan(legacy))
        return -1;

    *out = num_create_from_qcomplex(legacy);
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

        name = mf_read_any_name(&p);
        if (!name) {
            p++;
            continue;
        }

        if (!mf_is_function_name(p)) {
            ssize_t found = symbol_vec_find(symbols, name);
            number_t default_number = num_new();
            bool has_default_value = dv_get_default_constant_num(name, &default_number);

            if (found < 0) {
                if (symbol_vec_add(symbols,
                                   name,
                                   has_default_value || dv_is_default_constant_name(name),
                                   has_default_value,
                                   default_number) != 0) {
                    free(name);
                    num_destroy(&default_number);
                    return -1;
                }
                symbols->items[symbols->count - 1].used_in_expr = true;
            } else {
                num_destroy(&default_number);
                symbols->items[found].used_in_expr = true;
                free(name);
            }
        } else {
            free(name);
        }
    }

    return 0;
}

static int mf_push_trimmed_token(string_vec_t *cells,
                                 const char *start,
                                 const char *end,
                                 bool required)
{
    char *token = mf_trim_copy(start, (size_t)(end - start));

    if (!token || !*token) {
        free(token);
        return required ? -1 : 0;
    }
    return string_vec_push(cells, token);
}

static int mf_commit_paren_row(size_t *rows, size_t *cols, size_t current_cols)
{
    if (*rows == 0)
        *cols = current_cols;
    else if (current_cols != *cols)
        return -1;

    (*rows)++;
    return 0;
}

static int mf_finish_paren_field(string_vec_t *entries,
                                 const char **token_start,
                                 const char **pp,
                                 size_t *current_cols)
{
    if (mf_push_trimmed_token(entries, *token_start, *pp, true) != 0)
        return -1;

    (*current_cols)++;
    (*pp)++;
    mf_skip_spaces(pp);
    *token_start = *pp;
    return 0;
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
            if (mf_push_trimmed_token(cells, token_start, p, false) != 0)
                return -1;
            *pp = p + 1;
            return 0;
        }

        if (paren_depth == 0 && isspace((unsigned char)*p)) {
            if (mf_push_trimmed_token(cells, token_start, p, false) != 0)
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
    if (body[0] == '(') {
        const char *p = body + 1;
        const char *token_start = p;
        int paren_depth = 0;
        int bracket_depth = 0;
        size_t rows = 0;
        size_t cols = 0;
        size_t current_cols = 0;
        string_vec_t entries = {0};

        while (*p) {
            if (*p == '[') {
                bracket_depth++;
                p++;
                continue;
            }
            if (*p == ']' && bracket_depth > 0) {
                bracket_depth--;
                p++;
                continue;
            }

            if (bracket_depth == 0) {
                if (*p == '(') {
                    paren_depth++;
                    p++;
                    continue;
                }
                if (*p == ')') {
                    if (paren_depth > 0) {
                        paren_depth--;
                        p++;
                        continue;
                    }
                    if (mf_finish_paren_field(&entries, &token_start, &p, &current_cols) != 0)
                        goto fail_paren;
                    if (mf_commit_paren_row(&rows, &cols, current_cols) != 0)
                        goto fail_paren;
                    if (*p != '\0' || rows == 0 || cols == 0)
                        goto fail_paren;

                    *entries_out = entries.items;
                    *rows_out = rows;
                    *cols_out = cols;
                    return 0;
                }
                if (paren_depth == 0 && *p == ',') {
                    if (mf_finish_paren_field(&entries, &token_start, &p, &current_cols) != 0)
                        goto fail_paren;
                    continue;
                }
                if (paren_depth == 0 && *p == ';') {
                    if (mf_finish_paren_field(&entries, &token_start, &p, &current_cols) != 0)
                        goto fail_paren;
                    if (mf_commit_paren_row(&rows, &cols, current_cols) != 0)
                        goto fail_paren;
                    current_cols = 0;
                    continue;
                }
            }

            p++;
        }

fail_paren:
        string_vec_free(&entries);
        return -1;
    }

    const char *p = body;
    size_t rows = 0;
    size_t cols = 0;
    string_vec_t entries = {0};
    string_vec_t row = {0};

    mf_skip_spaces(&p);
    if (*p != '[' || p[1] != '[')
        goto fail;
    p++;

    for (;;) {
        row.items = NULL;
        row.count = 0;
        row.cap = 0;

        mf_skip_spaces(&p);
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

        mf_skip_spaces(&p);
        if (*p == ']') {
            p++;
            break;
        }
        if (*p != '[')
            goto fail;
    }

    mf_skip_spaces(&p);
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
        number_t value;

        while (*p && (isspace((unsigned char)*p) || *p == ','))
            p++;
        if (!*p)
            break;
        if (*p == ';') {
            in_constants = true;
            p++;
            continue;
        }

        name = mf_read_any_name(&p);
        if (!name)
            return -1;

        mf_skip_spaces(&p);
        if (*p != '=') {
            free(name);
            return -1;
        }
        p++;
        mf_skip_spaces(&p);

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

        if (mf_parse_number_literal(value_text, &value) != 0) {
            free(name);
            free(value_text);
            return -1;
        }
        free(value_text);

        found = symbol_vec_find(symbols, name);
        if (found >= 0) {
            if (symbols->items[found].has_value) {
                free(name);
                num_destroy(&value);
                return -1;
            }
            if (symbols->items[found].is_constant != in_constants) {
                free(name);
                num_destroy(&value);
                return -1;
            }
            symbols->items[found].is_constant = in_constants;
            symbols->items[found].has_value = true;
            num_destroy(&symbols->items[found].value);
            symbols->items[found].value = value;
            if (symbols->items[found].symbol) {
                dv_set_val_num(symbols->items[found].symbol, value);
            }
            free(name);
        } else {
            if (symbol_vec_add(symbols, name, in_constants, true, value) != 0) {
                free(name);
                num_destroy(&value);
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
    number_t *vals = mf_xmalloc(n * sizeof(*vals));
    matrix_t *A = NULL;

    for (size_t i = 0; i < n; ++i) {
        if (mf_entry_requires_symbolic(entries[i])) {
            for (size_t j = 0; j < i; ++j)
                num_destroy(&vals[j]);
            free(vals);
            return -1;
        }
        if (mf_parse_number_literal(entries[i], &vals[i]) != 0) {
            for (size_t j = 0; j < i; ++j)
                num_destroy(&vals[j]);
            free(vals);
            return -1;
        }
    }

    A = mat_create_num(rows, cols, vals);

    for (size_t i = 0; i < n; ++i)
        num_destroy(&vals[i]);
    free(vals);
    *A_out = A;
    return A ? 0 : -1;
}

static int mf_build_symbolic_matrix(char **entries,
                                    size_t rows,
                                    size_t cols,
                                    symbol_vec_t *symbols,
                                    mat_bindings_t **bindings_out,
                                    matrix_t **A_out)
{
    size_t n = rows * cols;
    size_t active_count = 0;
    dval_t **nodes = calloc(n, sizeof(*nodes));
    const char **names = NULL;
    dval_t **refs = NULL;
    matrix_t *A = NULL;
    mat_bindings_t *bindings = NULL;
    int ok = nodes != NULL;

    for (size_t i = 0; i < symbols->count; ++i) {
        if (symbols->items[i].used_in_expr)
            active_count++;
    }

    names = calloc(active_count ? active_count : 1, sizeof(*names));
    refs = calloc(active_count ? active_count : 1, sizeof(*refs));
    ok = ok && names && refs;

    for (size_t i = 0, active = 0; ok && i < symbols->count; ++i) {
        number_t init;

        if (!symbols->items[i].used_in_expr)
            continue;

        init = symbols->items[i].has_value
             ? symbols->items[i].value
             : num_create_from_qfloat(QF_NAN);

        if (!symbols->items[i].symbol) {
            symbols->items[i].symbol = symbols->items[i].is_constant
                                     ? dv_new_named_const_num(init, symbols->items[i].name)
                                     : dv_new_named_var_num(init, symbols->items[i].name);
            symbols->items[i].owns_symbol = true;
        } else if (symbols->items[i].has_value) {
            dv_set_val_num(symbols->items[i].symbol, init);
        }

        if (!symbols->items[i].has_value)
            num_destroy(&init);

        if (!symbols->items[i].symbol)
            ok = 0;
        names[active] = symbols->items[i].name;
        refs[active] = symbols->items[i].symbol;
        active++;
    }

    for (size_t i = 0; ok && i < n; ++i) {
        char *normalised = mf_normalise_expression_subscripts(entries[i]);

        if (!normalised) {
            ok = 0;
            continue;
        }
        nodes[i] = dval_from_expression_string(normalised, names, refs, active_count);
        free(normalised);
        if (!nodes[i])
            ok = 0;
    }

    if (ok)
        A = mat_create_dv(rows, cols, nodes);
    ok = ok && A;

    if (ok && bindings_out) {
        size_t total_names = 0;
        size_t active = 0;
        char *name_store;

        for (size_t i = 0; i < symbols->count; ++i) {
            if (!symbols->items[i].used_in_expr)
                continue;
            total_names += strlen(symbols->items[i].name) + 1;
            active++;
        }

        bindings = mf_bindings_create(active ? active : 1, total_names);
        if (!bindings)
            ok = 0;
        if (ok) {
            name_store = (char *)(bindings->entries + bindings->count);

            for (size_t i = 0, j = 0; i < symbols->count; ++i) {
                mat_binding_entry_t *entry;
                size_t name_len;

                if (!symbols->items[i].used_in_expr)
                    continue;
                entry = &bindings->entries[j];
                name_len = strlen(symbols->items[i].name) + 1;
                memcpy(name_store, symbols->items[i].name, name_len);
                entry->name = name_store;
                entry->dval = symbols->items[i].symbol;
                if (mf_bindings_index_entry(bindings, entry) != 0) {
                    ok = 0;
                    break;
                }
                name_store += name_len;
                j++;
            }
        }
    }

    if (!ok) {
        mf_bindings_destroy_partial(bindings);
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
    *A_out = A;
    return 0;
}

static matrix_t *mf_parse_matrix_string(const char *s,
                                        mat_bindings_t **bindings_out)
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
    if (!s) {
        mf_report_error("NULL input");
        return NULL;
    }

    mf_skip_spaces(&s);

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

    if (!wrapped &&
        mf_try_parse_numeric_matrix(entries, rows, cols, &A) == 0)
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
                                 bindings_out, &A) != 0) {
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
    mat_free(A);
    A = NULL;
    goto cleanup_success;
}

dval_t *mat_bindings_get(mat_bindings_t *bindings, const char *name)
{
    char *norm;
    mat_binding_entry_t *entry = NULL;

    if (!bindings || !name)
        return NULL;

    norm = dv_normalize_binding_name(name);
    if (!norm)
        return NULL;

    dictionary_get(bindings->index, &norm, &entry);
    free(norm);
    return entry ? entry->dval : NULL;
}

void mat_bindings_free(mat_bindings_t *bindings)
{
    mf_bindings_destroy_partial(bindings);
}

matrix_t *mat_from_string(const char *s, mat_bindings_t **bindings_out)
{
    return mf_parse_matrix_string(s, bindings_out);
}
