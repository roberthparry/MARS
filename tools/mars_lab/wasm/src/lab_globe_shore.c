/**
 * @file lab_globe_shore.c
 * @brief Native ownership, projection and hemisphere clipping of live SVG shorelines.
 *
 * Parses bounded geographic patches from the native server into array_t storage.
 * Projects and subdivides geographic edges with the same camera as picking, clips
 * the hidden hemisphere and joins clipped endpoints along the globe limb. Small
 * source patches make these limb arcs unambiguous. Nesting order is retained.
 * SVG viewport clipping is performed by the browser. The same native projection
 * supplies binary GPU coverage plans for terrain-colour compositing, without SVG
 * image decoding. A dispatched singleton owns all retained mutable state.
 */
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "array-t.h"
#include "webgl-t.h"
#include "lab_dom.h"
#include "lab_serial.h"
#include "lab_globe_shore.h"
#define MARS_LAB_GLOBE_SHORE_INTERNAL_ACCESS
#include "lab_globe_shore_internal.h"

typedef struct _shore_vertex_t { double lon, lat; } shore_vertex_t;
typedef struct _shore_ring_t { size_t first, count; unsigned level, ice; } shore_ring_t;
typedef struct _shore_point_t { double x, y, z; } shore_point_t;
typedef struct _shore_store_t {
    lab_serial_t serial;
    array_t *vertices, *rings;
    webgl_mask_t *mask;
    double bounds[4];
} shore_store_t;
typedef struct _shore_request_t {
    jsref text, bounds, orientation, view;
    double zoom, size;
    bool result;
    bool mask;
    const unsigned char *bytes;
    size_t length;
    const double *region;
} shore_request_t;
typedef struct _shore_output_t {
    char *text;
    size_t length, capacity, generated;
    webgl_mask_t *mask;
} shore_output_t;

static shore_store_t *shore_store(void)
{
    static shore_store_t instance = {.serial = LAB_SERIAL_INITIALIZER};
    return &instance;
}

static bool shore_write(shore_output_t *output, const char *text, size_t length)
{
    if (length > 32 * 1048576 - output->length) return false;
    if (length > output->capacity - output->length) {
        size_t capacity = output->capacity;
        while (capacity < output->length + length) capacity *= 2;
        char *next = realloc(output->text, capacity);
        if (!next) return false;
        output->text = next; output->capacity = capacity;
    }
    memcpy(output->text + output->length, text, length);
    output->length += length;
    return true;
}

/* The tiny fixed SVG grammar needs no stdio or WASI imports. */
static bool shore_append(shore_output_t *output, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    bool ok = true;
    while (ok && *format) {
        if (*format != '%') { ok = shore_write(output, format++, 1); continue; }
        ++format;
        if (*format == '%') {
            ok = shore_write(output, format++, 1);
        } else if (*format == 's') {
            const char *text = va_arg(arguments, const char *);
            ok = shore_write(output, text, strlen(text));
            ++format;
        } else if (*format == 'c') {
            char c = (char)va_arg(arguments, int);
            ok = shore_write(output, &c, 1); ++format;
        } else {
            bool decimal = !strncmp(format, ".3f", 3);
            if (!decimal && *format != 'u') { ok = false; break; }
            double value = decimal ? va_arg(arguments, double) : va_arg(arguments, unsigned);
            if (!isfinite(value) || fabs(value) > 1e9) { ok = false; break; }
            if (value < 0) ok = shore_write(output, "-", 1);
            unsigned long long number = (unsigned long long)floor(fabs(value) * (decimal ? 1000 : 1) + .5);
            unsigned long long whole = decimal ? number / 1000 : number, divisor = 1;
            while (divisor <= whole / 10) divisor *= 10;
            do {
                char c = '0' + whole / divisor % 10;
                ok = ok && shore_write(output, &c, 1); divisor /= 10;
            } while (divisor);
            if (decimal) {
                char fraction[4] = {'.', '0' + number / 100 % 10, '0' + number / 10 % 10, '0' + number % 10};
                ok = ok && shore_write(output, fraction, sizeof fraction);
            }
            format += decimal ? 3 : 1;
        }
    }
    va_end(arguments);
    return ok;
}

static bool shore_number(const char **cursor, double *value)
{
    const char *p = *cursor;
    while (*p == ' ') ++p;
    bool negative = *p == '-';
    if (negative) ++p;
    if (*p < '0' || *p > '9') return false;
    double number = 0;
    unsigned digits = 0;
    while (*p >= '0' && *p <= '9') {
        if (++digits > 4) return false;
        number = number * 10 + *p++ - '0';
    }
    if (*p == '.') {
        ++p; digits = 0;
        double scale = .1;
        while (*p >= '0' && *p <= '9') {
            if (++digits > 9) return false;
            number += (*p++ - '0') * scale; scale *= .1;
        }
        if (!digits) return false;
    }
    if (*p != ' ' && *p != '\n' && *p != '\r' && *p) return false;
    *value = negative ? -number : number;
    *cursor = p;
    return true;
}

static void shore_clear(void *context)
{
    (void)context;
    shore_store_t *store = shore_store();
    array_destroy(store->vertices); array_destroy(store->rings);
    store->vertices = store->rings = NULL;
    webgl_mask_free(store->mask);
    store->mask = NULL;
}

/* Clipping can leave an arbitrary number of collinear vertices along a tile edge.
 * Reject only exact degeneracy: thin real islands must retain their geometry. */
static bool shore_has_area(const array_t *vertices, const shore_ring_t *ring)
{
    if (ring->count < 3) return false;
    const shore_vertex_t *origin = array_get(vertices, ring->first);
    double dx = 0, dy = 0;
    for (size_t i = 1; i < ring->count; ++i) {
        const shore_vertex_t *point = array_get(vertices, ring->first + i);
        double x = point->lon - origin->lon, y = point->lat - origin->lat;
        if (dx == 0 && dy == 0) { dx = x; dy = y; }
        else if (dx * y != dy * x) return true;
    }
    return false;
}

static void shore_load(void *context)
{
    shore_request_t *request = context;
    bool binary = request->region || lab_dom_type(request->text) == LAB_VALUE_BYTES;
    size_t length = request->region ? request->length :
        binary ? lab_dom_count(request->text) : lab_dom_length(request->text);
    if ((!binary && lab_dom_type(request->text) != LAB_VALUE_STRING) || length > 24 * 1048576 ||
        (!request->region && lab_dom_count(request->bounds) != 4)) return;
    double bounds[4];
    for (unsigned i = 0; i < 4; ++i) {
        bounds[i] = request->region ? request->region[i] : lab_dom_to_number(lab_dom_item(request->bounds, i));
        if (!isfinite(bounds[i])) return;
    }
    if (bounds[2] <= bounds[0] || bounds[2] - bounds[0] > 360 || bounds[1] < -90 ||
        bounds[3] > 90 || bounds[3] <= bounds[1]) return;
    char *owned = NULL;
    const char *text = (const char *)request->bytes;
    if (!request->region) {
        owned = malloc(length + 1);
        if (!owned) return;
        int copied = binary ? lab_dom_bytes_copy(request->text, (unsigned char *)owned, length)
                            : lab_dom_utf8_copy(request->text, (unsigned char *)owned, length);
        if (copied != (int)length) { free(owned); return; }
        owned[length] = 0;
        text = owned;
    }
    if (!text || (binary && (length < 4 || memcmp(text, "MSH1", 4) || (length - 4) % 17))) {
        free(owned); return;
    }
    array_t *vertices = array_create(sizeof(shore_vertex_t), NULL, NULL);
    array_t *rings = array_create(sizeof(shore_ring_t), NULL, NULL);
    bool ok = vertices && rings, opened = false;
    shore_ring_t ring = {0};
    unsigned previous_level = 1;
    const char *cursor = text + (binary ? 4 : 0), *end = text + length;
    while (ok && cursor < end) {
        if (!binary && (*cursor == '\n' || *cursor == ' ' || *cursor == '\r')) { ++cursor; continue; }
        const unsigned char *record = (const unsigned char *)cursor;
        unsigned tag = *record;
        char command;
        if (binary) {
            command = tag == 0 ? 'Z' : tag == 16 ? 'L' : (tag & ~8u) >= 1 && (tag & ~8u) <= 4 ? 'M' : '?';
            cursor += 17;
        } else command = *cursor++;
        if (command == 'Z') {
            if (binary) {
                for (unsigned i = 1; i < 17; ++i) if (record[i]) ok = false;
                if (!ok) break;
            }
            ring.count = array_size(vertices) - ring.first;
            ok = opened && ring.count > 0;
            /* Display simplification may collapse tiny islands; do not reject the surrounding coastline. */
            if (ok && !shore_has_area(vertices, &ring))
                ok = array_remove_elements(vertices, ring.first, ring.count);
            else if (ok)
                ok = array_size(rings) < 100000 && array_append_carray(rings, &ring, 1);
            opened = false;
            continue;
        }
        if (command == 'M') {
            double level = tag & 7, ice = (tag >> 3) & 1;
            ok = !opened && (binary || (shore_number(&cursor, &level) && shore_number(&cursor, &ice))) &&
                level >= previous_level && level <= 4 && level == floor(level) && (ice == 0 || ice == 1);
            if (!ok) break;
            ring = (shore_ring_t){.first = array_size(vertices), .level = (unsigned)level, .ice = (unsigned)ice};
            previous_level = ring.level;
            opened = true;
        } else if (command != 'L' || !opened) { ok = false; break; }
        shore_vertex_t vertex;
        if (binary) {
            double coordinates[2];
            for (unsigned axis = 0; axis < 2; ++axis) {
                uint64_t bits = 0;
                for (unsigned byte = 0; byte < 8; ++byte) bits |= (uint64_t)record[1 + axis * 8 + byte] << (byte * 8);
                memcpy(&coordinates[axis], &bits, sizeof bits);
            }
            vertex = (shore_vertex_t){coordinates[0], coordinates[1]};
        }
        ok = (binary || (shore_number(&cursor, &vertex.lon) && shore_number(&cursor, &vertex.lat))) &&
            isfinite(vertex.lon) && isfinite(vertex.lat) &&
            vertex.lon >= bounds[0] - 1e-7 && vertex.lon <= bounds[2] + 1e-7 &&
            vertex.lat >= bounds[1] - 1e-7 && vertex.lat <= bounds[3] + 1e-7 &&
            array_size(vertices) < 500000 && array_append_carray(vertices, &vertex, 1);
    }
    free(owned);
    if (ok && !opened) {
        shore_clear(NULL);
        shore_store_t *store = shore_store();
        store->vertices = vertices; store->rings = rings;
        memcpy(store->bounds, bounds, sizeof bounds);
        request->result = true;
    } else { array_destroy(vertices); array_destroy(rings); }
}

static void shore_covers(void *context)
{
    shore_request_t *request = context;
    shore_store_t *store = shore_store();
    if ((!request->bounds && !store->rings) || lab_dom_count(request->view) != 7) return;
    double view[4], bounds[4];
    if (request->bounds && lab_dom_count(request->bounds) != 4) return;
    for (unsigned i = 0; i < 4; ++i) {
        view[i] = lab_dom_to_number(lab_dom_item(request->view, i + 3));
        bounds[i] = request->bounds ? lab_dom_to_number(lab_dom_item(request->bounds, i)) : store->bounds[i];
        if (!isfinite(view[i]) || !isfinite(bounds[i])) return;
    }
    double shift = 360 * round(((bounds[0] + bounds[2]) - (view[0] + view[2])) / 720);
    request->result = view[1] >= bounds[1] && view[3] <= bounds[3] &&
        (bounds[2] - bounds[0] >= 359.999999 ||
         (view[0] + shift >= bounds[0] && view[2] + shift <= bounds[2]));
}

static shore_point_t shore_project(double lon, double lat, const double matrix[9])
{
    double radians = 0.017453292519943295, c = cos(lat * radians);
    double p[3] = {c * sin(lon * radians), sin(lat * radians), c * cos(lon * radians)};
    return (shore_point_t){matrix[0] * p[0] + matrix[1] * p[1] + matrix[2] * p[2],
                          matrix[3] * p[0] + matrix[4] * p[1] + matrix[5] * p[2],
                          matrix[6] * p[0] + matrix[7] * p[1] + matrix[8] * p[2]};
}

static bool shore_clip(array_t *visible, shore_point_t a, shore_point_t b)
{
    if ((a.z >= 0) != (b.z >= 0)) {
        double t = a.z / (a.z - b.z);
        shore_point_t p = {a.x + t * (b.x - a.x), a.y + t * (b.y - a.y), 0};
        double norm = hypot(p.x, p.y);
        if (norm < 1e-12) return false;
        p.x /= norm; p.y /= norm;
        if (!array_append_carray(visible, &p, 1)) return false;
    }
    return b.z < 0 || array_append_carray(visible, &b, 1);
}

/* Interpolate latitude-dependent polar colour within each geographic patch, rather than using a hard ice boundary. */
static bool shore_polar_gradient(shore_output_t *output, const shore_ring_t *ring, const double matrix[9],
                                 double scale, double size)
{
    shore_store_t *store = shore_store();
    const shore_vertex_t *first = array_get(store->vertices, ring->first);
    double south = first->lat, north = first->lat, west = first->lon, east = first->lon;
    for (size_t i = 1; i < ring->count; ++i) {
        const shore_vertex_t *p = array_get(store->vertices, ring->first + i);
        south = fmin(south, p->lat); north = fmax(north, p->lat);
        west = fmin(west, p->lon); east = fmax(east, p->lon);
    }
    shore_point_t a = shore_project((west + east) / 2, south, matrix);
    shore_point_t b = shore_project((west + east) / 2, north, matrix);
    bool ok = shore_append(output,
        "<defs><linearGradient id=\"shore-polar-%u\" gradientUnits=\"userSpaceOnUse\" "
        "x1=\"%.3f\" y1=\"%.3f\" x2=\"%.3f\" y2=\"%.3f\">",
        (unsigned)ring->first, size / 2 + a.x * scale, size / 2 - a.y * scale,
        size / 2 + b.x * scale, size / 2 - b.y * scale);
    for (unsigned i = 0; ok && i <= 4; ++i) {
        double latitude = south + (north - south) * i / 4;
        double t = fmin(1, fmax(0, (fabs(latitude) - 55) / 25));
        t = t * t * (3 - 2 * t);
        ok = shore_append(output, "<stop offset=\"%.3f\" stop-color=\"rgb(%u,%u,%u)\"/>", i / 4.0,
                          (unsigned)(70 + 173 * t), (unsigned)(85 + 160 * t), (unsigned)(34 + 208 * t));
    }
    return ok && shore_append(output, "</linearGradient></defs>");
}

static bool shore_ring(shore_output_t *output, const shore_ring_t *ring, const double matrix[9], double scale, double size)
{
    shore_store_t *store = shore_store();
    array_t *visible = array_create(sizeof(shore_point_t), NULL, NULL);
    bool ok = visible != NULL;
    double step = fmin(1, sqrt(.4 / scale) / 0.017453292519943295);
    size_t generated = 0;
    for (size_t i = 0; ok && i < ring->count; ++i) {
        const shore_vertex_t *a = array_get(store->vertices, ring->first + i);
        const shore_vertex_t *b = array_get(store->vertices, ring->first + (i + 1) % ring->count);
        unsigned count = (unsigned)fmax(1, ceil(fmax(fabs(b->lon - a->lon), fabs(b->lat - a->lat)) / step));
        if ((generated += count) > 1000000 || (output->generated += count) > 2000000) { ok = false; break; }
        shore_point_t previous = shore_project(a->lon, a->lat, matrix);
        for (unsigned j = 1; ok && j <= count; ++j) {
            double t = (double)j / count;
            shore_point_t next = shore_project(a->lon + t * (b->lon - a->lon), a->lat + t * (b->lat - a->lat), matrix);
            ok = shore_clip(visible, previous, next);
            previous = next;
        }
    }
    size_t count = array_size(visible);
    if (ok && output->mask && count >= 3) {
        ok = webgl_mask_begin(output->mask, ring->level);
        for (size_t i = 0; ok && i <= count; ++i) {
            const shore_point_t *p = array_get(visible, i % count);
            const shore_point_t *a = array_get(visible, (i + count - 1) % count);
            if (i && a->z == 0 && p->z == 0) {
                double angle = atan2(a->x * p->y - a->y * p->x, a->x * p->x + a->y * p->y);
                unsigned steps = (unsigned)fmax(1, ceil(fabs(angle) * sqrt(scale / .4)));
                for (unsigned j = 1; ok && j < steps; ++j) {
                    double t = angle * j / steps, c = cos(t), s = sin(t);
                    ok = webgl_mask_point(output->mask, (a->x * c - a->y * s) * scale / size,
                                          -(a->x * s + a->y * c) * scale / size);
                }
            }
            /* Texture row zero is the SVG top, matching browser image uploads without UNPACK_FLIP_Y. */
            if (ok) ok = webgl_mask_point(output->mask, p->x * scale / size, -p->y * scale / size);
        }
        ok = ok && webgl_mask_end(output->mask);
    }
    if (ok && count >= 3) {
        const char *colour = ring->level % 2 ? (ring->ice ? "#f3f5f2" : "#465522") : "#071440";
        if (ring->ice && ring->level % 2) {
            ok = shore_polar_gradient(output, ring, matrix, scale, size) &&
                shore_append(output, "<path fill=\"url(#shore-polar-%u)\" stroke=\"url(#shore-polar-%u)\" ",
                             (unsigned)ring->first, (unsigned)ring->first);
        } else
            ok = shore_append(output, "<path fill=\"%s\" stroke=\"%s\" ", colour, colour);
        /* Cover antialiasing gaps at shared clipping edges without drawing a geographic grid.
         * Match the fill (including polar gradients) and keep the overlap screen-sized. */
        ok = ok && shore_append(output, "stroke-width=\"1\" vector-effect=\"non-scaling-stroke\" "
                                       "stroke-linejoin=\"round\" d=\"");
        for (size_t i = 0; ok && i <= count; ++i) {
            const shore_point_t *p = array_get(visible, i % count);
            const shore_point_t *previous = array_get(visible, (i + count - 1) % count);
            double x = size / 2 + p->x * scale, y = size / 2 - p->y * scale;
            if (i && p->z == 0 && previous->z == 0)
                ok = shore_append(output, "A%.3f %.3f 0 0 %u %.3f %.3f", scale, scale,
                                  (unsigned)(previous->x * p->y - previous->y * p->x < 0), x, y);
            else ok = shore_append(output, "%c%.3f %.3f", i ? 'L' : 'M', x, y);
        }
        ok = ok && shore_append(output, "Z\"/>");
    }
    array_destroy(visible);
    return ok;
}


static void shore_svg(void *context)
{
    shore_request_t *request = context;
    shore_store_t *store = shore_store();
    double zoom = request->zoom, size = request->size, matrix[9];
    if (!store->rings || !isfinite(zoom) || zoom < 1 || zoom > 4096 || !isfinite(size) || size <= 0 || size > 16384) {
        lab_dom_return(0); return;
    }
    lab_surface_rotation(request->orientation, matrix);
    for (unsigned i = 0; i < 9; ++i) if (!isfinite(matrix[i])) { lab_dom_return(0); return; }
    shore_output_t output = {.capacity = 65536};
    if (request->mask) {
        if (!store->mask) store->mask = webgl_mask_new(1048576);
        webgl_mask_reset(store->mask);
        output.mask = store->mask;
        if (!output.mask) { lab_dom_return(0); return; }
    }
    output.text = malloc(output.capacity);
    double scale = size * zoom / 2.24;
    bool ok = output.text && shore_append(&output, "<circle cx=\"%.3f\" cy=\"%.3f\" r=\"%.3f\" fill=\"#071440\"/>",
                                          size / 2, size / 2, scale);
    /* This bounded pass renders every selected polygon; spatial selection happens on the native server. */
    for (size_t i = 0; ok && i < array_size(store->rings); ++i)
        ok = shore_ring(&output, array_get(store->rings, i), matrix, scale, size);
    lab_dom_return(ok ? lab_dom_utf8_string((const unsigned char *)output.text, output.length) : 0);
    free(output.text);
}

/* Atomically admit a replacement display region. */
bool lab_globe_shore_load(jsref text, jsref bounds)
{
    shore_request_t request = {.text = text, .bounds = bounds};
    return lab_serial_dispatch(&shore_store()->serial, shore_load, &request) && request.result;
}

/* Admit the HTTP receiver's borrowed bytes without a host handle or staging copy. */
bool lab_globe_shore_admit(const unsigned char *bytes, size_t length, const double bounds[4])
{
    if (!bytes || !bounds) return false;
    shore_request_t request = {.bytes = bytes, .length = length, .region = bounds};
    return lab_serial_dispatch(&shore_store()->serial, shore_load, &request) && request.result;
}

/* Query retained geographic coverage without a JavaScript geometry cache. */
bool lab_globe_shore_covers(jsref view)
{
    shore_request_t request = {.view = view};
    return lab_serial_dispatch(&shore_store()->serial, shore_covers, &request) && request.result;
}

/* Check a published frame independently of a replacement polygon cache. */
bool lab_globe_shore_frame_covers(jsref view, jsref bounds)
{
    if (!bounds) return false;
    shore_request_t request = {.view = view, .bounds = bounds};
    return lab_serial_dispatch(&shore_store()->serial, shore_covers, &request) && request.result;
}

/* Project retained polygons to SVG through serial ownership. */
void lab_globe_shore_svg(quaternion_jsref orientation, double zoom, double size)
{
    shore_request_t request = {.orientation = orientation, .zoom = zoom, .size = size};
    if (!lab_serial_dispatch(&shore_store()->serial, shore_svg, &request)) lab_dom_return(0);
}

/* Generate one projection for the visible SVG and the binary coverage plan. */
void lab_globe_shore_frame(quaternion_jsref orientation, double zoom, double size)
{
    shore_request_t request = {.orientation = orientation, .zoom = zoom, .size = size, .mask = true};
    if (!lab_serial_dispatch(&shore_store()->serial, shore_svg, &request)) lab_dom_return(0);
}

/* Borrow native vertices for a synchronous worker transfer snapshot. */
const float *lab_globe_mask_vertices(void)
{
    return webgl_mask_vertices(shore_store()->mask, NULL);
}

/* Report binary vertex count without a host-value collection. */
unsigned lab_globe_mask_vertex_count(void)
{
    size_t count = 0;
    webgl_mask_vertices(shore_store()->mask, &count);
    return (unsigned)count;
}

/* Borrow fixed-layout GPU commands. */
const void *lab_globe_mask_batches(void)
{
    return webgl_mask_batches(shore_store()->mask, NULL);
}

/* Report binary command count. */
unsigned lab_globe_mask_batch_count(void)
{
    size_t count = 0;
    webgl_mask_batches(shore_store()->mask, &count);
    return (unsigned)count;
}

/* Source ownership stays native; browser WebGL still compiles GLSL once per context. */
void lab_globe_mask_shader(unsigned fragment)
{
    const char *source = fragment ? webgl_mask_fragment_shader() : webgl_mask_vertex_shader();
    lab_dom_return(lab_dom_utf8_string((const unsigned char *)source, strlen(source)));
}

/* Release the optional vector surface. */
void lab_globe_shore_clear(void)
{
    lab_serial_dispatch(&shore_store()->serial, shore_clear, NULL);
}
