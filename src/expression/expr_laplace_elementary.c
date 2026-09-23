#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#include <stdio.h>
#include <string.h>

typedef expr_t *(*elementary_rule_fn)(const expr_t *, const expr_t *, const expr_t *, number_t *, expr_t **);

static bool elementary_uses(const expr_t *f, const expr_t *t)
{
    bool used = false;
    expr_t *variable = (expr_t *)t;
    return !expr_collect_var_usage(f, 1u, &variable, &used) || used;
}

/* A numerically bound free variable is not a known constant parameter. */
static bool elementary_constant(const expr_t *f, number_t *value)
{
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(f);
    bool constant = true;
    for (size_t i = 0u; i < expr_bindings_count(bindings); ++i) {
        expr_t *binding = expr_bindings_get(bindings, expr_bindings_name_at(bindings, i));
        if (!expr_is_const(binding))
            constant = false;
    }
    expr_bindings_free(bindings);
    if (!constant)
        return false;
    *value = expr_eval(f);
    return num_is_finite(*value);
}

/* Preserve exact constants rather than replacing their magnitude with an approximate evaluation. */
static expr_t *elementary_absolute(const expr_t *f)
{
    number_t value = NUM_NAN;
    bool known_real = elementary_constant(f, &value) && num_is_real(value);
    expr_t *out = known_real ? (num_cmp(value, NUM_ZERO) < 0 ? expr_neg(f) : expr_clone(f)) : expr_abs(f);
    num_destroy(&value);
    return out;
}

/* Keep this local collector until the common affine matcher is available through the module API. */
static bool elementary_affine_parts(const expr_t *f, const expr_t *t, expr_t **rate, expr_t **offset)
{
    if (!elementary_uses(f, t)) {
        *rate = expr_const_zero();
        *offset = expr_clone(f);
        return true;
    }
    if (expr_is_var(f) && f->var_id == t->var_id) {
        *rate = expr_const_one();
        *offset = expr_const_zero();
        return true;
    }
    expr_t *a = NULL, *b = NULL, *c = NULL, *d = NULL;
    bool matched = false;
    if ((f->ops == &ops_add || f->ops == &ops_sub) && elementary_affine_parts(f->a, t, &a, &b) &&
        elementary_affine_parts(f->b, t, &c, &d)) {
        *rate = f->ops == &ops_add ? expr_add(a, c) : expr_sub(a, c);
        *offset = f->ops == &ops_add ? expr_add(b, d) : expr_sub(b, d);
        matched = true;
    } else if (f->ops == &ops_neg && elementary_affine_parts(f->a, t, &a, &b)) {
        *rate = expr_neg(a);
        *offset = expr_neg(b);
        matched = true;
    } else if (f->ops == &ops_mul) {
        const expr_t *coefficient = !elementary_uses(f->a, t) ? f->a : f->b;
        const expr_t *operand = coefficient == f->a ? f->b : f->a;
        if (!elementary_uses(coefficient, t) && elementary_affine_parts(operand, t, &a, &b)) {
            *rate = expr_mul(coefficient, a);
            *offset = expr_mul(coefficient, b);
            matched = true;
        }
    } else if (f->ops == &ops_div && !elementary_uses(f->b, t) &&
               elementary_affine_parts(f->a, t, &a, &b)) {
        *rate = expr_div(a, f->b);
        *offset = expr_div(b, f->b);
        matched = true;
    }
    expr_free(d);
    expr_free(c);
    expr_free(b);
    expr_free(a);
    return matched;
}

static bool elementary_affine(const expr_t *f, const expr_t *t, expr_t **rate, expr_t **offset)
{
    if (!elementary_affine_parts(f, t, rate, offset))
        return false;
    expr_t *a = expr_beautify(*rate);
    expr_t *b = expr_beautify(*offset);
    expr_free(*rate);
    expr_free(*offset);
    *rate = a;
    *offset = b;
    return a && b;
}

/* Match the condition representation consumed by expr_laplace_result(). */
static void elementary_half_plane(const expr_t *s, const expr_t *rate, expr_t **conditions)
{
    expr_t *pair = expr_alloc(&ops_argument_list);
    pair->a = expr_clone(s);
    pair->b = expr_new_unary_internal(&ops_real_bound, expr_clone(rate));
    expr_t *item = expr_alloc(&ops_argument_list);
    item->a = pair;
    item->b = *conditions;
    *conditions = item;
}

static expr_t *elementary_linear_formula(const expr_t *a, const expr_t *b, const expr_t *s)
{
    expr_t *square = expr_mul(s, s);
    expr_t *linear = expr_div(a, square);
    expr_t *constant = expr_div(b, s);
    expr_t *out = expr_add(linear, constant);
    expr_free(constant);
    expr_free(linear);
    expr_free(square);
    return out;
}

/* Re-enter only after removing the elementary operator; the core owns the underlying rules. */
static expr_t *elementary_reduced(const expr_t *f, const expr_t *t, const expr_t *s,
                                  number_t *bound, expr_t **conditions)
{
    expr_t *args[] = {(expr_t *)f, (expr_t *)t, (expr_t *)s};
    expr_t *transform = expr_laplace_from_args(3u, args);
    number_t reduced_bound = NUM_ZERO;
    expr_t *out = transform ? expr_laplace_formula(transform, &reduced_bound, conditions) : NULL;
    if (out) {
        num_destroy(bound);
        *bound = num_clone(reduced_bound);
    }
    num_destroy(&reduced_bound);
    expr_free(transform);
    return out;
}

typedef struct {
    expr_apply_unary_fn function;
    bool add;
    bool halve;
} elementary_circular_t;

static const elementary_circular_t elementary_circular[EXPR_KIND_COUNT] = {
    [EXPR_KIND_VERSIN    ] = {expr_cos, false, false},
    [EXPR_KIND_VERCOS    ] = {expr_cos, true,  false},
    [EXPR_KIND_COVERSIN  ] = {expr_sin, false, false},
    [EXPR_KIND_COVERCOS  ] = {expr_sin, true,  false},
    [EXPR_KIND_HAVERSIN  ] = {expr_cos, false, true },
    [EXPR_KIND_HAVERCOS  ] = {expr_cos, true,  true },
    [EXPR_KIND_HACOVERSIN] = {expr_sin, false, true },
    [EXPR_KIND_HACOVERCOS] = {expr_sin, true,  true },
};

static expr_t *elementary_circular_rule(const expr_t *f, const expr_t *t, const expr_t *s,
                                        number_t *bound, expr_t **conditions)
{
    const elementary_circular_t *rule = &elementary_circular[f->ops->kind];
    expr_t *one = expr_const_one();
    expr_t *trig = rule->function(f->a);
    expr_t *reduced = rule->add ? expr_add(one, trig) : expr_sub(one, trig);
    if (rule->halve) {
        expr_t *two = expr_const_long(2);
        expr_t *half = expr_div(reduced, two);
        expr_free(reduced);
        expr_free(two);
        reduced = half;
    }
    expr_t *out = elementary_reduced(reduced, t, s, bound, conditions);
    expr_free(reduced);
    expr_free(trig);
    expr_free(one);
    return out;
}

static expr_t *elementary_sech_rule(const expr_t *f, const expr_t *t, const expr_t *s,
                                    number_t *bound, expr_t **conditions)
{
    expr_t *rate = NULL, *offset = NULL, *out = NULL;
    number_t value = NUM_NAN;
    if (!elementary_affine(f->a, t, &rate, &offset) || !expr_is_exact_zero(offset))
        goto done;
    bool known = elementary_constant(rate, &value);
    if (known && !num_is_real(value))
        goto done;
    if (known && num_is_zero(value)) {
        expr_t *one = expr_const_one();
        out = expr_div(one, s);
        expr_free(one);
        goto done;
    }
    expr_t *square = expr_mul(rate, rate);
    expr_t *q = known ? elementary_absolute(rate) : expr_sqrt(square);
    expr_t *one = expr_const_one();
    expr_t *two = expr_const_long(2);
    expr_t *three = expr_const_long(3);
    expr_t *four = expr_const_long(4);
    expr_t *two_q = expr_mul(two, q);
    expr_t *four_q = expr_mul(four, q);
    expr_t *z = expr_div(s, four_q);
    expr_t *quarter = expr_div(one, four);
    expr_t *three_quarters = expr_div(three, four);
    expr_t *upper = expr_add(z, three_quarters);
    expr_t *lower = expr_add(z, quarter);
    expr_t *psi_upper = expr_digamma(upper);
    expr_t *psi_lower = expr_digamma(lower);
    expr_t *difference = expr_sub(psi_upper, psi_lower);
    out = expr_div(difference, two_q);
    num_destroy(bound);
    if (known) {
        number_t magnitude = num_abs(value);
        *bound = num_neg(magnitude);
        num_destroy(&magnitude);
    } else {
        expr_t *zero = expr_const_zero();
        expr_t *negative_q = expr_neg(q);
        *bound = num_clone(NUM_NINF);
        elementary_half_plane(q, zero, conditions);
        elementary_half_plane(s, negative_q, conditions);
        expr_free(negative_q);
        expr_free(zero);
    }
    expr_free(difference);
    expr_free(psi_lower);
    expr_free(psi_upper);
    expr_free(lower);
    expr_free(upper);
    expr_free(three_quarters);
    expr_free(quarter);
    expr_free(z);
    expr_free(four_q);
    expr_free(two_q);
    expr_free(four);
    expr_free(three);
    expr_free(two);
    expr_free(one);
    expr_free(q);
    expr_free(square);
done:
    num_destroy(&value);
    expr_free(offset);
    expr_free(rate);
    return out;
}

static expr_t *elementary_linear_rule(const expr_t *f, const expr_t *t, const expr_t *s,
                                      number_t *bound, expr_t **conditions)
{
    (void)bound;
    (void)conditions;
    expr_t *rate = NULL, *offset = NULL, *out = NULL;
    if (elementary_affine(f->a, t, &rate, &offset)) {
        expr_t *a = f->ops->apply_unary(rate);
        expr_t *b = f->ops->apply_unary(offset);
        out = elementary_linear_formula(a, b, s);
        expr_free(b);
        expr_free(a);
    }
    expr_free(offset);
    expr_free(rate);
    return out;
}

static expr_t *elementary_abs_rule(const expr_t *f, const expr_t *t, const expr_t *s,
                                   number_t *bound, expr_t **conditions)
{
    (void)bound;
    (void)conditions;
    expr_t *rate = NULL, *offset = NULL, *out = NULL;
    number_t a = NUM_NAN, b = NUM_NAN;
    if (!elementary_affine(f->a, t, &rate, &offset))
        goto done;
    if (expr_is_exact_zero(offset)) {
        expr_t *magnitude = elementary_absolute(rate);
        out = elementary_linear_formula(magnitude, offset, s);
        expr_free(magnitude);
        goto done;
    }
    if (expr_is_exact_zero(rate)) {
        expr_t *magnitude = elementary_absolute(offset);
        out = expr_div(magnitude, s);
        expr_free(magnitude);
        goto done;
    }
    if (!elementary_constant(rate, &a) || !elementary_constant(offset, &b) ||
        !num_is_real(a) || !num_is_real(b))
        goto done;
    bool negative_initial = num_cmp(num_is_zero(b) ? a : b, NUM_ZERO) < 0;
    expr_t *line = elementary_linear_formula(rate, offset, s);
    out = negative_initial ? expr_neg(line) : expr_clone(line);
    expr_free(line);
    if (!num_is_zero(a) && !num_is_zero(b) && (num_cmp(a, NUM_ZERO) < 0) != negative_initial) {
        /* The slope changes by 2|a| at the positive root -b/a. */
        expr_t *ratio = expr_div(offset, rate);
        expr_t *exponent = expr_mul(s, ratio);
        expr_t *exponential = expr_exp(exponent);
        expr_t *magnitude = elementary_absolute(rate);
        expr_t *two = expr_const_long(2);
        expr_t *jump = expr_mul(two, magnitude);
        expr_t *numerator = expr_mul(jump, exponential);
        expr_t *square = expr_mul(s, s);
        expr_t *correction = expr_div(numerator, square);
        expr_t *sum = expr_add(out, correction);
        expr_free(out);
        out = sum;
        expr_free(correction);
        expr_free(square);
        expr_free(numerator);
        expr_free(jump);
        expr_free(two);
        expr_free(magnitude);
        expr_free(exponential);
        expr_free(exponent);
        expr_free(ratio);
    }
done:
    num_destroy(&b);
    num_destroy(&a);
    expr_free(offset);
    expr_free(rate);
    return out;
}

static expr_t *elementary_staircase_rule(const expr_t *f, const expr_t *t, const expr_t *s,
                                         number_t *bound, expr_t **conditions)
{
    (void)conditions;
    expr_t *rate = NULL, *offset = NULL, *out = NULL;
    number_t value = NUM_NAN;
    if (!elementary_affine(f->a, t, &rate, &offset) || !expr_is_exact_zero(offset) ||
        !elementary_constant(rate, &value) || !num_is_real(value))
        goto done;
    if (num_is_zero(value)) {
        num_destroy(bound);
        *bound = num_clone(NUM_NINF);
        out = expr_const_zero();
        goto done;
    }
    bool negative = num_cmp(value, NUM_ZERO) < 0;
    bool ceiling = (f->ops == &ops_ceil) != negative;
    expr_t *q = elementary_absolute(rate);
    expr_t *z = expr_div(s, q);
    expr_t *exponential = expr_exp(z);
    expr_t *one = expr_const_one();
    expr_t *difference = expr_sub(exponential, one);
    expr_t *denominator = expr_mul(s, difference);
    expr_t *positive = expr_div(ceiling ? exponential : one, denominator);
    out = negative ? expr_neg(positive) : expr_clone(positive);
    expr_free(positive);
    expr_free(denominator);
    expr_free(difference);
    expr_free(one);
    expr_free(exponential);
    expr_free(z);
    expr_free(q);
done:
    num_destroy(&value);
    expr_free(offset);
    expr_free(rate);
    return out;
}

/* For Re(s)>0 and q>0, +/-i*s/q never lies on the E1 branch cut. */
static expr_t *elementary_arctangent_rule(const expr_t *f, const expr_t *t, const expr_t *s,
                                          number_t *bound, expr_t **conditions)
{
    (void)conditions;
    expr_t *rate = NULL, *offset = NULL, *out = NULL;
    number_t value = NUM_NAN;
    if (!elementary_affine(f->a, t, &rate, &offset) || !expr_is_exact_zero(offset) ||
        !elementary_constant(rate, &value) || !num_is_real(value))
        goto done;
    if (num_is_zero(value)) {
        out = expr_const_zero();
        if (f->ops == &ops_atan) {
            num_destroy(bound);
            *bound = num_clone(NUM_NINF);
        }
    } else {
        expr_t *q = elementary_absolute(rate);
        expr_t *z = expr_div(s, q);
        expr_t *imaginary = expr_new_const(NUM_I);
        expr_t *iz = expr_mul(imaginary, z);
        expr_t *negative_iz = expr_neg(iz);
        expr_t *positive_exp = expr_exp(iz);
        expr_t *negative_exp = expr_exp(negative_iz);
        expr_t *positive_E1 = expr_E1(iz);
        expr_t *negative_E1 = expr_E1(negative_iz);
        expr_t *first = expr_mul(negative_exp, negative_E1);
        expr_t *second = expr_mul(positive_exp, positive_E1);
        expr_t *difference = expr_sub(first, second);
        expr_t *two = expr_const_long(2);
        expr_t *two_i = expr_mul(two, imaginary);
        expr_t *denominator = expr_mul(two_i, s);
        expr_t *positive = expr_div(difference, denominator);
        out = num_cmp(value, NUM_ZERO) < 0 ? expr_neg(positive) : expr_clone(positive);
        expr_free(positive);
        expr_free(denominator);
        expr_free(two_i);
        expr_free(two);
        expr_free(difference);
        expr_free(second);
        expr_free(first);
        expr_free(negative_E1);
        expr_free(positive_E1);
        expr_free(negative_exp);
        expr_free(positive_exp);
        expr_free(negative_iz);
        expr_free(iz);
        expr_free(imaginary);
        expr_free(z);
        expr_free(q);
    }
    if (f->ops == &ops_acot) {
        expr_t *pi = expr_new_named_const(NUM_PI, "@pi");
        expr_t *two = expr_const_long(2);
        expr_t *twice_s = expr_mul(two, s);
        expr_t *constant = expr_div(pi, twice_s);
        expr_t *complement = expr_sub(constant, out);
        expr_free(out);
        out = complement;
        expr_free(constant);
        expr_free(twice_s);
        expr_free(two);
        expr_free(pi);
    }
done:
    num_destroy(&value);
    expr_free(offset);
    expr_free(rate);
    return out;
}

/* Real-axis atanh uses the native +i*pi/2 boundary value on both exterior cuts.
 * Its logarithmic singularity is locally integrable. With q=|a| and z=s/q,
 * the transform is ((a/q)*(exp(-z)*Ei(z)+exp(z)*E1(z))+i*pi*exp(-z))/(2*s).
 * Re(s)>0 keeps Ei and E1 away from their own cuts, including for complex s. */
static expr_t *elementary_atanh_rule(const expr_t *f, const expr_t *t, const expr_t *s,
                                    number_t *bound, expr_t **conditions)
{
    expr_t *rate = NULL, *offset = NULL, *out = NULL;
    number_t value = NUM_NAN;
    if (!elementary_affine(f->a, t, &rate, &offset) || !expr_is_exact_zero(offset))
        goto done;
    bool known = elementary_constant(rate, &value);
    if (known && !num_is_real(value))
        goto done;
    if (known && num_is_zero(value)) {
        num_destroy(bound);
        *bound = num_clone(NUM_NINF);
        out = expr_const_zero();
        goto done;
    }
    expr_t *q = elementary_absolute(rate);
    expr_t *z = expr_div(s, q);
    expr_t *minus_z = expr_neg(z);
    expr_t *positive_exp = expr_exp(z);
    expr_t *negative_exp = expr_exp(minus_z);
    expr_t *ei = expr_Ei(z);
    expr_t *e1 = expr_E1(z);
    expr_t *first = expr_mul(negative_exp, ei);
    expr_t *second = expr_mul(positive_exp, e1);
    expr_t *sum = expr_add(first, second);
    expr_t *sign = expr_div(rate, q);
    expr_t *signed_sum = expr_mul(sign, sum);
    expr_t *pi = expr_new_named_const(NUM_PI, "@pi");
    expr_t *imaginary = expr_new_const(NUM_I);
    expr_t *i_pi = expr_mul(imaginary, pi);
    expr_t *jump = expr_mul(i_pi, negative_exp);
    expr_t *numerator = expr_add(signed_sum, jump);
    expr_t *two = expr_const_long(2);
    expr_t *denominator = expr_mul(two, s);
    out = expr_div(numerator, denominator);
    if (!known) {
        expr_t *predicate = expr_new_unary_internal(&ops_real_parameter, expr_clone(rate));
        expr_t *zero = expr_const_zero();
        elementary_half_plane(predicate, zero, conditions);
        elementary_half_plane(q, zero, conditions);
        expr_free(zero);
        expr_free(predicate);
    }
    expr_free(denominator);
    expr_free(two);
    expr_free(numerator);
    expr_free(jump);
    expr_free(i_pi);
    expr_free(imaginary);
    expr_free(pi);
    expr_free(signed_sum);
    expr_free(sign);
    expr_free(sum);
    expr_free(second);
    expr_free(first);
    expr_free(e1);
    expr_free(ei);
    expr_free(negative_exp);
    expr_free(positive_exp);
    expr_free(minus_z);
    expr_free(z);
    expr_free(q);
done:
    num_destroy(&value);
    expr_free(offset);
    expr_free(rate);
    return out;
}

/* Positive real affine arguments avoid both logarithmic and E1 branch-cut ambiguities. */
static expr_t *elementary_logarithm_rule(const expr_t *f, const expr_t *t, const expr_t *s,
                                         number_t *bound, expr_t **conditions)
{
    (void)bound;
    (void)conditions;
    expr_t *rate = NULL, *offset = NULL, *out = NULL;
    number_t a = NUM_NAN, b = NUM_NAN;
    if (!elementary_affine(f->a, t, &rate, &offset) || !elementary_constant(rate, &a) ||
        !elementary_constant(offset, &b) || !num_is_real(a) || !num_is_real(b) ||
        num_cmp(a, NUM_ZERO) < 0 || num_cmp(b, NUM_ZERO) < 0 || (num_is_zero(a) && num_is_zero(b)))
        goto done;
    if (num_is_zero(a)) {
        expr_t *initial = expr_log(offset);
        out = expr_div(initial, s);
        expr_free(initial);
    } else if (num_is_zero(b)) {
        expr_t *log_rate = expr_log(rate);
        expr_t *gamma = expr_new_const(NUM_EULER_MASCHERONI);
        expr_t *log_s = expr_log(s);
        expr_t *constant = expr_sub(log_rate, gamma);
        expr_t *numerator = expr_sub(constant, log_s);
        out = expr_div(numerator, s);
        expr_free(numerator);
        expr_free(constant);
        expr_free(log_s);
        expr_free(gamma);
        expr_free(log_rate);
    } else {
        expr_t *ratio = expr_div(offset, rate);
        expr_t *z = expr_mul(s, ratio);
        expr_t *exponential = expr_exp(z);
        expr_t *integral = expr_E1(z);
        expr_t *product = expr_mul(exponential, integral);
        expr_t *initial = expr_log(offset);
        expr_t *numerator = expr_add(initial, product);
        out = expr_div(numerator, s);
        expr_free(numerator);
        expr_free(initial);
        expr_free(product);
        expr_free(integral);
        expr_free(exponential);
        expr_free(z);
        expr_free(ratio);
    }
    if (out && f->ops == &ops_log10) {
        expr_t *ten = expr_const_long(10);
        expr_t *scale = expr_log(ten);
        expr_t *scaled = expr_div(out, scale);
        expr_free(out);
        out = scaled;
        expr_free(scale);
        expr_free(ten);
    }
done:
    num_destroy(&b);
    num_destroy(&a);
    expr_free(offset);
    expr_free(rate);
    return out;
}

/* H0 uses the existing entire 1F2 primitive; positive q keeps Y0 off its cut. The formula is valid
 * throughout Re(s)>0, but num_bessel_y currently has no complex evaluator, so non-real s yields NaN. */
static expr_t *elementary_asinh_rule(const expr_t *f, const expr_t *t, const expr_t *s,
                                     number_t *bound, expr_t **conditions)
{
    (void)conditions;
    expr_t *rate = NULL, *offset = NULL, *out = NULL;
    number_t value = NUM_NAN;
    if (!elementary_affine(f->a, t, &rate, &offset) || !expr_is_exact_zero(offset) ||
        !elementary_constant(rate, &value) || !num_is_real(value))
        goto done;
    if (num_is_zero(value)) {
        num_destroy(bound);
        *bound = num_clone(NUM_NINF);
        out = expr_const_zero();
        goto done;
    }
    expr_t *q = elementary_absolute(rate);
    expr_t *z = expr_div(s, q);
    expr_t *zero = expr_const_zero();
    expr_t *one = expr_const_one();
    expr_t *two = expr_const_long(2);
    expr_t *three = expr_const_long(3);
    expr_t *four = expr_const_long(4);
    expr_t *pi = expr_new_named_const(NUM_PI, "@pi");
    expr_t *three_halves = expr_div(three, two);
    expr_t *square = expr_mul(z, z);
    expr_t *negative_square = expr_neg(square);
    expr_t *argument = expr_div(negative_square, four);
    const expr_t *upper[] = {one};
    const expr_t *lower[] = {three_halves, three_halves};
    expr_t *hypergeometric = expr_hypergeometric_pFq(1u, upper, 2u, lower, argument);
    expr_t *twice_z = expr_mul(two, z);
    expr_t *scale = expr_div(twice_z, pi);
    expr_t *struve = expr_mul(scale, hypergeometric);
    expr_t *bessel = expr_bessel_y(zero, z);
    expr_t *difference = expr_sub(struve, bessel);
    expr_t *numerator = expr_mul(pi, difference);
    expr_t *denominator = expr_mul(two, s);
    expr_t *positive = expr_div(numerator, denominator);
    out = num_cmp(value, NUM_ZERO) < 0 ? expr_neg(positive) : expr_clone(positive);
    expr_free(positive);
    expr_free(denominator);
    expr_free(numerator);
    expr_free(difference);
    expr_free(bessel);
    expr_free(struve);
    expr_free(scale);
    expr_free(twice_z);
    expr_free(hypergeometric);
    expr_free(argument);
    expr_free(negative_square);
    expr_free(square);
    expr_free(three_halves);
    expr_free(pi);
    expr_free(four);
    expr_free(three);
    expr_free(two);
    expr_free(one);
    expr_free(zero);
    expr_free(z);
    expr_free(q);
done:
    num_destroy(&value);
    expr_free(offset);
    expr_free(rate);
    return out;
}

/* Keep every binomial expansion bounded and unevaluated: even four explicit rational
 * terms can trigger expensive common-denominator beautification for an unbound target. */
static expr_t *elementary_hyperbolic_sum(const expr_t *base, const expr_t *rate, const expr_t *offset,
                                         long order, const expr_t *t, const expr_t *s,
                                         number_t *bound, expr_t **conditions)
{
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(base);
    char name[64];
    size_t suffix = 0u;
    do {
        snprintf(name, sizeof(name), "laplace_hyperbolic_index_%zu", suffix++);
    } while ((s->name && strcmp(s->name, name) == 0) || (t->name && strcmp(t->name, name) == 0) ||
             expr_bindings_get(bindings, name));
    expr_bindings_free(bindings);
    expr_t *index = expr_new_named_var(NUM_ZERO, name);
    expr_t *n = expr_const_long(order);
    expr_t *zero = expr_const_zero();
    expr_t *two = expr_const_long(2);
    expr_t *twice_index = expr_mul(two, index);
    expr_t *harmonic = expr_sub(n, twice_index);
    expr_t *frequency = expr_mul(harmonic, rate);
    expr_t *phase = expr_mul(harmonic, offset);
    expr_t *exponential = expr_exp(phase);
    expr_t *weight = expr_binomial(n, index);
    if (base->ops == &ops_sinh) {
        expr_t *minus_one = expr_const_long(-1);
        expr_t *sign = expr_pow_xp(minus_one, index);
        expr_t *signed_weight = expr_mul(sign, weight);
        expr_free(weight);
        weight = signed_weight;
        expr_free(sign);
        expr_free(minus_one);
    }
    expr_t *numerator = expr_mul(weight, exponential);
    expr_t *denominator = expr_sub(s, frequency);
    expr_t *term = expr_div(numerator, denominator);
    expr_t *sum = expr_new_finite_summation_range(term, index, zero, n);
    expr_t *scale = expr_const_long(1L << order);
    expr_t *out = sum ? expr_div(sum, scale) : NULL;
    if (out) {
        expr_t *growth = expr_mul(n, rate);
        expr_t *negative_growth = expr_neg(growth);
        num_destroy(bound);
        *bound = num_clone(NUM_NINF);
        elementary_half_plane(s, growth, conditions);
        elementary_half_plane(s, negative_growth, conditions);
        expr_free(negative_growth);
        expr_free(growth);
    }
    expr_free(scale);
    expr_free(sum);
    expr_free(term);
    expr_free(denominator);
    expr_free(numerator);
    expr_free(weight);
    expr_free(exponential);
    expr_free(phase);
    expr_free(frequency);
    expr_free(harmonic);
    expr_free(twice_index);
    expr_free(two);
    expr_free(zero);
    expr_free(n);
    expr_free(index);
    return out;
}

/* Construct the spectral finite sum directly, without recursively transforming an expanded power. */
static expr_t *elementary_hyperbolic_power(const expr_t *f, const expr_t *t, const expr_t *s,
                                           number_t *bound, expr_t **conditions)
{
    const expr_t *base = NULL, *power = NULL;
    number_t exponent = NUM_NAN;
    bool matched = expr_match_pow_const(f, &base, &exponent);
    if (!matched && expr_match_pow_expr(f, &base, &power))
        matched = !elementary_uses(power, t) && elementary_constant(power, &exponent);
    long order = 0, denominator = 0;
    bool valid = matched && base && (base->ops == &ops_sinh || base->ops == &ops_cosh) &&
                 num_get_small_rational(exponent, &order, &denominator) && denominator == 1 &&
                 order >= 0 && order <= 16;
    num_destroy(&exponent);
    if (!valid)
        return NULL;
    expr_t *rate = NULL, *offset = NULL;
    expr_t *out = NULL;
    if (elementary_affine(base->a, t, &rate, &offset)) {
        if (order == 0 || expr_is_exact_zero(rate)) {
            expr_t *initial = base->ops->apply_unary(offset);
            expr_t *n = expr_const_long(order);
            expr_t *constant = order == 0 ? expr_const_one() : expr_pow_xp(initial, n);
            expr_t *value = expr_beautify(constant);
            out = expr_div(value, s);
            if (expr_is_exact_zero(value)) {
                num_destroy(bound);
                *bound = num_clone(NUM_NINF);
            }
            expr_free(value);
            expr_free(constant);
            expr_free(n);
            expr_free(initial);
        } else {
            out = elementary_hyperbolic_sum(base, rate, offset, order, t, s, bound, conditions);
        }
    }
    expr_free(offset);
    expr_free(rate);
    return out;
}

static const elementary_rule_fn elementary_rules[EXPR_KIND_COUNT] = {
    [EXPR_KIND_VERSIN    ] = elementary_circular_rule,
    [EXPR_KIND_VERCOS    ] = elementary_circular_rule,
    [EXPR_KIND_COVERSIN  ] = elementary_circular_rule,
    [EXPR_KIND_COVERCOS  ] = elementary_circular_rule,
    [EXPR_KIND_HAVERSIN  ] = elementary_circular_rule,
    [EXPR_KIND_HAVERCOS  ] = elementary_circular_rule,
    [EXPR_KIND_HACOVERSIN] = elementary_circular_rule,
    [EXPR_KIND_HACOVERCOS] = elementary_circular_rule,
    [EXPR_KIND_SECH      ] = elementary_sech_rule,
    [EXPR_KIND_ABS       ] = elementary_abs_rule,
    [EXPR_KIND_CONJ      ] = elementary_linear_rule,
    [EXPR_KIND_REAL_BOUND] = elementary_linear_rule,
    [EXPR_KIND_FLOOR     ] = elementary_staircase_rule,
    [EXPR_KIND_CEIL      ] = elementary_staircase_rule,
    [EXPR_KIND_ATAN      ] = elementary_arctangent_rule,
    [EXPR_KIND_ACOT      ] = elementary_arctangent_rule,
    [EXPR_KIND_ATANH     ] = elementary_atanh_rule,
    [EXPR_KIND_ASINH     ] = elementary_asinh_rule,
    [EXPR_KIND_LOG       ] = elementary_logarithm_rule,
    [EXPR_KIND_LOG10     ] = elementary_logarithm_rule,
    [EXPR_KIND_POW       ] = elementary_hyperbolic_power,
    [EXPR_KIND_POW_D     ] = elementary_hyperbolic_power,
};

/* Return a supported elementary formula and commit its convergence metadata only on success. */
expr_t *expr_laplace_elementary_rule(const expr_t *f, const expr_t *t, const expr_t *s,
                                    number_t *bound, expr_t **conditions)
{
    if (!f || !f->ops || !t || !s || !bound || !conditions || !expr_is_var(t) ||
        (unsigned int)f->ops->kind >= EXPR_KIND_COUNT)
        return NULL;
    elementary_rule_fn rule = elementary_rules[f->ops->kind];
    if (!rule)
        return NULL;
    number_t local_bound = NUM_ZERO;
    expr_t *local_conditions = NULL;
    expr_t *out = rule(f, t, s, &local_bound, &local_conditions);
    if (out) {
        num_destroy(bound);
        *bound = num_clone(local_bound);
        /* Join the short condition list; function dispatch itself uses direct indexing. */
        expr_t **tail = &local_conditions;
        while (*tail)
            tail = &(*tail)->b;
        *tail = *conditions;
        *conditions = local_conditions;
    } else {
        expr_free(local_conditions);
    }
    num_destroy(&local_bound);
    return out;
}
