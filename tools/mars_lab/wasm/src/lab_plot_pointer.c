/**
 * @file lab_plot_pointer.c
 * @brief Curve guides and pointer-aligned curve/surface range zoom without magnifying SVG artwork.
 *
 * Uses native viewport metadata and browser geometry to inspect the horizontal
 * coordinate. Zoom edits range controls around the pointer and requests native
 * resampling; toolbar buttons use the centre. Reset restores the pre-zoom controls.
 * Double coordinate metadata is a UI approximation; non-finite or collapsed ranges
 * are left to manual native bounds. No expression is interpreted in the browser.
 */
#include "lab_dom.h"
#include "lab_workspace.h"
#include "lab_plot_controls.h"
#include "lab_plot_pointer.h"

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
static void zoom_bound(int control, double value, double span)
{
    int text = lab_dom_precision(value, lab_plot_bound_digits(value, span));
    lab_dom_set(control, "marsZoomText", text);
    lab_dom_set(control, "marsZoomValue", lab_dom_format(value, "", ""));
    lab_dom_write(control, 5, "", text);
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

/* The guide is transient UI and is not inserted into the copyable SVG. */
void lab_plot_pointer_dispatch(unsigned action, int event, int context)
{
    (void)context;
    lab_dom_return(lab_dom_object(5));
    int rendered = lab_dom_id("plotRendered");
    int chart = lab_dom_query(rendered, "[data-plot-chart][data-x-min]:not([data-surface])");
    int line = lab_dom_query(rendered, ".plot-pointer-line"), tip = lab_dom_query(rendered, ".plot-pointer-tip");
    double box[4], bounds[4], x = number(event, "clientX"), y = number(event, "clientY");
    if (action || !chart || !frame(chart, box, bounds) || x < box[0] || x > box[0] + box[2] ||
        y < box[1] || y > box[1] + box[3]) {
        lab_dom_class(line, "hidden", 1);
        lab_dom_class(tip, "hidden", 1);
        focus_curve(rendered, 0, 0);
        return;
    }
    focus_curve(rendered, chart, lab_dom_get(event, "target"));
    line = overlay(rendered, ".plot-pointer-line", "plot-pointer-line");
    tip = overlay(rendered, ".plot-pointer-tip", "plot-pointer-tip");
    double left = lab_dom_measure(rendered, 7) - number(rendered, "scrollLeft");
    double top = lab_dom_measure(rendered, 8) - number(rendered, "scrollTop");
    pixels(line, "left", x - left);
    pixels(line, "top", box[1] - top);
    pixels(line, "height", box[3]);
    double fraction = (x - box[0]) / box[2];
    double value = bounds[0] * (1 - fraction) + bounds[1] * fraction;
    int name = lab_dom_read(lab_dom_id("plotFirst"), 5, "");
    int text = lab_dom_join(name, lab_dom_format(value, " ≈ ", ""), "");
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
    for (unsigned i = 0; i < 5; ++i) {
        int control = lab_dom_id(ids[i]);
        lab_dom_push(snapshot, lab_dom_read(control, 5, ""));
        lab_dom_push(snapshot, lab_dom_get(control, "marsZoomText"));
        lab_dom_push(snapshot, lab_dom_get(control, "marsZoomValue"));
    }
    lab_dom_set(state, "panGeometry", geometry);
    lab_dom_set(state, "panSnapshot", snapshot);
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
        zoom_bound(lab_dom_id(ids[i]), values[i], values[(i / 2) * 2 + 1] - values[(i / 2) * 2]);
    lab_dom_delete(range, "marsZoomChart");
    lab_dom_write(lab_dom_id("plotScale"), 5, "", lab_dom_string("manual"));
    lab_plot_controls_sync();
    return 1;
}

/* This is a temporary geometric translation, never a substitute for fresh native samples. */
void lab_plot_pan_preview(int state)
{
    int chart = lab_dom_get(state, "surface");
    double sx = lab_dom_measure(chart, 13), sy = lab_dom_measure(chart, 16);
    if (!(sx > 0) || !(sy > 0))
        return;
    double dx = (number(state, "panX") - number(state, "panDrawnX")) / sx;
    double dy = (number(state, "panY") - number(state, "panDrawnY")) / sy;
    int transform = lab_dom_join(lab_dom_format(dx, "translate(", " "), lab_dom_format(dy, "", ")"), "");
    int curves = lab_dom_all(chart, "[data-series]");
    for (unsigned i = 0; i < lab_dom_count(curves); ++i)
        lab_dom_write(lab_dom_item(curves, i), 2, "transform", transform);
}

/* Cancellation restores authored strings as well as the precise coordinates behind rounded labels. */
void lab_plot_pan_cancel(int state)
{
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

/* UI range arithmetic changes only the requested viewport; the server resamples the original expression. */
int lab_plot_pointer_zoom(int card, int event, int calls, int reset, double direction, int wheel)
{
    int chart = lab_dom_query(card, "[data-plot-chart][data-x-min], [data-surface][data-x-min]");
    if (!chart)
        return 0;
    double box[4], bounds[4];
    int surface = lab_dom_has(chart, "data-surface");
    if (surface) {
        static const char *const keys[] = {"xMin", "xMax", "yMin", "yMax"};
        for (unsigned i = 0; i < 4; ++i) {
            bounds[i] = data(chart, keys[i]);
            if (!finite(bounds[i]))
                return 1;
        }
        box[0] = lab_dom_measure(chart, 17);
        box[1] = lab_dom_measure(chart, 18);
        box[2] = lab_dom_measure(chart, 13) * lab_dom_measure(chart, 5);
        box[3] = lab_dom_measure(chart, 16) * lab_dom_measure(chart, 6);
        if (!finite(box[0]) || !finite(box[1]) || !(box[2] > 0) || !(box[3] > 0))
            return 1;
    } else if (!frame(chart, box, bounds))
        return 1;
    int range = lab_dom_id("plotRange");
    const char *ids[] = {"plotLower", "plotUpper", surface ? "plotSecondLower" : "plotYLower",
                        surface ? "plotSecondUpper" : "plotYUpper", "plotScale"};
    int original = lab_dom_get(range, "marsZoomOriginal");
    int title = lab_dom_join(label_text(lab_dom_query(card, "[data-plot-figure] > title")),
                            lab_dom_string(surface ? "surface" : "curve"), "");
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
        if (surface && wheel) {
            int facet = lab_dom_closest(lab_dom_get(event, "target"), "[data-uv]");
            fx = fy = 0.5;
            if (facet) {
                int map = lab_dom_split(lab_dom_read(facet, 3, "uv"), " ");
                double px = (number(event, "clientX") - box[0]) * lab_dom_measure(chart, 5) / box[2];
                double py = (number(event, "clientY") - box[1]) * lab_dom_measure(chart, 6) / box[3];
                fx = lab_dom_to_number(lab_dom_item(map, 0)) * px + lab_dom_to_number(lab_dom_item(map, 1)) * py +
                     lab_dom_to_number(lab_dom_item(map, 2));
                fy = lab_dom_to_number(lab_dom_item(map, 3)) * px + lab_dom_to_number(lab_dom_item(map, 4)) * py +
                     lab_dom_to_number(lab_dom_item(map, 5));
                if (!finite(fx) || !finite(fy))
                    return 1;
                fx = fx < 0 ? 0 : fx > 1 ? 1 : fx;
                fy = fy < 0 ? 0 : fy > 1 ? 1 : fy;
            }
        }
        /* Repeated wheel events accumulate against pending bounds until the replacement SVG arrives. */
        if (lab_dom_equal(chart, lab_dom_get(range, "marsZoomChart"))) {
            int pending = lab_dom_get(range, "marsZoomBounds");
            for (unsigned i = 0; i < 4; ++i)
                bounds[i] = lab_dom_to_number(lab_dom_item(pending, i));
        }
        double fractions[] = {fx, fy}, values[4], factor = direction > 0 ? 0.8 : 1.25;
        for (unsigned a = 0; a < 2; ++a) {
            double lo = bounds[a * 2], hi = bounds[a * 2 + 1];
            double anchor = lo * (1 - fractions[a]) + hi * fractions[a];
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
        for (unsigned i = 0; i < 4; ++i)
            zoom_bound(lab_dom_id(ids[i]), values[i], values[(i / 2) * 2 + 1] - values[(i / 2) * 2]);
        int pending = lab_dom_object(4);
        for (unsigned i = 0; i < 4; ++i)
            lab_dom_push(pending, lab_dom_numeric(values[i]));
        lab_dom_set(range, "marsZoomChart", chart);
        lab_dom_set(range, "marsZoomBounds", pending);
        if (!surface)
            lab_dom_write(lab_dom_id("plotScale"), 5, "", lab_dom_string("manual"));
    }
    lab_plot_controls_sync();
    lab_dom_class(lab_dom_query(card, ".plot-pointer-line"), "hidden", 1);
    lab_dom_class(lab_dom_query(card, ".plot-pointer-tip"), "hidden", 1);
    int call = lab_dom_object(5), args = lab_dom_object(4);
    lab_dom_set(call, "service", lab_dom_string("takePlot"));
    lab_dom_push(args, lab_dom_read(lab_dom_id("plotFirst"), 5, ""));
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
    return 1;
}
