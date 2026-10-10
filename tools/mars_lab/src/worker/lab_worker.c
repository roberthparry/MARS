/**
 * @file lab_worker.c
 * @brief Sorted dispatch table for calculations built into the MARS Lab server.
 *
 * Keeps mode validation, optional diagnostic executable overrides and entry-point
 * selection together. Calculation entry points are invoked only by the executable's
 * early --worker branch, not directly by persistent request handlers.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lab_worker.h"
#include "lab_worker_internal.h"

typedef struct {
    const char *name;
    const char *environment;
    int (*run)(int argc, char **argv);
} lab_worker_entry_t;

static const lab_worker_entry_t lab_worker_entries[] = {
    {"almanac_event_lab", "MARS_LAB_ALMANAC_EVENT_BINARY", lab_worker_almanac_event},
    {"almanac_lab", "MARS_LAB_ALMANAC_BINARY", lab_worker_almanac},
    {"datetime_lab", "MARS_LAB_DATETIME_BINARY", lab_worker_datetime},
    {"diffequation_lab", "MARS_LAB_DIFFEQUATION_BINARY", lab_worker_diffequation},
    {"equation_lab", "MARS_LAB_EQUATION_BINARY", lab_worker_equation},
    {"holiday_lab", "MARS_LAB_HOLIDAY_BINARY", lab_worker_holiday},
    {"integrator_lab", "MARS_LAB_INTEGRATOR_BINARY", lab_worker_integrator},
    {"mars_lab", "MARS_LAB_BINARY", lab_worker_expression},
    {"matrix_lab", "MARS_LAB_MATRIX_BINARY", lab_worker_matrix},
    {"ophelia", "MARS_LAB_OPHELIA_BINARY", lab_worker_ophelia},
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
    return entry->run(argc, argv);
}
