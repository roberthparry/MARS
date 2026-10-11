/**
 * @file lab_jobs.h
 * @brief Browser-thread admission and ownership policy for isolated codec jobs.
 *
 * The host owns worker objects, promises and deadlines. This freestanding module
 * bounds pending/running jobs and bytes, assigns non-reused tokens, and selects
 * queued work. No pointers, DOM objects or memory are shared with workers.
 * One queue belongs to one browser module instance; all calls are synchronous.
 */
#ifndef MARS_LAB_JOBS_H
#define MARS_LAB_JOBS_H

#include <stdint.h>

/** @brief Return the byte threshold above which response decoding leaves the UI thread. */
unsigned lab_jobs_threshold(void);

/** @brief Return the total admitted-job limit, including the running job. */
unsigned lab_jobs_capacity(void);

/** @brief Return the whole-job timeout, including queueing and worker startup, in milliseconds. */
unsigned lab_jobs_timeout(void);

/**
 * @brief Admit one bounded message without taking ownership of its memory.
 * @param bytes Message size, at most 4 MiB.
 * @return Unique nonzero token, or zero for invalid size, exhausted tokens or a full queue/byte budget.
 */
uint32_t lab_jobs_submit(unsigned bytes);

/** @brief Mark the oldest queued job running; return its token, or zero when none is queued. */
uint32_t lab_jobs_take(void);

/**
 * @brief Retire a queued/running job; stale tokens cannot retire a replacement.
 * @param token Token returned by lab_jobs_submit().
 * @return One when retired, zero for an absent or stale token.
 */
unsigned lab_jobs_finish(uint32_t token);

/** @brief Return the number of queued and running jobs. */
unsigned lab_jobs_count(void);

/** @brief Return the byte budget held by queued and running jobs. */
unsigned lab_jobs_bytes(void);

#endif
