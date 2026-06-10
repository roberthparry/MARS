#include <stdbool.h>
#include <stddef.h>

#include "equation_internal.h"
#include "expression/expr_internal.h"

static void equation_init_numbers(number_t *values, size_t count)
{
    for (size_t i = 0u; i < count; ++i)
        values[i] = num_new();
}

static void equation_destroy_numbers(number_t *values, size_t count)
{
    for (size_t i = 0u; i < count; ++i)
        num_destroy(&values[i]);
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

    equation_init_numbers(direct_poly, 3u);
    if (equation_match_polynomial_expr(expr, wrt, 2u, direct_poly) &&
        !num_is_zero(direct_poly[2])) {
        num_destroy(constant_out);
        *constant_out = num_clone(direct_poly[0]);
        num_destroy(linear_out);
        *linear_out = num_clone(direct_poly[1]);
        num_destroy(quadratic_out);
        *quadratic_out = num_clone(direct_poly[2]);
        equation_destroy_numbers(direct_poly, 3u);
        return true;
    }
    equation_destroy_numbers(direct_poly, 3u);

    basis_constant = num_new();
    basis_coeffs[0] = num_new();
    equation_init_numbers(poly, 5u);
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
    equation_destroy_numbers(poly, 5u);
    num_destroy(&basis_coeffs[0]);
    num_destroy(&basis_constant);
    return ok;
}
