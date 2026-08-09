#include <stdbool.h>

#define MARS_EXPR_INTEGRATE_INTERNAL_ACCESS
#include "expr_integrate_internal.h"

static bool is_wrt_square_power(const expr_t *expr, const expr_t *wrt);

static bool match_wrt_monomial_degree(const expr_t *expr, const expr_t *wrt, unsigned int *degree_out)
{
    if (!expr || !wrt || !degree_out)
        return false;

    if (is_wrt_symbolic_affine_leaf(expr, wrt)) {
        *degree_out = 1u;
        return true;
    }

    if (is_wrt_square_power(expr, wrt)) {
        *degree_out = 2u;
        return true;
    }

    return false;
}

static bool match_symbolic_affine_power_factor(const expr_t *expr, const expr_t *wrt, expr_t **base_out,
                                               expr_t **constant_out, expr_t **coeff_out, number_t *exponent_out)
{
    number_t exponent = num_new();
    const expr_t *base = NULL;
    expr_t *base_clone = NULL;
    expr_t *constant = NULL;
    expr_t *coeff = NULL;
    bool ok = false;

    if (!expr || !wrt || !base_out || !constant_out || !coeff_out || !exponent_out) {
        num_destroy(&exponent);
        return false;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_SQRT && expr->a) {
        base = expr->a;
        ok = match_symbolic_affine_constant_and_coeff(base, wrt, &constant, &coeff);
        if (ok) {
            base_clone = expr_clone(base);
            if (!base_clone)
                ok = false;
            else {
                num_destroy(exponent_out);
                *exponent_out = num_clone(NUM_HALF);
            }
        }
    } else if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D && expr->a) {
        base = expr->a;
        ok = match_symbolic_affine_constant_and_coeff(base, wrt, &constant, &coeff);
        if (ok) {
            base_clone = expr_clone(base);
            if (!base_clone)
                ok = false;
            else {
                num_destroy(exponent_out);
                *exponent_out = num_clone(expr->c);
            }
        }
    } else if (expr->ops && expr->ops->kind == EXPR_KIND_POW && expr->a && expr->b &&
               expr_match_const_value(expr->b, &exponent)) {
        base = expr->a;
        ok = match_symbolic_affine_constant_and_coeff(base, wrt, &constant, &coeff);
        if (ok) {
            base_clone = expr_clone(base);
            if (!base_clone)
                ok = false;
            else {
                num_destroy(exponent_out);
                *exponent_out = num_clone(exponent);
            }
        }
    } else {
        base = expr;
        ok = match_symbolic_affine_constant_and_coeff(base, wrt, &constant, &coeff);
        if (ok) {
            base_clone = expr_clone(base);
            if (!base_clone)
                ok = false;
            else {
                num_destroy(exponent_out);
                *exponent_out = num_clone(NUM_ONE);
            }
        }
    }

    if (ok) {
        *base_out = base_clone;
        *constant_out = constant;
        *coeff_out = coeff;
        base_clone = NULL;
        constant = NULL;
        coeff = NULL;
    }

    num_destroy(&exponent);
    expr_free(coeff);
    expr_free(constant);
    expr_free(base_clone);
    return ok;
}

static bool match_symbolic_constant_plus_minus_wrt(const expr_t *expr, const expr_t *wrt, expr_t **constant_out,
                                                   bool *is_minus_out)
{
    if (!expr || !wrt || !constant_out || !is_minus_out)
        return false;

    if (expr_is_op(expr, &ops_add)) {
        if (!depends_on_wrt(expr->a, wrt) && is_wrt_symbolic_affine_leaf(expr->b, wrt)) {
            *constant_out = expr_clone(expr->a);
            *is_minus_out = false;
            return *constant_out != NULL;
        }
        if (!depends_on_wrt(expr->b, wrt) && is_wrt_symbolic_affine_leaf(expr->a, wrt)) {
            *constant_out = expr_clone(expr->b);
            *is_minus_out = false;
            return *constant_out != NULL;
        }
        if (!depends_on_wrt(expr->a, wrt) && is_negated_wrt_symbolic_affine_leaf(expr->b, wrt)) {
            *constant_out = expr_clone(expr->a);
            *is_minus_out = true;
            return *constant_out != NULL;
        }
        if (!depends_on_wrt(expr->b, wrt) && is_negated_wrt_symbolic_affine_leaf(expr->a, wrt)) {
            *constant_out = expr_clone(expr->b);
            *is_minus_out = true;
            return *constant_out != NULL;
        }
    }

    if (expr_is_op(expr, &ops_sub) && !depends_on_wrt(expr->a, wrt) && is_wrt_symbolic_affine_leaf(expr->b, wrt)) {
        *constant_out = expr_clone(expr->a);
        *is_minus_out = true;
        return *constant_out != NULL;
    }

    return false;
}

static bool match_symbolic_wrt_minus_constant(const expr_t *expr, const expr_t *wrt, expr_t **constant_out)
{
    if (!expr || !wrt || !constant_out)
        return false;

    if (expr_is_op(expr, &ops_sub) && is_wrt_symbolic_affine_leaf(expr->a, wrt) && !depends_on_wrt(expr->b, wrt)) {
        *constant_out = expr_clone(expr->b);
        return *constant_out != NULL;
    }

    return false;
}

bool match_symbolic_affine_constant_and_coeff(const expr_t *expr, const expr_t *wrt, expr_t **constant_term_out,
                                              expr_t **coeff_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool is_sub = false;
    expr_t *coeff = NULL;

    if (!expr || !wrt || !constant_term_out || !coeff_out)
        return false;

    coeff = match_symbolic_wrt_factor_coeff(expr, wrt);
    if (coeff) {
        *constant_term_out = expr_new_const(NUM_ZERO);
        if (!*constant_term_out) {
            expr_free(coeff);
            return false;
        }
        *coeff_out = coeff;
        return true;
    }

    if (!expr_match_add_sub_expr(expr, &left, &right, &is_sub))
        return false;

    coeff = match_symbolic_wrt_factor_coeff(left, wrt);
    if (coeff && !depends_on_wrt(right, wrt)) {
        *constant_term_out = is_sub ? expr_negate_owned(expr_clone(right)) : expr_clone(right);
        if (!*constant_term_out) {
            expr_free(coeff);
            return false;
        }
        *coeff_out = coeff;
        return true;
    }
    expr_free(coeff);

    coeff = match_symbolic_wrt_factor_coeff(right, wrt);
    if (coeff && !depends_on_wrt(left, wrt)) {
        *constant_term_out = expr_clone(left);
        if (!*constant_term_out) {
            expr_free(coeff);
            return false;
        }
        *coeff_out = is_sub ? expr_negate_owned(coeff) : coeff;
        return *coeff_out != NULL;
    }
    expr_free(coeff);
    return false;
}

static bool symbolic_depends_on_wrt_leaf(const expr_t *expr, const expr_t *wrt)
{
    if (!expr || !wrt)
        return false;
    if (is_wrt_symbolic_affine_leaf(expr, wrt))
        return true;
    return symbolic_depends_on_wrt_leaf(expr->a, wrt) || symbolic_depends_on_wrt_leaf(expr->b, wrt);
}

static bool match_symbolic_wrt_monomial_coeff_rec(const expr_t *expr, const expr_t *wrt, unsigned int *degree_out,
                                                  expr_t **coeff_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    unsigned int left_degree = 0u;
    unsigned int right_degree = 0u;
    expr_t *left_coeff = NULL;
    expr_t *right_coeff = NULL;
    expr_t *coeff = NULL;

    if (!expr || !wrt || !degree_out || !coeff_out)
        return false;

    if (is_wrt_symbolic_affine_leaf(expr, wrt)) {
        *degree_out = 1u;
        *coeff_out = expr_new_const(NUM_ONE);
        return *coeff_out != NULL;
    }

    if (is_wrt_square_power(expr, wrt)) {
        *degree_out = 2u;
        *coeff_out = expr_new_const(NUM_ONE);
        return *coeff_out != NULL;
    }

    if (expr_is_neg(expr)) {
        if (!match_symbolic_wrt_monomial_coeff_rec(expr->a, wrt, degree_out, &coeff))
            return false;
        *coeff_out = expr_negate_owned(coeff);
        return *coeff_out != NULL;
    }

    if (expr_match_mul_expr(expr, &left, &right)) {
        if (!match_symbolic_wrt_monomial_coeff_rec(left, wrt, &left_degree, &left_coeff) ||
            !match_symbolic_wrt_monomial_coeff_rec(right, wrt, &right_degree, &right_coeff)) {
            expr_free(left_coeff);
            expr_free(right_coeff);
            return false;
        }
        coeff = expr_mul(left_coeff, right_coeff);
        expr_free(right_coeff);
        expr_free(left_coeff);
        *degree_out = left_degree + right_degree;
        *coeff_out = simplify_owned(coeff);
        return *coeff_out != NULL;
    }

    if (expr_is_op(expr, &ops_div) && expr->a && expr->b && !symbolic_depends_on_wrt_leaf(expr->b, wrt)) {
        if (!match_symbolic_wrt_monomial_coeff_rec(expr->a, wrt, degree_out, &left_coeff))
            return false;
        right_coeff = expr_clone(expr->b);
        coeff = (left_coeff && right_coeff) ? expr_div(left_coeff, right_coeff) : NULL;
        expr_free(right_coeff);
        expr_free(left_coeff);
        *coeff_out = simplify_owned(coeff);
        return *coeff_out != NULL;
    }

    if (!symbolic_depends_on_wrt_leaf(expr, wrt)) {
        *degree_out = 0u;
        *coeff_out = expr_clone(expr);
        return *coeff_out != NULL;
    }

    return false;
}

static bool match_symbolic_wrt_monomial_coeff(const expr_t *expr, const expr_t *wrt, unsigned int degree,
                                              expr_t **coeff_out)
{
    unsigned int matched_degree = 0u;
    expr_t *coeff = NULL;

    if (!expr || !wrt || !coeff_out)
        return false;

    if (!match_symbolic_wrt_monomial_coeff_rec(expr, wrt, &matched_degree, &coeff) || matched_degree != degree) {
        expr_free(coeff);
        return false;
    }

    *coeff_out = coeff;
    return true;
}

static bool add_quadratic_coeff_term(expr_t **slot, expr_t *term)
{
    expr_t *sum = NULL;

    if (!slot || !term)
        return false;

    sum = expr_add_owned(*slot, term);
    *slot = simplify_owned(sum);
    return *slot != NULL;
}

static bool collect_symbolic_quadratic_coeffs(const expr_t *expr, const expr_t *wrt, bool negate, expr_t **quad_io,
                                              expr_t **linear_io, expr_t **constant_io)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool is_sub = false;
    expr_t *term = NULL;

    if (!expr || !wrt || !quad_io || !linear_io || !constant_io)
        return false;

    if (expr_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        return collect_symbolic_quadratic_coeffs(left, wrt, negate, quad_io, linear_io, constant_io) &&
               collect_symbolic_quadratic_coeffs(right, wrt, negate ^ is_sub, quad_io, linear_io, constant_io);
    }

    if (expr_is_neg(expr))
        return collect_symbolic_quadratic_coeffs(expr->a, wrt, !negate, quad_io, linear_io, constant_io);

    if (match_symbolic_wrt_monomial_coeff(expr, wrt, 2u, &term)) {
        if (negate)
            term = expr_negate_owned(term);
        return add_quadratic_coeff_term(quad_io, term);
    }

    if (match_symbolic_wrt_monomial_coeff(expr, wrt, 1u, &term)) {
        if (negate)
            term = expr_negate_owned(term);
        return add_quadratic_coeff_term(linear_io, term);
    }

    if (!depends_on_wrt(expr, wrt)) {
        term = expr_clone(expr);
        if (negate)
            term = expr_negate_owned(term);
        return add_quadratic_coeff_term(constant_io, term);
    }

    return false;
}

bool match_symbolic_quadratic_coeffs(const expr_t *expr, const expr_t *wrt, expr_t **quad_out, expr_t **linear_out,
                                     expr_t **constant_out)
{
    expr_t *quad = NULL;
    expr_t *linear = NULL;
    expr_t *constant = NULL;
    bool ok = false;

    if (!expr || !wrt || !quad_out || !linear_out || !constant_out)
        return false;

    quad = expr_new_const(NUM_ZERO);
    linear = expr_new_const(NUM_ZERO);
    constant = expr_new_const(NUM_ZERO);
    if (!quad || !linear || !constant)
        goto cleanup;

    if (!collect_symbolic_quadratic_coeffs(expr, wrt, false, &quad, &linear, &constant) || expr_is_exact_zero(quad))
        goto cleanup;

    *quad_out = quad;
    *linear_out = linear;
    *constant_out = constant;
    quad = NULL;
    linear = NULL;
    constant = NULL;
    ok = true;

cleanup:
    expr_free(constant);
    expr_free(linear);
    expr_free(quad);
    return ok;
}

expr_t *integrate_sqrt_wrt_over_symbolic_unit_affine(const expr_t *base, const expr_t *wrt)
{
    expr_t *constant = NULL;
    expr_t *tmp_left = NULL;
    expr_t *tmp_right = NULL;
    expr_t *denom = NULL;
    expr_t *quotient = NULL;
    expr_t *arg = NULL;
    expr_t *inverse = NULL;
    expr_t *scaled_inverse = NULL;
    expr_t *product = NULL;
    expr_t *root = NULL;
    expr_t *out = NULL;
    bool is_minus = false;

    if (!base || !wrt)
        goto cleanup;

    if (expr_is_op(base, &ops_div) && is_wrt_symbolic_affine_leaf(base->a, wrt) &&
        match_symbolic_constant_plus_minus_wrt(base->b, wrt, &constant, &is_minus)) {
        denom = expr_clone(base->b);
    } else if (expr_is_op(base, &ops_div) && is_negated_wrt_symbolic_affine_leaf(base->a, wrt) &&
               match_symbolic_wrt_minus_constant(base->b, wrt, &constant)) {
        tmp_left = expr_clone(constant);
        tmp_right = expr_clone(wrt);
        denom = (tmp_left && tmp_right) ? expr_sub(tmp_left, tmp_right) : NULL;
        expr_free(tmp_right);
        expr_free(tmp_left);
        tmp_right = NULL;
        tmp_left = NULL;
        is_minus = true;
    } else if (expr_is_neg(base) && expr_is_op(base->a, &ops_div) && is_wrt_symbolic_affine_leaf(base->a->a, wrt) &&
               match_symbolic_wrt_minus_constant(base->a->b, wrt, &constant)) {
        tmp_left = expr_clone(constant);
        tmp_right = expr_clone(wrt);
        denom = (tmp_left && tmp_right) ? expr_sub(tmp_left, tmp_right) : NULL;
        expr_free(tmp_right);
        expr_free(tmp_left);
        tmp_right = NULL;
        tmp_left = NULL;
        is_minus = true;
    } else {
        goto cleanup;
    }

    tmp_left = expr_clone(wrt);
    product = (tmp_left && denom) ? expr_mul(tmp_left, denom) : NULL;
    expr_free(tmp_left);
    tmp_left = NULL;
    root = product ? expr_sqrt(product) : NULL;
    tmp_left = expr_clone(wrt);
    tmp_right = expr_clone(denom);
    quotient = (tmp_left && tmp_right) ? expr_div(tmp_left, tmp_right) : NULL;
    expr_free(tmp_right);
    expr_free(tmp_left);
    tmp_right = NULL;
    tmp_left = NULL;
    arg = quotient ? expr_sqrt(quotient) : NULL;

    if (is_minus) {
        inverse = arg ? expr_atan(arg) : NULL;
        scaled_inverse = (constant && inverse) ? expr_mul(constant, inverse) : NULL;
        out = expr_sub(scaled_inverse, root);
    } else {
        inverse = arg ? expr_atanh(arg) : NULL;
        scaled_inverse = (constant && inverse) ? expr_mul(constant, inverse) : NULL;
        out = expr_sub(root, scaled_inverse);
    }
    out = simplify_owned(out);

cleanup:
    expr_free(tmp_right);
    expr_free(tmp_left);
    expr_free(root);
    expr_free(product);
    expr_free(scaled_inverse);
    expr_free(inverse);
    expr_free(arg);
    expr_free(quotient);
    expr_free(denom);
    expr_free(constant);
    return out;
}

expr_t *integrate_wrt_over_symbolic_affine_root(const expr_t *expr, const expr_t *wrt)
{
    expr_t *constant_term = NULL;
    expr_t *coeff = NULL;
    expr_t *tmp_left = NULL;
    expr_t *tmp_right = NULL;
    expr_t *base = NULL;
    expr_t *root = NULL;
    expr_t *coeff_sq = NULL;
    expr_t *three_coeff_sq = NULL;
    expr_t *scaled_x = NULL;
    expr_t *two_constant = NULL;
    expr_t *linear_factor = NULL;
    expr_t *product = NULL;
    expr_t *scaled_product = NULL;
    expr_t *out = NULL;
    expr_t *three = expr_from_string("{ 3 }", NULL);
    expr_t *two = expr_from_string("{ 2 }", NULL);

    if (!expr || !wrt || !expr->a || !expr->b || !is_wrt_symbolic_affine_leaf(expr->a, wrt) || !expr->b->ops ||
        expr->b->ops->kind != EXPR_KIND_SQRT || !expr->b->a ||
        !match_symbolic_affine_constant_and_coeff(expr->b->a, wrt, &constant_term, &coeff))
        goto cleanup;

    base = expr_clone(expr->b->a);
    tmp_left = expr_clone(base);
    root = tmp_left ? expr_sqrt(tmp_left) : NULL;
    expr_free(tmp_left);
    tmp_left = NULL;
    tmp_left = expr_clone(coeff);
    coeff_sq = tmp_left ? expr_pow(tmp_left, &NUM_TWO) : NULL;
    expr_free(tmp_left);
    tmp_left = NULL;
    three_coeff_sq = (three && coeff_sq) ? expr_mul(three, coeff_sq) : NULL;
    tmp_left = expr_clone(coeff);
    tmp_right = expr_clone(wrt);
    scaled_x = (tmp_left && tmp_right) ? expr_mul(tmp_left, tmp_right) : NULL;
    expr_free(tmp_right);
    expr_free(tmp_left);
    tmp_right = NULL;
    tmp_left = NULL;
    two_constant = constant_term ? expr_mul_num(constant_term, &NUM_TWO) : NULL;
    linear_factor = (scaled_x && two_constant) ? expr_sub(scaled_x, two_constant) : NULL;
    product = (linear_factor && root) ? expr_mul(linear_factor, root) : NULL;
    scaled_product = (two && product) ? expr_mul(two, product) : NULL;
    out = (scaled_product && three_coeff_sq) ? expr_div(scaled_product, three_coeff_sq) : NULL;
    out = simplify_owned(out);

cleanup:
    expr_free(tmp_right);
    expr_free(tmp_left);
    expr_free(scaled_product);
    expr_free(linear_factor);
    expr_free(two_constant);
    expr_free(scaled_x);
    expr_free(three_coeff_sq);
    expr_free(coeff_sq);
    expr_free(product);
    expr_free(root);
    expr_free(base);
    expr_free(coeff);
    expr_free(constant_term);
    expr_free(two);
    expr_free(three);
    return out;
}

expr_t *integrate_symbolic_monomial_times_affine_power(const expr_t *expr, const expr_t *wrt)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    const expr_t *power_expr = NULL;
    unsigned int degree = 0u;
    expr_t *base = NULL;
    expr_t *constant = NULL;
    expr_t *coeff = NULL;
    expr_t *coeff_power = NULL;
    expr_t *denom = NULL;
    expr_t *u_pow = NULL;
    expr_t *term1 = NULL;
    expr_t *term2 = NULL;
    expr_t *term3 = NULL;
    expr_t *scaled_const = NULL;
    expr_t *const_sq = NULL;
    expr_t *numerator = NULL;
    expr_t *out = NULL;
    expr_t *scale = NULL;
    expr_t *tmp_left = NULL;
    number_t three = num_create_from_long(3);
    number_t exponent = num_new();
    number_t next1 = num_new();
    number_t next2 = num_new();
    number_t next3 = num_new();

    if (!expr || !wrt)
        goto cleanup;

    if (expr_match_mul_expr(expr, &left, &right)) {
        if (match_wrt_monomial_degree(left, wrt, &degree)) {
            power_expr = right;
        } else if (match_wrt_monomial_degree(right, wrt, &degree)) {
            power_expr = left;
        } else {
            goto cleanup;
        }
    } else if (expr->ops && expr->ops->kind == EXPR_KIND_DIV && expr->a && expr->b) {
        if (!match_wrt_monomial_degree(expr->a, wrt, &degree))
            goto cleanup;
        power_expr = expr->b;
    } else {
        goto cleanup;
    }

    if ((degree != 1u && degree != 2u) ||
        !match_symbolic_affine_power_factor(power_expr, wrt, &base, &constant, &coeff, &exponent))
        goto cleanup;

    if (expr->ops && expr->ops->kind == EXPR_KIND_DIV && degree == 1u)
        goto cleanup;

    if (expr->ops && expr->ops->kind == EXPR_KIND_DIV) {
        number_t neg_exponent = num_neg(exponent);

        num_destroy(&exponent);
        exponent = neg_exponent;
    }

    num_destroy(&next1);
    next1 = num_add(exponent, NUM_ONE);
    num_destroy(&next2);
    next2 = num_add(exponent, NUM_TWO);
    if (degree == 2u) {
        num_destroy(&next3);
        next3 = num_add(next2, NUM_ONE);
    }

    if (num_eq(next1, NUM_ZERO) || num_eq(next2, NUM_ZERO) || (degree == 2u && num_eq(next3, NUM_ZERO)))
        goto cleanup;

    tmp_left = expr_clone(coeff);
    coeff_power = tmp_left ? expr_pow(tmp_left, degree == 1u ? &NUM_TWO : &three) : NULL;
    expr_free(tmp_left);
    tmp_left = NULL;

    tmp_left = expr_clone(base);
    u_pow = tmp_left ? expr_pow(tmp_left, degree == 1u ? &next2 : &next3) : NULL;
    expr_free(tmp_left);
    tmp_left = NULL;
    scale = expr_new_const(degree == 1u ? next2 : next3);
    tmp_left = expr_clone(coeff_power);
    denom = (tmp_left && scale) ? expr_mul(tmp_left, scale) : NULL;
    expr_free(tmp_left);
    tmp_left = NULL;
    term1 = (u_pow && denom) ? expr_div(u_pow, denom) : NULL;
    expr_free(u_pow);
    expr_free(denom);
    expr_free(scale);
    u_pow = NULL;
    denom = NULL;
    scale = NULL;

    tmp_left = expr_clone(constant);
    scaled_const = tmp_left ? expr_mul_num(tmp_left, degree == 1u ? &NUM_ONE : &NUM_TWO) : NULL;
    expr_free(tmp_left);
    tmp_left = NULL;

    tmp_left = expr_clone(base);
    u_pow = tmp_left ? expr_pow(tmp_left, degree == 1u ? &next1 : &next2) : NULL;
    expr_free(tmp_left);
    tmp_left = NULL;
    numerator = (scaled_const && u_pow) ? expr_mul(scaled_const, u_pow) : NULL;
    scale = expr_new_const(degree == 1u ? next1 : next2);
    tmp_left = expr_clone(coeff_power);
    denom = (tmp_left && scale) ? expr_mul(tmp_left, scale) : NULL;
    expr_free(tmp_left);
    tmp_left = NULL;
    term2 = (numerator && denom) ? expr_div(numerator, denom) : NULL;
    expr_free(numerator);
    expr_free(u_pow);
    expr_free(denom);
    expr_free(scale);
    numerator = NULL;
    u_pow = NULL;
    denom = NULL;
    scale = NULL;
    expr_free(scaled_const);
    scaled_const = NULL;

    if (degree == 1u) {
        out = term1 ? expr_add_owned(term1, expr_negate_owned(term2)) : NULL;
        term1 = NULL;
        term2 = NULL;
    } else {
        tmp_left = expr_clone(constant);
        const_sq = tmp_left ? expr_pow(tmp_left, &NUM_TWO) : NULL;
        expr_free(tmp_left);
        tmp_left = NULL;

        tmp_left = expr_clone(base);
        u_pow = tmp_left ? expr_pow(tmp_left, &next1) : NULL;
        expr_free(tmp_left);
        tmp_left = NULL;
        numerator = (const_sq && u_pow) ? expr_mul(const_sq, u_pow) : NULL;
        scale = expr_new_const(next1);
        tmp_left = expr_clone(coeff_power);
        denom = (tmp_left && scale) ? expr_mul(tmp_left, scale) : NULL;
        expr_free(tmp_left);
        tmp_left = NULL;
        term3 = (numerator && denom) ? expr_div(numerator, denom) : NULL;
        expr_free(numerator);
        expr_free(u_pow);
        expr_free(denom);
        expr_free(scale);
        numerator = NULL;
        u_pow = NULL;
        denom = NULL;
        scale = NULL;

        out = term1 ? expr_add_owned(term1, expr_negate_owned(term2)) : NULL;
        term1 = NULL;
        term2 = NULL;
        out = expr_add_owned(out, term3);
        term3 = NULL;
    }

    out = simplify_owned(out);

cleanup:
    num_destroy(&three);
    num_destroy(&next3);
    num_destroy(&next2);
    num_destroy(&next1);
    num_destroy(&exponent);
    expr_free(term3);
    expr_free(numerator);
    expr_free(const_sq);
    expr_free(scaled_const);
    expr_free(term2);
    expr_free(term1);
    expr_free(u_pow);
    expr_free(denom);
    expr_free(scale);
    expr_free(tmp_left);
    expr_free(coeff_power);
    expr_free(coeff);
    expr_free(constant);
    expr_free(base);
    return out;
}

typedef enum {
    SYMBOLIC_SQUARE_PLUS,
    SYMBOLIC_SQUARE_A2_MINUS_X2,
    SYMBOLIC_SQUARE_X2_MINUS_A2
} symbolic_square_family_kind_t;

static bool match_square_power_base(const expr_t *expr, const expr_t **base_out)
{
    number_t exponent = num_new();
    bool ok = false;

    if (!expr || !base_out) {
        num_destroy(&exponent);
        return false;
    }

    if (expr->ops && expr->ops->kind == EXPR_KIND_POW_D && expr->a && num_eq(expr->c, NUM_TWO)) {
        *base_out = expr->a;
        ok = true;
    } else if (expr->ops && expr->ops->kind == EXPR_KIND_POW && expr->a && expr->b &&
               expr_match_const_value(expr->b, &exponent) && num_eq(exponent, NUM_TWO)) {
        *base_out = expr->a;
        ok = true;
    }

    num_destroy(&exponent);
    return ok;
}

static bool is_wrt_square_power(const expr_t *expr, const expr_t *wrt)
{
    const expr_t *base = NULL;

    return match_square_power_base(expr, &base) && is_wrt_symbolic_affine_leaf(base, wrt);
}

static bool is_negated_wrt_square_power(const expr_t *expr, const expr_t *wrt)
{
    return expr && expr_is_neg(expr) && is_wrt_square_power(expr->a, wrt);
}

static bool match_symbolic_constant_square_power(const expr_t *expr, const expr_t *wrt, expr_t **base_out)
{
    const expr_t *base = NULL;

    if (!expr || !wrt || !base_out)
        return false;

    if (!match_square_power_base(expr, &base) || depends_on_wrt(base, wrt))
        return false;

    *base_out = expr_clone(base);
    return *base_out != NULL;
}

static bool match_negated_symbolic_constant_square_power(const expr_t *expr, const expr_t *wrt, expr_t **base_out)
{
    return expr && expr_is_neg(expr) && match_symbolic_constant_square_power(expr->a, wrt, base_out);
}

static bool match_symbolic_square_family(const expr_t *expr, const expr_t *wrt, expr_t **param_out,
                                         symbolic_square_family_kind_t *kind_out)
{
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    bool is_sub = false;
    expr_t *param = NULL;
    bool left_wrt = false;
    bool right_wrt = false;
    bool left_neg_wrt = false;
    bool right_neg_wrt = false;
    bool left_const = false;
    bool right_const = false;
    bool left_neg_const = false;
    bool right_neg_const = false;

    if (!expr || !wrt || !param_out || !kind_out)
        return false;

    if (!expr_match_add_sub_expr(expr, &left, &right, &is_sub))
        return false;

    left_wrt = is_wrt_square_power(left, wrt);
    right_wrt = is_wrt_square_power(right, wrt);
    left_neg_wrt = is_negated_wrt_square_power(left, wrt);
    right_neg_wrt = is_negated_wrt_square_power(right, wrt);
    left_const = match_symbolic_constant_square_power(left, wrt, &param);
    expr_free(param);
    param = NULL;
    right_const = match_symbolic_constant_square_power(right, wrt, &param);
    expr_free(param);
    param = NULL;
    left_neg_const = match_negated_symbolic_constant_square_power(left, wrt, &param);
    expr_free(param);
    param = NULL;
    right_neg_const = match_negated_symbolic_constant_square_power(right, wrt, &param);
    expr_free(param);
    param = NULL;

    if (!is_sub) {
        if (left_wrt && match_symbolic_constant_square_power(right, wrt, &param)) {
            *param_out = param;
            *kind_out = SYMBOLIC_SQUARE_PLUS;
            return true;
        }
        if (right_wrt && match_symbolic_constant_square_power(left, wrt, &param)) {
            *param_out = param;
            *kind_out = SYMBOLIC_SQUARE_PLUS;
            return true;
        }
        if (left_const && right_neg_wrt && match_symbolic_constant_square_power(left, wrt, &param)) {
            *param_out = param;
            *kind_out = SYMBOLIC_SQUARE_A2_MINUS_X2;
            return true;
        }
        if (left_wrt && right_neg_const && match_negated_symbolic_constant_square_power(right, wrt, &param)) {
            *param_out = param;
            *kind_out = SYMBOLIC_SQUARE_X2_MINUS_A2;
            return true;
        }
        if (right_const && left_neg_wrt && match_symbolic_constant_square_power(right, wrt, &param)) {
            *param_out = param;
            *kind_out = SYMBOLIC_SQUARE_A2_MINUS_X2;
            return true;
        }
        if (right_wrt && left_neg_const && match_negated_symbolic_constant_square_power(left, wrt, &param)) {
            *param_out = param;
            *kind_out = SYMBOLIC_SQUARE_X2_MINUS_A2;
            return true;
        }
        return false;
    }

    if (left_const && right_wrt && match_symbolic_constant_square_power(left, wrt, &param)) {
        *param_out = param;
        *kind_out = SYMBOLIC_SQUARE_A2_MINUS_X2;
        return true;
    }
    expr_free(param);
    param = NULL;

    if (left_wrt && right_const && match_symbolic_constant_square_power(right, wrt, &param)) {
        *param_out = param;
        *kind_out = SYMBOLIC_SQUARE_X2_MINUS_A2;
        return true;
    }

    return false;
}

expr_t *integrate_symbolic_square_family_root(const expr_t *quadratic, const expr_t *wrt)
{
    symbolic_square_family_kind_t kind;
    expr_t *tmp_left = NULL;
    expr_t *tmp_right = NULL;
    expr_t *param = NULL;
    expr_t *param_sq = NULL;
    expr_t *root = NULL;
    expr_t *x_root = NULL;
    expr_t *first = NULL;
    expr_t *arg = NULL;
    expr_t *inverse = NULL;
    expr_t *second = NULL;
    expr_t *out = NULL;

    if (!quadratic || !wrt || !match_symbolic_square_family(quadratic, wrt, &param, &kind))
        goto cleanup;

    tmp_left = expr_clone(param);
    param_sq = tmp_left ? expr_pow(tmp_left, &NUM_TWO) : NULL;
    expr_free(tmp_left);
    tmp_left = NULL;

    tmp_left = expr_clone(quadratic);
    root = tmp_left ? expr_sqrt(tmp_left) : NULL;
    expr_free(tmp_left);
    tmp_left = NULL;

    tmp_left = expr_clone(wrt);
    tmp_right = expr_clone(root);
    x_root = (tmp_left && tmp_right) ? expr_mul(tmp_left, tmp_right) : NULL;
    expr_free(tmp_right);
    expr_free(tmp_left);
    tmp_right = NULL;
    tmp_left = NULL;
    first = x_root ? mul_number_owned(x_root, NUM_HALF) : NULL;
    x_root = NULL;
    tmp_left = expr_clone(wrt);
    tmp_right = expr_clone(param);
    arg = (tmp_left && tmp_right) ? expr_div(tmp_left, tmp_right) : NULL;
    expr_free(tmp_right);
    expr_free(tmp_left);
    tmp_right = NULL;
    tmp_left = NULL;

    if (kind == SYMBOLIC_SQUARE_PLUS) {
        inverse = arg ? expr_asinh(arg) : NULL;
        second = (param_sq && inverse) ? expr_mul(param_sq, inverse) : NULL;
        second = second ? mul_number_owned(second, NUM_HALF) : NULL;
        out = simplify_owned(expr_add_owned(first, second));
        first = NULL;
        second = NULL;
    } else if (kind == SYMBOLIC_SQUARE_A2_MINUS_X2) {
        inverse = arg ? expr_asin(arg) : NULL;
        second = (param_sq && inverse) ? expr_mul(param_sq, inverse) : NULL;
        second = second ? mul_number_owned(second, NUM_HALF) : NULL;
        out = simplify_owned(expr_add_owned(first, second));
        first = NULL;
        second = NULL;
    } else {
        expr_t *sum = NULL;
        expr_t *log_term = NULL;

        tmp_left = expr_clone(wrt);
        tmp_right = expr_clone(root);
        sum = (tmp_left && tmp_right) ? expr_add(tmp_left, tmp_right) : NULL;
        expr_free(tmp_right);
        expr_free(tmp_left);
        tmp_right = NULL;
        tmp_left = NULL;
        log_term = sum ? expr_log(sum) : NULL;
        second = (param_sq && log_term) ? expr_mul(param_sq, log_term) : NULL;
        second = second ? mul_number_owned(second, NUM_HALF) : NULL;
        out = simplify_owned(first ? expr_add_owned(first, expr_negate_owned(second)) : NULL);
        first = NULL;
        second = NULL;
        expr_free(log_term);
        expr_free(sum);
    }

cleanup:
    expr_free(tmp_right);
    expr_free(tmp_left);
    expr_free(second);
    expr_free(inverse);
    expr_free(arg);
    expr_free(first);
    expr_free(x_root);
    expr_free(root);
    expr_free(param_sq);
    expr_free(param);
    return out;
}

expr_t *integrate_symbolic_square_family_inverse_root(const expr_t *expr, const expr_t *wrt)
{
    symbolic_square_family_kind_t kind;
    expr_t *tmp_left = NULL;
    expr_t *tmp_right = NULL;
    expr_t *param = NULL;
    expr_t *quadratic = NULL;
    expr_t *arg = NULL;
    expr_t *root = NULL;
    expr_t *sum = NULL;
    number_t one = num_clone(NUM_ONE);
    expr_t *out = NULL;

    if (!expr || !wrt || !expr->a || !expr->b || !expr_match_const_value(expr->a, &one) || !num_eq(one, NUM_ONE) ||
        !expr->b->ops || expr->b->ops->kind != EXPR_KIND_SQRT || !expr->b->a ||
        !match_symbolic_square_family(expr->b->a, wrt, &param, &kind))
        goto cleanup;

    quadratic = expr_clone(expr->b->a);
    tmp_left = expr_clone(wrt);
    tmp_right = expr_clone(param);
    arg = (tmp_left && tmp_right) ? expr_div(tmp_left, tmp_right) : NULL;
    expr_free(tmp_right);
    expr_free(tmp_left);
    tmp_right = NULL;
    tmp_left = NULL;

    if (kind == SYMBOLIC_SQUARE_PLUS) {
        out = arg ? expr_asinh(arg) : NULL;
    } else if (kind == SYMBOLIC_SQUARE_A2_MINUS_X2) {
        out = arg ? expr_asin(arg) : NULL;
    } else {
        tmp_left = expr_clone(quadratic);
        root = tmp_left ? expr_sqrt(tmp_left) : NULL;
        expr_free(tmp_left);
        tmp_left = NULL;

        tmp_left = expr_clone(wrt);
        tmp_right = expr_clone(root);
        sum = (tmp_left && tmp_right) ? expr_add(tmp_left, tmp_right) : NULL;
        expr_free(tmp_right);
        expr_free(tmp_left);
        tmp_right = NULL;
        tmp_left = NULL;
        out = sum ? expr_log(sum) : NULL;
    }

    out = simplify_owned(out);

cleanup:
    expr_free(tmp_right);
    expr_free(tmp_left);
    num_destroy(&one);
    expr_free(sum);
    expr_free(root);
    expr_free(arg);
    expr_free(quadratic);
    expr_free(param);
    return out;
}

expr_t *integrate_symbolic_square_family_wrt_over_root(const expr_t *expr, const expr_t *wrt)
{
    symbolic_square_family_kind_t kind;
    expr_t *param = NULL;
    expr_t *root_arg = NULL;
    expr_t *out = NULL;

    if (!expr || !wrt || !expr->a || !expr->b || !is_wrt_symbolic_affine_leaf(expr->a, wrt) || !expr->b->ops ||
        expr->b->ops->kind != EXPR_KIND_SQRT || !expr->b->a ||
        !match_symbolic_square_family(expr->b->a, wrt, &param, &kind))
        goto cleanup;

    root_arg = expr_clone(expr->b->a);
    out = root_arg ? expr_sqrt(root_arg) : NULL;
    expr_free(root_arg);
    root_arg = NULL;
    if (kind == SYMBOLIC_SQUARE_A2_MINUS_X2)
        out = expr_negate_owned(out);
    out = simplify_owned(out);

cleanup:
    expr_free(root_arg);
    expr_free(param);
    return out;
}

expr_t *integrate_symbolic_square_family_times_root(const expr_t *expr, const expr_t *wrt)
{
    const expr_t *poly_expr = NULL;
    const expr_t *sqrt_expr = NULL;
    symbolic_square_family_kind_t kind;
    expr_t *param = NULL;
    expr_t *base = NULL;
    expr_t *power = NULL;
    expr_t *out = NULL;
    number_t three = num_create_from_long(3);
    number_t three_halves = num_div(three, NUM_TWO);
    bool negate_result = false;

    if (!expr || !wrt || !expr->a || !expr->b)
        goto cleanup;

    if (expr->a->ops && expr->a->ops->kind == EXPR_KIND_SQRT) {
        sqrt_expr = expr->a;
        poly_expr = expr->b;
    } else if (expr->b->ops && expr->b->ops->kind == EXPR_KIND_SQRT) {
        sqrt_expr = expr->b;
        poly_expr = expr->a;
    } else {
        goto cleanup;
    }

    if (!is_wrt_symbolic_affine_leaf(poly_expr, wrt))
        goto cleanup;
    if (!match_symbolic_square_family(sqrt_expr->a, wrt, &param, &kind))
        goto cleanup;

    base = expr_clone(sqrt_expr->a);
    power = base ? expr_pow(base, &three_halves) : NULL;
    out = div_number_owned(power, three);
    power = NULL;
    negate_result = (kind == SYMBOLIC_SQUARE_A2_MINUS_X2);
    if (negate_result)
        out = expr_negate_owned(out);
    out = simplify_owned(out);

cleanup:
    num_destroy(&three_halves);
    num_destroy(&three);
    expr_free(power);
    expr_free(base);
    expr_free(param);
    return out;
}
