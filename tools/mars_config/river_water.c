/**
 * @file river_water.c
 * @brief Optional offline preparation of indexed mapped water polygons and recorded river widths.
 *
 * Reads an OSM GeoJSON sequence into a new auxiliary SQLite candidate. GEOS validates polygon topology and
 * clips large polygons into one-degree pieces, retaining islands and holes. Coordinates use microdegrees,
 * matching the globe reader. Nothing is installed, simplified or inferred from waterway names. The original
 * extract and completed centreline network remain untouched. GEOS is only an offline tool dependency.
 */
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <geos_c.h>

#include "file-t.h"
#include "json-t.h"
#include "sqlite-t.h"
#include "string-t.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"

typedef struct _water_t {
    GEOSContextHandle_t geos;
    GEOSGeoJSONReader *reader;
    sqlite_stmt_t *bank, *bounds, *width;
    int64_t banks, widths, ignored_widths, source_id;
    unsigned source_type;
} water_t;

static const json_t *member(const json_t *object, const char *name)
{
    string_t *key = string_new_with(name);
    const json_t *value = key ? json_object_get(object, key) : NULL;
    string_free(key);
    return value;
}

static bool equal(const json_t *value, const char *text)
{
    const string_t *s = json_string_value(value);
    return s && !strcmp(string_c_str(s), text);
}

static void put(unsigned char *p, uint32_t value)
{
    p[0] = value >> 24; p[1] = value >> 16; p[2] = value >> 8; p[3] = value;
}

static bool step(sqlite_stmt_t *q)
{
    bool ok = sqlite_stmt_step(q) == SQLITE_STEP_DONE;
    sqlite_stmt_reset(q);
    return ok;
}

static bool polygon(water_t *w, const GEOSGeometry *geometry)
{
    int holes = GEOSGetNumInteriorRings_r(w->geos, geometry);
    if (holes < 0 || holes >= 4096) return false;
    size_t bytes = 4;
    for (int ring = 0; ring <= holes; ++ring) {
        const GEOSGeometry *g = ring ? GEOSGetInteriorRingN_r(w->geos, geometry, ring - 1) :
                                      GEOSGetExteriorRing_r(w->geos, geometry);
        const GEOSCoordSequence *seq = GEOSGeom_getCoordSeq_r(w->geos, g);
        unsigned count = 0;
        if (!seq || !GEOSCoordSeq_getSize_r(w->geos, seq, &count) || count < 4 || count > 250000) return false;
        bytes += 4 + (size_t)count * 8;
    }
    if (bytes > 4194304) return false;
    unsigned char *blob = malloc(bytes);
    if (!blob) return false;
    put(blob, (unsigned)holes + 1);
    size_t offset = 4;
    int32_t west = INT32_MAX, east = INT32_MIN, south = INT32_MAX, north = INT32_MIN;
    bool ok = true;
    for (int ring = 0; ok && ring <= holes; ++ring) {
        const GEOSGeometry *g = ring ? GEOSGetInteriorRingN_r(w->geos, geometry, ring - 1) :
                                      GEOSGetExteriorRing_r(w->geos, geometry);
        const GEOSCoordSequence *seq = GEOSGeom_getCoordSeq_r(w->geos, g);
        unsigned count = 0;
        ok = GEOSCoordSeq_getSize_r(w->geos, seq, &count);
        put(blob + offset, count); offset += 4;
        for (unsigned i = 0; ok && i < count; ++i) {
            double x, y;
            ok = GEOSCoordSeq_getX_r(w->geos, seq, i, &x) && GEOSCoordSeq_getY_r(w->geos, seq, i, &y) &&
                 isfinite(x) && isfinite(y) && fabs(x) <= 180 && fabs(y) <= 90;
            if (!ok) break;
            int32_t ix = (int32_t)llround(x * 1e6), iy = (int32_t)llround(y * 1e6);
            if (ix < west) west = ix;
            if (ix > east) east = ix;
            if (iy < south) south = iy;
            if (iy > north) north = iy;
            put(blob + offset, (uint32_t)ix); put(blob + offset + 4, (uint32_t)iy); offset += 8;
        }
    }
    if (ok) {
        ++w->banks;
        ok = sqlite_stmt_bind_int64(w->bank, 1, w->banks) && sqlite_stmt_bind_int64(w->bank, 2, w->source_id) &&
             sqlite_stmt_bind_int(w->bank, 3, w->source_type) && sqlite_stmt_bind_blob(w->bank, 4, blob, bytes) && step(w->bank) &&
             sqlite_stmt_bind_int64(w->bounds, 1, w->banks) && sqlite_stmt_bind_int(w->bounds, 2, west) &&
             sqlite_stmt_bind_int(w->bounds, 3, east) && sqlite_stmt_bind_int(w->bounds, 4, south) &&
             sqlite_stmt_bind_int(w->bounds, 5, north) && step(w->bounds);
    }
    free(blob);
    return ok;
}

static bool pieces(water_t *w, const GEOSGeometry *geometry, unsigned depth)
{
    if (!geometry || depth > 8) return false;
    if (GEOSisEmpty_r(w->geos, geometry) == 1) return true;
    int type = GEOSGeomTypeId_r(w->geos, geometry);
    if (type == GEOS_POLYGON) return polygon(w, geometry);
    if (type != GEOS_MULTIPOLYGON && type != GEOS_GEOMETRYCOLLECTION) return true; /* Zero-area tile contacts. */
    int count = GEOSGetNumGeometries_r(w->geos, geometry);
    if (count < 0 || count > 10000) return false;
    for (int i = 0; i < count; ++i)
        if (!pieces(w, GEOSGetGeometryN_r(w->geos, geometry, i), depth + 1)) return false;
    return true;
}

static bool area(water_t *w, const json_t *geometry)
{
    string_t *text = json_to_string(geometry);
    GEOSGeometry *raw = text ? GEOSGeoJSONReader_readGeometry_r(w->geos, w->reader, string_c_str(text)) : NULL;
    string_free(text);
    bool ok = raw && GEOSisValid_r(w->geos, raw) == 1;
    GEOSGeometry *g = ok ? GEOSGeom_setPrecision_r(w->geos, raw, .000001, 0) : NULL;
    GEOSGeom_destroy_r(w->geos, raw);
    double west, east, south, north;
    ok = g && GEOSisValid_r(w->geos, g) == 1 && GEOSGeom_getXMin_r(w->geos, g, &west) &&
         GEOSGeom_getXMax_r(w->geos, g, &east) && GEOSGeom_getYMin_r(w->geos, g, &south) &&
         GEOSGeom_getYMax_r(w->geos, g, &north) && west >= -180 && east <= 180 && south >= -90 && north <= 90 &&
         east - west < 180 && (ceil(east) - floor(west)) * (ceil(north) - floor(south)) <= 1024;
    for (int y = ok ? (int)floor(south) : 0; ok && y < ceil(north); ++y) {
        for (int x = (int)floor(west); ok && x < ceil(east); ++x) {
            GEOSGeometry *box = GEOSGeom_createRectangle_r(w->geos, x, y, x + 1., y + 1.);
            GEOSGeometry *cut = box ? GEOSIntersectionPrec_r(w->geos, g, box, .000001) : NULL;
            ok = cut && pieces(w, cut, 0);
            GEOSGeom_destroy_r(w->geos, cut); GEOSGeom_destroy_r(w->geos, box);
        }
    }
    GEOSGeom_destroy_r(w->geos, g);
    return ok;
}

static bool feature(water_t *w, const json_t *f)
{
    const json_t *properties = member(f, "properties"), *geometry = member(f, "geometry");
    const string_t *id = json_number_text(member(properties, "@id"));
    if (!equal(member(f, "type"), "Feature") || !geometry || !id) return false;
    char *end;
    errno = 0;
    w->source_id = strtoll(string_c_str(id), &end, 10);
    w->source_type = equal(member(properties, "@type"), "relation") ? 2 : 1;
    if (errno || *end || w->source_id <= 0) return false;
    const json_t *type = member(geometry, "type"), *waterway = member(properties, "waterway");
    if (equal(type, "LineString")) {
        double width;
        if (!(equal(waterway, "river") || equal(waterway, "stream") || equal(waterway, "canal") ||
              equal(waterway, "tidal_channel")) || !cfg_database_river_width_metres(properties, &width)) return true;
        if (width > 100000) { ++w->ignored_widths; return true; }
        ++w->widths;
        return sqlite_stmt_bind_int64(w->width, 1, w->source_id) &&
               sqlite_stmt_bind_double(w->width, 2, width) && step(w->width);
    }
    /* Open-water areas include lakes, lagoons and reservoirs, not just river-tagged banks.
     * Wetland alone is deliberately insufficient: marsh vegetation is not open water. */
    if (!equal(type, "Polygon") && !equal(type, "MultiPolygon")) return true;
    if (!cfg_database_river_open_water(properties)) return true;
    return area(w, geometry);
}

static string_t *opl_text(const char *first, const char *end)
{
    string_t *text = string_new();
    bool ok = text != NULL;
    while (ok && first < end) {
        if (*first != '%') { ok = string_append_char(text, *first++) == 0; continue; }
        ++first;
        uint32_t value = 0;
        unsigned digits = 0;
        while (first < end && *first != '%' && digits < 6) {
            unsigned c = (unsigned char)*first++;
            unsigned digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                             c >= 'A' && c <= 'F' ? c - 'A' + 10 : 16;
            if (digit > 15) { ok = false; break; }
            value = value * 16 + digit; ++digits;
        }
        ok = ok && digits && first < end && *first++ == '%' && value <= 0x10ffff &&
             !(value >= 0xd800 && value <= 0xdfff) && string_append_rune(text, rune_from_value(value)) == 0;
    }
    if (ok) return text;
    string_free(text);
    return NULL;
}

/* Metadata-only OPL avoids rebuilding a planet-sized node index merely to recover width tags. */
static bool opl_width(water_t *w, const char *line)
{
    if (*line++ != 'w') return false;
    char *end;
    errno = 0;
    int64_t id = strtoll(line, &end, 10);
    if (errno || id <= 0 || *end != ' ') return false;
    json_t *properties = json_new_object();
    bool ok = properties != NULL;
    const char *field = end;
    while (ok && *field) {
        while (*field == ' ') ++field;
        const char *last = strchr(field, ' ');
        if (!last) last = field + strlen(field);
        if (*field == 'T') {
            const char *tag = field + 1;
            while (ok && tag < last) {
                const char *stop = memchr(tag, ',', (size_t)(last - tag));
                if (!stop) stop = last;
                const char *equals = memchr(tag, '=', (size_t)(stop - tag));
                if (!equals) { ok = false; break; }
                string_t *key = opl_text(tag, equals), *value = opl_text(equals + 1, stop);
                json_t *text = value ? json_new_string(value) : NULL;
                ok = key && text && json_object_set(properties, key, text);
                string_free(key); string_free(value); json_free(text);
                tag = stop == last ? last : stop + 1;
            }
        }
        field = last;
    }
    double width;
    const json_t *kind = member(properties, "waterway");
    if (ok && (equal(kind, "river") || equal(kind, "stream") || equal(kind, "canal") || equal(kind, "tidal_channel")) &&
        cfg_database_river_width_metres(properties, &width)) {
        if (width > 100000) ++w->ignored_widths;
        else {
            ok = sqlite_stmt_bind_int64(w->width, 1, id) && sqlite_stmt_bind_double(w->width, 2, width) && step(w->width);
            if (ok) ++w->widths;
        }
    }
    json_free(properties);
    return ok;
}

/* Build only a new candidate; installation remains a separate validated transaction. */
int main(int argc, char **argv)
{
    if (argc != 3) { fputs("Usage: river_water INPUT.geojsonseq-or-opl NEW-CANDIDATE.db\n", stderr); return 2; }
    string_t *input_path = string_new_with(argv[1]), *output_path = string_new_with(argv[2]);
    file_t *input = file_new(input_path), *output = file_new(output_path);
    bool ok = input && output && !file_exists(output) && file_open_text(input);
    sqlite_t *db = ok ? sqlite_open_plain(output_path) : NULL;
    water_t w = {.geos = GEOS_init_r()};
    w.reader = w.geos ? GEOSGeoJSONReader_create_r(w.geos) : NULL;
    ok = db && w.reader && sqlite_exec_cstr(db,
        "pragma cache_size=-8192;begin immediate;"
        "create table rivers_width(feature_id integer primary key,metres real not null check(metres>0));"
        "create table rivers_bank(id integer primary key,source_id integer,source_type integer,rings blob not null);"
        "create virtual table rivers_bank_bounds using rtree_i32(id,min_x,max_x,min_y,max_y)");
    if (ok) {
        w.width = sqlite_stmt_prepare(db, "insert into rivers_width values(?1,?2) on conflict(feature_id) do update set metres=excluded.metres");
        w.bank = sqlite_stmt_prepare(db, "insert into rivers_bank values(?1,?2,?3,?4)");
        w.bounds = sqlite_stmt_prepare(db, "insert into rivers_bank_bounds values(?1,?2,?3,?4,?5)");
        ok = w.width && w.bank && w.bounds;
    }
    size_t records = 0;
    string_t *line = NULL;
    while (ok && file_read_line(input, &line) && line) {
        const char *text = string_c_str(line);
        string_t *record = string_new_with(text + (*text == '\036'));
        json_t *f = *text != 'w' && string_byte_length(line) <= 8 * 1024 * 1024 ? json_from_text(record) : NULL;
        ++records;
        ok = string_byte_length(line) <= 8 * 1024 * 1024 && (*text == 'w' ? opl_width(&w, text) : f && feature(&w, f));
        json_free(f); string_free(record); string_free(line); line = NULL;
    }
    string_free(line);
    ok = ok && !file_last_error(input) && records > 0 && sqlite_exec_cstr(db, "commit");
    if (!ok) { sqlite_exec_cstr(db, "rollback"); fprintf(stderr, "Mapped water preparation failed at record %zu.\n", records); }
    else printf("Mapped water candidate: %lld polygon pieces; %lld recorded widths; %zu source features.\n",
                (long long)w.banks, (long long)w.widths, records);
    if (ok && w.ignored_widths) printf("Retained as unknown: %lld widths exceeding the 100 km validation limit.\n",
                                      (long long)w.ignored_widths);
    sqlite_stmt_finalize(w.width); sqlite_stmt_finalize(w.bank); sqlite_stmt_finalize(w.bounds);
    sqlite_close(db); file_free(input); file_free(output); string_free(input_path); string_free(output_path);
    if (w.reader) GEOSGeoJSONReader_destroy_r(w.geos, w.reader);
    if (w.geos) GEOS_finish_r(w.geos);
    return ok ? 0 : 1;
}
