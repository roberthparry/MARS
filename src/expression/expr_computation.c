/**
 * @file expr_computation.c
 * @brief Scoped calculation checkpoints for recursive expression algorithms.
 *
 * Expression operations inherit computation_run's native thread-local control.
 * Each recursive node or iterative step reserves one unit; low-frequency reports
 * let observers cancel without reporting every node. Stop handling uses ordinary
 * returns and ownership cleanup, never non-local exits through mathematical code.
 */
#include "computation-t.h"
#define MARS_EXPR_INTERNAL_ACCESS
#include "expr_internal.h"

/* Report only at bounded work intervals; stage totals are deliberately unknown. */
bool expr_computation_step(const char *stage)
{
    computation_t *control = computation_current();
    if (!control)
        return true;
    if (!computation_checkpoint(control, 1))
        return false;
    uint64_t used = computation_work_used(control);
    return (used != 1 && used % 256 != 0) || computation_report(control, stage, used, 0);
}

/* A zero-cost poll is also used before publishing cache entries or results. */
bool expr_computation_active(void)
{
    computation_t *control = computation_current();
    return !control || computation_checkpoint(control, 0);
}
