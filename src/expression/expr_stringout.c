/* expr_stringout.c - symbolic/string conversion for expr_t
 *
 * Produces human-readable string representations of a expr_t DAG via
 * expr_to_text(dv, style).  Four styles are supported:
 *
 *   style_EXPRESSION  — infix notation, e.g.
 *                         { sin(x)·cos(y) | x = 1, y = ½π }
 *                       or, when no bindings are needed:
 *                         sin(x)·cos(y)
 *                       This is the preferred round-trip format accepted by
 *                       expr_from_string().
 *
 *   style_UNBOUND     — the same infix expression body before the
 *                       { expr | bindings } wrapper is added, e.g.
 *                         sin(x)·cos(y)
 *
 *   style_FUNCTION    — MARS function notation, e.g.
 *                         expression expr(x, y, const c₀) {
 *                             return sin(x).cos(y).
 *                         }
 *
 *                         output(expr(x, y, c₀)).
 *                       Useful for debugging graph structure and generated
 *                       callable forms.
 *
 *   style_LATEX         — native TeX notation generated directly from the DAG,
 *                       e.g.
 *                         \left\{ \sin(x)\cos(y) \;\middle|\;
 *                         x = 1, y = \frac{\pi}{2} \right\}
 *
 * Responsibilities of this file:
 *   • Operator precedence and parenthesisation (infix only)
 *   • Unicode superscripts and fraction glyphs for compact powers/coefficients
 *   • Shared low-level expression, TeX, and function-body emitters
 *
 * Style-specific wrappers live in expr_stringout_expr.c,
 * expr_stringout_TeX.c, and expr_stringout_func.c.
 *
 * Algebraic simplification is deliberately not part of ordinary rendering:
 * callers see the expression shape they built or parsed.  Owning derivative
 * creation simplifies derivatives in the DAG before they are rendered.
 */

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

/* ------------------------------------------------------------------------- */
/* Growable string buffer                                                    */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* Precedence and superscripts                                               */
/* ------------------------------------------------------------------------- */

static const char *sup_digits[10] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};

static const char *sub_digits[10] = {"₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉"};

static int expr_append_padding(string_t *out, int count)
{
    for (int i = 0; i < count; ++i) {
        if (string_append_char(out, ' ') != 0)
            return -1;
    }
    return 0;
}

static style_t expr_format_style(const string_format_spec_t *spec, string_format_result_t *result)
{
    if (!spec || !result)
        return style_EXPRESSION;

    switch (spec->trailing_modifier) {
        case 'u':
        case 'U':
            *result = STRING_FORMAT_HANDLED_WITH_TRAILING_MODIFIER;
            return style_UNBOUND;
        case 't':
        case 'T':
            *result = STRING_FORMAT_HANDLED_WITH_TRAILING_MODIFIER;
            return style_LATEX;
        case 'f':
        case 'F':
            *result = STRING_FORMAT_HANDLED_WITH_TRAILING_MODIFIER;
            return style_FUNCTION;
        default:
            return style_EXPRESSION;
    }
}

static string_format_result_t expr_format_callback(string_t *out, const string_format_spec_t *spec, va_list ap,
                                                   void *user)
{
    bool left;
    bool old_scientific;
    bool scientific;
    int width;
    int old_precision;
    int precision;
    int pad;
    size_t text_len;
    const expr_t *expr;
    string_t *text;
    style_t style;
    string_format_result_t result = STRING_FORMAT_HANDLED;

    (void)user;

    if (!out || !spec || (spec->conversion != 'n' && spec->conversion != 'N'))
        return STRING_FORMAT_UNHANDLED;

    width = spec->width;
    left = spec->flag_left;
    if (spec->width_from_argument) {
        width = va_arg(ap, int);
        if (width < 0) {
            left = true;
            width = -width;
        }
    }
    precision = spec->precision;
    if (spec->precision_from_argument) {
        precision = va_arg(ap, int);
        if (precision < 0)
            precision = -1;
    }

    style = expr_format_style(spec, &result);
    scientific = spec->conversion == 'N';
    expr = va_arg(ap, const expr_t *);
    old_scientific = expr_set_number_scientific_local(scientific);
    old_precision = expr_set_number_precision_local(precision);
    text = expr_to_text(expr, style);
    expr_set_number_precision_local(old_precision);
    expr_set_number_scientific_local(old_scientific);
    if (!text)
        return STRING_FORMAT_ERROR;

    text_len = string_length(text);
    pad = width > (int)text_len ? width - (int)text_len : 0;

    if (!left && expr_append_padding(out, pad) != 0)
        goto fail;
    if (string_append_string(out, text) != 0)
        goto fail;
    if (left && expr_append_padding(out, pad) != 0)
        goto fail;

    string_free(text);
    return result;

fail:
    string_free(text);
    return STRING_FORMAT_ERROR;
}

static void emit_superscript_int(sbuf_t *b, long n)
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

static void emit_subscript_int(sbuf_t *b, long n)
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

static bool expr_try_get_small_integer_exponent(number_t value, long *out)
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

static int expr_tostring_should_emit_binding_expr(const expr_t *f)
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

static void emit_atom(expr_t *f, sbuf_t *b)
{
    if (expr_is_const(f)) {
        if (expr_tostring_should_emit_binding_expr(f)) {
            char *text = expr_binding_expr_to_string(f->binding_expr);

            if (text) {
                sbuf_puts(b, text);
                free(text);
            }
        } else if (f->name && *f->name) {
            emit_name(b, f->name);
        } else {
            char *text = expr_const_to_string_local(f);
            if (text) {
                sbuf_puts(b, text);
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

static bool emit_negative_const_binding_expr_abs(const expr_t *f, sbuf_t *b, bool tex)
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
static int pow_exp_needs_parens(const expr_t *e)
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

static int pow_base_needs_visible_parens(const expr_t *base)
{
    number_t real;
    int has_real_part;

    if (base && (expr_is_formal_derivative(base) || expr_is_pow_d_expr(base) || expr_is_op(base, &ops_pow)))
        return 1;

    if (base && expr_is_const(base) && expr_tostring_should_emit_binding_expr(base) && base->binding_expr) {
        if (base->binding_expr->kind == EXPR_BINDING_EXPR_ADD || base->binding_expr->kind == EXPR_BINDING_EXPR_SUB)
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
    real = num_real_part(base->c);
    has_real_part = !num_eq(real, NUM_ZERO);
    num_destroy(&real);
    return has_real_part;
}

static int mul_factor_needs_visible_parens(const expr_t *factor)
{
    number_t real;
    int has_real_part;

    if (factor && expr_is_const(factor) && expr_tostring_should_emit_binding_expr(factor) && factor->binding_expr) {
        if (factor->binding_expr->kind == EXPR_BINDING_EXPR_ADD || factor->binding_expr->kind == EXPR_BINDING_EXPR_SUB)
            return 1;
        if (factor->binding_expr->kind == EXPR_BINDING_EXPR_UNARY_OP ||
            factor->binding_expr->kind == EXPR_BINDING_EXPR_BINARY_OP ||
            factor->binding_expr->kind == EXPR_BINDING_EXPR_POWI)
            return 0;
    }

    if (!factor || !expr_is_const(factor) || num_is_real(factor->c))
        return 0;

    real = num_real_part(factor->c);
    has_real_part = !num_eq(real, NUM_ZERO);
    num_destroy(&real);
    return has_real_part;
}

static int add_rhs_needs_visible_parens(const expr_t *rhs)
{
    if ((rhs && expr_is_const(rhs) && mul_factor_needs_visible_parens(rhs)) ||
        (expr_is_neg(rhs) && rhs->a && expr_is_const(rhs->a) && mul_factor_needs_visible_parens(rhs->a)))
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

static bool expr_is_const_half_local(const expr_t *f)
{
    return f && expr_is_const(f) && num_eq(f->c, NUM_HALF);
}

static bool number_is_neg_half_local(number_t value)
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

static bool expr_const_half_can_render_as_sqrt_local(const expr_t *f)
{
    return expr_is_const_half_local(f) && (!f->name || !*f->name) && !f->binding_expr;
}

static bool expr_const_neg_half_can_render_as_sqrt_local(const expr_t *f)
{
    return expr_is_const_neg_half_local(f) && (!f->name || !*f->name) && !f->binding_expr;
}

static int is_atomic_for_mul(const expr_t *f);

static void emit_expr_mul_separator_local(const expr_t *left, const expr_t *right, sbuf_t *b)
{
    int left_atomic;
    int right_atomic;

    left_atomic = is_atomic_for_mul(left);
    right_atomic = is_atomic_for_mul(right);
    if (!(left_atomic && right_atomic))
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

static void flatten_mul(expr_t *f, expr_t **buf, int *count, int max)
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

static const char *function_temporary_name(const expr_t *expr)
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

static void flatten_func_mul(expr_t *f, expr_t **buf, int *count, int max)
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

static const char *function_factored_temporary_name(const expr_t *product, const expr_t *denominator,
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

static int expr_tostring_is_primary_variable_name(const char *name)
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

static bool display_poly_contains_indexed_arbitrary_constant(const expr_t *expr);
static bool display_poly_is_indexed_arbitrary_constant(const expr_t *expr);

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
static void sort_factors(expr_t **fac, int n)
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
static void emit_expr_abs(const expr_t *f, sbuf_t *b, int parent_prec);
static void emit_expr_abs_bars(const expr_t *f, sbuf_t *b);
void emit_TeX_expr(const expr_t *f, sbuf_t *b, int parent_prec);
static void emit_TeX_expr_abs(const expr_t *f, sbuf_t *b, int parent_prec);
void emit_func(const expr_t *f, sbuf_t *b, int parent_prec);
static void emit_func_abs(const expr_t *f, sbuf_t *b, int parent_prec);

static bool expr_stringout_tree_has_symbol_name(const expr_t *expr, const char *name)
{
    if (!expr || !name)
        return false;
    if (expr->name && strcmp(expr->name, name) == 0)
        return true;
    return expr_stringout_tree_has_symbol_name(expr->a, name) || expr_stringout_tree_has_symbol_name(expr->b, name);
}

static void emit_TeX_log_cartesian_real(const expr_t *real, const expr_t *imaginary, sbuf_t *b)
{
    sbuf_puts(b, "\\frac{1}{2}\\ln\\left(");
    emit_TeX_expr(real, b, PREC_POW);
    sbuf_puts(b, "^{2}+");
    emit_TeX_expr(imaginary, b, PREC_POW);
    sbuf_puts(b, "^{2}\\right)");
}

static void emit_TeX_cartesian_norm_squared(const expr_t *real, const expr_t *imaginary, sbuf_t *b)
{
    emit_TeX_expr(real, b, PREC_POW);
    sbuf_puts(b, "^{2}+");
    emit_TeX_expr(imaginary, b, PREC_POW);
    sbuf_puts(b, "^{2}");
}

static void emit_TeX_log_cartesian_imaginary(const expr_t *real, const expr_t *imaginary, sbuf_t *b)
{
    sbuf_puts(b, "\\operatorname{atan2}(");
    emit_TeX_expr(imaginary, b, PREC_LOWEST);
    sbuf_putc(b, ',');
    emit_TeX_expr(real, b, PREC_LOWEST);
    sbuf_putc(b, ')');
}

static void emit_TeX_log_cartesian_norm_squared(const expr_t *real, const expr_t *imaginary, sbuf_t *b)
{
    sbuf_puts(b, "\\left(");
    emit_TeX_log_cartesian_real(real, imaginary, b);
    sbuf_puts(b, "\\right)^{2}+\\operatorname{atan2}^{2}(");
    emit_TeX_expr(imaginary, b, PREC_LOWEST);
    sbuf_putc(b, ',');
    emit_TeX_expr(real, b, PREC_LOWEST);
    sbuf_putc(b, ')');
}

static void emit_TeX_log_cartesian_argument(const expr_t *real, const expr_t *imaginary, sbuf_t *b)
{
    sbuf_puts(b, "\\operatorname{atan2}\\left(");
    emit_TeX_log_cartesian_imaginary(real, imaginary, b);
    sbuf_putc(b, ',');
    emit_TeX_log_cartesian_real(real, imaginary, b);
    sbuf_puts(b, "\\right)");
}

static bool emit_TeX_logarithmic_integral_cartesian(const expr_t *f, sbuf_t *b)
{
    static const char *index_names[] = {"n", "m", "k", "l", "j"};
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    bool has_imaginary = false;
    const char *index = "j";

    if (!f || !expr_is_op(f, &ops_Li) ||
        !expr_cartesian_parts_for_display(f->a, &real, &imaginary, &has_imaginary) || !has_imaginary) {
        expr_free(imaginary);
        expr_free(real);
        return false;
    }
    for (size_t i = 0u; i < sizeof(index_names) / sizeof(index_names[0]); ++i) {
        if (!expr_stringout_tree_has_symbol_name(f, index_names[i])) {
            index = index_names[i];
            break;
        }
    }

    sbuf_puts(b, "\\begin{aligned}\n\\operatorname{Li}(");
    emit_TeX_expr(f->a, b, PREC_LOWEST);
    sbuf_puts(b, ")&=p+q\\mkern-2mu i,\\\\\np&=\\gamma+\\frac{1}{2}\\ln\\left(");
    emit_TeX_log_cartesian_norm_squared(real, imaginary, b);
    sbuf_puts(b, "\\right)+\\sum_{");
    sbuf_puts(b, index);
    sbuf_puts(b, "=1}^{\\infty}\\frac{\\left(");
    emit_TeX_log_cartesian_norm_squared(real, imaginary, b);
    sbuf_puts(b, "\\right)^{");
    sbuf_puts(b, index);
    sbuf_puts(b, "/2}\\cos\\left(");
    sbuf_puts(b, index);
    emit_TeX_log_cartesian_argument(real, imaginary, b);
    sbuf_puts(b, "\\right)}{");
    sbuf_puts(b, index);
    sbuf_puts(b, "\\,");
    sbuf_puts(b, index);
    sbuf_puts(b, "!},\\\\\nq&=");
    emit_TeX_log_cartesian_argument(real, imaginary, b);
    sbuf_puts(b, "+\\sum_{");
    sbuf_puts(b, index);
    sbuf_puts(b, "=1}^{\\infty}\\frac{\\left(");
    emit_TeX_log_cartesian_norm_squared(real, imaginary, b);
    sbuf_puts(b, "\\right)^{");
    sbuf_puts(b, index);
    sbuf_puts(b, "/2}\\sin\\left(");
    sbuf_puts(b, index);
    emit_TeX_log_cartesian_argument(real, imaginary, b);
    sbuf_puts(b, "\\right)}{");
    sbuf_puts(b, index);
    sbuf_puts(b, "\\,");
    sbuf_puts(b, index);
    sbuf_puts(b, "!}\\end{aligned}");

    expr_free(imaginary);
    expr_free(real);
    return true;
}

static bool emit_TeX_exponential_integral_cartesian(const expr_t *f, sbuf_t *b)
{
    static const char *index_names[] = {"n", "m", "k", "l", "j"};
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    bool has_imaginary = false;
    const char *index = "j";

    if (!f || !expr_is_op(f, &ops_Ei) ||
        !expr_cartesian_parts_for_display(f->a, &real, &imaginary, &has_imaginary) || !has_imaginary) {
        expr_free(imaginary);
        expr_free(real);
        return false;
    }
    for (size_t i = 0u; i < sizeof(index_names) / sizeof(index_names[0]); ++i) {
        if (!expr_stringout_tree_has_symbol_name(f, index_names[i])) {
            index = index_names[i];
            break;
        }
    }

    sbuf_puts(b, "\\begin{aligned}\n\\operatorname{Ei}(");
    emit_TeX_expr(f->a, b, PREC_LOWEST);
    sbuf_puts(b, ")&=p+q\\mkern-2mu i,\\\\\np&=\\gamma+\\frac{1}{2}\\ln\\left(");
    emit_TeX_cartesian_norm_squared(real, imaginary, b);
    sbuf_puts(b, "\\right)+\\sum_{");
    sbuf_puts(b, index);
    sbuf_puts(b, "=1}^{\\infty}\\frac{\\left(");
    emit_TeX_cartesian_norm_squared(real, imaginary, b);
    sbuf_puts(b, "\\right)^{");
    sbuf_puts(b, index);
    sbuf_puts(b, "/2}\\cos\\left(");
    sbuf_puts(b, index);
    sbuf_puts(b, "\\operatorname{atan2}(");
    emit_TeX_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ",");
    emit_TeX_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, ")\\right)}{");
    sbuf_puts(b, index);
    sbuf_puts(b, "\\,");
    sbuf_puts(b, index);
    sbuf_puts(b, "!},\\\\\nq&=\\operatorname{atan2}(");
    emit_TeX_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ",");
    emit_TeX_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, ")+\\sum_{");
    sbuf_puts(b, index);
    sbuf_puts(b, "=1}^{\\infty}\\frac{\\left(");
    emit_TeX_cartesian_norm_squared(real, imaginary, b);
    sbuf_puts(b, "\\right)^{");
    sbuf_puts(b, index);
    sbuf_puts(b, "/2}\\sin\\left(");
    sbuf_puts(b, index);
    sbuf_puts(b, "\\operatorname{atan2}(");
    emit_TeX_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ",");
    emit_TeX_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, ")\\right)}{");
    sbuf_puts(b, index);
    sbuf_puts(b, "\\,");
    sbuf_puts(b, index);
    sbuf_puts(b, "!}\\end{aligned}");

    expr_free(imaginary);
    expr_free(real);
    return true;
}

static bool expr_log_cartesian_parts_for_output(const expr_t *argument, expr_t **real_out, expr_t **imaginary_out);

static void emit_expr_cartesian_norm_squared(const expr_t *real, const expr_t *imaginary, sbuf_t *b)
{
    emit_expr(real, b, PREC_POW);
    sbuf_puts(b, "² + ");
    emit_expr(imaginary, b, PREC_POW);
    sbuf_puts(b, "²");
}

static bool emit_expr_exponential_integral_cartesian(const expr_t *f, sbuf_t *b, int parent_prec)
{
    static const char *index_names[] = {"n", "m", "k", "l", "j"};
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    bool has_imaginary = false;
    const char *index = "j";
    bool parenthesise = PREC_ADD < parent_prec;

    if (!f || (!expr_is_op(f, &ops_Ei) && !expr_is_op(f, &ops_Li)) ||
        !(expr_is_op(f, &ops_Li) ? expr_log_cartesian_parts_for_output(f->a, &real, &imaginary)
                                 : expr_cartesian_parts_for_display(f->a, &real, &imaginary, &has_imaginary)) ||
        (expr_is_op(f, &ops_Ei) && !has_imaginary)) {
        expr_free(imaginary);
        expr_free(real);
        return false;
    }
    for (size_t i = 0u; i < sizeof(index_names) / sizeof(index_names[0]); ++i) {
        if (!expr_stringout_tree_has_symbol_name(f, index_names[i])) {
            index = index_names[i];
            break;
        }
    }
    if (parenthesise)
        sbuf_putc(b, '(');
    sbuf_puts(b, "γ + ½ln(");
    emit_expr_cartesian_norm_squared(real, imaginary, b);
    sbuf_puts(b, ") + Σ_(");
    sbuf_puts(b, index);
    sbuf_puts(b, "=1)^∞ (");
    emit_expr_cartesian_norm_squared(real, imaginary, b);
    sbuf_puts(b, ")^(");
    sbuf_puts(b, index);
    sbuf_puts(b, "/2)cos(");
    sbuf_puts(b, index);
    sbuf_puts(b, "·atan2(");
    emit_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, "))/(");
    sbuf_puts(b, index);
    sbuf_puts(b, "·");
    sbuf_puts(b, index);
    sbuf_puts(b, "!) + (atan2(");
    emit_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, ") + Σ_(");
    sbuf_puts(b, index);
    sbuf_puts(b, "=1)^∞ (");
    emit_expr_cartesian_norm_squared(real, imaginary, b);
    sbuf_puts(b, ")^(");
    sbuf_puts(b, index);
    sbuf_puts(b, "/2)sin(");
    sbuf_puts(b, index);
    sbuf_puts(b, "·atan2(");
    emit_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, "))/(");
    sbuf_puts(b, index);
    sbuf_puts(b, "·");
    sbuf_puts(b, index);
    sbuf_puts(b, "!))i");
    if (parenthesise)
        sbuf_putc(b, ')');
    expr_free(imaginary);
    expr_free(real);
    return true;
}

static bool emit_func_exponential_integral_cartesian(const expr_t *f, sbuf_t *b, int parent_prec)
{
    static const char *index_names[] = {"n", "m", "k", "l", "j"};
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    bool has_imaginary = false;
    const char *index = "j";
    bool parenthesise = PREC_ADD < parent_prec;

    if (!f || (!expr_is_op(f, &ops_Ei) && !expr_is_op(f, &ops_Li)) ||
        !(expr_is_op(f, &ops_Li) ? expr_log_cartesian_parts_for_output(f->a, &real, &imaginary)
                                 : expr_cartesian_parts_for_display(f->a, &real, &imaginary, &has_imaginary)) ||
        (expr_is_op(f, &ops_Ei) && !has_imaginary)) {
        expr_free(imaginary);
        expr_free(real);
        return false;
    }
    for (size_t i = 0u; i < sizeof(index_names) / sizeof(index_names[0]); ++i) {
        if (!expr_stringout_tree_has_symbol_name(f, index_names[i])) {
            index = index_names[i];
            break;
        }
    }
    if (parenthesise)
        sbuf_putc(b, '(');
    sbuf_puts(b, "@gamma + ln(");
    emit_func(real, b, PREC_LOWEST);
    sbuf_puts(b, "^2 + ");
    emit_func(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, "^2)/2 + sum(");
    sbuf_puts(b, index);
    sbuf_puts(b, ", 1, @inf, (");
    emit_func(real, b, PREC_LOWEST);
    sbuf_puts(b, "^2 + ");
    emit_func(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, "^2)^(");
    sbuf_puts(b, index);
    sbuf_puts(b, "/2).cos(");
    sbuf_puts(b, index);
    sbuf_puts(b, ".atan2(");
    emit_func(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_func(real, b, PREC_LOWEST);
    sbuf_puts(b, "))/(");
    sbuf_puts(b, index);
    sbuf_puts(b, ".factorial(");
    sbuf_puts(b, index);
    sbuf_puts(b, "))) + (atan2(");
    emit_func(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_func(real, b, PREC_LOWEST);
    sbuf_puts(b, ") + sum(");
    sbuf_puts(b, index);
    sbuf_puts(b, ", 1, @inf, (");
    emit_func(real, b, PREC_LOWEST);
    sbuf_puts(b, "^2 + ");
    emit_func(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, "^2)^(");
    sbuf_puts(b, index);
    sbuf_puts(b, "/2).sin(");
    sbuf_puts(b, index);
    sbuf_puts(b, ".atan2(");
    emit_func(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_func(real, b, PREC_LOWEST);
    sbuf_puts(b, "))/(");
    sbuf_puts(b, index);
    sbuf_puts(b, ".factorial(");
    sbuf_puts(b, index);
    sbuf_puts(b, ")))).i");
    if (parenthesise)
        sbuf_putc(b, ')');
    expr_free(imaginary);
    expr_free(real);
    return true;
}

static expr_t *expr_log_cartesian_for_output(const expr_t *argument)
{
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    expr_t *real_squared = NULL;
    expr_t *imaginary_squared = NULL;
    expr_t *norm_squared = NULL;
    expr_t *norm_log = NULL;
    expr_t *half = NULL;
    expr_t *real_part = NULL;
    expr_t *imaginary_part = NULL;
    expr_t *imaginary_unit = NULL;
    expr_t *imaginary_term = NULL;
    expr_t *result = NULL;
    bool has_imaginary = false;

    if (!expr_cartesian_parts_for_display(argument, &real, &imaginary, &has_imaginary) || !has_imaginary)
        goto cleanup;
    real_squared = expr_pow(real, &NUM_TWO);
    imaginary_squared = expr_pow(imaginary, &NUM_TWO);
    norm_squared = real_squared && imaginary_squared ? expr_add(real_squared, imaginary_squared) : NULL;
    norm_log = norm_squared ? expr_log(norm_squared) : NULL;
    half = expr_new_const(NUM_HALF);
    real_part = half && norm_log ? expr_mul(half, norm_log) : NULL;
    imaginary_part = expr_atan2(imaginary, real);
    imaginary_unit = expr_new_named_const(NUM_I, "i");
    imaginary_term = imaginary_part && imaginary_unit ? expr_mul(imaginary_part, imaginary_unit) : NULL;
    result = real_part && imaginary_term ? expr_add(real_part, imaginary_term) : NULL;

cleanup:
    expr_free(imaginary_term);
    expr_free(imaginary_unit);
    expr_free(imaginary_part);
    expr_free(real_part);
    expr_free(half);
    expr_free(norm_log);
    expr_free(norm_squared);
    expr_free(imaginary_squared);
    expr_free(real_squared);
    expr_free(imaginary);
    expr_free(real);
    return result;
}

static bool expr_log_cartesian_parts_for_output(const expr_t *argument, expr_t **real_out, expr_t **imaginary_out)
{
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    expr_t *real_squared = NULL;
    expr_t *imaginary_squared = NULL;
    expr_t *norm_squared = NULL;
    expr_t *norm_log = NULL;
    expr_t *half = NULL;
    bool has_imaginary = false;
    bool ok = false;

    if (!real_out || !imaginary_out ||
        !expr_cartesian_parts_for_display(argument, &real, &imaginary, &has_imaginary) || !has_imaginary)
        goto cleanup;
    real_squared = expr_pow(real, &NUM_TWO);
    imaginary_squared = expr_pow(imaginary, &NUM_TWO);
    norm_squared = real_squared && imaginary_squared ? expr_add(real_squared, imaginary_squared) : NULL;
    norm_log = norm_squared ? expr_log(norm_squared) : NULL;
    half = expr_new_const(NUM_HALF);
    *real_out = half && norm_log ? expr_mul(half, norm_log) : NULL;
    *imaginary_out = expr_atan2(imaginary, real);
    ok = *real_out && *imaginary_out;

cleanup:
    if (!ok) {
        expr_free(*imaginary_out);
        expr_free(*real_out);
        *imaginary_out = NULL;
        *real_out = NULL;
    }
    expr_free(half);
    expr_free(norm_log);
    expr_free(norm_squared);
    expr_free(imaginary_squared);
    expr_free(real_squared);
    expr_free(imaginary);
    expr_free(real);
    return ok;
}

static bool emit_expr_integral_cartesian(const expr_t *f, sbuf_t *b, int parent_prec)
{
    expr_t *cartesian_logarithm;
    expr_t *exponential_integral;
    bool emitted;

    if (emit_expr_exponential_integral_cartesian(f, b, parent_prec))
        return true;
    if (!f || !expr_is_op(f, &ops_Li) || !f->a)
        return false;
    cartesian_logarithm = expr_log_cartesian_for_output(f->a);
    exponential_integral = cartesian_logarithm ? expr_Ei(cartesian_logarithm) : NULL;
    emitted = exponential_integral &&
              emit_expr_exponential_integral_cartesian(exponential_integral, b, parent_prec);
    expr_free(exponential_integral);
    expr_free(cartesian_logarithm);
    return emitted;
}

static bool emit_func_integral_cartesian(const expr_t *f, sbuf_t *b, int parent_prec)
{
    expr_t *cartesian_logarithm;
    expr_t *exponential_integral;
    bool emitted;

    if (emit_func_exponential_integral_cartesian(f, b, parent_prec))
        return true;
    if (!f || !expr_is_op(f, &ops_Li) || !f->a)
        return false;
    cartesian_logarithm = expr_log_cartesian_for_output(f->a);
    exponential_integral = cartesian_logarithm ? expr_Ei(cartesian_logarithm) : NULL;
    emitted = exponential_integral &&
              emit_func_exponential_integral_cartesian(exponential_integral, b, parent_prec);
    expr_free(exponential_integral);
    expr_free(cartesian_logarithm);
    return emitted;
}

bool emit_func_integral_cartesian_body(const expr_t *f, sbuf_t *b)
{
    static const char *index_names[] = {"n", "m", "k", "l", "j"};
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    bool has_imaginary = false;
    bool logarithmic_integral;
    bool cache_real_part;
    const char *index = "j";
    const char *radius;
    const char *angle;
    const char *log_real;
    const char *log_radius;
    const char *log_angle;
    const char *series_term;
    const char *series_angle;

    if (!f || (!expr_is_op(f, &ops_Ei) && !expr_is_op(f, &ops_Li)) ||
        !expr_cartesian_parts_for_display(f->a, &real, &imaginary, &has_imaginary) || !has_imaginary) {
        expr_free(imaginary);
        expr_free(real);
        return false;
    }
    for (size_t i = 0u; i < sizeof(index_names) / sizeof(index_names[0]); ++i) {
        if (!expr_stringout_tree_has_symbol_name(f, index_names[i])) {
            index = index_names[i];
            break;
        }
    }
    logarithmic_integral = expr_is_op(f, &ops_Li);
    cache_real_part = !expr_is_var(real);
    if (cache_real_part) {
        sbuf_puts(b, "    v1 = ");
        emit_func(real, b, PREC_LOWEST);
        sbuf_puts(b, ".\n");
    }
    radius = cache_real_part ? "v2" : "v1";
    angle = cache_real_part ? "v3" : "v2";
    log_real = cache_real_part ? "v4" : "v3";
    log_radius = cache_real_part ? "v5" : "v4";
    log_angle = cache_real_part ? "v6" : "v5";
    series_term = cache_real_part ? (logarithmic_integral ? "v7" : "v4")
                                         : (logarithmic_integral ? "v6" : "v3");
    series_angle = cache_real_part ? (logarithmic_integral ? "v8" : "v5")
                                          : (logarithmic_integral ? "v7" : "v4");
    sbuf_puts(b, "    ");
    sbuf_puts(b, radius);
    sbuf_puts(b, " = ");
    if (cache_real_part)
        sbuf_puts(b, "v1");
    else
        emit_func(real, b, PREC_POW);
    sbuf_puts(b, "^2 + ");
    emit_func(imaginary, b, PREC_POW);
    sbuf_puts(b, "^2.\n    ");
    sbuf_puts(b, angle);
    sbuf_puts(b, " = atan2(");
    emit_func(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ", ");
    if (cache_real_part)
        sbuf_puts(b, "v1");
    else
        emit_func(real, b, PREC_LOWEST);
    sbuf_puts(b, ").\n");
    if (logarithmic_integral) {
        sbuf_puts(b, "    ");
        sbuf_puts(b, log_real);
        sbuf_puts(b, " = ln(");
        sbuf_puts(b, radius);
        sbuf_puts(b, ")/2.\n    ");
        sbuf_puts(b, log_radius);
        sbuf_puts(b, " = ");
        sbuf_puts(b, log_real);
        sbuf_puts(b, "^2 + ");
        sbuf_puts(b, angle);
        sbuf_puts(b, "^2.\n    ");
        sbuf_puts(b, log_angle);
        sbuf_puts(b, " = atan2(");
        sbuf_puts(b, angle);
        sbuf_puts(b, ", ");
        sbuf_puts(b, log_real);
        sbuf_puts(b, ").\n");
    }
    sbuf_puts(b, "    ");
    sbuf_puts(b, series_term);
    sbuf_puts(b, " = ");
    sbuf_puts(b, logarithmic_integral ? log_radius : radius);
    sbuf_puts(b, "^(");
    sbuf_puts(b, index);
    sbuf_puts(b, "/2)/(");
    sbuf_puts(b, index);
    sbuf_puts(b, ".factorial(");
    sbuf_puts(b, index);
    sbuf_puts(b, ")).\n");
    sbuf_puts(b, "    ");
    sbuf_puts(b, series_angle);
    sbuf_puts(b, " = ");
    sbuf_puts(b, index);
    sbuf_putc(b, '.');
    sbuf_puts(b, logarithmic_integral ? log_angle : angle);
    sbuf_puts(b, ".\n");
    sbuf_puts(b, "    p = @gamma + ln(");
    sbuf_puts(b, logarithmic_integral ? log_radius : radius);
    sbuf_puts(b, ")/2 + sum(");
    sbuf_puts(b, index);
    sbuf_puts(b, ", 1, @inf, ");
    sbuf_puts(b, series_term);
    sbuf_puts(b, ".cos(");
    sbuf_puts(b, series_angle);
    sbuf_puts(b, ")).\n    q = ");
    sbuf_puts(b, logarithmic_integral ? log_angle : angle);
    sbuf_puts(b, " + sum(");
    sbuf_puts(b, index);
    sbuf_puts(b, ", 1, @inf, ");
    sbuf_puts(b, series_term);
    sbuf_puts(b, ".sin(");
    sbuf_puts(b, series_angle);
    sbuf_puts(b, ")).\n\n    return p + q.i.\n");
    expr_free(imaginary);
    expr_free(real);
    return true;
}

static bool expr_kind_has_analytic_cartesian_series(expr_op_kind_t kind)
{
    static const bool supported[EXPR_KIND_COUNT] = {
        [EXPR_KIND_SQRT]              = true,
        [EXPR_KIND_ERF]               = true,
        [EXPR_KIND_ERFC]              = true,
        [EXPR_KIND_LGAMMA]            = true,
        [EXPR_KIND_ERFINV]            = true,
        [EXPR_KIND_ERFCINV]           = true,
        [EXPR_KIND_GAMMA]             = true,
        [EXPR_KIND_DIGAMMA]           = true,
        [EXPR_KIND_TRIGAMMA]          = true,
        [EXPR_KIND_ZETA]              = true,
        [EXPR_KIND_GAMMAINV]          = true,
        [EXPR_KIND_LAMBERT_W0]        = true,
        [EXPR_KIND_LAMBERT_WM1]       = true,
        [EXPR_KIND_NORMAL_PDF]        = true,
        [EXPR_KIND_NORMAL_CDF]        = true,
        [EXPR_KIND_NORMAL_LOGPDF]     = true,
        [EXPR_KIND_E1]                = true,
        [EXPR_KIND_DILOG]             = true,
        [EXPR_KIND_POLYLOG1]          = true,
        [EXPR_KIND_VERSIN]            = true,
        [EXPR_KIND_VERCOS]            = true,
        [EXPR_KIND_COVERSIN]          = true,
        [EXPR_KIND_COVERCOS]          = true,
        [EXPR_KIND_HAVERSIN]          = true,
        [EXPR_KIND_HAVERCOS]          = true,
        [EXPR_KIND_HACOVERSIN]        = true,
        [EXPR_KIND_HACOVERCOS]        = true,
        [EXPR_KIND_ARCVERSIN]         = true,
        [EXPR_KIND_ARCVERCOS]         = true,
        [EXPR_KIND_ARCCOVERSIN]       = true,
        [EXPR_KIND_ARCCOVERCOS]       = true,
        [EXPR_KIND_ARCHAVERSIN]       = true,
        [EXPR_KIND_ARCHAVERCOS]       = true,
        [EXPR_KIND_ARCHACOVERSIN]     = true,
        [EXPR_KIND_ARCHACOVERCOS]     = true,
    };

    return kind >= 0 && kind < EXPR_KIND_COUNT && supported[kind];
}

static bool emit_TeX_cube_root_cartesian(const expr_t *f, sbuf_t *b)
{
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    bool has_imaginary = false;

    if (!f || !expr_is_op(f, &ops_cubrt) ||
        !expr_cartesian_parts_for_display(f->a, &real, &imaginary, &has_imaginary) || !has_imaginary) {
        expr_free(imaginary);
        expr_free(real);
        return false;
    }

    sbuf_puts(b, "\\begin{aligned}\n\\sqrt[3]{");
    emit_TeX_expr(f->a, b, PREC_LOWEST);
    sbuf_puts(b, "}&=p+q\\mkern-2mu i,\\\\\np&=\\left(");
    emit_TeX_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, "^{2}+");
    emit_TeX_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, "^{2}\\right)^{1/6}\\cos\\left(\\frac{1}{3}\\operatorname{atan2}(");
    emit_TeX_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ",");
    emit_TeX_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, ")\\right),\\\\\nq&=\\left(");
    emit_TeX_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, "^{2}+");
    emit_TeX_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, "^{2}\\right)^{1/6}\\sin\\left(\\frac{1}{3}\\operatorname{atan2}(");
    emit_TeX_expr(imaginary, b, PREC_LOWEST);
    sbuf_puts(b, ",");
    emit_TeX_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, ")\\right)\\end{aligned}");

    expr_free(imaginary);
    expr_free(real);
    return true;
}

static bool emit_TeX_analytic_unary_cartesian(const expr_t *f, sbuf_t *b)
{
    static const char *index_names[] = {"n", "m", "k", "l", "j"};
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    bool has_imaginary = false;
    const char *index = "j";
    const char *name;

    if (!f || !f->ops || f->ops->arity != EXPR_OP_UNARY ||
        !expr_kind_has_analytic_cartesian_series(f->ops->kind) ||
        !expr_cartesian_parts_for_display(f->a, &real, &imaginary, &has_imaginary) || !has_imaginary) {
        expr_free(imaginary);
        expr_free(real);
        return false;
    }
    for (size_t i = 0u; i < sizeof(index_names) / sizeof(index_names[0]); ++i) {
        if (!expr_stringout_tree_has_symbol_name(f, index_names[i])) {
            index = index_names[i];
            break;
        }
    }
    name = f->ops->TeX_name ? f->ops->TeX_name : "\\operatorname{f}";

    sbuf_puts(b, "\\begin{aligned}\n");
    sbuf_puts(b, name);
    sbuf_putc(b, '(');
    emit_TeX_expr(f->a, b, PREC_LOWEST);
    sbuf_puts(b, ")&=p+q\\mkern-2mu i,\\\\\np&=\\sum_{");
    sbuf_puts(b, index);
    sbuf_puts(b, "=0}^{\\infty}\\frac{(-1)^{");
    sbuf_puts(b, index);
    sbuf_puts(b, "}");
    emit_TeX_expr(imaginary, b, PREC_POW);
    sbuf_puts(b, "^{2");
    sbuf_puts(b, index);
    sbuf_puts(b, "}}{(2");
    sbuf_puts(b, index);
    sbuf_puts(b, ")!}");
    sbuf_puts(b, "\\left(");
    sbuf_puts(b, name);
    sbuf_puts(b, "\\right)^{(2");
    sbuf_puts(b, index);
    sbuf_puts(b, ")}(");
    emit_TeX_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, "),\\\\\nq&=\\sum_{");
    sbuf_puts(b, index);
    sbuf_puts(b, "=0}^{\\infty}\\frac{(-1)^{");
    sbuf_puts(b, index);
    sbuf_puts(b, "}");
    emit_TeX_expr(imaginary, b, PREC_POW);
    sbuf_puts(b, "^{2");
    sbuf_puts(b, index);
    sbuf_puts(b, "+1}}{(2");
    sbuf_puts(b, index);
    sbuf_puts(b, "+1)!}");
    sbuf_puts(b, "\\left(");
    sbuf_puts(b, name);
    sbuf_puts(b, "\\right)^{(2");
    sbuf_puts(b, index);
    sbuf_puts(b, "+1)}(");
    emit_TeX_expr(real, b, PREC_LOWEST);
    sbuf_puts(b, ")\\end{aligned}");

    expr_free(imaginary);
    expr_free(real);
    return true;
}

static void emit_expr_integral(const expr_t *f, sbuf_t *b, int parent_prec)
{
    int need = PREC_UNARY < parent_prec;
    const expr_t *lower = expr_integral_lower_bound_expr(f);
    const expr_t *upper = expr_integral_upper_bound_expr(f);
    const expr_t *display_integrand = f ? f->a : NULL;
    const expr_t *display_dummy = expr_integral_dummy_expr(f);
    bool group_upper = upper && expr_is_addsub(upper);
    bool group_lower = lower && expr_is_addsub(lower);
    bool group_integrand = expr_is_addsub(display_integrand);

    if (need)
        sbuf_putc(b, '(');
    sbuf_puts(b, "∫^");
    if (group_upper)
        sbuf_putc(b, '(');
    emit_expr(upper, b, PREC_LOWEST);
    if (group_upper)
        sbuf_putc(b, ')');
    if (lower) {
        sbuf_putc(b, '_');
        if (group_lower)
            sbuf_putc(b, '(');
        emit_expr(lower, b, PREC_LOWEST);
        if (group_lower)
            sbuf_putc(b, ')');
    }
    sbuf_putc(b, ' ');
    if (group_integrand)
        sbuf_putc(b, '(');
    emit_expr(display_integrand, b, PREC_LOWEST);
    if (group_integrand)
        sbuf_putc(b, ')');
    sbuf_puts(b, "·d");
    emit_expr(display_dummy, b, PREC_LOWEST);
    if (need)
        sbuf_putc(b, ')');
}

static void emit_formal_derivative_expr(const expr_t *f, sbuf_t *b)
{
    sbuf_putc(b, 'D');
    for (size_t i = 0u; i < f->formal_wrt_count; ++i) {
        const expr_t *wrt = f->formal_wrts[i];

        emit_name(b, (wrt && wrt->name) ? wrt->name : "x");
    }
    sbuf_putc(b, '(');
    emit_expr(f->a, b, PREC_LOWEST);
    sbuf_putc(b, ')');
}

static _Thread_local unsigned int expr_TeX_partial_derivative_depth;
static _Thread_local unsigned int expr_TeX_total_derivative_depth;

void expr_TeX_partial_derivatives_push(void)
{
    expr_TeX_partial_derivative_depth++;
}

void expr_TeX_partial_derivatives_pop(void)
{
    if (expr_TeX_partial_derivative_depth > 0u)
        expr_TeX_partial_derivative_depth--;
}

bool expr_TeX_partial_derivatives_enabled(void)
{
    return expr_TeX_partial_derivative_depth > 0u;
}

void expr_TeX_total_derivatives_push(void)
{
    expr_TeX_total_derivative_depth++;
}

void expr_TeX_total_derivatives_pop(void)
{
    if (expr_TeX_total_derivative_depth > 0u)
        expr_TeX_total_derivative_depth--;
}

bool expr_TeX_total_derivatives_enabled(void)
{
    return expr_TeX_total_derivative_depth > 0u;
}

static void emit_formal_partial_denominator(const expr_t *f, sbuf_t *b)
{
    size_t i = f->formal_wrt_count;
    bool first = true;

    while (i > 0u) {
        const expr_t *wrt = f->formal_wrts[i - 1u];
        size_t multiplicity = 1u;

        while (i > multiplicity && expr_struct_eq(f->formal_wrts[i - multiplicity - 1u], wrt)) {
            multiplicity++;
        }
        if (!first)
            sbuf_puts(b, "\\,");
        sbuf_puts(b, "\\partial ");
        emit_TeX_name(b, (wrt && wrt->name) ? wrt->name : "x");
        if (multiplicity > 1u) {
            char exponent[32];

            snprintf(exponent, sizeof(exponent), "^{%zu}", multiplicity);
            sbuf_puts(b, exponent);
        }
        first = false;
        i -= multiplicity;
    }
}

static void emit_formal_derivative_TeX(const expr_t *f, sbuf_t *b)
{
    if (expr_TeX_partial_derivatives_enabled()) {
        sbuf_puts(b, "\\frac{\\partial");
        if (f->formal_wrt_count > 1u) {
            char order[32];

            snprintf(order, sizeof(order), "^{%zu}", f->formal_wrt_count);
            sbuf_puts(b, order);
        }
        sbuf_putc(b, ' ');
        emit_TeX_expr(f->a, b, PREC_LOWEST);
        sbuf_puts(b, "}{");
        emit_formal_partial_denominator(f, b);
        sbuf_putc(b, '}');
        return;
    }
    if (expr_TeX_total_derivatives_enabled()) {
        const expr_t *wrt = f->formal_wrt_count > 0u ? f->formal_wrts[0] : NULL;

        sbuf_puts(b, "\\frac{d");
        if (f->formal_wrt_count > 1u) {
            char order[32];

            snprintf(order, sizeof(order), "^{%zu}", f->formal_wrt_count);
            sbuf_puts(b, order);
        }
        sbuf_putc(b, ' ');
        emit_TeX_expr(f->a, b, PREC_LOWEST);
        sbuf_puts(b, "}{d ");
        emit_TeX_name(b, (wrt && wrt->name) ? wrt->name : "x");
        if (f->formal_wrt_count > 1u) {
            char order[32];

            snprintf(order, sizeof(order), "^{%zu}", f->formal_wrt_count);
            sbuf_puts(b, order);
        }
        sbuf_putc(b, '}');
        return;
    }

    sbuf_puts(b, "\\operatorname{D}_{");
    for (size_t i = 0u; i < f->formal_wrt_count; ++i) {
        const expr_t *wrt = f->formal_wrts[i];

        if (i > 0u)
            sbuf_putc(b, ' ');
        emit_TeX_name(b, (wrt && wrt->name) ? wrt->name : "x");
    }
    sbuf_puts(b, "}\\left(");
    emit_TeX_expr(f->a, b, PREC_LOWEST);
    sbuf_puts(b, "\\right)");
}

static void emit_arbitrary_function_expr(const expr_t *f, sbuf_t *b)
{
    const char *name = f->name ? f->name : "F";
    size_t length = strlen(name);
    size_t prime_count = 0u;

    if (strcmp(name, "BesselJ") == 0 && f->a && f->a->ops == &ops_argument_list) {
        sbuf_puts(b, "BesselJ(");
        emit_expr(f->a, b, PREC_LOWEST);
        sbuf_putc(b, ')');
        return;
    }

    if (strcmp(name, "LommelS") == 0 && f->a && f->a->ops == &ops_argument_list) {
        sbuf_puts(b, "LommelS(");
        emit_expr(f->a, b, PREC_LOWEST);
        sbuf_putc(b, ')');
        return;
    }

    if (strcmp(name, "Si") == 0 || strcmp(name, "Ci") == 0) {
        sbuf_puts(b, name);
        sbuf_putc(b, '(');
        emit_expr(f->a, b, PREC_LOWEST);
        sbuf_putc(b, ')');
        return;
    }

    while (prime_count < length && name[length - prime_count - 1u] == '\'')
        ++prime_count;
    if (prime_count > 0u && prime_count < length) {
        char *base_name = strndup(name, length - prime_count);

        if (base_name) {
            emit_name(b, base_name);
            free(base_name);
            for (size_t i = 0u; i < prime_count; ++i)
                sbuf_putc(b, '\'');
        } else {
            emit_name(b, name);
        }
    } else {
        emit_name(b, name);
    }
    sbuf_putc(b, '(');
    emit_expr(f->a, b, PREC_LOWEST);
    sbuf_putc(b, ')');
}

static void emit_argument_list_expr(const expr_t *f, sbuf_t *b)
{
    emit_expr(f->a, b, PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_expr(f->b, b, PREC_LOWEST);
}

static void emit_formal_derivative_func(const expr_t *f, sbuf_t *b)
{
    sbuf_putc(b, 'D');
    for (size_t i = 0u; i < f->formal_wrt_count; ++i) {
        const expr_t *wrt = f->formal_wrts[i];

        emit_name_func(b, (wrt && wrt->name) ? wrt->name : "x");
    }
    sbuf_putc(b, '(');
    emit_func(f->a, b, PREC_LOWEST);
    sbuf_putc(b, ')');
}

static void emit_arbitrary_function_func(const expr_t *f, sbuf_t *b)
{
    const char *name = f->name ? f->name : "F";
    const char *function_name = name;
    size_t length;
    size_t prime_count = 0u;

    if (strcmp(name, "BesselJ") == 0)
        function_name = "besselj";
    else if (strcmp(name, "BesselY") == 0)
        function_name = "bessely";
    else if (strcmp(name, "LommelS") == 0)
        function_name = "lommels";

    length = strlen(function_name);
    while (prime_count < length && function_name[length - prime_count - 1u] == '\'')
        ++prime_count;
    if (prime_count > 0u && prime_count < length) {
        char *base_name = strndup(function_name, length - prime_count);

        if (base_name) {
            if (expr_tostring_is_safe_func_name(base_name))
                sbuf_puts(b, base_name);
            else
                emit_name_func(b, base_name);
            free(base_name);
            for (size_t i = 0u; i < prime_count; ++i)
                sbuf_putc(b, '\'');
        } else {
            emit_name_func(b, function_name);
        }
    } else if (expr_tostring_is_safe_func_name(function_name)) {
        sbuf_puts(b, function_name);
    } else {
        emit_name_func(b, function_name);
    }
    sbuf_putc(b, '(');
    emit_func(f->a, b, PREC_LOWEST);
    sbuf_putc(b, ')');
}

static void emit_argument_list_func(const expr_t *f, sbuf_t *b)
{
    emit_func(f->a, b, PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_func(f->b, b, PREC_LOWEST);
}

static void emit_arbitrary_function_TeX(const expr_t *f, sbuf_t *b)
{
    const char *name = f->name ? f->name : "F";
    size_t length = strlen(name);
    size_t prime_count = 0u;

    if (strcmp(name, "BesselJ") == 0 && f->a && f->a->ops == &ops_argument_list) {
        sbuf_puts(b, "J_{");
        emit_TeX_expr(f->a->a, b, PREC_LOWEST);
        sbuf_puts(b, "}\\left(");
        emit_TeX_expr(f->a->b, b, PREC_LOWEST);
        sbuf_puts(b, "\\right)");
        return;
    }

    if (strcmp(name, "LommelS") == 0 && f->a && f->a->ops == &ops_argument_list && f->a->a &&
        f->a->a->ops == &ops_argument_list) {
        sbuf_puts(b, "s_{");
        emit_TeX_expr(f->a->a->a, b, PREC_LOWEST);
        sbuf_puts(b, ",");
        emit_TeX_expr(f->a->a->b, b, PREC_LOWEST);
        sbuf_puts(b, "}\\left(");
        emit_TeX_expr(f->a->b, b, PREC_LOWEST);
        sbuf_puts(b, "\\right)");
        return;
    }

    if (strcmp(name, "Si") == 0 || strcmp(name, "Ci") == 0) {
        sbuf_puts(b, "\\operatorname{");
        sbuf_puts(b, name);
        sbuf_puts(b, "}\\left(");
        emit_TeX_expr(f->a, b, PREC_LOWEST);
        sbuf_puts(b, "\\right)");
        return;
    }

    while (prime_count < length && name[length - prime_count - 1u] == '\'')
        ++prime_count;
    if (prime_count > 0u && prime_count < length) {
        char *base_name = strndup(name, length - prime_count);

        if (base_name) {
            emit_TeX_name(b, base_name);
            free(base_name);
            for (size_t i = 0u; i < prime_count; ++i)
                sbuf_putc(b, '\'');
        } else {
            emit_TeX_name(b, name);
        }
    } else {
        emit_TeX_name(b, name);
    }
    sbuf_puts(b, "\\left(");
    emit_TeX_expr(f->a, b, PREC_LOWEST);
    sbuf_puts(b, "\\right)");
}

static void emit_argument_list_TeX(const expr_t *f, sbuf_t *b)
{
    emit_TeX_expr(f->a, b, PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_TeX_expr(f->b, b, PREC_LOWEST);
}

static void emit_TeX_integral(const expr_t *f, sbuf_t *b, int parent_prec)
{
    int need = PREC_UNARY < parent_prec;
    const expr_t *lower = expr_integral_lower_bound_expr(f);
    const expr_t *upper = expr_integral_upper_bound_expr(f);
    const expr_t *display_integrand = f ? f->a : NULL;
    const expr_t *display_dummy = expr_integral_dummy_expr(f);

    if (need)
        sbuf_puts(b, "\\left(");
    sbuf_puts(b, "\\int");
    if (lower) {
        sbuf_puts(b, "_{");
        emit_TeX_expr(lower, b, PREC_LOWEST);
        sbuf_putc(b, '}');
    }
    sbuf_puts(b, "^{");
    emit_TeX_expr(upper, b, PREC_LOWEST);
    sbuf_puts(b, "} ");
    emit_TeX_expr(display_integrand, b, PREC_LOWEST);
    sbuf_puts(b, "\\, d");
    emit_TeX_expr(display_dummy, b, PREC_LOWEST);
    if (need)
        sbuf_puts(b, "\\right)");
}

static void emit_func_integral(const expr_t *f, sbuf_t *b)
{
    const expr_t *lower = expr_integral_lower_bound_expr(f);
    const expr_t *upper = expr_integral_upper_bound_expr(f);
    const expr_t *display_integrand = f ? f->a : NULL;
    const expr_t *display_dummy = expr_integral_dummy_expr(f);
    sbuf_puts(b, "integral(");
    if (lower) {
        emit_func(lower, b, PREC_LOWEST);
        sbuf_puts(b, ", ");
    }
    emit_func(upper, b, PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_func(display_integrand, b, PREC_LOWEST);
    sbuf_puts(b, ", ");
    emit_func(display_dummy, b, PREC_LOWEST);
    sbuf_putc(b, ')');
}

static bool emit_TeX_unit_fraction_power(const expr_t *base, number_t exponent, sbuf_t *b, int parent_prec)
{
    long numerator;
    long denominator;
    bool reciprocal;
    bool group_power_base;
    int need;
    unsigned long magnitude;

    if (!num_get_small_rational(exponent, &numerator, &denominator) || numerator == 0L || denominator <= 1L)
        return false;

    reciprocal = numerator < 0L;
    magnitude = reciprocal ? (unsigned long)(-(numerator + 1L)) + 1u : (unsigned long)numerator;
    group_power_base = magnitude > 1u && pow_base_needs_visible_parens(base);
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
    if (group_power_base)
        sbuf_puts(b, "\\left(");
    emit_TeX_expr(base, b, magnitude > 1u ? PREC_POW : PREC_LOWEST);
    if (magnitude > 1u) {
        char exponent_text[32];

        snprintf(exponent_text, sizeof(exponent_text), "%lu", magnitude);
        if (group_power_base)
            sbuf_puts(b, "\\right)");
        sbuf_puts(b, "^{");
        sbuf_puts(b, exponent_text);
        sbuf_putc(b, '}');
    }
    sbuf_putc(b, '}');
    if (reciprocal)
        sbuf_putc(b, '}');
    if (need)
        sbuf_puts(b, "\\right)");
    return true;
}

static void emit_expr_sqrt_power(const expr_t *base, sbuf_t *b, int parent_prec, bool reciprocal)
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

static bool match_atan_over_argument_denominator(const expr_t *expr, const expr_t **atan_expr_out,
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

static bool match_sum_quotient(const expr_t *expr, const expr_t **factor_out, const expr_t **sum_out)
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

static int expr_is_negative(const expr_t *f)
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

static void emit_factor_abs(const expr_t *f, sbuf_t *b)
{
    if (expr_is_negative(f))
        emit_expr_abs(f, b, PREC_MUL);
    else
        emit_expr(f, b, PREC_MUL);
}

static int expr_renders_negative(const expr_t *f)
{
    sbuf_t b;
    int neg;

    sbuf_init(&b);
    emit_expr(f, &b, 0);
    neg = (sbuf_len(&b) > 0u && rune_is_equal(string_at(b.text, 0u), '-'));
    sbuf_free(&b);
    return neg;
}

#define DISPLAY_POLY_MAX_VARS 16u

typedef struct {
    const expr_t *expr;
    bool subtract;
    long degree[DISPLAY_POLY_MAX_VARS];
} display_poly_term_t;

typedef struct {
    const char *name[DISPLAY_POLY_MAX_VARS];
    size_t count;
} display_poly_var_list_t;

static bool display_poly_vars_add(display_poly_var_list_t *vars, const char *name)
{
    size_t pos;

    if (!vars || !name || !*name || !expr_tostring_is_primary_variable_name(name))
        return true;

    for (size_t i = 0u; i < vars->count; ++i) {
        int cmp = strcmp(vars->name[i], name);

        if (cmp == 0)
            return true;
        if (cmp > 0)
            break;
    }

    if (vars->count >= DISPLAY_POLY_MAX_VARS)
        return false;

    pos = vars->count;
    while (pos > 0u && strcmp(vars->name[pos - 1u], name) > 0) {
        vars->name[pos] = vars->name[pos - 1u];
        --pos;
    }
    vars->name[pos] = name;
    ++vars->count;
    return true;
}

static bool display_poly_collect_vars(const expr_t *expr, display_poly_var_list_t *vars)
{
    if (!expr || !vars)
        return true;

    if (expr_is_var(expr) && expr->name)
        return display_poly_vars_add(vars, expr->name);

    return display_poly_collect_vars(expr->a, vars) && display_poly_collect_vars(expr->b, vars);
}

static int display_poly_var_index(const display_poly_var_list_t *vars, const char *name)
{
    if (!vars || !name)
        return -1;
    for (size_t i = 0u; i < vars->count; ++i) {
        if (strcmp(vars->name[i], name) == 0)
            return (int)i;
    }
    return -1;
}

static bool display_poly_expr_contains_any_var(const expr_t *expr, const display_poly_var_list_t *vars)
{
    if (!expr || !vars)
        return false;
    if (expr_is_var(expr) && expr->name && display_poly_var_index(vars, expr->name) >= 0)
        return true;
    return display_poly_expr_contains_any_var(expr->a, vars) || display_poly_expr_contains_any_var(expr->b, vars);
}

static bool display_poly_expr_contains_imaginary_unit(const expr_t *expr)
{
    if (!expr)
        return false;
    if (expr_is_const(expr) && expr->name && strcmp(expr->name, "i") == 0)
        return true;
    return display_poly_expr_contains_imaginary_unit(expr->a) || display_poly_expr_contains_imaginary_unit(expr->b);
}

static bool display_poly_is_pure_imag_const(const expr_t *expr)
{
    number_t real;
    bool pure_imag;

    if (!expr || !expr_is_const(expr) || num_is_real(expr->c))
        return false;

    real = num_real_part(expr->c);
    pure_imag = num_eq(real, NUM_ZERO);
    num_destroy(&real);
    return pure_imag;
}

static bool display_poly_is_real_const(const expr_t *expr)
{
    return expr && expr_is_const(expr) && num_is_real(expr->c);
}

static bool display_poly_is_i_const(const expr_t *expr)
{
    return expr && expr_is_const(expr) && (num_eq(expr->c, NUM_I) || num_eq(expr->c, NUM_NEG_I));
}

static bool display_poly_is_imaginary_term(const expr_t *expr)
{
    if (!expr)
        return false;
    if (display_poly_is_i_const(expr) || display_poly_is_pure_imag_const(expr))
        return true;
    return expr_is_mul(expr) && ((display_poly_is_real_const(expr->a) && display_poly_is_i_const(expr->b)) ||
                                 (display_poly_is_i_const(expr->a) && display_poly_is_real_const(expr->b)));
}

static bool display_poly_is_negative_real_complex_const(const expr_t *expr)
{
    number_t real;
    bool negative_real;

    if (!expr || !expr_is_const(expr) || num_is_real(expr->c))
        return false;

    real = num_real_part(expr->c);
    negative_real = num_lt(real, NUM_ZERO);
    num_destroy(&real);
    return negative_real;
}

static bool match_add_negative_complex_rhs(const expr_t *expr, const expr_t **base_out, const expr_t **complex_out)
{
    if (!expr || !expr_is_op(expr, &ops_add) || !expr->a || !display_poly_is_negative_real_complex_const(expr->b))
        return false;

    if (base_out)
        *base_out = expr->a;
    if (complex_out)
        *complex_out = expr->b;
    return true;
}

static bool match_additive_complex_shift(const expr_t *expr, const expr_t **base_out, const expr_t **real_out,
                                         const expr_t **imag_out)
{
    if (!expr || !expr_is_op(expr, &ops_add) || !expr->a || !expr->b || !expr_is_op(expr->a, &ops_sub) || !expr->a->a ||
        !expr->a->b || !display_poly_is_real_const(expr->a->b) || !display_poly_is_imaginary_term(expr->b))
        return false;

    if (base_out)
        *base_out = expr->a->a;
    if (real_out)
        *real_out = expr->a->b;
    if (imag_out)
        *imag_out = expr->b;
    return true;
}

static bool display_poly_add_degree(long *degree, long add)
{
    if (!degree || add < 0 || *degree > LONG_MAX - add)
        return false;
    *degree += add;
    return true;
}

static bool display_poly_term_degrees(const expr_t *expr, const display_poly_var_list_t *vars, long *degree)
{
    number_t exponent = num_new();
    long power = 0;
    int index;
    bool ok = false;

    if (!expr || !vars || !degree)
        goto cleanup;

    if (expr_is_const(expr)) {
        ok = true;
        goto cleanup;
    }

    if (expr_is_var(expr)) {
        index = expr->name ? display_poly_var_index(vars, expr->name) : -1;
        ok = index < 0 || display_poly_add_degree(&degree[index], 1);
        goto cleanup;
    }

    if (expr_is_neg(expr)) {
        ok = display_poly_term_degrees(expr->a, vars, degree);
        goto cleanup;
    }

    if (expr_is_mul(expr)) {
        ok = display_poly_term_degrees(expr->a, vars, degree) && display_poly_term_degrees(expr->b, vars, degree);
        goto cleanup;
    }

    if (expr_is_op(expr, &ops_div)) {
        ok = !display_poly_expr_contains_any_var(expr->b, vars) && display_poly_term_degrees(expr->a, vars, degree);
        goto cleanup;
    }

    if (expr_is_pow_d_expr(expr)) {
        index = (expr->a && expr_is_var(expr->a) && expr->a->name) ? display_poly_var_index(vars, expr->a->name) : -1;
        if (index >= 0) {
            ok = expr_try_get_small_integer_exponent(expr->c, &power) && display_poly_add_degree(&degree[index], power);
        } else {
            ok = !display_poly_expr_contains_any_var(expr->a, vars);
        }
        goto cleanup;
    }

    if (expr_is_op(expr, &ops_pow)) {
        index = (expr->a && expr_is_var(expr->a) && expr->a->name) ? display_poly_var_index(vars, expr->a->name) : -1;
        if (index >= 0) {
            ok = expr_match_const_value(expr->b, &exponent) && expr_try_get_small_integer_exponent(exponent, &power) &&
                 display_poly_add_degree(&degree[index], power);
        } else {
            ok = !display_poly_expr_contains_any_var(expr, vars);
        }
        goto cleanup;
    }

    ok = !display_poly_expr_contains_any_var(expr, vars);

cleanup:
    num_destroy(&exponent);
    return ok;
}

static bool display_poly_collect_add_terms(const expr_t *expr, bool subtract, const display_poly_var_list_t *vars,
                                           display_poly_term_t *terms, size_t *count, size_t max_terms)
{
    if (!expr || !vars || !terms || !count)
        return false;

    if (display_poly_expr_contains_any_var(expr, vars) && expr_is_op(expr, &ops_add))
        return display_poly_collect_add_terms(expr->a, subtract, vars, terms, count, max_terms) &&
               display_poly_collect_add_terms(expr->b, subtract, vars, terms, count, max_terms);

    if (display_poly_expr_contains_any_var(expr, vars) && expr_is_op(expr, &ops_sub))
        return display_poly_collect_add_terms(expr->a, subtract, vars, terms, count, max_terms) &&
               display_poly_collect_add_terms(expr->b, !subtract, vars, terms, count, max_terms);

    if (*count >= max_terms)
        return false;

    terms[*count].expr = expr;
    terms[*count].subtract = subtract;
    memset(terms[*count].degree, 0, sizeof(terms[*count].degree));
    ++*count;
    return true;
}

static int display_poly_compare_terms(const display_poly_term_t *left, const display_poly_term_t *right,
                                      size_t var_count, bool ascending)
{
    for (size_t i = 0u; i < var_count; ++i) {
        if (left->degree[i] > right->degree[i])
            return ascending ? 1 : -1;
        if (left->degree[i] < right->degree[i])
            return ascending ? -1 : 1;
    }
    return 0;
}

static void display_poly_sort_terms(display_poly_term_t *terms, size_t count, size_t var_count, bool ascending)
{
    for (size_t i = 1u; i < count; ++i) {
        display_poly_term_t key = terms[i];
        size_t j = i;

        while (j > 0u && display_poly_compare_terms(&key, &terms[j - 1u], var_count, ascending) < 0) {
            terms[j] = terms[j - 1u];
            --j;
        }
        terms[j] = key;
    }
}

static bool display_poly_is_indexed_arbitrary_constant(const expr_t *expr)
{
    const unsigned char *name;

    if (!expr || !expr_is_const(expr) || !expr->name || expr->name[0] != 'C')
        return false;

    name = (const unsigned char *)expr->name + 1u;
    if (!isdigit(*name) && !(strlen((const char *)name) >= 3u && name[0] == 0xe2u && name[1] == 0x82u &&
                             name[2] >= 0x80u && name[2] <= 0x89u))
        return false;
    while (*name) {
        if (isdigit(*name)) {
            ++name;
            continue;
        }
        if (strlen((const char *)name) >= 3u && name[0] == 0xe2u && name[1] == 0x82u && name[2] >= 0x80u &&
            name[2] <= 0x89u) {
            name += 3u;
            continue;
        }
        return false;
    }
    return *name == '\0';
}

static bool display_poly_contains_indexed_arbitrary_constant(const expr_t *expr)
{
    if (!expr)
        return false;
    if (display_poly_is_indexed_arbitrary_constant(expr))
        return true;
    return display_poly_contains_indexed_arbitrary_constant(expr->a) ||
           display_poly_contains_indexed_arbitrary_constant(expr->b);
}

static bool display_poly_is_explicit_arbitrary_amplitude(const display_poly_term_t *terms, size_t count,
                                                         size_t var_count)
{
    bool degrees[96] = {false};

    if (count < 2u || count > sizeof(degrees) / sizeof(degrees[0]))
        return false;

    for (size_t i = 0u; i < count; ++i) {
        long total_degree = 0;

        for (size_t j = 0u; j < var_count; ++j)
            total_degree += terms[i].degree[j];

        if (total_degree < 0 || (size_t)total_degree != i || (size_t)total_degree >= count || degrees[total_degree] ||
            !display_poly_contains_indexed_arbitrary_constant(terms[i].expr))
            return false;
        degrees[total_degree] = true;
    }
    for (size_t i = 0u; i < count; ++i) {
        if (!degrees[i])
            return false;
    }
    return true;
}

static bool display_poly_prepare_terms(const expr_t *expr, display_poly_term_t *terms, size_t *count, size_t max_terms)
{
    display_poly_var_list_t vars = {0};
    bool has_variable_term = false;

    if (!expr || !terms || !count || !expr_is_addsub(expr))
        return false;

    if (display_poly_expr_contains_imaginary_unit(expr) || !display_poly_collect_vars(expr, &vars) || vars.count == 0u)
        return false;

    *count = 0u;
    if (!display_poly_collect_add_terms(expr, false, &vars, terms, count, max_terms) || *count < 2u)
        return false;

    for (size_t i = 0u; i < *count; ++i) {
        if (!display_poly_term_degrees(terms[i].expr, &vars, terms[i].degree))
            return false;
        for (size_t j = 0u; j < vars.count; ++j) {
            if (terms[i].degree[j] > 0) {
                has_variable_term = true;
                break;
            }
        }
    }

    if (!has_variable_term)
        return false;

    display_poly_sort_terms(terms, *count, vars.count,
                            display_poly_is_explicit_arbitrary_amplitude(terms, *count, vars.count));
    if (terms[0].subtract != (expr_renders_negative(terms[0].expr) ? true : false))
        return false;
    return true;
}

static bool emit_expr_display_polynomial_sum(const expr_t *expr, sbuf_t *b, int parent_prec)
{
    display_poly_term_t terms[96];
    size_t count = 0u;
    int need = PREC_ADD < parent_prec;

    if (!display_poly_prepare_terms(expr, terms, &count, sizeof(terms) / sizeof(terms[0])))
        return false;

    if (need)
        sbuf_putc(b, '(');

    for (size_t i = 0u; i < count; ++i) {
        bool term_negative = expr_renders_negative(terms[i].expr);
        bool effective_negative = terms[i].subtract != term_negative;
        bool term_needs_parens = expr_is_addsub(terms[i].expr);

        if (i == 0u) {
            if (effective_negative)
                sbuf_putc(b, '-');
        } else {
            sbuf_puts(b, effective_negative ? " - " : " + ");
        }

        if (term_needs_parens)
            sbuf_putc(b, '(');
        if (term_negative)
            emit_expr_abs(terms[i].expr, b, PREC_ADD);
        else
            emit_expr(terms[i].expr, b, PREC_ADD);
        if (term_needs_parens)
            sbuf_putc(b, ')');
    }

    if (need)
        sbuf_putc(b, ')');
    return true;
}

static bool emit_TeX_display_polynomial_sum(const expr_t *expr, sbuf_t *b, int parent_prec)
{
    display_poly_term_t terms[96];
    size_t count = 0u;
    int need = PREC_ADD < parent_prec;

    if (!display_poly_prepare_terms(expr, terms, &count, sizeof(terms) / sizeof(terms[0])))
        return false;

    if (need)
        sbuf_puts(b, "\\left(");

    for (size_t i = 0u; i < count; ++i) {
        bool term_negative = expr_renders_negative(terms[i].expr);
        bool effective_negative = terms[i].subtract != term_negative;
        bool term_needs_parens = expr_is_addsub(terms[i].expr);

        if (i == 0u) {
            if (effective_negative)
                sbuf_putc(b, '-');
        } else {
            sbuf_puts(b, effective_negative ? " - " : " + ");
        }

        if (term_needs_parens)
            sbuf_puts(b, "\\left(");
        if (term_negative)
            emit_TeX_expr_abs(terms[i].expr, b, PREC_ADD);
        else
            emit_TeX_expr(terms[i].expr, b, PREC_ADD);
        if (term_needs_parens)
            sbuf_puts(b, "\\right)");
    }

    if (need)
        sbuf_puts(b, "\\right)");
    return true;
}

static bool emit_func_display_polynomial_sum(const expr_t *expr, sbuf_t *b, int parent_prec)
{
    display_poly_term_t terms[96];
    size_t count = 0u;
    int need = PREC_ADD < parent_prec;

    if (!display_poly_prepare_terms(expr, terms, &count, sizeof(terms) / sizeof(terms[0])))
        return false;

    if (need)
        sbuf_putc(b, '(');

    for (size_t i = 0u; i < count; ++i) {
        bool term_negative = expr_renders_negative(terms[i].expr);
        bool effective_negative = terms[i].subtract != term_negative;
        bool term_needs_parens = expr_is_addsub(terms[i].expr);

        if (i == 0u) {
            if (effective_negative)
                sbuf_putc(b, '-');
        } else {
            sbuf_puts(b, effective_negative ? " - " : " + ");
        }

        if (term_needs_parens)
            sbuf_putc(b, '(');
        if (term_negative)
            emit_func_abs(terms[i].expr, b, PREC_ADD);
        else
            emit_func(terms[i].expr, b, PREC_ADD);
        if (term_needs_parens)
            sbuf_putc(b, ')');
    }

    if (need)
        sbuf_putc(b, ')');
    return true;
}

void emit_func_display(const expr_t *f, sbuf_t *b, int parent_prec)
{
    if (f && expr_is_addsub(f) && emit_func_display_polynomial_sum(f, b, parent_prec))
        return;
    emit_func(f, b, parent_prec);
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

static void emit_expr_abs(const expr_t *f, sbuf_t *b, int parent_prec)
{
    if (!f) {
        sbuf_puts(b, "0");
        return;
    }

    if (expr_tostring_is_negative_const(f)) {
        char *text;
        number_t pos_value = num_neg(f->c);

        if (emit_negative_const_binding_expr_abs(f, b, false)) {
            num_destroy(&pos_value);
            return;
        }

        text = expr_number_to_string_local(pos_value);
        if (text) {
            sbuf_puts(b, text);
            free(text);
        }
        return;
    }

    if (expr_is_const(f) && expr_renders_negative(f)) {
        char *text;
        number_t pos_value = num_neg(f->c);

        if (emit_negative_const_binding_expr_abs(f, b, false)) {
            num_destroy(&pos_value);
            return;
        }

        text = expr_number_to_string_local(pos_value);
        if (text) {
            sbuf_puts(b, text);
            free(text);
        }
        return;
    }

    if (expr_is_neg(f)) {
        emit_expr(f->a, b, parent_prec);
        return;
    }

    if (expr_is_mul(f)) {
        expr_t *fac[64];
        int n = 0;

        flatten_mul((expr_t *)f, fac, &n, 64);
        sort_factors(fac, n);

        for (int i = 0; i < n; ++i) {
            if (expr_tostring_is_negative_const(fac[i])) {
                if (num_eq(fac[i]->c, NUM_NEG_ONE)) {
                    for (int j = i; j < n - 1; ++j)
                        fac[j] = fac[j + 1];
                    --n;
                }
            }
        }

        for (int i = 0; i < n; ++i) {
            if (i > 0)
                emit_expr_mul_separator_local(fac[i - 1], fac[i], b);
            if (n > 1 && mul_factor_needs_visible_parens(fac[i]))
                sbuf_putc(b, '(');
            emit_factor_abs(fac[i], b);
            if (n > 1 && mul_factor_needs_visible_parens(fac[i]))
                sbuf_putc(b, ')');
        }
        return;
    }

    if (expr_is_op(f, &ops_div) && expr_is_negative(f->a)) {
        int need = PREC_MUL < parent_prec;
        if (need)
            sbuf_putc(b, '(');
        emit_expr_abs(f->a, b, PREC_MUL);
        sbuf_putc(b, '/');
        emit_expr(f->b, b, PREC_POW);
        if (need)
            sbuf_putc(b, ')');
        return;
    }

    emit_expr(f, b, parent_prec);
}

static void emit_expr_abs_bars(const expr_t *f, sbuf_t *b)
{
    sbuf_putc(b, '|');
    emit_expr_abs(f, b, 0);
    sbuf_putc(b, '|');
}

void emit_TeX_name(sbuf_t *b, const char *name)
{
    char *tex;

    if (!name || !*name) {
        sbuf_puts(b, "x");
        return;
    }
    if (strcmp(name, "±") == 0) {
        sbuf_puts(b, "\\pm");
        return;
    }

    if (expr_tostring_is_simple_name(name)) {
        tex = expr_tostring_texify(name);
        if (tex) {
            sbuf_puts(b, tex);
            free(tex);
        } else {
            sbuf_puts(b, name);
        }
        return;
    }

    sbuf_putc(b, '[');
    tex = expr_tostring_texify(name);
    if (tex) {
        sbuf_puts(b, tex);
        free(tex);
    } else {
        sbuf_puts(b, name);
    }
    sbuf_putc(b, ']');
}

static const char *expr_known_constant_TeX_local(number_t value)
{
    if (num_eq(value, NUM_SQRT1ONPI))
        return "\\frac{1}{\\sqrt{\\pi}}";
    if (num_eq(value, NUM_2_SQRTPI))
        return "\\frac{2}{\\sqrt{\\pi}}";
    if (num_eq(value, NUM_NEG_TWO_OVER_SQRT_PI))
        return "-\\frac{2}{\\sqrt{\\pi}}";
    if (num_eq(value, NUM_SQRT_PI))
        return "\\sqrt{\\pi}";
    if (num_eq(value, NUM_SQRT_2PI))
        return "\\sqrt{2\\pi}";
    if (num_eq(value, NUM_INV_SQRT_2PI))
        return "\\frac{1}{\\sqrt{2\\pi}}";
    if (num_eq(value, NUM_SQRT_PI_OVER_TWO))
        return "\\sqrt{\\pi/2}";
    if (num_eq(value, NUM_SQRT2))
        return "\\sqrt{2}";
    if (num_eq(value, NUM_SQRT3))
        return "\\sqrt{3}";
    if (num_eq(value, NUM_SQRT_HALF))
        return "\\sqrt{1/2}";
    if (num_eq(value, NUM_SQRT2_OVER_TWO))
        return "\\frac{\\sqrt{2}}{2}";
    if (num_eq(value, NUM_SQRT3_OVER_TWO))
        return "\\frac{\\sqrt{3}}{2}";
    return NULL;
}

static void emit_TeX_number_value(sbuf_t *b, number_t value)
{
    const char *constant_TeX = NULL;
    char *text = expr_number_to_string_local(num_clone(value));
    char *tex;

    if (!text)
        return;

    if (!num_is_exact(value))
        constant_TeX = expr_known_constant_TeX_local(value);
    if (constant_TeX) {
        sbuf_puts(b, constant_TeX);
        free(text);
        return;
    }

    tex = expr_text_to_TeX_local(text);
    if (tex) {
        sbuf_puts(b, tex);
        free(tex);
    } else {
        sbuf_puts(b, text);
    }
    free(text);
}

static void emit_TeX_const_value(sbuf_t *b, const expr_t *dv)
{
    const char *constant_TeX = NULL;
    char *text = expr_const_to_string_local(dv);
    char *tex;

    if (!text)
        return;

    if (dv && !num_is_exact(dv->c))
        constant_TeX = expr_known_constant_TeX_local(dv->c);
    if (constant_TeX) {
        sbuf_puts(b, constant_TeX);
        free(text);
        return;
    }

    tex = expr_text_to_TeX_local(text);
    if (tex) {
        sbuf_puts(b, tex);
        free(tex);
    } else {
        sbuf_puts(b, text);
    }
    free(text);
}

static bool emit_TeX_exp_unit_fraction_root(const expr_t *arg, sbuf_t *b)
{
    long numerator;
    long denominator;

    if (!expr_is_const(arg) || !num_get_small_rational(arg->c, &numerator, &denominator) || numerator != 1L ||
        denominator <= 1L)
        return false;

    if (denominator == 2L) {
        sbuf_puts(b, "\\sqrt{e}");
    } else {
        char index_text[32];

        snprintf(index_text, sizeof(index_text), "%ld", denominator);
        sbuf_puts(b, "\\sqrt[");
        sbuf_puts(b, index_text);
        sbuf_puts(b, "]{e}");
    }
    return true;
}

static void emit_TeX_atom(const expr_t *f, sbuf_t *b)
{
    if (expr_is_const(f)) {
        if (expr_tostring_should_emit_binding_expr(f)) {
            char *text = expr_binding_expr_to_TeX(f->binding_expr);

            if (text) {
                sbuf_puts(b, text);
                free(text);
            }
        } else if (f->name && *f->name)
            emit_TeX_name(b, f->name);
        else
            emit_TeX_const_value(b, f);
        return;
    }

    if (expr_is_var(f)) {
        emit_TeX_name(b, f->name ? f->name : "x");
        return;
    }

    emit_TeX_number_value(b, expr_eval(f));
}

static const char *TeX_unary_name(const expr_t *f)
{
    if (!f || !f->ops)
        return NULL;

    if (expr_is_op(f, &ops_abs))
        return NULL;
    if (expr_is_sqrt_expr(f))
        return "\\sqrt";
    return f->ops->TeX_name;
}

static bool TeX_unary_arg_is_greek_symbol(const expr_t *arg)
{
    const char *name;
    string_t *name_text;
    string_t *normalized;
    string_cursor_t *cursor;
    uint32_t codepoint = 0u;
    bool greek = false;

    if (!arg || (!expr_is_var(arg) && !expr_is_const(arg)) || !arg->name || !*arg->name)
        return false;

    name = arg->name[0] == '@' ? arg->name + 1 : arg->name;
    name_text = string_new_with(name);
    if (!name_text)
        return false;
    normalized = expr_normalise_greek_alias_text(name_text);
    if (normalized) {
        greek = true;
        goto cleanup;
    }

    cursor = string_cursor_new(name_text);
    if (cursor) {
        codepoint = rune_value(string_cursor_peek(cursor));
        string_cursor_next(cursor);
        greek = string_cursor_done(cursor) &&
                ((codepoint >= 0x0370u && codepoint <= 0x03ffu) || (codepoint >= 0x1f00u && codepoint <= 0x1fffu));
        string_cursor_free(cursor);
    }

cleanup:
    string_free(normalized);
    string_free(name_text);
    return greek;
}

static const char *expr_unary_name(const expr_t *f)
{
    if (!f || !f->ops)
        return "?";
    return expr_ops_expression_name(f->ops);
}

static int expr_polygamma_order(const expr_t *f, long *order)
{
    return f && expr_is_op(f, &ops_polygamma) && f->a && expr_is_const(f->a) &&
           expr_try_get_small_integer_exponent(f->a->c, order) && *order >= 0;
}

static int expr_has_polygamma_order(const expr_t *f)
{
    long order;

    return expr_polygamma_order(f, &order);
}

static int expr_polylog_order(const expr_t *f, long *order)
{
    return f && expr_is_op(f, &ops_polylog) && f->a && expr_is_const(f->a) &&
           expr_try_get_small_integer_exponent(f->a->c, order) && *order >= 0;
}

static int expr_has_polylog_order(const expr_t *f)
{
    long order;

    return expr_polylog_order(f, &order);
}

static int expr_legendre_chi_order(const expr_t *f, long *order)
{
    return f && expr_is_op(f, &ops_legendre_chi) && f->a && expr_is_const(f->a) &&
           expr_try_get_small_integer_exponent(f->a->c, order) && *order >= 0;
}

static int expr_has_legendre_chi_order(const expr_t *f)
{
    long order;

    return expr_legendre_chi_order(f, &order);
}

static void emit_expr_lambert_wn(const expr_t *f, sbuf_t *b)
{
    sbuf_puts(b, expr_ops_expression_name(f->ops));
    sbuf_putc(b, '(');
    emit_expr(f->a, b, 0);
    sbuf_puts(b, ", ");
    emit_expr(f->b, b, 0);
    sbuf_putc(b, ')');
}

static void emit_expr_polygamma(const expr_t *f, sbuf_t *b)
{
    long order;

    if (!expr_polygamma_order(f, &order))
        return;
    sbuf_puts(b, expr_ops_expression_name(f->ops));
    sbuf_puts(b, "⁽");
    emit_superscript_int(b, order);
    sbuf_puts(b, "⁾(");
    emit_expr(f->b, b, 0);
    sbuf_putc(b, ')');
}

static void emit_expr_polylog(const expr_t *f, sbuf_t *b)
{
    long order;

    if (!expr_polylog_order(f, &order))
        return;
    if (order == 1L) {
        sbuf_puts(b, "polylog(1, ");
        emit_expr(f->b, b, 0);
        sbuf_putc(b, ')');
        return;
    }
    sbuf_puts(b, "Li");
    emit_subscript_int(b, order);
    sbuf_putc(b, '(');
    emit_expr(f->b, b, 0);
    sbuf_putc(b, ')');
}

static void emit_expr_legendre_chi(const expr_t *f, sbuf_t *b)
{
    long order;

    if (!expr_legendre_chi_order(f, &order))
        return;
    sbuf_puts(b, "χ");
    emit_subscript_int(b, order);
    sbuf_putc(b, '(');
    emit_expr(f->b, b, 0);
    sbuf_putc(b, ')');
}

static void emit_expr_appell_f1(const expr_t *f, sbuf_t *b)
{
    const expr_t *a = NULL;
    const expr_t *b1 = NULL;
    const expr_t *b2 = NULL;
    const expr_t *c = NULL;
    const expr_t *x = NULL;
    const expr_t *y = NULL;

    if (!expr_appell_f1_unpack(f, &a, &b1, &b2, &c, &x, &y))
        return;
    sbuf_puts(b, expr_ops_expression_name(f->ops));
    sbuf_putc(b, '(');
    emit_expr(a, b, 0);
    sbuf_puts(b, "; ");
    emit_expr(b1, b, 0);
    sbuf_puts(b, ", ");
    emit_expr(b2, b, 0);
    sbuf_puts(b, "; ");
    emit_expr(c, b, 0);
    sbuf_puts(b, "; ");
    emit_expr(x, b, 0);
    sbuf_puts(b, ", ");
    emit_expr(y, b, 0);
    sbuf_putc(b, ')');
}

static void emit_expr_lauricella_f(const expr_t *f, sbuf_t *out)
{
    const expr_t *a = NULL;
    const expr_t **parameters = NULL;
    const expr_t *c = NULL;
    const expr_t **variables = NULL;
    size_t count = 0u;
    char prefix[64];

    if (!expr_lauricella_f_unpack(f, &a, &parameters, &c, &variables, &count))
        return;
    snprintf(prefix, sizeof(prefix), "(%zu, ", count);
    sbuf_puts(out, expr_ops_expression_name(f->ops));
    sbuf_puts(out, prefix);
    emit_expr(a, out, 0);
    for (size_t i = 0u; i < count; ++i) {
        sbuf_puts(out, ", ");
        emit_expr(parameters[i], out, 0);
    }
    sbuf_puts(out, ", ");
    emit_expr(c, out, 0);
    for (size_t i = 0u; i < count; ++i) {
        sbuf_puts(out, ", ");
        emit_expr(variables[i], out, 0);
    }
    sbuf_putc(out, ')');
    free(variables);
    free(parameters);
}

static void emit_expr_hypergeometric_pFq(const expr_t *f, sbuf_t *b)
{
    const expr_t **upper = NULL;
    const expr_t **lower = NULL;
    const expr_t *argument = NULL;
    size_t p = 0u;
    size_t q = 0u;
    char counts[64];

    if (!expr_hypergeometric_pFq_unpack(f, &upper, &p, &lower, &q, &argument))
        return;
    snprintf(counts, sizeof(counts), "(%zu, %zu", p, q);
    sbuf_puts(b, expr_ops_expression_name(f->ops));
    sbuf_puts(b, counts);
    for (size_t i = 0u; i < p; ++i) {
        sbuf_puts(b, ", ");
        emit_expr(upper[i], b, 0);
    }
    for (size_t i = 0u; i < q; ++i) {
        sbuf_puts(b, ", ");
        emit_expr(lower[i], b, 0);
    }
    sbuf_puts(b, ", ");
    emit_expr(argument, b, 0);
    sbuf_putc(b, ')');
    free(lower);
    free(upper);
}

static void emit_expr_lommel_s(const expr_t *f, sbuf_t *b)
{
    const expr_t *mu = NULL;
    const expr_t *nu = NULL;
    const expr_t *argument = NULL;

    if (!expr_lommel_s_unpack(f, &mu, &nu, &argument))
        return;
    sbuf_puts(b, expr_ops_expression_name(f->ops));
    sbuf_putc(b, '(');
    emit_expr(mu, b, 0);
    sbuf_puts(b, ", ");
    emit_expr(nu, b, 0);
    sbuf_puts(b, ", ");
    emit_expr(argument, b, 0);
    sbuf_putc(b, ')');
}

static void emit_TeX_polygamma(const expr_t *f, sbuf_t *b)
{
    long order;
    char buf[32];

    if (!f || !expr_is_op(f, &ops_polygamma) || !f->a || !f->b)
        return;
    sbuf_puts(b, "\\psi^{(");
    if (expr_polygamma_order(f, &order)) {
        snprintf(buf, sizeof(buf), "%ld", order);
        sbuf_puts(b, buf);
    } else {
        emit_TeX_expr(f->a, b, PREC_LOWEST);
    }
    sbuf_puts(b, ")}(");
    emit_TeX_expr(f->b, b, 0);
    sbuf_putc(b, ')');
}

static void emit_TeX_polylog(const expr_t *f, sbuf_t *b)
{
    long order;
    char buf[32];

    if (!expr_polylog_order(f, &order))
        return;
    snprintf(buf, sizeof(buf), "%ld", order);
    sbuf_puts(b, "\\operatorname{Li}_{");
    sbuf_puts(b, buf);
    sbuf_puts(b, "}(");
    emit_TeX_expr(f->b, b, 0);
    sbuf_putc(b, ')');
}

static void emit_TeX_harmonic_poly(const expr_t *f, sbuf_t *b)
{
    if (!f || !expr_is_op(f, &ops_harmonic_poly) || !f->a || !f->b)
        return;
    sbuf_puts(b, "H_{");
    emit_TeX_expr(f->a, b, PREC_LOWEST);
    sbuf_puts(b, "}(");
    emit_TeX_expr(f->b, b, PREC_LOWEST);
    sbuf_putc(b, ')');
}

static void emit_TeX_legendre_chi(const expr_t *f, sbuf_t *b)
{
    long order;
    char buf[32];

    if (!expr_legendre_chi_order(f, &order))
        return;
    snprintf(buf, sizeof(buf), "%ld", order);
    sbuf_puts(b, "\\chi_{");
    sbuf_puts(b, buf);
    sbuf_puts(b, "}(");
    emit_TeX_expr(f->b, b, 0);
    sbuf_putc(b, ')');
}

static void emit_TeX_appell_f1(const expr_t *f, sbuf_t *b)
{
    const expr_t *a = NULL;
    const expr_t *b1 = NULL;
    const expr_t *b2 = NULL;
    const expr_t *c = NULL;
    const expr_t *x = NULL;
    const expr_t *y = NULL;

    if (!expr_appell_f1_unpack(f, &a, &b1, &b2, &c, &x, &y))
        return;
    sbuf_puts(b, "F_{1}\\left(");
    emit_TeX_expr(a, b, 0);
    sbuf_puts(b, "; ");
    emit_TeX_expr(b1, b, 0);
    sbuf_puts(b, ", ");
    emit_TeX_expr(b2, b, 0);
    sbuf_puts(b, "; ");
    emit_TeX_expr(c, b, 0);
    sbuf_puts(b, "; ");
    emit_TeX_expr(x, b, 0);
    sbuf_puts(b, ", ");
    emit_TeX_expr(y, b, 0);
    sbuf_puts(b, "\\right)");
}

static void emit_TeX_lauricella_f(const expr_t *f, sbuf_t *out)
{
    const expr_t *a = NULL;
    const expr_t **parameters = NULL;
    const expr_t *c = NULL;
    const expr_t **variables = NULL;
    size_t count = 0u;
    char prefix[64];

    if (!expr_lauricella_f_unpack(f, &a, &parameters, &c, &variables, &count))
        return;
    snprintf(prefix, sizeof(prefix), "F_D^{(%zu)}\\left(", count);
    sbuf_puts(out, prefix);
    emit_TeX_expr(a, out, 0);
    sbuf_puts(out, "; ");
    for (size_t i = 0u; i < count; ++i) {
        if (i > 0u)
            sbuf_puts(out, ", ");
        emit_TeX_expr(parameters[i], out, 0);
    }
    sbuf_puts(out, "; ");
    emit_TeX_expr(c, out, 0);
    sbuf_puts(out, "; ");
    for (size_t i = 0u; i < count; ++i) {
        if (i > 0u)
            sbuf_puts(out, ", ");
        emit_TeX_expr(variables[i], out, 0);
    }
    sbuf_puts(out, "\\right)");
    free(variables);
    free(parameters);
}

static void emit_TeX_hypergeometric_pFq(const expr_t *f, sbuf_t *b)
{
    const expr_t **upper = NULL;
    const expr_t **lower = NULL;
    const expr_t *argument = NULL;
    size_t p = 0u;
    size_t q = 0u;
    char order[64];

    if (!expr_hypergeometric_pFq_unpack(f, &upper, &p, &lower, &q, &argument))
        return;
    snprintf(order, sizeof(order), "{}_{%zu}F_{%zu}\\left(", p, q);
    sbuf_puts(b, order);
    if (p == 0u)
        sbuf_puts(b, "-");
    for (size_t i = 0u; i < p; ++i) {
        if (i > 0u)
            sbuf_puts(b, ", ");
        emit_TeX_expr(upper[i], b, 0);
    }
    sbuf_puts(b, "; ");
    if (q == 0u)
        sbuf_puts(b, "-");
    for (size_t i = 0u; i < q; ++i) {
        if (i > 0u)
            sbuf_puts(b, ", ");
        emit_TeX_expr(lower[i], b, 0);
    }
    sbuf_puts(b, "; ");
    emit_TeX_expr(argument, b, 0);
    sbuf_puts(b, "\\right)");
    free(lower);
    free(upper);
}

static void emit_TeX_lommel_s(const expr_t *f, sbuf_t *b)
{
    const expr_t *mu = NULL;
    const expr_t *nu = NULL;
    const expr_t *argument = NULL;

    if (!expr_lommel_s_unpack(f, &mu, &nu, &argument))
        return;
    sbuf_puts(b, "s_{");
    emit_TeX_expr(mu, b, 0);
    sbuf_puts(b, ",");
    emit_TeX_expr(nu, b, 0);
    sbuf_puts(b, "}\\left(");
    emit_TeX_expr(argument, b, 0);
    sbuf_puts(b, "\\right)");
}

static void emit_TeX_lambert_wn(const expr_t *f, sbuf_t *b)
{
    sbuf_puts(b, "W_{");
    emit_TeX_expr(f->a, b, 0);
    sbuf_puts(b, "}(");
    emit_TeX_expr(f->b, b, 0);
    sbuf_putc(b, ')');
}

static void emit_func_polygamma(const expr_t *f, sbuf_t *b)
{
    long order;
    char buf[32];

    if (!expr_polygamma_order(f, &order))
        return;
    snprintf(buf, sizeof(buf), "%ld", order);
    emit_function_builtin_name(b, f->ops);
    sbuf_putc(b, '(');
    sbuf_puts(b, buf);
    sbuf_puts(b, ", ");
    emit_func(f->b, b, 0);
    sbuf_putc(b, ')');
}

static void emit_func_appell_f1(const expr_t *f, sbuf_t *b)
{
    const expr_t *a = NULL;
    const expr_t *b1 = NULL;
    const expr_t *b2 = NULL;
    const expr_t *c = NULL;
    const expr_t *x = NULL;
    const expr_t *y = NULL;

    if (!expr_appell_f1_unpack(f, &a, &b1, &b2, &c, &x, &y))
        return;
    emit_function_builtin_name(b, f->ops);
    sbuf_putc(b, '(');
    emit_func(a, b, 0);
    sbuf_puts(b, ", ");
    emit_func(b1, b, 0);
    sbuf_puts(b, ", ");
    emit_func(b2, b, 0);
    sbuf_puts(b, ", ");
    emit_func(c, b, 0);
    sbuf_puts(b, ", ");
    emit_func(x, b, 0);
    sbuf_puts(b, ", ");
    emit_func(y, b, 0);
    sbuf_putc(b, ')');
}

static void emit_func_lauricella_f(const expr_t *f, sbuf_t *out)
{
    const expr_t *a = NULL;
    const expr_t **parameters = NULL;
    const expr_t *c = NULL;
    const expr_t **variables = NULL;
    size_t count = 0u;
    char prefix[64];

    if (!expr_lauricella_f_unpack(f, &a, &parameters, &c, &variables, &count))
        return;
    snprintf(prefix, sizeof(prefix), "(%zu, ", count);
    emit_function_builtin_name(out, f->ops);
    sbuf_puts(out, prefix);
    emit_func(a, out, 0);
    for (size_t i = 0u; i < count; ++i) {
        sbuf_puts(out, ", ");
        emit_func(parameters[i], out, 0);
    }
    sbuf_puts(out, ", ");
    emit_func(c, out, 0);
    for (size_t i = 0u; i < count; ++i) {
        sbuf_puts(out, ", ");
        emit_func(variables[i], out, 0);
    }
    sbuf_putc(out, ')');
    free(variables);
    free(parameters);
}

static void emit_func_hypergeometric_pFq(const expr_t *f, sbuf_t *b)
{
    const expr_t **upper = NULL;
    const expr_t **lower = NULL;
    const expr_t *argument = NULL;
    size_t p = 0u;
    size_t q = 0u;
    char counts[64];

    if (!expr_hypergeometric_pFq_unpack(f, &upper, &p, &lower, &q, &argument))
        return;
    snprintf(counts, sizeof(counts), "(%zu, %zu", p, q);
    emit_function_builtin_name(b, f->ops);
    sbuf_puts(b, counts);
    for (size_t i = 0u; i < p; ++i) {
        sbuf_puts(b, ", ");
        emit_func(upper[i], b, 0);
    }
    for (size_t i = 0u; i < q; ++i) {
        sbuf_puts(b, ", ");
        emit_func(lower[i], b, 0);
    }
    sbuf_puts(b, ", ");
    emit_func(argument, b, 0);
    sbuf_putc(b, ')');
    free(lower);
    free(upper);
}

static void emit_func_lommel_s(const expr_t *f, sbuf_t *b)
{
    const expr_t *mu = NULL;
    const expr_t *nu = NULL;
    const expr_t *argument = NULL;

    if (!expr_lommel_s_unpack(f, &mu, &nu, &argument))
        return;
    emit_function_builtin_name(b, f->ops);
    sbuf_putc(b, '(');
    emit_func(mu, b, 0);
    sbuf_puts(b, ", ");
    emit_func(nu, b, 0);
    sbuf_puts(b, ", ");
    emit_func(argument, b, 0);
    sbuf_putc(b, ')');
}

static void emit_func_lambert_wn(const expr_t *f, sbuf_t *b)
{
    emit_function_builtin_name(b, f->ops);
    sbuf_putc(b, '(');
    emit_func(f->a, b, 0);
    sbuf_puts(b, ", ");
    emit_func(f->b, b, 0);
    sbuf_putc(b, ')');
}

/* Emit the canonical built-in name stored for FUNCTION source. */
void emit_function_builtin_name(sbuf_t *b, const expr_ops_t *ops)
{
    sbuf_puts(b, expr_ops_function_name(ops));
}

static int TeX_exp_needs_parens(const expr_t *e)
{
    if (!e)
        return 0;
    if (e->ops->arity == EXPR_OP_ATOM)
        return 0;
    if (expr_is_neg(e) || expr_is_pow_d_expr(e))
        return 1;
    if (e->ops->arity == EXPR_OP_UNARY)
        return 0;
    if (expr_is_addsub(e) || expr_is_mul(e) || expr_is_op(e, &ops_div) || expr_is_op(e, &ops_pow))
        return 1;
    return 0;
}

static void emit_TeX_factor_abs(const expr_t *f, sbuf_t *b)
{
    if (expr_is_negative(f))
        emit_TeX_expr_abs(f, b, PREC_MUL);
    else
        emit_TeX_expr(f, b, PREC_MUL);
}

static void emit_TeX_mul_separator(const expr_t *left, const expr_t *right, sbuf_t *b)
{
    const expr_t *right_power_base = NULL;

    if (right && (expr_is_op(right, &ops_pow) || expr_is_pow_d_expr(right)))
        right_power_base = right->a;

    if (left && expr_is_const(left) && (!left->name || !*left->name) &&
        ((right_power_base && expr_is_const(right_power_base) &&
          (!right_power_base->name || !*right_power_base->name)) ||
         (right && expr_is_const(right) && right->binding_expr &&
          expr_binding_expr_needs_explicit_mul_separator(right->binding_expr)))) {
        sbuf_puts(b, " \\times ");
        return;
    }
    sbuf_puts(b, "\\mkern-2mu ");
}

static bool expr_is_rendered_log_local(const expr_t *expr)
{
    return expr_is_op(expr, &ops_log) ||
           (expr && expr_is_const(expr) && expr->binding_expr &&
            expr->binding_expr->kind == EXPR_BINDING_EXPR_UNARY_OP &&
            expr->binding_expr->u.unary_op.ops == &ops_log);
}

static void emit_TeX_expr_abs(const expr_t *f, sbuf_t *b, int parent_prec)
{
    if (!f) {
        sbuf_puts(b, "0");
        return;
    }

    if (expr_tostring_is_negative_const(f)) {
        if (emit_negative_const_binding_expr_abs(f, b, true))
            return;
        {
            number_t positive = num_neg(f->c);

            emit_TeX_number_value(b, positive);
            num_destroy(&positive);
        }
        return;
    }

    if (expr_is_const(f) && expr_renders_negative(f)) {
        if (emit_negative_const_binding_expr_abs(f, b, true))
            return;
        {
            number_t positive = num_neg(f->c);

            emit_TeX_number_value(b, positive);
            num_destroy(&positive);
        }
        return;
    }

    if (expr_is_neg(f)) {
        emit_TeX_expr(f->a, b, parent_prec);
        return;
    }

    if (expr_is_mul(f)) {
        expr_t *fac[64];
        int n = 0;

        flatten_mul((expr_t *)f, fac, &n, 64);
        sort_factors(fac, n);

        for (int i = 0; i < n; ++i) {
            if (expr_tostring_is_negative_const(fac[i]) && num_eq(fac[i]->c, NUM_NEG_ONE)) {
                for (int j = i; j < n - 1; ++j)
                    fac[j] = fac[j + 1];
                --n;
                break;
            }
        }

        for (int i = 0; i < n; ++i) {
            if (i > 0)
                emit_TeX_mul_separator(fac[i - 1], fac[i], b);
            if (n > 1 && mul_factor_needs_visible_parens(fac[i]))
                sbuf_puts(b, "\\left(");
            emit_TeX_factor_abs(fac[i], b);
            if (n > 1 && mul_factor_needs_visible_parens(fac[i]))
                sbuf_puts(b, "\\right)");
        }
        return;
    }

    if (expr_is_op(f, &ops_div) && expr_is_negative(f->a)) {
        int need = PREC_MUL < parent_prec;
        if (need)
            sbuf_puts(b, "\\left(");
        sbuf_puts(b, "\\frac{");
        emit_TeX_expr_abs(f->a, b, PREC_LOWEST);
        sbuf_puts(b, "}{");
        emit_TeX_expr(f->b, b, PREC_LOWEST);
        sbuf_putc(b, '}');
        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    emit_TeX_expr(f, b, parent_prec);
}

static bool emit_expr_abs_needs_visible_add_parens(const expr_t *expr)
{
    const expr_t *constant = expr_is_neg(expr) ? expr->a : expr;

    return constant && expr_is_const(constant) && mul_factor_needs_visible_parens(constant);
}

static bool emit_TeX_expr_abs_needs_visible_add_parens(const expr_t *expr)
{
    return emit_expr_abs_needs_visible_add_parens(expr);
}

static void emit_func_abs(const expr_t *f, sbuf_t *b, int parent_prec)
{
    sbuf_t tmp;

    if (!f) {
        sbuf_puts(b, "0");
        return;
    }

    if (expr_is_neg(f)) {
        emit_func(f->a, b, parent_prec);
        return;
    }

    if (!expr_renders_negative(f)) {
        emit_func(f, b, parent_prec);
        return;
    }

    sbuf_init(&tmp);
    emit_func(f, &tmp, parent_prec);
    if (sbuf_len(&tmp) > 0u && rune_is_equal(string_at(tmp.text, 0u), '-')) {
        string_t *tail = string_substr(tmp.text, 1u, sbuf_len(&tmp) - 1u);

        if (tail) {
            sbuf_put_string(b, tail);
            string_free(tail);
        }
    } else {
        sbuf_put_string(b, tmp.text);
    }
    sbuf_free(&tmp);
}

static _Thread_local unsigned TeX_expression_depth;
static void emit_TeX_expr_inner(const expr_t *f, sbuf_t *b, int parent_prec);

typedef struct TeX_index_domain {
    const expr_t *index;
    bool nonnegative_integer;
    const struct TeX_index_domain *outer;
} TeX_index_domain_t;

static _Thread_local const TeX_index_domain_t *TeX_index_domain;

static bool TeX_known_nonnegative_integer(const expr_t *expr)
{
    if (!expr)
        return false;
    /* The bounded scope stack also records unknown domains, so inner binders shadow outer ones. */
    for (const TeX_index_domain_t *scope = TeX_index_domain; scope; scope = scope->outer) {
        if (expr == scope->index || (expr->name && scope->index && scope->index->name &&
                                    strcmp(expr->name, scope->index->name) == 0))
            return scope->nonnegative_integer;
    }
    if (expr_is_const(expr) && !expr->name &&
        (!expr->binding_expr || expr_binding_expr_is_numeric_literal(expr->binding_expr)))
        return num_is_real(expr->c) && num_is_integer(expr->c) && num_ge(expr->c, NUM_ZERO);
    if (expr_is_op(expr, &ops_add) || expr_is_op(expr, &ops_mul))
        return TeX_known_nonnegative_integer(expr->a) && TeX_known_nonnegative_integer(expr->b);
    if (expr_is_op(expr, &ops_pow_d))
        return TeX_known_nonnegative_integer(expr->a) && num_is_real(expr->c) &&
               num_is_integer(expr->c) && num_ge(expr->c, NUM_ZERO);
    if (expr_is_op(expr, &ops_factorial))
        return TeX_known_nonnegative_integer(expr->a);
    return false;
}

static const expr_t *TeX_postfix_factorial_argument(const expr_t *expr)
{
    if (expr_is_op(expr, &ops_factorial))
        return TeX_known_nonnegative_integer(expr->a) ? expr->a : NULL;
    if (expr_is_op(expr, &ops_gamma) && expr_is_op(expr->a, &ops_add)) {
        const expr_t *left = expr->a->a;
        const expr_t *right = expr->a->b;

        if (expr_is_const(right) && !right->name && num_eq(right->c, NUM_ONE) &&
            TeX_known_nonnegative_integer(left))
            return left;
        if (expr_is_const(left) && !left->name && num_eq(left->c, NUM_ONE) &&
            TeX_known_nonnegative_integer(right))
            return right;
    }
    return NULL;
}

void emit_TeX_expr(const expr_t *f, sbuf_t *b, int parent_prec)
{
    expr_cartesian_composition_t view;

    if (TeX_expression_depth++ == 0u && expr_cartesian_composition_init(f, &view)) {
        sbuf_puts(b, "\\begin{aligned}\n&");
        emit_TeX_expr(view.compact, b, PREC_LOWEST);
        sbuf_puts(b, ",\\\\\n");
        emit_TeX_expr(view.real_name, b, PREC_LOWEST);
        sbuf_puts(b, "&=");
        emit_TeX_expr(view.real_definition, b, PREC_LOWEST);
        sbuf_puts(b, ",\\\\\n");
        emit_TeX_expr(view.imaginary_name, b, PREC_LOWEST);
        sbuf_puts(b, "&=");
        emit_TeX_expr(view.imaginary_definition, b, PREC_LOWEST);
        sbuf_puts(b, "\\end{aligned}");
        expr_cartesian_composition_clear(&view);
    } else {
        emit_TeX_expr_inner(f, b, parent_prec);
    }
    --TeX_expression_depth;
}

static void emit_TeX_expr_inner(const expr_t *f, sbuf_t *b, int parent_prec)
{
    if (!f) {
        sbuf_puts(b, "0");
        return;
    }

    if (expr_is_formal_derivative(f)) {
        emit_formal_derivative_TeX(f, b);
        return;
    }
    if (expr_is_arbitrary_function(f)) {
        emit_arbitrary_function_TeX(f, b);
        return;
    }
    if (expr_is_op(f, &ops_argument_list)) {
        emit_argument_list_TeX(f, b);
        return;
    }

    if (expr_is_const(f) || expr_is_var(f)) {
        emit_TeX_atom(f, b);
        return;
    }

    if (expr_is_op(f, &ops_integral)) {
        emit_TeX_integral(f, b, parent_prec);
        return;
    }

    if (expr_is_neg(f)) {
        int need = PREC_UNARY < parent_prec;
        const expr_t *a = f->a;

        if (need)
            sbuf_puts(b, "\\left(");
        if (expr_is_neg(a)) {
            emit_TeX_expr(a->a, b, 0);
        } else if (expr_is_negative(a)) {
            emit_TeX_expr_abs(a, b, 0);
        } else {
            int child_needs_paren = expr_is_addsub(a);
            sbuf_putc(b, '-');
            if (child_needs_paren)
                sbuf_puts(b, "\\left(");
            emit_TeX_expr(a, b, 0);
            if (child_needs_paren)
                sbuf_puts(b, "\\right)");
        }
        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (f->ops->arity == EXPR_OP_UNARY) {
        int need = PREC_UNARY < parent_prec;
        const char *name = TeX_unary_name(f);
        const expr_t *factorial_argument = TeX_postfix_factorial_argument(f);

        if (need)
            sbuf_puts(b, "\\left(");

        if (TeX_expression_depth == 1u &&
            (emit_TeX_logarithmic_integral_cartesian(f, b) || emit_TeX_exponential_integral_cartesian(f, b) ||
             emit_TeX_cube_root_cartesian(f, b) || emit_TeX_analytic_unary_cartesian(f, b))) {
            /* The complete Cartesian identity has already been emitted. */
        } else if (expr_is_op(f, &ops_factorial) || factorial_argument) {
            if (!factorial_argument) {
                expr_t *argument = expr_add_long(f->a, 1L);

                sbuf_puts(b, "\\Gamma(");
                emit_TeX_expr(argument, b, PREC_LOWEST);
                sbuf_putc(b, ')');
                expr_free(argument);
                if (need)
                    sbuf_puts(b, "\\right)");
                return;
            }
            bool group_argument = factorial_argument->ops->arity != EXPR_OP_ATOM;

            if (group_argument)
                sbuf_puts(b, "\\left(");
            emit_TeX_expr(factorial_argument, b, PREC_LOWEST);
            if (group_argument)
                sbuf_puts(b, "\\right)");
            sbuf_putc(b, '!');
        } else if (expr_is_op(f, &ops_abs)) {
            sbuf_puts(b, "\\left|");
            emit_TeX_expr_abs(f->a, b, 0);
            sbuf_puts(b, "\\right|");
        } else if (expr_is_op(f, &ops_floor)) {
            sbuf_puts(b, "\\left\\lfloor ");
            emit_TeX_expr(f->a, b, 0);
            sbuf_puts(b, " \\right\\rfloor");
        } else if (expr_is_op(f, &ops_ceil)) {
            sbuf_puts(b, "\\left\\lceil ");
            emit_TeX_expr(f->a, b, 0);
            sbuf_puts(b, " \\right\\rceil");
        } else if (expr_is_sqrt_expr(f)) {
            sbuf_puts(b, "\\sqrt{");
            emit_TeX_expr(f->a, b, 0);
            sbuf_putc(b, '}');
        } else if (expr_is_op(f, &ops_cubrt)) {
            sbuf_puts(b, "\\sqrt[3]{");
            emit_TeX_expr(f->a, b, 0);
            sbuf_putc(b, '}');
        } else if (expr_is_op(f, &ops_exp)) {
            if (!emit_TeX_exp_unit_fraction_root(f->a, b)) {
                sbuf_puts(b, "e^{");
                emit_TeX_expr(f->a, b, 0);
                sbuf_putc(b, '}');
            }
        } else {
            sbuf_puts(b, name ? name : "\\operatorname{f}");
            if (TeX_unary_arg_is_greek_symbol(f->a)) {
                sbuf_putc(b, ' ');
                emit_TeX_expr(f->a, b, PREC_UNARY);
            } else {
                sbuf_putc(b, '(');
                emit_TeX_expr(f->a, b, 0);
                sbuf_putc(b, ')');
            }
        }

        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (expr_is_pow_d_expr(f)) {
        int need = PREC_POW < parent_prec;
        long ei = 0;
        int exponent_has_small_int = expr_try_get_small_integer_exponent(f->c, &ei);

        if (emit_TeX_unit_fraction_power(f->a, f->c, b, parent_prec))
            return;

        if (exponent_has_small_int && ei < 0) {
            int recip_need = PREC_MUL < parent_prec;
            long positive_exponent = -ei;

            if (recip_need)
                sbuf_puts(b, "\\left(");
            sbuf_puts(b, "\\frac{1}{");
            bool group_factorial = positive_exponent != 1L && TeX_postfix_factorial_argument(f->a);

            if (group_factorial)
                sbuf_puts(b, "\\left(");
            emit_TeX_expr(f->a, b, positive_exponent == 1L ? PREC_LOWEST : PREC_POW);
            if (group_factorial)
                sbuf_puts(b, "\\right)");
            if (positive_exponent != 1L) {
                char buf[64];

                snprintf(buf, sizeof(buf), "%ld", positive_exponent);
                sbuf_puts(b, "^{");
                sbuf_puts(b, buf);
                sbuf_putc(b, '}');
            }
            sbuf_putc(b, '}');
            if (recip_need)
                sbuf_puts(b, "\\right)");
            return;
        }

        if (need)
            sbuf_puts(b, "\\left(");

        const char *unary_name = f->a->ops->arity == EXPR_OP_UNARY ? TeX_unary_name(f->a) : NULL;
        if (f->a->ops->arity == EXPR_OP_UNARY && !expr_is_formal_derivative(f->a) && !expr_is_neg(f->a) &&
            !expr_is_sqrt_expr(f->a) && !expr_is_op(f->a, &ops_abs) && !expr_is_op(f->a, &ops_factorial) &&
            !TeX_postfix_factorial_argument(f->a) &&
            !strchr(unary_name ? unary_name : "", '^')) {
            const char *name = unary_name;
            sbuf_puts(b, name ? name : "\\operatorname{f}");
            sbuf_puts(b, "^{");
            if (exponent_has_small_int) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%ld", ei);
                sbuf_puts(b, buf);
            } else {
                emit_TeX_const_value(b, f);
            }
            if (TeX_unary_arg_is_greek_symbol(f->a->a)) {
                sbuf_puts(b, "} ");
                emit_TeX_expr(f->a->a, b, PREC_UNARY);
            } else {
                sbuf_puts(b, "}(");
                emit_TeX_expr(f->a->a, b, 0);
                sbuf_putc(b, ')');
            }
        } else {
            int base_needs_parens = pow_base_needs_visible_parens(f->a) || TeX_postfix_factorial_argument(f->a);

            if (base_needs_parens)
                sbuf_puts(b, "\\left(");
            emit_TeX_expr(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
            if (base_needs_parens)
                sbuf_puts(b, "\\right)");
            sbuf_puts(b, "^{");
            if (exponent_has_small_int) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%ld", ei);
                sbuf_puts(b, buf);
            } else {
                emit_TeX_const_value(b, f);
            }
            sbuf_putc(b, '}');
        }

        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (expr_is_mul(f)) {
        int need = PREC_MUL < parent_prec;
        expr_t *fac[64];
        int n = 0;
        int sign = 1;

        if (need)
            sbuf_puts(b, "\\left(");

        flatten_mul((expr_t *)f, fac, &n, 64);
        sort_factors(fac, n);

        for (int i = 0; i < n; i++) {
            if (!expr_is_negative(fac[i]))
                continue;

            sign = -sign;

            if (expr_tostring_is_negative_const(fac[i])) {
                if (num_eq(fac[i]->c, NUM_NEG_ONE)) {
                    for (int j = i; j < n - 1; j++)
                        fac[j] = fac[j + 1];
                    n--;
                    i--;
                    continue;
                }
                continue;
            }

            if (expr_is_neg(fac[i])) {
                fac[i] = fac[i]->a;
                continue;
            }

            break;
        }

        if (sign < 0)
            sbuf_putc(b, '-');

        for (int i = 0; i < n; i++) {
            if (i > 0)
                emit_TeX_mul_separator(fac[i - 1], fac[i], b);
            if (n > 1 && mul_factor_needs_visible_parens(fac[i]))
                sbuf_puts(b, "\\left(");
            emit_TeX_factor_abs(fac[i], b);
            if (n > 1 && mul_factor_needs_visible_parens(fac[i]))
                sbuf_puts(b, "\\right)");
        }

        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (expr_is_addsub(f)) {
        int need = PREC_ADD < parent_prec;
        bool neg = expr_renders_negative(f->b);
        const expr_t *negative_complex_base = NULL;
        const expr_t *negative_complex_rhs = NULL;
        const expr_t *complex_shift_base = NULL;
        const expr_t *complex_shift_real = NULL;
        const expr_t *complex_shift_imag = NULL;

        if (match_add_negative_complex_rhs(f, &negative_complex_base, &negative_complex_rhs)) {
            if (need)
                sbuf_puts(b, "\\left(");
            emit_TeX_expr(negative_complex_base, b, PREC_ADD);
            sbuf_puts(b, " - \\left(");
            emit_TeX_expr_abs(negative_complex_rhs, b, PREC_ADD);
            sbuf_puts(b, "\\right)");
            if (need)
                sbuf_puts(b, "\\right)");
            return;
        }

        if (match_additive_complex_shift(f, &complex_shift_base, &complex_shift_real, &complex_shift_imag)) {
            bool imag_neg = expr_renders_negative(complex_shift_imag);

            if (need)
                sbuf_puts(b, "\\left(");
            emit_TeX_expr(complex_shift_base, b, PREC_ADD);
            sbuf_puts(b, " - \\left(");
            emit_TeX_expr(complex_shift_real, b, PREC_ADD);
            sbuf_puts(b, imag_neg ? " - " : " + ");
            if (imag_neg)
                emit_TeX_expr_abs(complex_shift_imag, b, PREC_ADD);
            else
                emit_TeX_expr(complex_shift_imag, b, PREC_ADD);
            sbuf_puts(b, "\\right)");
            if (need)
                sbuf_puts(b, "\\right)");
            return;
        }

        if (emit_TeX_display_polynomial_sum(f, b, parent_prec))
            return;

        if (need)
            sbuf_puts(b, "\\left(");
        emit_TeX_expr(f->a, b, PREC_ADD);

        if (expr_is_op(f, &ops_add))
            sbuf_puts(b, neg ? " - " : " + ");
        else
            sbuf_puts(b, neg ? " + " : " - ");

        int rhs_parens = add_rhs_needs_visible_parens(f->b);
        if (neg && !rhs_parens)
            rhs_parens = emit_TeX_expr_abs_needs_visible_add_parens(f->b);
        if (rhs_parens)
            sbuf_puts(b, "\\left(");
        if (neg)
            emit_TeX_expr_abs(f->b, b, PREC_ADD);
        else
            emit_TeX_expr(f->b, b, PREC_ADD);
        if (rhs_parens)
            sbuf_puts(b, "\\right)");

        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (expr_is_op(f, &ops_div)) {
        int need = PREC_MUL < parent_prec;
        bool neg_num = expr_is_negative(f->a);
        bool neg_den = expr_is_negative(f->b);
        const expr_t *atan_expr = NULL;
        const expr_t *denominator = NULL;
        const expr_t *common_numerator_factor = NULL;
        const expr_t *common_sum = NULL;

        if (expr_is_div(f->a) && f->a->a && f->a->b) {
            if (need)
                sbuf_puts(b, "\\left(");
            sbuf_puts(b, "\\frac{");
            emit_TeX_expr(f->a->a, b, PREC_LOWEST);
            sbuf_puts(b, "}{");
            emit_TeX_expr(f->a->b, b, PREC_MUL);
            sbuf_puts(b, "\\mkern-2mu ");
            emit_TeX_expr(f->b, b, PREC_MUL);
            sbuf_putc(b, '}');
            if (need)
                sbuf_puts(b, "\\right)");
            return;
        }

        if (!expr_is_rendered_log_local(f->b) && match_sum_quotient(f, &common_numerator_factor, &common_sum)) {
            if (need)
                sbuf_puts(b, "\\left(");
            sbuf_puts(b, "\\frac{");
            if (common_numerator_factor)
                emit_TeX_expr(common_numerator_factor, b, PREC_LOWEST);
            else
                emit_TeX_expr(common_sum, b, PREC_LOWEST);
            sbuf_puts(b, "}{");
            emit_TeX_expr(f->b, b, PREC_LOWEST);
            sbuf_putc(b, '}');
            if (common_numerator_factor) {
                sbuf_puts(b, "\\mkern-2mu \\left(");
                emit_TeX_expr(common_sum, b, PREC_LOWEST);
                sbuf_puts(b, "\\right)");
            }
            if (need)
                sbuf_puts(b, "\\right)");
            return;
        }

        if (match_atan_over_argument_denominator(f, &atan_expr, &denominator) && !expr_is_negative(denominator)) {
            if (need)
                sbuf_puts(b, "\\left(");
            sbuf_puts(b, "\\frac{1}{");
            emit_TeX_expr(denominator, b, PREC_LOWEST);
            sbuf_puts(b, "} ");
            emit_TeX_expr(atan_expr, b, PREC_MUL);
            if (need)
                sbuf_puts(b, "\\right)");
            return;
        }

        if (need)
            sbuf_puts(b, "\\left(");
        if (neg_num ^ neg_den)
            sbuf_putc(b, '-');

        sbuf_puts(b, "\\frac{");
        if (neg_num)
            emit_TeX_expr_abs(f->a, b, PREC_LOWEST);
        else
            emit_TeX_expr(f->a, b, PREC_LOWEST);
        sbuf_puts(b, "}{");
        if (neg_den)
            emit_TeX_expr_abs(f->b, b, PREC_LOWEST);
        else
            emit_TeX_expr(f->b, b, PREC_LOWEST);
        sbuf_putc(b, '}');

        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (expr_is_op(f, &ops_pow)) {
        int need = PREC_POW < parent_prec;
        int base_is_composite_binding =
            expr_is_const(f->a) && expr_tostring_should_emit_binding_expr(f->a) && f->a->binding_expr &&
            f->a->binding_expr->kind != EXPR_BINDING_EXPR_NUMBER &&
            f->a->binding_expr->kind != EXPR_BINDING_EXPR_CONST;
        int base_needs_parens =
            pow_base_needs_visible_parens(f->a) || expr_is_op(f->a, &ops_exp) || base_is_composite_binding ||
            TeX_postfix_factorial_argument(f->a);

        if (expr_is_const(f->b) && (!f->b->name || !*f->b->name) &&
            emit_TeX_unit_fraction_power(f->a, f->b->c, b, parent_prec))
            return;

        if (need)
            sbuf_puts(b, "\\left(");
        if (base_needs_parens)
            sbuf_puts(b, "\\left(");
        emit_TeX_expr(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
        if (base_needs_parens)
            sbuf_puts(b, "\\right)");
        sbuf_puts(b, "^{");
        if (TeX_exp_needs_parens(f->b))
            emit_TeX_expr(f->b, b, 0);
        else
            emit_TeX_expr(f->b, b, 0);
        sbuf_putc(b, '}');
        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (f->ops->arity == EXPR_OP_BINARY) {
        if (expr_is_op(f, &ops_root)) {
            sbuf_puts(b, "\\sqrt[");
            emit_TeX_expr(f->b, b, PREC_LOWEST);
            sbuf_puts(b, "]{");
            emit_TeX_expr(f->a, b, PREC_LOWEST);
            sbuf_putc(b, '}');
            return;
        }
        if (expr_is_op(f, &ops_indexed_symbol)) {
            emit_TeX_expr(f->a, b, 0);
            sbuf_puts(b, "_{");
            emit_TeX_expr(f->b, b, 0);
            sbuf_putc(b, '}');
            return;
        }
        if (expr_is_op(f, &ops_summation) || expr_is_op(f, &ops_product)) {
            const expr_t *index = f->b;
            const expr_t *lower = NULL;
            const expr_t *upper = NULL;

            if (expr_is_op(f->b, &ops_argument_list)) {
                index = f->b->a;
                upper = f->b->b;
                if (expr_is_op(upper, &ops_argument_list)) {
                    lower = upper->a;
                    upper = upper->b;
                }
            }
            sbuf_puts(b, expr_is_op(f, &ops_summation) ? "\\sum_{" : "\\prod_{");
            emit_TeX_expr(index, b, 0);
            sbuf_putc(b, '=');
            if (lower)
                emit_TeX_expr(lower, b, 0);
            else
                sbuf_putc(b, '0');
            sbuf_puts(b, "}^{");
            if (upper)
                emit_TeX_expr(upper, b, 0);
            else
                sbuf_puts(b, "\\infty");
            sbuf_putc(b, '}');
            TeX_index_domain_t scope = {index, !lower || TeX_known_nonnegative_integer(lower), TeX_index_domain};

            TeX_index_domain = &scope;
            emit_TeX_expr(f->a, b, PREC_MUL);
            TeX_index_domain = scope.outer;
            return;
        }
        if (expr_is_op(f, &ops_qdigamma)) {
            sbuf_puts(b, "\\psi_{");
            emit_TeX_expr(f->a, b, PREC_LOWEST);
            sbuf_puts(b, "}\\left(");
            emit_TeX_expr(f->b, b, PREC_LOWEST);
            sbuf_puts(b, "\\right)");
            return;
        }
        if (expr_is_op(f, &ops_polygamma)) {
            emit_TeX_polygamma(f, b);
            return;
        }
        if (expr_is_op(f, &ops_zetah) || expr_is_op(f, &ops_zatahp)) {
            sbuf_puts(b, f->ops->TeX_name);
            sbuf_putc(b, '(');
            emit_TeX_expr(f->a, b, 0);
            sbuf_puts(b, ", ");
            emit_TeX_expr(f->b, b, 0);
            sbuf_putc(b, ')');
            return;
        }
        if (expr_is_op(f, &ops_lerch_phi) && f->a && expr_is_op(f->a, &ops_lerch_phi_pack)) {
            sbuf_puts(b, "\\Phi\\left(");
            emit_TeX_expr(f->a->a, b, 0);
            sbuf_puts(b, ", ");
            emit_TeX_expr(f->a->b, b, 0);
            sbuf_puts(b, ", ");
            emit_TeX_expr(f->b, b, 0);
            sbuf_puts(b, "\\right)");
            return;
        }
        if (expr_has_polylog_order(f)) {
            emit_TeX_polylog(f, b);
            return;
        }
        if (expr_is_op(f, &ops_harmonic_poly)) {
            emit_TeX_harmonic_poly(f, b);
            return;
        }
        if (expr_has_legendre_chi_order(f)) {
            emit_TeX_legendre_chi(f, b);
            return;
        }
        if (expr_is_op(f, &ops_bessel_j) || expr_is_op(f, &ops_bessel_y)) {
            sbuf_puts(b, f->ops->TeX_name);
            sbuf_puts(b, "_{");
            emit_TeX_expr(f->a, b, PREC_LOWEST);
            sbuf_puts(b, "}\\left(");
            emit_TeX_expr(f->b, b, PREC_LOWEST);
            sbuf_puts(b, "\\right)");
            return;
        }
        if (expr_is_op(f, &ops_lommel_s)) {
            emit_TeX_lommel_s(f, b);
            return;
        }
        if (expr_is_op(f, &ops_lambert_wn)) {
            emit_TeX_lambert_wn(f, b);
            return;
        }
        if (expr_is_op(f, &ops_appell_f1)) {
            emit_TeX_appell_f1(f, b);
            return;
        }
        if (expr_is_op(f, &ops_lauricella_f)) {
            emit_TeX_lauricella_f(f, b);
            return;
        }
        if (expr_is_op(f, &ops_hypergeometric_pFq)) {
            emit_TeX_hypergeometric_pFq(f, b);
            return;
        }
        if (f->ops->TeX_name)
            sbuf_puts(b, f->ops->TeX_name);
        else {
            sbuf_puts(b, "\\operatorname{");
            sbuf_puts(b, expr_ops_expression_name(f->ops));
            sbuf_putc(b, '}');
        }
        sbuf_putc(b, '(');
        emit_TeX_expr(f->a, b, 0);
        sbuf_puts(b, ", ");
        emit_TeX_expr(f->b, b, 0);
        sbuf_putc(b, ')');
        return;
    }

    emit_TeX_atom(f, b);
}

static _Thread_local unsigned expression_output_depth;
static void emit_expr_inner(const expr_t *f, sbuf_t *b, int parent_prec);

void emit_expr(const expr_t *f, sbuf_t *b, int parent_prec)
{
    expr_cartesian_composition_t view;

    if (expression_output_depth++ == 0u && expr_cartesian_composition_init(f, &view)) {
        emit_expr_inner(view.expanded, b, parent_prec);
        expr_cartesian_composition_clear(&view);
    } else {
        emit_expr_inner(f, b, parent_prec);
    }
    --expression_output_depth;
}

static void emit_expr_inner(const expr_t *f, sbuf_t *b, int parent_prec)
{
    if (!f) {
        sbuf_puts(b, "0");
        return;
    }

    if (emit_expr_integral_cartesian(f, b, parent_prec))
        return;

    if (expr_is_formal_derivative(f)) {
        emit_formal_derivative_expr(f, b);
        return;
    }
    if (expr_is_arbitrary_function(f)) {
        emit_arbitrary_function_expr(f, b);
        return;
    }
    if (expr_is_op(f, &ops_argument_list)) {
        emit_argument_list_expr(f, b);
        return;
    }

    /* Atoms */
    if (expr_is_const(f) || expr_is_var(f)) {
        emit_atom((expr_t *)f, b);
        return;
    }

    if (expr_is_op(f, &ops_integral)) {
        emit_expr_integral(f, b, parent_prec);
        return;
    }

    /* Negation: -a  — only parenthesise when the child is an add/sub */
    if (expr_is_neg(f)) {
        int need = PREC_UNARY < parent_prec;
        if (need)
            sbuf_putc(b, '(');

        const expr_t *a = f->a;
        if (expr_is_neg(a)) {
            emit_expr(a->a, b, 0);
            if (need)
                sbuf_putc(b, ')');
            return;
        }
        if (expr_is_negative(a)) {
            emit_expr_abs(a, b, 0);
            if (need)
                sbuf_putc(b, ')');
            return;
        }
        int child_needs_paren = expr_is_addsub(a);
        sbuf_putc(b, '-');
        if (child_needs_paren)
            sbuf_putc(b, '(');
        emit_expr(a, b, 0);
        if (child_needs_paren)
            sbuf_putc(b, ')');

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    /* Unary ops */
    if (f->ops->arity == EXPR_OP_UNARY) {
        int need = PREC_UNARY < parent_prec;
        if (need)
            sbuf_putc(b, '(');

        if (expr_is_op(f, &ops_abs)) {
            emit_expr_abs_bars(f->a, b);
            if (need)
                sbuf_putc(b, ')');
            return;
        }
        if (expr_is_op(f, &ops_floor)) {
            sbuf_puts(b, "⌊");
            emit_expr(f->a, b, 0);
            sbuf_puts(b, "⌋");
            if (need)
                sbuf_putc(b, ')');
            return;
        }
        if (expr_is_op(f, &ops_ceil)) {
            sbuf_puts(b, "⌈");
            emit_expr(f->a, b, 0);
            sbuf_puts(b, "⌉");
            if (need)
                sbuf_putc(b, ')');
            return;
        }
        if (expr_is_sqrt_expr(f))
            sbuf_puts(b, "√");
        else
            sbuf_puts(b, expr_unary_name(f));
        sbuf_putc(b, '(');
        emit_expr(f->a, b, 0);
        sbuf_putc(b, ')');

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    /* Power */
    if (expr_is_pow_d_expr(f)) {
        int need = PREC_POW < parent_prec;
        long ei = 0;
        int exponent_has_small_int = expr_try_get_small_integer_exponent(f->c, &ei);

        if (num_eq(f->c, NUM_HALF)) {
            emit_expr_sqrt_power(f->a, b, parent_prec, false);
            return;
        }

        if (number_is_neg_half_local(f->c)) {
            emit_expr_sqrt_power(f->a, b, parent_prec, true);
            return;
        }

        if (exponent_has_small_int && ei < 0) {
            int recip_need = PREC_MUL < parent_prec;
            int base_needs_parens = pow_base_needs_visible_parens(f->a);
            long positive_exponent = -ei;

            if (recip_need)
                sbuf_putc(b, '(');
            sbuf_puts(b, "1/");
            if (positive_exponent == 1L) {
                emit_expr(f->a, b, PREC_POW);
            } else {
                if (base_needs_parens)
                    sbuf_putc(b, '(');
                emit_expr(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
                if (base_needs_parens)
                    sbuf_putc(b, ')');
                emit_superscript_int(b, positive_exponent);
            }
            if (recip_need)
                sbuf_putc(b, ')');
            return;
        }

        if (need)
            sbuf_putc(b, '(');

        /* For unary functions raised to a power, write func²(arg)
         * rather than func(arg)² so the exponent binds to the function name.
         * Floor/ceiling keep their mathematical brackets: ⌊x⌋². */
        if (f->a->ops->arity == EXPR_OP_UNARY && !expr_is_formal_derivative(f->a) && !expr_is_neg(f->a)) {
            expr_t *inner = f->a;
            if (expr_is_op(inner, &ops_floor) || expr_is_op(inner, &ops_ceil)) {
                emit_expr(inner, b, PREC_POW);
            } else {
                if (expr_is_sqrt_expr(inner))
                    sbuf_puts(b, "√");
                else
                    sbuf_puts(b, expr_unary_name(inner));
            }

            if (exponent_has_small_int)
                emit_superscript_int(b, ei);
            else {
                sbuf_putc(b, '^');
                char *text = expr_const_to_string_local(f);
                if (text) {
                    sbuf_puts(b, text);
                    free(text);
                }
            }

            if (!expr_is_op(inner, &ops_floor) && !expr_is_op(inner, &ops_ceil)) {
                sbuf_putc(b, '(');
                emit_expr(inner->a, b, 0);
                sbuf_putc(b, ')');
            }

            if (need)
                sbuf_putc(b, ')');
            return;
        }

        {
            int base_needs_parens = pow_base_needs_visible_parens(f->a);

            if (base_needs_parens)
                sbuf_putc(b, '(');
            emit_expr(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
            if (base_needs_parens)
                sbuf_putc(b, ')');
        }

        if (exponent_has_small_int)
            emit_superscript_int(b, ei);
        else {
            sbuf_putc(b, '^');
            char *text = expr_const_to_string_local(f);
            if (text) {
                sbuf_puts(b, text);
                free(text);
            }
        }

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    /* Multiplication with sign folding */
    if (expr_is_mul(f)) {
        int need = PREC_MUL < parent_prec;
        expr_t *fac[64];
        int n = 0;

        if (need)
            sbuf_putc(b, '(');

        flatten_mul((expr_t *)f, fac, &n, 64);
        sort_factors(fac, n);

        int sign = 1;
        for (int i = 0; i < n; i++) {
            if (!expr_is_negative(fac[i]))
                continue;

            sign = -sign;

            if (expr_tostring_is_negative_const(fac[i])) {
                if (num_eq(fac[i]->c, NUM_NEG_ONE)) {
                    for (int j = i; j < n - 1; j++)
                        fac[j] = fac[j + 1];
                    n--;
                    i--;
                    continue;
                }
                continue;
            }

            if (expr_is_neg(fac[i])) {
                fac[i] = fac[i]->a;
                continue;
            }

            break;
        }

        if (sign < 0)
            sbuf_putc(b, '-');

        for (int i = 0; i < n; i++) {
            if (i > 0)
                emit_expr_mul_separator_local(fac[i - 1], fac[i], b);
            if (n > 1 && mul_factor_needs_visible_parens(fac[i]))
                sbuf_putc(b, '(');
            emit_factor_abs(fac[i], b);
            if (n > 1 && mul_factor_needs_visible_parens(fac[i]))
                sbuf_putc(b, ')');
        }

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    /* Addition/subtraction with a + -b → a - b and a - -b → a + b */
    if (expr_is_addsub(f)) {
        int need = PREC_ADD < parent_prec;
        const expr_t *negative_complex_base = NULL;
        const expr_t *negative_complex_rhs = NULL;
        const expr_t *complex_shift_base = NULL;
        const expr_t *complex_shift_real = NULL;
        const expr_t *complex_shift_imag = NULL;

        if (match_add_negative_complex_rhs(f, &negative_complex_base, &negative_complex_rhs)) {
            if (need)
                sbuf_putc(b, '(');
            emit_expr(negative_complex_base, b, PREC_ADD);
            sbuf_puts(b, " - (");
            emit_expr_abs(negative_complex_rhs, b, PREC_ADD);
            sbuf_putc(b, ')');
            if (need)
                sbuf_putc(b, ')');
            return;
        }

        if (match_additive_complex_shift(f, &complex_shift_base, &complex_shift_real, &complex_shift_imag)) {
            bool imag_neg = expr_renders_negative(complex_shift_imag);

            if (need)
                sbuf_putc(b, '(');
            emit_expr(complex_shift_base, b, PREC_ADD);
            sbuf_puts(b, " - (");
            emit_expr(complex_shift_real, b, PREC_ADD);
            sbuf_puts(b, imag_neg ? " - " : " + ");
            if (imag_neg)
                emit_expr_abs(complex_shift_imag, b, PREC_ADD);
            else
                emit_expr(complex_shift_imag, b, PREC_ADD);
            sbuf_putc(b, ')');
            if (need)
                sbuf_putc(b, ')');
            return;
        }

        if (emit_expr_display_polynomial_sum(f, b, parent_prec))
            return;

        if (need)
            sbuf_putc(b, '(');

        emit_expr(f->a, b, PREC_ADD);

        bool neg = expr_renders_negative(f->b);

        /* Emit flipped operator if needed */
        if (expr_is_op(f, &ops_add)) {
            sbuf_puts(b, neg ? " - " : " + ");
        } else { /* subtraction */
            sbuf_puts(b, neg ? " + " : " - ");
        }

        int rhs_parens = add_rhs_needs_visible_parens(f->b);
        if (neg && !rhs_parens)
            rhs_parens = emit_expr_abs_needs_visible_add_parens(f->b);
        if (rhs_parens)
            sbuf_putc(b, '(');
        if (neg) {
            emit_expr_abs(f->b, b, PREC_ADD);
        } else {
            emit_expr(f->b, b, PREC_ADD);
        }
        if (rhs_parens)
            sbuf_putc(b, ')');

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    /* Division: normalise sign onto the outside when possible */
    if (expr_is_op(f, &ops_div)) {
        int need = PREC_MUL < parent_prec;
        bool neg_num = expr_is_negative(f->a);
        bool neg_den = expr_is_negative(f->b);
        bool neg = neg_num ^ neg_den;
        const expr_t *atan_expr = NULL;
        const expr_t *denominator = NULL;
        const expr_t *common_numerator_factor = NULL;
        const expr_t *common_sum = NULL;

        if (expr_is_div(f->a) && f->a->a && f->a->b) {
            if (need)
                sbuf_putc(b, '(');
            emit_expr(f->a->a, b, PREC_MUL);
            sbuf_putc(b, '/');
            emit_expr(f->a->b, b, PREC_MUL);
            sbuf_puts(b, "·");
            emit_expr(f->b, b, PREC_MUL);
            if (need)
                sbuf_putc(b, ')');
            return;
        }

        if (!expr_is_rendered_log_local(f->b) && match_sum_quotient(f, &common_numerator_factor, &common_sum)) {
            if (need)
                sbuf_putc(b, '(');
            if (common_numerator_factor)
                emit_expr(common_numerator_factor, b, PREC_MUL);
            else
                sbuf_putc(b, '1');
            sbuf_putc(b, '/');
            emit_expr(f->b, b, PREC_POW);
            sbuf_puts(b, "·(");
            emit_expr(common_sum, b, PREC_LOWEST);
            sbuf_putc(b, ')');
            if (need)
                sbuf_putc(b, ')');
            return;
        }

        if (match_atan_over_argument_denominator(f, &atan_expr, &denominator) && !expr_is_negative(denominator)) {
            if (need)
                sbuf_putc(b, '(');
            sbuf_puts(b, "1/");
            emit_expr(denominator, b, PREC_POW);
            sbuf_puts(b, "·");
            emit_expr(atan_expr, b, PREC_MUL);
            if (need)
                sbuf_putc(b, ')');
            return;
        }

        if (need)
            sbuf_putc(b, '(');
        if (neg)
            sbuf_putc(b, '-');

        if (neg_num)
            emit_expr_abs(f->a, b, PREC_MUL);
        else
            emit_expr(f->a, b, PREC_MUL);

        sbuf_putc(b, '/');

        if (neg_den)
            emit_expr_abs(f->b, b, PREC_POW);
        else
            emit_expr(f->b, b, PREC_POW);

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    /* Binary power: base^exp  or  base^(exp) when exponent needs grouping */
    if (expr_is_op(f, &ops_pow)) {
        int need = PREC_POW < parent_prec;
        int base_needs_parens = pow_base_needs_visible_parens(f->a);

        if (expr_const_half_can_render_as_sqrt_local(f->b)) {
            emit_expr_sqrt_power(f->a, b, parent_prec, false);
            return;
        }

        if (expr_const_neg_half_can_render_as_sqrt_local(f->b)) {
            emit_expr_sqrt_power(f->a, b, parent_prec, true);
            return;
        }

        if (need)
            sbuf_putc(b, '(');

        if (base_needs_parens)
            sbuf_putc(b, '(');
        emit_expr(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
        if (base_needs_parens)
            sbuf_putc(b, ')');
        sbuf_putc(b, '^');
        int ep = pow_exp_needs_parens(f->b);
        if (ep)
            sbuf_putc(b, '(');
        emit_expr(f->b, b, 0);
        if (ep)
            sbuf_putc(b, ')');

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    /* Named binary functions (e.g. atan2) */
    if (f->ops->arity == EXPR_OP_BINARY) {
        if (expr_is_op(f, &ops_indexed_symbol)) {
            emit_expr(f->a, b, 0);
            sbuf_puts(b, "_(");
            emit_expr(f->b, b, 0);
            sbuf_putc(b, ')');
            return;
        }
        if (expr_is_op(f, &ops_summation) || expr_is_op(f, &ops_product)) {
            const expr_t *index = f->b;
            const expr_t *lower = NULL;
            const expr_t *upper = NULL;
            number_t upper_value = NUM_NAN;
            bool infinite = false;

            if (expr_is_op(f->b, &ops_argument_list)) {
                index = f->b->a;
                upper = f->b->b;
                if (expr_is_op(upper, &ops_argument_list)) {
                    lower = upper->a;
                    upper = upper->b;
                }
            }
            if (upper) {
                upper_value = expr_eval(upper);
                infinite = num_is_inf(upper_value) && num_get_sign(upper_value) > 0;
            }
            sbuf_puts(b, expr_is_op(f, &ops_summation) ? "Σ_(" : "Π_(");
            emit_expr(index, b, 0);
            sbuf_putc(b, '=');
            if (lower && expr_is_addsub(lower))
                sbuf_putc(b, '(');
            if (lower)
                emit_expr(lower, b, 0);
            else
                sbuf_putc(b, '0');
            if (lower && expr_is_addsub(lower))
                sbuf_putc(b, ')');
            sbuf_putc(b, ')');
            sbuf_putc(b, '^');
            if (!upper || infinite)
                sbuf_puts(b, "∞");
            else if (expr_is_addsub(upper))
                sbuf_putc(b, '(');
            if (upper && !infinite)
                emit_expr(upper, b, 0);
            if (upper && !infinite && expr_is_addsub(upper))
                sbuf_putc(b, ')');
            sbuf_putc(b, ' ');
            emit_expr(f->a, b, PREC_MUL);
            num_destroy(&upper_value);
            return;
        }
        if (expr_has_polygamma_order(f)) {
            emit_expr_polygamma(f, b);
            return;
        }
        if (expr_has_polylog_order(f)) {
            emit_expr_polylog(f, b);
            return;
        }
        if (expr_has_legendre_chi_order(f)) {
            emit_expr_legendre_chi(f, b);
            return;
        }
        if (expr_is_op(f, &ops_lambert_wn)) {
            emit_expr_lambert_wn(f, b);
            return;
        }
        if (expr_is_op(f, &ops_lommel_s)) {
            emit_expr_lommel_s(f, b);
            return;
        }
        if (expr_is_op(f, &ops_appell_f1)) {
            emit_expr_appell_f1(f, b);
            return;
        }
        if (expr_is_op(f, &ops_lauricella_f)) {
            emit_expr_lauricella_f(f, b);
            return;
        }
        if (expr_is_op(f, &ops_hypergeometric_pFq)) {
            emit_expr_hypergeometric_pFq(f, b);
            return;
        }
        if (expr_is_op(f, &ops_lerch_phi) && f->a && expr_is_op(f->a, &ops_lerch_phi_pack)) {
            sbuf_puts(b, expr_ops_expression_name(f->ops));
            sbuf_putc(b, '(');
            emit_expr(f->a->a, b, 0);
            sbuf_puts(b, ", ");
            emit_expr(f->a->b, b, 0);
            sbuf_puts(b, ", ");
            emit_expr(f->b, b, 0);
            sbuf_putc(b, ')');
            return;
        }
        else
            sbuf_puts(b, expr_ops_expression_name(f->ops));
        sbuf_putc(b, '(');
        emit_expr(f->a, b, 0);
        sbuf_puts(b, ", ");
        emit_expr(f->b, b, 0);
        sbuf_putc(b, ')');
        return;
    }

    /* Fallback */
    emit_atom((expr_t *)f, b);
}

/* ------------------------------------------------------------------------- */
/* FUNCTION MODE (calculator-style)                                          */
/* ------------------------------------------------------------------------- */

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

static size_t function_ascii_stacked_fraction(char *out, const char *text)
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

static size_t function_ascii_vulgar_fraction(char *out, const char *text)
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

void emit_func_fragment(sbuf_t *b, const char *text)
{
    static const struct {
        const char *symbol;
        const char *alias;
    } named_constants[] = {
        {"π", "@pi"},
        {"φ", "@phi"},
        {"γ", "@gamma"},
        {"τ", "@tau"},
    };
    char *normalised;
    size_t input_length;
    size_t input_index = 0u;
    size_t output_index = 0u;

    if (!text)
        return;

    input_length = strlen(text);
    if (input_length > ((size_t)-1 - 1u) / 3u) {
        sbuf_puts(b, text);
        return;
    }
    normalised = malloc(input_length * 3u + 1u);
    if (!normalised) {
        sbuf_puts(b, text);
        return;
    }

    while (text[input_index] != '\0') {
        size_t fraction_width = function_ascii_stacked_fraction(normalised + output_index, text + input_index);
        bool emitted_named_constant = false;

        if (fraction_width == 0u)
            fraction_width = function_ascii_vulgar_fraction(normalised + output_index, text + input_index);
        if (fraction_width != 0u) {
            output_index += strlen(normalised + output_index);
            input_index += fraction_width;
            continue;
        }
        for (size_t constant_index = 0u;
             constant_index < sizeof(named_constants) / sizeof(named_constants[0]); ++constant_index) {
            size_t symbol_width = strlen(named_constants[constant_index].symbol);

            if (strncmp(text + input_index, named_constants[constant_index].symbol, symbol_width) != 0)
                continue;
            strcpy(normalised + output_index, named_constants[constant_index].alias);
            output_index += strlen(named_constants[constant_index].alias);
            input_index += symbol_width;
            emitted_named_constant = true;
            break;
        }
        if (emitted_named_constant)
            continue;
        if (strncmp(text + input_index, "inf", 3u) == 0 &&
            (input_index == 0u || (!isalnum((unsigned char)text[input_index - 1u]) &&
                                   text[input_index - 1u] != '_' && text[input_index - 1u] != '@')) &&
            !isalnum((unsigned char)text[input_index + 3u]) && text[input_index + 3u] != '_') {
            memcpy(normalised + output_index, "@inf", 4u);
            output_index += 4u;
            input_index += 3u;
            continue;
        }
        if (text[input_index] == ' ') {
            size_t operator_index = input_index;

            while (text[operator_index] == ' ')
                ++operator_index;
            if (text[operator_index] == '*') {
                input_index = operator_index + 1u;
                while (text[input_index] == ' ')
                    ++input_index;
                normalised[output_index++] = '.';
                continue;
            }
        }
        if (text[input_index] == '*') {
            normalised[output_index++] = '.';
            ++input_index;
            while (text[input_index] == ' ')
                ++input_index;
            continue;
        }
        normalised[output_index++] = text[input_index++];
    }
    normalised[output_index] = '\0';
    sbuf_puts(b, normalised);
    free(normalised);
}

static void emit_func_additive_chain(const expr_t *expr, sbuf_t *b, bool subtract, bool *emitted)
{
    const char *temporary_name = function_temporary_name(expr);
    bool negative;
    bool effective_subtract;

    if (!temporary_name && expr_is_op(expr, &ops_add)) {
        emit_func_additive_chain(expr->a, b, subtract, emitted);
        emit_func_additive_chain(expr->b, b, subtract, emitted);
        return;
    }
    if (!temporary_name && expr_is_op(expr, &ops_sub)) {
        emit_func_additive_chain(expr->a, b, subtract, emitted);
        emit_func_additive_chain(expr->b, b, !subtract, emitted);
        return;
    }
    if (!temporary_name && expr_is_op(expr, &ops_neg)) {
        emit_func_additive_chain(expr->a, b, !subtract, emitted);
        return;
    }

    negative = !temporary_name && expr_renders_negative(expr);
    effective_subtract = subtract != negative;
    if (*emitted)
        sbuf_puts(b, effective_subtract ? " - " : " + ");
    else if (effective_subtract)
        sbuf_putc(b, '-');

    if (negative)
        emit_func_abs(expr, b, PREC_ADD);
    else
        emit_func(expr, b, PREC_ADD);
    *emitted = true;
}

void emit_func(const expr_t *f, sbuf_t *b, int parent_prec)
{
    const char *temporary_name = function_temporary_name(f);

    if (temporary_name) {
        sbuf_puts(b, temporary_name);
        return;
    }

    if (!f) {
        sbuf_puts(b, "0");
        return;
    }


    if (emit_func_integral_cartesian(f, b, parent_prec))
        return;

    if (expr_is_formal_derivative(f)) {
        emit_formal_derivative_func(f, b);
        return;
    }
    if (expr_is_arbitrary_function(f)) {
        emit_arbitrary_function_func(f, b);
        return;
    }
    if (expr_is_op(f, &ops_argument_list)) {
        emit_argument_list_func(f, b);
        return;
    }

    if (expr_is_const(f)) {
        if (expr_tostring_should_emit_binding_expr(f)) {
            char *text = expr_binding_expr_to_function_string(f->binding_expr);

            if (text) {
                emit_func_fragment(b, text);
                free(text);
            }
        } else if (f->name && *f->name)
            emit_name_func(b, f->name);
        else {
            char *text = expr_const_to_string_local(f);
            if (text) {
                emit_func_fragment(b, text);
                free(text);
            }
        }
        return;
    }

    if (expr_is_var(f)) {
        emit_name_func(b, f->name ? f->name : "x");
        return;
    }

    if (expr_is_op(f, &ops_integral)) {
        emit_func_integral(f, b);
        return;
    }

    if (expr_is_op(f, &ops_neg)) {
        int need = PREC_UNARY < parent_prec;
        int nested_negation = f->a && expr_is_op(f->a, &ops_neg);
        int child_precedence = f->a && (expr_is_mul(f->a) || expr_is_op(f->a, &ops_div)) ? PREC_MUL : PREC_UNARY;

        if (need)
            sbuf_putc(b, '(');
        sbuf_putc(b, '-');
        if (nested_negation)
            sbuf_putc(b, '(');
        emit_func(f->a, b, nested_negation ? PREC_LOWEST : child_precedence);
        if (nested_negation)
            sbuf_putc(b, ')');
        if (need)
            sbuf_putc(b, ')');
        return;
    }

    if (f->ops->arity == EXPR_OP_UNARY) {
        int need = PREC_UNARY < parent_prec;
        if (need)
            sbuf_putc(b, '(');

        emit_function_builtin_name(b, f->ops);
        sbuf_putc(b, '(');
        emit_func(f->a, b, 0);
        sbuf_putc(b, ')');

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    if (expr_is_pow_d_expr(f)) {
        int need = PREC_POW < parent_prec;
        int base_needs_parens = pow_base_needs_visible_parens(f->a);
        long ei = 0;
        int exponent_has_small_int = expr_try_get_small_integer_exponent(f->c, &ei);

        if (exponent_has_small_int && ei < 0) {
            int recip_need = PREC_MUL < parent_prec;
            long positive_exponent = -ei;

            if (recip_need)
                sbuf_putc(b, '(');
            sbuf_puts(b, "1/");
            if (positive_exponent == 1L) {
                emit_func(f->a, b, PREC_POW);
            } else {
                if (base_needs_parens)
                    sbuf_putc(b, '(');
                emit_func(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
                if (base_needs_parens)
                    sbuf_putc(b, ')');
                sbuf_putc(b, '^');
                char buf[64];
                snprintf(buf, sizeof(buf), "%ld", positive_exponent);
                sbuf_puts(b, buf);
            }
            if (recip_need)
                sbuf_putc(b, ')');
            return;
        }

        if (need)
            sbuf_putc(b, '(');

        if (base_needs_parens)
            sbuf_putc(b, '(');
        emit_func(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
        if (base_needs_parens)
            sbuf_putc(b, ')');

        sbuf_putc(b, '^');
        char *text = expr_const_to_string_local(f);
        if (text) {
            emit_func_fragment(b, text);
            free(text);
        }

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    if (expr_is_mul(f)) {
        int need = PREC_MUL < parent_prec;
        bool leading_half_as_divisor = false;
        bool emitted = false;
        bool consumed[64] = {false};
        const char *factored_temporary;

        if (need)
            sbuf_putc(b, '(');

        expr_t *fac[64];
        int n = 0;
        flatten_func_mul((expr_t *)f, fac, &n, 64);
        sort_factors(fac, n);
        factored_temporary = function_factored_temporary_name(f, NULL, fac, n, consumed);
        leading_half_as_divisor = n > 1 && !consumed[0] && expr_is_const_half_local(fac[0]);

        for (int i = 0; i < n; i++) {
            if (consumed[i] || (leading_half_as_divisor && i == 0))
                continue;

            if (emitted)
                sbuf_putc(b, '.');
            if ((n > 1 || factored_temporary) && !function_temporary_name(fac[i]) &&
                mul_factor_needs_visible_parens(fac[i]))
                sbuf_putc(b, '(');
            emit_func(fac[i], b, PREC_MUL);
            if ((n > 1 || factored_temporary) && !function_temporary_name(fac[i]) &&
                mul_factor_needs_visible_parens(fac[i]))
                sbuf_putc(b, ')');
            emitted = true;
        }

        if (factored_temporary) {
            if (emitted)
                sbuf_putc(b, '.');
            sbuf_puts(b, factored_temporary);
        }

        if (leading_half_as_divisor)
            sbuf_puts(b, "/2");

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    if (expr_is_addsub(f)) {
        int need = PREC_ADD < parent_prec;
        bool emitted = false;

        if (need)
            sbuf_putc(b, '(');

        emit_func_additive_chain(f, b, false, &emitted);

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    /* Division: a/b */
    if (expr_is_op(f, &ops_div)) {
        int need = PREC_MUL < parent_prec;
        expr_t *numerator_factors[64];
        bool consumed[64] = {false};
        const char *factored_temporary;
        int numerator_count = 0;

        flatten_func_mul(f->a, numerator_factors, &numerator_count, 64);
        sort_factors(numerator_factors, numerator_count);
        factored_temporary =
            function_factored_temporary_name(f, f->b, numerator_factors, numerator_count, consumed);
        if (factored_temporary) {
            bool emitted = false;

            if (need)
                sbuf_putc(b, '(');
            for (int index = 0; index < numerator_count; ++index) {
                if (consumed[index])
                    continue;
                if (emitted)
                    sbuf_putc(b, '.');
                if (mul_factor_needs_visible_parens(numerator_factors[index]))
                    sbuf_putc(b, '(');
                emit_func(numerator_factors[index], b, PREC_MUL);
                if (mul_factor_needs_visible_parens(numerator_factors[index]))
                    sbuf_putc(b, ')');
                emitted = true;
            }
            if (emitted)
                sbuf_putc(b, '.');
            sbuf_puts(b, factored_temporary);
            if (need)
                sbuf_putc(b, ')');
            return;
        }

        if (need)
            sbuf_putc(b, '(');

        emit_func(f->a, b, PREC_MUL);
        sbuf_putc(b, '/');
        emit_func(f->b, b, PREC_POW);

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    /* Binary power: base^exp  or  base^(exp) when exponent needs grouping */
    if (expr_is_op(f, &ops_pow)) {
        int need = PREC_POW < parent_prec;
        int base_needs_parens = function_temporary_name(f->a) ? 0 : pow_base_needs_visible_parens(f->a);
        if (need)
            sbuf_putc(b, '(');

        if (base_needs_parens)
            sbuf_putc(b, '(');
        emit_func(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
        if (base_needs_parens)
            sbuf_putc(b, ')');
        sbuf_putc(b, '^');
        int ep = function_temporary_name(f->b) ? 0 : pow_exp_needs_parens(f->b);
        if (ep)
            sbuf_putc(b, '(');
        emit_func(f->b, b, 0);
        if (ep)
            sbuf_putc(b, ')');

        if (need)
            sbuf_putc(b, ')');
        return;
    }

    /* Named binary functions (e.g. atan2) */
    if (f->ops->arity == EXPR_OP_BINARY) {
        if (expr_is_op(f, &ops_indexed_symbol)) {
            sbuf_puts(b, "indexed(");
            emit_func(f->a, b, 0);
            sbuf_puts(b, ", ");
            emit_func(f->b, b, 0);
            sbuf_putc(b, ')');
            return;
        }
        if (expr_is_op(f, &ops_summation) || expr_is_op(f, &ops_product)) {
            const expr_t *index = f->b;
            const expr_t *lower = NULL;
            const expr_t *upper = NULL;
            number_t upper_value = NUM_NAN;
            bool infinite = false;

            if (expr_is_op(f->b, &ops_argument_list)) {
                index = f->b->a;
                upper = f->b->b;
                if (expr_is_op(upper, &ops_argument_list)) {
                    lower = upper->a;
                    upper = upper->b;
                }
            }
            if (upper) {
                upper_value = expr_eval(upper);
                infinite = num_is_inf(upper_value) && num_get_sign(upper_value) > 0;
            }
            sbuf_puts(b, expr_is_op(f, &ops_summation) ? "sum(" : "product(");
            emit_func(index, b, 0);
            sbuf_puts(b, ", ");
            if (lower)
                emit_func(lower, b, 0);
            else
                sbuf_putc(b, '0');
            sbuf_puts(b, ", ");
            if (upper && !infinite)
                emit_func(upper, b, 0);
            else
                sbuf_puts(b, "@inf");
            sbuf_puts(b, ", ");
            emit_func(f->a, b, 0);
            sbuf_putc(b, ')');
            num_destroy(&upper_value);
            return;
        }
        if (expr_has_polygamma_order(f)) {
            emit_func_polygamma(f, b);
            return;
        }
        if (expr_is_op(f, &ops_lommel_s)) {
            emit_func_lommel_s(f, b);
            return;
        }
        if (expr_is_op(f, &ops_lerch_phi) && f->a && expr_is_op(f->a, &ops_lerch_phi_pack)) {
            emit_function_builtin_name(b, f->ops);
            sbuf_putc(b, '(');
            emit_func(f->a->a, b, 0);
            sbuf_puts(b, ", ");
            emit_func(f->a->b, b, 0);
            sbuf_puts(b, ", ");
            emit_func(f->b, b, 0);
            sbuf_putc(b, ')');
            return;
        }
        if (expr_is_op(f, &ops_appell_f1)) {
            emit_func_appell_f1(f, b);
            return;
        }
        if (expr_is_op(f, &ops_lauricella_f)) {
            emit_func_lauricella_f(f, b);
            return;
        }
        if (expr_is_op(f, &ops_hypergeometric_pFq)) {
            emit_func_hypergeometric_pFq(f, b);
            return;
        }
        if (expr_is_op(f, &ops_lambert_wn)) {
            emit_func_lambert_wn(f, b);
            return;
        }
        emit_function_builtin_name(b, f->ops);
        sbuf_putc(b, '(');
        emit_func(f->a, b, 0);
        sbuf_puts(b, ", ");
        emit_func(f->b, b, 0);
        sbuf_putc(b, ')');
        return;
    }

    emit_name_func(b, f->name ? f->name : "?");
}

/* ------------------------------------------------------------------------- */
/* Public entry points                                                       */
/* ------------------------------------------------------------------------- */

static string_t *expr_to_TeX_text(const expr_t *dv)
{
    char *expr = NULL;
    char *bindings = NULL;
    sbuf_t b;
    string_t *out;

    if (expr_to_TeX_parts(dv, &expr, &bindings) != 0)
        return expr_to_text_expr(dv);

    sbuf_init(&b);
    if (bindings && *bindings) {
        sbuf_puts(&b, "\\left\\{ ");
        sbuf_puts(&b, expr);
        sbuf_puts(&b, " \\;\\middle|\\; ");
        sbuf_puts(&b, bindings);
        sbuf_puts(&b, " \\right\\}");
    } else {
        sbuf_puts(&b, expr);
    }

    free(expr);
    free(bindings);
    out = sbuf_to_string(&b);
    sbuf_free(&b);
    return out;
}

static bool expr_is_trailing_display_space(rune_t rune)
{
    return rune_is_equal(rune, '\n') || rune_is_equal(rune, '\r') || rune_is_equal(rune, ' ') ||
           rune_is_equal(rune, '\t');
}

static string_t *expr_trim_trailing_display_space(string_t *text)
{
    size_t len;
    string_t *trimmed;

    if (!text)
        return NULL;

    len = string_length(text);
    while (len > 0u && expr_is_trailing_display_space(string_at(text, len - 1u)))
        len--;

    if (len == string_length(text))
        return text;

    trimmed = string_substring(text, 0u, len);
    string_free(text);
    return trimmed;
}

string_t *expr_to_text(const expr_t *dv, style_t style)
{
    string_t *text;

    if (!dv)
        return string_new_with("NULL");

    if (style == style_LATEX) {
        text = expr_to_TeX_text(dv);
    } else if (style == style_FUNCTION) {
        text = expr_to_text_function(dv);
    } else if (style == style_UNBOUND) {
        text = expr_to_text_unbound(dv);
    } else {
        text = expr_to_text_expr(dv);
    }

    return expr_trim_trailing_display_space(text);
}

char *expr_to_string(const expr_t *expr, style_t style)
{
    string_t *text = expr_to_text(expr, style);
    const char *src;
    size_t len;
    char *out;

    if (!text)
        return NULL;

    src = string_c_str(text);
    len = strlen(src) + 1u;
    out = malloc(len);
    if (out)
        memcpy(out, src, len);
    string_free(text);
    return out;
}

string_t *expr_vsprintf_text(const char *fmt, va_list ap)
{
    return string_vsprintf_with_callback(fmt, ap, expr_format_callback, NULL);
}

string_t *expr_sprintf_text(const char *fmt, ...)
{
    va_list ap;
    string_t *text;

    va_start(ap, fmt);
    text = expr_vsprintf_text(fmt, ap);
    va_end(ap);
    return text;
}

int expr_sprintf(char *out, size_t out_size, const char *fmt, ...)
{
    int n;
    va_list ap;
    string_t *text;
    size_t len;

    va_start(ap, fmt);
    text = expr_vsprintf_text(fmt, ap);
    va_end(ap);
    if (!text)
        return -1;

    len = string_length(text);
    if (out && out_size > 0u) {
        size_t copy_len = len < out_size - 1u ? len : out_size - 1u;

        memcpy(out, string_c_str(text), copy_len);
        out[copy_len] = '\0';
    }

    n = len <= (size_t)INT_MAX ? (int)len : -1;
    string_free(text);
    return n;
}

int expr_printf(const char *fmt, ...)
{
    va_list ap;
    int written;
    string_t *text;

    va_start(ap, fmt);
    text = expr_vsprintf_text(fmt, ap);
    va_end(ap);
    if (!text)
        return -1;

    written = string_printf("%S", text);
    string_free(text);
    return written;
}

void expr_print(const expr_t *dv)
{
    if (expr_printf("%n\n", dv) < 0)
        string_printf("NULL\n");
}

bool expr_serialize(const expr_t *expr, string_t **out_type, string_t **out_encoding, void **out_data, size_t *out_len)
{
    string_t *type = NULL;
    string_t *encoding = NULL;
    string_t *text = NULL;
    void *payload = NULL;

    if (!expr || !out_type || !out_encoding || !out_data || !out_len)
        return false;

    text = expr_to_text(expr, style_EXPRESSION);
    if (!text)
        return false;

    payload = malloc(string_byte_length(text));
    if (!payload) {
        string_free(text);
        return false;
    }
    memcpy(payload, string_c_str(text), string_byte_length(text));

    type = string_new_with("expr_t");
    encoding = string_new_with("mars/expression");
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

expr_t *expr_deserialise(const void *data, size_t len, const string_t *type, const string_t *encoding)
{
    string_t *text;
    expr_t *expr;

    if (!data || !type || !encoding)
        return NULL;
    if (strcmp(string_c_str(type), "expr_t") != 0 || strcmp(string_c_str(encoding), "mars/expression") != 0)
        return NULL;

    text = string_new();
    if (!text)
        return NULL;
    if (string_append_chars(text, (const char *)data, len) != 0) {
        string_free(text);
        return NULL;
    }
    expr = expr_from_text(text, NULL);
    string_free(text);
    return expr;
}
