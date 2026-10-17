/**
 * @file tba_jobs.c
 * @brief Fixed-capacity admission and stale-completion protection for browser worker jobs.
 *
 * Eight slots bound both scheduling work and retained message memory. Tokens
 * carry the slot index for constant-time cancellation; monotonic generations
 * prevent a late worker reply from releasing a newer job. The host serialises
 * calls on the browser thread, so this module requires neither atomics nor locks.
 */
#include "tba_browser.h"
#include "tba_jobs.h"

enum { tba_jobs_slots = 8, tba_jobs_budget = 8 * 1024 * 1024 };
typedef struct {
    uint32_t token;
    unsigned bytes, running;
} tba_job_t;

static tba_job_t tba_jobs[tba_jobs_slots];
static uint32_t tba_jobs_serial;
static unsigned tba_jobs_used, tba_jobs_size;

/* Leave small transfers on the low-overhead synchronous path. */
unsigned tba_jobs_threshold(void)
{
    return 128 * 1024;
}

/* Expose the same policy to the host and regression tests. */
unsigned tba_jobs_capacity(void)
{
    return tba_jobs_slots;
}

/* Bound queueing, startup and execution together. */
unsigned tba_jobs_timeout(void)
{
    return 30000;
}

/* Reserve before the host copies or transfers any retained message bytes. */
uint32_t tba_jobs_submit(unsigned bytes)
{
    if (!bytes || bytes > tba_browser_limit || bytes > tba_jobs_budget - tba_jobs_size ||
        tba_jobs_used == tba_jobs_slots || tba_jobs_serial == UINT32_MAX / tba_jobs_slots)
        return 0;
    /* At most eight slots: fixed admission cost, independent of input size. */
    for (unsigned i = 0; i < tba_jobs_slots; ++i) {
        if (tba_jobs[i].token)
            continue;
        uint32_t token = (++tba_jobs_serial * tba_jobs_slots) | i;
        tba_jobs[i] = (tba_job_t){token, bytes, 0};
        ++tba_jobs_used;
        tba_jobs_size += bytes;
        return token;
    }
    return 0;
}

/* A fixed eight-slot minimum selects FIFO order despite cancelled holes. */
uint32_t tba_jobs_take(void)
{
    tba_job_t *first = 0;
    for (unsigned i = 0; i < tba_jobs_slots; ++i)
        if (tba_jobs[i].token && !tba_jobs[i].running && (!first || tba_jobs[i].token < first->token))
            first = &tba_jobs[i];
    if (!first)
        return 0;
    first->running = 1;
    return first->token;
}

/* Decode the slot directly and verify its generation before changing counters. */
unsigned tba_jobs_finish(uint32_t token)
{
    tba_job_t *job = &tba_jobs[token & (tba_jobs_slots - 1)];
    if (!token || job->token != token)
        return 0;
    --tba_jobs_used;
    tba_jobs_size -= job->bytes;
    *job = (tba_job_t){0};
    return 1;
}

/* Observe queue pressure without mutating ownership. */
unsigned tba_jobs_count(void)
{
    return tba_jobs_used;
}

/* Observe the retained input budget. */
unsigned tba_jobs_bytes(void)
{
    return tba_jobs_size;
}
