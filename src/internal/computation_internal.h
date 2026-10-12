/**
 * @file computation_internal.h
 * @brief Controlled cross-module access to optional scoped calculation checkpoints.
 *
 * Native algorithms use these checkpoints at work boundaries and poll before
 * publication. Applications establish the scope with the public computation-t.h
 * API. Neither function installs a control or interrupts cleanup.
 */
#ifndef MARS_SHARED_COMPUTATION_INTERNAL_H
#define MARS_SHARED_COMPUTATION_INTERNAL_H
#if !defined(MARS_SHARED_COMPUTATION_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_COMPUTATION_INTERNAL_ACCESS."
#endif

#include <stdbool.h>

/** @brief Reserve and report one algorithm-specific unit; without a scope this succeeds immediately. */
bool computation_step_current(const char *stage);

/** @brief Poll scoped stopping without reserving work; without a scope this succeeds immediately. */
bool computation_poll_current(void);

#endif
