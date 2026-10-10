/**
 * @file lab_flow_state_sync.h
 * @brief Synchronous native plans for worksheet persistence and workspace projection.
 *
 * Preserves synchronous browser API contracts while C selects the operations and
 * their order. Browser-owned frames carry values between calls; local storage,
 * clocks, timers and DOM capabilities remain host resources. No mathematics is
 * interpreted here and no borrowed handle is retained by C.
 */
#ifndef LAB_FLOW_STATE_SYNC_H
#define LAB_FLOW_STATE_SYNC_H

/**
 * @brief Advance one synchronous persistence or workspace operation.
 * @param kind Save worksheet 0, publish save 1, save calendar 2, save mode 3,
 * restore editor 4, restore result 5, select mode 6, apply mode 7, keyboard 8,
 * history snapshot 9, history push 10, normalise control 11, flush 12,
 * location completion 13, integrator result projection 14, capture save 15,
 * integrator bounds availability 16.
 * @param frame Borrowed mutable browser-owned continuation frame.
 */
void lab_state_sync(unsigned kind, int frame);

#endif
