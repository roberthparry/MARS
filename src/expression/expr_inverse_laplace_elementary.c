#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#include <stdlib.h>

/* Infer a candidate from a spectral feature, then check its complete forward formula. Nothing in
 * this matcher depends on transform provenance, supplied free-variable values, or sampled values.
 * The local arena owns all scratch expressions; only the final clone escapes it. */
typedef struct {
    expr_t **nodes;
    size_t count, capacity;
    const expr_t *f, *s, *t;
    bool failed, exact_pair;
} elementary_pair_t;

static expr_t *elementary_keep(elementary_pair_t *c, expr_t *e)
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

#define ELEMENTARY_UNARY(name) \
    static expr_t *elementary_##name(elementary_pair_t *c, const expr_t *a) \
    { return elementary_keep(c, expr_##name(a)); }
#define ELEMENTARY_BINARY(name) \
    static expr_t *elementary_##name(elementary_pair_t *c, const expr_t *a, const expr_t *b) \
    { return elementary_keep(c, expr_##name(a, b)); }
ELEMENTARY_UNARY(neg)
ELEMENTARY_BINARY(add)
ELEMENTARY_BINARY(sub)
ELEMENTARY_BINARY(mul)
ELEMENTARY_BINARY(div)
#undef ELEMENTARY_BINARY
#undef ELEMENTARY_UNARY

static expr_t *elementary_integer(elementary_pair_t *c, long n) { return elementary_keep(c, expr_const_long(n)); }

/* Cancelling imaginary units can leave a real value in a complex backend. Normalise literal
 * numeric coefficients throughout this matcher, without evaluating symbolic pi/gamma expressions. */
static expr_t *elementary_clean(elementary_pair_t *c, const expr_t *e)
{
    expr_t *out = elementary_keep(c, expr_beautify(e));
    if (expr_is_unnamed_const(out) && num_is_complex_backend(out->c) && num_is_real(out->c) &&
        (!out->binding_expr || expr_binding_expr_is_numeric_literal(out->binding_expr))) {
        number_t real = num_real_part(out->c);
        out = elementary_keep(c, expr_new_const(real));
        num_destroy(&real);
    }
    return out;
}

static bool elementary_uses(const expr_t *e, const expr_t *s)
{
    bool used = true;
    expr_t *variable = (expr_t *)s;
    return !expr_collect_var_usage(e, 1u, &variable, &used) || used;
}

static bool elementary_equal(elementary_pair_t *c, const expr_t *a, const expr_t *b)
{
    if (expr_struct_eq(a, b))
        return true;
    return expr_const_is_zero(elementary_clean(c, elementary_sub(c, a, b)));
}

static bool elementary_power(elementary_pair_t *c, const expr_t *f, const expr_t **base, const expr_t **power)
{
    if (expr_match_pow_expr(f, base, power))
        return true;
    if (f->ops == &ops_pow_d) {
        *base = f->a;
        *power = elementary_keep(c, expr_new_const(f->c));
        return true;
    }
    if (f->ops == &ops_sqrt) {
        *base = f->a;
        *power = elementary_div(c, elementary_integer(c, 1), elementary_integer(c, 2));
        return true;
    }
    return false;
}

/* Parser literals and computed coefficients may use floating or complex backends even when
 * they are exactly integral. The bounded conversion is exact, not a tolerance comparison. */
static bool elementary_small_integer(number_t value, long *integer)
{
    if (!num_is_finite(value) || !num_is_real(value) || !num_is_integer(value))
        return false;
    double real = num_to_double(value);
    if (real < -64.0 || real > 64.0)
        return false;
    *integer = (long)real;
    return true;
}

/* Constructor and parser power encodings differ, as can the association of an affine argument.
 * Match the actual function and every argument, never merely its numerical value at a point. */
static bool elementary_same_polynomial(elementary_pair_t *c, const expr_t *a, const expr_t *b);

static bool elementary_same(elementary_pair_t *c, const expr_t *a, const expr_t *b)
{
    if (expr_struct_eq(a, b))
        return true;
    if (!a || !b)
        return false;
    if (a->ops == &ops_summation || b->ops == &ops_summation) {
        if (a->ops != b->ops || !expr_is_op(a->b, &ops_argument_list) ||
            !expr_is_op(b->b, &ops_argument_list) || !elementary_same(c, a->b->b, b->b->b))
            return false;
        /* Alpha-equivalence avoids expanding a fifteen-term degree-thirty recurrence.
         * An unnamed fresh index cannot capture a free symbol in either summand. */
        expr_t *index = elementary_keep(c, expr_new_var(NUM_NAN));
        expr_t *left = elementary_keep(c, expr_substitute(a->a, a->b->a, index));
        expr_t *right = elementary_keep(c, expr_substitute(b->a, b->b->a, index));
        return elementary_same(c, left, right);
    }
    if (expr_is_unnamed_const(a) && expr_is_unnamed_const(b) &&
        (!a->binding_expr || expr_binding_expr_is_numeric_literal(a->binding_expr)) &&
        (!b->binding_expr || expr_binding_expr_is_numeric_literal(b->binding_expr)))
        return num_eq(a->c, b->c);
    const expr_t *ab = NULL, *ap = NULL, *bb = NULL, *bp = NULL;
    bool pa = elementary_power(c, a, &ab, &ap), pb = elementary_power(c, b, &bb, &bp);
    if (pa || pb)
        return (pa && pb && elementary_equal(c, ap, bp) && elementary_same(c, ab, bb)) ||
               elementary_equal(c, a, b) || elementary_same_polynomial(c, a, b);
    if (a->ops != b->ops || a->ops->arity == EXPR_OP_ATOM) {
        bool arithmetic_a = a->ops == &ops_add || a->ops == &ops_sub || a->ops == &ops_mul ||
                            a->ops == &ops_div || a->ops == &ops_neg;
        bool arithmetic_b = b->ops == &ops_add || b->ops == &ops_sub || b->ops == &ops_mul ||
                            b->ops == &ops_div || b->ops == &ops_neg;
        return (arithmetic_a && arithmetic_b) &&
               (elementary_equal(c, a, b) || elementary_same_polynomial(c, a, b));
    }
    if (elementary_same(c, a->a, b->a) && elementary_same(c, a->b, b->b))
        return true;
    if ((a->ops == &ops_add || a->ops == &ops_mul) &&
        elementary_same(c, a->a, b->b) && elementary_same(c, a->b, b->a))
        return true;
    return elementary_equal(c, a, b) || elementary_same_polynomial(c, a, b);
}

/* Collect degree-at-most-two polynomials by structure. The three coefficients are indexed by
 * degree; no differentiation or evaluation of a free parameter is used to infer a coefficient. */
static bool elementary_polynomial(elementary_pair_t *c, const expr_t *f, expr_t **p)
{
    for (size_t k = 0u; k < 3u; ++k)
        p[k] = elementary_integer(c, 0);
    if (!elementary_uses(f, c->s)) {
        p[0] = elementary_keep(c, expr_clone(f));
        return true;
    }
    if (expr_struct_eq(f, c->s)) {
        p[1] = elementary_integer(c, 1);
        return true;
    }
    expr_t *a[3], *b[3];
    if ((f->ops == &ops_add || f->ops == &ops_sub) &&
        elementary_polynomial(c, f->a, a) && elementary_polynomial(c, f->b, b)) {
        for (size_t k = 0u; k < 3u; ++k)
            p[k] = elementary_clean(c, f->ops == &ops_add ? elementary_add(c, a[k], b[k]) : elementary_sub(c, a[k], b[k]));
        return true;
    }
    if (f->ops == &ops_neg && elementary_polynomial(c, f->a, a)) {
        for (size_t k = 0u; k < 3u; ++k)
            p[k] = elementary_clean(c, elementary_neg(c, a[k]));
        return true;
    }
    if (f->ops == &ops_div && !elementary_uses(f->b, c->s) && elementary_polynomial(c, f->a, a)) {
        for (size_t k = 0u; k < 3u; ++k)
            p[k] = elementary_clean(c, elementary_div(c, a[k], f->b));
        return true;
    }
    const expr_t *base = NULL, *power = NULL;
    bool square = elementary_power(c, f, &base, &power) && elementary_equal(c, power, elementary_integer(c, 2));
    if ((f->ops == &ops_mul || square) && elementary_polynomial(c, square ? base : f->a, a) &&
        elementary_polynomial(c, square ? base : f->b, b)) {
        for (size_t i = 0u; i < 3u; ++i) {
            for (size_t j = 0u; j < 3u; ++j) {
                if (expr_const_is_zero(a[i]) || expr_const_is_zero(b[j]))
                    continue;
                if (i + j > 2u)
                    return false;
                p[i + j] = elementary_clean(c, elementary_add(c, p[i + j], elementary_mul(c, a[i], b[j])));
            }
        }
        return true;
    }
    return false;
}

static bool elementary_affine(elementary_pair_t *c, const expr_t *f, expr_t **a, expr_t **b)
{
    expr_t *p[3];
    if (!elementary_polynomial(c, f, p) || !expr_const_is_zero(p[2]))
        return false;
    *a = p[1];
    *b = p[0];
    return !expr_const_is_zero(*a);
}

/* Compare low-degree arguments coefficient by coefficient, without expanding special functions
 * or high-degree recurrences. In particular, -(s/q)^2/4 and -s^2 agree when q=1/2. */
static bool elementary_same_polynomial(elementary_pair_t *c, const expr_t *a, const expr_t *b)
{
    expr_t *left[3], *right[3];
    const expr_t *arguments[] = {a, b};
    for (size_t k = 0u; k < 2u; ++k) {
        const expr_t *f = arguments[k];
        if (!expr_is_var(f) && !expr_is_addsub(f) && !expr_is_mul(f) && !expr_is_div(f) && !expr_is_neg(f) &&
            f->ops != &ops_pow && f->ops != &ops_pow_d)
            return false;
    }
    if (!elementary_uses(a, c->s) || !elementary_uses(b, c->s) ||
        !elementary_polynomial(c, a, left) || !elementary_polynomial(c, b, right))
        return false;
    for (size_t k = 0u; k < 3u; ++k)
        if (!elementary_equal(c, left[k], right[k]))
            return false;
    return true;
}

static bool elementary_contains(elementary_pair_t *c, const expr_t *f, const expr_t *feature)
{
    return f && (elementary_same(c, f, feature) || elementary_contains(c, f->a, feature) ||
                 elementary_contains(c, f->b, feature));
}

/* Extract a coefficient only when the expression is genuinely affine in the selected feature.
 * This permits scalar multiples and an additional c/s, without dropping any other spectral term. */
static bool elementary_linear(elementary_pair_t *c, const expr_t *f, const expr_t *feature, expr_t **a, expr_t **b)
{
    if (elementary_same(c, f, feature)) {
        *a = elementary_integer(c, 1);
        *b = elementary_integer(c, 0);
        return true;
    }
    if (!elementary_contains(c, f, feature)) {
        *a = elementary_integer(c, 0);
        *b = elementary_keep(c, expr_clone(f));
        return true;
    }
    expr_t *la = NULL, *lb = NULL, *ra = NULL, *rb = NULL;
    if (f->ops == &ops_neg && elementary_linear(c, f->a, feature, &la, &lb)) {
        *a = elementary_clean(c, elementary_neg(c, la));
        *b = elementary_clean(c, elementary_neg(c, lb));
        return true;
    }
    if ((f->ops != &ops_add && f->ops != &ops_sub && f->ops != &ops_mul && f->ops != &ops_div) ||
        !elementary_linear(c, f->a, feature, &la, &lb) || !elementary_linear(c, f->b, feature, &ra, &rb))
        return false;
    if (f->ops == &ops_add || f->ops == &ops_sub) {
        *a = elementary_clean(c, f->ops == &ops_add ? elementary_add(c, la, ra) : elementary_sub(c, la, ra));
        *b = elementary_clean(c, f->ops == &ops_add ? elementary_add(c, lb, rb) : elementary_sub(c, lb, rb));
        return true;
    }
    if (f->ops == &ops_mul && (expr_const_is_zero(la) || expr_const_is_zero(ra))) {
        *a = elementary_clean(c, elementary_add(c, elementary_mul(c, la, rb), elementary_mul(c, lb, ra)));
        *b = elementary_clean(c, elementary_mul(c, lb, rb));
        return true;
    }
    if (f->ops == &ops_div && expr_const_is_zero(ra)) {
        *a = elementary_clean(c, elementary_div(c, la, rb));
        *b = elementary_clean(c, elementary_div(c, lb, rb));
        return true;
    }
    return false;
}

/* Integer arithmetic in a spectral rational expression may be rearranged safely without
 * changing any non-integer power or special-function branch. Those remain indivisible atoms. */
static bool elementary_fraction(elementary_pair_t *c, const expr_t *f, expr_t **n, expr_t **d, unsigned depth)
{
    if (!f || depth > 64u || c->failed)
        return false;
    expr_t *an = NULL, *ad = NULL, *bn = NULL, *bd = NULL;
    if (f->ops == &ops_neg && elementary_fraction(c, f->a, &an, &ad, depth + 1u)) {
        *n = elementary_neg(c, an);
        *d = ad;
        return true;
    }
    if ((f->ops == &ops_add || f->ops == &ops_sub || f->ops == &ops_mul || f->ops == &ops_div) &&
        elementary_fraction(c, f->a, &an, &ad, depth + 1u) &&
        elementary_fraction(c, f->b, &bn, &bd, depth + 1u)) {
        if (f->ops == &ops_add || f->ops == &ops_sub) {
            expr_t *left = elementary_mul(c, an, bd), *right = elementary_mul(c, bn, ad);
            *n = f->ops == &ops_add ? elementary_add(c, left, right) : elementary_sub(c, left, right);
            *d = elementary_mul(c, ad, bd);
        } else {
            *n = elementary_mul(c, an, f->ops == &ops_mul ? bn : bd);
            *d = elementary_mul(c, ad, f->ops == &ops_mul ? bd : bn);
        }
        *n = elementary_clean(c, *n);
        *d = elementary_clean(c, *d);
        return true;
    }
    const expr_t *base = NULL, *power = NULL;
    number_t value = NUM_NAN;
    long numerator = 0;
    bool integral = elementary_power(c, f, &base, &power) && expr_match_const_value(power, &value) &&
                    elementary_small_integer(value, &numerator);
    num_destroy(&value);
    if (integral && elementary_fraction(c, base, &an, &ad, depth + 1u)) {
        expr_t *degree = elementary_integer(c, numerator < 0 ? -numerator : numerator);
        *n = elementary_keep(c, expr_pow_xp(numerator < 0 ? ad : an, degree));
        *d = elementary_keep(c, expr_pow_xp(numerator < 0 ? an : ad, degree));
        return true;
    }
    *n = elementary_keep(c, expr_clone(f));
    *d = elementary_integer(c, 1);
    return true;
}

static expr_t *elementary_expanded(elementary_pair_t *c, const expr_t *e)
{
    expr_t *out = elementary_clean(c, e);
    out = elementary_keep(c, expr_expand_preserved_for_display(out));
    out = elementary_keep(c, expr_expand_products_internal(out));
    return elementary_clean(c, out);
}

static expr_t *elementary_reduced(elementary_pair_t *c, const expr_t *e)
{
    expr_t *out = elementary_clean(c, e), *numerator = NULL, *denominator = NULL;
    if (!elementary_uses(out, c->s))
        return out;
    out = elementary_expanded(c, out);
    if (!elementary_uses(out, c->s))
        return out;
    if (elementary_fraction(c, out, &numerator, &denominator, 0u)) {
        numerator = elementary_expanded(c, numerator);
        denominator = elementary_expanded(c, denominator);
        out = elementary_clean(c, elementary_div(c, numerator, denominator));
    }
    return out;
}

/* Keep finite recurrences opaque. Their alpha-equivalence is checked structurally above, so a
 * degree-thirty-two denominator never multiplies out a fifteen-term recurrence for comparison. */
static expr_t *elementary_normalise_sums(elementary_pair_t *c, const expr_t *f, unsigned depth)
{
    if (!f || depth > 64u)
        return elementary_keep(c, expr_clone(f));
    if (f->ops == &ops_summation)
        return elementary_keep(c, expr_clone(f));
    static expr_t *(*const expansions[])(const expr_t *, const expr_t *) = {
        [EXPR_KIND_STRUVE_H] = expr_struve_h_hypergeometric,
        [EXPR_KIND_STRUVE_L] = expr_struve_l_hypergeometric,
        [EXPR_KIND_BESSEL_I] = expr_bessel_i_hypergeometric,
    };
    size_t kind = (size_t)f->ops->kind;
    if (kind < sizeof(expansions) / sizeof(expansions[0]) && expansions[kind]) {
        expr_t *expanded = elementary_keep(c, expansions[kind](f->a, f->b));
        if (expanded)
            return elementary_normalise_sums(c, expanded, depth + 1u);
    }
    expr_t *out = elementary_keep(c, expr_clone(f));
    if (out->a) {
        expr_free(out->a);
        out->a = expr_clone(elementary_normalise_sums(c, f->a, depth + 1u));
    }
    if (out->b) {
        expr_free(out->b);
        out->b = expr_clone(elementary_normalise_sums(c, f->b, depth + 1u));
    }
    return elementary_clean(c, out);
}

/* Check a complete forward formula, not just a suggestive special-function node. Conditions on
 * the Bromwich coordinate are not time-domain restrictions; all parameter conditions survive. */
enum { elementary_factor_limit = 16 };
typedef struct {
    const expr_t *numerator[elementary_factor_limit], *denominator[elementary_factor_limit];
    size_t numerator_count, denominator_count;
    expr_t *scalar;
} elementary_factors_t;

static bool elementary_collect_factors(elementary_pair_t *c, const expr_t *f, bool denominator, elementary_factors_t *factors)
{
    if (!elementary_uses(f, c->s)) {
        factors->scalar = elementary_clean(c, denominator ? elementary_div(c, factors->scalar, f)
                                                   : elementary_mul(c, factors->scalar, f));
        return true;
    }
    if (f->ops == &ops_neg) {
        factors->scalar = elementary_clean(c, elementary_neg(c, factors->scalar));
        return elementary_collect_factors(c, f->a, denominator, factors);
    }
    if (f->ops == &ops_mul || f->ops == &ops_div)
        return elementary_collect_factors(c, f->a, denominator, factors) &&
               elementary_collect_factors(c, f->b, denominator != (f->ops == &ops_div), factors);
    size_t *count = denominator ? &factors->denominator_count : &factors->numerator_count;
    if (*count == elementary_factor_limit)
        return false;
    (denominator ? factors->denominator : factors->numerator)[(*count)++] = f;
    return true;
}

/* Special pairs have only a handful of factors. The bounded 16-by-16 cancellation handles
 * equivalent constructor/parser power nodes without asking the general simplifier to divide
 * two large special-function expressions. It never splits or combines fractional powers. */
static expr_t *elementary_scalar_ratio(elementary_pair_t *c, const expr_t *numerator, const expr_t *denominator)
{
    elementary_factors_t factors = {.scalar = elementary_integer(c, 1)};
    if (!elementary_collect_factors(c, numerator, false, &factors) ||
        !elementary_collect_factors(c, denominator, true, &factors) ||
        factors.numerator_count != factors.denominator_count)
        return NULL;
    for (size_t i = 0u; i < factors.numerator_count; ++i) {
        bool found = false;
        for (size_t j = 0u; j < factors.denominator_count; ++j) {
            if (factors.denominator[j] && elementary_same(c, factors.numerator[i], factors.denominator[j])) {
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

enum { elementary_atom_limit = 32 };
typedef struct {
    const expr_t *items[elementary_atom_limit];
    size_t count;
} elementary_atoms_t;

static const bool elementary_spectral_atoms[EXPR_KIND_COUNT] = {
    [EXPR_KIND_DIGAMMA           ] = true,
    [EXPR_KIND_E1                ] = true,
    [EXPR_KIND_EI                ] = true,
    [EXPR_KIND_EXP               ] = true,
    [EXPR_KIND_BESSEL_Y          ] = true,
    [EXPR_KIND_HYPERGEOMETRIC_PFQ] = true,
    [EXPR_KIND_SUMMATION         ] = true,
};

static bool elementary_collect_atoms(elementary_pair_t *c, const expr_t *f,
                                     elementary_atoms_t *atoms, unsigned depth)
{
    if (!f)
        return true;
    if (depth > 64u)
        return false;
    if (elementary_spectral_atoms[f->ops->kind] && elementary_uses(f, c->s)) {
        if (atoms->count == elementary_atom_limit)
            return false;
        atoms->items[atoms->count++] = f;
        return true;
    }
    return elementary_collect_atoms(c, f->a, atoms, depth + 1u) &&
           elementary_collect_atoms(c, f->b, atoms, depth + 1u);
}

/* Pair formulae normally contain at most three spectral special-function atoms. Match their
 * arguments algebraically before cancelling a rational combination of them; a parser's s*s
 * and a constructor's s^2 must denote the same hypergeometric argument. The bounded fallback
 * rejects over-large atom sets rather than scanning an ever-growing catalogue. */
static expr_t *elementary_align_atoms(elementary_pair_t *c, const expr_t *f,
                                      const elementary_atoms_t *atoms, unsigned depth)
{
    if (!f || depth > 64u)
        return elementary_keep(c, expr_clone(f));
    if (elementary_spectral_atoms[f->ops->kind]) {
        for (size_t i = 0u; i < atoms->count; ++i)
            if (f->ops == atoms->items[i]->ops && elementary_same(c, f, atoms->items[i]))
                return elementary_keep(c, expr_clone(atoms->items[i]));
    }
    expr_t *out = elementary_keep(c, expr_clone(f));
    if (out->a) {
        expr_free(out->a);
        out->a = expr_clone(elementary_align_atoms(c, f->a, atoms, depth + 1u));
    }
    if (out->b) {
        expr_free(out->b);
        out->b = expr_clone(elementary_align_atoms(c, f->b, atoms, depth + 1u));
    }
    return out;
}

/* Remove matched special-function coefficients before asking for the constant offset. In
 * particular, (c-k*s*H(s))/s+k*H(s) is c/s even when the generic simplifier keeps H opaque.
 * Every removed coefficient must cancel exactly; no spectral term is silently discarded. */
static expr_t *elementary_offset(elementary_pair_t *c, const expr_t *left, const expr_t *right,
                                  const expr_t *scale, const elementary_atoms_t *atoms)
{
    for (size_t i = 0u; i < atoms->count; ++i) {
        expr_t *la = NULL, *lb = NULL, *ra = NULL, *rb = NULL;
        if (!elementary_linear(c, left, atoms->items[i], &la, &lb) ||
            !elementary_linear(c, right, atoms->items[i], &ra, &rb))
            return NULL;
        expr_t *difference = elementary_reduced(c, elementary_sub(c, la, elementary_mul(c, scale, ra)));
        if (!expr_const_is_zero(difference))
            return NULL;
        left = lb;
        right = rb;
    }
    expr_t *remainder = elementary_sub(c, left, elementary_mul(c, scale, right));
    expr_t *offset = elementary_reduced(c, elementary_mul(c, c->s, remainder));
    return elementary_uses(offset, c->s) ? NULL : offset;
}

static expr_t *elementary_verify(elementary_pair_t *c, const expr_t *candidate, const expr_t *feature)
{
    if (!candidate || elementary_uses(candidate, c->s))
        return NULL;
    expr_t *args[] = {(expr_t *)candidate, (expr_t *)c->t, (expr_t *)c->s};
    expr_t *transform = elementary_keep(c, expr_laplace_from_args(3u, args));
    if (!transform)
        return NULL;
    number_t bound = NUM_ZERO;
    expr_t *conditions = NULL;
    expr_t *formula = elementary_keep(c, expr_laplace_formula(transform, &bound, &conditions));
    elementary_keep(c, conditions);
    num_destroy(&bound);
    if (!formula)
        return NULL;
    formula = elementary_normalise_sums(c, formula, 0u);
    elementary_atoms_t atoms = {0};
    if (!elementary_collect_atoms(c, c->f, &atoms, 0u))
        return NULL;
    formula = elementary_align_atoms(c, formula, &atoms, 0u);
    expr_t *scale = elementary_scalar_ratio(c, c->f, formula);
    if (!scale)
        scale = elementary_reduced(c, elementary_div(c, c->f, formula));
    expr_t *offset = elementary_integer(c, 0);
    if (elementary_uses(scale, c->s)) {
        if (expr_const_is_zero(elementary_reduced(c, elementary_sub(c, c->f, formula))))
            scale = elementary_integer(c, 1);
        else if (expr_const_is_zero(elementary_reduced(c, elementary_add(c, c->f, formula))))
            scale = elementary_integer(c, -1);
    }
    if (elementary_uses(scale, c->s)) {
        expr_t *fa = NULL, *fb = NULL, *ga = NULL, *gb = NULL;
        if (!feature || !elementary_linear(c, c->f, feature, &fa, &fb) ||
            !elementary_linear(c, formula, feature, &ga, &gb) || expr_const_is_zero(ga))
            return NULL;
        scale = elementary_scalar_ratio(c, fa, ga);
        if (!scale)
            scale = elementary_reduced(c, elementary_div(c, fa, ga));
        if (elementary_uses(scale, c->s))
            return NULL;
        offset = elementary_reduced(c, elementary_mul(c, c->s, elementary_sub(c, fb, elementary_mul(c, scale, gb))));
        if (elementary_uses(offset, c->s))
            offset = elementary_offset(c, fb, gb, scale, &atoms);
        if (!offset)
            return NULL;
    }
    if (expr_const_is_zero(scale))
        return NULL;
    if (c->exact_pair && (!expr_const_is_one(scale) || !expr_const_is_zero(offset)))
        return NULL;
    expr_t *body = elementary_clean(c, elementary_add(c, elementary_mul(c, scale, candidate), offset));
    expr_t *zero = elementary_integer(c, 0);
    expr_t *domain_args[] = {body, (expr_t *)c->t, zero};
    expr_t *out = elementary_keep(c, expr_real_domain_from_args(3u, domain_args));
    if (!out)
        return NULL;
    expr_t **tail = &out->b->b->b;
    for (const expr_t *item = conditions; item; item = item->b) {
        bool half_plane = expr_is_op(item->a, &ops_argument_list);
        const expr_t *value = half_plane ? item->a->a : item->a;
        const expr_t *limit = half_plane ? item->a->b : NULL;
        if (elementary_uses(value, c->s) || (limit && elementary_uses(limit, c->s)))
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

static expr_t *elementary_unary_candidate(elementary_pair_t *c, const expr_ops_t *ops, const expr_t *rate,
                                    const expr_t *feature)
{
    expr_t *argument = elementary_clean(c, elementary_mul(c, rate, c->t));
    expr_t *candidate = elementary_keep(c, ops->apply_unary(argument));
    return elementary_verify(c, candidate, feature);
}


/* Only literal real rates are admitted where the forward rule has a real-axis branch convention.
 * A supplied value on a free variable must not turn it into a known constant. */
static bool elementary_literal_real(const expr_t *f, bool positive)
{
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(f);
    bool literal = expr_bindings_count(bindings) == 0u;
    expr_bindings_free(bindings);
    if (!literal)
        return false;
    number_t value = expr_eval(f);
    bool valid = num_is_finite(value) && num_is_real(value) && !num_is_zero(value) &&
                 (!positive || num_gt(value, NUM_ZERO));
    num_destroy(&value);
    return valid;
}

/* The exponent of s in a Laurent monomial identifies a Clausen order directly, without trying
 * every entry in the catalogue. Source-independent factors may be symbolic. */
static bool elementary_monomial_degree(elementary_pair_t *c, const expr_t *f, long *degree, unsigned depth)
{
    if (!f || depth > 64u)
        return false;
    if (!elementary_uses(f, c->s)) {
        *degree = 0;
        return true;
    }
    if (expr_struct_eq(f, c->s)) {
        *degree = 1;
        return true;
    }
    if (f->ops == &ops_neg)
        return elementary_monomial_degree(c, f->a, degree, depth + 1u);
    long a = 0, b = 0;
    if ((f->ops == &ops_mul || f->ops == &ops_div) &&
        elementary_monomial_degree(c, f->a, &a, depth + 1u) &&
        elementary_monomial_degree(c, f->b, &b, depth + 1u)) {
        *degree = a + (f->ops == &ops_div ? -b : b);
        return *degree >= -64 && *degree <= 64;
    }
    const expr_t *base = NULL, *power = NULL;
    number_t value = NUM_NAN;
    long numerator = 0;
    bool valid = elementary_power(c, f, &base, &power) && expr_match_const_value(power, &value) &&
                 elementary_small_integer(value, &numerator) &&
                 numerator >= -32 && numerator <= 32 &&
                 elementary_monomial_degree(c, base, &a, depth + 1u);
    num_destroy(&value);
    if (!valid)
        return false;
    *degree = a * numerator;
    return *degree >= -64 && *degree <= 64;
}

/* Digamma offsets distinguish the hyperbolic pairs from the conjugate Clausen pair. */
static expr_t *elementary_digamma(elementary_pair_t *c, const expr_t *node)
{
    expr_t *slope = NULL, *offset = NULL;
    if (!elementary_affine(c, node->a, &slope, &offset))
        return NULL;
    expr_t *one = elementary_integer(c, 1), *four = elementary_integer(c, 4);
    expr_t *quarter = elementary_div(c, one, four);
    expr_t *three_quarters = elementary_div(c, elementary_integer(c, 3), four);
    expr_t *rate = elementary_clean(c, elementary_div(c, one, elementary_mul(c, four, slope)));
    if (elementary_equal(c, offset, quarter) || elementary_equal(c, offset, three_quarters))
        return elementary_unary_candidate(c, &ops_sech, rate, node);

    /* Imaginary slopes occur in Cl_p, not in the real-scale hyperbolic pairs. */
    expr_t *imaginary = elementary_keep(c, expr_new_const(NUM_I));
    expr_t *clausen_rate = elementary_clean(c, elementary_div(c, imaginary, slope));
    if (elementary_equal(c, offset, one) && elementary_literal_real(clausen_rate, false)) {
        expr_t *coefficient = NULL, *remainder = NULL;
        long degree = 0;
        if (!elementary_linear(c, c->f, node, &coefficient, &remainder) ||
            !elementary_monomial_degree(c, coefficient, &degree, 0u) || degree >= 0 || degree < -32)
            return NULL;
        if (!elementary_literal_real(clausen_rate, true))
            clausen_rate = elementary_clean(c, elementary_neg(c, clausen_rate));
        expr_t *order = elementary_integer(c, -degree);
        expr_t *argument = elementary_clean(c, elementary_mul(c, clausen_rate, c->t));
        expr_t *candidate = elementary_keep(c, expr_clausen_xp(order, argument));
        return elementary_verify(c, candidate, node);
    }
    expr_t *half = elementary_div(c, one, elementary_integer(c, 2));
    if (expr_const_is_zero(offset) || elementary_equal(c, offset, half) || elementary_equal(c, offset, one))
        return elementary_unary_candidate(c, &ops_tanh, rate, node);
    return NULL;
}

/* exp(s/q) fixes the staircase spacing. The four bounded alternatives account for ceil/floor
 * and a negative real scale; complete verification rejects isolated delay exponentials. */
static expr_t *elementary_staircase(elementary_pair_t *c, const expr_t *node)
{
    const expr_t *argument = node->a;
    if (node->ops != &ops_exp) {
        const expr_t *base = NULL, *power = NULL;
        if (!elementary_power(c, node, &base, &power) || !expr_is_const(base) || !num_eq(base->c, NUM_E))
            return NULL;
        argument = power;
    }
    expr_t *slope = NULL, *offset = NULL;
    if (!elementary_affine(c, argument, &slope, &offset) || !expr_const_is_zero(offset))
        return NULL;
    expr_t *rate = elementary_clean(c, elementary_div(c, elementary_integer(c, 1), slope));
    if (!elementary_literal_real(rate, true))
        return NULL;
    expr_t *out = elementary_unary_candidate(c, &ops_floor, rate, NULL);
    if (!out)
        out = elementary_unary_candidate(c, &ops_ceil, rate, NULL);
    if (!out) {
        rate = elementary_clean(c, elementary_neg(c, rate));
        out = elementary_unary_candidate(c, &ops_floor, rate, NULL);
        if (!out)
            out = elementary_unary_candidate(c, &ops_ceil, rate, NULL);
    }
    return out;
}

/* The E1/Ei arguments determine the real scale. Replaying the whole pair checks both conjugate
 * phases and, for atanh, the +i*pi*exp(-s/|a|) term on both exterior real cuts. */
static expr_t *elementary_inverse_circular(elementary_pair_t *c, const expr_t *node)
{
    expr_t *slope = NULL, *offset = NULL;
    if (!elementary_affine(c, node->a, &slope, &offset) || !expr_const_is_zero(offset))
        return NULL;
    expr_t *rate = elementary_clean(c, elementary_div(c, elementary_integer(c, 1), slope));
    expr_t *out = NULL;
    if (node->ops == &ops_E1) {
        expr_t *imaginary = elementary_keep(c, expr_new_const(NUM_I));
        expr_t *circular_rate = elementary_clean(c, elementary_mul(c, imaginary, rate));
        if (elementary_literal_real(circular_rate, false)) {
            out = elementary_unary_candidate(c, &ops_atan, circular_rate, node);
            if (!out)
                out = elementary_unary_candidate(c, &ops_acot, circular_rate, node);
            return out;
        }
    }
    if (!elementary_literal_real(rate, true))
        return NULL;
    out = elementary_unary_candidate(c, &ops_atanh, rate, node);
    if (!out)
        out = elementary_unary_candidate(c, &ops_atanh, elementary_clean(c, elementary_neg(c, rate)), node);
    return out;
}

/* Reconstruct a frequency-shifted candidate, then verify the complete forward spectrum. */
static expr_t *elementary_shifted_hyperbolic_candidate(elementary_pair_t *c, const expr_ops_t *ops,
                                                      const expr_t *rate, const expr_t *shift,
                                                      const expr_t *feature)
{
    expr_t *argument = elementary_clean(c, elementary_mul(c, rate, c->t));
    expr_t *candidate = elementary_keep(c, ops->apply_unary(argument));
    if (!expr_const_is_zero(shift)) {
        expr_t *exponent = elementary_clean(c, elementary_neg(c, elementary_mul(c, shift, c->t)));
        expr_t *factor = elementary_keep(c, expr_exp(exponent));
        candidate = elementary_mul(c, factor, candidate);
    }
    return elementary_verify(c, candidate, feature);
}

/* Y_0(s/q) and K_0(s/q) are only features here: the Struve/hypergeometric companion must
 * also be present. This does not implement or intercept the inverse Bessel family itself. */
static expr_t *elementary_inverse_hyperbolic(elementary_pair_t *c, const expr_t *node)
{
    if (!expr_const_is_zero(node->a))
        return NULL;
    expr_t *slope = NULL, *offset = NULL;
    if (!elementary_affine(c, node->b, &slope, &offset))
        return NULL;
    expr_t *rate = elementary_clean(c, elementary_div(c, elementary_integer(c, 1), slope));
    if (!elementary_literal_real(rate, true))
        return NULL;
    bool cosine = node->ops == &ops_bessel_k;
    expr_t *shift = elementary_clean(c, elementary_div(c, offset, slope));
    if (cosine) {
        /* Six bounded alternatives distinguish identical Bessel features by their complete
         * spectra, before the general scalar/offset matcher can replace a circular function
         * with an equivalent affine expression in acosh. */
        static const expr_ops_t *const families[] = {&ops_acosh, &ops_asin, &ops_acos};
        expr_t *rates[] = {rate, elementary_clean(c, elementary_neg(c, rate))};
        c->exact_pair = true;
        for (size_t family = 0u; family < sizeof(families) / sizeof(families[0]); ++family) {
            for (size_t sign = 0u; sign < 2u; ++sign) {
                expr_t *exact = elementary_shifted_hyperbolic_candidate(c, families[family], rates[sign], shift, node);
                if (exact) {
                    c->exact_pair = false;
                    return exact;
                }
            }
        }
        c->exact_pair = false;
    }
    expr_t *out = elementary_shifted_hyperbolic_candidate(c, cosine ? &ops_acosh : &ops_asinh, rate, shift, node);
    if (!out && cosine)
        out = elementary_shifted_hyperbolic_candidate(c, &ops_acosh, elementary_clean(c, elementary_neg(c, rate)),
                                                       shift, node);
    return out;
}

typedef expr_t *(*elementary_feature_fn)(elementary_pair_t *, const expr_t *);
static const elementary_feature_fn elementary_features[EXPR_KIND_COUNT] = {
    [EXPR_KIND_DIGAMMA ] = elementary_digamma,
    [EXPR_KIND_EXP     ] = elementary_staircase,
    [EXPR_KIND_POW     ] = elementary_staircase,
    [EXPR_KIND_POW_D   ] = elementary_staircase,
    [EXPR_KIND_E1      ] = elementary_inverse_circular,
    [EXPR_KIND_EI      ] = elementary_inverse_circular,
    [EXPR_KIND_BESSEL_Y] = elementary_inverse_hyperbolic,
    [EXPR_KIND_BESSEL_K] = elementary_inverse_hyperbolic,
};

/* Traversal is linear in the input tree, not in the transform catalogue. Stop at the first
 * completely verified pair; failed feature guesses leave the original spectrum untouched. */
static expr_t *elementary_visit(elementary_pair_t *c, const expr_t *node, unsigned depth)
{
    if (!node || c->failed || depth > 64u)
        return NULL;
    elementary_feature_fn match = elementary_features[node->ops->kind];
    expr_t *out = match ? match(c, node) : NULL;
    if (!out)
        out = elementary_visit(c, node->a, depth + 1u);
    if (!out)
        out = elementary_visit(c, node->b, depth + 1u);
    return out;
}

/* Recover elementary-transcendental pairs from spectral algebra, preserving parameter conditions. */
expr_t *expr_inverse_laplace_elementary_pair(const expr_t *f, const expr_t *s, const expr_t *t)
{
    if (!f || !s || !t)
        return NULL;
    elementary_pair_t context = {.f = f, .s = s, .t = t};
    context.f = elementary_normalise_sums(&context, f, 0u);
    expr_t *matched = elementary_visit(&context, context.f, 0u);
    expr_t *out = matched && !context.failed ? expr_clone(matched) : NULL;
    for (size_t i = context.count; i > 0u; --i)
        expr_free(context.nodes[i - 1u]);
    free(context.nodes);
    return out;
}
