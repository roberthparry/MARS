/**
 * @file lab_surface_visibility.c
 * @brief Native axis visibility fragments for software rendering and vector export.
 *
 * Projected surface triangles enter the shared spatial index. Axis faces keep
 * their complete faint drawing and receive convex visible fragments for opaque
 * overpainting. Holes remain clear; no browser mathematical interpretation or
 * sampled coordinate evaluation is involved.
 */
#include <stdbool.h>
#include <stddef.h>

#define MARS_SHARED_PLOT_INTERNAL_ACCESS
#include "internal/plot_internal.h"
#include "lab_dom.h"
#include "lab_surface_geometry.h"

static bool polygon(jsref source, double distance, plot_occlusion_polygon_t *out)
{
    int points = lab_dom_get(source, "points"), depths = lab_dom_get(source, "depths");
    unsigned count = lab_dom_count(points);
    if (count < 3 || count > PLOT_OCCLUSION_POINTS || lab_dom_count(depths) != count) return false;
    out->count = count;
    for (unsigned i = 0; i < count; ++i) {
        int point = lab_dom_item(points, i);
        out->point[i][0] = lab_dom_to_number(lab_dom_item(point, 0));
        out->point[i][1] = lab_dom_to_number(lab_dom_item(point, 1));
        double depth = lab_dom_to_number(lab_dom_item(depths, i));
        out->point[i][2] = distance ? 1 / (distance + depth) : -depth;
    }
    return true;
}

static bool emit(const plot_occlusion_polygon_t *polygon, void *context)
{
    jsref result = *(jsref *)context;
    unsigned mark = lab_dom_mark();
    int points = lab_dom_object(LAB_VALUE_ARRAY);
    for (unsigned i = 0; i < polygon->count; ++i) {
        int point = lab_dom_object(LAB_VALUE_ARRAY);
        lab_dom_push(point, lab_dom_numeric(polygon->point[i][0]));
        lab_dom_push(point, lab_dom_numeric(polygon->point[i][1]));
        lab_dom_push(points, point);
    }
    lab_dom_push(result, points);
    lab_dom_release(mark);
    return true;
}

/* Attach geometry only; colour and opacity remain presentation supplied by the frame. */
void lab_surface_visibility(frame_jsref frame, jsref polygons)
{
    if (lab_dom_truth(lab_dom_get(frame, "axisVisibility"))) return;
    plot_occlusion_t *index = plot_occlusion_new();
    bool ok = index != NULL;
    double distance = lab_dom_to_number(lab_dom_get(frame, "distance"));
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; ok && i < lab_dom_count(polygons); ++i) {
        plot_occlusion_polygon_t face;
        if (polygon(lab_dom_item(polygons, i), distance, &face)) {
            for (unsigned j = 1; ok && j + 1 < face.count; ++j) {
                double triangle[3][3];
                unsigned corners[] = {0, j, j + 1};
                for (unsigned k = 0; k < 3; ++k)
                    for (unsigned axis = 0; axis < 3; ++axis) triangle[k][axis] = face.point[corners[k]][axis];
                ok = plot_occlusion_add(index, triangle);
            }
        }
        lab_dom_release(mark);
    }
    ok = ok && plot_occlusion_build(index);
    int solids = lab_dom_get(frame, "solids");
    for (unsigned i = 0; ok && i < lab_dom_count(solids); ++i) {
        int source = lab_dom_item(solids, i);
        plot_occlusion_polygon_t face;
        if (polygon(source, distance, &face)) {
            jsref visible = lab_dom_object(LAB_VALUE_ARRAY);
            ok = plot_occlusion_visible(index, &face, emit, &visible);
            if (ok) lab_dom_set(source, "visible", visible);
        }
        lab_dom_release(mark);
    }
    plot_occlusion_free(index);
    if (ok) lab_dom_set(frame, "axisVisibility", lab_dom_scalar(LAB_VALUE_BOOLEAN, true));
}
