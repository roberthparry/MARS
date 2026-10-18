/**
 * @file lab_job_worker.c
 * @brief Lifecycle policy for the isolated browser decoding worker.
 *
 * Plans lazy startup, serial dispatch, reply validation, cancellation and closure.
 * The JavaScript host retains an opaque phase per pool, its active token and the
 * actual Worker, promises and timers. No browser resource or transient DOM handle
 * is stored here. Both browser and standalone codec modules use this policy.
 */
#include "lab_jobs.h"

/* Pack the next opaque phase with the browser effect selected by native policy. */
static unsigned worker_plan(lab_job_worker_phase_t phase, lab_job_worker_action_t action)
{
    return (unsigned)phase | ((unsigned)action << 8);
}

/* Only live controller phases may admit synchronous or asynchronous decoding. */
bool lab_jobs_worker_accepts(lab_job_worker_phase_t phase)
{
    return (unsigned)phase < LAB_JOB_WORKER_CLOSED;
}

/**
 * @brief Select startup or dispatch for a queue-pump event.
 * @param phase Valid open worker phase.
 * @param token Event token, unused for queue pumping.
 * @param active Currently dispatched token, or zero when no job is running.
 * @return Packed next phase and action, including invalid-transition handling.
 */
static unsigned worker_pump(lab_job_worker_phase_t phase, double token, uint32_t active);

/**
 * @brief Accept startup completion only while waiting for an idle worker.
 * @param phase Valid open worker phase.
 * @param token Event token, unused for startup completion.
 * @param active Currently dispatched token, which must be zero.
 * @return Packed ready phase and wait action, or an invalid-transition plan.
 */
static unsigned worker_ready(lab_job_worker_phase_t phase, double token, uint32_t active);

/**
 * @brief Match a result or job-error token without integer conversion.
 * @param phase Valid open worker phase.
 * @param token Unconverted numeric reply token.
 * @param active Currently dispatched token, which must be nonzero and match token.
 * @return Packed ready phase and finish action, or an invalid-transition plan.
 */
static unsigned worker_reply(lab_job_worker_phase_t phase, double token, uint32_t active);

/**
 * @brief Cancel only the matching active job, retaining other phases unchanged.
 * @param phase Valid open worker phase.
 * @param token Unconverted numeric cancellation token.
 * @param active Currently dispatched token, or zero when no job is running.
 * @return Packed idle phase and stop action on a match; otherwise the current phase and wait action.
 */
static unsigned worker_cancel(lab_job_worker_phase_t phase, double token, uint32_t active);

/* Read queue occupancy before applying the existing phase and active-token checks. */
static unsigned worker_pump(lab_job_worker_phase_t phase, double token, uint32_t active)
{
    (void)token;
    if (!lab_jobs_count() || phase == LAB_JOB_WORKER_STARTING || phase == LAB_JOB_WORKER_BUSY)
        return worker_plan(phase, lab_job_worker_wait);
    if (!active && phase == LAB_JOB_WORKER_IDLE)
        return worker_plan(LAB_JOB_WORKER_STARTING, lab_job_worker_start);
    if (!active && phase == LAB_JOB_WORKER_READY)
        return worker_plan(LAB_JOB_WORKER_BUSY, lab_job_worker_send);
    return worker_plan(LAB_JOB_WORKER_IDLE, lab_job_worker_invalid);
}

/* A ready event can only complete the startup phase. */
static unsigned worker_ready(lab_job_worker_phase_t phase, double token, uint32_t active)
{
    (void)token;
    return phase == LAB_JOB_WORKER_STARTING && !active
               ? worker_plan(LAB_JOB_WORKER_READY, lab_job_worker_wait)
               : worker_plan(LAB_JOB_WORKER_IDLE, lab_job_worker_invalid);
}

/* Results and job errors share exact token matching and retirement policy. */
static unsigned worker_reply(lab_job_worker_phase_t phase, double token, uint32_t active)
{
    return phase == LAB_JOB_WORKER_BUSY && active && token == active
               ? worker_plan(LAB_JOB_WORKER_READY, lab_job_worker_finish)
               : worker_plan(LAB_JOB_WORKER_IDLE, lab_job_worker_invalid);
}

/* A nonmatching cancellation remains a wait, unlike an invalid reply. */
static unsigned worker_cancel(lab_job_worker_phase_t phase, double token, uint32_t active)
{
    return phase == LAB_JOB_WORKER_BUSY && active && token == active
               ? worker_plan(LAB_JOB_WORKER_IDLE, lab_job_worker_stop)
               : worker_plan(phase, lab_job_worker_wait);
}

/* Match exact numeric tokens before any integer conversion or job retirement. */
unsigned lab_jobs_worker_step(lab_job_worker_phase_t phase, lab_job_worker_event_t event, double token, uint32_t active)
{
    if ((unsigned)phase > LAB_JOB_WORKER_CLOSED)
        return worker_plan(LAB_JOB_WORKER_CLOSED, lab_job_worker_fail);
    if (phase == LAB_JOB_WORKER_CLOSED)
        return worker_plan(phase, lab_job_worker_wait);
    if (event == lab_job_worker_close)
        return worker_plan(LAB_JOB_WORKER_CLOSED, lab_job_worker_fail);
    if (event == lab_job_worker_failure || (event == lab_job_worker_error && token == 0))
        return worker_plan(LAB_JOB_WORKER_IDLE, lab_job_worker_fail);
    static unsigned (*const handlers[lab_job_worker_unknown])(lab_job_worker_phase_t, double, uint32_t) = {
        [lab_job_worker_pump]   = worker_pump,
        [lab_job_worker_ready]  = worker_ready,
        [lab_job_worker_result] = worker_reply,
        [lab_job_worker_error]  = worker_reply,
        [lab_job_worker_cancel] = worker_cancel,
    };
    if ((unsigned)event < sizeof handlers / sizeof *handlers && handlers[event])
        return handlers[event](phase, token, active);
    return worker_plan(LAB_JOB_WORKER_IDLE, lab_job_worker_invalid);
}
