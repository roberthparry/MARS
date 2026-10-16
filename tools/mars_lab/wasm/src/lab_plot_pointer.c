/**
 * @file lab_plot_pointer.c
 * @brief Curve inspection and routing of curve-range zoom versus retained surface-camera zoom.
 *
 * Uses native viewport metadata and browser geometry to inspect the horizontal
 * coordinate. Curve zoom edits ranges and requests native resampling. Surface
 * zoom delegates to the retained volume preview; this module formats its range fields.
 * Toolbar buttons use the centre; Reset restores the corresponding viewport.
 * Double coordinate metadata is a UI approximation; non-finite or collapsed ranges
 * are left to manual native bounds. No expression is interpreted in the browser.
 */
#include "lab_dom.h"
#include "lab_workspace.h"
#include "lab_plot_controls.h"
#include "lab_plot_pointer.h"
#include "lab_plot_drag.h"

static double number(int node, const char *key)
{
    return lab_dom_to_number(lab_dom_get(node, key));
}

static double data(int node, const char *key)
{
    return lab_dom_to_number(lab_dom_read(node, 3, key));
}

static int finite(double value)
{
    return __builtin_isfinite(value);
}

static int label_text(int title)
{
    int saved = lab_dom_get(title, "marsPlotLabel");
    if (lab_dom_truth(saved))
        return saved;
    int text = lab_dom_read(title, 0, "");
    return lab_dom_truth(text) ? text : lab_dom_read(lab_dom_get(title, "parentNode"), 2, "aria-label");
}

/* Modify only the live DOM; native SVG retained in the result record remains copyable with its labels. */
void lab_plot_pointer_prepare(int rendered)
{
    if (!lab_dom_query(rendered, "[data-plot-chart][data-x-min]:not([data-surface])"))
        return;
    int titles = lab_dom_all(rendered, "svg title");
    for (unsigned i = 0; i < lab_dom_count(titles); ++i) {
        int title = lab_dom_item(titles, i), text = label_text(title);
        lab_dom_set(title, "marsPlotLabel", text);
        lab_dom_write(lab_dom_get(title, "parentNode"), 2, "aria-label", text);
        lab_dom_write(title, 0, "", lab_dom_string(""));
    }
}

/* Decimal orders are bounded by binary64's exponent range; no mathematical source is interpreted. */
static int decimal_order(double value)
{
    int order = 0;
    value = __builtin_fabs(value);
    if (value == 0)
        return 0;
    while (value >= 10) {
        value /= 10;
        ++order;
    }
    while (value < 1) {
        value *= 10;
        --order;
    }
    return order;
}

/* Match displayed precision to the interval, bounded by browser coordinate precision. */
unsigned lab_plot_bound_digits(double value, double span)
{
    if (!finite(value) || !finite(span) || span <= 0)
        return 17;
    int digits = decimal_order(value) - decimal_order(span) + 4;
    return digits < 1 ? 1 : digits > 17 ? 17 : (unsigned)digits;
}

/* Keep about four significant digits of the span, retaining exact gesture bounds separately. */
void lab_plot_write_bound(int control, double value, double span)
{
    int text = lab_dom_precision(value, lab_plot_bound_digits(value, span));
    lab_dom_set(control, "marsZoomText", text);
    lab_dom_set(control, "marsZoomValue", lab_dom_format(value, "", ""));
    lab_dom_write(control, 5, "", text);
}

/* Use the same interval-aware presentation for all three surface axes. */
void lab_plot_volume_bounds(int bounds)
{
    static const char *const ids[] = {
        "plotLower", "plotUpper", "plotSecondLower", "plotSecondUpper", "plotYLower", "plotYUpper"
    };
    for (unsigned i = 0; i < 6; ++i) {
        unsigned lower = (i / 2) * 2;
        double span = lab_dom_to_number(lab_dom_item(bounds, lower + 1)) -
                      lab_dom_to_number(lab_dom_item(bounds, lower));
        lab_plot_write_bound(lab_dom_id(ids[i]), lab_dom_to_number(lab_dom_item(bounds, i)), span);
    }
}

static int frame(int chart, double box[4], double bounds[4])
{
    /* A nested SVG's bounding rectangle describes its artwork, not its coordinate viewport. */
    double sx = lab_dom_measure(chart, 13), sy = lab_dom_measure(chart, 16);
    if (!(sx > 0) || !(sy > 0) || lab_dom_measure(chart, 14) != 0 || lab_dom_measure(chart, 15) != 0)
        return 0;
    box[0] = lab_dom_measure(chart, 17) + data(chart, "plotLeft") * sx;
    box[1] = lab_dom_measure(chart, 18) + data(chart, "plotTop") * sy;
    box[2] = data(chart, "plotWidth") * sx;
    box[3] = data(chart, "plotHeight") * sy;
    static const char *const keys[] = {"xMin", "xMax", "yMin", "yMax"};
    for (unsigned i = 0; i < 4; ++i) {
        bounds[i] = data(chart, keys[i]);
        if (!finite(bounds[i]) || !finite(box[i]))
            return 0;
    }
    return box[2] > 0 && box[3] > 0 && bounds[0] < bounds[1] && bounds[2] < bounds[3];
}

/* Resolve a projection by its native card identifier, not by its mathematical labels. */
int lab_plot_is_plane(int chart)
{
    return lab_dom_equal(lab_dom_read(lab_dom_closest(chart, ".complex-panel"), 3, "complexPanel"),
                         lab_dom_string("complex_plane"));
}

/* Preserve native automatic height for complex scalar projections during zoom. */
int lab_plot_auto_height(int chart)
{
    int projection = lab_dom_read(lab_dom_closest(chart, ".complex-panel"), 3, "complexPanel");
    return !lab_dom_equal(lab_dom_read(lab_dom_id("plotScale"), 5, ""), lab_dom_string("manual")) &&
           (lab_dom_equal(projection, lab_dom_string("components")) ||
            lab_dom_equal(projection, lab_dom_string("amplitude")));
}

/* Keep Argand viewport coordinates separate from the input and component-value controls. */
void lab_plot_plane_bounds(const double bounds[4])
{
    int values = lab_dom_object(4);
    for (unsigned i = 0; i < 4; ++i)
        lab_dom_push(values, lab_dom_format(bounds[i], "", ""));
    lab_dom_set(lab_dom_id("plotRendered"), "marsComplexPlaneBounds", values);
}

static int overlay(int rendered, const char *selector, const char *class_name)
{
    int node = lab_dom_query(rendered, selector);
    if (!node) {
        node = lab_dom_create("div");
        lab_dom_class(node, class_name, 1);
        lab_dom_write(node, 2, "aria-hidden", lab_dom_string("true"));
        lab_dom_append(rendered, node);
    }
    return node;
}

static void pixels(int node, const char *key, double value)
{
    lab_dom_write(node, 4, key, lab_dom_format(value, "", "px"));
}

static void focus_curve(int rendered, int chart, int target)
{
    int active = 0;
    if (chart && target && lab_workspace_mode() == 2 &&
        lab_dom_count(lab_dom_all(chart, "[data-series]")) > 1) {
        active = lab_dom_closest(target, "[data-series]");
        if (active && !lab_dom_equal(lab_dom_closest(active, "[data-plot-chart]"), chart))
            active = 0;
    }
    int previous = lab_dom_query(rendered, ".plot-curve-active");
    if (lab_dom_equal(previous, active))
        return;
    if (previous)
        lab_dom_class(previous, "plot-curve-active", 0);
    if (active)
        lab_dom_class(active, "plot-curve-active", 1);
}

/* Inspect only rendered line segments, never expressions or gaps. Multiple crossings
 * require a bounded scan of the displayed samples; select the closest visible branch. */
static int curve_height(int chart, double x, double pointer_y, double *height)
{
    int active = lab_dom_query(chart, ".plot-curve-active");
    int paths = lab_dom_all(active ? active : chart, active ? ":scope > path" : "[data-series] > path");
    double best = __builtin_inf();
    int found = 0;
    for (unsigned i = 0; i < lab_dom_count(paths); ++i) {
        unsigned mark = lab_dom_mark();
        int path = lab_dom_item(paths, i);
        int parts = lab_dom_get(path, "marsHoverSegment");
        if (!lab_dom_truth(parts)) {
            parts = lab_dom_split(lab_dom_read(path, 2, "d"), " ");
            lab_dom_set(path, "marsHoverSegment", parts);
        }
        if (lab_dom_count(parts) != 6 || !lab_dom_equal(lab_dom_item(parts, 0), lab_dom_string("M")) ||
            !lab_dom_equal(lab_dom_item(parts, 3), lab_dom_string("L"))) {
            lab_dom_release(mark);
            continue;
        }
        double ax = lab_dom_to_number(lab_dom_item(parts, 1)), ay = lab_dom_to_number(lab_dom_item(parts, 2));
        double bx = lab_dom_to_number(lab_dom_item(parts, 4)), by = lab_dom_to_number(lab_dom_item(parts, 5));
        if (!finite(ax) || !finite(ay) || !finite(bx) || !finite(by) || x < (ax < bx ? ax : bx) ||
            x > (ax > bx ? ax : bx)) {
            lab_dom_release(mark);
            continue;
        }
        double low = ay < by ? ay : by, high = ay > by ? ay : by;
        double y = ax == bx ? (pointer_y < low ? low : pointer_y > high ? high : pointer_y) :
                             ay + (x - ax) * (by - ay) / (bx - ax);
        double distance = __builtin_fabs(y - pointer_y);
        if (distance < best) {
            best = distance;
            *height = y;
            found = 1;
        }
        lab_dom_release(mark);
    }
    return found;
}

/* The guide is transient UI and is not inserted into the copyable SVG. */
void lab_plot_pointer_dispatch(unsigned action, int event, int context)
{
    (void)context;
    lab_dom_return(lab_dom_object(5));
    int rendered = lab_dom_id("plotRendered");
    int panel = lab_dom_closest(lab_dom_get(event, "target"), ".complex-panel");
    int chart = lab_dom_query(panel ? panel : rendered, "[data-plot-chart][data-x-min]:not([data-surface])");
    int line = lab_dom_query(rendered, ".plot-pointer-line"), tip = lab_dom_query(rendered, ".plot-pointer-tip");
    int horizontal = lab_dom_query(rendered, ".plot-pointer-horizontal");
    int marker = lab_dom_query(rendered, ".plot-pointer-marker");
    double box[4], bounds[4], x = number(event, "clientX"), y = number(event, "clientY");
    int dragging = lab_dom_truth(lab_dom_get(lab_dom_get(rendered, "marsPlotDrag"), "active"));
    if (action || dragging || number(event, "buttons") != 0 || !chart || !frame(chart, box, bounds) ||
        x < box[0] || x > box[0] + box[2] ||
        y < box[1] || y > box[1] + box[3]) {
        lab_dom_class(line, "hidden", 1);
        lab_dom_class(horizontal, "hidden", 1);
        lab_dom_class(marker, "hidden", 1);
        lab_dom_class(tip, "hidden", 1);
        focus_curve(rendered, 0, 0);
        return;
    }
    focus_curve(rendered, chart, lab_dom_get(event, "target"));
    line = overlay(rendered, ".plot-pointer-line", "plot-pointer-line");
    tip = overlay(rendered, ".plot-pointer-tip", "plot-pointer-tip");
    horizontal = overlay(rendered, ".plot-pointer-horizontal", "plot-pointer-horizontal");
    marker = overlay(rendered, ".plot-pointer-marker", "plot-pointer-marker");
    lab_dom_class(horizontal, "hidden", 1);
    lab_dom_class(marker, "hidden", 1);
    double left = lab_dom_measure(rendered, 7) - number(rendered, "scrollLeft");
    double top = lab_dom_measure(rendered, 8) - number(rendered, "scrollTop");
    pixels(line, "left", x - left);
    pixels(line, "top", box[1] - top);
    pixels(line, "height", box[3]);
    double fraction = (x - box[0]) / box[2];
    double value = bounds[0] * (1 - fraction) + bounds[1] * fraction;
    int name = lab_plot_is_plane(chart) ? lab_dom_string("ℜ") : lab_dom_read(lab_dom_id("plotFirst"), 5, "");
    int text = lab_dom_join(name, lab_dom_format(value, " ≈ ", ""), "");
    double height = 0;
    double svg_x = data(chart, "plotLeft") + fraction * data(chart, "plotWidth");
    double svg_y = data(chart, "plotTop") + (y - box[1]) / box[3] * data(chart, "plotHeight");
    if (curve_height(chart, svg_x, svg_y, &height)) {
        double vertical_fraction = (height - data(chart, "plotTop")) / data(chart, "plotHeight");
        double vertical_value = bounds[3] * (1 - vertical_fraction) + bounds[2] * vertical_fraction;
        text = lab_dom_join(text, lab_dom_format(vertical_value, lab_plot_is_plane(chart) ? "\nℑ ≈ " : "\ny ≈ ", ""), "");
        pixels(horizontal, "left", box[0] - left);
        pixels(horizontal, "top", box[1] - top + vertical_fraction * box[3]);
        pixels(horizontal, "width", box[2]);
        lab_dom_class(horizontal, "hidden", 0);
        pixels(marker, "left", x - left);
        pixels(marker, "top", box[1] - top + vertical_fraction * box[3]);
        lab_dom_class(marker, "hidden", 0);
    }
    int label = lab_dom_query(chart, ".plot-curve-active > title");
    if (label) {
        int constants = label_text(label);
        if (lab_dom_truth(constants)) {
            int values = lab_dom_split(constants, ", ");
            for (unsigned i = 0; i < lab_dom_count(values); ++i)
                text = lab_dom_join(lab_dom_join(text, lab_dom_string("\n"), ""), lab_dom_item(values, i), "");
        }
    }
    lab_dom_write(tip, 0, "", text);
    lab_dom_class(line, "hidden", 0);
    lab_dom_class(tip, "hidden", 0);
    double width = lab_dom_measure(tip, 3);
    double tip_x = x + 10;
    if (tip_x + width > box[0] + box[2])
        tip_x = x - width - 10;
    if (tip_x < box[0])
        tip_x = box[0];
    pixels(tip, "left", tip_x - left);
    pixels(tip, "top", box[1] - top + 8);
}

/* Register without taking over scrolling, curve tooltips or surface dragging. */
void lab_plot_pointer_install(void)
{
    int rendered = lab_dom_id("plotRendered");
    lab_dom_subscribe(rendered, "pointermove", "lab_plot_pointer_dispatch", 0, 0, 1);
    lab_dom_subscribe(rendered, "pointerleave", "lab_plot_pointer_dispatch", 1, 0, 1);
    lab_dom_subscribe(rendered, "pointercancel", "lab_plot_pointer_dispatch", 1, 0, 1);
    lab_dom_subscribe(rendered, "pointerup", "lab_plot_pointer_dispatch", 1, 0, 1);
    lab_dom_subscribe(rendered, "scroll", "lab_plot_pointer_dispatch", 1, 0, 1);
}

/* Capture immutable screen and range coordinates before asynchronous redraws replace the chart. */
int lab_plot_pan_start(int state, int chart, int event)
{
    double box[4], bounds[4];
    if (!frame(chart, box, bounds))
        return 0;
    double x = number(event, "clientX"), y = number(event, "clientY");
    if (x < box[0] || x > box[0] + box[2] || y < box[1] || y > box[1] + box[3])
        return 0;
    int geometry = lab_dom_object(4), snapshot = lab_dom_object(4);
    static const char *const ids[] = {"plotLower", "plotUpper", "plotYLower", "plotYUpper", "plotScale"};
    for (unsigned i = 0; i < 4; ++i)
        lab_dom_push(geometry, lab_dom_numeric(bounds[i]));
    lab_dom_push(geometry, lab_dom_numeric((bounds[1] - bounds[0]) / box[2]));
    lab_dom_push(geometry, lab_dom_numeric((bounds[3] - bounds[2]) / box[3]));
    lab_dom_push(geometry, lab_dom_numeric(lab_dom_measure(chart, 13)));
    lab_dom_push(geometry, lab_dom_numeric(lab_dom_measure(chart, 16)));
    for (unsigned i = 0; i < 5; ++i) {
        int control = lab_dom_id(ids[i]);
        lab_dom_push(snapshot, lab_dom_read(control, 5, ""));
        lab_dom_push(snapshot, lab_dom_get(control, "marsZoomText"));
        lab_dom_push(snapshot, lab_dom_get(control, "marsZoomValue"));
    }
    lab_dom_set(state, "panGeometry", geometry);
    lab_dom_set(state, "panSnapshot", snapshot);
    lab_dom_set(state, "panPlane", lab_dom_scalar(1, lab_plot_is_plane(chart)));
    lab_dom_set(state, "panPlaneSnapshot", lab_dom_get(lab_dom_id("plotRendered"), "marsComplexPlaneBounds"));
    lab_dom_set(state, "panProjection", lab_dom_read(lab_dom_closest(chart, ".complex-panel"), 3, "complexPanel"));
    lab_dom_set(state, "panCard", lab_dom_closest(chart, ".result-card"));
    return 1;
}

/* Translate both axes without changing their spans; the existing gesture queue resamples the result. */
int lab_plot_pan_move(int state, double dx, double dy)
{
    int geometry = lab_dom_get(state, "panGeometry");
    double values[4], offsets[] = {-dx * lab_dom_to_number(lab_dom_item(geometry, 4)),
                                  dy * lab_dom_to_number(lab_dom_item(geometry, 5))};
    for (unsigned i = 0; i < 4; ++i) {
        values[i] = lab_dom_to_number(lab_dom_item(geometry, i)) + offsets[i / 2];
        if (!finite(values[i]))
            return 0;
    }
    if (values[0] >= values[1] || values[2] >= values[3])
        return 0;
    if (lab_dom_truth(lab_dom_get(state, "panPlane"))) {
        lab_plot_plane_bounds(values);
        return 1;
    }
    int range = lab_dom_id("plotRange"), card = lab_dom_get(state, "panCard");
    int title = lab_dom_join(label_text(lab_dom_query(card, "[data-plot-figure] > title")),
                            lab_dom_string("curve"), "");
    if (!lab_dom_equal(title, lab_dom_get(range, "marsZoomTitle"))) {
        lab_dom_set(range, "marsZoomTitle", title);
        lab_dom_delete(range, "marsZoomOriginal");
    }
    if (!lab_dom_get(range, "marsZoomOriginal")) {
        int original = lab_dom_object(4), snapshot = lab_dom_get(state, "panSnapshot");
        for (unsigned i = 0; i < 5; ++i)
            lab_dom_push(original, lab_dom_item(snapshot, i * 3));
        lab_dom_set(range, "marsZoomOriginal", original);
    }
    static const char *const ids[] = {"plotLower", "plotUpper", "plotYLower", "plotYUpper"};
    for (unsigned i = 0; i < 4; ++i)
        lab_plot_write_bound(lab_dom_id(ids[i]), values[i], values[(i / 2) * 2 + 1] - values[(i / 2) * 2]);
    lab_dom_delete(range, "marsZoomChart");
    lab_dom_write(lab_dom_id("plotScale"), 5, "", lab_dom_string("manual"));
    if (!lab_dom_truth(lab_dom_get(state, "changed")))
        lab_plot_controls_sync();
    return 1;
}

/* This is a temporary geometric translation, never a substitute for fresh native samples. */
void lab_plot_pan_preview(int state)
{
    int chart = lab_dom_get(state, "surface");
    int geometry = lab_dom_get(state, "panGeometry");
    int active = lab_dom_truth(lab_dom_get(state, "active"));
    double sx = active ? lab_dom_to_number(lab_dom_item(geometry, 6)) : lab_dom_measure(chart, 13);
    double sy = active ? lab_dom_to_number(lab_dom_item(geometry, 7)) : lab_dom_measure(chart, 16);
    if (!(sx > 0) || !(sy > 0))
        return;
    double dx = (number(state, "panX") - number(state, "panDrawnX")) / sx;
    double dy = (number(state, "panY") - number(state, "panDrawnY")) / sy;
    int transform = lab_dom_join(lab_dom_format(dx, "translate(", " "), lab_dom_format(dy, "", ")"), "");
    int curves = lab_dom_all(chart, "[data-series]");
    for (unsigned i = 0; i < lab_dom_count(curves); ++i)
        lab_dom_write(lab_dom_item(curves, i), 2, "transform", transform);
    /* Native groups bind each tick to its label and grid line; no tick text is reinterpreted. */
    int ticks = lab_dom_all(chart, "[data-plot-pan-axis]");
    double lower[] = {data(chart, "plotLeft"), data(chart, "plotTop")};
    double upper[] = {lower[0] + data(chart, "plotWidth"), lower[1] + data(chart, "plotHeight")};
    int shifts[] = {lab_dom_format(dx, "translate(", " 0)"), lab_dom_format(dy, "translate(0 ", ")")};
    for (unsigned i = 0; i < lab_dom_count(ticks); ++i) {
        unsigned mark = lab_dom_mark();
        int tick = lab_dom_item(ticks, i);
        unsigned axis = data(tick, "plotPanAxis") == 1;
        double position = data(tick, "plotPanPosition") + (axis ? dy : dx);
        lab_dom_write(tick, 2, "transform", shifts[axis]);
        lab_dom_write(tick, 2, "visibility", lab_dom_string(
            position < lower[axis] - 0.001 || position > upper[axis] + 0.001 ? "hidden" : "visible"));
        lab_dom_release(mark);
    }
}

/* Cancellation restores authored strings as well as the precise coordinates behind rounded labels. */
void lab_plot_pan_cancel(int state)
{
    if (lab_dom_truth(lab_dom_get(state, "panPlane"))) {
        lab_dom_set(lab_dom_id("plotRendered"), "marsComplexPlaneBounds", lab_dom_get(state, "panPlaneSnapshot"));
        return;
    }
    static const char *const ids[] = {"plotLower", "plotUpper", "plotYLower", "plotYUpper", "plotScale"};
    int snapshot = lab_dom_get(state, "panSnapshot");
    for (unsigned i = 0; i < 5; ++i) {
        int control = lab_dom_id(ids[i]);
        lab_dom_write(control, 5, "", lab_dom_item(snapshot, i * 3));
        lab_dom_set(control, "marsZoomText", lab_dom_item(snapshot, i * 3 + 1));
        lab_dom_set(control, "marsZoomValue", lab_dom_item(snapshot, i * 3 + 2));
    }
    lab_dom_delete(lab_dom_id("plotRange"), "marsZoomChart");
    lab_plot_controls_sync();
}

static void redraw_curve(int chart, int calls, int wheel)
{
    if (wheel) {
        lab_plot_wheel_zoom(chart, calls);
        return;
    }
    int call = lab_dom_object(5), args = lab_dom_object(4);
    lab_dom_set(call, "service", lab_dom_string("takePlot"));
    lab_dom_push(args, lab_dom_read(lab_dom_id("plotFirst"), 5, ""));
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
}

/* Keep surface-camera magnification separate from native curve-domain resampling. */
int lab_plot_pointer_zoom(int card, int event, int calls, int reset, double direction, int wheel)
{
    int panel = lab_dom_closest(lab_dom_get(event, "target"), ".complex-panel");
    if (wheel && !panel && lab_dom_query(card, ".complex-grid"))
        return 1;
    if (!panel)
        panel = lab_dom_query(card, ".complex-expanded");
    int chart = lab_dom_query(panel ? panel : card, "[data-plot-chart][data-x-min], [data-surface][data-x-min]");
    if (!chart)
        return 0;
    if (wheel && panel && !lab_dom_node_contains(panel, chart))
        return 0;
    if (!wheel)
        lab_plot_wheel_stop();
    double box[4], bounds[4];
    int surface = lab_dom_has(chart, "data-surface");
    if (surface) {
        int call = lab_dom_object(5), args = lab_dom_object(4);
        lab_dom_push(args, chart);
        lab_dom_push(args, event);
        lab_dom_push(args, lab_dom_numeric(reset));
        lab_dom_push(args, lab_dom_numeric(direction));
        lab_dom_push(args, lab_dom_numeric(wheel));
        lab_dom_set(call, "service", lab_dom_string("zoomSurface"));
        lab_dom_set(call, "args", args);
        lab_dom_push(calls, call);
        return 1;
    }
    unsigned coordinates = lab_plot_auto_height(chart) ? 2 : 4;
    if (!frame(chart, box, bounds))
        return 1;
    if (lab_plot_is_plane(chart)) {
        int host = lab_dom_id("plotRendered"), pending = lab_dom_get(host, "marsComplexPlaneBounds");
        if (reset)
            lab_dom_delete(host, "marsComplexPlaneBounds");
        else {
            double fractions[] = {wheel ? (number(event, "clientX") - box[0]) / box[2] : 0.5,
                                  wheel ? 1 - (number(event, "clientY") - box[1]) / box[3] : 0.5};
            if (fractions[0] < 0 || fractions[0] > 1 || fractions[1] < 0 || fractions[1] > 1)
                return 1;
            if (lab_dom_count(pending) == 4)
                for (unsigned i = 0; i < 4; ++i)
                    bounds[i] = lab_dom_to_number(lab_dom_item(pending, i));
            double factor = direction > 0 ? 0.8 : 1.25;
            for (unsigned axis = 0; axis < 2; ++axis) {
                double anchor = bounds[2 * axis] * (1 - fractions[axis]) + bounds[2 * axis + 1] * fractions[axis];
                bounds[2 * axis] = anchor + (bounds[2 * axis] - anchor) * factor;
                bounds[2 * axis + 1] = anchor + (bounds[2 * axis + 1] - anchor) * factor;
                if (!finite(bounds[2 * axis]) || !finite(bounds[2 * axis + 1]) ||
                    bounds[2 * axis] >= bounds[2 * axis + 1])
                    return 1;
            }
            lab_plot_plane_bounds(bounds);
        }
        redraw_curve(chart, calls, wheel);
        return 1;
    }
    int range = lab_dom_id("plotRange");
    const char *ids[] = {"plotLower", "plotUpper", "plotYLower", "plotYUpper", "plotScale"};
    int original = lab_dom_get(range, "marsZoomOriginal");
    int title = lab_dom_join(label_text(lab_dom_query(card, "[data-plot-figure] > title")),
                            lab_dom_string("curve"), "");
    if (!lab_dom_equal(title, lab_dom_get(range, "marsZoomTitle"))) {
        original = 0;
        lab_dom_delete(range, "marsZoomOriginal");
        lab_dom_delete(range, "marsZoomChart");
        lab_dom_set(range, "marsZoomTitle", title);
    }
    if (reset) {
        if (!original)
            return 1;
        for (unsigned i = 0; i < 5; ++i) {
            lab_dom_delete(lab_dom_id(ids[i]), "marsZoomText");
            lab_dom_delete(lab_dom_id(ids[i]), "marsZoomValue");
            lab_dom_write(lab_dom_id(ids[i]), 5, "", lab_dom_item(original, i));
        }
        lab_dom_delete(range, "marsZoomOriginal");
        lab_dom_delete(range, "marsZoomChart");
    } else {
        double fx = wheel ? (number(event, "clientX") - box[0]) / box[2] : 0.5;
        double fy = wheel ? 1 - (number(event, "clientY") - box[1]) / box[3] : 0.5;
        if (fx < 0 || fx > 1 || fy < 0 || fy > 1)
            return 1;
        double fractions[] = {fx, fy}, anchors[2];
        for (unsigned a = 0; a < coordinates / 2; ++a)
            anchors[a] = bounds[a * 2] * (1 - fractions[a]) + bounds[a * 2 + 1] * fractions[a];
        int previous = lab_dom_get(range, "marsZoomChart");
        /* A replacement image must not roll back bounds already requested by newer wheel events. */
        if (previous && lab_dom_count(lab_dom_get(range, "marsZoomBounds")) == coordinates) {
            int pending = lab_dom_get(range, "marsZoomBounds");
            for (unsigned i = 0; i < coordinates; ++i)
                bounds[i] = lab_dom_to_number(lab_dom_item(pending, i));
        }
        int anchor_state = lab_dom_get(range, "marsZoomAnchor");
        double px = number(event, "clientX"), py = number(event, "clientY");
        /* Repainting an overlapping sheet is not a pointer movement. Keep the world
         * anchor so reversing the wheel reverses the zoom instead of migrating sheets. */
        if (wheel && previous && lab_dom_count(anchor_state) == coordinates / 2 + 2 &&
            __builtin_fabs(px - lab_dom_to_number(lab_dom_item(anchor_state, 0))) <= 2 &&
            __builtin_fabs(py - lab_dom_to_number(lab_dom_item(anchor_state, 1))) <= 2) {
            for (unsigned a = 0; a < coordinates / 2; ++a)
                anchors[a] = lab_dom_to_number(lab_dom_item(anchor_state, a + 2));
        } else if (!wheel) {
            for (unsigned a = 0; a < coordinates / 2; ++a)
                anchors[a] = bounds[a * 2] * 0.5 + bounds[a * 2 + 1] * 0.5;
        }
        double values[6], factor = direction > 0 ? 0.8 : 1.25;
        for (unsigned a = 0; a < coordinates / 2; ++a) {
            double lo = bounds[a * 2], hi = bounds[a * 2 + 1];
            double anchor = anchors[a];
            values[a * 2] = anchor + (lo - anchor) * factor;
            values[a * 2 + 1] = anchor + (hi - anchor) * factor;
            if (!finite(values[a * 2]) || !finite(values[a * 2 + 1]) || values[a * 2] >= values[a * 2 + 1])
                return 1;
        }
        if (!original) {
            original = lab_dom_object(4);
            for (unsigned i = 0; i < 5; ++i)
                lab_dom_push(original, lab_dom_read(lab_dom_id(ids[i]), 5, ""));
            lab_dom_set(range, "marsZoomOriginal", original);
        }
        for (unsigned i = 0; i < coordinates; ++i)
            lab_plot_write_bound(lab_dom_id(ids[i]), values[i], values[(i / 2) * 2 + 1] - values[(i / 2) * 2]);
        int pending = lab_dom_object(4);
        for (unsigned i = 0; i < coordinates; ++i)
            lab_dom_push(pending, lab_dom_numeric(values[i]));
        lab_dom_set(range, "marsZoomChart", chart);
        lab_dom_set(range, "marsZoomBounds", pending);
        lab_dom_delete(range, "marsZoomAnchor");
        if (wheel) {
            anchor_state = lab_dom_object(4);
            lab_dom_push(anchor_state, lab_dom_numeric(px));
            lab_dom_push(anchor_state, lab_dom_numeric(py));
            for (unsigned a = 0; a < coordinates / 2; ++a)
                lab_dom_push(anchor_state, lab_dom_numeric(anchors[a]));
            lab_dom_set(range, "marsZoomAnchor", anchor_state);
        }
        if (coordinates == 4)
            lab_dom_write(lab_dom_id("plotScale"), 5, "", lab_dom_string("manual"));
    }
    lab_plot_controls_sync();
    lab_dom_class(lab_dom_query(card, ".plot-pointer-line"), "hidden", 1);
    lab_dom_class(lab_dom_query(card, ".plot-pointer-horizontal"), "hidden", 1);
    lab_dom_class(lab_dom_query(card, ".plot-pointer-marker"), "hidden", 1);
    lab_dom_class(lab_dom_query(card, ".plot-pointer-tip"), "hidden", 1);
    redraw_curve(chart, calls, wheel);
    return 1;
}
