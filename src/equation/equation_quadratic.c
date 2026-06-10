#include <stdbool.h>
#include <stddef.h>

#include "equation_internal.h"
#include "expression/expr_internal.h"

static void equation_init_poly2(number_t *poly)
{
    for (size_t i = 0u; i < 3u; ++i)
        poly[i] = num_new();
}

static void equation_destroy_poly2(number_t *poly)
{
    for (size_t i = 0u; i < 3u; ++i)
        num_destroy(&poly[i]);
}

static void equation_zero_poly2(number_t *poly)
{
    for (size_t i = 0u; i < 3u; ++i) {
        num_destroy(&poly[i]);
        poly[i] = num_new();
    }
}

static bool equation_scaled_poly2(const number_t *src,
                                  number_t scale,
                                  number_t *dst)
{
    if (!num_is_real(scale))
        return false;

    for (size_t i = 0u; i < 3u; ++i) {
        number_t scaled = num_mul(src[i], scale);

        num_destroy(&dst[i]);
        dst[i] = scaled;
    }
    return true;
}

static bool equation_add_sub_poly2(const number_t *left,
                                   const number_t *right,
                                   bool subtract,
                                   number_t *out)
{
    for (size_t i = 0u; i < 3u; ++i) {
        number_t value = subtract ? num_sub(left[i], right[i])
                                  : num_add(left[i], right[i]);

        num_destroy(&out[i]);
        out[i] = value;
    }
    return true;
}

static bool equation_mul_poly2(const number_t *left,
                               const number_t *right,
                               number_t *out)
{
    number_t tmp[5];
    bool ok;

    for (size_t i = 0u; i < 5u; ++i)
        tmp[i] = num_new();

    for (size_t i = 0u; i < 3u; ++i) {
        for (size_t j = 0u; j < 3u; ++j) {
            number_t product = num_mul(left[i], right[j]);
            number_t next = num_add(tmp[i + j], product);

            num_destroy(&tmp[i + j]);
            tmp[i + j] = next;
            num_destroy(&product);
        }
    }

    ok = num_is_zero(tmp[3]) && num_is_zero(tmp[4]);
    if (ok) {
        for (size_t i = 0u; i < 3u; ++i) {
            num_destroy(&out[i]);
            out[i] = num_clone(tmp[i]);
        }
    }

    for (size_t i = 0u; i < 5u; ++i)
        num_destroy(&tmp[i]);
    return ok;
}

static bool equation_collect_poly2(const expr_t *expr,
                                   const expr_t *wrt,
                                   number_t *out);

static bool equation_collect_scaled_poly2(const expr_t *expr,
                                          const expr_t *wrt,
                                          number_t *out)
{
    number_t scale = num_new();
    const expr_t *base = NULL;
    number_t base_poly[3];
    bool ok = false;

    if (!expr_match_scaled_expr(expr, &scale, &base) || !base || base == expr)
        goto cleanup_scale;

    equation_init_poly2(base_poly);
    ok = equation_collect_poly2(base, wrt, base_poly) &&
         equation_scaled_poly2(base_poly, scale, out);
    equation_destroy_poly2(base_poly);

cleanup_scale:
    num_destroy(&scale);
    return ok;
}

static bool equation_collect_power_poly2(const expr_t *expr,
                                         const expr_t *wrt,
                                         number_t *out)
{
    number_t base_poly[3];
    bool ok = false;

    if (!expr || !expr->ops || expr->ops->kind != EXPR_KIND_POW_D)
        return false;
    if (!num_is_integer(expr->c) || !num_is_real(expr->c))
        return false;

    if (num_is_zero(expr->c)) {
        equation_zero_poly2(out);
        num_destroy(&out[0]);
        out[0] = num_clone(NUM_ONE);
        return true;
    }

    if (num_eq(expr->c, NUM_ONE))
        return equation_collect_poly2(expr->a, wrt, out);
    if (!num_eq(expr->c, NUM_TWO))
        return false;

    equation_init_poly2(base_poly);
    ok = equation_collect_poly2(expr->a, wrt, base_poly) &&
         equation_mul_poly2(base_poly, base_poly, out);
    equation_destroy_poly2(base_poly);
    return ok;
}

static bool equation_collect_poly2(const expr_t *expr,
                                   const expr_t *wrt,
                                   number_t *out)
{
    expr_t *vars[1];
    size_t index = 0u;
    number_t value = num_new();
    const expr_t *left = NULL;
    const expr_t *right = NULL;
    number_t left_poly[3];
    number_t right_poly[3];
    bool is_sub = false;
    bool ok = false;

    if (!expr || !wrt || !out)
        goto cleanup_value;

    if (expr_match_const_value(expr, &value)) {
        equation_zero_poly2(out);
        num_destroy(&out[0]);
        out[0] = num_clone(value);
        ok = true;
        goto cleanup_value;
    }

    vars[0] = (expr_t *)wrt;
    if (expr_match_var_expr(expr, 1u, vars, &index) && index == 0u) {
        equation_zero_poly2(out);
        num_destroy(&out[1]);
        out[1] = num_clone(NUM_ONE);
        ok = true;
        goto cleanup_value;
    }

    if (equation_collect_scaled_poly2(expr, wrt, out)) {
        ok = true;
        goto cleanup_value;
    }

    if (equation_collect_power_poly2(expr, wrt, out)) {
        ok = true;
        goto cleanup_value;
    }

    equation_init_poly2(left_poly);
    equation_init_poly2(right_poly);
    if (expr_match_add_sub_expr(expr, &left, &right, &is_sub)) {
        ok = equation_collect_poly2(left, wrt, left_poly) &&
             equation_collect_poly2(right, wrt, right_poly) &&
             equation_add_sub_poly2(left_poly, right_poly, is_sub, out);
    } else if (expr_match_mul_expr(expr, &left, &right)) {
        ok = equation_collect_poly2(left, wrt, left_poly) &&
             equation_collect_poly2(right, wrt, right_poly) &&
             equation_mul_poly2(left_poly, right_poly, out);
    }
    equation_destroy_poly2(right_poly);
    equation_destroy_poly2(left_poly);

cleanup_value:
    num_destroy(&value);
    return ok;
}

static void equation_init_poly5(number_t *poly)
{
    for (size_t i = 0u; i < 5u; ++i)
        poly[i] = num_new();
}

static void equation_destroy_poly5(number_t *poly)
{
    for (size_t i = 0u; i < 5u; ++i)
        num_destroy(&poly[i]);
}

static number_t equation_quadratic_constant(number_t p0,
                                            number_t p1,
                                            number_t p2,
                                            number_t basis_constant)
{
    number_t linear_offset = num_mul(p1, basis_constant);
    number_t basis_sq = num_mul(basis_constant, basis_constant);
    number_t quadratic_offset = num_mul(p2, basis_sq);
    number_t first_sum = num_add(p0, linear_offset);
    number_t constant = num_add(first_sum, quadratic_offset);

    num_destroy(&first_sum);
    num_destroy(&quadratic_offset);
    num_destroy(&basis_sq);
    num_destroy(&linear_offset);
    return constant;
}

static number_t equation_quadratic_linear(number_t p1,
                                          number_t p2,
                                          number_t basis_constant,
                                          number_t basis_coeff)
{
    number_t p1_term = num_mul(p1, basis_coeff);
    number_t p2_basis = num_mul(p2, basis_constant);
    number_t p2_basis_coeff = num_mul(p2_basis, basis_coeff);
    number_t p2_term = num_mul_long(p2_basis_coeff, 2L);
    number_t linear = num_add(p1_term, p2_term);

    num_destroy(&p2_term);
    num_destroy(&p2_basis_coeff);
    num_destroy(&p2_basis);
    num_destroy(&p1_term);
    return linear;
}

static number_t equation_quadratic_coeff(number_t p2, number_t basis_coeff)
{
    number_t basis_coeff_sq = num_mul(basis_coeff, basis_coeff);
    number_t quadratic = num_mul(p2, basis_coeff_sq);

    num_destroy(&basis_coeff_sq);
    return quadratic;
}

bool equation_match_quadratic_expr(const expr_t *expr,
                                   const expr_t *wrt,
                                   number_t *constant_out,
                                   number_t *linear_out,
                                   number_t *quadratic_out)
{
    expr_t *vars[1];
    number_t direct_poly[3];
    number_t poly[5];
    number_t basis_constant;
    number_t basis_coeffs[1];
    number_t constant;
    number_t linear;
    number_t quadratic;
    bool ok;

    if (!expr || !wrt || !constant_out || !linear_out || !quadratic_out)
        return false;

    equation_init_poly2(direct_poly);
    if (equation_collect_poly2(expr, wrt, direct_poly) &&
        !num_is_zero(direct_poly[2])) {
        num_destroy(constant_out);
        *constant_out = num_clone(direct_poly[0]);
        num_destroy(linear_out);
        *linear_out = num_clone(direct_poly[1]);
        num_destroy(quadratic_out);
        *quadratic_out = num_clone(direct_poly[2]);
        equation_destroy_poly2(direct_poly);
        return true;
    }
    equation_destroy_poly2(direct_poly);

    basis_constant = num_new();
    basis_coeffs[0] = num_new();
    equation_init_poly5(poly);
    vars[0] = (expr_t *)wrt;

    ok = expr_match_affine_poly_deg4(expr, 1u, vars, poly, &basis_constant,
                                     basis_coeffs) &&
         num_is_zero(poly[3]) &&
         num_is_zero(poly[4]) &&
         !num_is_zero(poly[2]) &&
         !num_is_zero(basis_coeffs[0]);
    if (!ok)
        goto cleanup;

    constant = equation_quadratic_constant(poly[0], poly[1], poly[2],
                                           basis_constant);
    linear = equation_quadratic_linear(poly[1], poly[2], basis_constant,
                                       basis_coeffs[0]);
    quadratic = equation_quadratic_coeff(poly[2], basis_coeffs[0]);

    if (num_is_zero(quadratic)) {
        num_destroy(&quadratic);
        num_destroy(&linear);
        num_destroy(&constant);
        ok = false;
        goto cleanup;
    }

    num_destroy(constant_out);
    *constant_out = constant;
    num_destroy(linear_out);
    *linear_out = linear;
    num_destroy(quadratic_out);
    *quadratic_out = quadratic;

cleanup:
    equation_destroy_poly5(poly);
    num_destroy(&basis_coeffs[0]);
    num_destroy(&basis_constant);
    return ok;
}
