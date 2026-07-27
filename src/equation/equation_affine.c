#include <stdbool.h>

#define MARS_EQUATION_INTERNAL_ACCESS
#include "equation_internal.h"
#define MARS_SHARED_EXPR_INTERNAL_ACCESS
#include "internal/expr_internal.h"

bool equ_match_affine_linear_expr(const expr_t *expr,
                                       const expr_t *wrt,
                                       bool require_nonzero_coeff,
                                       number_t *constant_out,
                                       number_t *coeff_out)
{
    expr_t *vars[1];
    number_t poly[5];
    number_t basis_constant;
    number_t basis_coeffs[1];
    number_t affine_constant;
    number_t affine_coeff;
    bool ok;

    if (!expr || !wrt || !constant_out || !coeff_out)
        return false;

    basis_constant = num_new();
    vars[0] = (expr_t *)wrt;
    for (size_t i = 0u; i < 5u; ++i)
        poly[i] = num_new();
    basis_coeffs[0] = num_new();

    ok = expr_match_affine_poly_deg4(expr, 1u, vars, poly, &basis_constant,
                                     basis_coeffs) &&
         num_is_zero(poly[2]) &&
         num_is_zero(poly[3]) &&
         num_is_zero(poly[4]) &&
         (!require_nonzero_coeff ||
          (!num_is_zero(poly[1]) && !num_is_zero(basis_coeffs[0])));
    if (!ok)
        goto cleanup;

    {
        number_t constant_offset = num_mul(poly[1], basis_constant);

        affine_constant = num_add(poly[0], constant_offset);
        num_destroy(&constant_offset);
    }
    affine_coeff = num_mul(poly[1], basis_coeffs[0]);

    num_destroy(constant_out);
    *constant_out = affine_constant;
    num_destroy(coeff_out);
    *coeff_out = affine_coeff;

cleanup:
    for (size_t i = 0u; i < 5u; ++i)
        num_destroy(&poly[i]);
    num_destroy(&basis_constant);
    num_destroy(&basis_coeffs[0]);
    return ok;
}
