#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expr_binding_simplify.h"
#include "expr_stringout.h"
#include "expr_stringout_internal.h"
#include "internal/number_internal.h"
#include "ustring.h"

static bool binding_simplify_cursor_peek_digit(const string_cursor_t *cursor,
                                               unsigned int *digit_out)
{
    char ch;

    if (!rune_to_ascii(string_cursor_peek(cursor), &ch) ||
        ch < '0' || ch > '9')
        return false;

    if (digit_out)
        *digit_out = (unsigned int)(ch - '0');
    return true;
}

static bool binding_text_to_ulong(const string_t *text, unsigned long *out)
{
    string_cursor_t *cursor;
    unsigned long parsed = 0ul;
    size_t digits = 0u;
    bool ok = false;

    if (!text || !out)
        return false;

    cursor = string_cursor_new(text);
    if (!cursor)
        return false;

    while (binding_simplify_cursor_peek_digit(cursor, NULL)) {
        unsigned int digit;

        (void)binding_simplify_cursor_peek_digit(cursor, &digit);
        if (parsed > (ULONG_MAX - digit) / 10ul)
            goto done;
        parsed = parsed * 10ul + digit;
        digits++;
        (void)string_cursor_next(cursor);
    }

    ok = digits > 0u && string_cursor_done(cursor);
    if (ok)
        *out = parsed;

done:
    string_cursor_free(cursor);
    return ok;
}

static bool binding_text_to_long(const string_t *text, long *out)
{
    string_cursor_t *cursor;
    unsigned long parsed = 0ul;
    unsigned long limit;
    size_t digits = 0u;
    bool negative = false;
    bool ok = false;

    if (!text || !out)
        return false;

    cursor = string_cursor_new(text);
    if (!cursor)
        return false;

    if (rune_is_equal(string_cursor_peek(cursor), '+') ||
        rune_is_equal(string_cursor_peek(cursor), '-')) {
        negative = rune_is_equal(string_cursor_peek(cursor), '-');
        (void)string_cursor_next(cursor);
    }

    limit = negative ? (unsigned long)LONG_MAX + 1ul : (unsigned long)LONG_MAX;
    while (binding_simplify_cursor_peek_digit(cursor, NULL)) {
        unsigned int digit;

        (void)binding_simplify_cursor_peek_digit(cursor, &digit);
        if (parsed > (limit - digit) / 10ul)
            goto done;
        parsed = parsed * 10ul + digit;
        digits++;
        (void)string_cursor_next(cursor);
    }

    ok = digits > 0u && string_cursor_done(cursor);
    if (ok) {
        if (negative && parsed == (unsigned long)LONG_MAX + 1ul)
            *out = LONG_MIN;
        else if (negative)
            *out = -(long)parsed;
        else
            *out = (long)parsed;
    }

done:
    string_cursor_free(cursor);
    return ok;
}

static bool binding_expr_positive_ulong_value(const expr_binding_expr_t *expr,
                                              unsigned long *out)
{
    number_t value;
    string_t *text;
    unsigned long parsed;
    bool ok;

    if (!out || !expr_binding_expr_number_value(expr, &value))
        return false;
    if (!num_is_real(value) || !num_is_integer(value) || !num_gt(value, NUM_ZERO)) {
        num_destroy(&value);
        return false;
    }

    text = num_to_string(value);
    num_destroy(&value);
    if (!text)
        return false;

    ok = binding_text_to_ulong(text, &parsed) && parsed > 0ul;
    string_free(text);
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

static bool binding_checked_mul_long(long a, long b, long *out)
{
    if (!out)
        return false;
    if (a > 0L) {
        if ((b > 0L && a > LONG_MAX / b) ||
            (b < 0L && b < LONG_MIN / a))
            return false;
    } else if (a < 0L) {
        if ((b > 0L && a < LONG_MIN / b) ||
            (b < 0L && a < LONG_MAX / b))
            return false;
    }
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

static expr_binding_expr_t *binding_expr_new_ulong(unsigned long value)
{
    char text[32];

    snprintf(text, sizeof(text), "%lu", value);
    return expr_binding_expr_new_number_text(text);
}

static expr_binding_expr_t *binding_expr_new_long(long value)
{
    char text[32];

    snprintf(text, sizeof(text), "%ld", value);
    return expr_binding_expr_new_number_text(text);
}

expr_binding_expr_t *binding_expr_try_simplify_logbeta_integers(expr_binding_expr_t *expr)
{
    unsigned long a;
    unsigned long b;
    unsigned long denominator;
    expr_binding_expr_t *out;

    if (!expr ||
        expr->kind != EXPR_BINDING_EXPR_BINARY_OP ||
        expr->u.binary_op.ops != &ops_logbeta ||
        !binding_expr_positive_ulong_value(expr->u.binary_op.left, &a) ||
        !binding_expr_positive_ulong_value(expr->u.binary_op.right, &b) ||
        !binding_logbeta_integer_denominator(a, b, &denominator))
        return expr;

    if (denominator == 1ul) {
        out = expr_binding_expr_new_number_text("0");
    } else {
        out = expr_binding_expr_new_neg(
            expr_binding_expr_new_unary_op(&ops_log,
                                         binding_expr_new_ulong(denominator)));
    }

    expr_binding_expr_free(expr);
    return out;
}

bool binding_number_text_eq_long(const expr_binding_expr_t *expr,
                                 long expected_long)
{
    number_t value;
    number_t expected;
    bool equal;

    if (!expr || expr->kind != EXPR_BINDING_EXPR_NUMBER)
        return false;

    value = binding_number_from_text(expr->u.text);
    expected = num_create_from_long(expected_long);
    equal = num_eq(value, expected);
    num_destroy(&expected);
    num_destroy(&value);
    return equal;
}

bool binding_number_text_to_long(const expr_binding_expr_t *expr, long *out)
{
    number_t value;
    number_t floor_value;
    bool ok = false;

    if (!expr || expr->kind != EXPR_BINDING_EXPR_NUMBER)
        return false;

    value = binding_number_from_text(expr->u.text);
    floor_value = num_floor(value);
    if (num_eq(value, floor_value)) {
        string_t *text = num_to_string(value);
        long parsed = 0;

        if (text && binding_text_to_long(text, &parsed)) {
            *out = parsed;
            ok = true;
        }
        string_free(text);
    }
    num_destroy(&floor_value);
    num_destroy(&value);
    return ok;
}

bool binding_number_text_to_small_rational(const expr_binding_expr_t *expr,
                                           long *numerator,
                                           long *denominator)
{
    number_t value;
    bool ok;

    if (!expr || expr->kind != EXPR_BINDING_EXPR_NUMBER)
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

bool binding_number_text_log10_power_exponent(const expr_binding_expr_t *expr,
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
static bool binding_expr_as_integer_power(const expr_binding_expr_t *expr,
                                          const expr_binding_expr_t **base_out,
                                          long *exponent_out)
{
    if (!expr || !base_out || !exponent_out)
        return false;

    if (expr->kind == EXPR_BINDING_EXPR_POWI) {
        *base_out = expr->u.powi.base;
        *exponent_out = expr->u.powi.exponent;
        return true;
    }

    if (expr->kind == EXPR_BINDING_EXPR_BINARY_OP &&
        expr->u.binary_op.ops == &ops_pow &&
        binding_number_text_to_long(expr->u.binary_op.right, exponent_out)) {
        *base_out = expr->u.binary_op.left;
        return true;
    }

    *base_out = expr;
    *exponent_out = 1L;
    return true;
}

expr_binding_expr_t *binding_expr_try_combine_mul_powers(expr_binding_expr_t *expr)
{
    const expr_binding_expr_t *left_base;
    const expr_binding_expr_t *right_base;
    long left_exponent;
    long right_exponent;
    long sum_exponent;
    expr_binding_expr_t *base;

    if (!expr || expr->kind != EXPR_BINDING_EXPR_MUL)
        return expr;

    if (!binding_expr_as_integer_power(expr->u.binary.left,
                                       &left_base,
                                       &left_exponent) ||
        !binding_expr_as_integer_power(expr->u.binary.right,
                                       &right_base,
                                       &right_exponent) ||
        !expr_binding_expr_struct_eq(left_base, right_base))
        return expr;

    if ((right_exponent > 0 && left_exponent > LONG_MAX - right_exponent) ||
        (right_exponent < 0 && left_exponent < LONG_MIN - right_exponent))
        return expr;

    sum_exponent = left_exponent + right_exponent;
    base = expr_binding_expr_clone(left_base);
    expr_binding_expr_free(expr);

    if (sum_exponent == 0L) {
        expr_binding_expr_free(base);
        return expr_binding_expr_new_number_text("1");
    }
    if (sum_exponent == 1L)
        return base;
    return expr_binding_expr_simplify(expr_binding_expr_new_powi(base, sum_exponent));
}

expr_binding_expr_t *binding_expr_try_simplify_nested_power(expr_binding_expr_t *expr)
{
    expr_binding_expr_t *inner;
    expr_binding_expr_t *base;
    expr_binding_expr_t *inner_exponent;
    expr_binding_expr_t *scaled_exponent;
    long exponent;

    if (!expr || expr->kind != EXPR_BINDING_EXPR_POWI ||
        !expr->u.powi.base)
        return expr;

    inner = expr->u.powi.base;

    if (inner->kind == EXPR_BINDING_EXPR_POWI) {
        if (!binding_checked_mul_long(inner->u.powi.exponent,
                                      expr->u.powi.exponent,
                                      &exponent))
            return expr;

        base = inner->u.powi.base;
        inner->u.powi.base = NULL;
        expr->u.powi.base = NULL;

        expr_binding_expr_free(inner);
        expr_binding_expr_free(expr);
        return expr_binding_expr_simplify(expr_binding_expr_new_powi(base, exponent));
    }

    if (inner->kind != EXPR_BINDING_EXPR_BINARY_OP ||
        inner->u.binary_op.ops != &ops_pow)
        return expr;

    base = inner->u.binary_op.left;
    inner_exponent = inner->u.binary_op.right;
    inner->u.binary_op.left = NULL;
    inner->u.binary_op.right = NULL;
    expr->u.powi.base = NULL;

    scaled_exponent = expr_binding_expr_simplify(
        expr_binding_expr_new_mul(binding_expr_new_long(expr->u.powi.exponent),
                                inner_exponent));

    expr_binding_expr_free(inner);
    expr_binding_expr_free(expr);
    return expr_binding_expr_simplify(
        expr_binding_expr_new_binary_op(&ops_pow, base, scaled_exponent));
}

expr_binding_expr_t *binding_expr_try_simplify_sqrt_square(expr_binding_expr_t *expr)
{
    expr_binding_expr_t *sqrt_expr;
    expr_binding_expr_t *inner;

    if (!expr)
        return expr;

    if (expr->kind == EXPR_BINDING_EXPR_POWI &&
        expr->u.powi.exponent == 2L &&
        expr->u.powi.base &&
        expr->u.powi.base->kind == EXPR_BINDING_EXPR_UNARY_OP &&
        expr->u.powi.base->u.unary_op.ops == &ops_sqrt) {
        sqrt_expr = expr->u.powi.base;
        expr->u.powi.base = NULL;
    } else if (expr->kind == EXPR_BINDING_EXPR_BINARY_OP &&
               expr->u.binary_op.ops == &ops_pow &&
               binding_number_text_eq_long(expr->u.binary_op.right, 2L) &&
               expr->u.binary_op.left &&
               expr->u.binary_op.left->kind == EXPR_BINDING_EXPR_UNARY_OP &&
               expr->u.binary_op.left->u.unary_op.ops == &ops_sqrt) {
        sqrt_expr = expr->u.binary_op.left;
        expr->u.binary_op.left = NULL;
    } else {
        return expr;
    }

    inner = sqrt_expr->u.unary_op.child;
    sqrt_expr->u.unary_op.child = NULL;

    expr_binding_expr_free(expr);
    expr_binding_expr_free(sqrt_expr);
    return expr_binding_expr_simplify(inner);
}

static expr_binding_expr_t *binding_expr_number_from_value(number_t value)
{
    char *text;
    expr_binding_expr_t *expr;

    if (num_is_inf(value))
        return expr_binding_expr_new_number_text(num_get_sign(value) < 0 ? "-∞" : "∞");

    text = expr_number_to_string_local(num_clone(value));
    expr = expr_binding_expr_new_number_text(text ? text : "NAN");

    free(text);
    return expr;
}

static bool binding_string_is_decimal_literal(const string_t *text)
{
    string_cursor_t *cursor;
    bool have_digit = false;
    bool have_decimal_marker = false;
    bool have_exp_digit = false;

    if (!text || string_length(text) == 0u)
        return false;

    cursor = string_cursor_new(text);
    if (!cursor)
        return false;

    if (rune_is_equal(string_cursor_peek(cursor), '+') ||
        rune_is_equal(string_cursor_peek(cursor), '-'))
        (void)string_cursor_next(cursor);

    while (binding_simplify_cursor_peek_digit(cursor, NULL)) {
        have_digit = true;
        (void)string_cursor_next(cursor);
    }

    if (rune_is_equal(string_cursor_peek(cursor), '.')) {
        have_decimal_marker = true;
        (void)string_cursor_next(cursor);
        while (binding_simplify_cursor_peek_digit(cursor, NULL)) {
            have_digit = true;
            (void)string_cursor_next(cursor);
        }
    }

    if (rune_is_equal(string_cursor_peek(cursor), 'e') ||
        rune_is_equal(string_cursor_peek(cursor), 'E')) {
        have_decimal_marker = true;
        (void)string_cursor_next(cursor);
        if (rune_is_equal(string_cursor_peek(cursor), '+') ||
            rune_is_equal(string_cursor_peek(cursor), '-'))
            (void)string_cursor_next(cursor);
        while (binding_simplify_cursor_peek_digit(cursor, NULL)) {
            have_exp_digit = true;
            (void)string_cursor_next(cursor);
        }
        if (!have_exp_digit) {
            string_cursor_free(cursor);
            return false;
        }
    }

    have_digit = have_digit && have_decimal_marker &&
                 string_cursor_done(cursor);
    string_cursor_free(cursor);
    return have_digit;
}

static string_t *binding_negated_decimal_text(const char *text)
{
    string_t *input = text ? string_new_with(text) : NULL;
    string_cursor_t *cursor;
    string_t *out;
    string_pos_t start;

    if (!input)
        return NULL;
    if (!binding_string_is_decimal_literal(input)) {
        string_free(input);
        return NULL;
    }

    cursor = string_cursor_new(input);
    if (!cursor) {
        string_free(input);
        return NULL;
    }

    if (rune_is_equal(string_cursor_peek(cursor), '-')) {
        (void)string_cursor_next(cursor);
        start = string_cursor_position(cursor);
        out = string_cursor_slice_between(start,
                                          string_cursor_end_position(cursor),
                                          cursor);
        string_cursor_free(cursor);
        string_free(input);
        return out;
    }

    if (rune_is_equal(string_cursor_peek(cursor), '+'))
        (void)string_cursor_next(cursor);

    start = string_cursor_position(cursor);
    out = string_new_with("-");
    if (out) {
        if (string_cursor_append_slice_between(out,
                                               start,
                                               string_cursor_end_position(cursor),
                                               cursor) != 0) {
            string_free(out);
            out = NULL;
        }
    }
    string_cursor_free(cursor);
    string_free(input);
    return out;
}

expr_binding_expr_t *binding_expr_try_preserve_negated_decimal_owned(
    expr_binding_expr_t *expr)
{
    expr_binding_expr_t *child;
    expr_binding_expr_t *folded;
    string_t *text;

    if (!expr || expr->kind != EXPR_BINDING_EXPR_NEG)
        return expr;

    child = expr->u.unary.child;
    if (!child || child->kind != EXPR_BINDING_EXPR_NUMBER)
        return expr;

    text = binding_negated_decimal_text(child->u.text);
    if (!text)
        return expr;

    folded = expr_binding_expr_new_number_text(string_c_str(text));
    string_free(text);
    expr_binding_expr_free(expr);
    return folded;
}

static expr_binding_expr_t *binding_expr_product_owned(expr_binding_expr_t *left,
                                                     expr_binding_expr_t *right)
{
    if (!left)
        return right;
    if (!right)
        return left;
    return expr_binding_expr_new_mul(left, right);
}

static expr_binding_expr_t *binding_expr_scaled_product_owned(number_t coeff,
                                                            expr_binding_expr_t *left_rest,
                                                            expr_binding_expr_t *right_rest)
{
    expr_binding_expr_t *rest;

    if (num_is_zero(coeff)) {
        num_destroy(&coeff);
        expr_binding_expr_free(left_rest);
        expr_binding_expr_free(right_rest);
        return binding_expr_number_from_value(NUM_ZERO);
    }

    rest = binding_expr_product_owned(left_rest, right_rest);
    if (!rest) {
        expr_binding_expr_t *out = binding_expr_number_from_value(coeff);

        num_destroy(&coeff);
        return out;
    }

    if (num_eq(coeff, NUM_ONE)) {
        num_destroy(&coeff);
        return rest;
    }
    if (num_eq(coeff, NUM_NEG_ONE)) {
        num_destroy(&coeff);
        return expr_binding_expr_new_neg(rest);
    }

    {
        expr_binding_expr_t *coeff_expr = binding_expr_number_from_value(coeff);

        num_destroy(&coeff);
        return expr_binding_expr_new_mul(coeff_expr, rest);
    }
}

expr_binding_expr_t *binding_expr_fold_to_number_owned(expr_binding_expr_t *expr,
                                                     number_t value)
{
    value = num_scope_detach(value);

    expr_binding_expr_t *folded = binding_expr_number_from_value(value);

    num_destroy(&value);
    expr_binding_expr_free(expr);
    return folded;
}

static expr_binding_expr_t *binding_expr_fold_to_expr_owned(expr_binding_expr_t *expr,
                                                          expr_binding_expr_t *folded)
{
    expr_binding_expr_free(expr);
    return folded;
}

static expr_binding_expr_t *binding_expr_add_one(const expr_binding_expr_t *arg)
{
    return expr_binding_expr_simplify(
        expr_binding_expr_new_add(expr_binding_expr_clone(arg),
                                binding_expr_new_long(1L)));
}

static expr_binding_expr_t *binding_expr_double_arg_unary(
    const expr_binding_expr_t *arg,
    const expr_ops_t *ops)
{
    return expr_binding_expr_new_unary_op(
        ops,
            expr_binding_expr_simplify(
                expr_binding_expr_new_mul(binding_expr_new_long(2L),
                                        expr_binding_expr_clone(arg))));
}

static expr_binding_expr_t *binding_expr_double_arg(
    const expr_binding_expr_t *arg)
{
    return expr_binding_expr_simplify(
        expr_binding_expr_new_mul(binding_expr_new_long(2L),
                                expr_binding_expr_clone(arg)));
}

static bool binding_expr_is_unary_op(const expr_binding_expr_t *expr,
                                     const expr_ops_t *ops)
{
    return expr &&
           expr->kind == EXPR_BINDING_EXPR_UNARY_OP &&
           expr->u.unary_op.ops == ops;
}

static const expr_binding_expr_t *binding_expr_matching_unary_args(
    const expr_binding_expr_t *a,
    const expr_binding_expr_t *b,
    const expr_ops_t *left_ops,
    const expr_ops_t *right_ops)
{
    if (binding_expr_is_unary_op(a, left_ops) &&
        binding_expr_is_unary_op(b, right_ops) &&
        expr_binding_expr_struct_eq(a->u.unary_op.child,
                                  b->u.unary_op.child))
        return a->u.unary_op.child;
    if (binding_expr_is_unary_op(a, right_ops) &&
        binding_expr_is_unary_op(b, left_ops) &&
        expr_binding_expr_struct_eq(a->u.unary_op.child,
                                  b->u.unary_op.child))
        return a->u.unary_op.child;
    return NULL;
}

static bool binding_expr_is_square_of_unary(const expr_binding_expr_t *expr,
                                            const expr_ops_t *ops,
                                            const expr_binding_expr_t **arg_out)
{
    if (!expr || !arg_out ||
        expr->kind != EXPR_BINDING_EXPR_POWI ||
        expr->u.powi.exponent != 2 ||
        !binding_expr_is_unary_op(expr->u.powi.base, ops))
        return false;

    *arg_out = expr->u.powi.base->u.unary_op.child;
    return true;
}

static bool binding_expr_is_i_times_unary(const expr_binding_expr_t *expr,
                                          const expr_ops_t *ops,
                                          const expr_binding_expr_t **arg_out)
{
    const expr_binding_expr_t *candidate;

    if (!expr || !arg_out || expr->kind != EXPR_BINDING_EXPR_MUL)
        return false;

    if (binding_expr_is_const_id(expr->u.binary.left, EXPR_BINDING_CONST_I)) {
        candidate = expr->u.binary.right;
    } else if (binding_expr_is_const_id(expr->u.binary.right, EXPR_BINDING_CONST_I)) {
        candidate = expr->u.binary.left;
    } else {
        return false;
    }

    if (!binding_expr_is_unary_op(candidate, ops))
        return false;

    *arg_out = candidate->u.unary_op.child;
    return true;
}

static bool binding_expr_is_euler_sum(const expr_binding_expr_t *expr,
                                      const expr_binding_expr_t **arg_out)
{
    const expr_binding_expr_t *cos_arg = NULL;
    const expr_binding_expr_t *sin_arg = NULL;

    if (!expr || !arg_out || expr->kind != EXPR_BINDING_EXPR_ADD)
        return false;

    if (binding_expr_is_unary_op(expr->u.binary.left, &ops_cos) &&
        binding_expr_is_i_times_unary(expr->u.binary.right, &ops_sin,
                                      &sin_arg)) {
        cos_arg = expr->u.binary.left->u.unary_op.child;
    } else if (binding_expr_is_unary_op(expr->u.binary.right, &ops_cos) &&
               binding_expr_is_i_times_unary(expr->u.binary.left, &ops_sin,
                                             &sin_arg)) {
        cos_arg = expr->u.binary.right->u.unary_op.child;
    } else {
        return false;
    }

    if (!expr_binding_expr_struct_eq(cos_arg, sin_arg))
        return false;

    *arg_out = cos_arg;
    return true;
}

expr_binding_expr_t *binding_expr_try_simplify_euler_square(expr_binding_expr_t *expr)
{
    const expr_binding_expr_t *arg = NULL;
    expr_binding_expr_t *double_arg;
    expr_binding_expr_t *cos_term;
    expr_binding_expr_t *sin_term;
    expr_binding_expr_t *imag_term;
    expr_binding_expr_t *out;

    if (!expr ||
        expr->kind != EXPR_BINDING_EXPR_POWI ||
        expr->u.powi.exponent != 2 ||
        !binding_expr_is_euler_sum(expr->u.powi.base, &arg))
        return expr;

    double_arg = binding_expr_double_arg(arg);
    cos_term = expr_binding_expr_new_unary_op(&ops_cos,
                                            expr_binding_expr_clone(double_arg));
    sin_term = expr_binding_expr_new_unary_op(&ops_sin, double_arg);
    imag_term = expr_binding_expr_new_mul(
        expr_binding_expr_new_const(EXPR_BINDING_CONST_I),
        sin_term);
    out = expr_binding_expr_new_add(cos_term, imag_term);

    return binding_expr_fold_to_expr_owned(expr, expr_binding_expr_simplify(out));
}

static bool binding_expr_is_neg_square_of_unary(
    const expr_binding_expr_t *expr,
    const expr_ops_t *ops,
    const expr_binding_expr_t **arg_out)
{
    return expr &&
           expr->kind == EXPR_BINDING_EXPR_NEG &&
           binding_expr_is_square_of_unary(expr->u.unary.child, ops, arg_out);
}

static bool binding_expr_i_unit_sign(const expr_binding_expr_t *expr,
                                     int *sign_out)
{
    int child_sign;

    if (!expr || !sign_out)
        return false;

    if (binding_expr_is_const_id(expr, EXPR_BINDING_CONST_I)) {
        *sign_out = 1;
        return true;
    }

    if (expr->kind == EXPR_BINDING_EXPR_NEG &&
        binding_expr_i_unit_sign(expr->u.unary.child, &child_sign)) {
        *sign_out = -child_sign;
        return true;
    }

    return false;
}

static bool binding_expr_extract_i_unit_factor_owned(const expr_binding_expr_t *expr,
                                                     int *sign_out,
                                                     expr_binding_expr_t **rest_out)
{
    expr_binding_expr_t *child_rest = NULL;
    int child_sign;

    if (!expr || !sign_out || !rest_out)
        return false;

    *rest_out = NULL;

    if (binding_expr_i_unit_sign(expr, sign_out)) {
        return true;
    }

    if (expr->kind == EXPR_BINDING_EXPR_NEG &&
        binding_expr_extract_i_unit_factor_owned(expr->u.unary.child,
                                                 &child_sign,
                                                 &child_rest)) {
        *sign_out = -child_sign;
        *rest_out = child_rest;
        return true;
    }

    if (expr->kind == EXPR_BINDING_EXPR_MUL) {
        if (binding_expr_extract_i_unit_factor_owned(expr->u.binary.left,
                                                     sign_out,
                                                     &child_rest)) {
            *rest_out = binding_expr_product_owned(
                child_rest,
                expr_binding_expr_clone(expr->u.binary.right));
            return true;
        }
        if (binding_expr_extract_i_unit_factor_owned(expr->u.binary.right,
                                                     sign_out,
                                                     &child_rest)) {
            *rest_out = binding_expr_product_owned(
                expr_binding_expr_clone(expr->u.binary.left),
                child_rest);
            return true;
        }
    }

    if (expr->kind == EXPR_BINDING_EXPR_DIV &&
        binding_expr_extract_i_unit_factor_owned(expr->u.binary.left,
                                                 sign_out,
                                                 &child_rest)) {
        *rest_out = expr_binding_expr_new_div(
            child_rest ? child_rest : binding_expr_new_long(1L),
            expr_binding_expr_clone(expr->u.binary.right));
            return true;
    }

    return false;
}

expr_binding_expr_t *binding_expr_try_simplify_i_unit_product(
    expr_binding_expr_t *expr)
{
    expr_binding_expr_t *left_rest = NULL;
    expr_binding_expr_t *right_rest = NULL;
    expr_binding_expr_t *out;
    int left_sign;
    int right_sign;
    int coeff_sign;

    if (!expr || expr->kind != EXPR_BINDING_EXPR_MUL ||
        !binding_expr_extract_i_unit_factor_owned(expr->u.binary.left,
                                                  &left_sign,
                                                  &left_rest))
        return expr;
    if (!binding_expr_extract_i_unit_factor_owned(expr->u.binary.right,
                                                 &right_sign,
                                                 &right_rest)) {
        expr_binding_expr_free(left_rest);
        return expr;
    }

    coeff_sign = -(left_sign * right_sign);
    out = binding_expr_product_owned(left_rest, right_rest);
    if (!out)
        out = binding_expr_new_long(1L);
    if (coeff_sign < 0)
        out = expr_binding_expr_new_neg(out);

    return binding_expr_fold_to_expr_owned(expr, expr_binding_expr_simplify(out));
}

static expr_binding_expr_t *binding_expr_imaginary_scaled_owned(
    int sign,
    expr_binding_expr_t *expr)
{
    expr_binding_expr_t *out = expr_binding_expr_new_mul(
        expr_binding_expr_new_const(EXPR_BINDING_CONST_I),
        expr);

    return sign < 0 ? expr_binding_expr_new_neg(out) : out;
}

expr_binding_expr_t *binding_expr_try_simplify_imag_trig_bridge(
    expr_binding_expr_t *expr)
{
    expr_binding_expr_t *arg = NULL;
    const expr_ops_t *target_ops = NULL;
    expr_binding_expr_t *out;
    int sign;
    bool multiply_i = false;

    if (!expr ||
        expr->kind != EXPR_BINDING_EXPR_UNARY_OP ||
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
        expr_binding_expr_free(arg);
        return expr;
    }

    out = expr_binding_expr_new_unary_op(target_ops,
                                       arg ? arg : binding_expr_new_long(1L));
    if (multiply_i)
        out = binding_expr_imaginary_scaled_owned(sign, out);

    return binding_expr_fold_to_expr_owned(expr, expr_binding_expr_simplify(out));
}

expr_binding_expr_t *binding_expr_try_simplify_basic_sum(expr_binding_expr_t *expr)
{
    const expr_binding_expr_t *log = NULL;
    const expr_binding_expr_t *lgamma = NULL;
    expr_binding_expr_t *successor_arg;
    expr_binding_expr_t *out;

    if (!expr || expr->kind != EXPR_BINDING_EXPR_ADD)
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
        !expr_binding_expr_struct_eq(log->u.unary_op.child,
                                   lgamma->u.unary_op.child))
        return expr;

    successor_arg = binding_expr_add_one(lgamma->u.unary_op.child);
    out = expr_binding_expr_new_unary_op(&ops_lgamma, successor_arg);
    return binding_expr_fold_to_expr_owned(expr, out);
}

static bool binding_expr_positive_log_argument_value(
    const expr_binding_expr_t *expr,
    number_t *out)
{
    if (!binding_expr_is_unary_op(expr, &ops_log) ||
        !expr_binding_expr_number_value(expr->u.unary_op.child, out))
        return false;

    if (!num_is_real(*out) || !num_gt(*out, NUM_ZERO)) {
        num_destroy(out);
        return false;
    }
    return true;
}

static expr_binding_expr_t *binding_expr_make_log_difference_owned(
    expr_binding_expr_t *expr,
    const expr_binding_expr_t *left_log,
    const expr_binding_expr_t *right_log)
{
    number_t left = NUM_ZERO;
    number_t right = NUM_ZERO;
    number_t quotient = NUM_ZERO;
    expr_binding_expr_t *quotient_expr;
    expr_binding_expr_t *out;

    if (!binding_expr_positive_log_argument_value(left_log, &left))
        return expr;
    if (!binding_expr_positive_log_argument_value(right_log, &right)) {
        num_destroy(&left);
        return expr;
    }

    quotient = num_scope_detach(num_div(left, right));
    num_destroy(&right);
    num_destroy(&left);
    if (!num_is_finite(quotient) || !num_gt(quotient, NUM_ZERO)) {
        num_destroy(&quotient);
        return expr;
    }

    if (num_eq(quotient, NUM_ONE)) {
        num_destroy(&quotient);
        return binding_expr_fold_to_expr_owned(
            expr, expr_binding_expr_new_number_text("0"));
    }

    quotient_expr = binding_expr_number_from_value(quotient);
    num_destroy(&quotient);
    out = expr_binding_expr_new_unary_op(&ops_log, quotient_expr);
    return binding_expr_fold_to_expr_owned(expr,
                                        expr_binding_expr_simplify(out));
}

expr_binding_expr_t *binding_expr_try_simplify_log_difference(
    expr_binding_expr_t *expr)
{
    if (!expr ||
        (expr->kind != EXPR_BINDING_EXPR_SUB &&
         expr->kind != EXPR_BINDING_EXPR_ADD))
        return expr;

    if (expr->kind == EXPR_BINDING_EXPR_SUB)
        return binding_expr_make_log_difference_owned(
            expr, expr->u.binary.left, expr->u.binary.right);

    if (expr->u.binary.left &&
        expr->u.binary.left->kind == EXPR_BINDING_EXPR_NEG)
        return binding_expr_make_log_difference_owned(
            expr, expr->u.binary.right, expr->u.binary.left->u.unary.child);

    if (expr->u.binary.right &&
        expr->u.binary.right->kind == EXPR_BINDING_EXPR_NEG)
        return binding_expr_make_log_difference_owned(
            expr, expr->u.binary.left, expr->u.binary.right->u.unary.child);

    return expr;
}

expr_binding_expr_t *binding_expr_try_simplify_basic_product(expr_binding_expr_t *expr)
{
    const expr_binding_expr_t *gamma = NULL;
    const expr_binding_expr_t *factor = NULL;
    expr_binding_expr_t *successor_arg;
    expr_binding_expr_t *out;

    if (!expr || expr->kind != EXPR_BINDING_EXPR_MUL)
        return expr;

    if (binding_expr_is_unary_op(expr->u.binary.left, &ops_gamma)) {
        gamma = expr->u.binary.left;
        factor = expr->u.binary.right;
    } else if (binding_expr_is_unary_op(expr->u.binary.right, &ops_gamma)) {
        gamma = expr->u.binary.right;
        factor = expr->u.binary.left;
    }

    if (!gamma || !factor ||
        !expr_binding_expr_struct_eq(factor, gamma->u.unary_op.child))
        return expr;

    successor_arg = binding_expr_add_one(gamma->u.unary_op.child);
    out = expr_binding_expr_new_unary_op(&ops_gamma, successor_arg);
    return binding_expr_fold_to_expr_owned(expr, out);
}

static bool binding_expr_scaled_gamma_product_parts(
    const expr_binding_expr_t *expr,
    const expr_binding_expr_t **scaled_arg_out,
    const expr_binding_expr_t **gamma_out)
{
    if (!expr || expr->kind != EXPR_BINDING_EXPR_MUL ||
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

expr_binding_expr_t *binding_expr_try_simplify_basic_quotient(expr_binding_expr_t *expr)
{
    const expr_binding_expr_t *scaled_arg;
    const expr_binding_expr_t *gamma;
    expr_binding_expr_t *arg;
    expr_binding_expr_t *successor_arg;
    expr_binding_expr_t *out;

    if (!expr || expr->kind != EXPR_BINDING_EXPR_DIV ||
        !binding_expr_scaled_gamma_product_parts(expr->u.binary.left,
                                                 &scaled_arg,
                                                 &gamma))
        return expr;

    arg = expr_binding_expr_simplify(
        expr_binding_expr_new_div(expr_binding_expr_clone(scaled_arg),
                                expr_binding_expr_clone(expr->u.binary.right)));
    if (!expr_binding_expr_struct_eq(arg, gamma->u.unary_op.child)) {
        expr_binding_expr_free(arg);
        return expr;
    }

    successor_arg = binding_expr_add_one(arg);
    out = expr_binding_expr_new_unary_op(&ops_gamma, successor_arg);
    expr_binding_expr_free(arg);
    return binding_expr_fold_to_expr_owned(expr, out);
}

expr_binding_expr_t *binding_expr_try_simplify_reciprocal_unary(expr_binding_expr_t *expr)
{
    expr_binding_expr_t *denominator;
    const expr_ops_t *replacement_ops;

    if (!expr ||
        expr->kind != EXPR_BINDING_EXPR_DIV ||
        !binding_number_text_eq_long(expr->u.binary.left, 1L))
        return expr;

    denominator = expr->u.binary.right;
    if (!denominator ||
        denominator->kind != EXPR_BINDING_EXPR_UNARY_OP ||
        !denominator->u.unary_op.ops)
        return expr;

    replacement_ops = expr_ops_reciprocal_unary(denominator->u.unary_op.ops);
    if (replacement_ops) {
        expr_binding_expr_t *out = expr_binding_expr_new_unary_op(
            replacement_ops,
            expr_binding_expr_clone(denominator->u.unary_op.child));

        return binding_expr_fold_to_expr_owned(expr,
                                               expr_binding_expr_simplify(out));
    }

    return expr;
}

expr_binding_expr_t *binding_expr_try_simplify_trig_product(expr_binding_expr_t *expr)
{
    const expr_binding_expr_t *arg;
    expr_binding_expr_t *out;

    if (!expr || expr->kind != EXPR_BINDING_EXPR_MUL)
        return expr;

    arg = binding_expr_matching_unary_args(expr->u.binary.left,
                                           expr->u.binary.right,
                                           &ops_sin,
                                           &ops_cos);
    if (arg) {
        out = expr_binding_expr_new_mul(
            binding_expr_number_from_value(NUM_HALF),
            binding_expr_double_arg_unary(arg, &ops_sin));
        return binding_expr_fold_to_expr_owned(expr,
                                               expr_binding_expr_simplify(out));
    }

    arg = binding_expr_matching_unary_args(expr->u.binary.left,
                                           expr->u.binary.right,
                                           &ops_sinh,
                                           &ops_cosh);
    if (arg) {
        out = expr_binding_expr_new_mul(
            binding_expr_number_from_value(NUM_HALF),
            binding_expr_double_arg_unary(arg, &ops_sinh));
        return binding_expr_fold_to_expr_owned(expr,
                                               expr_binding_expr_simplify(out));
    }

    arg = binding_expr_matching_unary_args(expr->u.binary.left,
                                           expr->u.binary.right,
                                           &ops_cos,
                                           &ops_tan);
    if (arg) {
        out = expr_binding_expr_new_unary_op(&ops_sin, expr_binding_expr_clone(arg));
        return binding_expr_fold_to_expr_owned(expr, out);
    }

    arg = binding_expr_matching_unary_args(expr->u.binary.left,
                                           expr->u.binary.right,
                                           &ops_cosh,
                                           &ops_tanh);
    if (arg) {
        out = expr_binding_expr_new_unary_op(&ops_sinh, expr_binding_expr_clone(arg));
        return binding_expr_fold_to_expr_owned(expr, out);
    }

    return expr;
}

expr_binding_expr_t *binding_expr_try_simplify_trig_sum(expr_binding_expr_t *expr)
{
    const expr_binding_expr_t *left_arg = NULL;
    const expr_binding_expr_t *right_arg = NULL;
    bool subtract;

    if (!expr || (expr->kind != EXPR_BINDING_EXPR_ADD &&
                  expr->kind != EXPR_BINDING_EXPR_SUB))
        return expr;

    subtract = expr->kind == EXPR_BINDING_EXPR_SUB;

    if (!subtract &&
        binding_expr_is_square_of_unary(expr->u.binary.left, &ops_sin,
                                        &left_arg) &&
        binding_expr_is_square_of_unary(expr->u.binary.right, &ops_cos,
                                        &right_arg) &&
        expr_binding_expr_struct_eq(left_arg, right_arg))
        return binding_expr_fold_to_number_owned(expr, num_clone(NUM_ONE));

    if (!subtract &&
        binding_expr_is_square_of_unary(expr->u.binary.left, &ops_cos,
                                        &left_arg) &&
        binding_expr_is_square_of_unary(expr->u.binary.right, &ops_sin,
                                        &right_arg) &&
        expr_binding_expr_struct_eq(left_arg, right_arg))
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
        expr_binding_expr_struct_eq(left_arg, right_arg)) {
        expr_binding_expr_t *out = binding_expr_double_arg_unary(left_arg,
                                                               &ops_cos);

        return binding_expr_fold_to_expr_owned(expr, out);
    }

    if (subtract &&
        binding_expr_is_square_of_unary(expr->u.binary.left, &ops_cosh,
                                        &left_arg) &&
        binding_expr_is_square_of_unary(expr->u.binary.right, &ops_sinh,
                                        &right_arg) &&
        expr_binding_expr_struct_eq(left_arg, right_arg))
        return binding_expr_fold_to_number_owned(expr, num_clone(NUM_ONE));

    if (!subtract &&
        binding_expr_is_neg_square_of_unary(expr->u.binary.left, &ops_sinh,
                                            &left_arg) &&
        binding_expr_is_square_of_unary(expr->u.binary.right, &ops_cosh,
                                        &right_arg) &&
        expr_binding_expr_struct_eq(left_arg, right_arg))
        return binding_expr_fold_to_number_owned(expr, num_clone(NUM_ONE));

    if (!subtract &&
        binding_expr_is_square_of_unary(expr->u.binary.left, &ops_sinh,
                                        &left_arg) &&
        binding_expr_is_square_of_unary(expr->u.binary.right, &ops_cosh,
                                        &right_arg) &&
        expr_binding_expr_struct_eq(left_arg, right_arg)) {
        expr_binding_expr_t *out = binding_expr_double_arg_unary(left_arg,
                                                               &ops_cosh);

        return binding_expr_fold_to_expr_owned(expr, out);
    }

    if (!subtract &&
        binding_expr_is_square_of_unary(expr->u.binary.left, &ops_cosh,
                                        &left_arg) &&
        binding_expr_is_square_of_unary(expr->u.binary.right, &ops_sinh,
                                        &right_arg) &&
        expr_binding_expr_struct_eq(left_arg, right_arg)) {
        expr_binding_expr_t *out = binding_expr_double_arg_unary(left_arg,
                                                               &ops_cosh);

        return binding_expr_fold_to_expr_owned(expr, out);
    }

    return expr;
}

static expr_binding_expr_t *binding_expr_from_exact_real(number_t value)
{
    string_t *text = num_to_string(value);
    expr_binding_expr_t *expr =
        expr_binding_expr_new_number_text(text ? string_c_str(text) : "NAN");

    string_free(text);
    return expr;
}

static expr_binding_expr_t *binding_expr_from_exact_imag(number_t imag)
{
    number_t abs_imag;
    expr_binding_expr_t *unit;

    if (num_eq(imag, NUM_ONE))
        return expr_binding_expr_new_const(EXPR_BINDING_CONST_I);
    if (num_eq(imag, NUM_NEG_ONE))
        return expr_binding_expr_new_neg(expr_binding_expr_new_const(EXPR_BINDING_CONST_I));

    abs_imag = num_get_sign(imag) < 0 ? num_abs(imag) : num_clone(imag);
    unit = expr_binding_expr_new_mul(binding_expr_from_exact_real(abs_imag),
                                   expr_binding_expr_new_const(EXPR_BINDING_CONST_I));
    num_destroy(&abs_imag);
    return num_get_sign(imag) < 0 ? expr_binding_expr_new_neg(unit) : unit;
}

static expr_binding_expr_t *binding_expr_from_exact_complex(
    const binding_exact_complex_t *value)
{
    expr_binding_expr_t *real_expr;
    expr_binding_expr_t *imag_expr;
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
        return expr_binding_expr_new_sub(real_expr, imag_expr);
    }

    imag_expr = binding_expr_from_exact_imag(value->imag);
    return expr_binding_expr_new_add(real_expr, imag_expr);
}

expr_binding_expr_t *binding_expr_try_fold_exact_complex_owned(
    expr_binding_expr_t *expr)
{
    binding_exact_complex_t value;
    expr_binding_expr_t *folded;

    if (!expr_binding_expr_exact_complex(expr, &value))
        return expr;

    folded = binding_expr_from_exact_complex(&value);
    expr_binding_exact_complex_clear(&value);
    return binding_expr_fold_to_expr_owned(expr, folded);
}

expr_binding_expr_t *binding_expr_try_fold_number_owned(expr_binding_expr_t *expr)
{
    number_t value;

    if (expr_binding_expr_number_value(expr, &value))
        return binding_expr_fold_to_number_owned(expr, value);
    return expr;
}

static bool binding_expr_pi_ratio_twelfths(const expr_binding_expr_t *expr,
                                           long *twelfths_out)
{
    long numer;
    long denom;
    long twelfths;
    expr_binding_const_id_t const_id;

    if (!expr || !twelfths_out)
        return false;

    if (binding_expr_scaled_const_ratio(expr, &numer, &denom, &const_id)) {
        if (const_id != EXPR_BINDING_CONST_PI)
            return false;
    } else if (expr->kind == EXPR_BINDING_EXPR_DIV &&
               binding_const_ratio_parts(expr->u.binary.left,
                                         expr->u.binary.right,
                                         &numer,
                                         &denom,
                                         &const_id) &&
               const_id == EXPR_BINDING_CONST_PI) {
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

static bool binding_expr_pi_ratio_parts(const expr_binding_expr_t *expr,
                                        long *numer_out,
                                        long *denom_out)
{
    long numer;
    long denom;
    long gcd;
    expr_binding_const_id_t const_id;

    if (!expr || !numer_out || !denom_out)
        return false;

    if (binding_expr_scaled_const_ratio(expr, &numer, &denom, &const_id)) {
        if (const_id != EXPR_BINDING_CONST_PI)
            return false;
    } else if (expr->kind == EXPR_BINDING_EXPR_DIV &&
               binding_const_ratio_parts(expr->u.binary.left,
                                         expr->u.binary.right,
                                         &numer,
                                         &denom,
                                         &const_id) &&
               const_id == EXPR_BINDING_CONST_PI) {
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

static bool binding_expr_principal_inverse_domain(const expr_ops_t *outer_ops,
                                                  const expr_ops_t *inner_ops,
                                                  const expr_binding_expr_t *arg)
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

expr_binding_expr_t *binding_expr_try_simplify_principal_inverse(expr_binding_expr_t *expr)
{
    expr_binding_expr_t *inner;
    expr_binding_expr_t *out;

    if (!expr ||
        expr->kind != EXPR_BINDING_EXPR_UNARY_OP ||
        !expr->u.unary_op.ops)
        return expr;

    inner = expr->u.unary_op.child;
    if (!inner ||
        inner->kind != EXPR_BINDING_EXPR_UNARY_OP ||
        !inner->u.unary_op.ops ||
        !binding_expr_principal_inverse_domain(expr->u.unary_op.ops,
                                               inner->u.unary_op.ops,
                                               inner->u.unary_op.child))
        return expr;

    out = inner->u.unary_op.child;
    inner->u.unary_op.child = NULL;
    expr->u.unary_op.child = NULL;
    expr_binding_expr_free(expr);
    expr_binding_expr_free(inner);
    return out;
}

static expr_binding_expr_t *binding_expr_fold_to_neg_number_owned(expr_binding_expr_t *expr,
                                                                number_t value)
{
    return binding_expr_fold_to_number_owned(expr, num_neg(value));
}

static expr_binding_expr_t *binding_expr_sqrt_ulong(unsigned long value)
{
    return expr_binding_expr_new_unary_op(&ops_sqrt,
                                        binding_expr_new_ulong(value));
}

static expr_binding_expr_t *binding_expr_sqrt_quotient_ulong(unsigned long radicand,
                                                           unsigned long denom)
{
    return expr_binding_expr_new_div(binding_expr_sqrt_ulong(radicand),
                                   binding_expr_new_ulong(denom));
}

static expr_binding_expr_t *binding_expr_neg_sqrt_ulong(unsigned long radicand)
{
    return expr_binding_expr_new_neg(binding_expr_sqrt_ulong(radicand));
}

static expr_binding_expr_t *binding_expr_neg_sqrt_quotient_ulong(unsigned long radicand,
                                                               unsigned long denom)
{
    return expr_binding_expr_new_neg(
        binding_expr_sqrt_quotient_ulong(radicand, denom));
}

typedef struct binding_trig_exact_rule_t binding_trig_exact_rule_t;
typedef expr_binding_expr_t *(*binding_trig_fold_fn)(
    expr_binding_expr_t *expr,
    const binding_trig_exact_rule_t *rule);

struct binding_trig_exact_rule_t {
    binding_trig_fold_fn fold;
    const number_t *number_value;
    unsigned long radicand;
    unsigned long denominator;
    unsigned long scale;
};

#define BINDING_TRIG_EXACT_TWELFTH_COUNT 24u
#define BINDING_TRIG_EXACT_INDEX_MISSING (-1)

static expr_binding_expr_t *binding_trig_fold_number(
    expr_binding_expr_t *expr,
    const binding_trig_exact_rule_t *rule)
{
    return binding_expr_fold_to_number_owned(expr, num_clone(*rule->number_value));
}

static expr_binding_expr_t *binding_trig_fold_neg_number(
    expr_binding_expr_t *expr,
    const binding_trig_exact_rule_t *rule)
{
    return binding_expr_fold_to_neg_number_owned(expr, *rule->number_value);
}

static expr_binding_expr_t *binding_trig_fold_sqrt(
    expr_binding_expr_t *expr,
    const binding_trig_exact_rule_t *rule)
{
    return binding_expr_fold_to_expr_owned(expr,
                                           binding_expr_sqrt_ulong(rule->radicand));
}

static expr_binding_expr_t *binding_trig_fold_neg_sqrt(
    expr_binding_expr_t *expr,
    const binding_trig_exact_rule_t *rule)
{
    return binding_expr_fold_to_expr_owned(expr,
                                           binding_expr_neg_sqrt_ulong(rule->radicand));
}

static expr_binding_expr_t *binding_trig_fold_sqrt_quotient(
    expr_binding_expr_t *expr,
    const binding_trig_exact_rule_t *rule)
{
    return binding_expr_fold_to_expr_owned(
        expr,
        binding_expr_sqrt_quotient_ulong(rule->radicand, rule->denominator));
}

static expr_binding_expr_t *binding_trig_fold_neg_sqrt_quotient(
    expr_binding_expr_t *expr,
    const binding_trig_exact_rule_t *rule)
{
    return binding_expr_fold_to_expr_owned(
        expr,
        binding_expr_neg_sqrt_quotient_ulong(rule->radicand, rule->denominator));
}

static expr_binding_expr_t *binding_expr_scaled_sqrt_quotient_ulong(
    unsigned long scale,
    unsigned long radicand,
    unsigned long denom)
{
    expr_binding_expr_t *out = expr_binding_expr_new_mul(
        binding_expr_new_ulong(scale),
        binding_expr_sqrt_quotient_ulong(radicand, denom));

    return expr_binding_expr_simplify(out);
}

static expr_binding_expr_t *binding_expr_neg_scaled_sqrt_quotient_ulong(
    unsigned long scale,
    unsigned long radicand,
    unsigned long denom)
{
    return expr_binding_expr_new_neg(
        binding_expr_scaled_sqrt_quotient_ulong(scale, radicand, denom));
}

static expr_binding_expr_t *binding_trig_fold_scaled_sqrt_quotient(
    expr_binding_expr_t *expr,
    const binding_trig_exact_rule_t *rule)
{
    return binding_expr_fold_to_expr_owned(
        expr,
        binding_expr_scaled_sqrt_quotient_ulong(rule->scale,
                                               rule->radicand,
                                               rule->denominator));
}

static expr_binding_expr_t *binding_trig_fold_neg_scaled_sqrt_quotient(
    expr_binding_expr_t *expr,
    const binding_trig_exact_rule_t *rule)
{
    return binding_expr_fold_to_expr_owned(
        expr,
        binding_expr_neg_scaled_sqrt_quotient_ulong(rule->scale,
                                                    rule->radicand,
                                                    rule->denominator));
}

static expr_binding_expr_t *binding_expr_new_pi_ratio_long(long numer,
                                                           unsigned long denom)
{
    expr_binding_expr_t *base = NULL;
    expr_binding_expr_t *out = NULL;
    bool negative = false;

    if (denom == 0u)
        return NULL;
    if (numer == 0L)
        return binding_expr_new_long(0L);
    if (numer < 0L) {
        negative = true;
        numer = -numer;
    }

    base = expr_binding_expr_new_const(EXPR_BINDING_CONST_PI);
    if (!base)
        return NULL;

    if (numer != 1L) {
        expr_binding_expr_t *coeff = binding_expr_new_long(numer);

        out = coeff ? expr_binding_expr_new_mul(coeff, base) : NULL;
        base = NULL;
        if (!out)
            return NULL;
        base = expr_binding_expr_simplify(out);
        out = NULL;
    }

    if (denom == 1u)
        return negative ? expr_binding_expr_new_neg(base) : base;

    out = expr_binding_expr_new_div(base, binding_expr_new_ulong(denom));
    base = NULL;
    out = expr_binding_expr_simplify(out);
    return negative ? expr_binding_expr_new_neg(out) : out;
}

static bool binding_inverse_trig_exact_ratio(const expr_ops_t *ops,
                                             const number_t *value,
                                             long *numer_out,
                                             unsigned long *denom_out)
{
    number_t three;
    number_t neg_half;
    number_t neg_sqrt2_over_two;
    number_t neg_sqrt3_over_two;
    number_t sqrt3_over_three;
    number_t neg_sqrt3_over_three;
    number_t two_sqrt3;
    number_t two_sqrt3_over_three;
    number_t neg_two_sqrt3_over_three;
    number_t neg_sqrt3;
    number_t neg_sqrt2;
    number_t neg_two;
    long numer = 0L;
    unsigned long denom = 1u;
    bool matched = false;

    if (!ops || !value || !numer_out || !denom_out)
        return false;

    three = num_create_from_long(3L);
    neg_half = num_neg(NUM_HALF);
    neg_sqrt2_over_two = num_neg(NUM_SQRT2_OVER_TWO);
    neg_sqrt3_over_two = num_neg(NUM_SQRT3_OVER_TWO);
    sqrt3_over_three = num_div(NUM_SQRT3, three);
    neg_sqrt3_over_three = num_neg(sqrt3_over_three);
    two_sqrt3 = num_mul(NUM_TWO, NUM_SQRT3);
    two_sqrt3_over_three = num_div(two_sqrt3, three);
    neg_two_sqrt3_over_three = num_neg(two_sqrt3_over_three);
    neg_sqrt3 = num_neg(NUM_SQRT3);
    neg_sqrt2 = num_neg(NUM_SQRT2);
    neg_two = num_neg(NUM_TWO);

    if (ops == &ops_asin) {
        if (num_eq(*value, NUM_NEG_ONE)) {
            numer = -1L; denom = 2u; matched = true;
        } else if (num_eq(*value, neg_half)) {
            numer = -1L; denom = 6u; matched = true;
        } else if (num_eq(*value, neg_sqrt2_over_two)) {
            numer = -1L; denom = 4u; matched = true;
        } else if (num_eq(*value, neg_sqrt3_over_two)) {
            numer = -1L; denom = 3u; matched = true;
        } else if (num_eq(*value, NUM_ZERO)) {
            numer = 0L; denom = 1u; matched = true;
        } else if (num_eq(*value, NUM_HALF)) {
            numer = 1L; denom = 6u; matched = true;
        } else if (num_eq(*value, NUM_SQRT2_OVER_TWO)) {
            numer = 1L; denom = 4u; matched = true;
        } else if (num_eq(*value, NUM_SQRT3_OVER_TWO)) {
            numer = 1L; denom = 3u; matched = true;
        } else if (num_eq(*value, NUM_ONE)) {
            numer = 1L; denom = 2u; matched = true;
        }
    } else if (ops == &ops_acos) {
        if (num_eq(*value, NUM_ONE)) {
            numer = 0L; denom = 1u; matched = true;
        } else if (num_eq(*value, NUM_SQRT3_OVER_TWO)) {
            numer = 1L; denom = 6u; matched = true;
        } else if (num_eq(*value, NUM_SQRT2_OVER_TWO)) {
            numer = 1L; denom = 4u; matched = true;
        } else if (num_eq(*value, NUM_HALF)) {
            numer = 1L; denom = 3u; matched = true;
        } else if (num_eq(*value, NUM_ZERO)) {
            numer = 1L; denom = 2u; matched = true;
        } else if (num_eq(*value, neg_half)) {
            numer = 2L; denom = 3u; matched = true;
        } else if (num_eq(*value, neg_sqrt2_over_two)) {
            numer = 3L; denom = 4u; matched = true;
        } else if (num_eq(*value, neg_sqrt3_over_two)) {
            numer = 5L; denom = 6u; matched = true;
        } else if (num_eq(*value, NUM_NEG_ONE)) {
            numer = 1L; denom = 1u; matched = true;
        }
    } else if (ops == &ops_atan) {
        if (num_eq(*value, neg_sqrt3)) {
            numer = -1L; denom = 3u; matched = true;
        } else if (num_eq(*value, NUM_NEG_ONE)) {
            numer = -1L; denom = 4u; matched = true;
        } else if (num_eq(*value, neg_sqrt3_over_three)) {
            numer = -1L; denom = 6u; matched = true;
        } else if (num_eq(*value, NUM_ZERO)) {
            numer = 0L; denom = 1u; matched = true;
        } else if (num_eq(*value, sqrt3_over_three)) {
            numer = 1L; denom = 6u; matched = true;
        } else if (num_eq(*value, NUM_ONE)) {
            numer = 1L; denom = 4u; matched = true;
        } else if (num_eq(*value, NUM_SQRT3)) {
            numer = 1L; denom = 3u; matched = true;
        }
    } else if (ops == &ops_asec) {
        if (num_eq(*value, NUM_ONE)) {
            numer = 0L; denom = 1u; matched = true;
        } else if (num_eq(*value, two_sqrt3_over_three)) {
            numer = 1L; denom = 6u; matched = true;
        } else if (num_eq(*value, NUM_SQRT2)) {
            numer = 1L; denom = 4u; matched = true;
        } else if (num_eq(*value, NUM_TWO)) {
            numer = 1L; denom = 3u; matched = true;
        } else if (num_is_inf(*value) && num_get_sign(*value) > 0) {
            numer = 1L; denom = 2u; matched = true;
        } else if (num_eq(*value, NUM_NEG_ONE)) {
            numer = 1L; denom = 1u; matched = true;
        } else if (num_eq(*value, neg_two_sqrt3_over_three)) {
            numer = 5L; denom = 6u; matched = true;
        } else if (num_eq(*value, neg_sqrt2)) {
            numer = 3L; denom = 4u; matched = true;
        } else if (num_eq(*value, neg_two)) {
            numer = 2L; denom = 3u; matched = true;
        } else if (num_is_inf(*value) && num_get_sign(*value) < 0) {
            numer = 1L; denom = 2u; matched = true;
        }
    } else if (ops == &ops_acosec) {
        if (num_eq(*value, NUM_NEG_ONE)) {
            numer = -1L; denom = 2u; matched = true;
        } else if (num_eq(*value, neg_two_sqrt3_over_three)) {
            numer = -1L; denom = 3u; matched = true;
        } else if (num_eq(*value, neg_sqrt2)) {
            numer = -1L; denom = 4u; matched = true;
        } else if (num_eq(*value, neg_two)) {
            numer = -1L; denom = 6u; matched = true;
        } else if (num_is_inf(*value)) {
            numer = 0L; denom = 1u; matched = true;
        } else if (num_eq(*value, NUM_TWO)) {
            numer = 1L; denom = 6u; matched = true;
        } else if (num_eq(*value, NUM_SQRT2)) {
            numer = 1L; denom = 4u; matched = true;
        } else if (num_eq(*value, two_sqrt3_over_three)) {
            numer = 1L; denom = 3u; matched = true;
        } else if (num_eq(*value, NUM_ONE)) {
            numer = 1L; denom = 2u; matched = true;
        }
    } else if (ops == &ops_acot) {
        if (num_is_inf(*value) && num_get_sign(*value) > 0) {
            numer = 0L; denom = 1u; matched = true;
        } else if (num_eq(*value, NUM_SQRT3)) {
            numer = 1L; denom = 6u; matched = true;
        } else if (num_eq(*value, NUM_ONE)) {
            numer = 1L; denom = 4u; matched = true;
        } else if (num_eq(*value, sqrt3_over_three)) {
            numer = 1L; denom = 3u; matched = true;
        } else if (num_eq(*value, NUM_ZERO)) {
            numer = 1L; denom = 2u; matched = true;
        } else if (num_eq(*value, neg_sqrt3_over_three)) {
            numer = 2L; denom = 3u; matched = true;
        } else if (num_eq(*value, NUM_NEG_ONE)) {
            numer = 3L; denom = 4u; matched = true;
        } else if (num_eq(*value, neg_sqrt3)) {
            numer = 5L; denom = 6u; matched = true;
        } else if (num_is_inf(*value) && num_get_sign(*value) < 0) {
            numer = 1L; denom = 1u; matched = true;
        }
    }

    num_destroy(&neg_two);
    num_destroy(&neg_sqrt2);
    num_destroy(&neg_sqrt3);
    num_destroy(&neg_two_sqrt3_over_three);
    num_destroy(&two_sqrt3_over_three);
    num_destroy(&two_sqrt3);
    num_destroy(&neg_sqrt3_over_three);
    num_destroy(&sqrt3_over_three);
    num_destroy(&neg_sqrt3_over_two);
    num_destroy(&neg_sqrt2_over_two);
    num_destroy(&neg_half);
    num_destroy(&three);

    if (!matched)
        return false;

    *numer_out = numer;
    *denom_out = denom;
    return true;
}

expr_binding_expr_t *binding_expr_try_simplify_asin_exact(expr_binding_expr_t *expr)
{
    long numer;
    unsigned long denom;
    expr_binding_expr_t *out;
    number_t value;

    if (!expr ||
        expr->kind != EXPR_BINDING_EXPR_UNARY_OP)
        return expr;

    value = expr_binding_expr_eval(expr->u.unary_op.child);
    if (!binding_inverse_trig_exact_ratio(expr->u.unary_op.ops,
                                          &value,
                                          &numer,
                                          &denom)) {
        num_destroy(&value);
        return expr;
    }
    num_destroy(&value);

    out = binding_expr_new_pi_ratio_long(numer, denom);
    return out ? binding_expr_fold_to_expr_owned(expr, out) : expr;
}

static const binding_trig_exact_rule_t s_binding_trig_exact_rules_sin[] = {
    { binding_trig_fold_number,            &NUM_ZERO,    0ul, 0ul, 0ul },
    { binding_trig_fold_number,            &NUM_HALF,    0ul, 0ul, 0ul },
    { binding_trig_fold_sqrt_quotient,     NULL,         2ul, 2ul, 0ul },
    { binding_trig_fold_sqrt_quotient,     NULL,         3ul, 2ul, 0ul },
    { binding_trig_fold_number,            &NUM_ONE,     0ul, 0ul, 0ul },
    { binding_trig_fold_sqrt_quotient,     NULL,         3ul, 2ul, 0ul },
    { binding_trig_fold_sqrt_quotient,     NULL,         2ul, 2ul, 0ul },
    { binding_trig_fold_number,            &NUM_HALF,    0ul, 0ul, 0ul },
    { binding_trig_fold_number,            &NUM_ZERO,    0ul, 0ul, 0ul },
    { binding_trig_fold_neg_number,        &NUM_HALF,    0ul, 0ul, 0ul },
    { binding_trig_fold_neg_sqrt_quotient, NULL,         2ul, 2ul, 0ul },
    { binding_trig_fold_neg_sqrt_quotient, NULL,         3ul, 2ul, 0ul },
    { binding_trig_fold_number,            &NUM_NEG_ONE, 0ul, 0ul, 0ul },
    { binding_trig_fold_neg_sqrt_quotient, NULL,         3ul, 2ul, 0ul },
    { binding_trig_fold_neg_sqrt_quotient, NULL,         2ul, 2ul, 0ul },
    { binding_trig_fold_neg_number,        &NUM_HALF,    0ul, 0ul, 0ul }
};

static const binding_trig_exact_rule_t s_binding_trig_exact_rules_cos[] = {
    { binding_trig_fold_number,            &NUM_ONE,     0ul, 0ul, 0ul },
    { binding_trig_fold_sqrt_quotient,     NULL,         3ul, 2ul, 0ul },
    { binding_trig_fold_sqrt_quotient,     NULL,         2ul, 2ul, 0ul },
    { binding_trig_fold_number,            &NUM_HALF,    0ul, 0ul, 0ul },
    { binding_trig_fold_number,            &NUM_ZERO,    0ul, 0ul, 0ul },
    { binding_trig_fold_neg_number,        &NUM_HALF,    0ul, 0ul, 0ul },
    { binding_trig_fold_neg_sqrt_quotient, NULL,         2ul, 2ul, 0ul },
    { binding_trig_fold_neg_sqrt_quotient, NULL,         3ul, 2ul, 0ul },
    { binding_trig_fold_number,            &NUM_NEG_ONE, 0ul, 0ul, 0ul },
    { binding_trig_fold_neg_sqrt_quotient, NULL,         3ul, 2ul, 0ul },
    { binding_trig_fold_neg_sqrt_quotient, NULL,         2ul, 2ul, 0ul },
    { binding_trig_fold_neg_number,        &NUM_HALF,    0ul, 0ul, 0ul },
    { binding_trig_fold_number,            &NUM_ZERO,    0ul, 0ul, 0ul },
    { binding_trig_fold_number,            &NUM_HALF,    0ul, 0ul, 0ul },
    { binding_trig_fold_sqrt_quotient,     NULL,         2ul, 2ul, 0ul },
    { binding_trig_fold_sqrt_quotient,     NULL,         3ul, 2ul, 0ul }
};

static const binding_trig_exact_rule_t s_binding_trig_exact_rules_tan[] = {
    { binding_trig_fold_number,            &NUM_ZERO,    0ul, 0ul, 0ul },
    { binding_trig_fold_sqrt_quotient,     NULL,         3ul, 3ul, 0ul },
    { binding_trig_fold_number,            &NUM_ONE,     0ul, 0ul, 0ul },
    { binding_trig_fold_sqrt,              NULL,         3ul, 0ul, 0ul },
    { binding_trig_fold_number,            &NUM_INF,     0ul, 0ul, 0ul },
    { binding_trig_fold_neg_sqrt,          NULL,         3ul, 0ul, 0ul },
    { binding_trig_fold_number,            &NUM_NEG_ONE, 0ul, 0ul, 0ul },
    { binding_trig_fold_neg_sqrt_quotient, NULL,         3ul, 3ul, 0ul },
    { binding_trig_fold_number,            &NUM_ZERO,    0ul, 0ul, 0ul },
    { binding_trig_fold_sqrt_quotient,     NULL,         3ul, 3ul, 0ul },
    { binding_trig_fold_number,            &NUM_ONE,     0ul, 0ul, 0ul },
    { binding_trig_fold_sqrt,              NULL,         3ul, 0ul, 0ul },
    { binding_trig_fold_number,            &NUM_NINF,    0ul, 0ul, 0ul },
    { binding_trig_fold_neg_sqrt,          NULL,         3ul, 0ul, 0ul },
    { binding_trig_fold_number,            &NUM_NEG_ONE, 0ul, 0ul, 0ul },
    { binding_trig_fold_neg_sqrt_quotient, NULL,         3ul, 3ul, 0ul }
};

static const binding_trig_exact_rule_t s_binding_trig_exact_rules_sec[] = {
    { binding_trig_fold_number,                   &NUM_ONE,     0ul, 0ul, 0ul },
    { binding_trig_fold_scaled_sqrt_quotient,     NULL,         3ul, 3ul, 2ul },
    { binding_trig_fold_sqrt,                     NULL,         2ul, 0ul, 0ul },
    { binding_trig_fold_number,                   &NUM_TWO,     0ul, 0ul, 0ul },
    { binding_trig_fold_number,                   &NUM_INF,     0ul, 0ul, 0ul },
    { binding_trig_fold_neg_number,               &NUM_TWO,     0ul, 0ul, 0ul },
    { binding_trig_fold_neg_sqrt,                 NULL,         2ul, 0ul, 0ul },
    { binding_trig_fold_neg_scaled_sqrt_quotient, NULL,         3ul, 3ul, 2ul },
    { binding_trig_fold_number,                   &NUM_NEG_ONE, 0ul, 0ul, 0ul },
    { binding_trig_fold_neg_scaled_sqrt_quotient, NULL,         3ul, 3ul, 2ul },
    { binding_trig_fold_neg_sqrt,                 NULL,         2ul, 0ul, 0ul },
    { binding_trig_fold_neg_number,               &NUM_TWO,     0ul, 0ul, 0ul },
    { binding_trig_fold_number,                   &NUM_INF,     0ul, 0ul, 0ul },
    { binding_trig_fold_number,                   &NUM_TWO,     0ul, 0ul, 0ul },
    { binding_trig_fold_sqrt,                     NULL,         2ul, 0ul, 0ul },
    { binding_trig_fold_scaled_sqrt_quotient,     NULL,         3ul, 3ul, 2ul }
};

static const binding_trig_exact_rule_t s_binding_trig_exact_rules_cosec[] = {
    { binding_trig_fold_number,                   &NUM_INF,     0ul, 0ul, 0ul },
    { binding_trig_fold_number,                   &NUM_TWO,     0ul, 0ul, 0ul },
    { binding_trig_fold_sqrt,                     NULL,         2ul, 0ul, 0ul },
    { binding_trig_fold_scaled_sqrt_quotient,     NULL,         3ul, 3ul, 2ul },
    { binding_trig_fold_number,                   &NUM_ONE,     0ul, 0ul, 0ul },
    { binding_trig_fold_scaled_sqrt_quotient,     NULL,         3ul, 3ul, 2ul },
    { binding_trig_fold_sqrt,                     NULL,         2ul, 0ul, 0ul },
    { binding_trig_fold_number,                   &NUM_TWO,     0ul, 0ul, 0ul },
    { binding_trig_fold_number,                   &NUM_INF,     0ul, 0ul, 0ul },
    { binding_trig_fold_neg_number,               &NUM_TWO,     0ul, 0ul, 0ul },
    { binding_trig_fold_neg_sqrt,                 NULL,         2ul, 0ul, 0ul },
    { binding_trig_fold_neg_scaled_sqrt_quotient, NULL,         3ul, 3ul, 2ul },
    { binding_trig_fold_number,                   &NUM_NEG_ONE, 0ul, 0ul, 0ul },
    { binding_trig_fold_neg_scaled_sqrt_quotient, NULL,         3ul, 3ul, 2ul },
    { binding_trig_fold_neg_sqrt,                 NULL,         2ul, 0ul, 0ul },
    { binding_trig_fold_neg_number,               &NUM_TWO,     0ul, 0ul, 0ul }
};

static const binding_trig_exact_rule_t s_binding_trig_exact_rules_cot[] = {
    { binding_trig_fold_number,            &NUM_INF,     0ul, 0ul, 0ul },
    { binding_trig_fold_sqrt,              NULL,         3ul, 0ul, 0ul },
    { binding_trig_fold_number,            &NUM_ONE,     0ul, 0ul, 0ul },
    { binding_trig_fold_sqrt_quotient,     NULL,         3ul, 3ul, 0ul },
    { binding_trig_fold_number,            &NUM_ZERO,    0ul, 0ul, 0ul },
    { binding_trig_fold_neg_sqrt_quotient, NULL,         3ul, 3ul, 0ul },
    { binding_trig_fold_number,            &NUM_NEG_ONE, 0ul, 0ul, 0ul },
    { binding_trig_fold_neg_sqrt,          NULL,         3ul, 0ul, 0ul },
    { binding_trig_fold_number,            &NUM_INF,     0ul, 0ul, 0ul },
    { binding_trig_fold_sqrt,              NULL,         3ul, 0ul, 0ul },
    { binding_trig_fold_number,            &NUM_ONE,     0ul, 0ul, 0ul },
    { binding_trig_fold_sqrt_quotient,     NULL,         3ul, 3ul, 0ul },
    { binding_trig_fold_number,            &NUM_ZERO,    0ul, 0ul, 0ul },
    { binding_trig_fold_neg_sqrt_quotient, NULL,         3ul, 3ul, 0ul },
    { binding_trig_fold_number,            &NUM_NEG_ONE, 0ul, 0ul, 0ul },
    { binding_trig_fold_neg_sqrt,          NULL,         3ul, 0ul, 0ul }
};

static const int8_t s_binding_trig_exact_index[BINDING_TRIG_EXACT_TWELFTH_COUNT] = {
    [0]  = 0,
    [1]  = BINDING_TRIG_EXACT_INDEX_MISSING,
    [2]  = 1,
    [3]  = 2,
    [4]  = 3,
    [5]  = BINDING_TRIG_EXACT_INDEX_MISSING,
    [6]  = 4,
    [7]  = BINDING_TRIG_EXACT_INDEX_MISSING,
    [8]  = 5,
    [9]  = 6,
    [10] = 7,
    [11] = BINDING_TRIG_EXACT_INDEX_MISSING,
    [12] = 8,
    [13] = BINDING_TRIG_EXACT_INDEX_MISSING,
    [14] = 9,
    [15] = 10,
    [16] = 11,
    [17] = BINDING_TRIG_EXACT_INDEX_MISSING,
    [18] = 12,
    [19] = BINDING_TRIG_EXACT_INDEX_MISSING,
    [20] = 13,
    [21] = 14,
    [22] = 15,
    [23] = BINDING_TRIG_EXACT_INDEX_MISSING
};

enum {
    BINDING_TRIG_EXACT_RULE_KIND_MIN = (int)EXPR_KIND_SIN,
    BINDING_TRIG_EXACT_RULE_KIND_MAX = (int)EXPR_KIND_COT,
    BINDING_TRIG_EXACT_RULE_KIND_COUNT =
        BINDING_TRIG_EXACT_RULE_KIND_MAX - BINDING_TRIG_EXACT_RULE_KIND_MIN + 1
};

static const binding_trig_exact_rule_t *const
    s_binding_trig_exact_rule_tables[BINDING_TRIG_EXACT_RULE_KIND_COUNT] = {
    [EXPR_KIND_SIN - BINDING_TRIG_EXACT_RULE_KIND_MIN] = s_binding_trig_exact_rules_sin,
    [EXPR_KIND_COS - BINDING_TRIG_EXACT_RULE_KIND_MIN] = s_binding_trig_exact_rules_cos,
    [EXPR_KIND_TAN - BINDING_TRIG_EXACT_RULE_KIND_MIN] = s_binding_trig_exact_rules_tan,
    [EXPR_KIND_SEC - BINDING_TRIG_EXACT_RULE_KIND_MIN] = s_binding_trig_exact_rules_sec,
    [EXPR_KIND_COSEC - BINDING_TRIG_EXACT_RULE_KIND_MIN] = s_binding_trig_exact_rules_cosec,
    [EXPR_KIND_COT - BINDING_TRIG_EXACT_RULE_KIND_MIN] = s_binding_trig_exact_rules_cot
};

static const binding_trig_exact_rule_t *binding_trig_exact_rule_lookup(
    const expr_ops_t *ops,
    long twelfths)
{
    const binding_trig_exact_rule_t *rules;
    int kind;
    int8_t index;

    kind = ops ? (int)ops->kind : -1;
    if (!ops ||
        twelfths < 0L || twelfths >= (long)BINDING_TRIG_EXACT_TWELFTH_COUNT ||
        kind < BINDING_TRIG_EXACT_RULE_KIND_MIN ||
        kind > BINDING_TRIG_EXACT_RULE_KIND_MAX)
        return NULL;

    rules = s_binding_trig_exact_rule_tables[
        kind - BINDING_TRIG_EXACT_RULE_KIND_MIN];
    if (!rules)
        return NULL;

    index = s_binding_trig_exact_index[twelfths];
    return index >= 0 ? &rules[index] : NULL;
}

static expr_binding_expr_t *binding_expr_fold_trig_rule_owned(
    expr_binding_expr_t *expr,
    const binding_trig_exact_rule_t *rule)
{
    return rule && rule->fold ? rule->fold(expr, rule) : expr;
}

expr_binding_expr_t *binding_expr_try_simplify_trig_exact(expr_binding_expr_t *expr)
{
    long twelfths;
    const binding_trig_exact_rule_t *rule;

    if (!expr || expr->kind != EXPR_BINDING_EXPR_UNARY_OP)
        return expr;

    if (binding_number_text_eq_long(expr->u.unary_op.child, 0L))
        twelfths = 0L;
    else if (!binding_expr_pi_ratio_twelfths(expr->u.unary_op.child, &twelfths))
        return expr;

    rule = binding_trig_exact_rule_lookup(expr->u.unary_op.ops, twelfths);
    if (rule)
        return binding_expr_fold_trig_rule_owned(expr, rule);

    return expr;
}

expr_binding_expr_t *binding_expr_try_simplify_direct_inverse(expr_binding_expr_t *expr)
{
    expr_binding_expr_t *inner;
    expr_binding_expr_t *out;

    if (!expr ||
        expr->kind != EXPR_BINDING_EXPR_UNARY_OP ||
        !expr->u.unary_op.ops)
        return expr;

    inner = expr->u.unary_op.child;
    if (!inner ||
        inner->kind != EXPR_BINDING_EXPR_UNARY_OP ||
        !expr_ops_are_direct_inverse_pair(expr->u.unary_op.ops,
                                        inner->u.unary_op.ops))
        return expr;

    out = inner->u.unary_op.child;
    inner->u.unary_op.child = NULL;
    expr->u.unary_op.child = NULL;
    expr_binding_expr_free(expr);
    expr_binding_expr_free(inner);
    return out;
}

static const expr_binding_expr_t *binding_expr_lambert_arg(const expr_binding_expr_t *expr)
{
    if (!expr ||
        expr->kind != EXPR_BINDING_EXPR_UNARY_OP ||
        !expr_ops_is_lambert(expr->u.unary_op.ops))
        return NULL;

    return expr->u.unary_op.child;
}

static bool binding_expr_lambert_args_match(const expr_binding_expr_t *left,
                                            const expr_binding_expr_t *right)
{
    number_t left_value;
    number_t right_value;
    bool equal;

    if (expr_binding_expr_struct_eq(left, right))
        return true;
    if (!left || !right)
        return false;

    left_value = expr_binding_expr_eval(left);
    right_value = expr_binding_expr_eval(right);
    equal = num_eq(left_value, right_value);
    num_destroy(&right_value);
    num_destroy(&left_value);
    return equal;
}

static bool binding_expr_extract_exp_arg(const expr_binding_expr_t *expr,
                                         expr_binding_expr_t **arg_out)
{
    if (!expr || !arg_out)
        return false;

    *arg_out = NULL;

    if (expr->kind == EXPR_BINDING_EXPR_UNARY_OP &&
        expr->u.unary_op.ops == &ops_exp) {
        *arg_out = expr_binding_expr_clone(expr->u.unary_op.child);
        return *arg_out != NULL;
    }

    if (binding_expr_is_const_id(expr, EXPR_BINDING_CONST_E)) {
        *arg_out = binding_expr_new_long(1L);
        return *arg_out != NULL;
    }

    if (expr->kind == EXPR_BINDING_EXPR_POWI &&
        binding_expr_is_const_id(expr->u.powi.base, EXPR_BINDING_CONST_E)) {
        *arg_out = binding_expr_new_long(expr->u.powi.exponent);
        return *arg_out != NULL;
    }

    if (expr->kind == EXPR_BINDING_EXPR_BINARY_OP &&
        expr->u.binary_op.ops == &ops_pow &&
        binding_expr_is_const_id(expr->u.binary_op.left, EXPR_BINDING_CONST_E)) {
        *arg_out = expr_binding_expr_clone(expr->u.binary_op.right);
        return *arg_out != NULL;
    }

    return false;
}

expr_binding_expr_t *binding_expr_try_simplify_integer_exp_power(expr_binding_expr_t *expr)
{
    expr_binding_expr_t *arg = NULL;
    expr_binding_expr_t *scaled_arg;
    expr_binding_expr_t *out;

    if (!expr ||
        expr->kind != EXPR_BINDING_EXPR_POWI ||
        !binding_expr_extract_exp_arg(expr->u.powi.base, &arg))
        return expr;

    scaled_arg = expr_binding_expr_simplify(
        expr_binding_expr_new_mul(binding_expr_new_long(expr->u.powi.exponent),
                                arg));
    out = expr_binding_expr_new_unary_op(&ops_exp, scaled_arg);
    return binding_expr_fold_to_expr_owned(expr, expr_binding_expr_simplify(out));
}

expr_binding_expr_t *binding_expr_try_simplify_exp_product(expr_binding_expr_t *expr)
{
    expr_binding_expr_t *left_arg = NULL;
    expr_binding_expr_t *right_arg = NULL;
    expr_binding_expr_t *sum;
    expr_binding_expr_t *out;

    if (!expr ||
        expr->kind != EXPR_BINDING_EXPR_MUL ||
        !binding_expr_extract_exp_arg(expr->u.binary.left, &left_arg))
        return expr;

    if (!binding_expr_extract_exp_arg(expr->u.binary.right, &right_arg)) {
        expr_binding_expr_free(left_arg);
        return expr;
    }

    sum = expr_binding_expr_simplify(expr_binding_expr_new_add(left_arg, right_arg));
    out = expr_binding_expr_new_unary_op(&ops_exp, sum);
    return binding_expr_fold_to_expr_owned(expr, expr_binding_expr_simplify(out));
}

static bool binding_expr_is_exp_of_same_lambert(const expr_binding_expr_t *expr,
                                                const expr_binding_expr_t *lambert_expr)
{
    expr_binding_expr_t *arg = NULL;
    bool match = false;

    if (!binding_expr_extract_exp_arg(expr, &arg))
        return false;

    match = expr_binding_expr_struct_eq(arg, lambert_expr);
    expr_binding_expr_free(arg);
    return match;
}

expr_binding_expr_t *binding_expr_try_simplify_lambert_product(expr_binding_expr_t *expr)
{
    const expr_binding_expr_t *arg = NULL;
    expr_binding_expr_t *out;

    if (!expr || expr->kind != EXPR_BINDING_EXPR_MUL)
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

    out = expr_binding_expr_clone(arg);
    if (!out)
        return expr;

    expr_binding_expr_free(expr);
    return out;
}

static bool binding_expr_extract_exp_factor(const expr_binding_expr_t *expr,
                                            expr_binding_expr_t **exp_arg_out,
                                            expr_binding_expr_t **other_out)
{
    expr_binding_expr_t *exp_arg = NULL;
    expr_binding_expr_t *other = NULL;

    if (!expr || !exp_arg_out || !other_out)
        return false;

    *exp_arg_out = NULL;
    *other_out = NULL;

    if (binding_expr_extract_exp_arg(expr, exp_arg_out)) {
        *other_out = binding_expr_new_long(1L);
        if (*other_out)
            return true;
        expr_binding_expr_free(*exp_arg_out);
        *exp_arg_out = NULL;
        return false;
    }

    if (expr->kind == EXPR_BINDING_EXPR_MUL) {
        if (binding_expr_extract_exp_arg(expr->u.binary.left, exp_arg_out)) {
            *other_out = expr_binding_expr_clone(expr->u.binary.right);
            if (*other_out)
                return true;
            expr_binding_expr_free(*exp_arg_out);
            *exp_arg_out = NULL;
            return false;
        }
        if (binding_expr_extract_exp_arg(expr->u.binary.right, exp_arg_out)) {
            *other_out = expr_binding_expr_clone(expr->u.binary.left);
            if (*other_out)
                return true;
            expr_binding_expr_free(*exp_arg_out);
            *exp_arg_out = NULL;
            return false;
        }
    }

    if (expr->kind == EXPR_BINDING_EXPR_DIV &&
        binding_expr_extract_exp_factor(expr->u.binary.left, &exp_arg, &other)) {
        *exp_arg_out = exp_arg;
        *other_out = expr_binding_expr_new_div(other,
                                             expr_binding_expr_clone(expr->u.binary.right));
        if (!*other_out) {
            expr_binding_expr_free(exp_arg);
            expr_binding_expr_free(other);
            *exp_arg_out = NULL;
            return false;
        }
        return true;
    }

    return false;
}

static expr_binding_expr_t *binding_expr_lambert_inverse_arg(const expr_binding_expr_t *expr)
{
    expr_binding_expr_t *exp_arg = NULL;
    expr_binding_expr_t *other = NULL;
    bool match = false;

    if (!binding_expr_extract_exp_factor(expr, &exp_arg, &other))
        return NULL;

    exp_arg = expr_binding_expr_simplify(exp_arg);
    other = expr_binding_expr_simplify(other);
    match = binding_expr_lambert_args_match(other, exp_arg);
    expr_binding_expr_free(other);
    if (match)
        return exp_arg;

    expr_binding_expr_free(exp_arg);
    return NULL;
}

static bool binding_expr_lambert_inverse_domain_ok(const expr_ops_t *ops,
                                                   const expr_binding_expr_t *arg)
{
    number_t value;
    bool ok = false;

    if (!ops || !arg)
        return false;

    value = expr_binding_expr_eval(arg);
    ok = expr_inverse_unary_candidate_value_ok(ops, value);
    num_destroy(&value);
    return ok;
}

expr_binding_expr_t *binding_expr_try_simplify_lambert_inverse(expr_binding_expr_t *expr)
{
    expr_binding_expr_t *arg;
    expr_binding_expr_t *out;

    if (!expr ||
        expr->kind != EXPR_BINDING_EXPR_UNARY_OP ||
        !expr_ops_is_lambert(expr->u.unary_op.ops))
        return expr;

    arg = binding_expr_lambert_inverse_arg(expr->u.unary_op.child);
    if (!arg ||
        !binding_expr_lambert_inverse_domain_ok(expr->u.unary_op.ops, arg)) {
        expr_binding_expr_free(arg);
        return expr;
    }

    out = arg;

    expr_binding_expr_free(expr);
    return out;
}

expr_binding_expr_t *binding_expr_try_simplify_complex_floor_ceil(expr_binding_expr_t *expr)
{
    number_t value;

    if (!expr ||
        expr->kind != EXPR_BINDING_EXPR_UNARY_OP ||
        !expr_ops_is_floor_or_ceil(expr->u.unary_op.ops))
        return expr;

    value = expr_binding_expr_eval(expr);
    if (num_is_finite(value) && !num_is_real(value))
        return binding_expr_fold_to_number_owned(expr, value);

    num_destroy(&value);
    return expr;
}

expr_binding_expr_t *binding_expr_try_simplify_e_power(expr_binding_expr_t *expr)
{
    expr_binding_expr_t *out;

    if (!expr ||
        expr->kind != EXPR_BINDING_EXPR_BINARY_OP ||
        expr->u.binary_op.ops != &ops_pow ||
        !binding_expr_is_const_id(expr->u.binary_op.left, EXPR_BINDING_CONST_E))
        return expr;

    out = expr_binding_expr_new_unary_op(
        &ops_exp,
        expr_binding_expr_clone(expr->u.binary_op.right));
    return binding_expr_fold_to_expr_owned(expr, expr_binding_expr_simplify(out));
}

expr_binding_expr_t *binding_expr_try_simplify_log_e(expr_binding_expr_t *expr)
{
    if (!expr ||
        expr->kind != EXPR_BINDING_EXPR_UNARY_OP ||
        expr->u.unary_op.ops != &ops_log ||
        !binding_expr_is_const_id(expr->u.unary_op.child, EXPR_BINDING_CONST_E))
        return expr;

    return binding_expr_fold_to_number_owned(expr, num_clone(NUM_ONE));
}

expr_binding_expr_t *binding_expr_try_simplify_log10_power(expr_binding_expr_t *expr)
{
    long exponent;

    if (!expr ||
        expr->kind != EXPR_BINDING_EXPR_UNARY_OP ||
        expr->u.unary_op.ops != &ops_log10 ||
        !binding_number_text_log10_power_exponent(expr->u.unary_op.child,
                                                  &exponent))
        return expr;

    return binding_expr_fold_to_expr_owned(expr,
                                           binding_expr_new_long(exponent));
}

expr_binding_expr_t *binding_expr_try_fold_neg_leading_number(expr_binding_expr_t *expr)
{
    number_t coeff;
    number_t neg;
    expr_binding_expr_t *rest = NULL;

    if (!expr || expr->kind != EXPR_BINDING_EXPR_NEG)
        return expr;

    if (!expr_binding_expr_split_leading_number(expr->u.unary.child,
                                              &coeff,
                                              &rest))
        return expr;

    neg = num_scope_detach(num_neg(coeff));
    num_destroy(&coeff);
    expr_binding_expr_free(expr);
    return binding_expr_scaled_product_owned(neg, rest, NULL);
}

expr_binding_expr_t *binding_expr_try_fold_mul_leading_numbers(expr_binding_expr_t *expr)
{
    number_t left_coeff;
    number_t right_coeff;
    number_t folded;
    expr_binding_expr_t *left_rest = NULL;
    expr_binding_expr_t *right_rest = NULL;

    if (!expr || expr->kind != EXPR_BINDING_EXPR_MUL)
        return expr;

    if (!expr_binding_expr_split_leading_number(expr->u.binary.left,
                                              &left_coeff,
                                              &left_rest))
        return expr;

    if (!expr_binding_expr_split_leading_number(expr->u.binary.right,
                                              &right_coeff,
                                              &right_rest)) {
        num_destroy(&left_coeff);
        expr_binding_expr_free(left_rest);
        return expr;
    }

    folded = num_scope_detach(num_mul(left_coeff, right_coeff));
    num_destroy(&right_coeff);
    num_destroy(&left_coeff);
    expr_binding_expr_free(expr);
    return binding_expr_scaled_product_owned(folded, left_rest, right_rest);
}

expr_binding_expr_t *binding_expr_try_fold_div_leading_number(expr_binding_expr_t *expr)
{
    number_t left_coeff;
    number_t right_coeff;
    number_t folded;
    expr_binding_expr_t *left_rest = NULL;

    if (!expr ||
        expr->kind != EXPR_BINDING_EXPR_DIV ||
        !expr_binding_expr_number_value(expr->u.binary.right, &right_coeff))
        return expr;

    if (!expr_binding_expr_split_leading_number(expr->u.binary.left,
                                              &left_coeff,
                                              &left_rest)) {
        num_destroy(&right_coeff);
        return expr;
    }

    folded = num_scope_detach(num_div(left_coeff, right_coeff));
    num_destroy(&left_coeff);
    num_destroy(&right_coeff);
    expr_binding_expr_free(expr);
    return binding_expr_scaled_product_owned(folded, left_rest, NULL);
}
