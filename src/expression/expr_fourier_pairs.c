/* Fourier pairs sharing kernel matching, including periodic principal-value spectra. */
#include "expr_fourier_internal.h"

/* ------------------------------------------------------------------ */
/* Shared kernel matching */
/* ------------------------------------------------------------------ */

/* Collect products and quotients of exponentials without assuming a particular factor association. */
static bool exponential_factors(fourier_context_t *c, const expr_t *f, const expr_t *x,
                                expr_t **phase, expr_t **scale)
{
    const expr_t *argument = expr_fourier_exponent(f);
    if (argument) {
        *phase = expr_fourier_keep(c, expr_clone(argument));
        *scale = integer(c, 1);
        return true;
    }
    if (!expr_fourier_uses(f, x)) {
        *phase = integer(c, 0);
        *scale = expr_fourier_keep(c, expr_clone(f));
        return true;
    }
    if (f->ops == &ops_neg) {
        if (!exponential_factors(c, f->a, x, phase, scale))
            return false;
        *scale = ft_neg(c, *scale);
        return true;
    }
    if (f->ops != &ops_mul && f->ops != &ops_div)
        return false;
    expr_t *left_phase, *left_scale, *right_phase, *right_scale;
    if (!exponential_factors(c, f->a, x, &left_phase, &left_scale) ||
        !exponential_factors(c, f->b, x, &right_phase, &right_scale))
        return false;
    bool divide = f->ops == &ops_div;
    *phase = clean(c, divide ? ft_sub(c, left_phase, right_phase) : ft_add(c, left_phase, right_phase));
    *scale = clean(c, divide ? ft_div(c, left_scale, right_scale) : ft_mul(c, left_scale, right_scale));
    return true;
}

/* Only rational product factors can cancel the source variable when its pole is removed. */
static bool has_source_denominator(fourier_context_t *c, const expr_t *f, const expr_t *x)
{
    if (f->ops == &ops_div && expr_fourier_uses(f->b, x))
        return true;
    const expr_t *base = NULL, *power = NULL;
    if (expr_fourier_match_power(c, f, &base, &power) && expr_is_const(power) && num_eq(power->c, NUM_NEG_ONE))
        return expr_fourier_uses(base, x);
    if (f->ops == &ops_neg)
        return has_source_denominator(c, f->a, x);
    if (f->ops == &ops_mul || f->ops == &ops_div)
        return has_source_denominator(c, f->a, x) || has_source_denominator(c, f->b, x);
    return false;
}

/* ------------------------------------------------------------------ */
/* Locally integrable absolute-power pairs */
/* ------------------------------------------------------------------ */

/* Read a scalar multiple of |u|^p without composing principal powers of complex-valued bases. */
static bool absolute_power_parts(fourier_context_t *c, const expr_t *f, const expr_t *x,
                                  const expr_t **argument, expr_t **power, expr_t **scale)
{
    if (f->ops == &ops_neg) {
        if (!absolute_power_parts(c, f->a, x, argument, power, scale))
            return false;
        *scale = ft_neg(c, *scale);
        return true;
    }
    if (f->ops == &ops_div && !expr_fourier_uses(f->a, x)) {
        if (!absolute_power_parts(c, f->b, x, argument, power, scale))
            return false;
        *power = clean(c, ft_neg(c, *power));
        *scale = ft_div(c, f->a, *scale);
        return true;
    }
    if (f->ops == &ops_mul || (f->ops == &ops_div && !expr_fourier_uses(f->b, x))) {
        const expr_t *scalar = !expr_fourier_uses(f->a, x) ? f->a : f->b;
        const expr_t *dependent = scalar == f->a ? f->b : f->a;
        if (expr_fourier_uses(scalar, x) || !absolute_power_parts(c, dependent, x, argument, power, scale))
            return false;
        *scale = f->ops == &ops_div ? ft_div(c, *scale, scalar) : ft_mul(c, *scale, scalar);
        return true;
    }
    if (f->ops == &ops_sqrt) {
        if (!absolute_power_parts(c, f->a, x, argument, power, scale))
            return false;
        number_t exponent_value = NUM_NAN, scale_value = NUM_NAN;
        bool safe = expr_fourier_literal_value(*power, &exponent_value) && num_is_real(exponent_value) &&
                    expr_fourier_literal_value(*scale, &scale_value) && num_is_real(scale_value) && num_gt(scale_value, NUM_ZERO);
        num_destroy(&exponent_value);
        num_destroy(&scale_value);
        if (!safe)
            return false;
        *power = clean(c, ft_div(c, *power, integer(c, 2)));
        *scale = ft_sqrt(c, *scale);
        return true;
    }
    const expr_t *base = f, *order = NULL;
    *power = integer(c, 1);
    if (expr_fourier_match_power(c, f, &base, &order)) {
        if (expr_fourier_uses(order, x))
            return false;
        *power = expr_fourier_keep(c, expr_clone(order));
    }
    if (base->ops == &ops_sqrt) {
        base = base->a;
        *power = clean(c, ft_div(c, *power, integer(c, 2)));
    }
    if (base->ops != &ops_abs || !expr_fourier_uses(base->a, x))
        return false;
    *argument = base->a;
    *scale = integer(c, 1);
    return true;
}

/* The cosine Mellin integral gives an ordinary oscillatory transform in -1 < Re(p) < 0. */
expr_t *expr_fourier_absolute_power_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w)
{
    const expr_t *argument = NULL;
    expr_t *power = NULL, *scale = NULL, *rate = NULL, *offset = NULL;
    if (!absolute_power_parts(c, f, x, &argument, &power, &scale) ||
        !expr_fourier_affine(c, argument, x, &rate, &offset) || expr_const_is_zero(rate))
        return NULL;
    expr_t *order = clean(c, ft_add(c, power, integer(c, 1)));
    expr_t *saved_conditions = expr_clone(c->conditions);
    if (!expr_fourier_positive(c, order) || !expr_fourier_positive(c, ft_neg(c, power)) ||
        !expr_fourier_real_parameter(c, rate) || !expr_fourier_real_parameter(c, offset) ||
        !expr_fourier_positive(c, expr_fourier_abs(c, rate)) || !expr_fourier_positive(c, expr_fourier_abs(c, w))) {
        expr_free(c->conditions);
        c->conditions = saved_conditions;
        return NULL;
    }
    expr_free(saved_conditions);
    expr_t *angle = ft_div(c, ft_mul(c, pi_constant(c), order), integer(c, 2));
    number_t value = NUM_NAN;
    bool half_order = expr_fourier_literal_value(order, &value) && num_eq(value, NUM_HALF);
    num_destroy(&value);
    /* Gamma(1/2) = sqrt(pi) and cos(pi/4) = 1/sqrt(2), retaining exact symbolic pi. */
    expr_t *coefficient = half_order ? ft_sqrt(c, ft_mul(c, integer(c, 2), pi_constant(c)))
                                    : ft_mul(c, integer(c, 2),
                                             ft_mul(c, expr_fourier_keep(c, expr_gamma(order)),
                                                    expr_fourier_keep(c, expr_cos(angle))));
    expr_t *scaled_coefficient = clean(c, ft_mul(c, scale, coefficient));
    expr_t *kernel;
    if (half_order) {
        expr_t *width = ft_sqrt(c, ft_mul(c, expr_fourier_abs(c, rate), expr_fourier_abs(c, w)));
        kernel = ft_div(c, scaled_coefficient, width);
    } else {
        expr_t *magnitude = ft_mul(c, ft_pow_xp(c, expr_fourier_abs(c, rate), power),
                                     ft_pow_xp(c, expr_fourier_abs(c, w), ft_neg(c, order)));
        kernel = ft_mul(c, scaled_coefficient, magnitude);
    }
    expr_t *i = constant(c, c->inverse ? NUM_NEG_I : NUM_I);
    expr_t *phase = ft_exp(c, ft_div(c, ft_mul(c, ft_mul(c, i, w), offset), rate));
    return ft_mul(c, phase, kernel);
}

/* An integrable power may cross its own excluded point; unrelated source restrictions remain in force. */
bool expr_fourier_absolute_power_pole_condition(fourier_context_t *c, const expr_t *f,
                                                const expr_t *x, const expr_t *condition)
{
    if (!expr_is_op(condition, &ops_abs))
        return false;
    const expr_t *argument = NULL;
    expr_t *power = NULL, *scale = NULL, *rate = NULL, *offset = NULL;
    expr_t *guard_rate = NULL, *guard_offset = NULL;
    if (!absolute_power_parts(c, f, x, &argument, &power, &scale) ||
        !expr_fourier_affine(c, argument, x, &rate, &offset) || expr_const_is_zero(rate) ||
        !expr_fourier_affine(c, condition->a, x, &guard_rate, &guard_offset))
        return false;
    number_t value = NUM_NAN;
    bool nonzero = expr_fourier_literal_value(guard_rate, &value) && !num_is_zero(value);
    num_destroy(&value);
    if (!nonzero || !expr_const_is_zero(clean(c, ft_sub(c, ft_mul(c, rate, guard_offset),
                                                         ft_mul(c, offset, guard_rate)))))
        return false;
    /* Check the whole supported strip and affine domain before discarding a source exclusion. */
    fourier_context_t probe = {.inverse = c->inverse};
    bool matched = expr_fourier_absolute_power_pair(&probe, f, x, x) && !probe.failed;
    expr_free(probe.conditions);
    for (size_t n = 0u; n < probe.count; ++n)
        expr_free(probe.nodes[n]);
    free(probe.nodes);
    return matched;
}

/* ------------------------------------------------------------------ */
/* Sign-function reciprocal pair */
/* ------------------------------------------------------------------ */

/* Match C exp(i p x)/x, not a higher-order pole or a quotient with another source-dependent factor. */
static bool sgn_spectrum(fourier_context_t *c, const expr_t *f, const expr_t *x,
                         expr_t **modulation, expr_t **scale)
{
    if (!has_source_denominator(c, f, x))
        return false;
    expr_t *regular = clean(c, ft_mul(c, f, x));
    expr_t *phase = NULL, *rate = NULL, *offset = NULL;
    if (!exponential_factors(c, regular, x, &phase, scale) ||
        !expr_fourier_affine(c, phase, x, &rate, &offset))
        return false;
    *modulation = clean(c, ft_neg(c, ft_mul(c, constant(c, NUM_I), rate)));
    *scale = ft_mul(c, *scale, ft_exp(c, offset));
    return true;
}

/* The reciprocal representative uses symmetric cancellation at zero in either transform direction. */
expr_t *expr_fourier_sgn_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w)
{
    expr_t *i = constant(c, NUM_I);
    if (c->inverse)
        i = ft_neg(c, i);
    if (f->ops == &ops_sgn) {
        expr_t *rate = NULL, *offset = NULL;
        if (!expr_fourier_affine(c, f->a, x, &rate, &offset))
            return NULL;
        if (!expr_fourier_real_parameter(c, rate) || !expr_fourier_real_parameter(c, offset))
            return NULL;
        if (expr_const_is_zero(rate)) {
            expr_t *value = expr_fourier_keep(c, expr_sgn(offset));
            return ft_mul(c, ft_mul(c, integer(c, 2), pi_constant(c)), ft_mul(c, value, ft_delta(c, w)));
        }
        if (!expr_fourier_positive(c, expr_fourier_abs(c, rate)) || !expr_fourier_positive(c, expr_fourier_abs(c, w)))
            return NULL;
        expr_t *phase = ft_exp(c, ft_div(c, ft_mul(c, ft_mul(c, i, w), offset), rate));
        expr_t *orientation = ft_div(c, rate, expr_fourier_abs(c, rate));
        expr_t *coefficient = ft_neg(c, ft_mul(c, integer(c, 2), ft_mul(c, i, orientation)));
        return ft_div(c, ft_mul(c, coefficient, phase), w);
    }
    expr_t *modulation = NULL, *scale = NULL;
    if (!sgn_spectrum(c, f, x, &modulation, &scale) || !expr_fourier_real_parameter(c, modulation))
        return NULL;
    expr_t *coordinate = expr_const_is_zero(modulation) ? (expr_t *)w :
        clean(c, ft_sub(c, w, c->inverse ? ft_neg(c, modulation) : modulation));
    expr_t *body = expr_fourier_keep(c, expr_sgn(coordinate));
    return ft_neg(c, ft_mul(c, ft_mul(c, i, pi_constant(c)), ft_mul(c, scale, body)));
}

/* Consume the spectrum's own zero-frequency exclusion, not an unrelated restriction. */
bool expr_fourier_sgn_pole_condition(fourier_context_t *c, const expr_t *f,
                                    const expr_t *x, const expr_t *condition)
{
    if (!expr_is_op(condition, &ops_abs))
        return false;
    expr_t *rate = NULL, *offset = NULL, *modulation = NULL, *scale = NULL;
    if (!expr_fourier_affine(c, condition->a, x, &rate, &offset) || !expr_const_is_zero(offset))
        return false;
    number_t value = NUM_NAN;
    bool nonzero = expr_fourier_literal_value(rate, &value) && num_is_finite(value) && !num_is_zero(value);
    num_destroy(&value);
    return nonzero && sgn_spectrum(c, f, x, &modulation, &scale);
}

/* Mathematical domain predicates remain separate from the distributional interpretation note. */
const char *expr_fourier_sgn_note(const expr_t *transform)
{
    fourier_context_t c = {.inverse = transform->ops == &ops_inverse_fourier};
    expr_t *pair = expr_fourier_sgn_pair(&c, transform->a, transform->b->a, transform->b->b->a);
    bool matched = pair && !c.failed;
    expr_free(c.conditions);
    for (size_t index = 0u; index < c.count; ++index)
        expr_free(c.nodes[index]);
    free(c.nodes);
    return matched ? "This Fourier pair is distributional. Reciprocal spectra use symmetric cancellation at zero; "
                     "their numerical values apply away from zero, and sgn(0) = 0."
                   : NULL;
}

/* ------------------------------------------------------------------ */
/* Arctangent exponential-reciprocal pair */
/* ------------------------------------------------------------------ */

/* Recognise C exp(-d |x| + i p x)/x, retaining the actual singular denominator. */
static bool atan_spectrum(fourier_context_t *c, const expr_t *f, const expr_t *x,
                          expr_t **decay, expr_t **modulation, expr_t **scale)
{
    if (!has_source_denominator(c, f, x))
        return false;
    expr_t *regular = clean(c, ft_mul(c, f, x));
    expr_t *phase = NULL;
    if (!exponential_factors(c, regular, x, &phase, scale))
        return false;
    const expr_t *absolute = expr_fourier_absolute_source(phase, x);
    if (!absolute)
        return false;
    expr_t *dummy = expr_fourier_fresh_variable(c, phase, x);
    expr_t *rewritten = replace(c, phase, absolute, dummy);
    expr_t *coefficient = NULL, *remainder = NULL, *constant_part = NULL;
    if (!expr_fourier_affine(c, rewritten, dummy, &coefficient, &remainder) || expr_fourier_uses(coefficient, x) ||
        !expr_fourier_affine(c, remainder, x, modulation, &constant_part))
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
        if (!expr_fourier_affine(c, f->a, x, &rate, &offset))
            return NULL;
        if (expr_const_is_zero(rate)) {
            if (expr_const_is_zero(offset))
                return integer(c, 0);
            expr_t *body = expr_fourier_keep(c, expr_atan(offset));
            return ft_mul(c, ft_mul(c, integer(c, 2), pi_constant(c)), ft_mul(c, body, ft_delta(c, w)));
        }
        expr_t *magnitude = expr_fourier_abs(c, rate);
        if (!expr_fourier_real_parameter(c, rate) || !expr_fourier_real_parameter(c, offset) ||
            !expr_fourier_positive(c, magnitude) ||
            !expr_fourier_positive(c, expr_fourier_abs(c, w)))
            return NULL;
        expr_t *phase = ft_sub(c, ft_div(c, ft_mul(c, ft_mul(c, i, w), offset), rate),
                               ft_div(c, expr_fourier_abs(c, w), magnitude));
        expr_t *coefficient = ft_neg(c, ft_mul(c, ft_mul(c, i, pi_constant(c)), ft_div(c, rate, magnitude)));
        return ft_div(c, ft_mul(c, coefficient, ft_exp(c, phase)), w);
    }
    expr_t *decay = NULL, *modulation = NULL, *scale = NULL;
    if (!atan_spectrum(c, f, x, &decay, &modulation, &scale) ||
        !expr_fourier_real_parameter(c, decay) || !expr_fourier_positive(c, decay) || !expr_fourier_real_parameter(c, modulation))
        return NULL;
    expr_t *coordinate = ft_div(c, ft_sub(c, w, c->inverse ? ft_neg(c, modulation) : modulation), decay);
    expr_t *body = expr_fourier_keep(c, expr_atan(coordinate));
    return ft_neg(c, ft_mul(c, ft_mul(c, integer(c, 2), i), ft_mul(c, scale, body)));
}

/* Consume only the matched spectrum's own pole exclusion, never an unrelated source restriction. */
bool expr_fourier_atan_pole_condition(fourier_context_t *c, const expr_t *f,
                                     const expr_t *x, const expr_t *condition)
{
    if (!expr_is_op(condition, &ops_abs))
        return false;
    expr_t *rate = NULL, *offset = NULL, *decay = NULL, *modulation = NULL, *scale = NULL;
    return expr_fourier_affine(c, condition->a, x, &rate, &offset) && !expr_const_is_zero(rate) &&
           expr_const_is_zero(offset) && atan_spectrum(c, f, x, &decay, &modulation, &scale);
}

/* Keep distributional interpretation outside the expression's mathematical conditions. */
const char *expr_fourier_atan_note(const expr_t *transform)
{
    if (transform->a->ops != &ops_atan && transform->a->ops != &ops_asinh)
        return NULL;
    expr_t *specialised = expr_transform_bound_constants(transform);
    if (specialised)
        transform = specialised;
    fourier_context_t c = {.inverse = transform->ops == &ops_inverse_fourier};
    expr_t *rate = NULL, *offset = NULL;
    const expr_t *source = transform->b->a;
    expr_t *(*pair)(fourier_context_t *, const expr_t *, const expr_t *, const expr_t *) =
        transform->a->ops == &ops_atan ? expr_fourier_atan_pair : expr_fourier_asinh_pair;
    bool matched = expr_fourier_affine(&c, transform->a->a, source, &rate, &offset) && !expr_const_is_zero(rate) &&
                   pair(&c, transform->a, source, transform->b->b->a) && !c.failed;
    expr_free(c.conditions);
    for (size_t n = 0u; n < c.count; ++n)
        expr_free(c.nodes[n]);
    free(c.nodes);
    expr_free(specialised);
    return matched ? "This Fourier pair is distributional. The displayed spectrum excludes zero; "
                     "its inverse uses symmetric cancellation at zero and is defined on the whole real axis."
                   : NULL;
}

/* ------------------------------------------------------------------ */
/* Inverse hyperbolic sine and modified-Bessel pair */
/* ------------------------------------------------------------------ */

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
    const expr_t *absolute = expr_fourier_absolute_source(kernel->b, x);
    if (!absolute)
        return false;
    expr_t *dummy = expr_fourier_fresh_variable(c, f, x), *offset = NULL;
    if (!expr_fourier_affine(c, replace(c, kernel->b, absolute, dummy), dummy, width, &offset) ||
        expr_fourier_uses(*width, x) || !expr_const_is_zero(offset))
        return false;
    expr_t *residual = clean(c, ft_div(c, ft_mul(c, f, x), kernel));
    expr_t *phase = NULL, *constant_part = NULL;
    if (!exponential_factors(c, residual, x, &phase, scale) ||
        !expr_fourier_affine(c, phase, x, modulation, &constant_part))
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
        if (!expr_fourier_affine(c, f->a, x, &rate, &offset))
            return NULL;
        if (expr_const_is_zero(rate)) {
            if (expr_const_is_zero(offset))
                return integer(c, 0);
            expr_t *body = expr_fourier_keep(c, expr_asinh(offset));
            return ft_mul(c, ft_mul(c, integer(c, 2), pi_constant(c)), ft_mul(c, body, ft_delta(c, w)));
        }
        expr_t *magnitude = expr_fourier_abs(c, rate);
        if (!expr_fourier_real_parameter(c, rate) || !expr_fourier_real_parameter(c, offset) ||
            !expr_fourier_positive(c, magnitude) ||
            !expr_fourier_positive(c, expr_fourier_abs(c, w)))
            return NULL;
        expr_t *argument = ft_div(c, expr_fourier_abs(c, w), magnitude);
        expr_t *kernel = expr_fourier_keep(c, expr_bessel_k(integer(c, 0), argument));
        expr_t *phase = ft_exp(c, ft_div(c, ft_mul(c, ft_mul(c, i, w), offset), rate));
        expr_t *coefficient = ft_neg(c, ft_mul(c, ft_mul(c, integer(c, 2), i), ft_div(c, rate, magnitude)));
        return ft_div(c, ft_mul(c, coefficient, ft_mul(c, phase, kernel)), w);
    }
    expr_t *width = NULL, *modulation = NULL, *scale = NULL;
    if (!asinh_spectrum(c, f, x, &width, &modulation, &scale) ||
        !expr_fourier_real_parameter(c, width) || !expr_fourier_positive(c, width) || !expr_fourier_real_parameter(c, modulation))
        return NULL;
    expr_t *coordinate = ft_div(c, ft_sub(c, w, c->inverse ? ft_neg(c, modulation) : modulation), width);
    expr_t *body = expr_fourier_keep(c, expr_asinh(coordinate));
    return ft_neg(c, ft_mul(c, ft_mul(c, pi_constant(c), i), ft_mul(c, scale, body)));
}

/* Consume precisely the recognised spectrum's zero-frequency exclusion. */
bool expr_fourier_asinh_pole_condition(fourier_context_t *c, const expr_t *f,
                                      const expr_t *x, const expr_t *condition)
{
    if (!expr_is_op(condition, &ops_abs))
        return false;
    expr_t *rate = NULL, *offset = NULL, *width = NULL, *modulation = NULL, *scale = NULL;
    return expr_fourier_affine(c, condition->a, x, &rate, &offset) && !expr_const_is_zero(rate) &&
           expr_const_is_zero(offset) && asinh_spectrum(c, f, x, &width, &modulation, &scale);
}

/* ------------------------------------------------------------------ */
/* Vertical-line gamma and nested-exponential pair */
/* ------------------------------------------------------------------ */

static const expr_t *inner_exponential(const expr_t *f, const expr_t *x)
{
    if (!f)
        return NULL;
    if (expr_fourier_exponent(f) && expr_fourier_uses(f, x))
        return f;
    const expr_t *left = inner_exponential(f->a, x);
    return left ? left : inner_exponential(f->b, x);
}

/* A real non-zero slope gives super-exponential growth on one real tail, regardless of the offset. */
static bool real_gamma_slope(fourier_context_t *c, const expr_t *f, const expr_t *x)
{
    expr_t *rate = NULL, *offset = NULL;
    number_t value = NUM_NAN;
    bool matched = f->ops == &ops_gamma && expr_fourier_affine(c, f->a, x, &rate, &offset) &&
                   expr_fourier_literal_value(rate, &value) && num_is_real(value) && !num_is_zero(value);
    num_destroy(&value);
    return matched;
}

/* Euler's integral on a vertical line is the inverse Fourier integral of exp(a k - exp(k)). */
expr_t *expr_fourier_gamma_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w)
{
    expr_t *i = constant(c, NUM_I);
    if (f->ops == &ops_gamma) {
        if (real_gamma_slope(c, f, x))
            return constant(c, NUM_NAN);
        expr_t *slope = NULL, *a = NULL;
        if (!expr_fourier_affine(c, f->a, x, &slope, &a))
            return NULL;
        expr_t *b = clean(c, ft_neg(c, ft_mul(c, i, slope)));
        if (expr_const_is_zero(b) || !expr_fourier_real_parameter(c, b) ||
            !expr_fourier_positive(c, expr_fourier_abs(c, b)) || !expr_fourier_positive(c, a))
            return NULL;
        expr_t *q = ft_div(c, c->inverse ? ft_neg(c, w) : w, b);
        expr_t *kernel = ft_exp(c, ft_sub(c, ft_mul(c, a, q), ft_exp(c, q)));
        return ft_div(c, ft_mul(c, ft_mul(c, integer(c, 2), pi_constant(c)), kernel), expr_fourier_abs(c, b));
    }
    expr_t *phase = NULL, *scale = NULL;
    if (!exponential_factors(c, f, x, &phase, &scale))
        return NULL;
    const expr_t *nested = inner_exponential(phase, x);
    if (!nested)
        return NULL;
    expr_t *b = NULL, *shift = NULL;
    if (!expr_fourier_affine(c, expr_fourier_exponent(nested), x, &b, &shift) || expr_const_is_zero(b))
        return NULL;
    expr_t *dummy = expr_fourier_fresh_variable(c, f, w);
    expr_t *coefficient = NULL, *remainder = NULL, *linear = NULL, *constant_part = NULL;
    if (!expr_fourier_affine(c, replace(c, phase, nested, dummy), dummy, &coefficient, &remainder) ||
        expr_fourier_uses(coefficient, x) ||
        !expr_fourier_affine(c, remainder, x, &linear, &constant_part))
        return NULL;
    expr_t *d = clean(c, ft_neg(c, ft_mul(c, coefficient, ft_exp(c, shift))));
    expr_t *a = clean(c, ft_div(c, linear, b));
    if (!expr_fourier_real_parameter(c, b) || !expr_fourier_positive(c, expr_fourier_abs(c, b)) ||
        !expr_fourier_real_parameter(c, d) || !expr_fourier_positive(c, d) || !expr_fourier_positive(c, a))
        return NULL;
    expr_t *frequency = ft_mul(c, i, ft_div(c, w, b));
    expr_t *argument = ft_add(c, a, c->inverse ? frequency : ft_neg(c, frequency));
    expr_t *kernel = ft_mul(c, expr_fourier_keep(c, expr_gamma(argument)), ft_pow_xp(c, d, ft_neg(c, argument)));
    return ft_div(c, ft_mul(c, ft_mul(c, scale, ft_exp(c, constant_part)), kernel), expr_fourier_abs(c, b));
}

static void gamma_cartesian_components(fourier_context_t *c, expr_t **node, bool argument)
{
    expr_t *f = *node;
    if (!f)
        return;
    if (argument && f->ops == &ops_imag_coordinate) {
        expr_t *difference = ft_sub(c, f->a, expr_fourier_keep(c, expr_real_coordinate(f->a)));
        *node = expr_clone(clean(c, ft_mul(c, constant(c, NUM_NEG_I), difference)));
        expr_free(f);
        return;
    }
    gamma_cartesian_components(c, &f->a, argument || f->ops == &ops_gamma);
    gamma_cartesian_components(c, &f->b, argument);
    f->simplified = false;
    f->simplify_epoch = 0u;
}

/* Re(z) + i Im(z) reconstructs z; affine argument scales are simplified by the same algebra. */
expr_t *expr_fourier_gamma_cartesian_result(fourier_context_t *c, const expr_t *result)
{
    expr_t *out = expr_clone(result);
    gamma_cartesian_components(c, &out, false);
    return clean(c, expr_fourier_keep(c, out));
}

/* Explain the mathematical failure, rather than describing a missing symbolic rule. */
const char *expr_fourier_gamma_note(const expr_t *transform)
{
    if (transform->b->a->ops == &ops_imag_coordinate)
        return "This Fourier transform integrates over the imaginary coordinate, holding the real coordinate fixed. "
               "It is a vertical-line transform, not the real-axis Fourier transform.";
    expr_t *specialised = expr_transform_bound_constants(transform);
    if (specialised)
        transform = specialised;
    fourier_context_t c = {0};
    bool divergent = real_gamma_slope(&c, transform->a, transform->b->a);
    for (size_t n = 0u; n < c.count; ++n)
        expr_free(c.nodes[n]);
    free(c.nodes);
    expr_free(specialised);
    return divergent ? "This gamma function has no ordinary or tempered-distribution Fourier transform: "
                       "its super-exponential growth on a real tail prevents convergence. "
                       "Excluding its poles does not remove that obstruction." : NULL;
}

/* Periodic principal-value spectra and their impulse-series inverses. */

/* A periodic principal value has an impulse series, not an ordinary pointwise spectrum.
 * With the negative Fourier kernel, PV(tan x) = 2 sum (-1)^(n-1) sin(2nx),
 * while PV(cot x) = 2 sum sin(2nx); these series converge as distributions. */
static expr_t *impulse_term(fourier_context_t *c, const expr_t *coordinate, const expr_t *index, bool alternating)
{
    expr_t *harmonic = ft_mul(c, integer(c, 2), index);
    expr_t *term = ft_sub(c, ft_delta(c, ft_sub(c, coordinate, harmonic)),
                            ft_delta(c, ft_add(c, coordinate, harmonic)));
    if (alternating)
        term = ft_mul(c, ft_pow_xp(c, integer(c, -1), index), term);
    return clean(c, term);
}

/* Search only the summand's expression tree; a candidate is subsequently checked in full. */
static const expr_t *first_impulse(const expr_t *f)
{
    if (!f || f->ops == &ops_summation)
        return NULL;
    if (f->ops == &ops_delta)
        return f;
    const expr_t *left = first_impulse(f->a);
    return left ? left : first_impulse(f->b);
}

static expr_t *series_index(fourier_context_t *c, const expr_t *f, const expr_t *w)
{
    expr_bindings_t *source = expr_bindings_from_expr_internal(f);
    expr_bindings_t *target = expr_bindings_from_expr_internal(w);
    bool available = !expr_bindings_get(source, "n") && !expr_bindings_get(target, "n");
    expr_bindings_free(source);
    expr_bindings_free(target);
    return available ? expr_fourier_keep(c, expr_new_named_var(NUM_NAN, "n")) : expr_fourier_fresh_variable(c, f, w);
}

static expr_t *periodic_spectrum(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w)
{
    expr_t *rate = NULL, *offset = NULL;
    if (!expr_fourier_affine(c, f->a, x, &rate, &offset) || expr_const_is_zero(rate) ||
        !expr_fourier_real_parameter(c, rate) || !expr_fourier_real_parameter(c, offset) ||
        !expr_fourier_positive(c, expr_fourier_abs(c, rate)))
        return NULL;
    expr_t *index = series_index(c, f, w);
    expr_t *coordinate = clean(c, ft_div(c, w, rate));
    expr_t *term = impulse_term(c, coordinate, index, f->ops == &ops_tan);
    expr_t *sum = expr_fourier_keep(c, expr_new_finite_summation_range(term, index, integer(c, 1), constant(c, NUM_INF)));
    expr_t *i = constant(c, NUM_I);
    if (c->inverse)
        i = ft_neg(c, i);
    expr_t *phase = ft_exp(c, ft_mul(c, ft_mul(c, i, coordinate), offset));
    expr_t *coefficient = ft_mul(c, ft_mul(c, integer(c, 2), pi_constant(c)), i);
    if (f->ops == &ops_cot)
        coefficient = ft_neg(c, coefficient);
    return ft_div(c, ft_mul(c, coefficient, ft_mul(c, phase, sum)), expr_fourier_abs(c, rate));
}

static bool positive_harmonics(const expr_t *sum)
{
    if (!sum || sum->ops != &ops_summation || !sum->b || sum->b->ops != &ops_argument_list ||
        !expr_is_var(sum->b->a))
        return false;
    const expr_t *limits = sum->b->b;
    return limits && limits->ops == &ops_argument_list && expr_const_is_one(limits->a) &&
           expr_is_const(limits->b) && num_is_inf(limits->b->c) && num_get_sign(limits->b->c) > 0;
}

static const expr_t *first_series(const expr_t *f)
{
    if (!f || positive_harmonics(f))
        return f;
    if (f->ops != &ops_add && f->ops != &ops_sub && f->ops != &ops_neg)
        return NULL;
    const expr_t *left = first_series(f->a);
    return left ? left : first_series(f->b);
}

/* Summation simplification can separate the two cotangent half-trains. Recombine
 * only matching ranges, renaming their bound indices before checking the entire term. */
static expr_t *series_term(fourier_context_t *c, const expr_t *f, const expr_t *index)
{
    if (positive_harmonics(f)) {
        if (!expr_struct_eq(f->b->a, index) && expr_fourier_uses(f->a, index))
            return NULL;
        return replace(c, f->a, f->b->a, index);
    }
    if (f->ops == &ops_add || f->ops == &ops_sub) {
        expr_t *left = series_term(c, f->a, index), *right = series_term(c, f->b, index);
        return left && right ? (f->ops == &ops_add ? ft_add(c, left, right) : ft_sub(c, left, right)) : NULL;
    }
    if (f->ops == &ops_neg) {
        expr_t *term = series_term(c, f->a, index);
        return term ? ft_neg(c, term) : NULL;
    }
    return NULL;
}

/* Delta arguments must agree exactly as affine expressions, not just in spelling:
 * parsed w/2 and constructed (1/2)*w are equivalent. Replace the two independent
 * impulses by dummy symbols before comparing their complete coefficients. */
static expr_t *impulse_symbols(fourier_context_t *c, const expr_t *f, const expr_t *coordinate,
                              const expr_t *harmonic, const expr_t *minus, const expr_t *plus)
{
    if (!f)
        return NULL;
    if (f->ops == &ops_delta) {
        expr_t *difference = ft_sub(c, f->a, coordinate);
        if (expr_const_is_zero(clean(c, ft_add(c, difference, harmonic))))
            return expr_fourier_keep(c, expr_clone(minus));
        if (expr_const_is_zero(clean(c, ft_sub(c, difference, harmonic))))
            return expr_fourier_keep(c, expr_clone(plus));
        return expr_fourier_keep(c, expr_clone(f));
    }
    expr_t *out = expr_fourier_keep(c, expr_clone(f));
    if (f->a) {
        expr_free(out->a);
        out->a = expr_clone(impulse_symbols(c, f->a, coordinate, harmonic, minus, plus));
    }
    if (f->b) {
        expr_free(out->b);
        out->b = expr_clone(impulse_symbols(c, f->b, coordinate, harmonic, minus, plus));
    }
    out->simplified = false;
    out->simplify_epoch = 0u;
    return out;
}

static expr_t *periodic_series_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w)
{
    const expr_t *sum = first_series(f);
    if (!sum || expr_fourier_uses(x, sum->b->a) || expr_fourier_uses(w, sum->b->a))
        return NULL;
    const expr_t *index = sum->b->a;
    expr_t *term = expr_fourier_exact_literals(c, series_term(c, f, index));
    const expr_t *impulse = first_impulse(term);
    expr_t *rate = NULL, *offset = NULL;
    if (!impulse || !expr_fourier_affine(c, impulse->a, x, &rate, &offset) || expr_const_is_zero(rate) ||
        expr_fourier_uses(rate, index))
        return NULL;
    expr_t *coordinate = clean(c, ft_mul(c, rate, x));
    expr_t *minus = expr_fourier_fresh_variable(c, f, w), *plus = expr_fourier_fresh_variable(c, f, minus);
    expr_t *symbolic = impulse_symbols(c, term, coordinate, ft_mul(c, integer(c, 2), index), minus, plus);
    /* Two fixed templates cover the tangent and cotangent families, with any bound-index name. */
    for (unsigned alternating = 0u; alternating < 2u; ++alternating) {
        expr_t *expected = ft_sub(c, minus, plus);
        if (alternating)
            expected = ft_mul(c, ft_pow_xp(c, integer(c, -1), index), expected);
        expr_t *difference = expr_fourier_keep(c, expr_beautify(ft_sub(c, symbolic, expected)));
        if (!expr_const_is_zero(difference))
            continue;
        if (!expr_fourier_real_parameter(c, rate) || !expr_fourier_positive(c, expr_fourier_abs(c, rate)))
            return NULL;
        expr_t *scale = expr_fourier_keep(c, expr_beautify(ft_div(c, integer(c, 1), rate)));
        expr_t *expanded = expr_fourier_keep(c, expr_expand_products_internal(ft_mul(c, w, scale)));
        expr_t *argument = expr_fourier_keep(c, expr_beautify(expanded));
        expr_t *body = expr_fourier_keep(c, alternating ? expr_tan(argument) : expr_cot(argument));
        expr_t *denominator = expr_fourier_keep(c, alternating ? expr_cos(argument) : expr_sin(argument));
        if (!expr_fourier_positive(c, expr_fourier_abs(c, denominator)))
            return NULL;
        expr_t *i = constant(c, NUM_I);
        if (c->inverse != (alternating == 0u))
            i = ft_neg(c, i);
        return ft_mul(c, ft_mul(c, i, body), expr_fourier_abs(c, scale));
    }
    return NULL;
}

/* A copied ordinary representative may exclude exactly its own periodic poles.
 * This is not permission to discard arbitrary restrictions on the transform source. */
bool expr_fourier_periodic_pole_condition(const expr_t *body, const expr_t *condition)
{
    if (!body || !condition || condition->ops != &ops_abs)
        return false;
    const expr_t *denominator = condition->a;
    if (((body->ops == &ops_tan && denominator->ops == &ops_cos) ||
         (body->ops == &ops_cot && denominator->ops == &ops_sin)) &&
        expr_simplify_same_factor(body->a, denominator->a))
        return true;
    return expr_fourier_periodic_pole_condition(body->a, condition) ||
           expr_fourier_periodic_pole_condition(body->b, condition);
}

/* Recognise both directions from the actual formula; no transform provenance is retained. */
expr_t *expr_fourier_periodic_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w)
{
    if (f->ops == &ops_principal_value)
        f = f->a;
    if (f->ops == &ops_tan || f->ops == &ops_cot)
        return periodic_spectrum(c, f, x, w);
    return periodic_series_pair(c, f, x, w);
}
