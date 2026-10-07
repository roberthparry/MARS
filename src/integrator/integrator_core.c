/**
 * @file integrator_core.c
 * @brief Integrator configuration and public operation dispatch.
 *
 * Owns tolerances, subdivision limits, exact-result state and integrator lifecycle. It coordinates expression and
 * symbolic entry points with numerical implementations rather than embedding every quadrature method here.
 *
 * This is part of integrator.h. Callers configure opaque integrator handles and inspect completion or error
 * information rather than relying on a particular internal algorithm.
 */

#define MARS_INTEGRATOR_INTERNAL_ACCESS
#include "integrator_internal.h"

static const char integrator_default_tolerance_text[] = "1e-27";

enum { INTEGRATOR_DEFAULT_MAX_INTERVALS = 5000 };

integrator_t *intg_new(void)
{
    integrator_t *ig = malloc(sizeof(integrator_t));
    if (!ig)
        return NULL;
    ig->abs_tol = num_create_from_string(integrator_default_tolerance_text);
    ig->rel_tol = num_create_from_string(integrator_default_tolerance_text);
    ig->max_intervals = INTEGRATOR_DEFAULT_MAX_INTERVALS;
    ig->last_intervals = 0;
    ig->last_exact_result = NULL;
    return ig;
}

void intg_free(integrator_t *ig)
{
    if (!ig)
        return;
    expr_free(ig->last_exact_result);
    num_destroy(&ig->abs_tol);
    num_destroy(&ig->rel_tol);
    free(ig);
}

void intg_set_tolerance(integrator_t *ig, number_t abs_tol, number_t rel_tol)
{
    if (!ig)
        return;
    num_destroy(&ig->abs_tol);
    num_destroy(&ig->rel_tol);
    ig->abs_tol = num_clone(abs_tol);
    ig->rel_tol = num_clone(rel_tol);
}

void intg_set_interval_count_max(integrator_t *ig, size_t max_intervals)
{
    if (!ig)
        return;
    ig->max_intervals = max_intervals;
}

size_t intg_get_interval_count_used(const integrator_t *ig)
{
    return ig ? ig->last_intervals : 0;
}

const expr_t *intg_get_exact_result(const integrator_t *ig)
{
    return ig ? ig->last_exact_result : NULL;
}

static expr_integration_bound_kind_t intg_expr_bound_kind_from_public(intg_bound_kind_t kind)
{
    switch (kind) {
        case INTG_BOUND_DEFINITE:
            return EXPR_INTEGRATION_BOUND_DEFINITE;
        case INTG_BOUND_UPPER_ONLY:
            return EXPR_INTEGRATION_BOUND_UPPER_ONLY;
        case INTG_BOUND_INDEFINITE:
        default:
            return EXPR_INTEGRATION_BOUND_INDEFINITE;
    }
}

static expr_integration_bound_kind_t *intg_convert_bound_kinds(const intg_bound_kind_t *kinds, size_t ndim)
{
    expr_integration_bound_kind_t *converted;

    if (!kinds && ndim > 0u)
        return NULL;
    converted = calloc(ndim ? ndim : 1u, sizeof(*converted));
    if (!converted)
        return NULL;
    for (size_t i = 0; i < ndim; ++i)
        converted[i] = intg_expr_bound_kind_from_public(kinds[i]);
    return converted;
}

bool intg_integrand_has_unbound_parameters(const expr_t *integrand, size_t ndim, expr_t *const *vars)
{
    return expr_has_unbound_parameters(integrand, ndim, vars);
}

expr_t *intg_integrate_iterated_symbolic(const expr_t *integrand, size_t ndim, expr_t *const *vars,
                                         const intg_bound_kind_t *kinds, expr_t *const *lo, expr_t *const *hi,
                                         size_t max_steps, size_t *completed_steps_out,
                                         expr_t **first_antiderivative_out)
{
    expr_integration_bound_kind_t *converted = intg_convert_bound_kinds(kinds, ndim);
    expr_t *result;

    if (!converted)
        return NULL;
    result = expr_integrate_iterated(integrand, ndim, vars, converted, lo, hi, max_steps, completed_steps_out,
                                     first_antiderivative_out);
    free(converted);
    return result;
}

expr_t *intg_integrate_iterated_symbolic_best_effort(const expr_t *integrand, size_t ndim, expr_t *const *vars,
                                                     const intg_bound_kind_t *kinds, expr_t *const *lo,
                                                     expr_t *const *hi, size_t *completed_steps_out,
                                                     size_t *remaining_ndim_out, expr_t **remaining_vars_out,
                                                     number_t *remaining_lo_num_out, number_t *remaining_hi_num_out,
                                                     const number_t *lo_num, const number_t *hi_num)
{
    expr_integration_bound_kind_t *converted = intg_convert_bound_kinds(kinds, ndim);
    expr_t *result;

    if (!converted)
        return NULL;
    result = expr_integrate_iterated_best_effort(integrand, ndim, vars, converted, lo, hi, completed_steps_out,
                                                 remaining_ndim_out, remaining_vars_out, remaining_lo_num_out,
                                                 remaining_hi_num_out, lo_num, hi_num);
    free(converted);
    return result;
}

void intg_clear_exact_result(integrator_t *ig)
{
    if (!ig)
        return;
    expr_free(ig->last_exact_result);
    ig->last_exact_result = NULL;
}

void intg_set_exact_result_owned(integrator_t *ig, expr_t *expr)
{
    if (!ig) {
        expr_free(expr);
        return;
    }
    expr_free(ig->last_exact_result);
    ig->last_exact_result = expr;
}


/* Dispatch rectangular N-dimensional integration through the shared numeric engine. */
int intg_integral_multi(integrator_t *ig, expr_t *expr, size_t ndim, expr_t *const *vars, const number_t *lo,
                        const number_t *hi, number_t *result, number_t *error_est)
{
    intg_clear_exact_result(ig);
    return intg_integral_multi_num(ig, expr, ndim, vars, lo, hi, result, error_est);
}

/* Adapt the two-dimensional public call to the shared integration engine. */
int intg_double_integral(integrator_t *ig, expr_t *expr, expr_t *x_var, number_t ax, number_t bx, expr_t *y_var,
                         number_t ay, number_t by, number_t *result, number_t *error_est)
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

/* Adapt the three-dimensional public call to the shared integration engine. */
int intg_triple_integral(integrator_t *ig, expr_t *expr, expr_t *x_var, number_t ax, number_t bx, expr_t *y_var,
                         number_t ay, number_t by, expr_t *z_var, number_t az, number_t bz, number_t *result,
                         number_t *error_est)
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
