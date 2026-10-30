/**
 * @file satellite_fetch.c
 * @brief Retrieve bounded source-footprint metadata for offline satellite preparation.
 *
 * Queries the public CDSE STAC service for two fixed, credited 2025 quarters, retaining only identifiers,
 * geographic bounds and RGB-band sizes. Metadata pages are bounded and pagination remains on the authorised
 * service. The resulting small catalogue is input to satellite_tiles; it contains no downloaded raster pixels.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "file-t.h"
#include "http-t.h"
#include "json-t.h"
#include "string-t.h"

static const json_t *field(const json_t *object, const char *name)
{
    string_t *key = string_new_with(name);
    const json_t *value = key ? json_object_get(object, key) : NULL;
    string_free(key);
    return value;
}

static const char *text(const json_t *value)
{
    const string_t *source = json_string_value(value);
    return source ? string_c_str(source) : "";
}

static bool set(json_t *object, const char *name, const json_t *value)
{
    string_t *key = string_new_with(name);
    bool ok = key && value && json_object_set(object, key, value);
    string_free(key);
    return ok;
}

static bool append_feature(json_t *features, const json_t *source)
{
    json_t *item = json_new_object(), *assets = json_new_object();
    bool ok = item && assets && set(item, "id", field(source, "id")) && set(item, "bbox", field(source, "bbox"));
    static const char *const bands[] = {"B02", "B03", "B04"};
    for (unsigned i = 0; ok && i < 3; ++i) {
        json_t *band = json_new_object();
        const json_t *size = field(field(field(source, "assets"), bands[i]), "file:size");
        ok = band && json_type(size) == JSON_NUMBER && set(band, "file:size", size) && set(assets, bands[i], band);
        json_free(band);
    }
    ok = ok && set(item, "assets", assets) && json_array_append(features, item);
    json_free(assets); json_free(item);
    return ok;
}

static bool fetch_quarter(http_client_t *client, json_t *features, const double bounds[4], unsigned quarter)
{
    const char *period = quarter == 2 ? "2025-04-01T00:00:00Z/2025-06-30T23:59:59Z" :
                                      "2025-07-01T00:00:00Z/2025-09-30T23:59:59Z";
    string_t *url = string_sprintf("https://stac.dataspace.copernicus.eu/v1/search?"
        "collections=sentinel-2-global-mosaics&bbox=%.12g,%.12g,%.12g,%.12g&datetime=%s&limit=100",
        bounds[0], bounds[1], bounds[2], bounds[3], period);
    unsigned page = 0;
    bool ok = url != NULL;
    while (ok && url && ++page <= 1000) {
        http_request_t *request = http_request_new(HTTP_GET, url);
        json_t *data = NULL;
        for (unsigned attempt = 0; request && !data && attempt < 6; ++attempt) {
            http_response_t *response = http_client_send(client, request);
            if (http_response_ok(response)) data = http_response_json(response);
            if (!data) {
                fprintf(stderr, "Metadata retry %u/6: Q%u page %u, HTTP %ld\n", attempt + 1,
                        quarter, page, http_response_status(response));
                if (attempt < 5) {
                    struct timespec pause = {.tv_sec = 1u << attempt};
                    nanosleep(&pause, NULL);
                }
            }
            http_response_free(response);
        }
        const json_t *rows = field(data, "features"), *links = field(data, "links");
        ok = json_type(rows) == JSON_ARRAY && json_array_size(rows) <= 100 && json_array_size(features) <= 32768;
        for (size_t i = 0; ok && i < json_array_size(rows); ++i)
            ok = append_feature(features, json_array_get(rows, i));
        string_t *next = NULL;
        /* A STAC page has a small fixed collection of navigation links. */
        for (size_t i = 0; ok && i < json_array_size(links); ++i) {
            const json_t *link = json_array_get(links, i);
            if (strcmp(text(field(link, "rel")), "next")) continue;
            const char *href = text(field(link, "href"));
            static const char prefix[] = "https://stac.dataspace.copernicus.eu/v1/search?";
            ok = !next && !strncmp(href, prefix, sizeof prefix - 1);
            if (ok) next = string_new_with(href);
        }
        printf("2025 Q%u metadata page %u: %zu footprints retained\n", quarter, page, json_array_size(features));
        fflush(stdout);
        string_free(url); url = next;
        json_free(data); http_request_free(request);
    }
    ok = ok && !url;
    string_free(url);
    return ok;
}

/* Retrieve a slim, atomically published source catalogue; no image or database is changed. */
int main(int argc, char **argv)
{
    if (argc != 6) {
        fprintf(stderr, "Usage: %s output.json west south east north\n", argv[0]);
        return EXIT_FAILURE;
    }
    double bounds[4];
    for (unsigned i = 0; i < 4; ++i) {
        char *end = NULL;
        bounds[i] = strtod(argv[i + 2], &end);
        if (!end || end == argv[i + 2] || *end || !isfinite(bounds[i])) return EXIT_FAILURE;
    }
    if (bounds[0] < -180 || bounds[2] > 180 || bounds[1] < -90 || bounds[3] > 90 ||
        bounds[0] >= bounds[2] || bounds[1] >= bounds[3]) return EXIT_FAILURE;
    http_client_t *client = http_client_new();
    http_limits_t limits = {.connect_timeout_ms = 15000, .total_timeout_ms = 90000, .max_body_bytes = 8388608};
    json_t *catalogue = json_new_object(), *features = json_new_array();
    bool ok = client && catalogue && features && http_client_set_limits(client, &limits);
    for (unsigned quarter = 2; ok && quarter <= 3; ++quarter) ok = fetch_quarter(client, features, bounds, quarter);
    ok = ok && set(catalogue, "features", features);
    string_t *serialised = ok ? json_to_string(catalogue) : NULL;
    string_t *path = string_sprintf("%s.part", argv[1]);
    file_t *temporary = path ? file_new(path) : NULL, *target = file_new_cstr(argv[1]);
    ok = ok && serialised && temporary && target && file_write_all_text(temporary, serialised) &&
         file_move(temporary, target, true);
    file_free(temporary); file_free(target); string_free(path); string_free(serialised);
    json_free(features); json_free(catalogue); http_client_free(client);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
