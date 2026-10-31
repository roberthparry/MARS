/**
 * @file test_satellite_overviews.c
 * @brief Native regression for overview holes hidden by neighbouring rectangular footprints.
 *
 * Builds two synthetic RGBA tiles in an isolated directory. Checks filled observations,
 * unchanged existing and partially transparent pixels, retained source gaps and safe
 * replacement of a hard-linked image. Repeated preparation is idempotent; unavailable
 * intersecting sources must fail without publishing metadata. A planning regression
 * distinguishes proven complete coverage from a neighbouring tile's empty rectangle.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <spawn.h>
#include <sys/wait.h>
#include <gdal.h>
#include <cpl_error.h>

#include "file-t.h"
#include "json-t.h"
#include "string-t.h"
#include "satellite_overviews.h"
#include "test_harness.h"

TEST_SUITE_CONFIG(TEST_CONFIG_GLOBAL);

extern char **environ;

static bool photograph(const string_t *path, bool detail, unsigned channels)
{
    const unsigned edge = 1800;
    unsigned char *pixels = malloc((size_t)edge * edge * 4);
    if (!pixels) return false;
    for (unsigned y = 0; y < edge; ++y) for (unsigned x = 0; x < edge; ++x) {
        unsigned char *p = pixels + ((size_t)y * edge + x) * 4;
        p[0] = detail ? 30 : 11; p[1] = detail ? 60 : 22; p[2] = detail ? 90 : 33;
        p[3] = (detail ? x >= 1350 : x >= 900 && y >= 900) ? 0 : 255;
    }
    if (!detail) {
        pixels[((size_t)1500 * edge + 1001) * 4 + 3] = 255;
        pixels[((size_t)1500 * edge + 1002) * 4 + 3] = 128;
    }
    GDALDatasetH memory = GDALCreate(GDALGetDriverByName("MEM"), "", edge, edge, channels, GDT_Byte, NULL);
    int bands[] = {1, 2, 3, 4};
    bool ok = memory && GDALDatasetRasterIO(memory, GF_Write, 0, 0, edge, edge, pixels,
        edge, edge, GDT_Byte, channels, bands, 4, edge * 4, 1) == CE_None;
    for (unsigned i = 0; ok && i < channels; ++i)
        ok = GDALSetRasterColorInterpretation(GDALGetRasterBand(memory, i + 1),
                                              (GDALColorInterp)(GCI_RedBand + i)) == CE_None;
    char *options[] = {"ZLEVEL=1", NULL};
    GDALDatasetH output = ok ? GDALCreateCopy(GDALGetDriverByName("PNG"), string_c_str(path), memory,
                                              false, options, NULL, NULL) : NULL;
    ok = output != NULL;
    GDALClose(output); GDALClose(memory); free(pixels);
    return ok;
}

static bool pixel(const string_t *path, unsigned x, unsigned y, const unsigned char expected[4])
{
    GDALDatasetH image = GDALOpen(string_c_str(path), GA_ReadOnly);
    unsigned char actual[4];
    int bands[] = {1, 2, 3, 4};
    bool ok = image && GDALDatasetRasterIO(image, GF_Read, x, y, 1, 1, actual,
                                           1, 1, GDT_Byte, 4, bands, 4, 4, 1) == CE_None && !memcmp(actual, expected, 4);
    GDALClose(image);
    return ok;
}

static json_t *manifest(void)
{
    string_t *text = string_new_with(
        "{\"regions\":[{\"bounds\":[0,0,2,2]},{\"bounds\":[1,0,1.5,0.5]}],\"assets\":{"
        "\"regional/00.png\":{\"width\":1800,\"height\":1800},"
        "\"regional/01.png\":{\"width\":1800,\"height\":1800}}}");
    json_t *result = text ? json_from_text(text) : NULL;
    string_free(text);
    return result;
}

static void test_overview_observations(void)
{
    GDALAllRegister();
    string_t *parent = string_new_with("build/overview-repair");
    file_t *parent_directory = file_new(parent);
    bool ok = parent_directory && (file_exists(parent_directory) || file_create_directory(parent_directory, 0755, true));
    file_t *directory = ok ? file_create_temp_directory(parent) : NULL;
    string_t *regional_path = directory ? string_sprintf("%s/regional", file_path(directory)) : NULL;
    file_t *regional = regional_path ? file_new(regional_path) : NULL;
    ok = regional && file_create_directory(regional, 0755, false);
    string_t *source_path = directory ? string_sprintf("%s/source.png", file_path(directory)) : NULL;
    string_t *overview_path = regional_path ? string_sprintf("%s/00.png", string_c_str(regional_path)) : NULL;
    string_t *detail_path = regional_path ? string_sprintf("%s/01.png", string_c_str(regional_path)) : NULL;
    file_t *source = source_path ? file_new(source_path) : NULL, *overview = overview_path ? file_new(overview_path) : NULL;
    file_t *detail = detail_path ? file_new(detail_path) : NULL;
    ok = ok && source && overview && detail && photograph(source_path, false, 4) &&
        file_create_hard_link(source, overview) && photograph(detail_path, true, 4);
    string_t *before = ok ? file_sha256(source) : NULL;
    string_t *root = directory ? string_new_with(file_path(directory)) : NULL;
    json_t *data = manifest();
    size_t changed = 0;
    const double outside[] = {5, 5, 6, 6};
    ok = ok && root && data && satellite_overviews_repair(root, data, outside, &changed) && changed == 0;
    ok = ok && before && root && data && satellite_overviews_repair(root, data, NULL, &changed) && changed == 1;
    const unsigned char filled[] = {30, 60, 90, 255}, retained[] = {11, 22, 33, 255};
    const unsigned char partial[] = {11, 22, 33, 128}, missing[] = {11, 22, 33, 0};
    ok = ok && pixel(overview_path, 1000, 1500, filled) && pixel(overview_path, 1001, 1500, retained) &&
        pixel(overview_path, 1002, 1500, partial) && pixel(overview_path, 1300, 1500, missing) &&
        pixel(overview_path, 1500, 1000, missing) && pixel(overview_path, 100, 100, retained);
    string_t *after = file_sha256(source), *repaired = file_sha256(overview);
    ok = ok && after && repaired && !string_compare(before, after) && string_compare(before, repaired) &&
        satellite_overviews_repair(root, data, NULL, &changed) && changed == 0;
    ok = ok && file_delete(detail) && photograph(detail_path, true, 3) &&
        satellite_overviews_repair(root, data, NULL, &changed) && changed == 1 && pixel(overview_path, 1300, 1500, filled);
    json_t *invalid = manifest();
    CPLPushErrorHandler(CPLQuietErrorHandler);
    ok = ok && file_delete(detail) && !satellite_overviews_repair(root, invalid, NULL, &changed);
    CPLPopErrorHandler();
    json_free(invalid); json_free(data);
    string_free(before); string_free(after); string_free(repaired); string_free(root);
    if (source && file_exists(source)) file_delete(source);
    if (overview && file_exists(overview)) file_delete(overview);
    if (detail && file_exists(detail)) file_delete(detail);
    if (regional) file_remove_directory(regional);
    if (directory) file_remove_directory(directory);
    file_free(source); file_free(overview); file_free(detail); file_free(regional); file_free(directory);
    file_free(parent_directory); string_free(parent); string_free(regional_path);
    string_free(source_path); string_free(overview_path); string_free(detail_path);
    TEST_ASSERT_TRUE(ok, "overview gaps use real detail, preserve existing pixels and immutable sources, and fail safely");
}

static void test_planner_requires_observations(void)
{
    string_t *parent = string_new_with("build/overview-repair");
    file_t *parent_directory = file_new(parent);
    bool ok = parent_directory && (file_exists(parent_directory) || file_create_directory(parent_directory, 0755, true));
    file_t *directory = ok ? file_create_temp_directory(parent) : NULL;
    const char *names[] = {"catalogue.json", "countries.geojson", "manifest.json", "plan.json"};
    string_t *paths[4] = {0};
    file_t *files[4] = {0};
    ok = directory != NULL;
    for (unsigned i = 0; ok && i < 4; ++i) {
        paths[i] = string_sprintf("%s/%s", file_path(directory), names[i]);
        files[i] = paths[i] ? file_new(paths[i]) : NULL;
        ok = files[i] != NULL;
    }
    string_t *catalogue = string_new_with("{\"features\":[]}");
    string_t *countries = string_new_with(
        "{\"type\":\"FeatureCollection\",\"features\":[{\"type\":\"Feature\","
        "\"properties\":{\"ADMIN\":\"United Kingdom\"},\"geometry\":{\"type\":\"Polygon\","
        "\"coordinates\":[[[-5.4,51.3],[-5.3,51.3],[-5.3,51.4],[-5.4,51.4],[-5.4,51.3]]]}}]}");
    ok = ok && catalogue && countries && file_write_all_text(files[0], catalogue) && file_write_all_text(files[1], countries);
    for (unsigned complete = 0; ok && complete < 2; ++complete) {
        string_t *input = string_sprintf(
            "{\"regions\":[{\"bounds\":[-5.51,51.24,-2.61,53.493333333333],\"valid_pixels\":%u}],"
            "\"assets\":{\"regional/00.png\":{\"width\":1800,\"height\":1800}}}", complete ? 3240000 : 3239999);
        ok = input && file_write_all_text(files[2], input);
        string_free(input);
        char *const args[] = {SATELLITE_PLANNER, (char *)string_c_str(paths[0]), (char *)string_c_str(paths[1]),
            (char *)string_c_str(paths[2]), (char *)string_c_str(paths[3]), NULL};
        pid_t child;
        int status = 0;
        ok = ok && !posix_spawn(&child, args[0], NULL, NULL, args, environ) &&
            waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
        string_t *output = ok ? file_read_all_text(files[3]) : NULL;
        json_t *plan = output ? json_from_text(output) : NULL;
        string_t *tiles_key = string_new_with("tiles"), *detail_key = string_new_with("detail");
        const json_t *tiles = plan && tiles_key ? json_object_get(plan, tiles_key) : NULL;
        const json_t *first = json_array_get(tiles, 0);
        bool detailed = false;
        ok = ok && first && detail_key && json_bool_value(json_object_get(first, detail_key), &detailed) &&
            detailed == (complete != 0);
        json_free(plan); string_free(output); string_free(tiles_key); string_free(detail_key);
    }
    string_free(catalogue); string_free(countries);
    for (unsigned i = 0; i < 4; ++i) {
        if (files[i] && file_exists(files[i])) file_delete(files[i]);
        file_free(files[i]); string_free(paths[i]);
    }
    if (directory) file_remove_directory(directory);
    file_free(directory); file_free(parent_directory); string_free(parent);
    TEST_ASSERT_TRUE(ok, "planning reuses complete observations and retains tiles needed behind partial neighbouring coverage");
}

/* Run the optional offline preparation regression independently of ordinary Lab tests. */
int tests_main(void)
{
    TEST_RUN_IN_GROUP(test_overview_observations, tests, NULL);
    TEST_RUN_IN_GROUP(test_planner_requires_observations, tests, NULL);
    return TEST_EXIT_CODE();
}
