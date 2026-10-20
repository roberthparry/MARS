/**
 * @file shoreline_paths.c
 * @brief Spatially selected, streamed vector polygons for shoreline display.
 *
 * Visits cached rings through their spatial tree, retaining enclosing land and lake polygons even when
 * their boundaries lie outside the viewport. Four streaming half-plane clippers bound output without retaining
 * a full ring. Display-only distance simplification follows clipping; the stored source and hit-tests are unchanged.
 */
#include <math.h>
#include <stdint.h>
#include <string.h>

#define MARS_GLOBE_INTERNAL_ACCESS
#include "globe_layers.h"
#define MARS_SHORELINE_INTERNAL_ACCESS
#include "shoreline_internal.h"

typedef struct { double x, y; } coast_point_t;
typedef struct {
    coast_point_t first, previous;
    bool present;
} coast_clip_t;
typedef struct {
    coast_clip_t clips[4];
    double limits[4], tolerance;
    coast_point_t last;
    unsigned level;
    bool emitted, ok, antarctic;
    globe_path_fn visit;
    void *context;
} coast_path_t;

static double coast_distance(coast_path_t *path, unsigned stage, coast_point_t point)
{
    double coordinate = stage < 2 ? point.x : point.y;
    return stage % 2 ? path->limits[stage] - coordinate : coordinate - path->limits[stage];
}

static void coast_feed(coast_path_t *path, unsigned stage, coast_point_t point);

static void coast_edge(coast_path_t *path, unsigned stage, coast_point_t a, coast_point_t b)
{
    double da = coast_distance(path, stage, a), db = coast_distance(path, stage, b);
    if ((da >= 0) != (db >= 0)) {
        double fraction = da / (da - db);
        coast_point_t intersection = {a.x + fraction * (b.x - a.x), a.y + fraction * (b.y - a.y)};
        coast_feed(path, stage + 1, intersection);
    }
    if (db >= 0) coast_feed(path, stage + 1, b);
}

static void coast_feed(coast_path_t *path, unsigned stage, coast_point_t point)
{
    if (!path->ok) return;
    if (stage == 4) {
        if (path->emitted && hypot(point.x - path->last.x, point.y - path->last.y) < path->tolerance) return;
        path->ok = path->visit(path->emitted ? GLOBE_PATH_LINE : GLOBE_PATH_MOVE,
                              point.x, point.y, path->level, path->antarctic, path->context);
        path->emitted = true;
        path->last = point;
        return;
    }
    coast_clip_t *clip = &path->clips[stage];
    if (clip->present)
        coast_edge(path, stage, clip->previous, point);
    else {
        clip->first = point;
        clip->present = true;
    }
    clip->previous = point;
}

static void coast_finish(coast_path_t *path)
{
    for (unsigned stage = 0; stage < 4; ++stage) {
        coast_clip_t *clip = &path->clips[stage];
        if (clip->present) coast_edge(path, stage, clip->previous, clip->first);
    }
    if (path->ok && path->emitted)
        path->ok = path->visit(GLOBE_PATH_CLOSE, 0, 0, path->level, path->antarctic, path->context);
    memset(path->clips, 0, sizeof path->clips);
    path->emitted = false;
}

typedef struct {
    coast_path_t path;
    int wrap, lod;
} coast_display_t;

static bool coast_display_ring(const coast_geometry_t *geometry, const coast_ring_t *ring, void *context)
{
    coast_display_t *display = context;
    coast_path_t *path = &display->path;
    path->level = ring->level;
    path->antarctic = ring->antarctic;
    uint32_t count = display->lod < 0 ? ring->count : ring->lod_count[display->lod];
    for (uint32_t i = 0; path->ok && i < count; ++i) {
        uint32_t index = display->lod < 0 ? ring->first + i : geometry->lod[ring->lod_first[display->lod] + i];
        coast_vertex_t vertex = geometry->vertices[index];
        coast_point_t point = {vertex.x * 0.000001 - display->wrap * 360, vertex.y * 0.000001};
        coast_feed(path, 0, point);
    }
    coast_finish(path);
    return path->ok;
}

/* Clip spatially indexed resident geometry, retaining exact coordinates for close views and hit-testing. */
bool shoreline_paths(shoreline_t *shoreline, double west, double south, double east, double north,
                     double tolerance, globe_path_fn visit, void *context)
{
    if (!shoreline || !visit || !isfinite(west) || !isfinite(east) || !isfinite(south) || !isfinite(north) ||
        !isfinite(tolerance) || tolerance < 0 || tolerance > 1 || west < -360 || west > 180 ||
        east <= west || east - west > 360 || south < -90 || north > 90 || north <= south)
        return false;
    coast_geometry_t *geometry = coast_geometry_acquire(shoreline, true);
    if (!geometry) return false;
    coast_display_t display = {
        .path = {.limits = {west, east, south, north}, .tolerance = tolerance,
                 .ok = true, .visit = visit, .context = context},
        .lod = coast_geometry_lod(tolerance)
    };
    for (display.wrap = -1; display.path.ok && display.wrap <= 1; ++display.wrap) {
        double bounds[] = {(west + display.wrap * 360) * 1000000, south * 1000000,
                           (east + display.wrap * 360) * 1000000, north * 1000000};
        display.path.ok = coast_geometry_visit(geometry, bounds, tolerance * 1000000, coast_display_ring, &display);
    }
    coast_geometry_release(geometry);
    return display.path.ok;
}
