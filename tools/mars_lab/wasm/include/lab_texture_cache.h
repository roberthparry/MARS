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
 * @brief Dispatch native create/get/put/cover/clear/stats/photographic-page/remove/journey operations numbered 0..8.
 * @param request Scoped browser object specifying an action, cache token and action-specific values. Insertions
 * may pin one slot or an array of at most 64 slots, and rank photographic quality from zero to two. Coverage
 * normally requires the entire view at an adequate scale; partial queries retain intersecting imagery for drawing.
 * A preferred slot stabilises drawing while equivalent, overlapping photographs are being prefetched.
 * @note The scoped host result describes the selected slot, bounds and evictions, or null on failure.
 * Resource ownership remains with the browser adapter. No request handle survives this serialised call.
 * Photographic pages select fixed 512-pixel geographic cells, retain the finest cached ancestor per cell and
 * return the next missing cell in centre-first order. Pages retain two finer resolution tiers when zooming out.
 * Page requests may supply protected slots from the visible page while selecting an arrival page; the returned
 * protection combines both sets. Their 16-by-16 RGBA page bytes occupy borrowed native memory
 * at the returned pointer, valid until the next page request or cache disposal. Copy or upload them synchronously.
 * Caches accept 1..64 resource slots within a byte budget of at most 64 MiB. Allocation or invalid input returns null.
 * A page first fills broad coverage two resolution levels below its requested detail, bounded to source cells
 * of at most 22.5 degrees, then refines it. The journey action accepts arrival bounds and pixels, predicted
 * ahead bounds and whether the worker is idle. It prioritises missing visible, predicted and arrival coverage,
 * then alternates refinement fairly. Its pace factor in 0.3..1 follows the actual visible pixel resolution,
 * increasing smoothly towards full pace as requested detail arrives, without blocking drawing.
 * Journey planning preserves the displayed page bytes and pins visible and arrival resources together.
 * Page and journey requests may describe a current image job by bounds and level. The replace flag is true
 * only when that job cannot provide adequate coverage for any current visible, predicted or arrival window;
 * the browser may then abort its transport and submit the selected replacement. No browser job is retained.
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
