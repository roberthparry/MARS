/**
 * @file lab_flag_cache.h
 * @brief Browser application API for the native country-flag cache.
 *
 * Owns copied UTF-8 country metadata in dictionary_t. No browser handles survive
 * calls. The cache retains at most 32 entries and 1 MiB of text, evicting oldest
 * insertions first. JavaScript retains only asynchronous requests and DOM images.
 * Each entry point dispatches synchronously through the service's private singleton. Browser handles
 * require the owning non-shared WASM instance; native dispatch serialises complete operations with a mutex.
 */
#ifndef LAB_WASM_FLAG_CACHE_H
#define LAB_WASM_FLAG_CACHE_H

#include <stdbool.h>

#include "lab_handle.h"

/** @brief Return copied country metadata or null for an absent country code. */
void lab_flag_cache_get(jsref code);

/** @brief Copy validated metadata into the bounded dictionary; return false on invalid input or allocation failure. */
bool lab_flag_cache_put(jsref code, jsref name, jsref flag);

/** @brief Release all entries and the dictionary allocation. */
void lab_flag_cache_clear(void);

/** @brief Return entry count and retained UTF-8 text bytes for diagnostics and regression checks. */
void lab_flag_cache_stats(void);

#endif
