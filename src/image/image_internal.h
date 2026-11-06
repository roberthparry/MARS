/**
 * @file image_internal.h
 * @brief Private storage validation and PNG compression boundaries for the image module.
 *
 * Only image implementation files include this header. Applications use image-t.h; no browser, geographic
 * or application-specific interface is exposed here. Native and WASM compression preserve the same pixel format.
 */
#ifndef MARS_IMAGE_INTERNAL_H
#define MARS_IMAGE_INTERNAL_H
#if !defined(MARS_IMAGE_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_IMAGE_INTERNAL_ACCESS."
#endif

#include "image-t.h"

/** @brief Maximum owned image or encoded PNG allocation, independent of application budgets. */
enum { IMAGE_BYTE_LIMIT = 256 * 1024 * 1024 };

/**
 * @brief Validate dimensions and compute tightly packed storage without allocating pixels.
 * @param width Required positive pixel width, at most 32768.
 * @param height Required positive pixel height, at most 32768.
 * @param[out] bytes Required RGBA byte count output, unchanged on failure.
 * @return True for bounded dimensions; false for invalid input, overflow or excessive storage. No ownership transfers.
 */
bool image_storage_size(unsigned width, unsigned height, size_t *bytes);

/**
 * @brief Inflate exactly one bounded zlib stream into caller-owned bytes.
 * @param input Borrowed complete zlib stream, not retained.
 * @param size Accessible encoded byte count.
 * @param output Borrowed writable destination, unchanged only when failure precedes decoding.
 * @param expected Exact required decoded byte count.
 * @return True for a complete checksum-valid stream of exactly expected bytes; false for malformed input or failure.
 * @note Partial output on failure must be discarded. No allocation or ownership transfer occurs.
 */
bool image_inflate(const unsigned char *input, size_t size, unsigned char *output, size_t expected);

/**
 * @brief Produce one zlib stream from bounded caller-owned bytes.
 * @param input Borrowed readable bytes, not modified or retained.
 * @param size Input length, bounded by IMAGE_BYTE_LIMIT.
 * @return Owned encoded bytes released with array_destroy(), or NULL on invalid input or allocation failure.
 */
array_t *image_deflate(const unsigned char *input, size_t size);

#endif
