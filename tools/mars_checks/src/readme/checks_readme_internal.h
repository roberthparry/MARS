/**
 * @file checks_readme_internal.h
 * @brief Private single-line progress display for the sequential README runner.
 *
 * Only the README implementation uses this interface. The process observer refreshes elapsed time while a
 * compiler or example runs, without reading its output or changing process ownership. Ordinary callers use
 * checks_readme.h; no progress state is shared between invocations.
 */
#ifndef MARS_CHECKS_README_INTERNAL_H
#define MARS_CHECKS_README_INTERNAL_H

#if !defined(MARS_CHECKS_README_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private README progress requires MARS_CHECKS_README_INTERNAL_ACCESS."
#endif

#include <stdbool.h>
#include <stddef.h>

#include "string-t.h"

/** @brief Owned progress display with a monotonic clock; release with checks_readme_progress_free(). */
typedef struct _checks_readme_progress_t checks_readme_progress_t;

/**
 * @brief Start a compact progress display for an already planned README run.
 * @param total Number of enabled examples; disabled examples must be excluded. Zero is accepted.
 * @return New caller-owned state; release with checks_readme_progress_free(). Allocation or clock failure
 * terminates the checker with an infrastructure error. Use sequentially on the owning thread.
 */
checks_readme_progress_t *checks_readme_progress_new(size_t total);

/**
 * @brief Update completed-example counters and refresh the display when its interval expires.
 * @param[in,out] progress Borrowed state; valid until checks_readme_progress_free().
 * @param completed Number of completed enabled examples, including failures; no greater than the planned total.
 * @param failed Number of failed completed examples; no greater than completed.
 * @return No value. Writes only to standard output; does not retain any example data or create files.
 */
void checks_readme_progress_update(checks_readme_progress_t *progress, size_t completed, size_t failed);

/**
 * @brief Refresh the run's elapsed time from a synchronous subprocess observation callback.
 * @param[in] output_path Borrowed process-capture pathname; ignored and never opened or retained.
 * @param elapsed Child elapsed seconds; ignored because this display times the entire README run.
 * @param[in,out] context Borrowed checks_readme_progress_t pointer, valid throughout the observed invocation.
 * @return No value. Called sequentially by checks_process_run_observed(); never reaps or modifies the child.
 */
void checks_readme_progress_observe(const string_t *output_path, double elapsed, void *context);

/**
 * @brief Clear a replacing terminal line before printing a permanent failure diagnostic.
 * @param[in,out] progress Borrowed display; ownership is unchanged.
 * @return No value. Does nothing when output is redirected or no replacing line is present.
 */
void checks_readme_progress_clear(checks_readme_progress_t *progress);

/**
 * @brief Replace live progress with one final coloured summary, retaining its bar, count and elapsed duration.
 * @param[in,out] progress Borrowed state; ownership remains with the caller.
 * @param success Whether the complete command succeeded, including inspection, evidence writing and cleanup.
 * @return No value. An unsuccessful command is labelled FAIL even with no failed examples; an empty successful
 * selection is labelled SKIPPED. Call once after the run, before checks_readme_progress_free().
 */
void checks_readme_progress_finish(checks_readme_progress_t *progress, bool success);

/**
 * @brief Release progress state and clear any unfinished terminal line.
 * @param[in,out] progress Owned state to release; NULL is accepted and finished summaries remain visible.
 * @return No value. The pointer is invalid afterwards; no background thread or file needs separate cleanup.
 */
void checks_readme_progress_free(checks_readme_progress_t *progress);

#endif
