#include <stdbool.h>
#include <stdlib.h>

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"
#define MARS_SHARED_NUMBER_INTERNAL_ACCESS
#include "internal/number_internal.h"

static bool expr_beautify_is_one(const expr_t *expr)
{
    number_t value = number_invalid();
    bool one = expr_match_const_value(expr, &value) && num_eq(value, NUM_ONE);

    num_destroy(&value);
    return one;
}

static expr_t *expr_beautify_symmetric_square_root(const expr_t *expr)
{
    const expr_t *left;
    const expr_t *right;
    const expr_t *argument;
    number_t value = number_invalid();
    expr_t *ordered = NULL;
    expr_t *rewrite = NULL;

    if (!expr_is_sqrt_expr(expr) || !(argument = expr->a) ||
        !expr_match_binary_op(argument, EXPR_KIND_ADD, &left, &right) || !expr_match_const_value(left, &value) ||
        !num_is_real(value) || num_sign(value) <= 0 || !expr_is_sqrt_expr(right))
        goto cleanup;
    ordered = expr_add(right, left);
    rewrite = ordered ? expr_sqrt(ordered) : NULL;

cleanup:
    expr_free(ordered);
    num_destroy(&value);
    return rewrite;
}

static expr_t *expr_beautify_radical_sum(const expr_t *expr)
{
    const expr_t *left;
    const expr_t *right;
    number_t value = number_invalid();
    expr_t *rewrite = NULL;

    if (expr_match_binary_op(expr, EXPR_KIND_ADD, &left, &right) && expr_is_sqrt_expr(left) &&
        expr_match_const_value(right, &value) && num_is_real(value) && num_sign(value) > 0)
        rewrite = expr_add(right, left);
    num_destroy(&value);
    return rewrite;
}

static expr_t *expr_beautify_negative_square_root(const expr_t *expr)
{
    number_t value = number_invalid();
    expr_t *rewrite = NULL;

    if (expr_is_sqrt_expr(expr) && expr_match_const_value(expr->a, &value) && num_is_exact(value) &&
        num_is_real(value) && num_sign(value) < 0) {
        number_t magnitude = num_neg(value);
        expr_t *argument = expr_new_const(magnitude);
        expr_t *root = argument ? expr_sqrt(argument) : NULL;
        expr_t *imaginary_unit = expr_new_const(NUM_I);

        rewrite = root && imaginary_unit ? expr_mul(imaginary_unit, root) : NULL;
        expr_free(imaginary_unit);
        expr_free(root);
        expr_free(argument);
        num_destroy(&magnitude);
    }
    num_destroy(&value);
    return rewrite;
}

static expr_t *expr_beautify_expand_preserved(const expr_t *expr)
{
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *expanded = NULL;
    expr_t *rebuilt = NULL;

    if (!expr)
        return NULL;
    if (expr->ops && expr->ops->arity == EXPR_OP_ATOM) {
        if (expr->binding_expr && !expr_binding_expr_is_numeric_literal(expr->binding_expr) &&
            expr->binding_expr->kind != EXPR_BINDING_EXPR_CONST) {
            expanded = expr_binding_expr_eval_expr(expr->binding_expr);
            rebuilt = expanded ? expr_beautify_expand_preserved(expanded) : NULL;
            expr_free(expanded);
            return rebuilt;
        }
        return expr_clone(expr);
    }
    if (!expr->ops)
        return expr_clone(expr);
    left = expr_beautify_expand_preserved(expr->a);
    if (!left)
        goto cleanup;
    if (expr->ops->arity == EXPR_OP_BINARY) {
        right = expr_beautify_expand_preserved(expr->b);
        if (!right)
            goto cleanup;
        rebuilt = expr_new_binary_internal(expr->ops, left, right);
    } else if (expr_is_pow_d_expr(expr)) {
        rebuilt = expr_new_pow_const_internal(left, expr->c);
    } else {
        rebuilt = expr_new_unary_internal(expr->ops, left);
    }
    if (rebuilt) {
        left = NULL;
        right = NULL;
    }

cleanup:
    expr_free(right);
    expr_free(left);
    return rebuilt;
}

static bool expr_beautify_manifestly_real(const expr_t *expr)
{
    number_t value = number_invalid();
    bool real;

    if (!expr)
        return false;
    if (expr_match_const_value(expr, &value)) {
        real = num_is_real(value);
        num_destroy(&value);
        return real;
    }
    if (expr_is_sqrt_expr(expr))
        return expr_beautify_manifestly_real(expr->a);
    if (!expr->ops)
        return false;
    switch (expr->ops->kind) {
        case EXPR_KIND_ADD:
        case EXPR_KIND_SUB:
        case EXPR_KIND_MUL:
        case EXPR_KIND_DIV:
            return expr_beautify_manifestly_real(expr->a) && expr_beautify_manifestly_real(expr->b);

        case EXPR_KIND_NEG:
            return expr_beautify_manifestly_real(expr->a);

        default:
            return false;
    }
}

static bool expr_beautify_cartesian_parts(const expr_t *expr, expr_t **real_out, expr_t **imaginary_out,
                                           bool *has_imaginary_out)
{
    const expr_t *left;
    const expr_t *right;
    number_t value = number_invalid();
    expr_t *left_real = NULL;
    expr_t *left_imaginary = NULL;
    expr_t *right_real = NULL;
    expr_t *right_imaginary = NULL;
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    bool left_has_imaginary = false;
    bool right_has_imaginary = false;
    bool ok = false;

    if (!expr || !real_out || !imaginary_out || !has_imaginary_out)
        goto cleanup;
    if (expr_match_const_value(expr, &value) && num_eq(value, NUM_I)) {
        real = expr_const_zero();
        imaginary = expr_const_one();
        *has_imaginary_out = true;
        goto success;
    }
    num_destroy(&value);
    value = number_invalid();
    if (expr_beautify_manifestly_real(expr)) {
        real = expr_clone(expr);
        imaginary = expr_const_zero();
        *has_imaginary_out = false;
        goto success;
    }
    if (expr->ops && expr->ops->kind == EXPR_KIND_NEG) {
        left = expr->a;
        if (!expr_beautify_cartesian_parts(left, &real, &imaginary, has_imaginary_out))
            goto cleanup;
        real = expr_negate_owned(real);
        imaginary = expr_negate_owned(imaginary);
        goto success;
    }
    if (expr_match_binary_op(expr, EXPR_KIND_ADD, &left, &right) ||
        expr_match_binary_op(expr, EXPR_KIND_SUB, &left, &right)) {
        bool subtract = expr_match_binary_op(expr, EXPR_KIND_SUB, &left, &right);

        if (!expr_beautify_cartesian_parts(left, &left_real, &left_imaginary, &left_has_imaginary) ||
            !expr_beautify_cartesian_parts(right, &right_real, &right_imaginary, &right_has_imaginary))
            goto cleanup;
        real = subtract ? expr_sub_simplify_owned(left_real, right_real)
                        : expr_add_simplify_owned(left_real, right_real);
        left_real = NULL;
        right_real = NULL;
        imaginary = subtract ? expr_sub_simplify_owned(left_imaginary, right_imaginary)
                             : expr_add_simplify_owned(left_imaginary, right_imaginary);
        left_imaginary = NULL;
        right_imaginary = NULL;
        *has_imaginary_out = left_has_imaginary || right_has_imaginary;
        goto success;
    }
    if (expr_match_binary_op(expr, EXPR_KIND_MUL, &left, &right)) {
        expr_t *ac;
        expr_t *bd;
        expr_t *ad;
        expr_t *bc;

        if (!expr_beautify_cartesian_parts(left, &left_real, &left_imaginary, &left_has_imaginary) ||
            !expr_beautify_cartesian_parts(right, &right_real, &right_imaginary, &right_has_imaginary))
            goto cleanup;
        ac = expr_mul(left_real, right_real);
        bd = expr_mul(left_imaginary, right_imaginary);
        ad = expr_mul(left_real, right_imaginary);
        bc = expr_mul(left_imaginary, right_real);
        real = expr_sub_simplify_owned(ac, bd);
        imaginary = expr_add_simplify_owned(ad, bc);
        *has_imaginary_out = left_has_imaginary || right_has_imaginary;
        goto success;
    }
    if (expr_match_binary_op(expr, EXPR_KIND_DIV, &left, &right) && expr_beautify_manifestly_real(right)) {
        if (!expr_beautify_cartesian_parts(left, &left_real, &left_imaginary, &left_has_imaginary))
            goto cleanup;
        real = expr_div_simplify_owned(left_real, expr_clone(right));
        left_real = NULL;
        imaginary = expr_div_simplify_owned(left_imaginary, expr_clone(right));
        left_imaginary = NULL;
        *has_imaginary_out = left_has_imaginary;
        goto success;
    }
    goto cleanup;

success:
    if (!real || !imaginary)
        goto cleanup;
    *real_out = real;
    *imaginary_out = imaginary;
    real = NULL;
    imaginary = NULL;
    ok = true;

cleanup:
    expr_free(imaginary);
    expr_free(real);
    expr_free(right_imaginary);
    expr_free(right_real);
    expr_free(left_imaginary);
    expr_free(left_real);
    num_destroy(&value);
    return ok;
}

static bool expr_beautify_integer_coefficient(const expr_t *expr, long *coefficient_out, expr_t **rest_out)
{
    const expr_t *left;
    const expr_t *right;
    number_t value = number_invalid();
    long coefficient;
    long denominator;

    if (!expr || !coefficient_out || !rest_out)
        return false;
    if (expr->ops && expr->ops->kind == EXPR_KIND_NEG) {
        if (!expr_beautify_integer_coefficient(expr->a, &coefficient, rest_out))
            return false;
        *coefficient_out = -coefficient;
        return true;
    }
    if (expr_match_binary_op(expr, EXPR_KIND_MUL, &left, &right)) {
        if (expr_match_const_value(left, &value) && num_get_small_rational(value, &coefficient, &denominator) &&
            denominator == 1L) {
            *coefficient_out = coefficient;
            *rest_out = expr_clone(right);
            num_destroy(&value);
            return *rest_out != NULL;
        }
        num_destroy(&value);
        value = number_invalid();
        if (expr_match_const_value(right, &value) && num_get_small_rational(value, &coefficient, &denominator) &&
            denominator == 1L) {
            *coefficient_out = coefficient;
            *rest_out = expr_clone(left);
            num_destroy(&value);
            return *rest_out != NULL;
        }
        num_destroy(&value);
        value = number_invalid();
        if (expr_beautify_integer_coefficient(left, &coefficient, rest_out) && coefficient != 1L) {
            expr_t *combined = expr_mul(*rest_out, right);

            expr_free(*rest_out);
            *rest_out = combined;
            *coefficient_out = coefficient;
            return combined != NULL;
        }
        expr_free(*rest_out);
        *rest_out = NULL;
        if (expr_beautify_integer_coefficient(right, &coefficient, rest_out) && coefficient != 1L) {
            expr_t *combined = expr_mul(left, *rest_out);

            expr_free(*rest_out);
            *rest_out = combined;
            *coefficient_out = coefficient;
            return combined != NULL;
        }
        expr_free(*rest_out);
        *rest_out = NULL;
    }
    num_destroy(&value);
    *coefficient_out = 1L;
    *rest_out = expr_clone(expr);
    return *rest_out != NULL;
}

static long expr_beautify_gcd(long left, long right)
{
    left = labs(left);
    right = labs(right);
    while (right != 0L) {
        long remainder = left % right;

        left = right;
        right = remainder;
    }
    return left;
}

static expr_t *expr_beautify_scale_integer_first(const expr_t *expr, long coefficient)
{
    number_t value;
    expr_t *constant;
    expr_t *scaled;

    if (coefficient == 1L)
        return expr_clone(expr);
    value = num_create_from_long(coefficient);
    constant = expr_new_const(value);
    num_destroy(&value);
    scaled = constant ? expr_mul(constant, expr) : NULL;
    expr_free(constant);
    return scaled;
}

static expr_t *expr_beautify_factor_integer_sum(const expr_t *expr)
{
    const expr_t *left;
    const expr_t *right;
    expr_t *left_rest = NULL;
    expr_t *right_rest = NULL;
    expr_t *left_scaled = NULL;
    expr_t *right_scaled = NULL;
    expr_t *inner = NULL;
    expr_t *factored = NULL;
    long left_coefficient;
    long right_coefficient;
    long common;
    bool subtract;

    if (!expr_match_binary_op(expr, EXPR_KIND_ADD, &left, &right) &&
        !expr_match_binary_op(expr, EXPR_KIND_SUB, &left, &right))
        return NULL;
    subtract = expr_match_binary_op(expr, EXPR_KIND_SUB, &left, &right);
    if (!expr_beautify_integer_coefficient(left, &left_coefficient, &left_rest) ||
        !expr_beautify_integer_coefficient(right, &right_coefficient, &right_rest))
        goto cleanup;
    common = expr_beautify_gcd(left_coefficient, right_coefficient);
    if (common <= 1L)
        goto cleanup;
    left_scaled = expr_beautify_scale_integer_first(left_rest, left_coefficient / common);
    right_scaled = expr_beautify_scale_integer_first(right_rest, right_coefficient / common);
    inner = left_scaled && right_scaled
                ? (subtract ? expr_sub(left_scaled, right_scaled) : expr_add(left_scaled, right_scaled))
                : NULL;
    factored = inner ? expr_mul_long(inner, common) : NULL;

cleanup:
    expr_free(inner);
    expr_free(right_scaled);
    expr_free(left_scaled);
    expr_free(right_rest);
    expr_free(left_rest);
    return factored;
}

static bool expr_beautify_common_product_factor(const expr_t *left_expr, const expr_t *right_expr,
                                                 expr_t **factor_out, expr_t **left_rest_out,
                                                 expr_t **right_rest_out)
{
    const expr_t *left_a;
    const expr_t *left_b;
    const expr_t *right_a;
    const expr_t *right_b;
    bool negate_right = false;

    if (!left_expr || !right_expr || !factor_out || !left_rest_out || !right_rest_out)
        return false;
    if (right_expr->ops && right_expr->ops->kind == EXPR_KIND_NEG) {
        right_expr = right_expr->a;
        negate_right = true;
    }
    if (!expr_match_binary_op(left_expr, EXPR_KIND_MUL, &left_a, &left_b) ||
        !expr_match_binary_op(right_expr, EXPR_KIND_MUL, &right_a, &right_b))
        return false;
    if (expr_struct_eq(left_a, right_a)) {
        *factor_out = expr_clone(left_a);
        *left_rest_out = expr_clone(left_b);
        *right_rest_out = expr_clone(right_b);
    } else if (expr_struct_eq(left_a, right_b)) {
        *factor_out = expr_clone(left_a);
        *left_rest_out = expr_clone(left_b);
        *right_rest_out = expr_clone(right_a);
    } else if (expr_struct_eq(left_b, right_a)) {
        *factor_out = expr_clone(left_b);
        *left_rest_out = expr_clone(left_a);
        *right_rest_out = expr_clone(right_b);
    } else if (expr_struct_eq(left_b, right_b)) {
        *factor_out = expr_clone(left_b);
        *left_rest_out = expr_clone(left_a);
        *right_rest_out = expr_clone(right_a);
    } else {
        return false;
    }
    if (negate_right)
        *right_rest_out = expr_negate_owned(*right_rest_out);
    return *factor_out && *left_rest_out && *right_rest_out;
}

static expr_t *expr_beautify_cartesian_product(const expr_t *expr)
{
    const expr_t *left;
    const expr_t *right;
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    expr_t *factored_real = NULL;
    expr_t *factored_imaginary = NULL;
    expr_t *real_rest = NULL;
    expr_t *imaginary_rest = NULL;
    expr_t *common_factor = NULL;
    expr_t *imaginary_unit = NULL;
    expr_t *inner = NULL;
    expr_t *imaginary_term = NULL;
    expr_t *rewrite = NULL;
    bool has_imaginary = false;
    long real_coefficient = 1L;
    long imaginary_coefficient = 1L;
    long common_coefficient = 1L;

    if (!expr_match_binary_op(expr, EXPR_KIND_MUL, &left, &right) ||
        !expr_beautify_cartesian_parts(expr, &real, &imaginary, &has_imaginary) || !has_imaginary)
        goto cleanup;
    factored_real = expr_factor_common_post_calculus(real);
    factored_imaginary = expr_factor_common_post_calculus(imaginary);
    if (factored_real) {
        expr_t *integer_factored = expr_beautify_factor_integer_sum(factored_real);

        if (integer_factored) {
            expr_free(factored_real);
            factored_real = integer_factored;
        }
    }
    if (factored_imaginary) {
        expr_t *integer_factored = expr_beautify_factor_integer_sum(factored_imaginary);

        if (integer_factored) {
            expr_free(factored_imaginary);
            factored_imaginary = integer_factored;
        }
    }
    if (factored_real && factored_imaginary &&
        expr_beautify_integer_coefficient(factored_real, &real_coefficient, &real_rest) &&
        expr_beautify_integer_coefficient(factored_imaginary, &imaginary_coefficient, &imaginary_rest) &&
        labs(real_coefficient) > 1L && labs(real_coefficient) == labs(imaginary_coefficient)) {
        common_coefficient = labs(real_coefficient);
        if (real_coefficient < 0)
            real_rest = expr_negate_owned(real_rest);
        if (imaginary_coefficient < 0)
            imaginary_rest = expr_negate_owned(imaginary_rest);
        imaginary_unit = expr_new_const(NUM_I);
        imaginary_term = imaginary_unit ? expr_mul(imaginary_unit, imaginary_rest) : NULL;
        inner = imaginary_term ? expr_add(real_rest, imaginary_term) : NULL;
        rewrite = inner ? expr_mul_long(inner, common_coefficient) : NULL;
        goto cleanup;
    }
    expr_free(real_rest);
    real_rest = NULL;
    expr_free(imaginary_rest);
    imaginary_rest = NULL;
    if (factored_real && factored_imaginary &&
        expr_beautify_common_product_factor(factored_real, factored_imaginary, &common_factor, &real_rest,
                                             &imaginary_rest)) {
        imaginary_unit = expr_new_const(NUM_I);
        imaginary_term = imaginary_unit ? expr_mul(imaginary_unit, imaginary_rest) : NULL;
        inner = imaginary_term ? expr_add(real_rest, imaginary_term) : NULL;
        rewrite = common_factor && inner ? expr_mul(common_factor, inner) : NULL;
        goto cleanup;
    }
    if (expr_is_exact_zero(factored_imaginary)) {
        rewrite = expr_clone(factored_real);
    } else {
        expr_t *imaginary_unit = expr_new_const(NUM_I);

        imaginary_term = imaginary_unit ? expr_mul(imaginary_unit, factored_imaginary) : NULL;
        expr_free(imaginary_unit);
        rewrite = imaginary_term ? expr_add(factored_real, imaginary_term) : NULL;
    }

cleanup:
    expr_free(inner);
    expr_free(common_factor);
    expr_free(imaginary_unit);
    expr_free(imaginary_rest);
    expr_free(real_rest);
    expr_free(factored_imaginary);
    expr_free(factored_real);
    expr_free(imaginary_term);
    expr_free(imaginary);
    expr_free(real);
    return rewrite;
}

static expr_t *expr_beautify_pure_imaginary_square(const expr_t *expr)
{
    const expr_t *left;
    const expr_t *right;
    number_t value = number_invalid();
    number_t real = number_invalid();
    number_t imaginary = number_invalid();
    expr_t *square = NULL;

    if (expr_is_sqrt_expr(expr) && expr_match_const_value(expr->a, &value) && num_is_exact(value) &&
        num_is_real(value) && num_sign(value) < 0) {
        number_t magnitude = num_neg(value);

        square = expr_new_const(magnitude);
        num_destroy(&magnitude);
        goto cleanup;
    }
    if (expr_match_const_value(expr, &value) && num_is_exact(value)) {
        real = num_real_part(value);
        imaginary = num_imag_part(value);
        if (num_is_zero(real) && !num_is_zero(imaginary)) {
            number_t magnitude = num_mul(imaginary, imaginary);

            square = expr_new_const(magnitude);
            num_destroy(&magnitude);
        }
        goto cleanup;
    }
    if (expr_match_binary_op(expr, EXPR_KIND_MUL, &left, &right)) {
        expr_t *imaginary_square = expr_beautify_pure_imaginary_square(left);
        const expr_t *real_factor = right;
        number_t factor_value = number_invalid();

        if (!imaginary_square) {
            imaginary_square = expr_beautify_pure_imaginary_square(right);
            real_factor = left;
        }
        if (imaginary_square && expr_match_const_value(real_factor, &factor_value) && num_is_exact(factor_value) &&
            num_is_real(factor_value)) {
            expr_t *real_square = expr_pow_long(real_factor, 2L);

            square = real_square ? expr_mul_simplify_owned(imaginary_square, real_square) : NULL;
            if (square)
                imaginary_square = NULL;
        }
        expr_free(imaginary_square);
        num_destroy(&factor_value);
    }

cleanup:
    num_destroy(&imaginary);
    num_destroy(&real);
    num_destroy(&value);
    return square;
}

static expr_t *expr_beautify_complex_square_root_reciprocal(const expr_t *expr)
{
    const expr_t *numerator;
    const expr_t *denominator;
    const expr_t *argument;
    const expr_t *real_term;
    const expr_t *imaginary_term;
    expr_t *owned_numerator = NULL;
    number_t exponent = number_invalid();
    long exponent_numerator;
    long exponent_denominator;
    number_t real_value = number_invalid();
    number_t imaginary_value = number_invalid();
    number_t imaginary_part = number_invalid();
    number_t imaginary_real_part = number_invalid();
    number_t norm_value = number_invalid();
    expr_t *imaginary_square = NULL;
    expr_t *real_square = NULL;
    expr_t *norm_square = NULL;
    expr_t *norm_root = NULL;
    expr_t *real_norm_root = NULL;
    expr_t *imaginary_norm_root = NULL;
    expr_t *real_argument = NULL;
    expr_t *imaginary_argument = NULL;
    expr_t *real_root = NULL;
    expr_t *imaginary_root = NULL;
    expr_t *imaginary_unit = NULL;
    expr_t *scaled_imaginary = NULL;
    expr_t *symmetric_numerator = NULL;
    expr_t *scaled_norm_square = NULL;
    expr_t *symmetric_denominator = NULL;
    expr_t *scaled_numerator = NULL;
    expr_t *rewrite = NULL;

    if (expr_match_binary_op(expr, EXPR_KIND_DIV, &numerator, &denominator) && expr_is_sqrt_expr(denominator)) {
        argument = denominator->a;
    } else if (expr_match_pow_const(expr, &argument, &exponent) &&
               num_get_small_rational(exponent, &exponent_numerator, &exponent_denominator) &&
               exponent_numerator == -1L && exponent_denominator == 2L) {
        owned_numerator = expr_const_one();
        numerator = owned_numerator;
    } else {
        goto cleanup;
    }
    if (!argument || !expr_match_binary_op(argument, EXPR_KIND_ADD, &real_term, &imaginary_term) ||
        expr_has_unbound_parameters(argument, 0u, NULL))
        goto cleanup;
    if (!expr_match_const_value(real_term, &real_value)) {
        const expr_t *swap = real_term;

        real_term = imaginary_term;
        imaginary_term = swap;
        if (!expr_match_const_value(real_term, &real_value))
            goto cleanup;
    }
    if (!num_is_exact(real_value) || !num_is_real(real_value) || num_sign(real_value) <= 0)
        goto cleanup;

    imaginary_value = expr_eval(imaginary_term);
    imaginary_part = num_imag_part(imaginary_value);
    imaginary_real_part = num_real_part(imaginary_value);
    if (!num_is_zero(imaginary_real_part) || num_is_zero(imaginary_part))
        goto cleanup;

    imaginary_square = expr_beautify_pure_imaginary_square(imaginary_term);
    if (!imaginary_square) {
        expr_t *raw_square = expr_pow_long(imaginary_term, 2L);
        expr_t *negative_square = raw_square ? expr_neg(raw_square) : NULL;

        imaginary_square = expr_simplify_owned(negative_square);
        expr_free(raw_square);
    }
    real_square = expr_simplify_owned(expr_pow_long(real_term, 2L));
    norm_square = imaginary_square && real_square ? expr_add_simplify_owned(real_square, imaginary_square) : NULL;
    if (norm_square) {
        real_square = NULL;
        imaginary_square = NULL;
    }
    if (!norm_square || !expr_match_const_value(norm_square, &norm_value) || !num_is_real(norm_value) ||
        num_sign(norm_value) <= 0)
        goto cleanup;

    norm_root = expr_sqrt(norm_square);
    real_norm_root = norm_root ? expr_clone(norm_root) : NULL;
    imaginary_norm_root = norm_root ? expr_clone(norm_root) : NULL;
    real_argument = real_norm_root ? expr_add(real_norm_root, real_term) : NULL;
    imaginary_argument = imaginary_norm_root ? expr_sub(imaginary_norm_root, real_term) : NULL;
    real_root = real_argument ? expr_sqrt(real_argument) : NULL;
    imaginary_root = imaginary_argument ? expr_sqrt(imaginary_argument) : NULL;
    imaginary_unit = expr_new_const(NUM_I);
    scaled_imaginary = imaginary_root && imaginary_unit ? expr_mul(imaginary_unit, imaginary_root) : NULL;
    if (real_root && scaled_imaginary) {
        symmetric_numerator = num_sign(imaginary_part) > 0 ? expr_sub(real_root, scaled_imaginary)
                                                          : expr_add(real_root, scaled_imaginary);
    }
    scaled_norm_square = expr_simplify_owned(expr_mul_long(norm_square, 2L));
    symmetric_denominator = scaled_norm_square ? expr_sqrt(scaled_norm_square) : NULL;
    scaled_numerator = symmetric_numerator
                           ? (expr_beautify_is_one(numerator) ? expr_clone(symmetric_numerator)
                                                             : expr_mul(numerator, symmetric_numerator))
                           : NULL;
    rewrite = scaled_numerator && symmetric_denominator ? expr_div(scaled_numerator, symmetric_denominator) : NULL;

cleanup:
    num_destroy(&exponent);
    expr_free(owned_numerator);
    expr_free(scaled_numerator);
    expr_free(symmetric_denominator);
    expr_free(scaled_norm_square);
    expr_free(symmetric_numerator);
    expr_free(scaled_imaginary);
    expr_free(imaginary_unit);
    expr_free(imaginary_root);
    expr_free(real_root);
    expr_free(imaginary_argument);
    expr_free(real_argument);
    expr_free(imaginary_norm_root);
    expr_free(real_norm_root);
    expr_free(norm_root);
    expr_free(norm_square);
    expr_free(real_square);
    expr_free(imaginary_square);
    num_destroy(&imaginary_part);
    num_destroy(&imaginary_real_part);
    num_destroy(&norm_value);
    num_destroy(&imaginary_value);
    num_destroy(&real_value);
    return rewrite;
}

static expr_t *expr_beautify_node(const expr_t *expr, bool rewrite_negative_roots)
{
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *rebuilt = NULL;
    expr_t *rewrite = NULL;

    if (!expr)
        return NULL;
    if (!expr->ops || expr->ops->arity == EXPR_OP_ATOM)
        return expr_clone(expr);

    switch (expr->ops->kind) {
        case EXPR_KIND_ADD:
        case EXPR_KIND_SUB:
        case EXPR_KIND_MUL:
        case EXPR_KIND_DIV:
        case EXPR_KIND_POW:
        case EXPR_KIND_POW_D:
        case EXPR_KIND_NEG:
        case EXPR_KIND_SQRT:
            break;

        default:
            left = expr_beautify_node(expr->a, rewrite_negative_roots);
            if (!left)
                goto cleanup;
            if (expr->ops->arity == EXPR_OP_BINARY) {
                right = expr_beautify_node(expr->b, rewrite_negative_roots);
                rebuilt = right && expr->ops->apply_binary ? expr->ops->apply_binary(left, right) : NULL;
            } else {
                rebuilt = expr->ops->apply_unary ? expr->ops->apply_unary(left) : NULL;
            }
            if (!rebuilt)
                rebuilt = expr_clone(expr);
            goto cleanup;
    }

    left = expr_beautify_node(expr->a, rewrite_negative_roots);
    if (!left)
        goto cleanup;
    if (expr->ops->arity == EXPR_OP_BINARY) {
        right = expr_beautify_node(expr->b, rewrite_negative_roots);
        if (!right)
            goto cleanup;
        rebuilt = expr_new_binary_internal(expr->ops, left, right);
    } else if (expr_is_pow_d_expr(expr)) {
        rebuilt = expr_new_pow_const_internal(left, expr->c);
    } else {
        rebuilt = expr_new_unary_internal(expr->ops, left);
    }
    if (!rebuilt)
        goto cleanup;
    left = NULL;
    right = NULL;
    rewrite = expr_beautify_radical_sum(rebuilt);
    if (rewrite) {
        expr_free(rebuilt);
        rebuilt = rewrite;
        rewrite = NULL;
    }
    rewrite = expr_beautify_symmetric_square_root(rebuilt);
    if (rewrite) {
        expr_free(rebuilt);
        rebuilt = rewrite;
        rewrite = NULL;
    }
    if (rewrite_negative_roots) {
        rewrite = expr_beautify_negative_square_root(rebuilt);
        if (rewrite) {
            expr_free(rebuilt);
            rebuilt = rewrite;
            rewrite = NULL;
        }
        rewrite = expr_beautify_cartesian_product(rebuilt);
        if (rewrite) {
            expr_free(rebuilt);
            rebuilt = rewrite;
            rewrite = NULL;
        }
    }
    rewrite = expr_beautify_complex_square_root_reciprocal(rebuilt);
    if (rewrite) {
        expr_free(rebuilt);
        rebuilt = rewrite;
        rewrite = NULL;
    }

cleanup:
    expr_free(rewrite);
    expr_free(right);
    expr_free(left);
    return rebuilt;
}

/* Return a display-oriented but algebraically equivalent expression tree. */
expr_t *expr_beautify(const expr_t *expr)
{
    expr_t *simplified;
    expr_t *expanded;
    expr_t *symmetric;
    expr_t *beautified;

    if (!expr)
        return NULL;
    simplified = expr_simplify(expr);
    expanded = simplified ? expr_beautify_expand_preserved(simplified) : NULL;
    symmetric = expanded ? expr_beautify_node(expanded, false) : NULL;
    beautified = symmetric ? expr_beautify_node(symmetric, true) : NULL;
    expr_free(symmetric);
    expr_free(expanded);
    expr_free(simplified);
    return beautified;
}
