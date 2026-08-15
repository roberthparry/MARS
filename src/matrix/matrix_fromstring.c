#include <ctype.h>
#include <limits.h>
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

static int mf_superscript_digit_value(uint32_t value)
{
    if (value == 0x2070u || (value >= 0x2074u && value <= 0x2079u))
        return (int)(value - 0x2070u);
    if (value == 0x00B9u)
        return 1;
    if (value == 0x00B2u)
        return 2;
    if (value == 0x00B3u)
        return 3;
    return -1;
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

static string_t *mf_try_read_plain_greek_name_cursor(string_cursor_t *cursor)
{
    string_cursor_t *probe;
    string_pos_t start;
    string_t *candidate = NULL;
    string_t *normalised = NULL;

    if (!cursor || !mf_rune_is_letter(string_cursor_peek(cursor)))
        return NULL;

    probe = string_cursor_clone(cursor);
    if (!probe)
        return NULL;
    start = string_cursor_position(probe);
    while (!string_cursor_done(probe)) {
        char ascii;

        if (!rune_to_ascii(string_cursor_peek(probe), &ascii) || !isalpha((unsigned char)ascii))
            break;
        if (string_cursor_next(probe) != 0)
            break;
    }

    candidate = string_cursor_extract(start, probe);
    normalised = candidate ? expr_normalise_greek_alias_text(candidate) : NULL;
    if (normalised) {
        string_cursor_t *after = string_cursor_clone(probe);

        if (!after) {
            string_free(normalised);
            normalised = NULL;
        } else {
            string_cursor_skip_spaces(after);
            if (mf_rune_is_name_continuation(string_cursor_peek(probe)) ||
                rune_is_equal(string_cursor_peek(after), '(')) {
                string_free(normalised);
                normalised = NULL;
            } else {
                string_cursor_seek(cursor, string_cursor_position(probe));
            }
            string_cursor_free(after);
        }
    }

    string_free(candidate);
    string_cursor_free(probe);
    return normalised;
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

    name = mf_try_read_plain_greek_name_cursor(cursor);
    if (name)
        return name;

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

        if (!prev_is_name) {
            alias = mf_try_read_plain_greek_name_cursor(cursor);
            if (alias) {
                if (string_append_string(out, alias) != 0) {
                    string_free(alias);
                    goto fail;
                }
                string_free(alias);
                prev_is_name = true;
                continue;
            }
        }

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

static bool mf_consume_function_name_cursor(string_cursor_t *cursor)
{
    string_cursor_t *probe;
    bool is_function = false;

    if (!cursor || !mf_rune_is_letter(string_cursor_peek(cursor)))
        return false;

    probe = string_cursor_clone(cursor);
    if (!probe)
        return false;

    while (!string_cursor_done(probe)) {
        rune_t rune = string_cursor_peek(probe);

        if (!rune_is_alpha_numeric(rune) && !rune_is_equal(rune, '_'))
            break;
        if (string_cursor_next(probe) != 0)
            break;
    }
    string_cursor_skip_spaces(probe);
    if (rune_is_equal(string_cursor_peek(probe), '(')) {
        string_cursor_seek(cursor, string_cursor_position(probe));
        is_function = true;
    }

    string_cursor_free(probe);
    return is_function;
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

        if (mf_consume_function_name_cursor(cursor))
            continue;

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
        number_t value = NUM_NAN;
        bool has_value;

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

        has_value = strcmp(string_c_str(value_text), "?") != 0 && strcasecmp(string_c_str(value_text), "NAN") != 0;
        if (has_value && mf_parse_number_literal_text(value_text, &value) != 0) {
            string_free(name);
            string_free(value_text);
            goto cleanup;
        }
        string_free(value_text);

        found = symbol_vec_find(symbols, name);
        if (found >= 0) {
            if (symbols->items[found].has_value && has_value) {
                string_free(name);
                num_destroy(&value);
                goto cleanup;
            }
            symbols->items[found].is_constant = in_constants;
            symbols->items[found].has_value = has_value;
            num_destroy(&symbols->items[found].value);
            symbols->items[found].value = value;
            if (has_value && symbols->items[found].symbol) {
                expr_set_val(symbols->items[found].symbol, value);
            }
            string_free(name);
        } else {
            if (symbol_vec_add(symbols, name, in_constants, has_value, value) != 0) {
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
        const char *entry_text = string_c_str(entries[i]);

        if (entry_text && (strstr(entry_text, "√") || strstr(entry_text, "sqrt("))) {
            for (size_t j = 0; j < i; ++j)
                num_destroy(&vals[j]);
            free(vals);
            return -1;
        }
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

static bool mf_symbol_is_editable_binding(const matrix_symbol_t *symbol)
{
    number_t value = NUM_ZERO;
    bool is_builtin;

    if (!symbol || !symbol->used_in_expr)
        return false;

    is_builtin = expr_get_default_constant_num_text(symbol->name, &value);
    if (is_builtin)
        num_destroy(&value);
    return !is_builtin;
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
            if (mf_symbol_is_editable_binding(&symbols->items[i]))
                active++;
        }

        bindings = mf_bindings_create(active);
        if (!bindings)
            ok = 0;
        if (ok) {
            for (size_t i = 0, j = 0; i < symbols->count; ++i) {
                mat_binding_entry_t *entry;

                if (!mf_symbol_is_editable_binding(&symbols->items[i]))
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

/* Return the number of bindings discovered by the matrix parser. */
size_t mat_bindings_count(const mat_bindings_t *bindings)
{
    return bindings ? bindings->count : 0u;
}

/* Borrow a discovered matrix binding name by index. */
const char *mat_bindings_name_at(const mat_bindings_t *bindings, size_t index)
{
    if (!bindings || index >= bindings->count || !bindings->entries[index].name)
        return NULL;
    return string_c_str(bindings->entries[index].name);
}

/* Borrow a discovered matrix binding name object by index. */
const string_t *mat_bindings_name_text_at(const mat_bindings_t *bindings, size_t index)
{
    if (!bindings || index >= bindings->count)
        return NULL;
    return bindings->entries[index].name;
}

/* Borrow a discovered matrix binding expression by index. */
expr_t *mat_bindings_expr_at(mat_bindings_t *bindings, size_t index)
{
    if (!bindings || index >= bindings->count)
        return NULL;
    return bindings->entries[index].expr;
}

/* Report whether a discovered matrix binding is a constant. */
bool mat_bindings_is_constant_at(const mat_bindings_t *bindings, size_t index)
{
    expr_t *binding;

    if (!bindings || index >= bindings->count)
        return false;
    binding = bindings->entries[index].expr;
    return binding && !expr_is_variable(binding);
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

static void mf_expr_array_free(expr_t **values, size_t count)
{
    if (!values)
        return;
    for (size_t i = 0u; i < count; ++i)
        expr_free(values[i]);
    free(values);
}

static void mf_number_array_destroy(number_t *values, size_t count)
{
    if (!values)
        return;
    for (size_t i = 0u; i < count; ++i)
        num_destroy(&values[i]);
    free(values);
}

static number_t mf_registered_unary_number(const char *function_name, const number_t value)
{
    expr_t *argument = expr_new_const(value);
    expr_t *mapped = argument ? expr_apply_unary_function(function_name, argument, NULL) : NULL;
    number_t result = mapped ? expr_eval(mapped) : num_clone(NUM_NAN);

    expr_free(mapped);
    expr_free(argument);
    return result;
}

static matrix_t *mf_registered_unary_diagonal(const matrix_t *matrix, const char *function_name)
{
    size_t order = mat_get_row_count(matrix);

    if (mat_typeof(matrix) == MAT_TYPE_EXPR) {
        expr_t **mapped = calloc(order, sizeof(*mapped));
        matrix_t *result = NULL;

        if (!mapped)
            return NULL;
        for (size_t i = 0u; i < order; ++i) {
            expr_t *argument = NULL;

            mat_get(matrix, i, i, &argument);
            mapped[i] = argument ? expr_apply_unary_function(function_name, argument, NULL) : NULL;
            if (!mapped[i]) {
                mf_expr_array_free(mapped, order);
                return NULL;
            }
        }
        result = mat_create_diagonal_expr(order, mapped);
        mf_expr_array_free(mapped, order);
        return result;
    }

    {
        number_t *mapped = calloc(order, sizeof(*mapped));
        matrix_t *result;

        if (!mapped)
            return NULL;
        for (size_t i = 0u; i < order; ++i) {
            number_t argument = mat_get_num(matrix, i, i);

            mapped[i] = mf_registered_unary_number(function_name, argument);
            num_destroy(&argument);
        }
        result = mat_create_diagonal(order, mapped);
        mf_number_array_destroy(mapped, order);
        return result;
    }
}

static matrix_t *mf_registered_unary_symbolic(const matrix_t *matrix, const char *function_name)
{
    size_t order = mat_get_row_count(matrix);
    expr_t **eigenvalues = calloc(order, sizeof(*eigenvalues));
    expr_t **mapped = calloc(order, sizeof(*mapped));
    matrix_t *eigenvectors = NULL;
    matrix_t *diagonal = NULL;
    matrix_t *product = NULL;
    matrix_t *inverse = NULL;
    matrix_t *result = NULL;

    if (!eigenvalues || !mapped)
        goto cleanup;
    if (mat_eigendecompose_expr(matrix, eigenvalues, &eigenvectors) != 0 || !eigenvectors)
        goto cleanup;
    for (size_t i = 0u; i < order; ++i) {
        mapped[i] = expr_apply_unary_function(function_name, eigenvalues[i], NULL);
        if (!mapped[i])
            goto cleanup;
    }
    diagonal = mat_create_diagonal_expr(order, mapped);
    inverse = mat_inverse(eigenvectors);
    product = diagonal ? mat_mul(eigenvectors, diagonal) : NULL;
    result = product && inverse ? mat_mul(product, inverse) : NULL;

cleanup:
    mat_free(inverse);
    mat_free(product);
    mat_free(diagonal);
    mat_free(eigenvectors);
    mf_expr_array_free(mapped, order);
    mf_expr_array_free(eigenvalues, order);
    return result;
}

static matrix_t *mf_registered_unary_numeric(const matrix_t *matrix, const char *function_name)
{
    size_t order = mat_get_row_count(matrix);
    number_t *eigenvalues = calloc(order, sizeof(*eigenvalues));
    number_t *mapped = calloc(order, sizeof(*mapped));
    matrix_t *eigenvectors = NULL;
    matrix_t *diagonal = NULL;
    matrix_t *product = NULL;
    matrix_t *inverse = NULL;
    matrix_t *result = NULL;

    if (!eigenvalues || !mapped)
        goto cleanup;
    if (mat_eigendecompose(matrix, eigenvalues, &eigenvectors) != 0 || !eigenvectors)
        goto cleanup;
    for (size_t i = 0u; i < order; ++i)
        mapped[i] = mf_registered_unary_number(function_name, eigenvalues[i]);
    diagonal = mat_create_diagonal(order, mapped);
    inverse = mat_inverse(eigenvectors);
    product = diagonal ? mat_mul(eigenvectors, diagonal) : NULL;
    result = product && inverse ? mat_mul(product, inverse) : NULL;

cleanup:
    mat_free(inverse);
    mat_free(product);
    mat_free(diagonal);
    mat_free(eigenvectors);
    mf_number_array_destroy(mapped, order);
    mf_number_array_destroy(eigenvalues, order);
    return result;
}

static matrix_t *mf_registered_unary_apply(const matrix_t *matrix, const char *function_name)
{
    if (!matrix || !function_name || mat_get_row_count(matrix) != mat_get_col_count(matrix))
        return NULL;
    if (strcmp(function_name, "sqrt") == 0)
        return mat_sqrt(matrix);
    if (mat_is_diagonal(matrix))
        return mf_registered_unary_diagonal(matrix, function_name);
    if (mat_typeof(matrix) == MAT_TYPE_EXPR)
        return mf_registered_unary_symbolic(matrix, function_name);
    return mf_registered_unary_numeric(matrix, function_name);
}

static char *mf_expression_duplicate_range(const char *start, const char *end)
{
    size_t length;
    char *copy;

    if (!start || !end || end < start)
        return NULL;

    length = (size_t)(end - start);
    copy = malloc(length + 1u);
    if (!copy)
        return NULL;
    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
}

static char *mf_expression_parenthesised_literal(const char *start, const char *end)
{
    size_t length = (size_t)(end - start);
    char *matrix_text = malloc(length + 3u);

    if (!matrix_text)
        return NULL;
    matrix_text[0] = '(';
    memcpy(matrix_text + 1u, start, length);
    matrix_text[length + 1u] = ')';
    matrix_text[length + 2u] = '\0';
    return matrix_text;
}

static char *mf_expression_literal_text(const char *start, const char *end)
{
    while (start < end && isspace((unsigned char)*start))
        start++;
    while (end > start && isspace((unsigned char)end[-1]))
        end--;
    if (start == end)
        return NULL;

    if (*start == '(' || *start == '[' || *start == '{')
        return mf_expression_duplicate_range(start, end);
    return mf_expression_parenthesised_literal(start, end);
}

typedef enum {
    MF_MATRIX_FUNCTION_INVERSE,
    MF_MATRIX_FUNCTION_DETERMINANT,
    MF_MATRIX_FUNCTION_TRACE,
    MF_MATRIX_FUNCTION_TRANSPOSE,
    MF_MATRIX_FUNCTION_HERMITIAN,
} mf_matrix_function_id_t;

typedef enum {
    MF_MATRIX_RESULT_MATRIX,
    MF_MATRIX_RESULT_SCALAR,
} mf_matrix_result_kind_t;

typedef struct {
    const char *spelling;
    const char *canonical_name;
    mf_matrix_function_id_t function;
    mf_matrix_result_kind_t result_kind;
    bool requires_square;
} mf_matrix_function_entry_t;

#define MF_MATRIX_FUNCTION_HASH_SIZE 14u
#define MF_MATRIX_FUNCTION_HASH_SEED 1529u

/* Generated collision-free table for the native matrix-function vocabulary. */
static const mf_matrix_function_entry_t mf_matrix_functions[MF_MATRIX_FUNCTION_HASH_SIZE] = {
    [0]  = {"det",                 "det",       MF_MATRIX_FUNCTION_DETERMINANT, MF_MATRIX_RESULT_SCALAR, true},
    [1]  = {"conjtrans",           "hermitian", MF_MATRIX_FUNCTION_HERMITIAN,   MF_MATRIX_RESULT_MATRIX, false},
    [2]  = {"hermitian",           "hermitian", MF_MATRIX_FUNCTION_HERMITIAN,   MF_MATRIX_RESULT_MATRIX, false},
    [3]  = {"ctranspose",          "hermitian", MF_MATRIX_FUNCTION_HERMITIAN,   MF_MATRIX_RESULT_MATRIX, false},
    [4]  = {"inv",                 "inverse",   MF_MATRIX_FUNCTION_INVERSE,     MF_MATRIX_RESULT_MATRIX, true},
    [5]  = {"inverse",             "inverse",   MF_MATRIX_FUNCTION_INVERSE,     MF_MATRIX_RESULT_MATRIX, true},
    [6]  = {"trace",               "trace",     MF_MATRIX_FUNCTION_TRACE,       MF_MATRIX_RESULT_SCALAR, true},
    [7]  = {"trans",               "transpose", MF_MATRIX_FUNCTION_TRANSPOSE,   MF_MATRIX_RESULT_MATRIX, false},
    [8]  = {"transpose",           "transpose", MF_MATRIX_FUNCTION_TRANSPOSE,   MF_MATRIX_RESULT_MATRIX, false},
    [10] = {"adjoint",             "hermitian", MF_MATRIX_FUNCTION_HERMITIAN,   MF_MATRIX_RESULT_MATRIX, false},
    [11] = {"tr",                  "trace",     MF_MATRIX_FUNCTION_TRACE,       MF_MATRIX_RESULT_SCALAR, true},
    [12] = {"determinant",         "det",       MF_MATRIX_FUNCTION_DETERMINANT, MF_MATRIX_RESULT_SCALAR, true},
    [13] = {"conjugate_transpose", "hermitian", MF_MATRIX_FUNCTION_HERMITIAN,   MF_MATRIX_RESULT_MATRIX, false},
};

static unsigned mf_matrix_function_hash(const char *start, const char *end)
{
    unsigned hash = MF_MATRIX_FUNCTION_HASH_SEED;

    for (const char *cursor = start; cursor < end; ++cursor)
        hash = (hash ^ (unsigned char)*cursor) * 16777619u;
    return hash % MF_MATRIX_FUNCTION_HASH_SIZE;
}

static const mf_matrix_function_entry_t *mf_matrix_function_lookup(const char *start, const char *end)
{
    const mf_matrix_function_entry_t *entry;
    size_t length;

    if (!start || !end || start >= end)
        return NULL;
    entry = &mf_matrix_functions[mf_matrix_function_hash(start, end)];
    if (!entry->spelling)
        return NULL;
    length = (size_t)(end - start);
    return strlen(entry->spelling) == length && memcmp(entry->spelling, start, length) == 0 ? entry : NULL;
}

static bool mf_expression_unary_operation_name(const char *start, const char *open,
                                               const mf_matrix_function_entry_t **matrix_function_out,
                                               char **function_name_out, const char **canonical_name_out)
{
    expr_t *zero = NULL;
    expr_t *probe = NULL;
    bool found;

    *matrix_function_out = mf_matrix_function_lookup(start, open);
    *function_name_out = NULL;
    if (canonical_name_out)
        *canonical_name_out = NULL;
    if (*matrix_function_out)
        return (*matrix_function_out)->result_kind == MF_MATRIX_RESULT_MATRIX;

    *function_name_out = mf_expression_duplicate_range(start, open);
    zero = *function_name_out ? expr_new_const(NUM_ZERO) : NULL;
    probe = zero ? expr_apply_unary_function(*function_name_out, zero, canonical_name_out) : NULL;
    found = probe != NULL;
    expr_free(probe);
    expr_free(zero);
    if (*function_name_out && found)
        return true;

    free(*function_name_out);
    *function_name_out = NULL;
    return false;
}

static bool mf_expression_calculus_name(const char *start, const char *open, bool *integrate_out, char *variable,
                                        size_t variable_size)
{
    const char *variable_start;
    size_t variable_length;

    if (!start || !open || !integrate_out || !variable || variable_size == 0u)
        return false;

    if (open - start > 1 && *start == 'D') {
        *integrate_out = false;
        variable_start = start + 1;
    } else if (open - start > 3 && strncmp(start, "@S^", 3u) == 0) {
        *integrate_out = true;
        variable_start = start + 3;
    } else {
        return false;
    }

    variable_length = (size_t)(open - variable_start);
    if (variable_length == 0u || variable_length >= variable_size)
        return false;
    memcpy(variable, variable_start, variable_length);
    variable[variable_length] = '\0';
    return true;
}

static void mf_expression_trim_span(const char **start, const char **end)
{
    while (*start < *end && isspace((unsigned char)**start))
        (*start)++;
    while (*end > *start && isspace((unsigned char)(*end)[-1]))
        (*end)--;
}

static const char *mf_expression_matching_parenthesis(const char *open, const char *end)
{
    int depth = 0;

    if (!open || open >= end || *open != '(')
        return NULL;

    for (const char *cursor = open; cursor < end; ++cursor) {
        if (*cursor == '(')
            depth++;
        else if (*cursor == ')') {
            depth--;
            if (depth == 0)
                return cursor;
            if (depth < 0)
                return NULL;
        }
    }
    return NULL;
}

static bool mf_expression_parentheses_enclose_matrix_literal(const char *open, const char *close)
{
    int depth = 0;

    if (!open || !close || open >= close || *open != '(' || *close != ')')
        return false;

    for (const char *cursor = open + 1; cursor < close; ++cursor) {
        if (*cursor == '(')
            depth++;
        else if (*cursor == ')')
            depth--;
        else if (depth == 0 && (*cursor == ',' || *cursor == ';'))
            return true;
    }

    return false;
}

static const char *mf_expression_product_operator(const char *start, const char *end)
{
    const char *product = NULL;
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;

    for (const char *cursor = start; cursor < end; ++cursor) {
        unsigned char c = (unsigned char)*cursor;

        if (c == '(')
            paren_depth++;
        else if (c == ')')
            paren_depth--;
        else if (c == '[')
            bracket_depth++;
        else if (c == ']')
            bracket_depth--;
        else if (c == '{')
            brace_depth++;
        else if (c == '}')
            brace_depth--;
        else if (c == '.' && paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
            bool decimal_point = cursor + 1 < end && isdigit((unsigned char)cursor[1]) &&
                                 (cursor == start || isdigit((unsigned char)cursor[-1]) ||
                                  isspace((unsigned char)cursor[-1]) || cursor[-1] == '+' || cursor[-1] == '-');

            if (!decimal_point)
                product = cursor;
        }
    }
    return product;
}

static const char *mf_expression_power_operator(const char *start, const char *end)
{
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;

    if (!start || !end || end <= start)
        return NULL;

    for (const char *cursor = end; cursor-- > start;) {
        unsigned char c = (unsigned char)*cursor;

        if (c == ')')
            paren_depth++;
        else if (c == '(')
            paren_depth--;
        else if (c == ']')
            bracket_depth++;
        else if (c == '[')
            bracket_depth--;
        else if (c == '}')
            brace_depth++;
        else if (c == '{')
            brace_depth--;
        else if (c == '^' && paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
            const char *base_end = cursor;

            while (base_end > start && isspace((unsigned char)base_end[-1]))
                base_end--;
            if (base_end > start && (base_end[-1] == ')' || base_end[-1] == ']' || base_end[-1] == '}'))
                return cursor;
        }
    }

    return NULL;
}

static const char *mf_expression_adjoint_base_end(const char *start, const char *end)
{
    static const char *const suffixes[] = {"^dagger", "^H", "^*", "^†", "†"};

    for (size_t suffix = 0u; suffix < sizeof(suffixes) / sizeof(suffixes[0]); ++suffix) {
        size_t length = strlen(suffixes[suffix]);

        if ((size_t)(end - start) > length && memcmp(end - length, suffixes[suffix], length) == 0)
            return end - length;
    }
    return NULL;
}

static const char *mf_expression_sum_operator(const char *start, const char *end)
{
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;

    if (!start || !end || end <= start)
        return NULL;

    for (const char *cursor = end; cursor-- > start;) {
        unsigned char c = (unsigned char)*cursor;

        if (c == ')')
            paren_depth++;
        else if (c == '(')
            paren_depth--;
        else if (c == ']')
            bracket_depth++;
        else if (c == '[')
            bracket_depth--;
        else if (c == '}')
            brace_depth++;
        else if (c == '{')
            brace_depth--;
        else if ((c == '+' || c == '-') && paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
            const char *left_end = cursor;

            while (left_end > start && isspace((unsigned char)left_end[-1]))
                left_end--;
            if (left_end > start && left_end[-1] != '^' &&
                (left_end[-1] == ')' || left_end[-1] == ']' || left_end[-1] == '}' ||
                 mf_expression_power_operator(start, left_end)))
                return cursor;
        }
    }

    return NULL;
}

static matrix_t *mf_expression_parse_literal(const char *start, const char *end, const char *binding_start,
                                             const char *binding_end, mat_bindings_t **bindings)
{
    char *matrix_text = mf_expression_literal_text(start, end);
    char *wrapped_text = NULL;
    matrix_t *matrix;

    if (!matrix_text)
        return NULL;
    if (binding_start && binding_end && binding_start < binding_end) {
        size_t matrix_length = strlen(matrix_text);
        size_t binding_length = (size_t)(binding_end - binding_start);

        wrapped_text = malloc(matrix_length + binding_length + 8u);
        if (!wrapped_text) {
            free(matrix_text);
            return NULL;
        }
        snprintf(wrapped_text, matrix_length + binding_length + 8u, "{ %s | %.*s }", matrix_text,
                 (int)binding_length, binding_start);
    }
    matrix = mat_from_string_expr(wrapped_text ? wrapped_text : matrix_text, bindings);
    free(wrapped_text);
    free(matrix_text);
    return matrix;
}

static char *mf_expression_with_bindings(const char *start, const char *end, const char *binding_start,
                                         const char *binding_end)
{
    size_t expression_length = (size_t)(end - start);

    if (!binding_start || !binding_end || binding_start >= binding_end)
        return mf_expression_duplicate_range(start, end);

    size_t binding_length = (size_t)(binding_end - binding_start);
    char *wrapped = malloc(expression_length + binding_length + 8u);

    if (!wrapped)
        return NULL;
    snprintf(wrapped, expression_length + binding_length + 8u, "{ %.*s | %.*s }", (int)expression_length, start,
             (int)binding_length, binding_start);
    return wrapped;
}

static mat_bindings_t *mf_bindings_merge_expression(mat_bindings_t *matrix_bindings, expr_bindings_t *expr_bindings)
{
    size_t matrix_count = mat_bindings_count(matrix_bindings);
    size_t expression_count = expr_bindings_count(expr_bindings);
    size_t count = matrix_count;
    mat_bindings_t *merged;
    size_t output = 0u;

    for (size_t i = 0u; i < expression_count; ++i) {
        const char *name = expr_bindings_name_at(expr_bindings, i);

        if (name && !mat_bindings_get(matrix_bindings, name))
            count++;
    }

    if (count == 0u)
        return NULL;
    merged = mf_bindings_create(count);
    if (!merged)
        return NULL;

    for (size_t i = 0u; i < matrix_count; ++i) {
        const char *name = mat_bindings_name_at(matrix_bindings, i);
        expr_t *expr = mat_bindings_expr_at(matrix_bindings, i);
        mat_binding_entry_t *entry = &merged->entries[output++];

        entry->name = name ? string_new_with(name) : NULL;
        entry->expr = expr;
        if (expr)
            expr_retain(expr);
        if (!entry->name || !entry->expr || mf_bindings_index_entry(merged, entry) != 0) {
            mf_bindings_destroy_partial(merged);
            return NULL;
        }
    }

    for (size_t i = 0u; i < expression_count; ++i) {
        const char *name = expr_bindings_name_at(expr_bindings, i);
        expr_t *expr = expr_bindings_expr_at(expr_bindings, i);
        mat_binding_entry_t *entry;

        if (!name || mat_bindings_get(matrix_bindings, name))
            continue;
        entry = &merged->entries[output++];
        entry->name = string_new_with(name);
        entry->expr = expr;
        if (expr)
            expr_retain(expr);
        if (!entry->name || !entry->expr || mf_bindings_index_entry(merged, entry) != 0) {
            mf_bindings_destroy_partial(merged);
            return NULL;
        }
    }

    return merged;
}

static bool mf_expression_bindings_have_unresolved(expr_bindings_t *bindings)
{
    for (size_t i = 0u; i < expr_bindings_count(bindings); ++i) {
        expr_t *binding = expr_bindings_expr_at(bindings, i);
        number_t value;
        bool unresolved;

        if (!binding)
            return true;
        value = expr_get_val(binding);
        unresolved = num_is_nan(value);
        num_destroy(&value);
        if (unresolved)
            return true;
    }
    return false;
}

static matrix_t *mf_expression_evaluate_span(const char *start, const char *end, bool allow_literal,
                                             const char *binding_start, const char *binding_end,
                                             mat_bindings_t **bindings_out, const char **operation_out);

static matrix_t *mf_expression_scalar_identity(const char *start, const char *end, size_t order,
                                               const char *binding_start, const char *binding_end)
{
    matrix_t *coefficient = NULL;
    matrix_t *identity = NULL;

    mf_expression_trim_span(&start, &end);
    if (order == 0u || start == end || end[-1] != 'I')
        return NULL;
    end--;
    while (end > start && isspace((unsigned char)end[-1]))
        end--;
    if (end > start && (end[-1] == '.' || end[-1] == '*')) {
        end--;
        while (end > start && isspace((unsigned char)end[-1]))
            end--;
    }

    if (start == end)
        return mat_create_identity(order);

    coefficient = mf_expression_evaluate_span(start, end, true, binding_start, binding_end, NULL, NULL);
    if (!coefficient || mat_get_row_count(coefficient) != 1u || mat_get_col_count(coefficient) != 1u)
        goto cleanup;

    if (mat_typeof(coefficient) == MAT_TYPE_EXPR) {
        expr_t *scalar = NULL;
        expr_t **diagonal = calloc(order, sizeof(*diagonal));

        mat_get(coefficient, 0u, 0u, &scalar);
        if (!diagonal || !scalar) {
            free(diagonal);
            goto cleanup;
        }
        for (size_t index = 0u; index < order; ++index)
            diagonal[index] = scalar;
        identity = mat_create_diagonal_expr(order, diagonal);
        free(diagonal);
    } else {
        number_t scalar = mat_get_num(coefficient, 0u, 0u);
        number_t *diagonal = calloc(order, sizeof(*diagonal));

        if (!diagonal) {
            num_destroy(&scalar);
            goto cleanup;
        }
        for (size_t index = 0u; index < order; ++index)
            diagonal[index] = num_clone(scalar);
        identity = mat_create_diagonal(order, diagonal);
        for (size_t index = 0u; index < order; ++index)
            num_destroy(&diagonal[index]);
        free(diagonal);
        num_destroy(&scalar);
    }

cleanup:
    mat_free(coefficient);
    return identity;
}

static matrix_t *mf_expression_apply_derivative_sequence(const matrix_t *matrix, mat_bindings_t *bindings,
                                                         const char *suffix)
{
    string_view_t variables;
    string_t *variables_string = NULL;
    char *variables_copy;
    const char *variables_text = NULL;
    size_t variables_length;
    size_t position = 0u;
    matrix_t *current = (matrix_t *)matrix;

    if (!matrix || !suffix)
        return NULL;

    variables_length = strlen(suffix);
    if (variables_length >= 2u && suffix[0] == '[' && suffix[variables_length - 1u] == ']') {
        suffix++;
        variables_length -= 2u;
    }
    if (variables_length == 0u)
        return NULL;

    variables_copy = mf_expression_duplicate_range(suffix, suffix + variables_length);
    variables_string = variables_copy ? string_new_with(variables_copy) : NULL;
    free(variables_copy);
    if (!variables_string)
        return NULL;
    variables_text = string_c_str(variables_string);
    variables = string_view_all(variables_string);
    while (position < variables_length) {
        uint32_t value;
        string_pos_t next_position;
        size_t repeat = 1u;
        char *variable_name;
        expr_t *temporary_variable = NULL;
        expr_t *wrt;

        if (!string_view_peek_rune_value(variables, position, &value, &next_position) || !mf_is_letter(value))
            goto fail;

        variable_name = mf_expression_duplicate_range(variables_text + position, variables_text + next_position);
        if (!variable_name)
            goto fail;
        position = next_position;

        if (position < variables_length) {
            unsigned char marker;

            if (string_view_peek_ascii(variables, position, &marker) && marker == '^') {
                size_t parsed_repeat = 0u;
                bool has_digit = false;

                position++;
                while (position < variables_length && string_view_peek_ascii(variables, position, &marker) && isdigit(marker)) {
                    size_t digit = (size_t)(marker - '0');

                    if (parsed_repeat > (SIZE_MAX - digit) / 10u) {
                        free(variable_name);
                        goto fail;
                    }
                    parsed_repeat = parsed_repeat * 10u + digit;
                    has_digit = true;
                    position++;
                }
                if (!has_digit || parsed_repeat == 0u) {
                    free(variable_name);
                    goto fail;
                }
                repeat = parsed_repeat;
            } else {
                size_t parsed_repeat = 0u;
                bool has_digit = false;

                while (position < variables_length &&
                       string_view_peek_rune_value(variables, position, &value, &next_position)) {
                    int digit = mf_superscript_digit_value(value);

                    if (digit < 0)
                        break;
                    if (parsed_repeat > (SIZE_MAX - (size_t)digit) / 10u) {
                        free(variable_name);
                        goto fail;
                    }
                    parsed_repeat = parsed_repeat * 10u + (size_t)digit;
                    has_digit = true;
                    position = next_position;
                }
                if (has_digit) {
                    if (parsed_repeat == 0u) {
                        free(variable_name);
                        goto fail;
                    }
                    repeat = parsed_repeat;
                }
            }
        }

        wrt = mat_bindings_get(bindings, variable_name);
        if (!wrt) {
            temporary_variable = expr_new_named_var(NUM_ZERO, variable_name);
            wrt = temporary_variable;
        }
        free(variable_name);
        if (!wrt)
            goto fail;

        for (size_t derivative = 0u; derivative < repeat; ++derivative) {
            matrix_t *next = mat_deriv(current, wrt);

            if (current != matrix)
                mat_free(current);
            current = next;
            if (!current) {
                expr_free(temporary_variable);
                goto fail;
            }
        }
        expr_free(temporary_variable);
    }

    string_free(variables_string);
    return current == matrix ? NULL : current;

fail:
    if (current != matrix)
        mat_free(current);
    string_free(variables_string);
    return NULL;
}

static matrix_t *mf_expression_evaluate_span(const char *start, const char *end, bool allow_literal,
                                             const char *binding_start, const char *binding_end,
                                             mat_bindings_t **bindings_out, const char **operation_out)
{
    const char *sum;
    const char *product;
    const char *power;
    const char *adjoint_base_end;
    const char *open;
    const char *close;
    char *unary_function_spelling = NULL;
    char *exponent_text = NULL;
    const char *unary_function_name = NULL;
    const mf_matrix_function_entry_t *matrix_function = NULL;
    char calculus_variable[128];
    bool integrate = false;
    mat_bindings_t *calculus_bindings = NULL;
    mat_bindings_t *power_base_bindings = NULL;
    mat_bindings_t *power_bindings = NULL;
    expr_bindings_t *exponent_bindings = NULL;
    expr_t *exponent_expr = NULL;
    expr_t *temporary_variable = NULL;
    matrix_t *left = NULL;
    matrix_t *right = NULL;
    matrix_t *result = NULL;
    number_t exponent = NUM_NAN;

    mf_expression_trim_span(&start, &end);
    if (start == end)
        return NULL;

    if ((*start == '+' || *start == '-') && start + 1 < end) {
        const char *operand_start = start + 1;

        while (operand_start < end && isspace((unsigned char)*operand_start))
            operand_start++;
        if (operand_start < end && (*operand_start == '(' || isalpha((unsigned char)*operand_start) || *operand_start == '@')) {
            if (operation_out)
                *operation_out = "eval";
            left = mf_expression_evaluate_span(operand_start, end, true, binding_start, binding_end, bindings_out, NULL);
            if (!left) {
                mf_report_error("could not parse unary matrix operand");
                goto cleanup;
            }
            result = *start == '-' ? mat_neg(left) : left;
            if (*start == '+')
                left = NULL;
            goto cleanup;
        }
    }

    if (allow_literal && *start == '(') {
        close = mf_expression_matching_parenthesis(start, end);
        if (close && close + 1 == end && !mf_expression_parentheses_enclose_matrix_literal(start, close))
            return mf_expression_evaluate_span(start + 1, close, true, binding_start, binding_end, bindings_out,
                                               operation_out);
    }

    sum = mf_expression_sum_operator(start, end);
    if (sum) {
        if (operation_out)
            *operation_out = "eval";
        left = mf_expression_evaluate_span(start, sum, true, binding_start, binding_end, NULL, NULL);
        if (left && mat_get_row_count(left) == mat_get_col_count(left))
            right = mf_expression_scalar_identity(sum + 1, end, mat_get_row_count(left), binding_start, binding_end);
        if (!right)
            right = mf_expression_evaluate_span(sum + 1, end, true, binding_start, binding_end, NULL, NULL);
        if (!left || !right) {
            mf_report_error("could not parse matrix addition operand");
            goto cleanup;
        }
        if (mat_get_row_count(left) != mat_get_row_count(right) || mat_get_col_count(left) != mat_get_col_count(right)) {
            fprintf(stderr, "mat_from_string: matrix addition requires matching dimensions; received %zux%zu and %zux%zu\n",
                    mat_get_row_count(left), mat_get_col_count(left), mat_get_row_count(right), mat_get_col_count(right));
            goto cleanup;
        }
        result = *sum == '+' ? mat_add(left, right) : mat_sub(left, right);
        if (result && bindings_out)
            *bindings_out = mat_bindings_from_matrix(result);
        goto cleanup;
    }

    product = mf_expression_product_operator(start, end);
    if (product) {
        if (operation_out)
            *operation_out = "multiply";
        left = mf_expression_evaluate_span(start, product, true, binding_start, binding_end, NULL, NULL);
        right = mf_expression_evaluate_span(product + 1, end, true, binding_start, binding_end, NULL, NULL);
        if (!left || !right) {
            mf_report_error("could not parse matrix multiplication operand");
            goto cleanup;
        }
        if (mat_get_col_count(left) != mat_get_row_count(right)) {
            fprintf(stderr, "mat_from_string: matrix multiplication requires matching inner dimensions; received %zux%zu and "
                            "%zux%zu\n",
                    mat_get_row_count(left), mat_get_col_count(left), mat_get_row_count(right), mat_get_col_count(right));
            goto cleanup;
        }
        result = mat_mul(left, right);
        goto cleanup;
    }

    adjoint_base_end = mf_expression_adjoint_base_end(start, end);
    if (adjoint_base_end) {
        if (operation_out)
            *operation_out = "hermitian";
        left = mf_expression_evaluate_span(start, adjoint_base_end, true, binding_start, binding_end, bindings_out, NULL);
        if (!left) {
            mf_report_error("could not parse Hermitian-adjoint operand");
            goto cleanup;
        }
        result = mat_hermitian(left);
        goto cleanup;
    }

    power = mf_expression_power_operator(start, end);
    if (power) {
        long numerator;
        long denominator;

        if (operation_out)
            *operation_out = "power";
        left = mf_expression_evaluate_span(start, power, true, binding_start, binding_end, &power_base_bindings, NULL);
        if (!left) {
            mf_report_error("could not parse matrix power base");
            goto cleanup;
        }
        if (mat_get_row_count(left) != mat_get_col_count(left)) {
            fprintf(stderr, "mat_from_string: matrix power requires a square matrix; received %zux%zu\n",
                    mat_get_row_count(left), mat_get_col_count(left));
            goto cleanup;
        }

        exponent_text = mf_expression_with_bindings(power + 1, end, binding_start, binding_end);
        exponent_expr = exponent_text ? expr_from_string(exponent_text, &exponent_bindings) : NULL;
        if (!exponent_expr) {
            mf_report_error("could not parse matrix exponent");
            goto cleanup;
        }
        power_bindings = mf_bindings_merge_expression(power_base_bindings, exponent_bindings);
        if ((mat_bindings_count(power_base_bindings) != 0u || expr_bindings_count(exponent_bindings) != 0u) && !power_bindings) {
            mf_report_error("could not retain matrix power bindings");
            goto cleanup;
        }
        if (bindings_out) {
            *bindings_out = power_bindings;
            power_bindings = NULL;
        }

        exponent = expr_eval(exponent_expr);
        if (!num_is_finite(exponent)) {
            if (mf_expression_bindings_have_unresolved(exponent_bindings))
                result = mat_pow_expr(left, exponent_expr);
            else
                mf_report_error("matrix exponent did not evaluate to a finite number");
            goto cleanup;
        }

        if (num_get_small_rational(exponent, &numerator, &denominator)) {
            if (denominator == 1L && numerator >= INT_MIN && numerator <= INT_MAX)
                result = mat_pow_int(left, (int)numerator);
            else
                result = mat_pow_expr(left, exponent_expr);
        }
        if (!result)
            result = mat_pow(left, &exponent);
        if (!result)
            mf_report_error("could not calculate matrix power");
        goto cleanup;
    }

    open = memchr(start, '(', (size_t)(end - start));
    if (open && mf_expression_calculus_name(start, open, &integrate, calculus_variable, sizeof(calculus_variable))) {
        if (operation_out)
            *operation_out = "eval";
        close = mf_expression_matching_parenthesis(open, end);
        if (!close || close + 1 != end) {
            mf_report_error("could not parse matrix calculus input");
            return NULL;
        }
        left = mf_expression_evaluate_span(open + 1, close, true, binding_start, binding_end, &calculus_bindings, NULL);
        if (left) {
            if (integrate) {
                expr_t *wrt = mat_bindings_get(calculus_bindings, calculus_variable);

                if (!wrt) {
                    temporary_variable = expr_new_named_var(NUM_ZERO, calculus_variable);
                    wrt = temporary_variable;
                }
                if (wrt)
                    result = mat_integrate_family(left, wrt);
            } else {
                result = mf_expression_apply_derivative_sequence(left, calculus_bindings, calculus_variable);
            }
        }
        if (!result)
            fprintf(stderr, "mat_from_string: could not %s matrix entries with respect to %s\n",
                    integrate ? "integrate" : "differentiate", calculus_variable);
        if (result && bindings_out) {
            if (integrate) {
                *bindings_out = mat_bindings_from_matrix(result);
            } else {
                *bindings_out = calculus_bindings;
                calculus_bindings = NULL;
            }
        }
        goto cleanup;
    }

    if (open && mf_expression_unary_operation_name(start, open, &matrix_function, &unary_function_spelling,
                                                    &unary_function_name)) {
        const char *unary_operation = matrix_function ? matrix_function->canonical_name : unary_function_name;

        if (operation_out)
            *operation_out = unary_operation;
        close = mf_expression_matching_parenthesis(open, end);
        if (!close || close + 1 != end) {
            mf_report_error("could not parse matrix function input");
            return NULL;
        }

        left = mf_expression_evaluate_span(open + 1, close, true, binding_start, binding_end, bindings_out, NULL);
        if (!left) {
            mf_report_error("could not parse matrix function argument");
            goto cleanup;
        }
        if ((!matrix_function || matrix_function->requires_square) &&
            mat_get_row_count(left) != mat_get_col_count(left)) {
            fprintf(stderr, "mat_from_string: matrix function %s requires a square matrix; received %zux%zu\n", unary_operation,
                    mat_get_row_count(left), mat_get_col_count(left));
            goto cleanup;
        }
        if (!matrix_function)
            result = mf_registered_unary_apply(left, unary_function_spelling);
        else if (matrix_function->function == MF_MATRIX_FUNCTION_INVERSE)
            result = mat_inverse(left);
        else if (matrix_function->function == MF_MATRIX_FUNCTION_TRANSPOSE)
            result = mat_transpose(left);
        else if (matrix_function->function == MF_MATRIX_FUNCTION_HERMITIAN)
            result = mat_hermitian(left);
        goto cleanup;
    }

    if (allow_literal)
        return mf_expression_parse_literal(start, end, binding_start, binding_end, bindings_out);

cleanup:
    num_destroy(&exponent);
    expr_free(exponent_expr);
    expr_bindings_free(exponent_bindings);
    free(exponent_text);
    expr_free(temporary_variable);
    mat_bindings_free(calculus_bindings);
    mat_bindings_free(power_bindings);
    mat_bindings_free(power_base_bindings);
    free(unary_function_spelling);
    mat_free(right);
    mat_free(left);
    return result;
}

/* Parse and evaluate a complete matrix expression supplied as a C string. */
matrix_t *mat_expression_from_string(const char *text, mat_bindings_t **bindings_out, const char **operation_out)
{
    const char *start;
    const char *end;
    const char *body_start;
    const char *body_end;
    const char *binding_start = NULL;
    const char *binding_end = NULL;

    if (bindings_out)
        *bindings_out = NULL;
    if (operation_out)
        *operation_out = NULL;
    if (!text)
        return NULL;

    start = text;
    end = text + strlen(text);
    mf_expression_trim_span(&start, &end);
    body_start = start;
    body_end = end;
    if (start < end && *start == '{' && end[-1] == '}') {
        int paren_depth = 0;
        int bracket_depth = 0;
        int brace_depth = 0;

        for (const char *cursor = start + 1; cursor < end - 1; ++cursor) {
            if (*cursor == '(')
                paren_depth++;
            else if (*cursor == ')')
                paren_depth--;
            else if (*cursor == '[')
                bracket_depth++;
            else if (*cursor == ']')
                bracket_depth--;
            else if (*cursor == '{')
                brace_depth++;
            else if (*cursor == '}')
                brace_depth--;
            else if (*cursor == '|' && paren_depth == 0 && bracket_depth == 0 && brace_depth == 0 &&
                     memchr(cursor + 1, '=', (size_t)((end - 1) - (cursor + 1))) != NULL) {
                body_start = start + 1;
                body_end = cursor;
                binding_start = cursor + 1;
                binding_end = end - 1;
            }
        }
    }
    mf_expression_trim_span(&body_start, &body_end);
    if (binding_start)
        mf_expression_trim_span(&binding_start, &binding_end);
    return mf_expression_evaluate_span(body_start, body_end, true, binding_start, binding_end, bindings_out, operation_out);
}

static char *mf_scalar_matrix_function_operand_text(const char *text,
                                                    const mf_matrix_function_entry_t **function_out)
{
    const char *start = text;
    const char *end;
    const char *open;
    const char *name_end;
    int depth = 0;

    if (!text)
        return NULL;
    *function_out = NULL;
    end = text + strlen(text);
    mf_expression_trim_span(&start, &end);
    if (start < end && *start == '{' && end[-1] == '}') {
        int paren_depth = 0;
        int bracket_depth = 0;
        int brace_depth = 0;
        const char *binding_separator = NULL;

        for (const char *cursor = start + 1; cursor < end - 1; ++cursor) {
            if (*cursor == '(')
                paren_depth++;
            else if (*cursor == ')')
                paren_depth--;
            else if (*cursor == '[')
                bracket_depth++;
            else if (*cursor == ']')
                bracket_depth--;
            else if (*cursor == '{')
                brace_depth++;
            else if (*cursor == '}')
                brace_depth--;
            else if (*cursor == '|' && paren_depth == 0 && bracket_depth == 0 && brace_depth == 0 &&
                     memchr(cursor + 1, '=', (size_t)((end - 1) - (cursor + 1))) != NULL)
                binding_separator = cursor;
        }
        if (binding_separator) {
            char *body = mf_expression_duplicate_range(start + 1, binding_separator);
            char *operand = body ? mf_scalar_matrix_function_operand_text(body, function_out) : NULL;
            char *bound_operand = NULL;

            free(body);
            if (operand) {
                size_t binding_length = (size_t)((end - 1) - (binding_separator + 1));
                size_t required = strlen(operand) + binding_length + 8u;

                bound_operand = malloc(required);
                if (bound_operand)
                    snprintf(bound_operand, required, "{ %s | %.*s }", operand, (int)binding_length,
                             binding_separator + 1);
                free(operand);
            }
            return bound_operand;
        }
    }
    if (end - start >= 4 && start[0] == '|' && start[1] == '|' && end[-2] == '|' && end[-1] == '|') {
        *function_out = mf_matrix_function_lookup("det", "det" + 3u);
        return mf_expression_duplicate_range(start + 2, end - 2);
    }
    if ((size_t)(end - start) >= 6u && memcmp(start, "‖", 3u) == 0 && memcmp(end - 3, "‖", 3u) == 0) {
        *function_out = mf_matrix_function_lookup("det", "det" + 3u);
        return mf_expression_duplicate_range(start + 3, end - 3);
    }
    if (end - start >= 2 && *start == '|' && end[-1] == '|') {
        *function_out = mf_matrix_function_lookup("det", "det" + 3u);
        return mf_expression_duplicate_range(start + 1, end - 1);
    }

    open = memchr(start, '(', (size_t)(end - start));
    if (!open)
        return NULL;
    name_end = open;
    while (name_end > start && isspace((unsigned char)name_end[-1]))
        name_end--;
    *function_out = mf_matrix_function_lookup(start, name_end);
    if (!*function_out || (*function_out)->result_kind != MF_MATRIX_RESULT_SCALAR)
        return NULL;

    if (open >= end || *open != '(' || end[-1] != ')')
        return NULL;
    for (const char *cursor = open; cursor < end; ++cursor) {
        if (*cursor == '(')
            depth++;
        else if (*cursor == ')' && --depth == 0 && cursor != end - 1)
            return NULL;
        if (depth < 0)
            return NULL;
    }
    return depth == 0 ? mf_expression_duplicate_range(open + 1, end - 1) : NULL;
}

static bool mf_outer_determinant_delimiters_are_malformed(const char *text)
{
    const char *start = text;
    const char *end;
    bool starts;
    bool ends;

    if (!text)
        return false;
    end = text + strlen(text);
    mf_expression_trim_span(&start, &end);

    starts = end - start >= 2 && start[0] == '|' && start[1] == '|';
    ends = end - start >= 2 && end[-2] == '|' && end[-1] == '|';
    if (starts || ends)
        return starts != ends;

    starts = (size_t)(end - start) >= 3u && memcmp(start, "‖", 3u) == 0;
    ends = (size_t)(end - start) >= 3u && memcmp(end - 3, "‖", 3u) == 0;
    if (starts || ends)
        return starts != ends;

    starts = start < end && *start == '|';
    ends = start < end && end[-1] == '|';
    return starts != ends;
}

/* Parse a matrix-language expression and preserve whether its result is a matrix or a scalar. */
int mat_expression_evaluate(const char *text, mat_bindings_t **bindings_out, const char **operation_out,
                            matrix_t **matrix_out, expr_t **scalar_out)
{
    const mf_matrix_function_entry_t *scalar_function = NULL;
    char *scalar_operand;
    matrix_t *matrix;
    expr_t *scalar = NULL;

    if (bindings_out)
        *bindings_out = NULL;
    if (operation_out)
        *operation_out = NULL;
    if (!matrix_out || !scalar_out)
        return -1;
    *matrix_out = NULL;
    *scalar_out = NULL;

    if (mf_outer_determinant_delimiters_are_malformed(text)) {
        mf_report_error("determinant bars must have matching opening and closing delimiters");
        return -1;
    }

    scalar_operand = mf_scalar_matrix_function_operand_text(text, &scalar_function);
    if (!scalar_operand) {
        *matrix_out = mat_expression_from_string(text, bindings_out, operation_out);
        return *matrix_out ? 0 : -1;
    }

    if (operation_out)
        *operation_out = scalar_function->canonical_name;
    matrix = mat_expression_from_string(scalar_operand, bindings_out, NULL);
    free(scalar_operand);
    if (!matrix)
        return -1;
    if (scalar_function->requires_square && mat_get_row_count(matrix) != mat_get_col_count(matrix)) {
        mat_free(matrix);
        return -1;
    }
    if (mat_typeof(matrix) == MAT_TYPE_EXPR) {
        if (scalar_function->function == MF_MATRIX_FUNCTION_DETERMINANT) {
            if (mat_det_expr(matrix, &scalar) != 0)
                scalar = NULL;
        } else if (scalar_function->function == MF_MATRIX_FUNCTION_TRACE) {
            if (mat_trace_expr(matrix, &scalar) != 0)
                scalar = NULL;
        }
    } else {
        number_t value = num_new();

        if ((scalar_function->function == MF_MATRIX_FUNCTION_DETERMINANT && mat_det(matrix, &value) == 0) ||
            (scalar_function->function == MF_MATRIX_FUNCTION_TRACE && mat_trace(matrix, &value) == 0))
            scalar = expr_new_const(value);
        num_destroy(&value);
    }
    mat_free(matrix);
    if (!scalar) {
        if (bindings_out) {
            mat_bindings_free(*bindings_out);
            *bindings_out = NULL;
        }
        return -1;
    }
    *scalar_out = scalar;
    return 0;
}

/* Discover all editable symbolic bindings referenced by a matrix result. */
mat_bindings_t *mat_bindings_from_matrix(const matrix_t *A)
{
    expr_bindings_t *merged = NULL;
    expr_t **entries = NULL;
    mat_bindings_t *bindings = NULL;
    size_t entry_count;
    size_t binding_count;

    if (!A || mat_typeof(A) != MAT_TYPE_EXPR)
        return NULL;

    entry_count = mat_get_row_count(A) * mat_get_col_count(A);
    entries = calloc(entry_count ? entry_count : 1u, sizeof(*entries));
    if (!entries)
        return NULL;
    mat_get_data_expr(A, entries);

    for (size_t i = 0u; i < entry_count; ++i) {
        expr_bindings_t *entry_bindings = expr_bindings_from_expr_internal(entries[i]);
        expr_bindings_t *next;

        if (!entry_bindings)
            continue;
        next = expr_bindings_merge_internal(merged, entry_bindings);
        expr_bindings_free(entry_bindings);
        if (!next) {
            expr_bindings_free(merged);
            free(entries);
            return NULL;
        }
        expr_bindings_free(merged);
        merged = next;
    }
    free(entries);

    binding_count = expr_bindings_count(merged);
    if (binding_count == 0u) {
        expr_bindings_free(merged);
        return NULL;
    }

    bindings = mf_bindings_create(binding_count);
    if (!bindings) {
        expr_bindings_free(merged);
        return NULL;
    }
    for (size_t i = 0u; i < binding_count; ++i) {
        mat_binding_entry_t *entry = &bindings->entries[i];

        entry->name = string_clone(expr_bindings_name_text_at(merged, i));
        entry->expr = expr_bindings_expr_at(merged, i);
        if (!entry->name || !entry->expr) {
            mf_bindings_destroy_partial(bindings);
            expr_bindings_free(merged);
            return NULL;
        }
        expr_retain(entry->expr);
        if (mf_bindings_index_entry(bindings, entry) != 0) {
            mf_bindings_destroy_partial(bindings);
            expr_bindings_free(merged);
            return NULL;
        }
    }

    expr_bindings_free(merged);
    return bindings;
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
