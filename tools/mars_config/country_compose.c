/**
 * @file country_compose.c
 * @brief Offline replacement of Ukrainian and Russian country geometry from a pinned source viewpoint.
 *
 * Retains the map-unit ordering and attribute table, replacing only the two polygon records selected
 * by ISO_A2_EH. This permits Crimea, including Sevastopol, to remain part of Ukraine without changing
 * unrelated map units or inventing rectangular geographic overrides. Inputs are uncompressed SHP/DBF
 * pairs from Natural Earth 5.1.1. The output is a new SHP file; it does not modify installed databases.
 * The ordinary globe importer validates the resulting polygons and rebuilds shared borders afterwards.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file-t.h"

typedef struct _country_source_t {
    file_t *shapes;
    file_t *attributes;
    unsigned records, width, code_offset;
    unsigned char header[100];
} country_source_t;

static uint32_t little_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint32_t big_u32(const unsigned char *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static void put_big_u32(unsigned char *p, uint32_t value)
{
    p[0] = value >> 24; p[1] = value >> 16; p[2] = value >> 8; p[3] = value;
}

static bool read_bytes(file_t *file, void *data, size_t size)
{
    size_t read = 0;
    return file_read(file, data, size, &read) && read == size;
}

static bool write_bytes(file_t *file, const void *data, size_t size)
{
    size_t written = 0;
    return file_write(file, data, size, &written) && written == size;
}

static bool source_open(country_source_t *source, const char *shp, const char *dbf)
{
    source->shapes = file_new_cstr(shp);
    source->attributes = file_new_cstr(dbf);
    unsigned char header[32];
    if (!source->shapes || !source->attributes || !file_open_read(source->shapes) ||
        !file_open_read(source->attributes) || !read_bytes(source->shapes, source->header, 100) ||
        !read_bytes(source->attributes, header, 32) || big_u32(source->header) != 9994 ||
        little_u32(source->header + 28) != 1000 || little_u32(source->header + 32) != 5) return false;
    source->records = little_u32(header + 4);
    source->width = header[10] | (unsigned)header[11] << 8;
    unsigned length = header[8] | (unsigned)header[9] << 8, offset = 1;
    if (!source->records || source->records > 1023 || length < 65 || length % 32 != 1 ||
        !source->width || source->width > 16384) return false;
    for (unsigned pos = 32; pos + 32 < length; pos += 32) {
        if (!read_bytes(source->attributes, header, 32)) return false;
        if (!memcmp(header, "ISO_A2_EH\0", 10) && header[11] == 'C' && header[16] >= 2) {
            if (source->code_offset) return false;
            source->code_offset = offset;
        }
        offset += header[16];
    }
    return offset == source->width && source->code_offset && source->code_offset + 2 <= offset &&
           read_bytes(source->attributes, header, 1) && header[0] == 13;
}

static bool source_record(country_source_t *source, unsigned id, unsigned char **shape, size_t *size, int *slot)
{
    unsigned char row[16384], record[8];
    if (!read_bytes(source->attributes, row, source->width) || row[0] != ' ' ||
        !read_bytes(source->shapes, record, 8) || big_u32(record) != id) return false;
    uint32_t words = big_u32(record + 4);
    if (words < 22 || words > 8 * 1024 * 1024) return false;
    *size = (size_t)words * 2;
    *shape = malloc(*size);
    *slot = !memcmp(row + source->code_offset, "UA", 2) ? 0 :
            !memcmp(row + source->code_offset, "RU", 2) ? 1 : -1;
    return *shape && read_bytes(source->shapes, *shape, *size) && little_u32(*shape) == 5;
}

static bool source_end(country_source_t *source, uint32_t size)
{
    unsigned char byte;
    size_t extra = 0;
    return big_u32(source->header + 24) == size / 2 &&
           file_read(source->shapes, &byte, 1, &extra) && !extra;
}

static bool compose(country_source_t *base, country_source_t *view, file_t *output)
{
    unsigned char *replacement[2] = {NULL, NULL};
    size_t sizes[2] = {0, 0};
    bool ok = true, used[2] = {false, false};
    uint32_t source_size = 100, output_size = 100, view_size = 100;
    for (unsigned i = 1; ok && i <= view->records; ++i) {
        unsigned char *shape = NULL;
        size_t size = 0;
        int slot = -1;
        ok = source_record(view, i, &shape, &size, &slot);
        view_size += 8 + (uint32_t)size;
        if (ok && slot >= 0) {
            ok = !replacement[slot];
            if (ok) { replacement[slot] = shape; sizes[slot] = size; shape = NULL; }
        }
        free(shape);
    }
    ok = ok && source_end(view, view_size) && replacement[0] && replacement[1] &&
         file_open(output, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE) && write_bytes(output, base->header, 100);
    for (unsigned i = 1; ok && i <= base->records; ++i) {
        unsigned char *shape = NULL, record[8];
        size_t size = 0;
        int slot = -1;
        ok = source_record(base, i, &shape, &size, &slot);
        source_size += 8 + (uint32_t)size;
        const unsigned char *data = shape;
        if (ok && slot >= 0) {
            ok = !used[slot]; used[slot] = true;
            data = replacement[slot]; size = sizes[slot];
        }
        put_big_u32(record, i); put_big_u32(record + 4, (uint32_t)(size / 2));
        ok = ok && write_bytes(output, record, 8) && write_bytes(output, data, size);
        output_size += 8 + (uint32_t)size;
        free(shape);
    }
    ok = ok && used[0] && used[1] && source_end(base, source_size);
    put_big_u32(base->header + 24, output_size / 2);
    ok = ok && file_seek(output, 0, FILE_SEEK_BEGIN) && write_bytes(output, base->header, 100);
    free(replacement[0]); free(replacement[1]);
    return file_close(output) && ok;
}

/* Compose a new source file; never overwrite input sources or an existing output. */
int main(int argc, char **argv)
{
    if (argc != 6) {
        fputs("Usage: country_compose map.shp map.dbf viewpoint.shp viewpoint.dbf new.shp\n", stderr);
        return 2;
    }
    country_source_t base = {0}, view = {0};
    file_t *output = file_new_cstr(argv[5]);
    bool ok = output && !file_exists(output) && source_open(&base, argv[1], argv[2]) &&
              source_open(&view, argv[3], argv[4]) && compose(&base, &view, output);
    file_free(output);
    file_free(base.shapes); file_free(base.attributes);
    file_free(view.shapes); file_free(view.attributes);
    puts(ok ? "Replaced UA and RU geometry; all other map-unit records retained." : "Country composition failed.");
    return ok ? 0 : 1;
}
