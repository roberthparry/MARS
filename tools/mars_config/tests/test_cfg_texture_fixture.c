/**
 * @file test_cfg_texture_fixture.c
 * @brief Compact, valid PNG catalogue fixtures for transactional imagery tests.
 *
 * Keeps the real overview and original reference patch used by documented byte-count examples. Other images are
 * genuine single-colour PNGs at their declared dimensions, generated with bounded row buffers. A 128-region
 * catalogue exercises multi-digit addresses without copying the installed satellite collection. Fixtures stay
 * beneath caller-owned temporary directories and never change packaged images or configured databases.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sodium.h>
#include <zlib.h>

#include "array-t.h"
#include "file-t.h"
#include "json-t.h"
#include "string-t.h"
#include "test_cfg_support.h"

static file_t *fixture_root;

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

static void u32(unsigned char *out, uint32_t value)
{
    out[0] = value >> 24; out[1] = value >> 16; out[2] = value >> 8; out[3] = value;
}

static bool chunk(array_t *output, const char *kind, const void *data, size_t size)
{
    unsigned char length[4], checksum[4];
    u32(length, (uint32_t)size);
    uLong crc = crc32(0, (const Bytef *)kind, 4);
    if (size) crc = crc32(crc, data, (uInt)size);
    u32(checksum, (uint32_t)crc);
    return array_append_carray(output, length, 4) && array_append_carray(output, kind, 4) &&
           (!size || array_append_carray(output, data, size)) && array_append_carray(output, checksum, 4);
}

static json_t *pattern(const char *directory, unsigned width, unsigned height)
{
    size_t row_size = (size_t)width * 4 + 1;
    unsigned char *row = malloc(row_size), block[65536], header[13] = {0};
    array_t *compressed = array_create(1, NULL, NULL), *png = array_create(1, NULL, NULL);
    z_stream stream = {0};
    bool initialised = deflateInit(&stream, 1) == Z_OK;
    bool ok = row && compressed && png && initialised;
    if (row) {
        row[0] = 0;
        for (unsigned x = 0; x < width; ++x) {
            row[x * 4 + 1] = 35; row[x * 4 + 2] = 75; row[x * 4 + 3] = 53; row[x * 4 + 4] = 255;
        }
    }
    for (unsigned y = 0; ok && y < height; ++y) {
        stream.next_in = row; stream.avail_in = (uInt)row_size;
        do {
            stream.next_out = block; stream.avail_out = sizeof block;
            int rc = deflate(&stream, y + 1 == height ? Z_FINISH : Z_NO_FLUSH);
            ok = (rc == Z_OK || rc == Z_STREAM_END) &&
                 (stream.avail_out == sizeof block || array_append_carray(compressed, block, sizeof block - stream.avail_out));
            if (rc == Z_STREAM_END) break;
        } while (ok && (stream.avail_in || (y + 1 == height && !stream.avail_out)));
    }
    if (initialised) deflateEnd(&stream);
    free(row);
    u32(header, width); u32(header + 4, height); header[8] = 8; header[9] = 6;
    ok = ok && array_append_carray(png, "\211PNG\r\n\032\n", 8) && chunk(png, "IHDR", header, sizeof header) &&
         chunk(png, "IDAT", array_get(compressed, 0), array_size(compressed)) && chunk(png, "IEND", NULL, 0);
    string_t *path = string_sprintf("%s/pattern-%u.png", directory, width);
    file_t *file = file_new(path);
    unsigned char digest[crypto_hash_sha256_BYTES]; char hex[65];
    ok = ok && file_write_all_bytes(file, array_get(png, 0), array_size(png)) &&
         crypto_hash_sha256(digest, array_get(png, 0), array_size(png)) == 0 &&
         sodium_bin2hex(hex, sizeof hex, digest, sizeof digest);
    string_t *description = ok ? string_sprintf("{\"width\":%u,\"height\":%u,\"bytes\":%zu,\"sha256\":\"%s\"}",
                                               width, height, array_size(png), hex) : NULL;
    json_t *asset = description ? json_from_text(description) : NULL;
    file_free(file); string_free(path); string_free(description); array_destroy(png); array_destroy(compressed);
    return asset;
}

/* Create a compact package with real PNG payloads and a catalogue extending beyond two-digit slots. */
bool test_cfg_copy_textures(const char *directory)
{
    string_t *base = string_sprintf("%s/packaging/globe-db/textures", directory);
    string_t *manifest_path = string_sprintf("%s/packaging/globe-db/textures/manifest.json", MARS_CONFIG_ROOT_DIR);
    file_t *source_manifest = file_new(manifest_path);
    string_t *text = file_read_all_text(source_manifest);
    json_t *manifest = text ? json_from_text(text) : NULL, *assets = json_new_object(), *regions = json_new_array();
    const json_t *original_regions = field(manifest, "regions"), *original_assets = field(manifest, "assets");
    bool ok = manifest && assets && regions && json_array_size(original_regions) >= 44;
    static const char *folders[] = {"", "earth_tiles", "regional"};
    for (unsigned i = 0; ok && i < 3; ++i) {
        string_t *path = string_sprintf("%s/%s", string_c_str(base), folders[i]);
        file_t *folder = file_new(path);
        ok = file_create_directory(folder, 0700, true);
        file_free(folder); string_free(path);
    }
    json_t *small = ok ? pattern(string_c_str(base), 1800, 1800) : NULL;
    json_t *large = ok ? pattern(string_c_str(base), 3600, 3600) : NULL;
    json_t *detail = ok ? pattern(string_c_str(base), 8192, 4096) : NULL;
    ok = ok && small && large && detail;
    for (unsigned i = 0; ok && i < 74 + 128; ++i) {
        unsigned slot = i >= 74 ? i - 74 : 0;
        string_t *relative = i < 72 ? string_sprintf("earth_tiles/%02u-%02u.png", i % 12, i / 12) :
            i >= 74 ? string_sprintf("regional/%02u.png", slot) :
            string_new_with(i == 72 ? "earth_surface.png" : "earth_surface_detail.png");
        bool reference = i == 72 || i == 74;
        const json_t *asset = reference ? json_object_get(original_assets, relative) :
                             i == 73 ? detail : i < 72 ? small : large;
        string_t *source = reference ? string_sprintf("%s/packaging/globe-db/textures/%s", MARS_CONFIG_ROOT_DIR,
                                                      string_c_str(relative)) :
            string_sprintf("%s/pattern-%u.png", string_c_str(base), i == 73 ? 8192 : i < 72 ? 1800 : 3600);
        string_t *target = string_sprintf("%s/%s", string_c_str(base), string_c_str(relative));
        file_t *from = file_new(source), *to = file_new(target);
        ok = asset && (reference ? file_copy(from, to, false) :
             (file_create_hard_link(from, to) || file_copy(from, to, false))) &&
             json_object_set(assets, relative, asset);
        if (ok && i >= 74) {
            json_t *region = json_clone(json_array_get(original_regions, slot < 44 ? slot : 1));
            if (slot >= 44) {
                string_t *bounds_text = string_new_with("[-120,10,-119,11]");
                json_t *bounds = json_from_text(bounds_text);
                ok = region && set(region, "bounds", bounds);
                json_free(bounds); string_free(bounds_text);
            }
            ok = ok && region && json_array_append(regions, region);
            json_free(region);
        }
        file_free(from); file_free(to); string_free(source); string_free(target); string_free(relative);
    }
    ok = ok && set(manifest, "regions", regions) && set(manifest, "assets", assets);
    string_t *output = ok ? json_to_string_pretty(manifest, 2) : NULL;
    string_t *target = string_sprintf("%s/manifest.json", string_c_str(base));
    file_t *to = file_new(target);
    ok = output && file_write_all_text(to, output);
    file_free(to); string_free(target); string_free(output); string_free(base); string_free(manifest_path);
    file_free(source_manifest); string_free(text); json_free(manifest); json_free(assets); json_free(regions);
    json_free(small); json_free(large); json_free(detail);
    return ok;
}

static void fixture_cleanup(void)
{
    if (!fixture_root) return;
    string_t *base = string_sprintf("%s/packaging/globe-db/textures", file_path(fixture_root));
    for (unsigned i = 0; i < 74 + 128 + 4; ++i) {
        string_t *relative = i < 72 ? string_sprintf("earth_tiles/%02u-%02u.png", i % 12, i / 12) :
            i >= 74 && i < 202 ? string_sprintf("regional/%02u.png", i - 74) :
            i == 72 ? string_new_with("earth_surface.png") : i == 73 ? string_new_with("earth_surface_detail.png") :
            i == 202 ? string_new_with("manifest.json") : string_sprintf("pattern-%u.png", i == 203 ? 1800 : i == 204 ? 3600 : 8192);
        string_t *path = string_sprintf("%s/%s", string_c_str(base), string_c_str(relative));
        file_t *file = file_new(path);
        if (file_exists(file)) file_delete(file);
        file_free(file); string_free(path); string_free(relative);
    }
    static const char *folders[] = {"packaging/globe-db/textures/regional", "packaging/globe-db/textures/earth_tiles",
        "packaging/globe-db/textures", "packaging/globe-db", "packaging", ""};
    for (unsigned i = 0; i < sizeof folders / sizeof *folders; ++i) {
        string_t *path = string_sprintf("%s/%s", file_path(fixture_root), folders[i]);
        file_t *folder = file_new(path);
        file_remove_directory(folder); file_free(folder); string_free(path);
    }
    string_free(base); file_free(fixture_root); fixture_root = NULL;
}

/* Return process-owned fixture storage; tests are serial and the parent releases it at normal exit. */
const char *test_cfg_texture_root(void)
{
    if (!fixture_root) {
        const char *temporary = getenv("TMPDIR");
        string_t *parent = string_new_with(temporary && *temporary ? temporary : MARS_CONFIG_ROOT_DIR "/build");
        fixture_root = file_create_temp_directory(parent);
        string_free(parent);
        if (!fixture_root || !test_cfg_copy_textures(file_path(fixture_root))) { fixture_cleanup(); return NULL; }
        atexit(fixture_cleanup);
    }
    return file_path(fixture_root);
}
