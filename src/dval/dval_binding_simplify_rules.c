#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dval_binding_simplify.h"
#include "dval_tostring.h"
#include "internal/number_internal.h"

static bool binding_expr_positive_ulong_value(const dv_binding_expr_t *expr,
                                              unsigned long *out)
{
    number_t value;
    char *text;
    char *end = NULL;
    unsigned long parsed;
    bool ok;

    if (!out || !dv_binding_expr_number_value(expr, &value))
        return false;
    if (!num_is_real(value) || !num_is_integer(value) || !num_gt(value, NUM_ZERO)) {
        num_destroy(&value);
        return false;
    }

    text = num_to_string(value);
    num_destroy(&value);
    if (!text)
        return false;

    errno = 0;
    parsed = strtoul(text, &end, 10);
    ok = errno == 0 && end && *end == '\0' && parsed > 0ul;
    free(text);
    if (!ok)
        return false;

    *out = parsed;
    return true;
}

static unsigned long binding_gcd_ulong(unsigned long a, unsigned long b)
{
    while (b != 0ul) {
        unsigned long r = a % b;

        a = b;
        b = r;
    }
    return a;
}

static bool binding_checked_mul_ulong(unsigned long a,
                                      unsigned long b,
                                      unsigned long *out)
{
    if (b != 0ul && a > ULONG_MAX / b)
        return false;
    *out = a * b;
    return true;
}

static bool binding_logbeta_integer_denominator(unsigned long a,
                                                unsigned long b,
                                                unsigned long *den_out)
{
    unsigned long n;
    unsigned long k;
    unsigned long factor;
    unsigned long binom = 1ul;

    if (!den_out || a == 0ul || b == 0ul || a > ULONG_MAX - b + 1ul)
        return false;

    n = a + b - 1ul;
    if (a - 1ul <= b - 1ul) {
        k = a - 1ul;
        factor = b;
    } else {
        k = b - 1ul;
        factor = a;
    }

    for (unsigned long i = 1ul; i <= k; ++i) {
        unsigned long term = n - k + i;
        unsigned long divisor = i;
        unsigned long g;

        g = binding_gcd_ulong(term, divisor);
        term /= g;
        divisor /= g;

        g = binding_gcd_ulong(binom, divisor);
        binom /= g;
        divisor /= g;
        if (divisor != 1ul)
            return false;
        if (!binding_checked_mul_ulong(binom, term, &binom))
            return false;
    }

    return binding_checked_mul_ulong(binom, factor, den_out);
}

static dv_binding_expr_t *binding_expr_new_ulong(unsigned long value)
{
    char text[32];

    snprintf(text, sizeof(text), "%lu", value);
    return dv_binding_expr_new_number_text(text);
}

static dv_binding_expr_t *binding_expr_new_long(long value)
{
    char text[32];

    snprintf(text, sizeof(text), "%ld", value);
    return dv_binding_expr_new_number_text(text);
}

dv_binding_expr_t *binding_expr_try_simplify_logbeta_integers(dv_binding_expr_t *expr)
{
    unsigned long a;
    unsigned long b;
    unsigned long denominator;
    dv_binding_expr_t *out;

    if (!expr ||
        expr->kind != DV_BINDING_EXPR_BINARY_OP ||
        expr->u.binary_op.ops != &ops_logbeta ||
        !binding_expr_positive_ulong_value(expr->u.binary_op.left, &a) ||
        !binding_expr_positive_ulong_value(expr->u.binary_op.right, &b) ||
        !binding_logbeta_integer_denominator(a, b, &denominator))
        return expr;

    if (denominator == 1ul) {
        out = dv_binding_expr_new_number_text("0");
    } else {
        out = dv_binding_expr_new_neg(
            dv_binding_expr_new_unary_op(&ops_log,
                                         binding_expr_new_ulong(denominator)));
    }

    dv_binding_expr_free(expr);
    return out;
}

bool binding_number_text_eq_long(const dv_binding_expr_t *expr,
                                 long expected_long)
{
    number_t value;
    number_t expected;
    bool equal;

    if (!expr || expr->kind != DV_BINDING_EXPR_NUMBER)
        return false;

    value = binding_number_from_text(expr->u.text);
    expected = num_create_from_long(expected_long);
    equal = num_eq(value, expected);
    num_destroy(&expected);
    num_destroy(&value);
    return equal;
}

bool binding_number_text_to_long(const dv_binding_expr_t *expr, long *out)
{
    number_t value;
    number_t floor_value;
    bool ok = false;

    if (!expr || expr->kind != DV_BINDING_EXPR_NUMBER)
        return false;

    value = binding_number_from_text(expr->u.text);
    floor_value = num_floor(value);
    if (num_eq(value, floor_value)) {
        char *text = num_to_string(value);
        char *end = NULL;
        long parsed = 0;

        errno = 0;
        if (text)
            parsed = strtol(text, &end, 10);
        if (text && errno == 0 && end && *end == '\0') {
            *out = parsed;
            ok = true;
        }
        free(text);
    }
    num_destroy(&floor_value);
    num_destroy(&value);
    return ok;
}

bool binding_number_text_to_small_rational(const dv_binding_expr_t *expr,
                                           long *numerator,
                                           long *denominator)
{
    number_t value;
    bool ok;

    if (!expr || expr->kind != DV_BINDING_EXPR_NUMBER)
        return false;

    value = binding_number_from_text(expr->u.text);
    ok = num_get_small_rational(value, numerator, denominator);
    num_destroy(&value);
    return ok;
}

static bool binding_long_power_of_ten_exponent(long value, long *exponent_out)
{
    long exponent = 0;

    if (value <= 0L || !exponent_out)
        return false;

    while (value % 10L == 0L) {
        value /= 10L;
        ++exponent;
    }

    if (value != 1L)
        return false;

    *exponent_out = exponent;
    return true;
}

bool binding_number_text_log10_power_exponent(const dv_binding_expr_t *expr,
                                              long *exponent_out)
{
    long numerator;
    long denominator;
    long exponent;

    if (!binding_number_text_to_small_rational(expr, &numerator, &denominator) ||
        numerator <= 0L || denominator <= 0L || !exponent_out)
        return false;

    if (denominator == 1L &&
        binding_long_power_of_ten_exponent(numerator, &exponent)) {
        *exponent_out = exponent;
        return true;
    }

    if (numerator == 1L &&
        binding_long_power_of_ten_exponent(denominator, &exponent)) {
        *exponent_out = -exponent;
        return true;
    }

    return false;
}
static bool binding_expr_as_integer_power(const dv_binding_expr_t *expr,
                                          const dv_binding_expr_t **base_out,
                                          long *exponent_out)
{
    if (!expr || !base_out || !exponent_out)
        return false;

    if (expr->kind == DV_BINDING_EXPR_POWI) {
        *base_out = expr->u.powi.base;
        *exponent_out = expr->u.powi.exponent;
        return true;
    }

    if (expr->kind == DV_BINDING_EXPR_BINARY_OP &&
        expr->u.binary_op.ops == &ops_pow &&
        binding_number_text_to_long(expr->u.binary_op.right, exponent_out)) {
        *base_out = expr->u.binary_op.left;
        return true;
    }

    *base_out = expr;
    *exponent_out = 1L;
    return true;
}

dv_binding_expr_t *binding_expr_try_combine_mul_powers(dv_binding_expr_t *expr)
{
    const dv_binding_expr_t *left_base;
    const dv_binding_expr_t *right_base;
    long left_exponent;
    long right_exponent;
    long sum_exponent;
    dv_binding_expr_t *base;

    if (!expr || expr->kind != DV_BINDING_EXPR_MUL)
        return expr;

    if (!binding_expr_as_integer_power(expr->u.binary.left,
                                       &left_base,
                                       &left_exponent) ||
        !binding_expr_as_integer_power(expr->u.binary.right,
                                       &right_base,
                                       &right_exponent) ||
        !dv_binding_expr_struct_eq(left_base, right_base))
        return expr;

    if ((right_exponent > 0 && left_exponent > LONG_MAX - right_exponent) ||
        (right_exponent < 0 && left_exponent < LONG_MIN - right_exponent))
        return expr;

    sum_exponent = left_exponent + right_exponent;
    base = dv_binding_expr_clone(left_base);
    dv_binding_expr_free(expr);

    if (sum_exponent == 0L) {
        dv_binding_expr_free(base);
        return dv_binding_expr_new_number_text("1");
    }
    if (sum_exponent == 1L)
        return base;
    return dv_binding_expr_simplify(dv_binding_expr_new_powi(base, sum_exponent));
}
static dv_binding_expr_t *binding_expr_number_from_value(number_t value)
{
    char *text;
    dv_binding_expr_t *expr;

    if (num_is_inf(value))
        return dv_binding_expr_new_number_text(num_get_sign(value) < 0 ? "-∞" : "∞");

    text = num_to_string(value);
    expr = dv_binding_expr_new_number_text(text ? text : "NAN");

    free(text);
    return expr;
}

static dv_binding_expr_t *binding_expr_product_owned(dv_binding_expr_t *left,
                                                     dv_binding_expr_t *right)
{
    if (!left)
        return right;
    if (!right)
        return left;
    return dv_binding_expr_new_mul(left, right);
}

static dv_binding_expr_t *binding_expr_scaled_product_owned(number_t coeff,
                                                            dv_binding_expr_t *left_rest,
                                                            dv_binding_expr_t *right_rest)
{
    dv_binding_expr_t *rest;

    if (num_is_zero(coeff)) {
        num_destroy(&coeff);
        dv_binding_expr_free(left_rest);
        dv_binding_expr_free(right_rest);
        return binding_expr_number_from_value(NUM_ZERO);
    }

    rest = binding_expr_product_owned(left_rest, right_rest);
    if (!rest) {
        dv_binding_expr_t *out = binding_expr_number_from_value(coeff);

        num_destroy(&coeff);
        return out;
    }

    if (num_eq(coeff, NUM_ONE)) {
        num_destroy(&coeff);
        return rest;
    }
    if (num_eq(coeff, NUM_NEG_ONE)) {
        num_destroy(&coeff);
        return dv_binding_expr_new_neg(rest);
    }

    {
        dv_binding_expr_t *coeff_expr = binding_expr_number_from_value(coeff);

        num_destroy(&coeff);
        return dv_binding_expr_new_mul(coeff_expr, rest);
    }
}

dv_binding_expr_t *binding_expr_fold_to_number_owned(dv_binding_expr_t *expr,
                                                     number_t value)
{
    value = num_scope_detach(value);

    dv_binding_expr_t *folded = binding_expr_number_from_value(value);

    num_destroy(&value);
    dv_binding_expr_free(expr);
    return folded;
}

static dv_binding_expr_t *binding_expr_fold_to_expr_owned(dv_binding_expr_t *expr,
                                                          dv_binding_expr_t *folded)
{
    dv_binding_expr_free(expr);
    return folded;
}

static dv_binding_expr_t *binding_expr_add_one(const dv_binding_expr_t *arg)
{
    return dv_binding_expr_simplify(
        dv_binding_expr_new_add(dv_binding_expr_clone(arg),
                                binding_expr_new_long(1L)));
}

static dv_binding_expr_t *binding_expr_double_arg_unary(
    const dv_binding_expr_t *arg,
    const dval_ops_t *ops)
{
    return dv_binding_expr_new_unary_op(
        ops,
        dv_binding_expr_simplify(
            dv_binding_expr_new_mul(binding_expr_new_long(2L),
                                    dv_binding_expr_clone(arg))));
}

static bool binding_expr_is_unary_op(const dv_binding_expr_t *expr,
                                     const dval_ops_t *ops)
{
    return expr &&
           expr->kind == DV_BINDING_EXPR_UNARY_OP &&
           expr->u.unary_op.ops == ops;
}

static const dv_binding_expr_t *binding_expr_matching_unary_args(
    const dv_binding_expr_t *a,
    const dv_binding_expr_t *b,
    const dval_ops_t *left_ops,
    const dval_ops_t *right_ops)
{
    if (binding_expr_is_unary_op(a, left_ops) &&
        binding_expr_is_unary_op(b, right_ops) &&
        dv_binding_expr_struct_eq(a->u.unary_op.child,
                                  b->u.unary_op.child))
        return a->u.unary_op.child;
    if (binding_expr_is_unary_op(a, right_ops) &&
        binding_expr_is_unary_op(b, left_ops) &&
        dv_binding_expr_struct_eq(a->u.unary_op.child,
                                  b->u.unary_op.child))
        return a->u.unary_op.child;
    return NULL;
}

static bool binding_expr_is_square_of_unary(const dv_binding_expr_t *expr,
                                            const dval_ops_t *ops,
                                            const dv_binding_expr_t **arg_out)
{
    if (!expr || !arg_out ||
        expr->kind != DV_BINDING_EXPR_POWI ||
        expr->u.powi.exponent != 2 ||
        !binding_expr_is_unary_op(expr->u.powi.base, ops))
        return false;

    *arg_out = expr->u.powi.base->u.unary_op.child;
    return true;
}

static bool binding_expr_is_neg_square_of_unary(
    const dv_binding_expr_t *expr,
    const dval_ops_t *ops,
    const dv_binding_expr_t **arg_out)
{
    return expr &&
           expr->kind == DV_BINDING_EXPR_NEG &&
           binding_expr_is_square_of_unary(expr->u.unary.child, ops, arg_out);
}

static bool binding_expr_i_unit_sign(const dv_binding_expr_t *expr,
                                     int *sign_out)
{
    int child_sign;

    if (!expr || !sign_out)
        return false;

    if (binding_expr_is_const_id(expr, DV_BINDING_CONST_I)) {
        *sign_out = 1;
        return true;
    }

    if (expr->kind == DV_BINDING_EXPR_NEG &&
        binding_expr_i_unit_sign(expr->u.unary.child, &child_sign)) {
        *sign_out = -child_sign;
        return true;
    }

    return false;
}

static bool binding_expr_extract_i_unit_factor_owned(const dv_binding_expr_t *expr,
                                                     int *sign_out,
                                                     dv_binding_expr_t **rest_out)
{
    dv_binding_expr_t *child_rest = NULL;
    int child_sign;

    if (!expr || !sign_out || !rest_out)
        return false;

    *rest_out = NULL;

    if (binding_expr_i_unit_sign(expr, sign_out)) {
        return true;
    }

    if (expr->kind == DV_BINDING_EXPR_NEG &&
        binding_expr_extract_i_unit_factor_owned(expr->u.unary.child,
                                                 &child_sign,
                                                 &child_rest)) {
        *sign_out = -child_sign;
        *rest_out = child_rest;
        return true;
    }

    if (expr->kind == DV_BINDING_EXPR_MUL) {
        if (binding_expr_extract_i_unit_factor_owned(expr->u.binary.left,
                                                     sign_out,
                                                     &child_rest)) {
            *rest_out = binding_expr_product_owned(
                child_rest,
                dv_binding_expr_clone(expr->u.binary.right));
            return true;
        }
        if (binding_expr_extract_i_unit_factor_owned(expr->u.binary.right,
                                                     sign_out,
                                                     &child_rest)) {
            *rest_out = binding_expr_product_owned(
                dv_binding_expr_clone(expr->u.binary.left),
                child_rest);
            return true;
        }
    }

    if (expr->kind == DV_BINDING_EXPR_DIV &&
        binding_expr_extract_i_unit_factor_owned(expr->u.binary.left,
                                                 sign_out,
                                                 &child_rest)) {
        *rest_out = dv_binding_expr_new_div(
            child_rest ? child_rest : binding_expr_new_long(1L),
            dv_binding_expr_clone(expr->u.binary.right));
            return true;
    }

    return false;
}

dv_binding_expr_t *binding_expr_try_simplify_i_unit_product(
    dv_binding_expr_t *expr)
{
    dv_binding_expr_t *left_rest = NULL;
    dv_binding_expr_t *right_rest = NULL;
    dv_binding_expr_t *out;
    int left_sign;
    int right_sign;
    int coeff_sign;

    if (!expr || expr->kind != DV_BINDING_EXPR_MUL ||
        !binding_expr_extract_i_unit_factor_owned(expr->u.binary.left,
                                                  &left_sign,
                                                  &left_rest))
        return expr;
    if (!binding_expr_extract_i_unit_factor_owned(expr->u.binary.right,
                                                 &right_sign,
                                                 &right_rest)) {
        dv_binding_expr_free(left_rest);
        return expr;
    }

    coeff_sign = -(left_sign * right_sign);
    out = binding_expr_product_owned(left_rest, right_rest);
    if (!out)
        out = binding_expr_new_long(1L);
    if (coeff_sign < 0)
        out = dv_binding_expr_new_neg(out);

    return binding_expr_fold_to_expr_owned(expr, dv_binding_expr_simplify(out));
}

static dv_binding_expr_t *binding_expr_imaginary_scaled_owned(
    int sign,
    dv_binding_expr_t *expr)
{
    dv_binding_expr_t *out = dv_binding_expr_new_mul(
        dv_binding_expr_new_const(DV_BINDING_CONST_I),
        expr);

    return sign < 0 ? dv_binding_expr_new_neg(out) : out;
}

dv_binding_expr_t *binding_expr_try_simplify_imag_trig_bridge(
    dv_binding_expr_t *expr)
{
    dv_binding_expr_t *arg = NULL;
    const dval_ops_t *target_ops = NULL;
    dv_binding_expr_t *out;
    int sign;
    bool multiply_i = false;

    if (!expr ||
        expr->kind != DV_BINDING_EXPR_UNARY_OP ||
        !binding_expr_extract_i_unit_factor_owned(expr->u.unary_op.child,
                                                  &sign,
                                                  &arg))
        return expr;

    if (expr->u.unary_op.ops == &ops_cosh) {
        target_ops = &ops_cos;
    } else if (expr->u.unary_op.ops == &ops_cos) {
        target_ops = &ops_cosh;
    } else if (expr->u.unary_op.ops == &ops_sinh) {
        target_ops = &ops_sin;
        multiply_i = true;
    } else if (expr->u.unary_op.ops == &ops_sin) {
        target_ops = &ops_sinh;
        multiply_i = true;
    } else {
        dv_binding_expr_free(arg);
        return expr;
    }

    out = dv_binding_expr_new_unary_op(target_ops,
                                       arg ? arg : binding_expr_new_long(1L));
    if (multiply_i)
        out = binding_expr_imaginary_scaled_owned(sign, out);

    return binding_expr_fold_to_expr_owned(expr, dv_binding_expr_simplify(out));
}

dv_binding_expr_t *binding_expr_try_simplify_basic_sum(dv_binding_expr_t *expr)
{
    const dv_binding_expr_t *log = NULL;
    const dv_binding_expr_t *lgamma = NULL;
    dv_binding_expr_t *successor_arg;
    dv_binding_expr_t *out;

    if (!expr || expr->kind != DV_BINDING_EXPR_ADD)
        return expr;

    if (binding_expr_is_unary_op(expr->u.binary.left, &ops_log) &&
        binding_expr_is_unary_op(expr->u.binary.right, &ops_lgamma)) {
        log = expr->u.binary.left;
        lgamma = expr->u.binary.right;
    } else if (binding_expr_is_unary_op(expr->u.binary.left, &ops_lgamma) &&
               binding_expr_is_unary_op(expr->u.binary.right, &ops_log)) {
        lgamma = expr->u.binary.left;
        log = expr->u.binary.right;
    }

    if (!log || !lgamma ||
        !dv_binding_expr_struct_eq(log->u.unary_op.child,
                                   lgamma->u.unary_op.child))
        return expr;

    successor_arg = binding_expr_add_one(lgamma->u.unary_op.child);
    out = dv_binding_expr_new_unary_op(&ops_lgamma, successor_arg);
    return binding_expr_fold_to_expr_owned(expr, out);
}

dv_binding_expr_t *binding_expr_try_simplify_basic_product(dv_binding_expr_t *expr)
{
    const dv_binding_expr_t *gamma = NULL;
    const dv_binding_expr_t *factor = NULL;
    dv_binding_expr_t *successor_arg;
    dv_binding_expr_t *out;

    if (!expr || expr->kind != DV_BINDING_EXPR_MUL)
        return expr;

    if (binding_expr_is_unary_op(expr->u.binary.left, &ops_gamma)) {
        gamma = expr->u.binary.left;
        factor = expr->u.binary.right;
    } else if (binding_expr_is_unary_op(expr->u.binary.right, &ops_gamma)) {
        gamma = expr->u.binary.right;
        factor = expr->u.binary.left;
    }

    if (!gamma || !factor ||
        !dv_binding_expr_struct_eq(factor, gamma->u.unary_op.child))
        return expr;

    successor_arg = binding_expr_add_one(gamma->u.unary_op.child);
    out = dv_binding_expr_new_unary_op(&ops_gamma, successor_arg);
    return binding_expr_fold_to_expr_owned(expr, out);
}

static bool binding_expr_scaled_gamma_product_parts(
    const dv_binding_expr_t *expr,
    const dv_binding_expr_t **scaled_arg_out,
    const dv_binding_expr_t **gamma_out)
{
    if (!expr || expr->kind != DV_BINDING_EXPR_MUL ||
        !scaled_arg_out || !gamma_out)
        return false;

    if (binding_expr_is_unary_op(expr->u.binary.left, &ops_gamma)) {
        *gamma_out = expr->u.binary.left;
        *scaled_arg_out = expr->u.binary.right;
        return true;
    }

    if (binding_expr_is_unary_op(expr->u.binary.right, &ops_gamma)) {
        *gamma_out = expr->u.binary.right;
        *scaled_arg_out = expr->u.binary.left;
        return true;
    }

    return false;
}

dv_binding_expr_t *binding_expr_try_simplify_basic_quotient(dv_binding_expr_t *expr)
{
    const dv_binding_expr_t *scaled_arg;
    const dv_binding_expr_t *gamma;
    dv_binding_expr_t *arg;
    dv_binding_expr_t *successor_arg;
    dv_binding_expr_t *out;

    if (!expr || expr->kind != DV_BINDING_EXPR_DIV ||
        !binding_expr_scaled_gamma_product_parts(expr->u.binary.left,
                                                 &scaled_arg,
                                                 &gamma))
        return expr;

    arg = dv_binding_expr_simplify(
        dv_binding_expr_new_div(dv_binding_expr_clone(scaled_arg),
                                dv_binding_expr_clone(expr->u.binary.right)));
    if (!dv_binding_expr_struct_eq(arg, gamma->u.unary_op.child)) {
        dv_binding_expr_free(arg);
        return expr;
    }

    successor_arg = binding_expr_add_one(arg);
    out = dv_binding_expr_new_unary_op(&ops_gamma, successor_arg);
    dv_binding_expr_free(arg);
    return binding_expr_fold_to_expr_owned(expr, out);
}

dv_binding_expr_t *binding_expr_try_simplify_trig_product(dv_binding_expr_t *expr)
{
    const dv_binding_expr_t *arg;
    dv_binding_expr_t *out;

    if (!expr || expr->kind != DV_BINDING_EXPR_MUL)
        return expr;

    arg = binding_expr_matching_unary_args(expr->u.binary.left,
                                           expr->u.binary.right,
                                           &ops_sin,
                                           &ops_cos);
    if (arg) {
        out = dv_binding_expr_new_mul(
            binding_expr_number_from_value(NUM_HALF),
            binding_expr_double_arg_unary(arg, &ops_sin));
        return binding_expr_fold_to_expr_owned(expr,
                                               dv_binding_expr_simplify(out));
    }

    arg = binding_expr_matching_unary_args(expr->u.binary.left,
                                           expr->u.binary.right,
                                           &ops_sinh,
                                           &ops_cosh);
    if (arg) {
        out = dv_binding_expr_new_mul(
            binding_expr_number_from_value(NUM_HALF),
            binding_expr_double_arg_unary(arg, &ops_sinh));
        return binding_expr_fold_to_expr_owned(expr,
                                               dv_binding_expr_simplify(out));
    }

    arg = binding_expr_matching_unary_args(expr->u.binary.left,
                                           expr->u.binary.right,
                                           &ops_cos,
                                           &ops_tan);
    if (arg) {
        out = dv_binding_expr_new_unary_op(&ops_sin, dv_binding_expr_clone(arg));
        return binding_expr_fold_to_expr_owned(expr, out);
    }

    arg = binding_expr_matching_unary_args(expr->u.binary.left,
                                           expr->u.binary.right,
                                           &ops_cosh,
                                           &ops_tanh);
    if (arg) {
        out = dv_binding_expr_new_unary_op(&ops_sinh, dv_binding_expr_clone(arg));
        return binding_expr_fold_to_expr_owned(expr, out);
    }

    return expr;
}

dv_binding_expr_t *binding_expr_try_simplify_trig_sum(dv_binding_expr_t *expr)
{
    const dv_binding_expr_t *left_arg = NULL;
    const dv_binding_expr_t *right_arg = NULL;
    bool subtract;

    if (!expr || (expr->kind != DV_BINDING_EXPR_ADD &&
                  expr->kind != DV_BINDING_EXPR_SUB))
        return expr;

    subtract = expr->kind == DV_BINDING_EXPR_SUB;

    if (!subtract &&
        binding_expr_is_square_of_unary(expr->u.binary.left, &ops_sin,
                                        &left_arg) &&
        binding_expr_is_square_of_unary(expr->u.binary.right, &ops_cos,
                                        &right_arg) &&
        dv_binding_expr_struct_eq(left_arg, right_arg))
        return binding_expr_fold_to_number_owned(expr, num_clone(NUM_ONE));

    if (!subtract &&
        binding_expr_is_square_of_unary(expr->u.binary.left, &ops_cos,
                                        &left_arg) &&
        binding_expr_is_square_of_unary(expr->u.binary.right, &ops_sin,
                                        &right_arg) &&
        dv_binding_expr_struct_eq(left_arg, right_arg))
        return binding_expr_fold_to_number_owned(expr, num_clone(NUM_ONE));

    if (((subtract &&
          binding_expr_is_square_of_unary(expr->u.binary.left, &ops_cos,
                                          &left_arg) &&
          binding_expr_is_square_of_unary(expr->u.binary.right, &ops_sin,
                                          &right_arg)) ||
         (!subtract &&
          binding_expr_is_neg_square_of_unary(expr->u.binary.left, &ops_sin,
                                              &left_arg) &&
          binding_expr_is_square_of_unary(expr->u.binary.right, &ops_cos,
                                          &right_arg))) &&
        dv_binding_expr_struct_eq(left_arg, right_arg)) {
        dv_binding_expr_t *out = binding_expr_double_arg_unary(left_arg,
                                                               &ops_cos);

        return binding_expr_fold_to_expr_owned(expr, out);
    }

    if (subtract &&
        binding_expr_is_square_of_unary(expr->u.binary.left, &ops_cosh,
                                        &left_arg) &&
        binding_expr_is_square_of_unary(expr->u.binary.right, &ops_sinh,
                                        &right_arg) &&
        dv_binding_expr_struct_eq(left_arg, right_arg))
        return binding_expr_fold_to_number_owned(expr, num_clone(NUM_ONE));

    if (!subtract &&
        binding_expr_is_neg_square_of_unary(expr->u.binary.left, &ops_sinh,
                                            &left_arg) &&
        binding_expr_is_square_of_unary(expr->u.binary.right, &ops_cosh,
                                        &right_arg) &&
        dv_binding_expr_struct_eq(left_arg, right_arg))
        return binding_expr_fold_to_number_owned(expr, num_clone(NUM_ONE));

    if (!subtract &&
        binding_expr_is_square_of_unary(expr->u.binary.left, &ops_sinh,
                                        &left_arg) &&
        binding_expr_is_square_of_unary(expr->u.binary.right, &ops_cosh,
                                        &right_arg) &&
        dv_binding_expr_struct_eq(left_arg, right_arg)) {
        dv_binding_expr_t *out = binding_expr_double_arg_unary(left_arg,
                                                               &ops_cosh);

        return binding_expr_fold_to_expr_owned(expr, out);
    }

    if (!subtract &&
        binding_expr_is_square_of_unary(expr->u.binary.left, &ops_cosh,
                                        &left_arg) &&
        binding_expr_is_square_of_unary(expr->u.binary.right, &ops_sinh,
                                        &right_arg) &&
        dv_binding_expr_struct_eq(left_arg, right_arg)) {
        dv_binding_expr_t *out = binding_expr_double_arg_unary(left_arg,
                                                               &ops_cosh);

        return binding_expr_fold_to_expr_owned(expr, out);
    }

    return expr;
}

static dv_binding_expr_t *binding_expr_from_exact_real(number_t value)
{
    char *text = num_to_string(value);
    dv_binding_expr_t *expr =
        dv_binding_expr_new_number_text(text ? text : "NAN");

    free(text);
    return expr;
}

static dv_binding_expr_t *binding_expr_from_exact_imag(number_t imag)
{
    number_t abs_imag;
    dv_binding_expr_t *unit;

    if (num_eq(imag, NUM_ONE))
        return dv_binding_expr_new_const(DV_BINDING_CONST_I);
    if (num_eq(imag, NUM_NEG_ONE))
        return dv_binding_expr_new_neg(dv_binding_expr_new_const(DV_BINDING_CONST_I));

    abs_imag = num_get_sign(imag) < 0 ? num_abs(imag) : num_clone(imag);
    unit = dv_binding_expr_new_mul(binding_expr_from_exact_real(abs_imag),
                                   dv_binding_expr_new_const(DV_BINDING_CONST_I));
    num_destroy(&abs_imag);
    return num_get_sign(imag) < 0 ? dv_binding_expr_new_neg(unit) : unit;
}

static dv_binding_expr_t *binding_expr_from_exact_complex(
    const binding_exact_complex_t *value)
{
    dv_binding_expr_t *real_expr;
    dv_binding_expr_t *imag_expr;
    number_t abs_imag;

    if (num_is_zero(value->imag))
        return binding_expr_from_exact_real(value->real);
    if (num_is_zero(value->real))
        return binding_expr_from_exact_imag(value->imag);

    real_expr = binding_expr_from_exact_real(value->real);
    if (num_get_sign(value->imag) < 0) {
        abs_imag = num_abs(value->imag);
        imag_expr = binding_expr_from_exact_imag(abs_imag);
        num_destroy(&abs_imag);
        return dv_binding_expr_new_sub(real_expr, imag_expr);
    }

    imag_expr = binding_expr_from_exact_imag(value->imag);
    return dv_binding_expr_new_add(real_expr, imag_expr);
}

dv_binding_expr_t *binding_expr_try_fold_exact_complex_owned(
    dv_binding_expr_t *expr)
{
    binding_exact_complex_t value;
    dv_binding_expr_t *folded;

    if (!dv_binding_expr_exact_complex(expr, &value))
        return expr;

    folded = binding_expr_from_exact_complex(&value);
    dv_binding_exact_complex_clear(&value);
    return binding_expr_fold_to_expr_owned(expr, folded);
}

dv_binding_expr_t *binding_expr_try_fold_number_owned(dv_binding_expr_t *expr)
{
    number_t value;

    if (dv_binding_expr_number_value(expr, &value))
        return binding_expr_fold_to_number_owned(expr, value);
    return expr;
}

static bool binding_expr_pi_ratio_twelfths(const dv_binding_expr_t *expr,
                                           long *twelfths_out)
{
    long numer;
    long denom;
    long twelfths;
    dv_binding_const_id_t const_id;

    if (!expr || !twelfths_out)
        return false;

    if (binding_expr_scaled_const_ratio(expr, &numer, &denom, &const_id)) {
        if (const_id != DV_BINDING_CONST_PI)
            return false;
    } else if (expr->kind == DV_BINDING_EXPR_DIV &&
               binding_const_ratio_parts(expr->u.binary.left,
                                         expr->u.binary.right,
                                         &numer,
                                         &denom,
                                         &const_id) &&
               const_id == DV_BINDING_CONST_PI) {
        /* numer/denom is already reduced by binding_const_ratio_parts. */
    } else {
        return false;
    }

    if (denom == 0L || 12L % denom != 0L)
        return false;

    twelfths = numer * (12L / denom);
    twelfths %= 24L;
    if (twelfths < 0L)
        twelfths += 24L;
    *twelfths_out = twelfths;
    return true;
}

static bool binding_expr_pi_ratio_parts(const dv_binding_expr_t *expr,
                                        long *numer_out,
                                        long *denom_out)
{
    long numer;
    long denom;
    long gcd;
    dv_binding_const_id_t const_id;

    if (!expr || !numer_out || !denom_out)
        return false;

    if (binding_expr_scaled_const_ratio(expr, &numer, &denom, &const_id)) {
        if (const_id != DV_BINDING_CONST_PI)
            return false;
    } else if (expr->kind == DV_BINDING_EXPR_DIV &&
               binding_const_ratio_parts(expr->u.binary.left,
                                         expr->u.binary.right,
                                         &numer,
                                         &denom,
                                         &const_id) &&
               const_id == DV_BINDING_CONST_PI) {
        /* numer/denom is already reduced by binding_const_ratio_parts. */
    } else {
        return false;
    }

    if (denom == 0L)
        return false;
    if (denom < 0L) {
        numer = -numer;
        denom = -denom;
    }
    gcd = binding_gcd_long(numer, denom);
    if (gcd > 1L) {
        numer /= gcd;
        denom /= gcd;
    }

    *numer_out = numer;
    *denom_out = denom;
    return true;
}

static int binding_compare_rational(long left_numer,
                                    long left_denom,
                                    long right_numer,
                                    long right_denom)
{
    long double left;
    long double right;

    if (left_denom < 0L) {
        left_numer = -left_numer;
        left_denom = -left_denom;
    }
    if (right_denom < 0L) {
        right_numer = -right_numer;
        right_denom = -right_denom;
    }

    left = (long double)left_numer / (long double)left_denom;
    right = (long double)right_numer / (long double)right_denom;
    if (left < right)
        return -1;
    if (left > right)
        return 1;
    return 0;
}

static bool binding_expr_principal_inverse_domain(const dval_ops_t *outer_ops,
                                                  const dval_ops_t *inner_ops,
                                                  const dv_binding_expr_t *arg)
{
    long numer;
    long denom;

    if (!outer_ops || !inner_ops ||
        !binding_expr_pi_ratio_parts(arg, &numer, &denom))
        return false;

    /*
     * These are principal-branch rewrites, not blanket inverse rewrites.
     * They are only valid when the inner argument is visibly inside the
     * standard real principal range of the outer inverse.
     */
    if (outer_ops == &ops_atan && inner_ops == &ops_tan)
        return binding_compare_rational(numer, denom, -1L, 2L) > 0 &&
               binding_compare_rational(numer, denom, 1L, 2L) < 0;
    if (outer_ops == &ops_asin && inner_ops == &ops_sin)
        return binding_compare_rational(numer, denom, -1L, 2L) >= 0 &&
               binding_compare_rational(numer, denom, 1L, 2L) <= 0;
    if (outer_ops == &ops_acos && inner_ops == &ops_cos)
        return binding_compare_rational(numer, denom, 0L, 1L) >= 0 &&
               binding_compare_rational(numer, denom, 1L, 1L) <= 0;

    return false;
}

dv_binding_expr_t *binding_expr_try_simplify_principal_inverse(dv_binding_expr_t *expr)
{
    dv_binding_expr_t *inner;
    dv_binding_expr_t *out;

    if (!expr ||
        expr->kind != DV_BINDING_EXPR_UNARY_OP ||
        !expr->u.unary_op.ops)
        return expr;

    inner = expr->u.unary_op.child;
    if (!inner ||
        inner->kind != DV_BINDING_EXPR_UNARY_OP ||
        !inner->u.unary_op.ops ||
        !binding_expr_principal_inverse_domain(expr->u.unary_op.ops,
                                               inner->u.unary_op.ops,
                                               inner->u.unary_op.child))
        return expr;

    out = inner->u.unary_op.child;
    inner->u.unary_op.child = NULL;
    expr->u.unary_op.child = NULL;
    dv_binding_expr_free(expr);
    dv_binding_expr_free(inner);
    return out;
}

static dv_binding_expr_t *binding_expr_fold_to_neg_number_owned(dv_binding_expr_t *expr,
                                                                number_t value)
{
    return binding_expr_fold_to_number_owned(expr, num_neg(value));
}

static dv_binding_expr_t *binding_expr_sqrt_ulong(unsigned long value)
{
    return dv_binding_expr_new_unary_op(&ops_sqrt,
                                        binding_expr_new_ulong(value));
}

static dv_binding_expr_t *binding_expr_sqrt_quotient_ulong(unsigned long radicand,
                                                           unsigned long denom)
{
    return dv_binding_expr_new_div(binding_expr_sqrt_ulong(radicand),
                                   binding_expr_new_ulong(denom));
}

static dv_binding_expr_t *binding_expr_neg_sqrt_ulong(unsigned long radicand)
{
    return dv_binding_expr_new_neg(binding_expr_sqrt_ulong(radicand));
}

static dv_binding_expr_t *binding_expr_neg_sqrt_quotient_ulong(unsigned long radicand,
                                                               unsigned long denom)
{
    return dv_binding_expr_new_neg(
        binding_expr_sqrt_quotient_ulong(radicand, denom));
}

typedef struct binding_trig_exact_rule_t binding_trig_exact_rule_t;
typedef dv_binding_expr_t *(*binding_trig_fold_fn)(
    dv_binding_expr_t *expr,
    const binding_trig_exact_rule_t *rule);

struct binding_trig_exact_rule_t {
    const dval_ops_t *ops;
    long twelfths;
    binding_trig_fold_fn fold;
    const number_t *number_value;
    unsigned long radicand;
    unsigned long denominator;
};

static dv_binding_expr_t *binding_trig_fold_number(
    dv_binding_expr_t *expr,
    const binding_trig_exact_rule_t *rule)
{
    return binding_expr_fold_to_number_owned(expr, num_clone(*rule->number_value));
}

static dv_binding_expr_t *binding_trig_fold_neg_number(
    dv_binding_expr_t *expr,
    const binding_trig_exact_rule_t *rule)
{
    return binding_expr_fold_to_neg_number_owned(expr, *rule->number_value);
}

static dv_binding_expr_t *binding_trig_fold_sqrt(
    dv_binding_expr_t *expr,
    const binding_trig_exact_rule_t *rule)
{
    return binding_expr_fold_to_expr_owned(expr,
                                           binding_expr_sqrt_ulong(rule->radicand));
}

static dv_binding_expr_t *binding_trig_fold_neg_sqrt(
    dv_binding_expr_t *expr,
    const binding_trig_exact_rule_t *rule)
{
    return binding_expr_fold_to_expr_owned(expr,
                                           binding_expr_neg_sqrt_ulong(rule->radicand));
}

static dv_binding_expr_t *binding_trig_fold_sqrt_quotient(
    dv_binding_expr_t *expr,
    const binding_trig_exact_rule_t *rule)
{
    return binding_expr_fold_to_expr_owned(
        expr,
        binding_expr_sqrt_quotient_ulong(rule->radicand, rule->denominator));
}

static dv_binding_expr_t *binding_trig_fold_neg_sqrt_quotient(
    dv_binding_expr_t *expr,
    const binding_trig_exact_rule_t *rule)
{
    return binding_expr_fold_to_expr_owned(
        expr,
        binding_expr_neg_sqrt_quotient_ulong(rule->radicand, rule->denominator));
}

static const binding_trig_exact_rule_t s_binding_trig_exact_rules[] = {
    { &ops_sin, 0L,  binding_trig_fold_number,            &NUM_ZERO,    0ul, 0ul },
    { &ops_sin, 12L, binding_trig_fold_number,            &NUM_ZERO,    0ul, 0ul },
    { &ops_sin, 2L,  binding_trig_fold_number,            &NUM_HALF,    0ul, 0ul },
    { &ops_sin, 10L, binding_trig_fold_number,            &NUM_HALF,    0ul, 0ul },
    { &ops_sin, 3L,  binding_trig_fold_sqrt_quotient,     NULL,         2ul, 2ul },
    { &ops_sin, 9L,  binding_trig_fold_sqrt_quotient,     NULL,         2ul, 2ul },
    { &ops_sin, 4L,  binding_trig_fold_sqrt_quotient,     NULL,         3ul, 2ul },
    { &ops_sin, 8L,  binding_trig_fold_sqrt_quotient,     NULL,         3ul, 2ul },
    { &ops_sin, 6L,  binding_trig_fold_number,            &NUM_ONE,     0ul, 0ul },
    { &ops_sin, 15L, binding_trig_fold_neg_sqrt_quotient, NULL,         2ul, 2ul },
    { &ops_sin, 21L, binding_trig_fold_neg_sqrt_quotient, NULL,         2ul, 2ul },
    { &ops_sin, 16L, binding_trig_fold_neg_sqrt_quotient, NULL,         3ul, 2ul },
    { &ops_sin, 20L, binding_trig_fold_neg_sqrt_quotient, NULL,         3ul, 2ul },
    { &ops_sin, 14L, binding_trig_fold_neg_number,        &NUM_HALF,    0ul, 0ul },
    { &ops_sin, 22L, binding_trig_fold_neg_number,        &NUM_HALF,    0ul, 0ul },
    { &ops_sin, 18L, binding_trig_fold_number,            &NUM_NEG_ONE, 0ul, 0ul },

    { &ops_cos, 0L,  binding_trig_fold_number,            &NUM_ONE,     0ul, 0ul },
    { &ops_cos, 4L,  binding_trig_fold_number,            &NUM_HALF,    0ul, 0ul },
    { &ops_cos, 20L, binding_trig_fold_number,            &NUM_HALF,    0ul, 0ul },
    { &ops_cos, 2L,  binding_trig_fold_sqrt_quotient,     NULL,         3ul, 2ul },
    { &ops_cos, 22L, binding_trig_fold_sqrt_quotient,     NULL,         3ul, 2ul },
    { &ops_cos, 3L,  binding_trig_fold_sqrt_quotient,     NULL,         2ul, 2ul },
    { &ops_cos, 21L, binding_trig_fold_sqrt_quotient,     NULL,         2ul, 2ul },
    { &ops_cos, 6L,  binding_trig_fold_number,            &NUM_ZERO,    0ul, 0ul },
    { &ops_cos, 18L, binding_trig_fold_number,            &NUM_ZERO,    0ul, 0ul },
    { &ops_cos, 9L,  binding_trig_fold_neg_sqrt_quotient, NULL,         2ul, 2ul },
    { &ops_cos, 15L, binding_trig_fold_neg_sqrt_quotient, NULL,         2ul, 2ul },
    { &ops_cos, 10L, binding_trig_fold_neg_sqrt_quotient, NULL,         3ul, 2ul },
    { &ops_cos, 14L, binding_trig_fold_neg_sqrt_quotient, NULL,         3ul, 2ul },
    { &ops_cos, 8L,  binding_trig_fold_neg_number,        &NUM_HALF,    0ul, 0ul },
    { &ops_cos, 16L, binding_trig_fold_neg_number,        &NUM_HALF,    0ul, 0ul },
    { &ops_cos, 12L, binding_trig_fold_number,            &NUM_NEG_ONE, 0ul, 0ul },

    { &ops_tan, 0L,  binding_trig_fold_number,            &NUM_ZERO,    0ul, 0ul },
    { &ops_tan, 12L, binding_trig_fold_number,            &NUM_ZERO,    0ul, 0ul },
    { &ops_tan, 3L,  binding_trig_fold_number,            &NUM_ONE,     0ul, 0ul },
    { &ops_tan, 15L, binding_trig_fold_number,            &NUM_ONE,     0ul, 0ul },
    { &ops_tan, 2L,  binding_trig_fold_sqrt_quotient,     NULL,         3ul, 3ul },
    { &ops_tan, 14L, binding_trig_fold_sqrt_quotient,     NULL,         3ul, 3ul },
    { &ops_tan, 4L,  binding_trig_fold_sqrt,              NULL,         3ul, 0ul },
    { &ops_tan, 16L, binding_trig_fold_sqrt,              NULL,         3ul, 0ul },
    { &ops_tan, 6L,  binding_trig_fold_number,            &NUM_INF,     0ul, 0ul },
    { &ops_tan, 8L,  binding_trig_fold_neg_sqrt,          NULL,         3ul, 0ul },
    { &ops_tan, 20L, binding_trig_fold_neg_sqrt,          NULL,         3ul, 0ul },
    { &ops_tan, 10L, binding_trig_fold_neg_sqrt_quotient, NULL,         3ul, 3ul },
    { &ops_tan, 22L, binding_trig_fold_neg_sqrt_quotient, NULL,         3ul, 3ul },
    { &ops_tan, 9L,  binding_trig_fold_number,            &NUM_NEG_ONE, 0ul, 0ul },
    { &ops_tan, 21L, binding_trig_fold_number,            &NUM_NEG_ONE, 0ul, 0ul },
    { &ops_tan, 18L, binding_trig_fold_number,            &NUM_NINF,    0ul, 0ul }
};

static dv_binding_expr_t *binding_expr_fold_trig_rule_owned(
    dv_binding_expr_t *expr,
    const binding_trig_exact_rule_t *rule)
{
    return rule && rule->fold ? rule->fold(expr, rule) : expr;
}

dv_binding_expr_t *binding_expr_try_simplify_trig_exact(dv_binding_expr_t *expr)
{
    long twelfths;
    size_t i;

    if (!expr || expr->kind != DV_BINDING_EXPR_UNARY_OP)
        return expr;

    if (binding_number_text_eq_long(expr->u.unary_op.child, 0L))
        twelfths = 0L;
    else if (!binding_expr_pi_ratio_twelfths(expr->u.unary_op.child, &twelfths))
        return expr;

    for (i = 0u; i < sizeof(s_binding_trig_exact_rules) /
                        sizeof(s_binding_trig_exact_rules[0]); ++i) {
        if (s_binding_trig_exact_rules[i].ops == expr->u.unary_op.ops &&
            s_binding_trig_exact_rules[i].twelfths == twelfths)
            return binding_expr_fold_trig_rule_owned(expr,
                                                     &s_binding_trig_exact_rules[i]);
    }

    return expr;
}

dv_binding_expr_t *binding_expr_try_simplify_direct_inverse(dv_binding_expr_t *expr)
{
    dv_binding_expr_t *inner;
    dv_binding_expr_t *out;

    if (!expr ||
        expr->kind != DV_BINDING_EXPR_UNARY_OP ||
        !expr->u.unary_op.ops)
        return expr;

    inner = expr->u.unary_op.child;
    if (!inner ||
        inner->kind != DV_BINDING_EXPR_UNARY_OP ||
        !dv_ops_are_direct_inverse_pair(expr->u.unary_op.ops,
                                        inner->u.unary_op.ops))
        return expr;

    out = inner->u.unary_op.child;
    inner->u.unary_op.child = NULL;
    expr->u.unary_op.child = NULL;
    dv_binding_expr_free(expr);
    dv_binding_expr_free(inner);
    return out;
}

static const dv_binding_expr_t *binding_expr_lambert_arg(const dv_binding_expr_t *expr)
{
    if (!expr ||
        expr->kind != DV_BINDING_EXPR_UNARY_OP ||
        !dv_ops_is_lambert(expr->u.unary_op.ops))
        return NULL;

    return expr->u.unary_op.child;
}

static bool binding_expr_lambert_args_match(const dv_binding_expr_t *left,
                                            const dv_binding_expr_t *right)
{
    number_t left_value;
    number_t right_value;
    bool equal;

    if (dv_binding_expr_struct_eq(left, right))
        return true;
    if (!left || !right)
        return false;

    left_value = dv_binding_expr_eval(left);
    right_value = dv_binding_expr_eval(right);
    equal = num_eq(left_value, right_value);
    num_destroy(&right_value);
    num_destroy(&left_value);
    return equal;
}

static bool binding_expr_extract_exp_arg(const dv_binding_expr_t *expr,
                                         dv_binding_expr_t **arg_out)
{
    if (!expr || !arg_out)
        return false;

    *arg_out = NULL;

    if (expr->kind == DV_BINDING_EXPR_UNARY_OP &&
        expr->u.unary_op.ops == &ops_exp) {
        *arg_out = dv_binding_expr_clone(expr->u.unary_op.child);
        return *arg_out != NULL;
    }

    if (expr->kind == DV_BINDING_EXPR_POWI &&
        binding_expr_is_const_id(expr->u.powi.base, DV_BINDING_CONST_E)) {
        *arg_out = binding_expr_new_long(expr->u.powi.exponent);
        return *arg_out != NULL;
    }

    if (expr->kind == DV_BINDING_EXPR_BINARY_OP &&
        expr->u.binary_op.ops == &ops_pow &&
        binding_expr_is_const_id(expr->u.binary_op.left, DV_BINDING_CONST_E)) {
        *arg_out = dv_binding_expr_clone(expr->u.binary_op.right);
        return *arg_out != NULL;
    }

    return false;
}

static bool binding_expr_is_exp_of_same_lambert(const dv_binding_expr_t *expr,
                                                const dv_binding_expr_t *lambert_expr)
{
    dv_binding_expr_t *arg = NULL;
    bool match = false;

    if (!binding_expr_extract_exp_arg(expr, &arg))
        return false;

    match = dv_binding_expr_struct_eq(arg, lambert_expr);
    dv_binding_expr_free(arg);
    return match;
}

dv_binding_expr_t *binding_expr_try_simplify_lambert_product(dv_binding_expr_t *expr)
{
    const dv_binding_expr_t *arg = NULL;
    dv_binding_expr_t *out;

    if (!expr || expr->kind != DV_BINDING_EXPR_MUL)
        return expr;

    arg = binding_expr_lambert_arg(expr->u.binary.left);
    if (!arg ||
        !binding_expr_is_exp_of_same_lambert(expr->u.binary.right,
                                             expr->u.binary.left)) {
        arg = binding_expr_lambert_arg(expr->u.binary.right);
        if (!arg ||
            !binding_expr_is_exp_of_same_lambert(expr->u.binary.left,
                                                 expr->u.binary.right))
            return expr;
    }

    out = dv_binding_expr_clone(arg);
    if (!out)
        return expr;

    dv_binding_expr_free(expr);
    return out;
}

static bool binding_expr_extract_exp_factor(const dv_binding_expr_t *expr,
                                            dv_binding_expr_t **exp_arg_out,
                                            dv_binding_expr_t **other_out)
{
    dv_binding_expr_t *exp_arg = NULL;
    dv_binding_expr_t *other = NULL;

    if (!expr || !exp_arg_out || !other_out)
        return false;

    *exp_arg_out = NULL;
    *other_out = NULL;

    if (binding_expr_extract_exp_arg(expr, exp_arg_out)) {
        *other_out = binding_expr_new_long(1L);
        if (*other_out)
            return true;
        dv_binding_expr_free(*exp_arg_out);
        *exp_arg_out = NULL;
        return false;
    }

    if (expr->kind == DV_BINDING_EXPR_MUL) {
        if (binding_expr_extract_exp_arg(expr->u.binary.left, exp_arg_out)) {
            *other_out = dv_binding_expr_clone(expr->u.binary.right);
            if (*other_out)
                return true;
            dv_binding_expr_free(*exp_arg_out);
            *exp_arg_out = NULL;
            return false;
        }
        if (binding_expr_extract_exp_arg(expr->u.binary.right, exp_arg_out)) {
            *other_out = dv_binding_expr_clone(expr->u.binary.left);
            if (*other_out)
                return true;
            dv_binding_expr_free(*exp_arg_out);
            *exp_arg_out = NULL;
            return false;
        }
    }

    if (expr->kind == DV_BINDING_EXPR_DIV &&
        binding_expr_extract_exp_factor(expr->u.binary.left, &exp_arg, &other)) {
        *exp_arg_out = exp_arg;
        *other_out = dv_binding_expr_new_div(other,
                                             dv_binding_expr_clone(expr->u.binary.right));
        if (!*other_out) {
            dv_binding_expr_free(exp_arg);
            dv_binding_expr_free(other);
            *exp_arg_out = NULL;
            return false;
        }
        return true;
    }

    return false;
}

static dv_binding_expr_t *binding_expr_lambert_inverse_arg(const dv_binding_expr_t *expr)
{
    dv_binding_expr_t *exp_arg = NULL;
    dv_binding_expr_t *other = NULL;
    bool match = false;

    if (!binding_expr_extract_exp_factor(expr, &exp_arg, &other))
        return NULL;

    exp_arg = dv_binding_expr_simplify(exp_arg);
    other = dv_binding_expr_simplify(other);
    match = binding_expr_lambert_args_match(other, exp_arg);
    dv_binding_expr_free(other);
    if (match)
        return exp_arg;

    dv_binding_expr_free(exp_arg);
    return NULL;
}

static bool binding_expr_lambert_inverse_domain_ok(const dval_ops_t *ops,
                                                   const dv_binding_expr_t *arg)
{
    number_t value;
    bool ok = false;

    if (!ops || !arg)
        return false;

    value = dv_binding_expr_eval(arg);
    ok = dv_inverse_unary_candidate_value_ok(ops, value);
    num_destroy(&value);
    return ok;
}

dv_binding_expr_t *binding_expr_try_simplify_lambert_inverse(dv_binding_expr_t *expr)
{
    dv_binding_expr_t *arg;
    dv_binding_expr_t *out;

    if (!expr ||
        expr->kind != DV_BINDING_EXPR_UNARY_OP ||
        !dv_ops_is_lambert(expr->u.unary_op.ops))
        return expr;

    arg = binding_expr_lambert_inverse_arg(expr->u.unary_op.child);
    if (!arg ||
        !binding_expr_lambert_inverse_domain_ok(expr->u.unary_op.ops, arg)) {
        dv_binding_expr_free(arg);
        return expr;
    }

    out = arg;

    dv_binding_expr_free(expr);
    return out;
}

dv_binding_expr_t *binding_expr_try_simplify_complex_floor_ceil(dv_binding_expr_t *expr)
{
    number_t value;

    if (!expr ||
        expr->kind != DV_BINDING_EXPR_UNARY_OP ||
        !dv_ops_is_floor_or_ceil(expr->u.unary_op.ops))
        return expr;

    value = dv_binding_expr_eval(expr);
    if (num_is_finite(value) && !num_is_real(value))
        return binding_expr_fold_to_number_owned(expr, value);

    num_destroy(&value);
    return expr;
}

dv_binding_expr_t *binding_expr_try_simplify_log_e(dv_binding_expr_t *expr)
{
    if (!expr ||
        expr->kind != DV_BINDING_EXPR_UNARY_OP ||
        expr->u.unary_op.ops != &ops_log ||
        !binding_expr_is_const_id(expr->u.unary_op.child, DV_BINDING_CONST_E))
        return expr;

    return binding_expr_fold_to_number_owned(expr, num_clone(NUM_ONE));
}

dv_binding_expr_t *binding_expr_try_simplify_log10_power(dv_binding_expr_t *expr)
{
    long exponent;

    if (!expr ||
        expr->kind != DV_BINDING_EXPR_UNARY_OP ||
        expr->u.unary_op.ops != &ops_log10 ||
        !binding_number_text_log10_power_exponent(expr->u.unary_op.child,
                                                  &exponent))
        return expr;

    return binding_expr_fold_to_expr_owned(expr,
                                           binding_expr_new_long(exponent));
}

dv_binding_expr_t *binding_expr_try_fold_neg_leading_number(dv_binding_expr_t *expr)
{
    number_t coeff;
    number_t neg;
    dv_binding_expr_t *rest = NULL;

    if (!expr || expr->kind != DV_BINDING_EXPR_NEG)
        return expr;

    if (!dv_binding_expr_split_leading_number(expr->u.unary.child,
                                              &coeff,
                                              &rest))
        return expr;

    neg = num_scope_detach(num_neg(coeff));
    num_destroy(&coeff);
    dv_binding_expr_free(expr);
    return binding_expr_scaled_product_owned(neg, rest, NULL);
}

dv_binding_expr_t *binding_expr_try_fold_mul_leading_numbers(dv_binding_expr_t *expr)
{
    number_t left_coeff;
    number_t right_coeff;
    number_t folded;
    dv_binding_expr_t *left_rest = NULL;
    dv_binding_expr_t *right_rest = NULL;

    if (!expr || expr->kind != DV_BINDING_EXPR_MUL)
        return expr;

    if (!dv_binding_expr_split_leading_number(expr->u.binary.left,
                                              &left_coeff,
                                              &left_rest))
        return expr;

    if (!dv_binding_expr_split_leading_number(expr->u.binary.right,
                                              &right_coeff,
                                              &right_rest)) {
        num_destroy(&left_coeff);
        dv_binding_expr_free(left_rest);
        return expr;
    }

    folded = num_scope_detach(num_mul(left_coeff, right_coeff));
    num_destroy(&right_coeff);
    num_destroy(&left_coeff);
    dv_binding_expr_free(expr);
    return binding_expr_scaled_product_owned(folded, left_rest, right_rest);
}

dv_binding_expr_t *binding_expr_try_fold_div_leading_number(dv_binding_expr_t *expr)
{
    number_t left_coeff;
    number_t right_coeff;
    number_t folded;
    dv_binding_expr_t *left_rest = NULL;

    if (!expr ||
        expr->kind != DV_BINDING_EXPR_DIV ||
        !dv_binding_expr_number_value(expr->u.binary.right, &right_coeff))
        return expr;

    if (!dv_binding_expr_split_leading_number(expr->u.binary.left,
                                              &left_coeff,
                                              &left_rest)) {
        num_destroy(&right_coeff);
        return expr;
    }

    folded = num_scope_detach(num_div(left_coeff, right_coeff));
    num_destroy(&left_coeff);
    num_destroy(&right_coeff);
    dv_binding_expr_free(expr);
    return binding_expr_scaled_product_owned(folded, left_rest, NULL);
}
