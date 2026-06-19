#ifndef INTEGRATOR_INTERNAL_H
#define INTEGRATOR_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "expression.h"
#include "integrator.h"
#include "internal/expr_internal.h"

struct _integrator_t {
    number_t abs_tol;
    number_t rel_tol;
    size_t max_intervals;
    size_t last_intervals;
    expr_t *last_exact_result;
};

void intg_clear_exact_result(integrator_t *ig);
void intg_set_exact_result_owned(integrator_t *ig, expr_t *expr);

int try_integral_multi_special_affine(integrator_t *ig, expr_t *expr,
                                      size_t ndim, expr_t *const *vars,
                                      const number_t *lo, const number_t *hi,
                                      number_t *result, number_t *error_est);

int intg_integral_multi_num(integrator_t *ig, expr_t *expr,
                          size_t ndim, expr_t * const *vars,
                          const number_t *lo, const number_t *hi,
                          number_t *result, number_t *error_est);

#endif
