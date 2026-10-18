/**
 * @file lab_plot_pinch.c
 * @brief Two-finger curve-range zoom and retained surface-camera gesture state.
 *
 * Curve zoom follows the touch midpoint and edits sampling ranges. Surface gestures
 * retain their initial midpoint and separation for native volume zoom, including
 * input-only refinement of space curves. The drag controller coalesces updates
 * and owns completion. Cancellation restores curve controls or delegates full
 * retained-volume restoration to the native surface zoom policy.
 */
#include <math.h>

#include "lab_dom.h"
#include "lab_plot_controls.h"
#include "lab_plot_pointer.h"
#include "lab_plot_pinch.h"

static double value(browser_jsref object, const char *key)
{
    return lab_dom_to_number(lab_dom_get(object, key));
}

static const char *control_id(unsigned i, int curve)
{
    static const char *const curve_ids[] = {"plotLower", "plotUpper", "plotYLower", "plotYUpper", "plotScale"};
    static const char *const surface_ids[] = {"plotLower", "plotUpper", "plotSecondLower", "plotSecondUpper",
                                             "plotYLower", "plotYUpper", "plotScale"};
    return curve ? curve_ids[i] : surface_ids[i];
}

/* Keep pointer IDs as opaque DOM values, including zero. */
bool lab_plot_pinch_start(gesture_jsref state, event_jsref event)
{
    if (!lab_dom_truth(lab_dom_get(state, "touch")) || lab_dom_truth(lab_dom_get(state, "pinch")) ||
        !lab_dom_equal(lab_dom_get(event, "pointerType"), lab_dom_string("touch")) ||
        lab_dom_equal(lab_dom_get(event, "pointerId"), lab_dom_get(state, "pointerId")))
        return false;
    int chart = lab_dom_get(state, "surface"), curve = lab_dom_truth(lab_dom_get(state, "pan"));
    int panel = lab_dom_closest(lab_dom_get(event, "target"), ".complex-panel");
    if (panel && !lab_dom_node_contains(panel, chart))
        return false;
    if (!lab_dom_has(chart, "data-x-min") ||
        !lab_dom_node_contains(lab_dom_id("plotRendered"), lab_dom_get(event, "target")))
        return false;
    double x = value(event, "clientX"), y = value(event, "clientY");
    double dx = x - value(state, "touchX"), dy = y - value(state, "touchY");
    double distance = hypot(dx, dy);
    double sx = lab_dom_measure(chart, LAB_DOM_MEASURE_CTM_A), sy = lab_dom_measure(chart, LAB_DOM_MEASURE_CTM_D);
    if (!(distance >= 8) || !isfinite(distance) || !(sx > 0) || !(sy > 0))
        return false;
    double box[] = {lab_dom_measure(chart, LAB_DOM_MEASURE_CTM_E), lab_dom_measure(chart, LAB_DOM_MEASURE_CTM_F),
                    lab_dom_measure(chart, LAB_DOM_MEASURE_VIEWBOX_WIDTH) * sx,
                    lab_dom_measure(chart, LAB_DOM_MEASURE_VIEWBOX_HEIGHT) * sy};
    if (curve) {
        box[0] += lab_dom_to_number(lab_dom_read(chart, LAB_DOM_DATASET, "plotLeft")) * sx;
        box[1] += lab_dom_to_number(lab_dom_read(chart, LAB_DOM_DATASET, "plotTop")) * sy;
        box[2] = lab_dom_to_number(lab_dom_read(chart, LAB_DOM_DATASET, "plotWidth")) * sx;
        box[3] = lab_dom_to_number(lab_dom_read(chart, LAB_DOM_DATASET, "plotHeight")) * sy;
    }
    double fx = curve ? ((x + value(state, "touchX")) / 2 - box[0]) / box[2] : 0.5;
    double fy = curve ? 1 - ((y + value(state, "touchY")) / 2 - box[1]) / box[3] : 0.5;
    if (!(fx >= 0 && fx <= 1 && fy >= 0 && fy <= 1))
        return false;
    int geometry = lab_dom_object(LAB_VALUE_ARRAY), snapshot = lab_dom_object(LAB_VALUE_ARRAY);
    static const char *const keys[] = {"xMin", "xMax", "yMin", "yMax"};
    for (unsigned i = 0; i < (curve ? 5u : 7u); ++i) {
        int control = lab_dom_id(control_id(i, curve));
        int text = lab_dom_read(control, LAB_DOM_VALUE, ""), exact = lab_dom_get(control, "marsZoomValue");
        lab_dom_push(snapshot, text);
        lab_dom_push(snapshot, lab_dom_get(control, "marsZoomText"));
        lab_dom_push(snapshot, exact);
        if (i < 4) {
            int plane = lab_dom_get(lab_dom_id("plotRendered"), "marsComplexPlaneBounds");
            if (lab_plot_is_plane(chart)) {
                lab_dom_push(geometry,
                             lab_dom_numeric(lab_dom_to_number(lab_dom_count(plane) == 4
                                                                   ? lab_dom_item(plane, i)
                                                                   : lab_dom_read(chart, LAB_DOM_DATASET, keys[i]))));
                continue;
            }
            int pending = curve && lab_dom_truth(lab_dom_get(state, "changed")) &&
                          lab_dom_equal(text, lab_dom_get(control, "marsZoomText"));
            lab_dom_push(geometry,
                         pending ? lab_dom_numeric(lab_dom_to_number(exact))
                                 : lab_dom_numeric(lab_dom_to_number(lab_dom_read(chart, LAB_DOM_DATASET, keys[i]))));
        }
    }
    lab_dom_push(geometry, lab_dom_numeric(fx));
    lab_dom_push(geometry, lab_dom_numeric(fy));
    for (unsigned i = 0; i < 4; ++i)
        lab_dom_push(geometry, lab_dom_numeric(box[i]));
    if (!curve) {
        lab_dom_push(geometry, lab_dom_numeric(lab_dom_to_number(lab_dom_read(chart, LAB_DOM_DATASET, "zMin"))));
        lab_dom_push(geometry, lab_dom_numeric(lab_dom_to_number(lab_dom_read(chart, LAB_DOM_DATASET, "zMax"))));
    }
    lab_dom_set(state, "pinchGeometry", geometry);
    lab_dom_set(state, "pinchAutoHeight", lab_dom_scalar(LAB_VALUE_BOOLEAN, lab_plot_auto_height(chart)));
    lab_dom_set(state, "pinchSnapshot", curve ? lab_dom_get(state, "panSnapshot") : snapshot);
    lab_dom_set(state, "pinchDistance", lab_dom_numeric(distance));
    lab_dom_set(state, "pinchAnchorX", lab_dom_numeric((x + value(state, "touchX")) / 2));
    lab_dom_set(state, "pinchAnchorY", lab_dom_numeric((y + value(state, "touchY")) / 2));
    lab_dom_delete(state, "surfaceCamera");
    lab_dom_delete(state, "surfaceExponent");
    lab_dom_delete(state, "surfaceZoomSnapshot");
    lab_dom_delete(state, "pinchCancelled");
    lab_dom_set(state, "secondId", lab_dom_get(event, "pointerId"));
    lab_dom_set(state, "secondX", lab_dom_numeric(x));
    lab_dom_set(state, "secondY", lab_dom_numeric(y));
    lab_dom_set(state, "pinch", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
    return true;
}

/* Update surface gesture coordinates, or scale curve ranges from their original baseline. */
bool lab_plot_pinch_move(gesture_jsref state, event_jsref event)
{
    int first = lab_dom_equal(lab_dom_get(event, "pointerId"), lab_dom_get(state, "pointerId"));
    lab_dom_set(state, first ? "touchX" : "secondX", lab_dom_get(event, "clientX"));
    lab_dom_set(state, first ? "touchY" : "secondY", lab_dom_get(event, "clientY"));
    double dx = value(state, "touchX") - value(state, "secondX");
    double dy = value(state, "touchY") - value(state, "secondY");
    double distance = hypot(dx, dy);
    if (!(distance >= 8) || !isfinite(distance))
        return false;
    if (!lab_dom_truth(lab_dom_get(state, "pan")))
        return true;
    double factor = value(state, "pinchDistance") / distance;
    int curve = lab_dom_truth(lab_dom_get(state, "pan")), geometry = lab_dom_get(state, "pinchGeometry");
    double g[12], bounds[6];
    for (unsigned i = 0; i < (curve ? 10u : 12u); ++i)
        g[i] = lab_dom_to_number(lab_dom_item(geometry, i));
    double fractions[] = {curve ? ((value(state, "touchX") + value(state, "secondX")) / 2 - g[6]) / g[8] : 0.5,
                          curve ? 1 - ((value(state, "touchY") + value(state, "secondY")) / 2 - g[7]) / g[9] : 0.5,
                          0.5};
    for (unsigned axis = 0; axis < (curve ? 2u : 3u); ++axis) {
        unsigned at = axis == 2 ? 10 : axis * 2;
        double lo = g[at], hi = g[at + 1], span = (hi - lo) * factor;
        double fraction = axis == 2 ? 0.5 : g[4 + axis];
        double anchor = lo * (1 - fraction) + hi * fraction;
        bounds[axis * 2] = anchor - span * fractions[axis];
        bounds[axis * 2 + 1] = anchor + span * (1 - fractions[axis]);
        if (!isfinite(bounds[axis * 2]) || !isfinite(bounds[axis * 2 + 1]) ||
            bounds[axis * 2] >= bounds[axis * 2 + 1])
            return false;
    }
    if (lab_dom_truth(lab_dom_get(state, "panPlane"))) {
        lab_plot_plane_bounds(bounds);
        return true;
    }
    int range = lab_dom_id("plotRange"), title_node = lab_dom_query(0, "#plotRendered [data-plot-figure] > title");
    int title = lab_dom_get(title_node, "marsPlotLabel");
    if (!lab_dom_truth(title))
        title = lab_dom_read(title_node, LAB_DOM_TEXT, "");
    title = lab_dom_join(title, lab_dom_string(curve ? "curve" : "surface"), "");
    if (!lab_dom_equal(title, lab_dom_get(range, "marsZoomTitle"))) {
        lab_dom_set(range, "marsZoomTitle", title);
        lab_dom_delete(range, "marsZoomOriginal");
    }
    if (!lab_dom_get(range, "marsZoomOriginal")) {
        int original = lab_dom_object(LAB_VALUE_ARRAY), snapshot = lab_dom_get(state, "pinchSnapshot");
        for (unsigned i = 0; i < (curve ? 5u : 7u); ++i)
            lab_dom_push(original, lab_dom_item(snapshot, i * 3));
        lab_dom_set(range, "marsZoomOriginal", original);
    }
    int auto_height = lab_dom_truth(lab_dom_get(state, "pinchAutoHeight"));
    for (unsigned i = 0; i < (auto_height ? 2u : curve ? 4u : 6u); ++i) {
        int control = lab_dom_id(control_id(i, curve));
        unsigned lower = (i / 2) * 2;
        lab_plot_write_bound(control, bounds[i], bounds[lower + 1] - bounds[lower]);
    }
    if (!auto_height)
        lab_dom_write(lab_dom_id("plotScale"), LAB_DOM_VALUE, "", lab_dom_string("manual"));
    lab_dom_delete(range, "marsZoomChart");
    lab_plot_controls_sync();
    return true;
}

/* Cancellation restores authored controls as well as their unrounded coordinates. */
void lab_plot_pinch_cancel(gesture_jsref state)
{
    if (lab_dom_truth(lab_dom_get(state, "panPlane"))) {
        lab_plot_pan_cancel(state);
        return;
    }
    if (!lab_dom_truth(lab_dom_get(state, "pan"))) {
        lab_dom_set(state, "pinchCancelled", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
        return;
    }
    int curve = lab_dom_truth(lab_dom_get(state, "pan")), snapshot = lab_dom_get(state, "pinchSnapshot");
    for (unsigned i = 0; i < (curve ? 5u : 7u); ++i) {
        int control = lab_dom_id(control_id(i, curve));
        lab_dom_write(control, LAB_DOM_VALUE, "", lab_dom_item(snapshot, i * 3));
        lab_dom_set(control, "marsZoomText", lab_dom_item(snapshot, i * 3 + 1));
        lab_dom_set(control, "marsZoomValue", lab_dom_item(snapshot, i * 3 + 2));
    }
    lab_dom_delete(lab_dom_id("plotRange"), "marsZoomChart");
    lab_plot_controls_sync();
}
