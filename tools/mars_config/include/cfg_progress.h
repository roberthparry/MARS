/**
 * @file cfg_progress.h
 * @brief Live progress for native installation stages.
 *
 * Owns a lightweight reporting thread while the calling thread performs blocking
 * work. Terminal bars and plain redirected logs go to stderr, never stdout.
 * Finish each stage before prompting or starting another reporter. Labels must
 * be static, trusted descriptions, never passwords, SQL or user-supplied paths.
 */
#ifndef MARS_CFG_PROGRESS_H
#define MARS_CFG_PROGRESS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct cfg_progress cfg_progress_t;

/**
 * @brief Start an indeterminate stage; reporting failure never prevents installation.
 * @param label Borrowed static description, retained until finish; must not be NULL.
 * @return Owned reporter, or NULL if unavailable; release with cfg_progress_finish.
 */
cfg_progress_t *cfg_progress_start(const char *label);

/**
 * @brief Change the current source and its processed-byte count.
 * @param progress Borrowed reporter; NULL is safe.
 * @param label Borrowed static safe description, retained until the next update or finish.
 * @param completed Number of processed bytes, clamped to total for display.
 * @param total Source size in bytes; zero selects an indeterminate indicator.
 */
void cfg_progress_update(cfg_progress_t *progress, const char *label, uint64_t completed, uint64_t total);

/**
 * @brief Stop reporting, print a final status and release the reporter.
 * @param progress Owned reporter; NULL is safe. No concurrent calls may follow.
 * @param success True reports completion; false reports failure, never 100 percent.
 */
void cfg_progress_finish(cfg_progress_t *progress, bool success);

#endif
