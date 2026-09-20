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
static expr_t *keep(fourier_context_t *c, expr_t *expr)
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
    static expr_t *ft_##name(fourier_context_t *c, const expr_t *a) { return keep(c, expr_##name(a)); }
#define BINARY_HELPER(name)                                                                                             \
    static expr_t *ft_##name(fourier_context_t *c, const expr_t *a, const expr_t *b) { return keep(c, expr_##name(a, b)); }
UNARY_HELPER(neg)
UNARY_HELPER(exp)
UNARY_HELPER(sqrt)
UNARY_HELPER(sech)
UNARY_HELPER(conj)
UNARY_HELPER(rect)
UNARY_HELPER(tri)
UNARY_HELPER(sinc)
UNARY_HELPER(delta)
UNARY_HELPER(step)
UNARY_HELPER(principal_value)
BINARY_HELPER(add)
BINARY_HELPER(sub)
BINARY_HELPER(mul)
BINARY_HELPER(div)
BINARY_HELPER(pow_xp)
BINARY_HELPER(chebyshev_t)
BINARY_HELPER(hermite_h)
#undef BINARY_HELPER
#undef UNARY_HELPER

static expr_t *integer(fourier_context_t *c, long n) { return keep(c, expr_const_long(n)); }
static expr_t *constant(fourier_context_t *c, number_t n) { return keep(c, expr_new_const(n)); }
static expr_t *clean(fourier_context_t *c, const expr_t *e) { return keep(c, expr_simplify(e)); }

static bool match_power(fourier_context_t *c, const expr_t *f, const expr_t **base, const expr_t **power)
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

static bool uses(const expr_t *e, const expr_t *x)
{
    bool used = true;
    expr_t *variable = (expr_t *)x;
    return !expr_collect_var_usage(e, 1u, &variable, &used) || used;
}

static expr_t *ft_abs(fourier_context_t *c, const expr_t *e)
{
    if (expr_is_const(e) && !e->name) {
        number_t value = num_abs(e->c);
        expr_t *out = constant(c, value);
        num_destroy(&value);
        return out;
    }
    return keep(c, expr_abs(e));
}

static expr_t *fresh_variable(fourier_context_t *c, const expr_t *f, const expr_t *target)
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

static const expr_t *absolute_source(const expr_t *f, const expr_t *x)
{
    if (!f)
        return NULL;
    if (f->ops == &ops_abs && expr_struct_eq(f->a, x))
        return f;
    const expr_t *left = absolute_source(f->a, x);
    return left ? left : absolute_source(f->b, x);
}

static bool literal_value(const expr_t *e, number_t *value)
{
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(e);
    bool literal = expr_bindings_count(bindings) == 0u;
    expr_bindings_free(bindings);
    if (literal)
        *value = expr_eval(e);
    return literal && num_is_finite(*value);
}

static bool positive(fourier_context_t *c, const expr_t *value)
{
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

static bool real_parameter(fourier_context_t *c, const expr_t *value)
{
    expr_t *test = keep(c, expr_new_unary_internal(&ops_real_parameter, expr_clone(value)));
    return positive(c, test);
}

static expr_t *replace(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *value)
{
    return clean(c, keep(c, expr_substitute(f, x, value)));
}

/* Collect affine coefficients structurally; do not differentiate or sample free parameters. */
static bool affine(fourier_context_t *c, const expr_t *f, const expr_t *x, expr_t **a, expr_t **b)
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

static bool quadratic(fourier_context_t *c, const expr_t *f, const expr_t *x,
                      expr_t **a, expr_t **b, expr_t **d)
{
    expr_t *derivative = keep(c, expr_create_deriv(f, x));
    expr_t *twice = NULL;
    if (!derivative || !affine(c, derivative, x, &twice, b))
        return false;
    *a = clean(c, ft_div(c, twice, integer(c, 2)));
    *d = replace(c, f, x, integer(c, 0));
    expr_t *polynomial = ft_add(c, ft_add(c, ft_mul(c, *a, ft_mul(c, x, x)), ft_mul(c, *b, x)), *d);
    expr_t *check = clean(c, ft_sub(c, f, polynomial));
    return *d && !uses(*d, x) && expr_const_is_zero(check);
}

static expr_t *formula(fourier_context_t *, const expr_t *, const expr_t *, const expr_t *, unsigned);

static expr_t *formal(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w)
{
    expr_t *args[] = {(expr_t *)f, (expr_t *)x, (expr_t *)w};
    expr_t *out = keep(c, c->inverse ? expr_inverse_fourier_from_args(3u, args)
                                    : expr_fourier_from_args(3u, args));
    return c->inverse ? ft_mul(c, ft_mul(c, integer(c, 2), constant(c, NUM_PI)), out) : out;
}

static expr_t *subformula(fourier_context_t *c, const expr_t *f, const expr_t *x,
                          const expr_t *w, unsigned depth)
{
    /* Discard conditions from an unsuccessful rule, not conditions established by its siblings. */
    expr_t *saved = expr_clone(c->conditions);
    expr_t *out = formula(c, f, x, w, depth + 1u);
    if (!out) {
        expr_free(c->conditions);
        c->conditions = saved;
        return formal(c, f, x, w);
    }
    expr_free(saved);
    return out;
}

static const expr_t *exponent(const expr_t *f)
{
    if (f->ops == &ops_exp)
        return f->a;
    const expr_t *base = NULL, *power = NULL;
    return expr_match_pow_expr(f, &base, &power) && expr_is_const(base) && num_eq(base->c, NUM_E) ? power : NULL;
}

static expr_t *differentiate(fourier_context_t *c, const expr_t *f, const expr_t *w, unsigned order)
{
    expr_t *out = keep(c, expr_clone(f));
    for (unsigned i = 0u; out && i < order; ++i)
        out = clean(c, keep(c, expr_create_deriv(out, w)));
    return out;
}

static const expr_t *time_power(fourier_context_t *c, const expr_t *f, const expr_t *x, long *order)
{
    if (f->ops == &ops_mul) {
        const expr_t *left = time_power(c, f->a, x, order);
        return left ? left : time_power(c, f->b, x, order);
    }
    if (expr_struct_eq(f, x)) {
        *order = 1;
        return f;
    }
    const expr_t *base = NULL, *power = NULL;
    if (match_power(c, f, &base, &power) && expr_struct_eq(base, x) && expr_is_const(power) &&
        !power->name && num_is_integer(power->c) && num_ge(power->c, NUM_ZERO) && num_to_double(power->c) <= 32.0) {
        *order = (long)num_to_double(power->c);
        return f;
    }
    return NULL;
}

static expr_t *remove_factor(fourier_context_t *c, const expr_t *f, const expr_t *factor, bool *removed)
{
    if (!*removed && f == factor) {
        *removed = true;
        return integer(c, 1);
    }
    if (f->ops == &ops_mul) {
        expr_t *left = remove_factor(c, f->a, factor, removed);
        expr_t *right = remove_factor(c, f->b, factor, removed);
        return clean(c, ft_mul(c, left, right));
    }
    return keep(c, expr_clone(f));
}

static expr_t *formula(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w, unsigned depth)
{
    if (!f || depth > 24u || c->failed)
        return NULL;
    expr_t *one = integer(c, 1), *two = integer(c, 2), *pi = constant(c, NUM_PI), *i = constant(c, NUM_I);
    if (c->inverse)
        i = clean(c, ft_neg(c, i));
    expr_t *two_pi = ft_mul(c, two, pi);
    if (expr_const_is_zero(f))
        return integer(c, 0);
    if (f->ops == &ops_convolution && expr_struct_eq(f->b, x)) {
        expr_t *left = subformula(c, f->a->a, x, w, depth);
        expr_t *right = subformula(c, f->a->b, x, w, depth);
        return ft_mul(c, left, right);
    }
    if (f->ops == &ops_real_domain) {
        for (const expr_t *pair = f->b; pair; pair = pair->b->b) {
            if (pair->a->ops == &ops_real_parameter && expr_struct_eq(pair->a->a, x))
                continue;
            if (uses(pair->a, x) || uses(pair->b->a, x))
                return NULL;
            expr_t *copy = expr_alloc(&ops_argument_list);
            copy->a = expr_clone(pair->a);
            copy->b = expr_alloc(&ops_argument_list);
            copy->b->a = expr_clone(pair->b->a);
            copy->b->b = c->conditions;
            c->conditions = copy;
        }
        return subformula(c, f->a, x, w, depth);
    }
    if ((f->ops == &ops_fourier || f->ops == &ops_inverse_fourier) && expr_struct_eq(f->b->b->a, x)) {
        bool reflect = (f->ops == &ops_fourier) != c->inverse;
        expr_t *body = replace(c, f->a, f->b->a, reflect ? ft_neg(c, w) : w);
        return f->ops == &ops_fourier ? ft_mul(c, two_pi, body) : body;
    }
    if (f->ops == &ops_principal_value) {
        expr_t *coefficient = clean(c, ft_mul(c, f->a, x));
        if (coefficient && !uses(coefficient, x)) {
            expr_t *sign = ft_sub(c, ft_mul(c, two, ft_step(c, w)), one);
            return ft_mul(c, ft_neg(c, ft_mul(c, i, pi)), ft_mul(c, coefficient, sign));
        }
        return NULL;
    }
    if (!uses(f, x))
        return ft_mul(c, ft_mul(c, two_pi, f), ft_delta(c, w));
    if (f->ops == &ops_add || f->ops == &ops_sub) {
        expr_t *left = subformula(c, f->a, x, w, depth);
        expr_t *right = subformula(c, f->b, x, w, depth);
        return f->ops == &ops_add ? ft_add(c, left, right) : ft_sub(c, left, right);
    }
    if (f->ops == &ops_neg)
        return ft_neg(c, subformula(c, f->a, x, w, depth));
    if (f->ops == &ops_mul) {
        if (!uses(f->a, x))
            return ft_mul(c, f->a, subformula(c, f->b, x, w, depth));
        if (!uses(f->b, x))
            return ft_mul(c, f->b, subformula(c, f->a, x, w, depth));
    }
    if (f->ops == &ops_div && !uses(f->b, x))
        return ft_div(c, subformula(c, f->a, x, w, depth), f->b);
    if (f->ops == &ops_conj)
        return ft_conj(c, subformula(c, f->a, x, ft_neg(c, w), depth));
    /* Recognise the compact Chebyshev spectrum as an actual Bessel pair, not only by nested duality. */
    if (f->ops == &ops_div && (f->a->ops == &ops_mul || f->a->ops == &ops_rect)) {
        const expr_t *numerator = f->a;
        const expr_t *pair[2] = {numerator->ops == &ops_mul ? numerator->a : one,
                                 numerator->ops == &ops_mul ? numerator->b : numerator};
        for (unsigned side = 0u; side < 2u; ++side) {
            const expr_t *poly = pair[side], *window = pair[!side];
            if (window->ops != &ops_rect || (poly->ops != &ops_chebyshev_t && !expr_const_is_one(poly)))
                continue;
            const expr_t *n = poly->ops == &ops_chebyshev_t ? poly->a : integer(c, 0);
            expr_t *coordinate = poly->ops == &ops_chebyshev_t ? keep(c, expr_clone(poly->b))
                                                               : ft_mul(c, two, window->a);
            expr_t *window_check = clean(c, ft_sub(c, window->a, ft_div(c, coordinate, two)));
            expr_t *denominator_check = clean(c, ft_sub(c, f->b,
                                                      ft_sqrt(c, ft_sub(c, one, ft_mul(c, coordinate, coordinate)))));
            expr_t *rate = NULL, *offset = NULL;
            if (!expr_const_is_zero(window_check) || !expr_const_is_zero(denominator_check) || uses(n, x) ||
                !affine(c, coordinate, x, &rate, &offset) || expr_const_is_zero(rate) ||
                !real_parameter(c, rate) || !real_parameter(c, offset) || !positive(c, ft_abs(c, rate)))
                continue;
            expr_t *order = keep(c, expr_new_unary_internal(&ops_nonnegative_integer, expr_clone(n)));
            if (!positive(c, order))
                return NULL;
            expr_t *frequency = ft_div(c, w, rate);
            expr_t *phase = ft_exp(c, ft_mul(c, ft_mul(c, i, frequency), offset));
            expr_t *bessel = keep(c, expr_bessel_j(n, frequency));
            expr_t *coefficient = ft_mul(c, pi, ft_pow_xp(c, ft_neg(c, i), n));
            return ft_div(c, ft_mul(c, coefficient, ft_mul(c, phase, bessel)), ft_abs(c, rate));
        }
    }
    if (f->ops == &ops_mul) {
        const expr_t *pair[2] = {f->a, f->b};
        for (unsigned side = 0u; side < 2u; ++side) {
            const expr_t *h = pair[side], *phase = exponent(pair[!side]);
            if (h->ops != &ops_hermite_h || !phase || uses(h->a, x))
                continue;
            expr_t *rate = NULL, *offset = NULL;
            if (!affine(c, h->b, x, &rate, &offset))
                continue;
            expr_t *residual = clean(c, ft_add(c, phase, ft_div(c, ft_mul(c, h->b, h->b), two)));
            if (!residual || uses(residual, x) || expr_const_is_zero(rate) ||
                !real_parameter(c, rate) || !real_parameter(c, offset) || !positive(c, ft_abs(c, rate)))
                continue;
            expr_t *order = keep(c, expr_new_unary_internal(&ops_nonnegative_integer, expr_clone(h->a)));
            if (!positive(c, order))
                return NULL;
            expr_t *coordinate = ft_div(c, w, rate);
            expr_t *rotation = ft_mul(c, ft_mul(c, i, coordinate), offset);
            expr_t *gaussian = ft_exp(c, ft_sub(c, ft_add(c, residual, rotation),
                                               ft_div(c, ft_mul(c, coordinate, coordinate), two)));
            expr_t *coefficient = ft_div(c, ft_mul(c, ft_sqrt(c, two_pi),
                                                   ft_pow_xp(c, ft_neg(c, i), h->a)), ft_abs(c, rate));
            return ft_mul(c, coefficient, ft_mul(c, gaussian, ft_hermite_h(c, h->a, coordinate)));
        }
    }
    /* DLMF 10.9.2: integer-order Bessel functions have a compactly supported spectrum. */
    if (f->ops == &ops_bessel_j && !uses(f->a, x)) {
        expr_t *rate = NULL, *offset = NULL;
        if (!affine(c, f->b, x, &rate, &offset) || expr_const_is_zero(rate) ||
            !real_parameter(c, rate) || !real_parameter(c, offset) || !positive(c, ft_abs(c, rate)) ||
            !real_parameter(c, f->a))
            return NULL;
        expr_t *integral_order = keep(c, expr_new_unary_internal(&ops_nonnegative_integer, expr_clone(ft_abs(c, f->a))));
        if (!positive(c, integral_order))
            return NULL;
        expr_t *coordinate = clean(c, ft_div(c, w, rate));
        expr_t *radicand = ft_sub(c, one, ft_pow_xp(c, coordinate, two));
        if (!positive(c, ft_abs(c, radicand)))
            return NULL;
        expr_t *polynomial = ft_chebyshev_t(c, ft_abs(c, f->a), coordinate);
        expr_t *window = ft_rect(c, ft_div(c, coordinate, two));
        expr_t *spectrum = ft_div(c, ft_mul(c, window, polynomial), ft_sqrt(c, radicand));
        expr_t *phase = ft_exp(c, ft_mul(c, ft_mul(c, i, coordinate), offset));
        expr_t *coefficient = ft_mul(c, two, ft_pow_xp(c, ft_neg(c, i), f->a));
        return ft_div(c, ft_mul(c, coefficient, ft_mul(c, phase, spectrum)), ft_abs(c, rate));
    }
    if (f->ops == &ops_ordered_derivative &&
        (expr_is_arbitrary_function(f->a) || f->a->ops == &ops_delta) &&
        expr_struct_eq(f->a->a, x) && !uses(f->b, x)) {
        expr_t *order = keep(c, expr_new_unary_internal(&ops_nonnegative_integer, expr_clone(f->b)));
        if (!positive(c, order))
            return NULL;
        expr_t *body = subformula(c, f->a, x, w, depth);
        return ft_mul(c, ft_pow_xp(c, ft_mul(c, i, w), f->b), body);
    }
    if (expr_is_formal_derivative(f)) {
        size_t order = expr_formal_derivative_order(f);
        for (size_t n = 0u; n < order; ++n)
            if (!expr_struct_eq(expr_formal_derivative_wrt_at(f, n), x))
                return NULL;
        expr_t *body = subformula(c, expr_formal_derivative_dependent(f), x, w, depth);
        return ft_mul(c, ft_pow_xp(c, ft_mul(c, i, w), integer(c, (long)order)), body);
    }
    /* Multiplication by a non-negative integral time power becomes a frequency derivative. */
    const expr_t *factors[2] = {f->ops == &ops_mul ? f->a : f, f->ops == &ops_mul ? f->b : NULL};
    long order = 0;
    const expr_t *factor = time_power(c, f, x, &order);
    if (factor && expr_is_var(w)) {
        bool removed = false;
        expr_t *rest = remove_factor(c, f, factor, &removed);
        expr_t *body = subformula(c, rest, x, w, depth);
        expr_t *derivative = differentiate(c, body, w, (unsigned)order);
        return derivative ? ft_mul(c, ft_pow_xp(c, i, integer(c, order)), derivative) : NULL;
    }
    const expr_t *monomial_base = NULL, *monomial_order = NULL;
    if (match_power(c, f, &monomial_base, &monomial_order) && expr_struct_eq(monomial_base, x) &&
        !uses(monomial_order, x)) {
        expr_t *integer_order = keep(c, expr_new_unary_internal(&ops_nonnegative_integer, expr_clone(monomial_order)));
        if (!positive(c, integer_order))
            return NULL;
        expr_t *impulse = ft_delta(c, w);
        expr_t *derivative = keep(c, expr_new_ordered_derivative(impulse, monomial_order));
        return derivative ? ft_mul(c, ft_mul(c, two_pi, ft_pow_xp(c, i, monomial_order)), derivative) : NULL;
    }
    const expr_t *arg = exponent(f);
    if (arg) {
        expr_t *a = NULL, *b = NULL, *d = NULL;
        if (quadratic(c, arg, x, &a, &b, &d) && !expr_const_is_zero(a)) {
            expr_t *decay = clean(c, ft_neg(c, a));
            if (!positive(c, decay))
                return NULL;
            expr_t *shift = ft_add(c, w, ft_mul(c, i, b));
            expr_t *phase = ft_sub(c, d, ft_div(c, ft_mul(c, shift, shift), ft_mul(c, integer(c, 4), decay)));
            return ft_mul(c, ft_sqrt(c, ft_div(c, pi, decay)), ft_exp(c, phase));
        }
        if (affine(c, arg, x, &a, &b)) {
            expr_t *frequency = clean(c, ft_neg(c, ft_mul(c, i, a)));
            if (!real_parameter(c, frequency))
                return NULL;
            return ft_mul(c, ft_mul(c, two_pi, ft_exp(c, b)), ft_delta(c, ft_sub(c, w, frequency)));
        }
        /* The two-sided exponential has an ordinary transform, not a one-sided Laplace value. */
        const expr_t *absolute = absolute_source(arg, x);
        expr_t *v = fresh_variable(c, arg, w);
        expr_t *rewritten = absolute ? replace(c, arg, absolute, v) : NULL;
        if (rewritten && affine(c, rewritten, v, &a, &b) && !uses(a, x) && !uses(b, x)) {
            expr_t *decay = clean(c, ft_neg(c, a));
            if (!positive(c, decay))
                return NULL;
            return ft_mul(c, ft_exp(c, b), ft_div(c, ft_mul(c, two, decay), ft_add(c, ft_mul(c, decay, decay), ft_mul(c, w, w))));
        }
    }
    if (f->ops == &ops_mul) {
        for (unsigned side = 0u; side < 2u; ++side) {
            const expr_t *phase = exponent(factors[side]);
            expr_t *a = NULL, *b = NULL;
            const expr_t *gate = factors[!side];
            expr_t *slope = NULL, *offset = NULL;
            if (phase && gate->ops == &ops_step && affine(c, phase, x, &a, &b) &&
                affine(c, gate->a, x, &slope, &offset) && !expr_const_is_zero(slope) &&
                real_parameter(c, slope) && real_parameter(c, offset) && positive(c, ft_abs(c, slope))) {
                expr_t *direction = clean(c, ft_div(c, slope, ft_abs(c, slope)));
                if (!positive(c, ft_neg(c, ft_mul(c, direction, a))))
                    return NULL;
                expr_t *edge = ft_neg(c, ft_div(c, offset, slope));
                expr_t *pole = ft_sub(c, ft_mul(c, i, w), a);
                expr_t *boundary = ft_exp(c, ft_sub(c, b, ft_mul(c, pole, edge)));
                return ft_div(c, ft_mul(c, direction, boundary), pole);
            }
            if (phase && affine(c, phase, x, &a, &b) && real_parameter(c, ft_neg(c, ft_mul(c, i, a)))) {
                expr_t *shift = clean(c, ft_add(c, w, ft_mul(c, i, a)));
                return ft_mul(c, ft_exp(c, b), subformula(c, factors[!side], x, shift, depth));
            }
        }
    }
    if (f->ops == &ops_sin || f->ops == &ops_cos) {
        expr_t *rate = NULL, *phase = NULL;
        if (affine(c, f->a, x, &rate, &phase) && real_parameter(c, rate)) {
            expr_t *rotation = ft_mul(c, i, phase);
            expr_t *left = ft_mul(c, ft_exp(c, rotation), ft_delta(c, ft_sub(c, w, rate)));
            expr_t *right = ft_mul(c, ft_exp(c, ft_neg(c, rotation)), ft_delta(c, ft_add(c, w, rate)));
            return f->ops == &ops_cos ? ft_mul(c, pi, ft_add(c, left, right))
                                     : ft_mul(c, ft_neg(c, ft_mul(c, i, pi)), ft_sub(c, left, right));
        }
    }
    /* A shifted quadratic denominator is dual to a two-sided decaying exponential. */
    const expr_t *denominator = f->ops == &ops_div && !uses(f->a, x) ? f->b : NULL;
    const expr_t *reciprocal_base = NULL, *reciprocal_power = NULL;
    if (!denominator && match_power(c, f, &reciprocal_base, &reciprocal_power) &&
        expr_is_const(reciprocal_power) && num_eq(reciprocal_power->c, NUM_NEG_ONE))
        denominator = reciprocal_base;
    if (denominator) {
        expr_t *a = NULL, *b = NULL, *d = NULL;
        if (affine(c, denominator, x, &a, &b) && !expr_const_is_zero(a)) {
            expr_t *rate = clean(c, ft_neg(c, ft_mul(c, i, a)));
            expr_t *amplitude = f->ops == &ops_div ? (expr_t *)f->a : one;
            if (!real_parameter(c, rate) || !positive(c, ft_abs(c, rate)))
                return NULL;
            if (!positive(c, b)) {
                b = clean(c, ft_neg(c, b));
                rate = clean(c, ft_neg(c, rate));
                amplitude = ft_neg(c, amplitude);
                if (!positive(c, b))
                    return NULL;
            }
            expr_t *coordinate = ft_div(c, w, rate);
            expr_t *envelope = ft_mul(c, ft_exp(c, ft_mul(c, b, coordinate)),
                                         ft_step(c, ft_neg(c, coordinate)));
            return ft_mul(c, ft_div(c, ft_mul(c, two_pi, amplitude), ft_abs(c, rate)), envelope);
        }
        if (quadratic(c, denominator, x, &a, &b, &d) && !expr_const_is_zero(a)) {
            expr_t *centre = clean(c, ft_neg(c, ft_div(c, b, ft_mul(c, two, a))));
            expr_t *width_squared = clean(c, ft_sub(c, ft_div(c, d, a), ft_mul(c, centre, centre)));
            expr_t *width = clean(c, ft_sqrt(c, width_squared));
            if (!real_parameter(c, centre) || !positive(c, width))
                return NULL;
            expr_t *phase = ft_sub(c, ft_neg(c, ft_mul(c, width, ft_abs(c, w))),
                                   ft_mul(c, ft_mul(c, i, w), centre));
            expr_t *numerator = f->ops == &ops_div ? ft_mul(c, pi, f->a) : pi;
            return ft_mul(c, ft_div(c, numerator, ft_mul(c, a, width)), ft_exp(c, phase));
        }
    }
    if (f->a && expr_struct_eq(f->a, x)) {
        expr_t *half_frequency = ft_div(c, w, two_pi);
        if (f->ops == &ops_delta)
            return one;
        if (f->ops == &ops_step)
            return ft_add(c, ft_mul(c, pi, ft_delta(c, w)), ft_principal_value(c, ft_div(c, one, ft_mul(c, i, w))));
        if (f->ops == &ops_rect)
            return ft_sinc(c, half_frequency);
        if (f->ops == &ops_tri)
            return ft_pow_xp(c, ft_sinc(c, half_frequency), two);
        if (f->ops == &ops_circ)
            return ft_mul(c, two, ft_sinc(c, ft_div(c, w, pi)));
        if (f->ops == &ops_sinc)
            return ft_rect(c, half_frequency);
        if (f->ops == &ops_sech)
            return ft_mul(c, pi, ft_sech(c, ft_div(c, ft_mul(c, pi, w), two)));
        if (f->ops == &ops_cos || f->ops == &ops_sin) {
            expr_t *left = ft_delta(c, ft_sub(c, w, one)), *right = ft_delta(c, ft_add(c, w, one));
            return f->ops == &ops_cos ? ft_mul(c, pi, ft_add(c, left, right))
                                     : ft_mul(c, ft_neg(c, ft_mul(c, i, pi)), ft_sub(c, left, right));
        }
    }
    const expr_t *base = NULL, *power = NULL;
    if (match_power(c, f, &base, &power) && base->ops == &ops_sinc &&
        expr_struct_eq(base->a, x) && expr_is_const(power) && num_eq(power->c, NUM_TWO))
        return ft_tri(c, ft_div(c, w, two_pi));
    /* Affine changes also apply inside a source-independent power of a unary function. */
    const expr_t *unary = f;
    bool powered = match_power(c, f, &base, &power) && !uses(power, x) && base->a &&
                   (base->ops->arity == EXPR_OP_UNARY || expr_is_arbitrary_function(base));
    if (powered)
        unary = base;
    if (unary->a && (unary->ops->arity == EXPR_OP_UNARY || expr_is_arbitrary_function(unary)) &&
        !expr_struct_eq(unary->a, x) && !expr_is_integral_transform(unary)) {
        expr_t *a = NULL, *b = NULL;
        if (affine(c, unary->a, x, &a, &b) && !expr_const_is_zero(a) &&
            real_parameter(c, a) && real_parameter(c, b) && positive(c, ft_abs(c, a))) {
            expr_t *unit = keep(c, expr_clone(f));
            expr_t *unit_unary = powered ? unit->a : unit;
            expr_free(unit_unary->a);
            unit_unary->a = expr_clone(x);
            unit_unary->simplified = false;
            unit_unary->simplify_epoch = 0u;
            unit->simplified = false;
            unit->simplify_epoch = 0u;
            expr_t *scaled_frequency = clean(c, ft_div(c, w, a));
            expr_t *body = subformula(c, unit, x, scaled_frequency, depth);
            expr_t *phase = ft_exp(c, ft_div(c, ft_mul(c, ft_mul(c, i, w), b), a));
            return ft_div(c, ft_mul(c, phase, body), ft_abs(c, a));
        }
    }
    if (f->ops == &ops_mul && expr_is_var(w)) {
        expr_t *left = subformula(c, f->a, x, w, depth);
        expr_t *right = subformula(c, f->b, x, w, depth);
        if (c->inverse) {
            left = clean(c, ft_div(c, left, two_pi));
            right = clean(c, ft_div(c, right, two_pi));
        }
        expr_t *convolution = keep(c, expr_convolve(left, right, w));
        return c->inverse ? ft_mul(c, two_pi, convolution) : ft_div(c, convolution, two_pi);
    }
    return NULL;
}

static bool contains_formal_derivative(const expr_t *expr)
{
    return expr && (expr_is_formal_derivative(expr) || contains_formal_derivative(expr->a) ||
                    contains_formal_derivative(expr->b));
}

/* Both directions share rules; inverse kernels reverse i and include their normalisation exactly once. */
expr_t *expr_fourier_result(const expr_t *transform)
{
    if (!transform || (transform->ops != &ops_fourier && transform->ops != &ops_inverse_fourier))
        return NULL;
    fourier_context_t c = {.inverse = transform->ops == &ops_inverse_fourier};
    const expr_t *source = transform->b->a, *target = transform->b->b->a;
    expr_t *frequency = expr_is_var(target) ? keep(&c, expr_clone(target)) : fresh_variable(&c, transform->a, target);
    expr_t *out = formula(&c, transform->a, source, frequency, 0u);
    if (out) {
        const expr_t *argument = target;
        if (c.inverse) {
            out = ft_div(&c, out, ft_mul(&c, integer(&c, 2), constant(&c, NUM_PI)));
        }
        bool depends_on_frequency = uses(out, frequency);
        if (expr_is_var(target))
            out = clean(&c, out);
        else
            out = contains_formal_derivative(out) ? NULL : replace(&c, out, frequency, argument);
        if (out && depends_on_frequency && !real_parameter(&c, target))
            out = constant(&c, NUM_NAN);
        if (out && c.conditions) {
            expr_t *domain = keep(&c, expr_alloc(&ops_real_domain));
            domain->a = expr_clone(out);
            domain->b = keep(&c, expr_substitute(c.conditions, frequency, argument));
            expr_retain(domain->b);
            out = domain;
        }
    }
    expr_t *result = out && !c.failed ? expr_clone(out) : NULL;
    expr_free(c.conditions);
    for (size_t n = 0u; n < c.count; ++n)
        expr_free(c.nodes[n]);
    free(c.nodes);
    return result;
}

static number_t fourier_eval(expr_t *expr)
{
    expr_t *formula = expr_fourier_result(expr);
    number_t out = formula ? expr_eval(formula) : num_clone(NUM_NAN);
    expr_free(formula);
    return out;
}

static expr_t *fourier_simplify(const expr_t *expr, expr_t *a, expr_t *b)
{
    expr_t *out = expr_fourier_result(expr);
    if (!out)
        return expr_simplify_passthrough(expr, a, b);
    expr_free(a);
    expr_free(b);
    return out;
}

static expr_t *fourier_deriv(expr_t *expr)
{
    expr_t *wrt = (expr_t *)expr_current_wrt_internal();
    return wrt ? expr_new_formal_derivative(expr, 1u, &wrt) : NULL;
}

const expr_ops_t ops_fourier = {
    .eval = fourier_eval, .deriv = fourier_deriv, .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_FOURIER, .arity = EXPR_OP_BINARY, .expression_name = "ℱ", .function_name = "Fourier",
    .TeX_name = "\\mathcal{F}", .simplify = fourier_simplify,
};
const expr_ops_t ops_inverse_fourier = {
    .eval = fourier_eval, .deriv = fourier_deriv, .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_INVERSE_FOURIER, .arity = EXPR_OP_BINARY, .expression_name = "ℱ⁻¹", .function_name = "InverseFourier",
    .TeX_name = "\\mathcal{F}^{-1}", .simplify = fourier_simplify,
};
