/**
 * @file tba_worker.h
 * @brief Private entry point for server-owned calculations.
 *
 * The in-memory entry point borrows observations for a synchronous calculation
 * and returns an owned result without file I/O. It does not fall back to paths
 * when an observation buffer is missing. No file-based entry point is exposed.
 */
#ifndef TBA_WORKER_H
#define TBA_WORKER_H
#include "tba_support.h"

/**
 * @brief Fit using borrowed, in-memory observations without reading or writing files.
 * @param argc Number of validated model arguments, including the programme name.
 * @param argv Borrowed model argument vector; no path arguments are accepted.
 * @param target Borrowed CSV text constructed from the selected observations in memory.
 * @param drivers Optional borrowed CSV text for additional selected observations.
 * @return Owned structured result or error; release with json_free.
 */
json_t *tba_worker_memory(int argc, const char *const *argv, const string_t *target, const string_t *drivers);
#endif
