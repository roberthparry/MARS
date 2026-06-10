#ifndef EQUATION_INTERNAL_H
#define EQUATION_INTERNAL_H

#include <stdbool.h>

#include "equation.h"
#include "number.h"

bool equation_match_affine_linear_expr(const expr_t *expr,
                                       const expr_t *wrt,
                                       bool require_nonzero_coeff,
                                       number_t *constant_out,
                                       number_t *coeff_out);

bool equation_match_quadratic_expr(const expr_t *expr,
                                   const expr_t *wrt,
                                   number_t *constant_out,
                                   number_t *linear_out,
                                   number_t *quadratic_out);

#endif
