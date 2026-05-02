#include "integrator_internal.h"

integrator_t *ig_new(void) {
    integrator_t *ig = malloc(sizeof(integrator_t));
    if (!ig) return NULL;
    ig->abs_tol       = qf_from_string("1e-27");
    ig->rel_tol       = qf_from_string("1e-27");
    ig->max_intervals = 500;
    ig->last_intervals = 0;
    return ig;
}

void ig_free(integrator_t *ig) {
    free(ig);
}

void ig_set_tolerance(integrator_t *ig, qfloat_t abs_tol, qfloat_t rel_tol) {
    if (!ig) return;
    ig->abs_tol = abs_tol;
    ig->rel_tol = rel_tol;
}

void ig_set_interval_count_max(integrator_t *ig, size_t max_intervals) {
    if (!ig) return;
    ig->max_intervals = max_intervals;
}

size_t ig_get_interval_count_used(const integrator_t *ig) {
    return ig ? ig->last_intervals : 0;
}
