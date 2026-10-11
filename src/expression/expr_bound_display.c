/**
 * @file expr_bound_display.c
 * @brief Binding-specific signs and domain checks for native calculus presentation.
 *
 * Builds a disposable display snapshot: supplied real parameter signs can remove
 * principal square roots of their squares, while satisfied parameter conditions
 * can be omitted. Parameter names remain symbolic and the active calculus variable
 * remains free. Callers must rebuild this snapshot whenever bindings change; it is
 * not an unconditional algebraic identity or a replacement for the generic result.
 * Binder bodies and unresolved calculus requests are not specialised here.
 */
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

static bool expr_bound_display_scoped(const expr_t *expr)
{
    return expr_is_op(expr, &ops_integral) || expr_is_op(expr, &ops_summation) ||
           expr_is_op(expr, &ops_product) || expr_is_formal_derivative(expr) ||
           expr_is_integral_transform(expr) || expr_is_arbitrary_function(expr);
}

/* Only complete parameter expressions may supply sign or condition evidence. */
static bool expr_bound_display_parameter(const expr_t *expr, const expr_t *wrt)
{
    if (!expr)
        return false;
    if (expr_is_var(expr))
        return wrt && expr_is_var(wrt) && expr->var_id != wrt->var_id && num_is_finite(expr->x);
    if (expr_is_const(expr))
        return num_is_finite(expr->c);
    if (expr_bound_display_scoped(expr) || expr_is_op(expr, &ops_real_domain))
        return false;
    return expr_bound_display_parameter(expr->a, wrt) &&
           (expr_is_pow_d_expr(expr) || expr->ops->arity != EXPR_OP_BINARY ||
            expr_bound_display_parameter(expr->b, wrt));
}

/* Extract one real, non-zero parameter square from a product under a principal root. */
static bool expr_bound_display_square(const expr_t *expr, const expr_t *wrt,
                                      expr_t **outside, expr_t **remaining)
{
    const expr_t *base = NULL;
    number_t exponent = num_new();
    bool square = expr_match_pow_const(expr, &base, &exponent) && num_eq(exponent, NUM_TWO);
    num_destroy(&exponent);
    if (square && expr_bound_display_parameter(base, wrt)) {
        number_t value = expr_eval(base);
        bool real = num_is_real(value) && num_is_finite(value) && !num_eq(value, NUM_ZERO);
        if (real) {
            *outside = num_lt(value, NUM_ZERO) ? expr_neg(base) : expr_clone(base);
            *remaining = expr_new_const(NUM_ONE);
        }
        num_destroy(&value);
        if (real)
            return true;
    }
    if (!expr_is_op(expr, &ops_mul))
        return false;
    expr_t *rest = NULL;
    if (expr_bound_display_square(expr->a, wrt, outside, &rest))
        *remaining = expr_mul(rest, expr->b);
    else if (expr_bound_display_square(expr->b, wrt, outside, &rest))
        *remaining = expr_mul(expr->a, rest);
    else
        return false;
    expr_free(rest);
    return true;
}

/* Mutate only the fresh snapshot; never detach symbols or restrictions in its source. */
static void expr_bound_display_rewrite(expr_t **slot, const expr_t *wrt)
{
    expr_t *expr = *slot;
    if (!expr || expr->ops->arity == EXPR_OP_ATOM || expr_bound_display_scoped(expr))
        return;
    if (expr_is_op(expr, &ops_real_domain)) {
        expr_t **link = &expr->b;
        while (*link) {
            expr_t *pair = *link;
            if (!expr_bound_display_parameter(pair->a, wrt) ||
                !expr_bound_display_parameter(pair->b->a, wrt)) {
                link = &pair->b->b;
                continue;
            }
            number_t value = expr_eval(pair->a);
            number_t limit = expr_eval(pair->b->a);
            number_t real = num_real_part(value);
            bool valid = num_is_finite(value) && num_is_finite(limit) && num_is_real(limit) && num_gt(real, limit);
            num_destroy(&real);
            num_destroy(&limit);
            num_destroy(&value);
            if (!valid) {
                *slot = expr_new_const(NUM_NAN);
                expr_free(expr);
                return;
            }
            *link = pair->b->b;
            pair->b->b = NULL;
            expr_free(pair);
        }
        if (!expr->b) {
            *slot = expr->a;
            expr->a = NULL;
            expr_free(expr);
            expr_bound_display_rewrite(slot, wrt);
            return;
        }
        expr_bound_display_rewrite(&expr->a, wrt);
        expr->simplified = false;
        return;
    }
    expr_bound_display_rewrite(&expr->a, wrt);
    expr_bound_display_rewrite(&expr->b, wrt);
    expr->simplified = false;
    if (expr_is_sqrt_expr(expr)) {
        expr_t *outside = NULL, *remaining = NULL;
        if (expr_bound_display_square(expr->a, wrt, &outside, &remaining)) {
            expr_t *root = expr_sqrt(remaining);
            expr_bound_display_rewrite(&root, wrt);
            *slot = expr_mul(outside, root);
            expr_free(root);
            expr_free(remaining);
            expr_free(outside);
            expr_free(expr);
        }
    }
}

/* Create a binding-specific rendering without substituting parameter values into its formula. */
expr_t *expr_specialise_bound_display(const expr_t *expr, const expr_t *wrt)
{
    expr_t *copy = expr_clone(expr);
    expr_bound_display_rewrite(&copy, wrt);
    expr_t *out = copy ? expr_beautify(copy) : NULL;
    expr_free(copy);
    return out;
}
