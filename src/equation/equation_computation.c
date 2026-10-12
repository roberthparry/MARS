/**
 * @file equation_computation.c
 * @brief Scoped equation work accounting and cooperative solver progress.
 *
 * Equation algorithms share their caller's computation_run allowance with nested
 * expression work. Checkpoints bound solver attempts and numerical iterations;
 * cleanup and rollback always run without reserving additional work.
 */
#include "computation-t.h"
#define MARS_EQUATION_INTERNAL_ACCESS
#include "equation_internal.h"

/* Report meaningful solver steps, with no invented total or percentage. */
bool equ_computation_step(const char *stage)
{
    computation_t *control = computation_current();
    if (!control)
        return true;
    return computation_checkpoint(control, 1) &&
           computation_report(control, stage, computation_work_used(control), 0);
}

/* Poll before fallback or publication without charging cleanup against the budget. */
bool equ_computation_active(void)
{
    computation_t *control = computation_current();
    return !control || computation_checkpoint(control, 0);
}
