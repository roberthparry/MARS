/**
 * @file executor_internal.h
 * @brief Private task publication and worker-pool ownership shared by executor sources.
 *
 * Only executor implementations may access mutable task storage. Public callers
 * use executor_t; terminal release stores publish result data to its readers.
 */
#ifndef MARS_EXECUTOR_INTERNAL_H
#define MARS_EXECUTOR_INTERNAL_H
#if !defined(MARS_EXECUTOR_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_EXECUTOR_INTERNAL_ACCESS."
#endif

#include <stdatomic.h>

#include "thread-t.h"
#include "executor-t.h"

typedef struct _executor_task_t {
    atomic_size_t references;
    atomic_int status;
    lock_t *done;
    computation_t *computation;
    executor_callback_t callback;
    void *argument;
    executor_cleanup_t argument_free;
    executor_cleanup_t result_free;
    void *result;
    diagnostic_t *diagnostic;
    int error;
} executor_task_t;

/** @brief Worker-local pool identity, used to reject blocking dependency and self-destruction calls. */
extern _Thread_local executor_t *executor_current;

/** @brief Create an unpublished task, borrowing its argument until successful submission. */
executor_task_t *executor_task_create(executor_callback_t callback, void *argument,
                                       const executor_task_options_t *options);

/** @brief Run or skip a task, dispose of its argument, and publish terminal output exactly once. */
void executor_task_execute(executor_task_t *task);

#endif
