#include "expr_fourier_internal.h"

/* Find a numerator K0 factor, not a similar function buried inside an unrelated composition. */
static const expr_t *k0_factor(const expr_t *f)
{
    if (f->ops == &ops_bessel_k && expr_const_is_zero(f->a))
        return f;
    if (f->ops == &ops_neg || f->ops == &ops_div)
        return k0_factor(f->a);
    if (f->ops != &ops_mul)
        return NULL;
    const expr_t *left = k0_factor(f->a);
    return left ? left : k0_factor(f->b);
}

/* Match C exp(i p x) K0(d |x|)/x, checking the complete residual rather than a single factor. */
static bool asinh_spectrum(fourier_context_t *c, const expr_t *f, const expr_t *x,
                           expr_t **width, expr_t **modulation, expr_t **scale)
{
    const expr_t *kernel = k0_factor(f);
    if (!kernel)
        return false;
    const expr_t *absolute = absolute_source(kernel->b, x);
    if (!absolute)
        return false;
    expr_t *dummy = fresh_variable(c, f, x), *offset = NULL;
    if (!affine(c, replace(c, kernel->b, absolute, dummy), dummy, width, &offset) ||
        uses(*width, x) || !expr_const_is_zero(offset))
        return false;
    expr_t *residual = clean(c, ft_div(c, ft_mul(c, f, x), kernel));
    expr_t *phase = NULL, *constant_part = NULL;
    if (!expr_fourier_exponential_factors(c, residual, x, &phase, scale) ||
        !affine(c, phase, x, modulation, &constant_part))
        return false;
    *modulation = clean(c, ft_neg(c, ft_mul(c, constant(c, NUM_I), *modulation)));
    *scale = ft_mul(c, *scale, ft_exp(c, constant_part));
    return true;
}

/* d(asinh(x))/dx = 1/sqrt(1+x*x), whose transform is 2 K0(|w|); oddness fixes the delta ambiguity. */
expr_t *expr_fourier_asinh_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w)
{
    expr_t *i = constant(c, c->inverse ? NUM_NEG_I : NUM_I);
    if (f->ops == &ops_asinh) {
        expr_t *rate = NULL, *offset = NULL;
        if (!affine(c, f->a, x, &rate, &offset))
            return NULL;
        if (expr_const_is_zero(rate)) {
            if (expr_const_is_zero(offset))
                return integer(c, 0);
            expr_t *body = keep(c, expr_asinh(offset));
            return ft_mul(c, ft_mul(c, integer(c, 2), pi_constant(c)), ft_mul(c, body, ft_delta(c, w)));
        }
        expr_t *magnitude = ft_abs(c, rate);
        if (!real_parameter(c, rate) || !real_parameter(c, offset) || !positive(c, magnitude) ||
            !positive(c, ft_abs(c, w)))
            return NULL;
        expr_t *argument = ft_div(c, ft_abs(c, w), magnitude);
        expr_t *kernel = keep(c, expr_bessel_k(integer(c, 0), argument));
        expr_t *phase = ft_exp(c, ft_div(c, ft_mul(c, ft_mul(c, i, w), offset), rate));
        expr_t *coefficient = ft_neg(c, ft_mul(c, ft_mul(c, integer(c, 2), i), ft_div(c, rate, magnitude)));
        return ft_div(c, ft_mul(c, coefficient, ft_mul(c, phase, kernel)), w);
    }
    expr_t *width = NULL, *modulation = NULL, *scale = NULL;
    if (!asinh_spectrum(c, f, x, &width, &modulation, &scale) ||
        !real_parameter(c, width) || !positive(c, width) || !real_parameter(c, modulation))
        return NULL;
    expr_t *coordinate = ft_div(c, ft_sub(c, w, c->inverse ? ft_neg(c, modulation) : modulation), width);
    expr_t *body = keep(c, expr_asinh(coordinate));
    return ft_neg(c, ft_mul(c, ft_mul(c, pi_constant(c), i), ft_mul(c, scale, body)));
}

/* Consume precisely the recognised spectrum's zero-frequency exclusion. */
bool expr_fourier_asinh_pole_condition(fourier_context_t *c, const expr_t *f,
                                      const expr_t *x, const expr_t *condition)
{
    if (!expr_is_op(condition, &ops_abs))
        return false;
    expr_t *rate = NULL, *offset = NULL, *width = NULL, *modulation = NULL, *scale = NULL;
    return affine(c, condition->a, x, &rate, &offset) && !expr_const_is_zero(rate) &&
           expr_const_is_zero(offset) && asinh_spectrum(c, f, x, &width, &modulation, &scale);
}
