/**
 * @file computation_internal.h
 * @brief Controlled cross-module access to optional scoped calculation checkpoints.
 *
 * Native algorithms use these checkpoints at work boundaries and poll before
 * publication. Applications establish the scope with the public computation-t.h
 * API. Define MARS_SHARED_COMPUTATION_INTERNAL_ACCESS immediately before inclusion. Both functions act on
 * the calling thread's current scope; neither installs a control nor transfers ownership or interrupts cleanup.
 */
#ifndef MARS_SHARED_COMPUTATION_INTERNAL_H
#define MARS_SHARED_COMPUTATION_INTERNAL_H
#if !defined(MARS_SHARED_COMPUTATION_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_COMPUTATION_INTERNAL_ACCESS."
#endif

#include <stdbool.h>

/**
 * @brief Reserve and report one algorithm-specific work unit in the calling thread's scope.
 * @param stage Borrowed stage name passed to the active computation control.
 * @return True when work may continue or no scope exists; false when the active control requests stopping.
 */
bool computation_step_current(const char *stage);

/**
 * @brief Poll the calling thread's active computation control without reserving work.
 * @return True when work may continue or no scope exists; false when the active control requests stopping.
 */
bool computation_poll_current(void);

#endif
