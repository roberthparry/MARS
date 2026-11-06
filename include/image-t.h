/**
 * @file image-t.h
 * @brief Owned RGBA images, lossless PNGs, alpha coverage and multiresolution block planning.
 *
 * Provides ordinary C image operations for native and WebAssembly callers. Images own tightly packed,
 * straight-alpha, eight-bit RGBA pixels. Area resizing weights colour by alpha to preserve transparent edges.
 * Pyramids describe rectangular images, resolution tiers and padded blocks without loading pixels or retaining
 * application objects. Geographic coordinates, URLs, browser handles, GPU resources and scheduling belong to callers.
 * PNG support accepts non-interlaced eight-bit images; unsupported formats and malformed streams fail explicitly.
 * All allocations and decoded sizes are bounded. Instances have one serial owner; independent instances may be
 * used concurrently. No operation starts a thread or calls JavaScript. Release owned objects with their matching
 * free function and encoded byte arrays with array_destroy().
 */
#ifndef MARS_IMAGE_H
#define MARS_IMAGE_H

#include <stdbool.h>
#include <stddef.h>

#include "array-t.h"

/** @brief Opaque owned straight-alpha RGBA image. */
typedef struct _image_t image_t;

/** @brief Opaque immutable image-pyramid geometry, independent of pixel storage. */
typedef struct _image_pyramid_t image_pyramid_t;

/** @brief Opaque sequential alpha-coverage accumulator. */
typedef struct _image_coverage_t image_coverage_t;

/** @brief Pixel rectangle; fractional coordinates are permitted for crop mappings. */
typedef struct _image_rectangle_t {
    double x; /**< Left pixel coordinate. */
    double y; /**< Top pixel coordinate. */
    double width; /**< Positive horizontal extent. */
    double height; /**< Positive vertical extent. */
} image_rectangle_t;

/** @brief One pyramid level; edge identifies its longest side in pixels. */
typedef struct _image_level_t {
    unsigned edge; /**< Stable tier identifier, including the original longest side. */
    unsigned width; /**< Decoded width at this tier. */
    unsigned height; /**< Decoded height at this tier. */
    unsigned columns; /**< Number of core block columns, including a partial final core. */
    unsigned rows; /**< Number of core block rows, including a partial final core. */
} image_level_t;

/** @brief Exact source-block address and mappings for a requested crop. */
typedef struct _image_block_t {
    unsigned tier; /**< Longest-side level identifier. */
    unsigned row; /**< Zero-based block row. */
    unsigned column; /**< Zero-based block column. */
    image_rectangle_t read; /**< Available source pixels, including clipped gutters, in level coordinates. */
    image_rectangle_t write; /**< Destination of read pixels within the padded block. */
    image_rectangle_t source; /**< Requested core intersection, in padded-block coordinates. */
    image_rectangle_t target; /**< Corresponding rectangle in the caller's destination coordinates. */
} image_block_t;

/**
 * @brief Allocate a transparent image with tightly packed RGBA pixels.
 * @param width Pixel width in [1,32768].
 * @param height Pixel height in [1,32768]; total pixel storage must not exceed 256 MiB.
 * @return Owned image, released with image_free(), or NULL for invalid dimensions or allocation failure.
 */
image_t *image_new(unsigned width, unsigned height);

/**
 * @brief Copy caller-owned RGBA rows into an independently owned image.
 * @param width Required pixel width.
 * @param height Required pixel height.
 * @param pixels Borrowed readable pixels, not retained; at least height rows must be accessible.
 * @param stride Input row spacing in bytes, at least width*4; padding is not copied.
 * @return Owned image released with image_free(), or NULL for invalid dimensions, stride or allocation failure.
 */
image_t *image_from_rgba(unsigned width, unsigned height, const void *pixels, size_t stride);

/** @brief Release an owned image and its pixels; @param image Owned image, or NULL. @return No value. */
void image_free(image_t *image);

/** @brief Read width. @param image Borrowed image, or NULL. @return Width in pixels, or zero for NULL. */
unsigned image_width(const image_t *image);

/** @brief Read height. @param image Borrowed image, or NULL. @return Height in pixels, or zero for NULL. */
unsigned image_height(const image_t *image);

/** @brief Read storage size. @param image Borrowed image, or NULL. @return RGBA byte count, or zero for NULL. */
size_t image_size(const image_t *image);

/**
 * @brief Access owned pixels for serial modification.
 * @param image Borrowed image, or NULL.
 * @return Borrowed mutable RGBA bytes, or NULL. Valid until image_free(); never free the returned pointer.
 */
unsigned char *image_pixels(image_t *image);

/**
 * @brief Access owned pixels without permitting modification.
 * @param image Borrowed image, or NULL.
 * @return Borrowed RGBA bytes, or NULL. Valid until image_free(); never free the returned pointer.
 */
const unsigned char *image_data(const image_t *image);

/**
 * @brief Area-resample an image with alpha-weighted colour and separately averaged coverage.
 * @param image Borrowed source, unchanged on every outcome.
 * @param width Required output width, subject to image_new() limits.
 * @param height Required output height, subject to image_new() limits.
 * @return Independently owned image released with image_free(), or NULL for invalid input or allocation failure.
 */
image_t *image_resize(const image_t *image, unsigned width, unsigned height);

/**
 * @brief Copy a fixed-size core and gutters, padding pixels outside the source with transparent black.
 * @param image Borrowed source, not modified or retained.
 * @param core Core edge in pixels, from 1 to 32768.
 * @param gutter Extra pixels on each side; padded dimensions remain subject to image_new() limits.
 * @param row Zero-based core row intersecting the source.
 * @param column Zero-based core column intersecting the source.
 * @return Owned padded image released with image_free(), or NULL for invalid input or allocation failure.
 */
image_t *image_block(const image_t *image, unsigned core, unsigned gutter, unsigned row, unsigned column);

/**
 * @brief Inspect the first 24 bytes of a PNG without decoding pixels.
 * @param bytes Borrowed encoded bytes, not retained.
 * @param size Accessible byte count; at least 24 are required.
 * @param[out] width Required output, written only on success.
 * @param[out] height Required output, written only on success.
 * @return True for a recognised signature, IHDR marker and bounded dimensions; false otherwise.
 * @note This preliminary check does not establish whole-stream validity; use image_png_validate() or decoding.
 */
bool image_png_header(const void *bytes, size_t size, unsigned *width, unsigned *height);

/**
 * @brief Validate PNG structure, supported format and every chunk CRC without decompressing pixels.
 * @param bytes Borrowed complete encoded PNG, not retained.
 * @param size Encoded size, at most 256 MiB.
 * @return True for a complete supported stream; false for malformed, unsupported or oversized input.
 * @note Decompression validity is additionally checked by image_png_decode().
 */
bool image_png_validate(const void *bytes, size_t size);

/**
 * @brief Decode a complete non-interlaced eight-bit PNG into straight-alpha RGBA.
 * @param bytes Borrowed encoded PNG, not modified or retained.
 * @param size Accessible encoded byte count, at most 256 MiB.
 * @return Owned image released with image_free(), or NULL for unsupported format, corruption or allocation failure.
 * @note Greyscale, RGB, indexed, greyscale-alpha and RGBA inputs are supported. Native and WASM builds validate
 * checksums and decoded bounds; no host image decoder or JavaScript processing is invoked.
 */
image_t *image_png_decode(const void *bytes, size_t size);

/**
 * @brief Encode an image losslessly as an eight-bit RGBA PNG.
 * @param image Borrowed source, not modified or retained.
 * @return Owned byte array released with array_destroy(), or NULL for invalid input or allocation failure.
 * @note Native builds use zlib compression; the dependency-free WASM encoder uses valid stored DEFLATE blocks.
 * Both preserve identical RGBA pixels. Encoding never writes a file or persists a cache entry.
 */
array_t *image_png_encode(const image_t *image);

/**
 * @brief Describe an image's original size and successively smaller power-of-two resolution tiers.
 * @param width Original pixel width, from 1 to 32768.
 * @param height Original pixel height, from 1 to 32768.
 * @param core Positive block-core edge, at most 32768.
 * @param gutter Padding on each core side; core+2*gutter must not exceed 32768.
 * @param minimum Smallest longest-side tier, from 1 to max(width,height).
 * @return Owned immutable geometry released with image_pyramid_free(), or NULL for invalid input or allocation failure.
 */
image_pyramid_t *image_pyramid_new(unsigned width, unsigned height, unsigned core, unsigned gutter, unsigned minimum);

/** @brief Release pyramid geometry. @param pyramid Owned geometry, or NULL. @return No value. */
void image_pyramid_free(image_pyramid_t *pyramid);

/** @brief Count levels. @param pyramid Borrowed geometry, or NULL. @return Level count, or zero for NULL. */
unsigned image_pyramid_levels(const image_pyramid_t *pyramid);

/**
 * @brief Read a level by direct index, from original detail towards the smallest overview.
 * @param pyramid Borrowed immutable geometry.
 * @param index Zero-based level index, smaller than image_pyramid_levels().
 * @param[out] level Required output, unchanged on failure.
 * @return True on success; false for invalid input.
 */
bool image_pyramid_level(const image_pyramid_t *pyramid, unsigned index, image_level_t *level);

/**
 * @brief Map a source rectangle into one installed resolution tier.
 * @param pyramid Borrowed immutable geometry.
 * @param tier Installed longest-side resolution identifier.
 * @param source Borrowed crop in original image coordinates.
 * @param[out] decoded Required output in decoded level coordinates, unchanged on failure.
 * @return True for valid geometry and crop, false otherwise. No allocation or ownership transfer occurs.
 */
bool image_pyramid_crop(const image_pyramid_t *pyramid, unsigned tier, const image_rectangle_t *source,
                        image_rectangle_t *decoded);

/**
 * @brief Visit an intersecting source block.
 * @param block Borrowed immutable address and mappings, valid only during the callback; do not retain or free it.
 * @param context Caller-owned callback state, not retained.
 * @return True to continue, false to stop and make image_pyramid_visit() return false.
 */
typedef bool (*image_block_visit_fn)(const image_block_t *block, void *context);

/**
 * @brief Probe reusable pixels without transferring ownership.
 * @param tier Longest-side resolution identifier.
 * @param block Borrowed block address, or NULL to query a complete resident level; valid only during this call.
 * @param context Caller-owned cache state, not retained; do not re-enter the same planning call.
 * @return True when that exact level or block is available, false otherwise.
 */
typedef bool (*image_pyramid_cached_fn)(unsigned tier, const image_block_t *block, void *context);

/**
 * @brief Select adequate resolution and optionally reuse a sharper complete cached crop.
 * @param pyramid Borrowed immutable geometry.
 * @param source Borrowed positive crop within original image coordinates.
 * @param target Borrowed positive destination rectangle; its origin may be outside the image.
 * @param cached Optional synchronous cache probe; NULL chooses the smallest adequate level.
 * @param context Caller-owned probe state, not retained.
 * @param[out] level Required selected level output, unchanged on failure.
 * @param[out] reused Optional output, true only when cached pixels cover the crop at the selected level.
 * @return True for a valid selection; false for invalid rectangles, geometry or outputs.
 * @note At most sixteen tiers are considered. Cache probes enumerate only intersecting cores and stop on a miss.
 */
bool image_pyramid_select(const image_pyramid_t *pyramid, const image_rectangle_t *source,
                          const image_rectangle_t *target, image_pyramid_cached_fn cached, void *context,
                          image_level_t *level, bool *reused);

/**
 * @brief Enumerate only blocks intersecting a crop, with exact gutter and destination mappings.
 * @param pyramid Borrowed immutable geometry.
 * @param tier Installed longest-side resolution identifier.
 * @param source Borrowed crop in original image coordinates.
 * @param target Borrowed destination rectangle.
 * @param visitor Required synchronous borrowed-block visitor; false aborts enumeration.
 * @param context Caller-owned visitor state, not retained.
 * @return True after complete enumeration; false for invalid input or visitor cancellation.
 */
bool image_pyramid_visit(const image_pyramid_t *pyramid, unsigned tier, const image_rectangle_t *source,
                         const image_rectangle_t *target, image_block_visit_fn visitor, void *context);

/**
 * @brief Allocate a sequential opaque-pixel coverage bitset.
 * @param width Pixel width in [1,32768].
 * @param height Pixel height in [1,32768]; storage must fit the image allocation limit.
 * @return Owned accumulator released with image_coverage_free(), or NULL for invalid input or allocation failure.
 */
image_coverage_t *image_coverage_new(unsigned width, unsigned height);

/** @brief Release coverage storage. @param coverage Owned accumulator, or NULL. @return No value. */
void image_coverage_free(image_coverage_t *coverage);

/**
 * @brief Record contiguous RGBA rows; row zero starts a fresh coverage pass.
 * @param coverage Borrowed mutable accumulator, serially owned.
 * @param row First destination row, zero initially and contiguous thereafter.
 * @param pixels Borrowed readable RGBA rows, not retained; NULL with row=rows=0 resets coverage.
 * @param rows Number of accessible input rows; zero is accepted only for an explicit reset.
 * @param stride Accessible input row spacing in bytes, at least width*4.
 * @return True for an accepted batch/reset; false invalidates coverage until a fresh complete pass is supplied.
 */
bool image_coverage_write(image_coverage_t *coverage, unsigned row, const void *pixels, unsigned rows, size_t stride);

/**
 * @brief Test exact opaque coverage of every pixel touched by a rectangle.
 * @param coverage Borrowed accumulator with a complete row pass.
 * @param rectangle Borrowed positive pixel rectangle within the accumulator.
 * @return True only when all touched pixels have alpha 255; false for incomplete coverage or invalid input.
 * @note Fractional edges are checked conservatively. No allocation, mutation or ownership transfer occurs.
 */
bool image_coverage_contains(const image_coverage_t *coverage, const image_rectangle_t *rectangle);

/**
 * @brief Intersect two finite positive rectangles.
 * @param first Borrowed rectangle.
 * @param second Borrowed rectangle.
 * @param[out] intersection Required output, unchanged for invalid or disjoint rectangles.
 * @return True for positive-area overlap; false for invalid, disjoint or merely touching rectangles.
 */
bool image_rectangle_intersection(const image_rectangle_t *first, const image_rectangle_t *second,
                                   image_rectangle_t *intersection);

#endif
