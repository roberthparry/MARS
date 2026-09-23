#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#include <stdlib.h>

/* Infer a candidate from a spectral feature, then check its complete forward formula. Nothing in
 * this matcher depends on transform provenance, supplied free-variable values, or sampled values.
 * The local arena owns all scratch expressions; only the final clone escapes it. */
typedef struct {
    expr_t **nodes;
    size_t count, capacity;
    const expr_t *f, *s, *t;
    bool failed;
} special_pair_t;

static expr_t *pair_keep(special_pair_t *c, expr_t *e)
{
    if (!e)
        return NULL;
    if (c->count == c->capacity) {
        size_t capacity = c->capacity ? c->capacity * 2u : 64u;
        expr_t **nodes = realloc(c->nodes, capacity * sizeof(*nodes));
        if (!nodes) {
            expr_free(e);
            c->failed = true;
            return NULL;
        }
        c->nodes = nodes;
        c->capacity = capacity;
    }
    c->nodes[c->count++] = e;
    return e;
}

#define PAIR_UNARY(name)                                                                                                \
    static expr_t *pair_##name(special_pair_t *c, const expr_t *a) { return pair_keep(c, expr_##name(a)); }
#define PAIR_BINARY(name)                                                                                               \
    static expr_t *pair_##name(special_pair_t *c, const expr_t *a, const expr_t *b)                                         \
    { return pair_keep(c, expr_##name(a, b)); }
PAIR_UNARY(neg)
PAIR_UNARY(sqrt)
PAIR_BINARY(add)
PAIR_BINARY(sub)
PAIR_BINARY(mul)
PAIR_BINARY(div)
#undef PAIR_BINARY
#undef PAIR_UNARY

static expr_t *pair_integer(special_pair_t *c, long n) { return pair_keep(c, expr_const_long(n)); }
static expr_t *pair_clean(special_pair_t *c, const expr_t *e) { return pair_keep(c, expr_beautify(e)); }

static bool pair_uses(const expr_t *e, const expr_t *s)
{
    bool used = true;
    expr_t *variable = (expr_t *)s;
    return !expr_collect_var_usage(e, 1u, &variable, &used) || used;
}

static bool pair_equal(special_pair_t *c, const expr_t *a, const expr_t *b)
{
    if (expr_struct_eq(a, b))
        return true;
    return expr_const_is_zero(pair_clean(c, pair_sub(c, a, b)));
}

static bool pair_power(special_pair_t *c, const expr_t *f, const expr_t **base, const expr_t **power)
{
    if (expr_match_pow_expr(f, base, power))
        return true;
    if (f->ops == &ops_pow_d) {
        *base = f->a;
        *power = pair_keep(c, expr_new_const(f->c));
        return true;
    }
    if (f->ops == &ops_sqrt) {
        *base = f->a;
        *power = pair_div(c, pair_integer(c, 1), pair_integer(c, 2));
        return true;
    }
    return false;
}

/* Parsing a displayed power can choose ops_pow, ops_pow_d or ops_sqrt independently of the
 * constructor used by the forward rule. Compare their mathematical structure, without replacing
 * a parameter by its current value or changing a fractional-power branch. */
static bool pair_same(special_pair_t *c, const expr_t *a, const expr_t *b)
{
    if (expr_struct_eq(a, b))
        return true;
    if (!a || !b)
        return false;
    const expr_t *ab = NULL, *ap = NULL, *bb = NULL, *bp = NULL;
    bool power_a = pair_power(c, a, &ab, &ap), power_b = pair_power(c, b, &bb, &bp);
    if (power_a || power_b)
        return power_a && power_b && pair_equal(c, ap, bp) && pair_same(c, ab, bb);
    if (a->ops != b->ops || a->ops->arity == EXPR_OP_ATOM)
        return false;
    if (pair_same(c, a->a, b->a) && pair_same(c, a->b, b->b))
        return true;
    return (a->ops == &ops_add || a->ops == &ops_mul) &&
           pair_same(c, a->a, b->b) && pair_same(c, a->b, b->a);
}

enum { pair_factor_limit = 16 };
typedef struct {
    const expr_t *numerator[pair_factor_limit], *denominator[pair_factor_limit];
    size_t numerator_count, denominator_count;
    expr_t *scalar;
} pair_factors_t;

static bool pair_collect_factors(special_pair_t *c, const expr_t *f, bool denominator, pair_factors_t *factors)
{
    if (!pair_uses(f, c->s)) {
        factors->scalar = pair_clean(c, denominator ? pair_div(c, factors->scalar, f)
                                                   : pair_mul(c, factors->scalar, f));
        return true;
    }
    if (f->ops == &ops_neg) {
        factors->scalar = pair_clean(c, pair_neg(c, factors->scalar));
        return pair_collect_factors(c, f->a, denominator, factors);
    }
    if (f->ops == &ops_mul || f->ops == &ops_div)
        return pair_collect_factors(c, f->a, denominator, factors) &&
               pair_collect_factors(c, f->b, denominator != (f->ops == &ops_div), factors);
    size_t *count = denominator ? &factors->denominator_count : &factors->numerator_count;
    if (*count == pair_factor_limit)
        return false;
    (denominator ? factors->denominator : factors->numerator)[(*count)++] = f;
    return true;
}

/* Special pairs have only a handful of factors. The bounded 16-by-16 cancellation handles
 * equivalent constructor/parser power nodes without asking the general simplifier to divide
 * two large special-function expressions. It never splits or combines fractional powers. */
static expr_t *pair_scalar_ratio(special_pair_t *c, const expr_t *numerator, const expr_t *denominator)
{
    pair_factors_t factors = {.scalar = pair_integer(c, 1)};
    if (!pair_collect_factors(c, numerator, false, &factors) ||
        !pair_collect_factors(c, denominator, true, &factors) ||
        factors.numerator_count != factors.denominator_count)
        return NULL;
    for (size_t i = 0u; i < factors.numerator_count; ++i) {
        bool found = false;
        for (size_t j = 0u; j < factors.denominator_count; ++j) {
            if (factors.denominator[j] && pair_same(c, factors.numerator[i], factors.denominator[j])) {
                factors.denominator[j] = NULL;
                found = true;
                break;
            }
        }
        if (!found)
            return NULL;
    }
    return factors.scalar;
}

/* Collect degree-at-most-two polynomials by structure. The three coefficients are indexed by
 * degree; no differentiation or evaluation of a free parameter is used to infer a coefficient. */
static bool pair_polynomial(special_pair_t *c, const expr_t *f, expr_t **p)
{
    for (size_t k = 0u; k < 3u; ++k)
        p[k] = pair_integer(c, 0);
    if (!pair_uses(f, c->s)) {
        p[0] = pair_keep(c, expr_clone(f));
        return true;
    }
    if (expr_struct_eq(f, c->s)) {
        p[1] = pair_integer(c, 1);
        return true;
    }
    expr_t *a[3], *b[3];
    if ((f->ops == &ops_add || f->ops == &ops_sub) &&
        pair_polynomial(c, f->a, a) && pair_polynomial(c, f->b, b)) {
        for (size_t k = 0u; k < 3u; ++k)
            p[k] = pair_clean(c, f->ops == &ops_add ? pair_add(c, a[k], b[k]) : pair_sub(c, a[k], b[k]));
        return true;
    }
    if (f->ops == &ops_neg && pair_polynomial(c, f->a, a)) {
        for (size_t k = 0u; k < 3u; ++k)
            p[k] = pair_clean(c, pair_neg(c, a[k]));
        return true;
    }
    if (f->ops == &ops_div && !pair_uses(f->b, c->s) && pair_polynomial(c, f->a, a)) {
        for (size_t k = 0u; k < 3u; ++k)
            p[k] = pair_clean(c, pair_div(c, a[k], f->b));
        return true;
    }
    const expr_t *base = NULL, *power = NULL;
    bool square = pair_power(c, f, &base, &power) && pair_equal(c, power, pair_integer(c, 2));
    if ((f->ops == &ops_mul || square) && pair_polynomial(c, square ? base : f->a, a) &&
        pair_polynomial(c, square ? base : f->b, b)) {
        for (size_t i = 0u; i < 3u; ++i) {
            for (size_t j = 0u; j < 3u; ++j) {
                if (expr_const_is_zero(a[i]) || expr_const_is_zero(b[j]))
                    continue;
                if (i + j > 2u)
                    return false;
                p[i + j] = pair_clean(c, pair_add(c, p[i + j], pair_mul(c, a[i], b[j])));
            }
        }
        return true;
    }
    return false;
}

static bool pair_affine(special_pair_t *c, const expr_t *f, expr_t **a, expr_t **b)
{
    expr_t *p[3];
    if (!pair_polynomial(c, f, p) || !expr_const_is_zero(p[2]))
        return false;
    *a = p[1];
    *b = p[0];
    return !expr_const_is_zero(*a);
}

static bool pair_contains(special_pair_t *c, const expr_t *f, const expr_t *feature)
{
    return f && (pair_same(c, f, feature) || pair_contains(c, f->a, feature) || pair_contains(c, f->b, feature));
}

/* Extract a coefficient only when the expression is genuinely affine in the selected feature.
 * This permits scalar multiples and an additional c/s, without dropping any other spectral term. */
static bool pair_linear(special_pair_t *c, const expr_t *f, const expr_t *feature, expr_t **a, expr_t **b)
{
    if (pair_same(c, f, feature)) {
        *a = pair_integer(c, 1);
        *b = pair_integer(c, 0);
        return true;
    }
    if (!pair_contains(c, f, feature)) {
        *a = pair_integer(c, 0);
        *b = pair_keep(c, expr_clone(f));
        return true;
    }
    expr_t *la = NULL, *lb = NULL, *ra = NULL, *rb = NULL;
    if (f->ops == &ops_neg && pair_linear(c, f->a, feature, &la, &lb)) {
        *a = pair_clean(c, pair_neg(c, la));
        *b = pair_clean(c, pair_neg(c, lb));
        return true;
    }
    if ((f->ops != &ops_add && f->ops != &ops_sub && f->ops != &ops_mul && f->ops != &ops_div) ||
        !pair_linear(c, f->a, feature, &la, &lb) || !pair_linear(c, f->b, feature, &ra, &rb))
        return false;
    if (f->ops == &ops_add || f->ops == &ops_sub) {
        *a = pair_clean(c, f->ops == &ops_add ? pair_add(c, la, ra) : pair_sub(c, la, ra));
        *b = pair_clean(c, f->ops == &ops_add ? pair_add(c, lb, rb) : pair_sub(c, lb, rb));
        return true;
    }
    if (f->ops == &ops_mul && (expr_const_is_zero(la) || expr_const_is_zero(ra))) {
        *a = pair_clean(c, pair_add(c, pair_mul(c, la, rb), pair_mul(c, lb, ra)));
        *b = pair_clean(c, pair_mul(c, lb, rb));
        return true;
    }
    if (f->ops == &ops_div && expr_const_is_zero(ra)) {
        *a = pair_clean(c, pair_div(c, la, rb));
        *b = pair_clean(c, pair_div(c, lb, rb));
        return true;
    }
    return false;
}

static expr_t *pair_reduced(special_pair_t *c, const expr_t *e)
{
    expr_t *out = pair_clean(c, e);
    out = pair_keep(c, expr_expand_preserved_for_display(out));
    out = pair_keep(c, expr_expand_products_internal(out));
    return pair_clean(c, out);
}

/* Check a complete forward formula, not just a suggestive special-function node. Conditions on
 * the Bromwich coordinate are not time-domain restrictions; all parameter conditions survive. */
static expr_t *pair_verify_with_offset(special_pair_t *c, const expr_t *candidate, const expr_t *feature,
                                      bool allow_offset)
{
    if (!candidate || pair_uses(candidate, c->s))
        return NULL;
    expr_t *args[] = {(expr_t *)candidate, (expr_t *)c->t, (expr_t *)c->s};
    expr_t *transform = pair_keep(c, expr_laplace_from_args(3u, args));
    if (!transform)
        return NULL;
    number_t bound = NUM_ZERO;
    expr_t *conditions = NULL;
    expr_t *formula = pair_keep(c, expr_laplace_formula(transform, &bound, &conditions));
    pair_keep(c, conditions);
    num_destroy(&bound);
    if (!formula)
        return NULL;
    expr_t *scale = pair_scalar_ratio(c, c->f, formula);
    if (!scale)
        scale = pair_clean(c, pair_div(c, c->f, formula));
    expr_t *offset = pair_integer(c, 0);
    if (pair_uses(scale, c->s)) {
        expr_t *fa = NULL, *fb = NULL, *ga = NULL, *gb = NULL;
        if (!feature || !pair_linear(c, c->f, feature, &fa, &fb) ||
            !pair_linear(c, formula, feature, &ga, &gb) || expr_const_is_zero(ga))
            return NULL;
        scale = pair_reduced(c, pair_div(c, fa, ga));
        if (pair_uses(scale, c->s))
            return NULL;
        offset = pair_reduced(c, pair_mul(c, c->s, pair_sub(c, fb, pair_mul(c, scale, gb))));
        if (pair_uses(offset, c->s))
            return NULL;
    }
    if (!allow_offset && !expr_const_is_zero(offset))
        return NULL;
    if (expr_const_is_zero(scale))
        return NULL;
    expr_t *body = pair_clean(c, pair_add(c, pair_mul(c, scale, candidate), offset));
    expr_t *zero = pair_integer(c, 0);
    expr_t *domain_args[] = {body, (expr_t *)c->t, zero};
    expr_t *out = pair_keep(c, expr_real_domain_from_args(3u, domain_args));
    if (!out)
        return NULL;
    expr_t **tail = &out->b->b->b;
    for (const expr_t *item = conditions; item; item = item->b) {
        bool half_plane = expr_is_op(item->a, &ops_argument_list);
        const expr_t *value = half_plane ? item->a->a : item->a;
        const expr_t *limit = half_plane ? item->a->b : NULL;
        if (pair_uses(value, c->s) || (limit && pair_uses(limit, c->s)))
            continue;
        *tail = expr_alloc(&ops_argument_list);
        (*tail)->a = expr_clone(value);
        (*tail)->b = expr_alloc(&ops_argument_list);
        (*tail)->b->a = limit ? expr_clone(limit)
                             : expr_const_long(expr_is_op(value, &ops_nonnegative_integer) ? 0 : -1);
        tail = &(*tail)->b->b;
    }
    return out;
}

static expr_t *pair_verify(special_pair_t *c, const expr_t *candidate, const expr_t *feature)
{
    return pair_verify_with_offset(c, candidate, feature, true);
}

static expr_t *pair_unary_candidate(special_pair_t *c, const expr_ops_t *ops, const expr_t *rate,
                                    const expr_t *feature)
{
    expr_t *argument = pair_clean(c, pair_mul(c, rate, c->t));
    expr_t *candidate = pair_keep(c, ops->apply_unary(argument));
    return pair_verify(c, candidate, feature);
}

/* ln(1+s/a)/s and -ln(s/a-1)/s have different real-axis exponential-integral branches. */
static expr_t *pair_logarithm(special_pair_t *c, const expr_t *node)
{
    expr_t *a = NULL, *b = NULL;
    if (!pair_affine(c, node->a, &a, &b))
        return NULL;
    expr_t *one = pair_integer(c, 1);
    if (pair_equal(c, b, one))
        return pair_unary_candidate(c, &ops_E1, pair_clean(c, pair_div(c, one, a)), node);
    if (pair_equal(c, b, pair_neg(c, one)))
        return pair_unary_candidate(c, &ops_Ei, pair_clean(c, pair_div(c, one, a)), node);
    return NULL;
}

/* An E1(r*s) feature identifies log(1+t/r); its full exponential multiplier is verified. */
static expr_t *pair_exponential_integral(special_pair_t *c, const expr_t *node)
{
    expr_t *a = NULL, *b = NULL;
    if (!pair_affine(c, node->a, &a, &b) || !expr_const_is_zero(b))
        return NULL;
    expr_t *one = pair_integer(c, 1), *rate = pair_clean(c, pair_div(c, one, a));
    expr_t *argument = pair_add(c, one, pair_mul(c, rate, c->t));
    return pair_verify(c, pair_keep(c, expr_log(argument)), node);
}

static expr_t *pair_gamma_candidate(special_pair_t *c, const expr_t *shape, const expr_t *rate,
                                    const expr_t *feature)
{
    if (pair_uses(shape, c->s) || pair_uses(rate, c->s))
        return NULL;
    expr_t *argument = pair_clean(c, pair_mul(c, rate, c->t));
    expr_t *lower = pair_keep(c, expr_gammainc_P(shape, argument));
    expr_t *out = pair_verify_with_offset(c, lower, feature, false);
    if (out)
        return out;
    /* Prefer a direct tail over an algebraically equivalent 1-P or 1-Q. Either subtraction
     * can discard all significant digits in its small tail; neither tail is preferred blindly. */
    expr_t *upper = pair_keep(c, expr_gammainc_Q(shape, argument));
    out = pair_verify_with_offset(c, upper, feature, false);
    return out ? out : pair_verify(c, lower, feature);
}

/* Both (a/(s+a))^v and the beautified reciprocal (s+a)^(-v) are recognised. The forward
 * verifier retains Re(v)>0 and Re(a)>0, including for symbolic shapes and rates. */
static expr_t *pair_gamma_power(special_pair_t *c, const expr_t *node)
{
    const expr_t *base = NULL, *power = NULL;
    if (!pair_power(c, node, &base, &power) || pair_uses(power, c->s))
        return NULL;
    expr_t *a = NULL, *b = NULL, *out = NULL;
    if (pair_affine(c, base, &a, &b) && !expr_const_is_zero(b)) {
        expr_t *rate = pair_clean(c, pair_div(c, b, a));
        out = pair_gamma_candidate(c, power, rate, node);
        if (!out)
            out = pair_gamma_candidate(c, pair_clean(c, pair_neg(c, power)), rate, node);
    }
    if (!out && base->ops == &ops_div && !pair_uses(base->a, c->s) &&
        pair_affine(c, base->b, &a, &b) && !expr_const_is_zero(b))
        out = pair_gamma_candidate(c, power, pair_clean(c, pair_div(c, b, a)), node);
    return out;
}

static expr_t *pair_bessel_order(special_pair_t *c, const expr_t *order, const expr_t *rate)
{
    if (pair_uses(order, c->s))
        return NULL;
    expr_t *argument = pair_clean(c, pair_mul(c, rate, c->t));
    return pair_verify(c, pair_keep(c, expr_bessel_j(order, argument)), NULL);
}

/* A single walk over algebraic powers supplies orders; it does not enumerate possible integers. */
static expr_t *pair_bessel_powers(special_pair_t *c, const expr_t *node, const expr_t *root, const expr_t *rate)
{
    if (!node)
        return NULL;
    const expr_t *base = NULL, *power = NULL;
    if (pair_power(c, node, &base, &power) && !pair_same(c, base, root) && pair_contains(c, base, root)) {
        expr_t *out = pair_bessel_order(c, power, rate);
        if (!out)
            out = pair_bessel_order(c, pair_clean(c, pair_neg(c, power)), rate);
        if (out)
            return out;
    }
    expr_t *out = pair_bessel_powers(c, node->a, root, rate);
    return out ? out : pair_bessel_powers(c, node->b, root, rate);
}

static expr_t *pair_radical(special_pair_t *c, const expr_t *node)
{
    const expr_t *base = NULL, *power = NULL;
    if (!pair_power(c, node, &base, &power) ||
        !pair_equal(c, power, pair_div(c, pair_integer(c, 1), pair_integer(c, 2))))
        return NULL;
    expr_t *p[3];
    if (!pair_polynomial(c, base, p) || !expr_const_is_zero(p[1]) ||
        !pair_equal(c, p[2], pair_integer(c, 1)))
        return pair_gamma_power(c, node);
    expr_t *rate = pair_clean(c, pair_sqrt(c, p[0]));
    expr_t *out = pair_bessel_order(c, pair_integer(c, 0), rate);
    if (!out)
        out = pair_bessel_order(c, pair_integer(c, 1), rate);
    if (!out) {
        expr_t *argument = pair_clean(c, pair_mul(c, rate, c->t));
        out = pair_verify(c, pair_keep(c, expr_bessel_y(pair_integer(c, 0), argument)), NULL);
    }
    return out ? out : pair_bessel_powers(c, c->f, node, rate);
}

static expr_t *pair_general_power(special_pair_t *c, const expr_t *node)
{
    expr_t *out = pair_radical(c, node);
    return out ? out : pair_gamma_power(c, node);
}

typedef expr_t *(*pair_feature_fn)(special_pair_t *, const expr_t *);
static const pair_feature_fn pair_features[EXPR_KIND_COUNT] = {
    [EXPR_KIND_LOG     ] = pair_logarithm,
    [EXPR_KIND_E1      ] = pair_exponential_integral,
    [EXPR_KIND_SQRT    ] = pair_radical,
    [EXPR_KIND_POW     ] = pair_general_power,
    [EXPR_KIND_POW_D   ] = pair_general_power,
};

/* Feature dispatch is directly indexed. Traversal cost is proportional to the supplied expression;
 * arbitrary function arguments are not mistaken for products or sums of transform factors. */
static expr_t *pair_visit(special_pair_t *c, const expr_t *node)
{
    if (!node || c->failed || !pair_uses(node, c->s))
        return NULL;
    pair_feature_fn match = pair_features[node->ops->kind];
    expr_t *out = match ? match(c, node) : NULL;
    if (out)
        return out;
    bool algebraic = node->ops == &ops_add || node->ops == &ops_sub || node->ops == &ops_mul ||
                     node->ops == &ops_div || node->ops == &ops_neg || node->ops == &ops_pow ||
                     node->ops == &ops_pow_d || node->ops == &ops_log;
    if (!algebraic)
        return NULL;
    out = pair_visit(c, node->a);
    return out ? out : pair_visit(c, node->b);
}

/* Return a verified special-function inverse on positive time, or NULL for an unmatched spectrum. */
expr_t *expr_inverse_laplace_special_pair(const expr_t *f, const expr_t *s, const expr_t *t)
{
    if (!f || !expr_is_var(s) || !expr_is_var(t))
        return NULL;
    special_pair_t c = {.f = f, .s = s, .t = t};
    expr_t *matched = pair_visit(&c, f);
    expr_t *out = matched && !c.failed ? expr_clone(matched) : NULL;
    for (size_t k = 0u; k < c.count; ++k)
        expr_free(c.nodes[k]);
    free(c.nodes);
    return out;
}
