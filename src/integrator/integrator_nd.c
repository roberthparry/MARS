#define MARS_INTEGRATOR_INTERNAL_ACCESS
#include "integrator_internal.h"

int intg_integral_multi(integrator_t *ig, expr_t *expr,
                      size_t ndim, expr_t * const *vars,
                      const number_t *lo, const number_t *hi,
                      number_t *result, number_t *error_est)
{
    intg_clear_exact_result(ig);
    return intg_integral_multi_num(ig, expr, ndim, vars, lo, hi, result, error_est);
}
