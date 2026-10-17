/**
 * @file lab_surface_volume.c
 * @brief C-owned coordinate-volume zoom without an absolute double-precision boundary.
 *
 * Each range is an opaque exact native baseline plus a binary scale exponent.
 * The worker evaluates centre +/- half-span * 2^-exponent with number_t and
 * adaptive precision. Recipes never accumulate or nest across gestures. The
 * browser's numbers are only approximate labels and relative GPU transforms.
 * Auto height grids zoom horizontally and accept a fresh native height range;
 * implicit meshes and other height policies retain three-axis volume clipping.
 * Space curves refine only their input interval, retaining output-axis controls
 * and fitting each accepted sample to its fresh native coordinate ranges.
 */
#include "lab_dom.h"
#include "lab_plot_controls.h"
#include "lab_plot_pointer.h"
#include "lab_surface_math.h"
#include "lab_surface_volume.h"

static const char *const keys[] = {"xMin", "xMax", "yMin", "yMax", "zMin", "zMax"};
static const char *const exact_keys[] = {"exactXMin", "exactXMax", "exactYMin", "exactYMax", "exactZMin", "exactZMax"};
static const char *const label_keys[] = {"labelXMin", "labelXMax", "labelYMin", "labelYMax", "labelZMin", "labelZMax"};
static const char *const controls[] = {
    "plotLower", "plotUpper", "plotSecondLower", "plotSecondUpper", "plotYLower", "plotYUpper", "plotScale"
};

static double numeric(int object, const char *key)
{
    return lab_dom_to_number(lab_dom_get(object, key));
}

static int read_bounds(int chart, const char *const names[6])
{
    int result = lab_dom_object(4);
    for (unsigned i = 0; i < 6; ++i) {
        int value = lab_dom_read(chart, 3, names[i]);
        lab_dom_push(result, names == keys ? lab_dom_numeric(lab_dom_to_number(value)) : value);
    }
    return result;
}

static int control_values(void)
{
    int result = lab_dom_object(4);
    for (unsigned i = 0; i < 7; ++i) lab_dom_push(result, lab_dom_read(lab_dom_id(controls[i]), 5, ""));
    return result;
}

static int authored_bound(unsigned i)
{
    int control = lab_dom_id(controls[i]), text = lab_dom_read(control, 5, "");
    int exact = lab_dom_get(control, "marsZoomValue");
    return exact && lab_dom_equal(text, lab_dom_get(control, "marsZoomText")) ? exact : text;
}

static int initial_exact(int chart)
{
    /* A retained curve can coexist with sibling plots that edit the shared controls. */
    if (lab_dom_equal(lab_dom_read(chart, 3, "segments"), lab_dom_string("true")))
        return read_bounds(chart, exact_keys);
    int result = lab_dom_object(4);
    int manual = lab_dom_equal(lab_dom_read(lab_dom_id("plotScale"), 5, ""), lab_dom_string("manual"));
    for (unsigned axis = 0; axis < 3; ++axis) {
        int low = authored_bound(axis*2), high = authored_bound(axis*2+1);
        int supplied = (axis < 2 || manual) && lab_dom_length(lab_dom_clean(low, 0)) &&
                       lab_dom_length(lab_dom_clean(high, 0));
        lab_dom_push(result, supplied ? low : lab_dom_read(chart, 3, exact_keys[axis*2]));
        lab_dom_push(result, supplied ? high : lab_dom_read(chart, 3, exact_keys[axis*2+1]));
    }
    return result;
}

/* Preserve exact native metadata, not the lossy compatibility coordinates. */
void lab_surface_volume_init(int view, int saved, unsigned same)
{
    int chart = lab_dom_get(view, "chart"), volume = lab_dom_get(saved, "volume");
    int segments = lab_dom_equal(lab_dom_read(chart, 3, "segments"), lab_dom_string("true"));
    int refining = lab_dom_truth(lab_dom_get(saved, "refining"));
    int manual = lab_dom_equal(lab_dom_read(lab_dom_id("plotScale"), 5, ""), lab_dom_string("manual"));
    int automatic = lab_dom_equal(lab_dom_read(lab_dom_id("plotScale"), 5, ""), lab_dom_string("auto"));
    /* Auto/Fit all must discard a clipped preview even if the native mesh is unchanged.
       Genuine zoom refinements retain their exact selected volume. */
    int was_auto = lab_dom_truth(lab_dom_get(volume, "autoHeight"));
    int restore_height = !segments && volume && !refining && lab_dom_truth(lab_dom_get(volume, "started")) &&
                         ((!manual && !was_auto) || (was_auto && !automatic));
    if (!volume || (!same && !refining) || restore_height) {
        volume = lab_dom_object(5);
        lab_dom_set(volume, "base", read_bounds(chart, keys));
        lab_dom_set(volume, "bounds", read_bounds(chart, keys));
        lab_dom_set(volume, "exact", restore_height ? read_bounds(chart, exact_keys) : initial_exact(chart));
        lab_dom_set(volume, "units", lab_dom_get(view, "units"));
        lab_dom_set(volume, "labelWidth", lab_dom_read(chart, 3, "labelWidth"));
        lab_dom_set(volume, "original", segments ? read_bounds(chart, exact_keys) : control_values());
        lab_dom_set(volume, "exponent", lab_dom_numeric(0));
        lab_dom_set(volume, "sampleExponent", lab_dom_numeric(0));
        lab_dom_set(volume, "autoHeight", lab_dom_scalar(1,
            lab_dom_equal(lab_dom_read(chart, 3, "mesh"), lab_dom_string("false")) &&
            automatic && !segments));
        if (restore_height)
            lab_dom_set(lab_dom_get(view, "camera"), "scale", lab_dom_numeric(1));
    }
    lab_dom_set(volume, "sampled", read_bounds(chart, keys));
    if (segments)
        lab_dom_set(lab_dom_get(view, "camera"), "scale", lab_dom_numeric(1));
    if (refining) {
        if (segments) {
            /* Output ranges belong to the newly sampled curve, never to x zoom recipes. */
            lab_dom_set(volume, "bounds", read_bounds(chart, keys));
            lab_dom_set(volume, "units", lab_dom_get(view, "units"));
            lab_dom_set(volume, "labelWidth", lab_dom_read(chart, 3, "labelWidth"));
            /* Even a reset keeps the original curve baseline: finite segment
             * extents can differ slightly from the requested input interval. */
        } else if (lab_dom_truth(lab_dom_get(volume, "autoHeight"))) {
            int previous = lab_dom_get(volume, "bounds"), bounds = lab_dom_object(4);
            int retain_input = lab_dom_truth(lab_dom_get(volume, "started"));
            for (unsigned i = 0; i < 6; ++i)
                lab_dom_push(bounds, i < 4 && retain_input ? lab_dom_item(previous, i) :
                             lab_dom_numeric(lab_dom_to_number(lab_dom_read(chart, 3, keys[i]))));
            lab_dom_set(volume, "bounds", bounds);
            /* Reset may restore Auto input bounds different from the old gesture baseline. */
            if (!retain_input) {
                lab_dom_set(volume, "base", read_bounds(chart, keys));
                lab_dom_set(volume, "exact", read_bounds(chart, exact_keys));
            }
            lab_dom_set(volume, "units", lab_dom_get(view, "units"));
            lab_dom_set(volume, "labelWidth", lab_dom_read(chart, 3, "labelWidth"));
        }
        lab_dom_set(volume, "dirty", lab_dom_scalar(1, 0));
        lab_dom_set(volume, "sampleExponent", lab_dom_get(volume, "exponent"));
        for (unsigned i = 0; i < (segments ? 2u : 6u) && lab_dom_truth(lab_dom_get(volume, "started")); ++i) {
            int control = lab_dom_id(controls[i]), text = lab_dom_read(chart, 3, label_keys[i]);
            lab_dom_write(control, 5, "", text);
            lab_dom_set(control, "marsZoomText", text);
            lab_dom_set(control, "marsZoomValue", lab_dom_read(chart, 3, exact_keys[i]));
        }
    }
    lab_dom_set(view, "volume", volume);
    lab_dom_set(view, "units", lab_dom_get(volume, "units"));
}

static int append(int text, int addition, const char *suffix)
{
    return lab_dom_join(text, addition, suffix);
}

/* A constant-sized recipe preserves the centre even when its span is microscopic. */
static int bound_recipe(int low, int high, double exponent, int upper)
{
    int text = append(lab_dom_string("(("), low, ")+(");
    text = append(text, high, upper ? "))/2+((" : "))/2-((");
    text = append(text, high, ")-(");
    text = append(text, low, "))/2*2^(-(");
    return append(text, lab_dom_format(exponent, "", ""), "))");
}

/* Volume metadata is a small flat record; only its coordinate arrays need independent storage. */
static int volume_copy(int source)
{
    int copy = lab_dom_object(5), names = lab_dom_keys(source);
    for (unsigned i = 0; i < lab_dom_count(names); ++i) {
        int key = lab_dom_item(names, i), value = lab_dom_key_get(source, key);
        if (lab_dom_type(value) == 4) {
            int array = lab_dom_object(4);
            for (unsigned j = 0; j < lab_dom_count(value); ++j)
                lab_dom_push(array, lab_dom_item(value, j));
            value = array;
        }
        lab_dom_key_set(copy, key, value);
    }
    return copy;
}

/* Keep DOM controls separate from the mathematical baseline, including absent exact-value metadata. */
static int pinch_snapshot(int volume, int camera)
{
    int snapshot = lab_dom_object(5), values = lab_dom_object(4);
    lab_dom_set(snapshot, "volume", volume_copy(volume));
    lab_dom_property_copy(snapshot, "scale", camera, "scale");
    lab_dom_set(snapshot, "inputRange", lab_dom_read(lab_dom_id("plotInputRange"), 5, ""));
    for (unsigned i = 0; i < 7; ++i) {
        int control = lab_dom_id(controls[i]), saved = lab_dom_object(5);
        lab_dom_set(saved, "text", lab_dom_read(control, 5, ""));
        lab_dom_property_copy(saved, "marsZoomText", control, "marsZoomText");
        lab_dom_property_copy(saved, "marsZoomValue", control, "marsZoomValue");
        lab_dom_push(values, saved);
    }
    lab_dom_set(snapshot, "controls", values);
    return snapshot;
}

/* Restore the full pre-gesture transaction without re-authoring bounds or requesting new samples. */
static void pinch_restore(int volume, int camera, int snapshot)
{
    int names = lab_dom_keys(volume);
    for (unsigned i = 0; i < lab_dom_count(names); ++i)
        lab_dom_key_delete(volume, lab_dom_item(names, i));
    int saved_volume = volume_copy(lab_dom_get(snapshot, "volume"));
    names = lab_dom_keys(saved_volume);
    for (unsigned i = 0; i < lab_dom_count(names); ++i) {
        int key = lab_dom_item(names, i);
        lab_dom_key_set(volume, key, lab_dom_key_get(saved_volume, key));
    }
    lab_dom_property_copy(camera, "scale", snapshot, "scale");
    int values = lab_dom_get(snapshot, "controls");
    for (unsigned i = 0; i < 7; ++i) {
        int control = lab_dom_id(controls[i]), saved = lab_dom_item(values, i);
        lab_dom_write(control, 5, "", lab_dom_get(saved, "text"));
        lab_dom_property_copy(control, "marsZoomText", saved, "marsZoomText");
        lab_dom_property_copy(control, "marsZoomValue", saved, "marsZoomValue");
    }
    lab_dom_write(lab_dom_id("plotInputRange"), 5, "", lab_dom_get(snapshot, "inputRange"));
    lab_plot_controls_sync();
}

/* Convert bounded gestures to a binary exponent, never to absolute coordinate differences. */
int lab_surface_zoom(int view, unsigned kind, double value, int event, unsigned reset)
{
    int volume = lab_dom_get(view, "volume"), camera = lab_dom_get(view, "camera");
    double exponent = numeric(volume, "exponent");
    if (kind == 0) exponent += lab_surface_log2(value);
    else if (kind == 1) exponent = value;
    else if (kind == 2) {
        double pixels = numeric(event, "deltaY"), mode = numeric(event, "deltaMode");
        pixels *= mode == 1 ? 16 : mode == 2 ? 400 : 1;
        if (pixels > 200) pixels = 200;
        if (pixels < -200) pixels = -200;
        exponent -= pixels * 0.002 / 0.6931471805599453094;
    } else if (kind == 3) {
        int snapshot = lab_dom_get(event, "surfaceZoomSnapshot");
        if (lab_dom_truth(lab_dom_get(event, "pinchCancelled"))) {
            if (snapshot)
                pinch_restore(volume, camera, snapshot);
            return 1;
        }
        double distance = numeric(event, "pinchDistance");
        if (!(distance > 0) || !__builtin_isfinite(distance))
            return 0;
        double dx = numeric(event, "touchX") - numeric(event, "secondX");
        double dy = numeric(event, "touchY") - numeric(event, "secondY");
        double current = __builtin_sqrt(dx*dx + dy*dy);
        if (!(current > 0) || !__builtin_isfinite(current))
            return 0;
        double ratio = current / distance;
        if (!(ratio > 0) || !__builtin_isfinite(ratio))
            return 0;
        if (!snapshot)
            lab_dom_set(event, "surfaceZoomSnapshot", pinch_snapshot(volume, camera));
        int start = lab_dom_get(event, "surfaceExponent");
        if (!start) { start = lab_dom_numeric(exponent); lab_dom_set(event, "surfaceExponent", start); }
        exponent = lab_dom_to_number(start) + lab_surface_log2(ratio);
    }
    if (reset) exponent = 0;
    int segments = lab_dom_equal(lab_dom_read(lab_dom_get(view, "chart"), 3, "segments"), lab_dom_string("true"));
    /* Native plotting has a 65,536-bit resource ceiling, not an f64 coordinate limit. */
    if (!__builtin_isfinite(exponent) || exponent > 32000 || exponent < -1024) {
        int feedback = lab_dom_id("plotFeedback");
        lab_dom_write(feedback, 0, "", lab_dom_string("Zoom exceeds the native plotting resource limit."));
        lab_dom_class(feedback, "hidden", 0);
        return 0;
    }
    if (!segments && !lab_dom_truth(lab_dom_get(volume, "started")) && !reset)
        lab_dom_set(volume, "original", control_values());
    int exact = lab_dom_get(volume, "exact"), base = lab_dom_get(volume, "base"), bounds = lab_dom_object(4);
    double scale = lab_surface_exp2(exponent);
    int auto_height = lab_dom_truth(lab_dom_get(volume, "autoHeight"));
    for (unsigned i = 0; i < 6; ++i) {
        if (segments && i >= 2) {
            lab_dom_push(bounds, lab_dom_item(lab_dom_get(volume, "sampled"), i));
            continue;
        }
        if (auto_height && i >= 4) {
            lab_dom_push(bounds, lab_dom_item(lab_dom_get(volume, "sampled"), i));
            if (reset) {
                int control = lab_dom_id(controls[i]);
                lab_dom_write(control, 5, "", lab_dom_item(lab_dom_get(volume, "original"), i));
                lab_dom_delete(control, "marsZoomText"); lab_dom_delete(control, "marsZoomValue");
            }
            continue;
        }
        unsigned lower = (i / 2) * 2;
        double lo = lab_dom_to_number(lab_dom_item(base, lower)), hi = lab_dom_to_number(lab_dom_item(base, lower + 1));
        double half = (hi - lo) / (2 * scale), centre = lo/2 + hi/2;
        double approximate = centre + (i % 2 ? half : -half);
        lab_dom_push(bounds, lab_dom_numeric(approximate));
        int control = lab_dom_id(controls[i]);
        if (reset) {
            int original = lab_dom_item(segments ? exact : lab_dom_get(volume, "original"), i);
            lab_dom_write(control, 5, "", original);
            if (segments && lab_dom_length(lab_dom_clean(original, 0))) {
                /* Display the curve's own exact baseline, never a sibling's range. */
                lab_dom_set(control, "marsZoomText", original);
                lab_dom_set(control, "marsZoomValue", lab_dom_item(exact, i));
            } else {
                lab_dom_delete(control, "marsZoomText"); lab_dom_delete(control, "marsZoomValue");
            }
        } else {
            int recipe = bound_recipe(lab_dom_item(exact, lower), lab_dom_item(exact, lower + 1), exponent, i % 2);
            int text = recipe;
            if (__builtin_isfinite(approximate) && half > __builtin_fabs(centre) * 1e-14)
                text = lab_dom_precision(approximate, lab_plot_bound_digits(approximate, 2*half));
            lab_dom_write(control, 5, "", text);
            lab_dom_set(control, "marsZoomText", text);
            lab_dom_set(control, "marsZoomValue", recipe);
        }
    }
    if (!segments)
        lab_dom_write(lab_dom_id("plotScale"), 5, "", reset ?
                      lab_dom_item(lab_dom_get(volume, "original"), 6) : lab_dom_string(auto_height ? "auto" : "manual"));
    lab_dom_set(volume, "bounds", bounds);
    lab_dom_set(volume, "exponent", lab_dom_numeric(exponent));
    lab_dom_set(volume, "dirty", lab_dom_scalar(1, 1));
    lab_dom_set(volume, "started", lab_dom_scalar(1, !reset));
    lab_dom_set(camera, "scale", lab_dom_numeric(segments ? 1 : scale));
    lab_plot_controls_sync();
    return 1;
}

/* The slider follows the current exponent, allowing repeated microscopic zoom. */
void lab_surface_zoom_sync(int view)
{
    double exponent = numeric(lab_dom_get(view, "volume"), "exponent");
    int slider = lab_dom_id("plotZoom");
    int segments = lab_dom_equal(lab_dom_read(lab_dom_get(view, "chart"), 3, "segments"), lab_dom_string("true"));
    lab_dom_write(slider, 2, "title", lab_dom_string(segments ?
        "Zoom the input range and redraw the 3-D curve" :
        "Zoom the coordinate volume equally on all axes"));
    lab_dom_write(slider, 2, "min", lab_dom_format(exponent < -3 ? (long)exponent-6 : -3, "", ""));
    lab_dom_write(slider, 2, "max", lab_dom_format(exponent > 6 ? (long)exponent+6 : 6, "", ""));
    lab_dom_write(slider, 5, "", lab_dom_format(exponent, "", ""));
    int label;
    if (exponent < 20 && exponent > -10) {
        double percent = lab_surface_exp2(exponent)*100;
        double rounded = __builtin_floor(percent * (percent < 100 ? 10 : 1) + 0.5) / (percent < 100 ? 10 : 1);
        label = lab_dom_format(rounded, "", "%");
    } else label = lab_dom_join(lab_dom_string("2^"), lab_dom_precision(exponent, 7), "×");
    lab_dom_write(slider, 2, "aria-valuetext", label);
    lab_dom_write(lab_dom_id("plotZoomValue"), 0, "", label);
}

/* Preview labels are approximate only; deep views wait for native tick labels. */
int lab_surface_tick_label(int volume, int tick)
{
    if (!lab_dom_truth(lab_dom_get(volume, "dirty"))) return lab_dom_get(tick, "label");
    unsigned axis = (unsigned)numeric(tick, "axis");
    int bounds = lab_dom_get(volume, "bounds");
    double lo = lab_dom_to_number(lab_dom_item(bounds, axis*2)), hi = lab_dom_to_number(lab_dom_item(bounds, axis*2+1));
    if (!(hi > lo) || !__builtin_isfinite(hi-lo)) return lab_dom_string("");
    double fraction = numeric(tick, "fraction"), value = lo*(1-fraction)+hi*fraction;
    return lab_dom_precision(value, lab_plot_bound_digits(value, hi-lo));
}
