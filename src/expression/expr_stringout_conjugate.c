/**
 * @file expr_stringout_conjugate.c
 * @brief Equal-weight conjugate sums in native mathematical presentation.
 *
 * Groups complete additive expressions by structural operand using a hash table. Only equal total coefficients
 * of an operand and its conjugate become a real-part projection. Unequal totals remain separate, avoiding a
 * partial rewrite which would mix conjugation and real-part notation for the same operand. Original expression
 * trees and exact complex radicals are retained; projection nodes here are unevaluated display views.
 */

#include <stdint.h>

#include "dictionary-t.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_stringout.h"
#define MARS_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "expr_stringout_internal.h"

typedef struct {
    const expr_t *base;
    number_t coefficients[2];
} expr_conjugate_group_t;

static bool expr_conjugate_sum_contains(const expr_t *expr)
{
    if (!expr)
        return false;
    if (expr_is_op(expr, &ops_conj))
        return true;
    return (expr_is_addsub(expr) || expr_is_neg(expr) || expr_is_mul(expr) || expr_is_div(expr)) &&
           (expr_conjugate_sum_contains(expr->a) || expr_conjugate_sum_contains(expr->b));
}

static size_t expr_conjugate_hash_node(const expr_t *expr);

static size_t expr_conjugate_hash_product(const expr_t *expr)
{
    if (expr_is_mul(expr))
        return expr_conjugate_hash_product(expr->a) * expr_conjugate_hash_product(expr->b);
    if (expr_is_neg(expr))
        return expr_conjugate_hash_product(expr->a);
    /* Structural product equality combines scalar factors, irrespective of their association. */
    return expr_is_const(expr) ? 1u : expr_conjugate_hash_node(expr);
}

static size_t expr_conjugate_hash_node(const expr_t *expr)
{
    if (!expr)
        return 1u;
    if (expr_is_var(expr))
        return (size_t)expr->var_id * 2654435761u + 3u;
    /* Numeric backends compare by value, not representation; constants deliberately share a hash bucket. */
    if (expr_is_const(expr))
        return 7u;
    if (expr_is_mul(expr))
        return expr_conjugate_hash_product(expr);
    bool square_root = expr_is_sqrt_expr(expr) || (expr_is_pow_d_expr(expr) && num_eq(expr->c, NUM_HALF));
    size_t kind = square_root ? EXPR_KIND_SQRT : expr->ops->kind;
    return (kind + 11u) * 2654435761u + 31u * expr_conjugate_hash_node(expr->a) +
           (square_root ? 0u : 37u * expr_conjugate_hash_node(expr->b));
}

static size_t expr_conjugate_hash(const void *key)
{
    return expr_conjugate_hash_node(*(const expr_t *const *)key);
}

static int expr_conjugate_compare(const void *left, const void *right)
{
    const expr_t *a = *(const expr_t *const *)left, *b = *(const expr_t *const *)right;
    if (expr_struct_eq(a, b))
        return 0;
    return (uintptr_t)a < (uintptr_t)b ? -1 : 1;
}

static void expr_conjugate_destroy(void *value)
{
    expr_conjugate_group_t *group = value;
    num_destroy(&group->coefficients[0]);
    num_destroy(&group->coefficients[1]);
}

static bool expr_conjugate_collect(const expr_t *expr, number_t scale, dictionary_t *groups, bool *has_conjugate)
{
    if (expr_is_addsub(expr)) {
        number_t right_scale = expr_is_op(expr, &ops_sub) ? num_neg(scale) : num_clone(scale);
        bool ok = expr_conjugate_collect(expr->a, scale, groups, has_conjugate) &&
                  expr_conjugate_collect(expr->b, right_scale, groups, has_conjugate);
        num_destroy(&right_scale);
        return ok;
    }
    const expr_t *base = expr;
    number_t factor = num_clone(NUM_ONE);
    (void)expr_match_scaled_expr(expr, &factor, &base);
    number_t coefficient = num_mul(scale, factor);
    num_destroy(&factor);
    if (base != expr && expr_is_addsub(base)) {
        bool ok = expr_conjugate_collect(base, coefficient, groups, has_conjugate);
        num_destroy(&coefficient);
        return ok;
    }
    size_t side = expr_is_op(base, &ops_conj) ? 1u : 0u;
    if (side) {
        base = base->a;
        *has_conjugate = true;
    }
    expr_conjugate_group_t group;
    bool present = dictionary_get(groups, &base, &group);
    if (!present)
        group = (expr_conjugate_group_t){base, {num_clone(NUM_ZERO), num_clone(NUM_ZERO)}};
    else {
        group.coefficients[0] = num_clone(group.coefficients[0]);
        group.coefficients[1] = num_clone(group.coefficients[1]);
    }
    number_t sum = num_add(group.coefficients[side], coefficient);
    num_destroy(&coefficient);
    num_destroy(&group.coefficients[side]);
    group.coefficients[side] = sum;
    bool ok = dictionary_set(groups, &base, &group);
    if (!ok)
        expr_conjugate_destroy(&group);
    return ok;
}

static void expr_conjugate_emit_term(const expr_t *base, number_t coefficient, bool conjugated, bool projected,
                                     sbuf_t *buffer, style_t style, bool *first)
{
    bool negative = num_is_real(coefficient) && num_lt(coefficient, NUM_ZERO);
    number_t magnitude = negative ? num_neg(coefficient) : num_clone(coefficient);
    expr_t *argument = expr_clone(base);
    expr_t *value = conjugated ? expr_new_unary_internal(&ops_conj, argument) :
                    projected ? expr_new_unary_internal(&ops_real_bound, argument) : argument;
    expr_t *term = num_is_one(magnitude) ? value :
                   expr_new_binary_internal(&ops_mul, expr_new_const(magnitude), value);
    bool term_negative = expr_renders_negative(term);
    negative = negative != term_negative;
    sbuf_puts(buffer, *first ? (negative ? "-" : "") : negative ? " - " : " + ");
    if (style == style_LATEX)
        (term_negative ? emit_TeX_expr_abs : emit_TeX_expr)(term, buffer, PREC_MUL);
    else if (style == style_FUNCTION)
        (term_negative ? emit_func_abs : emit_func)(term, buffer, PREC_MUL);
    else
        (term_negative ? emit_expr_abs : emit_expr)(term, buffer, PREC_MUL);
    *first = false;
    expr_free(term);
    num_destroy(&magnitude);
}

static bool expr_conjugate_common_factor(const expr_t *expr, sbuf_t *buffer, int parent_prec, style_t style)
{
    if (!expr_is_op(expr, &ops_add) || !expr_is_mul(expr->a) || !expr_is_mul(expr->b))
        return false;
    const expr_t *left[2] = {expr->a->a, expr->a->b}, *right[2] = {expr->b->a, expr->b->b};
    /* Exactly four factor placements; keep even a complex shared factor outside the real-part projection. */
    for (size_t i = 0u; i < 2u; ++i) {
        for (size_t j = 0u; j < 2u; ++j) {
            const expr_t *a = left[1u - i], *b = right[1u - j];
            const expr_t *argument = expr_is_op(a, &ops_conj) && expr_struct_eq(a->a, b) ? b :
                                     expr_is_op(b, &ops_conj) && expr_struct_eq(b->a, a) ? a : NULL;
            if (!argument || !expr_struct_eq(left[i], right[j]))
                continue;
            expr_t *projection = expr_new_unary_internal(&ops_real_bound, expr_clone(argument));
            expr_t *twice_factor = expr_simplify_owned(
                expr_new_binary_internal(&ops_mul, expr_new_const(NUM_TWO), expr_clone(left[i])));
            expr_t *result = expr_new_binary_internal(&ops_mul, twice_factor, projection);
            if (style == style_LATEX)
                emit_TeX_expr(result, buffer, parent_prec);
            else if (style == style_FUNCTION)
                emit_func(result, buffer, parent_prec);
            else
                emit_expr(result, buffer, parent_prec);
            expr_free(result);
            return true;
        }
    }
    return false;
}

/* Consume the complete sum, including unequal groups, so nested pairs cannot be rewritten in isolation. */
bool expr_emit_conjugate_sum(const expr_t *expr, sbuf_t *buffer, int parent_prec, style_t style)
{
    if (!expr_is_addsub(expr) || !expr_conjugate_sum_contains(expr))
        return false;
    if (expr_conjugate_common_factor(expr, buffer, parent_prec, style))
        return true;
    dictionary_t *groups = dictionary_create(sizeof(const expr_t *), sizeof(expr_conjugate_group_t),
                                              expr_conjugate_hash, expr_conjugate_compare, NULL, NULL,
                                              NULL, NULL, expr_conjugate_destroy);
    bool has_conjugate = false;
    bool ok = groups && expr_conjugate_collect(expr, NUM_ONE, groups, &has_conjugate);
    if (!ok || !has_conjugate) {
        dictionary_destroy(groups);
        return false;
    }
    bool bracketed = parent_prec > PREC_ADD, first = true;
    if (bracketed)
        sbuf_puts(buffer, style == style_LATEX ? "\\left(" : "(");
    for (size_t i = 0u; i < dictionary_size(groups); ++i) {
        const expr_conjugate_group_t *group = dictionary_get_value(groups, i);
        if (num_eq(group->coefficients[0], group->coefficients[1]) && !num_is_zero(group->coefficients[0])) {
            number_t doubled = num_add(group->coefficients[0], group->coefficients[0]);
            expr_conjugate_emit_term(group->base, doubled, false, true, buffer, style, &first);
            num_destroy(&doubled);
        } else {
            for (size_t side = 0u; side < 2u; ++side)
                if (!num_is_zero(group->coefficients[side]))
                    expr_conjugate_emit_term(group->base, group->coefficients[side], side != 0u, false,
                                             buffer, style, &first);
        }
    }
    if (first)
        sbuf_putc(buffer, '0');
    if (bracketed)
        sbuf_puts(buffer, style == style_LATEX ? "\\right)" : ")");
    dictionary_destroy(groups);
    return true;
}
