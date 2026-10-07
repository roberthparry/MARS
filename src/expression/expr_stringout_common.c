/**
 * @file expr_stringout_common.c
 * @brief Shared formatting atoms, precedence and temporary names.
 *
 * Provides the common machinery used by recursive renderers, including factors, numeric atoms and naming scopes.
 * It centralises style-independent decisions that should not drift between output formats.
 *
 * This is part of the expression.h implementation. Preserve expression ownership, symbol identity and mathematical
 * domain restrictions when extending these operations.
 */

/* Shared atoms, precedence, factors and temporary-name scopes. */

#include <ctype.h>
#include <gmp.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expr_bindings.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#include "expr_stringout.h"
#define MARS_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "expr_stringout_internal.h"
#include "expression.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#include "ustring.h"

static const char *sup_digits[10] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};

static const char *sub_digits[10] = {"₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉"};


void emit_superscript_int(sbuf_t *b, long n)
{
    if (n < 0) {
        sbuf_puts(b, "⁻");
        n = -n;
    }
    if (n == 0) {
        sbuf_puts(b, "⁰");
        return;
    }
    char tmp[32];
    int len = 0;
    while (n > 0 && len < (int)sizeof(tmp)) {
        tmp[len++] = (char)('0' + (n % 10));
        n /= 10;
    }
    for (int i = len - 1; i >= 0; --i) {
        int d = tmp[i] - '0';
        sbuf_puts(b, sup_digits[d]);
    }
}

void emit_subscript_int(sbuf_t *b, long n)
{
    if (n < 0) {
        sbuf_puts(b, "₋");
        n = -n;
    }
    if (n == 0) {
        sbuf_puts(b, "₀");
        return;
    }
    char tmp[32];
    int len = 0;
    while (n > 0 && len < (int)sizeof(tmp)) {
        tmp[len++] = (char)('0' + (n % 10));
        n /= 10;
    }
    for (int i = len - 1; i >= 0; --i) {
        int d = tmp[i] - '0';
        sbuf_puts(b, sub_digits[d]);
    }
}

static bool expr_tostring_text_to_long_local(const string_t *text, long *out)
{
    string_cursor_t *cursor;
    bool negative = false;
    bool saw_digit = false;
    unsigned long value = 0u;
    unsigned long limit;

    if (!text || !out || string_length(text) == 0u)
        return false;

    cursor = string_cursor_new(text);
    if (!cursor)
        return false;

    if (rune_is_equal(string_cursor_peek(cursor), '-')) {
        negative = true;
        (void)string_cursor_next(cursor);
    }

    limit = negative ? (unsigned long)LONG_MAX + 1u : (unsigned long)LONG_MAX;
    while (!string_cursor_done(cursor)) {
        rune_t rune = string_cursor_peek(cursor);
        char ch = '\0';
        unsigned int digit;

        if (!rune_to_ascii(rune, &ch) || ch < '0' || ch > '9') {
            string_cursor_free(cursor);
            return false;
        }

        digit = (unsigned int)(ch - '0');
        if (value > (limit - digit) / 10u) {
            string_cursor_free(cursor);
            return false;
        }
        value = value * 10u + digit;
        saw_digit = true;
        (void)string_cursor_next(cursor);
    }

    string_cursor_free(cursor);
    if (!saw_digit)
        return false;

    *out = negative && value == (unsigned long)LONG_MAX + 1u ? LONG_MIN : (negative ? -(long)value : (long)value);
    return true;
}

bool expr_try_get_small_integer_exponent(number_t value, long *out)
{
    string_t *text;
    long parsed;

    if (!out || !num_is_real(value) || !num_is_integer(value))
        return false;

    text = num_to_string(value);
    if (!text)
        return false;
    if (string_length(text) == 0u) {
        string_free(text);
        return false;
    }

    if (!expr_tostring_text_to_long_local(text, &parsed)) {
        string_free(text);
        return false;
    }

    string_free(text);
    *out = parsed;
    return true;
}

/* ------------------------------------------------------------------------- */
/* Atom helpers                                                              */
/* ------------------------------------------------------------------------- */

int expr_tostring_should_emit_binding_expr(const expr_t *f)
{
    number_t value;
    int is_builtin_const;
    int value_matches_builtin = 0;

    if (!f || !f->binding_expr)
        return 0;
    if (!f->name || !*f->name)
        return 1;

    is_builtin_const = expr_get_default_constant_num(f->name, &value);
    if (is_builtin_const) {
        value_matches_builtin = num_eq(f->c, value);
        num_destroy(&value);
    }
    return is_builtin_const && value_matches_builtin;
}

/* A numerical atom can print as a quotient or an imaginary product. Both need
 * grouping in denominators and powers: a/2i would otherwise parse as (a/2)i. */
bool numeric_atom_needs_grouping(const expr_t *expr, const char *text, int parent_prec)
{
    return text && parent_prec > PREC_MUL &&
           (strchr(text, '/') != NULL || (expr_is_const(expr) && !num_is_real(expr->c) &&
                                         !num_eq(expr->c, NUM_I)));
}

void emit_atom(expr_t *f, sbuf_t *b, int parent_prec)
{
    if (expr_is_const(f)) {
        if (expr_tostring_should_emit_binding_expr(f)) {
            char *text = expr_binding_expr_to_string(f->binding_expr);

            if (text) {
                bool grouped = numeric_atom_needs_grouping(f, text, parent_prec);
                if (grouped)
                    sbuf_putc(b, '(');
                sbuf_puts(b, text);
                if (grouped)
                    sbuf_putc(b, ')');
                free(text);
            }
        } else if (f->name && *f->name) {
            emit_name(b, f->name);
        } else {
            char *text = expr_const_to_string_local(f);
            if (text) {
                bool grouped = numeric_atom_needs_grouping(f, text, parent_prec);
                if (grouped)
                    sbuf_putc(b, '(');
                sbuf_puts(b, text);
                if (grouped)
                    sbuf_putc(b, ')');
                free(text);
            }
        }
    } else if (expr_is_var(f)) {
        emit_name(b, f->name ? f->name : "x");
    } else {
        char *text = expr_eval_to_string_local(f);
        if (text) {
            sbuf_puts(b, text);
            free(text);
        }
    }
}

bool emit_negative_const_binding_expr_abs(const expr_t *f, sbuf_t *b, bool tex)
{
    char *raw;
    string_t *text;
    string_cursor_t *cursor;
    string_pos_t rest_start;
    string_pos_t digits_start;
    string_pos_t digits_end;
    string_t *slice;
    unsigned char digit;
    bool ok = false;

    if (!f || !f->binding_expr)
        return false;

    raw = tex ? expr_binding_expr_to_TeX(f->binding_expr) : expr_binding_expr_to_string(f->binding_expr);
    if (!raw)
        return false;

    text = string_new_with(raw);
    free(raw);
    if (!text)
        return false;

    cursor = string_cursor_new(text);
    if (!cursor) {
        string_free(text);
        return false;
    }

    string_cursor_skip_spaces(cursor);
    if (!rune_is_equal(string_cursor_peek(cursor), '-'))
        goto done;

    (void)string_cursor_next(cursor);
    string_cursor_skip_spaces(cursor);

    rest_start = string_cursor_position(cursor);
    digits_start = rest_start;
    while (string_cursor_peek_ascii(cursor, &digit) && isdigit(digit))
        (void)string_cursor_next(cursor);
    digits_end = string_cursor_position(cursor);

    if (digits_end > digits_start) {
        if (tex && string_cursor_consume(cursor, " \\cdot \\pi")) {
            slice = string_cursor_slice_between(digits_start, digits_end, cursor);
            if (slice) {
                sbuf_puts(b, string_c_str(slice));
                string_free(slice);
            }
            sbuf_puts(b, "\\pi");
            slice =
                string_cursor_slice_between(string_cursor_position(cursor), string_cursor_end_position(cursor), cursor);
            if (slice) {
                sbuf_puts(b, string_c_str(slice));
                string_free(slice);
            }
            ok = true;
            goto done;
        }
        if (!tex && string_cursor_consume(cursor, "·π")) {
            slice = string_cursor_slice_between(digits_start, digits_end, cursor);
            if (slice) {
                sbuf_puts(b, string_c_str(slice));
                string_free(slice);
            }
            sbuf_puts(b, "π");
            slice =
                string_cursor_slice_between(string_cursor_position(cursor), string_cursor_end_position(cursor), cursor);
            if (slice) {
                sbuf_puts(b, string_c_str(slice));
                string_free(slice);
            }
            ok = true;
            goto done;
        }
    }

    slice = string_cursor_slice_between(rest_start, string_cursor_end_position(cursor), cursor);
    if (slice) {
        sbuf_puts(b, string_c_str(slice));
        string_free(slice);
        ok = true;
    }

done:
    string_cursor_free(cursor);
    string_free(text);
    return ok;
}

/* -------------------------------------------------------------
   Helper: does a pow exponent need wrapping parens?
   Atoms (var/const) and function calls (unary/binary — they have their own
   parentheses) are self-delimiting; infix operators and neg are not.
   ------------------------------------------------------------- */
int pow_exp_needs_parens(const expr_t *e)
{
    if (!e)
        return 0;
    if (expr_is_const(e) && !num_is_real(e->c) && !num_eq(e->c, NUM_I))
        return 1;
    if (e->ops->arity == EXPR_OP_ATOM)
        return 0; /* var, const */
    if (expr_is_neg(e))
        return 1;
    if (expr_is_pow_d_expr(e))
        return 1; /* e.g. y² is ambiguous as exponent */
    if (e->ops->arity == EXPR_OP_UNARY)
        return 0; /* sin(…), exp(…), etc. */
    /* EXPR_OP_BINARY: arithmetic/pow need parens; named functions (atan2 …) don't */
    if (expr_is_addsub(e) || expr_is_mul(e) || expr_is_op(e, &ops_div) || expr_is_op(e, &ops_pow))
        return 1;
    return 0;
}

int pow_base_needs_visible_parens(const expr_t *base)
{
    if (base && (expr_is_neg(base) || expr_is_formal_derivative(base) || expr_is_pow_d_expr(base) ||
                 expr_is_op(base, &ops_pow)))
        return 1;

    if (base && expr_is_const(base) && expr_tostring_should_emit_binding_expr(base) && base->binding_expr) {
        if (base->binding_expr->kind == EXPR_BINDING_EXPR_ADD || base->binding_expr->kind == EXPR_BINDING_EXPR_SUB)
            return 1;
        if (base->binding_expr->kind == EXPR_BINDING_EXPR_UNARY_OP &&
            base->binding_expr->u.unary_op.ops == &ops_neg)
            return 1;
        if (base->binding_expr->kind == EXPR_BINDING_EXPR_UNARY_OP ||
            base->binding_expr->kind == EXPR_BINDING_EXPR_BINARY_OP ||
            base->binding_expr->kind == EXPR_BINDING_EXPR_POWI)
            return 0;
    }

    if (!base || !expr_is_const(base))
        return 0;
    if (num_is_real(base->c))
        return num_lt(base->c, NUM_ZERO);
    return !num_eq(base->c, NUM_I);
}

int mul_factor_needs_visible_parens(const expr_t *factor)
{
    number_t real;
    number_t imaginary;
    int needs_parens;

    if (factor && expr_is_const(factor) && expr_tostring_should_emit_binding_expr(factor) && factor->binding_expr) {
        if (factor->binding_expr->kind == EXPR_BINDING_EXPR_ADD || factor->binding_expr->kind == EXPR_BINDING_EXPR_SUB)
            return 1;
        if (factor->binding_expr->kind == EXPR_BINDING_EXPR_UNARY_OP &&
            factor->binding_expr->u.unary_op.ops == &ops_neg)
            return 1;
        if (factor->binding_expr->kind == EXPR_BINDING_EXPR_UNARY_OP ||
            factor->binding_expr->kind == EXPR_BINDING_EXPR_BINARY_OP ||
            factor->binding_expr->kind == EXPR_BINDING_EXPR_POWI)
            return 0;
    }

    if (!factor || !expr_is_const(factor) || num_is_real(factor->c))
        return 0;

    real = num_real_part(factor->c);
    imaginary = num_imag_part(factor->c);
    /* A trailing negative imaginary factor must not turn a product such as x*(-i) into x-i. */
    needs_parens = !num_eq(real, NUM_ZERO) || num_lt(imaginary, NUM_ZERO);
    num_destroy(&imaginary);
    num_destroy(&real);
    return needs_parens;
}

/* A leading imaginary coefficient needs no grouping; a trailing negative factor still does. */
bool mul_coefficient_needs_parens(const expr_t *factor, bool leading)
{
    if (leading && expr_is_unnamed_const(factor) && !num_is_real(factor->c) &&
        !expr_tostring_should_emit_binding_expr(factor)) {
        number_t real = num_real_part(factor->c);
        bool imaginary = num_is_zero(real);
        num_destroy(&real);
        if (imaginary)
            return false;
    }
    return mul_factor_needs_visible_parens(factor);
}

/* Addition emits the leading sign separately; a pure imaginary coefficient is then a single term.
 * Products still need grouping around trailing negative factors, and a+bi still needs grouping. */
bool additive_const_needs_visible_parens(const expr_t *constant)
{
    if (!constant || !expr_is_const(constant))
        return false;
    if (expr_tostring_should_emit_binding_expr(constant))
        return mul_factor_needs_visible_parens(constant);
    if (num_is_real(constant->c))
        return false;
    number_t real = num_real_part(constant->c);
    bool grouped = !num_is_zero(real);
    num_destroy(&real);
    return grouped;
}

int add_rhs_needs_visible_parens(const expr_t *rhs)
{
    if (additive_const_needs_visible_parens(rhs) ||
        (expr_is_neg(rhs) && additive_const_needs_visible_parens(rhs->a)))
        return 1;

    if (rhs && expr_is_const(rhs) && expr_tostring_should_emit_binding_expr(rhs) && rhs->binding_expr) {
        if (rhs->binding_expr->kind == EXPR_BINDING_EXPR_ADD || rhs->binding_expr->kind == EXPR_BINDING_EXPR_SUB)
            return 1;
        if (rhs->binding_expr->kind == EXPR_BINDING_EXPR_UNARY_OP ||
            rhs->binding_expr->kind == EXPR_BINDING_EXPR_BINARY_OP || rhs->binding_expr->kind == EXPR_BINDING_EXPR_POWI)
            return 0;
    }

    if (expr_is_neg(rhs) && rhs->a && expr_is_const(rhs->a) && expr_tostring_should_emit_binding_expr(rhs->a) &&
        rhs->a->binding_expr) {
        if (rhs->a->binding_expr->kind == EXPR_BINDING_EXPR_ADD || rhs->a->binding_expr->kind == EXPR_BINDING_EXPR_SUB)
            return 1;
        if (rhs->a->binding_expr->kind == EXPR_BINDING_EXPR_UNARY_OP ||
            rhs->a->binding_expr->kind == EXPR_BINDING_EXPR_BINARY_OP ||
            rhs->a->binding_expr->kind == EXPR_BINDING_EXPR_POWI)
            return 0;
    }

    return expr_is_addsub(rhs);
}

static bool expr_binding_expr_is_number_text_local(const expr_binding_expr_t *expr, const char *text)
{
    return expr && expr->kind == EXPR_BINDING_EXPR_NUMBER && expr->u.text && strcmp(expr->u.text, text) == 0;
}

static bool expr_is_preserved_ln10_const_local(const expr_t *f)
{
    const expr_binding_expr_t *expr;

    if (!f || !expr_is_const(f) || !f->binding_expr)
        return false;

    expr = f->binding_expr;
    return expr->kind == EXPR_BINDING_EXPR_UNARY_OP && expr->u.unary_op.ops == &ops_log &&
           expr_binding_expr_is_number_text_local(expr->u.unary_op.child, "10");
}

bool expr_is_const_half_local(const expr_t *f)
{
    return f && expr_is_const(f) && num_eq(f->c, NUM_HALF);
}

bool number_is_neg_half_local(number_t value)
{
    number_t neg_half = num_neg(NUM_HALF);
    bool out = num_eq(value, neg_half);

    num_destroy(&neg_half);
    return out;
}

static bool expr_is_const_neg_half_local(const expr_t *f)
{
    return f && expr_is_const(f) && number_is_neg_half_local(f->c);
}

bool expr_const_half_can_render_as_sqrt_local(const expr_t *f)
{
    return expr_is_const_half_local(f) && (!f->name || !*f->name) && !f->binding_expr;
}

bool expr_const_neg_half_can_render_as_sqrt_local(const expr_t *f)
{
    return expr_is_const_neg_half_local(f) && (!f->name || !*f->name) && !f->binding_expr;
}

static int is_atomic_for_mul(const expr_t *f);

void emit_expr_mul_separator_local(const expr_t *left, const expr_t *right, sbuf_t *b)
{
    if (mul_factor_needs_visible_parens(right)) {
        sbuf_puts(b, "·");
        return;
    }
    int left_atomic;
    int right_atomic;

    left_atomic = is_atomic_for_mul(left);
    right_atomic = is_atomic_for_mul(right);
    /* Keep a symbolic factor and i separate: xi and pi are also parser aliases for Greek letters. */
    bool imaginary_pair = right && expr_is_const(right) && num_eq(right->c, NUM_I) &&
                          left && left->name && *left->name;
    /* An uncombined integer factor must not join preceding digits: 29*1 must not print as 291. */
    bool numeric_pair = expr_is_const(left) && (!left->name || !*left->name) &&
                        expr_is_const(right) && (!right->name || !*right->name) &&
                        num_is_real(left->c) && num_is_integer(right->c) &&
                        (!right->binding_expr || expr_binding_expr_is_numeric_literal(right->binding_expr));
    if (imaginary_pair || numeric_pair || !(left_atomic && right_atomic))
        sbuf_puts(b, "·");
}

/* -------------------------------------------------------------
   Helper: atomic factors for implicit multiplication (EXPR mode)
   ------------------------------------------------------------- */
static int is_atomic_for_mul(const expr_t *f)
{
    if (!f)
        return 0;

    if (expr_is_const(f)) {
        /* Unnamed numeric constants are always atomic (e.g. the leading "6" in 6x²).
         * Named constants are atomic only when their name is "simple" (single letter
         * or letter + subscript digits).  Multi-char names like "pi" or "radius"
         * are non-atomic so that a middle-dot separator is inserted between adjacent
         * bracketed terms: [pi]·[radius]² instead of [pi][radius]². */
        if (expr_is_preserved_ln10_const_local(f))
            return 0;
        if (f->binding_expr && expr_binding_expr_needs_explicit_mul_separator(f->binding_expr))
            return 0;
        if (!f->name || !*f->name)
            return 1;
        return expr_tostring_is_simple_name(f->name);
    }

    if (expr_is_var(f))
        return expr_tostring_is_simple_name(f->name);

    if (expr_tostring_is_var_pow_d(f))
        return num_sign(f->c) > 0 && expr_tostring_is_simple_name(f->a->name);

    return 0;
}

/* -------------------------------------------------------------
   Factor classification / flattening / ordering
   ------------------------------------------------------------- */

void flatten_mul(expr_t *f, expr_t **buf, int *count, int max)
{
    if (!f || *count >= max)
        return;

    if (expr_is_mul(f)) {
        flatten_mul(f->a, buf, count, max);
        flatten_mul(f->b, buf, count, max);
    } else {
        buf[(*count)++] = f;
    }
}

typedef struct {
    const expr_t *const *nodes;
    const char *const *names;
    size_t count;
    const expr_t *expanded_node;
} function_temporary_context_t;

static _Thread_local function_temporary_context_t function_temporary_context;
static _Thread_local bool mathematical_temporaries_active;

const char *function_temporary_name(const expr_t *expr)
{
    if (!expr || expr == function_temporary_context.expanded_node)
        return NULL;

    for (size_t index = 0u; index < function_temporary_context.count; ++index) {
        if (function_temporary_context.nodes[index] == expr ||
            expr_struct_eq(function_temporary_context.nodes[index], expr))
            return function_temporary_context.names[index];
    }
    return NULL;
}

void flatten_func_mul(expr_t *f, expr_t **buf, int *count, int max)
{
    if (!f || *count >= max)
        return;

    if (expr_is_mul(f) && !function_temporary_name(f)) {
        flatten_func_mul(f->a, buf, count, max);
        flatten_func_mul(f->b, buf, count, max);
    } else {
        buf[(*count)++] = f;
    }
}

const char *function_factored_temporary_name(const expr_t *product, const expr_t *denominator,
                                                    expr_t **factors, int factor_count, bool *consumed)
{
    size_t best_index = (size_t)-1;
    int best_factor_count = 0;
    bool best_consumed[64] = {false};

    for (size_t temporary_index = 0u; temporary_index < function_temporary_context.count; ++temporary_index) {
        const expr_t *candidate = function_temporary_context.nodes[temporary_index];
        const expr_t *candidate_numerator = candidate;
        const expr_t *candidate_denominator = NULL;
        expr_t *candidate_factors[64];
        bool candidate_consumed[64] = {false};
        int candidate_count = 0;
        bool matches = true;

        if (!candidate || candidate == function_temporary_context.expanded_node || candidate == product ||
            expr_struct_eq(candidate, product))
            continue;
        if (expr_is_op(candidate, &ops_div)) {
            candidate_numerator = candidate->a;
            candidate_denominator = candidate->b;
        } else if (!expr_is_mul(candidate)) {
            continue;
        }
        if ((candidate_denominator || denominator) &&
            (!candidate_denominator || !denominator || !expr_struct_eq(candidate_denominator, denominator)))
            continue;
        flatten_mul((expr_t *)candidate_numerator, candidate_factors, &candidate_count, 64);
        if (candidate_count + (candidate_denominator ? 1 : 0) < 2 || candidate_count > factor_count ||
            candidate_count <= best_factor_count)
            continue;

        for (int candidate_factor = 0; candidate_factor < candidate_count; ++candidate_factor) {
            bool found = false;

            for (int product_factor = 0; product_factor < factor_count; ++product_factor) {
                if (!candidate_consumed[product_factor] &&
                    (candidate_factors[candidate_factor] == factors[product_factor] ||
                     expr_struct_eq(candidate_factors[candidate_factor], factors[product_factor]))) {
                    candidate_consumed[product_factor] = true;
                    found = true;
                    break;
                }
            }
            if (!found) {
                matches = false;
                break;
            }
        }
        if (matches) {
            best_index = temporary_index;
            best_factor_count = candidate_count;
            memcpy(best_consumed, candidate_consumed, sizeof(best_consumed));
        }
    }

    if (best_index == (size_t)-1)
        return NULL;
    memcpy(consumed, best_consumed, sizeof(best_consumed));
    return function_temporary_context.names[best_index];
}

static int expr_tostring_is_named_const_pow_d(const expr_t *f)
{
    return expr_is_pow_d_expr(f) && expr_is_const(f->a) && f->a->name && *f->a->name;
}

static int expr_tostring_rune_is_digit_suffix(rune_t rune)
{
    uint32_t value = rune_value(rune);

    return rune_is_digit(rune) || (value >= 0x2080u && value <= 0x2089u);
}

int expr_tostring_is_primary_variable_name(const char *name)
{
    string_t *text;
    string_cursor_t *cursor;
    char first;
    int ok = 0;

    if (!name || !*name)
        return 0;

    text = string_new_with(name);
    cursor = text ? string_cursor_new(text) : NULL;
    if (!cursor)
        goto done;

    if (!rune_to_ascii(string_cursor_peek(cursor), &first) ||
        (first != 't' && first != 'x' && first != 'y' && first != 'z'))
        goto done;

    if (string_cursor_next(cursor) != 0)
        goto done;
    if (string_cursor_done(cursor)) {
        ok = 1;
        goto done;
    }

    if (rune_is_equal(string_cursor_peek(cursor), '_')) {
        if (string_cursor_next(cursor) != 0)
            goto done;
        if (string_cursor_done(cursor)) {
            ok = 1;
            goto done;
        }
    }

    while (!string_cursor_done(cursor)) {
        if (!expr_tostring_rune_is_digit_suffix(string_cursor_peek(cursor)))
            goto done;
        if (string_cursor_next(cursor) != 0)
            goto done;
    }
    ok = 1;

done:
    string_cursor_free(cursor);
    string_free(text);
    return ok;
}

static int expr_tostring_name_starts_non_ascii(const char *name)
{
    string_t *text;
    string_cursor_t *cursor;
    int non_ascii = 0;

    if (!name || !*name)
        return 0;

    text = string_new_with(name);
    cursor = text ? string_cursor_new(text) : NULL;
    if (!cursor)
        goto done;

    non_ascii = !rune_to_ascii(string_cursor_peek(cursor), NULL);

done:
    string_cursor_free(cursor);
    string_free(text);
    return non_ascii;
}

static int expr_tostring_is_coefficient_var(const expr_t *f)
{
    return expr_is_var(f) && f->name && *f->name && !expr_tostring_is_primary_variable_name(f->name);
}

static int expr_tostring_is_coefficient_var_pow_d(const expr_t *f)
{
    return expr_is_pow_d_expr(f) && f->a && expr_tostring_is_coefficient_var(f->a);
}

bool display_poly_contains_indexed_arbitrary_constant(const expr_t *expr);
bool display_poly_is_indexed_arbitrary_constant(const expr_t *expr);

static bool expr_tostring_is_indexed_constant_times_variable_power(const expr_t *expr)
{
    const expr_t *variable_power;

    if (!expr_is_mul(expr))
        return false;
    if (display_poly_is_indexed_arbitrary_constant(expr->a))
        variable_power = expr->b;
    else if (display_poly_is_indexed_arbitrary_constant(expr->b))
        variable_power = expr->a;
    else
        return false;
    if (expr_is_var(variable_power))
        return variable_power->name && expr_tostring_is_primary_variable_name(variable_power->name);
    return expr_is_pow_d_expr(variable_power) && variable_power->a && expr_is_var(variable_power->a) &&
           variable_power->a->name && expr_tostring_is_primary_variable_name(variable_power->a->name);
}

static bool expr_tostring_is_explicit_arbitrary_amplitude_terms(const expr_t *expr)
{
    if (!expr)
        return false;
    if (expr_is_addsub(expr))
        return expr_tostring_is_explicit_arbitrary_amplitude_terms(expr->a) &&
               expr_tostring_is_explicit_arbitrary_amplitude_terms(expr->b);
    return display_poly_is_indexed_arbitrary_constant(expr) ||
           expr_tostring_is_indexed_constant_times_variable_power(expr);
}

static bool expr_tostring_is_explicit_arbitrary_amplitude(const expr_t *expr)
{
    const expr_t *first = expr;

    if (!expr || !expr_is_addsub(expr))
        return false;
    while (first && expr_is_addsub(first))
        first = first->a;
    return display_poly_is_indexed_arbitrary_constant(first) &&
           expr_tostring_is_explicit_arbitrary_amplitude_terms(expr);
}

/* Sort group for multiplication factors:
 *   0 = unnamed numeric constant       (e.g. 6)
 *   1 = Greek immortal named constant  (e.g. π)
 *   2 = Latin/other immortal constant  (e.g. e)
 *   3 = coefficient-like symbolic factor (e.g. H, δ, α²)
 *   4 = variable or var^n              (e.g. x, x³)
 *   5 = everything else (unary/binary fns) — sort by primary arg var name,
 *       stable so same-arg functions keep their original tree order
 */
static int factor_group(const expr_t *f)
{
    if (expr_is_neg(f))
        f = f->a;

    if (expr_is_op(f, &ops_indexed_symbol))
        return 3;
    if (expr_is_op(f, &ops_summation) || expr_is_op(f, &ops_product))
        return 4;
    if (expr_tostring_is_explicit_arbitrary_amplitude(f))
        return 4;

    if (expr_is_const(f)) {
        if (expr_is_preserved_ln10_const_local(f))
            return 5;
        if (!f->name || !*f->name)
            return 0;
        if (f->binding_expr && !expr_is_immortal_default_const_local(f))
            return 3;
        return expr_tostring_name_starts_non_ascii(f->name) ? 1 : 2;
    }

    if (expr_tostring_is_coefficient_var(f))
        return 3;

    if (expr_tostring_is_coefficient_var_pow_d(f))
        return 3;

    if (expr_is_var(f))
        return 4;

    if (expr_tostring_is_var_pow_d(f))
        return 4;

    if (expr_tostring_is_named_const_pow_d(f))
        return 3;

    return 5;
}

/* DFS to find the name of the first variable in an expression. */
static const char *first_var_name(const expr_t *f)
{
    if (!f)
        return "";
    if (expr_is_var(f))
        return f->name ? f->name : "";
    const char *a = first_var_name(f->a);
    if (*a)
        return a;
    return first_var_name(f->b);
}

/* Counts levels of function *nesting* (not tree depth).
 * pow_d and neg are transparent — cos²(x) has the same nesting depth as cos(x).
 * This makes cos²(x) (depth 1) sort before exp(sin(x)) (depth 2). */
static int factor_depth(const expr_t *f)
{
    if (!f || expr_is_const(f) || expr_is_var(f))
        return 0;
    if (expr_is_neg(f) || expr_is_pow_d_expr(f))
        return factor_depth(f->a);
    if (f->ops->arity == EXPR_OP_UNARY)
        return 1 + factor_depth(f->a);
    if (f->ops->arity == EXPR_OP_BINARY) {
        int da = factor_depth(f->a), db = factor_depth(f->b);
        return 1 + (da > db ? da : db);
    }
    return 0;
}

static const char *factor_sort_name(const expr_t *f)
{
    if (expr_is_neg(f))
        f = f->a;

    if (expr_is_const(f))
        return (f->name && *f->name) ? f->name : "";

    if (expr_is_var(f))
        return f->name ? f->name : "";

    if (expr_tostring_is_var_pow_d(f))
        return f->a->name ? f->a->name : "";

    if (expr_tostring_is_named_const_pow_d(f))
        return f->a->name ? f->a->name : "";

    /* Unary/binary functions: sort by the primary variable in the argument
     * so e.g. sin(x) and cos(y) sort by x vs y, not by function name.
     * Functions with the same primary variable keep their original order
     * (handled by the stable sort below). */
    return first_var_name(f->a);
}

static int factor_power_log_order(const expr_t *left, const expr_t *right)
{
    bool left_sum = expr_is_addsub(left);
    bool right_sum = expr_is_addsub(right);
    bool left_power = expr_is_op(left, &ops_pow) || expr_is_pow_d_expr(left);
    bool right_power = expr_is_op(right, &ops_pow) || expr_is_pow_d_expr(right);
    bool left_log = expr_is_op(left, &ops_log);
    bool right_log = expr_is_op(right, &ops_log);
    bool left_preserved_power = expr_is_unnamed_const(left) && left->binding_expr &&
                                left->binding_expr->kind == EXPR_BINDING_EXPR_BINARY_OP &&
                                left->binding_expr->u.binary_op.ops == &ops_pow;
    bool right_preserved_power = expr_is_unnamed_const(right) && right->binding_expr &&
                                 right->binding_expr->kind == EXPR_BINDING_EXPR_BINARY_OP &&
                                 right->binding_expr->u.binary_op.ops == &ops_pow;
    bool left_preserved_sqrt = expr_is_unnamed_const(left) && left->binding_expr &&
                               left->binding_expr->kind == EXPR_BINDING_EXPR_UNARY_OP &&
                               left->binding_expr->u.unary_op.ops == &ops_sqrt;
    bool right_preserved_sqrt = expr_is_unnamed_const(right) && right->binding_expr &&
                                right->binding_expr->kind == EXPR_BINDING_EXPR_UNARY_OP &&
                                right->binding_expr->u.unary_op.ops == &ops_sqrt;

    if (left_preserved_power && right_preserved_power) {
        number_t left_exponent = expr_binding_expr_eval(left->binding_expr->u.binary_op.right);
        number_t right_exponent = expr_binding_expr_eval(right->binding_expr->u.binary_op.right);
        bool left_symbolic = num_is_nan(left_exponent);
        bool right_symbolic = num_is_nan(right_exponent);

        num_destroy(&right_exponent);
        num_destroy(&left_exponent);
        if (left_symbolic != right_symbolic)
            return left_symbolic ? -1 : 1;
    }

    if (left_preserved_power && right_preserved_sqrt)
        return -1;
    if (left_preserved_sqrt && right_preserved_power)
        return 1;

    if (left_sum && right_power)
        return -1;
    if (left_power && right_sum)
        return 1;
    if (left_power && right_log)
        return -1;
    if (left_log && right_power)
        return 1;
    return 0;
}

/* Stable insertion sort for factor arrays. Within the function group, place powers before logarithms; otherwise sort
 * shallower expressions first so that, for example, cos(x) appears before exp(sin(x)). */
void sort_factors(expr_t **fac, int n)
{
    bool imaginary_unit_last =
        n > 1 && expr_is_const(fac[n - 1]) && (num_eq(fac[n - 1]->c, NUM_I) || num_eq(fac[n - 1]->c, NUM_NEG_I));

    for (int s = 1; s < n; s++) {
        expr_t *key = fac[s];
        int kg = factor_group(key);
        const char *kn = factor_sort_name(key);
        int kd = (kg == 5) ? factor_depth(key) : 0;
        int t = s - 1;
        while (t >= 0) {
            int tg = factor_group(fac[t]);
            int cmp = factor_power_log_order(fac[t], key);

            if (cmp != 0) {
                /* Keep this explicit algebraic preference across the broader factor groups. */
            } else if (tg != kg) {
                cmp = tg - kg;
            } else if (kg == 5) {
                cmp = factor_power_log_order(fac[t], key);

                if (cmp == 0) {
                    int td = factor_depth(fac[t]);

                    cmp = (td != kd) ? (td - kd) : strcmp(factor_sort_name(fac[t]), kn);
                }
            } else {
                cmp = strcmp(factor_sort_name(fac[t]), kn);
            }
            if (cmp <= 0)
                break;
            fac[t + 1] = fac[t];
            t--;
        }
        fac[t + 1] = key;
    }
    if (imaginary_unit_last) {
        for (int index = 0; index < n - 1; ++index) {
            if (expr_is_const(fac[index]) && (num_eq(fac[index]->c, NUM_I) || num_eq(fac[index]->c, NUM_NEG_I))) {
                expr_t *imaginary_unit = fac[index];

                for (int next = index; next < n - 1; ++next)
                    fac[next] = fac[next + 1];
                fac[n - 1] = imaginary_unit;
                break;
            }
        }
    }
}

/* ------------------------------------------------------------------------- */
/* EXPRESSION MODE (pretty maths)                                            */
/* ------------------------------------------------------------------------- */

void emit_expr(const expr_t *f, sbuf_t *b, int parent_prec);
void emit_expr_abs(const expr_t *f, sbuf_t *b, int parent_prec);
void emit_expr_abs_bars(const expr_t *f, sbuf_t *b);
void emit_TeX_expr(const expr_t *f, sbuf_t *b, int parent_prec);
void emit_TeX_expr_abs(const expr_t *f, sbuf_t *b, int parent_prec);
void emit_func(const expr_t *f, sbuf_t *b, int parent_prec);
void emit_func_abs(const expr_t *f, sbuf_t *b, int parent_prec);


bool emit_TeX_unit_fraction_power(const expr_t *base, number_t exponent, sbuf_t *b, int parent_prec)
{
    long numerator;
    long denominator;
    bool reciprocal;
    int need;

    /* Root(base^p, q) is not the principal base^(p/q) for general complex bases. */
    if (!num_get_small_rational(exponent, &numerator, &denominator) ||
        (numerator != 1L && numerator != -1L) || denominator <= 1L)
        return false;

    reciprocal = numerator < 0L;
    need = (reciprocal ? PREC_MUL : PREC_UNARY) < parent_prec;
    if (need)
        sbuf_puts(b, "\\left(");
    if (reciprocal)
        sbuf_puts(b, "\\frac{1}{");
    if (denominator == 2L) {
        sbuf_puts(b, "\\sqrt{");
    } else {
        char index_text[32];

        snprintf(index_text, sizeof(index_text), "%ld", denominator);
        sbuf_puts(b, "\\sqrt[");
        sbuf_puts(b, index_text);
        sbuf_puts(b, "]{");
    }
    emit_TeX_expr(base, b, PREC_LOWEST);
    sbuf_putc(b, '}');
    if (reciprocal)
        sbuf_putc(b, '}');
    if (need)
        sbuf_puts(b, "\\right)");
    return true;
}

void emit_expr_sqrt_power(const expr_t *base, sbuf_t *b, int parent_prec, bool reciprocal)
{
    if (reciprocal) {
        int need = PREC_MUL < parent_prec;

        if (need)
            sbuf_putc(b, '(');
        sbuf_puts(b, "1/√(");
        emit_expr(base, b, PREC_LOWEST);
        sbuf_putc(b, ')');
        if (need)
            sbuf_putc(b, ')');
        return;
    }

    {
        int need = PREC_UNARY < parent_prec;

        if (need)
            sbuf_putc(b, '(');
        sbuf_puts(b, "√(");
        emit_expr(base, b, PREC_LOWEST);
        sbuf_putc(b, ')');
        if (need)
            sbuf_putc(b, ')');
    }
}

bool match_atan_over_argument_denominator(const expr_t *expr, const expr_t **atan_expr_out,
                                                 const expr_t **denominator_out)
{
    if (!expr || !expr_is_op(expr, &ops_div) || !expr->a || !expr->b || !expr_is_op(expr->a, &ops_atan) ||
        !expr->a->a || !expr_is_op(expr->a->a, &ops_div) || !expr->a->a->a || !expr->a->a->b ||
        !expr_struct_eq(expr->a->a->b, expr->b))
        return false;

    if (atan_expr_out)
        *atan_expr_out = expr->a;
    if (denominator_out)
        *denominator_out = expr->b;
    return true;
}

bool match_sum_quotient(const expr_t *expr, const expr_t **factor_out, const expr_t **sum_out)
{
    if (!expr || !expr_is_op(expr, &ops_div) || !expr->a || !expr->b)
        return false;
    if (expr_is_addsub(expr->a)) {
        *factor_out = NULL;
        *sum_out = expr->a;
        return true;
    }
    if (expr_is_mul(expr->a) && expr_is_addsub(expr->a->a)) {
        *factor_out = expr->a->b;
        *sum_out = expr->a->a;
        return true;
    }
    if (expr_is_mul(expr->a) && expr_is_addsub(expr->a->b)) {
        *factor_out = expr->a->a;
        *sum_out = expr->a->b;
        return true;
    }
    return false;
}

int expr_is_negative(const expr_t *f)
{
    expr_t *fac[64];
    int n = 0;
    int sign = 0;

    if (!f)
        return 0;
    if (expr_tostring_is_negative_const(f) || expr_is_neg(f))
        return 1;
    if (expr_is_mul(f)) {
        flatten_mul((expr_t *)f, fac, &n, 64);
        for (int i = 0; i < n; ++i)
            sign ^= expr_is_negative(fac[i]) ? 1 : 0;
        return sign;
    }
    if (expr_is_op(f, &ops_div))
        return expr_is_negative(f->a) ^ expr_is_negative(f->b);
    return 0;
}

void emit_factor_abs(const expr_t *f, sbuf_t *b)
{
    if (expr_is_negative(f))
        emit_expr_abs(f, b, PREC_MUL);
    else
        emit_expr(f, b, PREC_MUL);
}

int expr_renders_negative(const expr_t *f)
{
    sbuf_t b;
    int neg;

    sbuf_init(&b);
    emit_expr(f, &b, 0);
    neg = (sbuf_len(&b) > 0u && rune_is_equal(string_at(b.text, 0u), '-'));
    sbuf_free(&b);
    return neg;
}


void emit_func_with_temporaries(const expr_t *f, sbuf_t *b, int parent_prec, const expr_t *const *nodes,
                                const char *const *names, size_t count, const expr_t *expanded_node)
{
    function_temporary_context_t previous = function_temporary_context;

    function_temporary_context.nodes = nodes;
    function_temporary_context.names = names;
    function_temporary_context.count = count;
    function_temporary_context.expanded_node = expanded_node;
    emit_func(f, b, parent_prec);
    function_temporary_context = previous;
}

/* Use the same structural abbreviations in mathematical output as in executable functions. */
void emit_math_with_temporaries(const expr_t *f, sbuf_t *b, style_t style, const expr_t *const *nodes,
                                const char *const *names, size_t count)
{
    function_temporary_context_t previous = function_temporary_context;
    bool previous_active = mathematical_temporaries_active;
    mathematical_temporaries_active = true;
    function_temporary_context.nodes = nodes;
    function_temporary_context.names = names;
    function_temporary_context.count = count;
    function_temporary_context.expanded_node = NULL;
    if (style == style_LATEX)
        emit_TeX_expr(f, b, PREC_LOWEST);
    else
        emit_expr(f, b, PREC_LOWEST);
    function_temporary_context = previous;
    mathematical_temporaries_active = previous_active;
}


static int function_fraction_digit(const char *text, const char *const digits[10], size_t *width)
{
    for (int digit = 0; digit < 10; ++digit) {
        size_t digit_width = strlen(digits[digit]);

        if (strncmp(text, digits[digit], digit_width) == 0) {
            if (width)
                *width = digit_width;
            return digit;
        }
    }
    return -1;
}

size_t function_ascii_stacked_fraction(char *out, const char *text)
{
    const char *cursor = text;
    char *output = out;
    size_t digit_width = 0u;
    int digit;

    while ((digit = function_fraction_digit(cursor, sup_digits, &digit_width)) >= 0) {
        *output++ = (char)('0' + digit);
        cursor += digit_width;
    }
    if (output == out || strncmp(cursor, "⁄", strlen("⁄")) != 0)
        return 0u;

    *output++ = '/';
    cursor += strlen("⁄");
    char *denominator = output;
    while ((digit = function_fraction_digit(cursor, sub_digits, &digit_width)) >= 0) {
        *output++ = (char)('0' + digit);
        cursor += digit_width;
    }
    if (output == denominator)
        return 0u;

    *output = '\0';
    return (size_t)(cursor - text);
}

size_t function_ascii_vulgar_fraction(char *out, const char *text)
{
    static const struct {
        const char *unicode;
        const char *ascii;
    } fractions[] = {
        {"¼", "1/4"}, {"½", "1/2"}, {"¾", "3/4"}, {"⅐", "1/7"}, {"⅑", "1/9"}, {"⅒", "1/10"},
        {"⅓", "1/3"}, {"⅔", "2/3"}, {"⅕", "1/5"}, {"⅖", "2/5"}, {"⅗", "3/5"}, {"⅘", "4/5"},
        {"⅙", "1/6"}, {"⅚", "5/6"}, {"⅛", "1/8"}, {"⅜", "3/8"}, {"⅝", "5/8"}, {"⅞", "7/8"},
    };

    for (size_t i = 0u; i < sizeof(fractions) / sizeof(fractions[0]); ++i) {
        size_t width = strlen(fractions[i].unicode);

        if (strncmp(text, fractions[i].unicode, width) == 0) {
            strcpy(out, fractions[i].ascii);
            return width;
        }
    }
    return 0u;
}


/* Keep temporary expansion state private to the common emitter. */
const expr_t *expr_stringout_set_expanded_node(const expr_t *node)
{
    const expr_t *previous = function_temporary_context.expanded_node;
    function_temporary_context.expanded_node = node;
    return previous;
}

const char *expr_math_temporary_name(const expr_t *expr)
{
    return mathematical_temporaries_active ? function_temporary_name(expr) : NULL;
}
