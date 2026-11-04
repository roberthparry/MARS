/**
 * @file texture_global.c
 * @brief Prepare and publish bounded lossless tiles from the full-resolution NASA GeoTIFF quadrants.
 *
 * This optional offline maintainer utility reads one 1800-row RGB strip at a time, preserving source pixels
 * without resampling or JPEG encoding. Eight quadrant checkpoints describe a 48 by 24 grid of 1800-square PNGs.
 * Publication verifies every content digest before adding immutable files and atomically replacing the package
 * manifest. Existing global images, regional slots and attribution are preserved. Databases are not modified.
 * TIFF development headers are needed only to build this utility; ordinary builds and runtime need no TIFF codec.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tiffio.h>
#include <zlib.h>

#include "array-t.h"
#include "file-t.h"
#include "globe-t.h"
#include "json-t.h"
#include "string-t.h"

static const json_t *field(const json_t *object, const char *name)
{
    string_t *key = string_new_with(name);
    const json_t *value = key ? json_object_get(object, key) : NULL;
    string_free(key);
    return value;
}

static bool set(json_t *object, const char *name, const json_t *value)
{
    string_t *key = string_new_with(name);
    bool ok = key && value && json_object_set(object, key, value);
    string_free(key);
    return ok;
}

static const char *text(const json_t *value)
{
    const string_t *s = json_string_value(value);
    return s ? string_c_str(s) : "";
}

static json_t *load(const string_t *path)
{
    file_t *file = path ? file_new(path) : NULL;
    file_info_t *info = file ? file_get_info(file) : NULL;
    string_t *source = info && file_info_size(info) <= GLOBE_TEXTURE_MANIFEST_LIMIT ? file_read_all_text(file) : NULL;
    json_t *value = source ? json_from_text(source) : NULL;
    string_free(source); file_info_free(info); file_free(file);
    return value;
}

static bool save_json(const string_t *path, const json_t *value)
{
    string_t *part = path ? string_sprintf("%s.part", string_c_str(path)) : NULL;
    string_t *source = value ? json_to_string_pretty(value, 2) : NULL;
    file_t *temporary = part ? file_new(part) : NULL, *target = path ? file_new(path) : NULL;
    bool ok = source && temporary && target && file_write_all_text(temporary, source) &&
              file_move(temporary, target, true);
    file_free(temporary); file_free(target); string_free(part); string_free(source);
    return ok;
}

static void u32(unsigned char *out, uint32_t value)
{
    out[0] = value >> 24; out[1] = value >> 16; out[2] = value >> 8; out[3] = value;
}

static bool chunk(array_t *output, const char *type, const unsigned char *data, size_t size)
{
    unsigned char prefix[8], checksum[4];
    u32(prefix, (uint32_t)size); memcpy(prefix + 4, type, 4);
    uLong crc = crc32(0, (const Bytef *)type, 4);
    if (size) crc = crc32(crc, data, (uInt)size);
    u32(checksum, (uint32_t)crc);
    return array_append_carray(output, prefix, 8) && (!size || array_append_carray(output, data, size)) &&
           array_append_carray(output, checksum, 4);
}

/* Sub filtering preserves RGB exactly while avoiding the storage overhead of unfiltered photographic PNGs. */
static bool save_tile(const string_t *path, const unsigned char *strip, unsigned column)
{
    const size_t stride = 21600 * 3, row_size = 1800 * 3 + 1, size = row_size * 1800;
    unsigned char *raw = malloc(size);
    uLongf length = compressBound(size);
    unsigned char *compressed = malloc(length);
    bool ok = raw && compressed;
    for (unsigned y = 0; ok && y < 1800; ++y) {
        const unsigned char *source = strip + y * stride + column * 1800 * 3;
        unsigned char *row = raw + y * row_size;
        row[0] = 1;
        for (unsigned x = 0; x < 1800 * 3; ++x) row[x + 1] = source[x] - (x >= 3 ? source[x - 3] : 0);
    }
    ok = ok && compress2(compressed, &length, raw, size, 6) == Z_OK;
    free(raw);
    array_t *png = ok ? array_create(1, NULL, NULL) : NULL;
    unsigned char header[13] = {0};
    u32(header, 1800); u32(header + 4, 1800); header[8] = 8; header[9] = 2;
    ok = png && array_append_carray(png, "\211PNG\r\n\032\n", 8) && chunk(png, "IHDR", header, 13) &&
         chunk(png, "IDAT", compressed, length) && chunk(png, "IEND", NULL, 0);
    free(compressed);
    string_t *part = path ? string_sprintf("%s.part", string_c_str(path)) : NULL;
    file_t *temporary = part ? file_new(part) : NULL, *target = path ? file_new(path) : NULL;
    ok = ok && temporary && target && file_write_all_bytes(temporary, array_get(png, 0), array_size(png)) &&
         file_move(temporary, target, true);
    file_free(temporary); file_free(target); string_free(part); array_destroy(png);
    return ok;
}

static uint32_t read_u32(const unsigned char *value)
{
    return (uint32_t)value[0] << 24 | (uint32_t)value[1] << 16 | (uint32_t)value[2] << 8 | value[3];
}

/* Decode the written PNG independently and compare every source pixel before a download may be discarded. */
static bool verify_tile(const string_t *path, const unsigned char *strip, unsigned column)
{
    file_t *file = path ? file_new(path) : NULL;
    file_info_t *info = file ? file_get_info(file) : NULL;
    array_t *bytes = info && file_info_size(info) <= 16777216 ? file_read_all_bytes(file) : NULL;
    size_t size = array_size(bytes), expected = (1800 * 3 + 1) * 1800;
    const unsigned char *png = size ? array_get(bytes, 0) : NULL, *compressed = NULL;
    size_t compressed_size = 0;
    bool ok = png && size >= 57 && !memcmp(png, "\211PNG\r\n\032\n", 8);
    size_t at = 8;
    for (unsigned part = 0; ok && part < 3; ++part) {
        ok = size - at >= 12;
        uint32_t length = ok ? read_u32(png + at) : 0;
        ok = ok && length <= size - at - 12;
        if (!ok) break;
        const unsigned char *kind = png + at + 4, *body = png + at + 8;
        uLong crc = crc32(0, kind, 4);
        if (length) crc = crc32(crc, body, length);
        ok = crc == read_u32(body + length);
        if (part == 0) ok = ok && !memcmp(kind, "IHDR", 4) && length == 13 && read_u32(body) == 1800 &&
            read_u32(body + 4) == 1800 && body[8] == 8 && body[9] == 2 && !body[10] && !body[11] && !body[12];
        else if (part == 1) {
            ok = ok && !memcmp(kind, "IDAT", 4);
            compressed = body; compressed_size = length;
        } else ok = ok && !memcmp(kind, "IEND", 4) && !length;
        at += (size_t)length + 12;
    }
    unsigned char *raw = ok && at == size ? malloc(expected) : NULL;
    uLongf decoded = expected;
    ok = raw && uncompress(raw, &decoded, compressed, compressed_size) == Z_OK && decoded == expected;
    for (unsigned y = 0; ok && y < 1800; ++y) {
        unsigned char *row = raw + (size_t)y * (1800 * 3 + 1);
        ok = row[0] == 1;
        for (unsigned x = 3; ok && x < 1800 * 3; ++x) row[x + 1] += row[x - 2];
        ok = ok && !memcmp(row + 1, strip + (size_t)y * 21600 * 3 + column * 1800 * 3, 1800 * 3);
    }
    free(raw); array_destroy(bytes); file_info_free(info); file_free(file);
    return ok;
}

static json_t *asset(const string_t *path)
{
    file_t *file = path ? file_new(path) : NULL;
    file_info_t *info = file ? file_get_info(file) : NULL;
    string_t *digest = info ? file_sha256(file) : NULL;
    string_t *source = digest ? string_sprintf(
        "{\"width\":1800,\"height\":1800,\"bytes\":%llu,\"sha256\":\"%s\"}",
        (unsigned long long)file_info_size(info), string_c_str(digest)) : NULL;
    json_t *result = source ? json_from_text(source) : NULL;
    string_free(source); string_free(digest); file_info_free(info); file_free(file);
    return result;
}

static bool quadrant(const char *input, const char *directory, unsigned column, unsigned row, bool verify)
{
    TIFF *image = TIFFOpen(input, "r");
    uint32_t width = 0, height = 0;
    uint16_t bits = 0, channels = 0, planar = 0, photo = 0, orientation = 0;
    bool ok = image && column < 4 && row < 2 && TIFFGetField(image, TIFFTAG_IMAGEWIDTH, &width) &&
        TIFFGetField(image, TIFFTAG_IMAGELENGTH, &height) &&
        TIFFGetFieldDefaulted(image, TIFFTAG_BITSPERSAMPLE, &bits) &&
        TIFFGetFieldDefaulted(image, TIFFTAG_SAMPLESPERPIXEL, &channels) &&
        TIFFGetFieldDefaulted(image, TIFFTAG_PLANARCONFIG, &planar) &&
        TIFFGetFieldDefaulted(image, TIFFTAG_PHOTOMETRIC, &photo) &&
        TIFFGetFieldDefaulted(image, TIFFTAG_ORIENTATION, &orientation) &&
        width == 21600 && height == 21600 && bits == 8 && channels == 3 && planar == PLANARCONFIG_CONTIG &&
        photo == PHOTOMETRIC_RGB && orientation == ORIENTATION_TOPLEFT && !TIFFIsTiled(image) &&
        TIFFScanlineSize64(image) == 21600 * 3;
    file_t *folder = file_new_cstr(directory);
    ok = ok && folder && file_create_directory(folder, 0755, true);
    unsigned char *strip = ok ? malloc((size_t)1800 * 21600 * 3) : NULL;
    json_t *assets = json_new_object();
    ok = ok && strip && assets;
    for (unsigned y = 0; ok && y < 12; ++y) {
        for (unsigned line = 0; ok && line < 1800; ++line)
            ok = TIFFReadScanline(image, strip + (size_t)line * 21600 * 3, y * 1800 + line, 0) >= 0;
        for (unsigned x = 0; ok && x < 12; ++x) {
            string_t *relative = string_sprintf("earth_tiles_detail/%02u-%02u.png", column * 12 + x, row * 12 + y);
            string_t *path = string_sprintf("%s/%02u-%02u.png", directory, column * 12 + x, row * 12 + y);
            ok = relative && path && (verify || save_tile(path, strip, x)) && verify_tile(path, strip, x);
            json_t *description = ok ? asset(path) : NULL;
            ok = description && json_object_set(assets, relative, description);
            json_free(description); string_free(path); string_free(relative);
            if (ok) fprintf(stderr, "Tiles completed: %u/144 (%c%u)\n", y * 12 + x + 1, 'A' + column, row + 1);
        }
    }
    if (image) TIFFClose(image);
    free(strip); file_free(folder);
    file_t *source = file_new_cstr(input);
    string_t *digest = ok ? file_sha256(source) : NULL;
    string_t *metadata = digest ? string_sprintf(
        "{\"source_url\":\"https://assets.science.nasa.gov/content/dam/science/esd/eo/images/bmng/"
        "bmng-topography-bathymetry/july/world.topo.bathy.200407.3x21600x21600.%c%u_geo.tif\","
        "\"source_sha256\":\"%s\"}", 'A' + column, row + 1, string_c_str(digest)) : NULL;
    json_t *checkpoint = metadata ? json_from_text(metadata) : NULL;
    string_t *path = string_sprintf("%s/%c%u.json", directory, 'A' + column, row + 1);
    ok = ok && checkpoint && set(checkpoint, "assets", assets) && save_json(path, checkpoint);
    string_free(path); string_free(metadata); string_free(digest); file_free(source);
    json_free(checkpoint); json_free(assets);
    return ok;
}

static bool publish(const char *directory, const char *package)
{
    string_t *path = string_sprintf("%s/manifest.json", package);
    file_t *base = path ? file_new(path) : NULL;
    string_t *before = base ? file_sha256(base) : NULL;
    json_t *manifest = load(path), *assets = json_clone(field(manifest, "assets"));
    string_t *layout = string_new_with("{\"width\":86400,\"height\":43200,\"columns\":48,\"rows\":24,"
                                      "\"tile_size\":1800,\"reference_month\":\"2004-07\"}");
    json_t *detail = json_from_text(layout), *sources = json_new_array();
    string_t *folder_path = string_sprintf("%s/earth_tiles_detail", package);
    file_t *folder = file_new(folder_path);
    bool ok = before && manifest && assets && detail && sources && folder && file_create_directory(folder, 0755, true);
    uint64_t bytes = 0;
    for (unsigned row = 0; ok && row < 2; ++row) {
        for (unsigned column = 0; ok && column < 4; ++column) {
            string_t *checkpoint_path = string_sprintf("%s/%c%u.json", directory, 'A' + column, row + 1);
            json_t *checkpoint = load(checkpoint_path);
            const json_t *images = field(checkpoint, "assets");
            json_t *credit = json_new_object();
            ok = checkpoint && json_object_size(images) == 144 && credit &&
                 set(credit, "source_url", field(checkpoint, "source_url")) &&
                 set(credit, "source_sha256", field(checkpoint, "source_sha256")) && json_array_append(sources, credit);
            for (unsigned y = 0; ok && y < 12; ++y) {
                for (unsigned x = 0; ok && x < 12; ++x) {
                    string_t *relative = string_sprintf("earth_tiles_detail/%02u-%02u.png", column * 12 + x, row * 12 + y);
                    string_t *from = string_sprintf("%s/%02u-%02u.png", directory, column * 12 + x, row * 12 + y);
                    string_t *to = string_sprintf("%s/%s", package, string_c_str(relative));
                    json_t *current = asset(from);
                    const json_t *expected = json_object_get(images, relative);
                    file_t *source = file_new(from), *target = file_new(to);
                    ok = current && expected && source && target &&
                         !strcmp(text(field(current, "sha256")), text(field(expected, "sha256")));
                    if (ok && file_exists(target)) {
                        string_t *digest = file_sha256(target);
                        ok = digest && !strcmp(string_c_str(digest), text(field(expected, "sha256")));
                        string_free(digest);
                    } else if (ok) ok = file_create_hard_link(source, target);
                    ok = ok && json_object_set(assets, relative, current);
                    file_info_t *info = ok ? file_get_info(source) : NULL;
                    if (info) bytes += file_info_size(info);
                    file_info_free(info); file_free(source); file_free(target); json_free(current);
                    string_free(relative); string_free(from); string_free(to);
                }
            }
            json_free(credit); json_free(checkpoint); string_free(checkpoint_path);
        }
    }
    string_t *after = ok ? file_sha256(base) : NULL;
    ok = ok && after && !string_compare(before, after) &&
         set(detail, "sources", sources) && set(manifest, "global_detail", detail) &&
         set(manifest, "assets", assets) && save_json(path, manifest);
    if (ok) printf("Published 1152 full-resolution NASA PNGs: %llu bytes. Ready for make release.\n",
                   (unsigned long long)bytes);
    file_free(folder); string_free(folder_path); string_free(path); string_free(layout);
    file_free(base); string_free(before); string_free(after);
    json_free(manifest); json_free(assets); json_free(detail); json_free(sources);
    return ok;
}

int main(int argc, char **argv)
{
    if (argc == 4 && !strcmp(argv[1], "--publish")) return publish(argv[2], argv[3]) ? EXIT_SUCCESS : EXIT_FAILURE;
    if (argc != 6 || (strcmp(argv[1], "--quadrant") && strcmp(argv[1], "--verify")) ||
        strlen(argv[4]) != 1 || strlen(argv[5]) != 1 ||
        argv[4][0] < '0' || argv[4][0] > '3' || argv[5][0] < '0' || argv[5][0] > '1') {
        fprintf(stderr, "Usage: %s --quadrant source.tif tile-directory column(0..3) row(0..1)\n"
                        "       %s --verify source.tif tile-directory column(0..3) row(0..1)\n"
                        "       %s --publish tile-directory package-directory\n", argv[0], argv[0], argv[0]);
        return 2;
    }
    return quadrant(argv[2], argv[3], (unsigned)(argv[4][0] - '0'), (unsigned)(argv[5][0] - '0'),
                    !strcmp(argv[1], "--verify")) ?
        EXIT_SUCCESS : EXIT_FAILURE;
}
