/**
 * @file satellite_mask.c
 * @brief Lossless offshore-pixel removal using the globe's indexed shoreline polygons.
 *
 * Optional offline preparation shared by the catalogue executable and its tests.
 * Rasterises only outer coastline rings at twice the photograph resolution, then
 * averages coverage into alpha. Inland lakes, source colour and nodata are preserved;
 * fully transparent RGB is cleared so PNG compression can discard ocean detail.
 */
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <gdal.h>
#include <gdal_alg.h>
#include <ogr_api.h>

#include "file-t.h"
#include "satellite_mask.h"

typedef struct _coast_mask_t {
    OGRGeometryH ring;
    OGRGeometryH polygons;
    const double *bounds;
    unsigned edge;
} coast_mask_t;

static bool coast_point(globe_path_event_t event, double longitude, double latitude,
                        unsigned level, bool antarctic, void *context)
{
    (void)antarctic;
    coast_mask_t *mask = context;
    if (level != 1) return true;
    if (event == GLOBE_PATH_MOVE) {
        if (mask->ring) return false;
        mask->ring = OGR_G_CreateGeometry(wkbLinearRing);
    }
    if (!mask->ring) return false;
    if (event != GLOBE_PATH_CLOSE) {
        double x = (longitude - mask->bounds[0]) * mask->edge / (mask->bounds[2] - mask->bounds[0]);
        double y = (mask->bounds[3] - latitude) * mask->edge / (mask->bounds[3] - mask->bounds[1]);
        OGR_G_AddPoint_2D(mask->ring, x, y);
        return true;
    }
    OGR_G_CloseRings(mask->ring);
    OGRGeometryH polygon = OGR_G_CreateGeometry(wkbPolygon);
    bool ok = polygon && OGR_G_AddGeometryDirectly(polygon, mask->ring) == OGRERR_NONE;
    if (ok) mask->ring = NULL;
    if (ok) ok = OGR_G_AddGeometryDirectly(mask->polygons, polygon) == OGRERR_NONE;
    if (!ok) OGR_G_DestroyGeometry(polygon);
    return ok;
}

static bool merge_pixels(GDALDatasetH input, GDALDatasetH mask, unsigned edge,
                         unsigned char *pixels, satellite_mask_stats_t *stats)
{
    int channels = GDALGetRasterCount(input), bands[] = {1, 2, 3, 4};
    if ((channels != 3 && channels != 4) || GDALDatasetRasterIO(input, GF_Read, 0, 0, edge, edge,
        pixels, edge, edge, GDT_Byte, channels, bands, 4, edge * 4, 1) != CE_None) return false;
    size_t count = (size_t)edge * edge;
    if (channels == 3) for (size_t i = 0; i < count; ++i) pixels[i * 4 + 3] = 255;
    unsigned char *rows = malloc((size_t)edge * 4);
    if (!rows) return false;
    bool ok = true;
    for (unsigned y = 0; ok && y < edge; ++y) {
        ok = GDALRasterIO(GDALGetRasterBand(mask, 1), GF_Read, 0, y * 2, edge * 2, 2,
                          rows, edge * 2, 2, GDT_Byte, 1, edge * 2) == CE_None;
        for (unsigned x = 0; ok && x < edge; ++x) {
            unsigned coverage = (rows[x * 2] + rows[x * 2 + 1] +
                                 rows[edge * 2 + x * 2] + rows[edge * 2 + x * 2 + 1] + 2) / 4;
            unsigned char *p = pixels + ((size_t)y * edge + x) * 4;
            unsigned alpha = (p[3] * coverage + 127) / 255;
            stats->removed_pixels += p[3] && !alpha;
            stats->changed |= alpha != p[3];
            p[3] = (unsigned char)alpha;
            if (!alpha) {
                stats->changed |= p[0] || p[1] || p[2];
                p[0] = p[1] = p[2] = 0;
            } else ++stats->visible_pixels;
        }
    }
    free(rows);
    return ok;
}

/* Prepare one immutable derivative; ownership of the source and geography remains with the caller. */
bool satellite_mask_png(globe_t *globe, const string_t *source, const string_t *destination,
                        const double bounds[4], size_t source_visible_pixels, satellite_mask_stats_t *stats)
{
    if (stats) *stats = (satellite_mask_stats_t){0};
    if (!globe || !source || !destination || !bounds || !stats) return false;
    for (unsigned i = 0; i < 4; ++i) if (!isfinite(bounds[i])) return false;
    if (bounds[0] < -180 || bounds[2] > 180 || bounds[0] >= bounds[2] ||
        bounds[1] < -90 || bounds[3] > 90 || bounds[1] >= bounds[3]) return false;
    file_t *output = file_new(destination), *original = file_new(source);
    bool vacant = output && !file_exists(output);
    bool ok = vacant && original;
    GDALDatasetH input = ok ? GDALOpen(string_c_str(source), GA_ReadOnly) : NULL;
    int size = input ? GDALGetRasterXSize(input) : 0;
    ok = input && (size == 1800 || size == 3600) && GDALGetRasterYSize(input) == size &&
         GDALGetRasterDataType(GDALGetRasterBand(input, 1)) == GDT_Byte &&
         (GDALGetRasterCount(input) == 3 || GDALGetRasterCount(input) == 4) &&
         !strcmp(GDALGetDriverShortName(GDALGetDatasetDriver(input)), "PNG");
    unsigned edge = ok ? (unsigned)size : 0;
    GDALDatasetH raster = ok ? GDALCreate(GDALGetDriverByName("MEM"), "", edge * 2, edge * 2, 1, GDT_Byte, NULL) : NULL;
    double transform[] = {0, 1, 0, 0, 0, 1};
    if (raster && GDALSetGeoTransform(raster, transform) != CE_None) { GDALClose(raster); raster = NULL; }
    coast_mask_t mask = {.polygons = OGR_G_CreateGeometry(wkbMultiPolygon), .bounds = bounds, .edge = edge * 2};
    bool crossing = true;
    ok = raster && mask.polygons && globe_coastline_window(globe, bounds, &crossing);
    if (ok && !crossing) {
        globe_surface_t surface = globe_hit_test_radius(globe, (bounds[1] + bounds[3]) / 2,
                                                        (bounds[0] + bounds[2]) / 2, 0);
        ok = surface != GLOBE_UNKNOWN && GDALFillRaster(GDALGetRasterBand(raster, 1),
                                                       surface == GLOBE_SEA ? 0 : 255, 0) == CE_None;
    } else if (ok) ok = globe_surface_paths(globe, bounds[0], bounds[1], bounds[2], bounds[3],
        fmin(bounds[2] - bounds[0], bounds[3] - bounds[1]) / (edge * 8), coast_point, &mask);
    if (ok && OGR_G_GetGeometryCount(mask.polygons)) {
        int band = 1;
        double value = 255;
        /* One raster pass for all islands, rather than allocating a tile-sized
         * scan buffer and traversing its rows again for every individual polygon. */
        ok = GDALRasterizeGeometries(raster, 1, &band, 1, &mask.polygons,
                                     NULL, NULL, &value, NULL, NULL, NULL) == CE_None;
    }
    double range[2] = {0};
    ok = ok && GDALComputeRasterMinMax(GDALGetRasterBand(raster, 1), false, range) == CE_None;
    size_t count = (size_t)edge * edge;
    if (ok && GDALGetRasterCount(input) == 3) source_visible_pixels = count;
    if (source_visible_pixels != SIZE_MAX && source_visible_pixels > count) ok = false;
    bool inland = ok && range[0] == 255 && source_visible_pixels != SIZE_MAX;
    bool ocean = ok && range[1] == 0 && source_visible_pixels != SIZE_MAX;
    unsigned char *pixels = ok && !inland ? calloc(count, 4) : NULL;
    if (inland) stats->visible_pixels = source_visible_pixels;
    else if (ocean && pixels) { stats->changed = true; stats->removed_pixels = source_visible_pixels; }
    else ok = pixels && merge_pixels(input, raster, edge, pixels, stats);
    GDALClose(input); GDALClose(raster);
    OGR_G_DestroyGeometry(mask.ring); OGR_G_DestroyGeometry(mask.polygons);
    GDALDatasetH image = NULL, encoded = NULL;
    if (ok && !stats->changed) ok = file_create_hard_link(original, output) ||
                                       (file_last_error(original) == EXDEV && file_copy(original, output, false));
    else if (ok) {
        image = GDALCreate(GDALGetDriverByName("MEM"), "", edge, edge, 4, GDT_Byte, NULL);
        int bands[] = {1, 2, 3, 4};
        ok = image && GDALDatasetRasterIO(image, GF_Write, 0, 0, edge, edge, pixels, edge, edge,
                                          GDT_Byte, 4, bands, 4, edge * 4, 1) == CE_None;
        for (unsigned i = 0; ok && i < 4; ++i)
            ok = GDALSetRasterColorInterpretation(GDALGetRasterBand(image, i + 1),
                                                  (GDALColorInterp)(GCI_RedBand + i)) == CE_None;
        char *options[] = {"ZLEVEL=3", NULL};
        if (ok) encoded = GDALCreateCopy(GDALGetDriverByName("PNG"), string_c_str(destination), image,
                                         false, options, NULL, NULL);
        ok = encoded != NULL;
    }
    GDALClose(encoded); GDALClose(image); free(pixels);
    if (!ok && vacant && file_exists(output)) file_delete(output);
    file_free(output); file_free(original);
    return ok;
}
