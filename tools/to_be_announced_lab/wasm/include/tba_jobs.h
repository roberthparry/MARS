/**
 * @file tba_jobs.h
 * @brief Browser-thread admission and ownership policy for isolated codec jobs.
 *
 * The host owns worker objects, promises and deadlines. This freestanding module
 * bounds pending/running jobs and bytes, assigns non-reused tokens, and selects
 * queued work. Worker lifecycle transitions are native policy; the host retains
 * only an opaque phase value and browser resource handles. No pointers, DOM
 * objects or memory are shared with workers.
 * One queue belongs to one browser module instance; all calls are synchronous.
 */
#ifndef MARS_TBA_JOBS_H
#define MARS_TBA_JOBS_H

#include <stdint.h>

/** @brief Return the byte threshold above which response decoding leaves the UI thread. */
unsigned tba_jobs_threshold(void);

/** @brief Return the total admitted-job limit, including the running job. */
unsigned tba_jobs_capacity(void);

/** @brief Return the whole-job timeout, including queueing and worker startup, in milliseconds. */
unsigned tba_jobs_timeout(void);

/**
 * @brief Admit one bounded message without taking ownership of its memory.
 * @param bytes Message size, bounded by tba_browser_message_limit().
 * @return Unique nonzero token, or zero for invalid size, exhausted tokens or a full queue/byte budget.
 */
uint32_t tba_jobs_submit(unsigned bytes);

/** @brief Mark the oldest queued job running; return its token, or zero when none is queued. */
uint32_t tba_jobs_take(void);

/**
 * @brief Retire a queued/running job; stale tokens cannot retire a replacement.
 * @param token Token returned by tba_jobs_submit().
 * @return One when retired, zero for an absent or stale token.
 */
unsigned tba_jobs_finish(uint32_t token);

/** @brief Return the number of queued and running jobs. */
unsigned tba_jobs_count(void);

/** @brief Return the byte budget held by queued and running jobs. */
unsigned tba_jobs_bytes(void);

/** @brief Browser worker events; numeric values are part of the browser-module ABI. */
enum tba_job_worker_event {
    tba_job_worker_pump,
    tba_job_worker_ready,
    tba_job_worker_result,
    tba_job_worker_error,
    tba_job_worker_failure,
    tba_job_worker_cancel,
    tba_job_worker_close,
    tba_job_worker_unknown
};

/** @brief Host effects returned by the worker controller, above the low phase byte. */
enum tba_job_worker_action {
    tba_job_worker_wait,
    tba_job_worker_start,
    tba_job_worker_send,
    tba_job_worker_finish,
    tba_job_worker_fail,
    tba_job_worker_invalid,
    tba_job_worker_stop
};

/**
 * @brief Plan one worker lifecycle transition without retaining browser resources.
 * @param phase Opaque phase from the previous result's low byte; initially zero.
 * @param event A tba_job_worker_event value.
 * @param token Reply or cancellation token, kept as a double to reject fractional or wrapped identifiers.
 * @param active Currently dispatched token, or zero when no job is running.
 * @return New phase in the low byte and a tba_job_worker_action in the remaining bits.
 */
unsigned tba_jobs_worker_step(unsigned phase, unsigned event, double token, uint32_t active);

/** @brief Return whether an opaque worker phase still accepts decode requests. */
unsigned tba_jobs_worker_accepts(unsigned phase);

#endif
