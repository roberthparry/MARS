/**
 * @file globe_view.c
 * @brief Pure orthographic point projection and spherical-cap bounds.
 *
 * Shared by native and WASM consumers without database access. Callers choose
 * the spherical cap radius; zoom limits, padding and caching policy remain outside
 * the module. The routines do not read application configuration or browser state.
 */
#include <math.h>
#include <string.h>

#include "globe-t.h"

static const double view_radian = 0.017453292519943295;

static bool view_rotation_valid(const double rotation[9])
{
    if (!rotation) return false;
    for (unsigned i = 0; i < 9; ++i) if (!isfinite(rotation[i])) return false;
    return true;
}

/* Use the same orthographic point transform as the globe shaders. */
bool globe_view_project(const double rotation[9], double latitude, double longitude, double point[3])
{
    if (!point || !view_rotation_valid(rotation) || !isfinite(latitude) || !isfinite(longitude) ||
        fabs(latitude) > 90) return false;
    double p = latitude * view_radian, l = longitude * view_radian;
    const double world[] = {cos(p) * sin(l), sin(p), cos(p) * cos(l)};
    double result[3] = {0};
    for (unsigned row = 0; row < 3; ++row)
        for (unsigned col = 0; col < 3; ++col) result[row] += rotation[row * 3 + col] * world[col];
    result[1] = -result[1];
    memcpy(point, result, sizeof result);
    return true;
}

/* Bound a padded viewport by the tangent meridians of its spherical cap. */
bool globe_view_bounds(const double rotation[9], double radius, double view[6])
{
    if (!view || !view_rotation_valid(rotation) || !isfinite(radius) || radius < 0 || radius > 90) return false;
    double latitude = asin(fmax(-1, fmin(1, rotation[7]))) / view_radian;
    double longitude = atan2(rotation[6], rotation[8]) / view_radian;
    longitude -= 360 * floor((longitude + 180) / 360);
    double span = radius;
    double south = fmax(-90, latitude - span), north = fmin(90, latitude + span);
    double longitude_span = south <= -90 || north >= 90 ? 180 :
        asin(fmin(1, sin(span * view_radian) / cos(latitude * view_radian))) / view_radian;
    const double result[] = {latitude, longitude, longitude - longitude_span, south,
                             longitude + longitude_span, north};
    memcpy(view, result, sizeof result);
    return true;
}
