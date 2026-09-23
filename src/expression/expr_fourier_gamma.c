#include "expr_fourier_internal.h"

static const expr_t *inner_exponential(const expr_t *f, const expr_t *x)
{
    if (!f)
        return NULL;
    if (exponent(f) && uses(f, x))
        return f;
    const expr_t *left = inner_exponential(f->a, x);
    return left ? left : inner_exponential(f->b, x);
}

/* A real non-zero slope gives super-exponential growth on one real tail, regardless of the offset. */
static bool real_gamma_slope(fourier_context_t *c, const expr_t *f, const expr_t *x)
{
    expr_t *rate = NULL, *offset = NULL;
    number_t value = NUM_NAN;
    bool matched = f->ops == &ops_gamma && affine(c, f->a, x, &rate, &offset) &&
                   literal_value(rate, &value) && num_is_real(value) && !num_is_zero(value);
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
        if (!affine(c, f->a, x, &slope, &a))
            return NULL;
        expr_t *b = clean(c, ft_neg(c, ft_mul(c, i, slope)));
        if (expr_const_is_zero(b) || !real_parameter(c, b) || !positive(c, ft_abs(c, b)) || !positive(c, a))
            return NULL;
        expr_t *q = ft_div(c, c->inverse ? ft_neg(c, w) : w, b);
        expr_t *kernel = ft_exp(c, ft_sub(c, ft_mul(c, a, q), ft_exp(c, q)));
        return ft_div(c, ft_mul(c, ft_mul(c, integer(c, 2), pi_constant(c)), kernel), ft_abs(c, b));
    }
    expr_t *phase = NULL, *scale = NULL;
    if (!expr_fourier_exponential_factors(c, f, x, &phase, &scale))
        return NULL;
    const expr_t *nested = inner_exponential(phase, x);
    if (!nested)
        return NULL;
    expr_t *b = NULL, *shift = NULL;
    if (!affine(c, exponent(nested), x, &b, &shift) || expr_const_is_zero(b))
        return NULL;
    expr_t *dummy = fresh_variable(c, f, w);
    expr_t *coefficient = NULL, *remainder = NULL, *linear = NULL, *constant_part = NULL;
    if (!affine(c, replace(c, phase, nested, dummy), dummy, &coefficient, &remainder) || uses(coefficient, x) ||
        !affine(c, remainder, x, &linear, &constant_part))
        return NULL;
    expr_t *d = clean(c, ft_neg(c, ft_mul(c, coefficient, ft_exp(c, shift))));
    expr_t *a = clean(c, ft_div(c, linear, b));
    if (!real_parameter(c, b) || !positive(c, ft_abs(c, b)) ||
        !real_parameter(c, d) || !positive(c, d) || !positive(c, a))
        return NULL;
    expr_t *frequency = ft_mul(c, i, ft_div(c, w, b));
    expr_t *argument = ft_add(c, a, c->inverse ? frequency : ft_neg(c, frequency));
    expr_t *kernel = ft_mul(c, keep(c, expr_gamma(argument)), ft_pow_xp(c, d, ft_neg(c, argument)));
    return ft_div(c, ft_mul(c, ft_mul(c, scale, ft_exp(c, constant_part)), kernel), ft_abs(c, b));
}

static void gamma_cartesian_components(fourier_context_t *c, expr_t **node, bool argument)
{
    expr_t *f = *node;
    if (!f)
        return;
    if (argument && f->ops == &ops_imag_coordinate) {
        expr_t *difference = ft_sub(c, f->a, keep(c, expr_real_coordinate(f->a)));
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
    return clean(c, keep(c, out));
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
