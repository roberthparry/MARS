/**
 * @file rivers_import.c
 * @brief Transactional WDBII river import into the jurisdiction database.
 *
 * Consumes GSHHG 2.3.7 full-resolution river headers and coordinate pairs with bounded working memory. Overlapping
 * 257-point chunks preserve every source edge and have independent integer R-tree bounds. No polygon closure is
 * invented and no unrelated tables are changed. Failed or truncated replacements restore the previous dataset.
 */
#include <stdlib.h>
#include <string.h>

#define MARS_GLOBE_INTERNAL_ACCESS
#include "globe_layers.h"
#define MARS_RIVERS_INTERNAL_ACCESS
#include "rivers_internal.h"

typedef struct {
    sqlite_stmt_t *chunk, *bounds;
    int64_t chunks, features, points;
} rivers_import_t;

static bool rivers_execute(sqlite_stmt_t *stmt)
{
    bool ok = sqlite_stmt_step(stmt) == SQLITE_STEP_DONE;
    sqlite_stmt_reset(stmt);
    return ok;
}

static bool rivers_chunk(rivers_import_t *state, int id, unsigned kind, const unsigned char *p, size_t length)
{
    int32_t west = INT32_MAX, east = INT32_MIN, south = INT32_MAX, north = INT32_MIN;
    for (size_t i = 0; i < length; i += 8) {
        int32_t x = rivers_coordinate(p + i), y = rivers_coordinate(p + i + 4);
        if (x < west) west = x;
        if (x > east) east = x;
        if (y < south) south = y;
        if (y > north) north = y;
    }
    ++state->chunks;
    return sqlite_stmt_bind_int64(state->chunk, 1, state->chunks) &&
           sqlite_stmt_bind_int(state->chunk, 2, id) && sqlite_stmt_bind_int(state->chunk, 3, (int)kind) &&
           sqlite_stmt_bind_blob(state->chunk, 4, p, length) && rivers_execute(state->chunk) &&
           sqlite_stmt_bind_int64(state->bounds, 1, state->chunks) &&
           sqlite_stmt_bind_int(state->bounds, 2, west) && sqlite_stmt_bind_int(state->bounds, 3, east) &&
           sqlite_stmt_bind_int(state->bounds, 4, south) && sqlite_stmt_bind_int(state->bounds, 5, north) &&
           rivers_execute(state->bounds);
}

static bool rivers_feature(rivers_import_t *state, file_t *file, const unsigned char *header)
{
    int id = rivers_coordinate(header), n = rivers_coordinate(header + 4);
    uint32_t flag = (uint32_t)rivers_coordinate(header + 8), kind = flag & 255;
    int32_t west = rivers_coordinate(header + 12), east = rivers_coordinate(header + 16);
    if (id < 0 || n < 2 || n > 20000000 || ((flag >> 8) & 255) != 15 ||
        west < -180000000 || east > 360000000 || east < west) return false;
    unsigned char chunk[2056], input[8192];
    size_t used = 0;
    int32_t previous = 0;
    for (int i = 0; i < n; ++i) {
        if (i % 1024 == 0) {
            size_t count, length = (size_t)(n - i < 1024 ? n - i : 1024) * 8;
            if (!file_read(file, input, length, &count) || count != length) return false;
        }
        unsigned char *p = input + (i % 1024) * 8;
        int64_t x = rivers_coordinate(p);
        int32_t y = rivers_coordinate(p + 4);
        if (x > east && x - 360000000 >= west) x -= 360000000;
        if (x < west && x + 360000000 <= east) x += 360000000;
        if (x < (int64_t)west - 2 || x > (int64_t)east + 2 || x < -180000000 || x > 360000000 ||
            y < -90000000 || y > 90000000 || (i && llabs(x - previous) > 180000000)) return false;
        previous = (int32_t)x;
        uint32_t encoded = (uint32_t)x;
        p[0] = encoded >> 24; p[1] = encoded >> 16; p[2] = encoded >> 8; p[3] = encoded;
        memcpy(chunk + used, p, 8);
        used += 8;
        if (used == sizeof chunk) {
            if (!rivers_chunk(state, id, kind, chunk, used)) return false;
            memcpy(chunk, p, 8);
            used = 8;
        }
    }
    if (used > 8 && !rivers_chunk(state, id, kind, chunk, used)) return false;
    state->points += n;
    ++state->features;
    return true;
}

/* Replace only river tables, publishing the new dataset after full validation. */
bool rivers_import(sqlite_t *db, file_t *source)
{
    if (!db || !source || !sqlite_exec_cstr(db, "begin immediate")) return false;
    bool ok = sqlite_exec_cstr(db,
        "drop table if exists rivers_bounds;drop table if exists rivers_chunk;drop table if exists rivers_dataset;"
        "create table rivers_dataset(id integer primary key check(id=1),format_version integer not null,"
        "source text not null,point_count integer not null,feature_count integer not null);"
        "create table rivers_chunk(id integer primary key,feature_id integer not null,kind integer not null,"
        "vertices blob not null check(length(vertices) between 16 and 2056 and length(vertices)%8=0));"
        "create index rivers_feature_chunks on rivers_chunk(feature_id,id);"
        "create virtual table rivers_bounds using rtree_i32(id,min_x,max_x,min_y,max_y);");
    rivers_import_t state = {0};
    if (ok) {
        state.chunk = sqlite_stmt_prepare(db, "insert into rivers_chunk values(?1,?2,?3,?4)");
        state.bounds = sqlite_stmt_prepare(db, "insert into rivers_bounds values(?1,?2,?3,?4,?5)");
        ok = state.chunk && state.bounds;
    }
    while (ok) {
        unsigned char header[44];
        size_t count;
        ok = file_read(source, header, sizeof header, &count);
        if (!ok || !count) break;
        ok = count == sizeof header && rivers_feature(&state, source, header);
    }
    sqlite_stmt_finalize(state.chunk);
    sqlite_stmt_finalize(state.bounds);
    ok = ok && state.features > 0;
    if (ok) {
        sqlite_stmt_t *meta = sqlite_stmt_prepare(db,
            "insert into rivers_dataset values(1,1,'WDBII / GSHHG 2.3.7',?1,?2)");
        ok = meta && sqlite_stmt_bind_int64(meta, 1, state.points) &&
             sqlite_stmt_bind_int64(meta, 2, state.features) && rivers_execute(meta);
        sqlite_stmt_finalize(meta);
    }
    if (ok) ok = sqlite_exec_cstr(db, "commit");
    if (!ok) sqlite_exec_cstr(db, "rollback");
    return ok;
}
