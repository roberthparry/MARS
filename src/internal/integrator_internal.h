#ifndef INTEGRATOR_SHARED_INTERNAL_H
#define INTEGRATOR_SHARED_INTERNAL_H

#include "expression.h"
#include "integrator.h"

const expr_t *intg_get_exact_result(const integrator_t *ig);
void intg_clear_exact_result(integrator_t *ig);
void intg_set_exact_result_owned(integrator_t *ig, expr_t *expr);

#endif /* INTEGRATOR_SHARED_INTERNAL_H */
