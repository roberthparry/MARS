/**
 * @file lab_texture_cache.h
 * @brief Native policy for browser-owned photographic resources.
 *
 * dictionary_t indexes copied cache keys; native slots, byte budgets and recency control eviction.
 * Browser adapters retain only ImageBitmap/WebGL handles in indexed slots. All mutable policy is serialised;
 * no scoped host reference survives a call. Geographic window selection and pixel premultiplication run in WASM.
 */
#ifndef LAB_TEXTURE_CACHE_H
#define LAB_TEXTURE_CACHE_H

#include "lab_handle.h"

/**
 * @brief Dispatch native create/get/put/cover/clear/stats operations numbered 0..5.
 * @param request Scoped browser object specifying an action, cache token and action-specific values. Insertions
 * may pin one slot or an array of at most four slots, and rank photographic quality from zero to two. Coverage
 * normally requires the entire view at an adequate scale; partial queries retain intersecting imagery for drawing.
 * A preferred slot stabilises drawing while equivalent, overlapping photographs are being prefetched.
 * @note The scoped host result describes the selected slot, bounds and evictions, or null on failure.
 * Resource ownership remains with the browser adapter. No request handle survives this serialised call.
 */
void lab_texture_cache(jsref request);

/** @brief Return stable padded geographic bounds for a four-number west/south/east/north window. */
void lab_globe_surface_bounds(jsref bounds);

/** @brief Premultiply up to 64 rows of RGBA bytes; return borrowed scratch storage valid until the next call. */
unsigned lab_texture_premultiply(jsref bytes);

#endif
