/**
 * @file satellite_catalogue.c
 * @brief Validate rendered satellite checkpoints and assemble an installable imagery catalogue.
 *
 * This optional offline maintainer tool combines a base texture package with labelled rendering plans. It checks
 * contiguous slot identities, bounds, dimensions, observation counts, PNG chunk CRCs and content digests before
 * publishing the manifest. Images are immutable hard links where possible, or copies across filesystems. File and
 * JSON operations use MARS APIs; only one encoded image is resident at a time. An optional repair mode fills
 * transparent overview gaps from indexed detail in a disposable output package. No database is changed here.
 */
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sodium.h>
#include <zlib.h>
#include <gdal.h>

#include "array-t.h"
#include "file-t.h"
#include "globe-t.h"
#include "json-t.h"
#include "string-t.h"
#include "satellite_overviews.h"

typedef struct {
    string_t *source;
    json_t *region;
    json_t *asset;
} satellite_entry_t;

typedef struct {
    satellite_entry_t entries[GLOBE_TEXTURE_REGION_LIMIT];
    size_t count;
    uint64_t bytes;
    bool coastal;
} satellite_catalogue_t;

static const json_t *field(const json_t *object, const char *name)
{
    string_t *key = string_new_with(name);
    const json_t *value = key ? json_object_get(object, key) : NULL;
    string_free(key);
    return value;
}

static const char *text(const json_t *value)
{
    const string_t *s = json_string_value(value);
    return s ? string_c_str(s) : "";
}

static double number(const json_t *value)
{
    const string_t *s = json_number_text(value);
    char *end = NULL;
    double result = s ? strtod(string_c_str(s), &end) : NAN;
    return end && !*end ? result : NAN;
}

static bool set(json_t *object, const char *name, const json_t *value)
{
    string_t *key = string_new_with(name);
    bool ok = key && value && json_object_set(object, key, value);
    string_free(key);
    return ok;
}

static json_t *load(const string_t *path)
{
    file_t *file = path ? file_new(path) : NULL;
    file_info_t *info = file ? file_get_info(file) : NULL;
    string_t *s = info && file_info_size(info) <= GLOBE_TEXTURE_MANIFEST_LIMIT ? file_read_all_text(file) : NULL;
    json_t *value = s ? json_from_text(s) : NULL;
    string_free(s); file_info_free(info); file_free(file);
    return value;
}

static uint32_t u32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static bool png_valid(const unsigned char *p, size_t size, unsigned width, unsigned height)
{
    if (size < 57 || memcmp(p, "\211PNG\r\n\032\n", 8)) return false;
    bool header = false, pixels = false;
    for (size_t at = 8; at + 12 <= size;) {
        uint32_t length = u32(p + at);
        if (length > size - at - 12) return false;
        const unsigned char *kind = p + at + 4, *body = kind + 4;
        if (crc32(0, kind, length + 4) != u32(body + length)) return false;
        if (!header) {
            if (memcmp(kind, "IHDR", 4) || length != 13 || u32(body) != width || u32(body + 4) != height ||
                body[8] != 8 || (body[9] != 2 && body[9] != 6) || body[10] || body[11] || body[12]) return false;
            header = true;
        } else if (!memcmp(kind, "IHDR", 4)) return false;
        if (!memcmp(kind, "IDAT", 4)) pixels = true;
        if (!memcmp(kind, "IEND", 4)) return !length && pixels && at + 12 == size;
        at += (size_t)length + 12;
    }
    return false;
}

static bool publish_image(const string_t *source, const string_t *output, const json_t *asset, uint64_t *total)
{
    file_t *input = source ? file_new(source) : NULL, *target = output ? file_new(output) : NULL;
    file_info_t *info = input ? file_get_info(input) : NULL;
    double width = number(field(asset, "width")), height = number(field(asset, "height"));
    const char *expected = text(field(asset, "sha256"));
    bool ok = expected && strlen(expected) == 64 && info && file_info_type(info) == FILE_TYPE_REGULAR &&
              file_info_size(info) <= 134217728 &&
              number(field(asset, "bytes")) == file_info_size(info) && width > 0 && width <= 8192 &&
              height > 0 && height <= 4096 && floor(width) == width && floor(height) == height;
    array_t *bytes = ok ? file_read_all_bytes(input) : NULL;
    size_t count = array_size(bytes);
    const unsigned char *p = count ? array_get(bytes, 0) : NULL;
    unsigned char digest[crypto_hash_sha256_BYTES];
    char hex[65];
    ok = p && count == file_info_size(info) && png_valid(p, count, (unsigned)width, (unsigned)height) &&
         crypto_hash_sha256(digest, p, count) == 0 && sodium_bin2hex(hex, sizeof hex, digest, sizeof digest) &&
         !strcmp(hex, expected);
    array_destroy(bytes);
    if (ok && target && !file_exists(target))
        ok = file_create_hard_link(input, target) || (file_last_error(input) == EXDEV && file_copy(input, target, false));
    else ok = false;
    if (ok) *total += count;
    else fprintf(stderr, "Image validation or publication failed: %s\n", source ? string_c_str(source) : "(missing)");
    file_info_free(info); file_free(input); file_free(target);
    return ok;
}

static bool same_bounds(const json_t *left, const json_t *right)
{
    if (json_array_size(left) != 4 || json_array_size(right) != 4) return false;
    for (unsigned i = 0; i < 4; ++i) {
        double a = number(json_array_get(left, i)), b = number(json_array_get(right, i));
        if (!isfinite(a) || !isfinite(b) || fabs(a - b) > 1e-9) return false;
    }
    double west = number(json_array_get(left, 0)), south = number(json_array_get(left, 1));
    double east = number(json_array_get(left, 2)), north = number(json_array_get(left, 3));
    return west >= -180 && west < east && east <= 180 && south >= -90 && south < north && north <= 90;
}

static bool add_batch(satellite_catalogue_t *catalogue, const char *label, const char *plan_path, const char *directory)
{
    string_t *path = string_new_with(plan_path);
    json_t *plan = load(path);
    string_free(path);
    const json_t *tiles = field(plan, "tiles");
    bool ok = json_type(tiles) == JSON_ARRAY && json_array_size(tiles) > 0;
    for (size_t i = 0; ok && i < json_array_size(tiles); ++i) {
        const json_t *tile = json_array_get(tiles, i);
        double slot_value = number(field(tile, "slot"));
        unsigned slot = slot_value >= 0 && slot_value < GLOBE_TEXTURE_REGION_LIMIT ? (unsigned)slot_value :
                                                                                  GLOBE_TEXTURE_REGION_LIMIT;
        if (slot >= GLOBE_TEXTURE_REGION_LIMIT || slot_value != slot || slot > catalogue->count) { ok = false; break; }
        path = string_sprintf("%s/%02u.json", directory, slot);
        json_t *saved = load(path);
        string_free(path);
        const json_t *region = field(saved, "region"), *asset = field(saved, "asset");
        double edge = number(field(tile, "width"));
        if (!field(tile, "width")) edge = 3600;
        double valid = number(field(saved, "valid_pixels"));
        ok = number(field(saved, "slot")) == slot && (edge == 1800 || edge == 3600) &&
             number(field(asset, "width")) == edge && number(field(asset, "height")) == edge &&
             (valid > 0 || (valid == 0 && json_type(field(saved, "coastal_mask")) == JSON_STRING)) &&
             valid <= edge * edge && floor(valid) == valid &&
             same_bounds(field(tile, "bounds"), field(region, "bounds"));
        if (catalogue->coastal) ok = ok && strlen(text(field(saved, "coastal_mask"))) == 64;
        satellite_entry_t *entry = &catalogue->entries[slot];
        if (ok && slot < catalogue->count) ok = same_bounds(field(entry->region, "bounds"), field(region, "bounds"));
        json_t *next_region = ok ? json_clone(region) : NULL, *next_asset = ok ? json_clone(asset) : NULL;
        string_t *name = string_sprintf("%s Sentinel tile %u", label, slot);
        json_t *title = name ? json_new_string(name) : NULL;
        ok = ok && next_region && next_asset && title && set(next_region, "name", title) &&
             set(next_region, "valid_pixels", field(saved, "valid_pixels"));
        if (ok && field(saved, "coastal_mask"))
            ok = set(next_region, "coastal_mask_identity", field(saved, "coastal_mask"));
        json_free(title); string_free(name);
        if (ok) {
            string_free(entry->source); json_free(entry->region); json_free(entry->asset);
            entry->source = string_sprintf("%s/%02u.png", directory, slot);
            entry->region = next_region; entry->asset = next_asset;
            if (slot == catalogue->count) ++catalogue->count;
            ok = entry->source != NULL;
        } else { json_free(next_region); json_free(next_asset); }
        json_free(saved);
    }
    if (!ok) fprintf(stderr, "Checkpoint plan failed validation: %s\n", plan_path);
    else printf("Validated metadata: %s (%zu tiles)\n", label, json_array_size(tiles));
    fflush(stdout);
    json_free(plan);
    return ok;
}

static bool assemble(int argc, char **argv, bool repair, const double *window)
{
    string_t *path = string_sprintf("%s/manifest.json", argv[1]);
    json_t *manifest = load(path), *assets = json_clone(field(manifest, "assets"));
    string_free(path);
    const json_t *old = field(manifest, "regions");
    size_t base_count = json_array_size(old);
    satellite_catalogue_t *catalogue = calloc(1, sizeof *catalogue);
    bool ok = manifest && assets && catalogue && json_array_size(old) <= GLOBE_TEXTURE_REGION_LIMIT;
    if (catalogue) catalogue->coastal = json_type(field(manifest, "coastal_mask")) == JSON_OBJECT;
    for (size_t i = 0; ok && i < json_array_size(old); ++i) {
        string_t *relative = string_sprintf("regional/%02zu.png", i);
        satellite_entry_t *entry = &catalogue->entries[i];
        entry->source = relative ? string_sprintf("%s/%s", argv[1], string_c_str(relative)) : NULL;
        entry->region = json_clone(json_array_get(old, i));
        entry->asset = relative ? json_clone(json_object_get(assets, relative)) : NULL;
        ok = entry->source && entry->region && entry->asset;
        string_free(relative);
        ++catalogue->count;
    }
    for (int i = 3; ok && i < argc; i += 3) ok = add_batch(catalogue, argv[i], argv[i + 1], argv[i + 2]);
    file_t *directory = file_new_cstr(argv[2]);
    ok = ok && directory && !file_exists(directory) && file_create_directory(directory, 0755, true);
    static const char *folders[] = {"earth_tiles", "regional"};
    for (unsigned i = 0; ok && i < 2; ++i) {
        path = string_sprintf("%s/%s", argv[2], folders[i]);
        file_t *folder = file_new(path);
        ok = folder && file_create_directory(folder, 0755, true);
        file_free(folder); string_free(path);
    }
    for (unsigned i = 0; ok && i < 74; ++i) {
        string_t *relative = i < 72 ? string_sprintf("earth_tiles/%02u-%02u.png", i % 12, i / 12) :
                           string_new_with(i == 72 ? "earth_surface.png" : "earth_surface_detail.png");
        string_t *source = relative ? string_sprintf("%s/%s", argv[1], string_c_str(relative)) : NULL;
        path = relative ? string_sprintf("%s/%s", argv[2], string_c_str(relative)) : NULL;
        ok = relative && publish_image(source, path, json_object_get(assets, relative), &catalogue->bytes);
        string_free(relative); string_free(source); string_free(path);
    }
    json_t *regions = json_new_array();
    for (size_t i = 0; ok && i < catalogue->count; ++i) {
        satellite_entry_t *entry = &catalogue->entries[i];
        string_t *relative = string_sprintf("regional/%02zu.png", i);
        path = relative ? string_sprintf("%s/%s", argv[2], string_c_str(relative)) : NULL;
        ok = relative && regions && publish_image(entry->source, path, entry->asset, &catalogue->bytes) &&
             json_array_append(regions, entry->region) && json_object_set(assets, relative, entry->asset);
        string_free(relative); string_free(path);
        if (ok && (i % 100 == 99 || i + 1 == catalogue->count)) {
            printf("Validated PNGs: %zu/%zu\n", i + 1, catalogue->count); fflush(stdout);
        }
    }
    ok = ok && set(manifest, "regions", regions) && set(manifest, "assets", assets);
    size_t repaired = 0;
    string_t *output_directory = repair ? string_new_with(argv[2]) : NULL;
    if (ok && repair) ok = output_directory && satellite_overviews_repair(output_directory, manifest, window, &repaired);
    string_free(output_directory);
    if (ok && repair) {
        const json_t *updated = field(manifest, "assets");
        catalogue->bytes = 0;
        for (size_t i = 0; i < json_object_size(updated); ++i)
            catalogue->bytes += (uint64_t)number(field(json_object_value_at(updated, i), "bytes"));
        printf("Overview repair complete: %zu images updated.\n", repaired);
    }
    if (ok && catalogue->coastal && catalogue->count > base_count) {
        uint64_t bytes = 0;
        for (size_t i = 0; i < catalogue->count; ++i) bytes += (uint64_t)number(field(catalogue->entries[i].asset, "bytes"));
        json_t *mask = json_new_object();
        string_t *one = string_new_with("1"), *length = string_sprintf("%llu", (unsigned long long)bytes);
        json_t *version = one ? json_new_number(one) : NULL, *size = length ? json_new_number(length) : NULL;
        ok = mask && set(mask, "previous_catalogue", field(manifest, "coastal_mask")) &&
             set(mask, "version", version) && set(mask, "masked_bytes", size) && set(manifest, "coastal_mask", mask);
        json_free(version); json_free(size); json_free(mask); string_free(one); string_free(length);
    }
    string_t *serialised = ok ? json_to_string_pretty(manifest, 2) : NULL;
    path = string_sprintf("%s/manifest.json.part", argv[2]);
    string_t *final_path = string_sprintf("%s/manifest.json", argv[2]);
    file_t *temporary = file_new(path), *final = file_new(final_path);
    ok = ok && serialised && file_write_all_text(temporary, serialised) && file_move(temporary, final, false);
    if (ok) printf("CATALOGUE READY: %zu regional images, %.2f GiB; complete manifest published.\n",
                   catalogue->count, catalogue->bytes / 1073741824.);
    for (size_t i = 0; catalogue && i < catalogue->count; ++i) {
        string_free(catalogue->entries[i].source); json_free(catalogue->entries[i].region);
        json_free(catalogue->entries[i].asset);
    }
    free(catalogue); file_free(directory); json_free(regions); json_free(assets); json_free(manifest);
    file_free(temporary); file_free(final); string_free(path); string_free(final_path); string_free(serialised);
    return ok;
}

/* Assemble a new package; the caller chooses when to replace the active source package and install it. */
int main(int argc, char **argv)
{
    bool repair = (argc == 4 || argc == 8) && !strcmp(argv[1], "--repair-overviews");
    double window[4];
    bool bounded = repair && argc == 8;
    for (unsigned i = 0; bounded && i < 4; ++i) {
        char *end = NULL;
        window[i] = strtod(argv[4 + i], &end);
        if (!end || !*argv[4 + i] || *end || !isfinite(window[i])) return EXIT_FAILURE;
    }
    if (bounded && (window[0] < -180 || window[2] > 180 || window[1] < -90 || window[3] > 90 ||
        window[0] >= window[2] || window[1] >= window[3])) return EXIT_FAILURE;
    if (bounded) argc = 4;
    if (repair) { --argc; ++argv; }
    if (argc < 3 || (argc - 3) % 3) {
        fprintf(stderr, "Usage: %s base-textures output-textures [label plan.json rendered-directory]...\n"
                        "       %s --repair-overviews base-textures output-textures [west south east north]\n", argv[0], argv[0]);
        return EXIT_FAILURE;
    }
    if (repair) GDALAllRegister();
    return sodium_init() >= 0 && assemble(argc, argv, repair, bounded ? window : NULL) ? EXIT_SUCCESS : EXIT_FAILURE;
}
