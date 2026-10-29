/**
 * @file satellite_mask_main.c
 * @brief Checkpointed offline coastal masking of a photographic texture package.
 *
 * Accepts explicit source, shoreline database and output locations. Writes a new
 * package, retaining image addresses, source attribution and the global ocean
 * photographs. Sidecars permit restart after interruption; only a fully processed
 * package receives a manifest. Existing installed databases are read, never changed.
 */
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <gdal.h>

#include "array-t.h"
#include "file-t.h"
#include "json-t.h"
#include "satellite_mask.h"

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

static bool set_text(json_t *object, const char *name, const char *s)
{
    string_t *value = string_new_with(s);
    json_t *item = value ? json_new_string(value) : NULL;
    bool ok = set(object, name, item);
    string_free(value); json_free(item);
    return ok;
}

static bool set_size(json_t *object, const char *name, uint64_t value)
{
    string_t *s = string_sprintf("%" PRIu64, value);
    json_t *item = s ? json_new_number(s) : NULL;
    bool ok = set(object, name, item);
    string_free(s); json_free(item);
    return ok;
}

static json_t *load(const string_t *path)
{
    file_t *file = path ? file_new(path) : NULL;
    file_info_t *info = file ? file_get_info(file) : NULL;
    string_t *data = info && file_info_size(info) <= 8388608 ? file_read_all_text(file) : NULL;
    json_t *value = data ? json_from_text(data) : NULL;
    string_free(data); file_info_free(info); file_free(file);
    return value;
}

static bool save(const json_t *value, const string_t *path)
{
    string_t *temporary = string_sprintf("%s.part", string_c_str(path));
    file_t *output = file_new(temporary), *final = file_new(path);
    string_t *data = json_to_string_pretty(value, 2);
    bool ok = output && final && data && file_write_all_text(output, data) && file_move(output, final, true);
    string_free(temporary); string_free(data); file_free(output); file_free(final);
    return ok;
}

static bool keep_image(const string_t *source, const string_t *target, const json_t *asset)
{
    file_t *input = file_new(source), *output = file_new(target);
    string_t *sha = input ? file_sha256(input) : NULL;
    bool ok = sha && !strcmp(string_c_str(sha), text(field(asset, "sha256")));
    if (ok && file_exists(output)) {
        string_t *saved = file_sha256(output);
        ok = saved && !string_compare(sha, saved);
        string_free(saved);
    } else if (ok) ok = file_create_hard_link(input, output) ||
                           (file_last_error(input) == EXDEV && file_copy(input, output, false));
    string_free(sha); file_free(input); file_free(output);
    return ok;
}

static json_t *mask_image(globe_t *globe, const char *source_root, const char *output_root,
                         size_t slot, const json_t *region, const json_t *asset, const char *identity)
{
    string_t *source = string_sprintf("%s/regional/%02zu.png", source_root, slot);
    string_t *target = string_sprintf("%s/regional/%02zu.png", output_root, slot);
    string_t *record = string_sprintf("%s/checkpoints/%02zu.json", output_root, slot);
    file_t *input = file_new(source), *output = file_new(target);
    string_t *before = file_sha256(input);
    bool ok = before && !strcmp(string_c_str(before), text(field(asset, "sha256")));
    json_t *saved = ok ? load(record) : NULL;
    const char *old_sha = text(field(saved, "source_sha256"));
    string_t *written = saved ? file_sha256(output) : NULL;
    double maximum = number(field(asset, "width")) * number(field(asset, "height"));
    double visible = number(field(saved, "visible_pixels")), removed = number(field(saved, "removed_pixels"));
    bool resumed = ok && written && !strcmp(old_sha, string_c_str(before)) &&
        !strcmp(text(field(saved, "shoreline_identity")), identity) &&
        !strcmp(text(field(field(saved, "asset"), "sha256")), string_c_str(written)) &&
        isfinite(visible) && isfinite(removed) && visible >= 0 && removed >= 0 &&
        visible <= maximum && removed <= maximum && floor(visible) == visible && floor(removed) == removed;
    string_free(written);
    if (!resumed) {
        json_free(saved); saved = NULL;
        double bounds[4];
        const json_t *box = field(region, "bounds");
        ok = ok && json_array_size(box) == 4;
        for (unsigned i = 0; ok && i < 4; ++i) bounds[i] = number(json_array_get(box, i));
        if (ok && file_exists(output)) ok = file_delete(output);
        satellite_mask_stats_t stats;
        double source_count = number(field(region, "valid_pixels"));
        size_t source_visible = isfinite(source_count) && source_count >= 0 && source_count <= maximum &&
            floor(source_count) == source_count ? (size_t)source_count : SIZE_MAX;
        if (ok) ok = satellite_mask_png(globe, source, target, bounds, source_visible, &stats);
        file_info_t *info = ok ? file_get_info(output) : NULL;
        string_t *sha = info ? file_sha256(output) : NULL;
        saved = sha ? json_new_object() : NULL;
        json_t *next = saved ? json_clone(asset) : NULL;
        ok = saved && next && set_size(next, "bytes", file_info_size(info)) &&
            set_text(next, "sha256", string_c_str(sha)) && set(saved, "asset", next) &&
            set_size(saved, "visible_pixels", stats.visible_pixels) && set_size(saved, "removed_pixels", stats.removed_pixels) &&
            set_text(saved, "source_sha256", string_c_str(before)) && set_text(saved, "shoreline_identity", identity) &&
            save(saved, record);
        json_free(next); string_free(sha); file_info_free(info);
    }
    if (!ok) { fprintf(stderr, "Coastal masking failed at tile %zu; completed checkpoints remain reusable.\n", slot);
        json_free(saved); saved = NULL; }
    string_free(before); file_free(input); file_free(output);
    string_free(source); string_free(target); string_free(record);
    return saved;
}

/* Prepare a new derived package; the caller separately chooses publication and database installation. */
int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s source-textures shoreline.db output-textures\n", argv[0]);
        return EXIT_FAILURE;
    }
    GDALAllRegister(); GDALSetCacheMax64(64 * 1024 * 1024);
    string_t *path = string_sprintf("%s/manifest.json", argv[1]);
    json_t *manifest = load(path), *assets = json_clone(field(manifest, "assets"));
    json_t *regions = json_clone(field(manifest, "regions"));
    file_t *manifest_file = file_new(path);
    string_t *manifest_sha = manifest_file ? file_sha256(manifest_file) : NULL;
    string_free(path); file_free(manifest_file);
    bool ok = manifest_sha && manifest && assets && json_type(regions) == JSON_ARRAY &&
        json_array_size(regions) <= GLOBE_TEXTURE_REGION_LIMIT &&
        !setenv("MARS_GLOBE_DB_PATH", argv[2], 1) && !setenv("MARS_GLOBE_DB_KEY", "", 1);
    if (field(manifest, "coastal_mask")) {
        fputs("This package is already coast-masked; regenerate from the unmasked source package.\n", stderr);
        ok = false;
    }
    globe_reader_t *reader = ok ? globe_reader_new() : NULL;
    string_t *identity = reader ? globe_reader_identity(reader) : NULL;
    globe_t *globe = identity ? globe_reader_get(reader) : NULL;
    file_t *directory = file_new_cstr(argv[3]);
    path = string_sprintf("%s/coast-mask.json", argv[3]);
    json_t *marker = load(path);
    bool exists = directory && file_exists(directory);
    ok = ok && globe && directory && (!exists || (marker && number(field(marker, "version")) == 1 &&
        !strcmp(text(field(marker, "source_sha256")), string_c_str(manifest_sha)) &&
        !strcmp(text(field(marker, "shoreline_identity")), string_c_str(identity))));
    json_free(marker);
    if (ok && !exists) {
        marker = json_new_object();
        ok = file_create_directory(directory, 0755, true) && set_size(marker, "version", 1) &&
            set_text(marker, "source_sha256", string_c_str(manifest_sha)) &&
            set_text(marker, "shoreline_identity", string_c_str(identity)) && save(marker, path);
        json_free(marker);
    }
    string_free(path);
    const char *folders[] = {"earth_tiles", "regional", "checkpoints"};
    for (unsigned i = 0; ok && i < 3; ++i) {
        path = string_sprintf("%s/%s", argv[3], folders[i]);
        file_t *folder = file_new(path);
        ok = folder && (file_exists(folder) || file_create_directory(folder, 0755, true));
        string_free(path); file_free(folder);
    }
    if (ok) {
        puts("Preparing the indexed coastline once; raster working buffers stay below 256 MiB."); fflush(stdout);
        ok = globe_surface_prepare(globe);
    }
    for (unsigned i = 0; ok && i < 74; ++i) {
        string_t *relative = i < 72 ? string_sprintf("earth_tiles/%02u-%02u.png", i % 12, i / 12) :
            string_new_with(i == 72 ? "earth_surface.png" : "earth_surface_detail.png");
        string_t *source = string_sprintf("%s/%s", argv[1], string_c_str(relative));
        string_t *target = string_sprintf("%s/%s", argv[3], string_c_str(relative));
        ok = keep_image(source, target, json_object_get(assets, relative));
        string_free(relative); string_free(source); string_free(target);
    }
    uint64_t old_bytes = 0, new_bytes = 0, removed = 0;
    for (size_t i = 0; ok && i < json_array_size(regions); ++i) {
        struct statvfs space;
        ok = !statvfs(argv[3], &space) && (uint64_t)space.f_bavail * space.f_frsize >= UINT64_C(8) * 1073741824;
        if (!ok) { fputs("Stopped at the 8 GiB disk reserve; completed checkpoints remain reusable.\n", stderr); break; }
        json_t *region = json_array_get(regions, i) ? json_clone(json_array_get(regions, i)) : NULL;
        string_t *relative = string_sprintf("regional/%02zu.png", i);
        const json_t *asset = relative ? json_object_get(assets, relative) : NULL;
        json_t *saved = region && asset ? mask_image(globe, argv[1], argv[3], i, region, asset, string_c_str(identity)) : NULL;
        string_t *after = globe_reader_identity(reader);
        ok = saved && after && !string_compare(identity, after);
        if (ok) {
            old_bytes += (uint64_t)number(field(asset, "bytes"));
            new_bytes += (uint64_t)number(field(field(saved, "asset"), "bytes"));
            removed += (uint64_t)number(field(saved, "removed_pixels"));
            ok = json_object_set(assets, relative, field(saved, "asset")) &&
                set_size(region, "valid_pixels", (uint64_t)number(field(saved, "visible_pixels")));
            if (ok && number(field(saved, "removed_pixels")) > 0) {
                string_t *processing = string_sprintf("%s Offshore pixels cleared using the packaged coastline; "
                    "inland waters retained; coastline alpha uses two-by-two coverage samples.",
                    text(field(region, "processing")));
                ok = processing && set_text(region, "processing", string_c_str(processing));
                string_free(processing);
            }
            if (ok) {
                /* Existing JSON arrays have no replacement operation; this ordered rebuild is bounded to the catalogue. */
                if (!i) {
                    json_t *next = json_new_array();
                    ok = next && set(manifest, "regions", next);
                    json_free(next);
                }
                string_t *key = string_new_with("regions");
                json_t *next = json_object_get_mutable(manifest, key);
                ok = ok && next && json_array_append(next, region);
                string_free(key);
            }
        }
        string_free(after); json_free(saved); json_free(region); string_free(relative);
        if (ok && (i % 25 == 24 || i + 1 == json_array_size(regions))) {
            printf("Coastal masking: %zu/%zu; %.2f -> %.2f GiB, %.2f GiB saved\n", i + 1, json_array_size(regions),
                   old_bytes / 1073741824., new_bytes / 1073741824., ((double)old_bytes - new_bytes) / 1073741824.);
            fflush(stdout);
        }
    }
    json_t *mask = json_new_object();
    ok = ok && mask && set_size(mask, "version", 1) && set_text(mask, "shoreline_identity", string_c_str(identity)) &&
        set_size(mask, "removed_pixels", removed) && set_size(mask, "source_bytes", old_bytes) &&
        set_size(mask, "masked_bytes", new_bytes) && set(manifest, "coastal_mask", mask) && set(manifest, "assets", assets);
    path = string_sprintf("%s/manifest.json", argv[3]);
    ok = ok && save(manifest, path);
    if (ok) printf("COASTAL PACKAGE READY: %.2f GiB saved; global ocean imagery preserved.\n",
                   ((double)old_bytes - new_bytes) / 1073741824.);
    string_free(path); json_free(mask); json_free(regions); json_free(assets); json_free(manifest);
    string_free(identity); string_free(manifest_sha); file_free(directory); globe_reader_free(reader);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
