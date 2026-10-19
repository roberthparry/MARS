/**
 * @file lab_globe.c
 * @brief Bounded globe gestures, orthographic picking and display projection for the observer selector.
 *
 * These double-precision screen calculations do not replace the native geographic database or astronomical
 * calculations. The GPU uses the same orthographic equations to sample native-produced shoreline imagery.
 */
#include <math.h>

#include "lab_dom.h"
#include "lab_globe.h"

static const double globe_radian = 0.017453292519943295;

static double globe_longitude(double longitude)
{
    return longitude - 360 * floor((longitude + 180) / 360);
}

static void globe_result(const double *values, unsigned count)
{
    jsref result = lab_dom_object(LAB_VALUE_ARRAY);
    for (unsigned i = 0; i < count; ++i) lab_dom_push(result, lab_dom_numeric(values[i]));
    lab_dom_return(result);
}

/* Centre explicitly north-up; subsequent gestures retain all three quaternion axes. */
void lab_globe_centre(double latitude, double longitude)
{
    if (!isfinite(latitude) || !isfinite(longitude) || fabs(latitude) > 90) {
        lab_dom_return(0);
        return;
    }
    double p = latitude * globe_radian / 2, l = globe_longitude(longitude) * globe_radian / 2;
    double north[] = {sin(p), 0, 0, cos(p)}, east[] = {0, -sin(l), 0, cos(l)}, result[4];
    lab_surface_quaternion_multiply(north, east, result);
    globe_result(result, 4);
}

/* Rotate about screen axes, avoiding latitude clamps and divisions by polar cosines. */
void lab_globe_rotate(quaternion_jsref orientation, double zoom, double dx, double dy, double roll)
{
    if (!isfinite(zoom) || !isfinite(dx) || !isfinite(dy) || !isfinite(roll) || zoom < 1 || zoom > 4096) {
        lab_dom_return(0);
        return;
    }
    dx /= zoom;
    dy /= zoom;
    double q[4], result[4], delta[] = {0, 0, 0, 1}, distance = hypot(hypot(dx, dy), roll);
    for (unsigned i = 0; i < 4; ++i) q[i] = lab_dom_to_number(lab_dom_item(orientation, i));
    if (distance > 0) {
        double half = distance * 30 * globe_radian, scale = sin(half) / distance;
        delta[0] = dy * scale;
        delta[1] = dx * scale;
        delta[2] = roll * scale;
        delta[3] = cos(half);
    }
    lab_surface_quaternion_multiply(delta, q, result);
    globe_result(result, 4);
}

/* Return projected coordinates and front/back visibility from the same rotation used by the shader. */
void lab_globe_project(double latitude, double longitude, quaternion_jsref orientation)
{
    double p = latitude * globe_radian, l = longitude * globe_radian, matrix[9];
    double point[] = {cos(p) * sin(l), sin(p), cos(p) * cos(l)}, values[3] = {0};
    lab_surface_rotation(orientation, matrix);
    for (unsigned row = 0; row < 3; ++row)
        for (unsigned col = 0; col < 3; ++col) values[row] += matrix[row * 3 + col] * point[col];
    values[1] = -values[1];
    globe_result(values, 3);
}

/* Ignore clicks beyond the limb rather than selecting a point on the far side. */
void lab_globe_pick(double x, double y, quaternion_jsref orientation)
{
    if (!isfinite(x) || !isfinite(y) || x * x + y * y > 1) { lab_dom_return(0); return; }
    double z = sqrt(fmax(0, 1 - x * x - y * y)), matrix[9], point[3];
    lab_surface_rotation(orientation, matrix);
    for (unsigned i = 0; i < 3; ++i) point[i] = matrix[i] * x - matrix[3 + i] * y + matrix[6 + i] * z;
    double values[] = {asin(fmax(-1, fmin(1, point[1]))) / globe_radian,
                       globe_longitude(atan2(point[0], point[2]) / globe_radian)};
    globe_result(values, 2);
}

/* Match the shader's graticule spacing and allow a small pointer tolerance around its strokes. */
void lab_globe_grid(double latitude, double longitude, double zoom, double size)
{
    if (!isfinite(latitude) || !isfinite(longitude) || !isfinite(zoom) || !isfinite(size) ||
        fabs(latitude) > 90 || zoom < 1 || zoom > 4096 || size <= 0) {
        lab_dom_return(0);
        return;
    }
    static const double limits[] = {4, 12, 60, 240, 1200, INFINITY};
    static const double steps[] = {15, 5, 1, 0.25, 0.05, 0.01};
    unsigned index = 0;
    /* Six fixed display levels, mirroring the fragment shader. */
    while (zoom >= limits[index]) ++index;
    double step = steps[index], lat = round(latitude / step) * step;
    double lon = globe_longitude(round(longitude / step) * step);
    double tolerance = 384 / (size * zoom);
    unsigned flags = fabs(latitude - lat) <= tolerance;
    if (fabs(globe_longitude(longitude - lon)) * cos(latitude * globe_radian) <= tolerance &&
        fabs(latitude) < 89.999) flags |= 2;
    double values[] = {lat, lon, flags};
    globe_result(values, 3);
}

/* Keep the camera and requested texture rectangle finite, including at the poles and date line. */
void lab_globe_view(quaternion_jsref orientation, double zoom, double factor)
{
    if (!isfinite(zoom) || !isfinite(factor) || factor <= 0) { lab_dom_return(0); return; }
    double matrix[9];
    lab_surface_rotation(orientation, matrix);
    double latitude = asin(fmax(-1, fmin(1, matrix[7]))) / globe_radian;
    double longitude = globe_longitude(atan2(matrix[6], matrix[8]) / globe_radian);
    zoom = fmax(1, fmin(4096, zoom));
    zoom = fmax(1, fmin(4096, zoom * factor));
    double span = asin(fmin(1, 1.6 / zoom)) / globe_radian;
    double south = fmax(-90, latitude - span), north = fmin(90, latitude + span);
    double lon_span = south <= -90 || north >= 90 ? 180 :
        fmin(180, span / fmax(0.00001, cos(fmax(fabs(south), fabs(north)) * globe_radian)));
    double values[] = {latitude, longitude, zoom, longitude - lon_span, south, longitude + lon_span, north};
    globe_result(values, 7);
}
