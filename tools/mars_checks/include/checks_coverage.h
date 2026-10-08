/**
 * @file checks_coverage.h
 * @brief Native GCC JSON coverage policy for the public file module.
 *
 * The developer command reads compressed reports through a shell-free gzip
 * process and uses the public string, JSON and file APIs. It requires reports
 * for every src/file C source, execution of every public file function, and
 * aggregate line and branch coverage of at least 90 and 80 per cent respectively.
 * Inputs are borrowed; temporary report data is released before returning.
 */
#ifndef MARS_CHECKS_COVERAGE_H
#define MARS_CHECKS_COVERAGE_H

#include "ustring.h"

/**
 * @brief Implement file-coverage DIR relative to the supplied repository root.
 * @param root Borrowed repository root, also used to resolve a relative DIR.
 * @param argc Number of command arguments, excluding the command name; must be one.
 * @param argv Borrowed arguments containing the report directory.
 * @return Zero on success, one for rejected coverage or reports, two for invalid usage or I/O failure.
 * @details Requires gzip on PATH. Rejects duplicate sources, lines, functions and
 * JSON members, invalid unsigned 64-bit counters and zero aggregate denominators.
 */
int checks_coverage(const string_t *root, int argc, char **argv);

#endif
