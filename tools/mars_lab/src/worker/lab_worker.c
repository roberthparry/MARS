/**
 * @file lab_worker.c
 * @brief Sorted dispatch table for calculations built into the MARS Lab server.
 *
 * Keeps mode validation, optional diagnostic executable overrides and entry-point
 * selection together. Calculation entry points are invoked only by the executable's
 * early --worker branch, not directly by persistent request handlers. Successful
 * mathematical calls include monotonic elapsed-time metadata without repeating work.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lab_worker.h"
#define MARS_LAB_WORKER_INTERNAL_ACCESS
#include "lab_worker_internal.h"

typedef struct {
    const char *name;
    const char *environment;
    int (*run)(int argc, char **argv);
    bool timed;
} lab_worker_entry_t;

static const lab_worker_entry_t lab_worker_entries[] = {
    {"almanac_event_lab", "MARS_LAB_ALMANAC_EVENT_BINARY", lab_worker_almanac_event, false},
    {"almanac_lab", "MARS_LAB_ALMANAC_BINARY", lab_worker_almanac, false},
    {"datetime_lab", "MARS_LAB_DATETIME_BINARY", lab_worker_datetime, false},
    {"diffequation_lab", "MARS_LAB_DIFFEQUATION_BINARY", lab_worker_diffequation, true},
    {"equation_lab", "MARS_LAB_EQUATION_BINARY", lab_worker_equation, true},
    {"holiday_lab", "MARS_LAB_HOLIDAY_BINARY", lab_worker_holiday, false},
    {"integrator_lab", "MARS_LAB_INTEGRATOR_BINARY", lab_worker_integrator, true},
    {"mars_lab", "MARS_LAB_BINARY", lab_worker_expression, true},
    {"matrix_lab", "MARS_LAB_MATRIX_BINARY", lab_worker_matrix, true},
    {"ophelia", "MARS_LAB_OPHELIA_BINARY", lab_worker_ophelia, false},
};

static int lab_worker_compare(const void *name, const void *entry)
{
    return strcmp(name, ((const lab_worker_entry_t *)entry)->name);
}

static const lab_worker_entry_t *lab_worker_find(const char *name)
{
    return name ? bsearch(name, lab_worker_entries, sizeof(lab_worker_entries) / sizeof(*lab_worker_entries),
                          sizeof(*lab_worker_entries), lab_worker_compare) : NULL;
}

/* Share mode validation and diagnostic override names with the process adapter. */
const char *lab_worker_environment(const char *name)
{
    const lab_worker_entry_t *entry = lab_worker_find(name);
    return entry ? entry->environment : NULL;
}

/* Run once in an isolated server invocation, before any listener or cache setup. */
int lab_worker_dispatch(int argc, char **argv)
{
    const lab_worker_entry_t *entry = argc > 0 && argv ? lab_worker_find(argv[0]) : NULL;
    if (!entry) {
        fputs("Unknown or missing MARS Lab calculation mode\n", stderr);
        return 2;
    }
    struct timespec started, finished;
    bool plot = argc > 4 && !strcmp(argv[4], "plot");
    bool timed = entry->timed && !plot && clock_gettime(CLOCK_MONOTONIC, &started) == 0;
    int result = entry->run(argc, argv);
    if (!result && timed && clock_gettime(CLOCK_MONOTONIC, &finished) == 0) {
        int64_t elapsed = (int64_t)(finished.tv_sec - started.tv_sec) * INT64_C(1000000000) +
                          finished.tv_nsec - started.tv_nsec;
        if (elapsed >= 0)
            printf("calculation_time_ns %llu\n", (unsigned long long)elapsed);
    }
    return result;
}
