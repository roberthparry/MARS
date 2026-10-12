/**
 * @file lab_process_request.h
 * @brief Request-local process cancellation without exposing POSIX signal types.
 *
 * Used by HTTP routes to connect their borrowed transport lifetime to child
 * supervision. Signal-handler integration remains in lab_process.h.
 */
#ifndef MARS_LAB_PROCESS_REQUEST_H
#define MARS_LAB_PROCESS_REQUEST_H

#include <stdbool.h>

/** @brief Borrowed request cancellation context; immutable while copied worker scopes are active. */
typedef struct {
    bool (*check)(void *); /**< Optional thread-safe observer; never frees its borrowed argument. */
    void *argument; /**< Must outlive every task carrying this snapshot. */
} lab_proc_cancel_context_t;

/** @brief Install a borrowed thread-local cancellation poll; clear before its argument goes out of scope. */
void lab_proc_set_cancel_check(bool (*check)(void *), void *argument);

/** @brief Copy the calling thread's observer for explicitly propagated, bounded worker tasks. */
lab_proc_cancel_context_t lab_proc_cancel_context(void);

#endif
