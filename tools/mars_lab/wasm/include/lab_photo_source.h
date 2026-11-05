/**
 * @file lab_photo_source.h
 * @brief Native pixel-grid planning for bounded photographic source reuse.
 *
 * Browser workers perform codec and storage operations. This interface owns the exact source crop, geographic
 * destination mapping, stable decoded resolution tiers and 512-pixel source-cell keys without retaining browser handles.
 */
#ifndef LAB_PHOTO_SOURCE_H
#define LAB_PHOTO_SOURCE_H

#include <stdbool.h>

#include "lab_handle.h"

/**
 * @brief Test whether a content-addressed photograph or source block is already resident.
 * @param key Borrowed NUL-terminated key, valid only during this callback; do not retain or free it.
 * @param context Borrowed caller-owned cache state, valid throughout the synchronous planning call.
 * @return True when pixels with this exact digest, resolution and block address are reusable; false on a miss.
 * @note Do not re-enter the host bridge. The callback only queries native cache dictionaries and transfers no ownership.
 */
typedef bool (*lab_photo_source_cached_fn)(const char *key, void *context);

/**
 * @brief Plan a source crop, reusing an adequate cached resolution before requesting another PNG decode.
 * @param photo Borrowed scoped source metadata with URL, digest, installed width and crop rectangles.
 * @param cached Optional synchronous cache lookup; NULL selects the normal minimum adequate resolution.
 * @param context Borrowed state passed unchanged to cached; ignored when cached is NULL.
 * @return Independently copied scoped plan with a tier-independent prefix and a cached-provider-hit flag,
 * or zero for invalid metadata. The result belongs to the active host
 * call and requires no explicit release; callers must not retain its handle beyond that call.
 * @note Higher-resolution reuse requires the whole photograph or every intersecting source block. Lower-resolution
 * data never satisfies finer demand. At most seven fixed resolution tiers and 64 addressed blocks are tested per tier.
 */
jsref lab_photo_source_plan(jsref photo, lab_photo_source_cached_fn cached, void *context);

/**
 * @brief Describe fixed source cells, their one-pixel gutters and their destination rectangles.
 * @param photo Scoped source metadata containing url, sha256, width, source and target rectangles.
 * @return No C value; the scoped host result describes a decoded width, photograph key, content prefix,
 * retain flag, false cached flag, decoded source rectangle
 * and an array named parts with at most 64 cell records, or null for invalid, non-finite or out-of-bounds metadata.
 * @note No ownership transfers. Records contain copied keys and numbers and survive independently of this call.
 * The source image is square and either 1800 or 3600 pixels wide. Each cached cell is a 514-square RGBA image;
 * its 512-square core is fixed in decoded source coordinates. A power-of-two tier of at least 64 pixels,
 * capped at the installed width, supplies at least one source pixel per destination pixel. A cache-aware plan
 * may reuse a sharper tier; the standalone entry point always selects the minimum adequate tier. Keys include the
 * digest and tier so low-resolution blocks cannot satisfy finer requests. Browser resources remain owned
 * by the worker; decoded photograph and block handles share its native-accounted byte budget. Only intersecting
 * rows and columns are visited and returned; unused source cells incur no host-object or key-construction work.
 * The retain flag permits whole photographs up to 2048 square (16 MiB). Larger images contribute only their
 * needed reusable blocks, preventing one 3600-square coastal overview from evicting most of the 64 MiB cache.
 * The same flag bounds optional read-ahead to one additional photograph of at most 16 MiB; admission and
 * geometric selection remain native, while the worker owns asynchronous browser codec handles.
 */
void lab_photo_source(jsref photo);

/**
 * @brief Verify a source PNG's signature, IHDR marker and installed dimensions before a reduced browser decode.
 * @param bytes Borrowed scoped byte array containing exactly the first 24 encoded bytes.
 * @param width Expected square source width, either 1800 or 3600 pixels.
 * @return True for the expected header; false for invalid bytes, signature, dimensions or unsupported width.
 * @note Does not allocate or retain input. The browser decoder still validates the remaining PNG stream;
 * this dimension check avoids allocating a full-sized ImageBitmap solely to inspect its width and height.
 */
bool lab_photo_source_header(jsref bytes, unsigned width);

/**
 * @brief Replace the exact alpha coverage of sequential rows in one 512-square photographic cell.
 * @param bytes Borrowed scoped RGBA byte array containing one to 64 complete 512-pixel rows.
 * An empty array with row zero resets coverage without supplying pixels.
 * @param row First destination row; zero starts a new coverage pass. Subsequent batches must be contiguous.
 * @return No value. Invalid, oversized or out-of-order input invalidates the current coverage pass.
 * @note Copies only alpha coverage into a fixed 32 KiB bitset; no image or host handle is retained. Only alpha 255
 * counts as opaque. Serial worker calls must complete all 512 rows before querying covered targets. No release is needed.
 */
void lab_photo_coverage_rows(jsref bytes, unsigned row);

/**
 * @brief Test whether every pixel touched by a source's destination rectangle is already opaque.
 * @param target Borrowed scoped four-number array containing x, y, width and height within the 512-square cell.
 * @return True only for complete recorded coverage of the entire rectangle; false for incomplete coverage,
 * invalid coordinates, or any transparent or partly transparent pixel. Fractional edges are checked conservatively.
 * @note Uses the latest complete lab_photo_coverage_rows pass on the same serial worker. No ownership transfers,
 * allocations or host handles survive the call; there is nothing to release.
 */
bool lab_photo_target_covered(jsref target);

/**
 * @brief Test whether two destination rectangles overlap before starting speculative photographic reads.
 * @param first Borrowed scoped x, y, width, height array describing the current foreground source.
 * @param second Borrowed scoped destination rectangle of a possible following source.
 * @return True for positive-area overlap or invalid rectangles; false for valid disjoint or merely touching edges.
 * @note Stateless and synchronous; no ownership transfers or release is required. An overlapping source should
 * be considered after the foreground's real alpha coverage is available, rather than fetched speculatively.
 */
bool lab_photo_targets_overlap(jsref first, jsref second);

#endif
