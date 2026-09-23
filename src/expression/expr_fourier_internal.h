#ifndef MARS_EXPR_FOURIER_INTERNAL_H
#define MARS_EXPR_FOURIER_INTERNAL_H

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    expr_t **nodes;
    size_t count, capacity;
    expr_t *conditions;
    bool failed;
    bool inverse;
} fourier_context_t;

/* A local owning arena makes temporary formula construction explicit and exception-safe. */
static inline expr_t *keep(fourier_context_t *c, expr_t *expr)
{
    if (!expr)
        return NULL;
    if (c->count == c->capacity) {
        size_t capacity = c->capacity ? c->capacity * 2u : 64u;
        expr_t **nodes = realloc(c->nodes, capacity * sizeof(*nodes));
        if (!nodes) {
            expr_free(expr);
            c->failed = true;
            return NULL;
        }
        c->nodes = nodes;
        c->capacity = capacity;
    }
    c->nodes[c->count++] = expr;
    return expr;
}

#define UNARY_HELPER(name)                                                                                              \
    static inline expr_t *ft_##name(fourier_context_t *c, const expr_t *a) { return keep(c, expr_##name(a)); }
#define BINARY_HELPER(name)                                                                                             \
    static inline expr_t *ft_##name(fourier_context_t *c, const expr_t *a, const expr_t *b) { return keep(c, expr_##name(a, b)); }
UNARY_HELPER(neg)
UNARY_HELPER(exp)
UNARY_HELPER(sqrt)
UNARY_HELPER(sech)
UNARY_HELPER(sinh)
UNARY_HELPER(cosh)
UNARY_HELPER(conj)
UNARY_HELPER(rect)
UNARY_HELPER(tri)
UNARY_HELPER(sinc)
UNARY_HELPER(delta)
UNARY_HELPER(step)
UNARY_HELPER(principal_value)
UNARY_HELPER(finite_part)
UNARY_HELPER(ln)
BINARY_HELPER(add)
BINARY_HELPER(sub)
BINARY_HELPER(mul)
BINARY_HELPER(div)
BINARY_HELPER(pow_xp)
BINARY_HELPER(chebyshev_t)
BINARY_HELPER(hermite_h)
BINARY_HELPER(beta)
#undef BINARY_HELPER
#undef UNARY_HELPER

static inline expr_t *integer(fourier_context_t *c, long n) { return keep(c, expr_const_long(n)); }
static inline expr_t *constant(fourier_context_t *c, number_t n) { return keep(c, expr_new_const(n)); }
static inline expr_t *pi_constant(fourier_context_t *c) { return keep(c, expr_new_named_const(NUM_PI, "@pi")); }
static inline expr_t *euler_constant(fourier_context_t *c)
{
    expr_t *out = keep(c, expr_new_named_const(NUM_EULER_MASCHERONI, "@gamma"));
    if (out)
        out->binding_expr = expr_binding_expr_new_const(EXPR_BINDING_CONST_GAMMA);
    return out;
}
static inline expr_t *clean(fourier_context_t *c, const expr_t *e) { return keep(c, expr_simplify(e)); }

/* Reify literal coefficient provenance without substituting any named parameter or variable binding. */
static inline void exact_literals_owned(expr_t **node)
{
    expr_t *f = *node;
    if (!f)
        return;
    if (expr_is_unnamed_const(f) && f->binding_expr && !expr_binding_expr_is_numeric_literal(f->binding_expr)) {
        expr_t *expanded = expr_expand_preserved_for_display(f);
        if (expanded) {
            *node = expanded;
            expr_free(f);
        }
        return;
    }
    exact_literals_owned(&f->a);
    exact_literals_owned(&f->b);
    f->simplified = false;
    f->simplify_epoch = 0u;
}

static inline expr_t *exact_literals(fourier_context_t *c, const expr_t *f)
{
    expr_t *out = expr_clone(f);
    exact_literals_owned(&out);
    return keep(c, out);
}

static inline bool match_power(fourier_context_t *c, const expr_t *f, const expr_t **base, const expr_t **power)
{
    if (expr_match_pow_expr(f, base, power))
        return true;
    if (f && f->ops == &ops_pow_d) {
        *base = f->a;
        *power = constant(c, f->c);
        return true;
    }
    return false;
}

static inline bool uses(const expr_t *e, const expr_t *x)
{
    bool used = true;
    expr_t *variable = (expr_t *)x;
    return !expr_collect_var_usage(e, 1u, &variable, &used) || used;
}

static inline bool literal_value(const expr_t *e, number_t *value);

static inline expr_t *ft_abs(fourier_context_t *c, const expr_t *e)
{
    while (e && e->ops == &ops_neg)
        e = e->a;
    if (expr_is_const(e) && !e->name) {
        number_t value = num_abs(e->c);
        expr_t *out = constant(c, value);
        num_destroy(&value);
        return out;
    }
    /* Retain exact named constants such as pi while removing a provable real sign.
     * A variable's supplied value must never determine the displayed algebra. */
    number_t value = NUM_NAN;
    bool known_real = literal_value(e, &value) && num_is_real(value);
    bool negative = known_real && num_lt(value, NUM_ZERO);
    num_destroy(&value);
    if (known_real)
        return negative ? clean(c, ft_neg(c, e)) : keep(c, expr_clone(e));
    return keep(c, expr_abs(e));
}

static inline expr_t *fresh_variable(fourier_context_t *c, const expr_t *f, const expr_t *target)
{
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(f);
    expr_bindings_t *targets = expr_bindings_from_expr_internal(target);
    char name[64];
    for (unsigned n = 0u;; ++n) {
        snprintf(name, sizeof(name), "_fourier_%u", n);
        if (!expr_bindings_get(bindings, name) && !expr_bindings_get(targets, name))
            break;
    }
    expr_t *out = keep(c, expr_new_named_var(NUM_NAN, name));
    expr_bindings_free(targets);
    expr_bindings_free(bindings);
    return out;
}

static inline const expr_t *absolute_source(const expr_t *f, const expr_t *x)
{
    if (!f)
        return NULL;
    if (f->ops == &ops_abs && expr_struct_eq(f->a, x))
        return f;
    const expr_t *left = absolute_source(f->a, x);
    return left ? left : absolute_source(f->b, x);
}

static inline bool literal_value(const expr_t *e, number_t *value)
{
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(e);
    bool literal = expr_bindings_count(bindings) == 0u;
    expr_bindings_free(bindings);
    if (literal)
        *value = expr_eval(e);
    return literal && num_is_finite(*value);
}

static inline bool positive(fourier_context_t *c, const expr_t *value)
{
    if (value && value->ops == &ops_real_bound)
        value = value->a;
    expr_t *bound = clean(c, value);
    number_t n = NUM_NAN;
    bool known = literal_value(bound, &n);
    number_t real = num_real_part(n);
    bool valid = !known || num_gt(real, NUM_ZERO);
    num_destroy(&real);
    num_destroy(&n);
    if (!valid)
        return false;
    if (!known) {
        expr_t *pair = expr_alloc(&ops_argument_list);
        pair->a = expr_clone(bound);
        pair->b = expr_alloc(&ops_argument_list);
        pair->b->a = expr_const_zero();
        pair->b->b = c->conditions;
        c->conditions = pair;
    }
    return true;
}

static inline bool real_parameter(fourier_context_t *c, const expr_t *value)
{
    value = clean(c, value);
    if (value && (value->ops == &ops_real_bound || value->ops == &ops_imag_coordinate))
        return true;
    while (value && value->ops == &ops_neg)
        value = value->a;
    expr_t *test = keep(c, expr_new_unary_internal(&ops_real_parameter, expr_clone(value)));
    return positive(c, test);
}

static inline expr_t *replace(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *value)
{
    return clean(c, keep(c, expr_substitute(f, x, value)));
}

/* Collect affine coefficients structurally; do not differentiate or sample free parameters. */
static inline bool affine(fourier_context_t *c, const expr_t *f, const expr_t *x, expr_t **a, expr_t **b)
{
    if (!uses(f, x)) {
        *a = integer(c, 0);
        *b = keep(c, expr_clone(f));
        return true;
    }
    if (expr_struct_eq(f, x)) {
        *a = integer(c, 1);
        *b = integer(c, 0);
        return true;
    }
    expr_t *left_a = NULL, *left_b = NULL, *right_a = NULL, *right_b = NULL;
    if ((f->ops == &ops_add || f->ops == &ops_sub) &&
        affine(c, f->a, x, &left_a, &left_b) && affine(c, f->b, x, &right_a, &right_b)) {
        *a = clean(c, f->ops == &ops_add ? ft_add(c, left_a, right_a) : ft_sub(c, left_a, right_a));
        *b = clean(c, f->ops == &ops_add ? ft_add(c, left_b, right_b) : ft_sub(c, left_b, right_b));
        return true;
    }
    if (f->ops == &ops_neg && affine(c, f->a, x, &left_a, &left_b)) {
        *a = clean(c, ft_neg(c, left_a));
        *b = clean(c, ft_neg(c, left_b));
        return true;
    }
    if (f->ops == &ops_mul) {
        const expr_t *coefficient = !uses(f->a, x) ? f->a : f->b;
        const expr_t *dependent = coefficient == f->a ? f->b : f->a;
        if (!uses(coefficient, x) && affine(c, dependent, x, &left_a, &left_b)) {
            *a = clean(c, ft_mul(c, coefficient, left_a));
            *b = clean(c, ft_mul(c, coefficient, left_b));
            return true;
        }
    }
    if (f->ops == &ops_div && !uses(f->b, x) && affine(c, f->a, x, &left_a, &left_b)) {
        *a = clean(c, ft_div(c, left_a, f->b));
        *b = clean(c, ft_div(c, left_b, f->b));
        return true;
    }
    return false;
}

static inline const expr_t *exponent(const expr_t *f)
{
    if (f->ops == &ops_exp)
        return f->a;
    const expr_t *base = NULL, *power = NULL;
    return expr_match_pow_expr(f, &base, &power) && expr_is_const(base) && num_eq(base->c, NUM_E) ? power : NULL;
}

/** Match a complete hyperbolic beta spectrum, returning an arena-owned unnormalised Fourier result. */
expr_t *expr_fourier_beta_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w);

/** Recognise a condition excluding exactly the ordinary representative's periodic poles. */
bool expr_fourier_periodic_pole_condition(const expr_t *body, const expr_t *condition);

/** Match periodic functions and their impulse series, before inverse normalisation. */
expr_t *expr_fourier_periodic_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w);

/** Match real affine odd hyperbolic Fourier pairs, before inverse normalisation. */
expr_t *expr_fourier_odd_hyperbolic_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w);

/** Recognise the affine pole exclusion of a cosech or coth spectrum. */
bool expr_fourier_odd_hyperbolic_pole_condition(fourier_context_t *c, const expr_t *f,
                                              const expr_t *x, const expr_t *condition);

/** Describe the singular-integral interpretation of a recognised odd hyperbolic pair. */
const char *expr_fourier_odd_hyperbolic_note(const expr_t *transform);

/** Match real affine arctangents and their exponentially damped reciprocal spectra. */
expr_t *expr_fourier_atan_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w);

/** Recognise the pole exclusion of a matched arctangent spectrum. */
bool expr_fourier_atan_pole_condition(fourier_context_t *c, const expr_t *f,
                                     const expr_t *x, const expr_t *condition);

/** Describe the distributional interpretation of a recognised arctangent transform. */
const char *expr_fourier_atan_note(const expr_t *transform);

/** Match inverse trigonometric and hyperbolic boundary-value pairs, before inverse normalisation. */
expr_t *expr_fourier_branch_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w);

/** Recognise precisely the real endpoint exclusions of an atanh argument. */
bool expr_fourier_branch_pole_condition(fourier_context_t *c, const expr_t *f, const expr_t *condition);

/** Match vertical-line gamma transforms and their exponential spectra. */
expr_t *expr_fourier_gamma_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w);
/** Recombine Cartesian components in recovered gamma arguments after a vertical-line inverse. */
expr_t *expr_fourier_gamma_cartesian_result(fourier_context_t *c, const expr_t *result);

/** Explain non-existence for a real affine gamma argument. */
const char *expr_fourier_gamma_note(const expr_t *transform);

/** Collect scalar factors and the combined exponent of a product or quotient of exponentials. */
bool expr_fourier_exponential_factors(fourier_context_t *c, const expr_t *f, const expr_t *x,
                                       expr_t **phase, expr_t **scale);

#endif
