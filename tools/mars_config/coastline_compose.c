/**
 * @file coastline_compose.c
 * @brief Offline composition of lossless coastal rings and existing nested inland waters.
 *
 * Streams MARSCF64 coastlines and GSHHG levels two to four into an exclusive staging
 * database, retaining original coordinates and source identifiers. The geographic
 * reviewer consumes this database before packaging; composition is not installation
 * approval. Per-ring validity, orientation and polar metadata remain available for
 * subsequent cross-ring and nesting checks. No installed database is opened or repaired.
 */
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <geos_c.h>
#include <sqlite3.h>

#include "file-t.h"
#include "string-t.h"

_Static_assert(sizeof(double) == 8 && DBL_MANT_DIG == 53, "Binary64 coordinates required");

typedef struct _compose_t {
    sqlite3 *db;
    GEOSContextHandle_t geos;
    sqlite3_stmt *ring, *geometry, *metadata, *bounds, *invalid;
    int64_t rings, points, waters, invalid_count;
} compose_t;

static uint32_t read_u32(const unsigned char *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static int64_t read_i32(const unsigned char *p)
{
    uint32_t value = read_u32(p);
    return value <= INT32_MAX ? (int64_t)value : (int64_t)value - 4294967296LL;
}

static double read_f64(const unsigned char *p)
{
    uint64_t bits = (uint64_t)read_u32(p) << 32 | read_u32(p + 4);
    double value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

static bool sql(sqlite3 *db, const char *text)
{
    bool ok = sqlite3_exec(db, text, NULL, NULL, NULL) == SQLITE_OK;
    if (!ok) fprintf(stderr, "Composition SQL: %s\n", sqlite3_errmsg(db));
    return ok;
}

static bool execute(sqlite3_stmt *stmt)
{
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
    return ok;
}

static bool prepare(compose_t *state)
{
    state->geos = GEOS_init_r();
    return state->geos && sql(state->db,
        "PRAGMA cache_size=-16384;PRAGMA temp_store=FILE;"
        "CREATE TABLE composition(status TEXT);INSERT INTO composition VALUES('incomplete');"
        "CREATE TABLE rings(id INTEGER PRIMARY KEY);"
        "CREATE TABLE simplified_rings(ring_id INTEGER,tolerance REAL,points INTEGER,geometry BLOB,"
        "PRIMARY KEY(ring_id,tolerance));"
        "CREATE TABLE ring_metadata(id INTEGER PRIMARY KEY,level INTEGER,source TEXT,source_id INTEGER,"
        "ccw INTEGER,area REAL,polar INTEGER);"
        "CREATE VIRTUAL TABLE ring_bounds USING rtree(id,west,east,south,north);"
        "CREATE TABLE invalid_rings(id INTEGER PRIMARY KEY,reason TEXT);BEGIN") &&
        sqlite3_prepare_v2(state->db, "INSERT INTO rings VALUES(?1)", -1, &state->ring, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(state->db, "INSERT INTO simplified_rings VALUES(?1,8,?2,?3)",
                           -1, &state->geometry, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(state->db, "INSERT INTO ring_metadata VALUES(?1,?2,?3,?4,?5,?6,?7)",
                           -1, &state->metadata, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(state->db, "INSERT INTO ring_bounds VALUES(?1,?2,?3,?4,?5)",
                           -1, &state->bounds, NULL) == SQLITE_OK &&
        sqlite3_prepare_v2(state->db, "INSERT INTO invalid_rings VALUES(?1,?2)",
                           -1, &state->invalid, NULL) == SQLITE_OK;
}

static bool store_ring(compose_t *state, GEOSGeometry *polygon, unsigned level, uint32_t source_id, bool precise)
{
    char ccw = 0;
    double west, east, south, north, area;
    int count = GEOSGetNumCoordinates_r(state->geos, polygon);
    bool ok = count >= 4 && GEOSGeom_getXMin_r(state->geos, polygon, &west) &&
        GEOSGeom_getXMax_r(state->geos, polygon, &east) && GEOSGeom_getYMin_r(state->geos, polygon, &south) &&
        GEOSGeom_getYMax_r(state->geos, polygon, &north) && GEOSArea_r(state->geos, polygon, &area) &&
        GEOSCoordSeq_isCCW_r(state->geos, GEOSGeom_getCoordSeq_r(state->geos,
                             GEOSGetExteriorRing_r(state->geos, polygon)), &ccw);
    char valid = GEOSisValid_r(state->geos, polygon);
    if (valid == 2) return false;
    int64_t id = state->rings + 1;
    if (!valid) {
        char *reason = GEOSisValidReason_r(state->geos, polygon);
        ok = ok && reason && sqlite3_bind_int64(state->invalid, 1, id) == SQLITE_OK &&
             sqlite3_bind_text(state->invalid, 2, reason, -1, SQLITE_TRANSIENT) == SQLITE_OK && execute(state->invalid);
        fprintf(stderr, "Invalid composed ring %" PRId64 " (%s %u): %s\n", id,
                precise ? "coast" : "inland", source_id, reason ? reason : "unknown");
        GEOSFree_r(state->geos, reason);
        ++state->invalid_count;
    }
    size_t bytes = 0;
    unsigned char *wkb = ok ? GEOSGeomToWKB_buf_r(state->geos, polygon, &bytes) : NULL;
    ok = ok && wkb && bytes <= 256 * 1048576 && sqlite3_bind_int64(state->ring, 1, id) == SQLITE_OK &&
        execute(state->ring) && sqlite3_bind_int64(state->geometry, 1, id) == SQLITE_OK &&
        sqlite3_bind_int(state->geometry, 2, count) == SQLITE_OK &&
        sqlite3_bind_blob(state->geometry, 3, wkb, (int)bytes, SQLITE_TRANSIENT) == SQLITE_OK && execute(state->geometry) &&
        sqlite3_bind_int64(state->metadata, 1, id) == SQLITE_OK &&
        sqlite3_bind_int(state->metadata, 2, (int)level) == SQLITE_OK &&
        sqlite3_bind_text(state->metadata, 3, precise ? "reviewed coastline" : "GSHHG inland", -1, SQLITE_STATIC) == SQLITE_OK &&
        sqlite3_bind_int64(state->metadata, 4, source_id) == SQLITE_OK &&
        sqlite3_bind_int(state->metadata, 5, ccw) == SQLITE_OK &&
        sqlite3_bind_double(state->metadata, 6, area) == SQLITE_OK &&
        sqlite3_bind_int(state->metadata, 7, south == -90 || north == 90) == SQLITE_OK && execute(state->metadata) &&
        sqlite3_bind_int64(state->bounds, 1, id) == SQLITE_OK &&
        sqlite3_bind_double(state->bounds, 2, west) == SQLITE_OK && sqlite3_bind_double(state->bounds, 3, east) == SQLITE_OK &&
        sqlite3_bind_double(state->bounds, 4, south) == SQLITE_OK && sqlite3_bind_double(state->bounds, 5, north) == SQLITE_OK &&
        execute(state->bounds);
    GEOSFree_r(state->geos, wkb);
    if (ok) {
        ++state->rings; state->points += count; state->waters += !precise;
        if (state->rings % 10000 == 0) {
            ok = sql(state->db, "COMMIT;BEGIN");
            fprintf(stderr, "Composition: %" PRId64 " rings, %" PRId64 " points, %" PRId64 " inland rings\n",
                    state->rings, state->points, state->waters);
            fflush(stderr);
        }
    }
    return ok;
}

static bool read_ring(compose_t *state, file_t *input, const unsigned char *header, bool precise)
{
    uint32_t count = read_u32(header + 4), flag = read_u32(header + 8), level = flag & 255;
    int64_t west = read_i32(header + 12), east = read_i32(header + 16);
    int64_t south = read_i32(header + 20), north = read_i32(header + 24);
    if (count < 2 || count > 8000000 || ((flag >> 8) & 255) != 15 || level < 1 || level > 6 ||
        west < -180000000 || east > 360000000 || south < -90000000 || north > 90000000 ||
        west > east || south > north || (precise && level != 1)) return false;
    bool retain = precise || (level >= 2 && level <= 4);
    size_t stride = precise ? 16 : 8;
    GEOSCoordSequence *coordinates = retain ? GEOSCoordSeq_create_r(state->geos, count, 2) : NULL;
    bool ok = !retain || coordinates != NULL;
    unsigned char buffer[16384];
    double first_x = 0, first_y = 0, last_x = 0, last_y = 0;
    for (uint32_t first = 0; ok && first < count; first += 1024) {
        unsigned n = count - first < 1024 ? count - first : 1024;
        size_t bytes = 0;
        ok = file_read(input, buffer, n * stride, &bytes) && bytes == n * stride;
        for (unsigned i = 0; retain && ok && i < n; ++i) {
            const unsigned char *p = buffer + i * stride;
            double x, y;
            if (precise) { x = read_f64(p); y = read_f64(p + 8); }
            else {
                int64_t raw = read_i32(p);
                if (raw > east && raw - 360000000 >= west) raw -= 360000000;
                if (raw < west && raw + 360000000 <= east) raw += 360000000;
                x = (double)raw * 0.000001; y = (double)read_i32(p + 4) * 0.000001;
            }
            ok = isfinite(x) && isfinite(y) && x >= -180 && x <= 360 && y >= -90 && y <= 90 &&
                 x * 1000000 >= west - 2 && x * 1000000 <= east + 2 &&
                 y * 1000000 >= south - 2 && y * 1000000 <= north + 2 &&
                 GEOSCoordSeq_setXY_r(state->geos, coordinates, first + i, x, y);
            if (!first && !i) { first_x = x; first_y = y; }
            last_x = x; last_y = y;
        }
    }
    if (!retain) return ok;
    bool closed = first_x == last_x && first_y == last_y;
    ok = ok && (!precise || closed);
    /* Preserve closed source sequences exactly; only close an open legacy ring. */
    if (ok && !closed) {
        GEOSCoordSequence *expanded = GEOSCoordSeq_create_r(state->geos, count + 1, 2);
        ok = expanded != NULL;
        for (unsigned i = 0; ok && i < count; ++i) {
            double x, y;
            ok = GEOSCoordSeq_getXY_r(state->geos, coordinates, i, &x, &y) &&
                 GEOSCoordSeq_setXY_r(state->geos, expanded, i, x, y);
        }
        ok = ok && GEOSCoordSeq_setXY_r(state->geos, expanded, count, first_x, first_y);
        GEOSCoordSeq_destroy_r(state->geos, coordinates);
        coordinates = expanded;
    }
    GEOSGeometry *shell = ok ? GEOSGeom_createLinearRing_r(state->geos, coordinates) : NULL;
    if (!ok) GEOSCoordSeq_destroy_r(state->geos, coordinates);
    GEOSGeometry *polygon = shell ? GEOSGeom_createPolygon_r(state->geos, shell, NULL, 0) : NULL;
    ok = polygon && store_ring(state, polygon, level, read_u32(header), precise);
    GEOSGeom_destroy_r(state->geos, polygon);
    return ok;
}

static bool read_stream(compose_t *state, file_t *input, bool precise)
{
    bool ok = file_open_read(input);
    size_t count = 0;
    unsigned char header[44];
    if (precise) ok = ok && file_read(input, header, 8, &count) && count == 8 && !memcmp(header, "MARSCF64", 8);
    while (ok) {
        ok = file_read(input, header, sizeof header, &count);
        if (!ok || !count) break;
        ok = count == sizeof header && read_ring(state, input, header, precise);
    }
    if (!ok) fprintf(stderr, "Malformed %s stream near composed ring %" PRId64 "\n",
                     precise ? "coastline" : "inland", state->rings);
    return file_close(input) && ok;
}

static void finish(compose_t *state)
{
    sqlite3_finalize(state->ring); sqlite3_finalize(state->geometry); sqlite3_finalize(state->metadata);
    sqlite3_finalize(state->bounds); sqlite3_finalize(state->invalid);
    if (state->geos) GEOS_finish_r(state->geos);
}

static void write_integer(unsigned char *p, uint64_t value, unsigned bytes)
{
    for (unsigned i = 0; i < bytes; ++i) p[bytes - 1 - i] = (unsigned char)(value >> (8 * i));
}

static bool stream_fixture(const char *directory, bool precise, unsigned level, bool closed, bool truncated)
{
    unsigned char bytes[132] = {0};
    unsigned offset = precise ? 8 : 0, count = closed ? 5 : 4, stride = precise ? 16 : 8;
    if (precise) memcpy(bytes, "MARSCF64", 8);
    unsigned char *header = bytes + offset;
    write_integer(header, 42, 4); write_integer(header + 4, count, 4);
    write_integer(header + 8, level | (15 << 8), 4);
    write_integer(header + 12, (uint32_t)-2000000, 4);
    write_integer(header + 16, (uint32_t)-1000000, 4);
    write_integer(header + 24, 1000000, 4);
    const double x[] = {-1.9999999, -1.0000001, -1.0000001, -1.9999999, -1.9999999};
    const double y[] = {0, 0, 1, 1, 0};
    for (unsigned i = 0; i < count; ++i) {
        unsigned char *p = header + 44 + i * stride;
        if (precise) {
            uint64_t bits;
            memcpy(&bits, &x[i], 8); write_integer(p, bits, 8);
            memcpy(&bits, &y[i], 8); write_integer(p + 8, bits, 8);
        } else {
            /* Legacy positive longitudes must wrap into the negative header interval. */
            write_integer(p, i == 1 || i == 2 ? 359000000 : 358000000, 4);
            write_integer(p + 4, (uint32_t)(y[i] * 1000000), 4);
        }
    }
    string_t *path = string_sprintf("%s/stream.bin", directory);
    file_t *file = path ? file_new(path) : NULL;
    compose_t state = {0};
    size_t size = offset + 44 + count * stride - (truncated ? 1 : 0), written = 0;
    bool ok = file && file_open(file, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE) &&
        file_write(file, bytes, size, &written) && written == size && file_close(file) &&
        sqlite3_open(":memory:", &state.db) == SQLITE_OK && prepare(&state);
    bool expected = !truncated && (!precise || (closed && level == 1));
    if (ok) ok = read_stream(&state, file, precise) == expected;
    bool retained = precise || (level >= 2 && level <= 4);
    if (ok && expected) {
        ok = state.rings == (retained ? 1 : 0) && state.points == (retained ? 5 : 0) && !state.invalid_count;
        sqlite3_stmt *row = NULL;
        if (ok && retained) {
            ok = sqlite3_prepare_v2(state.db,
                "SELECT geometry,level,source_id FROM simplified_rings JOIN ring_metadata ON id=ring_id",
                -1, &row, NULL) == SQLITE_OK && sqlite3_step(row) == SQLITE_ROW;
            GEOSGeometry *polygon = ok ? GEOSGeomFromWKB_buf_r(state.geos, sqlite3_column_blob(row, 0),
                                                               sqlite3_column_bytes(row, 0)) : NULL;
            double first_x = 0, first_y = 0;
            ok = ok && polygon && sqlite3_column_int(row, 1) == (int)level && sqlite3_column_int(row, 2) == 42 &&
                GEOSCoordSeq_getXY_r(state.geos, GEOSGeom_getCoordSeq_r(state.geos,
                    GEOSGetExteriorRing_r(state.geos, polygon)), 0, &first_x, &first_y) &&
                first_x == (precise ? x[0] : -2) && first_y == 0;
            GEOSGeom_destroy_r(state.geos, polygon);
        }
        sqlite3_finalize(row);
    }
    finish(&state);
    if (state.db) sqlite3_close(state.db);
    if (file && file_is_open(file)) file_close(file);
    if (file && file_exists(file)) ok = file_delete(file) && ok;
    file_free(file); string_free(path);
    return ok;
}

static bool self_test(void)
{
    compose_t state = {0};
    bool ok = sqlite3_open(":memory:", &state.db) == SQLITE_OK && prepare(&state);
    const char *polygons[] = {"POLYGON((0 0,10 0,10 10,0 10,0 0))", "POLYGON((1 1,1 9,9 9,9 1,1 1))",
                             "POLYGON((2 2,8 2,8 8,2 8,2 2))", "POLYGON((3 3,3 7,7 7,7 3,3 3))"};
    for (unsigned i = 0; ok && i < 4; ++i) {
        GEOSGeometry *polygon = GEOSGeomFromWKT_r(state.geos, polygons[i]);
        ok = polygon && store_ring(&state, polygon, i + 1, i, i == 0);
        GEOSGeom_destroy_r(state.geos, polygon);
    }
    ok = ok && state.rings == 4 && state.points == 20 && state.waters == 3 && state.invalid_count == 0;
    GEOSGeometry *bad = state.geos ? GEOSGeomFromWKT_r(state.geos, "POLYGON((0 0,2 2,0 2,2 0,0 0))") : NULL;
    ok = ok && bad && store_ring(&state, bad, 2, 99, false) && state.invalid_count == 1;
    GEOSGeom_destroy_r(state.geos, bad);
    finish(&state);
    if (state.db) sqlite3_close(state.db);
    string_t *parent = string_new_with("/tmp");
    file_t *directory = file_create_temp_directory(parent);
    ok = ok && directory && stream_fixture(file_path(directory), true, 1, true, false) &&
        stream_fixture(file_path(directory), true, 1, false, false) &&
        stream_fixture(file_path(directory), true, 1, true, true) &&
        stream_fixture(file_path(directory), true, 2, true, false);
    for (unsigned level = 1; ok && level <= 6; ++level) {
        ok = stream_fixture(file_path(directory), false, level, true, false) &&
             stream_fixture(file_path(directory), false, level, false, false) &&
             stream_fixture(file_path(directory), false, level, true, true);
    }
    if (directory) ok = file_remove_directory(directory) && ok;
    file_free(directory); string_free(parent);
    fprintf(stderr, "Coastline composition self-test: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

/* Build a separate geographic-review input, never a production database. */
int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--self-test")) return self_test() ? 0 : 1;
    if (argc != 4) {
        fprintf(stderr, "Usage: %s coast.cf64 inland-gshhg.bin NEW-composed.sqlite3\n", argv[0]);
        return 2;
    }
    file_t *coast = file_new_cstr(argv[1]), *inland = file_new_cstr(argv[2]), *output = file_new_cstr(argv[3]);
    compose_t state = {0};
    bool ok = coast && inland && output && file_open(output, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE) &&
        file_close(output) && sqlite3_open_v2(argv[3], &state.db, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK &&
        prepare(&state) && read_stream(&state, coast, true) && read_stream(&state, inland, false) && state.waters > 0;
    if (ok) ok = sql(state.db, state.invalid_count ?
        "UPDATE composition SET status='invalid_rings_not_approved';COMMIT" :
        "UPDATE composition SET status='composed_requires_boundary_and_nesting_review';COMMIT");
    fprintf(stderr, "Composition %s: %" PRId64 " rings, %" PRId64 " points, %" PRId64
                    " inland rings, %" PRId64 " invalid. Not installed.\n",
            ok ? "complete" : "incomplete", state.rings, state.points, state.waters, state.invalid_count);
    finish(&state);
    if (state.db) sqlite3_close(state.db);
    file_free(coast); file_free(inland); file_free(output);
    return ok && !state.invalid_count ? 0 : 1;
}
