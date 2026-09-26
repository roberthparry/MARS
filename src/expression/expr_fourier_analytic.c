#include "expr_fourier_internal.h"

/* Analytic evaluation is deliberately distinct from a real-axis Dirac distribution. */
bool expr_fourier_has_analytic_functional(const expr_t *f)
{
    return f && (f->ops == &ops_analytic_delta || expr_fourier_has_analytic_functional(f->a) ||
                 expr_fourier_has_analytic_functional(f->b));
}

/* Read a scalar exponential times one evaluation functional, without sampling any bindings. */
static bool evaluation_term(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w,
                            expr_t **phase, expr_t **scale, unsigned *count)
{
    if (f->ops == &ops_analytic_delta) {
        expr_t *a = NULL, *b = NULL;
        if (!affine(c, f->a, x, &a, &b) || !expr_const_is_one(a))
            return false;
        *phase = clean(c, ft_mul(c, ft_mul(c, constant(c, c->inverse ? NUM_NEG_I : NUM_I), w), b));
        *scale = integer(c, 1);
        ++*count;
        return true;
    }
    if (!uses(f, x)) {
        const expr_t *arg = exponent(f);
        *phase = arg ? keep(c, expr_clone(arg)) : integer(c, 0);
        *scale = arg ? integer(c, 1) : keep(c, expr_clone(f));
        return true;
    }
    if (f->ops == &ops_neg && evaluation_term(c, f->a, x, w, phase, scale, count)) {
        *scale = clean(c, ft_neg(c, *scale));
        return true;
    }
    if (f->ops == &ops_mul || (f->ops == &ops_div && !uses(f->b, x))) {
        expr_t *p, *q, *s, *t;
        if (!evaluation_term(c, f->a, x, w, &p, &s, count) ||
            !evaluation_term(c, f->b, x, w, &q, &t, count))
            return false;
        bool divide = f->ops == &ops_div;
        *phase = clean(c, divide ? ft_sub(c, p, q) : ft_add(c, p, q));
        *scale = clean(c, divide ? ft_div(c, s, t) : ft_mul(c, s, t));
        return true;
    }
    return false;
}

/* Keep real-frequency impulses ordinary; other centres act on entire test functions. */
static expr_t *exponential_spectrum(fourier_context_t *c, const expr_t *a, const expr_t *b, const expr_t *w)
{
    expr_t *i = constant(c, c->inverse ? NUM_NEG_I : NUM_I);
    expr_t *shift = clean(c, ft_mul(c, i, a));
    expr_t *argument = clean(c, ft_add(c, w, shift));
    number_t value = NUM_NAN;
    bool ordinary = literal_value(shift, &value) && num_is_real(value);
    num_destroy(&value);
    expr_t *impulse = ordinary ? ft_delta(c, argument) : ft_analytic_delta(c, argument);
    return ft_mul(c, ft_mul(c, integer(c, 2), pi_constant(c)), ft_mul(c, ft_exp(c, b), impulse));
}

/* Finite exponential sums have exact analytic-functional spectra and evaluation-based inverses. */
expr_t *expr_fourier_analytic_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w)
{
    if (expr_fourier_has_analytic_functional(f)) {
        if (f->ops == &ops_add || f->ops == &ops_sub) {
            expr_t *p, *q, *s, *t;
            unsigned left = 0u, right = 0u;
            if (evaluation_term(c, f->a, x, w, &p, &s, &left) && left == 1u &&
                evaluation_term(c, f->b, x, w, &q, &t, &right) && right == 1u &&
                expr_const_is_zero(clean(c, ft_add(c, p, q)))) {
                if (f->ops == &ops_sub)
                    t = clean(c, ft_neg(c, t));
                bool even = expr_const_is_zero(clean(c, ft_sub(c, s, t)));
                bool odd = expr_const_is_zero(clean(c, ft_add(c, s, t)));
                if (even || odd)
                    return ft_mul(c, ft_mul(c, integer(c, 2), s), even ? ft_cosh(c, p) : ft_sinh(c, p));
            }
        }
        expr_t *phase, *scale;
        unsigned count = 0u;
        if (evaluation_term(c, f, x, w, &phase, &scale, &count) && count == 1u)
            return ft_mul(c, scale, ft_exp(c, phase));
        return NULL;
    }
    const expr_t *base = f, *power = NULL;
    long order = 1;
    if (match_power(c, f, &base, &power)) {
        number_t value = NUM_NAN;
        bool valid = literal_value(power, &value) && num_is_real(value) && num_is_integer(value) &&
                     num_to_double(value) >= -32.0 && num_to_double(value) <= 32.0;
        if (valid)
            order = (long)num_to_double(value);
        num_destroy(&value);
        if (!valid)
            return NULL;
    }
    bool odd = base->ops == &ops_sinh || base->ops == &ops_cosech;
    bool even = base->ops == &ops_cosh || base->ops == &ops_sech;
    if (!odd && !even)
        return NULL;
    if (base->ops == &ops_cosech || base->ops == &ops_sech)
        order = -order;
    /* Bounded finite expansion only; fractional powers are not entire functions of exponential type. */
    if (order < 0 || order > 32)
        return NULL;
    expr_t *a = NULL, *b = NULL;
    if (!affine(c, base->a, x, &a, &b))
        return NULL;
    expr_t *sum = integer(c, 0);
    long choose = 1;
    for (long j = 0; j <= order; ++j) {
        expr_t *multiple = integer(c, order - 2 * j);
        expr_t *term = exponential_spectrum(c, clean(c, ft_mul(c, multiple, a)),
                                           clean(c, ft_mul(c, multiple, b)), w);
        term = ft_mul(c, integer(c, odd && (j & 1) ? -choose : choose), term);
        sum = ft_add(c, sum, term);
        if (j < order)
            choose = choose * (order - j) / (j + 1);
    }
    return ft_div(c, sum, ft_pow_xp(c, integer(c, 2), integer(c, order)));
}
