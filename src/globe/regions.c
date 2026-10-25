/**
 * @file regions.c
 * @brief Bounded transactional import of independent cultural display polygons.
 *
 * Converts reviewed GeoJSON into integer-microdegree edge chunks and an R-tree.
 * Cultural display codes never replace country ownership or shoreline data.
 * Lookups reuse the country's indexed parity reader through the globe module's
 * private interface. Imports are offline and retain the previous layer on error.
 */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "json-t.h"
#define MARS_GLOBE_INTERNAL_ACCESS
#include "globe_layers.h"

static const json_t *region_field(const json_t *value, const char *key)
{
    string_t *name = string_new_with(key);
    const json_t *field = name ? json_object_get(value, name) : NULL;
    string_free(name);
    return field;
}

static const char *region_text(const json_t *value)
{
    const string_t *text = json_string_value(value);
    return text ? string_c_str(text) : "";
}

static bool region_coordinate(const json_t *pair, int32_t xy[2])
{
    if (json_array_size(pair) != 2) return false;
    for (unsigned i = 0; i < 2; ++i) {
        const string_t *number = json_number_text(json_array_get(pair, i));
        if (!number) return false;
        const char *text = string_c_str(number);
        char *end;
        double value = strtod(text, &end);
        if (end == text || *end || !isfinite(value) || fabs(value) > (i ? 90 : 180)) return false;
        xy[i] = (int32_t)llround(value * 1000000);
    }
    return true;
}

static bool region_step(sqlite_stmt_t *statement)
{
    bool ok = sqlite_stmt_step(statement) == SQLITE_STEP_DONE;
    sqlite_stmt_reset(statement);
    return ok;
}

static bool region_ring(sqlite_stmt_t *chunk, sqlite_stmt_t *bounds, const json_t *ring, unsigned id,
                         int64_t *chunks, size_t *vertices)
{
    size_t count = json_array_size(ring);
    if (count < 4 || count > 1000000 - *vertices) return false;
    *vertices += count;
    int32_t first[2], last[2];
    if (!region_coordinate(json_array_get(ring, 0), first) ||
        !region_coordinate(json_array_get(ring, count - 1), last) || memcmp(first, last, sizeof first)) return false;
    for (size_t start = 0; start + 1 < count;) {
        size_t n = count - start > 257 ? 257 : count - start;
        unsigned char bytes[257 * 8];
        int32_t west = INT32_MAX, east = INT32_MIN, south = INT32_MAX, north = INT32_MIN;
        for (size_t i = 0; i < n; ++i) {
            int32_t xy[2];
            if (!region_coordinate(json_array_get(ring, start + i), xy)) return false;
            for (unsigned axis = 0; axis < 2; ++axis) {
                uint32_t value = (uint32_t)xy[axis];
                for (unsigned byte = 0; byte < 4; ++byte) bytes[i * 8 + axis * 4 + byte] = value >> (byte * 8);
            }
            if (xy[0] < west) west = xy[0];
            if (xy[0] > east) east = xy[0];
            if (xy[1] < south) south = xy[1];
            if (xy[1] > north) north = xy[1];
        }
        ++*chunks;
        if (!sqlite_stmt_bind_int64(chunk, 1, *chunks) || !sqlite_stmt_bind_int(chunk, 2, (int)id) ||
            !sqlite_stmt_bind_blob(chunk, 3, bytes, n * 8) || !region_step(chunk) ||
            !sqlite_stmt_bind_int64(bounds, 1, *chunks) || !sqlite_stmt_bind_int(bounds, 2, west) ||
            !sqlite_stmt_bind_int(bounds, 3, east) || !sqlite_stmt_bind_int(bounds, 4, south) ||
            !sqlite_stmt_bind_int(bounds, 5, north) || !region_step(bounds)) return false;
        start += n - 1;
    }
    return true;
}

static bool region_geometry(sqlite_stmt_t *chunk, sqlite_stmt_t *bounds, const json_t *geometry,
                             unsigned id, int64_t *chunks, size_t *vertices)
{
    const char *type = region_text(region_field(geometry, "type"));
    const json_t *coordinates = region_field(geometry, "coordinates");
    bool multiple = !strcmp(type, "MultiPolygon");
    if (!multiple && strcmp(type, "Polygon")) return false;
    size_t polygons = multiple ? json_array_size(coordinates) : 1;
    if (!polygons || polygons > 10000) return false;
    for (size_t p = 0; p < polygons; ++p) {
        const json_t *polygon = multiple ? json_array_get(coordinates, p) : coordinates;
        size_t rings = json_array_size(polygon);
        if (!rings || rings > 10000) return false;
        for (size_t r = 0; r < rings; ++r)
            if (!region_ring(chunk, bounds, json_array_get(polygon, r), id, chunks, vertices)) return false;
    }
    return true;
}

/* Validate bounded source data and publish only a complete independent region layer. */
bool globe_import_regions(sqlite_t *db, file_t *source)
{
    if (!db || !source) return false;
    file_info_t *info = file_get_info(source);
    bool sized = info && file_info_size(info) > 0 && file_info_size(info) <= 16 * 1024 * 1024;
    file_info_free(info);
    string_t *text = sized ? file_read_all_text(source) : NULL;
    json_t *document = text ? json_from_text(text) : NULL;
    string_free(text);
    const json_t *features = region_field(document, "features");
    size_t count = json_array_size(features), vertices = 0;
    bool transaction = document && !strcmp(region_text(region_field(document, "type")), "FeatureCollection") &&
                       count > 0 && count < 1024 && sqlite_exec_cstr(db, "savepoint region_import");
    bool ok = transaction && sqlite_exec_cstr(db,
        "drop table if exists region_bounds;"
        "drop table if exists region_chunk;"
        "drop table if exists region_name;"
        "create table region_name("
        " id integer primary key,"
        " name text not null,"
        " iso_country_code text not null"
        ");"
        "create table region_chunk("
        " id integer primary key,"
        " country integer not null,"
        " points blob not null"
        ");"
        "create virtual table region_bounds using rtree_i32("
        " id,"
        " west,"
        " east,"
        " south,"
        " north)");
    sqlite_stmt_t *name = ok ? sqlite_stmt_prepare(db, "insert into region_name values(?1,?2,?2)") : NULL;
    sqlite_stmt_t *chunk = ok ? sqlite_stmt_prepare(db, "insert into region_chunk values(?1,?2,?3)") : NULL;
    sqlite_stmt_t *bounds = ok ? sqlite_stmt_prepare(db, "insert into region_bounds values(?1,?2,?3,?4,?5)") : NULL;
    ok = ok && name && chunk && bounds;
    int64_t chunks = 0;
    for (size_t i = 0; ok && i < count; ++i) {
        const json_t *feature = json_array_get(features, i);
        const char *code = region_text(region_field(region_field(feature, "properties"), "code"));
        size_t length = strlen(code);
        ok = !strcmp(region_text(region_field(feature, "type")), "Feature") &&
             length >= 4 && length <= 6 && code[2] == '-';
        for (size_t c = 0; ok && c < length; ++c)
            ok = c == 2 || (code[c] >= 'A' && code[c] <= 'Z') || (c > 2 && code[c] >= '0' && code[c] <= '9');
        ok = ok && sqlite_stmt_bind_int(name, 1, (int)i + 1) && sqlite_stmt_bind_text(name, 2, code) &&
             region_step(name) && region_geometry(chunk, bounds, region_field(feature, "geometry"),
                                                   (unsigned)i + 1, &chunks, &vertices);
    }
    sqlite_stmt_finalize(name); sqlite_stmt_finalize(chunk); sqlite_stmt_finalize(bounds); json_free(document);
    if (ok) ok = sqlite_exec_cstr(db, "release region_import");
    if (!ok && transaction) sqlite_exec_cstr(db, "rollback to region_import;release region_import");
    return ok;
}
