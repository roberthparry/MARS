/**
 * @file test_satellite_mask.c
 * @brief Native regressions for lossless coastal transparency preparation.
 *
 * Uses synthetic nested coastline rings and a deterministic RGBA photograph in
 * an isolated directory. Checks sea removal, preserved inland lakes and islands,
 * antialiased edges, unchanged colour, source ownership and input rejection.
 * GDAL is an optional preparation/test dependency, never a Lab runtime dependency.
 */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <gdal.h>

#include "file-t.h"
#include "globe-t.h"
#include "satellite_mask.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

static void integer(unsigned char *p, int32_t value)
{
    uint32_t n = (uint32_t)value;
    p[0] = n >> 24; p[1] = n >> 16; p[2] = n >> 8; p[3] = n;
}

static bool ring(file_t *file, unsigned id, unsigned level, double w, double s, double e, double n)
{
    unsigned char bytes[44 + 1025 * 8] = {0};
    integer(bytes, id); integer(bytes + 4, 1025); integer(bytes + 8, 15 * 256 + level);
    integer(bytes + 12, (int32_t)lrint(w * 1000000)); integer(bytes + 16, (int32_t)lrint(e * 1000000));
    integer(bytes + 20, (int32_t)lrint(s * 1000000)); integer(bytes + 24, (int32_t)lrint(n * 1000000));
    const double x[] = {w, e, e, w, w}, y[] = {s, s, n, n, s};
    for (unsigned i = 0; i < 1025; ++i) {
        unsigned side = i == 1024 ? 3 : i / 256, offset = i == 1024 ? 256 : i % 256;
        double longitude = x[side] + (x[side + 1] - x[side]) * offset / 256;
        double latitude = y[side] + (y[side + 1] - y[side]) * offset / 256;
        integer(bytes + 44 + i * 8, (int32_t)lrint(longitude * 1000000));
        integer(bytes + 48 + i * 8, (int32_t)lrint(latitude * 1000000));
    }
    size_t written = 0;
    return file_write(file, bytes, sizeof bytes, &written) && written == sizeof bytes;
}

static bool photograph(const string_t *path)
{
    const unsigned edge = 1800;
    unsigned char *pixels = malloc((size_t)edge * edge * 4);
    if (!pixels) return false;
    uint32_t random = 12345;
    for (size_t i = 0; i < (size_t)edge * edge; ++i) {
        for (unsigned c = 0; c < 3; ++c) {
            random ^= random << 13; random ^= random >> 17; random ^= random << 5;
            pixels[i * 4 + c] = (unsigned char)random;
        }
        pixels[i * 4 + 3] = 255;
    }
    memset(pixels + ((size_t)100 * edge + 100) * 4, 0, 4);
    GDALDatasetH memory = GDALCreate(GDALGetDriverByName("MEM"), "", edge, edge, 4, GDT_Byte, NULL);
    int bands[] = {1, 2, 3, 4};
    bool ok = memory && GDALDatasetRasterIO(memory, GF_Write, 0, 0, edge, edge, pixels,
        edge, edge, GDT_Byte, 4, bands, 4, edge * 4, 1) == CE_None;
    for (unsigned c = 0; ok && c < 4; ++c)
        ok = GDALSetRasterColorInterpretation(GDALGetRasterBand(memory, c + 1),
                                              (GDALColorInterp)(GCI_RedBand + c)) == CE_None;
    char *options[] = {"ZLEVEL=1", NULL};
    GDALDatasetH png = ok ? GDALCreateCopy(GDALGetDriverByName("PNG"), string_c_str(path), memory,
                                           false, options, NULL, NULL) : NULL;
    ok = png != NULL;
    GDALClose(png); GDALClose(memory); free(pixels);
    return ok;
}

static bool pixel(const string_t *path, unsigned x, unsigned y, unsigned char values[4])
{
    GDALDatasetH image = GDALOpen(string_c_str(path), GA_ReadOnly);
    int bands[] = {1, 2, 3, 4};
    bool ok = image && GDALDatasetRasterIO(image, GF_Read, x, y, 1, 1, values,
                                           1, 1, GDT_Byte, 4, bands, 4, 4, 1) == CE_None;
    GDALClose(image);
    return ok;
}

static void test_satellite_coastal_mask(void)
{
    GDALAllRegister(); GDALSetCacheMax64(32 * 1024 * 1024);
    string_t *parent = string_new_with("build/coast-mask");
    file_t *directory = file_create_temp_directory(parent);
    const char *names[] = {"coast.b", "source.png", "masked.png", "inland.png", "ocean.png", "invalid.png", "unknown.png"};
    file_t *files[7] = {0};
    string_t *paths[7] = {0};
    bool ok = directory != NULL;
    for (unsigned i = 0; ok && i < 7; ++i) {
        paths[i] = string_sprintf("%s/%s", file_path(directory), names[i]);
        files[i] = file_new(paths[i]);
        ok = paths[i] && files[i];
    }
    string_t *memory = string_new_with(":memory:");
    sqlite_t *db = sqlite_open_plain(memory);
    ok = ok && db && file_open_write(files[0]) && ring(files[0], 0, 1, 0, 0, 2.001, 4) &&
         ring(files[0], 1, 2, .5, .5, 1.5, 1.5) && ring(files[0], 2, 1, 3, 1, 3.5, 1.5) &&
         file_close(files[0]) && file_open_read(files[0]) && globe_import_shoreline(db, files[0]);
    file_close(files[0]);
    globe_t *globe = ok ? globe_open_databases(db, NULL, NULL) : NULL;
    ok = ok && globe && photograph(paths[1]);
    string_t *before = ok ? file_sha256(files[1]) : NULL;
    const double bounds[] = {0, 0, 4, 4}, inland[] = {.1, .1, .4, .4}, ocean[] = {10, 10, 11, 11};
    bool may_cross = false;
    ok = ok && globe_coastline_window(globe, bounds, &may_cross) && may_cross &&
         globe_coastline_window(globe, inland, &may_cross) && !may_cross &&
         globe_coastline_window(globe, ocean, &may_cross) && !may_cross;
    satellite_mask_stats_t stats;
    ok = ok && satellite_mask_png(globe, paths[1], paths[2], bounds, 3239999, &stats) &&
         stats.changed && stats.visible_pixels > 1500000 && stats.removed_pixels > 1400000;
    unsigned char land[4], original[4], sea[4], lake[4], island[4], edge[4], missing[4];
    ok = ok && pixel(paths[2], 400, 400, land) && pixel(paths[1], 400, 400, original) && !memcmp(land, original, 4) &&
         pixel(paths[2], 1600, 100, sea) && !sea[0] && !sea[1] && !sea[2] && !sea[3] &&
         pixel(paths[2], 450, 1350, lake) && lake[3] == 255 &&
         pixel(paths[2], 1485, 1215, island) && island[3] == 255 &&
         pixel(paths[2], 900, 900, edge) && edge[3] > 0 && edge[3] < 255 &&
         pixel(paths[2], 100, 100, missing) && !missing[3];
    file_info_t *source_info = file_get_info(files[1]), *masked_info = file_get_info(files[2]);
    ok = ok && source_info && masked_info && file_info_size(masked_info) < file_info_size(source_info);
    ok = ok && satellite_mask_png(globe, paths[1], paths[3], inland, 3239999, &stats) &&
         !stats.changed && !stats.removed_pixels &&
         satellite_mask_png(globe, paths[1], paths[4], ocean, 3239999, &stats) && !stats.visible_pixels;
    ok = ok && satellite_mask_png(globe, paths[1], paths[6], inland, SIZE_MAX, &stats) &&
         !stats.changed && stats.visible_pixels == 3239999;
    ok = ok && !satellite_mask_png(globe, paths[1], paths[1], bounds, 3239999, &stats);
    string_t *after = file_sha256(files[1]);
    ok = ok && before && after && !string_compare(before, after);
    const double invalid[] = {0, 0, NAN, 4};
    ok = ok && !globe_coastline_window(globe, invalid, &may_cross) && may_cross &&
         !globe_coastline_window(globe, bounds, NULL);
    ok = ok && !satellite_mask_png(globe, paths[1], paths[5], invalid, 3239999, &stats) && !file_exists(files[5]);
    ok = ok && !satellite_mask_png(globe, paths[1], paths[5], inland, 3240001, &stats) && !file_exists(files[5]);
    string_free(before); string_free(after); file_info_free(source_info); file_info_free(masked_info);
    globe_close(globe); sqlite_close(db); string_free(memory);
    for (unsigned i = 0; i < 7; ++i) {
        if (files[i] && file_exists(files[i])) file_delete(files[i]);
        file_free(files[i]); string_free(paths[i]);
    }
    if (directory) file_remove_directory(directory);
    file_free(directory); string_free(parent);
    TEST_ASSERT_TRUE(ok, "coast masking preserves lakes and islands, clears ocean RGB, antialiases edges and protects inputs");
}

/* The optional preparation target runs this suite separately from ordinary Lab tests. */
int tests_main(void)
{
    TEST_RUN_IN_GROUP(test_satellite_coastal_mask, tests, NULL);
    return TEST_EXIT_CODE();
}
