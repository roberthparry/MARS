/**
 * @file lab_process_plot_cache.h
 * @brief Private bounded native plot cache for the process adapter.
 *
 * Stores opaque worker preparation and complete plot output in shared anonymous
 * memory. Only the process adapter uses these operations; callers use lab_process.h.
 */
#ifndef MARS_LAB_PROCESS_PLOT_CACHE_H
#define MARS_LAB_PROCESS_PLOT_CACHE_H
#if !defined(MARS_LAB_PROCESS_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_LAB_PROCESS_INTERNAL_ACCESS."
#endif
#include "string-t.h"

/**
 * @brief Build an exact cache key including executable identity and the relevant worker arguments.
 * @param[in] argv Borrowed NULL-terminated worker arguments, including the calculation name.
 * @param[in] cwd Borrowed directory identity, or NULL for the ordinary worker directory.
 * @param[in] path Borrowed executable path; its file identity prevents reuse after rebuilding.
 * @param prepared True selects source, axis, precision and worksheet mode; false includes viewport arguments too.
 * @return Independently owned key, released with string_free(), or NULL if caching is unavailable, disabled,
 * incompatible with a work limit, oversized or unallocatable. Input pointers are not retained.
 * Metadata checks and key construction run outside the process-shared cache lock.
 */
string_t *lab_proc_plot_key(const char *const argv[], const char *cwd, const string_t *path, bool prepared);

/**
 * @brief Build the matching plot-preparation identity for an already completed equation evaluation.
 * @param[in] record Borrowed bounded JSON replay record containing exact source, axis, precision and mode keys.
 * @param[in] cwd Borrowed working-directory identity; NULL denotes the ordinary worker directory.
 * @param[in] path Borrowed executable path used to include its current binary identity in the key.
 * @return An independently owned key, released with string_free(), or NULL for malformed identity fields, oversized or
 * unallocatable records or disabled caching. Inputs remain caller-owned and need live only for this call.
 * Parsing and key construction take place outside the shared cache lock and retain no input pointers.
 */
string_t *lab_proc_plot_record_key(const string_t *record, const char *cwd, const string_t *path);

/**
 * @brief Copy a matching bounded record from the process-shared cache.
 * @param[in] key Borrowed exact key; NULL is an ordinary miss.
 * @param prepared True selects preparation records; false selects complete plot output.
 * @return Independently owned text, released with string_free(), or NULL for a miss, unavailable cache,
 * corrupt or undecodable record, lock failure or allocation failure. No cache-owned pointer escapes.
 * Copies are protected by the shared mutex; decompression occurs after releasing it.
 */
string_t *lab_proc_plot_cache_get(const string_t *key, bool prepared);

/**
 * @brief Publish a complete successful record within the existing fixed cache limits.
 * @param[in] key Borrowed exact key; NULL skips publication.
 * @param[in] value Borrowed complete text to copy; NULL skips publication.
 * @param prepared True selects preparation records; false selects complete plot output.
 * @return Nothing. Oversized records, unavailable storage, compression, lock or allocation failures are
 * ordinary uncached results. Inputs stay caller-owned and need remain valid only for this call.
 * Compression runs outside the shared mutex, which protects only replacement of one bounded slot.
 */
void lab_proc_plot_cache_put(const string_t *key, const string_t *value, bool prepared);

/**
 * @brief Separate the private single-line preparation record from successful native worker output.
 * @param[in,out] output Pointer to caller-owned text. On success its previous value is freed and replaced by
 * independently owned output without the preparation line; the caller releases the replacement with string_free().
 * @return Independently owned preparation text, released with string_free(), or NULL if absent, incomplete or
 * unallocatable, leaving the original output unchanged. Does not validate JSON or perform mathematical work.
 * No input pointers are retained and no shared cache lock is acquired.
 */
string_t *lab_proc_plot_cache_extract(string_t **output);
#endif
