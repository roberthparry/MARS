/**
 * @file lab_jobs.c
 * @brief Fixed-capacity admission and stale-completion protection for browser worker jobs.
 *
 * Eight slots bound both scheduling work and retained message memory. Tokens
 * carry the slot index for constant-time cancellation; monotonic generations
 * prevent a late worker reply from releasing a newer job. The host serialises
 * calls on the browser thread, so this module requires neither atomics nor locks.
 */
#include "lab_jobs.h"

enum { lab_jobs_slots = 8, lab_jobs_budget = 8 * 1024 * 1024, lab_jobs_message = 4 * 1024 * 1024 };
typedef struct {
    uint32_t token;
    unsigned bytes, running;
} lab_job_t;

static lab_job_t lab_jobs[lab_jobs_slots];
static uint32_t lab_jobs_serial;
static unsigned lab_jobs_used, lab_jobs_size;

/* Leave small transfers on the low-overhead synchronous path. */
unsigned lab_jobs_threshold(void)
{
    return 128 * 1024;
}

/* Expose the same policy to the host and regression tests. */
unsigned lab_jobs_capacity(void)
{
    return lab_jobs_slots;
}

/* Bound queueing, startup and execution together. */
unsigned lab_jobs_timeout(void)
{
    return 30000;
}

/* Reserve before the host copies or transfers any retained message bytes. */
uint32_t lab_jobs_submit(unsigned bytes)
{
    if (!bytes || bytes > lab_jobs_message || bytes > lab_jobs_budget - lab_jobs_size ||
        lab_jobs_used == lab_jobs_slots || lab_jobs_serial == UINT32_MAX / lab_jobs_slots)
        return 0;
    /* At most eight slots: fixed admission cost, independent of input size. */
    for (unsigned i = 0; i < lab_jobs_slots; ++i) {
        if (lab_jobs[i].token)
            continue;
        uint32_t token = (++lab_jobs_serial * lab_jobs_slots) | i;
        lab_jobs[i] = (lab_job_t){token, bytes, 0};
        ++lab_jobs_used;
        lab_jobs_size += bytes;
        return token;
    }
    return 0;
}

/* A fixed eight-slot minimum selects FIFO order despite cancelled holes. */
uint32_t lab_jobs_take(void)
{
    lab_job_t *first = 0;
    for (unsigned i = 0; i < lab_jobs_slots; ++i)
        if (lab_jobs[i].token && !lab_jobs[i].running && (!first || lab_jobs[i].token < first->token))
            first = &lab_jobs[i];
    if (!first)
        return 0;
    first->running = 1;
    return first->token;
}

/* Decode the slot directly and verify its generation before changing counters. */
unsigned lab_jobs_finish(uint32_t token)
{
    lab_job_t *job = &lab_jobs[token & (lab_jobs_slots - 1)];
    if (!token || job->token != token)
        return 0;
    --lab_jobs_used;
    lab_jobs_size -= job->bytes;
    *job = (lab_job_t){0};
    return 1;
}

/* Observe queue pressure without mutating ownership. */
unsigned lab_jobs_count(void)
{
    return lab_jobs_used;
}

/* Observe the retained input budget. */
unsigned lab_jobs_bytes(void)
{
    return lab_jobs_size;
}
