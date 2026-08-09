#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define MARS_MATRIX_INTERNAL_ACCESS
#include "dictionary.h"
#include "matrix_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

typedef struct {
    string_t *name;
    expr_t *expr;
} mat_binding_entry_t;

struct mat_bindings_t {
    size_t count;
    mat_binding_entry_t *entries;
    dictionary_t *index;
};

typedef struct {
    string_t *name;
    bool is_constant;
    bool has_value;
    bool used_in_expr;
    bool owns_symbol;
    number_t value;
    expr_t *symbol;
} matrix_symbol_t;

typedef struct {
    string_t **items;
    size_t count;
    size_t cap;
} text_vec_t;

typedef struct {
    matrix_symbol_t *items;
    size_t count;
    size_t cap;
} symbol_vec_t;

static size_t mf_binding_name_hash(const void *key)
{
    const string_t *name = *(string_t *const *)key;

    return name ? (size_t)string_hash(name) : 0u;
}

static int mf_binding_name_cmp(const void *a, const void *b)
{
    const string_t *ka = *(string_t *const *)a;
    const string_t *kb = *(string_t *const *)b;

    if (!ka && !kb)
        return 0;
    if (!ka)
        return -1;
    if (!kb)
        return 1;
    return string_compare(ka, kb);
}

static dictionary_t *mf_binding_index_create(void)
{
    return dictionary_create(sizeof(string_t *), sizeof(mat_binding_entry_t *), mf_binding_name_hash,
                             mf_binding_name_cmp, NULL, NULL, NULL, NULL, NULL);
}

static void mf_bindings_destroy_partial(mat_bindings_t *bindings)
{
    if (!bindings)
        return;
    dictionary_destroy(bindings->index);
    if (bindings->entries) {
        for (size_t i = 0; i < bindings->count; ++i) {
            expr_free(bindings->entries[i].expr);
            string_free(bindings->entries[i].name);
        }
    }
    free(bindings->entries);
    free(bindings);
}

static mat_bindings_t *mf_bindings_create(size_t count)
{
    mat_bindings_t *bindings = calloc(1, sizeof(*bindings));

    if (!bindings)
        return NULL;

    bindings->entries = calloc(count ? count : 1u, sizeof(bindings->entries[0]));
    bindings->index = mf_binding_index_create();
    if (!bindings->entries || !bindings->index) {
        mf_bindings_destroy_partial(bindings);
        return NULL;
    }

    bindings->count = count;
    return bindings;
}

static int mf_bindings_index_entry(mat_bindings_t *bindings, mat_binding_entry_t *entry)
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

static void mf_report_error(const char *msg)
{
    fprintf(stderr, "mat_from_string: %s\n", msg);
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

static bool mf_rune_is_letter(rune_t rune)
{
    return !rune_is_none(rune) && mf_is_letter(rune_value(rune));
}

static bool mf_rune_is_subscript_digit(rune_t rune)
{
    uint32_t value = rune_value(rune);

    return value >= 0x2080u && value <= 0x2089u;
}

static bool mf_rune_is_name_continuation(rune_t rune)
{
    return rune_is_alpha_numeric(rune) || rune_is_equal(rune, '_') || mf_rune_is_subscript_digit(rune);
}

static int mf_append_subscript_digit(string_t *out, unsigned char digit)
{
    static const char *const subscript_digits[] = {"\xe2\x82\x80", "\xe2\x82\x81", "\xe2\x82\x82", "\xe2\x82\x83",
                                                   "\xe2\x82\x84", "\xe2\x82\x85", "\xe2\x82\x86", "\xe2\x82\x87",
                                                   "\xe2\x82\x88", "\xe2\x82\x89"};

    if (digit < '0' || digit > '9')
        return -1;
    return string_append_cstr(out, subscript_digits[digit - '0']);
}

static string_t *mf_read_bracketed_name_cursor(string_cursor_t *cursor)
{
    string_pos_t start;
    string_pos_t end;

    if (!string_cursor_consume(cursor, "["))
        return NULL;

    start = string_cursor_position(cursor);
    while (!string_cursor_done(cursor) && !rune_is_equal(string_cursor_peek(cursor), ']')) {
        if (string_cursor_next(cursor) != 0)
            return NULL;
    }

    end = string_cursor_position(cursor);
    if (!string_cursor_consume(cursor, "]"))
        return NULL;

    return string_cursor_slice_between(start, end, cursor);
}

static string_t *mf_read_simple_name_cursor(string_cursor_t *cursor)
{
    string_pos_t start = string_cursor_position(cursor);
    string_t *name = NULL;

    if (string_cursor_match(cursor, "pi")) {
        rune_t after = string_cursor_peek_at(cursor, start + 2u);

        if (rune_is_none(after) || (!rune_is_alpha_numeric(after) && !rune_is_equal(after, '_'))) {
            string_cursor_consume(cursor, "pi");
            return string_new_with("\xcf\x80");
        }
    }

    if (string_cursor_consume(cursor, "@")) {
        if (!mf_rune_is_letter(string_cursor_peek(cursor)))
            return NULL;

        while (mf_rune_is_letter(string_cursor_peek(cursor))) {
            if (string_cursor_next(cursor) != 0)
                break;
        }

        name = string_cursor_extract(start, cursor);
        if (name) {
            string_t *normalised = expr_normalise_name_text(name);

            string_free(name);
            name = normalised;
        }
        return name;
    }

    if (!mf_rune_is_letter(string_cursor_peek(cursor)))
        return NULL;

    name = string_new();
    if (!name)
        return NULL;

    if (string_append_rune(name, string_cursor_peek(cursor)) != 0 || string_cursor_next(cursor) != 0)
        goto cleanup;

    while (!string_cursor_done(cursor)) {
        rune_t rune = string_cursor_peek(cursor);
        unsigned char ascii = 0u;

        if (mf_rune_is_subscript_digit(rune)) {
            if (string_append_rune(name, rune) != 0 || string_cursor_next(cursor) != 0)
                goto cleanup;
            continue;
        }

        if (string_cursor_peek_ascii(cursor, &ascii)) {
            if (ascii >= '0' && ascii <= '9') {
                if (mf_append_subscript_digit(name, ascii) != 0 || string_cursor_next(cursor) != 0)
                    goto cleanup;
                continue;
            }

            if (ascii == '_') {
                string_pos_t underscore = string_cursor_position(cursor);
                unsigned char digit = 0u;

                if (string_cursor_next(cursor) != 0 || !string_cursor_peek_ascii(cursor, &digit) || digit < '0' ||
                    digit > '9') {
                    if (string_cursor_seek(cursor, underscore) != 0)
                        goto cleanup;
                    break;
                }
                if (mf_append_subscript_digit(name, digit) != 0 || string_cursor_next(cursor) != 0)
                    goto cleanup;
                continue;
            }
        }

        break;
    }

    return name;

cleanup:
    string_free(name);
    return NULL;
}

static string_t *mf_read_any_name_cursor(string_cursor_t *cursor)
{
    if (rune_is_equal(string_cursor_peek(cursor), '['))
        return mf_read_bracketed_name_cursor(cursor);
    return mf_read_simple_name_cursor(cursor);
}

static int text_vec_push(text_vec_t *v, string_t *item)
{
    if (v->count == v->cap) {
        size_t new_cap = v->cap ? v->cap * 2 : 8;
        string_t **grown = realloc(v->items, new_cap * sizeof(*grown));

        if (!grown)
            return -1;
        v->items = grown;
        v->cap = new_cap;
    }
    v->items[v->count++] = item;
    return 0;
}

static void text_vec_free(text_vec_t *v)
{
    for (size_t i = 0; i < v->count; ++i)
        string_free(v->items[i]);
    free(v->items);
    v->items = NULL;
    v->count = 0;
    v->cap = 0;
}

static ssize_t symbol_vec_find(const symbol_vec_t *v, const string_t *name)
{
    string_view_t needle;

    if (!v || !name)
        return -1;

    needle = string_view_all(name);
    for (size_t i = 0; i < v->count; ++i) {
        if (string_view_equals_view(string_view_all(v->items[i].name), needle))
            return (ssize_t)i;
    }
    return -1;
}

static int symbol_vec_add(symbol_vec_t *v, string_t *name, bool is_constant, bool has_value, number_t value)
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
        string_free(v->items[i].name);
        num_destroy(&v->items[i].value);
        if (v->items[i].owns_symbol && v->items[i].symbol)
            expr_free(v->items[i].symbol);
    }
    free(v->items);
    v->items = NULL;
    v->count = 0;
    v->cap = 0;
}

static bool mf_cursor_matches_pi_name(const string_cursor_t *cursor, bool prev_is_name)
{
    string_pos_t start = string_cursor_position(cursor);
    rune_t after;

    if (prev_is_name || !string_cursor_match(cursor, "pi"))
        return false;

    after = string_cursor_peek_at(cursor, start + 2u);
    return rune_is_none(after) || (!rune_is_alpha_numeric(after) && !rune_is_equal(after, '_'));
}

static string_t *mf_try_read_normalised_alias(string_cursor_t *cursor)
{
    string_cursor_t *probe;
    string_t *out = NULL;

    if (!rune_is_equal(string_cursor_peek(cursor), '@'))
        return NULL;

    probe = string_cursor_clone(cursor);
    if (!probe)
        return NULL;

    out = mf_read_simple_name_cursor(probe);
    if (out) {
        if (out)
            string_cursor_seek(cursor, string_cursor_position(probe));
    }

    string_cursor_free(probe);
    return out;
}

static string_t *mf_normalise_expression_subscripts_text(const string_t *expr)
{
    string_cursor_t *cursor;
    string_t *out;
    bool prev_is_name = false;

    if (!expr)
        return NULL;

    cursor = string_cursor_new(expr);
    out = string_new();
    if (!cursor || !out)
        goto fail;

    while (!string_cursor_done(cursor)) {
        rune_t rune = string_cursor_peek(cursor);
        string_t *alias;

        if (mf_cursor_matches_pi_name(cursor, prev_is_name)) {
            if (!string_cursor_consume(cursor, "pi") || string_append_cstr(out, "\xcf\x80") != 0)
                goto fail;
            prev_is_name = true;
            continue;
        }

        alias = mf_try_read_normalised_alias(cursor);
        if (alias) {
            if (string_append_string(out, alias) != 0) {
                string_free(alias);
                goto fail;
            }
            string_free(alias);
            prev_is_name = true;
            continue;
        }

        if (!prev_is_name && mf_rune_is_letter(rune)) {
            bool converted_digits = false;

            if (string_append_rune(out, rune) != 0 || string_cursor_next(cursor) != 0)
                goto fail;

            while (!string_cursor_done(cursor)) {
                unsigned char digit = 0u;

                if (!string_cursor_peek_ascii(cursor, &digit) || digit < '0' || digit > '9')
                    break;
                if (mf_append_subscript_digit(out, digit) != 0 || string_cursor_next(cursor) != 0)
                    goto fail;
                converted_digits = true;
            }

            prev_is_name = true;
            if (converted_digits)
                continue;
            continue;
        }

        if (string_append_rune(out, rune) != 0)
            goto fail;
        prev_is_name = mf_rune_is_name_continuation(rune);

        if (string_cursor_next(cursor) != 0 && !string_cursor_done(cursor))
            goto fail;
    }

    string_cursor_free(cursor);
    return out;

fail:
    string_cursor_free(cursor);
    string_free(out);
    return NULL;
}

static bool mf_text_has_ascii(const string_t *text, char needle)
{
    string_cursor_t *cursor;
    bool found = false;

    if (!text)
        return false;

    cursor = string_cursor_new(text);
    if (!cursor)
        return false;

    while (!string_cursor_done(cursor)) {
        if (rune_is_equal(string_cursor_peek(cursor), needle)) {
            found = true;
            break;
        }
        if (string_cursor_next(cursor) != 0)
            break;
    }

    string_cursor_free(cursor);
    return found;
}

static int mf_parse_number_literal_text(const string_t *text, number_t *out)
{
    string_cursor_t *cursor;
    string_pos_t content_start;
    string_pos_t split = 0u;
    string_pos_t after_split = 0u;
    string_pos_t end;
    unsigned char prev_non_space = 0u;
    char op = '\0';
    number_t value;

    if (!text || !out)
        return -1;

    value = num_create_from_text(text);
    if (!num_is_nan(value)) {
        *out = value;
        return 0;
    }
    num_destroy(&value);

    cursor = string_cursor_new(text);
    if (!cursor)
        return -1;

    string_cursor_skip_spaces(cursor);
    content_start = string_cursor_position(cursor);

    while (!string_cursor_done(cursor)) {
        string_pos_t pos = string_cursor_position(cursor);
        unsigned char ascii = 0u;

        if (string_cursor_peek_ascii(cursor, &ascii)) {
            if ((ascii == '+' || ascii == '-') && pos != content_start && prev_non_space != 'e' &&
                prev_non_space != 'E') {
                split = pos;
                op = (char)ascii;
                if (string_cursor_next(cursor) != 0)
                    break;
                after_split = string_cursor_position(cursor);
                continue;
            }

            if (!isspace(ascii))
                prev_non_space = ascii;
        }

        if (string_cursor_next(cursor) != 0)
            break;
    }

    end = string_cursor_end_position(cursor);

    if (op) {
        string_t *left = string_cursor_slice_between(content_start, split, cursor);
        string_t *right = string_cursor_slice_between(after_split, end, cursor);

        if (left && right) {
            string_t *rewritten = NULL;

            string_trim(left);
            string_trim(right);

            if (string_length(left) > 0u && string_length(right) > 0u &&
                (mf_text_has_ascii(left, 'i') || mf_text_has_ascii(left, 'j')) && !mf_text_has_ascii(right, 'i') &&
                !mf_text_has_ascii(right, 'j')) {
                rewritten =
                    op == '+' ? string_sprintf("%S + %S", right, left) : string_sprintf("-%S + %S", right, left);
            }

            if (rewritten) {
                value = num_create_from_text(rewritten);
                string_free(rewritten);
                if (!num_is_nan(value)) {
                    string_free(left);
                    string_free(right);
                    string_cursor_free(cursor);
                    *out = value;
                    return 0;
                }
                num_destroy(&value);
            }
        }

        string_free(left);
        string_free(right);
    }

    string_cursor_free(cursor);
    return -1;
}

static bool mf_cursor_at_function_name(const string_cursor_t *cursor)
{
    string_cursor_t *peek = string_cursor_clone(cursor);
    bool result;

    if (!peek)
        return false;

    string_cursor_skip_spaces(peek);
    result = rune_is_equal(string_cursor_peek(peek), '(');
    string_cursor_free(peek);
    return result;
}

static int mf_collect_expression_names_text(const string_t *expr, symbol_vec_t *symbols)
{
    string_cursor_t *cursor;
    int result = -1;

    if (!expr || !symbols)
        return -1;

    cursor = string_cursor_new(expr);
    if (!cursor)
        return -1;

    while (!string_cursor_done(cursor)) {
        string_t *name;

        name = mf_read_any_name_cursor(cursor);
        if (!name) {
            if (string_cursor_next(cursor) != 0)
                break;
            continue;
        }

        if (!mf_cursor_at_function_name(cursor)) {
            ssize_t found = symbol_vec_find(symbols, name);
            number_t default_number = NUM_ZERO;
            bool has_default_value = expr_get_default_constant_num_text(name, &default_number);

            if (found < 0) {
                if (symbol_vec_add(symbols, name, has_default_value || expr_is_default_constant_name_text(name),
                                   has_default_value, default_number) != 0) {
                    string_free(name);
                    num_destroy(&default_number);
                    goto cleanup;
                }
                symbols->items[symbols->count - 1].used_in_expr = true;
            } else {
                num_destroy(&default_number);
                symbols->items[found].used_in_expr = true;
                string_free(name);
            }
        } else {
            string_free(name);
        }
    }

    result = 0;

cleanup:
    string_cursor_free(cursor);
    return result;
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

static bool mf_rune_is_space(rune_t rune)
{
    uint32_t cp = rune_value(rune);

    return cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r' || cp == '\f' || cp == '\v' || cp == 0x0085u ||
           cp == 0x00A0u || cp == 0x1680u || cp == 0x2000u || cp == 0x2001u || cp == 0x2002u || cp == 0x2003u ||
           cp == 0x2004u || cp == 0x2005u || cp == 0x2006u || cp == 0x2007u || cp == 0x2008u || cp == 0x2009u ||
           cp == 0x200Au || cp == 0x2028u || cp == 0x2029u || cp == 0x202Fu || cp == 0x205Fu || cp == 0x3000u;
}

static bool mf_rune_can_end_compact_entry(rune_t rune)
{
    uint32_t cp = rune_value(rune);

    return rune_is_alpha_numeric(rune) || cp == '.' || cp == ')' || cp == ']' || mf_rune_is_subscript_digit(rune);
}

static bool mf_rune_can_start_compact_entry(rune_t rune)
{
    uint32_t cp = rune_value(rune);

    return rune_is_alpha_numeric(rune) || cp == '.' || cp == '[' || cp == '@' || mf_rune_is_subscript_digit(rune);
}

static bool mf_text_token_ends_compact_entry(const string_cursor_t *cursor, string_pos_t start, string_pos_t end)
{
    string_t *token = string_cursor_slice_between(start, end, cursor);
    size_t length;
    bool result;

    if (!token)
        return false;
    string_trim(token);
    length = string_length(token);
    result = length > 0u && mf_rune_can_end_compact_entry(string_at(token, length - 1u));
    string_free(token);
    return result;
}

static bool mf_space_starts_compact_entry(const string_cursor_t *cursor)
{
    string_cursor_t *lookahead = string_cursor_clone(cursor);
    rune_t rune;
    uint32_t cp;
    bool result = false;

    if (!lookahead)
        return false;
    string_cursor_skip_spaces(lookahead);
    rune = string_cursor_peek(lookahead);
    cp = rune_value(rune);
    if (mf_rune_can_start_compact_entry(rune)) {
        result = true;
    } else if (cp == '+' || cp == '-') {
        if (string_cursor_next(lookahead) == 0) {
            rune = string_cursor_peek(lookahead);
            result = !mf_rune_is_space(rune) && mf_rune_can_start_compact_entry(rune);
        }
    }
    string_cursor_free(lookahead);
    return result;
}

static int mf_push_trimmed_text_token(text_vec_t *cells, const string_cursor_t *cursor, string_pos_t start,
                                      string_pos_t end, bool required)
{
    string_t *token = string_cursor_slice_between(start, end, cursor);

    if (!token)
        return required ? -1 : 0;

    string_trim(token);
    if (string_length(token) == 0u) {
        string_free(token);
        return required ? -1 : 0;
    }

    if (text_vec_push(cells, token) != 0) {
        string_free(token);
        return -1;
    }
    return 0;
}

static int mf_finish_paren_text_field(text_vec_t *entries, string_cursor_t *cursor, string_pos_t *token_start,
                                      size_t *current_cols)
{
    string_pos_t end = string_cursor_position(cursor);

    if (mf_push_trimmed_text_token(entries, cursor, *token_start, end, true) != 0)
        return -1;

    (*current_cols)++;
    if (string_cursor_next(cursor) != 0)
        return -1;
    string_cursor_skip_spaces(cursor);
    *token_start = string_cursor_position(cursor);
    return 0;
}

static int mf_finish_compact_text_field(text_vec_t *entries, string_cursor_t *cursor, string_pos_t *token_start,
                                        size_t *current_cols)
{
    string_pos_t end = string_cursor_position(cursor);

    if (mf_push_trimmed_text_token(entries, cursor, *token_start, end, true) != 0)
        return -1;

    (*current_cols)++;
    string_cursor_skip_spaces(cursor);
    *token_start = string_cursor_position(cursor);
    return 0;
}

static int mf_parse_row_text(string_cursor_t *cursor, text_vec_t *cells)
{
    string_pos_t token_start;
    int paren_depth = 0;

    if (!string_cursor_consume(cursor, "["))
        return -1;
    token_start = string_cursor_position(cursor);

    while (!string_cursor_done(cursor)) {
        rune_t rune = string_cursor_peek(cursor);

        if (rune_is_equal(rune, '(')) {
            paren_depth++;
            if (string_cursor_next(cursor) != 0)
                return -1;
            continue;
        }
        if (rune_is_equal(rune, ')')) {
            if (paren_depth > 0)
                paren_depth--;
            if (string_cursor_next(cursor) != 0)
                return -1;
            continue;
        }

        if (paren_depth == 0 && rune_is_equal(rune, ']')) {
            string_pos_t end = string_cursor_position(cursor);

            if (mf_push_trimmed_text_token(cells, cursor, token_start, end, false) != 0)
                return -1;
            return string_cursor_next(cursor) == 0 ? 0 : -1;
        }

        if (paren_depth == 0 && mf_rune_is_space(rune)) {
            string_pos_t end = string_cursor_position(cursor);

            if (mf_push_trimmed_text_token(cells, cursor, token_start, end, false) != 0)
                return -1;
            string_cursor_skip_spaces(cursor);
            token_start = string_cursor_position(cursor);
            continue;
        }

        if (string_cursor_next(cursor) != 0)
            return -1;
    }

    return -1;
}

static int mf_parse_matrix_body_text(const string_t *body, string_t ***entries_out, size_t *rows_out, size_t *cols_out)
{
    string_cursor_t *cursor;
    rune_t first;

    if (!body || !entries_out || !rows_out || !cols_out)
        return -1;

    cursor = string_cursor_new(body);
    if (!cursor)
        return -1;

    string_cursor_skip_spaces(cursor);
    first = string_cursor_peek(cursor);

    if (rune_is_equal(first, '(')) {
        string_pos_t token_start;
        int paren_depth = 0;
        int bracket_depth = 0;
        size_t rows = 0;
        size_t cols = 0;
        size_t current_cols = 0;
        text_vec_t entries = {0};

        if (string_cursor_next(cursor) != 0)
            goto fail_paren;
        token_start = string_cursor_position(cursor);

        while (!string_cursor_done(cursor)) {
            rune_t rune = string_cursor_peek(cursor);

            if (rune_is_equal(rune, '[')) {
                bracket_depth++;
                if (string_cursor_next(cursor) != 0)
                    goto fail_paren;
                continue;
            }
            if (rune_is_equal(rune, ']') && bracket_depth > 0) {
                bracket_depth--;
                if (string_cursor_next(cursor) != 0)
                    goto fail_paren;
                continue;
            }

            if (bracket_depth == 0) {
                if (rune_is_equal(rune, '(')) {
                    paren_depth++;
                    if (string_cursor_next(cursor) != 0)
                        goto fail_paren;
                    continue;
                }
                if (rune_is_equal(rune, ')')) {
                    if (paren_depth > 0) {
                        paren_depth--;
                        if (string_cursor_next(cursor) != 0)
                            goto fail_paren;
                        continue;
                    }
                    if (mf_finish_paren_text_field(&entries, cursor, &token_start, &current_cols) != 0)
                        goto fail_paren;
                    if (mf_commit_paren_row(&rows, &cols, current_cols) != 0)
                        goto fail_paren;
                    string_cursor_skip_spaces(cursor);
                    if (!string_cursor_done(cursor) || rows == 0 || cols == 0)
                        goto fail_paren;

                    string_cursor_free(cursor);
                    *entries_out = entries.items;
                    *rows_out = rows;
                    *cols_out = cols;
                    return 0;
                }
                if (paren_depth == 0 && rune_is_equal(rune, ',')) {
                    if (mf_finish_paren_text_field(&entries, cursor, &token_start, &current_cols) != 0)
                        goto fail_paren;
                    continue;
                }
                if (paren_depth == 0 && rune_is_equal(rune, ';')) {
                    if (mf_finish_paren_text_field(&entries, cursor, &token_start, &current_cols) != 0)
                        goto fail_paren;
                    if (mf_commit_paren_row(&rows, &cols, current_cols) != 0)
                        goto fail_paren;
                    current_cols = 0;
                    continue;
                }
                if (paren_depth == 0 && mf_rune_is_space(rune) &&
                    mf_text_token_ends_compact_entry(cursor, token_start, string_cursor_position(cursor)) &&
                    mf_space_starts_compact_entry(cursor)) {
                    if (mf_finish_compact_text_field(&entries, cursor, &token_start, &current_cols) != 0)
                        goto fail_paren;
                    continue;
                }
            }

            if (string_cursor_next(cursor) != 0)
                goto fail_paren;
        }

    fail_paren:
        string_cursor_free(cursor);
        text_vec_free(&entries);
        return -1;
    }

    {
        size_t rows = 0;
        size_t cols = 0;
        text_vec_t entries = {0};
        text_vec_t row = {0};

        if (!string_cursor_consume(cursor, "[") || !rune_is_equal(string_cursor_peek(cursor), '['))
            goto fail;

        for (;;) {
            row.items = NULL;
            row.count = 0;
            row.cap = 0;

            string_cursor_skip_spaces(cursor);
            if (!rune_is_equal(string_cursor_peek(cursor), '['))
                goto fail_row;
            if (mf_parse_row_text(cursor, &row) != 0)
                goto fail_row;
            if (row.count == 0)
                goto fail_row;

            if (rows == 0)
                cols = row.count;
            else if (row.count != cols)
                goto fail_row;

            for (size_t i = 0; i < row.count; ++i) {
                if (text_vec_push(&entries, row.items[i]) != 0) {
                    row.items[i] = NULL;
                    goto fail_row;
                }
                row.items[i] = NULL;
            }

            free(row.items);
            rows++;

            string_cursor_skip_spaces(cursor);
            if (string_cursor_consume(cursor, "]"))
                break;
            if (!rune_is_equal(string_cursor_peek(cursor), '['))
                goto fail;
        }

        string_cursor_skip_spaces(cursor);
        if (!string_cursor_done(cursor))
            goto fail;

        string_cursor_free(cursor);
        *entries_out = entries.items;
        *rows_out = rows;
        *cols_out = cols;
        return 0;

    fail_row:
        text_vec_free(&row);
    fail:
        string_cursor_free(cursor);
        text_vec_free(&entries);
        return -1;
    }
}

static int mf_parse_binding_section_text(const string_t *text, symbol_vec_t *symbols)
{
    string_cursor_t *cursor;
    bool in_constants = false;
    int result = -1;

    if (!text || !symbols)
        return -1;

    cursor = string_cursor_new(text);
    if (!cursor)
        return -1;

    while (!string_cursor_done(cursor)) {
        string_pos_t value_start;
        string_pos_t value_end;
        string_t *value_text;
        string_t *name;
        ssize_t found;
        int paren_depth = 0;
        number_t value;

        for (;;) {
            string_cursor_skip_spaces(cursor);
            if (!string_cursor_consume(cursor, ","))
                break;
        }

        if (string_cursor_done(cursor))
            break;
        if (string_cursor_consume(cursor, ";")) {
            in_constants = true;
            continue;
        }

        name = mf_read_any_name_cursor(cursor);
        if (!name)
            goto cleanup;

        string_cursor_skip_spaces(cursor);
        if (!string_cursor_consume(cursor, "=")) {
            string_free(name);
            goto cleanup;
        }
        string_cursor_skip_spaces(cursor);

        value_start = string_cursor_position(cursor);
        while (!string_cursor_done(cursor)) {
            rune_t rune = string_cursor_peek(cursor);

            if (rune_is_equal(rune, '('))
                paren_depth++;
            else if (rune_is_equal(rune, ')') && paren_depth > 0)
                paren_depth--;
            else if (paren_depth == 0 && (rune_is_equal(rune, ',') || rune_is_equal(rune, ';')))
                break;
            if (string_cursor_next(cursor) != 0)
                break;
        }

        value_end = string_cursor_position(cursor);
        value_text = string_cursor_slice_between(value_start, value_end, cursor);
        if (!value_text) {
            string_free(name);
            goto cleanup;
        }
        string_trim(value_text);
        if (string_length(value_text) == 0u) {
            string_free(name);
            string_free(value_text);
            goto cleanup;
        }

        if (mf_parse_number_literal_text(value_text, &value) != 0) {
            string_free(name);
            string_free(value_text);
            goto cleanup;
        }
        string_free(value_text);

        found = symbol_vec_find(symbols, name);
        if (found >= 0) {
            if (symbols->items[found].has_value) {
                string_free(name);
                num_destroy(&value);
                goto cleanup;
            }
            if (symbols->items[found].is_constant != in_constants) {
                string_free(name);
                num_destroy(&value);
                goto cleanup;
            }
            symbols->items[found].is_constant = in_constants;
            symbols->items[found].has_value = true;
            num_destroy(&symbols->items[found].value);
            symbols->items[found].value = value;
            if (symbols->items[found].symbol) {
                expr_set_val(symbols->items[found].symbol, value);
            }
            string_free(name);
        } else {
            if (symbol_vec_add(symbols, name, in_constants, true, value) != 0) {
                string_free(name);
                num_destroy(&value);
                goto cleanup;
            }
        }
    }

    result = 0;

cleanup:
    string_cursor_free(cursor);
    return result;
}

static int mf_try_parse_numeric_matrix(string_t **entries, size_t rows, size_t cols, matrix_t **A_out)
{
    size_t n = rows * cols;
    number_t *vals = mf_xmalloc(n * sizeof(*vals));
    matrix_t *A = NULL;

    for (size_t i = 0; i < n; ++i) {
        if (mf_parse_number_literal_text(entries[i], &vals[i]) != 0) {
            for (size_t j = 0; j < i; ++j)
                num_destroy(&vals[j]);
            free(vals);
            return -1;
        }
    }

    A = mat_create(rows, cols, vals);

    for (size_t i = 0; i < n; ++i)
        num_destroy(&vals[i]);
    free(vals);
    *A_out = A;
    return A ? 0 : -1;
}

static int mf_build_symbolic_matrix(string_t **entries, size_t rows, size_t cols, symbol_vec_t *symbols,
                                    mat_bindings_t **bindings_out, matrix_t **A_out)
{
    size_t n = rows * cols;
    size_t active_count = 0;
    expr_t **nodes = calloc(n, sizeof(*nodes));
    string_t **names = NULL;
    expr_t **refs = NULL;
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

        init = symbols->items[i].has_value ? symbols->items[i].value : NUM_NAN;

        if (!symbols->items[i].symbol) {
            symbols->items[i].symbol = symbols->items[i].is_constant
                                           ? expr_new_named_const_text(init, symbols->items[i].name)
                                           : expr_new_named_var_text(init, symbols->items[i].name);
            symbols->items[i].owns_symbol = true;
        } else if (symbols->items[i].has_value) {
            expr_set_val(symbols->items[i].symbol, init);
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
        string_t *normalised = mf_normalise_expression_subscripts_text(entries[i]);

        if (!normalised) {
            ok = 0;
            continue;
        }
        nodes[i] = expr_from_expression_text(normalised, (const string_t *const *)names, refs, active_count);
        string_free(normalised);
        if (!nodes[i])
            ok = 0;
    }

    if (ok)
        A = mat_create_expr(rows, cols, nodes);
    ok = ok && A;

    if (ok && bindings_out) {
        size_t active = 0;

        for (size_t i = 0; i < symbols->count; ++i) {
            if (!symbols->items[i].used_in_expr)
                continue;
            active++;
        }

        bindings = mf_bindings_create(active);
        if (!bindings)
            ok = 0;
        if (ok) {
            for (size_t i = 0, j = 0; i < symbols->count; ++i) {
                mat_binding_entry_t *entry;

                if (!symbols->items[i].used_in_expr)
                    continue;
                entry = &bindings->entries[j];
                entry->name = string_clone(symbols->items[i].name);
                if (!entry->name) {
                    ok = 0;
                    break;
                }
                entry->expr = symbols->items[i].symbol;
                symbols->items[i].owns_symbol = false;
                if (mf_bindings_index_entry(bindings, entry) != 0) {
                    ok = 0;
                    break;
                }
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
            expr_free(nodes[i]);
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

static matrix_t *mf_parse_matrix_text(const string_t *text, mat_bindings_t **bindings_out)
{
    string_cursor_t *cursor = NULL;
    string_t *body = NULL;
    string_t *bindings = NULL;
    string_t **entries = NULL;
    size_t rows = 0;
    size_t cols = 0;
    size_t nentries = 0;
    symbol_vec_t symbols = {0};
    matrix_t *A = NULL;
    const char *error_msg = "invalid matrix string";

    if (bindings_out)
        *bindings_out = NULL;
    if (!text) {
        mf_report_error("NULL input");
        return NULL;
    }

    cursor = string_cursor_new(text);
    if (!cursor) {
        error_msg = "out of memory";
        goto cleanup;
    }

    string_cursor_skip_spaces(cursor);

    if (string_cursor_consume(cursor, "{")) {
        string_pos_t body_start = string_cursor_position(cursor);
        string_pos_t close = 0u;
        string_pos_t pipe = 0u;
        string_pos_t after_pipe = 0u;
        bool has_close = false;
        bool has_pipe = false;
        int depth = 0;

        while (!string_cursor_done(cursor)) {
            rune_t rune = string_cursor_peek(cursor);
            string_pos_t pos = string_cursor_position(cursor);

            if (rune_is_equal(rune, '('))
                depth++;
            else if (rune_is_equal(rune, ')') && depth > 0)
                depth--;
            else if (depth == 0 && !has_pipe && rune_is_equal(rune, '|')) {
                pipe = pos;
                has_pipe = true;
                if (string_cursor_next(cursor) != 0)
                    break;
                after_pipe = string_cursor_position(cursor);
                continue;
            } else if (rune_is_equal(rune, '}')) {
                close = pos;
                has_close = true;
            }

            if (string_cursor_next(cursor) != 0)
                break;
        }

        if (!has_close) {
            string_cursor_free(cursor);
            mf_report_error("missing closing '}'");
            return NULL;
        }

        body = string_cursor_slice_between(body_start, has_pipe ? pipe : close, cursor);
        if (has_pipe)
            bindings = string_cursor_slice_between(after_pipe, close, cursor);
    } else {
        body = string_clone(text);
    }

    string_cursor_free(cursor);
    cursor = NULL;

    if (!body)
        goto cleanup;
    string_trim(body);
    if (bindings)
        string_trim(bindings);

    if (mf_parse_matrix_body_text(body, &entries, &rows, &cols) != 0) {
        error_msg = "invalid matrix body syntax";
        goto cleanup;
    }
    nentries = rows * cols;

    if (mf_try_parse_numeric_matrix(entries, rows, cols, &A) == 0)
        goto cleanup_success;

    if (bindings && string_length(bindings) > 0u) {
        if (mf_parse_binding_section_text(bindings, &symbols) != 0) {
            error_msg = "invalid binding syntax";
            goto cleanup;
        }
    }

    for (size_t i = 0; i < nentries; ++i) {
        if (mf_collect_expression_names_text(entries[i], &symbols) != 0) {
            error_msg = "invalid symbolic name usage";
            goto cleanup;
        }
    }

    if (mf_build_symbolic_matrix(entries, rows, cols, &symbols, bindings_out, &A) != 0) {
        error_msg = "invalid symbolic expression";
        goto cleanup;
    }

cleanup_success:
    string_cursor_free(cursor);
    string_free(body);
    string_free(bindings);
    if (entries) {
        for (size_t i = 0; i < nentries; ++i)
            string_free(entries[i]);
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

expr_t *mat_bindings_get_text(mat_bindings_t *bindings, const string_t *name)
{
    string_t *normalised;
    mat_binding_entry_t *entry = NULL;

    if (!bindings || !name)
        return NULL;

    normalised = expr_normalise_binding_name_text(name);
    if (!normalised)
        return NULL;

    dictionary_get(bindings->index, &normalised, &entry);
    string_free(normalised);
    return entry ? entry->expr : NULL;
}

expr_t *mat_bindings_get(mat_bindings_t *bindings, const char *name)
{
    string_t *text;
    expr_t *expr;

    if (!name)
        return NULL;

    text = string_new_with(name);
    if (!text)
        return NULL;

    expr = mat_bindings_get_text(bindings, text);
    string_free(text);
    return expr;
}

void mat_bindings_free(mat_bindings_t *bindings)
{
    mf_bindings_destroy_partial(bindings);
}

matrix_t *mat_from_string_expr(const char *s, mat_bindings_t **bindings_out)
{
    string_t *text = s ? string_new_with(s) : NULL;
    matrix_t *A;

    if (!text) {
        if (bindings_out)
            *bindings_out = NULL;
        mf_report_error("NULL input");
        return NULL;
    }

    A = mf_parse_matrix_text(text, bindings_out);
    string_free(text);
    return A;
}

matrix_t *mat_from_text_expr(const string_t *text, mat_bindings_t **bindings_out)
{
    return mf_parse_matrix_text(text, bindings_out);
}

matrix_t *mat_from_string(const char *s)
{
    string_t *text = s ? string_new_with(s) : NULL;
    matrix_t *A;

    if (!text) {
        mf_report_error("NULL input");
        return NULL;
    }

    A = mat_from_text(text);
    string_free(text);
    return A;
}

matrix_t *mat_from_text(const string_t *text)
{
    matrix_t *A = NULL;
    matrix_t *evaluated = NULL;
    mat_bindings_t *bindings = NULL;

    A = mf_parse_matrix_text(text, &bindings);
    if (!A)
        return NULL;

    if (mat_typeof(A) == MAT_TYPE_NUMBER) {
        mat_bindings_free(bindings);
        return A;
    }

    if (mat_typeof(A) != MAT_TYPE_EXPR) {
        mat_bindings_free(bindings);
        mat_free(A);
        return NULL;
    }

    for (size_t i = 0; i < bindings->count; ++i) {
        number_t value;

        if (!bindings->entries[i].expr) {
            mat_bindings_free(bindings);
            mat_free(A);
            return NULL;
        }

        value = expr_eval(bindings->entries[i].expr);
        if (num_is_nan(value)) {
            num_destroy(&value);
            mat_bindings_free(bindings);
            mat_free(A);
            return NULL;
        }
        num_destroy(&value);
    }

    evaluated = mat_evaluate(A);
    mat_bindings_free(bindings);
    mat_free(A);
    return evaluated;
}
