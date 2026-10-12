/**
 * @file lab_worker.h
 * @brief Built-in calculation dispatch for the MARS Lab server executable.
 *
 * Resolves the fixed set of calculation modes and dispatches a single invocation
 * before listener initialisation. Normal request handlers use lab_process.h to
 * start a fresh instance of the server in calculation mode, retaining deadlines,
 * bounded output, cancellation and independent mathematical state.
 */
#ifndef MARS_LAB_WORKER_H
#define MARS_LAB_WORKER_H

/**
 * @brief Returns the optional executable-override environment key for a known mode.
 * @param name Borrowed calculation mode name, or NULL.
 * @return Static environment key, or NULL for an unknown name. Never reads the environment.
 */
const char *lab_worker_environment(const char *name);

/**
 * @brief Dispatches one calculation in a fresh process, without starting the listener.
 * @param argc Argument count including the calculation mode name.
 * @param argv Borrowed NULL-terminated argument vector, with the mode name at index zero.
 * @return Calculation exit status, or 2 for an unknown mode or invalid arguments.
 */
int lab_worker_dispatch(int argc, char **argv);

/** @brief Parse timeout and work-limit prefixes, then execute a mode under cooperative control in a fresh process. */
int lab_worker_dispatch_controlled(int argc, char **argv);

/** @brief Reserved controlled-worker exit codes; only interpreted for built-in workers. */
enum {
    LAB_WORKER_CANCELLED = 120,
    LAB_WORKER_TIMED_OUT = 121,
    LAB_WORKER_BUDGET_EXHAUSTED = 122,
    LAB_WORKER_CONTROL_FAILED = 123
};

#endif
