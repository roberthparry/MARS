/**
 * @file checks_checkpoint.h
 * @brief Content-verified reuse of completed suites and independent README examples.
 *
 * Explicit test commands retain successful checkpoints in tests/tests.db. Executables, supplied build inputs,
 * arguments, test selections, authored inputs, stored case rows and relevant execution settings define the identity.
 * A reused result is labelled separately and never contributes a fabricated test run or timing sample. No database
 * connection remains open while a test programme executes. Unavailable storage always falls back to running tests.
 */
#ifndef MARS_CHECKS_CHECKPOINT_H
#define MARS_CHECKS_CHECKPOINT_H

#include "checks_support.h"

/** @brief Owned immutable invocation identity; release with checks_checkpoint_free(). */
typedef struct _checks_checkpoint_t checks_checkpoint_t;

/**
 * @brief Prepare optional persistent progress for one explicit verification command.
 * @param[in] root Borrowed repository root containing the maintained test schema and selection.
 * @param[in] scope Borrowed command namespace, such as an executable identity or README command.
 * @param[in] arguments Borrowed canonical arguments; owned copies support a final input-identity check.
 * @param[in] files Borrowed absolute input filenames; each must be readable. NULL selects no extra files.
 * @param[in] resume True permits reuse; false forces execution and refreshes the successful checkpoint.
 * @return Owned context, or NULL when storage or input fingerprinting is unavailable. NULL contexts safely run fresh.
 * @note Creates storage only during explicit testing. Checkpoints are local to the repository path and host;
 * inputs are fingerprinted again before saving, so a concurrent build cannot certify an untested executable.
 */
checks_checkpoint_t *checks_checkpoint_new(const string_t *root, const char *scope,
                                          const checks_strings_t *arguments, const checks_strings_t *files, bool resume);

/**
 * @brief Reuse a matching success, otherwise invalidate that job's old checkpoint before executing it.
 * @param[in,out] checkpoint Borrowed context, or NULL for an uncached run.
 * @param[in] id Borrowed stable job identifier within the command scope.
 * @param[in] content Borrowed exact job source/output payload, or NULL for a native suite.
 * @param[out] passed Optional saved passing-test count; initially zero. A missing saved count forces execution.
 * @return True only for an exact previously successful match; false means execute the job, including on I/O failure.
 * @note Closes management storage before returning. Failed and interrupted attempts can never retain an older pass.
 */
bool checks_checkpoint_reuse(checks_checkpoint_t *checkpoint, const char *id, const string_t *content, size_t *passed);

/**
 * @brief Save one successfully completed verification without adding a measurement.
 * @param[in,out] checkpoint Borrowed context, or NULL to do nothing.
 * @param[in] id Borrowed job identifier previously passed to checks_checkpoint_reuse().
 * @param[in] content Borrowed identical source/output payload, or NULL for a native suite.
 * @param[in] passed Optional borrowed passing-test count copied into the successful checkpoint.
 * @return No value; an unavailable checkpoint is diagnosed and the actual test outcome remains authoritative.
 */
void checks_checkpoint_pass(checks_checkpoint_t *checkpoint, const char *id, const string_t *content,
                            const size_t *passed);

/**
 * @brief Release an invocation identity and its owned strings.
 * @param[in,out] checkpoint Owned context, invalid after return; NULL is accepted. No connection is retained.
 * @return No value.
 */
void checks_checkpoint_free(checks_checkpoint_t *checkpoint);

#endif
