/**
 * @file tba_job_worker.c
 * @brief Lifecycle policy for the isolated browser decoding worker.
 *
 * Plans lazy startup, serial dispatch, reply validation, cancellation and closure.
 * The JavaScript host retains an opaque phase per pool, its active token and the
 * actual Worker, promises and timers. No browser resource or transient DOM handle
 * is stored here. Both browser and standalone codec modules use this policy.
 */
#include "tba_jobs.h"

enum { worker_idle, worker_starting, worker_ready, worker_busy, worker_closed };

/* Pack the next opaque phase with the browser effect selected by native policy. */
static unsigned worker_plan(unsigned phase, unsigned action)
{
    return phase | (action << 8);
}

/* Only live controller phases may admit synchronous or asynchronous decoding. */
unsigned tba_jobs_worker_accepts(unsigned phase)
{
    return phase < worker_closed;
}

/* Match exact numeric tokens before any integer conversion or job retirement. */
unsigned tba_jobs_worker_step(unsigned phase, unsigned event, double token, uint32_t active)
{
    if (phase > worker_closed)
        return worker_plan(worker_closed, tba_job_worker_fail);
    if (phase == worker_closed)
        return worker_plan(phase, tba_job_worker_wait);
    if (event == tba_job_worker_close)
        return worker_plan(worker_closed, tba_job_worker_fail);
    if (event == tba_job_worker_failure || (event == tba_job_worker_error && token == 0))
        return worker_plan(worker_idle, tba_job_worker_fail);
    if (event == tba_job_worker_cancel)
        return phase == worker_busy && active && token == active
                   ? worker_plan(worker_idle, tba_job_worker_stop)
                   : worker_plan(phase, tba_job_worker_wait);
    if (event == tba_job_worker_pump) {
        if (!tba_jobs_count() || phase == worker_starting || phase == worker_busy)
            return worker_plan(phase, tba_job_worker_wait);
        if (!active && phase == worker_idle)
            return worker_plan(worker_starting, tba_job_worker_start);
        if (!active && phase == worker_ready)
            return worker_plan(worker_busy, tba_job_worker_send);
    } else if (event == tba_job_worker_ready && phase == worker_starting && !active) {
        return worker_plan(worker_ready, tba_job_worker_wait);
    } else if ((event == tba_job_worker_result || event == tba_job_worker_error) &&
               phase == worker_busy && active && token == active) {
        return worker_plan(worker_ready, tba_job_worker_finish);
    }
    return worker_plan(worker_idle, tba_job_worker_invalid);
}
