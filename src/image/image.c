/**
 * @file image.c
 * @brief Owned RGBA storage, alpha-aware area resizing and padded image blocks.
 *
 * Implements application-independent pixel operations shared unchanged by native and WASM builds. Every
 * allocation checks dimensions and overflow before touching pixels. Input images remain valid after failures.
 */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MARS_IMAGE_INTERNAL_ACCESS
#include "image_internal.h"

struct _image_t {
    unsigned width, height;
    size_t size;
    unsigned char *pixels;
};

bool image_storage_size(unsigned width, unsigned height, size_t *bytes)
{
    if (!bytes || !width || !height || width > 32768 || height > 32768 ||
        (uint64_t)width * height * 4 > IMAGE_BYTE_LIMIT) return false;
    *bytes = (size_t)width * height * 4;
    return true;
}

/* Allocate independently owned transparent pixels. */
image_t *image_new(unsigned width, unsigned height)
{
    size_t size;
    if (!image_storage_size(width, height, &size)) return NULL;
    image_t *image = calloc(1, sizeof *image);
    if (!image) return NULL;
    image->pixels = calloc(1, size);
    if (!image->pixels) { free(image); return NULL; }
    image->width = width; image->height = height; image->size = size;
    return image;
}

/* Copy strided caller pixels; padding and input ownership remain with the caller. */
image_t *image_from_rgba(unsigned width, unsigned height, const void *pixels, size_t stride)
{
    size_t bytes;
    if (!pixels || !image_storage_size(width, height, &bytes) || stride < (size_t)width * 4 ||
        stride > SIZE_MAX / height) return NULL;
    image_t *image = image_new(width, height);
    for (unsigned y = 0; image && y < height; ++y)
        memcpy(image->pixels + (size_t)y * width * 4, (const unsigned char *)pixels + y * stride, width * 4);
    return image;
}

/* Destroy one owned image; NULL is harmless. */
void image_free(image_t *image)
{
    if (image) { free(image->pixels); free(image); }
}

/* Read the immutable pixel width. */
unsigned image_width(const image_t *image) { return image ? image->width : 0; }

/* Read the immutable pixel height. */
unsigned image_height(const image_t *image) { return image ? image->height : 0; }

/* Read the tightly packed RGBA storage requirement. */
size_t image_size(const image_t *image) { return image ? image->size : 0; }

/* Borrow pixels for serial modification. */
unsigned char *image_pixels(image_t *image) { return image ? image->pixels : NULL; }

/* Borrow pixels without granting mutation. */
const unsigned char *image_data(const image_t *image) { return image ? image->pixels : NULL; }

/* Average coverage separately from premultiplied colour, preserving transparent boundaries. */
image_t *image_resize(const image_t *image, unsigned width, unsigned height)
{
    if (!image) return NULL;
    image_t *output = image_new(width, height);
    if (!output) return NULL;
    double rx = (double)image->width / width, ry = (double)image->height / height;
    for (unsigned y = 0; y < height; ++y) {
        double top = y * ry, bottom = fmin(image->height, (y + 1) * ry);
        for (unsigned x = 0; x < width; ++x) {
            double left = x * rx, right = fmin(image->width, (x + 1) * rx), sum[4] = {0};
            for (unsigned sy = (unsigned)top; sy < (unsigned)ceil(bottom); ++sy) {
                double wy = fmin(bottom, sy + 1) - fmax(top, sy);
                for (unsigned sx = (unsigned)left; sx < (unsigned)ceil(right); ++sx) {
                    const unsigned char *p = image->pixels + ((size_t)sy * image->width + sx) * 4;
                    double alpha = wy * (fmin(right, sx + 1) - fmax(left, sx)) * p[3];
                    for (unsigned c = 0; c < 3; ++c) sum[c] += alpha * p[c];
                    sum[3] += alpha;
                }
            }
            unsigned char *p = output->pixels + ((size_t)y * width + x) * 4;
            for (unsigned c = 0; c < 3; ++c) p[c] = sum[3] ? (unsigned char)lround(sum[c] / sum[3]) : 0;
            p[3] = (unsigned char)lround(sum[3] / (rx * ry));
        }
    }
    return output;
}

/* Extract one block with transparent padding outside the image and complete fixed-size gutters. */
image_t *image_block(const image_t *image, unsigned core, unsigned gutter, unsigned row, unsigned column)
{
    if (!image || !core || core > 32768 || gutter > (32768 - core) / 2 ||
        row >= (image->height + core - 1) / core || column >= (image->width + core - 1) / core) return NULL;
    unsigned edge = core + 2 * gutter;
    image_t *output = image_new(edge, edge);
    if (!output) return NULL;
    int64_t x = (int64_t)column * core - gutter, y = (int64_t)row * core - gutter;
    unsigned left = x > 0 ? (unsigned)x : 0, top = y > 0 ? (unsigned)y : 0;
    unsigned right = (unsigned)fmin(image->width, x + edge), bottom = (unsigned)fmin(image->height, y + edge);
    for (unsigned at = top; at < bottom; ++at)
        memcpy(output->pixels + ((size_t)(at - y) * edge + left - x) * 4,
               image->pixels + ((size_t)at * image->width + left) * 4, (right - left) * 4);
    return output;
}
