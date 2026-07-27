#define MARS_INTEGRATOR_INTERNAL_ACCESS
#include "integrator_internal.h"

int intg_double_integral(integrator_t *ig, expr_t *expr,
                       expr_t *x_var, number_t ax, number_t bx,
                       expr_t *y_var, number_t ay, number_t by,
                       number_t *result, number_t *error_est)
{
    expr_t *vars[2];
    number_t lo[2];
    number_t hi[2];

    if (!ig || !expr || !x_var || !y_var || !result)
        return -1;

    vars[0] = x_var;
    vars[1] = y_var;
    lo[0] = ax;
    hi[0] = bx;
    lo[1] = ay;
    hi[1] = by;
    intg_clear_exact_result(ig);
    return intg_integral_multi_num(ig, expr, 2u, vars, lo, hi, result, error_est);
}

int intg_triple_integral(integrator_t *ig, expr_t *expr,
                       expr_t *x_var, number_t ax, number_t bx,
                       expr_t *y_var, number_t ay, number_t by,
                       expr_t *z_var, number_t az, number_t bz,
                       number_t *result, number_t *error_est)
{
    expr_t *vars[3];
    number_t lo[3];
    number_t hi[3];

    if (!ig || !expr || !x_var || !y_var || !z_var || !result)
        return -1;

    vars[0] = x_var;
    vars[1] = y_var;
    vars[2] = z_var;
    lo[0] = ax;
    hi[0] = bx;
    lo[1] = ay;
    hi[1] = by;
    lo[2] = az;
    hi[2] = bz;
    intg_clear_exact_result(ig);
    return intg_integral_multi_num(ig, expr, 3u, vars, lo, hi, result, error_est);
}
