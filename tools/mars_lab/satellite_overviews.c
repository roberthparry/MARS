/**
 * @file satellite_overviews.c
 * @brief Offline reconstruction of missing overview pixels from existing detailed observations.
 *
 * An in-memory spatial index selects detail tiles intersecting each incomplete overview.
 * GDAL resamples their actual RGBA coverage, rather than assuming rectangular bounds imply
 * valid observations. Only transparent target pixels are filled. Preparation keeps a bounded
 * raster working set, replaces immutable image links safely and records updated provenance.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gdal.h>
#include <gdal_utils.h>

#include "file-t.h"
#include "globe-t.h"
#include "sqlite-t.h"
#include "satellite_overviews.h"

static const json_t *field(const json_t *object, const char *name)
{
    string_t *key = string_new_with(name);
    const json_t *value = key ? json_object_get(object, key) : NULL;
    string_free(key);
    return value;
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

static bool set_size(json_t *object, const char *name, uint64_t value)
{
    string_t *s = string_sprintf("%llu", (unsigned long long)value);
    json_t *item = s ? json_new_number(s) : NULL;
    bool ok = set(object, name, item);
    string_free(s); json_free(item);
    return ok;
}

static bool bounds(const json_t *region, double b[4])
{
    const json_t *box = field(region, "bounds");
    if (json_array_size(box) != 4) return false;
    for (unsigned i = 0; i < 4; ++i) b[i] = number(json_array_get(box, i));
    return isfinite(b[0]) && isfinite(b[1]) && isfinite(b[2]) && isfinite(b[3]) &&
        b[0] >= -180 && b[2] <= 180 && b[1] >= -90 && b[3] <= 90 && b[0] < b[2] && b[1] < b[3];
}

static bool overview(const double b[4])
{
    return fmax(b[2] - b[0], b[3] - b[1]) > 1;
}

static bool index_regions(sqlite_t *index, const json_t *regions)
{
    bool ok = sqlite_exec_cstr(index,
        "CREATE VIRTUAL TABLE detail USING rtree(slot, west, east, south, north)");
    sqlite_stmt_t *insert = ok ? sqlite_stmt_prepare(index, "INSERT INTO detail VALUES (?1, ?2, ?3, ?4, ?5)") : NULL;
    ok = insert != NULL;
    for (size_t slot = 0; ok && slot < json_array_size(regions); ++slot) {
        double b[4];
        ok = bounds(json_array_get(regions, slot), b);
        if (!ok || overview(b)) continue;
        ok = sqlite_stmt_bind_int(insert, 1, (int)slot) && sqlite_stmt_bind_double(insert, 2, b[0]) &&
            sqlite_stmt_bind_double(insert, 3, b[2]) && sqlite_stmt_bind_double(insert, 4, b[1]) &&
            sqlite_stmt_bind_double(insert, 5, b[3]) && sqlite_stmt_step(insert) == SQLITE_STEP_DONE;
        sqlite_stmt_reset(insert);
    }
    sqlite_stmt_finalize(insert);
    return ok;
}

static GDALDatasetH open_image(const string_t *directory, size_t slot, unsigned edge)
{
    string_t *path = string_sprintf("%s/regional/%02zu.png", string_c_str(directory), slot);
    GDALDatasetH image = path ? GDALOpen(string_c_str(path), GA_ReadOnly) : NULL;
    string_free(path);
    if (image && (GDALGetRasterXSize(image) != (int)edge || GDALGetRasterYSize(image) != (int)edge ||
        (GDALGetRasterCount(image) != 3 && GDALGetRasterCount(image) != 4) ||
        (GDALGetRasterCount(image) == 4 && GDALGetRasterColorInterpretation(GDALGetRasterBand(image, 4)) != GCI_AlphaBand))) {
        fprintf(stderr, "Unexpected regional PNG %zu: %dx%d, %d bands, expected %u-square RGB or RGBA.\n", slot,
            GDALGetRasterXSize(image), GDALGetRasterYSize(image), GDALGetRasterCount(image), edge);
        GDALClose(image); image = NULL;
    }
    return image;
}

static GDALDatasetH positioned_image(const string_t *directory, const json_t *regions, const json_t *assets, size_t slot)
{
    double b[4];
    string_t *relative = string_sprintf("regional/%02zu.png", slot);
    const json_t *asset = relative ? json_object_get(assets, relative) : NULL;
    double edge = number(field(asset, "width"));
    GDALDatasetH image = (edge == 1800 || edge == 3600) && number(field(asset, "height")) == edge &&
        bounds(json_array_get(regions, slot), b) ? open_image(directory, slot, (unsigned)edge) : NULL;
    string_free(relative);
    char coordinate[4][40];
    for (unsigned i = 0; image && i < 4; ++i) snprintf(coordinate[i], sizeof coordinate[i], "%.12f", b[i]);
    char *args[] = {"-of", "VRT", "-a_srs", "EPSG:4326", "-a_ullr", coordinate[0], coordinate[3],
        coordinate[2], coordinate[1], "-b", "1", "-b", "2", "-b", "3", "-b",
        image && GDALGetRasterCount(image) == 3 ? "mask,1" : "4", "-colorinterp", "red,green,blue,alpha", NULL};
    GDALTranslateOptions *options = image ? GDALTranslateOptionsNew(args, NULL) : NULL;
    int usage = 0;
    GDALDatasetH positioned = options ? GDALTranslate("", image, options, &usage) : NULL;
    GDALTranslateOptionsFree(options);
    /* The VRT owns a reference to its PNG source after GDALTranslate returns. */
    if (image) GDALReleaseDataset(image);
    if (positioned && usage) { GDALClose(positioned); positioned = NULL; }
    return positioned;
}

static GDALDatasetH detail_image(const string_t *directory, const json_t *regions, const json_t *assets,
                                sqlite_t *index, const double b[4], unsigned edge, bool *success)
{
    sqlite_stmt_t *query = sqlite_stmt_prepare(index,
        "SELECT slot\n"
        "FROM detail\n"
        "WHERE west < ?1 AND east > ?2 AND south < ?3 AND north > ?4\n"
        "ORDER BY slot");
    bool ok = query && sqlite_stmt_bind_double(query, 1, b[2]) && sqlite_stmt_bind_double(query, 2, b[0]) &&
        sqlite_stmt_bind_double(query, 3, b[3]) && sqlite_stmt_bind_double(query, 4, b[1]);
    /* At most 128 overlapping half-degree detail tiles cover one catalogue overview. */
    GDALDatasetH sources[128] = {0};
    unsigned count = 0;
    sqlite_step_result_t step = SQLITE_STEP_DONE;
    while (ok && (step = sqlite_stmt_step(query)) == SQLITE_STEP_ROW) {
        int slot = sqlite_stmt_column_int(query, 0);
        double source_bounds[4];
        ok = slot >= 0 && (size_t)slot < json_array_size(regions) &&
             bounds(json_array_get(regions, (size_t)slot), source_bounds);
        if (!ok || source_bounds[0] >= b[2] || source_bounds[2] <= b[0] ||
            source_bounds[1] >= b[3] || source_bounds[3] <= b[1]) continue;
        ok = count < 128;
        if (ok) { sources[count] = positioned_image(directory, regions, assets, (size_t)slot); ok = sources[count++] != NULL; }
    }
    sqlite_stmt_finalize(query);
    char coordinate[4][40], dimension[16];
    for (unsigned i = 0; i < 4; ++i) snprintf(coordinate[i], sizeof coordinate[i], "%.12f", b[i]);
    snprintf(dimension, sizeof dimension, "%u", edge);
    char *args[] = {"-of", "MEM", "-t_srs", "EPSG:4326", "-te", coordinate[0], coordinate[1],
        coordinate[2], coordinate[3], "-ts", dimension, dimension, "-r", "average", "-srcalpha", "-dstalpha",
        "-wm", "32", "-wo", "NUM_THREADS=1", NULL};
    GDALWarpAppOptions *options = ok && step == SQLITE_STEP_DONE && count ? GDALWarpAppOptionsNew(args, NULL) : NULL;
    int usage = 0;
    GDALDatasetH result = options ? GDALWarp("", NULL, count, sources, options, &usage) : NULL;
    GDALWarpAppOptionsFree(options);
    for (unsigned i = 0; i < count; ++i) GDALClose(sources[i]);
    *success = ok && step == SQLITE_STEP_DONE && (!count || (result && !usage && GDALGetRasterCount(result) == 4));
    if (result && !*success) { GDALClose(result); result = NULL; }
    return result;
}

static bool replace_image(const string_t *directory, size_t slot, GDALDatasetH image, json_t *asset)
{
    string_t *path = string_sprintf("%s/regional/%02zu.png", string_c_str(directory), slot);
    string_t *part = path ? string_sprintf("%s.part", string_c_str(path)) : NULL;
    file_t *target = path ? file_new(path) : NULL, *temporary = part ? file_new(part) : NULL;
    char *options[] = {"ZLEVEL=6", NULL};
    GDALDatasetH png = temporary ? GDALCreateCopy(GDALGetDriverByName("PNG"), string_c_str(part), image,
                                                 false, options, NULL, NULL) : NULL;
    bool ok = png != NULL;
    GDALClose(png);
    file_info_t *info = ok ? file_get_info(temporary) : NULL;
    string_t *sha = info ? file_sha256(temporary) : NULL;
    json_t *digest = sha ? json_new_string(sha) : NULL;
    ok = digest && set_size(asset, "bytes", file_info_size(info)) && set(asset, "sha256", digest) &&
        file_move(temporary, target, true);
    if (!ok && temporary && file_exists(temporary)) file_delete(temporary);
    string_free(sha); json_free(digest); file_info_free(info); file_free(temporary); file_free(target);
    string_free(part); string_free(path);
    return ok;
}

static bool repair_image(const string_t *directory, size_t slot, const json_t *regions, json_t *assets,
                         sqlite_t *index, json_t *region, json_t *asset, bool *changed)
{
    double b[4], edge_value = number(field(asset, "width"));
    bool ok = bounds(region, b) && (edge_value == 1800 || edge_value == 3600) &&
        number(field(asset, "height")) == edge_value;
    unsigned edge = ok ? (unsigned)edge_value : 0;
    GDALDatasetH input = ok ? open_image(directory, slot, edge) : NULL;
    if (!input) fprintf(stderr, "Cannot open overview %zu (declared edge %.0f).\n", slot, edge_value);
    /* An RGB overview is opaque by definition and contains no observation holes. */
    if (input && GDALGetRasterCount(input) == 3) { GDALClose(input); return true; }
    size_t bytes = (size_t)edge * edge * 4;
    unsigned char *pixels = input ? malloc(bytes) : NULL;
    int bands[] = {1, 2, 3, 4};
    ok = pixels && GDALDatasetRasterIO(input, GF_Read, 0, 0, edge, edge, pixels,
                                      edge, edge, GDT_Byte, 4, bands, 4, edge * 4, 1) == CE_None;
    GDALClose(input);
    size_t missing = 0, filled = 0;
    for (size_t i = 3; ok && i < bytes; i += 4) missing += !pixels[i];
    GDALDatasetH detail = ok && missing ? detail_image(directory, regions, assets, index, b, edge, &ok) : NULL;
    if (!ok) fprintf(stderr, "Cannot prepare detail for overview %zu.\n", slot);
    unsigned char *row = detail ? malloc((size_t)edge * 4) : NULL;
    if (detail && !row) ok = false;
    for (unsigned y = 0; ok && detail && y < edge; ++y) {
        ok = GDALDatasetRasterIO(detail, GF_Read, 0, y, edge, 1, row,
                                edge, 1, GDT_Byte, 4, bands, 4, edge * 4, 1) == CE_None;
        for (unsigned x = 0; ok && x < edge; ++x) {
            unsigned char *destination = pixels + ((size_t)y * edge + x) * 4;
            const unsigned char *source = row + x * 4;
            if (!destination[3] && source[3]) { memcpy(destination, source, 4); ++filled; }
        }
    }
    GDALClose(detail); free(row);
    GDALDatasetH memory = ok && filled ? GDALCreate(GDALGetDriverByName("MEM"), "", edge, edge, 4, GDT_Byte, NULL) : NULL;
    if (ok && filled) {
        ok = memory && GDALDatasetRasterIO(memory, GF_Write, 0, 0, edge, edge, pixels,
                                           edge, edge, GDT_Byte, 4, bands, 4, edge * 4, 1) == CE_None;
        for (unsigned i = 0; ok && i < 4; ++i)
            ok = GDALSetRasterColorInterpretation(GDALGetRasterBand(memory, i + 1),
                                                  (GDALColorInterp)(GCI_RedBand + i)) == CE_None;
        json_t *provenance = json_new_object();
        string_t *description = string_new_with("Transparent overview pixels filled from indexed packaged detail; "
                                                "area resampling; existing observations unchanged.");
        json_t *method = description ? json_new_string(description) : NULL;
        ok = ok && provenance && method && set_size(provenance, "version", 1) &&
            set_size(provenance, "filled_pixels", filled) && set(provenance, "method", method) &&
            set(region, "overview_gap_fill", provenance) && set_size(region, "valid_pixels", bytes / 4 - missing + filled) &&
            replace_image(directory, slot, memory, asset);
        json_free(provenance); json_free(method); string_free(description);
        if (ok) { *changed = true; printf("Overview %zu: filled %zu missing pixels\n", slot, filled); fflush(stdout); }
    }
    GDALClose(memory); free(pixels);
    return ok;
}

/* Reconstruct observations in a disposable package; callers publish its manifest only after success. */
bool satellite_overviews_repair(const string_t *directory, json_t *manifest, const double window[4], size_t *changed)
{
    if (changed) *changed = 0;
    if (!directory || !manifest || !changed) return false;
    if (window && (!isfinite(window[0]) || !isfinite(window[1]) || !isfinite(window[2]) || !isfinite(window[3]) ||
        window[0] < -180 || window[2] > 180 || window[1] < -90 || window[3] > 90 ||
        window[0] >= window[2] || window[1] >= window[3])) return false;
    const json_t *regions = field(manifest, "regions");
    size_t count = json_array_size(regions);
    json_t *next_regions = json_new_array(), *assets = json_clone(field(manifest, "assets"));
    string_t *memory = string_new_with(":memory:");
    sqlite_t *index = memory ? sqlite_open_plain(memory) : NULL;
    bool ok = json_type(regions) == JSON_ARRAY && count <= GLOBE_TEXTURE_REGION_LIMIT &&
        next_regions && json_type(assets) == JSON_OBJECT && index && index_regions(index, regions);
    if (!ok) fputs("Cannot index regional imagery for overview repair.\n", stderr);
    GDALSetCacheMax64(32 * 1024 * 1024);
    for (size_t slot = 0; ok && slot < count; ++slot) {
        json_t *region = json_clone(json_array_get(regions, slot));
        string_t *relative = string_sprintf("regional/%02zu.png", slot);
        json_t *asset = relative ? json_clone(json_object_get(assets, relative)) : NULL;
        double b[4];
        bool repaired = false;
        ok = region && asset && bounds(region, b);
        bool selected = ok && (!window || (b[0] < window[2] && b[2] > window[0] && b[1] < window[3] && b[3] > window[1]));
        if (selected && overview(b)) ok = repair_image(directory, slot, regions, assets, index, region, asset, &repaired);
        if (!ok) fprintf(stderr, "Overview preparation failed at regional slot %zu.\n", slot);
        ok = ok && json_array_append(next_regions, region) && json_object_set(assets, relative, asset);
        if (repaired) ++*changed;
        json_free(region); json_free(asset); string_free(relative);
    }
    if (ok) ok = set(manifest, "regions", next_regions) && set(manifest, "assets", assets);
    if (ok && field(manifest, "coastal_mask")) {
        uint64_t bytes = 0;
        for (size_t slot = 0; slot < count; ++slot) {
            string_t *relative = string_sprintf("regional/%02zu.png", slot);
            bytes += (uint64_t)number(field(json_object_get(assets, relative), "bytes"));
            string_free(relative);
        }
        json_t *mask = json_clone(field(manifest, "coastal_mask"));
        ok = mask && set_size(mask, "masked_bytes", bytes) && set(manifest, "coastal_mask", mask);
        json_free(mask);
    }
    sqlite_close(index); string_free(memory); json_free(next_regions); json_free(assets);
    return ok;
}
