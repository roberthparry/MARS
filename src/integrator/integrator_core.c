#include "integrator_internal.h"

static const char integrator_default_tolerance_text[] = "1e-27";

enum {
    INTEGRATOR_DEFAULT_MAX_INTERVALS = 5000
};

integrator_t *intg_new(void) {
    integrator_t *ig = malloc(sizeof(integrator_t));
    if (!ig) return NULL;
    ig->abs_tol       = num_create_from_string(integrator_default_tolerance_text);
    ig->rel_tol       = num_create_from_string(integrator_default_tolerance_text);
    ig->max_intervals = INTEGRATOR_DEFAULT_MAX_INTERVALS;
    ig->last_intervals = 0;
    ig->last_exact_result = NULL;
    return ig;
}

void intg_free(integrator_t *ig) {
    if (!ig)
        return;
    expr_free(ig->last_exact_result);
    num_destroy(&ig->abs_tol);
    num_destroy(&ig->rel_tol);
    free(ig);
}

void intg_set_tolerance(integrator_t *ig, number_t abs_tol, number_t rel_tol) {
    if (!ig) return;
    num_destroy(&ig->abs_tol);
    num_destroy(&ig->rel_tol);
    ig->abs_tol = num_clone(abs_tol);
    ig->rel_tol = num_clone(rel_tol);
}

void intg_set_interval_count_max(integrator_t *ig, size_t max_intervals) {
    if (!ig) return;
    ig->max_intervals = max_intervals;
}

size_t intg_get_interval_count_used(const integrator_t *ig) {
    return ig ? ig->last_intervals : 0;
}

const expr_t *intg_get_exact_result(const integrator_t *ig)
{
    return ig ? ig->last_exact_result : NULL;
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
