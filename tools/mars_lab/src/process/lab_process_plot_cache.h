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

/** @brief Build an owning exact cache key, including executable identity; NULL disables caching. */
string_t *lab_proc_plot_key(const char *const argv[], const char *cwd, const string_t *path, bool prepared);

/** @brief Clone a cached record; collisions and allocation failure are ordinary misses. */
string_t *lab_proc_plot_cache_get(const string_t *key, bool prepared);

/** @brief Store a bounded successful record; oversized records are not cached. */
void lab_proc_plot_cache_put(const string_t *key, const string_t *value, bool prepared);

/** @brief Remove and return an owning preparation record from successful worker output. */
string_t *lab_proc_plot_cache_extract(string_t **output);
#endif
