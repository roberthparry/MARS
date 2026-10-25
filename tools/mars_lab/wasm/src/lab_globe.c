/**
 * @file lab_globe.c
 * @brief Bounded globe gestures, orthographic picking and display projection for the observer selector.
 *
 * These double-precision screen calculations do not replace the native geographic database or astronomical
 * calculations. The GPU uses the same orthographic equations to sample native-produced shoreline imagery.
 * A constant-cost tangent-plane camera transform retains detailed local SVG during gestures while worker
 * projections refresh the exact geometry; it never alters the geographic source data.
 * Great-circle travel transports the camera heading and interpolates visible span; its distance-aware
 * display clock bounds projected translation, roll and magnification even when rendering is delayed.
 */
#include <math.h>

#include "quaternion-t.h"
#include "lab_dom.h"
#include "lab_globe.h"

static const double globe_radian = 0.017453292519943295;

static double globe_longitude(double longitude)
{
    return longitude - 360 * floor((longitude + 180) / 360);
}

/* Reuse nearby-town coverage using geographic longitude wrapping, not host arithmetic. */
bool lab_globe_nearby_covers(double latitude, double longitude, double cached_latitude, double cached_longitude)
{
    return isfinite(latitude) && isfinite(longitude) && isfinite(cached_latitude) && isfinite(cached_longitude) &&
           fabs(latitude) <= 90 && fabs(cached_latitude) <= 90 && fabs(longitude) <= 180 && fabs(cached_longitude) <= 180 &&
           fabs(latitude - cached_latitude) < 1 && fabs(globe_longitude(longitude - cached_longitude)) < 1;
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
    const double x[] = {1, 0, 0}, y[] = {0, 1, 0};
    quaternion_t north, east, result;
    if (!quaternion_axis_angle(north, x, latitude * globe_radian) ||
        !quaternion_axis_angle(east, y, -globe_longitude(longitude) * globe_radian)) {
        lab_dom_return(0); return;
    }
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
    quaternion_t q, result, delta = {0, 0, 0, 1};
    double distance = hypot(hypot(dx, dy), roll), axis[] = {dy, dx, roll};
    for (unsigned i = 0; i < 4; ++i) q[i] = lab_dom_to_number(lab_dom_item(orientation, i));
    if (distance > 0 && !quaternion_axis_angle(delta, axis, distance * 60 * globe_radian)) {
        lab_dom_return(0); return;
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

/* Reproject retained local vectors without scanning or reparsing any shoreline vertices. */
void lab_globe_frame_transform(jsref previous, jsref current)
{
    if (lab_dom_count(previous) != 6 || lab_dom_count(current) != 6) { lab_dom_return(0); return; }
    double old[6], now[6], a[9], b[9], relative[9] = {0};
    for (unsigned i = 0; i < 6; ++i) {
        old[i] = lab_dom_to_number(lab_dom_item(previous, i));
        now[i] = lab_dom_to_number(lab_dom_item(current, i));
        if (!isfinite(old[i]) || !isfinite(now[i])) { lab_dom_return(0); return; }
    }
    if (old[4] < 1 || now[4] < 1 || old[4] > 4096 || now[4] > 4096 ||
        old[5] <= 0 || now[5] <= 0 || old[5] > 16384 || now[5] > 16384) { lab_dom_return(0); return; }
    if (!quaternion_matrix3(a, old) || !quaternion_matrix3(b, now)) { lab_dom_return(0); return; }
    for (unsigned row = 0; row < 3; ++row)
        for (unsigned col = 0; col < 3; ++col)
            for (unsigned k = 0; k < 3; ++k) relative[row * 3 + col] += b[row * 3 + k] * a[col * 3 + k];
    if (relative[8] <= 0) { lab_dom_return(0); return; }
    double scale = now[4] * now[5] / 2.24, ratio = now[4] * now[5] / (old[4] * old[5]);
    double result[6] = {ratio * relative[0], -ratio * relative[3], -ratio * relative[1], ratio * relative[4]};
    result[4] = now[5] / 2 + scale * relative[2] - (result[0] + result[2]) * old[5] / 2;
    result[5] = now[5] / 2 - scale * relative[5] - (result[1] + result[3]) * old[5] / 2;
    for (unsigned i = 0; i < 6; ++i) if (!isfinite(result[i])) { lab_dom_return(0); return; }
    globe_result(result, 6);
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
    /* The viewport fits in this spherical cap. Its tangent meridians give the
     * exact longitude extent; dividing by the cosine at its northern edge
     * greatly over-fetches high-latitude views and can exhaust geometry budgets. */
    double lon_span = south <= -90 || north >= 90 ? 180 :
        asin(fmin(1, sin(span * globe_radian) / cos(latitude * globe_radian))) / globe_radian;
    double values[] = {latitude, longitude, zoom, longitude - lon_span, south, longitude + lon_span, north};
    globe_result(values, 7);
}

/* Select neighbouring source tiles without changing their photographic resolution. */
void lab_globe_photo_patch(double latitude, double longitude)
{
    if (!isfinite(latitude) || !isfinite(longitude) || fabs(latitude) > 90 || fabs(longitude) > 180) {
        lab_dom_return(0);
        return;
    }
    int column = (int)floor((globe_longitude(longitude) + 180) / 30 - .5);
    int row = (int)fmax(0, fmin(4, floor((90 - latitude) / 30 - .5)));
    double west = column * 30 - 180, north = 90 - row * 30;
    double values[] = {(column + 12) % 12, (column + 13) % 12, row, west, north - 60, west + 60, north};
    globe_result(values, 7);
}

static bool globe_camera_read(jsref camera, double values[5])
{
    if (lab_dom_type(camera) != LAB_VALUE_ARRAY || lab_dom_count(camera) != 5) return false;
    for (unsigned i = 0; i < 5; ++i) {
        jsref value = lab_dom_item(camera, i);
        if (lab_dom_type(value) != LAB_VALUE_NUMBER) return false;
        values[i] = lab_dom_to_number(value);
        if (!isfinite(values[i])) return false;
    }
    return quaternion_is_unit(values, 1e-6) && values[4] >= 1 && values[4] <= 4096;
}

/* Do not restore a camera which hides the location named by its history record. */
void lab_globe_history_camera(jsref camera, double latitude, double longitude, double zoom)
{
    double values[5], matrix[9];
    if (!isfinite(latitude) || !isfinite(longitude) || fabs(latitude) > 90 || fabs(longitude) > 180 ||
        !isfinite(zoom) || zoom < 1 || zoom > 4096) { lab_dom_return(0); return; }
    bool valid = globe_camera_read(camera, values);
    if (valid && quaternion_matrix3(matrix, values)) {
        double p = latitude * globe_radian, l = longitude * globe_radian;
        double point[] = {cos(p) * sin(l), sin(p), cos(p) * cos(l)}, projected[3] = {0};
        for (unsigned row = 0; row < 3; ++row)
            for (unsigned col = 0; col < 3; ++col) projected[row] += matrix[row * 3 + col] * point[col];
        if (projected[2] > 0 && fmax(fabs(projected[0]), fabs(projected[1])) * values[4] <= 1.05) {
            globe_result(values, 5);
            return;
        }
    }
    const double x[] = {1, 0, 0}, y[] = {0, 1, 0};
    quaternion_t north, east;
    if (!quaternion_axis_angle(north, x, latitude * globe_radian) ||
        !quaternion_axis_angle(east, y, -longitude * globe_radian) ||
        !quaternion_multiply(values, north, east)) { lab_dom_return(0); return; }
    if (!valid) values[4] = zoom;
    globe_result(values, 5);
}

static double globe_ease(double t)
{
    t = fmax(0, fmin(1, t));
    return t * t * t * (10 + t * (-15 + 6 * t));
}

static double globe_flight_zoom(double from, double to, double fraction)
{
    if (from == to || fraction <= 0) return from;
    if (fraction >= 1) return to;
    /* Interpolate visible span, not magnification: closing in must not outrun the approach. */
    return 1 / ((1 - fraction) / from + fraction / to);
}

/* Transport the view centre along a great circle; restore roll separately about the screen normal. */
static bool globe_flight_rotation(quaternion_t result, const quaternion_t start, const quaternion_t target,
                                  const double from[3], const double to[3], double heading, double turn, double settle)
{
    quaternion_t arc, inverse_arc, travelled, transported, departure, aligned, end, inverse_end, roll, partial_roll;
    const quaternion_t identity = {0, 0, 0, 1};
    const double normal[3] = {0, 0, 1};
    if (!quaternion_from_vectors(arc, from, to)) return false;
    /* The arc axis also defines a deterministic departure tangent for antipodal centres. */
    double tangent[3] = {arc[1] * from[2] - arc[2] * from[1],
                         arc[2] * from[0] - arc[0] * from[2],
                         arc[0] * from[1] - arc[1] * from[0]}, screen[3];
    if (!quaternion_rotate(screen, start, tangent)) return false;
    double bearing = hypot(screen[0], screen[1]) > 1e-12 ? atan2(screen[0], screen[1]) : 0;
    if (!quaternion_axis_angle(departure, normal, bearing * heading) ||
        !quaternion_multiply(aligned, departure, start) || !quaternion_conjugate(inverse_arc, arc) ||
        !quaternion_slerp(travelled, identity, inverse_arc, turn) ||
        !quaternion_multiply(transported, aligned, travelled) ||
        !quaternion_multiply(end, aligned, inverse_arc) || !quaternion_inverse(inverse_end, end) ||
        !quaternion_multiply(roll, target, inverse_end)) return false;
    /* Both endpoint views now have the same centre. Remove round-off in the other axes
     * so interpolating the residual roll cannot divert the geographic path. */
    roll[0] = roll[1] = 0;
    return quaternion_slerp(partial_roll, identity, roll, settle) &&
           quaternion_multiply(result, partial_roll, transported) && quaternion_normalise(result, result);
}

static bool globe_flight_frame(const double a[5], const double b[5], double progress, bool redirect, double result[5])
{
    if (progress <= 0 || progress >= 1) {
        for (unsigned i = 0; i < 5; ++i) result[i] = progress <= 0 ? a[i] : b[i];
        return true;
    }
    double start_matrix[9], target_matrix[9];
    if (!quaternion_matrix3(start_matrix, a) || !quaternion_matrix3(target_matrix, b)) {
        return false;
    }
    const double *from = start_matrix + 6, *to = target_matrix + 6;
    double projected[3] = {0};
    for (unsigned row = 0; row < 3; ++row)
        for (unsigned col = 0; col < 3; ++col) projected[row] += start_matrix[3 * row + col] * to[col];
    /* Fit the remaining half-route, not automatically the entire globe. Keep already-visible
     * destinations inside the square viewport by preserving their departure heading. */
    double extent = fmax(fabs(projected[0]), fabs(projected[1]));
    bool visible = projected[2] > 0 && extent * a[4] <= 1.12;
    bool align_heading = projected[2] <= 0 || hypot(projected[0], projected[1]) * a[4] > 1.12 * sqrt(2);
    double half_span = sqrt(fmax(0, (1 - fmin(1, projected[2])) / 2));
    double cruise = fmin(a[4], b[4]);
    if (!visible && half_span > 0) cruise = fmin(cruise, 1.05 / half_span);
    /* Quintic easing joins stationary phases with zero velocity and acceleration.
     * Progress is paced separately according to route distance and projected screen motion. */
    double departure = redirect ? 0 : 0.125;
    double travel = fmax(0, fmin(1, (progress - departure) / (0.875 - departure)));
    /* Redirects retain a non-zero initial route speed rather than restarting a
     * stationary easing curve. Both curves finish with zero speed and acceleration. */
    double turn = redirect ? travel + travel * travel * travel * (4 + travel * (-7 + 3 * travel))
                           : globe_ease(travel);
    /* A redirected route transports the visible orientation, without another stationary departure spin. */
    if (!globe_flight_rotation(result, a, b, from, to, !redirect && align_heading ? globe_ease(progress / 0.125) : 0, turn,
                               globe_ease((progress - 0.875) / 0.125))) return false;
    if (travel <= 0) result[4] = a[4];
    else if (travel >= 1) result[4] = b[4];
    else if (a[4] == cruise && b[4] == cruise) result[4] = cruise;
    else if (turn < 0.5) {
        /* Angular motion is already eased. Ease only the midpoint tangent here:
         * a second stationary endpoint easing would zoom in ahead of the destination. */
        double fraction = turn * 2, zoom = fraction * (2 - fraction);
        result[4] = globe_flight_zoom(a[4], cruise, zoom);
    } else {
        double fraction = turn * 2 - 1, zoom = fraction * fraction;
        result[4] = globe_flight_zoom(cruise, b[4], zoom);
    }
    return true;
}

/* Display-only interpolation; never alter the observer or retain browser handles between frames. */
void lab_globe_flight(jsref start, jsref target, double progress, bool redirect)
{
    double a[5], b[5], result[5];
    if (!isfinite(progress) || !globe_camera_read(start, a) || !globe_camera_read(target, b) ||
        !globe_flight_frame(a, b, progress, redirect, result)) { lab_dom_return(0); return; }
    globe_result(result, 5);
}

static double globe_flight_motion(const double a[5], const double b[5], double viewport)
{
    quaternion_t inverse, relative;
    double matrix[9];
    if (!quaternion_conjugate(inverse, a) || !quaternion_multiply(relative, b, inverse) ||
        !quaternion_matrix3(matrix, relative)) return INFINITY;
    double tilt = atan2(hypot(matrix[6], matrix[7]), matrix[8]);
    double roll = fabs(atan2(matrix[3] - matrix[1], matrix[0] + matrix[4]));
    /* Centre translation, edge rotation and scale change share one pixel budget. Roll is
     * independent of magnification; multiplying it by zoom would stall close-view arrivals. */
    return viewport * (tilt * fmax(a[4], b[4]) / 2.24 + roll * .5 + fabs(log(b[4] / a[4])) * .5);
}

/* Advance a bounded display clock without catching up after a delayed browser frame. */
void lab_globe_flight_advance(jsref start, jsref target, double progress, double elapsed_ms, double viewport, bool redirect)
{
    double a[5], b[5], before[5], after[5], am[9], bm[9];
    if (!isfinite(progress) || progress < 0 || progress > 1 || !isfinite(elapsed_ms) || elapsed_ms < 0 ||
        !isfinite(viewport) || viewport <= 0 || !globe_camera_read(start, a) || !globe_camera_read(target, b) ||
        !quaternion_matrix3(am, a) || !quaternion_matrix3(bm, b) ||
        !globe_flight_frame(a, b, progress, redirect, before)) { lab_dom_return(0); return; }
    double dot = am[6] * bm[6] + am[7] * bm[7] + am[8] * bm[8];
    double angle = acos(fmax(-1, fmin(1, dot)));
    double seconds = fmin(elapsed_ms, 50) / 1000;
    double budget = fmin(6, 360 * seconds);
    /* Heading and arrival roll are short local operations, not fractions of a long route. */
    double duration = !redirect && progress < .125 ? 1.2 / .125 :
                      progress >= .875 ? 2.5 / .125 : (2 + 6 * angle) / (redirect ? .875 : .75);
    double low = progress, high = fmin(1, progress + seconds / duration);
    if (!globe_flight_frame(a, b, high, redirect, after)) { lab_dom_return(0); return; }
    if (globe_flight_motion(before, after, viewport) > budget) {
        /* Fixed-cost search bounds angular motion as well as the rapid final magnification. */
        for (unsigned i = 0; i < 16; ++i) {
            double middle = (low + high) / 2;
            if (!globe_flight_frame(a, b, middle, redirect, after)) { lab_dom_return(0); return; }
            if (globe_flight_motion(before, after, viewport) > budget) high = middle;
            else low = middle;
        }
        high = low;
    }
    lab_dom_return(lab_dom_numeric(high));
}
