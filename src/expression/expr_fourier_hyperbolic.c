/* Hyperbolic Fourier rules: power families, analytic spectra, odd pairs and beta inverses. */
#include "expr_fourier_internal.h"

/* ------------------------------------------------------------------ */
/* Power families and convergence */
/* ------------------------------------------------------------------ */

typedef struct {
    bool supported;
    bool reciprocal;
    bool singular;
} hyperbolic_family_t;

static const hyperbolic_family_t hyperbolic_families[EXPR_KIND_COUNT] = {
    [EXPR_KIND_SINH]   = {true, false, true},
    [EXPR_KIND_COSH]   = {true, false, false},
    [EXPR_KIND_SECH]   = {true, true,  false},
    [EXPR_KIND_COSECH] = {true, true,  true},
};

/* Match one hyperbolic power without composing principal powers across a branch cut.
 * In particular, cosech(u)^n and sinh(u)^(-n) have different phases for non-integral n. */
bool expr_fourier_hyperbolic_parts(const expr_t *f, const expr_t **argument, expr_t **power,
                                  expr_t **branch_power, bool *singular)
{
    *argument = NULL;
    *power = NULL;
    *branch_power = NULL;
    if (!f)
        return false;
    if (f->ops == &ops_div && expr_const_is_one(f->a)) {
        if (!expr_fourier_hyperbolic_parts(f->b, argument, power, branch_power, singular))
            return false;
        expr_t *negative = expr_neg(*power);
        expr_free(*power);
        *power = negative;
        if (*branch_power) {
            negative = expr_neg(*branch_power);
            expr_free(*branch_power);
            *branch_power = negative;
        }
        return true;
    }
    const expr_t *base = f;
    expr_t *exponent = NULL;
    if (f->ops == &ops_pow) {
        base = f->a;
        exponent = expr_clone(f->b);
    } else if (f->ops == &ops_pow_d) {
        base = f->a;
        exponent = expr_new_const(f->c);
    } else if (f->ops == &ops_sqrt) {
        base = f->a;
        exponent = expr_new_const(NUM_HALF);
    } else {
        exponent = expr_const_one();
    }
    bool absolute = base->ops == &ops_abs;
    if (absolute)
        base = base->a;
    const hyperbolic_family_t family = hyperbolic_families[base->ops->kind];
    if (!family.supported) {
        expr_free(exponent);
        return false;
    }
    *argument = base->a;
    *singular = family.singular;
    *branch_power = family.singular && !absolute ? expr_clone(exponent) : NULL;
    *power = family.reciprocal ? expr_neg(exponent) : expr_clone(exponent);
    expr_free(exponent);
    return true;
}

/* Describe ordinary convergence and branch choices separately from unsupported distributional extensions. */
const char *expr_fourier_hyperbolic_note(const expr_t *power, bool singular,
                                        const expr_t *rate, const expr_t *offset)
{
    number_t n = expr_eval((expr_t *)power), a = expr_eval((expr_t *)rate), b = expr_eval((expr_t *)offset);
    number_t real = num_real_part(n);
    bool real_affine = num_is_real(a) && num_is_finite(a) && !num_is_zero(a) &&
                       num_is_real(b) && num_is_finite(b);
    bool possible_affine = (num_is_nan(a) || (num_is_real(a) && num_is_finite(a) && !num_is_zero(a))) &&
                           (num_is_nan(b) || (num_is_real(b) && num_is_finite(b)));
    const char *note = NULL;
    if (possible_affine && num_is_finite(n) && num_gt(real, NUM_ZERO)) {
        note = real_affine
                   ? "No ordinary Fourier transform: this real-affine hyperbolic power grows exponentially at infinity "
                     "and does not define a tempered distribution."
                   : "For real non-zero scale and real offset, this hyperbolic power grows exponentially: "
                     "neither its ordinary Fourier transform nor a tempered-distribution transform exists.";
    } else if (possible_affine && singular && num_is_finite(n) && num_le(real, NUM_NEG_ONE)) {
        note = real_affine
                   ? "No ordinary Fourier transform: the hyperbolic power has a non-integrable singularity at the "
                     "real zero of sinh. A principal-value or finite-part prescription must be specified separately."
                   : "For real non-zero scale and real offset, the hyperbolic power has a non-integrable singularity. "
                     "A principal-value or finite-part prescription must be specified separately.";
    } else if (possible_affine && !num_is_zero(n)) {
        note = singular
                   ? "The beta-function formula requires real non-zero scale, real offset and -1 < Re(n) < 0 for "
                     "the effective sinh exponent. Principal powers are used on the negative half-line. "
                     "Zero exponent gives a Dirac impulse; distributional boundary cases are not inferred."
                   : "The beta-function formula requires real non-zero scale, real offset and Re(n) < 0 for "
                     "the effective cosh exponent. Zero exponent gives a Dirac impulse; distributional boundary "
                     "cases are not inferred.";
    }
    num_destroy(&real);
    num_destroy(&b);
    num_destroy(&a);
    num_destroy(&n);
    return note;
}

/* ------------------------------------------------------------------ */
/* Finite exponential sums and analytic evaluation functionals */
/* ------------------------------------------------------------------ */

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
        if (!expr_fourier_affine(c, f->a, x, &a, &b) || !expr_const_is_one(a))
            return false;
        *phase = clean(c, ft_mul(c, ft_mul(c, constant(c, c->inverse ? NUM_NEG_I : NUM_I), w), b));
        *scale = integer(c, 1);
        ++*count;
        return true;
    }
    if (!expr_fourier_uses(f, x)) {
        const expr_t *arg = expr_fourier_exponent(f);
        *phase = arg ? expr_fourier_keep(c, expr_clone(arg)) : integer(c, 0);
        *scale = arg ? integer(c, 1) : expr_fourier_keep(c, expr_clone(f));
        return true;
    }
    if (f->ops == &ops_neg && evaluation_term(c, f->a, x, w, phase, scale, count)) {
        *scale = clean(c, ft_neg(c, *scale));
        return true;
    }
    if (f->ops == &ops_mul || (f->ops == &ops_div && !expr_fourier_uses(f->b, x))) {
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
    bool ordinary = expr_fourier_literal_value(shift, &value) && num_is_real(value);
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
    if (expr_fourier_match_power(c, f, &base, &power)) {
        number_t value = NUM_NAN;
        bool valid = expr_fourier_literal_value(power, &value) && num_is_real(value) && num_is_integer(value) &&
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
    if (!expr_fourier_affine(c, base->a, x, &a, &b))
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

/* ------------------------------------------------------------------ */
/* Odd hyperbolic distributional pairs */
/* ------------------------------------------------------------------ */

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
    else if (!expr_fourier_match_power(c, f, &base, &power) || !expr_is_const(power) || !num_eq(power->c, NUM_NEG_ONE))
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
    if (!argument || !expr_fourier_affine(c, argument, x, &rate, &offset))
        return NULL;
    if (expr_const_is_zero(rate)) {
        if (!pair->source_pole && expr_const_is_zero(offset))
            return integer(c, 0);
        expr_t *body = expr_fourier_keep(c, pair->source(offset));
        return ft_mul(c, ft_mul(c, integer(c, 2), pi_constant(c)), ft_mul(c, body, ft_delta(c, w)));
    }
    if (!expr_fourier_real_parameter(c, rate) || !expr_fourier_real_parameter(c, offset) ||
        !expr_fourier_positive(c, expr_fourier_abs(c, rate)))
        return NULL;
    if (pair->target_pole && !expr_fourier_positive(c, expr_fourier_abs(c, w)))
        return NULL;
    expr_t *coordinate = clean(c, ft_div(c, ft_mul(c, pi_constant(c), w), ft_mul(c, integer(c, 2), rate)));
    expr_t *body = expr_fourier_keep(c, pair->dual(coordinate));
    expr_t *i = constant(c, NUM_I);
    if (c->inverse)
        i = ft_neg(c, i);
    expr_t *phase = ft_exp(c, ft_div(c, ft_mul(c, ft_mul(c, i, w), offset), rate));
    expr_t *coefficient = ft_neg(c, ft_div(c, ft_mul(c, i, pi_constant(c)), expr_fourier_abs(c, rate)));
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
    if (argument && pair->source_pole && expr_fourier_affine(c, argument, x, &rate, &offset) && !expr_const_is_zero(rate) &&
        expr_fourier_affine(c, condition->a, x, &guard_rate, &guard_offset) && !expr_const_is_zero(guard_rate)) {
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
    if (argument && expr_fourier_affine(&c, argument, transform->b->a, &rate, &offset) && !expr_const_is_zero(rate) &&
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

/* ------------------------------------------------------------------ */
/* Inverse hyperbolic beta spectra */
/* ------------------------------------------------------------------ */

/* A compact hyperbolic spectrum has at most two beta factors. Stop at other operators: this is
 * a bounded structural pattern match, not a search through arbitrary function arguments. */
static bool beta_factors(const expr_t *f, const expr_t **nodes, size_t *count)
{
    if (f->ops == &ops_beta) {
        if (*count == 2u)
            return false;
        nodes[(*count)++] = f;
        return true;
    }
    if (f->ops == &ops_add || f->ops == &ops_sub || f->ops == &ops_mul || f->ops == &ops_div)
        return beta_factors(f->a, nodes, count) && beta_factors(f->b, nodes, count);
    return f->ops != &ops_neg || beta_factors(f->a, nodes, count);
}

static bool same_formula(fourier_context_t *c, const expr_t *left, const expr_t *right)
{
    const expr_t *left_exponent = expr_fourier_exponent(left), *right_exponent = expr_fourier_exponent(right);
    if (left_exponent && right_exponent)
        return same_formula(c, left_exponent, right_exponent);
    number_t left_value = NUM_NAN, right_value = NUM_NAN;
    bool equal_constants = expr_fourier_literal_value(left, &left_value) && expr_fourier_literal_value(right, &right_value) &&
                           num_eq(left_value, right_value);
    num_destroy(&right_value);
    num_destroy(&left_value);
    if (equal_constants)
        return true;
    expr_t *difference = clean(c, ft_sub(c, clean(c, left), clean(c, right)));
    if (expr_const_is_zero(difference))
        return true;
    difference = expr_fourier_keep(c, expr_expand_preserved_for_display(difference));
    difference = clean(c, expr_fourier_keep(c, expr_expand_products_internal(difference)));
    return expr_const_is_zero(difference);
}

static expr_t *beta_coefficient(const expr_t *f, const expr_t *unit, const expr_t *zero)
{
    if (f == unit)
        return expr_const_one();
    if (f == zero)
        return expr_const_zero();
    if (f->ops == &ops_neg)
        return expr_new_unary_internal(f->ops, beta_coefficient(f->a, unit, zero));
    if (f->ops == &ops_add || f->ops == &ops_sub || f->ops == &ops_mul || f->ops == &ops_div)
        return expr_new_binary_internal(f->ops, beta_coefficient(f->a, unit, zero),
                                                beta_coefficient(f->b, unit, zero));
    return expr_clone(f);
}

/* Invert actual beta spectra, including copied/serialised expressions, without relying on a nested
 * Fourier node. Verify the entire spectrum before accepting a candidate or adding its conditions. */
expr_t *expr_fourier_beta_pair(fourier_context_t *c, const expr_t *f, const expr_t *x, const expr_t *w)
{
    const expr_t *nodes[2] = {NULL, NULL};
    size_t count = 0u;
    if (!beta_factors(f, nodes, &count) || count == 0u)
        return NULL;
    expr_t *one = integer(c, 1), *two = integer(c, 2), *i = constant(c, NUM_I);
    expr_t *pi = pi_constant(c), *two_pi = ft_mul(c, two, pi);
    for (size_t index = 0u; index < count; ++index) {
        const expr_t *beta = nodes[index];
        bool singular = !expr_fourier_uses(beta->a, x) || !expr_fourier_uses(beta->b, x);
        const expr_t *argument = expr_fourier_uses(beta->a, x) ? beta->a : beta->b;
        const expr_t *other = argument == beta->a ? beta->b : beta->a;
        expr_t *rate = NULL, *offset = NULL;
        if (!expr_fourier_affine(c, argument, x, &rate, &offset) || expr_const_is_zero(rate))
            continue;
        expr_t *n = clean(c, singular ? ft_sub(c, other, one) : ft_neg(c, ft_mul(c, two, offset)));
        if (expr_fourier_uses(n, x) || !same_formula(c, offset, ft_div(c, ft_neg(c, n), two)))
            continue;
        for (unsigned direction = 0u; direction < 2u; ++direction) {
            expr_t *real_rate = clean(c, ft_mul(c, ft_neg(c, i), rate));
            expr_t *a = clean(c, ft_div(c, direction ? ft_neg(c, one) : one, ft_mul(c, two, real_rate)));
            expr_t *q = ft_div(c, ft_mul(c, i, x), a);
            expr_t *left = ft_div(c, ft_sub(c, q, n), two);
            expr_t *right = ft_div(c, ft_sub(c, ft_neg(c, q), n), two);
            expr_t *second = ft_add(c, n, one);
            const expr_t *left_beta = NULL, *right_beta = NULL;
            for (size_t k = 0u; k < count; ++k) {
                for (unsigned swap = 0u; swap < 2u; ++swap) {
                    const expr_t *first_arg = swap ? nodes[k]->b : nodes[k]->a;
                    const expr_t *second_arg = swap ? nodes[k]->a : nodes[k]->b;
                    if (same_formula(c, first_arg, left) &&
                        same_formula(c, second_arg, singular ? second : right))
                        left_beta = nodes[k];
                    if (singular && same_formula(c, first_arg, right) && same_formula(c, second_arg, second))
                        right_beta = nodes[k];
                }
            }
            if (!left_beta || (singular && (!right_beta || left_beta == right_beta)))
                continue;
            expr_t *left_coefficient = clean(c, expr_fourier_keep(c, beta_coefficient(f, left_beta, right_beta)));
            expr_t *right_coefficient = singular ? clean(c, expr_fourier_keep(c, beta_coefficient(f, right_beta,
                left_beta))) : one;
            const expr_t *reconstructed = singular
                                             ? ft_add(c, ft_mul(c, left_coefficient, left_beta),
                                                         ft_mul(c, right_coefficient, right_beta))
                                             : ft_mul(c, left_coefficient, left_beta);
            if (!same_formula(c, left_coefficient, one) || !same_formula(c, f, reconstructed))
                continue;
            /* Principal sinh, absolute sinh, and reciprocal sinh have distinct negative-half-line phases. */
            for (unsigned branch = 0u; branch < (singular ? 3u : 1u); ++branch) {
                expr_t *phase = branch == 1u ? one : ft_exp(c, ft_mul(c, ft_mul(c, i, pi),
                                                                           branch == 2u ? ft_neg(c, n) : n));
                if (singular && !same_formula(c, right_coefficient, phase))
                    continue;
                if (!expr_fourier_real_parameter(c, a) || !expr_fourier_positive(c, expr_fourier_abs(c, a)) ||
                    !expr_fourier_positive(c, ft_neg(c, n)) || (singular && !expr_fourier_positive(c, second)))
                    return NULL;
                expr_t *coordinate = clean(c, ft_mul(c, a, c->inverse ? w : ft_neg(c, w)));
                coordinate = clean(c, expr_fourier_keep(c, expr_expand_products_internal(coordinate)));
                expr_t *base = singular ? ft_sinh(c, coordinate) : ft_cosh(c, coordinate);
                if (singular && branch == 1u)
                    base = expr_fourier_abs(c, base);
                if (singular && branch == 2u)
                    base = ft_div(c, one, base);
                expr_t *body = ft_pow_xp(c, base, branch == 2u ? ft_neg(c, n) : n);
                expr_t *scale = ft_div(c, expr_fourier_abs(c, a), ft_pow_xp(c, two, ft_neg(c, second)));
                return ft_mul(c, two_pi, ft_mul(c, scale, body));
            }
        }
    }
    return NULL;
}
