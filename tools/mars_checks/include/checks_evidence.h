/**
 * @file checks_evidence.h
 * @brief Native release dependency evidence command.
 *
 * Collects source state, host and tool versions, dependency package ownership
 * and SHA-256 records for a built release. Reports are published atomically
 * only after mandatory inspections succeed. Optional probes may be unavailable.
 * The caller owns all arguments; execution is synchronous and shell-free.
 * Only inspect trusted built artefacts: dependency collection invokes ldd.
 * Output parent directories must be trusted. Existing output symbolic links,
 * multiply linked files and aliases of mandatory inputs are rejected. Cleanup
 * failure after publication is reported as a warning without reversing success.
 */
#ifndef MARS_CHECKS_EVIDENCE_H
#define MARS_CHECKS_EVIDENCE_H

#include "ustring.h"

/**
 * @brief Write release evidence using --library, --output and --allow-dirty.
 * @param root Borrowed repository root, used for relative paths and subprocesses.
 * @param argc Number of arguments following the release-evidence command.
 * @param argv Borrowed argument vector, excluding the command name.
 * @return Zero on success, one on inspection or publication failure, two for invalid arguments.
 */
int checks_evidence(const string_t *root, int argc, char **argv);

#endif
