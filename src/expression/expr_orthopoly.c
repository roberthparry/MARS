#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#include <stdio.h>

typedef number_t (*polynomial_numeric_fn)(number_t, number_t);
static const polynomial_numeric_fn numeric[EXPR_KIND_COUNT] = {
    [EXPR_KIND_CHEBYSHEV_T] = num_chebyshev_t,
    [EXPR_KIND_CHEBYSHEV_U] = num_chebyshev_u,
    [EXPR_KIND_HERMITE_H]   = num_hermite_h,
};

static number_t polynomial_eval(expr_t *e)
{
    number_t n = expr_eval(e->a), x = expr_eval(e->b);
    number_t out = numeric[e->ops->kind](n, x);
    num_destroy(&n);
    num_destroy(&x);
    return out;
}

static bool degree(const expr_t *e, long *n)
{
    if (!expr_is_const(e->a) || e->a->name || !num_is_integer(e->a->c) || !num_is_real(e->a->c))
        return false;
    double value = num_to_double(e->a->c);
    if (value < 0 || value > 64)
        return false;
    *n = (long)value;
    return true;
}

/* Expand bounded integral degrees for polynomial calculus, without changing display nodes. */
expr_t *expr_orthopoly_expand(const expr_t *e)
{
    long n;
    if (!e || !numeric[e->ops->kind] || !degree(e, &n))
        return NULL;
    expr_t *previous = expr_const_one(), *two = expr_const_long(2);
    expr_t *twice_x = expr_mul(two, e->b);
    expr_t *current = expr_clone(e->ops == &ops_chebyshev_t ? e->b : twice_x);
    for (long k = 1; k < n; ++k) {
        expr_t *factor = expr_const_long(e->ops == &ops_hermite_h ? 2 * k : 1);
        expr_t *front = expr_mul(twice_x, current), *back = expr_mul(factor, previous);
        expr_t *raw = expr_sub(front, back), *next = expr_simplify(raw);
        expr_free(factor);
        expr_free(front);
        expr_free(back);
        expr_free(raw);
        expr_free(previous);
        previous = current;
        current = next;
    }
    expr_t *out = expr_clone(n == 0 ? previous : current);
    expr_free(previous);
    expr_free(current);
    expr_free(two);
    expr_free(twice_x);
    return out;
}

static bool uses(const expr_t *e, const expr_t *x)
{
    expr_t *variable = (expr_t *)x;
    bool used = true;
    return !expr_collect_var_usage(e, 1u, &variable, &used) || used;
}

static expr_t *polynomial_deriv(expr_t *e)
{
    const expr_t *wrt = expr_current_wrt_internal();
    if (!wrt)
        return NULL;
    if (uses(e->a, wrt)) {
        expr_t *variable = (expr_t *)wrt;
        return expr_new_formal_derivative(e, 1u, &variable);
    }
    if (expr_const_is_zero(e->a))
        return expr_const_zero();
    if (e->ops == &ops_chebyshev_u) {
        expr_t *expanded = expr_orthopoly_expand(e);
        if (expanded) {
            expr_t *out = expr_create_deriv(expanded, wrt);
            expr_free(expanded);
            return out;
        }
        /* A finite polynomial sum avoids removable singularities at x = ±1. */
        expr_bindings_t *bindings = expr_bindings_from_expr_internal(e);
        char name[64] = "j";
        for (unsigned suffix = 1u; expr_bindings_get(bindings, name); ++suffix)
            snprintf(name, sizeof(name), "j%u", suffix);
        expr_t *index = expr_new_named_var(NUM_NAN, name);
        expr_bindings_free(bindings);
        expr_t *two = expr_const_long(2), *zero = expr_const_zero();
        expr_t *twice_index = expr_mul(two, index), *order = expr_sub(e->a, twice_index);
        expr_t *previous = expr_add_long(order, -1);
        expr_t *poly = expr_chebyshev_u(previous, e->b), *weighted = expr_mul(order, poly);
        expr_t *term = expr_mul(two, weighted), *last = expr_add_long(e->a, -1);
        expr_t *half = expr_div(last, two), *upper = expr_floor(half);
        expr_t *sum = expr_new_finite_summation_range(term, index, zero, upper);
        expr_t *chain = expr_create_deriv(e->b, wrt), *out = chain ? expr_mul(sum, chain) : NULL;
        expr_free(index);
        expr_free(two);
        expr_free(zero);
        expr_free(twice_index);
        expr_free(order);
        expr_free(previous);
        expr_free(poly);
        expr_free(weighted);
        expr_free(term);
        expr_free(last);
        expr_free(half);
        expr_free(upper);
        expr_free(sum);
        expr_free(chain);
        return out;
    }
    expr_t *previous = expr_add_long(e->a, -1);
    expr_t *poly = e->ops == &ops_chebyshev_t ? expr_chebyshev_u(previous, e->b)
                                            : expr_hermite_h(previous, e->b);
    expr_t *factor = e->ops == &ops_chebyshev_t ? expr_clone(e->a) : expr_add(e->a, e->a);
    expr_t *local = expr_mul(factor, poly), *chain = expr_create_deriv(e->b, wrt);
    expr_t *out = chain ? expr_mul(local, chain) : NULL;
    expr_free(previous);
    expr_free(poly);
    expr_free(factor);
    expr_free(local);
    expr_free(chain);
    return out;
}

static expr_t *polynomial_integrate(const expr_t *e, const expr_t *wrt)
{
    if (uses(e->a, wrt))
        return NULL;
    if (e->ops == &ops_chebyshev_t) {
        expr_t *expanded = expr_orthopoly_expand(e);
        expr_t *out = expanded ? expr_integrate(expanded, wrt) : NULL;
        expr_free(expanded);
        return out;
    }
    expr_t *rate = expr_create_deriv(e->b, wrt);
    if (!rate || uses(rate, wrt) || expr_const_is_zero(rate)) {
        expr_free(rate);
        return NULL;
    }
    expr_t *next = expr_add_long(e->a, 1);
    expr_t *poly = e->ops == &ops_hermite_h ? expr_hermite_h(next, e->b) : expr_chebyshev_t(next, e->b);
    expr_t *twice = e->ops == &ops_hermite_h ? expr_add(next, next) : expr_clone(next);
    expr_t *denominator = expr_mul(twice, rate), *out = expr_div(poly, denominator);
    expr_free(next);
    expr_free(poly);
    expr_free(twice);
    expr_free(denominator);
    expr_free(rate);
    return out;
}

static expr_t *polynomial_simplify(const expr_t *e, expr_t *a, expr_t *b)
{
    if (expr_const_is_zero(a)) {
        expr_free(a);
        expr_free(b);
        return expr_const_one();
    }
    return expr_simplify_binary_operator(e, a, b);
}

#define POLYNOMIAL_OP(name, id, spelling, display) \
    const expr_ops_t ops_##name = { .eval = polynomial_eval, .deriv = polynomial_deriv, \
        .kind = id, .arity = EXPR_OP_BINARY, .expression_name = spelling, .function_name = #name, \
        .TeX_name = display, .apply_binary = expr_##name, .simplify = polynomial_simplify, \
        .integrate = polynomial_integrate }

POLYNOMIAL_OP(chebyshev_t, EXPR_KIND_CHEBYSHEV_T, "Tn", "T");
POLYNOMIAL_OP(chebyshev_u, EXPR_KIND_CHEBYSHEV_U, "Un", "U");
POLYNOMIAL_OP(hermite_h, EXPR_KIND_HERMITE_H, "ℋ", "\\mathcal{H}");
#undef POLYNOMIAL_OP

static expr_t *polynomial_new(const expr_ops_t *ops, const expr_t *n, const expr_t *x)
{
    if (!n || !x)
        return NULL;
    expr_retain(n);
    expr_retain(x);
    return expr_new_binary_internal(ops, n, x);
}

/* Construct a first-kind Chebyshev polynomial. */
expr_t *expr_chebyshev_t(const expr_t *n, const expr_t *x) { return polynomial_new(&ops_chebyshev_t, n, x); }
/* Construct a second-kind Chebyshev polynomial. */
expr_t *expr_chebyshev_u(const expr_t *n, const expr_t *x) { return polynomial_new(&ops_chebyshev_u, n, x); }
/* Construct a physicists' Hermite polynomial without reusing the harmonic Hn alias. */
expr_t *expr_hermite_h(const expr_t *n, const expr_t *x) { return polynomial_new(&ops_hermite_h, n, x); }
