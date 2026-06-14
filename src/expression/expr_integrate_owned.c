#include "expr_integrate_internal.h"

expr_t *simplify_owned(expr_t *expr)
{
    return expr_simplify_owned(expr);
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

expr_t *div_number_owned_by_product(expr_t *expr,
                                    number_t left,
                                    number_t right)
{
    number_t denom = num_mul(left, right);

    return div_number_owned_consuming(expr, &denom);
}

expr_t *div_number_owned_by_long_product(expr_t *expr,
                                         long left,
                                         number_t right)
{
    number_t factor = num_create_from_long(left);
    expr_t *out = div_number_owned_by_product(expr, factor, right);

    num_destroy(&factor);
    return out;
}
