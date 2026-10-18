/**
 * @file expr_fourier_zeta.c
 * @brief Vertical-line zeta Fourier spectra and logarithmic impulse-series inverses.
 *
 * Uses the absolutely convergent Dirichlet series for Re(sigma) > 1. The spectrum
 * is a distribution, not a pointwise function. Real-axis zeta requests and other
 * convergence strips are not reinterpreted as this pair.
 */
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_fourier_internal.h"

static expr_t *zeta_index(fourier_context_t *c, const expr_t *f, const expr_t *w)
{
    expr_bindings_t *source = expr_bindings_from_expr_internal(f);
    expr_bindings_t *target = expr_bindings_from_expr_internal(w);
    bool available = !expr_bindings_get(source, "n") && !expr_bindings_get(target, "n");
    expr_bindings_free(source);
    expr_bindings_free(target);
    return available ? expr_fourier_keep(c, expr_new_named_var(NUM_NAN, "n")) :
                       expr_fourier_fresh_variable(c, f, w);
}

static const expr_t *zeta_impulse(const expr_t *f)
{
    if (!f || f->ops == &ops_summation)
        return NULL;
    if (f->ops == &ops_delta)
        return f;
    const expr_t *left = zeta_impulse(f->a);
    return left ? left : zeta_impulse(f->b);
}

static const expr_t *zeta_logarithm(const expr_t *f, const expr_t *index)
{
    if (!f)
        return NULL;
    if (f->ops == &ops_log && expr_struct_eq(f->a, index))
        return f;
    const expr_t *left = zeta_logarithm(f->a, index);
    return left ? left : zeta_logarithm(f->b, index);
}

static expr_t *zeta_series_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w)
{
    if (f->ops != &ops_summation || !f->b || f->b->ops != &ops_argument_list || !expr_is_var(f->b->a))
        return NULL;
    const expr_t *index = f->b->a, *limits = f->b->b;
    if (!limits || limits->ops != &ops_argument_list || !expr_const_is_one(limits->a) ||
        !expr_is_const(limits->b) || !num_is_inf(limits->b->c) || num_get_sign(limits->b->c) <= 0 ||
        expr_fourier_uses(x, index) || expr_fourier_uses(w, index))
        return NULL;
    const expr_t *impulse = zeta_impulse(f->a);
    expr_t *rate = NULL, *offset = NULL;
    if (!impulse || !expr_fourier_affine(c, impulse->a, x, &rate, &offset) || !expr_const_is_one(rate))
        return NULL;
    const expr_t *log_index = zeta_logarithm(offset, index);
    if (!log_index)
        return NULL;
    expr_t *b = NULL, *remainder = NULL;
    expr_t *dummy = expr_fourier_fresh_variable(c, f, w);
    expr_t *linear = replace(c, offset, log_index, dummy);
    if (!expr_fourier_affine(c, linear, dummy, &b, &remainder) || !expr_const_is_zero(remainder) ||
        expr_fourier_uses(b, index) || expr_fourier_uses(b, x))
        return NULL;
    expr_t *weight = NULL, *regular = NULL;
    expr_t *term = replace(c, f->a, impulse, dummy);
    if (!expr_fourier_affine(c, term, dummy, &weight, &regular) || !expr_const_is_zero(regular))
        return NULL;
    const expr_t *base = NULL, *power = NULL;
    bool reciprocal = weight->ops == &ops_div && expr_const_is_one(weight->a);
    const expr_t *factor = reciprocal ? weight->b : weight;
    if (!expr_fourier_match_power(c, factor, &base, &power) || !expr_struct_eq(base, index) ||
        expr_fourier_uses(power, index) || expr_fourier_uses(power, x))
        return NULL;
    expr_t *sigma = reciprocal ? expr_fourier_keep(c, expr_clone(power)) : ft_neg(c, power);
    if (!expr_fourier_real_parameter(c, b) || !expr_fourier_positive(c, expr_fourier_abs(c, b)) ||
        !expr_fourier_positive(c, ft_sub(c, sigma, integer(c, 1))))
        return NULL;
    expr_t *imaginary = ft_mul(c, constant(c, NUM_I), ft_mul(c, b, w));
    expr_t *argument = c->inverse ? ft_add(c, sigma, imaginary) : ft_sub(c, sigma, imaginary);
    return expr_fourier_keep(c, expr_zeta(argument));
}

/* Dirichlet exponentials transform to impulses at minus the logarithms of the integers. */
expr_t *expr_fourier_zeta_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w)
{
    if (f->ops != &ops_zeta)
        return zeta_series_pair(c, f, x, w);
    expr_t *slope = NULL, *sigma = NULL;
    if (!expr_fourier_affine(c, f->a, x, &slope, &sigma))
        return NULL;
    expr_t *b = clean(c, ft_mul(c, constant(c, NUM_NEG_I), slope));
    if (expr_const_is_zero(b) || !expr_fourier_real_parameter(c, b) ||
        !expr_fourier_positive(c, expr_fourier_abs(c, b)) ||
        !expr_fourier_positive(c, ft_sub(c, sigma, integer(c, 1))))
        return NULL;
    expr_t *index = zeta_index(c, f, w);
    expr_t *shift = expr_const_is_one(b) ? ft_ln(c, index) : ft_mul(c, b, ft_ln(c, index));
    expr_t *argument = c->inverse ? ft_sub(c, w, shift) : ft_add(c, w, shift);
    expr_t *term = ft_mul(c, ft_pow_xp(c, index, ft_neg(c, sigma)), ft_delta(c, argument));
    expr_t *sum = expr_fourier_keep(c, expr_new_finite_summation_range(term, index, integer(c, 1), constant(c, NUM_INF)));
    return ft_mul(c, ft_mul(c, integer(c, 2), pi_constant(c)), sum);
}
