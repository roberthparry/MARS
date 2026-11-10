/**
 * @file checks_benchmarks.h
 * @brief Persistent mathematical benchmark commands for the native repository tools.
 *
 * Only an explicit benchmark command creates bench/benchmarks.db. Measurements are isolated from the test catalogue,
 * retain their machine and build provenance, and use checked monotonic batch timings. This application interface
 * owns command dispatch; ordinary library clients do not depend on benchmark storage.
 */
#ifndef MARS_CHECKS_BENCHMARKS_H
#define MARS_CHECKS_BENCHMARKS_H

#include "string-t.h"

/**
 * @brief Execute a benchmark catalogue, measurement, reporting or documentation command.
 * @param[in] root Borrowed repository root, valid throughout this synchronous call.
 * @param[in] argc Number of command arguments, excluding the programme and bench-db command.
 * @param[in] argv Borrowed argument array; no argument ownership is transferred.
 * @return Zero on success, one on measurement, I/O or storage failure, or two for invalid arguments.
 * @note Run sequentially. Read-only status/report commands never create storage. Workers measure only mathematical
 * calls; argument parsing, result destruction, SQL and report generation remain outside timed intervals.
 */
int checks_benchmarks(const string_t *root, int argc, char **argv);

#endif
