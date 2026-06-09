#include "expr_integrate_internal.h"

expr_t *simplify_owned(expr_t *expr)
{
    expr_t *simplified;

    if (!expr)
        return NULL;
    simplified = expr_simplify(expr);
    expr_free(expr);
    return simplified;
}

expr_t *expr_integrate_clone_expr(const expr_t *expr)
{
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *out = NULL;

    if (!expr)
        return NULL;

    if (expr->ops->kind == EXPR_KIND_CONST) {
        if (expr->name && *expr->name)
            return expr_new_named_const(expr->c, expr->name);
        return expr_new_const(expr->c);
    }

    if (expr->ops->kind == EXPR_KIND_VAR) {
        if (expr->name && *expr->name)
            return expr_new_named_var(expr->x, expr->name);
        return expr_new_var(expr->x);
    }

    if (expr->ops->kind == EXPR_KIND_POW_D && expr->a) {
        left = expr_integrate_clone_expr(expr->a);
        if (!left)
            return NULL;
        out = expr_pow(left, &expr->c);
        expr_free(left);
        return out;
    }

    if (expr->ops->arity == EXPR_OP_UNARY && expr->ops->apply_unary) {
        left = expr_integrate_clone_expr(expr->a);
        if (!left)
            return NULL;
        out = expr->ops->apply_unary(left);
        expr_free(left);
        return out;
    }

    if (expr->ops->arity == EXPR_OP_BINARY && expr->ops->apply_binary) {
        left = expr_integrate_clone_expr(expr->a);
        right = expr_integrate_clone_expr(expr->b);
        if (!left || !right) {
            expr_free(left);
            expr_free(right);
            return NULL;
        }
        out = expr->ops->apply_binary(left, right);
        expr_free(left);
        expr_free(right);
        return out;
    }

    return NULL;
}

expr_t *expr_integrate_retain_expr(const expr_t *expr)
{
    if (!expr)
        return NULL;
    expr_retain(expr);
    return (expr_t *)expr;
}

expr_t *expr_integrate_negate_owned(expr_t *expr)
{
    expr_t *negated;

    if (!expr)
        return NULL;
    negated = expr_neg(expr);
    expr_free(expr);
    return simplify_owned(negated);
}

expr_t *expr_integrate_add_terms_owned(expr_t *left, expr_t *right)
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

expr_t *mul_number_owned(expr_t *expr, number_t factor)
{
    expr_t *scaled;

    if (!expr)
        return NULL;
    scaled = expr_mul_num(expr, &factor);
    expr_free(expr);
    return simplify_owned(scaled);
}

expr_t *mul_number_owned_consuming(expr_t *expr, number_t *factor)
{
    expr_t *out;

    if (!factor)
        return NULL;
    out = mul_number_owned(expr, *factor);
    num_destroy(factor);
    return out;
}

expr_t *div_number_owned(expr_t *expr, number_t denom)
{
    expr_t *scaled;

    if (!expr)
        return NULL;
    if (num_eq(denom, NUM_ZERO)) {
        expr_free(expr);
        return NULL;
    }
    if (num_eq(denom, NUM_ONE))
        return simplify_owned(expr);
    if (num_eq(denom, NUM_NEG_ONE)) {
        scaled = expr_neg(expr);
        expr_free(expr);
        return simplify_owned(scaled);
    }
    {
        number_t reciprocal = num_div(NUM_ONE, denom);

        return mul_number_owned_consuming(expr, &reciprocal);
    }
}

expr_t *div_number_owned_consuming(expr_t *expr, number_t *denom)
{
    expr_t *out;

    if (!denom)
        return NULL;
    out = div_number_owned(expr, *denom);
    num_destroy(denom);
    return out;
}

expr_t *expr_integrate_div_number_owned_by_product(expr_t *expr,
                                                   number_t left,
                                                   number_t right)
{
    number_t denom = num_mul(left, right);

    return div_number_owned_consuming(expr, &denom);
}

expr_t *expr_integrate_div_number_owned_by_long_product(expr_t *expr,
                                                        long left,
                                                        number_t right)
{
    number_t factor = num_create_from_long(left);
    expr_t *out = expr_integrate_div_number_owned_by_product(expr, factor, right);

    num_destroy(&factor);
    return out;
}
