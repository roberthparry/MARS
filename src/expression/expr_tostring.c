/* expr_tostring.c - symbolic/string conversion for expr_t
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
 *   style_FUNCTION    — C-like function notation, e.g.
 *                         expression expr(x, y, const c₀) {
 *                             return sin(x) * cos(y);
 *                         }
 *
 *                         expression expr_eval() {
 *                             x = 1;
 *                             y = π/2;
 *                             const c₀ = γ;
 *                             return expr(x, y, c₀);
 *                         }
 *                       Useful for debugging graph structure and generated
 *                       callable forms.
 *
 *   style_TEX         — native TeX notation generated directly from the DAG,
 *                       e.g.
 *                         \left\{ \sin(x)\cos(y) \;\middle|\;
 *                         x = 1, y = \frac{\pi}{2} \right\}
 *
 * Responsibilities of this file:
 *   • Operator precedence and parenthesisation (infix only)
 *   • Unicode superscripts and fraction glyphs for compact powers/coefficients
 *   • Shared low-level expression, TeX, and function-body emitters
 *
 * Style-specific wrappers live in expr_tostring_expr.c,
 * expr_tostring_tex.c, and expr_tostring_func.c.
 *
 * Algebraic simplification is deliberately not part of ordinary rendering:
 * callers see the expression shape they built or parsed.  Owning derivative
 * creation simplifies derivatives in the DAG before they are rendered.
 */

#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <gmp.h>

#include "expr_bindings.h"
#include "expr_internal.h"
#include "expr_tostring.h"
#include "expr_tostring_internal.h"
#include "expression.h"
#include "internal/number_internal.h"
#include "ustring.h"

/* ------------------------------------------------------------------------- */
/* Growable string buffer                                                    */
/* ------------------------------------------------------------------------- */

#define xmalloc expr_tostring_xmalloc
#define xstrdup expr_tostring_xstrdup

/* ------------------------------------------------------------------------- */
/* Precedence and superscripts                                               */
/* ------------------------------------------------------------------------- */

static const char *sup_digits[10] = {
    "⁰","¹","²","³","⁴","⁵","⁶","⁷","⁸","⁹"
};

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
    int  len = 0;
    while (n > 0 && len < (int)sizeof(tmp)) {
        tmp[len++] = (char)('0' + (n % 10));
        n /= 10;
    }
    for (int i = len - 1; i >= 0; --i) {
        int d = tmp[i] - '0';
        sbuf_puts(b, sup_digits[d]);
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

    *out = negative && value == (unsigned long)LONG_MAX + 1u
        ? LONG_MIN
        : (negative ? -(long)value : (long)value);
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

static bool emit_negative_const_binding_expr_abs(const expr_t *f,
                                                 sbuf_t *b,
                                                 bool tex)
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

    raw = tex
        ? expr_binding_expr_to_tex(f->binding_expr)
        : expr_binding_expr_to_string(f->binding_expr);
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
            slice = string_cursor_slice_between(digits_start,
                                                digits_end,
                                                cursor);
            if (slice) {
                sbuf_puts(b, string_c_str(slice));
                string_free(slice);
            }
            sbuf_puts(b, "\\pi");
            slice = string_cursor_slice_between(
                string_cursor_position(cursor),
                string_cursor_end_position(cursor),
                cursor);
            if (slice) {
                sbuf_puts(b, string_c_str(slice));
                string_free(slice);
            }
            ok = true;
            goto done;
        }
        if (!tex && string_cursor_consume(cursor, "·π")) {
            slice = string_cursor_slice_between(digits_start,
                                                digits_end,
                                                cursor);
            if (slice) {
                sbuf_puts(b, string_c_str(slice));
                string_free(slice);
            }
            sbuf_puts(b, "π");
            slice = string_cursor_slice_between(
                string_cursor_position(cursor),
                string_cursor_end_position(cursor),
                cursor);
            if (slice) {
                sbuf_puts(b, string_c_str(slice));
                string_free(slice);
            }
            ok = true;
            goto done;
        }
    }

    slice = string_cursor_slice_between(rest_start,
                                        string_cursor_end_position(cursor),
                                        cursor);
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
    if (!e) return 0;
    if (expr_is_const(e) && !num_is_real(e->c) && !num_eq(e->c, NUM_I))
        return 1;
    if (e->ops->arity == EXPR_OP_ATOM)  return 0;  /* var, const */
    if (expr_is_neg(e))                  return 1;
    if (expr_is_pow_d_expr(e))           return 1;  /* e.g. y² is ambiguous as exponent */
    if (e->ops->arity == EXPR_OP_UNARY)  return 0;  /* sin(…), exp(…), etc. */
    /* EXPR_OP_BINARY: arithmetic/pow need parens; named functions (atan2 …) don't */
    if (expr_is_addsub(e) || expr_is_mul(e) ||
        expr_is_op(e, &ops_div) || expr_is_op(e, &ops_pow)) return 1;
    return 0;
}

static int pow_base_needs_visible_parens(const expr_t *base)
{
    number_t real;
    int has_real_part;

    if (base && (expr_is_pow_d_expr(base) || expr_is_op(base, &ops_pow)))
        return 1;

    if (base && expr_is_const(base) &&
        expr_tostring_should_emit_binding_expr(base) && base->binding_expr) {
        if (base->binding_expr->kind == EXPR_BINDING_EXPR_ADD ||
            base->binding_expr->kind == EXPR_BINDING_EXPR_SUB)
            return 1;
        if (base->binding_expr->kind == EXPR_BINDING_EXPR_UNARY_OP ||
            base->binding_expr->kind == EXPR_BINDING_EXPR_BINARY_OP ||
            base->binding_expr->kind == EXPR_BINDING_EXPR_POWI)
            return 0;
    }

    if (!base || !expr_is_const(base) || num_is_real(base->c))
        return 0;
    real = num_real_part(base->c);
    has_real_part = !num_eq(real, NUM_ZERO);
    num_destroy(&real);
    return has_real_part;
}

static int mul_factor_needs_visible_parens(const expr_t *factor)
{
    number_t real;
    int has_real_part;

    if (factor && expr_is_const(factor) &&
        expr_tostring_should_emit_binding_expr(factor) && factor->binding_expr) {
        if (factor->binding_expr->kind == EXPR_BINDING_EXPR_ADD ||
            factor->binding_expr->kind == EXPR_BINDING_EXPR_SUB)
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
        (expr_is_neg(rhs) && rhs->a &&
         expr_is_const(rhs->a) && mul_factor_needs_visible_parens(rhs->a)))
        return 1;

    if (rhs && expr_is_const(rhs) &&
        expr_tostring_should_emit_binding_expr(rhs) && rhs->binding_expr) {
        if (rhs->binding_expr->kind == EXPR_BINDING_EXPR_ADD ||
            rhs->binding_expr->kind == EXPR_BINDING_EXPR_SUB)
            return 1;
        if (rhs->binding_expr->kind == EXPR_BINDING_EXPR_UNARY_OP ||
            rhs->binding_expr->kind == EXPR_BINDING_EXPR_BINARY_OP ||
            rhs->binding_expr->kind == EXPR_BINDING_EXPR_POWI)
            return 0;
    }

    if (expr_is_neg(rhs) && rhs->a && expr_is_const(rhs->a) &&
        expr_tostring_should_emit_binding_expr(rhs->a) && rhs->a->binding_expr) {
        if (rhs->a->binding_expr->kind == EXPR_BINDING_EXPR_ADD ||
            rhs->a->binding_expr->kind == EXPR_BINDING_EXPR_SUB)
            return 1;
        if (rhs->a->binding_expr->kind == EXPR_BINDING_EXPR_UNARY_OP ||
            rhs->a->binding_expr->kind == EXPR_BINDING_EXPR_BINARY_OP ||
            rhs->a->binding_expr->kind == EXPR_BINDING_EXPR_POWI)
            return 0;
    }

    return expr_is_addsub(rhs);
}

static bool expr_binding_expr_is_number_text_local(const expr_binding_expr_t *expr,
                                                 const char *text)
{
    return expr && expr->kind == EXPR_BINDING_EXPR_NUMBER &&
           expr->u.text && strcmp(expr->u.text, text) == 0;
}

static bool expr_is_preserved_ln10_const_local(const expr_t *f)
{
    const expr_binding_expr_t *expr;

    if (!f || !expr_is_const(f) || !f->binding_expr)
        return false;

    expr = f->binding_expr;
    return expr->kind == EXPR_BINDING_EXPR_UNARY_OP &&
           expr->u.unary_op.ops == &ops_log &&
           expr_binding_expr_is_number_text_local(expr->u.unary_op.child, "10");
}

static bool expr_is_const_half_local(const expr_t *f)
{
    return f && expr_is_const(f) && num_eq(f->c, NUM_HALF);
}

static int is_atomic_for_mul(const expr_t *f);

static void emit_expr_mul_separator_local(const expr_t *left,
                                          const expr_t *right,
                                          sbuf_t *b)
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
    if (!f) return 0;

    if (expr_is_const(f)) {
        /* Unnamed numeric constants are always atomic (e.g. the leading "6" in 6x²).
         * Named constants are atomic only when their name is "simple" (single letter
         * or letter + subscript digits).  Multi-char names like "pi" or "radius"
         * are non-atomic so that a middle-dot separator is inserted between adjacent
         * bracketed terms: [pi]·[radius]² instead of [pi][radius]². */
        if (expr_is_preserved_ln10_const_local(f))
            return 0;
        if (f->binding_expr &&
            expr_binding_expr_needs_explicit_mul_separator(f->binding_expr))
            return 0;
        if (!f->name || !*f->name) return 1;
        return expr_tostring_is_simple_name(f->name);
    }

    if (expr_is_var(f))
        return expr_tostring_is_simple_name(f->name);

    if (expr_tostring_is_var_pow_d(f))
        return expr_tostring_is_simple_name(f->a->name);

    return 0;
}

/* -------------------------------------------------------------
   Factor classification / flattening / ordering
   ------------------------------------------------------------- */

static void flatten_mul(expr_t *f, expr_t **buf, int *count, int max)
{
    if (!f || *count >= max) return;

    if (expr_is_mul(f)) {
        flatten_mul(f->a, buf, count, max);
        flatten_mul(f->b, buf, count, max);
    } else {
        buf[(*count)++] = f;
    }
}

static int expr_tostring_is_named_const_pow_d(const expr_t *f)
{
    return expr_is_pow_d_expr(f) && expr_is_const(f->a) &&
           f->a->name && *f->a->name;
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
        (first != 'x' && first != 'y' && first != 'z'))
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
    return expr_is_var(f) && f->name && *f->name &&
           !expr_tostring_is_primary_variable_name(f->name);
}

static int expr_tostring_is_coefficient_var_pow_d(const expr_t *f)
{
    return expr_is_pow_d_expr(f) && f->a &&
           expr_tostring_is_coefficient_var(f->a);
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
    if (expr_is_neg(f)) f = f->a;

    if (expr_is_const(f)) {
        if (expr_is_preserved_ln10_const_local(f))
            return 5;
        if (!f->name || !*f->name) return 0;
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
    if (!f) return "";
    if (expr_is_var(f)) return f->name ? f->name : "";
    const char *a = first_var_name(f->a);
    if (*a) return a;
    return first_var_name(f->b);
}

/* Counts levels of function *nesting* (not tree depth).
 * pow_d and neg are transparent — cos²(x) has the same nesting depth as cos(x).
 * This makes cos²(x) (depth 1) sort before exp(sin(x)) (depth 2). */
static int factor_depth(const expr_t *f)
{
    if (!f || expr_is_const(f) || expr_is_var(f)) return 0;
    if (expr_is_neg(f) || expr_is_pow_d_expr(f)) return factor_depth(f->a);
    if (f->ops->arity == EXPR_OP_UNARY) return 1 + factor_depth(f->a);
    if (f->ops->arity == EXPR_OP_BINARY) {
        int da = factor_depth(f->a), db = factor_depth(f->b);
        return 1 + (da > db ? da : db);
    }
    return 0;
}

static const char *factor_sort_name(const expr_t *f)
{
    if (expr_is_neg(f)) f = f->a;

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

/* Stable insertion sort for factor arrays.
 * Within group 4 (functions), sort shallower expressions first so that
 * e.g. cos(x) (depth 2) appears before exp(sin(x)) (depth 3). */
static void sort_factors(expr_t **fac, int n)
{
    for (int s = 1; s < n; s++) {
        expr_t *key = fac[s];
        int kg = factor_group(key);
        const char *kn = factor_sort_name(key);
        int kd = (kg == 5) ? factor_depth(key) : 0;
        int t = s - 1;
        while (t >= 0) {
            int tg = factor_group(fac[t]);
            int cmp;
            if (tg != kg) {
                cmp = tg - kg;
            } else if (kg == 5) {
                int td = factor_depth(fac[t]);
                cmp = (td != kd) ? (td - kd) : strcmp(factor_sort_name(fac[t]), kn);
            } else {
                cmp = strcmp(factor_sort_name(fac[t]), kn);
            }
            if (cmp <= 0) break;
            fac[t + 1] = fac[t];
            t--;
        }
        fac[t + 1] = key;
    }
}

/* ------------------------------------------------------------------------- */
/* EXPRESSION MODE (pretty maths)                                            */
/* ------------------------------------------------------------------------- */

void emit_expr(const expr_t *f, sbuf_t *b, int parent_prec);
static void emit_expr_abs(const expr_t *f, sbuf_t *b, int parent_prec);
static void emit_expr_abs_bars(const expr_t *f, sbuf_t *b);
void emit_tex_expr(const expr_t *f, sbuf_t *b, int parent_prec);
static void emit_tex_expr_abs(const expr_t *f, sbuf_t *b, int parent_prec);
void emit_func(const expr_t *f, sbuf_t *b, int parent_prec);
static void emit_func_abs(const expr_t *f, sbuf_t *b, int parent_prec);

static bool match_atan_over_argument_denominator(const expr_t *expr,
                                                 const expr_t **atan_expr_out,
                                                 const expr_t **denominator_out)
{
    if (!expr ||
        !expr_is_op(expr, &ops_div) ||
        !expr->a ||
        !expr->b ||
        !expr_is_op(expr->a, &ops_atan) ||
        !expr->a->a ||
        !expr_is_op(expr->a->a, &ops_div) ||
        !expr->a->a->a ||
        !expr->a->a->b ||
        !expr_struct_eq(expr->a->a->b, expr->b))
        return false;

    if (atan_expr_out)
        *atan_expr_out = expr->a;
    if (denominator_out)
        *denominator_out = expr->b;
    return true;
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
    neg = (sbuf_len(&b) > 0u &&
           rune_is_equal(string_at(b.text, 0u), '-'));
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

static bool display_poly_collect_vars(const expr_t *expr,
                                      display_poly_var_list_t *vars)
{
    if (!expr || !vars)
        return true;

    if (expr_is_var(expr) && expr->name)
        return display_poly_vars_add(vars, expr->name);

    return display_poly_collect_vars(expr->a, vars) &&
           display_poly_collect_vars(expr->b, vars);
}

static int display_poly_var_index(const display_poly_var_list_t *vars,
                                  const char *name)
{
    if (!vars || !name)
        return -1;
    for (size_t i = 0u; i < vars->count; ++i) {
        if (strcmp(vars->name[i], name) == 0)
            return (int)i;
    }
    return -1;
}

static bool display_poly_expr_contains_any_var(const expr_t *expr,
                                               const display_poly_var_list_t *vars)
{
    if (!expr || !vars)
        return false;
    if (expr_is_var(expr) && expr->name &&
        display_poly_var_index(vars, expr->name) >= 0)
        return true;
    return display_poly_expr_contains_any_var(expr->a, vars) ||
           display_poly_expr_contains_any_var(expr->b, vars);
}

static bool display_poly_expr_contains_imaginary_unit(const expr_t *expr)
{
    if (!expr)
        return false;
    if (expr_is_const(expr) && expr->name && strcmp(expr->name, "i") == 0)
        return true;
    return display_poly_expr_contains_imaginary_unit(expr->a) ||
           display_poly_expr_contains_imaginary_unit(expr->b);
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
    return expr && expr_is_const(expr) &&
           (num_eq(expr->c, NUM_I) || num_eq(expr->c, NUM_NEG_I));
}

static bool display_poly_is_imaginary_term(const expr_t *expr)
{
    if (!expr)
        return false;
    if (display_poly_is_i_const(expr) || display_poly_is_pure_imag_const(expr))
        return true;
    return expr_is_mul(expr) &&
           ((display_poly_is_real_const(expr->a) && display_poly_is_i_const(expr->b)) ||
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

static bool match_add_negative_complex_rhs(const expr_t *expr,
                                           const expr_t **base_out,
                                           const expr_t **complex_out)
{
    if (!expr ||
        !expr_is_op(expr, &ops_add) ||
        !expr->a ||
        !display_poly_is_negative_real_complex_const(expr->b))
        return false;

    if (base_out)
        *base_out = expr->a;
    if (complex_out)
        *complex_out = expr->b;
    return true;
}

static bool match_additive_complex_shift(const expr_t *expr,
                                         const expr_t **base_out,
                                         const expr_t **real_out,
                                         const expr_t **imag_out)
{
    if (!expr ||
        !expr_is_op(expr, &ops_add) ||
        !expr->a ||
        !expr->b ||
        !expr_is_op(expr->a, &ops_sub) ||
        !expr->a->a ||
        !expr->a->b ||
        !display_poly_is_real_const(expr->a->b) ||
        !display_poly_is_imaginary_term(expr->b))
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

static bool display_poly_term_degrees(const expr_t *expr,
                                      const display_poly_var_list_t *vars,
                                      long *degree)
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
        ok = display_poly_term_degrees(expr->a, vars, degree) &&
             display_poly_term_degrees(expr->b, vars, degree);
        goto cleanup;
    }

    if (expr_is_op(expr, &ops_div)) {
        ok = !display_poly_expr_contains_any_var(expr->b, vars) &&
             display_poly_term_degrees(expr->a, vars, degree);
        goto cleanup;
    }

    if (expr_is_pow_d_expr(expr)) {
        index = (expr->a && expr_is_var(expr->a) && expr->a->name)
                    ? display_poly_var_index(vars, expr->a->name)
                    : -1;
        if (index >= 0) {
            ok = expr_try_get_small_integer_exponent(expr->c, &power) &&
                 display_poly_add_degree(&degree[index], power);
        } else {
            ok = !display_poly_expr_contains_any_var(expr->a, vars);
        }
        goto cleanup;
    }

    if (expr_is_op(expr, &ops_pow)) {
        index = (expr->a && expr_is_var(expr->a) && expr->a->name)
                    ? display_poly_var_index(vars, expr->a->name)
                    : -1;
        if (index >= 0) {
            ok = expr_match_const_value(expr->b, &exponent) &&
                 expr_try_get_small_integer_exponent(exponent, &power) &&
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

static bool display_poly_collect_add_terms(const expr_t *expr,
                                           bool subtract,
                                           const display_poly_var_list_t *vars,
                                           display_poly_term_t *terms,
                                           size_t *count,
                                           size_t max_terms)
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

static int display_poly_compare_terms(const display_poly_term_t *left,
                                      const display_poly_term_t *right,
                                      size_t var_count)
{
    for (size_t i = 0u; i < var_count; ++i) {
        if (left->degree[i] > right->degree[i])
            return -1;
        if (left->degree[i] < right->degree[i])
            return 1;
    }
    return 0;
}

static void display_poly_sort_terms(display_poly_term_t *terms,
                                    size_t count,
                                    size_t var_count)
{
    for (size_t i = 1u; i < count; ++i) {
        display_poly_term_t key = terms[i];
        size_t j = i;

        while (j > 0u &&
               display_poly_compare_terms(&key, &terms[j - 1u], var_count) < 0) {
            terms[j] = terms[j - 1u];
            --j;
        }
        terms[j] = key;
    }
}

static bool display_poly_prepare_terms(const expr_t *expr,
                                       display_poly_term_t *terms,
                                       size_t *count,
                                       size_t max_terms)
{
    display_poly_var_list_t vars = { 0 };
    bool has_variable_term = false;

    if (!expr || !terms || !count || !expr_is_addsub(expr))
        return false;

    if (display_poly_expr_contains_imaginary_unit(expr) ||
        !display_poly_collect_vars(expr, &vars) ||
        vars.count == 0u)
        return false;

    *count = 0u;
    if (!display_poly_collect_add_terms(expr, false, &vars, terms, count, max_terms) ||
        *count < 2u)
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

    display_poly_sort_terms(terms, *count, vars.count);
    if (terms[0].subtract != (expr_renders_negative(terms[0].expr) ? true : false))
        return false;
    return true;
}

static bool emit_expr_display_polynomial_sum(const expr_t *expr,
                                             sbuf_t *b,
                                             int parent_prec)
{
    display_poly_term_t terms[96];
    size_t count = 0u;
    int need = PREC_ADD < parent_prec;

    if (!display_poly_prepare_terms(expr, terms, &count,
                                    sizeof(terms) / sizeof(terms[0])))
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

static bool emit_tex_display_polynomial_sum(const expr_t *expr,
                                            sbuf_t *b,
                                            int parent_prec)
{
    display_poly_term_t terms[96];
    size_t count = 0u;
    int need = PREC_ADD < parent_prec;

    if (!display_poly_prepare_terms(expr, terms, &count,
                                    sizeof(terms) / sizeof(terms[0])))
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
            emit_tex_expr_abs(terms[i].expr, b, PREC_ADD);
        else
            emit_tex_expr(terms[i].expr, b, PREC_ADD);
        if (term_needs_parens)
            sbuf_puts(b, "\\right)");
    }

    if (need)
        sbuf_puts(b, "\\right)");
    return true;
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
        if (need) sbuf_putc(b, '(');
        emit_expr_abs(f->a, b, PREC_MUL);
        sbuf_putc(b, '/');
        emit_expr(f->b, b, PREC_POW);
        if (need) sbuf_putc(b, ')');
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

void emit_tex_name(sbuf_t *b, const char *name)
{
    char *tex;

    if (!name || !*name) {
        sbuf_puts(b, "x");
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

static void emit_tex_number_value(sbuf_t *b, number_t value)
{
    char *text = expr_number_to_string_local(value);
    char *tex;

    if (!text)
        return;

    tex = expr_tostring_texify(text);
    if (tex) {
        sbuf_puts(b, tex);
        free(tex);
    } else {
        sbuf_puts(b, text);
    }
    free(text);
}

static void emit_tex_const_value(sbuf_t *b, const expr_t *dv)
{
    char *text = expr_const_to_string_local(dv);
    char *tex;

    if (!text)
        return;

    tex = expr_tostring_texify(text);
    if (tex) {
        sbuf_puts(b, tex);
        free(tex);
    } else {
        sbuf_puts(b, text);
    }
    free(text);
}

static bool emit_tex_exp_unit_fraction_root(const expr_t *arg, sbuf_t *b)
{
    long numerator;
    long denominator;

    if (!expr_is_const(arg) ||
        !num_get_small_rational(arg->c, &numerator, &denominator) ||
        numerator != 1L ||
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

static void emit_tex_atom(const expr_t *f, sbuf_t *b)
{
    if (expr_is_const(f)) {
        if (expr_tostring_should_emit_binding_expr(f)) {
            char *text = expr_binding_expr_to_tex(f->binding_expr);

            if (text) {
                sbuf_puts(b, text);
                free(text);
            }
        } else if (f->name && *f->name)
            emit_tex_name(b, f->name);
        else
            emit_tex_const_value(b, f);
        return;
    }

    if (expr_is_var(f)) {
        emit_tex_name(b, f->name ? f->name : "x");
        return;
    }

    emit_tex_number_value(b, expr_eval(f));
}

static const char *tex_unary_name(const expr_t *f)
{
    if (!f || !f->ops)
        return NULL;

    if (expr_is_op(f, &ops_abs))
        return NULL;
    if (expr_is_sqrt_expr(f))
        return "\\sqrt";
    return f->ops->tex_name;
}

static const char *expr_unary_name(const expr_t *f)
{
    if (!f || !f->ops)
        return "?";

    if (expr_is_op(f, &ops_gamma))
        return "Γ";
    if (expr_is_op(f, &ops_digamma))
        return "ψ⁽⁰⁾";
    if (expr_is_op(f, &ops_trigamma))
        return "ψ⁽¹⁾";
    return f->ops->name ? f->ops->name : "?";
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

static void emit_expr_polygamma(const expr_t *f, sbuf_t *b)
{
    long order;

    if (!expr_polygamma_order(f, &order))
        return;
    sbuf_puts(b, "ψ⁽");
    emit_superscript_int(b, order);
    sbuf_puts(b, "⁾(");
    emit_expr(f->b, b, 0);
    sbuf_putc(b, ')');
}

static void emit_tex_polygamma(const expr_t *f, sbuf_t *b)
{
    long order;
    char buf[32];

    if (!expr_polygamma_order(f, &order))
        return;
    sbuf_puts(b, "\\psi^{(");
    snprintf(buf, sizeof(buf), "%ld", order);
    sbuf_puts(b, buf);
    sbuf_puts(b, ")}(");
    emit_tex_expr(f->b, b, 0);
    sbuf_putc(b, ')');
}

static void emit_func_polygamma(const expr_t *f, sbuf_t *b)
{
    long order;
    char buf[32];

    if (!expr_polygamma_order(f, &order))
        return;
    snprintf(buf, sizeof(buf), "%ld", order);
    sbuf_puts(b, "polygamma(");
    sbuf_puts(b, buf);
    sbuf_puts(b, ", ");
    emit_func(f->b, b, 0);
    sbuf_putc(b, ')');
}

static int tex_exp_needs_parens(const expr_t *e)
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

static void emit_tex_factor_abs(const expr_t *f, sbuf_t *b)
{
    if (expr_is_negative(f))
        emit_tex_expr_abs(f, b, PREC_MUL);
    else
        emit_tex_expr(f, b, PREC_MUL);
}

static void emit_tex_expr_abs(const expr_t *f, sbuf_t *b, int parent_prec)
{
    if (!f) {
        sbuf_puts(b, "0");
        return;
    }

    if (expr_tostring_is_negative_const(f)) {
        if (emit_negative_const_binding_expr_abs(f, b, true))
            return;
        emit_tex_number_value(b, num_neg(f->c));
        return;
    }

    if (expr_is_const(f) && expr_renders_negative(f)) {
        if (emit_negative_const_binding_expr_abs(f, b, true))
            return;
        emit_tex_number_value(b, num_neg(f->c));
        return;
    }

    if (expr_is_neg(f)) {
        emit_tex_expr(f->a, b, parent_prec);
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
            if (i > 0) {
                int left_atomic = is_atomic_for_mul(fac[i - 1]);
                int right_atomic = is_atomic_for_mul(fac[i]);

                if (left_atomic && right_atomic)
                    sbuf_putc(b, ' ');
                else
                    sbuf_puts(b, " \\cdot ");
            }
            if (n > 1 && mul_factor_needs_visible_parens(fac[i]))
                sbuf_puts(b, "\\left(");
            emit_tex_factor_abs(fac[i], b);
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
        emit_tex_expr_abs(f->a, b, PREC_LOWEST);
        sbuf_puts(b, "}{");
        emit_tex_expr(f->b, b, PREC_LOWEST);
        sbuf_putc(b, '}');
        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    emit_tex_expr(f, b, parent_prec);
}

static bool emit_expr_abs_needs_visible_add_parens(const expr_t *expr)
{
    const expr_t *constant = expr_is_neg(expr) ? expr->a : expr;

    return constant && expr_is_const(constant) &&
           mul_factor_needs_visible_parens(constant);
}

static bool emit_tex_expr_abs_needs_visible_add_parens(const expr_t *expr)
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
    if (sbuf_len(&tmp) > 0u &&
        rune_is_equal(string_at(tmp.text, 0u), '-')) {
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

void emit_tex_expr(const expr_t *f, sbuf_t *b, int parent_prec)
{
    if (!f) {
        sbuf_puts(b, "0");
        return;
    }

    if (expr_is_const(f) || expr_is_var(f)) {
        emit_tex_atom(f, b);
        return;
    }

    if (expr_is_neg(f)) {
        int need = PREC_UNARY < parent_prec;
        const expr_t *a = f->a;

        if (need)
            sbuf_puts(b, "\\left(");
        if (expr_is_neg(a)) {
            emit_tex_expr(a->a, b, 0);
        } else if (expr_is_negative(a)) {
            emit_tex_expr_abs(a, b, 0);
        } else {
            int child_needs_paren = expr_is_addsub(a);
            sbuf_putc(b, '-');
            if (child_needs_paren)
                sbuf_puts(b, "\\left(");
            emit_tex_expr(a, b, 0);
            if (child_needs_paren)
                sbuf_puts(b, "\\right)");
        }
        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (f->ops->arity == EXPR_OP_UNARY) {
        int need = PREC_UNARY < parent_prec;
        const char *name = tex_unary_name(f);

        if (need)
            sbuf_puts(b, "\\left(");

        if (expr_is_op(f, &ops_abs)) {
            sbuf_puts(b, "\\left|");
            emit_tex_expr_abs(f->a, b, 0);
            sbuf_puts(b, "\\right|");
        } else if (expr_is_op(f, &ops_floor)) {
            sbuf_puts(b, "\\left\\lfloor ");
            emit_tex_expr(f->a, b, 0);
            sbuf_puts(b, " \\right\\rfloor");
        } else if (expr_is_op(f, &ops_ceil)) {
            sbuf_puts(b, "\\left\\lceil ");
            emit_tex_expr(f->a, b, 0);
            sbuf_puts(b, " \\right\\rceil");
        } else if (expr_is_sqrt_expr(f)) {
            sbuf_puts(b, "\\sqrt{");
            emit_tex_expr(f->a, b, 0);
            sbuf_putc(b, '}');
        } else if (expr_is_op(f, &ops_exp)) {
            if (!emit_tex_exp_unit_fraction_root(f->a, b)) {
                sbuf_puts(b, "e^{");
                emit_tex_expr(f->a, b, 0);
                sbuf_putc(b, '}');
            }
        } else {
            sbuf_puts(b, name ? name : "\\operatorname{f}");
            sbuf_putc(b, '(');
            emit_tex_expr(f->a, b, 0);
            sbuf_putc(b, ')');
        }

        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (expr_is_pow_d_expr(f)) {
        int need = PREC_POW < parent_prec;
        long ei = 0;
        int exponent_has_small_int = expr_try_get_small_integer_exponent(f->c, &ei);

        if (exponent_has_small_int && ei < 0) {
            int recip_need = PREC_MUL < parent_prec;
            long positive_exponent = -ei;

            if (recip_need)
                sbuf_puts(b, "\\left(");
            sbuf_puts(b, "\\frac{1}{");
            emit_tex_expr(f->a, b, positive_exponent == 1L ? PREC_LOWEST : PREC_POW);
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

        const char *unary_name = f->a->ops->arity == EXPR_OP_UNARY
            ? tex_unary_name(f->a) : NULL;
        if (f->a->ops->arity == EXPR_OP_UNARY && !expr_is_neg(f->a) &&
            !expr_is_sqrt_expr(f->a) &&
            !expr_is_op(f->a, &ops_abs) &&
            !strchr(unary_name ? unary_name : "", '^')) {
            const char *name = unary_name;
            sbuf_puts(b, name ? name : "\\operatorname{f}");
            sbuf_puts(b, "^{");
            if (exponent_has_small_int) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%ld", ei);
                sbuf_puts(b, buf);
            } else {
                emit_tex_const_value(b, f);
            }
            sbuf_puts(b, "}(");
            emit_tex_expr(f->a->a, b, 0);
            sbuf_putc(b, ')');
        } else {
            int base_needs_parens = pow_base_needs_visible_parens(f->a);

            if (base_needs_parens)
                sbuf_puts(b, "\\left(");
            emit_tex_expr(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
            if (base_needs_parens)
                sbuf_puts(b, "\\right)");
            sbuf_puts(b, "^{");
            if (exponent_has_small_int) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%ld", ei);
                sbuf_puts(b, buf);
            } else {
                emit_tex_const_value(b, f);
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
            if (i > 0) {
                int left_atomic = is_atomic_for_mul(fac[i - 1]);
                int right_atomic = is_atomic_for_mul(fac[i]);

                if (left_atomic && right_atomic)
                    sbuf_putc(b, ' ');
                else
                    sbuf_puts(b, " \\cdot ");
            }
            if (n > 1 && mul_factor_needs_visible_parens(fac[i]))
                sbuf_puts(b, "\\left(");
            emit_tex_factor_abs(fac[i], b);
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

        if (match_add_negative_complex_rhs(f,
                                           &negative_complex_base,
                                           &negative_complex_rhs)) {
            if (need)
                sbuf_puts(b, "\\left(");
            emit_tex_expr(negative_complex_base, b, PREC_ADD);
            sbuf_puts(b, " - \\left(");
            emit_tex_expr_abs(negative_complex_rhs, b, PREC_ADD);
            sbuf_puts(b, "\\right)");
            if (need)
                sbuf_puts(b, "\\right)");
            return;
        }

        if (match_additive_complex_shift(f,
                                         &complex_shift_base,
                                         &complex_shift_real,
                                         &complex_shift_imag)) {
            bool imag_neg = expr_renders_negative(complex_shift_imag);

            if (need)
                sbuf_puts(b, "\\left(");
            emit_tex_expr(complex_shift_base, b, PREC_ADD);
            sbuf_puts(b, " - \\left(");
            emit_tex_expr(complex_shift_real, b, PREC_ADD);
            sbuf_puts(b, imag_neg ? " - " : " + ");
            if (imag_neg)
                emit_tex_expr_abs(complex_shift_imag, b, PREC_ADD);
            else
                emit_tex_expr(complex_shift_imag, b, PREC_ADD);
            sbuf_puts(b, "\\right)");
            if (need)
                sbuf_puts(b, "\\right)");
            return;
        }

        if (emit_tex_display_polynomial_sum(f, b, parent_prec))
            return;

        if (need)
            sbuf_puts(b, "\\left(");
        emit_tex_expr(f->a, b, PREC_ADD);

        if (expr_is_op(f, &ops_add))
            sbuf_puts(b, neg ? " - " : " + ");
        else
            sbuf_puts(b, neg ? " + " : " - ");

        int rhs_parens = add_rhs_needs_visible_parens(f->b);
        if (neg && !rhs_parens)
            rhs_parens = emit_tex_expr_abs_needs_visible_add_parens(f->b);
        if (rhs_parens)
            sbuf_puts(b, "\\left(");
        if (neg)
            emit_tex_expr_abs(f->b, b, PREC_ADD);
        else
            emit_tex_expr(f->b, b, PREC_ADD);
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

        if (match_atan_over_argument_denominator(f, &atan_expr, &denominator) &&
            !expr_is_negative(denominator)) {
            if (need)
                sbuf_puts(b, "\\left(");
            sbuf_puts(b, "\\frac{1}{");
            emit_tex_expr(denominator, b, PREC_LOWEST);
            sbuf_puts(b, "} ");
            emit_tex_expr(atan_expr, b, PREC_MUL);
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
            emit_tex_expr_abs(f->a, b, PREC_LOWEST);
        else
            emit_tex_expr(f->a, b, PREC_LOWEST);
        sbuf_puts(b, "}{");
        if (neg_den)
            emit_tex_expr_abs(f->b, b, PREC_LOWEST);
        else
            emit_tex_expr(f->b, b, PREC_LOWEST);
        sbuf_putc(b, '}');

        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (expr_is_op(f, &ops_pow)) {
        int need = PREC_POW < parent_prec;
        int base_needs_parens = pow_base_needs_visible_parens(f->a);

        if (need)
            sbuf_puts(b, "\\left(");
        if (base_needs_parens)
            sbuf_puts(b, "\\left(");
        emit_tex_expr(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
        if (base_needs_parens)
            sbuf_puts(b, "\\right)");
        sbuf_puts(b, "^{");
        if (tex_exp_needs_parens(f->b))
            emit_tex_expr(f->b, b, 0);
        else
            emit_tex_expr(f->b, b, 0);
        sbuf_putc(b, '}');
        if (need)
            sbuf_puts(b, "\\right)");
        return;
    }

    if (f->ops->arity == EXPR_OP_BINARY) {
        if (expr_has_polygamma_order(f)) {
            emit_tex_polygamma(f, b);
            return;
        }
        sbuf_puts(b, "\\operatorname{");
        sbuf_puts(b, f->ops->name);
        sbuf_puts(b, "}(");
        emit_tex_expr(f->a, b, 0);
        sbuf_puts(b, ", ");
        emit_tex_expr(f->b, b, 0);
        sbuf_putc(b, ')');
        return;
    }

    emit_tex_atom(f, b);
}

void emit_expr(const expr_t *f, sbuf_t *b, int parent_prec)
{
    if (!f) { sbuf_puts(b, "0"); return; }

    /* Atoms */
    if (expr_is_const(f) || expr_is_var(f)) {
        emit_atom((expr_t *)f, b);
        return;
    }

    /* Negation: -a  — only parenthesise when the child is an add/sub */
    if (expr_is_neg(f)) {
        int need = PREC_UNARY < parent_prec;
        if (need) sbuf_putc(b, '(');

        const expr_t *a = f->a;
        if (expr_is_neg(a)) {
            emit_expr(a->a, b, 0);
            if (need) sbuf_putc(b, ')');
            return;
        }
        if (expr_is_negative(a)) {
            emit_expr_abs(a, b, 0);
            if (need) sbuf_putc(b, ')');
            return;
        }
        int child_needs_paren = expr_is_addsub(a);
        sbuf_putc(b, '-');
        if (child_needs_paren) sbuf_putc(b, '(');
        emit_expr(a, b, 0);
        if (child_needs_paren) sbuf_putc(b, ')');

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Unary ops */
    if (f->ops->arity == EXPR_OP_UNARY) {
        int need = PREC_UNARY < parent_prec;
        if (need) sbuf_putc(b, '(');

        if (expr_is_op(f, &ops_abs)) {
            emit_expr_abs_bars(f->a, b);
            if (need) sbuf_putc(b, ')');
            return;
        }
        if (expr_is_op(f, &ops_floor)) {
            sbuf_puts(b, "⌊");
            emit_expr(f->a, b, 0);
            sbuf_puts(b, "⌋");
            if (need) sbuf_putc(b, ')');
            return;
        }
        if (expr_is_op(f, &ops_ceil)) {
            sbuf_puts(b, "⌈");
            emit_expr(f->a, b, 0);
            sbuf_puts(b, "⌉");
            if (need) sbuf_putc(b, ')');
            return;
        }
        if (expr_is_sqrt_expr(f))
            sbuf_puts(b, "√");
        else
            sbuf_puts(b, expr_unary_name(f));
        sbuf_putc(b, '(');
        emit_expr(f->a, b, 0);
        sbuf_putc(b, ')');

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Power */
    if (expr_is_pow_d_expr(f)) {
        int need = PREC_POW < parent_prec;
        long ei = 0;
        int exponent_has_small_int = expr_try_get_small_integer_exponent(f->c, &ei);

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

        if (need) sbuf_putc(b, '(');

        /* For unary functions raised to a power, write func²(arg)
         * rather than func(arg)² so the exponent binds to the function name.
         * Floor/ceiling keep their mathematical brackets: ⌊x⌋². */
        if (f->a->ops->arity == EXPR_OP_UNARY && !expr_is_neg(f->a)) {
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

            if (need) sbuf_putc(b, ')');
            return;
        }

        {
            int base_needs_parens = pow_base_needs_visible_parens(f->a);

            if (base_needs_parens) sbuf_putc(b, '(');
            emit_expr(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
            if (base_needs_parens) sbuf_putc(b, ')');
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

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Multiplication with sign folding */
    if (expr_is_mul(f)) {
        int need = PREC_MUL < parent_prec;
        expr_t *fac[64];
        int n = 0;

        if (need) sbuf_putc(b, '(');

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

        if (need) sbuf_putc(b, ')');
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

        if (match_add_negative_complex_rhs(f,
                                           &negative_complex_base,
                                           &negative_complex_rhs)) {
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

        if (match_additive_complex_shift(f,
                                         &complex_shift_base,
                                         &complex_shift_real,
                                         &complex_shift_imag)) {
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

        if (need) sbuf_putc(b, '(');

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

        if (need) sbuf_putc(b, ')');
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

        if (match_atan_over_argument_denominator(f, &atan_expr, &denominator) &&
            !expr_is_negative(denominator)) {
            if (need) sbuf_putc(b, '(');
            sbuf_puts(b, "1/");
            emit_expr(denominator, b, PREC_POW);
            sbuf_puts(b, "·");
            emit_expr(atan_expr, b, PREC_MUL);
            if (need) sbuf_putc(b, ')');
            return;
        }

        if (need) sbuf_putc(b, '(');
        if (neg) sbuf_putc(b, '-');

        if (neg_num) emit_expr_abs(f->a, b, PREC_MUL);
        else         emit_expr(f->a, b, PREC_MUL);

        sbuf_putc(b, '/');

        if (neg_den) emit_expr_abs(f->b, b, PREC_POW);
        else         emit_expr(f->b, b, PREC_POW);

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Binary power: base^exp  or  base^(exp) when exponent needs grouping */
    if (expr_is_op(f, &ops_pow)) {
        int need = PREC_POW < parent_prec;
        int base_needs_parens = pow_base_needs_visible_parens(f->a);
        if (need) sbuf_putc(b, '(');

        if (base_needs_parens) sbuf_putc(b, '(');
        emit_expr(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
        if (base_needs_parens) sbuf_putc(b, ')');
        sbuf_putc(b, '^');
        int ep = pow_exp_needs_parens(f->b);
        if (ep) sbuf_putc(b, '(');
        emit_expr(f->b, b, 0);
        if (ep) sbuf_putc(b, ')');

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Named binary functions (e.g. atan2) */
    if (f->ops->arity == EXPR_OP_BINARY) {
        if (expr_has_polygamma_order(f)) {
            emit_expr_polygamma(f, b);
            return;
        }
        sbuf_puts(b, f->ops->name);
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

void emit_func(const expr_t *f, sbuf_t *b, int parent_prec)
{
    if (!f) { sbuf_puts(b, "0"); return; }

    if (expr_is_const(f)) {
        if (expr_tostring_should_emit_binding_expr(f)) {
            char *text = expr_binding_expr_to_function_string(f->binding_expr);

            if (text) {
                sbuf_puts(b, text);
                free(text);
            }
        } else if (f->name && *f->name)
            emit_name_func(b, f->name);
        else {
            char *text = expr_const_to_string_local(f);
            if (text) {
                sbuf_puts(b, text);
                free(text);
            }
        }
        return;
    }

    if (expr_is_var(f)) {
        emit_name_func(b, f->name ? f->name : "x");
        return;
    }

    if (f->ops->arity == EXPR_OP_UNARY) {
        int need = PREC_UNARY < parent_prec;
        if (need) sbuf_putc(b, '(');

        sbuf_puts(b, f->ops->name);
        sbuf_putc(b, '(');
        emit_func(f->a, b, 0);
        sbuf_putc(b, ')');

        if (need) sbuf_putc(b, ')');
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
            sbuf_puts(b, "1 / ");
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

        if (need) sbuf_putc(b, '(');

        if (base_needs_parens) sbuf_putc(b, '(');
        emit_func(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
        if (base_needs_parens) sbuf_putc(b, ')');

        sbuf_putc(b, '^');
        char *text = expr_const_to_string_local(f);
        if (text) {
            sbuf_puts(b, text);
            free(text);
        }

        if (need) sbuf_putc(b, ')');
        return;
    }

    if (expr_is_mul(f)) {
        int need = PREC_MUL < parent_prec;
        bool leading_half_as_divisor = false;
        bool emitted = false;

        if (need) sbuf_putc(b, '(');

        expr_t *fac[64];
        int n = 0;
        flatten_mul((expr_t *)f, fac, &n, 64);
        sort_factors(fac, n);
        leading_half_as_divisor = n > 1 && expr_is_const_half_local(fac[0]);

        for (int i = 0; i < n; i++) {
            if (leading_half_as_divisor && i == 0)
                continue;

            if (emitted)
                sbuf_puts(b, " * ");
            if (n > 1 && mul_factor_needs_visible_parens(fac[i]))
                sbuf_putc(b, '(');
            emit_func(fac[i], b, PREC_MUL);
            if (n > 1 && mul_factor_needs_visible_parens(fac[i]))
                sbuf_putc(b, ')');
            emitted = true;
        }

        if (leading_half_as_divisor)
            sbuf_puts(b, " / 2");

        if (need) sbuf_putc(b, ')');
        return;
    }

    if (expr_is_addsub(f)) {
        int need = PREC_ADD < parent_prec;
        int neg;

        if (need) sbuf_putc(b, '(');

        emit_func(f->a, b, PREC_ADD);
        neg = expr_renders_negative(f->b);

        if (expr_is_op(f, &ops_add))
            sbuf_puts(b, neg ? " - " : " + ");
        else
            sbuf_puts(b, neg ? " + " : " - ");

        int rhs_parens = add_rhs_needs_visible_parens(f->b);
        if (rhs_parens)
            sbuf_putc(b, '(');
        if (neg)
            emit_func_abs(f->b, b, PREC_ADD);
        else
            emit_func(f->b, b, PREC_ADD);
        if (rhs_parens)
            sbuf_putc(b, ')');

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Division: a/b */
    if (expr_is_op(f, &ops_div)) {
        int need = PREC_MUL < parent_prec;
        if (need) sbuf_putc(b, '(');

        emit_func(f->a, b, PREC_MUL);
        sbuf_puts(b, " / ");
        emit_func(f->b, b, PREC_POW);

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Binary power: base^exp  or  base^(exp) when exponent needs grouping */
    if (expr_is_op(f, &ops_pow)) {
        int need = PREC_POW < parent_prec;
        int base_needs_parens = pow_base_needs_visible_parens(f->a);
        if (need) sbuf_putc(b, '(');

        if (base_needs_parens) sbuf_putc(b, '(');
        emit_func(f->a, b, base_needs_parens ? PREC_LOWEST : PREC_POW);
        if (base_needs_parens) sbuf_putc(b, ')');
        sbuf_puts(b, " ^ ");
        int ep = pow_exp_needs_parens(f->b);
        if (ep) sbuf_putc(b, '(');
        emit_func(f->b, b, 0);
        if (ep) sbuf_putc(b, ')');

        if (need) sbuf_putc(b, ')');
        return;
    }

    /* Named binary functions (e.g. atan2) */
    if (f->ops->arity == EXPR_OP_BINARY) {
        if (expr_has_polygamma_order(f)) {
            emit_func_polygamma(f, b);
            return;
        }
        sbuf_puts(b, f->ops->name);
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

static string_t *expr_to_tex_text(const expr_t *dv)
{
    char *expr = NULL;
    char *bindings = NULL;
    sbuf_t b;
    string_t *out;

    if (expr_to_tex_parts(dv, &expr, &bindings) != 0)
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
    return rune_is_equal(rune, '\n') ||
           rune_is_equal(rune, '\r') ||
           rune_is_equal(rune, ' ') ||
           rune_is_equal(rune, '\t');
}

static string_t *expr_trim_trailing_display_space(string_t *text)
{
    size_t len;
    string_t *trimmed;

    if (!text)
        return NULL;

    len = string_length(text);
    while (len > 0u &&
           expr_is_trailing_display_space(string_at(text, len - 1u)))
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

    if (style == style_TEX) {
        text = expr_to_tex_text(dv);
    } else if (style == style_FUNCTION) {
        text = expr_to_text_function(dv);
    } else if (style == style_UNBOUND) {
        text = expr_to_text_unbound(dv);
    } else {
        text = expr_to_text_expr(dv);
    }

    return expr_trim_trailing_display_space(text);
}

void expr_print(const expr_t *dv)
{
    string_t *s = expr_to_text(dv, style_EXPRESSION);

    fputs(s ? string_c_str(s) : "NULL", stdout);
    fputc('\n', stdout);
    string_free(s);
}
