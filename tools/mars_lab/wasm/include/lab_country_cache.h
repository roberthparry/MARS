/**
 * @file lab_country_cache.h
 * @brief Native spatial-country cache for globe lookups.
 *
 * Retains copied country names, codes and optional nautical-zone suggestions, independently of flag image storage.
 * Keys distinguish named-town catalogue queries from arbitrary coordinates.
 * Each entry point dispatches synchronously through the service's private singleton. Browser handles
 * require the owning non-shared WASM instance; native dispatch serialises complete operations with a mutex.
 */
#ifndef LAB_COUNTRY_CACHE_H
#define LAB_COUNTRY_CACHE_H

#include <stdbool.h>

#include "lab_handle.h"

/** @brief Return a scoped name/code/nauticalZone record or null for an absent lookup key. */
void lab_country_cache_get(jsref key);

/** @brief Insert copied metadata and optional zone text (zero handle means absent), evicting beyond 64 entries or 1 MiB. */
bool lab_country_cache_put(jsref key, jsref name, jsref code, jsref nautical_zone);

/** @brief Release all retained metadata and indexes. */
void lab_country_cache_clear(void);

#endif
