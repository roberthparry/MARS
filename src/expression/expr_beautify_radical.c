/**
 * @file expr_beautify_radical.c
 * @brief Surd factor normalisation for display.
 *
 * Combines scales and radicands and normalises root factors while preserving their mathematical meaning. This unit
 * improves exact surd presentation after simplification rather than approximating roots numerically.
 *
 * This is part of the expression.h implementation. Preserve expression ownership, symbol identity and mathematical
 * domain restrictions when extending these operations.
 */

#include <stdbool.h>

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"

/* Do not treat a named constant or a variable's current binding as a rational coefficient. */
static bool rational_literal(const expr_t *expr, long *numerator, long *denominator)
{
    return expr_is_unnamed_const(expr) && num_is_exact(expr->c) &&
           (!expr->binding_expr || expr_binding_expr_is_numeric_literal(expr->binding_expr)) &&
           num_get_small_rational(expr->c, numerator, denominator);
}

/* Borrow the non-rational factor; NULL denotes an implicit factor of one. */
static bool radical_scale(const expr_t *arg, long *numerator, long *denominator, const expr_t **rest)
{
    long n;
    long d;

    *rest = NULL;
    if (rational_literal(arg, &n, &d)) {
        /* The entire radicand is rational. */
    } else if (expr_is_op(arg, &ops_mul) && rational_literal(arg->a, &n, &d)) {
        *rest = arg->b;
    } else if (expr_is_op(arg, &ops_mul) && rational_literal(arg->b, &n, &d)) {
        *rest = arg->a;
    } else if (expr_is_op(arg, &ops_div) && rational_literal(arg->b, &d, &n)) {
        *rest = arg->a;
    } else {
        return false;
    }
    if (n <= 0L || d <= 0L)
        return false;
    *numerator = n;
    *denominator = d;
    return true;
}

static expr_t *scaled_radicand(const expr_t *rest, long numerator, long denominator)
{
    expr_t *factor = expr_const_long(numerator);
    expr_t *top = rest ? (numerator == 1L ? expr_clone(rest) : expr_mul(factor, rest)) : expr_clone(factor);
    expr_t *bottom = denominator == 1L ? NULL : expr_const_long(denominator);
    expr_t *out = denominator == 1L ? expr_clone(top) : (bottom && top ? expr_div(top, bottom) : NULL);

    expr_free(bottom);
    expr_free(top);
    expr_free(factor);
    return out;
}

static expr_t *normalise_root(const expr_t *expr)
{
    const expr_t *rest;
    long numerator;
    long denominator;
    expr_t *arg;
    expr_t *out;

    if (!expr_is_sqrt_expr(expr) || !radical_scale(expr->a, &numerator, &denominator, &rest))
        return NULL;
    if (!rest && numerator == 1L && denominator > 1L) {
        expr_t *one = expr_const_long(1L);
        expr_t *integer = expr_const_long(denominator);
        expr_t *root = integer ? expr_sqrt(integer) : NULL;

        out = one && root ? expr_div(one, root) : NULL;
        expr_free(root);
        expr_free(integer);
        expr_free(one);
        return out;
    }
    if (!rest || !expr_is_op(expr->a, &ops_div))
        return NULL;
    arg = scaled_radicand(rest, numerator, denominator);
    out = arg && !expr_struct_eq(arg, expr->a) ? expr_sqrt(arg) : NULL;
    expr_free(arg);
    return out;
}

/* A rational scale may cross a principal root only through its positive square; retain its sign outside. */
static expr_t *absorb_scale(const expr_t *expr, number_t coefficient)
{
    const expr_t *rest;
    long old_n;
    long old_d;
    long new_n;
    long new_d;
    expr_t *out = NULL;

    if (expr_is_op(expr, &ops_mul)) {
        expr_t *child = absorb_scale(expr->a, coefficient);

        if (child) {
            out = expr_mul(child, expr->b);
        } else {
            child = absorb_scale(expr->b, coefficient);
            out = child ? expr_mul(expr->a, child) : NULL;
        }
        expr_free(child);
        return out;
    }
    if (!expr_is_sqrt_expr(expr) || !radical_scale(expr->a, &old_n, &old_d, &rest))
        return NULL;

    number_t original = num_create_from_frac(old_n, old_d);
    number_t square = num_mul(coefficient, coefficient);
    number_t combined = num_mul(original, square);

    /* Remove a factor only when it does not enlarge the rational numbers inside the root. */
    if (num_get_small_rational(combined, &new_n, &new_d) && new_n > 0L && new_d > 0L &&
        (new_n > new_d ? new_n : new_d) <= (old_n > old_d ? old_n : old_d)) {
        expr_t *arg = scaled_radicand(rest, new_n, new_d);
        expr_t *root = arg ? expr_sqrt(arg) : NULL;

        out = root && num_sign(coefficient) < 0 ? expr_neg(root) : expr_clone(root);
        expr_free(root);
        expr_free(arg);
    }
    num_destroy(&combined);
    num_destroy(&square);
    num_destroy(&original);
    return out;
}

/* Arrange exact rational radical factors once, independently of the output style. */
expr_t *expr_beautify_radical_factors_for_display(const expr_t *expr)
{
    const expr_t *rest = NULL;
    long numerator;
    long denominator;
    expr_t *out = normalise_root(expr);

    if (out)
        return out;
    if (expr_is_op(expr, &ops_div) && rational_literal(expr->b, &numerator, &denominator) && numerator != 0L) {
        number_t coefficient = num_create_from_frac(denominator, numerator);

        out = absorb_scale(expr->a, coefficient);
        num_destroy(&coefficient);
        return out;
    }
    if (!expr_is_op(expr, &ops_mul))
        return NULL;
    const expr_t *reciprocal = expr->b;
    const expr_t *other = expr->a;

    if (!expr_is_op(reciprocal, &ops_div)) {
        reciprocal = expr->a;
        other = expr->b;
    }
    if (expr_is_op(reciprocal, &ops_div) && rational_literal(reciprocal->a, &numerator, &denominator) &&
        numerator == 1L && denominator == 1L && expr_is_sqrt_expr(reciprocal->b) &&
        rational_literal(reciprocal->b->a, &numerator, &denominator) && numerator > 0L && denominator > 0L &&
        !expr_is_op(other, &ops_add) && !expr_is_op(other, &ops_sub) &&
        !expr_contains_calculus_request(other))
        return expr_div(other, reciprocal->b);
    if (rational_literal(expr->a, &numerator, &denominator))
        rest = expr->b;
    else if (rational_literal(expr->b, &numerator, &denominator))
        rest = expr->a;
    if (!rest || numerator == 0L || denominator <= 0L || numerator == denominator || numerator == -denominator)
        return NULL;

    number_t coefficient = num_create_from_frac(numerator, denominator);
    out = absorb_scale(rest, coefficient);
    num_destroy(&coefficient);
    return out;
}
