/**
 * @file country_islands.c
 * @brief Offline addition of one complete shoreline island to a country polygon source.
 *
 * Reads a plain globe database without changing it and extracts a single closed land ring inside a supplied
 * window. Rejects clipped edges, holes and multiple rings. Appends the unchanged vertices to the selected
 * ISO_A2_EH map unit in a new SHP, preserving every existing record and the original DBF. This preparation
 * tool records geometry only: the caller must establish the island's ownership independently. The normal
 * country importer and geographical regression tests validate the resulting source before installation.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file-t.h"
#include "globe-t.h"
#include "sqlite-t.h"
#include "string-t.h"

typedef struct _island_ring_t {
    double bounds[4], points[20000][2];
    unsigned count, rings;
    bool closed;
} island_ring_t;

static uint32_t read_u32(const unsigned char *p, bool big)
{
    return big ? (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3] :
                 (uint32_t)p[3] << 24 | (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 | p[0];
}

static void put_u32(unsigned char *p, uint32_t n, bool big)
{
    for (unsigned i = 0; i < 4; ++i) p[big ? 3 - i : i] = n >> (8 * i);
}

static void put_double(unsigned char *p, double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof bits);
    for (unsigned i = 0; i < 8; ++i) p[i] = bits >> (8 * i);
}

static double read_double(const unsigned char *p)
{
    uint64_t bits = (uint64_t)read_u32(p, false) | (uint64_t)read_u32(p + 4, false) << 32;
    double value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

static bool read_bytes(file_t *file, void *data, size_t length)
{
    size_t count = 0;
    return file_read(file, data, length, &count) && count == length;
}

static bool write_bytes(file_t *file, const void *data, size_t length)
{
    size_t count = 0;
    return file_write(file, data, length, &count) && count == length;
}

static bool island_path(globe_path_event_t event, double x, double y, unsigned level, bool ice, void *context)
{
    island_ring_t *ring = context;
    if (level != 1 || ice || ring->closed) return false;
    if (event == GLOBE_PATH_CLOSE) { ring->closed = true; return true; }
    if (event == GLOBE_PATH_MOVE && ++ring->rings != 1) return false;
    if (ring->count >= 19999 || x <= ring->bounds[0] || y <= ring->bounds[1] ||
        x >= ring->bounds[2] || y >= ring->bounds[3]) return false;
    ring->points[ring->count][0] = x;
    ring->points[ring->count++][1] = y;
    return true;
}

static bool append_ring(file_t *input, file_t *attributes, file_t *output, const char *iso, island_ring_t *ring)
{
    unsigned char header[100], dbf[32], descriptor[32];
    if (!read_bytes(input, header, 100) || read_u32(header, true) != 9994 ||
        read_u32(header + 32, false) != 5 || !read_bytes(attributes, dbf, 32)) return false;
    unsigned records = read_u32(dbf + 4, false), width = dbf[10] | dbf[11] << 8;
    unsigned length = dbf[8] | dbf[9] << 8, offset = 1, code = 0;
    if (!records || records > 1023 || !width || width > 16384 || length < 65 || length % 32 != 1) return false;
    for (unsigned pos = 32; pos + 32 < length; pos += 32) {
        if (!read_bytes(attributes, descriptor, 32)) return false;
        if (!memcmp(descriptor, "ISO_A2_EH\0", 10) && descriptor[16] >= 2) code = offset;
        offset += descriptor[16];
    }
    if (!code || code + 2 > width || offset != width || !read_bytes(attributes, descriptor, 1) ||
        descriptor[0] != 13 || !write_bytes(output, header, 100)) return false;
    bool found = false;
    uint32_t total = 100, original = 100;
    for (unsigned id = 1; id <= records; ++id) {
        unsigned char row[16384], record[8];
        if (!read_bytes(attributes, row, width) || row[0] != ' ' || !read_bytes(input, record, 8) ||
            read_u32(record, true) != id) return false;
        uint32_t size = read_u32(record + 4, true) * 2;
        if (size < 44 || size > 16 * 1024 * 1024) return false;
        original += size + 8;
        unsigned char *shape = malloc(size + 4 + ring->count * 16);
        if (!shape) return false;
        bool ok = read_bytes(input, shape, size) && read_u32(shape, false) == 5;
        if (ok && !memcmp(row + code, iso, 2)) {
            unsigned parts = read_u32(shape + 36, false), points = read_u32(shape + 40, false);
            ok = !found && parts && points && 44ull + 4ull * parts + 16ull * points == size;
            for (unsigned i = 0; ok && i < ring->count; ++i)
                ok = ring->points[i][0] >= read_double(shape + 4) && ring->points[i][0] <= read_double(shape + 20) &&
                     ring->points[i][1] >= read_double(shape + 12) && ring->points[i][1] <= read_double(shape + 28);
            /* A repeated preparation would cancel the ring under parity lookup; reject it. */
            for (unsigned i = 0; ok && i < points; ++i)
                ok = read_double(shape + 44 + parts * 4 + i * 16) != ring->points[0][0] ||
                     read_double(shape + 52 + parts * 4 + i * 16) != ring->points[0][1];
            if (ok) {
                memmove(shape + 48 + parts * 4, shape + 44 + parts * 4, points * 16);
                put_u32(shape + 44 + parts * 4, points, false);
                put_u32(shape + 36, parts + 1, false);
                put_u32(shape + 40, points + ring->count, false);
                for (unsigned i = 0; i < ring->count; ++i) {
                    put_double(shape + size + 4 + i * 16, ring->points[i][0]);
                    put_double(shape + size + 12 + i * 16, ring->points[i][1]);
                }
                size += 4 + ring->count * 16;
                found = true;
            }
        }
        put_u32(record + 4, size / 2, true);
        ok = ok && write_bytes(output, record, 8) && write_bytes(output, shape, size);
        free(shape);
        if (!ok) return false;
        total += 8 + size;
    }
    size_t extra = 0;
    if (!found || read_u32(header + 24, true) != original / 2 ||
        !file_read(input, descriptor, 1, &extra) || extra) return false;
    put_u32(header + 24, total / 2, true);
    return file_seek(output, 0, FILE_SEEK_BEGIN) && write_bytes(output, header, 100);
}

/* Prepare one independent output; ownership is supplied explicitly, never inferred from proximity. */
int main(int argc, char **argv)
{
    if (argc != 10 || strcmp(argv[5], "ZA")) {
        fputs("Usage: country_islands globe.db input.shp input.dbf output.shp ZA west south east north\n", stderr);
        return 2;
    }
    island_ring_t *ring = calloc(1, sizeof *ring);
    bool ok = ring != NULL;
    for (unsigned i = 0; ok && i < 4; ++i) {
        char *end;
        ring->bounds[i] = strtod(argv[6 + i], &end);
        ok = !*end && isfinite(ring->bounds[i]);
    }
    string_t *path = string_new_with(argv[1]);
    sqlite_t *db = ok ? sqlite_open_plain(path) : NULL;
    ok = db && sqlite_exec_cstr(db, "pragma query_only=on");
    globe_t *globe = ok ? globe_open_databases(db, NULL, NULL) : NULL;
    ok = globe && globe_surface_paths(globe, ring->bounds[0], ring->bounds[1], ring->bounds[2], ring->bounds[3],
                                      0, island_path, ring) && ring->closed && ring->rings == 1 && ring->count >= 3;
    if (ok && memcmp(ring->points[0], ring->points[ring->count - 1], sizeof ring->points[0])) {
        memcpy(ring->points[ring->count], ring->points[0], sizeof ring->points[0]);
        ++ring->count;
    }
    double area = 0;
    for (unsigned i = 1; ok && i < ring->count; ++i)
        area += ring->points[i - 1][0] * ring->points[i][1] - ring->points[i][0] * ring->points[i - 1][1];
    /* ESRI outer rings run clockwise; reversing a closed ring preserves its complete boundary. */
    if (ok && area > 0) for (unsigned i = 1; i < ring->count / 2; ++i) {
        double x = ring->points[i][0], y = ring->points[i][1];
        unsigned other = ring->count - 1 - i;
        memcpy(ring->points[i], ring->points[other], sizeof ring->points[i]);
        ring->points[other][0] = x; ring->points[other][1] = y;
    }
    file_t *input = file_new_cstr(argv[2]), *attributes = file_new_cstr(argv[3]), *output = file_new_cstr(argv[4]);
    ok = ok && input && attributes && output && !file_exists(output) && file_open_read(input) &&
         file_open_read(attributes) && file_open(output, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE) &&
         append_ring(input, attributes, output, argv[5], ring);
    if (output && file_is_open(output)) ok = file_close(output) && ok;
    if (ok) printf("Appended %u island vertices to %s; other records retained.\n", ring->count, argv[5]);
    else fputs("Island source composition failed.\n", stderr);
    file_free(input); file_free(attributes); file_free(output);
    globe_close(globe); sqlite_close(db); string_free(path); free(ring);
    return ok ? 0 : 1;
}
