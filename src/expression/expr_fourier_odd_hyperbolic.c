#include "expr_fourier_internal.h"

typedef struct {
    expr_t *(*source)(const expr_t *);
    expr_t *(*dual)(const expr_t *);
    bool source_pole;
    bool target_pole;
} odd_hyperbolic_pair_t;

static const odd_hyperbolic_pair_t odd_hyperbolic_pairs[EXPR_KIND_COUNT] = {
    [EXPR_KIND_TANH]   = { expr_tanh,   expr_cosech, false, true  },
    [EXPR_KIND_COSECH] = { expr_cosech, expr_tanh,   true,  false },
    [EXPR_KIND_COTH]   = { expr_coth,   expr_coth,   true,  true  },
};

/* Odd hyperbolic pairs share their scale and phase factors, but not their source and target poles. */
static const expr_t *pair_argument(fourier_context_t *c, const expr_t *f,
                                  const odd_hyperbolic_pair_t **pair)
{
    *pair = &odd_hyperbolic_pairs[f->ops->kind];
    if ((*pair)->source)
        return f->a;
    if (f->ops == &ops_div && expr_struct_eq(f->a->a, f->b->a)) {
        if (f->a->ops == &ops_cosh && f->b->ops == &ops_sinh)
            *pair = &odd_hyperbolic_pairs[EXPR_KIND_COTH];
        else if (f->a->ops == &ops_sinh && f->b->ops == &ops_cosh)
            *pair = &odd_hyperbolic_pairs[EXPR_KIND_TANH];
        if ((*pair)->source)
            return f->a->a;
    }
    const expr_t *base = NULL, *power = NULL;
    if (f->ops == &ops_div && expr_const_is_one(f->a))
        base = f->b;
    else if (!match_power(c, f, &base, &power) || !expr_is_const(power) || !num_eq(power->c, NUM_NEG_ONE))
        return NULL;
    if (!base)
        return NULL;
    if (base->ops == &ops_sinh)
        *pair = &odd_hyperbolic_pairs[EXPR_KIND_COSECH];
    else if (base->ops == &ops_tanh)
        *pair = &odd_hyperbolic_pairs[EXPR_KIND_COTH];
    else if (base->ops == &ops_coth)
        *pair = &odd_hyperbolic_pairs[EXPR_KIND_TANH];
    return (*pair)->source ? base->a : NULL;
}

/* The full pairs are distributional; the returned representatives carry their numerical domains. */
expr_t *expr_fourier_odd_hyperbolic_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w)
{
    const odd_hyperbolic_pair_t *pair = NULL;
    const expr_t *argument = pair_argument(c, f, &pair);
    expr_t *rate = NULL, *offset = NULL;
    if (!argument || !affine(c, argument, x, &rate, &offset))
        return NULL;
    if (expr_const_is_zero(rate)) {
        if (!pair->source_pole && expr_const_is_zero(offset))
            return integer(c, 0);
        expr_t *body = keep(c, pair->source(offset));
        return ft_mul(c, ft_mul(c, integer(c, 2), pi_constant(c)), ft_mul(c, body, ft_delta(c, w)));
    }
    if (!real_parameter(c, rate) || !real_parameter(c, offset) || !positive(c, ft_abs(c, rate)))
        return NULL;
    if (pair->target_pole && !positive(c, ft_abs(c, w)))
        return NULL;
    expr_t *coordinate = clean(c, ft_div(c, ft_mul(c, pi_constant(c), w), ft_mul(c, integer(c, 2), rate)));
    expr_t *body = keep(c, pair->dual(coordinate));
    expr_t *i = constant(c, NUM_I);
    if (c->inverse)
        i = ft_neg(c, i);
    expr_t *phase = ft_exp(c, ft_div(c, ft_mul(c, ft_mul(c, i, w), offset), rate));
    expr_t *coefficient = ft_neg(c, ft_div(c, ft_mul(c, i, pi_constant(c)), ft_abs(c, rate)));
    return ft_mul(c, coefficient, ft_mul(c, phase, body));
}

/* Only a singular factor's own affine pole exclusion can be consumed by this transform rule. */
bool expr_fourier_odd_hyperbolic_pole_condition(fourier_context_t *c, const expr_t *f,
                                              const expr_t *x, const expr_t *condition)
{
    if (!f || !expr_is_op(condition, &ops_abs))
        return false;
    const odd_hyperbolic_pair_t *pair = NULL;
    const expr_t *argument = pair_argument(c, f, &pair);
    expr_t *rate = NULL, *offset = NULL, *guard_rate = NULL, *guard_offset = NULL;
    if (argument && pair->source_pole && affine(c, argument, x, &rate, &offset) && !expr_const_is_zero(rate) &&
        affine(c, condition->a, x, &guard_rate, &guard_offset) && !expr_const_is_zero(guard_rate)) {
        expr_t *difference = ft_sub(c, ft_mul(c, rate, guard_offset), ft_mul(c, offset, guard_rate));
        if (expr_const_is_zero(clean(c, difference)))
            return true;
    }
    /* Scalar extraction and modulation may surround the actual pair. Do not infer conditions through sums. */
    if (f->ops == &ops_neg)
        return expr_fourier_odd_hyperbolic_pole_condition(c, f->a, x, condition);
    if (f->ops == &ops_mul || f->ops == &ops_div)
        return expr_fourier_odd_hyperbolic_pole_condition(c, f->a, x, condition) ||
               expr_fourier_odd_hyperbolic_pole_condition(c, f->b, x, condition);
    return false;
}

/* Keep interpretation notes outside the mathematical expression and its domain predicates. */
const char *expr_fourier_odd_hyperbolic_note(const expr_t *transform)
{
    expr_t *specialised = expr_transform_bound_constants(transform);
    if (specialised)
        transform = specialised;
    fourier_context_t c = {.inverse = transform->ops == &ops_inverse_fourier};
    const odd_hyperbolic_pair_t *pair = NULL;
    const char *note = NULL;
    const expr_t *argument = pair_argument(&c, transform->a, &pair);
    expr_t *rate = NULL, *offset = NULL;
    if (argument && affine(&c, argument, transform->b->a, &rate, &offset) && !expr_const_is_zero(rate) &&
        expr_fourier_odd_hyperbolic_pair(&c, transform->a, transform->b->a, transform->b->b->a) && !c.failed)
        note = pair->source_pole && pair->target_pole
                   ? "This Fourier pair uses symmetric cancellation at the source and frequency poles. "
                     "The returned numerical function excludes its pole through the displayed domain condition."
               : pair->source_pole
                   ? "The transform uses symmetric cancellation at the source pole. "
                     "The returned function is evaluated on its displayed real domain."
                   : "This Fourier pair is distributional. The displayed frequency function excludes zero; "
                     "its inverse uses symmetric cancellation at zero.";
    expr_free(c.conditions);
    for (size_t n = 0u; n < c.count; ++n)
        expr_free(c.nodes[n]);
    free(c.nodes);
    expr_free(specialised);
    return note;
}
