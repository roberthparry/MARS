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
 * @brief Dispatch native resource-cache, geographic-page, journey and source-plan operations numbered 0..9.
 * @param request Scoped browser object specifying an action, cache token and action-specific values. Insertions
 * may pin one slot or an array of at most 64 slots, and rank photographic quality from zero to two. Coverage
 * normally requires the entire view at an adequate scale; partial queries retain intersecting imagery for drawing.
 * A preferred slot stabilises drawing while equivalent, overlapping photographs are being prefetched.
 * @note The scoped host result describes the selected slot, bounds and evictions, or null on failure.
 * Resource ownership remains with the browser adapter. No request handle survives this serialised call.
 * Photographic pages select fixed 512-pixel geographic cells, retain the finest cached ancestor per cell and
 * return the next missing cell in centre-first order. Sparse child nodes retain all finer resident resolution
 * tiers when zooming out, without downloading or copying their photographs again.
 * Page requests may supply protected slots from the visible page while selecting an arrival page; the returned
 * protection combines both sets. Their 64-by-64 RGBA lookup bytes occupy borrowed native memory at the returned
 * pointer, valid until the next page request or cache disposal. Copy or upload them synchronously. The first
 * 256 entries form a 16-by-16 geographic root, with bounded four-child refinement nodes thereafter.
 * Root resolution always covers the entire submitted viewport with an alignment margin, independently of
 * requested photograph detail. Coverage is measured against the full viewport area, including all edges.
 * Coverage and detail are measured at the final sparse leaves, so finer retained descendants count after a
 * zoom-out. Fully detailed visible cells do not trigger coarser replacements. Every displayed sparse leaf is
 * protected before neighbour selection begins. Offscreen padding and arrival preparation cannot evict the
 * only displayed copy of a geographic region. If all slots are protected, a completed broader replacement
 * may reuse a finer slot only when it encloses that entire cell; partial uploads cannot make that substitution.
 * Jobs with no safe admission slot wait for camera movement or eviction of an offscreen entry rather than
 * repeatedly removing and rebuilding displayed imagery.
 * The revision changes only when these lookup bytes change, allowing camera redraws to reuse the GPU table.
 * Caches accept 1..64 resource slots within a byte budget of at most 64 MiB. Allocation or invalid input returns null.
 * A stationary page rounds its 510-pixel geographic core up to meet display pixel demand within the level cap,
 * and requests that detail immediately, centre first. Caches with fewer than 32 slots omit offscreen prefetch.
 * Larger caches also omit padding whenever its grid would exceed the resource-slot budget, so visible finer
 * cells fit without repeated eviction. The resident world image supplies
 * its background without waiting for additional coarse photographs.
 * Stationary page requests may supply a native-predicted ahead view during dragging. Once visible detail is
 * complete, that view takes priority over offscreen neighbourhood padding; visible gaps always load first.
 * The journey action first fills broad coverage up to two levels below requested detail, using cells no larger
 * than 5.625 degrees when that scale
 * is requested. Broader journeys use their requested level directly. The action accepts arrival bounds and pixels, predicted
 * ahead bounds and whether the worker is idle. It prioritises missing visible, predicted and arrival coverage,
 * then alternates refinement fairly. Its pace factor in 0.3..1 follows the actual visible pixel resolution,
 * increasing smoothly towards full pace as requested detail arrives, without blocking drawing.
 * Journey planning preserves the displayed page bytes and pins visible and arrival resources together.
 * Page and journey requests may describe a current image job by bounds and level. The replace flag is true
 * only when that job cannot provide adequate coverage for any current visible, predicted or arrival window;
 * the browser may then abort its transport and submit the selected replacement. No browser job is retained.
 * Geographic photo keys may enter with quality one while their source crops are still arriving. Such pixels
 * can draw, but do not count towards completed coverage, suppress further requests or displace a complete
 * ancestor. Quality two promotes the same slot to complete; ordinary quality-zero entries remain complete
 * for existing callers. Partial descendants remain drawable and protected through zoom-out where no complete
 * ancestor is available, without contributing to completed coverage. Cancelling transport need not delete
 * pixels already admitted to the cache.
 * Source-plan action nine accepts photo metadata, an optional buffered-cache token and up to 512 stored block
 * addresses read from the persistent cache's key index. It supplies native
 * dictionary lookups to the source planner, reusing a sufficiently detailed image or complete block set across
 * resolution changes. Queued raw blocks may satisfy those lookups; no image handles or pixel payloads enter C.
 * Stored addresses are copied into a temporary native dictionary and released before the scoped call returns.
 * Actual persisted image bytes are still validated by the storage adapter when read.
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

/**
 * @brief Report whether every alpha value in the last prepared pixel batch was fully opaque.
 * @return True only for a non-empty valid batch whose alpha bytes all equal 255; false otherwise.
 * @note Borrowed service state is valid until the next premultiplication call in this WASM instance.
 * Call immediately after lab_texture_premultiply on the same serial browser thread. No pixels are retained
 * beyond the existing scratch-buffer lifetime, and no resource is returned or requires release.
 */
bool lab_texture_opaque(void);

#endif
