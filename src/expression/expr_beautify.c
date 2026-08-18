#include <errno.h>
#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_binding_simplify.h"
#include "expr_bindings.h"
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

static bool expr_beautify_i_unit_sign(const expr_t *expr, int *sign_out)
{
    int child_sign;

    if (!expr || !sign_out)
        return false;
    if (expr_is_const(expr) && num_eq(expr->c, NUM_I)) {
        *sign_out = 1;
        return true;
    }
    if (expr_is_const(expr) && num_eq(expr->c, NUM_NEG_I)) {
        *sign_out = -1;
        return true;
    }
    if (expr_is_op(expr, &ops_neg) && expr_beautify_i_unit_sign(expr->a, &child_sign)) {
        *sign_out = -child_sign;
        return true;
    }
    return false;
}

static bool expr_beautify_extract_i_factor(const expr_t *expr, int *sign_out, const expr_t **coefficient_out)
{
    if (!expr || !sign_out || !coefficient_out)
        return false;
    if (expr_beautify_i_unit_sign(expr, sign_out)) {
        *coefficient_out = NULL;
        return true;
    }
    if (!expr_is_op(expr, &ops_mul))
        return false;
    if (expr_beautify_i_unit_sign(expr->a, sign_out)) {
        *coefficient_out = expr->b;
        return true;
    }
    if (expr_beautify_i_unit_sign(expr->b, sign_out)) {
        *coefficient_out = expr->a;
        return true;
    }
    return false;
}

static expr_t *expr_beautify_extract_exact_imaginary_coefficient(const expr_t *expr, int *sign_out)
{
    number_t value = number_invalid();
    number_t real = number_invalid();
    number_t imaginary = number_invalid();
    number_t magnitude = number_invalid();
    expr_t *coefficient = NULL;

    if (!expr || !sign_out || !expr_match_const_value(expr, &value) || !num_is_exact(value))
        goto cleanup;
    real = num_real_part(value);
    imaginary = num_imag_part(value);
    if (!num_is_zero(real) || num_is_zero(imaginary))
        goto cleanup;
    magnitude = num_abs(imaginary);
    coefficient = expr_new_const(magnitude);
    *sign_out = num_sign(imaginary);

cleanup:
    num_destroy(&magnitude);
    num_destroy(&imaginary);
    num_destroy(&real);
    num_destroy(&value);
    return coefficient;
}

static expr_t *expr_beautify_simplified_square_root(const expr_t *argument)
{
    number_t value = number_invalid();
    number_t root_value = number_invalid();
    expr_t *root = NULL;
    string_t *root_text = NULL;
    const char *root_chars;
    char *end = NULL;
    long root_long;

    if (!argument)
        goto cleanup;
    if (expr_match_const_value(argument, &value)) {
        if (expr_fold_sqrt_const(&value, &root_value)) {
            root = expr_new_const(root_value);
            goto cleanup;
        }
        num_destroy(&root_value);
        root_value = num_sqrt(value);
        if (num_is_real(root_value) && num_is_integer(root_value) && num_get_sign(root_value) >= 0) {
            root_text = num_to_string(root_value);
            root_chars = root_text ? string_c_str(root_text) : NULL;
            errno = 0;
            root_long = root_chars ? strtol(root_chars, &end, 10) : 0L;
            if (root_chars && end && *end == '\0' && errno == 0) {
                root = expr_const_long(root_long);
                goto cleanup;
            }
        }
    }
    root = expr_sqrt(argument);

cleanup:
    string_free(root_text);
    num_destroy(&root_value);
    num_destroy(&value);
    return root;
}

static expr_t *expr_beautify_cartesian_square_root(const expr_t *expr)
{
    const expr_t *argument;
    const expr_t *left;
    const expr_t *right;
    const expr_t *real_part = NULL;
    const expr_t *imaginary_coefficient = NULL;
    expr_t *owned_real_part = NULL;
    expr_t *owned_imaginary_coefficient = NULL;
    number_t argument_value = number_invalid();
    number_t real_value = number_invalid();
    number_t imaginary_value = number_invalid();
    number_t coefficient_value = number_invalid();
    expr_t *real_square = NULL;
    expr_t *imaginary_square = NULL;
    expr_t *norm_square = NULL;
    expr_t *norm = NULL;
    expr_t *real_sum = NULL;
    expr_t *imaginary_difference = NULL;
    expr_t *real_argument = NULL;
    expr_t *imaginary_argument = NULL;
    expr_t *real_root = NULL;
    expr_t *imaginary_root = NULL;
    expr_t *absolute_imaginary = NULL;
    expr_t *orientation = NULL;
    expr_t *oriented_imaginary = NULL;
    expr_t *imaginary_unit = NULL;
    expr_t *imaginary_term = NULL;
    expr_t *rewrite = NULL;
    int imaginary_sign;
    bool subtract;

    if (!expr_is_sqrt_expr(expr) || !(argument = expr->a))
        goto cleanup;

    if (expr_match_const_value(argument, &argument_value) && !num_is_real(argument_value)) {
        number_t imaginary_magnitude;

        real_value = num_real_part(argument_value);
        imaginary_value = num_imag_part(argument_value);
        if (num_is_zero(imaginary_value))
            goto cleanup;
        imaginary_magnitude = num_abs(imaginary_value);
        owned_real_part = expr_new_const(real_value);
        owned_imaginary_coefficient = expr_new_const(imaginary_magnitude);
        num_destroy(&imaginary_magnitude);
        if (!owned_real_part || !owned_imaginary_coefficient)
            goto cleanup;
        real_part = owned_real_part;
        imaginary_coefficient = owned_imaginary_coefficient;
        imaginary_sign = num_sign(imaginary_value);
    } else if (expr_beautify_extract_i_factor(argument, &imaginary_sign, &imaginary_coefficient)) {
        owned_real_part = expr_new_const(NUM_ZERO);
        real_part = owned_real_part;
    } else if ((owned_imaginary_coefficient =
                    expr_beautify_extract_exact_imaginary_coefficient(argument, &imaginary_sign))) {
        owned_real_part = expr_new_const(NUM_ZERO);
        real_part = owned_real_part;
        imaginary_coefficient = owned_imaginary_coefficient;
    } else {
        if (!(subtract = expr_match_binary_op(argument, EXPR_KIND_SUB, &left, &right)) &&
            !expr_match_binary_op(argument, EXPR_KIND_ADD, &left, &right))
            goto cleanup;


        if (expr_beautify_extract_i_factor(right, &imaginary_sign, &imaginary_coefficient)) {
            real_part = left;
            if (subtract)
                imaginary_sign = -imaginary_sign;
        } else if ((owned_imaginary_coefficient =
                        expr_beautify_extract_exact_imaginary_coefficient(right, &imaginary_sign))) {
            real_part = left;
            imaginary_coefficient = owned_imaginary_coefficient;
            if (subtract)
                imaginary_sign = -imaginary_sign;
        } else if (!subtract && expr_beautify_extract_i_factor(left, &imaginary_sign, &imaginary_coefficient)) {
            real_part = right;
        } else if (!subtract &&
                   (owned_imaginary_coefficient =
                        expr_beautify_extract_exact_imaginary_coefficient(left, &imaginary_sign))) {
            real_part = right;
            imaginary_coefficient = owned_imaginary_coefficient;
        } else {
            goto cleanup;
        }

        if (!imaginary_coefficient) {
            owned_imaginary_coefficient = expr_const_one();
            imaginary_coefficient = owned_imaginary_coefficient;
        }
    }
    if (!imaginary_coefficient)
        goto cleanup;

    real_square = expr_simplify_owned(expr_pow_long(real_part, 2L));
    imaginary_square = expr_simplify_owned(expr_pow_long(imaginary_coefficient, 2L));
    norm_square = expr_add_simplify_owned(real_square, imaginary_square);
    real_square = NULL;
    imaginary_square = NULL;
    norm = expr_beautify_simplified_square_root(norm_square);
    real_sum = norm ? expr_simplify_owned(expr_add(norm, real_part)) : NULL;
    imaginary_difference = norm ? expr_simplify_owned(expr_sub(norm, real_part)) : NULL;
    real_argument = real_sum ? expr_simplify_owned(expr_div_long(real_sum, 2L)) : NULL;
    imaginary_argument = imaginary_difference ? expr_simplify_owned(expr_div_long(imaginary_difference, 2L)) : NULL;
    real_root = expr_beautify_simplified_square_root(real_argument);
    imaginary_root = expr_beautify_simplified_square_root(imaginary_argument);
    if (expr_match_const_value(imaginary_coefficient, &coefficient_value) && num_is_real(coefficient_value) &&
        !num_is_zero(coefficient_value)) {
        if (num_sign(coefficient_value) < 0)
            imaginary_sign = -imaginary_sign;
        oriented_imaginary = imaginary_root ? expr_clone(imaginary_root) : NULL;
    } else {
        absolute_imaginary = expr_abs(imaginary_coefficient);
        orientation = absolute_imaginary ? expr_div(imaginary_coefficient, absolute_imaginary) : NULL;
        oriented_imaginary = orientation && imaginary_root ? expr_mul(orientation, imaginary_root) : NULL;
    }
    imaginary_unit = expr_new_const(NUM_I);
    if (imaginary_unit && oriented_imaginary)
        imaginary_term = expr_beautify_is_one(oriented_imaginary) ? expr_clone(imaginary_unit)
                                                                   : expr_mul(oriented_imaginary, imaginary_unit);
    if (real_root && imaginary_term)
        rewrite = imaginary_sign > 0 ? expr_add(real_root, imaginary_term) : expr_sub(real_root, imaginary_term);

cleanup:
    expr_free(imaginary_term);
    expr_free(imaginary_unit);
    expr_free(oriented_imaginary);
    expr_free(orientation);
    expr_free(absolute_imaginary);
    expr_free(imaginary_root);
    expr_free(real_root);
    expr_free(imaginary_argument);
    expr_free(real_argument);
    expr_free(imaginary_difference);
    expr_free(real_sum);
    expr_free(norm);
    expr_free(norm_square);
    expr_free(imaginary_square);
    expr_free(real_square);
    expr_free(owned_imaginary_coefficient);
    expr_free(owned_real_part);
    num_destroy(&coefficient_value);
    num_destroy(&imaginary_value);
    num_destroy(&real_value);
    num_destroy(&argument_value);
    return rewrite;
}

static expr_t *expr_beautify_cartesian_cube_root(const expr_t *expr)
{
    const expr_t *argument;
    number_t value = number_invalid();
    number_t real = number_invalid();
    number_t imaginary = number_invalid();
    number_t imaginary_magnitude = number_invalid();
    expr_t *one = NULL;
    expr_t *two = NULL;
    expr_t *three = NULL;
    expr_t *cube_root_two = NULL;
    expr_t *sqrt_three = NULL;
    expr_t *denominator = NULL;
    expr_t *sum = NULL;
    expr_t *difference = NULL;
    expr_t *scale = NULL;
    expr_t *imaginary_unit = NULL;
    expr_t *imaginary_term = NULL;
    expr_t *cartesian = NULL;
    expr_t *rewrite = NULL;
    int imaginary_sign;

    if (!expr_is_op(expr, &ops_cubrt) || !(argument = expr->a))
        goto cleanup;
    value = expr_eval(argument);
    if (!num_is_finite(value) || num_is_real(value))
        goto cleanup;
    real = num_real_part(value);
    imaginary = num_imag_part(value);
    imaginary_magnitude = num_abs(imaginary);
    if (!num_eq(real, NUM_ONE) || num_is_zero(imaginary) || !num_eq(imaginary_magnitude, NUM_ONE))
        goto cleanup;
    imaginary_sign = num_sign(imaginary);

    one = expr_const_one();
    two = expr_const_long(2L);
    three = expr_const_long(3L);
    cube_root_two = two ? expr_cubrt(two) : NULL;
    sqrt_three = three ? expr_sqrt(three) : NULL;
    denominator = cube_root_two ? expr_mul_long(cube_root_two, 2L) : NULL;
    sum = (sqrt_three && one) ? expr_add(sqrt_three, one) : NULL;
    difference = (sqrt_three && one) ? expr_sub(sqrt_three, one) : NULL;
    scale = (one && denominator) ? expr_div(one, denominator) : NULL;
    imaginary_unit = expr_new_const(NUM_I);
    imaginary_term = (imaginary_unit && difference) ? expr_mul(imaginary_unit, difference) : NULL;
    if (sum && imaginary_term)
        cartesian = imaginary_sign > 0 ? expr_add(sum, imaginary_term) : expr_sub(sum, imaginary_term);
    rewrite = (scale && cartesian) ? expr_mul(scale, cartesian) : NULL;

cleanup:
    expr_free(cartesian);
    expr_free(imaginary_term);
    expr_free(imaginary_unit);
    expr_free(scale);
    expr_free(difference);
    expr_free(sum);
    expr_free(denominator);
    expr_free(sqrt_three);
    expr_free(cube_root_two);
    expr_free(three);
    expr_free(two);
    expr_free(one);
    num_destroy(&imaginary_magnitude);
    num_destroy(&imaginary);
    num_destroy(&real);
    num_destroy(&value);
    return rewrite;
}

static expr_t *expr_beautify_cartesian_fourth_root(const expr_t *expr)
{
    const expr_t *argument;
    number_t order = number_invalid();
    number_t value = number_invalid();
    number_t real = number_invalid();
    number_t imaginary = number_invalid();
    number_t imaginary_magnitude = number_invalid();
    long order_numerator;
    long order_denominator;
    expr_t *two = NULL;
    expr_t *four = NULL;
    expr_t *one = NULL;
    expr_t *sqrt_two = NULL;
    expr_t *nested_square_root_sum = NULL;
    expr_t *nested_square_root_argument = NULL;
    expr_t *nested_square_root = NULL;
    expr_t *fourth_root_two = NULL;
    expr_t *real_radicand = NULL;
    expr_t *imaginary_radicand = NULL;
    expr_t *real_component = NULL;
    expr_t *imaginary_component = NULL;
    expr_t *imaginary_unit = NULL;
    expr_t *imaginary_term = NULL;
    expr_t *cartesian = NULL;
    expr_t *scale = NULL;
    expr_t *rewrite = NULL;
    int imaginary_sign;

    if (!expr_is_op(expr, &ops_root) || !(argument = expr->a) || !expr_match_const_value(expr->b, &order) ||
        !num_get_small_rational(order, &order_numerator, &order_denominator) || order_numerator != 4L ||
        order_denominator != 1L)
        goto cleanup;
    value = expr_eval(argument);
    if (!num_is_finite(value) || num_is_real(value))
        goto cleanup;
    real = num_real_part(value);
    imaginary = num_imag_part(value);
    imaginary_magnitude = num_abs(imaginary);
    if (!num_eq(real, NUM_ONE) || num_is_zero(imaginary) || !num_eq(imaginary_magnitude, NUM_ONE))
        goto cleanup;
    imaginary_sign = num_sign(imaginary);

    two = expr_const_long(2L);
    four = expr_const_long(4L);
    one = expr_const_long(1L);
    sqrt_two = two ? expr_sqrt(two) : NULL;
    nested_square_root_sum = (sqrt_two && one) ? expr_add(sqrt_two, one) : NULL;
    nested_square_root_argument = nested_square_root_sum ? expr_div_long(nested_square_root_sum, 2L) : NULL;
    nested_square_root = nested_square_root_argument ? expr_sqrt(nested_square_root_argument) : NULL;
    fourth_root_two = (two && four) ? expr_root(two, four) : NULL;
    real_radicand = (fourth_root_two && nested_square_root) ? expr_add(fourth_root_two, nested_square_root) : NULL;
    imaginary_radicand = (fourth_root_two && nested_square_root)
                             ? expr_sub(fourth_root_two, nested_square_root)
                             : NULL;
    real_component = real_radicand ? expr_sqrt(real_radicand) : NULL;
    imaginary_component = imaginary_radicand ? expr_sqrt(imaginary_radicand) : NULL;
    imaginary_unit = expr_new_const(NUM_I);
    imaginary_term = (imaginary_unit && imaginary_component) ? expr_mul(imaginary_unit, imaginary_component) : NULL;
    if (real_component && imaginary_term)
        cartesian = imaginary_sign > 0 ? expr_add(real_component, imaginary_term)
                                       : expr_sub(real_component, imaginary_term);
    scale = (one && sqrt_two) ? expr_div(one, sqrt_two) : NULL;
    rewrite = (scale && cartesian) ? expr_mul(scale, cartesian) : NULL;

cleanup:
    expr_free(scale);
    expr_free(cartesian);
    expr_free(imaginary_term);
    expr_free(imaginary_unit);
    expr_free(imaginary_component);
    expr_free(real_component);
    expr_free(imaginary_radicand);
    expr_free(real_radicand);
    expr_free(fourth_root_two);
    expr_free(nested_square_root);
    expr_free(nested_square_root_argument);
    expr_free(nested_square_root_sum);
    expr_free(sqrt_two);
    expr_free(one);
    expr_free(four);
    expr_free(two);
    num_destroy(&imaginary_magnitude);
    num_destroy(&imaginary);
    num_destroy(&real);
    num_destroy(&value);
    num_destroy(&order);
    return rewrite;
}

static expr_t *expr_beautify_root_turn_trig(long root_index, long order, bool sine)
{
    expr_t *exact = NULL;
    expr_t *rewrite = NULL;

    if (root_index < 0L || order < 2L)
        goto cleanup;
    exact = binding_expr_exact_trig_pi_ratio(sine ? &ops_sin : &ops_cos, 2L * root_index, (unsigned long)order);
    rewrite = exact ? expr_beautify_presimplified(exact) : NULL;

cleanup:
    expr_free(exact);
    return rewrite;
}

static long expr_beautify_principal_root_rotation(number_t seed, const expr_t *root, long order)
{
    NUM_SCOPE(scope);
    number_t principal;
    long best_index = -1L;
    double best_distance = DBL_MAX;

    if (!root || order < 2L)
        return -1L;
    principal = expr_eval(root);
    if (!num_is_finite(principal))
        return -1L;

    for (long root_index = 0L; root_index < order; ++root_index) {
        number_t turn = num_create_from_frac(2L * root_index, order);
        number_t angle = num_mul(NUM_PI, turn);
        number_t rotation = num_add(num_cos(angle), num_mul(num_sin(angle), NUM_I));
        number_t branch = num_mul(seed, rotation);
        number_t distance = num_abs(num_sub(branch, principal));
        double candidate_distance = num_to_double(distance);

        if (candidate_distance < best_distance) {
            best_distance = candidate_distance;
            best_index = root_index;
        }
    }
    return best_index;
}

static expr_t *expr_beautify_cartesian_exact_root(const expr_t *expr)
{
    number_t seed = number_invalid();
    number_t real_value = number_invalid();
    number_t imaginary_value = number_invalid();
    number_t result_imaginary_value = number_invalid();
    long order = 0L;
    long rotation_index;
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    expr_t *cosine = NULL;
    expr_t *sine = NULL;
    expr_t *real_cosine = NULL;
    expr_t *imaginary_sine = NULL;
    expr_t *real_sine = NULL;
    expr_t *imaginary_cosine = NULL;
    expr_t *real_part = NULL;
    expr_t *imaginary_part = NULL;
    expr_t *real_display = NULL;
    expr_t *imaginary_display = NULL;
    expr_t *imaginary_magnitude = NULL;
    expr_t *imaginary_unit = NULL;
    expr_t *imaginary_term = NULL;
    expr_t *rewrite = NULL;
    bool imaginary_is_negative;

    if (!expr_is_op(expr, &ops_root) || !expr_exact_complex_root_seed(expr, &seed, &order))
        goto cleanup;
    rotation_index = expr_beautify_principal_root_rotation(seed, expr, order);
    if (rotation_index < 0L)
        goto cleanup;
    if (rotation_index == 0L) {
        rewrite = expr_new_const(seed);
        goto cleanup;
    }

    real_value = num_real_part(seed);
    imaginary_value = num_imag_part(seed);
    real = expr_new_const(real_value);
    imaginary = expr_new_const(imaginary_value);
    cosine = expr_beautify_root_turn_trig(rotation_index, order, false);
    sine = expr_beautify_root_turn_trig(rotation_index, order, true);
    if (!real || !imaginary || !cosine || !sine)
        goto cleanup;

    real_cosine = expr_mul(real, cosine);
    imaginary_sine = expr_mul(imaginary, sine);
    real_sine = expr_mul(real, sine);
    imaginary_cosine = expr_mul(imaginary, cosine);
    real_part = (real_cosine && imaginary_sine) ? expr_sub(real_cosine, imaginary_sine) : NULL;
    imaginary_part = (real_sine && imaginary_cosine) ? expr_add(real_sine, imaginary_cosine) : NULL;
    real_display = real_part ? expr_beautify(real_part) : NULL;
    imaginary_display = imaginary_part ? expr_beautify(imaginary_part) : NULL;
    if (!real_display || !imaginary_display)
        goto cleanup;

    result_imaginary_value = expr_eval(imaginary_display);
    imaginary_is_negative = num_sign(result_imaginary_value) < 0;
    imaginary_magnitude = imaginary_is_negative ? expr_neg(imaginary_display) : expr_clone(imaginary_display);
    imaginary_unit = expr_new_const(NUM_I);
    imaginary_term = (imaginary_unit && imaginary_magnitude) ? expr_mul(imaginary_magnitude, imaginary_unit) : NULL;
    if (real_display && imaginary_term)
        rewrite = imaginary_is_negative ? expr_sub(real_display, imaginary_term) : expr_add(real_display, imaginary_term);

cleanup:
    expr_free(imaginary_term);
    expr_free(imaginary_unit);
    expr_free(imaginary_magnitude);
    expr_free(imaginary_display);
    expr_free(real_display);
    expr_free(imaginary_part);
    expr_free(real_part);
    expr_free(imaginary_cosine);
    expr_free(real_sine);
    expr_free(imaginary_sine);
    expr_free(real_cosine);
    expr_free(sine);
    expr_free(cosine);
    expr_free(imaginary);
    expr_free(real);
    num_destroy(&result_imaginary_value);
    num_destroy(&imaginary_value);
    num_destroy(&real_value);
    num_destroy(&seed);
    return rewrite;
}

static expr_t *expr_beautify_named_principal_root(const expr_t *expr)
{
    number_t order = number_invalid();
    long numerator;
    long denominator;
    expr_t *rewrite = NULL;

    if (!expr_is_op(expr, &ops_root) || !expr->a || !expr_match_const_value(expr->b, &order) ||
        !num_get_small_rational(order, &numerator, &denominator) || denominator != 1L)
        goto cleanup;
    if (numerator == 2L)
        rewrite = expr_sqrt(expr->a);
    else if (numerator == 3L)
        rewrite = expr_cubrt(expr->a);

cleanup:
    num_destroy(&order);
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
            expr->binding_expr->kind != EXPR_BINDING_EXPR_CONST && !expr_binding_expr_is_array(expr->binding_expr)) {
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
    if (expr_is_pow_d_expr(expr)) {
        rebuilt = expr_new_pow_const_internal(left, expr->c);
    } else if (expr->ops->arity == EXPR_OP_BINARY) {
        right = expr_beautify_expand_preserved(expr->b);
        if (!right)
            goto cleanup;
        rebuilt = expr_new_binary_internal(expr->ops, left, right);
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
    int imaginary_unit_sign;

    if (!expr_match_binary_op(expr, EXPR_KIND_MUL, &left, &right))
        goto cleanup;
    if ((expr_beautify_i_unit_sign(left, &imaginary_unit_sign) && expr_beautify_manifestly_real(right)) ||
        (expr_beautify_i_unit_sign(right, &imaginary_unit_sign) && expr_beautify_manifestly_real(left)))
        goto cleanup;
    if (!expr_beautify_cartesian_parts(expr, &real, &imaginary, &has_imaginary) || !has_imaginary)
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

static void expr_beautify_collect_product_factors(const expr_t *expr, const expr_t ***factors, size_t *count,
                                                   size_t *capacity);

static expr_t *expr_beautify_extract_imaginary_product_coefficient(const expr_t *expr, int *sign_out)
{
    const expr_t *numerator;
    const expr_t *denominator;
    const expr_t **factors = NULL;
    expr_t *coefficient = NULL;
    size_t count = 0u;
    size_t capacity = 0u;
    size_t imaginary_index = SIZE_MAX;
    int sign = 1;

    if (!expr || !sign_out)
        goto cleanup;
    if (expr_match_binary_op(expr, EXPR_KIND_DIV, &numerator, &denominator)) {
        expr_t *quotient;
        int denominator_sign;

        if (expr_beautify_i_unit_sign(denominator, &denominator_sign)) {
            coefficient = expr_clone(numerator);
            *sign_out = -denominator_sign;
            goto cleanup;
        }

        coefficient = expr_beautify_extract_imaginary_product_coefficient(numerator, sign_out);
        quotient = coefficient ? expr_div(coefficient, denominator) : NULL;
        expr_free(coefficient);
        coefficient = quotient;
        goto cleanup;
    }
    expr_beautify_collect_product_factors(expr, &factors, &count, &capacity);
    for (size_t index = 0u; index < count; ++index) {
        int factor_sign;

        if (imaginary_index == SIZE_MAX && expr_beautify_i_unit_sign(factors[index], &factor_sign)) {
            imaginary_index = index;
            sign = factor_sign;
        }
    }
    if (imaginary_index == SIZE_MAX)
        goto cleanup;
    for (size_t index = 0u; index < count; ++index) {
        expr_t *factor;

        if (index == imaginary_index)
            continue;
        factor = expr_clone(factors[index]);
        if (coefficient) {
            expr_t *product = expr_mul(coefficient, factor);

            expr_free(factor);
            expr_free(coefficient);
            coefficient = product;
        } else {
            coefficient = factor;
        }
    }
    if (!coefficient)
        coefficient = expr_const_one();
    *sign_out = sign;

cleanup:
    free((void *)factors);
    return coefficient;
}

static expr_t *expr_beautify_division_by_imaginary_unit_for_display(const expr_t *expr)
{
    const expr_t *numerator;
    const expr_t *denominator;
    const expr_t *real_part;
    const expr_t *imaginary_part;
    expr_t *imaginary_coefficient = NULL;
    expr_t *real_term = NULL;
    expr_t *imaginary_unit = NULL;
    expr_t *imaginary_term = NULL;
    expr_t *rewrite = NULL;
    int denominator_sign;
    int imaginary_sign;
    bool subtract;

    if (!expr_match_binary_op(expr, EXPR_KIND_DIV, &numerator, &denominator) ||
        !expr_beautify_i_unit_sign(denominator, &denominator_sign))
        goto cleanup;
    if (!(subtract = expr_match_binary_op(numerator, EXPR_KIND_SUB, &real_part, &imaginary_part)) &&
        !expr_match_binary_op(numerator, EXPR_KIND_ADD, &real_part, &imaginary_part))
        goto cleanup;
    imaginary_coefficient = expr_beautify_extract_imaginary_product_coefficient(imaginary_part, &imaginary_sign);
    if (!imaginary_coefficient)
        goto cleanup;

    imaginary_sign *= subtract ? -1 : 1;
    real_term = expr_clone(imaginary_coefficient);
    if (imaginary_sign * denominator_sign < 0)
        real_term = expr_negate_owned(real_term);
    imaginary_unit = expr_new_const(NUM_I);
    imaginary_term = imaginary_unit ? expr_mul(imaginary_unit, real_part) : NULL;
    if (real_term && imaginary_term) {
        rewrite = expr_new_binary_internal(denominator_sign > 0 ? &ops_sub : &ops_add, real_term, imaginary_term);
        if (rewrite) {
            real_term = NULL;
            imaginary_term = NULL;
        }
    }

cleanup:
    expr_free(imaginary_term);
    expr_free(imaginary_unit);
    expr_free(real_term);
    expr_free(imaginary_coefficient);
    return rewrite;
}

static expr_t *expr_beautify_imaginary_cartesian_product_direct(const expr_t *expr)
{
    const expr_t **factors = NULL;
    const expr_t *cartesian_real = NULL;
    const expr_t *cartesian_imaginary = NULL;
    const expr_t *cartesian_denominator = NULL;
    expr_t *owned_cartesian_imaginary = NULL;
    expr_t *outer_magnitude = NULL;
    expr_t *scale = NULL;
    expr_t *real_term = NULL;
    expr_t *imaginary_coefficient = NULL;
    expr_t *imaginary_unit = NULL;
    expr_t *imaginary_term = NULL;
    expr_t *sum = NULL;
    expr_t *rewrite = NULL;
    int outer_sign = 1;
    int inner_sign = 1;
    size_t count = 0u;
    size_t capacity = 0u;
    size_t imaginary_index = SIZE_MAX;
    size_t cartesian_index = SIZE_MAX;

    rewrite = expr_beautify_division_by_imaginary_unit_for_display(expr);
    if (rewrite)
        goto cleanup;
    if (!expr_is_op(expr, &ops_mul))
        goto cleanup;
    expr_beautify_collect_product_factors(expr, &factors, &count, &capacity);
    for (size_t index = 0u; index < count; ++index) {
        expr_t *magnitude;
        const expr_t *candidate = factors[index];
        const expr_t *left;
        const expr_t *right;
        const expr_t *denominator = NULL;
        int sign;
        bool subtract;

        magnitude = expr_beautify_extract_imaginary_product_coefficient(factors[index], &sign);
        if (magnitude && imaginary_index == SIZE_MAX) {
            imaginary_index = index;
            outer_magnitude = magnitude;
            outer_sign = sign;
            continue;
        }
        expr_free(magnitude);
        if (expr_match_binary_op(candidate, EXPR_KIND_DIV, &left, &denominator))
            candidate = left;
        if (!(subtract = expr_match_binary_op(candidate, EXPR_KIND_SUB, &left, &right)) &&
            !expr_match_binary_op(candidate, EXPR_KIND_ADD, &left, &right))
            continue;
        owned_cartesian_imaginary = expr_beautify_extract_imaginary_product_coefficient(right, &sign);
        if (!owned_cartesian_imaginary)
            continue;
        cartesian_imaginary = owned_cartesian_imaginary;
        cartesian_index = index;
        cartesian_real = left;
        cartesian_denominator = denominator;
        inner_sign = subtract ? -sign : sign;
    }
    if (imaginary_index == SIZE_MAX || cartesian_index == SIZE_MAX)
        goto cleanup;
    scale = expr_clone(outer_magnitude);
    for (size_t index = 0u; index < count && scale; ++index) {
        if (index == imaginary_index || index == cartesian_index)
            continue;
        scale = expr_mul_simplify_owned(scale, expr_clone(factors[index]));
    }
    if (scale && cartesian_denominator)
        scale = expr_div_simplify_owned(scale, expr_clone(cartesian_denominator));
    real_term = expr_clone(cartesian_imaginary);
    if (outer_sign * inner_sign > 0)
        real_term = expr_negate_owned(real_term);
    imaginary_coefficient = expr_clone(cartesian_real);
    imaginary_unit = expr_new_const(NUM_I);
    imaginary_term = imaginary_unit && imaginary_coefficient ? expr_mul(imaginary_unit, imaginary_coefficient) : NULL;
    if (outer_sign < 0)
        imaginary_term = expr_negate_owned(imaginary_term);
    sum = real_term && imaginary_term ? expr_add(real_term, imaginary_term) : NULL;
    rewrite = scale && sum ? expr_mul(scale, sum) : NULL;

cleanup:
    free((void *)factors);
    expr_free(sum);
    expr_free(imaginary_term);
    expr_free(imaginary_unit);
    expr_free(imaginary_coefficient);
    expr_free(real_term);
    expr_free(scale);
    expr_free(outer_magnitude);
    expr_free(owned_cartesian_imaginary);
    return rewrite;
}

static expr_t *expr_beautify_imaginary_cartesian_product_recursive(const expr_t *expr, bool *changed)
{
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *rebuilt = NULL;
    expr_t *rewrite = NULL;

    if (!expr)
        return NULL;
    if (!expr->ops || expr->ops->arity == EXPR_OP_ATOM)
        return expr_clone(expr);

    left = expr_beautify_imaginary_cartesian_product_recursive(expr->a, changed);
    if (!left)
        goto cleanup;
    if (expr_is_pow_d_expr(expr)) {
        rebuilt = expr_new_pow_const_internal(left, expr->c);
    } else if (expr->ops->arity == EXPR_OP_BINARY) {
        right = expr_beautify_imaginary_cartesian_product_recursive(expr->b, changed);
        if (!right)
            goto cleanup;
        rebuilt = expr_new_binary_internal(expr->ops, left, right);
    } else {
        rebuilt = expr_new_unary_internal(expr->ops, left);
    }
    if (!rebuilt)
        goto cleanup;
    left = NULL;
    right = NULL;
    rewrite = expr_beautify_imaginary_cartesian_product_direct(rebuilt);
    if (rewrite) {
        expr_free(rebuilt);
        rebuilt = rewrite;
        rewrite = NULL;
        *changed = true;
    }

cleanup:
    expr_free(rewrite);
    expr_free(right);
    expr_free(left);
    return rebuilt;
}

/* Rotate Cartesian expressions by exact imaginary factors recursively for presentation. */
expr_t *expr_beautify_imaginary_cartesian_product_for_display(const expr_t *expr)
{
    bool changed = false;
    expr_t *rewrite = expr_beautify_imaginary_cartesian_product_recursive(expr, &changed);

    if (!changed) {
        expr_free(rewrite);
        return NULL;
    }
    return rewrite;
}

static expr_t *expr_beautify_remove_named_addend_for_display(const expr_t *expr, const char *name, expr_t **removed)
{
    const expr_t *left_source;
    const expr_t *right_source;
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *out = NULL;
    bool subtract = false;

    if (!expr || !name || !removed)
        return NULL;
    if (!*removed && expr_is_named_const(expr) && expr_symbol_name(expr) && strcmp(expr_symbol_name(expr), name) == 0) {
        *removed = expr_clone(expr);
        return NULL;
    }
    if (expr_is_op(expr, &ops_neg)) {
        left = expr_beautify_remove_named_addend_for_display(expr->a, name, removed);
        out = left ? expr_new_unary_internal(&ops_neg, left) : NULL;
        if (out)
            left = NULL;
        expr_free(left);
        return out;
    }
    if (!expr_match_add_sub_expr(expr, &left_source, &right_source, &subtract))
        return expr_clone(expr);

    left = expr_beautify_remove_named_addend_for_display(left_source, name, removed);
    right = expr_beautify_remove_named_addend_for_display(right_source, name, removed);
    if (left && right) {
        out = expr_new_binary_internal(subtract ? &ops_sub : &ops_add, left, right);
        if (out) {
            left = NULL;
            right = NULL;
        }
    } else if (left) {
        out = expr_clone(left);
    } else if (right) {
        out = subtract ? expr_new_unary_internal(&ops_neg, right) : expr_clone(right);
        if (subtract && out)
            right = NULL;
    }
    expr_free(right);
    expr_free(left);
    return out;
}

/* Move a named additive constant last without resimplifying the displayed algebra. */
expr_t *expr_move_named_addend_last_for_display(const expr_t *expr, const char *name)
{
    expr_t *constant = NULL;
    expr_t *rest = expr_beautify_remove_named_addend_for_display(expr, name, &constant);
    expr_t *out = NULL;

    if (constant) {
        out = rest ? expr_new_binary_internal(&ops_add, rest, constant) : expr_clone(constant);
        if (rest && out) {
            rest = NULL;
            constant = NULL;
        }
    }
    expr_free(rest);
    expr_free(constant);
    return out;
}

static void expr_beautify_collect_product_factors(const expr_t *expr, const expr_t ***factors, size_t *count,
                                                   size_t *capacity)
{
    if (!expr || !factors || !count || !capacity)
        return;
    if (expr_is_op(expr, &ops_mul)) {
        expr_beautify_collect_product_factors(expr->a, factors, count, capacity);
        expr_beautify_collect_product_factors(expr->b, factors, count, capacity);
        return;
    }
    if (*count == *capacity) {
        size_t next_capacity = *capacity ? *capacity * 2u : 4u;
        const expr_t **next = realloc((void *)*factors, next_capacity * sizeof(*next));

        if (!next)
            return;
        *factors = next;
        *capacity = next_capacity;
    }
    (*factors)[(*count)++] = expr;
}

static expr_t *expr_beautify_imaginary_product(const expr_t *expr)
{
    const expr_t **factors = NULL;
    size_t count = 0u;
    size_t capacity = 0u;
    size_t imaginary_index = 0u;
    number_t imaginary_part = number_invalid();
    number_t real_part = number_invalid();
    number_t magnitude = number_invalid();
    expr_t *product = NULL;
    expr_t *imaginary_unit = NULL;
    expr_t *rewrite = NULL;
    bool found = false;
    bool negative = false;

    if (!expr_is_op(expr, &ops_mul))
        goto cleanup;
    expr_beautify_collect_product_factors(expr, &factors, &count, &capacity);
    for (size_t i = 0u; i < count; ++i) {
        if (!expr_is_const(factors[i]) || num_is_real(factors[i]->c))
            continue;
        real_part = num_real_part(factors[i]->c);
        imaginary_part = num_imag_part(factors[i]->c);
        if (num_is_zero(real_part) && !num_is_zero(imaginary_part)) {
            imaginary_index = i;
            found = true;
            break;
        }
        num_destroy(&imaginary_part);
        imaginary_part = number_invalid();
        num_destroy(&real_part);
        real_part = number_invalid();
    }
    if (!found || count < 2u || (imaginary_index + 1u == count && num_eq(imaginary_part, NUM_ONE)))
        goto cleanup;

    negative = num_sign(imaginary_part) < 0;
    magnitude = num_abs(imaginary_part);
    if (!num_is_one(magnitude))
        product = expr_new_const(magnitude);
    for (size_t i = 0u; i < count; ++i) {
        expr_t *factor;

        if (i == imaginary_index)
            continue;
        factor = expr_clone(factors[i]);
        if (!product) {
            product = factor;
        } else {
            expr_t *next = expr_mul(product, factor);

            expr_free(factor);
            expr_free(product);
            product = next;
        }
    }
    imaginary_unit = expr_new_named_const(NUM_I, "i");
    rewrite = product && imaginary_unit ? expr_mul(product, imaginary_unit) : NULL;
    if (negative && rewrite) {
        expr_t *negated = expr_neg(rewrite);

        expr_free(rewrite);
        rewrite = negated;
    }

cleanup:
    expr_free(imaginary_unit);
    expr_free(product);
    num_destroy(&magnitude);
    num_destroy(&real_part);
    num_destroy(&imaginary_part);
    free((void *)factors);
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

/* Write a symbolic reciprocal square root in Cartesian surd form. */
static expr_t *expr_beautify_symbolic_complex_square_root_reciprocal_direct(const expr_t *expr)
{
    const expr_t *argument;
    const expr_t *left;
    const expr_t *right;
    const expr_t *real_part = NULL;
    const expr_t *imaginary_coefficient = NULL;
    expr_t *owned_imaginary_coefficient = NULL;
    number_t exponent = number_invalid();
    number_t coefficient_value = number_invalid();
    long exponent_numerator;
    long exponent_denominator;
    expr_t *real_square = NULL;
    expr_t *imaginary_square = NULL;
    expr_t *norm_square = NULL;
    expr_t *norm = NULL;
    expr_t *real_sum = NULL;
    expr_t *imaginary_difference = NULL;
    expr_t *real_argument = NULL;
    expr_t *imaginary_argument = NULL;
    expr_t *real_root = NULL;
    expr_t *imaginary_root = NULL;
    expr_t *absolute_imaginary = NULL;
    expr_t *orientation = NULL;
    expr_t *oriented_imaginary = NULL;
    expr_t *imaginary_unit = NULL;
    expr_t *imaginary_term = NULL;
    expr_t *cartesian = NULL;
    expr_t *rewrite = NULL;
    int imaginary_sign;
    bool subtract;

    if (!expr_match_pow_const(expr, &argument, &exponent) ||
        !num_get_small_rational(exponent, &exponent_numerator, &exponent_denominator) ||
        exponent_numerator != -1L || exponent_denominator != 2L) {
        goto cleanup;
    }
    if (!argument)
        goto cleanup;
    if (!(subtract = expr_match_binary_op(argument, EXPR_KIND_SUB, &left, &right)) &&
        !expr_match_binary_op(argument, EXPR_KIND_ADD, &left, &right))
        goto cleanup;

    if (expr_beautify_extract_i_factor(right, &imaginary_sign, &imaginary_coefficient)) {
        real_part = left;
        if (subtract)
            imaginary_sign = -imaginary_sign;
    } else if ((owned_imaginary_coefficient =
                    expr_beautify_extract_exact_imaginary_coefficient(right, &imaginary_sign))) {
        real_part = left;
        imaginary_coefficient = owned_imaginary_coefficient;
        if (subtract)
            imaginary_sign = -imaginary_sign;
    } else if (!subtract && expr_beautify_extract_i_factor(left, &imaginary_sign, &imaginary_coefficient)) {
        real_part = right;
    } else if (!subtract &&
               (owned_imaginary_coefficient =
                    expr_beautify_extract_exact_imaginary_coefficient(left, &imaginary_sign))) {
        real_part = right;
        imaginary_coefficient = owned_imaginary_coefficient;
    } else {
        goto cleanup;
    }
    if (!imaginary_coefficient) {
        owned_imaginary_coefficient = expr_const_one();
        imaginary_coefficient = owned_imaginary_coefficient;
    }
    if (!real_part || !imaginary_coefficient)
        goto cleanup;

    real_square = expr_simplify_owned(expr_pow_long(real_part, 2L));
    imaginary_square = expr_simplify_owned(expr_pow_long(imaginary_coefficient, 2L));
    norm_square = real_square && imaginary_square ? expr_add_simplify_owned(real_square, imaginary_square) : NULL;
    real_square = NULL;
    imaginary_square = NULL;
    norm = norm_square ? expr_beautify_simplified_square_root(norm_square) : NULL;
    real_sum = norm ? expr_add(norm, real_part) : NULL;
    imaginary_difference = norm ? expr_sub(norm, real_part) : NULL;
    real_argument = real_sum ? expr_simplify_owned(expr_div_long(real_sum, 2L)) : NULL;
    imaginary_argument = imaginary_difference ? expr_simplify_owned(expr_div_long(imaginary_difference, 2L)) : NULL;
    real_root = real_argument ? expr_beautify_simplified_square_root(real_argument) : NULL;
    imaginary_root = imaginary_argument ? expr_beautify_simplified_square_root(imaginary_argument) : NULL;

    if (expr_match_const_value(imaginary_coefficient, &coefficient_value) && num_is_real(coefficient_value) &&
        !num_is_zero(coefficient_value)) {
        if (num_sign(coefficient_value) < 0)
            imaginary_sign = -imaginary_sign;
        oriented_imaginary = imaginary_root ? expr_clone(imaginary_root) : NULL;
    } else {
        absolute_imaginary = expr_abs(imaginary_coefficient);
        orientation = absolute_imaginary ? expr_div(imaginary_coefficient, absolute_imaginary) : NULL;
        oriented_imaginary = orientation && imaginary_root ? expr_mul(orientation, imaginary_root) : NULL;
    }

    imaginary_unit = expr_new_const(NUM_I);
    imaginary_term = imaginary_unit && oriented_imaginary ? expr_mul(imaginary_unit, oriented_imaginary) : NULL;
    if (real_root && imaginary_term)
        cartesian = imaginary_sign > 0 ? expr_sub(real_root, imaginary_term) : expr_add(real_root, imaginary_term);
    rewrite = cartesian && norm ? expr_div(cartesian, norm) : NULL;

cleanup:
    expr_free(cartesian);
    expr_free(imaginary_term);
    expr_free(imaginary_unit);
    expr_free(oriented_imaginary);
    expr_free(orientation);
    expr_free(absolute_imaginary);
    expr_free(imaginary_root);
    expr_free(real_root);
    expr_free(imaginary_argument);
    expr_free(real_argument);
    expr_free(imaginary_difference);
    expr_free(real_sum);
    expr_free(norm);
    expr_free(norm_square);
    expr_free(imaginary_square);
    expr_free(real_square);
    num_destroy(&coefficient_value);
    num_destroy(&exponent);
    expr_free(owned_imaginary_coefficient);
    return rewrite;
}

static expr_t *expr_beautify_symbolic_complex_square_root_reciprocal_recursive(const expr_t *expr, bool *changed)
{
    expr_t *left = NULL;
    expr_t *right = NULL;
    expr_t *rebuilt = NULL;
    expr_t *rewrite = NULL;

    if (!expr)
        return NULL;
    if (!expr->ops || expr->ops->arity == EXPR_OP_ATOM)
        return expr_clone(expr);

    left = expr_beautify_symbolic_complex_square_root_reciprocal_recursive(expr->a, changed);
    if (!left)
        goto cleanup;
    if (expr_is_pow_d_expr(expr)) {
        rebuilt = expr_new_pow_const_internal(left, expr->c);
    } else if (expr->ops->arity == EXPR_OP_BINARY) {
        right = expr_beautify_symbolic_complex_square_root_reciprocal_recursive(expr->b, changed);
        if (!right)
            goto cleanup;
        rebuilt = expr_new_binary_internal(expr->ops, left, right);
    } else {
        rebuilt = expr_new_unary_internal(expr->ops, left);
    }
    if (!rebuilt)
        goto cleanup;
    left = NULL;
    right = NULL;
    rewrite = expr_beautify_symbolic_complex_square_root_reciprocal_direct(rebuilt);
    if (rewrite) {
        expr_free(rebuilt);
        rebuilt = rewrite;
        rewrite = NULL;
        *changed = true;
    }

cleanup:
    expr_free(rewrite);
    expr_free(right);
    expr_free(left);
    return rebuilt;
}

/* Rewrite symbolic reciprocal square roots recursively for Cartesian presentation. */
expr_t *expr_beautify_symbolic_complex_square_root_reciprocal_for_display(const expr_t *expr)
{
    bool changed = false;
    expr_t *rewrite = expr_beautify_symbolic_complex_square_root_reciprocal_recursive(expr, &changed);

    if (!changed) {
        expr_free(rewrite);
        return NULL;
    }
    return rewrite;
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
        case EXPR_KIND_CUBRT:
        case EXPR_KIND_ROOT:
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
    if (expr_is_pow_d_expr(expr)) {
        rebuilt = expr_new_pow_const_internal(left, expr->c);
    } else if (expr->ops->arity == EXPR_OP_BINARY) {
        right = expr_beautify_node(expr->b, rewrite_negative_roots);
        if (!right)
            goto cleanup;
        rebuilt = expr_new_binary_internal(expr->ops, left, right);
    } else {
        rebuilt = expr_new_unary_internal(expr->ops, left);
    }
    if (!rebuilt)
        goto cleanup;
    left = NULL;
    right = NULL;
    rewrite = expr_beautify_cartesian_fourth_root(rebuilt);
    if (rewrite) {
        expr_free(rebuilt);
        rebuilt = rewrite;
        rewrite = NULL;
    }
    rewrite = expr_beautify_cartesian_exact_root(rebuilt);
    if (rewrite) {
        expr_free(rebuilt);
        rebuilt = rewrite;
        rewrite = NULL;
    }
    rewrite = expr_beautify_named_principal_root(rebuilt);
    if (rewrite) {
        expr_free(rebuilt);
        rebuilt = rewrite;
        rewrite = NULL;
    }
    if (!rewrite_negative_roots) {
        rewrite = expr_beautify_radical_sum(rebuilt);
        if (rewrite) {
            expr_free(rebuilt);
            rebuilt = rewrite;
            rewrite = NULL;
        }
    }
    rewrite = expr_beautify_symmetric_square_root(rebuilt);
    if (rewrite) {
        expr_free(rebuilt);
        rebuilt = rewrite;
        rewrite = NULL;
    }
    rewrite = expr_beautify_cartesian_square_root(rebuilt);
    if (rewrite) {
        expr_free(rebuilt);
        rebuilt = rewrite;
        rewrite = NULL;
    }
    rewrite = expr_beautify_cartesian_cube_root(rebuilt);
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
    rewrite = expr_beautify_imaginary_product(rebuilt);
    if (rewrite) {
        expr_free(rebuilt);
        rebuilt = rewrite;
        rewrite = NULL;
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

static bool expr_beautify_has_preserved_complex_root(const expr_t *expr)
{
    const expr_binding_expr_t *binding;
    const expr_binding_expr_t *radicand_binding = NULL;
    expr_t *radicand = NULL;
    number_t value = number_invalid();
    number_t imaginary = number_invalid();
    bool matched = false;

    if (!expr || !expr->ops || expr->ops->arity != EXPR_OP_ATOM || !(binding = expr->binding_expr))
        goto cleanup;
    if (binding->kind == EXPR_BINDING_EXPR_UNARY_OP &&
        (binding->u.unary_op.ops == &ops_sqrt || binding->u.unary_op.ops == &ops_cubrt))
        radicand_binding = binding->u.unary_op.child;
    else if (binding->kind == EXPR_BINDING_EXPR_BINARY_OP && binding->u.binary_op.ops == &ops_root)
        radicand_binding = binding->u.binary_op.left;
    if (!radicand_binding)
        goto cleanup;

    radicand = expr_binding_expr_eval_expr(radicand_binding);
    if (!radicand)
        goto cleanup;
    value = expr_eval(radicand);
    imaginary = num_imag_part(value);
    matched = !num_is_zero(imaginary);

cleanup:
    num_destroy(&imaginary);
    num_destroy(&value);
    expr_free(radicand);
    return matched;
}

/* Beautify an expression tree which has already received its intended algebraic simplification. */
expr_t *expr_beautify_presimplified(const expr_t *expr)
{
    expr_t *expanded = NULL;
    expr_t *symmetric;
    expr_t *beautified;
    const expr_t *source = expr;

    if (!expr)
        return NULL;
    if (expr_beautify_has_preserved_complex_root(expr)) {
        expanded = expr_beautify_expand_preserved(expr);
        if (expanded)
            source = expanded;
    }
    symmetric = expr_beautify_node(source, false);
    beautified = symmetric ? expr_beautify_node(symmetric, true) : NULL;
    expr_free(symmetric);
    expr_free(expanded);
    return beautified;
}

/* Return a display-oriented but algebraically equivalent expression tree. */
expr_t *expr_beautify(const expr_t *expr)
{
    expr_t *simplified;
    expr_t *expanded;
    expr_t *beautified;

    if (!expr)
        return NULL;
    simplified = expr_simplify(expr);
    expanded = simplified ? expr_beautify_expand_preserved(simplified) : NULL;
    beautified = expanded ? expr_beautify_presimplified(expanded) : NULL;
    expr_free(expanded);
    expr_free(simplified);
    return beautified;
}

static expr_t *expr_distribute_negative_terms_for_display(const expr_t *expr)
{
    expr_t *left;
    expr_t *right;
    expr_t *out;

    if (!expr)
        return NULL;
    if (expr_is_op(expr, &ops_add)) {
        left = expr_distribute_negative_terms_for_display(expr->a);
        right = expr_distribute_negative_terms_for_display(expr->b);
        out = (left && right) ? expr_add(left, right) : NULL;
        expr_free(right);
        expr_free(left);
        return out;
    }
    if (expr_is_op(expr, &ops_sub)) {
        left = expr_distribute_negative_terms_for_display(expr->a);
        right = expr_clone(expr->b);
        out = (left && right) ? expr_add(left, right) : NULL;
        expr_free(right);
        expr_free(left);
        return out;
    }

    return expr_negate_owned(expr_clone(expr));
}

/* Negate a display expression term by term while retaining each term's established form. */
expr_t *expr_distribute_negative_for_display(const expr_t *expr)
{
    expr_t *expanded = NULL;
    expr_t *out;

    if (!expr)
        return NULL;
    if (expr_is_const(expr) && !num_is_real(expr->c)) {
        expanded = expr_display_expanded(expr);
        if (!expanded)
            return NULL;
        expr = expanded;
    }

    out = expr_distribute_negative_terms_for_display(expr);
    expr_free(expanded);
    return out;
}

/* Separate a top-level Cartesian value without factoring a shared real scale back out. */
expr_t *expr_separate_cartesian_for_display(const expr_t *expr)
{
    const expr_t *product_left;
    const expr_t *product_right;
    const expr_t *inner_left;
    const expr_t *inner_right;
    const expr_t *scale = NULL;
    const expr_t *real_source = NULL;
    expr_t *owned_scale = NULL;
    expr_t *direct_imaginary = NULL;
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    expr_t *imaginary_unit = NULL;
    expr_t *imaginary_term = NULL;
    expr_t *out = NULL;
    bool has_imaginary = false;
    bool subtract = false;
    int imaginary_sign = 1;

    direct_imaginary = expr_beautify_extract_imaginary_product_coefficient(expr, &imaginary_sign);
    if (direct_imaginary) {
        real = expr_const_zero();
        imaginary = direct_imaginary;
        direct_imaginary = NULL;
        if (imaginary_sign < 0)
            imaginary = expr_negate_owned(imaginary);
        imaginary_unit = expr_new_const(NUM_I);
        imaginary_term = imaginary_unit ? expr_new_binary_internal(&ops_mul, imaginary_unit, imaginary) : NULL;
        if (imaginary_term) {
            imaginary_unit = NULL;
            imaginary = NULL;
        }
        out = real && imaginary_term ? expr_new_binary_internal(&ops_add, real, imaginary_term) : NULL;
        if (out) {
            real = NULL;
            imaginary_term = NULL;
        }
        goto cleanup;
    }

    if (expr_match_binary_op(expr, EXPR_KIND_DIV, &product_left, &product_right) &&
        ((subtract = expr_match_binary_op(product_left, EXPR_KIND_SUB, &inner_left, &inner_right)) ||
         expr_match_binary_op(product_left, EXPR_KIND_ADD, &inner_left, &inner_right))) {
        direct_imaginary = expr_beautify_extract_imaginary_product_coefficient(inner_right, &imaginary_sign);
        owned_scale = expr_div_simplify_owned(expr_const_one(), expr_clone(product_right));
        scale = owned_scale;
        real_source = inner_left;
    } else if (expr_match_binary_op(expr, EXPR_KIND_MUL, &product_left, &product_right)) {
        if ((subtract = expr_match_binary_op(product_right, EXPR_KIND_SUB, &inner_left, &inner_right)) ||
            expr_match_binary_op(product_right, EXPR_KIND_ADD, &inner_left, &inner_right)) {
            direct_imaginary = expr_beautify_extract_imaginary_product_coefficient(inner_right, &imaginary_sign);
            scale = product_left;
            real_source = inner_left;
        } else if ((subtract = expr_match_binary_op(product_left, EXPR_KIND_SUB, &inner_left, &inner_right)) ||
                   expr_match_binary_op(product_left, EXPR_KIND_ADD, &inner_left, &inner_right)) {
            direct_imaginary = expr_beautify_extract_imaginary_product_coefficient(inner_right, &imaginary_sign);
            scale = product_right;
            real_source = inner_left;
        }
    }
    if (scale && real_source && direct_imaginary) {
        real = expr_mul_simplify_owned(expr_clone(scale), expr_clone(real_source));
        imaginary = expr_mul_simplify_owned(expr_clone(scale), direct_imaginary);
        direct_imaginary = NULL;
        imaginary_sign = subtract ? -imaginary_sign : imaginary_sign;
        imaginary_unit = expr_new_const(NUM_I);
        imaginary_term = imaginary_unit ? expr_new_binary_internal(&ops_mul, imaginary_unit, imaginary) : NULL;
        if (imaginary_term) {
            imaginary_unit = NULL;
            imaginary = NULL;
        }
        out = real && imaginary_term
                  ? expr_new_binary_internal(imaginary_sign < 0 ? &ops_sub : &ops_add, real, imaginary_term)
                  : NULL;
        if (out) {
            real = NULL;
            imaginary_term = NULL;
            expr_binding_expr_free(out->binding_expr);
            out->binding_expr = NULL;
        }
        goto cleanup;
    }

    if (!expr_beautify_cartesian_parts(expr, &real, &imaginary, &has_imaginary) || !has_imaginary)
        goto cleanup;
    imaginary_unit = expr_new_const(NUM_I);
    imaginary_term = imaginary_unit ? expr_new_binary_internal(&ops_mul, imaginary_unit, imaginary) : NULL;
    if (imaginary_term) {
        imaginary_unit = NULL;
        imaginary = NULL;
    }
    out = imaginary_term ? expr_new_binary_internal(&ops_add, real, imaginary_term) : NULL;
    if (out) {
        real = NULL;
        imaginary_term = NULL;
        expr_binding_expr_free(out->binding_expr);
        out->binding_expr = NULL;
    }

cleanup:
    expr_free(owned_scale);
    expr_free(direct_imaginary);
    expr_free(imaginary_term);
    expr_free(imaginary_unit);
    expr_free(imaginary);
    expr_free(real);
    return out;
}
