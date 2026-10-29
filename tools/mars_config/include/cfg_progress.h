/**
 * @file cfg_progress.h
 * @brief Live progress for native installation stages.
 *
 * Owns a lightweight reporting thread while the calling thread performs blocking
 * work. Terminal bars and plain redirected logs go to stderr, never stdout.
 * Finish each stage before prompting or starting another reporter. Labels must
 * be static, trusted descriptions, never passwords, SQL or user-supplied paths.
 * A heartbeat confirms only that reporting is alive. Measured stages estimate
 * remaining time from source-local observations and identify unchanged counts.
 */
#ifndef MARS_CFG_PROGRESS_H
#define MARS_CFG_PROGRESS_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Opaque reporter owned by its creator and released with cfg_progress_finish(). */
typedef struct _cfg_progress_t cfg_progress_t;

/**
 * @brief Start an indeterminate stage; reporting failure never prevents installation.
 * @param label Borrowed static description, retained until finish; must not be NULL.
 * @return Owned reporter, or NULL if unavailable; release with cfg_progress_finish.
 */
cfg_progress_t *cfg_progress_start(const char *label);

/**
 * @brief Show the current source's processed/total byte counts alongside its bar and percentage.
 * @param progress Borrowed reporter; NULL is safe.
 * @param label Borrowed static safe description, retained until the next update or finish.
 * @param completed Number of processed bytes, clamped to total for display.
 * @param total Source size in bytes; zero selects an indeterminate indicator.
 */
void cfg_progress_update(cfg_progress_t *progress, const char *label, uint64_t completed, uint64_t total);

/**
 * @brief Report completed items with an explicit count alongside the progress bar.
 * @param[in,out] progress Borrowed reporter; NULL is safe. Serialise updates and finish on the calling thread.
 * @param[in] label Borrowed static safe description, retained until the next update or finish.
 * @param[in] completed Number of successfully processed items, clamped to total for display.
 * @param[in] total Number of expected items; zero selects an indeterminate indicator without a counter.
 * @param[in] unit Borrowed static counter description, such as "tiles imported"; retained until the next update
 * or finish. Must not be NULL and must never contain credentials, SQL or user-supplied paths.
 * @note No argument is consumed. The caller still confirms stage success with cfg_progress_finish(); reporting
 * all expected items does not itself report successful completion. Ordinary byte updates replace the item unit
 * with "bytes processed". Unknown totals retain the indeterminate indicator without a counter.
 */
void cfg_progress_update_items(cfg_progress_t *progress, const char *label, uint64_t completed, uint64_t total,
                               const char *unit);

/**
 * @brief Print an explanation above the live progress line, then redraw the current progress.
 * @param[in,out] progress Borrowed reporter; NULL is safe. Call serially with updates and finish.
 * @param[in] description Borrowed static, trusted stage explanation; NULL is safe. Printed during this call
 * and neither consumed nor retained. Never pass credentials, SQL text or user-supplied paths.
 * @note Use this for expensive or opaque stages whose duration or completion cannot be inferred from a count.
 * It does not claim that work has advanced and does not reset progress timing or counts.
 */
void cfg_progress_explain(cfg_progress_t *progress, const char *description);

/**
 * @brief Stop reporting, print a final status and release the reporter.
 * @param progress Owned reporter; NULL is safe. No concurrent calls may follow.
 * @param success True reports completion; false reports failure, never 100 percent.
 */
void cfg_progress_finish(cfg_progress_t *progress, bool success);

#endif
