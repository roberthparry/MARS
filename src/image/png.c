/**
 * @file png.c
 * @brief Bounded eight-bit PNG validation, RGBA decoding and lossless encoding.
 *
 * Implements PNG chunk checks, five byte filters and palette/transparency expansion in C. Pixel decoding uses
 * the module's portable DEFLATE core, shared by native and WASM builds. No image service or host callback is used.
 * Interlaced, sixteen-bit and sub-byte inputs are deliberately rejected rather than silently approximated.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifndef __wasm__
#include <zlib.h>
#endif

#define MARS_IMAGE_INTERNAL_ACCESS
#include "image_internal.h"

typedef struct {
    unsigned width, height, channels, colour;
    const unsigned char *palette, *transparency;
    size_t palette_size, transparency_size, compressed_size;
} png_info_t;

static uint32_t read32(const unsigned char *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static void write32(unsigned char *p, uint32_t value)
{
    p[0] = value >> 24; p[1] = value >> 16; p[2] = value >> 8; p[3] = value;
}

static uint32_t checksum(const unsigned char *p, size_t size)
{
#ifndef __wasm__
    return (uint32_t)crc32(0, p, (uInt)size);
#else
    static const uint32_t table[16] = {
        0,0x1db71064,0x3b6e20c8,0x26d930ac,0x76dc4190,0x6b6b51f4,0x4db26158,0x5005713c,
        0xedb88320,0xf00f9344,0xd6d6a3e8,0xcb61b38c,0x9b64c2b0,0x86d3d2d4,0xa00ae278,0xbdbdf21c
    };
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < size; ++i) {
        crc ^= p[i];
        crc = (crc >> 4) ^ table[crc & 15];
        crc = (crc >> 4) ^ table[crc & 15];
    }
    return ~crc;
#endif
}

/* Inspect bounded dimensions without allocating or trusting the remaining stream. */
bool image_png_header(const void *bytes, size_t size, unsigned *width, unsigned *height)
{
    static const unsigned char prefix[] = {137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82};
    const unsigned char *p = bytes;
    size_t storage;
    if (!p || size < 24 || !width || !height || memcmp(p, prefix, sizeof prefix) ||
        !image_storage_size(read32(p + 16), read32(p + 20), &storage)) return false;
    *width = read32(p + 16); *height = read32(p + 20);
    return true;
}

static bool inspect(const unsigned char *p, size_t size, png_info_t *info)
{
    if (size > IMAGE_BYTE_LIMIT || size < 57 || !image_png_header(p, size, &info->width, &info->height)) return false;
    bool header = false, data = false, ended = false;
    static const unsigned char channels[] = {1,0,3,1,2,0,4};
    for (size_t at = 8; at < size;) {
        if (size - at < 12) return false;
        size_t length = read32(p + at);
        if (length > size - at - 12 || length > IMAGE_BYTE_LIMIT) return false;
        const unsigned char *type = p + at + 4, *body = type + 4;
        if (checksum(type, length + 4) != read32(body + length)) return false;
        for (unsigned i = 0; i < 4; ++i)
            if (!((type[i] >= 'A' && type[i] <= 'Z') || (type[i] >= 'a' && type[i] <= 'z'))) return false;
        if (type[2] & 32) return false;
        if (!memcmp(type, "IHDR", 4)) {
            if (header || at != 8 || length != 13 || body[8] != 8 || body[9] > 6 ||
                !channels[body[9]] || body[10] || body[11] || body[12]) return false;
            info->colour = body[9]; info->channels = channels[body[9]]; header = true;
        } else if (!header) return false;
        else if (!memcmp(type, "PLTE", 4)) {
            if (data || info->palette || !length || length > 768 || length % 3 ||
                info->colour == 0 || info->colour == 4) return false;
            info->palette = body; info->palette_size = length / 3;
        } else if (!memcmp(type, "tRNS", 4)) {
            if (data || info->transparency || !length ||
                (info->colour == 0 && length != 2) || (info->colour == 2 && length != 6) ||
                (info->colour == 3 && (!info->palette || length > info->palette_size)) ||
                info->colour == 4 || info->colour == 6) return false;
            info->transparency = body; info->transparency_size = length;
        } else if (!memcmp(type, "IDAT", 4)) {
            if (ended || (info->colour == 3 && !info->palette) || length > IMAGE_BYTE_LIMIT - info->compressed_size)
                return false;
            info->compressed_size += length; data = true;
        } else if (!memcmp(type, "IEND", 4)) {
            return data && info->compressed_size >= 6 && !length && at + 12 == size;
        } else {
            if (!(type[0] & 32)) return false;
            if (data) ended = true;
        }
        at += length + 12;
    }
    return false;
}

/* Validate format, ordering, dimensions and all chunk checksums without decompressing the image. */
bool image_png_validate(const void *bytes, size_t size)
{
    png_info_t info = {0};
    return bytes && inspect(bytes, size, &info);
}

static unsigned char paeth(unsigned char left, unsigned char above, unsigned char corner)
{
    int estimate = (int)left + above - corner;
    int a = abs(estimate - left), b = abs(estimate - above), c = abs(estimate - corner);
    return a <= b && a <= c ? left : b <= c ? above : corner;
}

static bool unfilter(unsigned char *raw, const png_info_t *info)
{
    if (!info->width || info->width > 32768 || !info->height || info->height > 32768 ||
        !info->channels || info->channels > 4) return false;
    size_t stride = (size_t)info->width * info->channels;
    for (unsigned y = 0; y < info->height; ++y) {
        unsigned char *row = raw + (stride + 1) * y + 1;
        const unsigned char *previous = y ? row - stride - 1 : NULL;
        unsigned kind = row[-1];
        if (kind > 4) return false;
        for (size_t x = 0; x < stride; ++x) {
            unsigned char left = x >= info->channels ? row[x - info->channels] : 0;
            unsigned char above = previous ? previous[x] : 0;
            unsigned char corner = previous && x >= info->channels ? previous[x - info->channels] : 0;
            unsigned char prediction = kind == 1 ? left : kind == 2 ? above :
                                       kind == 3 ? (left + above) / 2 : kind == 4 ? paeth(left, above, corner) : 0;
            row[x] = (unsigned char)(row[x] + prediction);
        }
    }
    return true;
}

static bool expand(image_t *image, const unsigned char *raw, const png_info_t *info)
{
    unsigned char *output = image_pixels(image);
    size_t stride = (size_t)info->width * info->channels;
    for (unsigned y = 0; y < info->height; ++y) {
        const unsigned char *row = raw + (stride + 1) * y + 1;
        for (unsigned x = 0; x < info->width; ++x) {
            const unsigned char *p = row + x * info->channels;
            unsigned char *q = output + ((size_t)y * info->width + x) * 4;
            q[3] = 255;
            if (info->colour == 3) {
                unsigned index = *p;
                if (index >= info->palette_size) return false;
                memcpy(q, info->palette + index * 3, 3);
                if (index < info->transparency_size) q[3] = info->transparency[index];
            } else if (info->colour == 0 || info->colour == 4) {
                q[0] = q[1] = q[2] = *p;
                if (info->colour == 4) q[3] = p[1];
                else if (info->transparency && !info->transparency[0] && *p == info->transparency[1]) q[3] = 0;
            } else {
                memcpy(q, p, 3);
                if (info->colour == 6) q[3] = p[3];
                else if (info->transparency && !info->transparency[0] && !info->transparency[2] &&
                         !info->transparency[4] && p[0] == info->transparency[1] &&
                         p[1] == info->transparency[3] && p[2] == info->transparency[5]) q[3] = 0;
            }
        }
    }
    return true;
}

/* Decode bounded lossless pixels entirely within the C module. */
image_t *image_png_decode(const void *bytes, size_t size)
{
    png_info_t info = {0};
    if (!bytes || !inspect(bytes, size, &info)) return NULL;
    size_t stride = (size_t)info.width * info.channels, raw_size = (stride + 1) * info.height;
    if (raw_size > IMAGE_BYTE_LIMIT) return NULL;
    unsigned char *compressed = malloc(info.compressed_size), *raw = malloc(raw_size);
    const unsigned char *p = bytes;
    size_t written = 0;
    for (size_t at = 8; compressed && at < size;) {
        size_t length = read32(p + at);
        if (!memcmp(p + at + 4, "IDAT", 4)) {
            memcpy(compressed + written, p + at + 8, length); written += length;
        }
        at += length + 12;
    }
    bool ok = compressed && raw && image_inflate(compressed, written, raw, raw_size) && unfilter(raw, &info);
    image_t *image = ok ? image_new(info.width, info.height) : NULL;
    ok = image && expand(image, raw, &info);
    free(compressed); free(raw);
    if (!ok) { image_free(image); image = NULL; }
    return image;
}

static bool append_chunk(array_t *png, const char *type, const void *body, size_t length)
{
    if (length > IMAGE_BYTE_LIMIT) return false;
    unsigned char header[8], trailer[4];
    write32(header, (uint32_t)length); memcpy(header + 4, type, 4);
    size_t start = array_size(png);
    bool ok = array_append_carray(png, header, sizeof header) &&
              (!length || array_append_carray(png, body, length));
    if (ok) write32(trailer, checksum(array_get(png, start + 4), length + 4));
    return ok && array_append_carray(png, trailer, sizeof trailer);
}

/* Encode straight-alpha pixels losslessly, retaining the caller's image unchanged. */
array_t *image_png_encode(const image_t *image)
{
    if (!image) return NULL;
    unsigned width = image_width(image), height = image_height(image);
    size_t stride = (size_t)width * 4, raw_size = (stride + 1) * height;
    if (raw_size > IMAGE_BYTE_LIMIT) return NULL;
    unsigned char *raw = malloc(raw_size);
    if (!raw) return NULL;
    const unsigned char *pixels = image_data(image);
    for (unsigned y = 0; y < height; ++y) {
        size_t offset = y * (stride + 1);
        raw[offset] = 1;
        for (size_t x = 0; x < stride; ++x)
            raw[offset + x + 1] = pixels[y * stride + x] - (x >= 4 ? pixels[y * stride + x - 4] : 0);
    }
    array_t *compressed = image_deflate(raw, raw_size);
    free(raw);
    array_t *png = compressed ? array_create(1, NULL, NULL) : NULL;
    unsigned char header[13] = {0};
    write32(header, width); write32(header + 4, height); header[8] = 8; header[9] = 6;
    bool ok = png && array_append_carray(png, "\211PNG\r\n\032\n", 8) && append_chunk(png, "IHDR", header, 13) &&
              append_chunk(png, "IDAT", array_get(compressed, 0), array_size(compressed)) &&
              append_chunk(png, "IEND", NULL, 0);
    ok = ok && array_size(png) <= IMAGE_BYTE_LIMIT;
    array_destroy(compressed);
    if (!ok) { array_destroy(png); png = NULL; }
    return png;
}
