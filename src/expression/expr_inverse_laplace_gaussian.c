#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#include <stdlib.h>
#include <string.h>

/* These pairs share exp(q(s))*erfc(c*s+d), optionally divided by s. Collect only the
 * algebra surrounding this kernel, never the provenance of a previous transform. */
enum { gaussian_term_limit = 16, gaussian_depth_limit = 64 };

typedef struct {
    expr_t **nodes;
    size_t count, capacity;
    expr_t *conditions;
    bool failed;
} gaussian_context_t;

typedef struct {
    const expr_t *coefficient, *exponential, *complement;
    long power;
} gaussian_term_t;

typedef struct {
    gaussian_term_t terms[gaussian_term_limit];
    size_t count;
} gaussian_terms_t;

static expr_t *gaussian_keep(gaussian_context_t *c, expr_t *owned)
{
    if (!owned) {
        c->failed = true;
        return NULL;
    }
    if (c->count == c->capacity) {
        size_t capacity = c->capacity ? 2u * c->capacity : 64u;
        expr_t **nodes = realloc(c->nodes, capacity * sizeof(*nodes));
        if (!nodes) {
            expr_free(owned);
            c->failed = true;
            return NULL;
        }
        c->nodes = nodes;
        c->capacity = capacity;
    }
    c->nodes[c->count++] = owned;
    return owned;
}

#define GAUSSIAN_UNARY(name) \
    static expr_t *g_##name(gaussian_context_t *c, const expr_t *a) \
    { return gaussian_keep(c, expr_##name(a)); }
#define GAUSSIAN_BINARY(name) \
    static expr_t *g_##name(gaussian_context_t *c, const expr_t *a, const expr_t *b) \
    { return gaussian_keep(c, expr_##name(a, b)); }
GAUSSIAN_UNARY(neg)
GAUSSIAN_UNARY(exp)
GAUSSIAN_UNARY(erf)
GAUSSIAN_UNARY(sqrt)
GAUSSIAN_UNARY(abs)
GAUSSIAN_BINARY(add)
GAUSSIAN_BINARY(sub)
GAUSSIAN_BINARY(mul)
GAUSSIAN_BINARY(div)
#undef GAUSSIAN_BINARY
#undef GAUSSIAN_UNARY

static expr_t *g_integer(gaussian_context_t *c, long n)
{
    return gaussian_keep(c, expr_const_long(n));
}

static expr_t *g_clean(gaussian_context_t *c, const expr_t *f)
{
    return gaussian_keep(c, expr_simplify(f));
}

static bool gaussian_uses(const expr_t *f, const expr_t *s)
{
    bool used = true;
    expr_t *variable = (expr_t *)s;
    return !expr_collect_var_usage(f, 1u, &variable, &used) || used;
}

static bool gaussian_integer_power(const expr_t *f, const expr_t **base, long *power)
{
    number_t value = NUM_NAN;
    const expr_t *exponent = NULL;
    bool matched = expr_match_pow_const(f, base, &value);
    if (!matched && expr_match_pow_expr(f, base, &exponent))
        matched = expr_match_const_value(exponent, &value);
    long denominator = 0;
    bool valid = matched && num_get_small_rational(value, power, &denominator) && denominator == 1 &&
                 *power >= -2 && *power <= 2;
    num_destroy(&value);
    return valid;
}

/* A degree-two structural collector preserves symbolic coefficients and does not evaluate bindings. */
static bool gaussian_polynomial(gaussian_context_t *c, const expr_t *f, const expr_t *s,
                                expr_t **p, unsigned depth)
{
    if (!f || depth > gaussian_depth_limit || c->failed)
        return false;
    for (size_t i = 0; i < 3u; ++i)
        p[i] = g_integer(c, 0);
    if (!gaussian_uses(f, s)) {
        p[0] = gaussian_keep(c, expr_clone(f));
        return true;
    }
    if (expr_struct_eq(f, s)) {
        p[1] = g_integer(c, 1);
        return true;
    }
    expr_t *a[3], *b[3];
    const expr_t *base = NULL;
    long power = 0;
    if (gaussian_integer_power(f, &base, &power) && power >= 0) {
        if (!power) {
            p[0] = g_integer(c, 1);
            return true;
        }
        if (power == 1)
            return gaussian_polynomial(c, base, s, p, depth + 1u);
        if (!gaussian_polynomial(c, base, s, a, depth + 1u) || !expr_const_is_zero(a[2]))
            return false;
        p[0] = g_clean(c, g_mul(c, a[0], a[0]));
        p[1] = g_clean(c, g_mul(c, g_integer(c, 2), g_mul(c, a[0], a[1])));
        p[2] = g_clean(c, g_mul(c, a[1], a[1]));
        return true;
    }
    if (!gaussian_polynomial(c, f->a, s, a, depth + 1u))
        return false;
    if (f->ops == &ops_neg) {
        for (size_t i = 0; i < 3u; ++i)
            p[i] = g_clean(c, g_neg(c, a[i]));
        return true;
    }
    if (f->ops == &ops_div && !gaussian_uses(f->b, s)) {
        for (size_t i = 0; i < 3u; ++i)
            p[i] = g_clean(c, g_div(c, a[i], f->b));
        return true;
    }
    if ((f->ops != &ops_add && f->ops != &ops_sub && f->ops != &ops_mul) ||
        !gaussian_polynomial(c, f->b, s, b, depth + 1u))
        return false;
    if (f->ops != &ops_mul) {
        for (size_t i = 0; i < 3u; ++i)
            p[i] = g_clean(c, f->ops == &ops_add ? g_add(c, a[i], b[i]) : g_sub(c, a[i], b[i]));
        return true;
    }
    for (size_t i = 0; i < 3u; ++i) {
        for (size_t j = 0; j < 3u; ++j) {
            if (i + j >= 3u) {
                if (!expr_const_is_zero(a[i]) && !expr_const_is_zero(b[j]))
                    return false;
            } else {
                p[i + j] = g_clean(c, g_add(c, p[i + j], g_mul(c, a[i], b[j])));
            }
        }
    }
    return true;
}

/* Integer squaring may cross products, quotients and principal square roots without
 * changing branches. In particular (-1/sqrt(2))^2 is exactly 1/2, not a rounded
 * constant to compare against a separately evaluated reciprocal scale. */
static expr_t *gaussian_exact_square(gaussian_context_t *c, const expr_t *f, unsigned depth)
{
    if (!f || depth > gaussian_depth_limit)
        return NULL;
    if (expr_is_const(f) && f->binding_expr) {
        expr_t *expanded = gaussian_keep(c, expr_expand_preserved_for_display(f));
        if (expanded && !expr_struct_eq(expanded, f))
            return gaussian_exact_square(c, expanded, depth + 1u);
    }
    if (f->ops == &ops_neg)
        return gaussian_exact_square(c, f->a, depth + 1u);
    if (f->ops == &ops_sqrt)
        return gaussian_keep(c, expr_clone(f->a));
    if (f->ops == &ops_add || f->ops == &ops_sub) {
        expr_t *a = gaussian_exact_square(c, f->a, depth + 1u);
        expr_t *b = gaussian_exact_square(c, f->b, depth + 1u);
        if (!a || !b)
            return NULL;
        expr_t *cross = g_mul(c, g_integer(c, 2), g_mul(c, f->a, f->b));
        expr_t *sum = g_add(c, a, b);
        return g_clean(c, f->ops == &ops_add ? g_add(c, sum, cross) : g_sub(c, sum, cross));
    }
    if (f->ops == &ops_mul || f->ops == &ops_div) {
        expr_t *a = gaussian_exact_square(c, f->a, depth + 1u);
        expr_t *b = gaussian_exact_square(c, f->b, depth + 1u);
        if (!a || !b)
            return NULL;
        return g_clean(c, f->ops == &ops_mul ? g_mul(c, a, b) : g_div(c, a, b));
    }
    return g_clean(c, g_mul(c, f, f));
}

static bool gaussian_positive_real_literal(const expr_t *f)
{
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(f);
    bool literal = expr_bindings_count(bindings) == 0u;
    expr_bindings_free(bindings);
    if (!literal)
        return false;
    number_t value = expr_eval(f);
    bool positive = num_is_finite(value) && num_is_real(value) && num_gt(value, NUM_ZERO);
    num_destroy(&value);
    return positive;
}

/* Split a scalar times one radical. This bounded factor walk is used only for
 * branch-safe positive-real root products, not for approximate constant matching. */
static bool gaussian_root_factor(gaussian_context_t *c, const expr_t *f,
                                 expr_t **coefficient, const expr_t **radicand, unsigned depth)
{
    if (!f || depth > gaussian_depth_limit)
        return false;
    if (expr_is_const(f) && f->binding_expr) {
        expr_t *expanded = gaussian_keep(c, expr_expand_preserved_for_display(f));
        if (expanded && !expr_struct_eq(expanded, f))
            return gaussian_root_factor(c, expanded, coefficient, radicand, depth + 1u);
    }
    if (f->ops == &ops_sqrt && gaussian_positive_real_literal(f->a)) {
        *coefficient = g_integer(c, 1);
        *radicand = f->a;
        return true;
    }
    if (f->ops == &ops_neg && gaussian_root_factor(c, f->a, coefficient, radicand, depth + 1u)) {
        *coefficient = g_neg(c, *coefficient);
        return true;
    }
    if (f->ops != &ops_mul && f->ops != &ops_div)
        return false;
    if (gaussian_root_factor(c, f->a, coefficient, radicand, depth + 1u)) {
        *coefficient = f->ops == &ops_mul ? g_mul(c, *coefficient, f->b) : g_div(c, *coefficient, f->b);
        return true;
    }
    if (f->ops == &ops_mul && gaussian_root_factor(c, f->b, coefficient, radicand, depth + 1u)) {
        *coefficient = g_mul(c, f->a, *coefficient);
        return true;
    }
    return false;
}

/* Normalise only exact algebraic identities. Never infer equality by evaluating
 * closed transcendental constants, and never replace sqrt(z^2) with z. */
static expr_t *gaussian_exact_algebra(gaussian_context_t *c, const expr_t *f, unsigned depth)
{
    if (!f || depth > gaussian_depth_limit)
        return NULL;
    const expr_t *base = NULL;
    long power = 0;
    if (gaussian_integer_power(f, &base, &power) && (power == 2 || power == -2)) {
        expr_t *a = gaussian_exact_algebra(c, base, depth + 1u);
        expr_t *square = a ? gaussian_exact_square(c, a, depth + 1u) : NULL;
        return square && power == -2 ? g_clean(c, g_div(c, g_integer(c, 1), square)) : square;
    }
    if (f->ops == &ops_neg || f->ops == &ops_sqrt || f->ops == &ops_exp) {
        expr_t *a = gaussian_exact_algebra(c, f->a, depth + 1u);
        if (a && f->ops == &ops_exp)
            return g_exp(c, g_clean(c, a));
        /* Retain the radical structurally until its enclosing square is reduced. */
        return a ? gaussian_keep(c, expr_new_unary_internal(f->ops, expr_clone(a))) : NULL;
    }
    if (f->ops != &ops_add && f->ops != &ops_sub && f->ops != &ops_mul && f->ops != &ops_div)
        return gaussian_keep(c, expr_clone(f));
    expr_t *a = gaussian_exact_algebra(c, f->a, depth + 1u);
    expr_t *b = gaussian_exact_algebra(c, f->b, depth + 1u);
    if (!a || !b)
        return NULL;
    if (f->ops == &ops_mul && expr_struct_eq(a, b))
        return gaussian_exact_square(c, a, depth + 1u);
    if (f->ops == &ops_mul) {
        expr_t *left_scale = NULL, *right_scale = NULL;
        const expr_t *left_root = NULL, *right_root = NULL;
        if (gaussian_root_factor(c, a, &left_scale, &left_root, 0u) &&
            gaussian_root_factor(c, b, &right_scale, &right_root, 0u)) {
            expr_t *product = g_clean(c, g_mul(c, left_root, right_root));
            expr_t *root = g_sqrt(c, product);
            return g_clean(c, g_mul(c, g_mul(c, left_scale, right_scale), root));
        }
    }
    expr_t *out = gaussian_keep(c, expr_new_binary_internal(f->ops, expr_clone(a), expr_clone(b)));
    return g_clean(c, out);
}

static expr_t *gaussian_exact_clean(gaussian_context_t *c, const expr_t *f)
{
    expr_t *expanded = gaussian_keep(c, expr_expand_preserved_for_display(f));
    expr_t *exact = gaussian_exact_algebra(c, expanded, 0u);
    return exact ? g_clean(c, exact) : NULL;
}

/* Clear nested rational coefficients before testing identities such as
 * 2*(1/(2*sqrt(a^2)))*(b*sqrt(a^2)/a)=b/a. No parameter is sampled. */
static bool gaussian_fraction(gaussian_context_t *c, const expr_t *f, expr_t **n, expr_t **d, unsigned depth)
{
    if (!f || depth > gaussian_depth_limit)
        return false;
    const expr_t *base = NULL;
    long power = 0;
    expr_t *an = NULL, *ad = NULL, *bn = NULL, *bd = NULL;
    if (gaussian_integer_power(f, &base, &power) && power != 0) {
        if (!gaussian_fraction(c, base, &an, &ad, depth + 1u))
            return false;
        if (power == 2 || power == -2) {
            an = gaussian_exact_square(c, an, depth + 1u);
            ad = gaussian_exact_square(c, ad, depth + 1u);
        }
        *n = power < 0 ? ad : an;
        *d = power < 0 ? an : ad;
        return *n && *d;
    }
    if (f->ops != &ops_add && f->ops != &ops_sub && f->ops != &ops_mul &&
        f->ops != &ops_div && f->ops != &ops_neg) {
        *n = gaussian_keep(c, expr_clone(f));
        *d = g_integer(c, 1);
        return true;
    }
    if (!gaussian_fraction(c, f->a, &an, &ad, depth + 1u))
        return false;
    if (f->ops == &ops_neg) {
        *n = g_neg(c, an);
        *d = ad;
        return true;
    }
    if (!gaussian_fraction(c, f->b, &bn, &bd, depth + 1u))
        return false;
    if (f->ops == &ops_add || f->ops == &ops_sub) {
        expr_t *left = g_mul(c, an, bd), *right = g_mul(c, bn, ad);
        *n = f->ops == &ops_add ? g_add(c, left, right) : g_sub(c, left, right);
        *d = g_mul(c, ad, bd);
    } else {
        *n = g_mul(c, an, f->ops == &ops_mul ? bn : bd);
        *d = g_mul(c, ad, f->ops == &ops_mul ? bd : bn);
    }
    return true;
}

static bool gaussian_same(gaussian_context_t *c, const expr_t *a, const expr_t *b)
{
    if (expr_struct_eq(a, b))
        return true;
    expr_t *difference = g_clean(c, g_sub(c, g_clean(c, a), g_clean(c, b)));
    if (expr_const_is_zero(difference))
        return true;
    difference = gaussian_keep(c, expr_expand_preserved_for_display(difference));
    difference = g_clean(c, gaussian_keep(c, expr_expand_products_internal(difference)));
    if (expr_const_is_zero(difference))
        return true;
    expr_t *exact_a = gaussian_keep(c, expr_expand_preserved_for_display(a));
    expr_t *exact_b = gaussian_keep(c, expr_expand_preserved_for_display(b));
    exact_a = gaussian_exact_algebra(c, exact_a, 0u);
    exact_b = gaussian_exact_algebra(c, exact_b, 0u);
    if (!exact_a || !exact_b)
        return false;
    if (expr_const_is_zero(g_clean(c, g_sub(c, exact_a, exact_b))))
        return true;
    expr_t *an = NULL, *ad = NULL, *bn = NULL, *bd = NULL;
    if (!gaussian_fraction(c, exact_a, &an, &ad, 0u) || !gaussian_fraction(c, exact_b, &bn, &bd, 0u))
        return false;
    difference = g_sub(c, g_mul(c, an, bd), g_mul(c, bn, ad));
    difference = gaussian_keep(c, expr_expand_products_internal(difference));
    difference = gaussian_exact_algebra(c, difference, 0u);
    return difference && expr_const_is_zero(g_clean(c, difference));
}

/* The limit is on the small algebraic expansion of a pair (normally one or two terms),
 * not a scan of a growing transform catalogue. Products of two erfc kernels are rejected. */
static bool gaussian_terms(gaussian_context_t *c, const expr_t *f, const expr_t *s,
                           gaussian_terms_t *out, unsigned depth)
{
    if (!f || depth > gaussian_depth_limit || c->failed)
        return false;
    gaussian_term_t term = {g_integer(c, 1), g_integer(c, 0), NULL, 0};
    const expr_t *base = NULL, *exponent = NULL;
    long power = 0;
    if (!gaussian_uses(f, s)) {
        term.coefficient = f;
    } else if (expr_struct_eq(f, s)) {
        term.power = 1;
    } else if (gaussian_integer_power(f, &base, &power) && expr_struct_eq(base, s)) {
        term.power = power;
    } else if (f->ops == &ops_erfc) {
        term.complement = f->a;
    } else if (f->ops == &ops_exp) {
        term.exponential = f->a;
    } else if (f->ops == &ops_cos || f->ops == &ops_sin) {
        /* A copied complex exponential may be displayed in Cartesian form.
         * Euler's identities hold for complex arguments as well as real ones. */
        expr_t *i = gaussian_keep(c, expr_new_const(NUM_I));
        expr_t *angle = g_mul(c, i, f->a);
        expr_t *two = g_integer(c, 2), *one = g_integer(c, 1);
        term.coefficient = g_div(c, one, f->ops == &ops_sin ? g_mul(c, two, i) : two);
        term.exponential = angle;
        out->terms[out->count++] = term;
        term.exponential = g_neg(c, angle);
        if (f->ops == &ops_sin)
            term.coefficient = g_neg(c, term.coefficient);
        out->terms[out->count++] = term;
        return true;
    } else if (expr_match_pow_expr(f, &base, &exponent) && expr_is_const(base) &&
               num_eq(base->c, NUM_E) && (!base->name || strcmp(base->name, "e") == 0)) {
        term.exponential = exponent;
    } else {
        gaussian_terms_t left = {0}, right = {0};
        if (!gaussian_terms(c, f->a, s, &left, depth + 1u))
            return false;
        if (f->ops == &ops_neg) {
            *out = left;
            for (size_t i = 0; i < out->count; ++i)
                out->terms[i].coefficient = g_neg(c, out->terms[i].coefficient);
            return true;
        }
        if ((f->ops != &ops_add && f->ops != &ops_sub && f->ops != &ops_mul && f->ops != &ops_div) ||
            !gaussian_terms(c, f->b, s, &right, depth + 1u))
            return false;
        if (f->ops == &ops_add || f->ops == &ops_sub) {
            if (left.count + right.count > gaussian_term_limit)
                return false;
            *out = left;
            for (size_t i = 0; i < right.count; ++i) {
                term = right.terms[i];
                if (f->ops == &ops_sub)
                    term.coefficient = g_neg(c, term.coefficient);
                out->terms[out->count++] = term;
            }
            return true;
        }
        bool divide = f->ops == &ops_div;
        if ((divide && (right.count != 1u || right.terms[0].complement)) ||
            left.count * right.count > gaussian_term_limit)
            return false;
        for (size_t i = 0; i < left.count; ++i) {
            for (size_t j = 0; j < right.count; ++j) {
                const gaussian_term_t *a = &left.terms[i], *b = &right.terms[j];
                if (a->complement && b->complement)
                    return false;
                term.coefficient = divide ? g_div(c, a->coefficient, b->coefficient)
                                          : g_mul(c, a->coefficient, b->coefficient);
                term.exponential = divide ? g_sub(c, a->exponential, b->exponential)
                                          : g_add(c, a->exponential, b->exponential);
                term.complement = a->complement ? a->complement : b->complement;
                term.power = a->power + (divide ? -b->power : b->power);
                out->terms[out->count++] = term;
            }
        }
        return true;
    }
    out->terms[out->count++] = term;
    return true;
}

/* Literal parameters may discharge a restriction; bound variables must remain symbolic. */
static bool gaussian_positive(gaussian_context_t *c, const expr_t *f)
{
    f = g_clean(c, f);
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(f);
    bool literal = expr_bindings_count(bindings) == 0u;
    expr_bindings_free(bindings);
    if (literal) {
        number_t value = expr_eval(f), real = num_real_part(value);
        bool valid = num_is_finite(value) && num_gt(real, NUM_ZERO);
        num_destroy(&real);
        num_destroy(&value);
        return valid;
    }
    expr_t *condition = expr_alloc(&ops_argument_list);
    condition->a = expr_clone(f);
    condition->b = expr_alloc(&ops_argument_list);
    condition->b->a = expr_const_zero();
    condition->b->b = c->conditions;
    c->conditions = condition;
    return true;
}

static expr_t *gaussian_inverse_term(gaussian_context_t *c, const gaussian_term_t *term,
                                    const expr_t *s, const expr_t *t, expr_t **impulses)
{
    expr_t *z[3], *q[3];
    if (!gaussian_polynomial(c, term->exponential, s, q, 0u))
        return NULL;
    if (!term->complement) {
        if (!expr_const_is_zero(q[1]) || !expr_const_is_zero(q[2]))
            return NULL;
        expr_t *coefficient = g_clean(c, g_mul(c, term->coefficient, g_exp(c, q[0])));
        if (term->power == -1)
            return coefficient;
        if (term->power < 0 || term->power > 2)
            return NULL;
        impulses[term->power] = g_add(c, impulses[term->power], coefficient);
        return g_integer(c, 0);
    }
    if (term->power < -1 || term->power > 2 ||
        !gaussian_polynomial(c, term->complement, s, z, 0u) || !expr_const_is_zero(z[2]))
        return NULL;
    expr_t *slope = z[1], *offset = z[0], *two = g_integer(c, 2), *four = g_integer(c, 4);
    expr_t *square = g_clean(c, g_mul(c, slope, slope));
    if (!gaussian_same(c, q[2], square) ||
        !gaussian_same(c, q[1], g_mul(c, two, g_mul(c, slope, offset))))
        return NULL;
    /* Re(c)>0 chooses the principal-root branch; Re(c^2)>0 is equivalent to
     * Re(1/(4c^2))>0. Neither can be dropped for complex or negative scales. */
    if (!gaussian_positive(c, slope) || !gaussian_positive(c, square))
        return NULL;
    if (term->power == -1) {
        expr_t *argument = g_add(c, g_div(c, t, g_mul(c, two, slope)), offset);
        expr_t *integral = g_sub(c, g_erf(c, argument), g_erf(c, offset));
        return g_mul(c, term->coefficient, g_mul(c, g_exp(c, q[0]), integral));
    }
    expr_t *quadratic = g_div(c, g_mul(c, t, t), g_mul(c, four, square));
    expr_t *linear = g_mul(c, g_div(c, offset, slope), t);
    /* Normalise the completed-square constant before constructing exp(constant).
     * Otherwise a preserved reciprocal radical can leave exp(9/8-(1/(2sqrt(2)))^2)
     * distinct from exp(1), preventing exact cancellation of the initial impulse. */
    expr_t *constant = gaussian_exact_clean(c, g_sub(c, q[0], g_mul(c, offset, offset)));
    if (!constant)
        return NULL;
    expr_t *power = g_sub(c, g_sub(c, constant, quadratic), linear);
    expr_t *pi = gaussian_keep(c, expr_new_named_const(NUM_PI, "@pi"));
    expr_t *normaliser = g_mul(c, slope, g_sqrt(c, pi));
    expr_t *scale = g_div(c, term->coefficient, normaliser);
    expr_t *body = g_mul(c, scale, g_exp(c, power));
    if (!term->power)
        return body;
    /* L{g'}=s*G-g(0). Track the initial-value impulses until the entire copied
     * spectrum has been checked: weighted Gaussian spectra cancel them exactly. */
    expr_t *initial = g_mul(c, scale, g_exp(c, constant));
    impulses[term->power - 1] = g_add(c, impulses[term->power - 1], initial);
    expr_t *inverse_variance = g_div(c, g_integer(c, 1), g_mul(c, two, square));
    expr_t *initial_slope = g_neg(c, g_div(c, offset, slope));
    expr_t *log_derivative = g_sub(c, initial_slope, g_mul(c, inverse_variance, t));
    if (term->power == 1)
        return g_mul(c, log_derivative, body);
    impulses[0] = g_add(c, impulses[0], g_mul(c, initial_slope, initial));
    return g_mul(c, g_sub(c, g_mul(c, log_derivative, log_derivative), inverse_variance), body);
}

static expr_t *gaussian_inverse_formula(gaussian_context_t *c, const expr_t *f,
                                       const expr_t *s, const expr_t *t)
{
    gaussian_terms_t terms = {0};
    expr_t *sum = g_integer(c, 0), *impulses[3];
    for (size_t i = 0; i < 3u; ++i)
        impulses[i] = g_integer(c, 0);
    bool has_kernel = false;
    if (!gaussian_terms(c, f, s, &terms, 0u))
        return NULL;
    /* The expansion has a fixed small bound. Combine identical kernels before
     * inversion so the unwanted half of an Euler sine/cosine pair cancels exactly. */
    for (size_t i = 0; i < terms.count; ++i) {
        gaussian_term_t *left = &terms.terms[i];
        for (size_t j = i + 1u; j < terms.count; ++j) {
            gaussian_term_t *right = &terms.terms[j];
            bool same_kernel = (!left->complement && !right->complement) ||
                               (left->complement && right->complement &&
                                gaussian_same(c, left->complement, right->complement));
            if (left->power != right->power || !same_kernel ||
                !gaussian_same(c, left->exponential, right->exponential))
                continue;
            left->coefficient = g_add(c, left->coefficient, right->coefficient);
            right->coefficient = g_integer(c, 0);
        }
    }
    for (size_t i = 0; i < terms.count; ++i) {
        if (gaussian_same(c, terms.terms[i].coefficient, g_integer(c, 0)))
            continue;
        expr_t *term = gaussian_inverse_term(c, &terms.terms[i], s, t, impulses);
        if (!term)
            return NULL;
        has_kernel |= terms.terms[i].complement != NULL;
        sum = g_add(c, sum, term);
    }
    /* Do not silently discard uncancelled deltas from an improper spectrum. This
     * hook returns ordinary functions only; distributional inverses belong elsewhere. */
    for (size_t i = 0; i < 3u; ++i) {
        if (!gaussian_same(c, impulses[i], g_integer(c, 0)))
            return NULL;
    }
    return has_kernel && !c->failed ? sum : NULL;
}

/* Infer one common frequency origin from an affine denominator. The bounded tree
 * walk visits algebraic factors only; it does not search inside arbitrary functions. */
static expr_t *gaussian_frequency_shift(gaussian_context_t *c, const expr_t *f,
                                       const expr_t *s, unsigned depth)
{
    if (!f || depth > gaussian_depth_limit || c->failed)
        return NULL;
    const expr_t *base = NULL, *denominator = f->ops == &ops_div ? f->b : NULL;
    long power = 0;
    if (!denominator && gaussian_integer_power(f, &base, &power) && power < 0)
        denominator = base;
    if (denominator && gaussian_uses(denominator, s)) {
        expr_t *p[3];
        if (gaussian_polynomial(c, denominator, s, p, 0u) && expr_const_is_zero(p[2]) &&
            !expr_const_is_zero(p[1]) && !expr_const_is_zero(p[0]) && gaussian_positive(c, g_abs(c, p[1])))
            return g_clean(c, g_div(c, p[0], p[1]));
    }
    if (f->ops != &ops_add && f->ops != &ops_sub && f->ops != &ops_mul &&
        f->ops != &ops_div && f->ops != &ops_neg)
        return NULL;
    expr_t *shift = gaussian_frequency_shift(c, f->a, s, depth + 1u);
    return shift ? shift : gaussian_frequency_shift(c, f->b, s, depth + 1u);
}

/* Recover Gaussian and error-function pairs from their actual spectral algebra. */
expr_t *expr_inverse_laplace_gaussian_pair(const expr_t *f, const expr_t *s, const expr_t *t)
{
    if (!f || !s || !t)
        return NULL;
    gaussian_context_t c = {0};
    expr_t *result = NULL, *sum = gaussian_inverse_formula(&c, f, s, t);
    if (!sum && !c.failed) {
        expr_free(c.conditions);
        c.conditions = NULL;
        expr_t *shift = gaussian_frequency_shift(&c, f, s, 0u);
        if (shift) {
            /* F(s+h) corresponds to exp(-h*t)*f(t), also for complex h. Substituting
             * into the whole spectrum ensures numerator and denominator agree. */
            expr_t *argument = g_sub(&c, s, shift);
            expr_t *unshifted = g_clean(&c, gaussian_keep(&c, expr_substitute(f, s, argument)));
            sum = gaussian_inverse_formula(&c, unshifted, s, t);
            if (sum)
                sum = g_mul(&c, g_exp(&c, g_neg(&c, g_mul(&c, shift, t))), sum);
        }
    }
    if (!sum || c.failed)
        goto cleanup;
    result = expr_beautify(sum);
    if (result && c.conditions) {
        expr_t *restricted = expr_alloc(&ops_real_domain);
        restricted->a = result;
        restricted->b = c.conditions;
        c.conditions = NULL;
        result = restricted;
    }
cleanup:
    expr_free(c.conditions);
    for (size_t i = c.count; i > 0u; --i)
        expr_free(c.nodes[i - 1u]);
    free(c.nodes);
    return result;
}
