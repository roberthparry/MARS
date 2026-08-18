#define MARS_EXPR_INTEGRATE_INTERNAL_ACCESS
#include "expr_integrate_internal.h"

static expr_t *inverse_affine_coefficient(const expr_t *expr, const expr_t *wrt,
                                          expr_pattern_unary_affine_kind_t kind)
{
    expr_t *constant_expr = NULL;
    expr_t *coefficient_expr = NULL;
    number_t constant = num_new();
    number_t coefficient = num_new();

    if (match_affine_unary(expr, wrt, kind, &constant, &coefficient)) {
        coefficient_expr = expr_new_const(coefficient);
    } else if (expr && expr->a && expr_integrate_contains_imaginary_unit(expr) &&
               !match_symbolic_affine_constant_and_coeff(expr->a, wrt, &constant_expr, &coefficient_expr)) {
        expr_free(coefficient_expr);
        coefficient_expr = NULL;
    }
    if (expr_const_is_zero(coefficient_expr)) {
        expr_free(coefficient_expr);
        coefficient_expr = NULL;
    }
    num_destroy(&coefficient);
    num_destroy(&constant);
    expr_free(constant_expr);
    return coefficient_expr;
}

static expr_t *finish_inverse_affine_integral(expr_t *raw, expr_t *coefficient)
{
    expr_t *quotient = raw && coefficient ? expr_div(raw, coefficient) : NULL;
    expr_t *out = simplify_owned(quotient);

    expr_free(raw);
    expr_free(coefficient);
    return out;
}

static expr_t *inverse_argument_product(const expr_t *expr)
{
    expr_t *cartesian = expr_complex_unary_cartesian_for_display(expr);
    expr_t *out = expr && expr->a ? expr_mul(expr->a, cartesian ? cartesian : expr) : NULL;

    expr_free(cartesian);
    return out;
}

static expr_t *inverse_cartesian_unary_owned(expr_t *unary)
{
    expr_t *cartesian = expr_complex_unary_cartesian_for_display(unary);
    expr_t *separated_argument = NULL;

    if (!cartesian && unary && unary->ops && unary->ops->arity == EXPR_OP_UNARY && unary->a) {
        separated_argument = expr_separate_cartesian_for_display(unary->a);
        cartesian = separated_argument ? expr_simplify_try_complex_unary_cartesian(unary, separated_argument) : NULL;
        if (cartesian)
            separated_argument = NULL;
    }
    expr_free(separated_argument);
    if (cartesian) {
        expr_free(unary);
        return cartesian;
    }
    return unary;
}

static expr_t *inverse_cartesian_square(const expr_t *expr)
{
    if (!expr)
        return NULL;
    if (expr->ops && expr->ops->kind == EXPR_KIND_NEG)
        return inverse_cartesian_square(expr->a);
    if (expr->ops && expr->ops->kind == EXPR_KIND_MUL) {
        expr_t *left = inverse_cartesian_square(expr->a);
        expr_t *right = inverse_cartesian_square(expr->b);

        if (left && right)
            return expr_mul_simplify_owned(left, right);
        expr_free(right);
        expr_free(left);
        return NULL;
    }
    if (expr->ops && expr->ops->kind == EXPR_KIND_DIV) {
        expr_t *numerator = inverse_cartesian_square(expr->a);
        expr_t *denominator = inverse_cartesian_square(expr->b);

        if (numerator && denominator)
            return expr_div_simplify_owned(numerator, denominator);
        expr_free(denominator);
        expr_free(numerator);
        return NULL;
    }
    return expr_pow(expr, &NUM_TWO);
}

static expr_t *inverse_expanded_square_root(const expr_t *radicand)
{
    expr_t *expanded = expr_display_expanded(radicand);
    expr_t *preserved_expanded = expanded ? expr_expand_preserved_for_display(expanded) : NULL;
    const expr_t *cartesian_source = preserved_expanded ? preserved_expanded : expanded;
    expr_t *real = NULL;
    expr_t *imaginary = NULL;
    expr_t *real_squared = NULL;
    expr_t *imaginary_squared = NULL;
    expr_t *norm_squared = NULL;
    expr_t *norm = NULL;
    expr_t *real_sum = NULL;
    expr_t *imaginary_difference = NULL;
    expr_t *real_argument = NULL;
    expr_t *imaginary_argument = NULL;
    expr_t *real_root = NULL;
    expr_t *imaginary_root = NULL;
    expr_t *imaginary_absolute = NULL;
    expr_t *imaginary_orientation = NULL;
    expr_t *oriented_imaginary = NULL;
    expr_t *imaginary_unit = NULL;
    expr_t *imaginary_term = NULL;
    expr_t *out = NULL;
    bool has_imaginary = false;

    if (!cartesian_source || !expr_cartesian_parts_for_display(cartesian_source, &real, &imaginary, &has_imaginary) ||
        !has_imaginary)
        goto fallback;
    real_squared = inverse_cartesian_square(real);
    imaginary_squared = inverse_cartesian_square(imaginary);
    norm_squared = real_squared && imaginary_squared ? expr_add(real_squared, imaginary_squared) : NULL;
    norm = norm_squared ? expr_sqrt(norm_squared) : NULL;
    real_sum = norm ? expr_add(norm, real) : NULL;
    imaginary_difference = norm ? expr_sub(norm, real) : NULL;
    real_argument = real_sum ? expr_div_num(real_sum, &NUM_TWO) : NULL;
    imaginary_argument = imaginary_difference ? expr_div_num(imaginary_difference, &NUM_TWO) : NULL;
    real_root = real_argument ? expr_sqrt(real_argument) : NULL;
    imaginary_root = imaginary_argument ? expr_sqrt(imaginary_argument) : NULL;
    imaginary_absolute = expr_abs(imaginary);
    imaginary_orientation = imaginary_absolute ? expr_div(imaginary, imaginary_absolute) : NULL;
    oriented_imaginary =
        imaginary_orientation && imaginary_root ? expr_mul(imaginary_orientation, imaginary_root) : NULL;
    imaginary_unit = expr_new_const(NUM_I);
    imaginary_term = oriented_imaginary && imaginary_unit ? expr_mul(oriented_imaginary, imaginary_unit) : NULL;
    out = real_root && imaginary_term ? expr_add(real_root, imaginary_term) : NULL;
    goto cleanup;

fallback:
    out = expr_sqrt(cartesian_source ? cartesian_source : radicand);

cleanup:
    expr_free(imaginary_term);
    expr_free(imaginary_unit);
    expr_free(oriented_imaginary);
    expr_free(imaginary_orientation);
    expr_free(imaginary_absolute);
    expr_free(imaginary_root);
    expr_free(real_root);
    expr_free(imaginary_argument);
    expr_free(real_argument);
    expr_free(imaginary_difference);
    expr_free(real_sum);
    expr_free(norm);
    expr_free(norm_squared);
    expr_free(imaginary_squared);
    expr_free(real_squared);
    expr_free(imaginary);
    expr_free(real);
    expr_free(preserved_expanded);
    expr_free(expanded);
    return out;
}

/* Integrate an affine inverse secant. */
expr_t *integrate_asec_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *coeff = inverse_affine_coefficient(expr, wrt, EXPR_PATTERN_UNARY_ASEC);
    expr_t *u_asec_u;
    expr_t *acosh_u;
    expr_t *raw;

    if (!coeff)
        return NULL;

    u_asec_u = inverse_argument_product(expr);
    acosh_u = inverse_cartesian_unary_owned(expr_acosh(expr->a));
    raw = (u_asec_u && acosh_u) ? expr_sub(u_asec_u, acosh_u) : NULL;

    expr_free(acosh_u);
    expr_free(u_asec_u);
    return finish_inverse_affine_integral(raw, coeff);
}

/* Integrate an affine inverse cosecant. */
expr_t *integrate_acosec_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *coeff = inverse_affine_coefficient(expr, wrt, EXPR_PATTERN_UNARY_ACOSEC);
    expr_t *u_acosec_u;
    expr_t *acosh_u;
    expr_t *raw;

    if (!coeff)
        return NULL;

    u_acosec_u = inverse_argument_product(expr);
    acosh_u = inverse_cartesian_unary_owned(expr_acosh(expr->a));
    raw = (u_acosec_u && acosh_u) ? expr_add(u_acosec_u, acosh_u) : NULL;

    expr_free(acosh_u);
    expr_free(u_acosec_u);
    return finish_inverse_affine_integral(raw, coeff);
}

/* Integrate an affine inverse cotangent. */
expr_t *integrate_acot_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *coeff = inverse_affine_coefficient(expr, wrt, EXPR_PATTERN_UNARY_ACOT);
    expr_t *u_acot_u;
    expr_t *u_sq;
    expr_t *one_plus_u_sq;
    expr_t *log_term;
    expr_t *half_log_term;
    expr_t *raw;

    if (!coeff)
        return NULL;

    u_acot_u = inverse_argument_product(expr);
    u_sq = expr_pow(expr->a, &NUM_TWO);
    one_plus_u_sq = u_sq ? expr_add_num(u_sq, &NUM_ONE) : NULL;
    log_term = one_plus_u_sq ? inverse_cartesian_unary_owned(expr_log(one_plus_u_sq)) : NULL;
    half_log_term = log_term ? expr_mul_num(log_term, &NUM_HALF) : NULL;
    raw = (u_acot_u && half_log_term) ? expr_add(u_acot_u, half_log_term) : NULL;

    expr_free(half_log_term);
    expr_free(log_term);
    expr_free(one_plus_u_sq);
    expr_free(u_sq);
    expr_free(u_acot_u);
    return finish_inverse_affine_integral(raw, coeff);
}

/* Integrate an affine inverse sine. */
expr_t *integrate_asin_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *coeff = inverse_affine_coefficient(expr, wrt, EXPR_PATTERN_UNARY_ASIN);
    expr_t *u_asin_u;
    expr_t *u_sq;
    expr_t *one_minus_u_sq;
    expr_t *root;
    expr_t *raw;

    if (!coeff)
        return NULL;

    u_asin_u = inverse_argument_product(expr);
    u_sq = expr_pow(expr->a, &NUM_TWO);
    one_minus_u_sq = u_sq ? expr_sub_num(u_sq, &NUM_ONE) : NULL;
    root = one_minus_u_sq ? expr_neg(one_minus_u_sq) : NULL;
    if (root) {
        expr_t *tmp = inverse_expanded_square_root(root);
        expr_free(root);
        root = tmp;
    }
    raw = (u_asin_u && root) ? expr_add(u_asin_u, root) : NULL;

    expr_free(root);
    expr_free(one_minus_u_sq);
    expr_free(u_sq);
    expr_free(u_asin_u);
    return finish_inverse_affine_integral(raw, coeff);
}

/* Integrate an affine inverse cosine. */
expr_t *integrate_acos_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *coeff = inverse_affine_coefficient(expr, wrt, EXPR_PATTERN_UNARY_ACOS);
    expr_t *u_acos_u;
    expr_t *u_sq;
    expr_t *one_minus_u_sq;
    expr_t *root;
    expr_t *raw;

    if (!coeff)
        return NULL;

    u_acos_u = inverse_argument_product(expr);
    u_sq = expr_pow(expr->a, &NUM_TWO);
    one_minus_u_sq = u_sq ? expr_sub_num(u_sq, &NUM_ONE) : NULL;
    root = one_minus_u_sq ? expr_neg(one_minus_u_sq) : NULL;
    if (root) {
        expr_t *tmp = inverse_expanded_square_root(root);
        expr_free(root);
        root = tmp;
    }
    raw = (u_acos_u && root) ? expr_sub(u_acos_u, root) : NULL;

    expr_free(root);
    expr_free(one_minus_u_sq);
    expr_free(u_sq);
    expr_free(u_acos_u);
    return finish_inverse_affine_integral(raw, coeff);
}

/* Integrate an affine inverse tangent. */
expr_t *integrate_atan_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *constant = NULL;
    expr_t *coeff = NULL;
    expr_t *u_atan_u;
    expr_t *u_sq;
    expr_t *one_plus_u_sq;
    expr_t *log_term;
    expr_t *half_log_term;
    expr_t *raw;
    expr_t *out;

    if (!expr_is_op(expr, &ops_atan) || !expr->a ||
        !match_symbolic_affine_constant_and_coeff(expr->a, wrt, &constant, &coeff) || expr_const_is_zero(coeff)) {
        expr_free(coeff);
        expr_free(constant);
        return integrate_poly_times_rational_unary_by_parts(expr, wrt);
    }

    u_atan_u = inverse_argument_product(expr);
    u_sq = expr_pow(expr->a, &NUM_TWO);
    one_plus_u_sq = u_sq ? expr_add_num(u_sq, &NUM_ONE) : NULL;
    log_term = one_plus_u_sq ? inverse_cartesian_unary_owned(expr_log(one_plus_u_sq)) : NULL;
    half_log_term = log_term ? expr_mul_num(log_term, &NUM_HALF) : NULL;
    raw = (u_atan_u && half_log_term) ? expr_sub(u_atan_u, half_log_term) : NULL;

    expr_free(half_log_term);
    expr_free(log_term);
    expr_free(one_plus_u_sq);
    expr_free(u_sq);
    expr_free(u_atan_u);
    out = raw ? expr_div(raw, coeff) : NULL;
    expr_free(raw);
    expr_free(coeff);
    expr_free(constant);
    return simplify_owned(out);
}

/* Integrate an affine inverse hyperbolic sine. */
expr_t *integrate_asinh_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *coeff = inverse_affine_coefficient(expr, wrt, EXPR_PATTERN_UNARY_ASINH);
    expr_t *u_asinh_u;
    expr_t *u_sq;
    expr_t *one_plus_u_sq;
    expr_t *root;
    expr_t *raw;

    if (!coeff)
        return NULL;

    u_asinh_u = inverse_argument_product(expr);
    u_sq = expr_pow(expr->a, &NUM_TWO);
    one_plus_u_sq = u_sq ? expr_add_num(u_sq, &NUM_ONE) : NULL;
    root = one_plus_u_sq ? inverse_expanded_square_root(one_plus_u_sq) : NULL;
    raw = (u_asinh_u && root) ? expr_sub(u_asinh_u, root) : NULL;

    expr_free(root);
    expr_free(one_plus_u_sq);
    expr_free(u_sq);
    expr_free(u_asinh_u);
    return finish_inverse_affine_integral(raw, coeff);
}

/* Integrate an affine inverse hyperbolic cosine. */
expr_t *integrate_acosh_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *coeff = inverse_affine_coefficient(expr, wrt, EXPR_PATTERN_UNARY_ACOSH);
    expr_t *u_acosh_u;
    expr_t *u_minus_one;
    expr_t *u_plus_one;
    expr_t *sqrt1;
    expr_t *sqrt2;
    expr_t *root_product;
    expr_t *raw;

    if (!coeff)
        return NULL;

    u_acosh_u = inverse_argument_product(expr);
    u_minus_one = expr_sub_num(expr->a, &NUM_ONE);
    u_plus_one = expr_add_num(expr->a, &NUM_ONE);
    sqrt1 = u_minus_one ? inverse_expanded_square_root(u_minus_one) : NULL;
    sqrt2 = u_plus_one ? inverse_expanded_square_root(u_plus_one) : NULL;
    root_product = (sqrt1 && sqrt2) ? expr_mul(sqrt1, sqrt2) : NULL;
    raw = (u_acosh_u && root_product) ? expr_sub(u_acosh_u, root_product) : NULL;

    expr_free(root_product);
    expr_free(sqrt2);
    expr_free(sqrt1);
    expr_free(u_plus_one);
    expr_free(u_minus_one);
    expr_free(u_acosh_u);
    return finish_inverse_affine_integral(raw, coeff);
}

/* Integrate an affine inverse hyperbolic tangent. */
expr_t *integrate_atanh_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *coeff = inverse_affine_coefficient(expr, wrt, EXPR_PATTERN_UNARY_ATANH);
    expr_t *u_atanh_u;
    expr_t *u_sq;
    expr_t *one_minus_u_sq;
    expr_t *log_term;
    expr_t *half_log_term;
    expr_t *raw;

    if (!coeff)
        return NULL;

    u_atanh_u = inverse_argument_product(expr);
    u_sq = expr_pow(expr->a, &NUM_TWO);
    one_minus_u_sq = u_sq ? expr_sub_num(u_sq, &NUM_ONE) : NULL;
    log_term = one_minus_u_sq ? expr_neg(one_minus_u_sq) : NULL;
    if (log_term) {
        expr_t *tmp = inverse_cartesian_unary_owned(expr_log(log_term));
        expr_free(log_term);
        log_term = tmp;
    }
    half_log_term = log_term ? expr_mul_num(log_term, &NUM_HALF) : NULL;
    raw = (u_atanh_u && half_log_term) ? expr_add(u_atanh_u, half_log_term) : NULL;

    expr_free(half_log_term);
    expr_free(log_term);
    expr_free(one_minus_u_sq);
    expr_free(u_sq);
    expr_free(u_atanh_u);
    return finish_inverse_affine_integral(raw, coeff);
}

/* Integrate an affine inverse hyperbolic secant. */
expr_t *integrate_asech_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *coeff = inverse_affine_coefficient(expr, wrt, EXPR_PATTERN_UNARY_ASECH);
    expr_t *u_asech_u;
    expr_t *asin_u;
    expr_t *raw;

    if (!coeff)
        return NULL;

    u_asech_u = inverse_argument_product(expr);
    asin_u = inverse_cartesian_unary_owned(expr_asin(expr->a));
    raw = (u_asech_u && asin_u) ? expr_add(u_asech_u, asin_u) : NULL;

    expr_free(asin_u);
    expr_free(u_asech_u);
    return finish_inverse_affine_integral(raw, coeff);
}

/* Integrate an affine inverse hyperbolic cosecant. */
expr_t *integrate_acosech_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *coeff = inverse_affine_coefficient(expr, wrt, EXPR_PATTERN_UNARY_ACOSECH);
    expr_t *u_acosech_u;
    expr_t *asinh_u;
    expr_t *raw;

    if (!coeff)
        return NULL;

    u_acosech_u = inverse_argument_product(expr);
    asinh_u = inverse_cartesian_unary_owned(expr_asinh(expr->a));
    raw = (u_acosech_u && asinh_u) ? expr_add(u_acosech_u, asinh_u) : NULL;

    expr_free(asinh_u);
    expr_free(u_acosech_u);
    return finish_inverse_affine_integral(raw, coeff);
}

/* Integrate an affine inverse hyperbolic cotangent. */
expr_t *integrate_acoth_rule(const expr_t *expr, const expr_t *wrt)
{
    expr_t *coeff = inverse_affine_coefficient(expr, wrt, EXPR_PATTERN_UNARY_ACOTH);
    expr_t *u_acoth_u;
    expr_t *u_sq;
    expr_t *one_minus_u_sq;
    expr_t *log_term;
    expr_t *half_log_term;
    expr_t *raw;

    if (!coeff)
        return NULL;

    u_acoth_u = inverse_argument_product(expr);
    u_sq = expr_pow(expr->a, &NUM_TWO);
    one_minus_u_sq = u_sq ? expr_sub_num(u_sq, &NUM_ONE) : NULL;
    log_term = one_minus_u_sq ? expr_neg(one_minus_u_sq) : NULL;
    if (log_term) {
        expr_t *tmp = inverse_cartesian_unary_owned(expr_log(log_term));
        expr_free(log_term);
        log_term = tmp;
    }
    half_log_term = log_term ? expr_mul_num(log_term, &NUM_HALF) : NULL;
    raw = (u_acoth_u && half_log_term) ? expr_add(u_acoth_u, half_log_term) : NULL;

    expr_free(half_log_term);
    expr_free(log_term);
    expr_free(one_minus_u_sq);
    expr_free(u_sq);
    expr_free(u_acoth_u);
    return finish_inverse_affine_integral(raw, coeff);
}
