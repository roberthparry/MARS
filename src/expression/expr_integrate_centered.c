#include <stdbool.h>

#define MARS_EXPR_INTEGRATE_INTERNAL_ACCESS
#include "expr_integrate_internal.h"

static bool match_linear_poly_in_wrt(const expr_t *expr,
                                     const expr_t *wrt,
                                     number_t *poly)
{
    number_t basis_constant = num_new();
    number_t basis_coeff = num_new();
    expr_t *vars[1];
    bool ok;

    vars[0] = (expr_t *)wrt;
    ok = expr_match_affine_poly_deg4(expr, 1u, vars, poly, &basis_constant,
                                     &basis_coeff) &&
         expr_integrate_rewrite_poly_deg4_to_affine_basis(poly,
                                                          basis_constant,
                                                          basis_coeff,
                                                          NUM_ZERO,
                                                          NUM_ONE) &&
         num_is_zero(poly[2]) &&
         num_is_zero(poly[3]) &&
         num_is_zero(poly[4]);

    num_destroy(&basis_coeff);
    num_destroy(&basis_constant);
    return ok;
}

static bool match_centered_quadratic_expr(const expr_t *expr,
                                          const expr_t *wrt,
                                          number_t *constant_out,
                                          number_t *quad_coeff_out)
{
    number_t poly[5];
    number_t basis_constant = num_new();
    number_t basis_coeff = num_new();
    expr_t *vars[1];
    bool ok;

    vars[0] = (expr_t *)wrt;
    number_array_zero_local(poly, 5);
    ok = expr_match_affine_poly_deg4(expr, 1u, vars, poly, &basis_constant,
                                     &basis_coeff) &&
         expr_integrate_rewrite_poly_deg4_to_affine_basis(poly,
                                                          basis_constant,
                                                          basis_coeff,
                                                          NUM_ZERO,
                                                          NUM_ONE) &&
         num_is_zero(poly[1]) &&
         num_is_zero(poly[3]) &&
         num_is_zero(poly[4]) &&
         !num_is_zero(poly[0]) &&
         !num_is_zero(poly[2]) &&
         num_is_real(poly[0]) &&
         num_is_real(poly[2]);
    if (ok) {
        num_destroy(constant_out);
        *constant_out = num_clone(poly[0]);
        num_destroy(quad_coeff_out);
        *quad_coeff_out = num_clone(poly[2]);
    }

    number_array_clear_local(poly, 5);
    num_destroy(&basis_coeff);
    num_destroy(&basis_constant);
    return ok;
}

expr_t *integrate_sqrt_one_plus_minus_affine_square(const expr_t *quadratic,
                                                     const expr_t *wrt)
{
    number_t constant = num_new();
    number_t coeff = num_new();
    bool is_plus_square = false;
    expr_t *out = NULL;

    if (quadratic &&
        match_one_plus_minus_affine_square(quadratic, wrt, &is_plus_square,
                                           &constant, &coeff) &&
        !num_eq(coeff, NUM_ZERO)) {
        expr_t *u = build_affine_from_match(wrt, constant, coeff);
        expr_t *root = expr_sqrt(quadratic);
        expr_t *u_root = (u && root) ? expr_mul(u, root) : NULL;
        expr_t *inverse = u ? (is_plus_square ? expr_asinh(u) : expr_asin(u)) : NULL;
        expr_t *sum = expr_add_owned(u_root, inverse);
        expr_t *half = sum ? mul_number_owned(sum, NUM_HALF) : NULL;

        out = div_number_owned(half, coeff);
        expr_free(root);
        expr_free(u);
    }

    num_destroy(&coeff);
    num_destroy(&constant);
    return out;
}

static expr_t *integrate_centered_quadratic_inverse_root(const expr_t *quadratic,
                                                         const expr_t *wrt)
{
    number_t constant = num_new();
    number_t quad = num_new();
    expr_t *out = NULL;

    if (!match_centered_quadratic_expr(quadratic, wrt, &constant, &quad))
        goto cleanup;

    if (num_gt(constant, NUM_ZERO) && num_gt(quad, NUM_ZERO)) {
        number_t ratio = num_div(quad, constant);
        number_t arg_scale = num_sqrt(ratio);
        number_t denom = num_sqrt(quad);
        expr_t *arg = expr_mul_num(wrt, &arg_scale);
        expr_t *inverse = arg ? expr_asinh(arg) : NULL;

        out = div_number_owned(inverse, denom);
        expr_free(arg);
        num_destroy(&denom);
        num_destroy(&arg_scale);
        num_destroy(&ratio);
    } else if (num_gt(constant, NUM_ZERO) && num_lt(quad, NUM_ZERO)) {
        number_t neg_quad = num_neg(quad);
        number_t ratio = num_div(neg_quad, constant);
        number_t arg_scale = num_sqrt(ratio);
        number_t denom = num_sqrt(neg_quad);
        expr_t *arg = expr_mul_num(wrt, &arg_scale);
        expr_t *inverse = arg ? expr_asin(arg) : NULL;

        out = div_number_owned(inverse, denom);
        expr_free(arg);
        num_destroy(&denom);
        num_destroy(&arg_scale);
        num_destroy(&ratio);
        num_destroy(&neg_quad);
    } else if (num_lt(constant, NUM_ZERO) && num_gt(quad, NUM_ZERO)) {
        number_t neg_constant = num_neg(constant);
        number_t ratio = num_div(quad, neg_constant);
        number_t arg_scale = num_sqrt(ratio);
        number_t denom = num_sqrt(quad);
        expr_t *arg = expr_mul_num(wrt, &arg_scale);
        expr_t *inverse = arg ? expr_acosh(arg) : NULL;

        out = div_number_owned(inverse, denom);
        expr_free(arg);
        num_destroy(&denom);
        num_destroy(&arg_scale);
        num_destroy(&ratio);
        num_destroy(&neg_constant);
    }

cleanup:
    num_destroy(&quad);
    num_destroy(&constant);
    return out;
}

expr_t *integrate_centered_quadratic_root(const expr_t *quadratic, const expr_t *wrt)
{
    number_t constant = num_new();
    number_t quad = num_new();
    expr_t *out = NULL;

    if (!match_centered_quadratic_expr(quadratic, wrt, &constant, &quad))
        goto cleanup;

    if (num_gt(constant, NUM_ZERO) && num_gt(quad, NUM_ZERO)) {
        number_t ratio = num_div(quad, constant);
        number_t arg_scale = num_sqrt(ratio);
        number_t sqrt_quad = num_sqrt(quad);
        number_t denom = num_mul(NUM_TWO, sqrt_quad);
        number_t inverse_scale = num_div(constant, denom);
        expr_t *root = expr_sqrt(quadratic);
        expr_t *x_root = root ? expr_mul(wrt, root) : NULL;
        expr_t *first = x_root ? mul_number_owned(x_root, NUM_HALF) : NULL;
        expr_t *arg = expr_mul_num(wrt, &arg_scale);
        expr_t *inverse = arg ? expr_asinh(arg) : NULL;
        expr_t *second = inverse ? mul_number_owned(inverse, inverse_scale) : NULL;

        out = simplify_owned(expr_add_owned(first, second));
        expr_free(arg);
        expr_free(root);
        num_destroy(&inverse_scale);
        num_destroy(&denom);
        num_destroy(&sqrt_quad);
        num_destroy(&arg_scale);
        num_destroy(&ratio);
    } else if (num_gt(constant, NUM_ZERO) && num_lt(quad, NUM_ZERO)) {
        number_t neg_quad = num_neg(quad);
        number_t ratio = num_div(neg_quad, constant);
        number_t arg_scale = num_sqrt(ratio);
        number_t sqrt_neg_quad = num_sqrt(neg_quad);
        number_t denom = num_mul(NUM_TWO, sqrt_neg_quad);
        number_t inverse_scale = num_div(constant, denom);
        expr_t *root = expr_sqrt(quadratic);
        expr_t *x_root = root ? expr_mul(wrt, root) : NULL;
        expr_t *first = x_root ? mul_number_owned(x_root, NUM_HALF) : NULL;
        expr_t *arg = expr_mul_num(wrt, &arg_scale);
        expr_t *inverse = arg ? expr_asin(arg) : NULL;
        expr_t *second = inverse ? mul_number_owned(inverse, inverse_scale) : NULL;

        out = simplify_owned(expr_add_owned(first, second));
        expr_free(arg);
        expr_free(root);
        num_destroy(&inverse_scale);
        num_destroy(&denom);
        num_destroy(&sqrt_neg_quad);
        num_destroy(&arg_scale);
        num_destroy(&ratio);
        num_destroy(&neg_quad);
    } else if (num_lt(constant, NUM_ZERO) && num_gt(quad, NUM_ZERO)) {
        number_t neg_constant = num_neg(constant);
        number_t ratio = num_div(quad, neg_constant);
        number_t arg_scale = num_sqrt(ratio);
        number_t sqrt_quad = num_sqrt(quad);
        number_t denom = num_mul(NUM_TWO, sqrt_quad);
        number_t inverse_scale = num_div(neg_constant, denom);
        expr_t *root = expr_sqrt(quadratic);
        expr_t *x_root = root ? expr_mul(wrt, root) : NULL;
        expr_t *first = x_root ? mul_number_owned(x_root, NUM_HALF) : NULL;
        expr_t *arg = expr_mul_num(wrt, &arg_scale);
        expr_t *inverse = arg ? expr_acosh(arg) : NULL;
        expr_t *second = inverse ? mul_number_owned(inverse, inverse_scale) : NULL;
        expr_t *neg_second = second ? expr_neg(second) : NULL;

        out = simplify_owned(expr_add_owned(first, neg_second));
        expr_free(second);
        expr_free(arg);
        expr_free(root);
        num_destroy(&inverse_scale);
        num_destroy(&denom);
        num_destroy(&sqrt_quad);
        num_destroy(&arg_scale);
        num_destroy(&ratio);
        num_destroy(&neg_constant);
    }

cleanup:
    num_destroy(&quad);
    num_destroy(&constant);
    return out;
}

expr_t *integrate_linear_poly_over_centered_quadratic_root(const expr_t *expr,
                                                           const expr_t *wrt)
{
    number_t poly[5];
    number_t constant = num_new();
    number_t quad = num_new();
    expr_t *root_part = NULL;
    expr_t *inverse_part = NULL;
    expr_t *out = NULL;

    number_array_zero_local(poly, 5);
    if (!expr || !expr->a || !expr->b ||
        !expr->b->ops ||
        expr->b->ops->kind != EXPR_KIND_SQRT ||
        !expr->b->a ||
        !match_linear_poly_in_wrt(expr->a, wrt, poly) ||
        !match_centered_quadratic_expr(expr->b->a, wrt, &constant, &quad))
        goto cleanup;

    if (!num_is_zero(poly[0])) {
        expr_t *inverse = integrate_centered_quadratic_inverse_root(expr->b->a, wrt);

        inverse_part = inverse ? mul_number_owned(inverse, poly[0]) : NULL;
    }
    if (!num_is_zero(poly[1])) {
        expr_t *root = expr_sqrt(expr->b->a);
        expr_t *scaled = root ? mul_number_owned(root, poly[1]) : NULL;

        root_part = div_number_owned(scaled, quad);
    }

    out = simplify_owned(expr_add_owned(inverse_part, root_part));
    inverse_part = NULL;
    root_part = NULL;

cleanup:
    expr_free(root_part);
    expr_free(inverse_part);
    number_array_clear_local(poly, 5);
    num_destroy(&quad);
    num_destroy(&constant);
    return out;
}

expr_t *integrate_linear_poly_times_centered_quadratic_root(const expr_t *expr,
                                                            const expr_t *wrt)
{
    const expr_t *poly_expr = NULL;
    const expr_t *sqrt_expr = NULL;
    number_t poly[5];
    number_t constant = num_new();
    number_t quad = num_new();
    expr_t *linear_part = NULL;
    expr_t *root_part = NULL;
    expr_t *out = NULL;

    number_array_zero_local(poly, 5);
    if (!expr || !expr->a || !expr->b)
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

    if (!sqrt_expr->a ||
        !match_linear_poly_in_wrt(poly_expr, wrt, poly) ||
        !match_centered_quadratic_expr(sqrt_expr->a, wrt, &constant, &quad))
        goto cleanup;

    if (!num_is_zero(poly[0])) {
        expr_t *root_integral = integrate_centered_quadratic_root(sqrt_expr->a, wrt);

        root_part = root_integral ? mul_number_owned(root_integral, poly[0]) : NULL;
    }
    if (!num_is_zero(poly[1])) {
        number_t three = num_create_from_long(3);
        number_t three_halves = num_div(three, NUM_TWO);
        number_t denom = num_mul(three, quad);
        expr_t *power = expr_pow(sqrt_expr->a, &three_halves);
        expr_t *scaled = power ? mul_number_owned(power, poly[1]) : NULL;

        linear_part = div_number_owned(scaled, denom);
        num_destroy(&denom);
        num_destroy(&three_halves);
        num_destroy(&three);
    }

    out = simplify_owned(expr_add_owned(root_part, linear_part));
    root_part = NULL;
    linear_part = NULL;

cleanup:
    expr_free(linear_part);
    expr_free(root_part);
    number_array_clear_local(poly, 5);
    num_destroy(&quad);
    num_destroy(&constant);
    return out;
}
