#include <stdbool.h>

#include "expr_integrate_internal.h"

static bool match_symbolic_affine_or_constant(const expr_t *expr,
                                              const expr_t *wrt,
                                              expr_t **constant_out,
                                              expr_t **coeff_out)
{
    if (!expr || !wrt || !constant_out || !coeff_out)
        return false;

    if (!depends_on_wrt(expr, wrt)) {
        *constant_out = expr_integrate_clone_expr(expr);
        *coeff_out = expr_new_const(NUM_ZERO);
        if (!*constant_out || !*coeff_out) {
            expr_free(*constant_out);
            expr_free(*coeff_out);
            *constant_out = NULL;
            *coeff_out = NULL;
            return false;
        }
        return true;
    }

    return match_symbolic_affine_constant_and_coeff(expr, wrt, constant_out, coeff_out);
}

static expr_t *build_symbolic_general_quadratic_log(const expr_t *quadratic,
                                                    const expr_t *wrt,
                                                    const expr_t *quad_coeff,
                                                    const expr_t *linear_coeff)
{
    expr_t *sqrt_a = NULL;
    expr_t *sqrt_q = NULL;
    expr_t *two_sqrt_a = NULL;
    expr_t *root_term = NULL;
    expr_t *a_x = NULL;
    expr_t *two_a_x = NULL;
    expr_t *linear_term = NULL;
    expr_t *arg = NULL;
    expr_t *out = NULL;

    if (!quadratic || !wrt || !quad_coeff || !linear_coeff)
        goto cleanup;

    sqrt_a = expr_sqrt(quad_coeff);
    sqrt_q = expr_sqrt(quadratic);
    two_sqrt_a = sqrt_a ? expr_mul_num(sqrt_a, &NUM_TWO) : NULL;
    root_term = (two_sqrt_a && sqrt_q) ? expr_mul(two_sqrt_a, sqrt_q) : NULL;
    a_x = expr_mul(quad_coeff, wrt);
    two_a_x = a_x ? expr_mul_num(a_x, &NUM_TWO) : NULL;
    linear_term = two_a_x ? expr_add(two_a_x, linear_coeff) : NULL;
    arg = (root_term && linear_term) ? expr_add(root_term, linear_term) : NULL;
    out = arg ? expr_log(arg) : NULL;
    out = simplify_owned(out);

cleanup:
    expr_free(arg);
    expr_free(linear_term);
    expr_free(two_a_x);
    expr_free(a_x);
    expr_free(root_term);
    expr_free(two_sqrt_a);
    expr_free(sqrt_q);
    expr_free(sqrt_a);
    return out;
}

static expr_t *build_symbolic_general_quadratic_inverse_root_integral(const expr_t *quadratic,
                                                                      const expr_t *wrt,
                                                                      const expr_t *quad_coeff,
                                                                      const expr_t *linear_coeff)
{
    expr_t *sqrt_a = NULL;
    expr_t *log_term = NULL;
    expr_t *out = NULL;

    if (!quadratic || !wrt || !quad_coeff || !linear_coeff)
        goto cleanup;

    sqrt_a = expr_sqrt(quad_coeff);
    log_term = build_symbolic_general_quadratic_log(quadratic, wrt,
                                                    quad_coeff, linear_coeff);
    out = (log_term && sqrt_a) ? expr_div(log_term, sqrt_a) : NULL;
    out = simplify_owned(out);

cleanup:
    expr_free(log_term);
    expr_free(sqrt_a);
    return out;
}

expr_t *integrate_symbolic_general_quadratic_root(const expr_t *quadratic,
                                                         const expr_t *wrt)
{
    expr_t *quad_coeff = NULL;
    expr_t *linear_coeff = NULL;
    expr_t *constant_coeff = NULL;
    expr_t *sqrt_q = NULL;
    expr_t *a_x = NULL;
    expr_t *two_a_x = NULL;
    expr_t *linear = NULL;
    expr_t *linear_root = NULL;
    expr_t *four_a = NULL;
    expr_t *first = NULL;
    expr_t *a_c = NULL;
    expr_t *four_ac = NULL;
    expr_t *linear_sq = NULL;
    expr_t *delta = NULL;
    expr_t *inverse_root_integral = NULL;
    expr_t *delta_inverse = NULL;
    expr_t *eight_a = NULL;
    expr_t *second = NULL;
    expr_t *out = NULL;
    number_t four = num_create_from_long(4);
    number_t eight = num_create_from_long(8);

    if (!quadratic || !wrt ||
        !match_symbolic_quadratic_coeffs(quadratic, wrt,
                                         &quad_coeff, &linear_coeff, &constant_coeff))
        goto cleanup;

    sqrt_q = expr_sqrt(quadratic);
    a_x = expr_mul(quad_coeff, wrt);
    two_a_x = a_x ? expr_mul_num(a_x, &NUM_TWO) : NULL;
    linear = two_a_x ? expr_add(two_a_x, linear_coeff) : NULL;
    linear_root = (linear && sqrt_q) ? expr_mul(linear, sqrt_q) : NULL;
    four_a = expr_mul_num(quad_coeff, &four);
    first = (linear_root && four_a) ? expr_div(linear_root, four_a) : NULL;

    a_c = expr_mul(quad_coeff, constant_coeff);
    four_ac = a_c ? expr_mul_num(a_c, &four) : NULL;
    linear_sq = expr_pow(linear_coeff, &NUM_TWO);
    delta = (four_ac && linear_sq) ? expr_sub(four_ac, linear_sq) : NULL;
    inverse_root_integral =
        build_symbolic_general_quadratic_inverse_root_integral(quadratic, wrt,
                                                               quad_coeff, linear_coeff);
    delta_inverse = (delta && inverse_root_integral)
                        ? expr_mul(delta, inverse_root_integral)
                        : NULL;
    eight_a = expr_mul_num(quad_coeff, &eight);
    second = (delta_inverse && eight_a) ? expr_div(delta_inverse, eight_a) : NULL;

    out = simplify_owned(expr_integrate_add_terms_owned(first, second));
    first = NULL;
    second = NULL;

cleanup:
    num_destroy(&eight);
    num_destroy(&four);
    expr_free(second);
    expr_free(eight_a);
    expr_free(delta_inverse);
    expr_free(inverse_root_integral);
    expr_free(delta);
    expr_free(linear_sq);
    expr_free(four_ac);
    expr_free(a_c);
    expr_free(first);
    expr_free(four_a);
    expr_free(linear_root);
    expr_free(linear);
    expr_free(two_a_x);
    expr_free(a_x);
    expr_free(sqrt_q);
    expr_free(constant_coeff);
    expr_free(linear_coeff);
    expr_free(quad_coeff);
    return out;
}

expr_t *integrate_symbolic_general_quadratic_linear_over_root(const expr_t *expr,
                                                                     const expr_t *wrt)
{
    expr_t *numer_constant = NULL;
    expr_t *numer_linear = NULL;
    expr_t *quad_coeff = NULL;
    expr_t *linear_coeff = NULL;
    expr_t *constant_coeff = NULL;
    expr_t *inverse_root_integral = NULL;
    expr_t *sqrt_q = NULL;
    expr_t *root_over_quad = NULL;
    expr_t *two_quad = NULL;
    expr_t *linear_over_two_quad = NULL;
    expr_t *linear_correction = NULL;
    expr_t *wrt_over_root_integral = NULL;
    expr_t *constant_part = NULL;
    expr_t *linear_part = NULL;
    expr_t *out = NULL;

    if (!expr || !wrt || !expr->a || !expr->b ||
        !expr->b->ops || expr->b->ops->kind != EXPR_KIND_SQRT || !expr->b->a ||
        !match_symbolic_affine_or_constant(expr->a, wrt, &numer_constant, &numer_linear) ||
        !match_symbolic_quadratic_coeffs(expr->b->a, wrt,
                                         &quad_coeff, &linear_coeff, &constant_coeff))
        goto cleanup;

    inverse_root_integral =
        build_symbolic_general_quadratic_inverse_root_integral(expr->b->a, wrt,
                                                               quad_coeff, linear_coeff);

    sqrt_q = expr_sqrt(expr->b->a);
    root_over_quad = sqrt_q ? expr_div(sqrt_q, quad_coeff) : NULL;
    two_quad = expr_mul_num(quad_coeff, &NUM_TWO);
    linear_over_two_quad = two_quad ? expr_div(linear_coeff, two_quad) : NULL;
    linear_correction = (linear_over_two_quad && inverse_root_integral)
                            ? expr_mul(linear_over_two_quad, inverse_root_integral)
                            : NULL;
    wrt_over_root_integral = (root_over_quad && linear_correction)
                                 ? expr_sub(root_over_quad, linear_correction)
                                 : NULL;

    if (inverse_root_integral && !expr_is_exact_zero(numer_constant))
        constant_part = expr_mul(numer_constant, inverse_root_integral);
    if (wrt_over_root_integral && !expr_is_exact_zero(numer_linear))
        linear_part = expr_mul(numer_linear, wrt_over_root_integral);

    out = simplify_owned(expr_integrate_add_terms_owned(constant_part, linear_part));
    constant_part = NULL;
    linear_part = NULL;

cleanup:
    expr_free(linear_part);
    expr_free(constant_part);
    expr_free(wrt_over_root_integral);
    expr_free(linear_correction);
    expr_free(linear_over_two_quad);
    expr_free(two_quad);
    expr_free(root_over_quad);
    expr_free(sqrt_q);
    expr_free(inverse_root_integral);
    expr_free(constant_coeff);
    expr_free(linear_coeff);
    expr_free(quad_coeff);
    expr_free(numer_linear);
    expr_free(numer_constant);
    return out;
}

expr_t *integrate_symbolic_general_quadratic_times_root(const expr_t *expr,
                                                               const expr_t *wrt)
{
    const expr_t *poly_expr = NULL;
    const expr_t *sqrt_expr = NULL;
    expr_t *quad_coeff = NULL;
    expr_t *linear_coeff = NULL;
    expr_t *constant_coeff = NULL;
    expr_t *power = NULL;
    expr_t *three_a = NULL;
    expr_t *first = NULL;
    expr_t *root_integral = NULL;
    expr_t *two_a = NULL;
    expr_t *linear_over_two_a = NULL;
    expr_t *correction = NULL;
    expr_t *out = NULL;
    number_t three = num_create_from_long(3);
    number_t three_halves = num_div(three, NUM_TWO);

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

    if (!is_wrt_symbolic_affine_leaf(poly_expr, wrt) || !sqrt_expr->a ||
        !match_symbolic_quadratic_coeffs(sqrt_expr->a, wrt,
                                         &quad_coeff, &linear_coeff, &constant_coeff))
        goto cleanup;

    power = expr_pow(sqrt_expr->a, &three_halves);
    three_a = expr_mul_num(quad_coeff, &three);
    first = (power && three_a) ? expr_div(power, three_a) : NULL;

    root_integral = integrate_symbolic_general_quadratic_root(sqrt_expr->a, wrt);
    two_a = expr_mul_num(quad_coeff, &NUM_TWO);
    linear_over_two_a = two_a ? expr_div(linear_coeff, two_a) : NULL;
    correction = (linear_over_two_a && root_integral)
                     ? expr_mul(linear_over_two_a, root_integral)
                     : NULL;
    out = (first && correction) ? expr_sub(first, correction) : NULL;
    out = simplify_owned(out);

cleanup:
    num_destroy(&three_halves);
    num_destroy(&three);
    expr_free(correction);
    expr_free(linear_over_two_a);
    expr_free(two_a);
    expr_free(root_integral);
    expr_free(first);
    expr_free(three_a);
    expr_free(power);
    expr_free(constant_coeff);
    expr_free(linear_coeff);
    expr_free(quad_coeff);
    return out;
}
