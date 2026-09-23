#include "expr_fourier_internal.h"

/* Collect products and quotients of exponentials without assuming a particular factor association. */
bool expr_fourier_exponential_factors(fourier_context_t *c, const expr_t *f, const expr_t *x,
                                       expr_t **phase, expr_t **scale)
{
    const expr_t *argument = exponent(f);
    if (argument) {
        *phase = keep(c, expr_clone(argument));
        *scale = integer(c, 1);
        return true;
    }
    if (!uses(f, x)) {
        *phase = integer(c, 0);
        *scale = keep(c, expr_clone(f));
        return true;
    }
    if (f->ops == &ops_neg) {
        if (!expr_fourier_exponential_factors(c, f->a, x, phase, scale))
            return false;
        *scale = ft_neg(c, *scale);
        return true;
    }
    if (f->ops != &ops_mul && f->ops != &ops_div)
        return false;
    expr_t *left_phase, *left_scale, *right_phase, *right_scale;
    if (!expr_fourier_exponential_factors(c, f->a, x, &left_phase, &left_scale) ||
        !expr_fourier_exponential_factors(c, f->b, x, &right_phase, &right_scale))
        return false;
    bool divide = f->ops == &ops_div;
    *phase = clean(c, divide ? ft_sub(c, left_phase, right_phase) : ft_add(c, left_phase, right_phase));
    *scale = clean(c, divide ? ft_div(c, left_scale, right_scale) : ft_mul(c, left_scale, right_scale));
    return true;
}

/* Only rational product factors can cancel the source variable when its pole is removed. */
static bool has_source_denominator(fourier_context_t *c, const expr_t *f, const expr_t *x)
{
    if (f->ops == &ops_div && uses(f->b, x))
        return true;
    const expr_t *base = NULL, *power = NULL;
    if (match_power(c, f, &base, &power) && expr_is_const(power) && num_eq(power->c, NUM_NEG_ONE))
        return uses(base, x);
    if (f->ops == &ops_neg)
        return has_source_denominator(c, f->a, x);
    if (f->ops == &ops_mul || f->ops == &ops_div)
        return has_source_denominator(c, f->a, x) || has_source_denominator(c, f->b, x);
    return false;
}

/* Recognise C exp(-d |x| + i p x)/x, retaining the actual singular denominator. */
static bool atan_spectrum(fourier_context_t *c, const expr_t *f, const expr_t *x,
                          expr_t **decay, expr_t **modulation, expr_t **scale)
{
    if (!has_source_denominator(c, f, x))
        return false;
    expr_t *regular = clean(c, ft_mul(c, f, x));
    expr_t *phase = NULL;
    if (!expr_fourier_exponential_factors(c, regular, x, &phase, scale))
        return false;
    const expr_t *absolute = absolute_source(phase, x);
    if (!absolute)
        return false;
    expr_t *dummy = fresh_variable(c, phase, x);
    expr_t *rewritten = replace(c, phase, absolute, dummy);
    expr_t *coefficient = NULL, *remainder = NULL, *constant_part = NULL;
    if (!affine(c, rewritten, dummy, &coefficient, &remainder) || uses(coefficient, x) ||
        !affine(c, remainder, x, modulation, &constant_part))
        return false;
    *decay = clean(c, ft_neg(c, coefficient));
    *modulation = clean(c, ft_neg(c, ft_mul(c, constant(c, NUM_I), *modulation)));
    *scale = ft_mul(c, *scale, ft_exp(c, constant_part));
    return true;
}

/* Both directions use the same pairs; the caller applies inverse normalisation exactly once. */
expr_t *expr_fourier_atan_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w)
{
    expr_t *i = constant(c, NUM_I);
    if (c->inverse)
        i = ft_neg(c, i);
    if (f->ops == &ops_atan) {
        expr_t *rate = NULL, *offset = NULL;
        if (!affine(c, f->a, x, &rate, &offset))
            return NULL;
        if (expr_const_is_zero(rate)) {
            if (expr_const_is_zero(offset))
                return integer(c, 0);
            expr_t *body = keep(c, expr_atan(offset));
            return ft_mul(c, ft_mul(c, integer(c, 2), pi_constant(c)), ft_mul(c, body, ft_delta(c, w)));
        }
        expr_t *magnitude = ft_abs(c, rate);
        if (!real_parameter(c, rate) || !real_parameter(c, offset) || !positive(c, magnitude) ||
            !positive(c, ft_abs(c, w)))
            return NULL;
        expr_t *phase = ft_sub(c, ft_div(c, ft_mul(c, ft_mul(c, i, w), offset), rate),
                               ft_div(c, ft_abs(c, w), magnitude));
        expr_t *coefficient = ft_neg(c, ft_mul(c, ft_mul(c, i, pi_constant(c)), ft_div(c, rate, magnitude)));
        return ft_div(c, ft_mul(c, coefficient, ft_exp(c, phase)), w);
    }
    expr_t *decay = NULL, *modulation = NULL, *scale = NULL;
    if (!atan_spectrum(c, f, x, &decay, &modulation, &scale) ||
        !real_parameter(c, decay) || !positive(c, decay) || !real_parameter(c, modulation))
        return NULL;
    expr_t *coordinate = ft_div(c, ft_sub(c, w, c->inverse ? ft_neg(c, modulation) : modulation), decay);
    expr_t *body = keep(c, expr_atan(coordinate));
    return ft_neg(c, ft_mul(c, ft_mul(c, integer(c, 2), i), ft_mul(c, scale, body)));
}

/* Consume only the matched spectrum's own pole exclusion, never an unrelated source restriction. */
bool expr_fourier_atan_pole_condition(fourier_context_t *c, const expr_t *f,
                                     const expr_t *x, const expr_t *condition)
{
    if (!expr_is_op(condition, &ops_abs))
        return false;
    expr_t *rate = NULL, *offset = NULL, *decay = NULL, *modulation = NULL, *scale = NULL;
    return affine(c, condition->a, x, &rate, &offset) && !expr_const_is_zero(rate) &&
           expr_const_is_zero(offset) && atan_spectrum(c, f, x, &decay, &modulation, &scale);
}

/* Keep distributional interpretation outside the expression's mathematical conditions. */
const char *expr_fourier_atan_note(const expr_t *transform)
{
    if (transform->a->ops != &ops_atan)
        return NULL;
    expr_t *specialised = expr_transform_bound_constants(transform);
    if (specialised)
        transform = specialised;
    fourier_context_t c = {.inverse = transform->ops == &ops_inverse_fourier};
    expr_t *rate = NULL, *offset = NULL;
    const expr_t *source = transform->b->a;
    bool matched = affine(&c, transform->a->a, source, &rate, &offset) && !expr_const_is_zero(rate) &&
                   expr_fourier_atan_pair(&c, transform->a, source, transform->b->b->a) && !c.failed;
    expr_free(c.conditions);
    for (size_t n = 0u; n < c.count; ++n)
        expr_free(c.nodes[n]);
    free(c.nodes);
    expr_free(specialised);
    return matched ? "This Fourier pair is distributional. The displayed spectrum excludes zero; "
                     "its inverse uses symmetric cancellation at zero and is defined on the whole real axis."
                   : NULL;
}
