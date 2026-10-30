/**
 * @file satellite_tiles.c
 * @brief Offline satellite coverage planning and lossless raster preparation.
 *
 * Uses the GDAL C geometry API and MARS JSON/file facilities to select source mosaics
 * and overlapping display tiles. This optional maintainer tool has no runtime role.
 * Existing packaged regions are reused by exact bounds when planning. Rendering reprojects
 * source rasters directly to PNG with transparent missing observations and verified checkpoints.
 * An in-memory R-tree restricts each output to intersecting local or remote raster windows. Optional
 * coastal masking clears offshore pixels before checkpoint publication, preserving source colours and lakes.
 * No imagery is installed here.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sodium.h>
#include <sys/statvfs.h>
#include <gdal_utils.h>
#include <cpl_string.h>
#include <cpl_conv.h>
#include <gdal.h>
#include <ogr_api.h>

#include "array-t.h"
#include "file-t.h"
#include "json-t.h"
#include "sqlite-t.h"
#include "string-t.h"
#include "satellite_mask.h"

static const json_t *field(const json_t *object, const char *name)
{
    string_t *key = string_new_with(name);
    const json_t *value = json_object_get(object, key);
    string_free(key);
    return value;
}

static double number(const json_t *value)
{
    const string_t *text = json_number_text(value);
    return text ? strtod(string_c_str(text), NULL) : NAN;
}

static const char *text(const json_t *value)
{
    const string_t *s = json_string_value(value);
    return s ? string_c_str(s) : "";
}

static json_t *load(const char *path)
{
    file_t *file = file_new_cstr(path);
    string_t *data = file_read_all_text(file);
    json_t *value = data ? json_from_text(data) : NULL;
    string_free(data); file_free(file);
    return value;
}

static OGRGeometryH rectangle(double west, double south, double east, double north)
{
    OGRGeometryH ring = OGR_G_CreateGeometry(wkbLinearRing), polygon = OGR_G_CreateGeometry(wkbPolygon);
    OGR_G_AddPoint_2D(ring, west, south); OGR_G_AddPoint_2D(ring, east, south);
    OGR_G_AddPoint_2D(ring, east, north); OGR_G_AddPoint_2D(ring, west, north);
    OGR_G_AddPoint_2D(ring, west, south); OGR_G_AddGeometryDirectly(polygon, ring);
    return polygon;
}

static bool existing(const json_t *regions, const double bounds[4])
{
    /* Existing packaged catalogues are bounded; this one-off comparison never runs in the application. */
    for (size_t i = 0; i < json_array_size(regions); ++i) {
        const json_t *b = field(json_array_get(regions, i), "bounds");
        bool same = json_array_size(b) == 4;
        for (unsigned j = 0; same && j < 4; ++j) same = fabs(number(json_array_get(b, j)) - bounds[j]) < 1e-8;
        if (same) return true;
    }
    return false;
}

static int plan_main(int argc, char **argv)
{
    if (argc != 5 && argc != 6) {
        fprintf(stderr, "Usage: %s catalogue.json countries.geojson existing-manifest.json plan.json [region.json]\n"
                        "       %s --render plan.json source-directory|--remote output-directory [shoreline.db]\n",
                        argv[0], argv[0]);
        return 2;
    }
    GDALAllRegister();
    json_t *catalogue = load(argv[1]), *manifest = load(argv[3]);
    json_t *selection = argc == 6 ? load(argv[5]) : NULL;
    const json_t *selected_width = field(selection, "width");
    double tile_width = selected_width ? number(selected_width) : 3600;
    if (tile_width != 1800 && tile_width != 3600) return 1;
    const char *country_name = selection ? text(field(selection, "country")) : "United Kingdom";
    if (argc == 6 && !selection) return 1;
    for (const char *p = country_name; *p; ++p)
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || *p == ' ')) return 1;
    string_t *filter = string_sprintf("ADMIN = '%s'", country_name);
    GDALDatasetH outlines = GDALOpenEx(argv[2], GDAL_OF_VECTOR | GDAL_OF_READONLY, NULL, NULL, NULL);
    OGRLayerH layer = outlines ? GDALDatasetGetLayer(outlines, 0) : NULL;
    if (!catalogue || !manifest || !layer || !filter ||
        OGR_L_SetAttributeFilter(layer, string_c_str(filter)) != OGRERR_NONE)
        return 1;
    string_free(filter);
    OGRFeatureH feature = OGR_L_GetNextFeature(layer);
    OGRGeometryH selected = feature ? OGR_G_Clone(OGR_F_GetGeometryRef(feature)) : NULL;
    if (!selected) return 1;
    if (!strcmp(text(field(selection, "components")), "islands")) {
        /* Country outlines contain a bounded list of polygon components; omit the mainland component. */
        int count = OGR_G_GetGeometryCount(selected), largest = -1;
        double area = 0;
        for (int i = 0; i < count; ++i) {
            double candidate = OGR_G_Area(OGR_G_GetGeometryRef(selected, i));
            if (candidate > area) { area = candidate; largest = i; }
        }
        if (largest < 0 || OGR_G_RemoveGeometry(selected, largest, true) != OGRERR_NONE) return 1;
    }
    const json_t *clip = field(selection, "bounds");
    if (json_array_size(clip) == 4) {
        double b[4];
        for (unsigned i = 0; i < 4; ++i) b[i] = number(json_array_get(clip, i));
        OGRGeometryH box = rectangle(b[0], b[1], b[2], b[3]);
        OGRGeometryH clipped = !strcmp(text(field(selection, "footprint")), "bounds") ?
            OGR_G_Clone(box) : OGR_G_Intersection(selected, box);
        OGR_G_DestroyGeometry(box); OGR_G_DestroyGeometry(selected);
        selected = clipped;
    }
    OGRGeometryH country = selected ? OGR_G_Buffer(selected, .03, 8) : NULL;
    OGR_G_DestroyGeometry(selected);
    if (!country || OGR_G_IsEmpty(country)) {
        fprintf(stderr, "The requested footprint has no coverage in the supplied outline.\n");
        return 1;
    }
    OGRGeometryH coast = NULL;
    if (!strcmp(text(field(selection, "coverage")), "coastal")) {
        OGRGeometryH boundary = OGR_G_Boundary(country);
        coast = boundary ? OGR_G_Buffer(boundary, .06, 8) : NULL;
        OGR_G_DestroyGeometry(boundary);
        if (!coast) return 1;
    }
    OGREnvelope envelope;
    OGR_G_GetEnvelope(country, &envelope);
    string_t *plan = string_new_with("{\"sources\":["), *tiles = string_new_with("],\"tiles\":[");
    const json_t *features = field(catalogue, "features"), *regions = field(manifest, "regions");
    size_t source_count = 0, tile_count = 0, base = json_array_size(regions);
    double bytes = 0;
    for (size_t i = 0; i < json_array_size(features); ++i) {
        const json_t *item = json_array_get(features, i), *bbox = field(item, "bbox");
        if (json_array_size(bbox) != 4) return 1;
        double b[4];
        for (unsigned j = 0; j < 4; ++j) b[j] = number(json_array_get(bbox, j));
        OGRGeometryH box = rectangle(b[0], b[1], b[2], b[3]);
        bool intersects = OGR_G_Intersects(country, box);
        OGR_G_DestroyGeometry(box);
        if (!intersects) continue;
        const char *id = text(field(item, "id"));
        unsigned year;
        char quarter[3], tile[24];
        if (sscanf(id, "Sentinel-2_mosaic_%u_%2s_%23s", &year, quarter, tile) != 3) return 1;
        for (const char *p = id; *p; ++p)
            if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                  (*p >= '0' && *p <= '9') || *p == '_' || *p == '-')) return 1;
        const json_t *assets = field(item, "assets");
        for (unsigned band = 2; band <= 4; ++band) {
            char name[4]; snprintf(name, sizeof name, "B%02u", band);
            double size = number(field(field(assets, name), "file:size"));
            if (!isfinite(size) || size < 1) return 1;
            bytes += size;
            string_append_format(plan, "%s{\"id\":\"%s\",\"band\":\"%s\",\"file\":\"%s-%s.tif\","
                "\"url\":\"https://data.source.coop/tge-labs/sentinel-2-quarterly-cloudless-mosaics/%u/%s/%s/%s.tif\","
                "\"bytes\":%.0f,\"bounds\":[%.12f,%.12f,%.12f,%.12f]}", source_count++ ? "," : "",
                id, name, id, name, year, quarter, tile, name, size, b[0], b[1], b[2], b[3]);
        }
    }
    for (unsigned detail = 0; detail < 2; ++detail) {
        double dx = detail ? .48 : 2.88, dy = detail ? .32 : 2.24;
        double width = detail ? .5 : 2.9, height = detail ? 1.0 / 3 : 2.253333333333;
        int left = (int)floor((envelope.MinX + 5.51) / dx), right = (int)floor((envelope.MaxX + 5.51) / dx);
        int bottom = (int)floor((envelope.MinY - 51.24) / dy), top = (int)floor((envelope.MaxY - 51.24) / dy);
        for (int row = bottom; row <= top; ++row) for (int column = left; column <= right; ++column) {
            double west = -5.51 + column * dx, south = 51.24 + row * dy;
            double b[] = {west, south, west + width, south + height};
            OGRGeometryH box = rectangle(b[0], b[1], b[2], b[3]);
            bool intersects = OGR_G_Intersects(detail && coast ? coast : country, box);
            OGR_G_DestroyGeometry(box);
            if (!intersects || existing(regions, b)) continue;
            string_append_format(tiles, "%s{\"slot\":%zu,\"detail\":%s,\"width\":%.0f,"
                "\"bounds\":[%.12f,%.12f,%.12f,%.12f]}", tile_count ? "," : "", base + tile_count,
                detail ? "true" : "false", tile_width, b[0], b[1], b[2], b[3]);
            ++tile_count;
        }
    }
    string_append_format(plan, "%s", string_c_str(tiles));
    string_append_format(plan, "],\"download_bytes\":%.0f,\"existing_regions\":%zu}\n", bytes, base);
    file_t *output = file_new_cstr(argv[4]);
    bool ok = file_write_all_text(output, plan);
    printf("%zu source bands, %.2f GiB in source rasters; %zu new regions plus %zu existing regions\n",
           source_count, bytes / 1073741824., tile_count, base);
    file_free(output); string_free(plan); string_free(tiles); json_free(catalogue); json_free(manifest);
    OGR_G_DestroyGeometry(coast); OGR_G_DestroyGeometry(country);
    OGR_F_Destroy(feature); GDALClose(outlines); json_free(selection);
    return ok ? 0 : 1;
}

typedef struct _satellite_source_t {
    double bounds[4];
    const json_t *bands[3];
} satellite_source_t;

static bool overlaps(const double a[4], const double b[4])
{
    return a[0] < b[2] && a[2] > b[0] && a[1] < b[3] && a[3] > b[1];
}

static bool checkpoint(const char *directory, unsigned slot, const double bounds[4], size_t valid, unsigned edge,
                       const char *shoreline)
{
    string_t *path = string_sprintf("%s/%02u.png", directory, slot);
    file_t *file = file_new(path);
    array_t *bytes = file_read_all_bytes(file);
    size_t size = array_size(bytes);
    unsigned char digest[crypto_hash_sha256_BYTES];
    char hex[65];
    bool ok = size && crypto_hash_sha256(digest, array_get(bytes, 0), size) == 0 &&
              sodium_bin2hex(hex, sizeof hex, digest, sizeof digest);
    string_t *description = ok ? string_sprintf(
        "{\"slot\":%u,\"path\":\"regional/%02u.png\",\"asset\":{\"width\":%u,\"height\":%u,"
        "\"bytes\":%zu,\"sha256\":\"%s\"},\"valid_pixels\":%zu,\"region\":{"
        "\"name\":\"Sentinel tile %u\",\"bounds\":[%.12f,%.12f,%.12f,%.12f],"
        "\"source_url\":\"https://data.source.coop/tge-labs/sentinel-2-quarterly-cloudless-mosaics/2025/Q3/\","
        "\"gap_fill_source_url\":\"https://data.source.coop/tge-labs/sentinel-2-quarterly-cloudless-mosaics/2025/Q2/\","
        "\"attribution\":\"Contains modified Copernicus Sentinel data 2025. ESA/CDSE quarterly mosaics; "
        "Taylor Geospatial Engine mirror. RGB B04/B03/B02, 10-metre source resolution.\","
        "\"processing\":\"Q3 valid observations over Q2; WGS84 bilinear reprojection; overlapping %u-square "
        "tiles. DN 0..3000 scaled to 0..255 with exponent 0.7. Lossless RGBA PNG directly from source rasters; "
        "missing observations transparent.\"}}\n", slot, slot, edge, edge, size, hex, valid, slot,
        bounds[0], bounds[1], bounds[2], bounds[3], edge) : NULL;
    string_t *side = string_sprintf("%s/%02u.json", directory, slot);
    file_t *output = file_new(side);
    if (description && shoreline) {
        string_t *key = string_new_with("coastal_mask"), *identity = string_new_with(shoreline);
        json_t *record = json_from_text(description), *value = identity ? json_new_string(identity) : NULL;
        ok = ok && record && key && value && json_object_set(record, key, value);
        json_t *region = json_clone(field(record, "region"));
        string_t *processing = string_sprintf("%s Offshore pixels cleared using the packaged coastline; "
            "inland waters retained; coastline alpha uses two-by-two coverage samples.", text(field(region, "processing")));
        string_t *processing_key = string_new_with("processing"), *region_key = string_new_with("region");
        json_t *processing_value = processing ? json_new_string(processing) : NULL;
        ok = ok && region && processing_key && processing_value && region_key &&
             json_object_set(region, processing_key, processing_value) && json_object_set(record, region_key, region);
        json_free(region); json_free(processing_value); string_free(processing); string_free(processing_key);
        string_free(region_key);
        string_free(description);
        description = ok ? json_to_string(record) : NULL;
        json_free(value); json_free(record); string_free(key); string_free(identity);
    }
    ok = ok && description && file_write_all_text(output, description);
    file_free(output); string_free(side); string_free(description); array_destroy(bytes); file_free(file); string_free(path);
    return ok;
}

static GDALDatasetH source_open(const satellite_source_t *source, const char *directory)
{
    string_t *paths[3] = {0};
    const char *names[3] = {0};
    bool remote = !strcmp(directory, "--remote"), ok = true;
    for (unsigned band = 0; ok && band < 3; ++band) {
        const char *name = text(field(source->bands[band], remote ? "url" : "file"));
        static const char prefix[] = "https://data.source.coop/tge-labs/sentinel-2-quarterly-cloudless-mosaics/";
        ok = !remote || !strncmp(name, prefix, sizeof prefix - 1);
        paths[band] = ok ? (remote ? string_sprintf("/vsicurl/%s", name) :
                                    string_sprintf("%s/%s", directory, name)) : NULL;
        names[band] = string_c_str(paths[band]);
        ok = ok && paths[band];
    }
    char *arguments[] = {"-separate", "-srcnodata", "-32768", "-vrtnodata", "-32768", NULL};
    GDALBuildVRTOptions *options = ok ? GDALBuildVRTOptionsNew(arguments, NULL) : NULL;
    int usage = 0;
    GDALDatasetH rgb = options ? GDALBuildVRT("", 3, NULL, names, options, &usage) : NULL;
    if (rgb && (usage || GDALGetRasterCount(rgb) != 3)) { GDALClose(rgb); rgb = NULL; }
    if (rgb && remote && GDALGetOverviewCount(GDALGetRasterBand(rgb, 1)) < 5) {
        const int factors[] = {2, 4, 8, 16, 32};
        /* Virtual overviews let the warper read the existing COG pyramid at the output scale. */
        if (GDALBuildOverviews(rgb, "NEAREST", 5, factors, 0, NULL, NULL, NULL) != CE_None) {
            GDALClose(rgb); rgb = NULL;
        }
    }
    GDALBuildVRTOptionsFree(options);
    for (unsigned band = 0; band < 3; ++band) string_free(paths[band]);
    return rgb;
}

static bool render_tile(satellite_source_t *sources, size_t count, sqlite_t *index, const char *input_directory,
                        const double bounds[4], unsigned slot, const char *directory, unsigned edge,
                        globe_t *globe, const char *shoreline)
{
    string_t *side = string_sprintf("%s/%02u.json", directory, slot);
    json_t *saved = load(string_c_str(side));
    string_t *saved_path = string_sprintf("%s/%02u.png", directory, slot);
    file_t *saved_file = file_new(saved_path);
    array_t *saved_bytes = saved ? file_read_all_bytes(saved_file) : NULL;
    size_t saved_size = array_size(saved_bytes);
    unsigned char digest[crypto_hash_sha256_BYTES];
    char hex[65];
    bool done = number(field(field(saved, "asset"), "width")) == edge &&
        number(field(field(saved, "asset"), "height")) == edge && saved_size &&
        saved_size == number(field(field(saved, "asset"), "bytes")) &&
        crypto_hash_sha256(digest, array_get(saved_bytes, 0), saved_size) == 0 &&
        sodium_bin2hex(hex, sizeof hex, digest, sizeof digest) &&
        !strcmp(hex, text(field(field(saved, "asset"), "sha256")));
    done = done && (!shoreline || !strcmp(text(field(saved, "coastal_mask")), shoreline));
    const json_t *saved_bounds = field(field(saved, "region"), "bounds");
    for (unsigned i = 0; done && i < 4; ++i)
        done = fabs(number(json_array_get(saved_bounds, i)) - bounds[i]) < 1e-9;
    array_destroy(saved_bytes); file_free(saved_file); string_free(saved_path);
    json_free(saved); string_free(side);
    if (done) { printf("Reusing completed tile %u\n", slot); fflush(stdout); return true; }
    printf("Tile %u: preparing indexed source crops\n", slot); fflush(stdout);
    GDALDatasetH selected[128] = {0};
    unsigned used = 0;
    sqlite_stmt_t *query = sqlite_stmt_prepare(index,
        "SELECT id FROM footprints WHERE west<?1 AND east>?2 AND south<?3 AND north>?4 ORDER BY id");
    bool ok = query && sqlite_stmt_bind_double(query, 1, bounds[2]) && sqlite_stmt_bind_double(query, 2, bounds[0]) &&
        sqlite_stmt_bind_double(query, 3, bounds[3]) && sqlite_stmt_bind_double(query, 4, bounds[1]);
    sqlite_step_result_t step = SQLITE_STEP_DONE;
    while (ok && (step = sqlite_stmt_step(query)) == SQLITE_STEP_ROW) {
        int id = sqlite_stmt_column_int(query, 0);
        ok = id >= 0 && (size_t)id < count;
        if (ok && overlaps(sources[id].bounds, bounds)) {
            ok = used < 128;
            if (ok) { selected[used] = source_open(sources + id, input_directory); ok = selected[used++] != NULL; }
        }
    }
    ok = ok && step == SQLITE_STEP_DONE && (used || globe);
    sqlite_stmt_finalize(query);
    char coordinate[4][40], dimension[8];
    snprintf(dimension, sizeof dimension, "%u", edge);
    for (unsigned i = 0; i < 4; ++i) snprintf(coordinate[i], sizeof coordinate[i], "%.12f", bounds[i]);
    char *arguments[] = {"-of", "MEM", "-t_srs", "EPSG:4326", "-te", coordinate[0], coordinate[1],
        coordinate[2], coordinate[3], "-ts", dimension, dimension, "-r", "bilinear", "-srcnodata", "-32768",
        "-dstnodata", "-32768", "-dstalpha", "-wm", "64", "-wo", "NUM_THREADS=1", NULL};
    GDALWarpAppOptions *options = ok && used ? GDALWarpAppOptionsNew(arguments, NULL) : NULL;
    int usage = 0;
    GDALDatasetH warped = options ? GDALWarp("", NULL, used, selected, options, &usage) : NULL;
    GDALWarpAppOptionsFree(options);
    for (unsigned i = 0; i < used; ++i) GDALClose(selected[i]);
    if (ok && !used && globe) {
        /* A missing source footprint remains transparent, allowing the global photograph to show through. */
        warped = GDALCreate(GDALGetDriverByName("MEM"), "", edge, edge, 4, GDT_Int16, NULL);
        bool filled = warped != NULL;
        for (unsigned band = 1; filled && band <= 4; ++band)
            filled = GDALFillRaster(GDALGetRasterBand(warped, band), band == 4 ? 0 : -32768, 0) == CE_None;
        if (!filled) { GDALClose(warped); warped = NULL; }
    }
    if (!warped || usage || GDALGetRasterCount(warped) != 4) { GDALClose(warped); return false; }
    const size_t pixels = (size_t)edge * edge;
    int16_t *input = malloc(pixels * 4 * sizeof *input);
    unsigned char *output = malloc(pixels * 4), curve[32768];
    for (unsigned i = 0; i < 32768; ++i) curve[i] = (unsigned char)lrint(255 * pow(fmin(1, i / 3000.), .7));
    int bands[] = {1, 2, 3, 4};
    ok = input && output && GDALDatasetRasterIO(warped, GF_Read, 0, 0, edge, edge,
        input, edge, edge, GDT_Int16, 4, bands, 8, edge * 8, 2) == CE_None;
    size_t valid = 0;
    for (size_t i = 0; ok && i < pixels; ++i) {
        bool seen = input[i * 4 + 3] > 0 && input[i * 4] != -32768 && input[i * 4 + 1] != -32768 && input[i * 4 + 2] != -32768;
        for (unsigned channel = 0; channel < 3; ++channel) {
            int value = input[i * 4 + channel];
            output[i * 4 + channel] = seen && value > 0 ? curve[value] : 0;
        }
        output[i * 4 + 3] = seen ? 255 : 0;
        valid += seen;
    }
    free(input); GDALClose(warped);
    GDALDatasetH image = ok && (valid || globe) ?
        GDALCreate(GDALGetDriverByName("MEM"), "", edge, edge, 4, GDT_Byte, NULL) : NULL;
    ok = image && GDALDatasetRasterIO(image, GF_Write, 0, 0, edge, edge, output, edge, edge,
                                     GDT_Byte, 4, bands, 4, edge * 4, 1) == CE_None;
    free(output);
    if (ok) {
        GDALSetRasterColorInterpretation(GDALGetRasterBand(image, 1), GCI_RedBand);
        GDALSetRasterColorInterpretation(GDALGetRasterBand(image, 2), GCI_GreenBand);
        GDALSetRasterColorInterpretation(GDALGetRasterBand(image, 3), GCI_BlueBand);
        GDALSetRasterColorInterpretation(GDALGetRasterBand(image, 4), GCI_AlphaBand);
    }
    string_t *temporary = string_sprintf("%s/%02u.part.png", directory, slot);
    string_t *path = string_sprintf("%s/%02u.png", directory, slot);
    char *creation[] = {"ZLEVEL=6", NULL};
    GDALDatasetH png = ok ? GDALCreateCopy(GDALGetDriverByName("PNG"), string_c_str(temporary), image, false,
                                          creation, NULL, NULL) : NULL;
    ok = png != NULL;
    GDALClose(png); GDALClose(image);
    file_t *from = file_new(temporary), *to = file_new(path);
    if (ok && globe) {
        string_t *masked_path = string_sprintf("%s/%02u.coast.png", directory, slot);
        file_t *masked = masked_path ? file_new(masked_path) : NULL;
        if (masked && file_exists(masked)) ok = file_delete(masked);
        satellite_mask_stats_t stats;
        ok = ok && masked && satellite_mask_png(globe, temporary, masked_path, bounds, valid, &stats) &&
             file_move(masked, to, true);
        if (ok) valid = stats.visible_pixels;
        file_delete(from); file_free(masked); string_free(masked_path);
    } else ok = ok && file_move(from, to, true);
    ok = ok && checkpoint(directory, slot, bounds, valid, edge, shoreline);
    file_free(from); file_free(to); string_free(temporary); string_free(path);
    printf("Tile %u: %.2f%% valid source observations — %s\n", slot, 100. * valid / pixels, ok ? "complete" : "failed");
    fflush(stdout);
    return ok;
}

static int render_main(const char *plan_path, const char *directory, const char *output, const char *coast)
{
    CPLSetConfigOption("GDAL_MAX_DATASET_POOL_SIZE", "8");
    CPLSetConfigOption("GDAL_DISABLE_READDIR_ON_OPEN", "EMPTY_DIR");
    CPLSetConfigOption("CPL_VSIL_CURL_ALLOWED_EXTENSIONS", ".tif");
    CPLSetConfigOption("CPL_VSIL_CURL_CACHE_SIZE", "67108864");
    CPLSetConfigOption("GDAL_HTTP_TIMEOUT", "60");
    CPLSetConfigOption("GDAL_HTTP_MAX_RETRY", "5");
    CPLSetConfigOption("GDAL_HTTP_RETRY_DELAY", "2");
    CPLSetConfigOption("VRT_VIRTUAL_OVERVIEWS", "YES");
    GDALAllRegister(); GDALSetCacheMax64(64 * 1024 * 1024);
    json_t *plan = load(plan_path);
    const json_t *files = field(plan, "sources"), *tiles = field(plan, "tiles");
    size_t count = json_array_size(files) / 3;
    if (!count || count > 4096 || json_array_size(files) % 3) return 1;
    satellite_source_t *sources = calloc(count, sizeof *sources);
    string_t *memory = string_new_with(":memory:");
    sqlite_t *index = memory ? sqlite_open_plain(memory) : NULL;
    string_free(memory);
    bool ok = sources && index && sqlite_exec_cstr(index,
        "CREATE VIRTUAL TABLE footprints USING rtree(id,west,east,south,north)");
    sqlite_stmt_t *insert = ok ? sqlite_stmt_prepare(index, "INSERT INTO footprints VALUES(?1,?2,?3,?4,?5)") : NULL;
    globe_reader_t *reader = NULL;
    string_t *identity = NULL;
    globe_t *globe = NULL;
    if (ok && coast) {
        ok = !setenv("MARS_GLOBE_DB_PATH", coast, 1) && !setenv("MARS_GLOBE_DB_KEY", "", 1);
        reader = ok ? globe_reader_new() : NULL;
        identity = reader ? globe_reader_identity(reader) : NULL;
        globe = identity ? globe_reader_get(reader) : NULL;
        ok = globe != NULL;
    }
    for (size_t i = 0; ok && i < count; ++i) {
        for (unsigned band = 0; band < 3; ++band) sources[i].bands[band] = json_array_get(files, i * 3 + 2 - band);
        const json_t *bounds = field(json_array_get(files, i * 3), "bounds");
        for (unsigned j = 0; j < 4; ++j) sources[i].bounds[j] = number(json_array_get(bounds, j));
        sqlite_stmt_reset(insert);
        ok = sqlite_stmt_bind_int(insert, 1, (int)i) && sqlite_stmt_bind_double(insert, 2, sources[i].bounds[0]) &&
             sqlite_stmt_bind_double(insert, 3, sources[i].bounds[2]) &&
             sqlite_stmt_bind_double(insert, 4, sources[i].bounds[1]) &&
             sqlite_stmt_bind_double(insert, 5, sources[i].bounds[3]) && sqlite_stmt_step(insert) == SQLITE_STEP_DONE;
    }
    sqlite_stmt_finalize(insert);
    for (size_t i = 0; ok && i < json_array_size(tiles); ++i) {
        struct statvfs space;
        ok = statvfs(output, &space) == 0 && (double)space.f_bavail * space.f_frsize >= 8.0 * 1073741824;
        if (!ok) { fputs("Stopped safely: retain at least 8 GiB free; completed tiles can be resumed.\n", stderr); break; }
        const json_t *tile = json_array_get(tiles, i), *box = field(tile, "bounds");
        double bounds[4];
        for (unsigned j = 0; j < 4; ++j) bounds[j] = number(json_array_get(box, j));
        const json_t *width = field(tile, "width");
        double edge = width ? number(width) : 3600;
        ok = (edge == 1800 || edge == 3600) &&
             render_tile(sources, count, index, directory, bounds, (unsigned)number(field(tile, "slot")), output,
                         (unsigned)edge, globe, identity ? string_c_str(identity) : NULL);
        printf("Tiles completed: %zu/%zu\n", ok ? i + 1 : i, json_array_size(tiles)); fflush(stdout);
    }
    sqlite_close(index); string_free(identity); globe_reader_free(reader);
    free(sources); json_free(plan);
    return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
    if ((argc == 5 || argc == 6) && !strcmp(argv[1], "--render"))
        return render_main(argv[2], argv[3], argv[4], argc == 6 ? argv[5] : NULL);
    return plan_main(argc, argv);
}
