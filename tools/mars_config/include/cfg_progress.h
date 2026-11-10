/**
 * @file cfg_progress.h
 * @brief Live progress for native installation stages.
 *
 * Owns a lightweight reporting thread while the calling thread performs blocking
 * work. Terminal bars and plain redirected logs go to stderr, never stdout.
 * Terminal bars use solid and fractional blocks with blank space for unfinished work.
 * Finish each stage before prompting or starting another reporter. Labels must
 * be static, trusted descriptions, never passwords, SQL or user-supplied paths.
 * A heartbeat confirms only that reporting is alive. Measured stages estimate
 * remaining time from source-local observations. Item stages show the current segment, its total and elapsed
 * time since the previous completion; batch stages supply their own segment size. Byte-only stages omit
 * unknown segment counts rather than inventing totals.
 */
#ifndef MARS_CFG_PROGRESS_H
#define MARS_CFG_PROGRESS_H

#include <stdbool.h>
#include <stdint.h>

#include "string-t.h"

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
 * Each item is one segment; the current one-based segment and its elapsed seconds are shown during processing.
 */
void cfg_progress_update_items(cfg_progress_t *progress, const char *label, uint64_t completed, uint64_t total,
                               const char *unit);

/**
 * @brief Report completed units processed in fixed-size segments, including a possibly shorter final segment.
 * @param[in,out] progress Borrowed reporter; NULL is safe. Call serially with updates and finish.
 * @param[in] label Borrowed static safe description, retained until the next update or finish.
 * @param[in] completed Successfully processed units, clamped to total; update after each complete segment.
 * @param[in] total Expected unit count; zero omits unknown segment counts.
 * @param[in] unit Borrowed static counter description, retained until the next update or finish; NULL is ignored.
 * @param[in] segment_size Maximum units per segment; zero is ignored. A bulk operation may use total as its size.
 * @note No argument is consumed. Segment counts use ceiling division without integer overflow. While units remain,
 * the display shows the current one-based segment and the elapsed time since the preceding completion, or since
 * the source started for its first segment. Source changes and backwards counts reset timing. Counts alone never
 * confirm stage success; the caller must use cfg_progress_finish(). Reporting remains constant-time and thread-safe
 * against the reporting worker; callers must serialise their own updates and finish.
 */
void cfg_progress_update_segments(cfg_progress_t *progress, const char *label, uint64_t completed, uint64_t total,
                                  const char *unit, uint64_t segment_size);

/**
 * @brief Set an optional region, layer or table name for the segment about to run, without resetting its timer.
 * @param[in,out] progress Borrowed reporter; NULL is ignored. Call serially with updates and finish.
 * @param[in] name Borrowed trusted display text, copied during the call; no argument is consumed. NULL, empty
 * text or text longer than 96 UTF-8 bytes clears the name. Control characters become spaces. Never supply paths
 * or credentials. A known table identifier may be supplied directly.
 * @note The worker displays the owned copy in parentheses. Allocation failure omits the name without preventing
 * work. Advancing the completed count or changing sources clears it, so the previous region cannot label the next
 * segment. Set the next name before starting its work. The caller may free its string immediately after return.
 */
void cfg_progress_segment_name(cfg_progress_t *progress, const string_t *name);

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
