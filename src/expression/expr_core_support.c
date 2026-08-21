#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "number.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#include "expression.h"

typedef struct expr_default_constant_alias {
    const char *key;
    const char *value;
} expr_default_constant_alias_t;

enum {
    EXPR_DEFAULT_ALIAS_HASH_SIZE = 13,
    EXPR_DEFAULT_ALIAS_HASH_SEED = 864u,
    EXPR_DEFAULT_ALIAS_HASH_MUL = 2654435761u
};

static const expr_default_constant_alias_t s_default_constant_aliases[EXPR_DEFAULT_ALIAS_HASH_SIZE] = {
    [0] = {"π", "@pi"},  [2] = {"@gamma", "@gamma"}, [3] = {"@phi", "@phi"},     [4] = {"τ", "@tau"},
    [5] = {"pi", "@pi"}, [6] = {"φ", "@phi"},        [7] = {"@tau", "@tau"},     [8] = {"@pi", "@pi"},
    [9] = {"i", "i"},    [10] = {"phi", "@phi"},     [11] = {"gamma", "@gamma"}, [12] = {"γ", "@gamma"},
};

static size_t expr_default_constant_alias_hash(const string_t *name)
{
    string_cursor_t *cursor;
    uint32_t hash = EXPR_DEFAULT_ALIAS_HASH_SEED;

    if (!name)
        return 0;

    cursor = string_cursor_new(name);
    if (!cursor)
        return 0;

    while (!string_cursor_done(cursor)) {
        hash *= EXPR_DEFAULT_ALIAS_HASH_MUL;
        hash ^= rune_value(string_cursor_peek(cursor));
        string_cursor_next(cursor);
    }

    string_cursor_free(cursor);
    return hash % EXPR_DEFAULT_ALIAS_HASH_SIZE;
}

static const char *expr_default_constant_alias_literal(const string_t *name)
{
    string_view_t view;
    const expr_default_constant_alias_t *entry;

    if (!name)
        return NULL;

    entry = &s_default_constant_aliases[expr_default_constant_alias_hash(name)];
    if (!entry->key)
        return NULL;

    view = string_view_all(name);
    return string_view_equals_literal(view, entry->key) ? entry->value : NULL;
}

static const char *expr_lookup_default_constant_alias(const char *name)
{
    string_t *text;
    const char *mapped;

    if (!name)
        return NULL;

    text = string_new_with(name);
    if (!text)
        return name;

    mapped = expr_default_constant_alias_literal(text);
    string_free(text);

    return mapped ? mapped : name;
}

void expr_store_const_num(expr_t *dv, number_t value)
{
    num_destroy(&dv->c);
    dv->c = num_scope_detach(value);
}

void expr_store_value_num(expr_t *dv, number_t value)
{
    num_destroy(&dv->x);
    dv->x = num_scope_detach(value);
}

bool expr_ops_is_lambert(const expr_ops_t *ops)
{
    return ops == &ops_lambert_w || ops == &ops_lambert_wn || ops == &ops_lambert_w0 || ops == &ops_lambert_wm1;
}

const expr_t *expr_lambert_arg(const expr_t *expr)
{
    if (!expr || !expr_ops_is_lambert(expr->ops))
        return NULL;
    return expr_is_op(expr, &ops_lambert_wn) ? expr->b : expr->a;
}

bool expr_ops_is_floor_or_ceil(const expr_ops_t *ops)
{
    return ops == &ops_floor || ops == &ops_ceil;
}

bool expr_ops_are_direct_inverse_pair(const expr_ops_t *outer, const expr_ops_t *inner)
{
    return outer && inner && outer->direct_inverse == inner;
}

const expr_ops_t *expr_ops_reciprocal_unary(const expr_ops_t *ops)
{
    static const expr_ops_t *reciprocal_unary_ops[EXPR_KIND_COUNT] = {
        [EXPR_KIND_COS] = &ops_sec,   [EXPR_KIND_SIN] = &ops_cosec,   [EXPR_KIND_TAN] = &ops_cot,
        [EXPR_KIND_SEC] = &ops_cos,   [EXPR_KIND_COSEC] = &ops_sin,   [EXPR_KIND_COT] = &ops_tan,
        [EXPR_KIND_COSH] = &ops_sech, [EXPR_KIND_SINH] = &ops_cosech, [EXPR_KIND_TANH] = &ops_coth,
        [EXPR_KIND_SECH] = &ops_cosh, [EXPR_KIND_COSECH] = &ops_sinh, [EXPR_KIND_COTH] = &ops_tanh};

    if (!ops || (unsigned)ops->kind >= (unsigned)EXPR_KIND_COUNT)
        return NULL;
    return reciprocal_unary_ops[ops->kind];
}

typedef bool (*expr_inverse_candidate_ok_fn)(number_t value);

typedef struct expr_inverse_unary_rule {
    bool supported;
    expr_inverse_candidate_ok_fn candidate_ok;
} expr_inverse_unary_rule_t;

static bool expr_lambert_w_candidate_ok(number_t value)
{
    number_t imag;
    number_t neg_pi;
    bool ok;

    if (!num_is_finite(value))
        return false;

    if (num_is_real(value))
        return true;

    /* The branch-selecting W/productlog uses the principal complex strip. */
    imag = num_imag_part(value);
    neg_pi = num_neg(NUM_PI);
    ok = num_gt(imag, neg_pi) && num_lt(imag, NUM_PI);
    num_destroy(&neg_pi);
    num_destroy(&imag);
    return ok;
}

static bool expr_lambert_w0_candidate_ok(number_t value)
{
    return num_is_finite(value) && num_is_real(value) && num_ge(value, NUM_NEG_ONE);
}

static bool expr_lambert_wm1_candidate_ok(number_t value)
{
    return num_is_finite(value) && num_is_real(value) && num_le(value, NUM_NEG_ONE);
}

static const expr_inverse_unary_rule_t s_inverse_unary_rules[EXPR_KIND_COUNT] = {
    [EXPR_KIND_LAMBERT_W] = {true, expr_lambert_w_candidate_ok},
    [EXPR_KIND_LAMBERT_W0] = {true, expr_lambert_w0_candidate_ok},
    [EXPR_KIND_LAMBERT_WM1] = {true, expr_lambert_wm1_candidate_ok},
    [EXPR_KIND_LOG10] = {true, NULL},
};

static const expr_inverse_unary_rule_t *expr_inverse_unary_rule_for(const expr_ops_t *ops)
{
    if (!ops)
        return NULL;
    if ((unsigned)ops->kind >= (unsigned)EXPR_KIND_COUNT)
        return NULL;
    return s_inverse_unary_rules[ops->kind].supported ? &s_inverse_unary_rules[ops->kind] : NULL;
}

bool expr_ops_has_inverse_unary_simplify_rule(const expr_ops_t *ops)
{
    return expr_inverse_unary_rule_for(ops) != NULL;
}

bool expr_inverse_unary_candidate_value_ok(const expr_ops_t *ops, number_t value)
{
    const expr_inverse_unary_rule_t *rule = expr_inverse_unary_rule_for(ops);

    if (!rule)
        return false;
    if (!rule->candidate_ok)
        return true;

    return rule->candidate_ok(value);
}

/* ------------------------------------------------------------------------- */
/* Canonical singleton leaves                                                */
/* ------------------------------------------------------------------------- */

static struct _expr_t _EXPR_ZERO_NODE = {.ops = &ops_const,
                                         .a = NULL,
                                         .b = NULL,
                                         .c = {{0, 0, 0, 0, 0}},
                                         .x = {{0, 0, 0, 0, 0}},
                                         .x_valid = 1,
                                         .epoch = 0,
                                         .dx_cache = NULL,
                                         .name = NULL,
                                         .refcount = INT_MAX,
                                         .var_id = 0};

static struct _expr_t _EXPR_ONE_NODE = {.ops = &ops_const,
                                        .a = NULL,
                                        .b = NULL,
                                        .c = {{0, 0, 0, 0, 0}},
                                        .x = {{0, 0, 0, 0, 0}},
                                        .x_valid = 1,
                                        .epoch = 0,
                                        .dx_cache = NULL,
                                        .name = NULL,
                                        .refcount = INT_MAX,
                                        .var_id = 0};

static struct _expr_t _EXPR_LN10_NODE = {.ops = &ops_const,
                                         .a = NULL,
                                         .b = NULL,
                                         .c = {{0, 0, 0, 0, 0}},
                                         .x = {{0, 0, 0, 0, 0}},
                                         .x_valid = 1,
                                         .epoch = 0,
                                         .dx_cache = NULL,
                                         .name = "ln10",
                                         .refcount = INT_MAX,
                                         .var_id = 0};

const expr_t *const EXPR_ZERO = &_EXPR_ZERO_NODE;
const expr_t *const EXPR_ONE = &_EXPR_ONE_NODE;
const expr_t *const EXPR_LN10 = &_EXPR_LN10_NODE;

static uint64_t next_var_id = 1;
static int expr_singletons_ready = 0;

static void expr_shutdown_singletons(void)
{
    if (!expr_singletons_ready)
        return;

    num_destroy(&_EXPR_LN10_NODE.c);
    num_destroy(&_EXPR_LN10_NODE.x);
}

static void expr_init_singletons(void)
{
    if (expr_singletons_ready)
        return;

    _EXPR_ZERO_NODE.c = NUM_ZERO;
    _EXPR_ZERO_NODE.x = NUM_ZERO;
    _EXPR_ONE_NODE.c = NUM_ONE;
    _EXPR_ONE_NODE.x = NUM_ONE;
    _EXPR_LN10_NODE.c = num_scope_detach(num_const(NUM_LN10));
    _EXPR_LN10_NODE.x = num_scope_detach(num_clone(_EXPR_LN10_NODE.c));
    if (atexit(expr_shutdown_singletons) != 0)
        abort();
    expr_singletons_ready = 1;
}

static inline void refcount_inc(int *rc)
{
    if (*rc < INT_MAX)
        (*rc)++;
}

static inline int refcount_dec(int *rc)
{
    int prev = *rc;

    if (*rc < INT_MAX)
        (*rc)--;
    return prev;
}

static uint64_t alloc_var_id(void)
{
    return next_var_id++;
}

typedef struct {
    const char *ascii;
    size_t klen;
    const char *lower;
    const char *upper;
} greek_entry_t;

enum { GREEK_HT_SIZE = 30 };

static const greek_entry_t s_greek_names[GREEK_HT_SIZE] = {
    [0] = {"theta", 5, "θ", "Θ"},  [1] = {"psi", 3, "ψ", "Ψ"},      [2] = {"chi", 3, "χ", "Χ"},
    [4] = {"lambda", 6, "λ", "Λ"}, [5] = {"delta", 5, "δ", "Δ"},    [6] = {"omicron", 8, "ο", "Ο"},
    [8] = {"iota", 4, "ι", "Ι"},   [10] = {"mu", 2, "μ", "Μ"},      [11] = {"pi", 2, "π", "Π"},
    [12] = {"phi", 3, "φ", "Φ"},   [13] = {"alpha", 5, "α", "Α"},   [14] = {"zeta", 4, "ζ", "Ζ"},
    [15] = {"tau", 3, "τ", "Τ"},   [16] = {"rho", 3, "ρ", "Ρ"},     [17] = {"beta", 4, "β", "Β"},
    [19] = {"nu", 2, "ν", "Ν"},    [20] = {"kappa", 5, "κ", "Κ"},   [22] = {"sigma", 5, "σ", "Σ"},
    [23] = {"xi", 2, "ξ", "Ξ"},    [24] = {"eta", 3, "η", "Η"},     [25] = {"epsilon", 7, "ε", "Ε"},
    [26] = {"gamma", 5, "γ", "Γ"}, [27] = {"upsilon", 7, "υ", "Υ"}, [29] = {"omega", 5, "ω", "Ω"}};

char *expr_take_string_as_c_string(string_t *text)
{
    const char *src;
    char *out;

    if (!text)
        return NULL;

    src = string_c_str(text);
    out = strdup(src);
    string_free(text);
    if (!out)
        abort();
    return out;
}

static int expr_rune_ascii_alpha(rune_t rune, char *out)
{
    char ch;

    if (!rune_to_ascii(rune, &ch))
        return 0;
    if (!isalpha((unsigned char)ch))
        return 0;
    if (out)
        *out = ch;
    return 1;
}

static int expr_append_subscript_digit_text(string_t *out, char digit)
{
    return string_append_rune(out, rune_from_value(0x2080u + (uint32_t)(digit - '0')));
}

static int expr_string_equals_ascii_ci(const string_t *text, const char *literal)
{
    string_t *literal_text;
    string_view_t text_view;
    string_view_t literal_view;
    int equal;

    if (!text || !literal)
        return 0;

    literal_text = string_new_with(literal);
    if (!literal_text)
        return 0;

    text_view = string_view_all(text);
    literal_view = string_view_all(literal_text);
    equal = string_view_length(text_view) == string_view_length(literal_view) &&
            string_view_starts_with(text_view, literal_text, true);
    string_free(literal_text);
    return equal;
}

static unsigned greek_ht_hash_text(const string_t *text)
{
    string_cursor_t *cursor;
    unsigned x = 113u;

    if (!text)
        return 0;

    cursor = string_cursor_new(text);
    if (!cursor)
        return 0;

    while (!string_cursor_done(cursor)) {
        char ch;

        if (!rune_to_ascii(string_cursor_peek(cursor), &ch)) {
            string_cursor_free(cursor);
            return 0;
        }
        x *= 65599u;
        x ^= (unsigned char)tolower((unsigned char)ch);
        string_cursor_next(cursor);
    }

    string_cursor_free(cursor);
    x ^= (x >> 15);
    x *= 2654435761u;

    return x % GREEK_HT_SIZE;
}

static const greek_entry_t *lookup_greek_name_text(const string_t *name)
{
    const greek_entry_t *entry;

    if (!name)
        return NULL;

    entry = &s_greek_names[greek_ht_hash_text(name)];
    if (entry->ascii && expr_string_equals_ascii_ci(name, entry->ascii))
        return entry;

    return NULL;
}

size_t expr_match_leading_greek_alias_len(const string_cursor_t *cursor, string_pos_t pos)
{
    size_t best = 0u;

    if (!cursor)
        return 0u;

    for (size_t i = 0u; i < GREEK_HT_SIZE; ++i) {
        const greek_entry_t *entry = &s_greek_names[i];

        if (!entry->ascii)
            continue;
        if (entry->klen > best && string_cursor_match_at(cursor, pos, entry->ascii)) {
            best = entry->klen;
        }
    }

    return best;
}

static int expr_string_all_ascii_upper(const string_t *text)
{
    string_cursor_t *cursor;
    int upper = 1;

    if (!text)
        return 0;

    cursor = string_cursor_new(text);
    if (!cursor)
        return 0;

    while (!string_cursor_done(cursor)) {
        char ch;

        if (!rune_to_ascii(string_cursor_peek(cursor), &ch) || !isupper((unsigned char)ch)) {
            upper = 0;
            break;
        }
        string_cursor_next(cursor);
    }

    string_cursor_free(cursor);
    return upper;
}

string_t *expr_normalise_greek_alias_text(const string_t *alias)
{
    const greek_entry_t *entry = lookup_greek_name_text(alias);

    if (!entry)
        return NULL;
    return string_new_with(expr_string_all_ascii_upper(alias) ? entry->upper : entry->lower);
}

static string_t *expr_remove_at_runes_text(const string_t *text)
{
    string_t *out;
    string_cursor_t *cursor;

    out = string_new();
    cursor = string_cursor_new(text);
    if (!out || !cursor) {
        string_free(out);
        string_cursor_free(cursor);
        return NULL;
    }

    while (!string_cursor_done(cursor)) {
        rune_t rune = string_cursor_peek(cursor);

        if (!rune_is_equal(rune, '@') && string_append_rune(out, rune) != 0) {
            string_free(out);
            string_cursor_free(cursor);
            return NULL;
        }
        string_cursor_next(cursor);
    }

    string_cursor_free(cursor);
    return out;
}

static string_t *expr_expand_leading_greek_alias_text(const string_t *text)
{
    string_t *out = NULL;
    string_t *alias = NULL;
    string_t *rest = NULL;
    string_cursor_t *cursor;
    string_pos_t alias_start;
    string_pos_t rest_start;
    const greek_entry_t *entry;

    if (!text || !string_starts_with(text, "@"))
        return string_clone(text);

    cursor = string_cursor_new(text);
    if (!cursor)
        return NULL;

    string_cursor_next(cursor);
    alias_start = string_cursor_position(cursor);
    while (!string_cursor_done(cursor) && expr_rune_ascii_alpha(string_cursor_peek(cursor), NULL))
        string_cursor_next(cursor);

    alias = string_cursor_extract(alias_start, cursor);
    rest_start = string_cursor_position(cursor);
    while (!string_cursor_done(cursor))
        string_cursor_next(cursor);
    rest = string_cursor_extract(rest_start, cursor);
    string_cursor_free(cursor);

    entry = alias ? lookup_greek_name_text(alias) : NULL;
    if (entry) {
        out = string_new_with(expr_string_all_ascii_upper(alias) ? entry->upper : entry->lower);
        if (out && rest && string_append_string(out, rest) != 0) {
            string_free(out);
            out = NULL;
        }
    } else {
        out = string_clone(text);
    }

    string_free(rest);
    string_free(alias);
    return out;
}

static string_t *expr_subscript_underscore_digits_text(const string_t *text)
{
    string_t *out;
    string_cursor_t *cursor;

    out = string_new();
    cursor = string_cursor_new(text);
    if (!out || !cursor) {
        string_free(out);
        string_cursor_free(cursor);
        return NULL;
    }

    while (!string_cursor_done(cursor)) {
        rune_t rune = string_cursor_peek(cursor);

        if (rune_is_equal(rune, '_')) {
            string_cursor_next(cursor);
            rune = string_cursor_peek(cursor);
            if (rune_is_digit(rune)) {
                while (!string_cursor_done(cursor)) {
                    char digit;

                    rune = string_cursor_peek(cursor);
                    if (!rune_is_digit(rune) || !rune_to_ascii(rune, &digit))
                        break;
                    if (expr_append_subscript_digit_text(out, digit) != 0) {
                        string_free(out);
                        string_cursor_free(cursor);
                        return NULL;
                    }
                    string_cursor_next(cursor);
                }
                continue;
            }

            if (string_append_rune(out, rune_from_ascii('_')) != 0) {
                string_free(out);
                string_cursor_free(cursor);
                return NULL;
            }
            continue;
        }

        if (string_append_rune(out, rune) != 0) {
            string_free(out);
            string_cursor_free(cursor);
            return NULL;
        }
        string_cursor_next(cursor);
    }

    string_cursor_free(cursor);
    return out;
}

static string_t *expr_subscript_trailing_digits_text(const string_t *text)
{
    string_cursor_t *cursor;
    string_pos_t run_start = 0;
    int in_digit_run = 0;
    int trailing_digits = 0;
    string_t *out;

    cursor = string_cursor_new(text);
    if (!cursor)
        return NULL;

    while (!string_cursor_done(cursor)) {
        string_pos_t pos = string_cursor_position(cursor);
        rune_t rune = string_cursor_peek(cursor);

        if (rune_is_digit(rune)) {
            if (!in_digit_run) {
                run_start = pos;
                in_digit_run = 1;
            }
            trailing_digits = 1;
        } else {
            in_digit_run = 0;
            trailing_digits = 0;
        }
        string_cursor_next(cursor);
    }

    if (!trailing_digits || run_start == 0) {
        string_cursor_free(cursor);
        return string_clone(text);
    }

    out = string_new();
    if (!out || string_cursor_seek(cursor, 0) != 0) {
        string_free(out);
        string_cursor_free(cursor);
        return NULL;
    }

    while (!string_cursor_done(cursor)) {
        string_pos_t pos = string_cursor_position(cursor);
        rune_t rune = string_cursor_peek(cursor);

        if (pos >= run_start) {
            char digit;

            if (!rune_to_ascii(rune, &digit) || expr_append_subscript_digit_text(out, digit) != 0) {
                string_free(out);
                string_cursor_free(cursor);
                return NULL;
            }
        } else if (string_append_rune(out, rune) != 0) {
            string_free(out);
            string_cursor_free(cursor);
            return NULL;
        }
        string_cursor_next(cursor);
    }

    string_cursor_free(cursor);
    return out;
}

char *expr_normalise_name(const char *name)
{
    string_t *text;
    char *out;

    if (!name)
        return NULL;

    text = string_new_with(name);
    if (!text)
        return NULL;

    out = expr_take_string_as_c_string(expr_normalise_name_text(text));
    string_free(text);
    return out;
}

string_t *expr_normalise_name_text(const string_t *name)
{
    string_view_t trimmed_view;
    string_t *trimmed = NULL;
    string_t *expanded = NULL;
    string_t *clean = NULL;
    string_t *subscripted = NULL;
    string_t *out = NULL;

    if (!name)
        return NULL;

    trimmed_view = string_view_trim(string_view_all(name));
    if (string_view_is_empty(trimmed_view))
        return NULL;

    trimmed = string_from_view(&trimmed_view);
    expanded = expr_expand_leading_greek_alias_text(trimmed);
    clean = expr_remove_at_runes_text(expanded);
    subscripted = expr_subscript_underscore_digits_text(clean);
    out = expr_subscript_trailing_digits_text(subscripted);

    string_free(subscripted);
    string_free(clean);
    string_free(expanded);
    string_free(trimmed);
    return out;
}

char *expr_normalise_binding_name(const char *name)
{
    string_t *text;
    char *out;

    if (!name)
        return NULL;

    text = string_new_with(name);
    if (!text)
        return NULL;

    out = expr_take_string_as_c_string(expr_normalise_binding_name_text(text));
    string_free(text);
    return out;
}

string_t *expr_normalise_binding_name_text(const string_t *name)
{
    string_view_t trimmed_view;
    string_t *trimmed;
    string_t *canon;
    string_t *out;

    if (!name)
        return NULL;

    trimmed_view = string_view_trim(string_view_all(name));
    if (string_view_is_empty(trimmed_view))
        return NULL;

    trimmed = string_from_view(&trimmed_view);
    if (!trimmed)
        return NULL;

    if (string_starts_with(trimmed, "[") && string_ends_with(trimmed, "]")) {
        string_cursor_t *cursor = string_cursor_new(trimmed);
        string_pos_t start;
        string_pos_t end = 0;

        if (!cursor) {
            string_free(trimmed);
            return NULL;
        }

        string_cursor_next(cursor);
        start = string_cursor_position(cursor);
        while (!string_cursor_done(cursor)) {
            end = string_cursor_position(cursor);
            string_cursor_next(cursor);
        }

        out = string_cursor_slice_between(start, end, cursor);
        string_cursor_free(cursor);
        string_free(trimmed);
        return out;
    }

    canon = expr_default_constant_canonical_name_text(trimmed);
    out = expr_normalise_name_text(canon);
    string_free(canon);
    string_free(trimmed);
    return out;
}

int expr_is_default_constant_name(const char *name)
{
    string_t *text;
    int ok;

    if (!name)
        return 0;

    text = string_new_with(name);
    if (!text)
        return 0;

    ok = expr_is_default_constant_name_text(text);
    string_free(text);
    return ok;
}

int expr_is_default_constant_name_text(const string_t *name)
{
    string_cursor_t *cursor;
    rune_t rune;
    char ch;
    int uppercase_integral_constant = 0;
    int saw_digit = 0;
    int ok = 0;

    if (!name)
        return 0;

    cursor = string_cursor_new(name);
    if (!cursor)
        return 0;

    rune = string_cursor_peek(cursor);
    if (!rune_to_ascii(rune, &ch))
        goto done;
    uppercase_integral_constant = ch == 'C';
    if (ch != 'a' && ch != 'b' && ch != 'c' && ch != 'd' && ch != 'j' && ch != 'k' && ch != 'l' && ch != 'm' &&
        ch != 'n' && !uppercase_integral_constant)
        goto done;

    string_cursor_next(cursor);
    if (string_cursor_done(cursor)) {
        ok = 1;
        goto done;
    }

    rune = string_cursor_peek(cursor);
    if (rune_value(rune) >= 0x2080 && rune_value(rune) <= 0x2089) {
        while (!string_cursor_done(cursor)) {
            rune = string_cursor_peek(cursor);
            if (rune_value(rune) < 0x2080 || rune_value(rune) > 0x2089)
                goto done;
            string_cursor_next(cursor);
        }
        ok = 1;
        goto done;
    }

    if (!rune_is_equal(rune, '_'))
        goto done;
    string_cursor_next(cursor);

    while (!string_cursor_done(cursor)) {
        rune = string_cursor_peek(cursor);
        if (!rune_is_digit(rune))
            goto done;
        saw_digit = 1;
        string_cursor_next(cursor);
    }

    ok = saw_digit;

done:
    string_cursor_free(cursor);
    return ok;
}

int expr_get_default_constant_num(const char *name, number_t *value_out)
{
    string_t *text;
    int ok;

    if (!name)
        return 0;

    text = string_new_with(name);
    if (!text)
        return 0;

    ok = expr_get_default_constant_num_text(text, value_out);
    string_free(text);
    return ok;
}

int expr_get_default_constant_num_text(const string_t *name, number_t *value_out)
{
    string_t *canon;
    string_view_t view;
    int ok = 1;

    if (!name || !value_out)
        return 0;

    canon = expr_default_constant_canonical_name_text(name);
    if (!canon)
        return 0;

    view = string_view_all(canon);
    if (string_view_equals_literal(view, "e"))
        *value_out = num_const(NUM_E);
    else if (string_view_equals_literal(view, "i"))
        *value_out = num_const(NUM_I);
    else if (string_view_equals_literal(view, "@pi"))
        *value_out = num_const(NUM_PI);
    else if (string_view_equals_literal(view, "@phi"))
        *value_out = num_const(NUM_PHI);
    else if (string_view_equals_literal(view, "@gamma"))
        *value_out = num_const(NUM_EULER_MASCHERONI);
    else
        ok = 0;

    string_free(canon);
    return ok;
}

const char *expr_default_constant_canonical_name(const char *name)
{
    return expr_lookup_default_constant_alias(name);
}

string_t *expr_default_constant_canonical_name_text(const string_t *name)
{
    const char *mapped;

    if (!name)
        return NULL;

    mapped = expr_default_constant_alias_literal(name);
    return mapped ? string_new_with(mapped) : string_clone(name);
}

/* ------------------------------------------------------------------------- */
/* Lifetime                                                                  */
/* ------------------------------------------------------------------------- */

void expr_retain(const expr_t *dv)
{
    expr_init_singletons();
    if (dv)
        refcount_inc(&((expr_t *)dv)->refcount);
}

static void expr_release(expr_t *dv)
{
    expr_t *a;
    expr_t *b;
    expr_deriv_cache_t *ce;

    if (!dv)
        return;
    expr_init_singletons();
    if (refcount_dec(&dv->refcount) > 1)
        return;

    a = dv->a;
    b = dv->b;

    ce = dv->dx_cache;
    while (ce) {
        expr_deriv_cache_t *next = ce->next;
        expr_release(ce->dx);
        free(ce);
        ce = next;
    }

    if (dv->name)
        free(dv->name);
    if (dv->binding_expr)
        expr_binding_expr_free(dv->binding_expr);
    for (size_t i = 0u; i < dv->formal_wrt_count; ++i)
        expr_release(dv->formal_wrts[i]);
    free(dv->formal_wrts);
    num_destroy(&dv->c);
    num_destroy(&dv->x);
    free(dv);

    expr_release(a);
    expr_release(b);
}

void expr_free(expr_t *expr)
{
    expr_release(expr);
}

expr_t *expr_alloc(const expr_ops_t *ops)
{
    expr_t *dv = malloc(sizeof *dv);

    if (!dv)
        abort();

    expr_init_singletons();
    dv->ops = ops;
    dv->a = NULL;
    dv->b = NULL;
    dv->c = NUM_ZERO;
    dv->x = NUM_ZERO;
    dv->x_valid = 0;
    dv->epoch = 0;
    dv->simplified = false;
    dv->simplify_epoch = 0;
    dv->dx_cache = NULL;
    dv->name = NULL;
    dv->binding_expr = NULL;
    dv->formal_wrts = NULL;
    dv->formal_wrt_count = 0u;
    dv->refcount = 1;
    dv->var_id = 0;

    return dv;
}

/* ------------------------------------------------------------------------- */
/* Internal number_t-first leaf builders                                     */
/* ------------------------------------------------------------------------- */

expr_t *expr_make_const_num(number_t x)
{
    expr_t *dv = expr_alloc(&ops_const);

    expr_store_const_num(dv, x);
    expr_store_value_num(dv, num_clone(dv->c));
    dv->x_valid = 1;
    return dv;
}

expr_t *expr_make_var_num(number_t x)
{
    expr_t *dv = expr_alloc(&ops_var);

    expr_store_const_num(dv, x);
    expr_store_value_num(dv, num_clone(dv->c));
    dv->x_valid = 1;
    dv->var_id = alloc_var_id();
    return dv;
}

/* ------------------------------------------------------------------------- */
/* Public number_t constructors                                              */
/* ------------------------------------------------------------------------- */

expr_t *expr_new_const(number_t x)
{
    return expr_make_const_num(num_clone(x));
}

expr_t *expr_new_var(number_t x)
{
    return expr_make_var_num(num_clone(x));
}

static expr_t *expr_attach_name(expr_t *dv, const char *name)
{
    dv->name = expr_normalise_name(name);
    return dv;
}

static expr_t *expr_attach_name_text(expr_t *dv, const string_t *name)
{
    dv->name = name ? expr_take_string_as_c_string(expr_normalise_name_text(name)) : NULL;
    return dv;
}

expr_t *expr_new_named_const(number_t x, const char *name)
{
    return expr_attach_name(expr_new_const(x), name);
}

expr_t *expr_new_named_const_text(number_t x, const string_t *name)
{
    return expr_attach_name_text(expr_new_const(x), name);
}

expr_t *expr_new_named_var(number_t x, const char *name)
{
    return expr_attach_name(expr_new_var(x), name);
}

expr_t *expr_new_named_var_text(number_t x, const string_t *name)
{
    return expr_attach_name_text(expr_new_var(x), name);
}

expr_t *expr_retain_expr(const expr_t *expr)
{
    if (!expr)
        return NULL;
    expr_retain(expr);
    return (expr_t *)expr;
}

expr_t *expr_const_zero(void)
{
    return expr_new_const(NUM_ZERO);
}

expr_t *expr_const_one(void)
{
    return expr_new_const(NUM_ONE);
}

expr_t *expr_const_long(long value)
{
    number_t number = num_create_from_long(value);
    expr_t *expr = expr_new_const(number);

    num_destroy(&number);
    return expr;
}

expr_t *expr_simplify_owned(expr_t *expr)
{
    expr_t *simplified;

    if (!expr)
        return NULL;
    simplified = expr_simplify(expr);
    expr_free(expr);
    return simplified;
}

expr_t *expr_negate_owned(expr_t *expr)
{
    expr_t *negated;

    if (!expr)
        return NULL;
    negated = expr_neg(expr);
    expr_free(expr);
    return expr_simplify_owned(negated);
}

expr_t *expr_add_owned(expr_t *left, expr_t *right)
{
    expr_t *sum;

    if (!left)
        return right;
    if (!right)
        return left;

    sum = expr_add(left, right);
    expr_free(right);
    expr_free(left);
    return sum;
}

static expr_t *expr_binary_simplify_owned(const expr_t *left, const expr_t *right, expr_apply_binary_fn apply)
{
    expr_t *raw;

    if (!left || !right || !apply) {
        expr_free((expr_t *)left);
        expr_free((expr_t *)right);
        return NULL;
    }

    raw = apply(left, right);
    expr_free((expr_t *)right);
    expr_free((expr_t *)left);
    return expr_simplify_owned(raw);
}

expr_t *expr_add_long(const expr_t *expr, long value)
{
    number_t number = num_create_from_long(value);
    expr_t *out = expr ? expr_add_num(expr, &number) : NULL;

    num_destroy(&number);
    return out;
}

expr_t *expr_mul_long(const expr_t *expr, long value)
{
    number_t number = num_create_from_long(value);
    expr_t *out = expr ? expr_mul_num(expr, &number) : NULL;

    num_destroy(&number);
    return out;
}

expr_t *expr_div_long(const expr_t *expr, long value)
{
    expr_t *denominator = expr_const_long(value);
    expr_t *out = (expr && denominator) ? expr_div(expr, denominator) : NULL;

    expr_free(denominator);
    return out;
}

expr_t *expr_pow_long(const expr_t *expr, long exponent)
{
    number_t number = num_create_from_long(exponent);
    expr_t *out = expr ? expr_pow(expr, &number) : NULL;

    num_destroy(&number);
    return out;
}

expr_t *expr_add_simplify_owned(const expr_t *left, const expr_t *right)
{
    return expr_binary_simplify_owned(left, right, expr_add);
}

expr_t *expr_sub_simplify_owned(const expr_t *left, const expr_t *right)
{
    return expr_binary_simplify_owned(left, right, expr_sub);
}

expr_t *expr_mul_simplify_owned(const expr_t *left, const expr_t *right)
{
    return expr_binary_simplify_owned(left, right, expr_mul);
}

expr_t *expr_div_simplify_owned(const expr_t *left, const expr_t *right)
{
    return expr_binary_simplify_owned(left, right, expr_div);
}

static void expr_clone_copy_metadata(expr_t *out, const expr_t *expr)
{
    if (!out || !expr)
        return;
    if (expr_is_var(expr))
        out->var_id = expr->var_id;
    if (expr->binding_expr) {
        if (out->binding_expr)
            expr_binding_expr_free(out->binding_expr);
        out->binding_expr = expr_binding_expr_clone(expr->binding_expr);
    }
}

expr_t *expr_clone(const expr_t *expr)
{
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *out = NULL;

    if (!expr)
        return NULL;

    if (expr->ops->kind == EXPR_KIND_CONST) {
        out = (expr->name && *expr->name) ? expr_new_named_const(expr->c, expr->name) : expr_new_const(expr->c);
        expr_clone_copy_metadata(out, expr);
        return out;
    }

    if (expr->ops->kind == EXPR_KIND_VAR) {
        out = (expr->name && *expr->name) ? expr_new_named_var(expr->x, expr->name) : expr_new_var(expr->x);
        expr_clone_copy_metadata(out, expr);
        return out;
    }

    if (expr->ops->kind == EXPR_KIND_FORMAL_DERIVATIVE) {
        left = expr_clone(expr->a);
        if (!left)
            return NULL;
        out = expr_new_formal_derivative(left, expr->formal_wrt_count, expr->formal_wrts);
        expr_free(left);
        expr_clone_copy_metadata(out, expr);
        return out;
    }

    if (expr_is_arbitrary_function(expr)) {
        left = expr_clone(expr->a);
        if (!left)
            return NULL;
        out = expr_new_arbitrary_function(expr->name, left);
        expr_free(left);
        expr_clone_copy_metadata(out, expr);
        return out;
    }

    if (expr->ops->kind == EXPR_KIND_POW_D && expr->a) {
        left = expr_clone(expr->a);
        if (!left)
            return NULL;
        out = expr_pow(left, &expr->c);
        expr_free(left);
        expr_clone_copy_metadata(out, expr);
        return out;
    }

    if ((expr->ops->kind == EXPR_KIND_INTEGRAL || expr->ops->kind == EXPR_KIND_INTEGRAL_META ||
         expr->ops->kind == EXPR_KIND_INTEGRAL_BOUNDS) &&
        expr->a && expr->b) {
        left = expr_clone(expr->a);
        right = expr_clone(expr->b);
        if (!left || !right) {
            expr_free(left);
            expr_free(right);
            return NULL;
        }
        out = expr_new_binary_internal(expr->ops, left, right);
        expr_clone_copy_metadata(out, expr);
        return out;
    }

    if (expr->ops->arity == EXPR_OP_UNARY && expr->ops->apply_unary) {
        left = expr_clone(expr->a);
        if (!left)
            return NULL;
        out = expr->ops->apply_unary(left);
        expr_free(left);
        expr_clone_copy_metadata(out, expr);
        return out;
    }

    if (expr->ops->arity == EXPR_OP_BINARY) {
        left = expr_clone(expr->a);
        right = expr_clone(expr->b);
        if (!left || !right) {
            expr_free(left);
            expr_free(right);
            return NULL;
        }
        if (expr->ops->apply_binary) {
            out = expr->ops->apply_binary(left, right);
        } else {
            out = expr_new_binary_internal(expr->ops, left, right);
            if (out) {
                left = NULL;
                right = NULL;
            }
        }
        expr_free(left);
        expr_free(right);
        expr_clone_copy_metadata(out, expr);
        return out;
    }

    return NULL;
}
