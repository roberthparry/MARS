/**
 * @file lab_globe_borders.c
 * @brief Bounded native border geometry and camera-projected SVG paths for the observer globe.
 *
 * array_t owns unit-sphere segment endpoints. Projection shares the globe quaternion, subdivides long
 * arcs to subpixel chord error, clips the hidden hemisphere and viewport, and emits only SVG path data.
 * No database access, JavaScript geometry cache or raster texture is involved. Input and output are capped;
 * path storage grows with visible output rather than reserving the complete ceiling for every frame.
 * malformed replacement data leave the previously accepted cache intact.
 * Mutable resident state belongs to a private singleton. Public operations enter its serial dispatcher;
 * internal helpers execute only within that ownership boundary. No scoped browser handles are retained.
 */
#include <math.h>
#include <stdlib.h>

#include "array-t.h"
#include "lab_dom.h"
#include "lab_serial.h"
#include "lab_globe_borders.h"

typedef struct { double point[2][3]; } border_segment_t;
typedef struct _border_output_t {
    char *text;
    size_t length, capacity;
} border_output_t;
typedef struct _border_store_t {
    lab_serial_t serial;
    array_t *segments;
    double bounds[4];
    bool has_bounds;
    const size_t border_limit;
    const size_t text_limit;
} border_store_t;

/* The singleton is private; only dispatched operations may access its resident state. */
static border_store_t *border_store(void)
{
    static border_store_t instance = {.serial = LAB_SERIAL_INITIALIZER, .border_limit = 250000, .text_limit = 16 * 1048576};
    return &instance;
}

static void lab_globe_borders_clear_owned(void);
static bool lab_globe_borders_load_owned(jsref text);
static void lab_globe_borders_path_owned(quaternion_jsref orientation, double zoom, double size);

static bool whitespace(char c)
{
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

static bool read_coordinate(char **cursor, double *value)
{
    char *p = *cursor;
    while (whitespace(*p)) ++p;
    bool negative = *p == '-';
    if (*p == '-' || *p == '+') ++p;
    if (*p < '0' || *p > '9') return false;
    double number = 0;
    unsigned digits = 0;
    while (*p >= '0' && *p <= '9') {
        if (++digits > 6) return false;
        number = number * 10 + (*p++ - '0');
    }
    if (*p == '.') {
        ++p;
        double place = 0.1;
        digits = 0;
        while (*p >= '0' && *p <= '9') {
            if (++digits > 9) return false;
            number += (*p++ - '0') * place;
            place *= 0.1;
        }
        if (!digits) return false;
    }
    if (*p && !whitespace(*p)) return false;
    *cursor = p;
    *value = negative ? -number : number;
    return true;
}

/* Free the optional layer without affecting the observer or other native stores. */
static void lab_globe_borders_clear_owned(void)
{
    border_store_t *state = border_store();
    array_destroy(state->segments);
    state->segments = NULL;
    state->has_bounds = false;
}

/* Admit complete coordinate records only, with a fixed memory and segment budget. */
static bool lab_globe_borders_load_owned(jsref text)
{
    border_store_t *state = border_store();
    unsigned length = lab_dom_length(text);
    if (lab_dom_type(text) != LAB_VALUE_STRING || length > state->text_limit) return false;
    char *input = malloc((size_t)length + 1);
    if (!input) return false;
    int bytes = lab_dom_utf8_copy(text, (unsigned char *)input, length);
    if (bytes < 0 || (unsigned)bytes != length) { free(input); return false; }
    input[length] = 0;
    array_t *next = array_create(sizeof(border_segment_t), NULL, NULL);
    char *cursor = input, *end = input + length;
    bool ok = next != NULL;
    while (ok && cursor < end) {
        while (cursor < end && whitespace(*cursor)) ++cursor;
        if (cursor == end) break;
        double coordinates[4];
        for (unsigned i = 0; i < 4; ++i) {
            if (!read_coordinate(&cursor, &coordinates[i]) ||
                fabs(coordinates[i]) > (i % 2 ? 90 : 540)) { ok = false; break; }
        }
        if (!ok || array_size(next) >= state->border_limit) { ok = false; break; }
        border_segment_t segment;
        for (unsigned i = 0; i < 2; ++i) {
            double lon = coordinates[i * 2] * 0.017453292519943295;
            double lat = coordinates[i * 2 + 1] * 0.017453292519943295;
            segment.point[i][0] = cos(lat) * sin(lon);
            segment.point[i][1] = sin(lat);
            segment.point[i][2] = cos(lat) * cos(lon);
        }
        ok = array_append_carray(next, &segment, 1);
    }
    free(input);
    if (!ok) { array_destroy(next); return false; }
    lab_globe_borders_clear_owned();
    state->segments = next;
    return true;
}

static bool clip_axis(double start, double delta, double size, double *lo, double *hi)
{
    if (fabs(delta) < 1e-12) return start >= 0 && start <= size;
    double a = -start / delta, b = (size - start) / delta;
    *lo = fmax(*lo, fmin(a, b));
    *hi = fmin(*hi, fmax(a, b));
    return *lo <= *hi;
}

static void append_pixel(char *output, size_t *length, double value)
{
    unsigned hundredths = (unsigned)fmax(0, floor(value * 100 + 0.5));
    unsigned whole = hundredths / 100, divisor = 1;
    while (divisor <= whole / 10) divisor *= 10;
    do {
        output[(*length)++] = '0' + whole / divisor % 10;
        divisor /= 10;
    } while (divisor);
    output[(*length)++] = '.';
    output[(*length)++] = '0' + hundredths / 10 % 10;
    output[(*length)++] = '0' + hundredths % 10;
}

static bool reserve_segment(border_output_t *output)
{
    const size_t limit = border_store()->text_limit, needed = 40;
    if (limit - output->length < needed) return false;
    if (output->capacity - output->length >= needed) return true;
    size_t capacity = output->capacity ? output->capacity * 2 : 4096;
    if (capacity > limit) capacity = limit;
    char *text = realloc(output->text, capacity);
    if (!text) return false;
    output->text = text;
    output->capacity = capacity;
    return true;
}

static bool append_segment(border_output_t *output, double a[3], double b[3], double scale, double size)
{
    if (a[2] <= 0 && b[2] <= 0) return true;
    if (a[2] < 0 || b[2] < 0) {
        double t = a[2] / (a[2] - b[2]), intersection[3];
        for (unsigned i = 0; i < 3; ++i) intersection[i] = a[i] + t * (b[i] - a[i]);
        double radius = hypot(intersection[0], intersection[1]);
        if (radius < 1e-12) return true;
        double *hidden = a[2] < 0 ? a : b;
        hidden[0] = intersection[0] / radius;
        hidden[1] = intersection[1] / radius;
        hidden[2] = 0;
    }
    double x = size / 2 + a[0] * scale, y = size / 2 - a[1] * scale;
    double dx = (b[0] - a[0]) * scale, dy = -(b[1] - a[1]) * scale, lo = 0, hi = 1;
    if (!clip_axis(x, dx, size, &lo, &hi) || !clip_axis(y, dy, size, &lo, &hi)) return true;
    if (!reserve_segment(output)) return false;
    output->text[output->length++] = 'M';
    append_pixel(output->text, &output->length, x + lo * dx);
    output->text[output->length++] = ' ';
    append_pixel(output->text, &output->length, y + lo * dy);
    output->text[output->length++] = 'L';
    append_pixel(output->text, &output->length, x + hi * dx);
    output->text[output->length++] = ' ';
    append_pixel(output->text, &output->length, y + hi * dy);
    return true;
}

/* Return one bounded path using the same world-to-camera rotation as picking and markers. */
static void lab_globe_borders_path_owned(quaternion_jsref orientation, double zoom, double size)
{
    border_store_t *state = border_store();
    if (!state->segments || !isfinite(zoom) || !isfinite(size) || zoom < 1 || zoom > 4096 || size <= 0 || size > 16384) {
        lab_dom_return(0);
        return;
    }
    double matrix[9], scale = size * zoom / 2.24;
    lab_surface_rotation(orientation, matrix);
    for (unsigned i = 0; i < 9; ++i)
        if (!isfinite(matrix[i])) { lab_dom_return(0); return; }
    border_output_t output = {0};
    size_t pieces = 0;
    bool ok = true;
    double step = fmin(0.05, sqrt(0.8 / scale));
    double direct_dot = cos(step);
    /* Every spatially selected segment must follow the camera; this bounded pass is not a lookup. */
    for (size_t index = 0; ok && index < array_size(state->segments); ++index) {
        const border_segment_t *segment = array_get(state->segments, index);
        double camera[2][3] = {{0}}, dot = 0;
        for (unsigned row = 0; row < 3; ++row) {
            dot += segment->point[0][row] * segment->point[1][row];
            for (unsigned col = 0; col < 3; ++col)
                for (unsigned end = 0; end < 2; ++end)
                    camera[end][row] += matrix[row * 3 + col] * segment->point[end][col];
        }
        if (camera[0][2] <= 0 && camera[1][2] <= 0) continue;
        if (dot >= direct_dot) {
            ok = append_segment(&output, camera[0], camera[1], scale, size);
            continue;
        }
        double angle = acos(fmax(-1, fmin(1, dot))), sine = sin(angle);
        if (angle > 3.14) { ok = false; break; }
        unsigned count = (unsigned)fmax(1, ceil(angle / step));
        if ((pieces += count) > state->border_limit * 4) { ok = false; break; }
        double previous[3] = {camera[0][0], camera[0][1], camera[0][2]};
        for (unsigned part = 1; ok && part <= count; ++part) {
            double t = (double)part / count, current[3], a[3], b[3];
            double left = angle < 1e-8 ? 1 - t : sin((1 - t) * angle) / sine;
            double right = angle < 1e-8 ? t : sin(t * angle) / sine;
            for (unsigned i = 0; i < 3; ++i) {
                current[i] = left * camera[0][i] + right * camera[1][i];
                a[i] = previous[i]; b[i] = current[i]; previous[i] = current[i];
            }
            ok = append_segment(&output, a, b, scale, size);
        }
    }
    lab_dom_return(ok ? lab_dom_utf8_string((const unsigned char *)output.text, output.length) : 0);
    free(output.text);
}

static void globe_borders_clear_dispatch(void *context)
{
    (void)context;
    lab_globe_borders_clear_owned();
}

/* Execute the complete operation through the singleton's serial dispatcher. */
void lab_globe_borders_clear(void)
{
    border_store_t *state = border_store();
    if (!lab_serial_dispatch(&state->serial, globe_borders_clear_dispatch, NULL)) lab_dom_return(0);
}

typedef struct _globe_borders_load_request_t {
    jsref text;
    bool result;
} globe_borders_load_request_t;

static void globe_borders_load_dispatch(void *context)
{
    globe_borders_load_request_t *request = context;
    request->result = lab_globe_borders_load_owned(request->text);
}

/* Execute the complete operation through the singleton's serial dispatcher. */
bool lab_globe_borders_load(jsref text)
{
    border_store_t *state = border_store();
    globe_borders_load_request_t request = {.text = text, .result = 0};
    lab_serial_dispatch(&state->serial, globe_borders_load_dispatch, &request);
    return request.result;
}

typedef struct _border_region_request_t {
    jsref text, coordinates;
    bool result;
} border_region_request_t;

static bool border_bounds(jsref source, unsigned offset, double bounds[4])
{
    if (lab_dom_type(source) != LAB_VALUE_ARRAY || lab_dom_count(source) != offset + 4) return false;
    for (unsigned i = 0; i < 4; ++i) {
        jsref value = lab_dom_item(source, offset + i);
        if (lab_dom_type(value) != LAB_VALUE_NUMBER) return false;
        bounds[i] = lab_dom_to_number(value);
        if (!isfinite(bounds[i])) return false;
    }
    return bounds[0] < bounds[2] && bounds[2] - bounds[0] <= 360 &&
           bounds[1] >= -90 && bounds[3] <= 90 && bounds[1] < bounds[3];
}

static void border_region_load_dispatch(void *context)
{
    border_region_request_t *request = context;
    double bounds[4];
    if (!border_bounds(request->coordinates, 0, bounds) || !lab_globe_borders_load_owned(request->text)) return;
    border_store_t *state = border_store();
    for (unsigned i = 0; i < 4; ++i) state->bounds[i] = bounds[i];
    state->has_bounds = true;
    request->result = true;
}

/* Publish coverage and geometry together through the native owner. */
bool lab_globe_borders_load_region(jsref text, jsref bounds)
{
    border_region_request_t request = {.text = text, .coordinates = bounds};
    lab_serial_dispatch(&border_store()->serial, border_region_load_dispatch, &request);
    return request.result;
}

static void border_region_covers_dispatch(void *context)
{
    border_region_request_t *request = context;
    border_store_t *state = border_store();
    double view[4];
    if (!state->has_bounds || !border_bounds(request->coordinates, 3, view)) return;
    const double *bounds = state->bounds;
    if (view[1] < bounds[1] || view[3] > bounds[3]) return;
    if (bounds[2] - bounds[0] >= 360) { request->result = true; return; }
    double shift = 360 * floor((bounds[0] - view[0]) / 360);
    for (unsigned i = 0; i < 2; ++i, shift += 360)
        if (view[0] + shift >= bounds[0] && view[2] + shift <= bounds[2]) request->result = true;
}

/* Keep geographic cache policy out of the browser transport adapter. */
bool lab_globe_borders_covers(jsref view)
{
    border_region_request_t request = {.coordinates = view};
    lab_serial_dispatch(&border_store()->serial, border_region_covers_dispatch, &request);
    return request.result;
}

typedef struct _globe_borders_path_request_t {
    quaternion_jsref orientation;
    double zoom;
    double size;
} globe_borders_path_request_t;

static void globe_borders_path_dispatch(void *context)
{
    globe_borders_path_request_t *request = context;
    lab_globe_borders_path_owned(request->orientation, request->zoom, request->size);
}

/* Execute the complete operation through the singleton's serial dispatcher. */
void lab_globe_borders_path(quaternion_jsref orientation, double zoom, double size)
{
    border_store_t *state = border_store();
    globe_borders_path_request_t request = {.orientation = orientation, .zoom = zoom, .size = size};
    if (!lab_serial_dispatch(&state->serial, globe_borders_path_dispatch, &request)) lab_dom_return(0);
}
