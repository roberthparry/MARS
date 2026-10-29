/**
 * @file texture_tiles.c
 * @brief Offline preparation of local photographic surface tiles from NASA reference imagery.
 *
 * Splits the 21600 by 10800 RGB JPEG into a twelve by six grid without resampling.
 * Only one 1800-row strip is decoded at a time. Optional global textures use
 * area-weighted resampling with one decoded source row and one output image.
 * This optional maintainer utility
 * uses libjpeg and zlib; the application and ordinary builds do not require its development files.
 * File access uses the MARS file module. Source credit and provenance belong in
 * THIRD_PARTY_NOTICES.md alongside the generated assets.
 * The --manifest mode records the packaged images' dimensions, sizes, source attribution and SHA-256 digests
 * for transactional installation into the globe database, preserving an existing regional catalogue.
 * It does not re-encode any image.
 */
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include <zlib.h>

#include "array-t.h"
#include "globe-t.h"
#include "json-t.h"
#include "string-t.h"
#include "file-t.h"

static void texture_u32(unsigned char *out, unsigned value)
{
    out[0] = value >> 24; out[1] = value >> 16; out[2] = value >> 8; out[3] = value;
}

static bool texture_chunk(array_t *output, const char *type, const unsigned char *data, size_t size)
{
    unsigned char prefix[8], checksum[4];
    texture_u32(prefix, (unsigned)size);
    memcpy(prefix + 4, type, 4);
    uLong crc = crc32(0, (const Bytef *)type, 4);
    if (size) crc = crc32(crc, data, (uInt)size);
    texture_u32(checksum, (unsigned)crc);
    return array_append_carray(output, prefix, sizeof prefix) &&
           (!size || array_append_carray(output, data, size)) &&
           array_append_carray(output, checksum, sizeof checksum);
}

static bool save_image(const string_t *path, unsigned width, unsigned height, size_t stride, unsigned char *pixels)
{
    size_t row = (size_t)width * 3 + 1, size = row * height;
    unsigned char *raw = malloc(size);
    uLongf length = compressBound(size);
    unsigned char *compressed = malloc(length);
    if (!raw || !compressed) { free(raw); free(compressed); return false; }
    for (unsigned y = 0; y < height; ++y) {
        raw[y * row] = 0;
        memcpy(raw + y * row + 1, pixels + y * stride, row - 1);
    }
    bool ok = compress2(compressed, &length, raw, size, 6) == Z_OK;
    free(raw);
    array_t *encoded = ok ? array_create(1, NULL, NULL) : NULL;
    unsigned char header[13] = {0};
    texture_u32(header, width); texture_u32(header + 4, height);
    header[8] = 8; header[9] = 2;
    ok = encoded && array_append_carray(encoded, "\211PNG\r\n\032\n", 8) &&
         texture_chunk(encoded, "IHDR", header, sizeof header) &&
         texture_chunk(encoded, "IDAT", compressed, length) && texture_chunk(encoded, "IEND", NULL, 0);
    free(compressed);
    file_t *file = ok && path ? file_new(path) : NULL;
    ok = file && file_write_all_bytes(file, array_get(encoded, 0), array_size(encoded));
    file_free(file); array_destroy(encoded);
    return ok;
}

static bool save_tile(const char *directory, unsigned column, unsigned row, unsigned char *strip)
{
    string_t *path = string_sprintf("%s/%02u-%02u.png", directory, column, row);
    bool ok = path && save_image(path, 1800, 1800, 21600 * 3, strip + column * 1800 * 3);
    string_free(path);
    return ok;
}

static bool save_overview(array_t *bytes, const char *directory, unsigned width)
{
    struct jpeg_decompress_struct decoder;
    struct jpeg_error_mgr error;
    decoder.err = jpeg_std_error(&error);
    jpeg_create_decompress(&decoder);
    jpeg_mem_src(&decoder, array_get(bytes, 0), array_size(bytes));
    jpeg_read_header(&decoder, true);
    decoder.out_color_space = JCS_RGB;
    jpeg_start_decompress(&decoder);
    unsigned height = width / 2;
    unsigned char *pixels = malloc((size_t)width * height * 3), *source = malloc(21600 * 3);
    double *row = malloc(width * 3 * sizeof *row), *sum = malloc(width * 3 * sizeof *sum);
    bool ok = pixels && source && row && sum;
    double ratio = 21600. / width;
    int decoded = -1;
    for (unsigned y = 0; ok && y < height; ++y) {
        memset(sum, 0, width * 3 * sizeof *sum);
        double top = y * ratio, bottom = (y + 1) * ratio;
        for (int sy = (int)floor(top); sy < (int)ceil(bottom); ++sy) {
            if (decoded != sy) {
                JSAMPROW input = source;
                if (jpeg_read_scanlines(&decoder, &input, 1) != 1) { ok = false; break; }
                decoded = sy;
                for (unsigned x = 0; x < width; ++x) {
                    double left = x * ratio, right = (x + 1) * ratio, channels[3] = {0};
                    for (int sx = (int)floor(left); sx < (int)ceil(right); ++sx) {
                        double weight = fmin(right, sx + 1) - fmax(left, sx);
                        for (unsigned c = 0; c < 3; ++c) channels[c] += source[sx * 3 + c] * weight;
                    }
                    for (unsigned c = 0; c < 3; ++c) row[x * 3 + c] = channels[c] / ratio;
                }
            }
            double weight = fmin(bottom, sy + 1) - fmax(top, sy);
            for (unsigned x = 0; x < width * 3; ++x) sum[x] += row[x] * weight;
        }
        for (unsigned x = 0; ok && x < width * 3; ++x)
            pixels[(size_t)y * width * 3 + x] = (unsigned char)fmin(255, floor(sum[x] / ratio + .5));
    }
    if (ok) jpeg_finish_decompress(&decoder);
    jpeg_destroy_decompress(&decoder);
    string_t *path = string_sprintf("%s/earth_surface%s.png", directory, width == 2048 ? "" : "_detail");
    ok = ok && path && save_image(path, width, height, width * 3, pixels);
    string_free(path);
    free(pixels);
    free(source);
    free(row);
    free(sum);
    return ok;
}

static string_t *retain_regions(file_t *output, const string_t *text)
{
    string_t *previous = file_read_all_text(output);
    json_t *old = previous ? json_from_text(previous) : NULL;
    json_t *next = json_from_text(text);
    string_t *regions_key = string_new_with("regions"), *assets_key = string_new_with("assets");
    const json_t *regions = old && regions_key ? json_object_get(old, regions_key) : NULL;
    const json_t *old_assets = old && assets_key ? json_object_get(old, assets_key) : NULL;
    json_t *assets = next && assets_key ? json_clone(json_object_get(next, assets_key)) : NULL;
    bool ok = next && assets && regions_key && assets_key && (!previous || old);
    if (regions) {
        size_t count = json_array_size(regions);
        ok = ok && json_type(regions) == JSON_ARRAY && count <= GLOBE_TEXTURE_REGION_LIMIT && json_object_set(next, regions_key, regions);
        for (size_t i = 0; ok && i < count; ++i) {
            string_t *key = string_sprintf("regional/%02u.png", (unsigned)i);
            const json_t *asset = key ? json_object_get(old_assets, key) : NULL;
            ok = asset && json_object_set(assets, key, asset);
            string_free(key);
        }
        ok = ok && json_object_set(next, assets_key, assets);
    }
    string_t *result = ok ? json_to_string_pretty(next, 2) : NULL;
    json_free(assets); json_free(next); json_free(old);
    string_free(previous); string_free(regions_key); string_free(assets_key);
    return result;
}

static bool save_manifest(const char *directory)
{
    string_t *manifest = string_new_with(
        "{\n  \"format_version\": 2,\n  \"name\": \"Blue Marble Next Generation\",\n"
        "  \"reference_month\": \"2004-07\",\n"
        "  \"source_url\": \"https://assets.science.nasa.gov/content/dam/science/esd/eo/images/bmng/"
        "bmng-topography-bathymetry/july/world.topo.bathy.200407.3x21600x10800.jpg\",\n"
        "  \"source_sha256\": \"d225f1f35a6448a4d1d8f6de6e48f3433e470085b70a35800e64f384f269a7b0\",\n"
        "  \"attribution\": \"NASA Earth Observatory; Reto Stoeckli, NASA Goddard Space Flight Center. "
        "Historical July 2004 reference imagery. NASA imagery reuse policy applies; no endorsement implied.\",\n"
        "  \"assets\": {\n");
    static const unsigned widths[] = {2048, 8192, 1800}, heights[] = {1024, 4096, 1800};
    bool ok = manifest != NULL;
    unsigned index = 0;
    for (unsigned level = 0; ok && level < 3; ++level) {
        for (unsigned row = 0; ok && row < (level == 2 ? 6u : 1u); ++row) {
            for (unsigned column = 0; ok && column < (level == 2 ? 12u : 1u); ++column) {
                string_t *relative = level == 2 ? string_sprintf("earth_tiles/%02u-%02u.png", column, row)
                                    : string_new_with(level ? "earth_surface_detail.png" : "earth_surface.png");
                string_t *path = relative ? string_sprintf("%s/%s", directory, string_c_str(relative)) : NULL;
                file_t *file = path ? file_new(path) : NULL;
                file_info_t *info = file ? file_get_info(file) : NULL;
                string_t *digest = info ? file_sha256(file) : NULL;
                ok = digest && string_append_format(manifest,
                    "%s    \"%s\": {\"width\": %u, \"height\": %u, \"bytes\": %llu, \"sha256\": \"%s\"}",
                    index++ ? ",\n" : "", string_c_str(relative), widths[level], heights[level],
                    (unsigned long long)file_info_size(info), string_c_str(digest)) >= 0;
                string_free(digest); file_info_free(info); file_free(file); string_free(path); string_free(relative);
            }
        }
    }
    string_t *path = string_sprintf("%s/manifest.json", directory);
    file_t *output = path ? file_new(path) : NULL;
    ok = ok && string_append_cstr(manifest, "\n  }\n}\n") >= 0 && output;
    string_t *combined = ok ? retain_regions(output, manifest) : NULL;
    ok = combined && file_write_all_text(output, combined);
    string_free(combined);
    file_free(output); string_free(path); string_free(manifest);
    return ok;
}

int main(int argc, char **argv)
{
    if (argc == 3 && !strcmp(argv[1], "--manifest")) return save_manifest(argv[2]) ? 0 : 1;
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "Usage: %s source.jpg tile-directory [overview-directory]\n"
                        "       %s --manifest packaged-texture-directory\n", argv[0], argv[0]);
        return 2;
    }
    file_t *source = file_new_cstr(argv[1]), *directory = file_new_cstr(argv[2]);
    file_info_t *info = source ? file_get_info(source) : NULL;
    array_t *bytes = info && file_info_size(info) <= 67108864 ? file_read_all_bytes(source) : NULL;
    bool ok = bytes && array_size(bytes) && directory && file_create_directory(directory, 0755, true);
    struct jpeg_decompress_struct decoder;
    struct jpeg_error_mgr error;
    decoder.err = jpeg_std_error(&error);
    jpeg_create_decompress(&decoder);
    unsigned char *strip = NULL;
    if (ok) {
        jpeg_mem_src(&decoder, array_get(bytes, 0), array_size(bytes));
        ok = jpeg_read_header(&decoder, true) == JPEG_HEADER_OK &&
             decoder.image_width == 21600 && decoder.image_height == 10800;
    }
    if (ok) {
        decoder.out_color_space = JCS_RGB;
        jpeg_start_decompress(&decoder);
        strip = malloc((size_t)1800 * 21600 * 3);
        ok = strip != NULL && decoder.output_components == 3;
    }
    for (unsigned row = 0; ok && row < 6; ++row) {
        for (unsigned line = 0; line < 1800; ++line) {
            JSAMPROW pixels = strip + (size_t)line * 21600 * 3;
            if (jpeg_read_scanlines(&decoder, &pixels, 1) != 1) { ok = false; break; }
        }
        for (unsigned column = 0; ok && column < 12; ++column) ok = save_tile(argv[2], column, row, strip);
        if (ok) fprintf(stderr, "Prepared tile row %u/6\n", row + 1);
    }
    if (ok) jpeg_finish_decompress(&decoder);
    jpeg_destroy_decompress(&decoder);
    free(strip);
    if (ok && argc == 4) {
        ok = save_overview(bytes, argv[3], 2048) && save_overview(bytes, argv[3], 8192);
        if (ok) fprintf(stderr, "Prepared matching global textures\n");
    }
    array_destroy(bytes);
    file_info_free(info);
    file_free(source);
    file_free(directory);
    return ok ? 0 : 1;
}
