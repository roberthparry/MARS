#include "expr_fourier_internal.h"

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
    return available ? keep(c, expr_new_named_var(NUM_NAN, "n")) : fresh_variable(c, f, w);
}

static expr_t *periodic_spectrum(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w)
{
    expr_t *rate = NULL, *offset = NULL;
    if (!affine(c, f->a, x, &rate, &offset) || expr_const_is_zero(rate) ||
        !real_parameter(c, rate) || !real_parameter(c, offset) || !positive(c, ft_abs(c, rate)))
        return NULL;
    expr_t *index = series_index(c, f, w);
    expr_t *coordinate = clean(c, ft_div(c, w, rate));
    expr_t *term = impulse_term(c, coordinate, index, f->ops == &ops_tan);
    expr_t *sum = keep(c, expr_new_finite_summation_range(term, index, integer(c, 1), constant(c, NUM_INF)));
    expr_t *i = constant(c, NUM_I);
    if (c->inverse)
        i = ft_neg(c, i);
    expr_t *phase = ft_exp(c, ft_mul(c, ft_mul(c, i, coordinate), offset));
    expr_t *coefficient = ft_mul(c, ft_mul(c, integer(c, 2), pi_constant(c)), i);
    if (f->ops == &ops_cot)
        coefficient = ft_neg(c, coefficient);
    return ft_div(c, ft_mul(c, coefficient, ft_mul(c, phase, sum)), ft_abs(c, rate));
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
        if (!expr_struct_eq(f->b->a, index) && uses(f->a, index))
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
            return keep(c, expr_clone(minus));
        if (expr_const_is_zero(clean(c, ft_sub(c, difference, harmonic))))
            return keep(c, expr_clone(plus));
        return keep(c, expr_clone(f));
    }
    expr_t *out = keep(c, expr_clone(f));
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
    if (!sum || uses(x, sum->b->a) || uses(w, sum->b->a))
        return NULL;
    const expr_t *index = sum->b->a;
    expr_t *term = exact_literals(c, series_term(c, f, index));
    const expr_t *impulse = first_impulse(term);
    expr_t *rate = NULL, *offset = NULL;
    if (!impulse || !affine(c, impulse->a, x, &rate, &offset) || expr_const_is_zero(rate) || uses(rate, index))
        return NULL;
    expr_t *coordinate = clean(c, ft_mul(c, rate, x));
    expr_t *minus = fresh_variable(c, f, w), *plus = fresh_variable(c, f, minus);
    expr_t *symbolic = impulse_symbols(c, term, coordinate, ft_mul(c, integer(c, 2), index), minus, plus);
    /* Two fixed templates cover the tangent and cotangent families, with any bound-index name. */
    for (unsigned alternating = 0u; alternating < 2u; ++alternating) {
        expr_t *expected = ft_sub(c, minus, plus);
        if (alternating)
            expected = ft_mul(c, ft_pow_xp(c, integer(c, -1), index), expected);
        expr_t *difference = keep(c, expr_beautify(ft_sub(c, symbolic, expected)));
        if (!expr_const_is_zero(difference))
            continue;
        if (!real_parameter(c, rate) || !positive(c, ft_abs(c, rate)))
            return NULL;
        expr_t *scale = keep(c, expr_beautify(ft_div(c, integer(c, 1), rate)));
        expr_t *expanded = keep(c, expr_expand_products_internal(ft_mul(c, w, scale)));
        expr_t *argument = keep(c, expr_beautify(expanded));
        expr_t *body = keep(c, alternating ? expr_tan(argument) : expr_cot(argument));
        expr_t *denominator = keep(c, alternating ? expr_cos(argument) : expr_sin(argument));
        if (!positive(c, ft_abs(c, denominator)))
            return NULL;
        expr_t *i = constant(c, NUM_I);
        if (c->inverse != (alternating == 0u))
            i = ft_neg(c, i);
        return ft_mul(c, ft_mul(c, i, body), ft_abs(c, scale));
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
