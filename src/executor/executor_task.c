/**
 * @file executor_task.c
 * @brief Independent task lifetimes, cooperative execution and immutable completion records.
 *
 * A pool reference keeps abandoned handles alive until argument cleanup and
 * result publication finish. Terminal atomic states publish all output fields.
 * No callback or destructor runs while the pool's queue mutex is held.
 */
#include <errno.h>
#include <stdlib.h>

#define MARS_EXECUTOR_INTERNAL_ACCESS
#include "executor_internal.h"

_Thread_local executor_t *executor_current;

/* Construct the task without taking ownership of the submitted argument yet. */
executor_task_t *executor_task_create(executor_callback_t callback, void *argument,
                                       const executor_task_options_t *options)
{
    executor_task_t *task = calloc(1, sizeof(*task));
    if (!task)
        return NULL;
    atomic_init(&task->references, 1);
    atomic_init(&task->status, EXECUTOR_TASK_QUEUED);
    task->callback = callback;
    task->argument = argument;
    if (options) {
        task->argument_free = options->argument_free;
        task->result_free = options->result_free;
    }
    task->computation = options && options->computation ? computation_retain(options->computation) : computation_new(NULL);
    task->done = task->computation ? lock_new(LOCK_EVENT, 0) : NULL;
    if (!task->done) {
        executor_task_free(task);
        return NULL;
    }
    return task;
}

/* Retain completion storage independently of the pool. */
executor_task_t *executor_task_retain(executor_task_t *task)
{
    if (!task) {
        errno = EINVAL;
        return NULL;
    }
    size_t references = atomic_load(&task->references);
    do {
        if (references == SIZE_MAX) {
            errno = EOVERFLOW;
            return NULL;
        }
    } while (!atomic_compare_exchange_weak(&task->references, &references, references + 1));
    return task;
}

/* Final release owns the output, but never repeats argument cleanup. */
void executor_task_free(executor_task_t *task)
{
    int saved = errno;
    if (task && atomic_fetch_sub(&task->references, 1) == 1) {
        if (task->result && task->result_free)
            task->result_free(task->result);
        diagnostic_free(task->diagnostic);
        computation_free(task->computation);
        lock_free(task->done);
        free(task);
    }
    errno = saved;
}

/* Atomic status publishes result, diagnostic and error together. */
executor_task_status_t executor_task_status(const executor_task_t *task)
{
    if (!task)
        errno = EINVAL;
    return task ? atomic_load(&task->status) : EXECUTOR_TASK_INVALID;
}

/* Never block a bounded worker on another unfinished task. */
bool executor_task_wait(executor_task_t *task, uint64_t milliseconds)
{
    if (!task) {
        errno = EINVAL;
        return false;
    }
    if (executor_task_status(task) >= EXECUTOR_TASK_SUCCEEDED)
        return true;
    if (executor_current && milliseconds) {
        errno = EDEADLK;
        return false;
    }
    LOCK_AUTO(event) = lock_clone(task->done);
    return event && lock_wait_for(event, milliseconds);
}

/* A cancellation request is not a completion notification. */
void executor_task_cancel(executor_task_t *task)
{
    if (task)
        computation_cancel(task->computation);
}

/* Preserve the distinction between a successful NULL result and pending/failure. */
bool executor_task_result(const executor_task_t *task, void **result)
{
    if (!result) {
        errno = EINVAL;
        return false;
    }
    int error = executor_task_error(task);
    if (error) {
        errno = error;
        return false;
    }
    *result = task->result;
    return true;
}

/* Completion error is transported explicitly rather than through worker-local errno. */
int executor_task_error(const executor_task_t *task)
{
    executor_task_status_t status = executor_task_status(task);
    if (status == EXECUTOR_TASK_INVALID)
        return EINVAL;
    return status < EXECUTOR_TASK_SUCCEEDED ? EBUSY : task->error;
}

/* An observer owns its diagnostic reference even after the task is freed. */
diagnostic_t *executor_task_diagnostic(const executor_task_t *task)
{
    executor_task_status_t status = executor_task_status(task);
    if (status < EXECUTOR_TASK_SUCCEEDED) {
        errno = status == EXECUTOR_TASK_INVALID ? EINVAL : EBUSY;
        return NULL;
    }
    return task->diagnostic ? diagnostic_retain(task->diagnostic) : NULL;
}

/* Expose a retained control, not the task's internal reference. */
computation_t *executor_task_computation(const executor_task_t *task)
{
    if (!task) {
        errno = EINVAL;
        return NULL;
    }
    return computation_retain(task->computation);
}

static bool executor_invoke(void *argument)
{
    executor_task_t *task = argument;
    errno = 0;
    bool success = task->callback(task->argument, &task->result, &task->diagnostic);
    task->error = success ? 0 : errno ? errno : EIO;
    return success;
}

/* Complete skipped and executed tasks through the same ownership path. */
void executor_task_execute(executor_task_t *task)
{
    static const executor_task_status_t states[] = {
        [COMPUTATION_ACTIVE]           = EXECUTOR_TASK_FAILED,
        [COMPUTATION_COMPLETED]        = EXECUTOR_TASK_FAILED,
        [COMPUTATION_CANCELLED]        = EXECUTOR_TASK_CANCELLED,
        [COMPUTATION_TIMED_OUT]        = EXECUTOR_TASK_TIMED_OUT,
        [COMPUTATION_BUDGET_EXHAUSTED]  = EXECUTOR_TASK_BUDGET_EXHAUSTED,
        [COMPUTATION_FAILED]           = EXECUTOR_TASK_FAILED,
    };
    static const int errors[] = {
        [COMPUTATION_ACTIVE]           = EIO,
        [COMPUTATION_COMPLETED]        = EALREADY,
        [COMPUTATION_CANCELLED]        = ECANCELED,
        [COMPUTATION_TIMED_OUT]        = ETIMEDOUT,
        [COMPUTATION_BUDGET_EXHAUSTED]  = E2BIG,
        [COMPUTATION_FAILED]           = EIO,
    };
    atomic_store(&task->status, EXECUTOR_TASK_RUNNING);
    bool success = computation_run(task->computation, executor_invoke, task);
    computation_status_t control = computation_status(task->computation);
    success = success && control == COMPUTATION_ACTIVE;
    executor_task_status_t status = success ? EXECUTOR_TASK_SUCCEEDED : states[control];
    if (!success) {
        if (!task->error || control != COMPUTATION_ACTIVE)
            task->error = errors[control];
        if (task->result && task->result_free)
            task->result_free(task->result);
        task->result = NULL;
        if (!task->diagnostic)
            task->diagnostic = diagnostic_new(DIAGNOSTIC_ERROR, "executor", task->error,
                                               "Task did not complete successfully", NULL, NULL);
    }
    if (task->argument_free)
        task->argument_free(task->argument);
    task->argument = NULL;
    atomic_store(&task->status, status);
    lock_signal(task->done);
}
