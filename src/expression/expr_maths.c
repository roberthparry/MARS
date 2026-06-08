#include <limits.h>
#include <stddef.h>
#include <stdlib.h>

#include "expr_bindings.h"
#include "expr_maths.h"
#include "ustring.h"

static int expr_text_to_unsigned_long(const string_t *text, unsigned long *out)
{
    string_cursor_t *cursor;
    bool saw_digit = false;
    unsigned long value = 0u;

    if (!text || !out || string_length(text) == 0u)
        return 0;

    cursor = string_cursor_new(text);
    if (!cursor)
        return 0;

    if (rune_is_equal(string_cursor_peek(cursor), '+'))
        (void)string_cursor_next(cursor);

    while (!string_cursor_done(cursor)) {
        rune_t rune = string_cursor_peek(cursor);
        char ch = '\0';
        unsigned int digit;

        if (!rune_to_ascii(rune, &ch) || ch < '0' || ch > '9') {
            string_cursor_free(cursor);
            return 0;
        }

        digit = (unsigned int)(ch - '0');
        if (value > (ULONG_MAX - digit) / 10u) {
            string_cursor_free(cursor);
            return 0;
        }
        value = value * 10u + digit;
        saw_digit = true;
        (void)string_cursor_next(cursor);
    }

    string_cursor_free(cursor);
    if (!saw_digit)
        return 0;

    *out = value;
    return 1;
}

static int expr_text_to_long(const string_t *text, long *out)
{
    string_cursor_t *cursor;
    bool negative = false;
    bool saw_digit = false;
    unsigned long value = 0u;
    unsigned long limit;

    if (!text || !out || string_length(text) == 0u)
        return 0;

    cursor = string_cursor_new(text);
    if (!cursor)
        return 0;

    if (rune_is_equal(string_cursor_peek(cursor), '-')) {
        negative = true;
        (void)string_cursor_next(cursor);
    }

    limit = negative ? (unsigned long)LONG_MAX + 1u : (unsigned long)LONG_MAX;
    while (!string_cursor_done(cursor)) {
        rune_t rune = string_cursor_peek(cursor);
        char ch = '\0';
        unsigned int digit;

        if (!rune_to_ascii(rune, &ch) || ch < '0' || ch > '9') {
            string_cursor_free(cursor);
            return 0;
        }

        digit = (unsigned int)(ch - '0');
        if (value > (limit - digit) / 10u) {
            string_cursor_free(cursor);
            return 0;
        }
        value = value * 10u + digit;
        saw_digit = true;
        (void)string_cursor_next(cursor);
    }

    string_cursor_free(cursor);
    if (!saw_digit)
        return 0;

    *out = negative && value == (unsigned long)LONG_MAX + 1u
        ? LONG_MIN
        : (negative ? -(long)value : (long)value);
    return 1;
}

static inline number_t expr_eval_unary_num(expr_t *dv, number_t (*fn)(const number_t))
{
    return fn(expr_eval_num_internal(dv->a));
}

static inline number_t expr_eval_binary_num(
    expr_t *dv, number_t (*fn)(const number_t, const number_t))
{
    return fn(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv->b));
}

int expr_number_to_polygamma_order(number_t value, unsigned int *order)
{
    string_t *text;
    unsigned long parsed;

    if (!order || !num_is_real(value) || !num_is_integer(value) ||
        num_get_sign(value) < 0)
        return 0;

    text = num_to_string(value);
    if (!text)
        return 0;
    if (!expr_text_to_unsigned_long(text, &parsed) || parsed > UINT_MAX) {
        string_free(text);
        return 0;
    }
    string_free(text);
    *order = (unsigned int)parsed;
    return 1;
}

static int expr_number_to_unsigned_long(number_t value, unsigned long *out)
{
    string_t *text;
    unsigned long parsed;

    if (!out || !num_is_real(value) || !num_is_integer(value) ||
        num_get_sign(value) < 0)
        return 0;

    text = num_to_string(value);
    if (!text)
        return 0;
    if (!expr_text_to_unsigned_long(text, &parsed)) {
        string_free(text);
        return 0;
    }
    string_free(text);
    *out = parsed;
    return 1;
}

static int expr_number_to_long(number_t value, long *out)
{
    string_t *text;
    long parsed;

    if (!out || !num_is_real(value) || !num_is_integer(value))
        return 0;

    text = num_to_string(value);
    if (!text)
        return 0;
    if (!expr_text_to_long(text, &parsed)) {
        string_free(text);
        return 0;
    }
    string_free(text);
    *out = parsed;
    return 1;
}

static expr_t *expr_chain_rule_with_factor(const expr_t *dv, expr_t *factor)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *out = expr_mul(factor, da);
    expr_free(factor);
    expr_free(da);
    return out;
}

static expr_t *expr_const_long_local(long value)
{
    NUM_SCOPE(scope);
    number_t n = num_create_from_long(value);
    expr_t *dv = expr_new_const(n);

    return dv;
}

static expr_t *expr_pow_long_local(const expr_t *dv, long exponent)
{
    NUM_SCOPE(scope);
    number_t n = num_create_from_long(exponent);
    expr_t *out = expr_pow(dv, &n);

    return out;
}

static expr_t *expr_add_long_local(const expr_t *dv, long value)
{
    NUM_SCOPE(scope);
    number_t n = num_create_from_long(value);
    expr_t *out = expr_add_num(dv, &n);

    return out;
}

static expr_t *expr_const_num_local(number_t value)
{
    NUM_SCOPE(scope);
    expr_t *dv = expr_new_const(value);
    return dv;
}

static expr_t *expr_log10_scale_factor_local(void)
{
    NUM_SCOPE(scope);
    number_t value = num_log(NUM_TEN);
    expr_t *dv = expr_new_const(value);

    if (dv)
        dv->binding_expr =
            expr_binding_expr_new_unary_op(&ops_log,
                                         expr_binding_expr_new_number_text("10"));
    return dv;
}

static expr_t *expr_const_ratio_local(number_t numerator, number_t denominator)
{
    NUM_SCOPE(scope);
    number_t value = num_div(numerator, denominator);

    return expr_const_num_local(value);
}

static expr_t *expr_const_neg_ratio_local(number_t numerator, number_t denominator)
{
    NUM_SCOPE(scope);
    number_t value = num_div(numerator, denominator);
    number_t neg_value = num_neg(value);

    return expr_const_num_local(neg_value);
}

number_t eval_sin(expr_t *dv) { return expr_eval_unary_num(dv, num_sin); }
number_t eval_cos(expr_t *dv) { return expr_eval_unary_num(dv, num_cos); }
static bool binding_expr_is_number_long(const expr_binding_expr_t *expr,
                                        long expected)
{
    number_t value;
    number_t expected_value;
    bool match;

    if (!expr || expr->kind != EXPR_BINDING_EXPR_NUMBER)
        return false;

    value = expr_binding_expr_eval(expr);
    expected_value = num_create_from_long(expected);
    match = num_eq(value, expected_value);
    num_destroy(&expected_value);
    num_destroy(&value);
    return match;
}

static int binding_expr_pi_over_two_sign(const expr_binding_expr_t *expr)
{
    if (!expr)
        return 0;

    if (expr->kind == EXPR_BINDING_EXPR_NEG)
        return -binding_expr_pi_over_two_sign(expr->u.unary.child);

    if (expr->kind == EXPR_BINDING_EXPR_DIV &&
        expr->u.binary.left &&
        expr->u.binary.left->kind == EXPR_BINDING_EXPR_CONST &&
        expr->u.binary.left->u.const_id == EXPR_BINDING_CONST_PI &&
        binding_expr_is_number_long(expr->u.binary.right, 2))
        return 1;

    return 0;
}

number_t eval_tan(expr_t *dv)
{
    int pole_sign = dv && dv->a && dv->a->binding_expr
        ? binding_expr_pi_over_two_sign(dv->a->binding_expr) : 0;

    if (pole_sign > 0)
        return NUM_INF;
    if (pole_sign < 0)
        return NUM_NINF;
    return expr_eval_unary_num(dv, num_tan);
}

number_t eval_sec(expr_t *dv) { return expr_eval_unary_num(dv, num_sec); }
number_t eval_cosec(expr_t *dv) { return expr_eval_unary_num(dv, num_cosec); }
number_t eval_cot(expr_t *dv) { return expr_eval_unary_num(dv, num_cot); }

number_t eval_sinh(expr_t *dv) { return expr_eval_unary_num(dv, num_sinh); }
number_t eval_cosh(expr_t *dv) { return expr_eval_unary_num(dv, num_cosh); }
number_t eval_tanh(expr_t *dv) { return expr_eval_unary_num(dv, num_tanh); }
number_t eval_sech(expr_t *dv) { return expr_eval_unary_num(dv, num_sech); }
number_t eval_cosech(expr_t *dv) { return expr_eval_unary_num(dv, num_cosech); }
number_t eval_coth(expr_t *dv) { return expr_eval_unary_num(dv, num_coth); }

number_t eval_asin(expr_t *dv) { return expr_eval_unary_num(dv, num_asin); }
number_t eval_acos(expr_t *dv) { return expr_eval_unary_num(dv, num_acos); }
number_t eval_atan(expr_t *dv) { return expr_eval_unary_num(dv, num_atan); }
number_t eval_asec(expr_t *dv) { return expr_eval_unary_num(dv, num_asec); }
number_t eval_acosec(expr_t *dv) { return expr_eval_unary_num(dv, num_acosec); }
number_t eval_acot(expr_t *dv) { return expr_eval_unary_num(dv, num_acot); }

number_t eval_asinh(expr_t *dv) { return expr_eval_unary_num(dv, num_asinh); }
number_t eval_acosh(expr_t *dv) { return expr_eval_unary_num(dv, num_acosh); }
number_t eval_atanh(expr_t *dv) { return expr_eval_unary_num(dv, num_atanh); }
number_t eval_asech(expr_t *dv) { return expr_eval_unary_num(dv, num_asech); }
number_t eval_acosech(expr_t *dv) { return expr_eval_unary_num(dv, num_acosech); }
number_t eval_acoth(expr_t *dv) { return expr_eval_unary_num(dv, num_acoth); }

number_t eval_exp(expr_t *dv) { return expr_eval_unary_num(dv, num_exp); }
number_t eval_log(expr_t *dv) { return expr_eval_unary_num(dv, num_log); }
number_t eval_log10(expr_t *dv) { return expr_eval_unary_num(dv, num_log10); }
number_t eval_sqrt(expr_t *dv) { return expr_eval_unary_num(dv, num_sqrt); }
number_t eval_floor(expr_t *dv) { return expr_eval_unary_num(dv, num_floor); }
number_t eval_ceil(expr_t *dv) { return expr_eval_unary_num(dv, num_ceil); }
number_t eval_abs(expr_t *dv) { return expr_eval_unary_num(dv, num_abs); }
number_t eval_erf(expr_t *dv) { return expr_eval_unary_num(dv, num_erf); }
number_t eval_erfc(expr_t *dv) { return expr_eval_unary_num(dv, num_erfc); }
number_t eval_lgamma(expr_t *dv) { return expr_eval_unary_num(dv, num_lgamma); }
number_t eval_erfinv(expr_t *dv) { return expr_eval_unary_num(dv, num_erfinv); }
number_t eval_erfcinv(expr_t *dv) { return expr_eval_unary_num(dv, num_erfcinv); }
number_t eval_gamma(expr_t *dv) { return expr_eval_unary_num(dv, num_gamma); }
number_t eval_digamma(expr_t *dv) { return expr_eval_unary_num(dv, num_digamma); }
number_t eval_trigamma(expr_t *dv) { return expr_eval_unary_num(dv, num_trigamma); }
number_t eval_polygamma(expr_t *dv)
{
    number_t order_value = expr_eval_num_internal(dv->a);
    unsigned int order;

    if (!expr_number_to_polygamma_order(order_value, &order))
        return NUM_NAN;
    return num_polygamma(order, expr_eval_num_internal(dv->b));
}
number_t eval_gammainv(expr_t *dv) { return expr_eval_unary_num(dv, num_gammainv); }
number_t eval_lambert_w(expr_t *dv) { return expr_eval_unary_num(dv, num_productlog); }
number_t eval_lambert_w0(expr_t *dv) { return expr_eval_unary_num(dv, num_lambert_w0); }
number_t eval_lambert_wm1(expr_t *dv) { return expr_eval_unary_num(dv, num_lambert_wm1); }
number_t eval_normal_pdf(expr_t *dv) { return expr_eval_unary_num(dv, num_normal_pdf); }
number_t eval_normal_cdf(expr_t *dv) { return expr_eval_unary_num(dv, num_normal_cdf); }
number_t eval_normal_logpdf(expr_t *dv) { return expr_eval_unary_num(dv, num_normal_logpdf); }
number_t eval_ei(expr_t *dv) { return expr_eval_unary_num(dv, num_ei); }
number_t eval_e1(expr_t *dv) { return expr_eval_unary_num(dv, num_e1); }

number_t eval_hypot(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_hypot);
}

number_t eval_beta(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_beta);
}

number_t eval_logbeta(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_logbeta);
}

number_t eval_gammainc_lower(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_gammainc_lower);
}

number_t eval_gammainc_upper(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_gammainc_upper);
}

number_t eval_gammainc_P(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_gammainc_P);
}

number_t eval_gammainc_Q(expr_t *dv)
{
    return expr_eval_binary_num(dv, num_gammainc_Q);
}

number_t eval_factorial(expr_t *dv)
{
    number_t value = expr_eval_num_internal(dv->a);
    unsigned long n;

    return expr_number_to_unsigned_long(value, &n) ? num_factorial(n) : NUM_NAN;
}

number_t eval_fibonacci(expr_t *dv)
{
    number_t value = expr_eval_num_internal(dv->a);
    unsigned long n;

    return expr_number_to_unsigned_long(value, &n) ? num_fibonacci(n) : NUM_NAN;
}

number_t eval_partition(expr_t *dv) { return expr_eval_unary_num(dv, num_partition); }
number_t eval_isqrt(expr_t *dv) { return expr_eval_unary_num(dv, num_isqrt); }
number_t eval_gcd(expr_t *dv) { return expr_eval_binary_num(dv, num_gcd); }
number_t eval_lcm(expr_t *dv) { return expr_eval_binary_num(dv, num_lcm); }
number_t eval_mod(expr_t *dv) { return expr_eval_binary_num(dv, num_mod); }
number_t eval_modinv(expr_t *dv) { return expr_eval_binary_num(dv, num_modinv); }

number_t eval_is_prime(expr_t *dv)
{
    return num_create_from_long(num_is_prime(expr_eval_num_internal(dv->a)) ? 1L : 0L);
}

number_t eval_next_prime(expr_t *dv) { return expr_eval_unary_num(dv, num_next_prime); }
number_t eval_prev_prime(expr_t *dv) { return expr_eval_unary_num(dv, num_prev_prime); }

number_t eval_bit_and(expr_t *dv) { return expr_eval_binary_num(dv, num_bit_and); }
number_t eval_bit_or(expr_t *dv) { return expr_eval_binary_num(dv, num_bit_or); }
number_t eval_bit_xor(expr_t *dv) { return expr_eval_binary_num(dv, num_bit_xor); }
number_t eval_bit_not(expr_t *dv) { return expr_eval_unary_num(dv, num_bit_not); }

number_t eval_shl(expr_t *dv)
{
    number_t value = expr_eval_num_internal(dv->a);
    number_t bits_value = expr_eval_num_internal(dv->b);
    long bits;

    return expr_number_to_long(bits_value, &bits) ? num_shl(value, bits) : NUM_NAN;
}

number_t eval_shr(expr_t *dv)
{
    number_t value = expr_eval_num_internal(dv->a);
    number_t bits_value = expr_eval_num_internal(dv->b);
    long bits;

    return expr_number_to_long(bits_value, &bits) ? num_shr(value, bits) : NUM_NAN;
}

number_t eval_factors(expr_t *dv)
{
    return expr_eval_num_internal(dv->a);
}

number_t eval_atan2(expr_t *dv)
{
    return num_atan2(expr_eval_num_internal(dv->a), expr_eval_num_internal(dv->b));
}

expr_t *deriv_sin(expr_t *dv)
{
    return expr_chain_rule_with_factor(dv, expr_cos(dv->a));
}

expr_t *deriv_cos(expr_t *dv)
{
    expr_t *da      = expr_get_dx_internal(dv->a);
    expr_t *sin_a   = expr_sin(dv->a);
    expr_t *neg_sin = expr_neg(sin_a);
    expr_free(sin_a);
    expr_t *out     = expr_mul(neg_sin, da);
    expr_free(da);
    expr_free(neg_sin);
    return out;
}

expr_t *deriv_tan(expr_t *dv)
{
    expr_t *da  = expr_get_dx_internal(dv->a);
    expr_t *t   = expr_tan(dv->a);
    expr_t *t2  = expr_pow_long_local(t, 2);
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *fac = expr_add(one, t2);
    expr_t *out = expr_mul(fac, da);
    expr_free(da);
    expr_free(t);
    expr_free(t2);
    expr_free(one);
    expr_free(fac);
    return out;
}

expr_t *deriv_sec(expr_t *dv)
{
    expr_t *sec_a = expr_sec(dv->a);
    expr_t *tan_a = expr_tan(dv->a);
    expr_t *fac = expr_mul(sec_a, tan_a);

    expr_free(sec_a);
    expr_free(tan_a);
    return expr_chain_rule_with_factor(dv, fac);
}

expr_t *deriv_cosec(expr_t *dv)
{
    expr_t *cosec_a = expr_cosec(dv->a);
    expr_t *cot_a = expr_cot(dv->a);
    expr_t *product = expr_mul(cosec_a, cot_a);
    expr_t *fac = expr_neg(product);

    expr_free(cosec_a);
    expr_free(cot_a);
    expr_free(product);
    return expr_chain_rule_with_factor(dv, fac);
}

expr_t *deriv_cot(expr_t *dv)
{
    expr_t *cosec_a = expr_cosec(dv->a);
    expr_t *square = expr_pow_long_local(cosec_a, 2);
    expr_t *fac = expr_neg(square);

    expr_free(cosec_a);
    expr_free(square);
    return expr_chain_rule_with_factor(dv, fac);
}

expr_t *deriv_sinh(expr_t *dv)
{
    return expr_chain_rule_with_factor(dv, expr_cosh(dv->a));
}

expr_t *deriv_cosh(expr_t *dv)
{
    return expr_chain_rule_with_factor(dv, expr_sinh(dv->a));
}

expr_t *deriv_tanh(expr_t *dv)
{
    expr_t *da  = expr_get_dx_internal(dv->a);
    expr_t *t   = expr_tanh(dv->a);
    expr_t *t2  = expr_pow_long_local(t, 2);
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *fac = expr_sub(one, t2);
    expr_t *out = expr_mul(fac, da);
    expr_free(da);
    expr_free(t);
    expr_free(t2);
    expr_free(one);
    expr_free(fac);
    return out;
}

expr_t *deriv_sech(expr_t *dv)
{
    expr_t *sech_a = expr_sech(dv->a);
    expr_t *tanh_a = expr_tanh(dv->a);
    expr_t *product = expr_mul(sech_a, tanh_a);
    expr_t *fac = expr_neg(product);

    expr_free(sech_a);
    expr_free(tanh_a);
    expr_free(product);
    return expr_chain_rule_with_factor(dv, fac);
}

expr_t *deriv_cosech(expr_t *dv)
{
    expr_t *cosech_a = expr_cosech(dv->a);
    expr_t *coth_a = expr_coth(dv->a);
    expr_t *product = expr_mul(cosech_a, coth_a);
    expr_t *fac = expr_neg(product);

    expr_free(cosech_a);
    expr_free(coth_a);
    expr_free(product);
    return expr_chain_rule_with_factor(dv, fac);
}

expr_t *deriv_coth(expr_t *dv)
{
    expr_t *cosech_a = expr_cosech(dv->a);
    expr_t *square = expr_pow_long_local(cosech_a, 2);
    expr_t *fac = expr_neg(square);

    expr_free(cosech_a);
    expr_free(square);
    return expr_chain_rule_with_factor(dv, fac);
}

static expr_t *expr_deriv_inverse_reciprocal(const expr_t *dv,
                                           expr_t *(*inverse_fn)(const expr_t *))
{
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *reciprocal = expr_div(one, dv->a);
    expr_t *composed = inverse_fn(reciprocal);
    expr_t *out = expr_get_dx_internal(composed);

    expr_free(one);
    expr_free(reciprocal);
    expr_free(composed);
    return out;
}

expr_t *deriv_exp(expr_t *dv)
{
    return expr_chain_rule_with_factor(dv, expr_exp(dv->a));
}

expr_t *deriv_log(expr_t *dv)
{
    expr_t *da  = expr_get_dx_internal(dv->a);
    expr_t *out = expr_div(da, dv->a);
    expr_free(da);
    return out;
}

expr_t *deriv_log10(expr_t *dv)
{
    expr_t *da = expr_get_dx_internal(dv->a);
    expr_t *ln10 = expr_log10_scale_factor_local();
    expr_t *den = expr_mul(dv->a, ln10);
    expr_t *out = expr_div(da, den);

    expr_free(da);
    expr_free(ln10);
    expr_free(den);
    return out;
}

expr_t *deriv_sqrt(expr_t *dv)
{
    expr_t *da   = expr_get_dx_internal(dv->a);
    expr_t *two  = expr_const_long_local(2);
    expr_t *sqra = expr_sqrt(dv->a);
    expr_t *den  = expr_mul(two, sqra);
    expr_free(sqra);
    expr_t *out  = expr_div(da, den);
    expr_free(da);
    expr_free(two);
    expr_free(den);
    return out;
}

expr_t *deriv_floor(expr_t *dv)
{
    (void)dv;
    return expr_new_const(NUM_ZERO);
}

expr_t *deriv_ceil(expr_t *dv)
{
    (void)dv;
    return expr_new_const(NUM_ZERO);
}

expr_t *deriv_not_differentiable(expr_t *dv)
{
    (void)dv;
    return expr_new_const(NUM_NAN);
}

expr_t *deriv_asin(expr_t *dv)
{
    expr_t *da   = expr_get_dx_internal(dv->a);
    expr_t *a2   = expr_pow_long_local(dv->a, 2);
    expr_t *one  = expr_new_const(NUM_ONE);
    expr_t *sub  = expr_sub(one, a2);
    expr_t *den  = expr_sqrt(sub);
    expr_free(sub);
    expr_t *out  = expr_div(da, den);
    expr_free(da);
    expr_free(a2);
    expr_free(one);
    expr_free(den);
    return out;
}

expr_t *deriv_acos(expr_t *dv)
{
    expr_t *da   = expr_get_dx_internal(dv->a);
    expr_t *a2   = expr_pow_long_local(dv->a, 2);
    expr_t *one  = expr_new_const(NUM_ONE);
    expr_t *sub  = expr_sub(one, a2);
    expr_t *den  = expr_sqrt(sub);
    expr_free(sub);
    expr_t *num  = expr_neg(da);
    expr_t *out  = expr_div(num, den);
    expr_free(da);
    expr_free(a2);
    expr_free(one);
    expr_free(den);
    expr_free(num);
    return out;
}

expr_t *deriv_atan(expr_t *dv)
{
    expr_t *da  = expr_get_dx_internal(dv->a);
    expr_t *a2  = expr_pow_long_local(dv->a, 2);
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *den = expr_add(one, a2);
    expr_t *out = expr_div(da, den);
    expr_free(da);
    expr_free(a2);
    expr_free(one);
    expr_free(den);
    return out;
}

expr_t *deriv_asec(expr_t *dv)
{
    return expr_deriv_inverse_reciprocal(dv, expr_acos);
}

expr_t *deriv_acosec(expr_t *dv)
{
    return expr_deriv_inverse_reciprocal(dv, expr_asin);
}

expr_t *deriv_acot(expr_t *dv)
{
    return expr_deriv_inverse_reciprocal(dv, expr_atan);
}

expr_t *deriv_atan2(expr_t *dv)
{
    expr_t *y  = dv->a;
    expr_t *x  = dv->b;
    expr_t *dy = expr_get_dx_internal(y);
    expr_t *dx = expr_get_dx_internal(x);
    expr_t *x_dy = expr_mul(x, dy);
    expr_t *y_dx = expr_mul(y, dx);
    expr_t *num  = expr_sub(x_dy, y_dx);
    expr_t *x2  = expr_mul(x, x);
    expr_t *y2  = expr_mul(y, y);
    expr_t *den = expr_add(x2, y2);
    expr_t *out = expr_div(num, den);
    expr_free(dy);
    expr_free(dx);
    expr_free(x_dy);
    expr_free(y_dx);
    expr_free(num);
    expr_free(x2);
    expr_free(y2);
    expr_free(den);
    return out;
}

expr_t *deriv_asinh(expr_t *dv)
{
    expr_t *da   = expr_get_dx_internal(dv->a);
    expr_t *a2   = expr_pow_long_local(dv->a, 2);
    expr_t *one  = expr_new_const(NUM_ONE);
    expr_t *sum  = expr_add(one, a2);
    expr_t *den  = expr_sqrt(sum);
    expr_free(sum);
    expr_t *out  = expr_div(da, den);
    expr_free(da);
    expr_free(a2);
    expr_free(one);
    expr_free(den);
    return out;
}

expr_t *deriv_acosh(expr_t *dv)
{
    expr_t *da  = expr_get_dx_internal(dv->a);
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *am1 = expr_sub(dv->a, one);
    expr_t *ap1 = expr_add(dv->a, one);
    expr_t *s1  = expr_sqrt(am1);
    expr_t *s2  = expr_sqrt(ap1);
    expr_t *den = expr_mul(s1, s2);
    expr_t *out = expr_div(da, den);
    expr_free(da);
    expr_free(one);
    expr_free(am1);
    expr_free(ap1);
    expr_free(s1);
    expr_free(s2);
    expr_free(den);
    return out;
}

expr_t *deriv_atanh(expr_t *dv)
{
    expr_t *da  = expr_get_dx_internal(dv->a);
    expr_t *a2  = expr_pow_long_local(dv->a, 2);
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *den = expr_sub(one, a2);
    expr_t *out = expr_div(da, den);
    expr_free(da);
    expr_free(a2);
    expr_free(one);
    expr_free(den);
    return out;
}

expr_t *deriv_asech(expr_t *dv)
{
    return expr_deriv_inverse_reciprocal(dv, expr_acosh);
}

expr_t *deriv_acosech(expr_t *dv)
{
    return expr_deriv_inverse_reciprocal(dv, expr_asinh);
}

expr_t *deriv_acoth(expr_t *dv)
{
    return expr_deriv_inverse_reciprocal(dv, expr_atanh);
}

expr_t *deriv_abs(expr_t *dv)
{
    expr_t *da   = expr_get_dx_internal(dv->a);
    expr_t *absa = expr_abs(dv->a);
    expr_t *sign = expr_div(dv->a, absa);
    expr_t *out  = expr_mul(sign, da);
    expr_free(da);
    expr_free(absa);
    expr_free(sign);
    return out;
}

expr_t *deriv_erf(expr_t *dv)
{
    expr_t *da     = expr_get_dx_internal(dv->a);
    expr_t *c      = expr_const_num_local(NUM_2_SQRTPI);
    expr_t *a2     = expr_pow_long_local(dv->a, 2);
    expr_t *neg_a2 = expr_neg(a2);
    expr_t *ea2    = expr_exp(neg_a2);
    expr_t *fac    = expr_mul(c, ea2);
    expr_t *out    = expr_mul(fac, da);
    expr_free(da);
    expr_free(c);
    expr_free(a2);
    expr_free(neg_a2);
    expr_free(ea2);
    expr_free(fac);
    return out;
}

expr_t *deriv_erfc(expr_t *dv)
{
    expr_t *da     = expr_get_dx_internal(dv->a);
    expr_t *c      = expr_const_num_local(NUM_NEG_TWO_OVER_SQRT_PI);
    expr_t *a2     = expr_pow_long_local(dv->a, 2);
    expr_t *neg_a2 = expr_neg(a2);
    expr_t *ea2    = expr_exp(neg_a2);
    expr_t *fac    = expr_mul(c, ea2);
    expr_t *out    = expr_mul(fac, da);
    expr_free(da);
    expr_free(c);
    expr_free(a2);
    expr_free(neg_a2);
    expr_free(ea2);
    expr_free(fac);
    return out;
}

expr_t *deriv_lgamma(expr_t *dv)
{
    return expr_chain_rule_with_factor(dv, expr_digamma(dv->a));
}

expr_t *deriv_hypot(expr_t *dv)
{
    expr_t *a    = dv->a;
    expr_t *b    = dv->b;
    expr_t *da   = expr_get_dx_internal(a);
    expr_t *db   = expr_get_dx_internal(b);
    expr_t *a_da = expr_mul(a, da);
    expr_t *b_db = expr_mul(b, db);
    expr_t *num  = expr_add(a_da, b_db);
    expr_t *h    = expr_hypot(a, b);
    expr_t *out  = expr_div(num, h);
    expr_free(da);
    expr_free(db);
    expr_free(a_da);
    expr_free(b_db);
    expr_free(num);
    expr_free(h);
    return out;
}

expr_t *deriv_erfinv(expr_t *dv)
{
    expr_t *da  = expr_get_dx_internal(dv->a);
    expr_t *w   = expr_erfinv(dv->a);
    expr_t *w2  = expr_pow_long_local(w, 2);
    expr_t *ew2 = expr_exp(w2);
    expr_t *c   = expr_const_ratio_local(NUM_SQRT_PI, NUM_TWO);
    expr_t *fac = expr_mul(c, ew2);
    expr_t *out = expr_mul(fac, da);
    expr_free(da); expr_free(w); expr_free(w2); expr_free(ew2); expr_free(c); expr_free(fac);
    return out;
}

expr_t *deriv_erfcinv(expr_t *dv)
{
    expr_t *da  = expr_get_dx_internal(dv->a);
    expr_t *w   = expr_erfcinv(dv->a);
    expr_t *w2  = expr_pow_long_local(w, 2);
    expr_t *ew2 = expr_exp(w2);
    expr_t *c   = expr_const_neg_ratio_local(NUM_SQRT_PI, NUM_TWO);
    expr_t *fac = expr_mul(c, ew2);
    expr_t *out = expr_mul(fac, da);
    expr_free(da); expr_free(w); expr_free(w2); expr_free(ew2); expr_free(c); expr_free(fac);
    return out;
}

expr_t *deriv_gamma(expr_t *dv)
{
    expr_t *da  = expr_get_dx_internal(dv->a);
    expr_t *g   = expr_gamma(dv->a);
    expr_t *dg  = expr_digamma(dv->a);
    expr_t *gdg = expr_mul(g, dg);
    expr_t *out = expr_mul(gdg, da);
    expr_free(da); expr_free(g); expr_free(dg); expr_free(gdg);
    return out;
}

expr_t *deriv_digamma(expr_t *dv)
{
    return expr_chain_rule_with_factor(dv, expr_trigamma(dv->a));
}

expr_t *deriv_trigamma(expr_t *dv)
{
    expr_t *factor = expr_polygamma(2u, dv->a);

    return expr_chain_rule_with_factor(dv, factor);
}

expr_t *deriv_polygamma(expr_t *dv)
{
    number_t order_value = expr_eval_num_internal(dv->a);
    unsigned int order;
    expr_t *factor;
    expr_t *db;
    expr_t *out;

    if (!expr_number_to_polygamma_order(order_value, &order))
        return expr_new_const(NUM_NAN);

    factor = expr_polygamma(order + 1u, dv->b);
    db = expr_get_dx_internal(dv->b);
    out = expr_mul(factor, db);
    expr_free(factor);
    expr_free(db);
    return out;
}

expr_t *deriv_gammainv(expr_t *dv)
{
    expr_t *da   = expr_get_dx_internal(dv->a);
    expr_t *y    = expr_gammainv(dv->a);
    expr_t *psi  = expr_digamma(y);
    expr_t *xpsi = expr_mul(dv->a, psi);
    expr_t *one  = expr_new_const(NUM_ONE);
    expr_t *fac  = expr_div(one, xpsi);
    expr_t *out  = expr_mul(fac, da);
    expr_free(da); expr_free(y); expr_free(psi); expr_free(xpsi); expr_free(one); expr_free(fac);
    return out;
}

expr_t *deriv_lambert_w0(expr_t *dv)
{
    expr_t *da  = expr_get_dx_internal(dv->a);
    expr_t *w   = expr_lambert_w0(dv->a);
    expr_t *wp1 = expr_add_long_local(w, 1);
    expr_t *den = expr_mul(dv->a, wp1);
    expr_t *fac = expr_div(w, den);
    expr_t *out = expr_mul(fac, da);
    expr_free(da); expr_free(w); expr_free(wp1); expr_free(den); expr_free(fac);
    return out;
}

expr_t *deriv_lambert_w(expr_t *dv)
{
    expr_t *da  = expr_get_dx_internal(dv->a);
    expr_t *w   = expr_lambert_w(dv->a);
    expr_t *wp1 = expr_add_long_local(w, 1);
    expr_t *den = expr_mul(dv->a, wp1);
    expr_t *fac = expr_div(w, den);
    expr_t *out = expr_mul(fac, da);
    expr_free(da); expr_free(w); expr_free(wp1); expr_free(den); expr_free(fac);
    return out;
}

expr_t *deriv_lambert_wm1(expr_t *dv)
{
    expr_t *da  = expr_get_dx_internal(dv->a);
    expr_t *w   = expr_lambert_wm1(dv->a);
    expr_t *wp1 = expr_add_long_local(w, 1);
    expr_t *den = expr_mul(dv->a, wp1);
    expr_t *fac = expr_div(w, den);
    expr_t *out = expr_mul(fac, da);
    expr_free(da); expr_free(w); expr_free(wp1); expr_free(den); expr_free(fac);
    return out;
}

expr_t *deriv_normal_pdf(expr_t *dv)
{
    expr_t *da   = expr_get_dx_internal(dv->a);
    expr_t *neg_a = expr_neg(dv->a);
    expr_t *phi  = expr_normal_pdf(dv->a);
    expr_t *fac  = expr_mul(neg_a, phi);
    expr_t *out  = expr_mul(fac, da);
    expr_free(da); expr_free(neg_a); expr_free(phi); expr_free(fac);
    return out;
}

expr_t *deriv_normal_cdf(expr_t *dv)
{
    return expr_chain_rule_with_factor(dv, expr_normal_pdf(dv->a));
}

expr_t *deriv_normal_logpdf(expr_t *dv)
{
    return expr_chain_rule_with_factor(dv, expr_neg(dv->a));
}

expr_t *deriv_pdf(expr_t *dv)
{
    expr_t *da    = expr_get_dx_internal(dv->a);
    expr_t *neg_a = expr_neg(dv->a);
    expr_t *phi   = expr_pdf(dv->a);
    expr_t *fac   = expr_mul(neg_a, phi);
    expr_t *out   = expr_mul(fac, da);
    expr_free(da); expr_free(neg_a); expr_free(phi); expr_free(fac);
    return out;
}

expr_t *deriv_cdf(expr_t *dv)
{
    return expr_chain_rule_with_factor(dv, expr_pdf(dv->a));
}

expr_t *deriv_logpdf(expr_t *dv)
{
    return deriv_normal_logpdf(dv);
}

expr_t *deriv_ei(expr_t *dv)
{
    expr_t *da  = expr_get_dx_internal(dv->a);
    expr_t *ea  = expr_exp(dv->a);
    expr_t *fac = expr_div(ea, dv->a);
    expr_t *out = expr_mul(fac, da);
    expr_free(da); expr_free(ea); expr_free(fac);
    return out;
}

expr_t *deriv_e1(expr_t *dv)
{
    expr_t *da    = expr_get_dx_internal(dv->a);
    expr_t *neg_a = expr_neg(dv->a);
    expr_t *en_a  = expr_exp(neg_a);
    expr_t *neg_en = expr_neg(en_a);
    expr_t *fac   = expr_div(neg_en, dv->a);
    expr_t *out   = expr_mul(fac, da);
    expr_free(da); expr_free(neg_a); expr_free(en_a); expr_free(neg_en); expr_free(fac);
    return out;
}

expr_t *deriv_beta(expr_t *dv)
{
    expr_t *da    = expr_get_dx_internal(dv->a);
    expr_t *db    = expr_get_dx_internal(dv->b);
    expr_t *apb   = expr_add(dv->a, dv->b);
    expr_t *dg_a  = expr_digamma(dv->a);
    expr_t *dg_b  = expr_digamma(dv->b);
    expr_t *dg_ab = expr_digamma(apb);
    expr_t *diff_a = expr_sub(dg_a, dg_ab);
    expr_t *diff_b = expr_sub(dg_b, dg_ab);
    expr_t *beta_n = expr_beta(dv->a, dv->b);
    expr_t *ca    = expr_mul(beta_n, diff_a);
    expr_t *cb    = expr_mul(beta_n, diff_b);
    expr_t *ta    = expr_mul(ca, da);
    expr_t *tb    = expr_mul(cb, db);
    expr_t *out   = expr_add(ta, tb);
    expr_free(da); expr_free(db); expr_free(apb);
    expr_free(dg_a); expr_free(dg_b); expr_free(dg_ab);
    expr_free(diff_a); expr_free(diff_b); expr_free(beta_n);
    expr_free(ca); expr_free(cb); expr_free(ta); expr_free(tb);
    return out;
}

expr_t *deriv_logbeta(expr_t *dv)
{
    expr_t *da    = expr_get_dx_internal(dv->a);
    expr_t *db    = expr_get_dx_internal(dv->b);
    expr_t *apb   = expr_add(dv->a, dv->b);
    expr_t *dg_a  = expr_digamma(dv->a);
    expr_t *dg_b  = expr_digamma(dv->b);
    expr_t *dg_ab = expr_digamma(apb);
    expr_t *diff_a = expr_sub(dg_a, dg_ab);
    expr_t *diff_b = expr_sub(dg_b, dg_ab);
    expr_t *ta    = expr_mul(diff_a, da);
    expr_t *tb    = expr_mul(diff_b, db);
    expr_t *out   = expr_add(ta, tb);
    expr_free(da); expr_free(db); expr_free(apb);
    expr_free(dg_a); expr_free(dg_b); expr_free(dg_ab);
    expr_free(diff_a); expr_free(diff_b); expr_free(ta); expr_free(tb);
    return out;
}

static expr_t *gammainc_x_density(const expr_t *s, const expr_t *x)
{
    expr_t *one = expr_new_const(NUM_ONE);
    expr_t *s_minus_one = expr_sub(s, one);
    expr_t *x_pow = expr_pow_xp(x, s_minus_one);
    expr_t *neg_x = expr_neg(x);
    expr_t *exp_neg_x = expr_exp(neg_x);
    expr_t *density = expr_mul(x_pow, exp_neg_x);

    expr_free(one);
    expr_free(s_minus_one);
    expr_free(x_pow);
    expr_free(neg_x);
    expr_free(exp_neg_x);
    return density;
}

static expr_t *deriv_gammainc_x_only(expr_t *dv, int sign, int regularised)
{
    expr_t *ds = expr_get_dx_internal(dv->a);
    expr_t *dx = expr_get_dx_internal(dv->b);
    expr_t *density;
    expr_t *factor;
    expr_t *out;

    if (!expr_const_is_zero(ds)) {
        expr_free(ds);
        expr_free(dx);
        return expr_new_const(NUM_NAN);
    }

    density = gammainc_x_density(dv->a, dv->b);
    factor = density;

    if (regularised) {
        expr_t *gamma_s = expr_gamma(dv->a);
        factor = expr_div(density, gamma_s);
        expr_free(density);
        expr_free(gamma_s);
    }

    if (sign < 0) {
        expr_t *neg = expr_neg(factor);
        expr_free(factor);
        factor = neg;
    }

    out = expr_mul(factor, dx);
    expr_free(ds);
    expr_free(dx);
    expr_free(factor);
    return out;
}

expr_t *deriv_gammainc_lower(expr_t *dv)
{
    return deriv_gammainc_x_only(dv, 1, 0);
}

expr_t *deriv_gammainc_upper(expr_t *dv)
{
    return deriv_gammainc_x_only(dv, -1, 0);
}

expr_t *deriv_gammainc_P(expr_t *dv)
{
    return deriv_gammainc_x_only(dv, 1, 1);
}

expr_t *deriv_gammainc_Q(expr_t *dv)
{
    return deriv_gammainc_x_only(dv, -1, 1);
}
