#include "expr_fourier_internal.h"

/* A compact hyperbolic spectrum has at most two beta factors. Stop at other operators: this is
 * a bounded structural pattern match, not a search through arbitrary function arguments. */
static bool beta_factors(const expr_t *f, const expr_t **nodes, size_t *count)
{
    if (f->ops == &ops_beta) {
        if (*count == 2u)
            return false;
        nodes[(*count)++] = f;
        return true;
    }
    if (f->ops == &ops_add || f->ops == &ops_sub || f->ops == &ops_mul || f->ops == &ops_div)
        return beta_factors(f->a, nodes, count) && beta_factors(f->b, nodes, count);
    return f->ops != &ops_neg || beta_factors(f->a, nodes, count);
}

static bool same_formula(fourier_context_t *c, const expr_t *left, const expr_t *right)
{
    const expr_t *left_exponent = exponent(left), *right_exponent = exponent(right);
    if (left_exponent && right_exponent)
        return same_formula(c, left_exponent, right_exponent);
    number_t left_value = NUM_NAN, right_value = NUM_NAN;
    bool equal_constants = literal_value(left, &left_value) && literal_value(right, &right_value) &&
                           num_eq(left_value, right_value);
    num_destroy(&right_value);
    num_destroy(&left_value);
    if (equal_constants)
        return true;
    expr_t *difference = clean(c, ft_sub(c, clean(c, left), clean(c, right)));
    if (expr_const_is_zero(difference))
        return true;
    difference = keep(c, expr_expand_preserved_for_display(difference));
    difference = clean(c, keep(c, expr_expand_products_internal(difference)));
    return expr_const_is_zero(difference);
}

static expr_t *beta_coefficient(const expr_t *f, const expr_t *unit, const expr_t *zero)
{
    if (f == unit)
        return expr_const_one();
    if (f == zero)
        return expr_const_zero();
    if (f->ops == &ops_neg)
        return expr_new_unary_internal(f->ops, beta_coefficient(f->a, unit, zero));
    if (f->ops == &ops_add || f->ops == &ops_sub || f->ops == &ops_mul || f->ops == &ops_div)
        return expr_new_binary_internal(f->ops, beta_coefficient(f->a, unit, zero),
                                                beta_coefficient(f->b, unit, zero));
    return expr_clone(f);
}

/* Invert actual beta spectra, including copied/serialised expressions, without relying on a nested
 * Fourier node. Verify the entire spectrum before accepting a candidate or adding its conditions. */
expr_t *expr_fourier_beta_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w)
{
    const expr_t *nodes[2] = {NULL, NULL};
    size_t count = 0u;
    if (!beta_factors(f, nodes, &count) || count == 0u)
        return NULL;
    expr_t *one = integer(c, 1), *two = integer(c, 2), *i = constant(c, NUM_I);
    expr_t *pi = pi_constant(c), *two_pi = ft_mul(c, two, pi);
    for (size_t index = 0u; index < count; ++index) {
        const expr_t *beta = nodes[index];
        bool singular = !uses(beta->a, x) || !uses(beta->b, x);
        const expr_t *argument = uses(beta->a, x) ? beta->a : beta->b;
        const expr_t *other = argument == beta->a ? beta->b : beta->a;
        expr_t *rate = NULL, *offset = NULL;
        if (!affine(c, argument, x, &rate, &offset) || expr_const_is_zero(rate))
            continue;
        expr_t *n = clean(c, singular ? ft_sub(c, other, one) : ft_neg(c, ft_mul(c, two, offset)));
        if (uses(n, x) || !same_formula(c, offset, ft_div(c, ft_neg(c, n), two)))
            continue;
        for (unsigned direction = 0u; direction < 2u; ++direction) {
            expr_t *real_rate = clean(c, ft_mul(c, ft_neg(c, i), rate));
            expr_t *a = clean(c, ft_div(c, direction ? ft_neg(c, one) : one, ft_mul(c, two, real_rate)));
            expr_t *q = ft_div(c, ft_mul(c, i, x), a);
            expr_t *left = ft_div(c, ft_sub(c, q, n), two);
            expr_t *right = ft_div(c, ft_sub(c, ft_neg(c, q), n), two);
            expr_t *second = ft_add(c, n, one);
            const expr_t *left_beta = NULL, *right_beta = NULL;
            for (size_t k = 0u; k < count; ++k) {
                for (unsigned swap = 0u; swap < 2u; ++swap) {
                    const expr_t *first_arg = swap ? nodes[k]->b : nodes[k]->a;
                    const expr_t *second_arg = swap ? nodes[k]->a : nodes[k]->b;
                    if (same_formula(c, first_arg, left) &&
                        same_formula(c, second_arg, singular ? second : right))
                        left_beta = nodes[k];
                    if (singular && same_formula(c, first_arg, right) && same_formula(c, second_arg, second))
                        right_beta = nodes[k];
                }
            }
            if (!left_beta || (singular && (!right_beta || left_beta == right_beta)))
                continue;
            expr_t *left_coefficient = clean(c, keep(c, beta_coefficient(f, left_beta, right_beta)));
            expr_t *right_coefficient = singular ? clean(c, keep(c, beta_coefficient(f, right_beta, left_beta))) : one;
            const expr_t *reconstructed = singular
                                             ? ft_add(c, ft_mul(c, left_coefficient, left_beta),
                                                         ft_mul(c, right_coefficient, right_beta))
                                             : ft_mul(c, left_coefficient, left_beta);
            if (!same_formula(c, left_coefficient, one) || !same_formula(c, f, reconstructed))
                continue;
            /* Principal sinh, absolute sinh, and reciprocal sinh have distinct negative-half-line phases. */
            for (unsigned branch = 0u; branch < (singular ? 3u : 1u); ++branch) {
                expr_t *phase = branch == 1u ? one : ft_exp(c, ft_mul(c, ft_mul(c, i, pi),
                                                                           branch == 2u ? ft_neg(c, n) : n));
                if (singular && !same_formula(c, right_coefficient, phase))
                    continue;
                if (!real_parameter(c, a) || !positive(c, ft_abs(c, a)) ||
                    !positive(c, ft_neg(c, n)) || (singular && !positive(c, second)))
                    return NULL;
                expr_t *coordinate = clean(c, ft_mul(c, a, c->inverse ? w : ft_neg(c, w)));
                coordinate = clean(c, keep(c, expr_expand_products_internal(coordinate)));
                expr_t *base = singular ? ft_sinh(c, coordinate) : ft_cosh(c, coordinate);
                if (singular && branch == 1u)
                    base = ft_abs(c, base);
                if (singular && branch == 2u)
                    base = ft_div(c, one, base);
                expr_t *body = ft_pow_xp(c, base, branch == 2u ? ft_neg(c, n) : n);
                expr_t *scale = ft_div(c, ft_abs(c, a), ft_pow_xp(c, two, ft_neg(c, second)));
                return ft_mul(c, two_pi, ft_mul(c, scale, body));
            }
        }
    }
    return NULL;
}
