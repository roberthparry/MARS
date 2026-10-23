/**
 * @file coastline_stage.c
 * @brief Offline, disk-backed coastline topology and coordinate staging.
 *
 * Joins an audited OSM way-reference export to coordinate-bearing OPL or a
 * GeoJSON sequence by OSM way ID. OPL avoids area classification omissions.
 * Original node identities, directions and coordinates are retained: coincident
 * coordinates never invent connectivity. Indexed endpoint checks precede any
 * polygon construction; indexed traversal assigns ways to closed directed rings.
 * GEOS is an offline preparation dependency only. This
 * executable cannot publish an installed database or approve simplified geometry.
 * Interrupted output remains explicitly incomplete and is never overwritten.
 */
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <geos_c.h>
#include <sqlite3.h>

#include "file-t.h"
#include "string-t.h"

static bool execute(sqlite3 *db, const char *sql)
{
    char *error = NULL;
    int result = sqlite3_exec(db, sql, NULL, NULL, &error);
    if (result != SQLITE_OK) fprintf(stderr, "%s\n", error ? error : sqlite3_errmsg(db));
    sqlite3_free(error);
    return result == SQLITE_OK;
}

static bool positive_id(const char *text, int64_t *id, const char **end)
{
    char *tail;
    errno = 0;
    long long value = strtoll(text, &tail, 10);
    if (errno || tail == text || value <= 0) return false;
    *id = value;
    *end = tail;
    return true;
}

static bool references(sqlite3 *db, const char *path)
{
    file_t *file = file_new_cstr(path);
    sqlite3_stmt *insert = NULL;
    bool ok = file && file_open_read(file) &&
        sqlite3_prepare_v2(db, "INSERT INTO ways(id,first_node,last_node,source_points) VALUES(?1,?2,?3,?4)",
                           -1, &insert, NULL) == SQLITE_OK && execute(db, "BEGIN");
    string_t *line = NULL;
    int64_t ways = 0;
    while (ok && (ok = file_read_line(file, &line)) && line) {
        const char *text = string_c_str(line), *end = NULL, *nodes = strstr(text, " Nn");
        int64_t id = 0, first = 0, last = 0, count = 0;
        ok = *text == 'w' && positive_id(text + 1, &id, &end) && *end == ' ' && nodes;
        const char *p = nodes ? nodes + 3 : "";
        while (ok) {
            ok = positive_id(p, &last, &end);
            if (!ok) break;
            if (!count++) first = last;
            if (!*end || *end == '\r') break;
            if (end[0] != ',' || end[1] != 'n') { ok = false; break; }
            p = end + 2;
        }
        ok = ok && count >= 2 && sqlite3_bind_int64(insert, 1, id) == SQLITE_OK &&
            sqlite3_bind_int64(insert, 2, first) == SQLITE_OK && sqlite3_bind_int64(insert, 3, last) == SQLITE_OK &&
            sqlite3_bind_int64(insert, 4, count) == SQLITE_OK && sqlite3_step(insert) == SQLITE_DONE;
        sqlite3_reset(insert);
        string_free(line); line = NULL;
        if (ok && ++ways % 10000 == 0) {
            ok = execute(db, "COMMIT;BEGIN");
            if (ways % 100000 == 0) { fprintf(stderr, "References: %" PRId64 " ways\n", ways); fflush(stderr); }
        }
    }
    string_free(line);
    file_free(file);
    sqlite3_finalize(insert);
    return ok && ways > 0 && execute(db, "COMMIT;CREATE UNIQUE INDEX start_node ON ways(first_node);"
                                          "CREATE UNIQUE INDEX end_node ON ways(last_node)");
}

static GEOSGeometry *located_way(GEOSContextHandle_t geos, const char *text, int64_t *id)
{
    const char *end = NULL, *nodes = strstr(text, " Nn");
    if (!nodes || !positive_id(text + 1, id, &end) || *end != ' ') return NULL;
    unsigned count = 1;
    for (const char *p = nodes + 3; *p; ++p) if (*p == ',') ++count;
    if (count < 2 || count > 2000000) return NULL;
    GEOSCoordSequence *sequence = GEOSCoordSeq_create_r(geos, count, 2);
    if (!sequence) return NULL;
    const char *p = nodes + 3;
    bool ok = true;
    for (unsigned i = 0; ok && i < count; ++i) {
        int64_t node;
        ok = positive_id(p, &node, &end) && *end == 'x';
        if (!ok) break;
        char *tail;
        errno = 0;
        double x = strtod(end + 1, &tail);
        ok = !errno && tail != end + 1 && *tail == 'y' && isfinite(x) && fabs(x) <= 180;
        if (!ok) break;
        p = tail + 1;
        double y = strtod(p, &tail);
        ok = !errno && tail != p && isfinite(y) && fabs(y) <= 90 &&
             (i + 1 < count ? tail[0] == ',' && tail[1] == 'n' : !*tail || *tail == '\r') &&
             GEOSCoordSeq_setXY_r(geos, sequence, i, x, y);
        p = tail + (i + 1 < count ? 2 : 0);
    }
    if (!ok) { GEOSCoordSeq_destroy_r(geos, sequence); return NULL; }
    return GEOSGeom_createLineString_r(geos, sequence);
}

static bool coordinates(sqlite3 *db, const char *path)
{
    GEOSContextHandle_t geos = GEOS_init_r();
    GEOSGeoJSONReader *reader = geos ? GEOSGeoJSONReader_create_r(geos) : NULL;
    GEOSWKBWriter *writer = geos ? GEOSWKBWriter_create_r(geos) : NULL;
    file_t *file = file_new_cstr(path);
    sqlite3_stmt *update = NULL;
    bool ok = reader && writer && file && file_open_read(file) && sqlite3_prepare_v2(db,
        "UPDATE ways SET geometry=?1 WHERE id=?2 AND source_points=?3 AND geometry IS NULL", -1, &update, NULL) ==
        SQLITE_OK && execute(db, "BEGIN");
    string_t *line = NULL;
    int64_t ways = 0, points = 0;
    while (ok && (ok = file_read_line(file, &line)) && line) {
        const char *text = string_c_str(line);
        if (*text == 'n') { string_free(line); line = NULL; continue; }
        if (*text == '\036') ++text;
        const char *key = strstr(text, "\"id\":\"w"), *end = NULL;
        int64_t id = 0;
        GEOSGeometry *geometry = NULL;
        if (*text == 'w') geometry = located_way(geos, text, &id);
        else {
            ok = key && positive_id(key + 7, &id, &end) && *end == '"';
            geometry = ok ? GEOSGeoJSONReader_readGeometry_r(geos, reader, text) : NULL;
        }
        ok = ok && geometry && GEOSGeomTypeId_r(geos, geometry) == GEOS_LINESTRING;
        int count = ok ? GEOSGeomGetNumPoints_r(geos, geometry) : 0;
        size_t bytes = 0;
        unsigned char *wkb = ok ? GEOSWKBWriter_write_r(geos, writer, geometry, &bytes) : NULL;
        ok = ok && count >= 2 && wkb && bytes <= 16 * 1048576 &&
            sqlite3_bind_blob(update, 1, wkb, (int)bytes, SQLITE_TRANSIENT) == SQLITE_OK &&
            sqlite3_bind_int64(update, 2, id) == SQLITE_OK && sqlite3_bind_int(update, 3, count) == SQLITE_OK &&
            sqlite3_step(update) == SQLITE_DONE && sqlite3_changes(db) == 1;
        sqlite3_reset(update); sqlite3_clear_bindings(update);
        GEOSFree_r(geos, wkb);
        GEOSGeom_destroy_r(geos, geometry);
        string_free(line); line = NULL;
        points += count;
        if (ok && ++ways % 10000 == 0) {
            ok = execute(db, "COMMIT;BEGIN");
            if (ways % 100000 == 0) {
                fprintf(stderr, "Coordinates: %" PRId64 " ways, %" PRId64 " points\n", ways, points);
                fflush(stderr);
            }
        }
    }
    string_free(line); file_free(file); sqlite3_finalize(update);
    if (reader) GEOSGeoJSONReader_destroy_r(geos, reader);
    if (writer) GEOSWKBWriter_destroy_r(geos, writer);
    if (geos) GEOS_finish_r(geos);
    return ok && ways > 0 && execute(db, "COMMIT");
}

static bool audit(sqlite3 *db)
{
    sqlite3_stmt *query = NULL;
    bool ok = sqlite3_prepare_v2(db,
        "SELECT count(*),sum(source_points),sum(geometry IS NULL),"
        "sum(NOT EXISTS(SELECT 1 FROM ways n WHERE n.first_node=w.last_node)),"
        "sum(NOT EXISTS(SELECT 1 FROM ways n WHERE n.last_node=w.first_node)) FROM ways w",
        -1, &query, NULL) == SQLITE_OK && sqlite3_step(query) == SQLITE_ROW;
    if (ok) {
        fprintf(stderr, "Audit: %lld ways, %lld points, %lld missing geometries, %lld/%lld open ends\n",
                sqlite3_column_int64(query, 0), sqlite3_column_int64(query, 1), sqlite3_column_int64(query, 2),
                sqlite3_column_int64(query, 3), sqlite3_column_int64(query, 4));
        ok = sqlite3_column_int64(query, 0) > 0 && sqlite3_column_int64(query, 2) == 0 &&
             sqlite3_column_int64(query, 3) == 0 && sqlite3_column_int64(query, 4) == 0;
    }
    sqlite3_finalize(query);
    return ok && execute(db, "UPDATE preparation SET status='coordinates_and_endpoints_checked';PRAGMA optimize");
}

static bool assemble_topology(sqlite3 *db)
{
    bool ok = execute(db, "ALTER TABLE ways ADD COLUMN ring_id INTEGER;"
        "CREATE INDEX unassigned ON ways(id) WHERE ring_id IS NULL;"
        "CREATE TABLE rings(id INTEGER PRIMARY KEY,first_node INTEGER,points INTEGER,ways INTEGER);BEGIN");
    sqlite3_stmt *start = NULL, *next = NULL, *mark = NULL, *ring = NULL;
    ok = ok && sqlite3_prepare_v2(db, "SELECT id,first_node FROM ways WHERE ring_id IS NULL ORDER BY id LIMIT 1",
                                  -1, &start, NULL) == SQLITE_OK && sqlite3_prepare_v2(db,
        "SELECT id,last_node,source_points,ring_id FROM ways WHERE first_node=?1", -1, &next, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(db, "UPDATE ways SET ring_id=?1 WHERE id=?2", -1, &mark, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(db, "INSERT INTO rings VALUES(?1,?2,?3,?4)", -1, &ring, NULL) == SQLITE_OK;
    int64_t rings = 0, largest = 0, total = 0;
    int step = SQLITE_DONE;
    while (ok && (step = sqlite3_step(start)) == SQLITE_ROW) {
        int64_t id = sqlite3_column_int64(start, 0), first = sqlite3_column_int64(start, 1), node = first;
        int64_t points = 1, ways = 0;
        sqlite3_reset(start);
        do {
            ok = sqlite3_bind_int64(next, 1, node) == SQLITE_OK && sqlite3_step(next) == SQLITE_ROW &&
                 sqlite3_column_type(next, 3) == SQLITE_NULL;
            if (!ok) break;
            int64_t way = sqlite3_column_int64(next, 0);
            node = sqlite3_column_int64(next, 1);
            points += sqlite3_column_int64(next, 2) - 1;
            ++ways;
            sqlite3_reset(next);
            ok = sqlite3_bind_int64(mark, 1, id) == SQLITE_OK && sqlite3_bind_int64(mark, 2, way) == SQLITE_OK &&
                 sqlite3_step(mark) == SQLITE_DONE && sqlite3_changes(db) == 1;
            sqlite3_reset(mark);
        } while (ok && node != first);
        ok = ok && sqlite3_bind_int64(ring, 1, id) == SQLITE_OK && sqlite3_bind_int64(ring, 2, first) == SQLITE_OK &&
             sqlite3_bind_int64(ring, 3, points) == SQLITE_OK && sqlite3_bind_int64(ring, 4, ways) == SQLITE_OK &&
             sqlite3_step(ring) == SQLITE_DONE;
        sqlite3_reset(ring);
        if (points > largest) largest = points;
        total += points;
        if (ok && ++rings % 10000 == 0) {
            ok = execute(db, "COMMIT;BEGIN");
            fprintf(stderr, "Topology: %" PRId64 " closed rings, largest %" PRId64 " points\n", rings, largest);
            fflush(stderr);
        }
    }
    sqlite3_finalize(start); sqlite3_finalize(next); sqlite3_finalize(mark); sqlite3_finalize(ring);
    fprintf(stderr, "Topology totals: %" PRId64 " rings, %" PRId64 " points, largest %" PRId64 "\n", rings, total, largest);
    return ok && step == SQLITE_DONE && rings > 0 && execute(db,
        "COMMIT;UPDATE preparation SET status='closed_ring_topology_checked';PRAGMA optimize");
}

static GEOSGeometry *ring_geometry(sqlite3 *db, GEOSContextHandle_t geos, int64_t first, int64_t count,
                                   int64_t ring_point_limit)
{
    /* Bound each validation unit; large continental rings require a separate tiled validation pass. */
    if (count < 4 || count > ring_point_limit) return NULL;
    GEOSCoordSequence *sequence = GEOSCoordSeq_create_r(geos, (unsigned)count, 2);
    GEOSWKBReader *reader = GEOSWKBReader_create_r(geos);
    sqlite3_stmt *next = NULL;
    bool ok = sequence && reader && sqlite3_prepare_v2(db,
        "SELECT last_node,geometry FROM ways WHERE first_node=?1", -1, &next, NULL) == SQLITE_OK;
    int64_t node = first;
    unsigned written = 0;
    double last_x = 0, last_y = 0, first_x = 0, first_y = 0;
    do {
        ok = ok && sqlite3_bind_int64(next, 1, node) == SQLITE_OK && sqlite3_step(next) == SQLITE_ROW;
        if (!ok) break;
        node = sqlite3_column_int64(next, 0);
        GEOSGeometry *way = GEOSWKBReader_read_r(geos, reader, sqlite3_column_blob(next, 1),
                                                (size_t)sqlite3_column_bytes(next, 1));
        const GEOSCoordSequence *coordinates = way ? GEOSGeom_getCoordSeq_r(geos, way) : NULL;
        unsigned size = 0;
        ok = coordinates && GEOSCoordSeq_getSize_r(geos, coordinates, &size) && size >= 2;
        for (unsigned i = 0; ok && i < size; ++i) {
            double x, y;
            ok = GEOSCoordSeq_getXY_r(geos, coordinates, i, &x, &y);
            if (!ok) break;
            if (written) {
                while (x - last_x > 180) x -= 360;
                while (x - last_x < -180) x += 360;
                if (!i) { ok = x == last_x && y == last_y; continue; }
            } else { first_x = x; first_y = y; }
            ok = written < (unsigned)count && GEOSCoordSeq_setXY_r(geos, sequence, written++, x, y);
            last_x = x; last_y = y;
        }
        GEOSGeom_destroy_r(geos, way);
        sqlite3_reset(next);
    } while (ok && node != first);
    ok = ok && written == (unsigned)count && first_x == last_x && first_y == last_y;
    sqlite3_finalize(next);
    if (reader) GEOSWKBReader_destroy_r(geos, reader);
    if (!ok) { GEOSCoordSeq_destroy_r(geos, sequence); return NULL; }
    GEOSGeometry *shell = GEOSGeom_createLinearRing_r(geos, sequence);
    return shell ? GEOSGeom_createPolygon_r(geos, shell, NULL, 0) : NULL;
}

static bool validate_rings(sqlite3 *db, int64_t ring_point_limit)
{
    bool ok = execute(db,
        "CREATE TABLE IF NOT EXISTS ring_validation(ring_id INTEGER PRIMARY KEY,result TEXT,reason TEXT);BEGIN");
    GEOSContextHandle_t geos = GEOS_init_r();
    sqlite3_stmt *rings = NULL, *insert = NULL;
    ok = ok && geos && sqlite3_prepare_v2(db,
        "SELECT id,first_node,points FROM rings WHERE id NOT IN"
        "(SELECT ring_id FROM ring_validation WHERE result!='deferred') ORDER BY points",
        -1, &rings, NULL) == SQLITE_OK && sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO ring_validation VALUES(?1,?2,?3)", -1, &insert, NULL) == SQLITE_OK;
    int step = SQLITE_DONE;
    int64_t checked = 0, invalid = 0, deferred = 0;
    while (ok && (step = sqlite3_step(rings)) == SQLITE_ROW) {
        int64_t id = sqlite3_column_int64(rings, 0), points = sqlite3_column_int64(rings, 2);
        GEOSGeometry *geometry = ring_geometry(db, geos, sqlite3_column_int64(rings, 1), points, ring_point_limit);
        char valid = geometry ? GEOSisValid_r(geos, geometry) : 2;
        char *reason = geometry && valid != 1 ? GEOSisValidReason_r(geos, geometry) : NULL;
        const char *result = valid == 1 ? "valid" : valid == 0 ? "invalid" : "deferred";
        const char *description = reason ? reason : valid == 1 ? "individual ring only; inter-ring checks pending" :
            "large, polar, inconsistent or unconstructable ring requires separate review";
        ok = sqlite3_bind_int64(insert, 1, id) == SQLITE_OK &&
             sqlite3_bind_text(insert, 2, result, -1, SQLITE_STATIC) == SQLITE_OK &&
             sqlite3_bind_text(insert, 3, description, -1, SQLITE_TRANSIENT) == SQLITE_OK &&
             sqlite3_step(insert) == SQLITE_DONE;
        sqlite3_reset(insert); sqlite3_clear_bindings(insert);
        if (valid == 0) ++invalid;
        if (valid == 2) ++deferred;
        if (valid != 1) fprintf(stderr, "Ring %" PRId64 " (%" PRId64 " points): %s: %s\n", id, points, result, description);
        GEOSFree_r(geos, reason); GEOSGeom_destroy_r(geos, geometry);
        if (++checked % 10000 == 0) {
            ok = ok && execute(db, "COMMIT;BEGIN");
            fprintf(stderr, "Validity: %" PRId64 " checked, %" PRId64 " invalid, %" PRId64 " deferred\n",
                    checked, invalid, deferred);
            fflush(stderr);
        }
    }
    sqlite3_finalize(rings); sqlite3_finalize(insert);
    if (geos) GEOS_finish_r(geos);
    fprintf(stderr, "Validity totals this pass: %" PRId64 " checked, %" PRId64 " invalid, %" PRId64 " deferred\n",
            checked, invalid, deferred);
    return ok && step == SQLITE_DONE && execute(db, "COMMIT");
}

static bool same_orientation(GEOSContextHandle_t geos, const GEOSGeometry *source, const GEOSGeometry *candidate)
{
    if (!source || !candidate || GEOSGeomTypeId_r(geos, candidate) != GEOS_POLYGON ||
        GEOSGetNumInteriorRings_r(geos, candidate) != 0) return false;
    char before, after;
    const GEOSGeometry *a = GEOSGetExteriorRing_r(geos, source), *b = GEOSGetExteriorRing_r(geos, candidate);
    return a && b && GEOSCoordSeq_isCCW_r(geos, GEOSGeom_getCoordSeq_r(geos, a), &before) &&
           GEOSCoordSeq_isCCW_r(geos, GEOSGeom_getCoordSeq_r(geos, b), &after) && before == after;
}

static GEOSGeometry *simplify_large_ring(GEOSContextHandle_t geos, const GEOSGeometry *source, double tolerance)
{
    /* Simplify the boundary, not the polygon: never invoke GEOS polygon repair or invent new components.
     * The full topology-preserving simplifier's segment index exceeds the memory cap on continental rings.
     * The caller still rejects invalid/changed-orientation results and the cross-ring review remains mandatory. */
    const GEOSGeometry *boundary = GEOSGetExteriorRing_r(geos, source);
    GEOSGeometry *line = boundary ? GEOSSimplify_r(geos, boundary, tolerance) : NULL;
    const GEOSCoordSequence *coordinates = line ? GEOSGeom_getCoordSeq_r(geos, line) : NULL;
    GEOSCoordSequence *copy = coordinates ? GEOSCoordSeq_clone_r(geos, coordinates) : NULL;
    GEOSGeometry *ring = copy ? GEOSGeom_createLinearRing_r(geos, copy) : NULL;
    GEOSGeometry *polygon = ring ? GEOSGeom_createPolygon_r(geos, ring, NULL, 0) : NULL;
    GEOSGeom_destroy_r(geos, line);
    return polygon;
}

typedef struct _coast_part_t {
    GEOSGeometry *original;
    GEOSGeometry *candidate;
    bool restored;
} coast_part_t;

typedef struct _coast_restore_t {
    GEOSContextHandle_t geos;
    const GEOSGeometry *location;
    unsigned restored;
} coast_restore_t;

static void restore_contact(void *item, void *userdata)
{
    coast_part_t *part = item;
    coast_restore_t *context = userdata;
    double distance = INFINITY;
    if (!part->restored && GEOSDistance_r(context->geos, part->candidate, context->location, &distance) &&
        distance < 0.000001) {
        GEOSGeom_destroy_r(context->geos, part->candidate);
        part->candidate = GEOSGeom_clone_r(context->geos, part->original);
        part->restored = true;
        ++context->restored;
    }
}

static GEOSGeometry *join_parts(GEOSContextHandle_t geos, coast_part_t *parts, unsigned count)
{
    unsigned total = 1;
    for (unsigned i = 0; i < count; ++i) {
        int size = parts[i].candidate ? GEOSGetNumCoordinates_r(geos, parts[i].candidate) : 0;
        if (size < 2 || size > 8000000 || total > 8000000 - (unsigned)(size - 1)) return NULL;
        total += (unsigned)size - 1;
    }
    GEOSCoordSequence *joined = GEOSCoordSeq_create_r(geos, total, 2);
    bool ok = joined != NULL;
    unsigned written = 0;
    for (unsigned i = 0; ok && i < count; ++i) {
        const GEOSCoordSequence *sequence = GEOSGeom_getCoordSeq_r(geos, parts[i].candidate);
        unsigned size = 0;
        ok = sequence && GEOSCoordSeq_getSize_r(geos, sequence, &size);
        for (unsigned j = i ? 1 : 0; ok && j < size; ++j) {
            double x, y;
            ok = GEOSCoordSeq_getXY_r(geos, sequence, j, &x, &y) &&
                 GEOSCoordSeq_setXY_r(geos, joined, written++, x, y);
        }
    }
    if (!ok || written != total) { GEOSCoordSeq_destroy_r(geos, joined); return NULL; }
    GEOSGeometry *ring = GEOSGeom_createLinearRing_r(geos, joined);
    return ring ? GEOSGeom_createPolygon_r(geos, ring, NULL, 0) : NULL;
}

static void restore_zone_part(void *item, void *userdata)
{
    coast_part_t *part = item;
    GEOSContextHandle_t geos = userdata;
    if (part->restored) return;
    GEOSGeometry *original = GEOSGeom_clone_r(geos, part->original);
    GEOSGeom_destroy_r(geos, part->candidate);
    part->candidate = original;
    part->restored = true;
}

static int shift_longitude(double *x, double *y, void *context)
{
    (void)y;
    *x += *(double *)context;
    return 1;
}

static bool restore_review_zones(sqlite3 *review, int64_t ring, GEOSContextHandle_t geos,
                                GEOSSTRtree *index, double tolerance)
{
    sqlite3_stmt *q = NULL;
    bool ok = sqlite3_prepare_v2(review,
        "SELECT a.geometry,b.geometry FROM contacts c JOIN chunks a ON a.id=c.first_chunk "
        "JOIN chunks b ON b.id=c.second_chunk WHERE c.first_ring=?1 OR c.second_ring=?1", -1, &q, NULL) == SQLITE_OK &&
        sqlite3_bind_int64(q, 1, ring) == SQLITE_OK;
    int step = SQLITE_DONE;
    while (ok && (step = sqlite3_step(q)) == SQLITE_ROW) {
        GEOSGeometry *a = GEOSGeomFromWKB_buf_r(geos, sqlite3_column_blob(q, 0), sqlite3_column_bytes(q, 0));
        GEOSGeometry *b = GEOSGeomFromWKB_buf_r(geos, sqlite3_column_blob(q, 1), sqlite3_column_bytes(q, 1));
        GEOSGeometry *contact = a && b ? GEOSIntersection_r(geos, a, b) : NULL;
        GEOSGeometry *zone = contact ? GEOSBuffer_r(geos, contact, tolerance * 2, 1) : NULL;
        ok = zone && GEOSisEmpty_r(geos, zone) == 0;
        for (double shift = -360; ok && shift <= 360; shift += 360) {
            GEOSGeometry *copy = GEOSGeom_transformXY_r(geos, zone, shift_longitude, &shift);
            ok = copy != NULL;
            if (copy) GEOSSTRtree_query_r(geos, index, copy, restore_zone_part, geos);
            GEOSGeom_destroy_r(geos, copy);
        }
        GEOSGeom_destroy_r(geos, zone); GEOSGeom_destroy_r(geos, contact);
        GEOSGeom_destroy_r(geos, a); GEOSGeom_destroy_r(geos, b);
    }
    sqlite3_finalize(q);
    return ok && step == SQLITE_DONE;
}

static GEOSGeometry *simplify_partitioned_ring(GEOSContextHandle_t geos, const GEOSGeometry *source, double tolerance,
                                               sqlite3 *review, int64_t ring_id)
{
    const GEOSGeometry *shell = GEOSGetExteriorRing_r(geos, source);
    const GEOSCoordSequence *coordinates = shell ? GEOSGeom_getCoordSeq_r(geos, shell) : NULL;
    unsigned vertices = 0;
    if (!coordinates || !GEOSCoordSeq_getSize_r(geos, coordinates, &vertices) || vertices < 4) return NULL;
    unsigned count = (vertices - 2) / 16384 + 1;
    coast_part_t *parts = calloc(count, sizeof *parts);
    GEOSSTRtree *index = GEOSSTRtree_create_r(geos, 10);
    bool ok = parts && index;
    for (unsigned i = 0; ok && i < count; ++i) {
        unsigned first = i * 16384, size = vertices - first < 16385 ? vertices - first : 16385;
        GEOSCoordSequence *sequence = GEOSCoordSeq_create_r(geos, size, 2);
        ok = sequence != NULL;
        for (unsigned j = 0; ok && j < size; ++j) {
            double x, y;
            ok = GEOSCoordSeq_getXY_r(geos, coordinates, first + j, &x, &y) &&
                 GEOSCoordSeq_setXY_r(geos, sequence, j, x, y);
        }
        if (!ok) { GEOSCoordSeq_destroy_r(geos, sequence); break; }
        parts[i].original = GEOSGeom_createLineString_r(geos, sequence);
        parts[i].candidate = parts[i].original ? GEOSTopologyPreserveSimplify_r(geos, parts[i].original, tolerance) : NULL;
        ok = parts[i].candidate != NULL;
        if (ok) GEOSSTRtree_insert_r(geos, index, parts[i].original, &parts[i]);
    }
    GEOSGeometry *candidate = NULL;
    if (ok && review) ok = restore_review_zones(review, ring_id, geos, index, tolerance);
    for (unsigned attempt = 0; ok && attempt < 256; ++attempt) {
        candidate = join_parts(geos, parts, count);
        char *reason = NULL;
        GEOSGeometry *location = NULL;
        char valid = candidate ? GEOSisValidDetail_r(geos, candidate, 0, &reason, &location) : 2;
        GEOSFree_r(geos, reason);
        if (valid == 1) { GEOSGeom_destroy_r(geos, location); break; }
        coast_restore_t restore = {.geos = geos, .location = location};
        GEOSGeometry *query = location ? GEOSBuffer_r(geos, location, 0.000001, 1) : NULL;
        if (query) GEOSSTRtree_query_r(geos, index, query, restore_contact, &restore);
        GEOSGeom_destroy_r(geos, query); GEOSGeom_destroy_r(geos, location);
        GEOSGeom_destroy_r(geos, candidate); candidate = NULL;
        ok = valid == 0 && restore.restored > 0;
    }
    if (index) GEOSSTRtree_destroy_r(geos, index);
    for (unsigned i = 0; parts && i < count; ++i) {
        GEOSGeom_destroy_r(geos, parts[i].original); GEOSGeom_destroy_r(geos, parts[i].candidate);
    }
    free(parts);
    return candidate;
}

static bool simplify_rings(sqlite3 *db, double metres, int64_t ring_point_limit, bool refine)
{
    /* This is a candidate only. Individual validity does not establish cross-ring safety. */
    bool ok = execute(db,
        "CREATE TABLE IF NOT EXISTS simplified_rings("
        "ring_id INTEGER,tolerance REAL,points INTEGER,geometry BLOB,fallback INTEGER,"
        "PRIMARY KEY(ring_id,tolerance));BEGIN");
    GEOSContextHandle_t geos = GEOS_init_r();
    GEOSWKBWriter *writer = geos ? GEOSWKBWriter_create_r(geos) : NULL;
    sqlite3_stmt *rings = NULL, *insert = NULL;
    ok = ok && writer && sqlite3_prepare_v2(db,
        "SELECT r.id,r.first_node,r.points FROM rings r JOIN ring_validation v ON v.ring_id=r.id "
        "WHERE v.result='valid' AND ((?2=0 AND NOT EXISTS(SELECT 1 FROM simplified_rings s "
        "WHERE s.ring_id=r.id AND s.tolerance=?1)) OR (?2=1 AND r.points>500000 AND EXISTS("
        "SELECT 1 FROM simplified_rings s WHERE s.ring_id=r.id AND s.tolerance=?1 AND s.fallback=1))) "
        "ORDER BY r.points", -1, &rings, NULL) == SQLITE_OK && sqlite3_bind_double(rings, 1, metres) == SQLITE_OK &&
        sqlite3_bind_int(rings, 2, refine) == SQLITE_OK &&
        sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO simplified_rings VALUES(?1,?2,?3,?4,?5)", -1, &insert, NULL) == SQLITE_OK;
    int64_t done = 0, retained = 0, original = 0, fallbacks = 0;
    int step = SQLITE_DONE;
    while (ok && (step = sqlite3_step(rings)) == SQLITE_ROW) {
        int64_t id = sqlite3_column_int64(rings, 0), points = sqlite3_column_int64(rings, 2);
        GEOSGeometry *source = ring_geometry(db, geos, sqlite3_column_int64(rings, 1), points, ring_point_limit);
        /* 111700 m per degree conservatively scales both axes; this is not positional-accuracy metadata. */
        GEOSGeometry *candidate = !source ? NULL : points > 500000 ?
            simplify_large_ring(geos, source, metres / 111700.0) :
            GEOSTopologyPreserveSimplify_r(geos, source, metres / 111700.0);
        bool fallback = !same_orientation(geos, source, candidate) || GEOSisValid_r(geos, candidate) != 1;
        if (fallback && source && points > 500000) {
            GEOSGeom_destroy_r(geos, candidate);
            candidate = simplify_partitioned_ring(geos, source, metres / 111700.0, NULL, 0);
            fallback = !same_orientation(geos, source, candidate) || GEOSisValid_r(geos, candidate) != 1;
        }
        if (fallback) {
            GEOSGeom_destroy_r(geos, candidate);
            candidate = source ? GEOSGeom_clone_r(geos, source) : NULL;
            ++fallbacks;
        }
        int count = candidate ? GEOSGetNumCoordinates_r(geos, candidate) : 0;
        size_t size = 0;
        unsigned char *wkb = candidate ? GEOSWKBWriter_write_r(geos, writer, candidate, &size) : NULL;
        ok = source && candidate && wkb && count >= 4 && count <= points && size <= 256 * 1048576 &&
             sqlite3_bind_int64(insert, 1, id) == SQLITE_OK && sqlite3_bind_double(insert, 2, metres) == SQLITE_OK &&
             sqlite3_bind_int(insert, 3, count) == SQLITE_OK &&
             sqlite3_bind_blob(insert, 4, wkb, (int)size, SQLITE_TRANSIENT) == SQLITE_OK &&
             sqlite3_bind_int(insert, 5, fallback) == SQLITE_OK && sqlite3_step(insert) == SQLITE_DONE;
        sqlite3_reset(insert); sqlite3_clear_bindings(insert);
        GEOSFree_r(geos, wkb); GEOSGeom_destroy_r(geos, candidate); GEOSGeom_destroy_r(geos, source);
        original += points; retained += count;
        if (++done % 10000 == 0 || points > 100000) {
            ok = ok && execute(db, "COMMIT;BEGIN");
            fprintf(stderr, "Candidate: %" PRId64 " rings, %" PRId64 "/%" PRId64 " points, %" PRId64 " fallbacks\n",
                    done, retained, original, fallbacks);
            fflush(stderr);
        }
    }
    sqlite3_finalize(rings); sqlite3_finalize(insert);
    if (writer) GEOSWKBWriter_destroy_r(geos, writer);
    if (geos) GEOS_finish_r(geos);
    fprintf(stderr, "Candidate pass: %" PRId64 " rings, %" PRId64 "/%" PRId64 " points; NOT approved for installation\n",
            done, retained, original);
    return ok && step == SQLITE_DONE && execute(db, "COMMIT");
}

static bool restore_small_contacts(sqlite3 *db, sqlite3 *review, double metres)
{
    sqlite3_stmt *contacts = NULL, *ring = NULL, *update = NULL, *save = NULL;
    sqlite3_stmt *status = NULL;
    bool complete = sqlite3_prepare_v2(review,
        "SELECT status FROM review WHERE status='boundary_contacts_found'", -1, &status, NULL) == SQLITE_OK &&
        sqlite3_step(status) == SQLITE_ROW;
    sqlite3_finalize(status);
    if (!complete) return false;
    GEOSContextHandle_t geos = GEOS_init_r();
    bool ok = geos && execute(db,
        "CREATE TABLE IF NOT EXISTS contact_restoration_prior AS SELECT * FROM simplified_rings WHERE 0;BEGIN") &&
        sqlite3_prepare_v2(review,
        "SELECT first_ring FROM contacts UNION SELECT second_ring FROM contacts ORDER BY 1",
        -1, &contacts, NULL) == SQLITE_OK && sqlite3_prepare_v2(db,
        "SELECT r.first_node,r.points FROM rings r JOIN simplified_rings s ON s.ring_id=r.id "
        "WHERE r.id=?1 AND s.tolerance=?2 AND s.fallback=0 AND r.points<=500000", -1, &ring, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(db,
        "INSERT INTO contact_restoration_prior SELECT * FROM simplified_rings WHERE ring_id=?1 AND tolerance=?2",
        -1, &save, NULL) == SQLITE_OK && sqlite3_prepare_v2(db,
        "UPDATE simplified_rings SET geometry=?1,points=?2,fallback=1 WHERE ring_id=?3 AND tolerance=?4",
        -1, &update, NULL) == SQLITE_OK;
    int step = SQLITE_DONE;
    int64_t restored = 0;
    while (ok && (step = sqlite3_step(contacts)) == SQLITE_ROW) {
        int64_t id = sqlite3_column_int64(contacts, 0);
        ok = sqlite3_bind_int64(ring, 1, id) == SQLITE_OK && sqlite3_bind_double(ring, 2, metres) == SQLITE_OK;
        int found = ok ? sqlite3_step(ring) : SQLITE_ERROR;
        if (found == SQLITE_ROW) {
            int64_t points = sqlite3_column_int64(ring, 1);
            GEOSGeometry *source = ring_geometry(db, geos, sqlite3_column_int64(ring, 0), points, 500000);
            size_t size = 0;
            unsigned char *wkb = source ? GEOSGeomToWKB_buf_r(geos, source, &size) : NULL;
            ok = wkb && size <= 16000000 && GEOSisValid_r(geos, source) == 1 &&
                 sqlite3_bind_int64(save, 1, id) == SQLITE_OK && sqlite3_bind_double(save, 2, metres) == SQLITE_OK &&
                 sqlite3_step(save) == SQLITE_DONE &&
                 sqlite3_bind_blob(update, 1, wkb, (int)size, SQLITE_TRANSIENT) == SQLITE_OK &&
                 sqlite3_bind_int64(update, 2, points) == SQLITE_OK && sqlite3_bind_int64(update, 3, id) == SQLITE_OK &&
                 sqlite3_bind_double(update, 4, metres) == SQLITE_OK && sqlite3_step(update) == SQLITE_DONE;
            sqlite3_reset(save); sqlite3_reset(update);
            GEOSFree_r(geos, wkb); GEOSGeom_destroy_r(geos, source);
            if (ok) ++restored;
        } else ok = found == SQLITE_DONE;
        sqlite3_reset(ring);
        if (ok && restored && restored % 100 == 0) {
            ok = execute(db, "COMMIT;BEGIN");
            fprintf(stderr, "Restored original geometry for %" PRId64 " contacting small rings\n", restored);
            fflush(stderr);
        }
    }
    sqlite3_finalize(contacts); sqlite3_finalize(ring); sqlite3_finalize(update); sqlite3_finalize(save);
    if (geos) GEOS_finish_r(geos);
    fprintf(stderr, "Restored %" PRId64 " small rings; cross-ring revalidation required\n", restored);
    return ok && step == SQLITE_DONE && execute(db, "COMMIT");
}

static bool restore_contact_sections(sqlite3 *db, sqlite3 *review, double metres, const char *review_name)
{
    if (!restore_small_contacts(db, review, metres)) return false;
    bool ok = execute(db,
        "CREATE TABLE IF NOT EXISTS contact_section_prior("
        "review TEXT,ring_id INTEGER,tolerance REAL,points INTEGER,geometry BLOB,fallback INTEGER,"
        "PRIMARY KEY(review,ring_id,tolerance))");
    sqlite3_stmt *ids = NULL, *ring = NULL, *save = NULL, *update = NULL;
    ok = ok && sqlite3_prepare_v2(review,
        "SELECT first_ring FROM contacts UNION SELECT second_ring FROM contacts ORDER BY 1", -1, &ids, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(db,
        "SELECT r.first_node,r.points FROM rings r JOIN simplified_rings s ON s.ring_id=r.id AND s.tolerance=?2 "
        "WHERE r.id=?1 AND r.points>500000 AND s.fallback=0 AND NOT EXISTS(SELECT 1 FROM contact_section_prior p "
        "WHERE p.ring_id=r.id AND p.tolerance=?2 AND p.review=?3)", -1, &ring, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(db,
        "INSERT INTO contact_section_prior SELECT ?3,ring_id,tolerance,points,geometry,fallback "
        "FROM simplified_rings WHERE ring_id=?1 AND tolerance=?2", -1, &save, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(db, "UPDATE simplified_rings SET geometry=?1,points=?2 WHERE ring_id=?3 AND tolerance=?4",
                           -1, &update, NULL) == SQLITE_OK;
    GEOSContextHandle_t geos = GEOS_init_r();
    ok = ok && geos;
    int step = SQLITE_DONE;
    while (ok && (step = sqlite3_step(ids)) == SQLITE_ROW) {
        int64_t id = sqlite3_column_int64(ids, 0);
        ok = sqlite3_bind_int64(ring, 1, id) == SQLITE_OK && sqlite3_bind_double(ring, 2, metres) == SQLITE_OK &&
             sqlite3_bind_text(ring, 3, review_name, -1, SQLITE_TRANSIENT) == SQLITE_OK;
        int found = ok ? sqlite3_step(ring) : SQLITE_ERROR;
        if (found == SQLITE_ROW) {
            int64_t points = sqlite3_column_int64(ring, 1);
            fprintf(stderr, "Restoring contact sections: ring %" PRId64 ", source points %" PRId64 "\n", id, points);
            fflush(stderr);
            GEOSGeometry *source = ring_geometry(db, geos, sqlite3_column_int64(ring, 0), points, 8000000);
            GEOSGeometry *candidate = source ? simplify_partitioned_ring(geos, source, metres / 111700.0, review, id) : NULL;
            size_t size = 0;
            unsigned char *wkb = candidate ? GEOSGeomToWKB_buf_r(geos, candidate, &size) : NULL;
            int count = candidate ? GEOSGetNumCoordinates_r(geos, candidate) : 0;
            ok = wkb && size <= 256 * 1048576 && count >= 4 && count <= points &&
                 same_orientation(geos, source, candidate) && GEOSisValid_r(geos, candidate) == 1 && execute(db, "BEGIN") &&
                 sqlite3_bind_int64(save, 1, id) == SQLITE_OK && sqlite3_bind_double(save, 2, metres) == SQLITE_OK &&
                 sqlite3_bind_text(save, 3, review_name, -1, SQLITE_TRANSIENT) == SQLITE_OK && sqlite3_step(save) == SQLITE_DONE &&
                 sqlite3_bind_blob(update, 1, wkb, (int)size, SQLITE_TRANSIENT) == SQLITE_OK &&
                 sqlite3_bind_int(update, 2, count) == SQLITE_OK && sqlite3_bind_int64(update, 3, id) == SQLITE_OK &&
                 sqlite3_bind_double(update, 4, metres) == SQLITE_OK && sqlite3_step(update) == SQLITE_DONE && execute(db, "COMMIT");
            sqlite3_reset(save); sqlite3_reset(update); sqlite3_clear_bindings(update);
            GEOSFree_r(geos, wkb); GEOSGeom_destroy_r(geos, candidate); GEOSGeom_destroy_r(geos, source);
            if (ok) fprintf(stderr, "Contact repair checkpoint: ring %" PRId64 ", %d/%" PRId64 " points\n", id, count, points);
            fflush(stderr);
        } else ok = found == SQLITE_DONE;
        sqlite3_reset(ring);
    }
    sqlite3_finalize(ids); sqlite3_finalize(ring); sqlite3_finalize(save); sqlite3_finalize(update);
    if (geos) GEOS_finish_r(geos);
    if (!ok) execute(db, "ROLLBACK");
    fprintf(stderr, "Contact-section restoration %s; full cross-ring review still required\n", ok ? "finished" : "interrupted");
    return ok && step == SQLITE_DONE;
}

static bool self_test(void)
{
    string_t *parent = string_new_with("/tmp");
    file_t *directory = parent ? file_create_temp_directory(parent) : NULL;
    string_t *reference_path = directory ? string_sprintf("%s/ways.opl", file_path(directory)) : NULL;
    string_t *geometry_path = directory ? string_sprintf("%s/lines.geojsonseq", file_path(directory)) : NULL;
    file_t *reference = reference_path ? file_new(reference_path) : NULL;
    file_t *geometry = geometry_path ? file_new(geometry_path) : NULL;
    string_t *refs = string_new_with("w1 Nn1,n2,n3\nw2 Nn3,n1\n");
    string_t *json = string_new_with(
        "\036{\"type\":\"Feature\",\"id\":\"w1\",\"geometry\":{\"type\":\"LineString\","
        "\"coordinates\":[[0,0],[1,0],[1,1]]},\"properties\":{}}\n"
        "\036{\"type\":\"Feature\",\"id\":\"w2\",\"geometry\":{\"type\":\"LineString\","
        "\"coordinates\":[[1,1],[0,0]]},\"properties\":{}}\n");
    sqlite3 *db = NULL;
    bool ok = reference && geometry && refs && json && file_write_all_text(reference, refs) &&
        file_write_all_text(geometry, json) && sqlite3_open(":memory:", &db) == SQLITE_OK && execute(db,
        "CREATE TABLE preparation(status TEXT);INSERT INTO preparation VALUES('incomplete');"
        "CREATE TABLE ways(id INTEGER PRIMARY KEY,first_node INTEGER,last_node INTEGER,source_points INTEGER,geometry BLOB)");
    ok = ok && references(db, file_path(reference)) && coordinates(db, file_path(geometry)) && audit(db);
    string_t *located = string_new_with("w1 Nn1x0y0,n2x1y0,n3x1y1\nw2 Nn3x1y1,n1x0y0\n");
    ok = ok && located && file_write_all_text(geometry, located) && execute(db, "UPDATE ways SET geometry=NULL") &&
         coordinates(db, file_path(geometry)) && audit(db);
    string_free(located);
    ok = ok && assemble_topology(db) && validate_rings(db, 2000000) && simplify_rings(db, 8, 2000000, false);
    sqlite3_stmt *check = NULL;
    ok = ok && sqlite3_prepare_v2(db, "SELECT result FROM ring_validation WHERE ring_id=1", -1, &check, NULL) ==
         SQLITE_OK && sqlite3_step(check) == SQLITE_ROW && !strcmp((const char *)sqlite3_column_text(check, 0), "valid");
    sqlite3_finalize(check); check = NULL;
    sqlite3 *review = NULL;
    ok = ok && sqlite3_open(":memory:", &review) == SQLITE_OK && execute(review,
        "CREATE TABLE review(status TEXT);INSERT INTO review VALUES('boundary_contacts_found');"
        "CREATE TABLE contacts(first_ring INTEGER,second_ring INTEGER);INSERT INTO contacts VALUES(1,1)") &&
        restore_small_contacts(db, review, 8);
    if (review) sqlite3_close(review);
    ok = ok && sqlite3_prepare_v2(db, "SELECT count(*) FROM contact_restoration_prior", -1, &check, NULL) == SQLITE_OK &&
         sqlite3_step(check) == SQLITE_ROW && sqlite3_column_int(check, 0) == 1;
    sqlite3_finalize(check); check = NULL;
    string_t *crossed = string_new_with("w1 Nn1x0y0,n2x1y1,n4x0y1,n3x1y0\nw2 Nn3x1y0,n1x0y0\n");
    ok = ok && crossed && file_write_all_text(geometry, crossed) && execute(db,
        "UPDATE ways SET geometry=NULL;UPDATE ways SET source_points=4 WHERE id=1;"
        "UPDATE rings SET points=5;DELETE FROM ring_validation") &&
        coordinates(db, file_path(geometry)) && validate_rings(db, 2000000);
    string_free(crossed);
    ok = ok && sqlite3_prepare_v2(db, "SELECT result FROM ring_validation WHERE ring_id=1", -1, &check, NULL) ==
         SQLITE_OK && sqlite3_step(check) == SQLITE_ROW && !strcmp((const char *)sqlite3_column_text(check, 0), "invalid");
    sqlite3_finalize(check);
    /* The same coordinates must not conceal different, disconnected OSM node identities. */
    ok = ok && execute(db, "UPDATE ways SET first_node=99 WHERE id=1") && !audit(db);
    GEOSContextHandle_t geos = GEOS_init_r();
    GEOSGeometry *source = geos ? GEOSGeomFromWKT_r(geos,
        "POLYGON((0 0,0.5 0.000001,1 0,1 1,0 1,0 0))") : NULL;
    GEOSGeometry *reduced = source ? simplify_large_ring(geos, source, 8.0 / 111700.0) : NULL;
    ok = ok && reduced && GEOSisValid_r(geos, reduced) == 1 && same_orientation(geos, source, reduced) &&
         GEOSGetNumCoordinates_r(geos, reduced) == 5;
    GEOSGeom_destroy_r(geos, reduced);
    reduced = source ? simplify_partitioned_ring(geos, source, 8.0 / 111700.0, NULL, 0) : NULL;
    ok = ok && reduced && GEOSisValid_r(geos, reduced) == 1 && same_orientation(geos, source, reduced) &&
         GEOSGetNumCoordinates_r(geos, reduced) == 5;
    GEOSGeom_destroy_r(geos, reduced);
    sqlite3 *zones = NULL;
    ok = ok && sqlite3_open(":memory:", &zones) == SQLITE_OK && execute(zones,
        "CREATE TABLE contacts(first_ring INTEGER,second_ring INTEGER,first_chunk INTEGER,second_chunk INTEGER);"
        "CREATE TABLE chunks(id INTEGER PRIMARY KEY,geometry BLOB);INSERT INTO contacts VALUES(1,2,1,2)");
    GEOSGeometry *line = GEOSGeomFromWKT_r(geos, "LINESTRING(360.5 -0.0001,360.5 0.0001)");
    size_t bytes = 0;
    unsigned char *blob = line ? GEOSGeomToWKB_buf_r(geos, line, &bytes) : NULL;
    sqlite3_stmt *zone_insert = NULL;
    ok = ok && blob && sqlite3_prepare_v2(zones, "INSERT INTO chunks VALUES(1,?1),(2,?1)", -1, &zone_insert, NULL) == SQLITE_OK &&
         sqlite3_bind_blob(zone_insert, 1, blob, (int)bytes, SQLITE_TRANSIENT) == SQLITE_OK && sqlite3_step(zone_insert) == SQLITE_DONE;
    sqlite3_finalize(zone_insert); GEOSFree_r(geos, blob); GEOSGeom_destroy_r(geos, line);
    reduced = ok ? simplify_partitioned_ring(geos, source, 8.0 / 111700.0, zones, 1) : NULL;
    ok = ok && reduced && GEOSisValid_r(geos, reduced) == 1 && GEOSGetNumCoordinates_r(geos, reduced) == 6;
    GEOSGeom_destroy_r(geos, reduced); GEOSGeom_destroy_r(geos, source);
    if (zones) sqlite3_close(zones);
    if (geos) GEOS_finish_r(geos);
    if (db) sqlite3_close(db);
    if (reference && file_exists(reference)) ok = file_delete(reference) && ok;
    if (geometry && file_exists(geometry)) ok = file_delete(geometry) && ok;
    if (directory) ok = file_remove_directory(directory) && ok;
    file_free(reference); file_free(geometry); file_free(directory);
    string_free(parent); string_free(reference_path); string_free(geometry_path); string_free(refs); string_free(json);
    fprintf(stderr, "Coastline staging self-test: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

/* Stage source geometry without touching the installed globe database. */
int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--self-test")) return self_test() ? 0 : 1;
    if (argc == 5 && (!strcmp(argv[1], "--restore-small-contacts") || !strcmp(argv[1], "--restore-contact-sections"))) {
        char *end;
        errno = 0;
        double metres = strtod(argv[3], &end);
        if (errno || end == argv[3] || *end || !isfinite(metres) || metres < 0.5 || metres > 40) return 2;
        sqlite3 *db = NULL, *review = NULL;
        bool ok = sqlite3_open_v2(argv[2], &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX, NULL) == SQLITE_OK &&
                  sqlite3_open_v2(argv[4], &review, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK &&
                  execute(db, "PRAGMA cache_size=-32768;PRAGMA temp_store=FILE");
        if (ok) ok = !strcmp(argv[1], "--restore-contact-sections") ?
            restore_contact_sections(db, review, metres, argv[4]) : restore_small_contacts(db, review, metres);
        if (review) sqlite3_close(review);
        if (db) sqlite3_close(db);
        return ok ? 0 : 1;
    }
    if (argc == 4 && (!strcmp(argv[1], "--simplify-rings") || !strcmp(argv[1], "--refine-large-rings"))) {
        char *end;
        errno = 0;
        double metres = strtod(argv[3], &end);
        if (errno || end == argv[3] || *end || !isfinite(metres) || metres < 0.5 || metres > 40) return 2;
        sqlite3 *db = NULL;
        bool ok = sqlite3_open_v2(argv[2], &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX, NULL) == SQLITE_OK &&
                  execute(db, "PRAGMA cache_size=-32768;PRAGMA temp_store=FILE") &&
                  simplify_rings(db, metres, 8000000, !strcmp(argv[1], "--refine-large-rings"));
        if (db) sqlite3_close(db);
        return ok ? 0 : 1;
    }
    if (argc == 3 && (!strcmp(argv[1], "--validate-rings") || !strcmp(argv[1], "--validate-large-rings"))) {
        int64_t ring_point_limit = !strcmp(argv[1], "--validate-large-rings") ? 8000000 : 2000000;
        sqlite3 *db = NULL;
        bool ok = sqlite3_open_v2(argv[2], &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX, NULL) == SQLITE_OK &&
                  execute(db, "PRAGMA cache_size=-32768;PRAGMA temp_store=FILE") && validate_rings(db, ring_point_limit);
        if (db) sqlite3_close(db);
        return ok ? 0 : 1;
    }
    if (argc != 4) {
        fprintf(stderr, "Usage: %s ways.opl coordinates.opl-or-geojsonseq NEW-staging.sqlite3\n", argv[0]);
        return 2;
    }
    file_t *output = file_new_cstr(argv[3]);
    bool ok = output && file_open(output, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE) && file_close(output);
    sqlite3 *db = NULL;
    ok = ok && sqlite3_open_v2(argv[3], &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX, NULL) == SQLITE_OK;
    if (ok) ok = execute(db,
        "PRAGMA journal_mode=DELETE;PRAGMA synchronous=NORMAL;PRAGMA temp_store=FILE;PRAGMA cache_size=-32768;"
        "CREATE TABLE preparation(status TEXT NOT NULL);INSERT INTO preparation VALUES('incomplete');"
        "CREATE TABLE ways(id INTEGER PRIMARY KEY,first_node INTEGER NOT NULL,last_node INTEGER NOT NULL,"
        "source_points INTEGER NOT NULL,geometry BLOB)");
    ok = ok && references(db, argv[1]) && coordinates(db, argv[2]) && audit(db) && assemble_topology(db);
    if (!ok) fprintf(stderr, "Coastline staging failed; output is not approved: %s\n", db ? sqlite3_errmsg(db) : "open failed");
    if (db) sqlite3_close(db);
    file_free(output);
    return ok ? 0 : 1;
}
