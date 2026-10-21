/**
 * @file cfg_database_globe.c
 * @brief Globe database configuration, shoreline import and river data preparation.
 *
 * Owns the globe installer and offline OSM river conversion, candidate staging and
 * transactional river publication. Stored centrelines have a conservative eight-metre
 * error ceiling with junctions fixed; the separate source extract retains full detail.
 * Staging and publication enforce a 3.8 GB river-layer limit, including indexes.
 * Source selection and geographic acceptance remain separate; no network download is performed.
 */
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <zstd.h>

#include "globe-t.h"
#include "json-t.h"
#include "cfg_progress.h"
#include "cfg_prompt.h"
#include "cfg_storage.h"
#include "cfg_database.h"
#define MARS_CFG_DATABASE_INTERNAL_ACCESS
#include "cfg_database_internal.h"

static const json_t *osm_field(const json_t *value, const char *name);

static uint32_t cfg_coast_get(const unsigned char *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static void cfg_coast_put(unsigned char *p, uint32_t n)
{
    p[0] = n >> 24; p[1] = n >> 16; p[2] = n >> 8; p[3] = n;
}

static bool cfg_coast_integer(const json_t *value, int64_t minimum, int64_t maximum, int64_t *result)
{
    const string_t *number = value ? json_number_text(value) : NULL;
    if (!number) return false;
    const char *text = string_c_str(number);
    char *end;
    errno = 0;
    long long n = strtoll(text, &end, 10);
    if (errno || end == text || *end || n < minimum || n > maximum) return false;
    *result = n;
    return true;
}

static bool cfg_coast_write(file_t *file, const void *bytes, size_t length)
{
    size_t written;
    return file_write(file, bytes, length, &written) && written == length;
}

static bool cfg_coast_copy(file_t *input, file_t *output, size_t length)
{
    unsigned char buffer[65536];
    while (length) {
        size_t wanted = length < sizeof buffer ? length : sizeof buffer, read;
        if (!file_read(input, buffer, wanted, &read) || read != wanted || !cfg_coast_write(output, buffer, read))
            return false;
        length -= read;
    }
    return true;
}

/* Apply a reviewed, endpoint-checked span replacement, never a town-specific display offset. */
static bool cfg_coast_ring_patch(file_t *input, file_t *output, unsigned char *header, const json_t *patch)
{
    uint32_t count = cfg_coast_get(header + 4);
    int64_t expected, first, last;
    const json_t *coordinates = osm_field(patch, "coordinates");
    size_t replacements = json_array_size(coordinates);
    if (count < 2 || count > 4000000 || replacements < 2 || replacements > 100000 ||
        !cfg_coast_integer(osm_field(patch, "source_points"), count, count, &expected) ||
        !cfg_coast_integer(osm_field(patch, "first"), 0, count - 2, &first) ||
        !cfg_coast_integer(osm_field(patch, "last"), first + 1, count - 1, &last)) return false;
    size_t bytes = (size_t)count * 8, read;
    unsigned char *original = malloc(bytes), *replacement = malloc(replacements * 8);
    bool ok = original && replacement && file_read(input, original, bytes, &read) && read == bytes;
    for (size_t i = 0; ok && i < replacements; ++i) {
        const json_t *pair = json_array_get(coordinates, i);
        int64_t x, y;
        ok = json_array_size(pair) == 2 &&
             cfg_coast_integer(json_array_get(pair, 0), (int32_t)cfg_coast_get(header + 12),
                               (int32_t)cfg_coast_get(header + 16), &x) &&
             cfg_coast_integer(json_array_get(pair, 1), (int32_t)cfg_coast_get(header + 20),
                               (int32_t)cfg_coast_get(header + 24), &y);
        if (ok) {
            cfg_coast_put(replacement + i * 8, (uint32_t)x);
            cfg_coast_put(replacement + i * 8 + 4, (uint32_t)y);
        }
    }
    ok = ok && !memcmp(original + first * 8, replacement, 8) &&
         !memcmp(original + last * 8, replacement + (replacements - 1) * 8, 8);
    if (ok) {
        cfg_coast_put(header + 4, count - (uint32_t)(last - first + 1) + (uint32_t)replacements);
        ok = cfg_coast_write(output, header, 44) && cfg_coast_write(output, original, (size_t)first * 8) &&
             cfg_coast_write(output, replacement, replacements * 8) &&
             cfg_coast_write(output, original + (last + 1) * 8, (count - (size_t)last - 1) * 8);
    }
    free(original); free(replacement);
    return ok;
}

/* Keep the upstream archive intact; prepare a bounded corrected stream before opening any write transaction. */
bool cfg_database_coast_correct(file_t *input, file_t *output, file_t *corrections)
{
    file_info_t *info = file_get_info(corrections);
    bool ok = info && file_info_size(info) > 0 && file_info_size(info) <= 4 * 1024 * 1024;
    file_info_free(info);
    string_t *text = ok ? file_read_all_text(corrections) : NULL;
    json_t *document = text ? json_from_text(text) : NULL;
    string_free(text);
    const json_t *patches = osm_field(document, "patches");
    size_t count = json_array_size(patches), next = 0;
    int64_t revision, previous = -1;
    ok = ok && count > 0 && count <= 64 && cfg_coast_integer(osm_field(document, "revision"), 1, 1, &revision);
    for (size_t i = 0; ok && i < count; ++i) {
        int64_t polygon;
        ok = cfg_coast_integer(osm_field(json_array_get(patches, i), "polygon"), previous + 1, INT32_MAX, &polygon);
        if (ok) previous = polygon;
    }
    ok = ok && file_open_read(input) && file_open(output, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE);
    size_t total = 0;
    while (ok) {
        unsigned char header[44];
        size_t read;
        ok = file_read(input, header, sizeof header, &read);
        if (!ok || !read) break;
        if (read != sizeof header) { ok = false; break; }
        uint32_t points = cfg_coast_get(header + 4);
        if (points < 2 || points > 20000000 || (total += 44 + (size_t)points * 8) > 128 * 1024 * 1024) {
            ok = false;
            break;
        }
        const json_t *patch = next < count ? json_array_get(patches, next) : NULL;
        int64_t polygon = -1;
        if (patch) cfg_coast_integer(osm_field(patch, "polygon"), 0, INT32_MAX, &polygon);
        if ((int64_t)cfg_coast_get(header) == polygon) {
            ok = cfg_coast_ring_patch(input, output, header, patch);
            ++next;
        } else {
            ok = cfg_coast_write(output, header, sizeof header) && cfg_coast_copy(input, output, (size_t)points * 8);
        }
    }
    ok = file_close(input) && ok;
    ok = file_close(output) && ok && next == count;
    json_free(document);
    return ok;
}

/* Keep the optional geographic import separate from SQL statement ingestion. */
bool cfg_database_coast(sqlite_t *db, const string_t *root, const string_t *staging)
{
    string_t *packed_path = string_sprintf("%s/packaging/almanac-db/gshhs_f.b.zst", string_c_str(root));
    string_t *plain_path = string_sprintf("%s/shoreline.bin", string_c_str(staging));
    string_t *corrected_path = string_sprintf("%s/shoreline-corrected.bin", string_c_str(staging));
    string_t *patch_path = string_sprintf("%s/packaging/globe-db/shoreline-corrections.json", string_c_str(root));
    file_t *packed = packed_path ? file_new(packed_path) : NULL;
    file_t *plain = plain_path ? file_new(plain_path) : NULL;
    file_t *corrected = corrected_path ? file_new(corrected_path) : NULL;
    file_t *patch = patch_path ? file_new(patch_path) : NULL;
    bool exists = packed && file_exists(packed);
    bool ok = packed && plain && corrected && patch && exists && !file_last_error(packed);
    if (ok && exists) {
        cfg_progress_t *progress = cfg_progress_start("Indexing shorelines");
        ok = file_decompress_zstd(packed, plain, 128 * 1024 * 1024, false) &&
             cfg_database_coast_correct(plain, corrected, patch) &&
             file_open_read(corrected);
        bool transaction = ok && sqlite_exec_cstr(db, "begin immediate");
        ok = transaction && globe_import_shoreline(db, corrected) && sqlite_exec_cstr(db,
                 "update shoreline_dataset set source='GSHHG 2.3.7 + reviewed OSM shoreline corrections r1' where id=1") &&
             sqlite_exec_cstr(db, "commit");
        if (transaction && !ok) sqlite_exec_cstr(db, "rollback");
        if (!ok)
            fprintf(stderr, "Shoreline import failed (source error %d, staging error %d): %s\n",
                    file_last_error(packed), file_last_error(plain), string_c_str(sqlite_last_error(db)));
        file_close(plain);
        file_close(corrected);
        if (file_exists(plain) && !file_delete(plain)) ok = false;
        if (file_exists(corrected) && !file_delete(corrected)) ok = false;
        cfg_progress_finish(progress, ok);
    }
    file_free(packed);
    file_free(plain);
    file_free(corrected); file_free(patch);
    string_free(packed_path);
    string_free(plain_path);
    string_free(corrected_path); string_free(patch_path);
    return ok;
}

typedef struct { int32_t x, y; int64_t node; } osm_vertex_t;

typedef struct {
    sqlite_stmt_t *chunk, *bounds, *node;
    int64_t chunks, features, points, narrow_sections, unknown_width_sections, records;
    osm_vertex_t *vertices;
    size_t capacity;
    locale_t numeric_locale;
} osm_river_import_t;

/* Match the finest shoreline trial budget, not a fixed spacing or a vertex quota. */
static const double cfg_river_storage_metres = 8.0;

/* Independent coverage policy, not a consequence of the simplification error ceiling. */
static const double cfg_river_minimum_width_metres = 16.0;

/* Decimal GB: the installed river layer includes its tables and spatial/feature indexes. */
const int64_t cfg_database_river_storage_budget = INT64_C(3800000000);

static bool cfg_river_storage_bytes(sqlite_t *db, bool used_only, int64_t *bytes, int64_t *page_size)
{
    sqlite_stmt_t *q = sqlite_stmt_prepare(db,
        "select page_count,freelist_count,page_size from pragma_page_count,pragma_freelist_count,pragma_page_size");
    bool ok = q && sqlite_stmt_step(q) == SQLITE_STEP_ROW;
    int64_t pages = ok ? sqlite_stmt_column_int64(q, 0) : 0;
    int64_t free_pages = ok ? sqlite_stmt_column_int64(q, 1) : 0;
    int64_t size = ok ? sqlite_stmt_column_int64(q, 2) : 0;
    ok = ok && pages >= 0 && free_pages >= 0 && free_pages <= pages && size > 0 && pages <= INT64_MAX / size;
    if (ok) {
        *bytes = (pages - (used_only ? free_pages : 0)) * size;
        *page_size = size;
    }
    sqlite_stmt_finalize(q);
    return ok;
}

static bool cfg_river_candidate_fits(sqlite_t *db, int64_t budget)
{
    int64_t bytes = 0, page_size = 0;
    bool ok = cfg_river_storage_bytes(db, false, &bytes, &page_size) && bytes <= budget;
    if (!ok) fprintf(stderr, "River candidate exceeds the %lld-byte budget or its size could not be verified.\n",
                     (long long)budget);
    return ok;
}

static bool cfg_river_layer_fits(sqlite_t *db, int64_t baseline, int64_t budget)
{
    int64_t bytes = 0, page_size = 0;
    bool ok = cfg_river_storage_bytes(db, true, &bytes, &page_size) && bytes >= baseline &&
              page_size <= budget / 2 && bytes - baseline <= budget - 2 * page_size;
    /* Reserve two additional pages for shared header/schema overhead, including when free pages are reused. */
    if (!ok) fprintf(stderr, "River tables and indexes exceed the %lld-byte budget; replacement will be rolled back.\n",
                     (long long)budget);
    return ok;
}

static int32_t cfg_river_coordinate(const unsigned char *p)
{
    uint32_t n = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    return n <= INT32_MAX ? (int32_t)n : (int32_t)((int64_t)n - 4294967296LL);
}

static const json_t *osm_field(const json_t *value, const char *name)
{
    string_t *key = string_new_with(name);
    const json_t *result = key ? json_object_get(value, key) : NULL;
    string_free(key);
    return result;
}

static bool osm_text(const json_t *value, const char *text)
{
    const string_t *s = value ? json_string_value(value) : NULL;
    return s && !strcmp(string_c_str(s), text);
}

static bool osm_integer(const json_t *value, int64_t *out)
{
    const string_t *s = value ? json_number_text(value) : NULL;
    if (!s) return false;
    const char *text = string_c_str(s);
    char *end;
    errno = 0;
    long long n = strtoll(text, &end, 10);
    if (errno || end == text || *end || n <= 0) return false;
    *out = n;
    return true;
}

typedef struct { const char *first, *last; } osm_span_t;
typedef struct {
    osm_span_t coordinates, nodes;
    const char *copied;
    string_t *metadata;
} osm_record_t;

static void osm_json_space(const char **p, const char *end)
{
    while (*p < end && (**p == ' ' || **p == '\t' || **p == '\r' || **p == '\n')) ++*p;
}

static bool osm_json_take(const char **p, const char *end, char ch)
{
    osm_json_space(p, end);
    if (*p == end || **p != ch) return false;
    ++*p;
    return true;
}

/* Validate lexical strings without allocating a node for every coordinate or OSM reference. */
static bool osm_json_string(const char **p, const char *end)
{
    if (!osm_json_take(p, end, '"')) return false;
    while (*p < end) {
        unsigned char ch = (unsigned char)*(*p)++;
        if (ch == '"') return true;
        if (ch < 0x20) return false;
        if (ch == '\\') {
            if (*p == end) return false;
            ch = (unsigned char)*(*p)++;
            if (ch == 'u') {
                for (unsigned i = 0; i < 4; ++i)
                    if (*p == end || !isxdigit((unsigned char)*(*p)++)) return false;
            } else if (!strchr("\"\\/bfnrt", ch)) return false;
        }
    }
    return false;
}

static bool osm_json_number(const char **p, const char *end)
{
    if (*p < end && **p == '-') ++*p;
    if (*p == end) return false;
    if (**p == '0') ++*p;
    else {
        if (**p < '1' || **p > '9') return false;
        do { ++*p; } while (*p < end && **p >= '0' && **p <= '9');
    }
    if (*p < end && **p == '.') {
        ++*p;
        if (*p == end || **p < '0' || **p > '9') return false;
        do { ++*p; } while (*p < end && **p >= '0' && **p <= '9');
    }
    if (*p < end && (**p == 'e' || **p == 'E')) {
        ++*p;
        if (*p < end && (**p == '+' || **p == '-')) ++*p;
        if (*p == end || **p < '0' || **p > '9') return false;
        do { ++*p; } while (*p < end && **p >= '0' && **p <= '9');
    }
    return true;
}

/* Escaped field names retain the ordinary parser's interpretation; the usual ASCII keys allocate nothing. */
static bool osm_json_key(osm_span_t key, const char *name)
{
    size_t bytes = (size_t)(key.last - key.first);
    if (!memchr(key.first, '\\', bytes))
        return bytes == strlen(name) + 2 && !memcmp(key.first + 1, name, bytes - 2);
    string_t *text = string_new();
    bool ok = text && string_append_bytes(text, key.first, bytes) == 0;
    json_t *value = ok ? json_from_text(text) : NULL;
    ok = osm_text(value, name);
    json_free(value); string_free(text);
    return ok;
}

/* Scope is bounded: root, geometry and properties; nested unrelated objects cannot supply river fields. */
static bool osm_json_scan(const char **p, const char *end, unsigned depth, unsigned scope, osm_record_t *record)
{
    osm_json_space(p, end);
    if (*p == end || depth > 128) return false;
    if (**p == '"') return osm_json_string(p, end);
    if (**p == '[' || **p == '{') {
        bool object = *(*p)++ == '{';
        char close = object ? '}' : ']';
        if (osm_json_take(p, end, close)) return true;
        for (;;) {
            unsigned child = 0;
            osm_span_t *capture = NULL;
            if (object) {
                osm_json_space(p, end);
                osm_span_t key = {.first = *p};
                if (!osm_json_string(p, end)) return false;
                key.last = *p;
                if (!osm_json_take(p, end, ':')) return false;
                if (scope == 1 && osm_json_key(key, "geometry")) {
                    child = 2; record->coordinates = (osm_span_t){0};
                } else if (scope == 1 && osm_json_key(key, "properties")) {
                    child = 3; record->nodes = (osm_span_t){0};
                } else if (scope == 2 && osm_json_key(key, "coordinates")) capture = &record->coordinates;
                else if (scope == 3 && osm_json_key(key, "@way_nodes")) capture = &record->nodes;
            }
            osm_json_space(p, end);
            const char *start = *p;
            if (!osm_json_scan(p, end, depth + 1, child, record)) return false;
            if (capture) {
                *capture = (osm_span_t){start, *p};
                if (string_append_bytes(record->metadata, record->copied, (size_t)(start - record->copied)) != 0 ||
                    string_append_cstr(record->metadata, "null") < 0) return false;
                record->copied = *p;
            }
            if (osm_json_take(p, end, close)) return true;
            if (!osm_json_take(p, end, ',')) return false;
        }
    }
    static const char *const literals[] = {"null", "true", "false"};
    for (unsigned i = 0; i < 3; ++i) {
        size_t length = strlen(literals[i]);
        if ((size_t)(end - *p) >= length && !memcmp(*p, literals[i], length)) { *p += length; return true; }
    }
    return osm_json_number(p, end);
}

static bool osm_raw_coordinate(const char **p, const char *end, locale_t locale, int32_t *out, double limit)
{
    osm_json_space(p, end);
    char *tail;
    errno = 0;
    double value = strtod_l(*p, &tail, locale);
    if (errno || tail == *p || tail > end || !isfinite(value) || fabs(value) > limit) return false;
    *out = (int32_t)llround(value * 1e6);
    *p = tail;
    return true;
}

/* Reuse a bounded native vertex buffer; general-purpose arbitrary-precision JSON numbers are not needed here. */
static bool osm_raw_vertices(osm_river_import_t *state, const osm_record_t *record, size_t *count)
{
    const char *p = record->coordinates.first, *end = record->coordinates.last;
    size_t n = 0;
    if (!p || !record->nodes.first || !osm_json_take(&p, end, '[')) return false;
    do {
        if (n == 100000 || !osm_json_take(&p, end, '[')) return false;
        if (n == state->capacity) {
            size_t capacity = state->capacity ? state->capacity * 2 : 256;
            if (capacity > 100000) capacity = 100000;
            osm_vertex_t *vertices = realloc(state->vertices, capacity * sizeof(*vertices));
            if (!vertices) return false;
            state->vertices = vertices; state->capacity = capacity;
        }
        if (!osm_raw_coordinate(&p, end, state->numeric_locale, &state->vertices[n].x, 180) ||
            !osm_json_take(&p, end, ',') ||
            !osm_raw_coordinate(&p, end, state->numeric_locale, &state->vertices[n].y, 90) ||
            !osm_json_take(&p, end, ']')) return false;
        ++n;
    } while (osm_json_take(&p, end, ','));
    if (n < 2 || !osm_json_take(&p, end, ']') || p != end) return false;
    p = record->nodes.first; end = record->nodes.last;
    if (!osm_json_take(&p, end, '[')) return false;
    for (size_t i = 0; i < n; ++i) {
        if (i && !osm_json_take(&p, end, ',')) return false;
        osm_json_space(&p, end);
        char *tail;
        errno = 0;
        long long node = strtoll(p, &tail, 10);
        if (errno || tail == p || tail > end || node <= 0) return false;
        state->vertices[i].node = node;
        p = tail;
    }
    if (!osm_json_take(&p, end, ']') || p != end) return false;
    *count = n;
    return true;
}

static void osm_put(unsigned char *p, uint64_t value, unsigned bytes)
{
    for (unsigned i = 0; i < bytes; ++i) p[bytes - i - 1] = (unsigned char)(value >> (8 * i));
}

static bool osm_step(sqlite_stmt_t *q)
{
    bool ok = sqlite_stmt_step(q) == SQLITE_STEP_DONE;
    sqlite_stmt_reset(q);
    return ok;
}

static const char *osm_width_space(const char *text)
{
    while (isspace((unsigned char)*text)) ++text;
    return text;
}

/* OSM widths use decimal points, regardless of the installer's current locale. */
static bool osm_width_decimal(const char **text, double *value)
{
    const char *p = *text;
    double number = 0, place = 0.1;
    bool digits = false;
    while (*p >= '0' && *p <= '9') {
        number = number * 10 + (*p++ - '0');
        digits = true;
    }
    if (*p == '.') {
        ++p;
        while (*p >= '0' && *p <= '9') {
            number += (*p++ - '0') * place;
            place *= 0.1;
            digits = true;
        }
    }
    *text = p;
    *value = number;
    return digits && isfinite(number);
}

/* Unrecognised, range-valued or conflicting tags remain unknown; they never become a zero-width section. */
static bool osm_width_metres(const json_t *properties, double *width)
{
    const json_t *tag = osm_field(properties, "width");
    if (!tag) tag = osm_field(properties, "est_width");
    const string_t *value = tag ? json_string_value(tag) : NULL;
    if (!value && tag) value = json_number_text(tag);
    if (!value || !string_byte_length(value) || string_byte_length(value) > 128) return false;
    const char *text = string_c_str(value);
    if (strlen(text) != string_byte_length(value)) return false;
    const char *end = osm_width_space(text);
    double number;
    if (!osm_width_decimal(&end, &number)) return false;
    if (*end == '\'') {
        if (floor(number) != number) return false;
        ++end;
        double inches;
        if (!osm_width_decimal(&end, &inches) || inches >= 12 || *end++ != '"' || *osm_width_space(end))
            return false;
        number = number * 0.3048 + inches * 0.0254;
    } else {
        const char *unit = osm_width_space(end);
        if (*unit) {
            if (unit == end) return false;
            /* This fixed eight-entry unit vocabulary has bounded scan cost. */
            static const struct { const char *name; double metres; } units[] = {
                {"m", 1}, {"cm", 0.01}, {"mm", 0.001}, {"km", 1000},
                {"ft", 0.3048}, {"in", 0.0254}, {"yd", 0.9144}, {"mi", 1609.344}
            };
            bool known = false;
            for (size_t i = 0; i < sizeof units / sizeof *units; ++i) {
                size_t n = strlen(units[i].name);
                if (!strncmp(unit, units[i].name, n) && !*osm_width_space(unit + n)) {
                    number *= units[i].metres;
                    known = true;
                    break;
                }
            }
            if (!known) return false;
        }
    }
    if (!isfinite(number) || number <= 0) return false;
    *width = number;
    return true;
}

static bool osm_line(osm_river_import_t *s, const json_t *feature, const osm_record_t *record)
{
    if (!osm_text(osm_field(feature, "type"), "Feature")) return false;
    const json_t *geometry = osm_field(feature, "geometry"), *properties = osm_field(feature, "properties");
    if (!geometry || !properties) return false;
    /* Area outlines must never be passed off as river centrelines. */
    if (osm_text(osm_field(geometry, "type"), "MultiPolygon") ||
        osm_text(osm_field(geometry, "type"), "Polygon")) return true;
    if (!osm_text(osm_field(geometry, "type"), "LineString")) return false;
    static const char *const kinds[] = {"river", "stream", "canal", "tidal_channel"};
    unsigned kind = 0;
    for (unsigned i = 0; i < 4; ++i)
        if (osm_text(osm_field(properties, "waterway"), kinds[i])) kind = i + 1;
    if (!kind) return true;
    int64_t id;
    if (!osm_text(osm_field(properties, "@type"), "way") ||
        !osm_integer(osm_field(properties, "@id"), &id)) return false;
    size_t n;
    if (!osm_raw_vertices(s, record, &n)) return false;
    double width;
    if (osm_width_metres(properties, &width)) {
        if (width < cfg_river_minimum_width_metres) {
            ++s->narrow_sections;
            return true;
        }
    } else ++s->unknown_width_sections;
    /* Each OSM way is independent: neither names nor shared nodes propagate a width exclusion. */
    bool dateline = false;
    int32_t last = 0;
    for (size_t i = 0; i < n; ++i) {
        int32_t x = s->vertices[i].x;
        if (i && llabs((int64_t)x - last) > 180000000) dateline = true;
        last = x;
    }
    unsigned char vertices[2056], references[2056];
    size_t used = 0;
    int32_t west = INT32_MAX, east = INT32_MIN, south = INT32_MAX, north = INT32_MIN, previous = 0;
    for (size_t i = 0; i < n; ++i) {
        int32_t x = s->vertices[i].x, y = s->vertices[i].y;
        int64_t node = s->vertices[i].node;
        /* Unwrap date-line crossings into the reader's [-180,360] interval. */
        if (dateline && x < 0) x += 360000000;
        if (i && llabs((int64_t)x - previous) > 180000000) return false;
        previous = x;
        if (!sqlite_stmt_bind_int64(s->node, 1, node) || !osm_step(s->node)) return false;
        osm_put(vertices + used * 8, (uint32_t)x, 4);
        osm_put(vertices + used * 8 + 4, (uint32_t)y, 4);
        osm_put(references + used * 8, (uint64_t)node, 8);
        ++used;
        if (x < west) west = x;
        if (x > east) east = x;
        if (y < south) south = y;
        if (y > north) north = y;
        if (used == 257 || i + 1 == n) {
            if (used > 1) {
                ++s->chunks;
                if (!sqlite_stmt_bind_int64(s->chunk, 1, s->chunks) || !sqlite_stmt_bind_int64(s->chunk, 2, id) ||
                    !sqlite_stmt_bind_int(s->chunk, 3, (int)kind) ||
                    !sqlite_stmt_bind_blob(s->chunk, 4, vertices, used * 8) ||
                    !sqlite_stmt_bind_blob(s->chunk, 5, references, used * 8) || !osm_step(s->chunk) ||
                    !sqlite_stmt_bind_int64(s->bounds, 1, s->chunks) ||
                    !sqlite_stmt_bind_int(s->bounds, 2, west) || !sqlite_stmt_bind_int(s->bounds, 3, east) ||
                    !sqlite_stmt_bind_int(s->bounds, 4, south) || !sqlite_stmt_bind_int(s->bounds, 5, north) ||
                    !osm_step(s->bounds)) return false;
            }
            memcpy(vertices, vertices + (used - 1) * 8, 8);
            memcpy(references, references + (used - 1) * 8, 8);
            used = 1;
            west = east = x;
            south = north = y;
        }
    }
    s->points += (int64_t)n;
    ++s->features;
    return true;
}

static bool osm_anchors(sqlite_t *db)
{
    sqlite_stmt_t *rows = sqlite_stmt_prepare(db, "select id,node_ids from rivers_chunk order by id");
    sqlite_stmt_t *count = sqlite_stmt_prepare(db, "select uses from rivers_node_use where id=?1");
    sqlite_stmt_t *update = sqlite_stmt_prepare(db, "update rivers_chunk set anchors=?1,node_ids=NULL where id=?2");
    bool ok = rows && count && update;
    sqlite_step_result_t step = SQLITE_STEP_DONE;
    while (ok && (step = sqlite_stmt_step(rows)) == SQLITE_STEP_ROW) {
        size_t bytes = sqlite_stmt_column_bytes(rows, 1), n = bytes / 8;
        const unsigned char *p = sqlite_stmt_column_blob(rows, 1);
        unsigned char anchors[257];
        if (!p || bytes % 8 || n < 2 || n > 257) { ok = false; break; }
        for (size_t i = 0; ok && i < n; ++i) {
            uint64_t id = 0;
            for (unsigned j = 0; j < 8; ++j) id = (id << 8) | p[i * 8 + j];
            ok = sqlite_stmt_bind_int64(count, 1, (int64_t)id) && sqlite_stmt_step(count) == SQLITE_STEP_ROW;
            anchors[i] = !i || i + 1 == n || (ok && sqlite_stmt_column_int(count, 0) > 1);
            sqlite_stmt_reset(count);
        }
        ok = ok && sqlite_stmt_bind_blob(update, 1, anchors, n) &&
             sqlite_stmt_bind_int64(update, 2, sqlite_stmt_column_int64(rows, 0)) && osm_step(update);
    }
    sqlite_stmt_finalize(rows); sqlite_stmt_finalize(count); sqlite_stmt_finalize(update);
    return ok && step == SQLITE_STEP_DONE;
}

typedef struct { int64_t x, y; } cfg_river_point;

static int64_t cfg_river_turn(cfg_river_point a, cfg_river_point b, cfg_river_point c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

static bool cfg_river_same(cfg_river_point a, cfg_river_point b)
{
    return a.x == b.x && a.y == b.y;
}

/* Endpoint contact is allowed; crossings, interior touches and collinear overlaps are not. */
static bool cfg_river_conflict(cfg_river_point a, cfg_river_point b, cfg_river_point c, cfg_river_point d)
{
    int64_t ab_c = cfg_river_turn(a, b, c), ab_d = cfg_river_turn(a, b, d);
    int64_t cd_a = cfg_river_turn(c, d, a), cd_b = cfg_river_turn(c, d, b);
    if (fmax(a.x, b.x) < fmin(c.x, d.x) || fmax(c.x, d.x) < fmin(a.x, b.x) ||
        fmax(a.y, b.y) < fmin(c.y, d.y) || fmax(c.y, d.y) < fmin(a.y, b.y)) return false;
    if ((ab_c > 0 && ab_d > 0) || (ab_c < 0 && ab_d < 0) ||
        (cd_a > 0 && cd_b > 0) || (cd_a < 0 && cd_b < 0)) return false;
    bool shared = cfg_river_same(a, c) || cfg_river_same(a, d) || cfg_river_same(b, c) || cfg_river_same(b, d);
    if (!shared) return true;
    if (ab_c || ab_d || cd_a || cd_b) return false;
    return fmax(fmin(a.x, b.x), fmin(c.x, d.x)) < fmin(fmax(a.x, b.x), fmax(c.x, d.x)) ||
           fmax(fmin(a.y, b.y), fmin(c.y, d.y)) < fmin(fmax(a.y, b.y), fmax(c.y, d.y));
}

static void cfg_river_reduce(const cfg_river_point *p, unsigned char *keep, size_t first, size_t last)
{
    if (last <= first + 1) return;
    /* 112 km per degree conservatively bounds either geographic axis, also at high latitudes. */
    double limit = cfg_river_storage_metres / 0.112, maximum = limit * limit;
    double dx = p[last].x - p[first].x, dy = p[last].y - p[first].y, length = dx * dx + dy * dy;
    size_t split = first;
    for (size_t i = first + 1; i < last; ++i) {
        double x = p[i].x - p[first].x, y = p[i].y - p[first].y;
        double t = length ? fmax(0, fmin(1, (x * dx + y * dy) / length)) : 0;
        double ex = x - t * dx, ey = y - t * dy, error = ex * ex + ey * ey;
        if (error > maximum) { maximum = error; split = i; }
    }
    if (split != first) {
        keep[split] = 1;
        cfg_river_reduce(p, keep, first, split);
        cfg_river_reduce(p, keep, split, last);
    }
}

/* Use the existing spatial index; crowded neighbourhoods retain full geometry instead of consuming unbounded work. */
static bool cfg_river_neighbours(sqlite_stmt_t *query, int64_t chunk, cfg_river_point a, cfg_river_point b,
                                 bool *safe)
{
    static const int64_t shifts[] = {0, -360000000, 360000000};
    for (size_t k = 0; *safe && k < sizeof shifts / sizeof *shifts; ++k) {
        cfg_river_point c = {a.x + shifts[k], a.y}, d = {b.x + shifts[k], b.y};
        if (fmax(c.x, d.x) < -180000000 || fmin(c.x, d.x) > 360000000) continue;
        bool ok = sqlite_stmt_bind_int64(query, 1, chunk) &&
                  sqlite_stmt_bind_int64(query, 2, (int64_t)fmax(c.x, d.x)) &&
                  sqlite_stmt_bind_int64(query, 3, (int64_t)fmin(c.x, d.x)) &&
                  sqlite_stmt_bind_int64(query, 4, (int64_t)fmax(c.y, d.y)) &&
                  sqlite_stmt_bind_int64(query, 5, (int64_t)fmin(c.y, d.y));
        sqlite_step_result_t step = SQLITE_STEP_DONE;
        unsigned seen = 0;
        while (ok && *safe && (step = sqlite_stmt_step(query)) == SQLITE_STEP_ROW) {
            if (++seen > 128) { *safe = false; break; }
            const unsigned char *v = sqlite_stmt_column_blob(query, 0);
            size_t bytes = sqlite_stmt_column_bytes(query, 0);
            if (!v || bytes < 16 || bytes > 2056 || bytes % 8) { ok = false; break; }
            cfg_river_point previous = {cfg_river_coordinate(v), cfg_river_coordinate(v + 4)};
            for (size_t j = 8; *safe && j < bytes; j += 8) {
                cfg_river_point next = {cfg_river_coordinate(v + j), cfg_river_coordinate(v + j + 4)};
                if (cfg_river_conflict(c, d, previous, next)) *safe = false;
                previous = next;
            }
        }
        ok = ok && (!*safe || step == SQLITE_STEP_DONE);
        sqlite_stmt_reset(query);
        if (!ok) return false;
    }
    return true;
}

/* Reduce the candidate, never the source extract; anchors and whole-feature identities survive unchanged. */
static bool cfg_river_simplify(sqlite_t *db, int64_t *points)
{
    sqlite_stmt_t *rows = sqlite_stmt_prepare(db, "select id,vertices,anchors from rivers_chunk order by id");
    sqlite_stmt_t *update = sqlite_stmt_prepare(db, "update rivers_chunk set vertices=?1,anchors=?2 where id=?3");
    sqlite_stmt_t *neighbours = sqlite_stmt_prepare(db,
        "select c.vertices from rivers_bounds b join rivers_chunk c on c.id=b.id "
        "where b.id<>?1 and b.min_x<=?2 and b.max_x>=?3 and b.min_y<=?4 and b.max_y>=?5 limit 129");
    bool ok = rows && update && neighbours;
    sqlite_step_result_t step = SQLITE_STEP_DONE;
    while (ok && (step = sqlite_stmt_step(rows)) == SQLITE_STEP_ROW) {
        size_t bytes = sqlite_stmt_column_bytes(rows, 1), n = bytes / 8;
        const unsigned char *v = sqlite_stmt_column_blob(rows, 1), *a = sqlite_stmt_column_blob(rows, 2);
        if (!v || !a || n < 2 || n > 257 || bytes % 8 || sqlite_stmt_column_bytes(rows, 2) != n) {
            ok = false; break;
        }
        unsigned char vertices[2056], anchors[257], keep[257];
        cfg_river_point p[257];
        memcpy(vertices, v, bytes); memcpy(anchors, a, n); memcpy(keep, a, n);
        for (size_t i = 0; i < n; ++i)
            p[i] = (cfg_river_point){cfg_river_coordinate(v + i * 8), cfg_river_coordinate(v + i * 8 + 4)};
        size_t first = 0;
        for (size_t i = 1; i < n; ++i) if (keep[i]) { cfg_river_reduce(p, keep, first, i); first = i; }
        size_t selected[257], used = 0;
        for (size_t i = 0; i < n; ++i) if (keep[i]) selected[used++] = i;
        if (used == n) continue;
        /* Do not collapse small loops; keep all geometry if a shortcut crosses another retained segment. */
        bool safe = used >= 2 && !(cfg_river_same(p[0], p[n - 1]) && used < 4);
        for (size_t i = 1; safe && i < used; ++i) {
            if (selected[i] == selected[i - 1] + 1) continue;
            cfg_river_point start = p[selected[i - 1]], end = p[selected[i]];
            if (cfg_river_same(start, end)) { safe = false; break; }
            for (size_t j = 1; safe && j < used; ++j)
                if (j != i && cfg_river_conflict(start, end, p[selected[j - 1]], p[selected[j]])) safe = false;
            if (safe) ok = cfg_river_neighbours(neighbours, sqlite_stmt_column_int64(rows, 0), start, end, &safe);
            if (!ok) break;
        }
        if (!ok || !safe) continue;
        for (size_t i = 0; i < used; ++i) {
            memmove(vertices + i * 8, vertices + selected[i] * 8, 8);
            anchors[i] = anchors[selected[i]];
        }
        ok = sqlite_stmt_bind_blob(update, 1, vertices, used * 8) &&
             sqlite_stmt_bind_blob(update, 2, anchors, used) &&
             sqlite_stmt_bind_int64(update, 3, sqlite_stmt_column_int64(rows, 0)) && osm_step(update);
        if (ok) *points -= (int64_t)(n - used);
    }
    sqlite_stmt_finalize(rows); sqlite_stmt_finalize(update); sqlite_stmt_finalize(neighbours);
    return ok && step == SQLITE_STEP_DONE;
}

static bool osm_records_feed(osm_river_import_t *state, string_t *record, const unsigned char *input, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        if (input[i] == 0x1e) {
            if (string_byte_length(record)) return false;
        } else if (input[i] == '\n') {
            if (string_byte_length(record)) {
                const char *p = string_c_str(record), *end = p + string_byte_length(record);
                osm_record_t parsed = {.copied = p, .metadata = string_new()};
                bool ok = parsed.metadata && osm_json_scan(&p, end, 0, 1, &parsed);
                osm_json_space(&p, end);
                ok = ok && p == end &&
                     string_append_bytes(parsed.metadata, parsed.copied, (size_t)(end - parsed.copied)) == 0;
                json_t *feature = ok ? json_from_text(parsed.metadata) : NULL;
                ok = feature && osm_line(state, feature, &parsed);
                json_free(feature);
                string_free(parsed.metadata);
                if (!ok) {
                    fprintf(stderr, "River record %lld failed validation or storage (%zu bytes).\n",
                            (long long)state->records + 1, string_byte_length(record));
                    return false;
                }
                string_clear(record);
                if (++state->records % 100000 == 0) {
                    printf("River input: %lld records, %lld sections retained, %lld narrow sections omitted.\n",
                           (long long)state->records, (long long)state->features, (long long)state->narrow_sections);
                    fflush(stdout);
                }
            }
        } else {
            size_t end = i + 1;
            while (end < length && input[end] != '\n' && input[end] != 0x1e) ++end;
            size_t bytes = end - i;
            if (string_byte_length(record) > 4 * 1024 * 1024 - bytes ||
                string_append_bytes(record, (const char *)(input + i), bytes) != 0) return false;
            i = end - 1;
        }
    }
    return true;
}

/* Stream either plain records or exactly one bounded Zstandard frame; never create an expanded staging file. */
static bool osm_records_read(osm_river_import_t *state, file_t *source)
{
    unsigned char input[65536], output[65536];
    size_t length = 0;
    bool ok = true;
    while (ok && length < 4) {
        size_t read = 0;
        ok = file_read(source, input + length, 4 - length, &read);
        length += read;
        if (!read) break;
    }
    bool compressed = ok && length >= 4 && !memcmp(input, "\x28\xb5\x2f\xfd", 4);
    ZSTD_DCtx *decoder = compressed ? ZSTD_createDCtx() : NULL;
    string_t *record = string_new();
    ok = ok && record && (!compressed || (decoder &&
         !ZSTD_isError(ZSTD_DCtx_setParameter(decoder, ZSTD_d_windowLogMax, 23))));
    const uint64_t limit = UINT64_C(128) * 1024 * 1024 * 1024;
    uint64_t decoded = 0;
    bool complete = false;
    while (ok && length) {
        if (compressed) {
            if (complete) { ok = false; break; }
            ZSTD_inBuffer in = {input, length, 0};
            size_t produced;
            do {
                ZSTD_outBuffer out = {output, sizeof output, 0};
                size_t remaining = ZSTD_decompressStream(decoder, &out, &in);
                ok = !ZSTD_isError(remaining) && out.pos <= limit - decoded &&
                     osm_records_feed(state, record, output, out.pos);
                if (!ok) break;
                decoded += out.pos;
                if (!remaining) { complete = true; ok = in.pos == in.size; break; }
                produced = out.pos;
            } while (in.pos < in.size || produced == sizeof output);
        } else {
            ok = length <= limit - decoded && osm_records_feed(state, record, input, length);
            if (ok) decoded += length;
        }
        if (ok) ok = file_read(source, input, sizeof input, &length);
    }
    ok = ok && (!compressed || complete) && record && !string_byte_length(record) && state->features > 0;
    ZSTD_freeDCtx(decoder);
    string_free(record);
    return ok;
}

static double cfg_river_seconds(void)
{
    struct timespec now;
    return clock_gettime(CLOCK_MONOTONIC, &now) == 0 ? now.tv_sec + now.tv_nsec / 1e9 : 0;
}

/* Publish reduced centrelines only after records, junctions and the storage-detail pass have been validated. */
bool cfg_database_import_osm_rivers(sqlite_t *db, file_t *source)
{
    if (!db || !source || !sqlite_exec_cstr(db, "begin immediate")) return false;
    bool ok = sqlite_exec_cstr(db,
        "drop table if exists rivers_bounds;drop table if exists rivers_chunk;drop table if exists rivers_dataset;"
        "create table rivers_dataset(id integer primary key,format_version integer,source text,point_count integer,"
        "feature_count integer,source_point_count integer,storage_tolerance_metres real);"
        "create table rivers_chunk(id integer primary key,feature_id integer,kind integer,vertices blob,node_ids blob,anchors blob);"
        "create index rivers_feature_chunks on rivers_chunk(feature_id,id);"
        "create virtual table rivers_bounds using rtree_i32(id,min_x,max_x,min_y,max_y);"
        "create table rivers_node_use(id integer primary key,uses integer not null);");
    osm_river_import_t state = {0};
    state.numeric_locale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
    ok = ok && state.numeric_locale;
    if (ok) {
        state.chunk = sqlite_stmt_prepare(db, "insert into rivers_chunk values(?1,?2,?3,?4,?5,NULL)");
        state.bounds = sqlite_stmt_prepare(db, "insert into rivers_bounds values(?1,?2,?3,?4,?5)");
        state.node = sqlite_stmt_prepare(db,
            "insert into rivers_node_use values(?1,1) on conflict(id) do update set uses=2");
        ok = state.chunk && state.bounds && state.node;
    }
    /* A final newline and, when compressed, a complete frame are required before publication. */
    double phase = cfg_river_seconds();
    if (ok) ok = osm_records_read(&state, source);
    free(state.vertices);
    if (state.numeric_locale) freelocale(state.numeric_locale);
    sqlite_stmt_finalize(state.chunk); sqlite_stmt_finalize(state.bounds); sqlite_stmt_finalize(state.node);
    if (ok) {
        printf("River input complete: %lld records in %.1f s; resolving shared junctions.\n",
               (long long)state.records, cfg_river_seconds() - phase);
        fflush(stdout);
    }
    phase = cfg_river_seconds();
    if (ok) ok = osm_anchors(db);
    if (ok) {
        printf("River junctions complete in %.1f s.\n", cfg_river_seconds() - phase);
        fflush(stdout);
    }
    int64_t source_points = state.points;
    cfg_progress_t *progress = ok ? cfg_progress_start("Reducing stored rivers to shoreline detail") : NULL;
    if (ok) ok = cfg_river_simplify(db, &state.points);
    cfg_progress_finish(progress, ok);
    if (ok) {
        sqlite_stmt_t *meta = sqlite_stmt_prepare(db,
            "insert into rivers_dataset values(1,2,'OpenStreetMap centreline export / ODbL',?1,?2,?3,?4)");
        ok = meta && sqlite_stmt_bind_int64(meta, 1, state.points) &&
             sqlite_stmt_bind_int64(meta, 2, state.features) && sqlite_stmt_bind_int64(meta, 3, source_points) &&
             sqlite_stmt_bind_double(meta, 4, cfg_river_storage_metres) && osm_step(meta);
        sqlite_stmt_finalize(meta);
    }
    if (ok) ok = sqlite_exec_cstr(db, "drop table rivers_node_use;commit");
    if (!ok) sqlite_exec_cstr(db, "rollback");
    if (ok) printf("River width selection: %lld sections retained, %lld below %.0f m omitted, %lld retained with unknown width.\n",
                   (long long)state.features, (long long)state.narrow_sections, cfg_river_minimum_width_metres,
                   (long long)state.unknown_width_sections);
    return ok;
}

/* Build a candidate separately from the working globe database. */
int cfg_globe_river_stage(const char *input_path, const char *output_path)
{
    string_t *home = cfg_storage_home();
    string_t *config_path = home ? string_sprintf("%s/config/globe-db.env", string_c_str(home)) : NULL;
    file_t *config = config_path ? file_new(config_path) : NULL;
    const char *environment = getenv("MARS_GLOBE_DB_KEY");
    string_t *key = environment && *environment ? string_new_with(environment) :
                    config ? cfg_storage_setting(config, "MARS_GLOBE_DB_KEY") : NULL;
    file_t *input = file_new_cstr(input_path), *output = file_new_cstr(output_path);
    string_t *path = string_new_with(output_path);
    bool ok = key && string_byte_length(key) && input && output && path && file_open_read(input);
    if (ok) ok = file_open(output, FILE_MODE_CREATE_NEW, FILE_ACCESS_WRITE);
    if (output && file_is_open(output)) ok = file_close(output) && ok;
    if (ok) ok = file_chmod(output, 0600);
    sqlite_t *db = ok ? sqlite_open_encrypted(path, key) : NULL;
    ok = ok && db && sqlite_exec_cstr(db, "PRAGMA cache_size=-8192;PRAGMA temp_store=FILE;PRAGMA temp.cache_size=-1024;");
    if (ok) {
        puts("Staging rivers with an 8 m error ceiling and a 3.8 GB final size limit; installed globe unchanged.");
        fflush(stdout);
        ok = cfg_database_import_osm_rivers(db, input);
    }
    if (ok) ok = sqlite_exec_cstr(db, "VACUUM") && cfg_river_candidate_fits(db, cfg_database_river_storage_budget);
    sqlite_stmt_t *totals = ok ? sqlite_stmt_prepare(db,
        "select source_point_count,point_count,feature_count from rivers_dataset where id=1") : NULL;
    if (ok) {
        ok = totals && sqlite_stmt_step(totals) == SQLITE_STEP_ROW;
        if (ok) printf("River candidate: %lld source points, %lld stored points, %lld features.\n",
                       (long long)sqlite_stmt_column_int64(totals, 0),
                       (long long)sqlite_stmt_column_int64(totals, 1),
                       (long long)sqlite_stmt_column_int64(totals, 2));
    }
    sqlite_stmt_finalize(totals);
    sqlite_stmt_t *check = ok ? sqlite_stmt_prepare(db, "PRAGMA integrity_check") : NULL;
    if (ok) {
        const char *result = check && sqlite_stmt_step(check) == SQLITE_STEP_ROW ? sqlite_stmt_column_text(check, 0) : NULL;
        string_t *text = result ? string_new_with(result) : NULL;
        ok = text && string_view_equals_literal(string_view_all(text), "ok");
        string_free(text);
    }
    sqlite_stmt_finalize(check);
    sqlite_close(db);
    file_free(input); file_free(output); file_free(config);
    string_free(path); string_free(key); string_free(config_path); string_free(home);
    if (ok) puts("Encrypted river candidate ready for geographic review; not installed.");
    else fputs("River candidate failed; installed globe unchanged. Do not install the failed candidate.\n", stderr);
    return ok ? 0 : 1;
}

static bool cfg_river_step(sqlite_stmt_t *q)
{
    bool ok = sqlite_stmt_step(q) == SQLITE_STEP_DONE;
    sqlite_stmt_reset(q);
    return ok;
}

/* Copy only validated geometry, with bounded RAM and an all-or-nothing destination transaction. */
bool cfg_database_publish_rivers_limited(sqlite_t *destination, sqlite_t *source, int64_t budget)
{
    if (!destination || !source || destination == source || budget <= 0 || budget > cfg_database_river_storage_budget ||
        !sqlite_exec_cstr(source, "begin")) return false;
    sqlite_stmt_t *meta = sqlite_stmt_prepare(source,
        "select point_count,feature_count,source_point_count,storage_tolerance_metres "
        "from rivers_dataset where id=1 and format_version=2 "
        "and source='OpenStreetMap centreline export / ODbL'");
    bool ok = meta && sqlite_stmt_step(meta) == SQLITE_STEP_ROW;
    int64_t points = ok ? sqlite_stmt_column_int64(meta, 0) : 0;
    int64_t features = ok ? sqlite_stmt_column_int64(meta, 1) : 0;
    int64_t source_points = ok ? sqlite_stmt_column_int64(meta, 2) : 0;
    double tolerance = ok ? sqlite_stmt_column_double(meta, 3) : NAN;
    sqlite_stmt_finalize(meta);
    ok = ok && points >= 2 && features > 0 && features <= points / 2 && source_points >= points &&
         isfinite(tolerance) && tolerance > 0 && tolerance <= cfg_river_storage_metres;
    if (ok) ok = cfg_river_candidate_fits(source, budget);
    bool transaction = ok && sqlite_exec_cstr(destination, "begin immediate");
    ok = ok && transaction && sqlite_exec_cstr(destination,
        "drop table if exists rivers_bounds;drop table if exists rivers_chunk;drop table if exists rivers_dataset;");
    int64_t baseline = 0, page_size = 0;
    if (ok) ok = cfg_river_storage_bytes(destination, true, &baseline, &page_size);
    ok = ok && sqlite_exec_cstr(destination,
        "create table rivers_dataset(id integer primary key,format_version integer,source text,"
        "point_count integer,feature_count integer,source_point_count integer,storage_tolerance_metres real);"
        "create table rivers_chunk(id integer primary key,feature_id integer,kind integer,vertices blob,anchors blob);"
        "create index rivers_feature_chunks on rivers_chunk(feature_id,id);"
        "create virtual table rivers_bounds using rtree_i32(id,min_x,max_x,min_y,max_y);");
    sqlite_stmt_t *rows = ok ? sqlite_stmt_prepare(source,
        "select feature_id,kind,vertices,anchors from rivers_chunk order by feature_id,id") : NULL;
    sqlite_stmt_t *chunk = ok ? sqlite_stmt_prepare(destination,
        "insert into rivers_chunk values(?1,?2,?3,?4,?5)") : NULL;
    sqlite_stmt_t *bounds = ok ? sqlite_stmt_prepare(destination,
        "insert into rivers_bounds values(?1,?2,?3,?4,?5)") : NULL;
    int64_t chunks = 0, counted_points = 0, counted_features = 0, previous_id = -1;
    int previous_kind = 0;
    unsigned char previous_endpoint[8] = {0};
    ok = ok && rows && chunk && bounds;
    sqlite_step_result_t step = SQLITE_STEP_DONE;
    while (ok && (step = sqlite_stmt_step(rows)) == SQLITE_STEP_ROW) {
        int64_t id = sqlite_stmt_column_int64(rows, 0);
        int kind = sqlite_stmt_column_int(rows, 1);
        const unsigned char *p = sqlite_stmt_column_blob(rows, 2), *anchors = sqlite_stmt_column_blob(rows, 3);
        size_t bytes = sqlite_stmt_column_bytes(rows, 2), n = bytes / 8;
        ok = id > 0 && kind >= 1 && kind <= 4 && p && anchors && bytes >= 16 && bytes <= 2056 &&
             bytes % 8 == 0 && sqlite_stmt_column_bytes(rows, 3) == n && anchors[0] && anchors[n - 1];
        if (!ok) break;
        if (id == previous_id) {
            ok = kind == previous_kind && !memcmp(previous_endpoint, p, 8);
            --counted_points;
        } else ++counted_features;
        int32_t west = INT32_MAX, east = INT32_MIN, south = INT32_MAX, north = INT32_MIN, previous_x = 0;
        for (size_t i = 0; ok && i < n; ++i) {
            int32_t x = cfg_river_coordinate(p + i * 8), y = cfg_river_coordinate(p + i * 8 + 4);
            ok = x >= -180000000 && x <= 360000000 && y >= -90000000 && y <= 90000000 && anchors[i] <= 1 &&
                 (!i || llabs((int64_t)x - previous_x) <= 180000000);
            previous_x = x;
            if (x < west) west = x;
            if (x > east) east = x;
            if (y < south) south = y;
            if (y > north) north = y;
        }
        ++chunks;
        counted_points += (int64_t)n;
        previous_id = id;
        previous_kind = kind;
        memcpy(previous_endpoint, p + bytes - 8, 8);
        ok = ok && sqlite_stmt_bind_int64(chunk, 1, chunks) && sqlite_stmt_bind_int64(chunk, 2, id) &&
             sqlite_stmt_bind_int(chunk, 3, kind) && sqlite_stmt_bind_blob(chunk, 4, p, bytes) &&
             sqlite_stmt_bind_blob(chunk, 5, anchors, n) && cfg_river_step(chunk) &&
             sqlite_stmt_bind_int64(bounds, 1, chunks) && sqlite_stmt_bind_int(bounds, 2, west) &&
             sqlite_stmt_bind_int(bounds, 3, east) && sqlite_stmt_bind_int(bounds, 4, south) &&
             sqlite_stmt_bind_int(bounds, 5, north) && cfg_river_step(bounds);
        if (ok && chunks % 128 == 0) ok = cfg_river_layer_fits(destination, baseline, budget);
    }
    sqlite_stmt_finalize(rows); sqlite_stmt_finalize(chunk); sqlite_stmt_finalize(bounds);
    ok = ok && step == SQLITE_STEP_DONE && counted_features == features && counted_points == points;
    meta = ok ? sqlite_stmt_prepare(destination,
        "insert into rivers_dataset values(1,2,'OpenStreetMap centreline export / ODbL',?1,?2,?3,?4)") : NULL;
    ok = ok && meta && sqlite_stmt_bind_int64(meta, 1, points) &&
         sqlite_stmt_bind_int64(meta, 2, features) && sqlite_stmt_bind_int64(meta, 3, source_points) &&
         sqlite_stmt_bind_double(meta, 4, tolerance) && cfg_river_step(meta);
    sqlite_stmt_finalize(meta);
    if (ok) ok = cfg_river_layer_fits(destination, baseline, budget);
    if (!sqlite_exec_cstr(source, "rollback")) ok = false;
    if (ok) ok = sqlite_exec_cstr(destination, "commit");
    if (!ok && transaction) sqlite_exec_cstr(destination, "rollback");
    return ok;
}

/* Production publication always applies the full approved limit, not a reduced test budget. */
bool cfg_database_publish_rivers(sqlite_t *destination, sqlite_t *source)
{
    return cfg_database_publish_rivers_limited(destination, source, cfg_database_river_storage_budget);
}

static string_t *cfg_river_setting(file_t *config, const char *name)
{
    const char *environment = getenv(name);
    return environment && *environment ? string_new_with(environment) : cfg_storage_setting(config, name);
}

/* An explicit install reuses the saved key and preserves all non-river tables. */
int cfg_globe_river_install(const char *candidate_path)
{
    string_t *home = cfg_storage_home();
    string_t *config_path = home ? string_sprintf("%s/config/globe-db.env", string_c_str(home)) : NULL;
    file_t *config = config_path ? file_new(config_path) : NULL;
    string_t *key = config ? cfg_river_setting(config, "MARS_GLOBE_DB_KEY") : NULL;
    string_t *raw = config ? cfg_river_setting(config, "MARS_GLOBE_DB_PATH") : NULL;
    if (raw && !string_byte_length(raw)) {
        string_free(raw);
        raw = string_sprintf("%s/globe/globe.db", string_c_str(home));
    }
    string_t *path = raw ? cfg_storage_expand_path(raw) : NULL;
    string_t *candidate = candidate_path ? string_new_with(candidate_path) : NULL;
    file_t *target_file = path ? file_new(path) : NULL, *source_file = candidate ? file_new(candidate) : NULL;
    bool target_exists = false, source_exists = false;
    bool ok = key && string_byte_length(key) && target_file && source_file &&
              cfg_storage_safe_regular(target_file, &target_exists) && target_exists &&
              cfg_storage_safe_regular(source_file, &source_exists) && source_exists;
    sqlite_t *source = ok ? sqlite_open_encrypted(candidate, key) : NULL;
    sqlite_t *target = source ? sqlite_open_encrypted(path, key) : NULL;
    ok = ok && source && target &&
         sqlite_exec_cstr(source, "PRAGMA query_only=ON;PRAGMA cache_size=-8192;") &&
         sqlite_exec_cstr(target, "PRAGMA cache_size=-8192;");
    globe_t *globe = ok ? globe_open_databases(target, NULL, NULL) : NULL;
    ok = ok && globe && globe_has_layer(globe, GLOBE_LAYER_SHORELINE);
    globe_close(globe);
    if (ok) ok = cfg_database_publish_rivers(target, source);
    sqlite_close(target); sqlite_close(source);
    file_free(target_file); file_free(source_file); file_free(config);
    string_free(candidate); string_free(path); string_free(raw); string_free(key); string_free(config_path); string_free(home);
    if (ok) puts("River tables installed in the globe database; shoreline and configuration preserved.");
    else fputs("River installation failed; no river replacement committed.\n", stderr);
    return ok ? 0 : 1;
}

static string_t *cfg_globe_setting(file_t *config, const char *name, const string_t *fallback)
{
    const char *environment = getenv(name);
    string_t *value = environment && *environment ? cfg_storage_value(environment) : cfg_storage_setting(config, name);
    if (value && !string_byte_length(value) && fallback) {
        string_free(value);
        value = string_clone(fallback);
    }
    return value;
}

static bool cfg_globe_valid(sqlite_t *db)
{
    sqlite_stmt_t *q = db ? sqlite_stmt_prepare(db, "PRAGMA integrity_check") : NULL;
    const char *result = q && sqlite_stmt_step(q) == SQLITE_STEP_ROW ? sqlite_stmt_column_text(q, 0) : NULL;
    bool ok = result && !strcmp(result, "ok");
    sqlite_stmt_finalize(q);
    globe_t *globe = ok ? globe_open_databases(db, db, NULL) : NULL;
    ok = ok && globe && globe_has_layer(globe, GLOBE_LAYER_SHORELINE);
    globe_close(globe);
    return ok;
}

/* Keep globe setup independent; unsupported operations fail without changing storage. */
static int cfg_globe_execute(const char *action, const char *explicit_key)
{
    bool install = action && !strcmp(action, "globe-install");
    bool check = action && !strcmp(action, "globe-check");
    bool update = action && !strcmp(action, "globe-update");
    if (!install && !check && !update) {
        fputs("Globe removal is not implemented yet; no files changed.\n", stderr);
        return 2;
    }
    string_t *home = cfg_storage_home();
    if (!home) return 1;
    string_t *config_dir = string_sprintf("%s/config", string_c_str(home));
    string_t *config_path = string_sprintf("%s/config/globe-db.env", string_c_str(home));
    string_t *default_path = string_sprintf("%s/globe/globe.db", string_c_str(home));
    file_t *config = config_path ? file_new(config_path) : NULL;
    string_t *path = config ? cfg_globe_setting(config, "MARS_GLOBE_DB_PATH", default_path) : NULL;
    string_t *key = explicit_key ? string_new_with(explicit_key) :
                    config ? cfg_globe_setting(config, "MARS_GLOBE_DB_KEY", NULL) : NULL;
    if (install && !explicit_key && key && !string_byte_length(key) && isatty(STDIN_FILENO)) {
        string_free(key);
        key = cfg_prompt_read("Password for the new globe database: ", true);
        string_t *repeat = key ? cfg_prompt_read("Confirm password: ", true) : NULL;
        if (!repeat || string_compare(key, repeat)) {
            string_free(key);
            key = NULL;
        }
        string_free(repeat);
    }
    string_t *expanded = path ? cfg_storage_expand_path(path) : NULL;
    string_t *parent = expanded ? cfg_storage_parent(expanded) : NULL;
    file_t *destination = expanded ? file_new(expanded) : NULL;
    bool exists = false;
    bool ok = config_dir && config && parent && destination && key && string_byte_length(key) &&
              cfg_storage_safe_regular(destination, &exists);
    string_t *staging = NULL, *temporary_path = NULL, *root = NULL, *body = NULL;
    file_t *temporary = NULL;
    sqlite_t *db = NULL;
    root = cfg_storage_value(getenv("MARS_ROOT"));
    if (root && !string_byte_length(root)) {
        string_free(root);
        root = string_new_with(MARS_CONFIG_ROOT_DIR);
    }
    if (ok && update) {
        /* Import replaces only shoreline tables in one transaction; credentials and other layers are untouched. */
        staging = exists ? cfg_database_stage(parent) : NULL;
        db = staging ? sqlite_open_encrypted(expanded, key) : NULL;
        ok = db && root && sqlite_exec_cstr(db, "PRAGMA cache_size=-512; PRAGMA temp.cache_size=-512;") &&
             cfg_globe_valid(db) && cfg_database_coast(db, root, staging) && cfg_globe_valid(db);
        sqlite_close(db);
    } else if (ok && (check || exists)) {
        db = exists ? sqlite_open_encrypted(expanded, key) : NULL;
        ok = db && sqlite_exec_cstr(db, "PRAGMA query_only=ON; PRAGMA cache_size=-512;") && cfg_globe_valid(db);
        sqlite_close(db);
    } else if (ok && install) {
        ok = !exists && cfg_storage_private_directory(home, true) && cfg_storage_private_directory(parent, true) &&
             cfg_storage_private_directory(config_dir, true);
        staging = ok ? cfg_database_stage(parent) : NULL;
        temporary_path = staging ? string_sprintf("%s/database.db", string_c_str(staging)) : NULL;
        temporary = temporary_path ? file_new(temporary_path) : NULL;
        db = temporary && root ? sqlite_open_encrypted(temporary_path, key) : NULL;
        ok = ok && db && sqlite_exec_cstr(db, "PRAGMA cache_size=-512; PRAGMA temp.cache_size=-512;") &&
             cfg_database_coast(db, root, staging) && cfg_globe_valid(db);
        sqlite_close(db);
        string_t *quoted_path = path ? cfg_storage_quote(path) : NULL;
        string_t *quoted_key = key ? cfg_storage_quote(key) : NULL;
        body = ok && quoted_path && quoted_key ? string_sprintf(
            "# Generated by mars_config globe-install\nexport MARS_GLOBE_DB_PATH=%s\nexport MARS_GLOBE_DB_KEY=%s\n",
            string_c_str(quoted_path), string_c_str(quoted_key)) : NULL;
        string_free(quoted_path);
        string_free(quoted_key);
        ok = ok && body && cfg_database_publish(temporary, destination, config, config_dir, staging, body);
    }
    if (ok) puts(update ? "Globe shoreline updated; other layers and database settings preserved. Restart MARS Lab."
                        : install ? "Globe database ready: existing data preserved; shoreline available."
                        : "Globe database integrity and shoreline availability verified.");
    else fputs("Globe database operation failed; check its path, key and packaged shoreline source.\n", stderr);
    file_free(temporary);
    cfg_database_cleanup(staging);
    string_free(staging); string_free(temporary_path); string_free(root); string_free(body);
    file_free(destination); file_free(config);
    string_free(expanded); string_free(parent); string_free(path); string_free(key);
    string_free(default_path); string_free(config_path); string_free(config_dir); string_free(home);
    return ok ? 0 : 1;
}

/* Dispatch standalone globe operations using their saved settings. */
int cfg_globe_run(const char *action)
{
    return cfg_globe_execute(action, NULL);
}

/* The umbrella installer owns password selection; never prompt again here. */
int cfg_globe_install_key(const char *key)
{
    return key && *key ? cfg_globe_execute("globe-install", key) : 1;
}
