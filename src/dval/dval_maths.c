#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>

#include "dval_bindings_internal.h"
#include "dval_math_internal.h"

static inline number_t dv_eval_unary_num(dval_t *dv, number_t (*fn)(const number_t))
{
    return fn(dv_eval_num_internal(dv->a));
}

static inline number_t dv_eval_binary_num(
    dval_t *dv, number_t (*fn)(const number_t, const number_t))
{
    return fn(dv_eval_num_internal(dv->a), dv_eval_num_internal(dv->b));
}

static int dv_number_to_polygamma_order(number_t value, unsigned int *order)
{
    char *text;
    char *end = NULL;
    unsigned long parsed;

    if (!order || !num_is_real(value) || !num_is_integer(value) ||
        num_get_sign(value) < 0)
        return 0;

    text = num_to_string(value);
    if (!text)
        return 0;
    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno != 0 || !end || *end != '\0' || parsed > UINT_MAX) {
        free(text);
        return 0;
    }
    free(text);
    *order = (unsigned int)parsed;
    return 1;
}

static int dv_number_to_unsigned_long(number_t value, unsigned long *out)
{
    char *text;
    char *end = NULL;
    unsigned long parsed;

    if (!out || !num_is_real(value) || !num_is_integer(value) ||
        num_get_sign(value) < 0)
        return 0;

    text = num_to_string(value);
    if (!text)
        return 0;
    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno != 0 || !end || *end != '\0') {
        free(text);
        return 0;
    }
    free(text);
    *out = parsed;
    return 1;
}

static int dv_number_to_long(number_t value, long *out)
{
    char *text;
    char *end = NULL;
    long parsed;

    if (!out || !num_is_real(value) || !num_is_integer(value))
        return 0;

    text = num_to_string(value);
    if (!text)
        return 0;
    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || !end || *end != '\0') {
        free(text);
        return 0;
    }
    free(text);
    *out = parsed;
    return 1;
}

static dval_t *dv_chain_rule_with_factor(const dval_t *dv, dval_t *factor)
{
    dval_t *da = dv_get_dx_internal(dv->a);
    dval_t *out = dv_mul(factor, da);
    dv_free(factor);
    dv_free(da);
    return out;
}

static dval_t *dv_const_long_local(long value)
{
    NUM_SCOPE(scope);
    number_t n = num_create_from_long(value);
    dval_t *dv = dv_new_const(n);

    return dv;
}

static dval_t *dv_pow_long_local(const dval_t *dv, long exponent)
{
    NUM_SCOPE(scope);
    number_t n = num_create_from_long(exponent);
    dval_t *out = dv_pow(dv, &n);

    return out;
}

static dval_t *dv_add_long_local(const dval_t *dv, long value)
{
    NUM_SCOPE(scope);
    number_t n = num_create_from_long(value);
    dval_t *out = dv_add_num(dv, &n);

    return out;
}

static dval_t *dv_const_num_local(number_t value)
{
    NUM_SCOPE(scope);
    dval_t *dv = dv_new_const(value);
    return dv;
}

static dval_t *dv_log10_scale_factor_local(void)
{
    NUM_SCOPE(scope);
    number_t value = num_log(NUM_TEN);
    dval_t *dv = dv_new_const(value);

    if (dv)
        dv->binding_expr =
            dv_binding_expr_new_unary_op(&ops_log,
                                         dv_binding_expr_new_number_text("10"));
    return dv;
}

static dval_t *dv_const_ratio_local(number_t numerator, number_t denominator)
{
    NUM_SCOPE(scope);
    number_t value = num_div(numerator, denominator);

    return dv_const_num_local(value);
}

static dval_t *dv_const_neg_ratio_local(number_t numerator, number_t denominator)
{
    NUM_SCOPE(scope);
    number_t value = num_div(numerator, denominator);
    number_t neg_value = num_neg(value);

    return dv_const_num_local(neg_value);
}

number_t eval_sin(dval_t *dv) { return dv_eval_unary_num(dv, num_sin); }
number_t eval_cos(dval_t *dv) { return dv_eval_unary_num(dv, num_cos); }
static bool binding_expr_is_number_long(const dv_binding_expr_t *expr,
                                        long expected)
{
    number_t value;
    number_t expected_value;
    bool match;

    if (!expr || expr->kind != DV_BINDING_EXPR_NUMBER)
        return false;

    value = dv_binding_expr_eval(expr);
    expected_value = num_create_from_long(expected);
    match = num_eq(value, expected_value);
    num_destroy(&expected_value);
    num_destroy(&value);
    return match;
}

static int binding_expr_pi_over_two_sign(const dv_binding_expr_t *expr)
{
    if (!expr)
        return 0;

    if (expr->kind == DV_BINDING_EXPR_NEG)
        return -binding_expr_pi_over_two_sign(expr->u.unary.child);

    if (expr->kind == DV_BINDING_EXPR_DIV &&
        expr->u.binary.left &&
        expr->u.binary.left->kind == DV_BINDING_EXPR_CONST &&
        expr->u.binary.left->u.const_id == DV_BINDING_CONST_PI &&
        binding_expr_is_number_long(expr->u.binary.right, 2))
        return 1;

    return 0;
}

number_t eval_tan(dval_t *dv)
{
    int pole_sign = dv && dv->a && dv->a->binding_expr
        ? binding_expr_pi_over_two_sign(dv->a->binding_expr) : 0;

    if (pole_sign > 0)
        return NUM_INF;
    if (pole_sign < 0)
        return NUM_NINF;
    return dv_eval_unary_num(dv, num_tan);
}

number_t eval_sinh(dval_t *dv) { return dv_eval_unary_num(dv, num_sinh); }
number_t eval_cosh(dval_t *dv) { return dv_eval_unary_num(dv, num_cosh); }
number_t eval_tanh(dval_t *dv) { return dv_eval_unary_num(dv, num_tanh); }

number_t eval_asin(dval_t *dv) { return dv_eval_unary_num(dv, num_asin); }
number_t eval_acos(dval_t *dv) { return dv_eval_unary_num(dv, num_acos); }
number_t eval_atan(dval_t *dv) { return dv_eval_unary_num(dv, num_atan); }

number_t eval_asinh(dval_t *dv) { return dv_eval_unary_num(dv, num_asinh); }
number_t eval_acosh(dval_t *dv) { return dv_eval_unary_num(dv, num_acosh); }
number_t eval_atanh(dval_t *dv) { return dv_eval_unary_num(dv, num_atanh); }

number_t eval_exp(dval_t *dv) { return dv_eval_unary_num(dv, num_exp); }
number_t eval_log(dval_t *dv) { return dv_eval_unary_num(dv, num_log); }
number_t eval_log10(dval_t *dv) { return dv_eval_unary_num(dv, num_log10); }
number_t eval_sqrt(dval_t *dv) { return dv_eval_unary_num(dv, num_sqrt); }
number_t eval_floor(dval_t *dv) { return dv_eval_unary_num(dv, num_floor); }
number_t eval_ceil(dval_t *dv) { return dv_eval_unary_num(dv, num_ceil); }
number_t eval_abs(dval_t *dv) { return dv_eval_unary_num(dv, num_abs); }
number_t eval_erf(dval_t *dv) { return dv_eval_unary_num(dv, num_erf); }
number_t eval_erfc(dval_t *dv) { return dv_eval_unary_num(dv, num_erfc); }
number_t eval_lgamma(dval_t *dv) { return dv_eval_unary_num(dv, num_lgamma); }
number_t eval_erfinv(dval_t *dv) { return dv_eval_unary_num(dv, num_erfinv); }
number_t eval_erfcinv(dval_t *dv) { return dv_eval_unary_num(dv, num_erfcinv); }
number_t eval_gamma(dval_t *dv) { return dv_eval_unary_num(dv, num_gamma); }
number_t eval_digamma(dval_t *dv) { return dv_eval_unary_num(dv, num_digamma); }
number_t eval_trigamma(dval_t *dv) { return dv_eval_unary_num(dv, num_trigamma); }
number_t eval_polygamma(dval_t *dv)
{
    number_t order_value = dv_eval_num_internal(dv->a);
    unsigned int order;

    if (!dv_number_to_polygamma_order(order_value, &order))
        return NUM_NAN;
    return num_polygamma(order, dv_eval_num_internal(dv->b));
}
number_t eval_gammainv(dval_t *dv) { return dv_eval_unary_num(dv, num_gammainv); }
number_t eval_lambert_w(dval_t *dv) { return dv_eval_unary_num(dv, num_productlog); }
number_t eval_lambert_w0(dval_t *dv) { return dv_eval_unary_num(dv, num_lambert_w0); }
number_t eval_lambert_wm1(dval_t *dv) { return dv_eval_unary_num(dv, num_lambert_wm1); }
number_t eval_normal_pdf(dval_t *dv) { return dv_eval_unary_num(dv, num_normal_pdf); }
number_t eval_normal_cdf(dval_t *dv) { return dv_eval_unary_num(dv, num_normal_cdf); }
number_t eval_normal_logpdf(dval_t *dv) { return dv_eval_unary_num(dv, num_normal_logpdf); }
number_t eval_ei(dval_t *dv) { return dv_eval_unary_num(dv, num_ei); }
number_t eval_e1(dval_t *dv) { return dv_eval_unary_num(dv, num_e1); }

number_t eval_hypot(dval_t *dv)
{
    return dv_eval_binary_num(dv, num_hypot);
}

number_t eval_beta(dval_t *dv)
{
    return dv_eval_binary_num(dv, num_beta);
}

number_t eval_logbeta(dval_t *dv)
{
    return dv_eval_binary_num(dv, num_logbeta);
}

number_t eval_gammainc_lower(dval_t *dv)
{
    return dv_eval_binary_num(dv, num_gammainc_lower);
}

number_t eval_gammainc_upper(dval_t *dv)
{
    return dv_eval_binary_num(dv, num_gammainc_upper);
}

number_t eval_gammainc_P(dval_t *dv)
{
    return dv_eval_binary_num(dv, num_gammainc_P);
}

number_t eval_gammainc_Q(dval_t *dv)
{
    return dv_eval_binary_num(dv, num_gammainc_Q);
}

number_t eval_factorial(dval_t *dv)
{
    number_t value = dv_eval_num_internal(dv->a);
    unsigned long n;

    return dv_number_to_unsigned_long(value, &n) ? num_factorial(n) : NUM_NAN;
}

number_t eval_fibonacci(dval_t *dv)
{
    number_t value = dv_eval_num_internal(dv->a);
    unsigned long n;

    return dv_number_to_unsigned_long(value, &n) ? num_fibonacci(n) : NUM_NAN;
}

number_t eval_partition(dval_t *dv) { return dv_eval_unary_num(dv, num_partition); }
number_t eval_isqrt(dval_t *dv) { return dv_eval_unary_num(dv, num_isqrt); }
number_t eval_gcd(dval_t *dv) { return dv_eval_binary_num(dv, num_gcd); }
number_t eval_lcm(dval_t *dv) { return dv_eval_binary_num(dv, num_lcm); }
number_t eval_mod(dval_t *dv) { return dv_eval_binary_num(dv, num_mod); }
number_t eval_modinv(dval_t *dv) { return dv_eval_binary_num(dv, num_modinv); }

number_t eval_is_prime(dval_t *dv)
{
    return num_create_from_long(num_is_prime(dv_eval_num_internal(dv->a)) ? 1L : 0L);
}

number_t eval_next_prime(dval_t *dv) { return dv_eval_unary_num(dv, num_next_prime); }
number_t eval_prev_prime(dval_t *dv) { return dv_eval_unary_num(dv, num_prev_prime); }

number_t eval_bit_and(dval_t *dv) { return dv_eval_binary_num(dv, num_bit_and); }
number_t eval_bit_or(dval_t *dv) { return dv_eval_binary_num(dv, num_bit_or); }
number_t eval_bit_xor(dval_t *dv) { return dv_eval_binary_num(dv, num_bit_xor); }
number_t eval_bit_not(dval_t *dv) { return dv_eval_unary_num(dv, num_bit_not); }

number_t eval_shl(dval_t *dv)
{
    number_t value = dv_eval_num_internal(dv->a);
    number_t bits_value = dv_eval_num_internal(dv->b);
    long bits;

    return dv_number_to_long(bits_value, &bits) ? num_shl(value, bits) : NUM_NAN;
}

number_t eval_shr(dval_t *dv)
{
    number_t value = dv_eval_num_internal(dv->a);
    number_t bits_value = dv_eval_num_internal(dv->b);
    long bits;

    return dv_number_to_long(bits_value, &bits) ? num_shr(value, bits) : NUM_NAN;
}

number_t eval_factors(dval_t *dv)
{
    return dv_eval_num_internal(dv->a);
}

number_t eval_atan2(dval_t *dv)
{
    return num_atan2(dv_eval_num_internal(dv->a), dv_eval_num_internal(dv->b));
}

dval_t *deriv_sin(dval_t *dv)
{
    return dv_chain_rule_with_factor(dv, dv_cos(dv->a));
}

dval_t *deriv_cos(dval_t *dv)
{
    dval_t *da      = dv_get_dx_internal(dv->a);
    dval_t *sin_a   = dv_sin(dv->a);
    dval_t *neg_sin = dv_neg(sin_a);
    dv_free(sin_a);
    dval_t *out     = dv_mul(neg_sin, da);
    dv_free(da);
    dv_free(neg_sin);
    return out;
}

dval_t *deriv_tan(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *t   = dv_tan(dv->a);
    dval_t *t2  = dv_pow_long_local(t, 2);
    dval_t *one = dv_new_const(NUM_ONE);
    dval_t *fac = dv_add(one, t2);
    dval_t *out = dv_mul(fac, da);
    dv_free(da);
    dv_free(t);
    dv_free(t2);
    dv_free(one);
    dv_free(fac);
    return out;
}

dval_t *deriv_sinh(dval_t *dv)
{
    return dv_chain_rule_with_factor(dv, dv_cosh(dv->a));
}

dval_t *deriv_cosh(dval_t *dv)
{
    return dv_chain_rule_with_factor(dv, dv_sinh(dv->a));
}

dval_t *deriv_tanh(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *t   = dv_tanh(dv->a);
    dval_t *t2  = dv_pow_long_local(t, 2);
    dval_t *one = dv_new_const(NUM_ONE);
    dval_t *fac = dv_sub(one, t2);
    dval_t *out = dv_mul(fac, da);
    dv_free(da);
    dv_free(t);
    dv_free(t2);
    dv_free(one);
    dv_free(fac);
    return out;
}

dval_t *deriv_exp(dval_t *dv)
{
    return dv_chain_rule_with_factor(dv, dv_exp(dv->a));
}

dval_t *deriv_log(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *out = dv_div(da, dv->a);
    dv_free(da);
    return out;
}

dval_t *deriv_log10(dval_t *dv)
{
    dval_t *da = dv_get_dx_internal(dv->a);
    dval_t *ln10 = dv_log10_scale_factor_local();
    dval_t *den = dv_mul(dv->a, ln10);
    dval_t *out = dv_div(da, den);

    dv_free(da);
    dv_free(ln10);
    dv_free(den);
    return out;
}

dval_t *deriv_sqrt(dval_t *dv)
{
    dval_t *da   = dv_get_dx_internal(dv->a);
    dval_t *two  = dv_const_long_local(2);
    dval_t *sqra = dv_sqrt(dv->a);
    dval_t *den  = dv_mul(two, sqra);
    dv_free(sqra);
    dval_t *out  = dv_div(da, den);
    dv_free(da);
    dv_free(two);
    dv_free(den);
    return out;
}

dval_t *deriv_floor(dval_t *dv)
{
    (void)dv;
    return dv_new_const(NUM_ZERO);
}

dval_t *deriv_ceil(dval_t *dv)
{
    (void)dv;
    return dv_new_const(NUM_ZERO);
}

dval_t *deriv_not_differentiable(dval_t *dv)
{
    (void)dv;
    return dv_new_const(NUM_NAN);
}

dval_t *deriv_asin(dval_t *dv)
{
    dval_t *da   = dv_get_dx_internal(dv->a);
    dval_t *a2   = dv_pow_long_local(dv->a, 2);
    dval_t *one  = dv_new_const(NUM_ONE);
    dval_t *sub  = dv_sub(one, a2);
    dval_t *den  = dv_sqrt(sub);
    dv_free(sub);
    dval_t *out  = dv_div(da, den);
    dv_free(da);
    dv_free(a2);
    dv_free(one);
    dv_free(den);
    return out;
}

dval_t *deriv_acos(dval_t *dv)
{
    dval_t *da   = dv_get_dx_internal(dv->a);
    dval_t *a2   = dv_pow_long_local(dv->a, 2);
    dval_t *one  = dv_new_const(NUM_ONE);
    dval_t *sub  = dv_sub(one, a2);
    dval_t *den  = dv_sqrt(sub);
    dv_free(sub);
    dval_t *num  = dv_neg(da);
    dval_t *out  = dv_div(num, den);
    dv_free(da);
    dv_free(a2);
    dv_free(one);
    dv_free(den);
    dv_free(num);
    return out;
}

dval_t *deriv_atan(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *a2  = dv_pow_long_local(dv->a, 2);
    dval_t *one = dv_new_const(NUM_ONE);
    dval_t *den = dv_add(one, a2);
    dval_t *out = dv_div(da, den);
    dv_free(da);
    dv_free(a2);
    dv_free(one);
    dv_free(den);
    return out;
}

dval_t *deriv_atan2(dval_t *dv)
{
    dval_t *y  = dv->a;
    dval_t *x  = dv->b;
    dval_t *dy = dv_get_dx_internal(y);
    dval_t *dx = dv_get_dx_internal(x);
    dval_t *x_dy = dv_mul(x, dy);
    dval_t *y_dx = dv_mul(y, dx);
    dval_t *num  = dv_sub(x_dy, y_dx);
    dval_t *x2  = dv_mul(x, x);
    dval_t *y2  = dv_mul(y, y);
    dval_t *den = dv_add(x2, y2);
    dval_t *out = dv_div(num, den);
    dv_free(dy);
    dv_free(dx);
    dv_free(x_dy);
    dv_free(y_dx);
    dv_free(num);
    dv_free(x2);
    dv_free(y2);
    dv_free(den);
    return out;
}

dval_t *deriv_asinh(dval_t *dv)
{
    dval_t *da   = dv_get_dx_internal(dv->a);
    dval_t *a2   = dv_pow_long_local(dv->a, 2);
    dval_t *one  = dv_new_const(NUM_ONE);
    dval_t *sum  = dv_add(one, a2);
    dval_t *den  = dv_sqrt(sum);
    dv_free(sum);
    dval_t *out  = dv_div(da, den);
    dv_free(da);
    dv_free(a2);
    dv_free(one);
    dv_free(den);
    return out;
}

dval_t *deriv_acosh(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *one = dv_new_const(NUM_ONE);
    dval_t *am1 = dv_sub(dv->a, one);
    dval_t *ap1 = dv_add(dv->a, one);
    dval_t *s1  = dv_sqrt(am1);
    dval_t *s2  = dv_sqrt(ap1);
    dval_t *den = dv_mul(s1, s2);
    dval_t *out = dv_div(da, den);
    dv_free(da);
    dv_free(one);
    dv_free(am1);
    dv_free(ap1);
    dv_free(s1);
    dv_free(s2);
    dv_free(den);
    return out;
}

dval_t *deriv_atanh(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *a2  = dv_pow_long_local(dv->a, 2);
    dval_t *one = dv_new_const(NUM_ONE);
    dval_t *den = dv_sub(one, a2);
    dval_t *out = dv_div(da, den);
    dv_free(da);
    dv_free(a2);
    dv_free(one);
    dv_free(den);
    return out;
}

dval_t *deriv_abs(dval_t *dv)
{
    dval_t *da   = dv_get_dx_internal(dv->a);
    dval_t *absa = dv_abs(dv->a);
    dval_t *sign = dv_div(dv->a, absa);
    dval_t *out  = dv_mul(sign, da);
    dv_free(da);
    dv_free(absa);
    dv_free(sign);
    return out;
}

dval_t *deriv_erf(dval_t *dv)
{
    dval_t *da     = dv_get_dx_internal(dv->a);
    dval_t *c      = dv_const_num_local(NUM_2_SQRTPI);
    dval_t *a2     = dv_pow_long_local(dv->a, 2);
    dval_t *neg_a2 = dv_neg(a2);
    dval_t *ea2    = dv_exp(neg_a2);
    dval_t *fac    = dv_mul(c, ea2);
    dval_t *out    = dv_mul(fac, da);
    dv_free(da);
    dv_free(c);
    dv_free(a2);
    dv_free(neg_a2);
    dv_free(ea2);
    dv_free(fac);
    return out;
}

dval_t *deriv_erfc(dval_t *dv)
{
    dval_t *da     = dv_get_dx_internal(dv->a);
    dval_t *c      = dv_const_num_local(NUM_NEG_TWO_OVER_SQRT_PI);
    dval_t *a2     = dv_pow_long_local(dv->a, 2);
    dval_t *neg_a2 = dv_neg(a2);
    dval_t *ea2    = dv_exp(neg_a2);
    dval_t *fac    = dv_mul(c, ea2);
    dval_t *out    = dv_mul(fac, da);
    dv_free(da);
    dv_free(c);
    dv_free(a2);
    dv_free(neg_a2);
    dv_free(ea2);
    dv_free(fac);
    return out;
}

dval_t *deriv_lgamma(dval_t *dv)
{
    return dv_chain_rule_with_factor(dv, dv_digamma(dv->a));
}

dval_t *deriv_hypot(dval_t *dv)
{
    dval_t *a    = dv->a;
    dval_t *b    = dv->b;
    dval_t *da   = dv_get_dx_internal(a);
    dval_t *db   = dv_get_dx_internal(b);
    dval_t *a_da = dv_mul(a, da);
    dval_t *b_db = dv_mul(b, db);
    dval_t *num  = dv_add(a_da, b_db);
    dval_t *h    = dv_hypot(a, b);
    dval_t *out  = dv_div(num, h);
    dv_free(da);
    dv_free(db);
    dv_free(a_da);
    dv_free(b_db);
    dv_free(num);
    dv_free(h);
    return out;
}

dval_t *deriv_erfinv(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *w   = dv_erfinv(dv->a);
    dval_t *w2  = dv_pow_long_local(w, 2);
    dval_t *ew2 = dv_exp(w2);
    dval_t *c   = dv_const_ratio_local(NUM_SQRT_PI, NUM_TWO);
    dval_t *fac = dv_mul(c, ew2);
    dval_t *out = dv_mul(fac, da);
    dv_free(da); dv_free(w); dv_free(w2); dv_free(ew2); dv_free(c); dv_free(fac);
    return out;
}

dval_t *deriv_erfcinv(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *w   = dv_erfcinv(dv->a);
    dval_t *w2  = dv_pow_long_local(w, 2);
    dval_t *ew2 = dv_exp(w2);
    dval_t *c   = dv_const_neg_ratio_local(NUM_SQRT_PI, NUM_TWO);
    dval_t *fac = dv_mul(c, ew2);
    dval_t *out = dv_mul(fac, da);
    dv_free(da); dv_free(w); dv_free(w2); dv_free(ew2); dv_free(c); dv_free(fac);
    return out;
}

dval_t *deriv_gamma(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *g   = dv_gamma(dv->a);
    dval_t *dg  = dv_digamma(dv->a);
    dval_t *gdg = dv_mul(g, dg);
    dval_t *out = dv_mul(gdg, da);
    dv_free(da); dv_free(g); dv_free(dg); dv_free(gdg);
    return out;
}

dval_t *deriv_digamma(dval_t *dv)
{
    return dv_chain_rule_with_factor(dv, dv_trigamma(dv->a));
}

dval_t *deriv_trigamma(dval_t *dv)
{
    dval_t *factor = dv_polygamma(2u, dv->a);

    return dv_chain_rule_with_factor(dv, factor);
}

dval_t *deriv_polygamma(dval_t *dv)
{
    number_t order_value = dv_eval_num_internal(dv->a);
    unsigned int order;
    dval_t *factor;
    dval_t *db;
    dval_t *out;

    if (!dv_number_to_polygamma_order(order_value, &order))
        return dv_new_const(NUM_NAN);

    factor = dv_polygamma(order + 1u, dv->b);
    db = dv_get_dx_internal(dv->b);
    out = dv_mul(factor, db);
    dv_free(factor);
    dv_free(db);
    return out;
}

dval_t *deriv_gammainv(dval_t *dv)
{
    dval_t *da   = dv_get_dx_internal(dv->a);
    dval_t *y    = dv_gammainv(dv->a);
    dval_t *psi  = dv_digamma(y);
    dval_t *xpsi = dv_mul(dv->a, psi);
    dval_t *one  = dv_new_const(NUM_ONE);
    dval_t *fac  = dv_div(one, xpsi);
    dval_t *out  = dv_mul(fac, da);
    dv_free(da); dv_free(y); dv_free(psi); dv_free(xpsi); dv_free(one); dv_free(fac);
    return out;
}

dval_t *deriv_lambert_w0(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *w   = dv_lambert_w0(dv->a);
    dval_t *wp1 = dv_add_long_local(w, 1);
    dval_t *den = dv_mul(dv->a, wp1);
    dval_t *fac = dv_div(w, den);
    dval_t *out = dv_mul(fac, da);
    dv_free(da); dv_free(w); dv_free(wp1); dv_free(den); dv_free(fac);
    return out;
}

dval_t *deriv_lambert_w(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *w   = dv_lambert_w(dv->a);
    dval_t *wp1 = dv_add_long_local(w, 1);
    dval_t *den = dv_mul(dv->a, wp1);
    dval_t *fac = dv_div(w, den);
    dval_t *out = dv_mul(fac, da);
    dv_free(da); dv_free(w); dv_free(wp1); dv_free(den); dv_free(fac);
    return out;
}

dval_t *deriv_lambert_wm1(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *w   = dv_lambert_wm1(dv->a);
    dval_t *wp1 = dv_add_long_local(w, 1);
    dval_t *den = dv_mul(dv->a, wp1);
    dval_t *fac = dv_div(w, den);
    dval_t *out = dv_mul(fac, da);
    dv_free(da); dv_free(w); dv_free(wp1); dv_free(den); dv_free(fac);
    return out;
}

dval_t *deriv_normal_pdf(dval_t *dv)
{
    dval_t *da   = dv_get_dx_internal(dv->a);
    dval_t *neg_a = dv_neg(dv->a);
    dval_t *phi  = dv_normal_pdf(dv->a);
    dval_t *fac  = dv_mul(neg_a, phi);
    dval_t *out  = dv_mul(fac, da);
    dv_free(da); dv_free(neg_a); dv_free(phi); dv_free(fac);
    return out;
}

dval_t *deriv_normal_cdf(dval_t *dv)
{
    return dv_chain_rule_with_factor(dv, dv_normal_pdf(dv->a));
}

dval_t *deriv_normal_logpdf(dval_t *dv)
{
    return dv_chain_rule_with_factor(dv, dv_neg(dv->a));
}

dval_t *deriv_ei(dval_t *dv)
{
    dval_t *da  = dv_get_dx_internal(dv->a);
    dval_t *ea  = dv_exp(dv->a);
    dval_t *fac = dv_div(ea, dv->a);
    dval_t *out = dv_mul(fac, da);
    dv_free(da); dv_free(ea); dv_free(fac);
    return out;
}

dval_t *deriv_e1(dval_t *dv)
{
    dval_t *da    = dv_get_dx_internal(dv->a);
    dval_t *neg_a = dv_neg(dv->a);
    dval_t *en_a  = dv_exp(neg_a);
    dval_t *neg_en = dv_neg(en_a);
    dval_t *fac   = dv_div(neg_en, dv->a);
    dval_t *out   = dv_mul(fac, da);
    dv_free(da); dv_free(neg_a); dv_free(en_a); dv_free(neg_en); dv_free(fac);
    return out;
}

dval_t *deriv_beta(dval_t *dv)
{
    dval_t *da    = dv_get_dx_internal(dv->a);
    dval_t *db    = dv_get_dx_internal(dv->b);
    dval_t *apb   = dv_add(dv->a, dv->b);
    dval_t *dg_a  = dv_digamma(dv->a);
    dval_t *dg_b  = dv_digamma(dv->b);
    dval_t *dg_ab = dv_digamma(apb);
    dval_t *diff_a = dv_sub(dg_a, dg_ab);
    dval_t *diff_b = dv_sub(dg_b, dg_ab);
    dval_t *beta_n = dv_beta(dv->a, dv->b);
    dval_t *ca    = dv_mul(beta_n, diff_a);
    dval_t *cb    = dv_mul(beta_n, diff_b);
    dval_t *ta    = dv_mul(ca, da);
    dval_t *tb    = dv_mul(cb, db);
    dval_t *out   = dv_add(ta, tb);
    dv_free(da); dv_free(db); dv_free(apb);
    dv_free(dg_a); dv_free(dg_b); dv_free(dg_ab);
    dv_free(diff_a); dv_free(diff_b); dv_free(beta_n);
    dv_free(ca); dv_free(cb); dv_free(ta); dv_free(tb);
    return out;
}

dval_t *deriv_logbeta(dval_t *dv)
{
    dval_t *da    = dv_get_dx_internal(dv->a);
    dval_t *db    = dv_get_dx_internal(dv->b);
    dval_t *apb   = dv_add(dv->a, dv->b);
    dval_t *dg_a  = dv_digamma(dv->a);
    dval_t *dg_b  = dv_digamma(dv->b);
    dval_t *dg_ab = dv_digamma(apb);
    dval_t *diff_a = dv_sub(dg_a, dg_ab);
    dval_t *diff_b = dv_sub(dg_b, dg_ab);
    dval_t *ta    = dv_mul(diff_a, da);
    dval_t *tb    = dv_mul(diff_b, db);
    dval_t *out   = dv_add(ta, tb);
    dv_free(da); dv_free(db); dv_free(apb);
    dv_free(dg_a); dv_free(dg_b); dv_free(dg_ab);
    dv_free(diff_a); dv_free(diff_b); dv_free(ta); dv_free(tb);
    return out;
}

static dval_t *gammainc_x_density(const dval_t *s, const dval_t *x)
{
    dval_t *one = dv_new_const(NUM_ONE);
    dval_t *s_minus_one = dv_sub(s, one);
    dval_t *x_pow = dv_pow_dv(x, s_minus_one);
    dval_t *neg_x = dv_neg(x);
    dval_t *exp_neg_x = dv_exp(neg_x);
    dval_t *density = dv_mul(x_pow, exp_neg_x);

    dv_free(one);
    dv_free(s_minus_one);
    dv_free(x_pow);
    dv_free(neg_x);
    dv_free(exp_neg_x);
    return density;
}

static dval_t *deriv_gammainc_x_only(dval_t *dv, int sign, int regularised)
{
    dval_t *ds = dv_get_dx_internal(dv->a);
    dval_t *dx = dv_get_dx_internal(dv->b);
    dval_t *density;
    dval_t *factor;
    dval_t *out;

    if (!dv_const_is_zero(ds)) {
        dv_free(ds);
        dv_free(dx);
        return dv_new_const(NUM_NAN);
    }

    density = gammainc_x_density(dv->a, dv->b);
    factor = density;

    if (regularised) {
        dval_t *gamma_s = dv_gamma(dv->a);
        factor = dv_div(density, gamma_s);
        dv_free(density);
        dv_free(gamma_s);
    }

    if (sign < 0) {
        dval_t *neg = dv_neg(factor);
        dv_free(factor);
        factor = neg;
    }

    out = dv_mul(factor, dx);
    dv_free(ds);
    dv_free(dx);
    dv_free(factor);
    return out;
}

dval_t *deriv_gammainc_lower(dval_t *dv)
{
    return deriv_gammainc_x_only(dv, 1, 0);
}

dval_t *deriv_gammainc_upper(dval_t *dv)
{
    return deriv_gammainc_x_only(dv, -1, 0);
}

dval_t *deriv_gammainc_P(dval_t *dv)
{
    return deriv_gammainc_x_only(dv, 1, 1);
}

dval_t *deriv_gammainc_Q(dval_t *dv)
{
    return deriv_gammainc_x_only(dv, -1, 1);
}
