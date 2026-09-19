#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

/* Ordinary unilateral transforms only: local integrability at zero is a separate
 * requirement from exponential damping at infinity. The caller owns the result
 * and incorporates bound/conditions using expr_laplace_result's existing format.
 * This file deliberately provides no registration or changes to shared headers.
 */

static bool special_uses(const expr_t *expr, const expr_t *source)
{
    bool used = false;
    expr_t *variable = (expr_t *)source;
    return !expr_collect_var_usage(expr, 1u, &variable, &used) || used;
}

/* A supplied free-variable value must never select a branch of the formula.
 * Named constants with unset values remain symbolic and receive domain guards.
 */
static bool special_constant(const expr_t *expr)
{
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(expr);
    bool constant = true;
    for (size_t i = 0u; i < expr_bindings_count(bindings); ++i) {
        expr_t *binding = expr_bindings_get(bindings, expr_bindings_name_at(bindings, i));
        if (!expr_is_const(binding) || num_is_nan(binding->c)) {
            constant = false;
            break;
        }
    }
    expr_bindings_free(bindings);
    return constant;
}

/* Match homogeneous products and quotients structurally; offsets are not allowed. */
static expr_t *special_scale(const expr_t *expr, const expr_t *source)
{
    if (expr_is_var(expr) && expr->var_id == source->var_id)
        return expr_const_one();
    expr_t *inner = NULL;
    expr_t *out = NULL;
    if (expr->ops == &ops_neg) {
        inner = special_scale(expr->a, source);
        if (inner)
            out = expr_neg(inner);
    } else if (expr->ops == &ops_mul) {
        const expr_t *factor = !special_uses(expr->a, source) ? expr->a : expr->b;
        const expr_t *argument = factor == expr->a ? expr->b : expr->a;
        if (!special_uses(factor, source)) {
            inner = special_scale(argument, source);
            if (inner)
                out = expr_mul(factor, inner);
        }
    } else if (expr->ops == &ops_div && !special_uses(expr->b, source)) {
        inner = special_scale(expr->a, source);
        if (inner)
            out = expr_div(inner, expr->b);
    }
    expr_free(inner);
    return out;
}

/* Require a known, finite, non-zero real rate; positive-only families opt in. */
static expr_t *special_real_scale(const expr_t *argument, const expr_t *source, bool positive, number_t *value)
{
    expr_t *raw = special_scale(argument, source);
    expr_t *rate = raw ? expr_beautify(raw) : NULL;
    expr_free(raw);
    if (!rate || !special_constant(rate)) {
        expr_free(rate);
        return NULL;
    }
    number_t numeric = expr_eval(rate);
    bool valid = num_is_finite(numeric) && num_is_real(numeric) && !num_is_zero(numeric) &&
                 (!positive || num_gt(numeric, NUM_ZERO));
    if (!valid) {
        num_destroy(&numeric);
        expr_free(rate);
        return NULL;
    }
    *value = numeric;
    return rate;
}

/* Unknown rates are accepted with a deferred Re(a)>0 guard. Known rates remain
 * restricted to positive reals; accepting literal complex rates is a later step.
 * No condition is appended here, so an unsuccessful rule has no side effects.
 */
static expr_t *special_positive_scale(const expr_t *argument, const expr_t *source, bool *known)
{
    expr_t *raw = special_scale(argument, source);
    expr_t *rate = raw ? expr_beautify(raw) : NULL;
    expr_free(raw);
    if (!rate)
        return NULL;
    *known = special_constant(rate);
    if (*known) {
        number_t value = expr_eval(rate);
        bool valid = num_is_finite(value) && num_is_real(value) && num_gt(value, NUM_ZERO);
        num_destroy(&value);
        if (!valid) {
            expr_free(rate);
            return NULL;
        }
    }
    return rate;
}

/* Exactly the (value, real bound) pair encoding used by laplace_add_half_plane. */
static void special_add_condition(const expr_t *value, long lower_bound, expr_t **conditions)
{
    expr_t *limit = expr_const_long(lower_bound);
    expr_t *pair = expr_alloc(&ops_argument_list);
    pair->a = expr_clone(value);
    pair->b = expr_new_unary_internal(&ops_real_bound, limit);
    expr_t *item = expr_alloc(&ops_argument_list);
    item->a = pair;
    item->b = *conditions;
    *conditions = item;
}

static void special_set_bound(number_t *bound, number_t value)
{
    num_destroy(bound);
    *bound = num_clone(value);
}

/* E1(a*t) -> log(1+s/a)/s, Re(a)>0; Ei(-a*t) is its negative for real a>0.
 * Ei(a*t) -> -log(s/a-1)/s, a>0, Re(s)>a (real Ei on the positive axis).
 * These follow by integrating the defining exponential-integral representations;
 * the logarithmic singularity at t=0 is integrable. For real a>0, E1 and Ei(-a*t)
 * actually converge for Re(s)>-a, but Re(s)>0 safely excludes the quotient's
 * removable singularity at s=0. For symbolic a, Re(a)>0 and Re(s)>0 put both
 * a and a+s in the right half-plane, so arg(a+s)-arg(a) lies strictly within
 * (-pi,pi). Consequently log(1+s/a)=log(a+s)-log(a) on the principal branch.
 * The actual E1 half-plane is Re(s)>-Re(a). Ei still requires known real rates.
 */
static expr_t *special_exponential_integral(const expr_t *f, const expr_t *t, const expr_t *s,
                                           number_t *bound, expr_t **conditions)
{
    number_t rate_value = NUM_ZERO;
    bool known = true;
    expr_t *rate = f->ops == &ops_E1 ? special_positive_scale(f->a, t, &known)
                                    : special_real_scale(f->a, t, false, &rate_value);
    if (!rate)
        return NULL;
    bool growing = f->ops == &ops_Ei && num_gt(rate_value, NUM_ZERO);
    expr_t *magnitude = num_lt(rate_value, NUM_ZERO) ? expr_neg(rate) : expr_clone(rate);
    expr_t *ratio = expr_div(s, magnitude);
    expr_t *one = expr_const_one();
    expr_t *argument = growing ? expr_sub(ratio, one) : expr_add(one, ratio);
    expr_t *logarithm = expr_log(argument);
    expr_t *numerator = f->ops == &ops_Ei ? expr_neg(logarithm) : expr_clone(logarithm);
    expr_t *out = expr_div(numerator, s);
    if (out) {
        special_set_bound(bound, growing ? rate_value : NUM_ZERO);
        if (!known)
            special_add_condition(rate, 0, conditions);
    }
    expr_free(numerator);
    expr_free(logarithm);
    expr_free(argument);
    expr_free(one);
    expr_free(ratio);
    expr_free(magnitude);
    expr_free(rate);
    num_destroy(&rate_value);
    return out;
}

/* For Re(v)>0 and Re(a)>0, write R=(a/(s+a))^v. Then
 *   L{gamma(v,a*t)}=Gamma(v)*R/s, L{Gamma(v,a*t)}=Gamma(v)*(1-R)/s,
 *   L{P(v,a*t)}=R/s,              L{Q(v,a*t)}=(1-R)/s.
 * Fubini applied to the incomplete-gamma integral proves these formulas.
 * With Re(s)>0, both a and s+a have arguments strictly between -pi/2 and pi/2;
 * their difference lies in (-pi,pi), so the principal ratio power equals
 * exp(v*(log(a)-log(s+a))). The ratio need not itself have positive real part.
 * Upper gamma and Q also converge for Re(s)>-Re(a); that larger half-plane is not
 * claimed here because the quotient requires a separate removable value at s=0.
 * Shape extensions through zero/negative integers and time-dependent shapes are
 * deliberately excluded. Symbolic shapes/rates retain Re(v)>0 and Re(a)>0.
 */
static expr_t *special_incomplete_gamma(const expr_t *f, const expr_t *t, const expr_t *s,
                                       number_t *bound, expr_t **conditions)
{
    if (special_uses(f->a, t))
        return NULL;
    bool known = special_constant(f->a);
    if (known) {
        number_t shape = expr_eval(f->a);
        number_t real = num_real_part(shape);
        bool valid = num_is_finite(shape) && num_gt(real, NUM_ZERO);
        num_destroy(&real);
        num_destroy(&shape);
        if (!valid)
            return NULL;
    }
    bool known_rate = false;
    expr_t *rate = special_positive_scale(f->b, t, &known_rate);
    if (!rate)
        return NULL;
    bool upper = f->ops == &ops_gammainc_upper || f->ops == &ops_gammainc_Q;
    bool regularised = f->ops == &ops_gammainc_P || f->ops == &ops_gammainc_Q;
    expr_t *shift = expr_add(s, rate);
    expr_t *ratio = expr_div(rate, shift);
    expr_t *power = expr_pow_xp(ratio, f->a);
    expr_t *one = expr_const_one();
    expr_t *numerator = upper ? expr_sub(one, power) : expr_clone(power);
    expr_t *factor = regularised ? expr_const_one() : expr_gamma(f->a);
    expr_t *scaled = expr_mul(factor, numerator);
    expr_t *out = expr_div(scaled, s);
    if (out) {
        special_set_bound(bound, NUM_ZERO);
        if (!known)
            special_add_condition(f->a, 0, conditions);
        if (!known_rate)
            special_add_condition(rate, 0, conditions);
    }
    expr_free(scaled);
    expr_free(factor);
    expr_free(numerator);
    expr_free(one);
    expr_free(power);
    expr_free(ratio);
    expr_free(shift);
    expr_free(rate);
    return out;
}

/* Put r=sqrt(s^2+a^2), q=a/(s+r). For a>0 and Re(s)>0, the principal
 * root has Re(r)>0, s+r is in the right half-plane and q has a single principal
 * logarithm. Thus L{J_v(a*t)}=q^v/r for Re(v)>-1. At negative integer order
 * use J_{-n}=(-1)^n J_n BEFORE applying this formula: continuing q^v/r in v
 * would give the wrong ordinary transform. Other known orders with Re(v)<=-1
 * are rejected because the origin is not integrable.
 * L{Y_0(a*t)}=-2*log((s+r)/a)/(pi*r); its logarithmic origin is integrable.
 * No Y_n rule for n>=1 is inferred from the Y_0 formula.
 */
static expr_t *special_bessel(const expr_t *f, const expr_t *t, const expr_t *s,
                             number_t *bound, expr_t **conditions)
{
    if (special_uses(f->a, t))
        return NULL;
    bool second_kind = f->ops == &ops_bessel_y;
    bool known = special_constant(f->a);
    bool negative_integer = false;
    bool odd = false;
    if (known) {
        number_t order = expr_eval(f->a);
        if (!second_kind && num_is_zero(order)) {
            /* Preserve the existing J0 rule, including complex and zero scales. */
            num_destroy(&order);
            return NULL;
        }
        number_t real = num_real_part(order);
        negative_integer = num_is_finite(order) && num_is_real(order) && num_is_integer(order) &&
                           num_lt(order, NUM_ZERO);
        bool valid = num_is_finite(order) && (second_kind ? num_is_zero(order)
                                                        : negative_integer || num_gt(real, NUM_NEG_ONE));
        if (valid && negative_integer) {
            number_t two = num_create_from_long(2);
            number_t half = num_div(order, two);
            valid = num_is_finite(half);
            odd = !num_is_integer(half);
            num_destroy(&half);
            num_destroy(&two);
        }
        num_destroy(&real);
        num_destroy(&order);
        if (!valid)
            return NULL;
    } else if (second_kind) {
        return NULL;
    }
    number_t rate_value = NUM_ZERO;
    expr_t *rate = special_real_scale(f->b, t, true, &rate_value);
    if (!rate)
        return NULL;
    expr_t *s_square = expr_mul(s, s);
    expr_t *rate_square = expr_mul(rate, rate);
    expr_t *sum = expr_add(s_square, rate_square);
    expr_t *root = expr_sqrt(sum);
    expr_t *shift = expr_add(s, root);
    expr_t *out = NULL;
    if (second_kind) {
        expr_t *ratio = expr_div(shift, rate);
        expr_t *logarithm = expr_log(ratio);
        expr_t *minus_two = expr_const_long(-2);
        expr_t *numerator = expr_mul(minus_two, logarithm);
        expr_t *pi = expr_new_named_const(NUM_PI, "@pi");
        expr_t *denominator = expr_mul(pi, root);
        out = expr_div(numerator, denominator);
        expr_free(denominator);
        expr_free(pi);
        expr_free(numerator);
        expr_free(minus_two);
        expr_free(logarithm);
        expr_free(ratio);
    } else {
        expr_t *order = negative_integer ? expr_neg(f->a) : expr_clone(f->a);
        expr_t *ratio = expr_div(rate, shift);
        expr_t *power = expr_pow_xp(ratio, order);
        expr_t *numerator = odd ? expr_neg(power) : expr_clone(power);
        out = expr_div(numerator, root);
        expr_free(numerator);
        expr_free(power);
        expr_free(ratio);
        expr_free(order);
    }
    if (out) {
        special_set_bound(bound, NUM_ZERO);
        if (!known)
            special_add_condition(f->a, -1, conditions);
    }
    expr_free(shift);
    expr_free(root);
    expr_free(sum);
    expr_free(rate_square);
    expr_free(s_square);
    expr_free(rate);
    num_destroy(&rate_value);
    return out;
}

/* Keep the finite polynomial opaque to algebraic expansion. There are at most
 * 15 terms. A bounded search over single-letter names avoids capturing a binding
 * in z, including a supplied constant rate or a non-variable transform target.
 */
static expr_t *special_clausen_polynomial(const expr_t *z, unsigned int k)
{
    expr_bindings_t *bindings = expr_bindings_from_expr_internal(z);
    char name[2] = {0, 0};
    const char *candidates = "jklmnpqruvwxyzabcdefghio";
    for (const char *candidate = candidates; *candidate; ++candidate) {
        name[0] = *candidate;
        if (!expr_bindings_get(bindings, name))
            break;
        name[0] = 0;
    }
    expr_bindings_free(bindings);
    if (!name[0])
        return NULL;
    expr_t *index = expr_new_named_var(NUM_ZERO, name);
    expr_t *one = expr_const_one();
    expr_t *two = expr_const_long(2);
    expr_t *minus_one = expr_const_long(-1);
    expr_t *sign_order = expr_sub(index, one);
    expr_t *sign = expr_pow_xp(minus_one, sign_order);
    expr_t *twice_index = expr_mul(two, index);
    expr_t *degree = expr_const_long((long)(2u * k + 3u));
    expr_t *zeta_order = expr_sub(degree, twice_index);
    expr_t *zeta = expr_zeta(zeta_order);
    expr_t *power_degree = expr_const_long((long)(2u * k + 2u));
    expr_t *power_order = expr_sub(power_degree, twice_index);
    expr_t *power = expr_pow_xp(z, power_order);
    expr_t *coefficient = expr_mul(sign, zeta);
    expr_t *term = expr_mul(coefficient, power);
    expr_t *upper = expr_const_long((long)k);
    expr_t *out = expr_new_finite_summation_range(term, index, one, upper);
    expr_free(upper);
    expr_free(term);
    expr_free(coefficient);
    expr_free(power);
    expr_free(power_order);
    expr_free(power_degree);
    expr_free(zeta);
    expr_free(zeta_order);
    expr_free(degree);
    expr_free(twice_index);
    expr_free(sign);
    expr_free(sign_order);
    expr_free(minus_one);
    expr_free(two);
    expr_free(one);
    expr_free(index);
    return out;
}

/* Native Clausen convention: even p uses sum sin(n*t)/n^p, odd p uses
 * sum cos(n*t)/n^p. For Re(z)>0 define R_m=sum 1/(n^m*(n^2+z^2)). Then
 * R_1=(psi(1+i*z)+psi(1-i*z)+2*EulerGamma)/(2*z^2),
 * R_m=(zeta(m)-R_{m-2})/z^2 for odd m>=3, and
 * L{Cl_p(t)}=R_{p-1} for even p, z*R_p for odd p.
 * For p>=2 absolute Fourier convergence permits termwise integration. For p=1,
 * Abel limits give the same result for the locally integrable periodic function
 * -log|2*sin(t/2)|. No transform of its non-integrable cotangent derivative is used.
 * Scaling uses z=s/|a| and division by |a|. A negative real rate changes sign
 * only for even p. Re(s)>0 excludes every digamma pole and z=0. The finite
 * recurrence is accumulated with a single denominator, not nested quotients:
 * k=floor((p-1)/2), D=psi(1+i*z)+psi(1-i*z)+2*EulerGamma,
 * N=(-1)^k*D/2 + sum(j=1..k, (-1)^(j-1)*zeta(2*k+3-2*j)*z^(2*k+2-2*j)).
 * For either parity L{Cl_p(t)}=N/z^p. Keep the polynomial as a finite sum to
 * prevent rational simplification from expanding a deeply nested recurrence.
 * This exact representation can still lose numerical precision for very small
 * |z|. Fixed orders 1..32 bound numerical summation to at most 15 terms.
 */
static expr_t *special_clausen(const expr_t *f, const expr_t *t, const expr_t *s, number_t *bound)
{
    unsigned int order = 2u;
    const expr_t *argument = f->a;
    if (f->ops == &ops_clausen) {
        if (!special_constant(f->a))
            return NULL;
        number_t value = expr_eval(f->a);
        number_t maximum = num_create_from_long(32);
        bool valid = num_is_finite(value) && num_is_real(value) && num_is_integer(value) &&
                     num_gt(value, NUM_ZERO) && num_le(value, maximum);
        if (valid)
            order = (unsigned int)num_to_double(value); /* Exact after checking the small integer range. */
        num_destroy(&maximum);
        num_destroy(&value);
        if (!valid)
            return NULL;
        argument = f->b;
    }
    number_t rate_value = NUM_ZERO;
    expr_t *rate = special_real_scale(argument, t, false, &rate_value);
    if (!rate)
        return NULL;
    bool negative = num_lt(rate_value, NUM_ZERO);
    expr_t *magnitude = negative ? expr_neg(rate) : expr_clone(rate);
    expr_t *z = expr_div(s, magnitude);
    unsigned int k = (order - 1u) / 2u;
    expr_t *polynomial = k ? special_clausen_polynomial(z, k) : expr_const_zero();
    if (!polynomial) {
        expr_free(z);
        expr_free(magnitude);
        expr_free(rate);
        num_destroy(&rate_value);
        return NULL;
    }
    expr_t *one = expr_const_one();
    expr_t *two = expr_const_long(2);
    expr_t *imaginary = expr_new_const(NUM_I);
    expr_t *iz = expr_mul(imaginary, z);
    expr_t *plus = expr_add(one, iz);
    expr_t *minus = expr_sub(one, iz);
    expr_t *psi_plus = expr_digamma(plus);
    expr_t *psi_minus = expr_digamma(minus);
    expr_t *pair = expr_add(psi_plus, psi_minus);
    expr_t *gamma = expr_new_const(NUM_EULER_MASCHERONI);
    expr_t *twice_gamma = expr_mul(two, gamma);
    expr_t *numerator = expr_add(pair, twice_gamma);
    expr_t *half = expr_div(numerator, two);
    expr_t *signed_half = k % 2u ? expr_neg(half) : expr_clone(half);
    expr_t *combined = k ? expr_add(polynomial, signed_half) : expr_clone(signed_half);
    expr_t *signed_numerator = negative && !(order % 2u) ? expr_neg(combined) : expr_clone(combined);
    number_t exponent = num_create_from_long((long)order);
    expr_t *power = expr_pow(z, &exponent);
    expr_t *denominator = expr_mul(magnitude, power);
    expr_t *out = expr_div(signed_numerator, denominator);
    if (out)
        special_set_bound(bound, NUM_ZERO);
    expr_free(power);
    num_destroy(&exponent);
    expr_free(signed_numerator);
    expr_free(combined);
    expr_free(signed_half);
    expr_free(half);
    expr_free(polynomial);
    expr_free(denominator);
    expr_free(numerator);
    expr_free(twice_gamma);
    expr_free(gamma);
    expr_free(pair);
    expr_free(psi_minus);
    expr_free(psi_plus);
    expr_free(minus);
    expr_free(plus);
    expr_free(iz);
    expr_free(imaginary);
    expr_free(two);
    expr_free(one);
    expr_free(z);
    expr_free(magnitude);
    expr_free(rate);
    num_destroy(&rate_value);
    return out;
}

/* Return a supported special-function transform; a miss leaves caller metadata untouched. */
expr_t *expr_laplace_special_rule(const expr_t *f, const expr_t *t, const expr_t *s,
                                 number_t *bound, expr_t **conditions)
{
    if (!f || !t || !s || !bound || !conditions || !expr_is_var(t))
        return NULL;
    if (f->ops == &ops_E1 || f->ops == &ops_Ei)
        return special_exponential_integral(f, t, s, bound, conditions);
    if (f->ops == &ops_gammainc_lower || f->ops == &ops_gammainc_upper ||
        f->ops == &ops_gammainc_P || f->ops == &ops_gammainc_Q)
        return special_incomplete_gamma(f, t, s, bound, conditions);
    if (f->ops == &ops_bessel_j || f->ops == &ops_bessel_y)
        return special_bessel(f, t, s, bound, conditions);
    if (f->ops == &ops_clausen2 || f->ops == &ops_clausen)
        return special_clausen(f, t, s, bound);
    return NULL;
}
