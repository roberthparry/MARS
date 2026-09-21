#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Laplace rules retain a conservative right half-plane of convergence. */
static bool laplace_uses(const expr_t *expr, const expr_t *source)
{
    bool used = false;
    expr_t *variable = (expr_t *)source;
    return !expr_collect_var_usage(expr, 1u, &variable, &used) || used;
}

static bool laplace_numeric_argument(const expr_t *expr, const expr_t *source)
{
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(expr);
    bool numeric = true;
    for (size_t i = 0u; i < expr_bindings_count(bindings); ++i) {
        const char *name = expr_bindings_name_at(bindings, i);
        if (!source->name || strcmp(name, source->name) != 0)
            numeric = false;
    }
    expr_bindings_free(bindings);
    return numeric;
}

/* Resolve parameters only when every binding is a supplied constant, not a free variable. */
static bool laplace_constant_value(const expr_t *expr, number_t *value)
{
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(expr);
    bool constant = true;
    for (size_t i = 0u; i < expr_bindings_count(bindings); ++i) {
        expr_t *binding = expr_bindings_get(bindings, expr_bindings_name_at(bindings, i));
        if (!expr_is_const(binding))
            constant = false;
    }
    expr_bindings_free(bindings);
    if (!constant)
        return false;
    *value = expr_eval(expr);
    return num_is_finite(*value);
}

/* Keep large and symbolic powers as finite sums, rather than expanding polynomials. */
static expr_t *laplace_trig_sum(const expr_t *order, const expr_t *frequency, const expr_t *s, bool sine)
{
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(order);
    char name[2] = {0, 0};
    const char *candidates = "kjhmlpqruvwxyzabcdefgnot";
    for (const char *candidate = candidates; *candidate; ++candidate) {
        name[0] = *candidate;
        if ((!s->name || strcmp(s->name, name)) && !expr_bindings_get(bindings, name))
            break;
        name[0] = 0;
    }
    expr_bindings_free(bindings);
    if (!name[0])
        return NULL;
    expr_t *index = expr_new_named_var(NUM_ZERO, name);
    expr_t *two = expr_const_long(2);
    expr_t *twice_index = expr_mul(two, index);
    expr_t *harmonic = expr_sub(order, twice_index);
    expr_t *rate = expr_mul(frequency, harmonic);
    expr_t *weight = expr_binomial(order, index);
    expr_t *divisor = NULL;
    expr_t *numerator = NULL;
    expr_t *scale_base = NULL;
    if (sine) {
        expr_t *imaginary = expr_new_const(NUM_I);
        expr_t *shift = expr_mul(imaginary, rate);
        expr_t *minus_one = expr_const_long(-1);
        expr_t *sign = expr_pow_xp(minus_one, index);
        divisor = expr_sub(s, shift);
        numerator = expr_mul(weight, sign);
        scale_base = expr_mul(two, imaginary);
        expr_free(sign);
        expr_free(minus_one);
        expr_free(shift);
        expr_free(imaginary);
    } else {
        expr_t *rate_squared = expr_mul(rate, rate);
        expr_t *s_squared = expr_mul(s, s);
        divisor = expr_add(s_squared, rate_squared);
        numerator = expr_mul(weight, s);
        scale_base = expr_clone(two);
        expr_free(s_squared);
        expr_free(rate_squared);
    }
    expr_t *term = expr_div(numerator, divisor);
    expr_t *display_term = expr_beautify(term);
    expr_t *zero = expr_const_zero();
    expr_t *sum = expr_new_finite_summation_range(display_term ? display_term : term, index, zero, order);
    expr_t *scale = expr_pow_xp(scale_base, order);
    expr_t *out = sum ? expr_div(sum, scale) : NULL;
    expr_free(scale);
    expr_free(sum);
    expr_free(zero);
    expr_free(term);
    expr_free(display_term);
    expr_free(scale_base);
    expr_free(numerator);
    expr_free(divisor);
    expr_free(weight);
    expr_free(rate);
    expr_free(harmonic);
    expr_free(twice_index);
    expr_free(two);
    expr_free(index);
    return out;
}

static bool laplace_affine_parts(const expr_t *f, const expr_t *t, expr_t **rate, expr_t **offset);

/* Use the short recurrence for small orders and a finite sum for all other integer orders. */
static expr_t *laplace_trig_power(const expr_t *f, const expr_t *t, const expr_t *s, expr_t **conditions)
{
    const expr_t *base = NULL;
    const expr_t *power = NULL;
    number_t exponent = NUM_ZERO;
    bool matched = expr_match_pow_const(f, &base, &exponent);
    if (!matched && expr_match_pow_expr(f, &base, &power)) {
        matched = !laplace_uses(power, t);
        if (!matched || !laplace_constant_value(power, &exponent)) {
            num_destroy(&exponent);
            exponent = num_clone(NUM_NAN);
        }
    }
    long n = 0;
    long denominator = 0;
    bool small_order = num_get_small_rational(exponent, &n, &denominator) && denominator == 1 && n >= 0 && n <= 16;
    bool valid = matched && base && (base->ops == &ops_sin || base->ops == &ops_cos);
    expr_t *order = power ? expr_clone(power) : expr_new_const(exponent);
    num_destroy(&exponent);
    if (!valid || !laplace_numeric_argument(base->a, t)) {
        expr_free(order);
        return NULL;
    }

    expr_t *raw_frequency = NULL, *raw_offset = NULL;
    valid = laplace_affine_parts(base->a, t, &raw_frequency, &raw_offset);
    expr_t *frequency = valid ? expr_beautify(raw_frequency) : NULL;
    expr_t *offset = valid ? expr_beautify(raw_offset) : NULL;
    number_t rate = NUM_NAN;
    valid = valid && expr_const_is_zero(offset) && laplace_constant_value(frequency, &rate) && num_is_real(rate);
    expr_t *out = NULL;
    if (valid && !small_order) {
        out = laplace_trig_sum(order, frequency, s, base->ops == &ops_sin);
        if (out) {
            expr_t *condition = expr_alloc(&ops_nonnegative_integer);
            condition->a = expr_clone(order);
            expr_t *item = expr_alloc(&ops_argument_list);
            item->a = condition;
            item->b = *conditions;
            *conditions = item;
        }
    } else if (valid) {
        bool sine = base->ops == &ops_sin;
        expr_t *s_squared = expr_mul(s, s);
        expr_t *rate_squared = expr_mul(frequency, frequency);
        expr_t *numerator = n % 2 ? expr_clone(sine ? frequency : s) : expr_const_one();
        expr_t *divisor = n % 2 ? expr_add(s_squared, rate_squared) : expr_clone(s);
        for (long k = n % 2 + 2; k <= n; k += 2) {
            expr_t *factor = expr_const_long(k * (k - 1));
            expr_t *scale = expr_mul(factor, rate_squared);
            expr_t *term = expr_mul(scale, numerator);
            expr_t *initial = sine ? expr_const_zero() : expr_mul(s, divisor);
            expr_t *next_numerator = expr_add(initial, term);
            expr_t *square = expr_const_long(k * k);
            expr_t *shift = expr_mul(square, rate_squared);
            expr_t *quadratic = expr_add(s_squared, shift);
            expr_t *next_divisor = expr_mul(divisor, quadratic);
            expr_free(numerator);
            expr_free(divisor);
            numerator = next_numerator;
            divisor = next_divisor;
            expr_free(quadratic);
            expr_free(shift);
            expr_free(square);
            expr_free(initial);
            expr_free(term);
            expr_free(scale);
            expr_free(factor);
        }
        out = expr_div(numerator, divisor);
        expr_free(divisor);
        expr_free(numerator);
        expr_free(rate_squared);
        expr_free(s_squared);
    }
    num_destroy(&rate);
    expr_free(offset);
    expr_free(frequency);
    expr_free(raw_offset);
    expr_free(raw_frequency);
    expr_free(order);
    return out;
}

/* Collect source-independent coefficients structurally, without evaluating parameter bindings. */
static bool laplace_affine_parts(const expr_t *f, const expr_t *t, expr_t **rate, expr_t **offset)
{
    if (!laplace_uses(f, t)) {
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
    bool ok = false;
    if ((f->ops == &ops_add || f->ops == &ops_sub) && laplace_affine_parts(f->a, t, &a, &b) &&
        laplace_affine_parts(f->b, t, &c, &d)) {
        *rate = f->ops == &ops_add ? expr_add(a, c) : expr_sub(a, c);
        *offset = f->ops == &ops_add ? expr_add(b, d) : expr_sub(b, d);
        ok = true;
    } else if (f->ops == &ops_neg && laplace_affine_parts(f->a, t, &a, &b)) {
        *rate = expr_neg(a);
        *offset = expr_neg(b);
        ok = true;
    } else if (f->ops == &ops_mul) {
        const expr_t *coefficient = !laplace_uses(f->a, t) ? f->a : f->b;
        const expr_t *dependent = coefficient == f->a ? f->b : f->a;
        if (!laplace_uses(coefficient, t) && laplace_affine_parts(dependent, t, &a, &b)) {
            *rate = expr_mul(coefficient, a);
            *offset = expr_mul(coefficient, b);
            ok = true;
        }
    } else if (f->ops == &ops_div && !laplace_uses(f->b, t) && laplace_affine_parts(f->a, t, &a, &b)) {
        *rate = expr_div(a, f->b);
        *offset = expr_div(b, f->b);
        ok = true;
    }
    expr_free(a);
    expr_free(b);
    expr_free(c);
    expr_free(d);
    return ok;
}

static void laplace_add_half_plane(const expr_t *s, const expr_t *rate, expr_t **conditions)
{
    expr_t *pair = expr_alloc(&ops_argument_list);
    pair->a = expr_beautify(s);
    pair->b = expr_new_unary_internal(&ops_real_bound, expr_beautify(rate));
    expr_t *item = expr_alloc(&ops_argument_list);
    item->a = pair;
    item->b = *conditions;
    *conditions = item;
}

/* Symbolic affine exponentials carry their own half-plane, including complex rates. */
static expr_t *laplace_affine_exponential(const expr_t *f, const expr_t *t, const expr_t *s,
                                         number_t *bound, expr_t **conditions)
{
    const expr_t *argument = f->ops == &ops_exp ? f->a : NULL;
    const expr_t *base = NULL;
    const expr_t *power = NULL;
    if (!argument && expr_match_pow_expr(f, &base, &power) &&
        expr_is_const(base) && num_eq(base->c, NUM_E) && (!base->name || strcmp(base->name, "e") == 0))
        argument = power;
    if (!argument)
        return NULL;
    expr_t *rate = NULL;
    expr_t *offset = NULL;
    expr_t *out = NULL;
    if (laplace_affine_parts(argument, t, &rate, &offset)) {
        expr_t *clean_rate = expr_beautify(rate);
        expr_t *clean_offset = expr_beautify(offset);
        expr_free(rate);
        expr_free(offset);
        rate = clean_rate;
        offset = clean_offset;
        expr_t *denominator = expr_sub(s, rate);
        expr_t *numerator = expr_exp(offset);
        out = expr_div(numerator, denominator);
        laplace_add_half_plane(s, rate, conditions);
        num_destroy(bound);
        *bound = num_clone(NUM_NINF);
        expr_free(numerator);
        expr_free(denominator);
    }
    expr_free(rate);
    expr_free(offset);
    return out;
}

/* The principal square root is analytic in the selected Bessel convergence half-plane. */
static expr_t *laplace_scaled_bessel_zero(const expr_t *f, const expr_t *t, const expr_t *s,
                                        number_t *bound, expr_t **conditions)
{
    if (f->ops != &ops_bessel_j || !expr_is_exact_zero(f->a))
        return NULL;
    expr_t *raw_rate = NULL;
    expr_t *raw_offset = NULL;
    if (!laplace_affine_parts(f->b, t, &raw_rate, &raw_offset))
        return NULL;
    expr_t *rate = expr_beautify(raw_rate);
    expr_t *offset = expr_beautify(raw_offset);
    expr_free(raw_rate);
    expr_free(raw_offset);
    if (!expr_is_exact_zero(offset)) {
        expr_free(rate);
        expr_free(offset);
        return NULL;
    }
    expr_free(offset);
    expr_t *s_squared = expr_mul(s, s);
    expr_t *rate_squared = expr_mul(rate, rate);
    expr_t *sum = expr_add(s_squared, rate_squared);
    expr_t *root = expr_sqrt(sum);
    expr_t *one = expr_const_one();
    expr_t *out = expr_div(one, root);
    number_t rate_value = NUM_ZERO;
    bool real_rate = expr_match_const_value(rate, &rate_value) && num_is_real(rate_value);
    num_destroy(&rate_value);
    if (!real_rate) {
        expr_t *imaginary = expr_new_const(NUM_I);
        expr_t *growth = expr_mul(imaginary, rate);
        expr_t *negative_growth = expr_neg(growth);
        laplace_add_half_plane(s, growth, conditions);
        laplace_add_half_plane(s, negative_growth, conditions);
        num_destroy(bound);
        *bound = num_clone(NUM_NINF);
        expr_free(negative_growth);
        expr_free(growth);
        expr_free(imaginary);
    }
    expr_free(one);
    expr_free(root);
    expr_free(sum);
    expr_free(rate_squared);
    expr_free(s_squared);
    expr_free(rate);
    return out;
}

/* Select the decaying exponential direction without replacing sqrt(c^2) by c for complex parameters. */
static expr_t *laplace_scaled_tanh(const expr_t *f, const expr_t *t, const expr_t *s, expr_t **conditions)
{
    if (f->ops != &ops_tanh)
        return NULL;
    expr_t *rate = NULL, *offset = NULL;
    if (!laplace_affine_parts(f->a, t, &rate, &offset))
        return NULL;
    expr_t *clean_offset = expr_beautify(offset);
    expr_t *clean_rate = expr_beautify(rate);
    expr_free(rate);
    rate = clean_rate;
    bool unshifted = expr_const_is_zero(clean_offset);
    expr_free(clean_offset);
    expr_free(offset);
    if (!unshifted) {
        expr_free(rate);
        return NULL;
    }
    number_t value = NUM_NAN;
    bool known = laplace_constant_value(rate, &value);
    if (known && num_is_zero(value)) {
        num_destroy(&value);
        expr_free(rate);
        return NULL; /* The zero-transform rule handles this case before dispatch. */
    }
    expr_t *square = expr_mul(rate, rate);
    expr_t *q = known && num_is_real(value)
                    ? (num_cmp(value, NUM_ZERO) < 0 ? expr_neg(rate) : expr_clone(rate))
                    : expr_sqrt(square);
    expr_t *clean_q = expr_beautify(q);
    expr_free(q);
    q = clean_q;
    num_destroy(&value);
    expr_t *one = expr_const_one();
    expr_t *two = expr_const_long(2);
    expr_t *four = expr_const_long(4);
    expr_t *half = expr_div(one, two);
    expr_t *four_q = expr_mul(four, q);
    expr_t *z = expr_div(s, four_q);
    expr_t *upper = expr_add(one, z);
    expr_t *lower = expr_add(half, z);
    expr_t *psi_upper = expr_digamma(upper);
    expr_t *psi_lower = expr_digamma(lower);
    /* Use psi(z + 1) = psi(z) + 1/z before differentiation. The symmetric form
       avoids a quotient in s whose derivative introduces cancelling digamma terms. */
    expr_t *psi_z = expr_digamma(z);
    expr_t *endpoints = expr_add(psi_z, psi_upper);
    expr_t *midpoint = expr_mul(two, psi_lower);
    expr_t *numerator = expr_sub(midpoint, endpoints);
    expr_t *quarter = expr_div(one, four);
    expr_t *scaled_numerator = expr_mul(quarter, numerator);
    expr_t *out = expr_div(scaled_numerator, rate);
    expr_t *zero = expr_const_zero();
    laplace_add_half_plane(q, zero, conditions);
    expr_free(zero);
    expr_free(scaled_numerator);
    expr_free(quarter);
    expr_free(numerator);
    expr_free(midpoint);
    expr_free(endpoints);
    expr_free(psi_z);
    expr_free(psi_lower);
    expr_free(psi_upper);
    expr_free(lower);
    expr_free(upper);
    expr_free(z);
    expr_free(four_q);
    expr_free(half);
    expr_free(four);
    expr_free(two);
    expr_free(one);
    expr_free(q);
    expr_free(square);
    expr_free(rate);
    return out;
}

/* Trigonometric and hyperbolic pairs retain both exponential convergence bounds. */
static expr_t *laplace_affine_trigonometric(const expr_t *f, const expr_t *t, const expr_t *s,
                                       number_t *bound, expr_t **conditions)
{
    bool circular = f->ops == &ops_sin || f->ops == &ops_cos;
    bool odd = f->ops == &ops_sin || f->ops == &ops_sinh;
    if (!circular && f->ops != &ops_sinh && f->ops != &ops_cosh)
        return NULL;
    expr_t *raw_rate = NULL;
    expr_t *raw_offset = NULL;
    if (!laplace_affine_parts(f->a, t, &raw_rate, &raw_offset))
        return NULL;
    expr_t *rate = expr_beautify(raw_rate);
    expr_t *offset = expr_beautify(raw_offset);
    number_t offset_value = NUM_ZERO;
    bool zero_offset = expr_match_const_value(offset, &offset_value) && num_is_zero(offset_value);
    num_destroy(&offset_value);
    expr_t *sine = zero_offset ? expr_const_zero() : circular ? expr_sin(offset) : expr_sinh(offset);
    expr_t *cosine = zero_offset ? expr_const_one() : circular ? expr_cos(offset) : expr_cosh(offset);
    expr_t *first = expr_mul(s, odd ? sine : cosine);
    expr_t *second = expr_mul(rate, odd ? cosine : sine);
    expr_t *numerator = circular && !odd ? expr_sub(first, second) : expr_add(first, second);
    expr_t *s_squared = expr_mul(s, s);
    expr_t *rate_squared = expr_mul(rate, rate);
    expr_t *denominator = circular ? expr_add(s_squared, rate_squared) : expr_sub(s_squared, rate_squared);
    expr_t *out = expr_div(numerator, denominator);
    expr_t *imaginary = expr_new_const(NUM_I);
    expr_t *growth_rate = circular ? expr_mul(imaginary, rate) : expr_clone(rate);
    expr_t *negative_rate = expr_neg(growth_rate);
    number_t rate_value = NUM_ZERO;
    bool real_frequency = circular && expr_match_const_value(rate, &rate_value) && num_is_real(rate_value);
    if (!real_frequency) {
        laplace_add_half_plane(s, negative_rate, conditions);
        laplace_add_half_plane(s, growth_rate, conditions);
        num_destroy(bound);
        *bound = num_clone(NUM_NINF);
    }
    num_destroy(&rate_value);
    expr_free(negative_rate);
    expr_free(growth_rate);
    expr_free(imaginary);
    expr_free(denominator);
    expr_free(rate_squared);
    expr_free(s_squared);
    expr_free(numerator);
    expr_free(second);
    expr_free(first);
    expr_free(cosine);
    expr_free(sine);
    expr_free(offset);
    expr_free(rate);
    expr_free(raw_offset);
    expr_free(raw_rate);
    return out;
}

/* Integrate by parts and use the Gaussian integral, retaining the principal square root. */
static expr_t *laplace_affine_error_function(const expr_t *f, const expr_t *t, const expr_t *s,
                                            number_t *bound, expr_t **conditions)
{
    if (f->ops != &ops_erf && f->ops != &ops_erfc)
        return NULL;
    expr_t *raw_rate = NULL, *raw_offset = NULL;
    if (!laplace_affine_parts(f->a, t, &raw_rate, &raw_offset))
        return NULL;
    expr_t *rate = expr_beautify(raw_rate);
    expr_t *offset = expr_beautify(raw_offset);
    expr_free(raw_rate);
    expr_free(raw_offset);
    number_t rate_value = NUM_ZERO;
    bool zero_rate = laplace_constant_value(rate, &rate_value) && num_is_zero(rate_value);
    num_destroy(&rate_value);
    number_t offset_value = NUM_ZERO;
    bool zero_offset = expr_match_const_value(offset, &offset_value) && num_is_zero(offset_value);
    num_destroy(&offset_value);
    expr_t *initial = zero_offset ? expr_const_long(f->ops == &ops_erf ? 0 : 1)
                                 : f->ops == &ops_erf ? expr_erf(offset) : expr_erfc(offset);
    expr_t *out = NULL;
    if (zero_rate) {
        if (zero_offset && f->ops == &ops_erf) {
            out = expr_const_zero();
            num_destroy(bound);
            *bound = num_clone(NUM_NINF);
        } else {
            out = expr_div(initial, s);
        }
    } else {
        /* Use the reciprocal scale for divided arguments, before introducing square roots.
         * Re(z)>0 iff Re(1/z)>0, so the Gaussian condition is unchanged by this choice. */
        bool reciprocal = rate->ops == &ops_div;
        expr_t *two = expr_const_long(2);
        expr_t *twice_rate = reciprocal ? expr_mul(two, rate->a) : NULL;
        expr_t *raw_scale = reciprocal ? expr_div(rate->b, twice_rate) : expr_clone(rate);
        expr_t *scale = expr_beautify(raw_scale);
        expr_free(raw_scale);
        expr_free(twice_rate);
        /* The principal square root retains the sign information for negative real scales. */
        number_t two_value = num_create_from_long(2);
        expr_t *square = expr_pow(scale, &two_value);
        expr_t *q = expr_sqrt(square);
        expr_t *two_q = expr_mul(two, q);
        expr_t *z = reciprocal ? expr_mul(s, q) : expr_div(s, two_q);
        expr_t *four = expr_const_long(4);
        expr_t *four_square = expr_mul(four, square);
        expr_t *s_squared = expr_pow(s, &two_value);
        num_destroy(&two_value);
        expr_t *z_squared = reciprocal ? expr_mul(s_squared, square) : expr_div(s_squared, four_square);
        expr_t *twice_scale = expr_mul(two, scale);
        expr_t *shift_rate = reciprocal ? expr_mul(offset, twice_scale) : expr_div(offset, rate);
        expr_t *shift = expr_mul(s, shift_rate);
        expr_t *power = expr_add(z_squared, shift);
        expr_t *exponential = expr_exp(power);
        expr_t *sign = expr_div(scale, q);
        expr_t *shift_q = reciprocal ? expr_mul(offset, sign) : expr_mul(shift_rate, q);
        expr_t *argument = expr_add(z, shift_q);
        expr_t *complement = expr_erfc(argument);
        expr_t *product = expr_mul(exponential, complement);
        expr_t *term = expr_mul(sign, product);
        expr_t *numerator = f->ops == &ops_erf ? expr_add(initial, term) : expr_sub(initial, term);
        out = expr_div(numerator, s);
        expr_t *zero = expr_const_zero();
        laplace_add_half_plane(square, zero, conditions);
        expr_free(zero);
        expr_free(numerator);
        expr_free(term);
        expr_free(sign);
        expr_free(product);
        expr_free(complement);
        expr_free(argument);
        expr_free(shift_q);
        expr_free(exponential);
        expr_free(power);
        expr_free(shift);
        expr_free(shift_rate);
        expr_free(twice_scale);
        expr_free(z_squared);
        expr_free(s_squared);
        expr_free(four_square);
        expr_free(four);
        expr_free(z);
        expr_free(two_q);
        expr_free(two);
        expr_free(q);
        expr_free(square);
        expr_free(scale);
    }
    expr_free(initial);
    expr_free(offset);
    expr_free(rate);
    return out;
}

/* Root spellings and their reciprocals denote the same positive-source powers. */
static bool laplace_source_power(const expr_t *f, const expr_t *t, number_t *exponent)
{
    const expr_t *base = NULL;
    if (expr_match_pow_const(f, &base, exponent))
        return expr_is_var(base) && base->var_id == t->var_id;
    if (f->ops == &ops_div && expr_const_is_one(f->a) && laplace_source_power(f->b, t, exponent)) {
        number_t negative = num_neg(*exponent);
        num_destroy(exponent);
        *exponent = negative;
        return true;
    }
    if (!f->a || !expr_is_var(f->a) || f->a->var_id != t->var_id)
        return false;
    number_t order = NUM_ZERO;
    if (f->ops == &ops_sqrt)
        order = num_create_from_long(2);
    else if (f->ops == &ops_cubrt)
        order = num_create_from_long(3);
    else if (f->ops != &ops_root)
        return false;
    else if (!laplace_constant_value(f->b, &order)) {
        num_destroy(&order);
        return false;
    }
    bool valid = num_is_real(order) && num_is_integer(order) && num_gt(order, NUM_ONE);
    if (valid) {
        num_destroy(exponent);
        *exponent = num_div(NUM_ONE, order);
    }
    num_destroy(&order);
    return valid;
}

/* Preserve exact gamma factors; half-integer orders have an elementary pi expression. */
static expr_t *laplace_power_coefficient(number_t order)
{
    long numerator = 0, denominator = 0;
    if (num_get_small_rational(order, &numerator, &denominator) && denominator == 2 &&
        numerator > 0 && numerator <= 129) {
        number_t factor = num_clone(NUM_ONE);
        number_t two = num_create_from_long(2);
        for (long k = 1; k < numerator; k += 2) {
            number_t integer = num_create_from_long(k);
            number_t ratio = num_div(integer, two);
            number_t next = num_mul(factor, ratio);
            num_destroy(&factor);
            factor = next;
            num_destroy(&ratio);
            num_destroy(&integer);
        }
        expr_t *pi = expr_new_named_const(NUM_PI, "@pi");
        expr_t *root = expr_sqrt(pi);
        expr_t *scale = expr_new_const(factor);
        expr_t *out = expr_mul(scale, root);
        expr_free(scale);
        expr_free(root);
        expr_free(pi);
        num_destroy(&factor);
        num_destroy(&two);
        return out;
    }
    if (num_is_integer(order)) {
        number_t value = num_gamma(order);
        expr_t *out = expr_new_const(value);
        num_destroy(&value);
        return out;
    }
    expr_t *argument = expr_new_const(order);
    expr_t *out = expr_gamma(argument);
    expr_free(argument);
    return out;
}

/* Bound repeated differentiation to keep interactive transforms tractable. */
static unsigned int laplace_time_order(const expr_t *f, const expr_t *t)
{
    if (expr_is_var(f) && f->var_id == t->var_id)
        return 1u;
    number_t exponent = NUM_ZERO;
    const expr_t *base = NULL;
    const expr_t *power = NULL;
    bool matched = laplace_source_power(f, t, &exponent);
    if (!matched && expr_match_pow_expr(f, &base, &power) && expr_is_var(base) && base->var_id == t->var_id)
        matched = laplace_constant_value(power, &exponent);
    long numerator = 0, denominator = 0;
    bool valid = matched && num_get_small_rational(exponent, &numerator, &denominator) &&
                 denominator == 1 && numerator > 0 && numerator <= 64;
    num_destroy(&exponent);
    return valid ? (unsigned int)numerator : 0u;
}

static expr_t *laplace_rule(const expr_t *f, const expr_t *t, const expr_t *s, number_t *bound,
                            expr_t **conditions);

/* Extract a bounded time monomial without depending on multiplication association. */
static unsigned int laplace_time_monomial(const expr_t *f, const expr_t *t, expr_t **coefficient)
{
    *coefficient = NULL;
    unsigned int order = laplace_time_order(f, t);
    if (order) {
        *coefficient = expr_const_one();
        return order;
    }
    if (!laplace_uses(f, t)) {
        *coefficient = expr_clone(f);
        return 0u;
    }
    const expr_t *left = NULL, *right = NULL;
    if (!expr_match_mul_expr(f, &left, &right))
        return 0u;
    expr_t *a = NULL, *b = NULL;
    unsigned int first = laplace_time_monomial(left, t, &a);
    unsigned int second = laplace_time_monomial(right, t, &b);
    if (a && b && first + second <= 64u) {
        *coefficient = expr_mul(a, b);
        order = first + second;
    }
    expr_free(b);
    expr_free(a);
    return order;
}

/* Recognise quadratic exponents algebraically, without sampling binding values. */
static bool laplace_quadratic_parts(const expr_t *f, const expr_t *t, expr_t **quadratic,
                                   expr_t **linear, expr_t **constant)
{
    expr_t *derivative = expr_create_deriv(f, t);
    expr_t *rate = NULL, *offset = NULL;
    bool matched = derivative && laplace_affine_parts(derivative, t, &rate, &offset);
    expr_free(derivative);
    if (!matched) {
        expr_free(rate);
        expr_free(offset);
        return false;
    }
    expr_t *two = expr_const_long(2);
    expr_t *zero = expr_const_zero();
    expr_t *a = expr_div(rate, two);
    expr_t *b = expr_beautify(offset);
    expr_t *c = expr_substitute(f, t, zero);
    expr_t *square = expr_mul(t, t);
    expr_t *second = expr_mul(a, square);
    expr_t *first = expr_mul(b, t);
    expr_t *sum = expr_add(second, first);
    expr_t *candidate = expr_add(sum, c);
    expr_t *difference = expr_sub(f, candidate);
    expr_t *check = expr_simplify(difference);
    matched = check && expr_const_is_zero(check) && !laplace_uses(c, t);
    if (matched) {
        *quadratic = expr_beautify(a);
        *linear = expr_clone(b);
        *constant = expr_beautify(c);
    }
    expr_free(check);
    expr_free(difference);
    expr_free(candidate);
    expr_free(sum);
    expr_free(first);
    expr_free(second);
    expr_free(square);
    expr_free(c);
    expr_free(b);
    expr_free(a);
    expr_free(zero);
    expr_free(two);
    expr_free(offset);
    expr_free(rate);
    return matched;
}

/* Complete the square; Re(a)>0 makes exp(-a*t^2+b*t+d) integrable for every finite s. */
static expr_t *laplace_gaussian(const expr_t *a, const expr_t *b, const expr_t *d,
                               const expr_t *s, number_t *bound, expr_t **conditions)
{
    number_t coefficient = NUM_NAN;
    bool known = laplace_constant_value(a, &coefficient);
    number_t real = num_real_part(coefficient);
    bool excluded = known && !num_gt(real, NUM_ZERO);
    num_destroy(&real);
    num_destroy(&coefficient);
    if (excluded)
        return NULL;
    expr_t *root = expr_sqrt(a);
    expr_t *two = expr_const_long(2);
    expr_t *twice_root = expr_mul(two, root);
    expr_t *shift = expr_sub(s, b);
    expr_t *z = expr_div(shift, twice_root);
    expr_t *square = expr_mul(z, z);
    expr_t *power = expr_add(square, d);
    expr_t *exponential = expr_exp(power);
    expr_t *complement = expr_erfc(z);
    expr_t *product = expr_mul(exponential, complement);
    expr_t *pi = expr_new_const(NUM_PI);
    expr_t *sqrt_pi = expr_sqrt(pi);
    expr_t *numerator = expr_mul(sqrt_pi, product);
    expr_t *out = expr_div(numerator, twice_root);
    expr_t *zero = expr_const_zero();
    laplace_add_half_plane(a, zero, conditions);
    num_destroy(bound);
    *bound = num_clone(NUM_NINF);
    expr_free(zero);
    expr_free(numerator);
    expr_free(sqrt_pi);
    expr_free(pi);
    expr_free(product);
    expr_free(complement);
    expr_free(exponential);
    expr_free(power);
    expr_free(square);
    expr_free(z);
    expr_free(shift);
    expr_free(twice_root);
    expr_free(two);
    expr_free(root);
    return out;
}

static expr_t *laplace_gaussian_exponential(const expr_t *f, const expr_t *t, const expr_t *s,
                                          number_t *bound, expr_t **conditions)
{
    const expr_t *base = NULL, *power = NULL;
    const expr_t *argument = f->ops == &ops_exp ? f->a : NULL;
    if (!argument && expr_match_pow_expr(f, &base, &power) && expr_is_const(base) &&
        num_eq(base->c, NUM_E) && (!base->name || strcmp(base->name, "e") == 0))
        argument = power;
    if (!argument)
        return NULL;
    expr_t *a = NULL, *b = NULL, *d = NULL, *out = NULL;
    if (laplace_quadratic_parts(argument, t, &a, &b, &d) && !expr_const_is_zero(a)) {
        expr_t *decay = expr_neg(a);
        out = laplace_gaussian(decay, b, d, s, bound, conditions);
        expr_free(decay);
    }
    expr_free(d);
    expr_free(b);
    expr_free(a);
    return out;
}

/* Distribution aliases share their native kind and therefore the same transform formula. */
static expr_t *laplace_normal_distribution(const expr_t *f, const expr_t *t, const expr_t *s,
                                         number_t *bound, expr_t **conditions)
{
    expr_op_kind_t kind = f->ops->kind;
    if (kind != EXPR_KIND_NORMAL_PDF && kind != EXPR_KIND_NORMAL_CDF && kind != EXPR_KIND_NORMAL_LOGPDF)
        return NULL;
    expr_t *rate = NULL, *offset = NULL;
    if (!laplace_affine_parts(f->a, t, &rate, &offset))
        return NULL;
    expr_t *two = expr_const_long(2);
    expr_t *pi = expr_new_const(NUM_PI);
    expr_t *two_pi = expr_mul(two, pi);
    expr_t *out = NULL;
    if (kind == EXPR_KIND_NORMAL_CDF) {
        expr_t *sqrt_two = expr_sqrt(two);
        expr_t *scaled = expr_div(f->a, sqrt_two);
        expr_t *negative = expr_neg(scaled);
        expr_t *complement = expr_erfc(negative);
        expr_t *transform = laplace_affine_error_function(complement, t, s, bound, conditions);
        out = transform ? expr_div(transform, two) : NULL;
        expr_free(transform);
        expr_free(complement);
        expr_free(negative);
        expr_free(scaled);
        expr_free(sqrt_two);
    } else if (kind == EXPR_KIND_NORMAL_LOGPDF) {
        expr_t *square = expr_mul(f->a, f->a);
        expr_t *logarithm = expr_log(two_pi);
        expr_t *sum = expr_add(square, logarithm);
        expr_t *negative = expr_neg(sum);
        expr_t *polynomial = expr_div(negative, two);
        expr_t *expanded = expr_display_expanded(polynomial);
        out = expanded ? laplace_rule(expanded, t, s, bound, conditions) : NULL;
        expr_free(expanded);
        expr_free(polynomial);
        expr_free(negative);
        expr_free(sum);
        expr_free(logarithm);
        expr_free(square);
    } else {
        expr_t *clean_rate = expr_beautify(rate);
        if (expr_const_is_zero(clean_rate)) {
            expr_t *initial = expr_normal_pdf(offset);
            out = expr_div(initial, s);
            expr_free(initial);
        } else {
            expr_t *square = expr_mul(rate, rate);
            expr_t *decay = expr_div(square, two);
            expr_t *cross = expr_mul(rate, offset);
            expr_t *linear = expr_neg(cross);
            expr_t *offset_squared = expr_mul(offset, offset);
            expr_t *half_offset = expr_div(offset_squared, two);
            expr_t *constant = expr_neg(half_offset);
            expr_t *transform = laplace_gaussian(decay, linear, constant, s, bound, conditions);
            expr_t *normaliser = expr_sqrt(two_pi);
            out = transform ? expr_div(transform, normaliser) : NULL;
            expr_free(normaliser);
            expr_free(transform);
            expr_free(constant);
            expr_free(half_offset);
            expr_free(offset_squared);
            expr_free(linear);
            expr_free(cross);
            expr_free(decay);
            expr_free(square);
        }
        expr_free(clean_rate);
    }
    expr_free(two_pi);
    expr_free(pi);
    expr_free(two);
    expr_free(offset);
    expr_free(rate);
    return out;
}

/* Product association must not hide an exponential behind a time power or another factor. */
static bool laplace_split_exponential(const expr_t *f, const expr_t *t, expr_t **factor, expr_t **rest)
{
    const expr_t *argument = f->ops == &ops_exp ? f->a : NULL;
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    if (!argument && expr_match_pow_expr(f, &left, &right) && expr_is_const(left) &&
        num_eq(left->c, NUM_E) && (!left->name || strcmp(left->name, "e") == 0))
        argument = right;
    if (argument) {
        expr_t *rate = NULL;
        expr_t *offset = NULL;
        bool affine = laplace_affine_parts(argument, t, &rate, &offset);
        expr_free(rate);
        expr_free(offset);
        if (affine) {
            *factor = expr_clone(f);
            *rest = expr_const_one();
            return true;
        }
    }
    if (!expr_match_mul_expr(f, &left, &right))
        return false;
    expr_t *remaining = NULL;
    const expr_t *other = right;
    bool found = laplace_split_exponential(left, t, factor, &remaining);
    if (!found) {
        found = laplace_split_exponential(right, t, factor, &remaining);
        other = left;
    }
    if (!found)
        return false;
    expr_t *product = expr_mul(remaining, other);
    *rest = expr_beautify(product);
    expr_free(product);
    expr_free(remaining);
    return *rest != NULL;
}

/* Apply the shift to the whole base transform, including its convergence restrictions. */
static expr_t *laplace_exponential_product(const expr_t *factor, const expr_t *operand, const expr_t *t,
                                         const expr_t *s, number_t *bound, expr_t **conditions)
{
    const expr_t *argument = factor->ops == &ops_exp ? factor->a : NULL;
    const expr_t *base = NULL;
    const expr_t *power = NULL;
    if (!argument && expr_match_pow_expr(factor, &base, &power) && expr_is_const(base) &&
        num_eq(base->c, NUM_E) && (!base->name || strcmp(base->name, "e") == 0))
        argument = power;
    if (!argument)
        return NULL;
    expr_t *rate = NULL;
    expr_t *offset = NULL;
    if (!laplace_affine_parts(argument, t, &rate, &offset))
        return NULL;
    expr_t *shifted = expr_sub(s, rate);
    expr_t *clean_shifted = expr_beautify(shifted);
    expr_free(shifted);
    shifted = clean_shifted;
    expr_t *base_conditions = NULL;
    number_t base_bound = num_clone(NUM_ZERO);
    expr_t *formula = laplace_rule(operand, t, s, &base_bound, &base_conditions);
    expr_t *out = NULL;
    if (formula) {
        out = expr_substitute(formula, s, shifted);
        expr_t *shifted_conditions = expr_substitute(base_conditions, s, shifted);
        expr_t **tail = &shifted_conditions;
        while (*tail)
            tail = &(*tail)->b;
        *tail = *conditions;
        *conditions = shifted_conditions;
        if (!num_eq(base_bound, NUM_NINF)) {
            expr_t *limit = expr_new_const(base_bound);
            laplace_add_half_plane(shifted, limit, conditions);
            expr_free(limit);
        }
    } else {
        expr_t *args[] = {(expr_t *)operand, (expr_t *)t, shifted};
        out = expr_laplace_from_args(3u, args);
    }
    if (out) {
        expr_t *scale = expr_exp(offset);
        expr_t *scaled = expr_mul(scale, out);
        expr_free(scale);
        expr_free(out);
        out = scaled;
        num_destroy(bound);
        *bound = num_clone(NUM_NINF);
    }
    expr_free(formula);
    expr_free(base_conditions);
    num_destroy(&base_bound);
    expr_free(shifted);
    expr_free(offset);
    expr_free(rate);
    return out;
}

/* The unilateral derivative theorem uses right-hand initial values. No numerical
 * half-plane is inferred for an unspecified function; retain its formal transform. */
static expr_t *laplace_function_derivative(const expr_t *f, const expr_t *t, const expr_t *s,
                                          number_t *bound, expr_t **conditions)
{
    /* Native differentiation also produces arbitrary-function names ending in primes. */
    if (expr_is_arbitrary_function(f) && f->name) {
        size_t length = strlen(f->name), primes = 0u;
        while (primes < length && f->name[length - primes - 1u] == '\'')
            ++primes;
        if (primes && primes < length) {
            char *name = strndup(f->name, length - primes);
            expr_t *base = name ? expr_new_arbitrary_function(name, f->a) : NULL;
            expr_t *order = expr_const_long((long)primes);
            expr_t *derivative = base ? expr_new_ordered_derivative(base, order) : NULL;
            expr_t *out = derivative ? laplace_function_derivative(derivative, t, s, bound, conditions) : NULL;
            expr_free(derivative);
            expr_free(order);
            expr_free(base);
            free(name);
            return out;
        }
    }
    if (f->ops != &ops_ordered_derivative || !expr_is_arbitrary_function(f->a) ||
        !expr_is_var(f->a->a) || f->a->a->var_id != t->var_id || laplace_uses(f->b, t))
        return NULL;
    number_t value = NUM_NAN;
    bool known = laplace_constant_value(f->b, &value);
    bool valid = !known || (num_is_real(value) && num_is_integer(value) && num_ge(value, NUM_ZERO));
    long n = 0, denominator = 0;
    bool small = known && num_get_small_rational(value, &n, &denominator) && n <= 16;
    num_destroy(&value);
    if (!valid)
        return NULL;
    expr_t *args[] = {f->a, (expr_t *)t, (expr_t *)s};
    expr_t *transform = expr_laplace_from_args(3u, args);
    expr_t *power = expr_pow_xp(s, f->b);
    expr_t *leading = expr_mul(power, transform);
    expr_t *zero = expr_const_zero();
    expr_t *one = expr_const_one();
    expr_t *initial = expr_new_arbitrary_function(f->a->name, zero);
    expr_t *sum = NULL;
    if (small) {
        sum = expr_const_zero();
        for (long k = 0; k < n; ++k) {
            expr_t *order = expr_const_long(k);
            expr_t *exponent = expr_const_long(n - 1 - k);
            expr_t *at_zero = expr_new_ordered_derivative(initial, order);
            expr_t *factor = expr_pow_xp(s, exponent);
            expr_t *term = expr_mul(factor, at_zero);
            expr_t *next = expr_add(sum, term);
            expr_free(sum);
            sum = next;
            expr_free(term);
            expr_free(factor);
            expr_free(at_zero);
            expr_free(exponent);
            expr_free(order);
        }
    } else {
        expr_bindings_t *bindings = expr_bindings_from_expr_internal(f);
        char name[64] = {0};
        /* This bounded candidate list keeps the displayed dummy short without capturing parameters. */
        const char *candidates = "kjhmlpqruvwxyzabcdefgnot";
        for (const char *candidate = candidates; *candidate; ++candidate) {
            name[0] = *candidate;
            if (!expr_bindings_get(bindings, name) && (!s->name || strcmp(name, s->name) != 0) &&
                strcmp(name, f->a->name) != 0)
                break;
            name[0] = 0;
        }
        if (!name[0]) {
            size_t index = 0u;
            do {
                snprintf(name, sizeof(name), "laplace_initial_index_%zu", index++);
            } while (expr_bindings_get(bindings, name) || (s->name && strcmp(name, s->name) == 0));
        }
        expr_bindings_free(bindings);
        expr_t *k = expr_new_named_var(NUM_ZERO, name);
        expr_t *upper = expr_sub(f->b, one);
        expr_t *exponent = expr_sub(upper, k);
        expr_t *factor = expr_pow_xp(s, exponent);
        expr_t *at_zero = expr_new_ordered_derivative(initial, k);
        expr_t *term = expr_mul(factor, at_zero);
        sum = expr_new_finite_summation_range(term, k, zero, upper);
        expr_free(term);
        expr_free(at_zero);
        expr_free(factor);
        expr_free(exponent);
        expr_free(upper);
        expr_free(k);
    }
    expr_t *out = sum ? expr_sub(leading, sum) : NULL;
    if (out) {
        num_destroy(bound);
        *bound = num_clone(NUM_NINF);
        if (!known) {
            expr_t *item = expr_alloc(&ops_argument_list);
            item->a = expr_new_unary_internal(&ops_nonnegative_integer, expr_clone(f->b));
            item->b = *conditions;
            *conditions = item;
        }
    }
    expr_free(sum);
    expr_free(initial);
    expr_free(one);
    expr_free(zero);
    expr_free(leading);
    expr_free(power);
    expr_free(transform);
    return out;
}

/* Integrating in time divides the transform by s, retaining the primitive's initial value. */
static expr_t *laplace_time_integral(const expr_t *f, const expr_t *t, const expr_t *s, number_t *bound,
                                     expr_t **conditions)
{
    if (!expr_is_op(f, &ops_integral))
        return NULL;
    const expr_t *dummy = expr_integral_dummy_expr(f);
    const expr_t *upper = expr_integral_upper_bound_expr(f);
    const expr_t *lower = expr_integral_lower_bound_expr(f);
    if (!expr_is_var(dummy) || !expr_is_var(upper) || upper->var_id != t->var_id ||
        (lower && laplace_uses(lower, t)) || (dummy->var_id != t->var_id && laplace_uses(f->a, t)))
        return NULL;

    expr_t *integrand = expr_substitute(f->a, dummy, t);
    number_t base_bound = num_clone(NUM_ZERO);
    expr_t *base_conditions = NULL;
    expr_t *base = integrand ? laplace_rule(integrand, t, s, &base_bound, &base_conditions) : NULL;
    if (!base && integrand) {
        expr_t *args[] = {integrand, (expr_t *)t, (expr_t *)s};
        base = expr_laplace_from_args(3u, args);
        num_destroy(bound);
        *bound = num_clone(NUM_NINF);
    } else if (base) {
        num_destroy(bound);
        *bound = num_clone(num_cmp(base_bound, NUM_ZERO) < 0 ? NUM_ZERO : base_bound);
        /* Transfer only conditions belonging to a successfully established base transform. */
        expr_t **tail = &base_conditions;
        while (*tail)
            tail = &(*tail)->b;
        *tail = *conditions;
        *conditions = base_conditions;
        base_conditions = NULL;
    }
    expr_t *zero = expr_const_zero();
    expr_t *initial = lower && expr_const_is_zero(lower) ? expr_const_zero()
                        : lower ? expr_integral_with_bounds_internal(f->a, lower, zero, dummy)
                                : expr_integral_with_dummy_internal(f->a, zero, dummy);
    expr_t *numerator = base && initial ? expr_add(base, initial) : NULL;
    expr_t *out = numerator ? expr_div(numerator, s) : NULL;
    expr_free(numerator);
    expr_free(initial);
    expr_free(zero);
    expr_free(base);
    expr_free(integrand);
    expr_free(base_conditions);
    num_destroy(&base_bound);
    return out;
}

/* A real time translation of an unspecified function retains its finite history integral.
 * Use the original bound source variable: this cannot capture an unrelated parameter. */
static expr_t *laplace_function_translation(const expr_t *f, const expr_t *t, const expr_t *s,
                                           number_t *bound, expr_t **conditions)
{
    if (!expr_is_arbitrary_function(f) || f->b)
        return NULL;
    expr_t *rate = NULL, *offset = NULL;
    if (!laplace_affine_parts(f->a, t, &rate, &offset))
        return NULL;
    expr_t *clean_rate = expr_beautify(rate);
    expr_t *clean_offset = expr_beautify(offset);
    expr_free(rate);
    expr_free(offset);
    rate = clean_rate;
    offset = clean_offset;
    number_t value = NUM_NAN;
    bool known = laplace_constant_value(offset, &value);
    bool supported = expr_const_is_one(rate) && (!known || num_is_real(value));
    num_destroy(&value);
    expr_free(rate);
    if (!supported) {
        expr_free(offset);
        return NULL;
    }

    expr_t *base = expr_new_arbitrary_function(f->name, t);
    expr_t *args[] = {base, (expr_t *)t, (expr_t *)s};
    expr_t *transform = expr_laplace_from_args(3u, args);
    if (expr_const_is_zero(offset)) {
        bool unchanged = expr_struct_eq(f->a, t);
        expr_free(base);
        expr_free(offset);
        if (unchanged) {
            expr_free(transform);
            return NULL;
        }
        num_destroy(bound);
        *bound = num_clone(NUM_NINF);
        return transform;
    }
    expr_t *st = expr_mul(s, t);
    expr_t *minus_st = expr_neg(st);
    expr_t *kernel = expr_exp(minus_st);
    expr_t *integrand = expr_mul(kernel, base);
    expr_t *zero = expr_const_zero();
    expr_t *history = expr_integral_with_bounds_internal(integrand, offset, zero, t);
    expr_t *sum = expr_add(transform, history);
    expr_t *exponent = expr_mul(s, offset);
    expr_t *factor = expr_exp(exponent);
    expr_t *out = expr_mul(factor, sum);
    if (out) {
        num_destroy(bound);
        *bound = num_clone(NUM_NINF);
        if (!known) {
            expr_t *predicate = expr_new_unary_internal(&ops_real_parameter, expr_clone(offset));
            laplace_add_half_plane(predicate, zero, conditions);
            expr_free(predicate);
        }
    }
    expr_free(factor);
    expr_free(exponent);
    expr_free(sum);
    expr_free(history);
    expr_free(zero);
    expr_free(integrand);
    expr_free(kernel);
    expr_free(minus_st);
    expr_free(st);
    expr_free(transform);
    expr_free(base);
    expr_free(offset);
    return out;
}

/* Keep derivatives of unknown transforms compact instead of expanding their history terms. */
static bool laplace_contains_formal_transform(const expr_t *expr)
{
    return expr && (expr->ops == &ops_laplace || laplace_contains_formal_transform(expr->a) ||
                    laplace_contains_formal_transform(expr->b));
}

/* Linearity may retain an unspecified base transform without re-entering its simplifier. */
static expr_t *laplace_operand_rule(const expr_t *f, const expr_t *t, const expr_t *s, number_t *bound,
                                   expr_t **conditions)
{
    expr_t *out = laplace_rule(f, t, s, bound, conditions);
    if (!out && expr_is_arbitrary_function(f) && !f->b && expr_is_var(f->a) && f->a->var_id == t->var_id) {
        expr_t *args[] = {(expr_t *)f, (expr_t *)t, (expr_t *)s};
        out = expr_laplace_from_args(3u, args);
        num_destroy(bound);
        *bound = num_clone(NUM_NINF);
    }
    return out;
}

static expr_t *laplace_rule(const expr_t *f, const expr_t *t, const expr_t *s, number_t *bound,
                            expr_t **conditions)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool subtract = false;
    expr_t *out = NULL;
    expr_t *a = NULL;
    expr_t *b = NULL;
    number_t exponent = NUM_ZERO;

    if (f->ops == &ops_causal_convolution && expr_struct_eq(f->b, t)) {
        number_t left_bound = num_clone(NUM_ZERO), right_bound = num_clone(NUM_ZERO);
        expr_t *left = laplace_operand_rule(f->a->a, t, s, &left_bound, conditions);
        expr_t *right = laplace_operand_rule(f->a->b, t, s, &right_bound, conditions);
        if (!left) {
            expr_t *args[] = {f->a->a, (expr_t *)t, (expr_t *)s};
            left = expr_laplace_from_args(3u, args);
            num_destroy(&left_bound);
            left_bound = num_clone(NUM_NINF);
        }
        if (!right) {
            expr_t *args[] = {f->a->b, (expr_t *)t, (expr_t *)s};
            right = expr_laplace_from_args(3u, args);
            num_destroy(&right_bound);
            right_bound = num_clone(NUM_NINF);
        }
        out = left && right ? expr_mul(left, right) : NULL;
        num_destroy(bound);
        *bound = num_clone(num_cmp(left_bound, right_bound) > 0 ? left_bound : right_bound);
        num_destroy(&left_bound);
        num_destroy(&right_bound);
        expr_free(left);
        expr_free(right);
        return out;
    }

    out = laplace_time_integral(f, t, s, bound, conditions);
    if (out)
        return out;

    out = laplace_function_derivative(f, t, s, bound, conditions);
    if (out)
        return out;

    out = laplace_function_translation(f, t, s, bound, conditions);
    if (out)
        return out;

    if (expr_const_is_zero(f)) {
        num_destroy(bound);
        *bound = num_clone(NUM_NINF);
        return expr_const_zero();
    }
    if (f->ops == &ops_tan || f->ops == &ops_tanh) {
        expr_t *rate = NULL, *offset = NULL;
        number_t rate_value = NUM_NAN, offset_value = NUM_NAN;
        bool zero = laplace_affine_parts(f->a, t, &rate, &offset) &&
                    laplace_constant_value(rate, &rate_value) && num_is_zero(rate_value) &&
                    laplace_constant_value(offset, &offset_value) && num_is_zero(offset_value);
        expr_free(offset);
        expr_free(rate);
        num_destroy(&offset_value);
        num_destroy(&rate_value);
        if (zero) {
            num_destroy(bound);
            *bound = num_clone(NUM_NINF);
            return expr_const_zero();
        }
    }
    if ((f->ops == &ops_log || f->ops == &ops_log10) && expr_is_var(f->a) && f->a->var_id == t->var_id) {
        expr_t *gamma = expr_new_const(NUM_EULER_MASCHERONI);
        expr_t *logarithm = expr_log(s);
        expr_t *sum = expr_add(gamma, logarithm);
        expr_t *negative = expr_neg(sum);
        out = expr_div(negative, s);
        if (f->ops == &ops_log10) {
            expr_t *ten = expr_const_long(10);
            expr_t *scale = expr_log(ten);
            expr_t *scaled = expr_div(out, scale);
            expr_free(out);
            out = scaled;
            expr_free(scale);
            expr_free(ten);
        }
        expr_free(negative);
        expr_free(sum);
        expr_free(logarithm);
        expr_free(gamma);
        return out;
    }
    if (!laplace_uses(f, t))
        return expr_div(f, s);
    out = expr_laplace_elementary_rule(f, t, s, bound, conditions);
    if (out)
        return out;
    out = expr_laplace_special_rule(f, t, s, bound, conditions);
    if (out)
        return out;
    out = laplace_normal_distribution(f, t, s, bound, conditions);
    if (out)
        return out;
    out = laplace_gaussian_exponential(f, t, s, bound, conditions);
    if (out)
        return out;
    out = laplace_scaled_tanh(f, t, s, conditions);
    if (out)
        return out;
    out = laplace_scaled_bessel_zero(f, t, s, bound, conditions);
    if (out)
        return out;
    out = laplace_affine_error_function(f, t, s, bound, conditions);
    if (out)
        return out;
    out = laplace_affine_exponential(f, t, s, bound, conditions);
    if (out)
        return out;
    out = laplace_affine_trigonometric(f, t, s, bound, conditions);
    if (out)
        return out;
    out = laplace_trig_power(f, t, s, conditions);
    if (out)
        return out;
    if (expr_match_mul_expr(f, &left, &right)) {
        expr_t *factor = NULL;
        expr_t *rest = NULL;
        if (laplace_split_exponential(f, t, &factor, &rest))
            out = laplace_exponential_product(factor, rest, t, s, bound, conditions);
        expr_free(factor);
        expr_free(rest);
        if (out)
            return out;
    }
    if (expr_is_var(f) && f->var_id == t->var_id) {
        a = expr_mul(s, s);
        b = expr_const_one();
        out = expr_div(b, a);
    } else if (expr_match_add_sub_expr(f, &left, &right, &subtract)) {
        number_t other_bound = NUM_ZERO;
        a = laplace_operand_rule(left, t, s, bound, conditions);
        b = laplace_operand_rule(right, t, s, &other_bound, conditions);
        if (num_cmp(other_bound, *bound) > 0) {
            num_destroy(bound);
            *bound = num_clone(other_bound);
        }
        num_destroy(&other_bound);
        if (a && b && a->ops == &ops_div && b->ops == &ops_div && subtract) {
            expr_t *first = expr_mul(a->a, b->b);
            expr_t *second = expr_mul(b->a, a->b);
            expr_t *numerator = expr_sub(first, second);
            expr_t *denominator = expr_mul(a->b, b->b);
            out = expr_div(numerator, denominator);
            expr_free(first);
            expr_free(second);
            expr_free(numerator);
            expr_free(denominator);
        } else if (a && b)
            out = subtract ? expr_sub(a, b) : expr_add(a, b);
    } else if (f->ops == &ops_neg) {
        a = laplace_operand_rule(f->a, t, s, bound, conditions);
        if (a)
            out = expr_neg(a);
    } else if (f->ops == &ops_div && !laplace_uses(f->b, t)) {
        a = laplace_operand_rule(f->a, t, s, bound, conditions);
        if (a) {
            out = expr_div(a, f->b);
            expr_t *magnitude = expr_abs(f->b);
            expr_t *zero = expr_const_zero();
            laplace_add_half_plane(magnitude, zero, conditions);
            expr_free(magnitude);
            expr_free(zero);
        }
    } else if (expr_match_mul_expr(f, &left, &right)) {
        if (!laplace_uses(left, t)) {
            a = laplace_operand_rule(right, t, s, bound, conditions);
            if (a)
                out = expr_mul(left, a);
        } else if (!laplace_uses(right, t)) {
            a = laplace_operand_rule(left, t, s, bound, conditions);
            if (a)
                out = expr_mul(right, a);
        } else {
            expr_t *coefficient = NULL;
            unsigned int order = laplace_time_monomial(left, t, &coefficient);
            const expr_t *operand = right;
            if (!order) {
                expr_free(coefficient);
                order = laplace_time_monomial(right, t, &coefficient);
                operand = left;
            }
            if (order && !laplace_uses(operand, s)) {
                a = laplace_operand_rule(operand, t, s, bound, conditions);
                if (a) {
                    if (laplace_contains_formal_transform(a)) {
                        expr_t *wrts[64];
                        for (unsigned int i = 0u; i < order; ++i)
                            wrts[i] = (expr_t *)s;
                        b = expr_new_formal_derivative(a, order, wrts);
                    } else {
                        b = expr_create_nth_deriv(order, a, s);
                    }
                    if (b) {
                        expr_t *signed_derivative = order % 2u ? expr_neg(b) : expr_clone(b);
                        out = expr_mul(coefficient, signed_derivative);
                        expr_free(signed_derivative);
                    }
                }
            }
            expr_free(coefficient);
        }
    } else if (laplace_source_power(f, t, &exponent)) {
        number_t real = num_real_part(exponent);
        if (num_is_finite(exponent) && num_cmp(real, NUM_NEG_ONE) > 0) {
            number_t order = num_add(exponent, NUM_ONE);
            a = expr_pow(s, &order);
            b = laplace_power_coefficient(order);
            out = expr_div(b, a);
            num_destroy(&order);
        }
        num_destroy(&real);
    } else if (expr_match_pow_expr(f, &left, &right) && expr_is_var(left) &&
               left->var_id == t->var_id && !laplace_uses(right, t)) {
        expr_t *one = expr_const_one();
        expr_t *order = expr_add(right, one);
        a = expr_pow_xp(s, order);
        b = expr_gamma(order);
        out = expr_div(b, a);
        expr_t *condition = expr_alloc(&ops_argument_list);
        condition->a = expr_clone(right);
        condition->b = *conditions;
        *conditions = condition;
        expr_free(order);
        expr_free(one);
    }
    num_destroy(&exponent);
    expr_free(b);
    expr_free(a);
    return out;
}

expr_t *expr_laplace_formula(const expr_t *transform, number_t *abscissa, expr_t **conditions)
{
    if (!transform || transform->ops != &ops_laplace || !abscissa || !conditions)
        return NULL;
    const expr_t *target = transform->b->b->a;
    if (!expr_is_var(target)) {
        // Differentiate base formulas with respect to a variable, then evaluate at the requested argument.
        expr_bindings_t *bindings = expr_bindings_from_expr_internal(transform);
        char name[64];
        size_t index = 0u;
        do {
            snprintf(name, sizeof(name), "laplace_target_%zu", index++);
        } while ((transform->b->a->name && strcmp(transform->b->a->name, name) == 0) ||
                 expr_bindings_get(bindings, name));
        expr_bindings_free(bindings);
        expr_t *variable = expr_new_named_var(NUM_NAN, name);
        expr_t *base = expr_clone(transform);
        expr_free(base->b->b->a);
        base->b->b->a = expr_clone(variable);
        expr_t *base_conditions = NULL;
        expr_t *formula = expr_laplace_formula(base, abscissa, &base_conditions);
        expr_t *out = expr_substitute(formula, variable, target);
        *conditions = expr_substitute(base_conditions, variable, target);
        expr_free(base_conditions);
        expr_free(formula);
        expr_free(base);
        expr_free(variable);
        return out;
    }
    *conditions = NULL;
    *abscissa = num_clone(NUM_ZERO);
    expr_t *raw = laplace_rule(transform->a, transform->b->a, transform->b->b->a, abscissa, conditions);
    expr_t *out = raw ? expr_beautify(raw) : NULL;
    expr_free(raw);
    return out;
}

static number_t laplace_eval(expr_t *transform)
{
    if (transform->a->ops == &ops_tan || transform->a->ops == &ops_tanh) {
        expr_t *rate = NULL, *offset = NULL;
        bool affine = laplace_affine_parts(transform->a->a, transform->b->a, &rate, &offset);
        number_t r = affine ? expr_eval(rate) : num_clone(NUM_NAN);
        number_t b = affine ? expr_eval(offset) : num_clone(NUM_NAN);
        bool zero = num_is_zero(r) && num_is_zero(b);
        num_destroy(&b);
        num_destroy(&r);
        expr_free(offset);
        expr_free(rate);
        if (zero)
            return num_clone(NUM_ZERO);
    }
    expr_t *restricted = expr_laplace_result(transform);
    number_t result = restricted ? expr_eval(restricted) : num_clone(NUM_NAN);
    expr_free(restricted);
    return result;
}

/* Retain the formula and every convergence restriction in one native expression. */
expr_t *expr_laplace_result(const expr_t *transform)
{
    number_t bound = NUM_ZERO;
    expr_t *conditions = NULL;
    expr_t *formula = expr_laplace_formula(transform, &bound, &conditions);
    expr_t *limit = expr_new_const(bound);
    expr_t *args[] = {formula, transform->b->b->a, limit};
    expr_t *out = formula ? expr_real_domain_from_args(3u, args) : NULL;
    if (out) {
        if (num_eq(bound, NUM_NINF)) {
            expr_free(out->b);
            out->b = NULL;
        }
        expr_t **tail = out->b ? &out->b->b->b : &out->b;
        for (const expr_t *item = conditions; item; item = item->b) {
            bool half_plane = expr_is_op(item->a, &ops_argument_list);
            *tail = expr_alloc(&ops_argument_list);
            (*tail)->a = expr_clone(half_plane ? item->a->a : item->a);
            (*tail)->b = expr_alloc(&ops_argument_list);
            (*tail)->b->a = half_plane ? expr_clone(item->a->b)
                                     : expr_const_long(expr_is_op(item->a, &ops_nonnegative_integer) ? 0 : -1);
            tail = &(*tail)->b->b;
        }
    }
    expr_free(limit);
    expr_free(formula);
    expr_free(conditions);
    num_destroy(&bound);
    if (out && !out->b) {
        expr_t *unrestricted = expr_clone(out->a);
        expr_free(out);
        out = unrestricted;
    }
    return out;
}

static expr_t *laplace_simplify(const expr_t *expr, expr_t *a, expr_t *b)
{
    expr_t *result = expr_laplace_result(expr);
    if (!result)
        return expr_simplify_passthrough(expr, a, b);
    expr_free(a);
    expr_free(b);
    return result;
}

static expr_t *laplace_deriv(expr_t *transform)
{
    expr_t *wrt = (expr_t *)expr_current_wrt_internal();
    return wrt ? expr_new_formal_derivative(transform, 1u, &wrt) : NULL;
}

const expr_ops_t ops_laplace = {
    .eval = laplace_eval, .deriv = laplace_deriv, .reverse = expr_reverse_not_differentiable,
    .kind = EXPR_KIND_LAPLACE, .arity = EXPR_OP_BINARY, .diff_kind = EXPR_DIFF_SMOOTH,
    .expression_name = "ℒ", .function_name = "Laplace", .TeX_name = "\\mathcal{L}",
    .simplify = laplace_simplify,
};


/* Explain proven obstructions separately from transforms whose closed form is unsupported. */
const char *expr_transform_value_note(const expr_t *expr)
{
    if (!expr)
        return NULL;
    if (expr->ops == &ops_convolution || expr->ops == &ops_causal_convolution) {
        expr_t *simplified = expr_simplify(expr);
        const char *note = simplified && simplified->ops != expr->ops ? expr_transform_value_note(simplified) : NULL;
        expr_free(simplified);
        return note;
    }
    if (expr->ops == &ops_delta || expr->ops == &ops_principal_value || expr->ops == &ops_finite_part)
        return "This result is a distribution, not an ordinary pointwise function. "
               "Dirac impulses, principal values and finite parts do not have finite pointwise numerical values.";
    if (expr->ops == &ops_fourier || expr->ops == &ops_inverse_fourier) {
        expr_t *known = expr_fourier_result(expr);
        if (!known)
            return "Fourier transform left symbolic: no supported closed form has been established for this "
                   "function and its parameters. This does not establish non-existence of its transform.";
        if (expr->a->ops == &ops_bessel_j) {
            expr_free(known);
            return "The integer-order Bessel spectrum has inverse-square-root singularities at its support edges. "
                   "No finite pointwise value is assigned at those frequencies.";
        }
        /* A partial rule can still contain the same formal transform; do not recurse into it indefinitely. */
        const char *note = expr_struct_eq(known, expr) ? NULL : expr_transform_value_note(known);
        expr_free(known);
        return note;
    }
    if (expr->ops == &ops_laplace && expr->a->ops == &ops_tan) {
        expr_t *rate = NULL, *offset = NULL;
        if (!laplace_affine_parts(expr->a->a, expr->b->a, &rate, &offset)) {
            expr_free(offset);
            expr_free(rate);
            return NULL;
        }
        number_t r = expr_eval(rate), b = expr_eval(offset);
        const char *note = NULL;
        if (num_is_finite(r) && num_is_real(r) && !num_is_zero(r) && num_is_finite(b) && num_is_real(b))
            note = "The ordinary Laplace transform does not exist: the real tangent argument has poles on "
                   "the positive integration axis. Exponential damping does not remove these singularities.";
        else if (num_is_nan(r) || num_is_nan(b))
            note = "Transform left symbolic. A real affine tangent argument with non-zero rate has poles on "
                   "the positive integration axis, so its ordinary Laplace transform does not exist. "
                   "For tan(c*t), c = 0 gives zero; unspecified or complex c is not classified as divergent.";
        num_destroy(&b);
        num_destroy(&r);
        expr_free(offset);
        expr_free(rate);
        return note;
    }
    if (expr->ops == &ops_laplace) {
        const expr_t *operand = expr->a;
        const expr_t *source = expr->b->a;
        const char *note = NULL;
        const expr_t *base = NULL, *power = NULL;
        const expr_t *exponent = operand->ops == &ops_exp ? operand->a : NULL;
        if (!exponent && expr_match_pow_expr(operand, &base, &power) && expr_is_const(base) &&
            num_eq(base->c, NUM_E) && (!base->name || strcmp(base->name, "e") == 0))
            exponent = power;
        if (exponent) {
            expr_t *a = NULL, *b = NULL, *d = NULL;
            if (laplace_quadratic_parts(exponent, source, &a, &b, &d)) {
                number_t coefficient = NUM_NAN;
                bool known = laplace_constant_value(a, &coefficient);
                number_t real = num_real_part(coefficient);
                if (known && num_gt(real, NUM_ZERO))
                    note = "No ordinary Laplace transform: the positive real quadratic exponent grows faster "
                           "than any fixed exponential damping.";
                num_destroy(&real);
                num_destroy(&coefficient);
            }
            expr_free(d);
            expr_free(b);
            expr_free(a);
        }
        /* These tests deliberately require the direct source argument: translations and
           multipliers can remove a pole or change the growth and must be examined separately. */
        if (operand->a && expr_is_var(operand->a) && operand->a->var_id == source->var_id) {
            static const char *const obstruction[EXPR_KIND_COUNT] = {
                [EXPR_KIND_SEC] = "No ordinary Laplace transform: sec(t) has poles on the positive integration axis.",
                [EXPR_KIND_COSEC] = "No ordinary Laplace transform: cosec(t) has non-integrable poles, including at zero.",
                [EXPR_KIND_COT] = "No ordinary Laplace transform: cot(t) has non-integrable poles, including at zero.",
                [EXPR_KIND_COSECH] = "No ordinary Laplace transform: cosech(t) has a non-integrable pole at zero.",
                [EXPR_KIND_COTH] = "No ordinary Laplace transform: coth(t) has a non-integrable pole at zero.",
                [EXPR_KIND_GAMMA] = "No ordinary Laplace transform: gamma(t) has a pole at zero and super-exponential growth.",
                [EXPR_KIND_DIGAMMA] = "No ordinary Laplace transform: digamma(t) has a non-integrable pole at zero.",
                [EXPR_KIND_TRIGAMMA] = "No ordinary Laplace transform: trigamma(t) has a non-integrable pole at zero.",
                [EXPR_KIND_ZETA] = "No ordinary Laplace transform: zeta(t) has a non-integrable pole at t = 1.",
                [EXPR_KIND_ZETAP] = "No ordinary Laplace transform: the zeta derivative has a non-integrable pole at t = 1.",
            };
            if (obstruction[operand->ops->kind])
                note = obstruction[operand->ops->kind];
        }
        if (note)
            return note;
        expr_t *known = expr_laplace_result(expr);
        bool supported = known != NULL;
        expr_free(known);
        if (supported)
            return NULL;
        return "Transform left symbolic: no supported closed form has been established for this function and its parameters. "
               "This does not imply that the ordinary Laplace transform fails to exist.";
    }
    if (expr_is_integral_transform(expr))
        return NULL;
    const char *note = expr_transform_value_note(expr->a);
    return note ? note : expr_transform_value_note(expr->b);
}
