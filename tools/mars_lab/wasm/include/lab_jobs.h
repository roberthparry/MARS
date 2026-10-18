/**
 * @file lab_jobs.h
 * @brief Browser-thread admission and ownership policy for isolated codec jobs.
 *
 * The host owns worker objects, promises and deadlines. This freestanding module
 * bounds pending/running jobs and bytes, assigns non-reused tokens, and selects
 * queued work. Worker lifecycle transitions are native policy; the host retains
 * only an opaque phase value and browser resource handles. No pointers, DOM
 * objects or memory are shared with workers.
 * One queue belongs to one browser module instance; all calls are synchronous.
 */
#ifndef MARS_LAB_JOBS_H
#define MARS_LAB_JOBS_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Query the response size threshold for worker decoding.
 * @return Threshold in bytes, currently 128 KiB.
 */
unsigned lab_jobs_threshold(void);

/**
 * @brief Query the total admitted-job limit, including running jobs.
 * @return Maximum number of queued and running jobs, currently eight.
 */
unsigned lab_jobs_capacity(void);

/**
 * @brief Query the whole-job timeout, including queueing and worker startup.
 * @return Timeout in milliseconds, currently 30000.
 */
unsigned lab_jobs_timeout(void);

/**
 * @brief Admit one bounded message without taking ownership of its memory.
 * @param bytes Non-zero message size in bytes, at most lab_browser_limit (8 MiB).
 * @return Unique nonzero token, or zero for invalid size, exhausted tokens or a full queue/byte budget.
 */
uint32_t lab_jobs_submit(unsigned bytes);

/**
 * @brief Mark the oldest queued job as running.
 * @return Its admission token, or zero when no job is queued.
 */
uint32_t lab_jobs_take(void);

/**
 * @brief Retire a queued/running job; stale tokens cannot retire a replacement.
 * @param token Token returned by lab_jobs_submit().
 * @return True when retired, false for an absent or stale token.
 */
bool lab_jobs_finish(uint32_t token);

/**
 * @brief Query current queue occupancy without changing ownership.
 * @return Number of queued and running jobs.
 */
unsigned lab_jobs_count(void);

/**
 * @brief Query retained input memory without changing ownership.
 * @return Total message bytes reserved by queued and running jobs, not the remaining budget.
 */
unsigned lab_jobs_bytes(void);

/** @brief Browser worker events; numeric values are part of the browser-module ABI. */
typedef enum lab_job_worker_event {
    lab_job_worker_pump    = 0, /**< Dispatch queued work. */
    lab_job_worker_ready   = 1, /**< Worker startup completed. */
    lab_job_worker_result  = 2, /**< Worker returned a decoded result. */
    lab_job_worker_error   = 3, /**< Worker reported a job error. */
    lab_job_worker_failure = 4, /**< Worker resource failed. */
    lab_job_worker_cancel  = 5, /**< Cancel the selected job. */
    lab_job_worker_close   = 6, /**< Close the worker pool. */
    lab_job_worker_unknown = 7, /**< Unrecognised worker event. */
} lab_job_worker_event_t;

/** @brief Host effects returned by the worker controller, above the low phase byte. */
typedef enum lab_job_worker_action {
    lab_job_worker_wait    = 0, /**< No browser action. */
    lab_job_worker_start   = 1, /**< Start a worker. */
    lab_job_worker_send    = 2, /**< Send the next job. */
    lab_job_worker_finish  = 3, /**< Complete the active job. */
    lab_job_worker_fail    = 4, /**< Fail the pool's jobs. */
    lab_job_worker_invalid = 5, /**< Reject an invalid transition. */
    lab_job_worker_stop    = 6, /**< Stop the active worker. */
} lab_job_worker_action_t;

/** @brief Worker lifecycle phases stored in the low byte of each packed plan. */
typedef enum {
    LAB_JOB_WORKER_IDLE     = 0, /**< Open pool without a worker. */
    LAB_JOB_WORKER_STARTING = 1, /**< Worker startup is pending. */
    LAB_JOB_WORKER_READY    = 2, /**< Worker is ready for a job. */
    LAB_JOB_WORKER_BUSY     = 3, /**< Worker owns an active job. */
    LAB_JOB_WORKER_CLOSED   = 4, /**< Pool is closed. */
} lab_job_worker_phase_t;

/**
 * @brief Plan one worker lifecycle transition without retaining browser resources.
 * @param phase Phase from the previous result's low byte; initially LAB_JOB_WORKER_IDLE.
 * @param event Worker lifecycle event; unknown values retain invalid-transition handling.
 * @param token Reply or cancellation token, kept as a double to reject fractional or wrapped identifiers.
 * @param active Currently dispatched token, or zero when no job is running.
 * @return New lab_job_worker_phase_t in the low byte and lab_job_worker_action_t in the remaining bits.
 */
unsigned lab_jobs_worker_step(lab_job_worker_phase_t phase, lab_job_worker_event_t event,
                              double token, uint32_t active);

/**
 * @brief Check whether the worker lifecycle still admits decode requests.
 * @param phase Phase from the low byte of lab_jobs_worker_step; initially LAB_JOB_WORKER_IDLE.
 * @return True for an open phase; false for closed or invalid phases.
 */
bool lab_jobs_worker_accepts(lab_job_worker_phase_t phase);

#endif
