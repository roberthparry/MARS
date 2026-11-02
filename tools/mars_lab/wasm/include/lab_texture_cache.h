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
 * @brief Dispatch native create/get/put/cover/clear/stats/photographic-page/remove operations numbered 0..7.
 * @param request Scoped browser object specifying an action, cache token and action-specific values. Insertions
 * may pin one slot or an array of at most 64 slots, and rank photographic quality from zero to two. Coverage
 * normally requires the entire view at an adequate scale; partial queries retain intersecting imagery for drawing.
 * A preferred slot stabilises drawing while equivalent, overlapping photographs are being prefetched.
 * @note The scoped host result describes the selected slot, bounds and evictions, or null on failure.
 * Resource ownership remains with the browser adapter. No request handle survives this serialised call.
 * Photographic pages select fixed 512-pixel geographic cells, retain the finest cached ancestor per cell and
 * return the next missing cell in centre-first order. Their 8-by-8 RGBA page bytes occupy borrowed native memory
 * at the returned pointer, valid until the next page request or cache disposal. Copy or upload them synchronously.
 * Caches accept 1..64 resource slots within a byte budget of at most 64 MiB. Allocation or invalid input returns null.
 */
void lab_texture_cache(jsref request);

/**
 * @brief Return stable padded bounds for a regional mask window.
 * @param bounds Scoped four-number west/south/east/north array, borrowed for this call only.
 * @return No C value; the host result contains independently copied bounds, or null for invalid input.
 * @note Quantisation changes the presentation window only. No geometry or geographic data is modified or retained.
 */
void lab_globe_surface_bounds(jsref bounds);

/**
 * @brief Prepare straight-alpha RGBA bytes for direct premultiplied WebGL upload.
 * @param bytes Scoped byte array with at most 512 KiB and a length divisible by four, borrowed during the call.
 * @return Address of borrowed native scratch storage, or zero for invalid input or serialisation failure.
 * @note Copy the result before calling this function again; callers must not free the scratch buffer.
 * The operation is serialised within the texture service and never retains the host array.
 */
unsigned lab_texture_premultiply(jsref bytes);

#endif
