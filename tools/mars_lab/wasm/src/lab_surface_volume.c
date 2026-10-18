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
 * and the original coordinate-box proportions whilst adopting fresh output ranges.
 */
#include <math.h>

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

static double numeric(browser_jsref object, const char *key)
{
    return lab_dom_to_number(lab_dom_get(object, key));
}

/* New plots and explicit reset restore the same slider track as the initial page. */
static void reset_slider_limits(void)
{
    int slider = lab_dom_id("plotZoom");
    lab_dom_write(slider, LAB_DOM_ATTRIBUTE, "min", lab_dom_string("-3"));
    lab_dom_write(slider, LAB_DOM_ATTRIBUTE, "max", lab_dom_string("6"));
}

static browser_jsref read_bounds(chart_jsref chart, const char *const names[6])
{
    int result = lab_dom_object(LAB_VALUE_ARRAY);
    for (unsigned i = 0; i < 6; ++i) {
        int value = lab_dom_read(chart, LAB_DOM_DATASET, names[i]);
        lab_dom_push(result, names == keys ? lab_dom_numeric(lab_dom_to_number(value)) : value);
    }
    return result;
}

static browser_jsref control_values(void)
{
    int result = lab_dom_object(LAB_VALUE_ARRAY);
    for (unsigned i = 0; i < 7; ++i)
        lab_dom_push(result, lab_dom_read(lab_dom_id(controls[i]), LAB_DOM_VALUE, ""));
    return result;
}

static browser_jsref authored_bound(unsigned i)
{
    int control = lab_dom_id(controls[i]), text = lab_dom_read(control, LAB_DOM_VALUE, "");
    int exact = lab_dom_get(control, "marsZoomValue");
    return exact && lab_dom_equal(text, lab_dom_get(control, "marsZoomText")) ? exact : text;
}

static browser_jsref initial_exact(chart_jsref chart)
{
    /* A retained curve can coexist with sibling plots that edit the shared controls. */
    if (lab_dom_equal(lab_dom_read(chart, LAB_DOM_DATASET, "segments"), lab_dom_string("true")))
        return read_bounds(chart, exact_keys);
    int result = lab_dom_object(LAB_VALUE_ARRAY);
    int manual = lab_dom_equal(lab_dom_read(lab_dom_id("plotScale"), LAB_DOM_VALUE, ""), lab_dom_string("manual"));
    for (unsigned axis = 0; axis < 3; ++axis) {
        int low = authored_bound(axis * 2), high = authored_bound(axis * 2 + 1);
        int supplied = (axis < 2 || manual) && lab_dom_length(lab_dom_clean(low, LAB_DOM_CLEAN_TRIM)) &&
                       lab_dom_length(lab_dom_clean(high, LAB_DOM_CLEAN_TRIM));
        lab_dom_push(result, supplied ? low : lab_dom_read(chart, LAB_DOM_DATASET, exact_keys[axis * 2]));
        lab_dom_push(result, supplied ? high : lab_dom_read(chart, LAB_DOM_DATASET, exact_keys[axis * 2 + 1]));
    }
    return result;
}

/* Preserve exact native metadata, not the lossy compatibility coordinates. */
void lab_surface_volume_init(view_jsref view, browser_jsref saved, bool same)
{
    int chart = lab_dom_get(view, "chart"), volume = lab_dom_get(saved, "volume");
    int segments = lab_dom_equal(lab_dom_read(chart, LAB_DOM_DATASET, "segments"), lab_dom_string("true"));
    int refining = lab_dom_truth(lab_dom_get(saved, "refining"));
    int manual = lab_dom_equal(lab_dom_read(lab_dom_id("plotScale"), LAB_DOM_VALUE, ""), lab_dom_string("manual"));
    int automatic = lab_dom_equal(lab_dom_read(lab_dom_id("plotScale"), LAB_DOM_VALUE, ""), lab_dom_string("auto"));
    /* Auto/Fit all must discard a clipped preview even if the native mesh is unchanged.
       Genuine zoom refinements retain their exact selected volume. */
    int was_auto = lab_dom_truth(lab_dom_get(volume, "autoHeight"));
    int restore_height = !segments && volume && !refining && lab_dom_truth(lab_dom_get(volume, "started")) &&
                         ((!manual && !was_auto) || (was_auto && !automatic));
    if (!volume || (!same && !refining) || restore_height) {
        reset_slider_limits();
        volume = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(volume, "base", read_bounds(chart, keys));
        lab_dom_set(volume, "bounds", read_bounds(chart, keys));
        lab_dom_set(volume, "exact", restore_height ? read_bounds(chart, exact_keys) : initial_exact(chart));
        lab_dom_set(volume, "units", lab_dom_get(view, "units"));
        lab_dom_set(volume, "labelWidth", lab_dom_read(chart, LAB_DOM_DATASET, "labelWidth"));
        lab_dom_set(volume, "original", segments ? read_bounds(chart, exact_keys) : control_values());
        lab_dom_set(volume, "exponent", lab_dom_numeric(0));
        lab_dom_set(volume, "sampleExponent", lab_dom_numeric(0));
        lab_dom_set(volume, "autoHeight",
                    lab_dom_scalar(LAB_VALUE_BOOLEAN,
                                   lab_dom_equal(lab_dom_read(chart, LAB_DOM_DATASET, "mesh"), lab_dom_string("false")) &&
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
            lab_dom_set(volume, "labelWidth", lab_dom_read(chart, LAB_DOM_DATASET, "labelWidth"));
            /* Keep the original box proportions: the narrower interval fills the same x axis. */
            /* Even a reset keeps the original curve baseline: finite segment
             * extents can differ slightly from the requested input interval. */
        } else if (lab_dom_truth(lab_dom_get(volume, "autoHeight"))) {
            int previous = lab_dom_get(volume, "bounds"), bounds = lab_dom_object(LAB_VALUE_ARRAY);
            int retain_input = lab_dom_truth(lab_dom_get(volume, "started"));
            for (unsigned i = 0; i < 6; ++i)
                lab_dom_push(bounds, i < 4 && retain_input ? lab_dom_item(previous, i) :
                             lab_dom_numeric(lab_dom_to_number(lab_dom_read(chart, LAB_DOM_DATASET, keys[i]))));
            lab_dom_set(volume, "bounds", bounds);
            /* Reset may restore Auto input bounds different from the old gesture baseline. */
            if (!retain_input) {
                lab_dom_set(volume, "base", read_bounds(chart, keys));
                lab_dom_set(volume, "exact", read_bounds(chart, exact_keys));
            }
            lab_dom_set(volume, "units", lab_dom_get(view, "units"));
            lab_dom_set(volume, "labelWidth", lab_dom_read(chart, LAB_DOM_DATASET, "labelWidth"));
        }
        lab_dom_set(volume, "dirty", lab_dom_scalar(LAB_VALUE_BOOLEAN, 0));
        lab_dom_set(volume, "sampleExponent", lab_dom_get(volume, "exponent"));
        for (unsigned i = 0; i < (segments ? 2u : 6u) && lab_dom_truth(lab_dom_get(volume, "started")); ++i) {
            int control = lab_dom_id(controls[i]), text = lab_dom_read(chart, LAB_DOM_DATASET, label_keys[i]);
            lab_dom_write(control, LAB_DOM_VALUE, "", text);
            lab_dom_set(control, "marsZoomText", text);
            lab_dom_set(control, "marsZoomValue", lab_dom_read(chart, LAB_DOM_DATASET, exact_keys[i]));
        }
    }
    lab_dom_set(view, "volume", volume);
    lab_dom_set(view, "units", lab_dom_get(volume, "units"));
}

/* Roll back display policy without evaluating coordinates or replacing accepted geometry. */
static int rollback_volume(view_jsref view, unsigned restore_controls)
{
    int volume = lab_dom_get(view, "volume"), chart = lab_dom_get(view, "chart");
    double exponent = numeric(volume, "sampleExponent");
    if (!volume || (!lab_dom_truth(lab_dom_get(volume, "dirty")) && numeric(volume, "exponent") == exponent))
        return 0;
    int segments = lab_dom_equal(lab_dom_read(chart, LAB_DOM_DATASET, "segments"), lab_dom_string("true"));
    lab_dom_set(volume, "bounds", read_bounds(chart, keys));
    lab_dom_set(volume, "exponent", lab_dom_numeric(exponent));
    lab_dom_set(volume, "dirty", lab_dom_scalar(LAB_VALUE_BOOLEAN, 0));
    lab_dom_set(volume, "started", lab_dom_scalar(LAB_VALUE_BOOLEAN, exponent != 0));
    lab_dom_set(lab_dom_get(view, "camera"), "scale", lab_dom_numeric(segments ? 1 : lab_surface_exp2(exponent)));
    if (restore_controls) {
        for (unsigned i = 0; i < (segments ? 2u : 6u); ++i) {
            int control = lab_dom_id(controls[i]), text = lab_dom_read(chart, LAB_DOM_DATASET, label_keys[i]);
            lab_dom_write(control, LAB_DOM_VALUE, "", text);
            lab_dom_set(control, "marsZoomText", text);
            lab_dom_set(control, "marsZoomValue", lab_dom_read(chart, LAB_DOM_DATASET, exact_keys[i]));
        }
        if (!segments) {
            int policy = lab_dom_truth(lab_dom_get(volume, "autoHeight")) ? lab_dom_string("auto") :
                         exponent != 0 ? lab_dom_string("manual") : lab_dom_item(lab_dom_get(volume, "original"), 6);
            lab_dom_write(lab_dom_id("plotScale"), LAB_DOM_VALUE, "", policy);
        }
        lab_plot_controls_sync();
    }
    return 1;
}

static browser_jsref append(browser_jsref text, browser_jsref addition, const char *suffix)
{
    return lab_dom_join(text, addition, suffix);
}

/* A constant-sized recipe preserves the centre even when its span is microscopic. */
static browser_jsref bound_recipe(browser_jsref low, browser_jsref high, double exponent, int upper)
{
    int text = append(lab_dom_string("(("), low, ")+(");
    text = append(text, high, upper ? "))/2+((" : "))/2-((");
    text = append(text, high, ")-(");
    text = append(text, low, "))/2*2^(-(");
    return append(text, lab_dom_format(exponent, "", ""), "))");
}

/* Volume metadata is a small flat record; only its coordinate arrays need independent storage. */
static browser_jsref volume_copy(browser_jsref source)
{
    int copy = lab_dom_object(LAB_VALUE_OBJECT), names = lab_dom_keys(source);
    for (unsigned i = 0; i < lab_dom_count(names); ++i) {
        int key = lab_dom_item(names, i), value = lab_dom_key_get(source, key);
        if (lab_dom_type(value) == LAB_VALUE_ARRAY) {
            int array = lab_dom_object(LAB_VALUE_ARRAY);
            for (unsigned j = 0; j < lab_dom_count(value); ++j)
                lab_dom_push(array, lab_dom_item(value, j));
            value = array;
        }
        lab_dom_key_set(copy, key, value);
    }
    return copy;
}

/* Keep DOM controls separate from the mathematical baseline, including absent exact-value metadata. */
static browser_jsref pinch_snapshot(browser_jsref volume, browser_jsref camera)
{
    int snapshot = lab_dom_object(LAB_VALUE_OBJECT), values = lab_dom_object(LAB_VALUE_ARRAY);
    lab_dom_set(snapshot, "volume", volume_copy(volume));
    lab_dom_property_copy(snapshot, "scale", camera, "scale");
    lab_dom_set(snapshot, "inputRange", lab_dom_read(lab_dom_id("plotInputRange"), LAB_DOM_VALUE, ""));
    for (unsigned i = 0; i < 7; ++i) {
        int control = lab_dom_id(controls[i]), saved = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(saved, "text", lab_dom_read(control, LAB_DOM_VALUE, ""));
        lab_dom_property_copy(saved, "marsZoomText", control, "marsZoomText");
        lab_dom_property_copy(saved, "marsZoomValue", control, "marsZoomValue");
        lab_dom_push(values, saved);
    }
    lab_dom_set(snapshot, "controls", values);
    return snapshot;
}

/* Restore the full pre-gesture transaction without re-authoring bounds or requesting new samples. */
static void pinch_restore(browser_jsref volume, browser_jsref camera, snapshot_jsref snapshot)
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
        lab_dom_write(control, LAB_DOM_VALUE, "", lab_dom_get(saved, "text"));
        lab_dom_property_copy(control, "marsZoomText", saved, "marsZoomText");
        lab_dom_property_copy(control, "marsZoomValue", saved, "marsZoomValue");
    }
    lab_dom_write(lab_dom_id("plotInputRange"), LAB_DOM_VALUE, "", lab_dom_get(snapshot, "inputRange"));
    lab_plot_controls_sync();
}

/** @brief Gesture outcome before shared reset, validation and volume projection. */
typedef enum {
    LAB_SURFACE_GESTURE_REJECTED = 0, /**< Return failure without projecting the volume. */
    LAB_SURFACE_GESTURE_FINISHED = 1, /**< Return success after restoring a cancelled gesture. */
    LAB_SURFACE_GESTURE_CONTINUE = 2  /**< Apply reset and project the selected exponent. */
} lab_surface_gesture_result_t;

/** @brief Borrowed gesture state and the mutable relative zoom exponent. */
struct lab_surface_gesture_context {
    browser_jsref volume, camera;
    event_jsref event;
    double value, exponent;
};

/* Convert a relative scale to an exponent increment. */
static lab_surface_gesture_result_t zoom_factor(struct lab_surface_gesture_context *ctx)
{
    ctx->exponent += lab_surface_log2(ctx->value);
    return LAB_SURFACE_GESTURE_CONTINUE;
}

/* Accept the slider exponent before shared resource validation. */
static lab_surface_gesture_result_t zoom_exponent(struct lab_surface_gesture_context *ctx)
{
    ctx->exponent = ctx->value;
    return LAB_SURFACE_GESTURE_CONTINUE;
}

/* Scale exact wheel unit selectors, retaining pixel units for every unknown value. */
static lab_surface_gesture_result_t zoom_wheel(struct lab_surface_gesture_context *ctx)
{
    /** @brief Pixel multipliers for the browser's pixel, line and page wheel units. */
    static const double units[] = {
        [0] = 1,
        [1] = 16,
        [2] = 400,
    };
    double pixels = numeric(ctx->event, "deltaY"), mode = numeric(ctx->event, "deltaMode");
    double unit = 1;
    if (mode >= 0 && mode < sizeof units / sizeof *units && mode == (unsigned)mode)
        unit = units[(unsigned)mode];
    pixels *= unit;
    if (pixels > 200)
        pixels = 200;
    if (pixels < -200)
        pixels = -200;
    ctx->exponent -= pixels * 0.002 / 0.6931471805599453094;
    return LAB_SURFACE_GESTURE_CONTINUE;
}

/* Validate a pinch before snapshotting; cancellation restores state before shared reset. */
static lab_surface_gesture_result_t zoom_pinch(struct lab_surface_gesture_context *ctx)
{
    int event = ctx->event, snapshot = lab_dom_get(event, "surfaceZoomSnapshot");
    if (lab_dom_truth(lab_dom_get(event, "pinchCancelled"))) {
        if (snapshot)
            pinch_restore(ctx->volume, ctx->camera, snapshot);
        return LAB_SURFACE_GESTURE_FINISHED;
    }
    double distance = numeric(event, "pinchDistance");
    if (!(distance > 0) || !isfinite(distance))
        return LAB_SURFACE_GESTURE_REJECTED;
    double dx = numeric(event, "touchX") - numeric(event, "secondX");
    double dy = numeric(event, "touchY") - numeric(event, "secondY");
    double current = hypot(dx, dy);
    if (!(current > 0) || !isfinite(current))
        return LAB_SURFACE_GESTURE_REJECTED;
    double ratio = current / distance;
    if (!(ratio > 0) || !isfinite(ratio))
        return LAB_SURFACE_GESTURE_REJECTED;
    if (!snapshot)
        lab_dom_set(event, "surfaceZoomSnapshot", pinch_snapshot(ctx->volume, ctx->camera));
    int start = lab_dom_get(event, "surfaceExponent");
    if (!start) {
        start = lab_dom_numeric(ctx->exponent);
        lab_dom_set(event, "surfaceExponent", start);
    }
    ctx->exponent = lab_dom_to_number(start) + lab_surface_log2(ratio);
    return LAB_SURFACE_GESTURE_CONTINUE;
}

/* Convert bounded gestures to a binary exponent, never to absolute coordinate differences. */
bool lab_surface_zoom(view_jsref view, lab_surface_zoom_kind_t kind, double value, event_jsref event, bool reset)
{
    /** @brief Gesture handlers that run before shared reset and volume projection. */
    static lab_surface_gesture_result_t (*const handlers[])(struct lab_surface_gesture_context *) = {
        [LAB_SURFACE_ZOOM_FACTOR]   = zoom_factor,
        [LAB_SURFACE_ZOOM_EXPONENT] = zoom_exponent,
        [LAB_SURFACE_ZOOM_WHEEL]    = zoom_wheel,
        [LAB_SURFACE_ZOOM_PINCH]    = zoom_pinch,
    };
    if (kind == LAB_SURFACE_ZOOM_ROLLBACK)
        return rollback_volume(view, value != 0);
    int volume = lab_dom_get(view, "volume"), camera = lab_dom_get(view, "camera");
    struct lab_surface_gesture_context ctx = {volume, camera, event, value, numeric(volume, "exponent")};
    if ((unsigned)kind < sizeof handlers / sizeof *handlers) {
        lab_surface_gesture_result_t outcome = handlers[kind](&ctx);
        if (outcome != LAB_SURFACE_GESTURE_CONTINUE)
            return outcome == LAB_SURFACE_GESTURE_FINISHED;
    }
    double exponent = ctx.exponent;
    if (reset) {
        exponent = 0;
        reset_slider_limits();
    }
    int segments = lab_dom_equal(lab_dom_read(lab_dom_get(view, "chart"), LAB_DOM_DATASET, "segments"), lab_dom_string("true"));
    /* Native plotting has a 65,536-bit resource ceiling, not an f64 coordinate limit. */
    if (!isfinite(exponent) || exponent > 32000 || exponent < -1024) {
        int feedback = lab_dom_id("plotFeedback");
        lab_dom_write(feedback, LAB_DOM_TEXT, "", lab_dom_string("Zoom exceeds the native plotting resource limit."));
        lab_dom_class(feedback, "hidden", 0);
        return false;
    }
    if (!segments && !lab_dom_truth(lab_dom_get(volume, "started")) && !reset)
        lab_dom_set(volume, "original", control_values());
    int exact = lab_dom_get(volume, "exact"), base = lab_dom_get(volume, "base"),
        bounds = lab_dom_object(LAB_VALUE_ARRAY);
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
                lab_dom_write(control, LAB_DOM_VALUE, "", lab_dom_item(lab_dom_get(volume, "original"), i));
                lab_dom_delete(control, "marsZoomText");
                lab_dom_delete(control, "marsZoomValue");
            }
            continue;
        }
        unsigned lower = (i / 2) * 2;
        double lo = lab_dom_to_number(lab_dom_item(base, lower)), hi = lab_dom_to_number(lab_dom_item(base, lower + 1));
        double half = (hi - lo) / (2 * scale), centre = lo / 2 + hi / 2;
        double approximate = centre + (i % 2 ? half : -half);
        lab_dom_push(bounds, lab_dom_numeric(approximate));
        int control = lab_dom_id(controls[i]);
        if (reset) {
            int original = lab_dom_item(segments ? exact : lab_dom_get(volume, "original"), i);
            lab_dom_write(control, LAB_DOM_VALUE, "", original);
            if (segments && lab_dom_length(lab_dom_clean(original, LAB_DOM_CLEAN_TRIM))) {
                /* Display the curve's own exact baseline, never a sibling's range. */
                lab_dom_set(control, "marsZoomText", original);
                lab_dom_set(control, "marsZoomValue", lab_dom_item(exact, i));
            } else {
                lab_dom_delete(control, "marsZoomText");
                lab_dom_delete(control, "marsZoomValue");
            }
        } else {
            int recipe = bound_recipe(lab_dom_item(exact, lower), lab_dom_item(exact, lower + 1), exponent, i % 2);
            int text = recipe;
            if (isfinite(approximate) && half > __builtin_fabs(centre) * 1e-14)
                text = lab_dom_precision(approximate, lab_plot_bound_digits(approximate, 2 * half));
            lab_dom_write(control, LAB_DOM_VALUE, "", text);
            lab_dom_set(control, "marsZoomText", text);
            lab_dom_set(control, "marsZoomValue", recipe);
        }
    }
    if (!segments)
        lab_dom_write(lab_dom_id("plotScale"), LAB_DOM_VALUE, "", reset ?
                      lab_dom_item(lab_dom_get(volume, "original"), 6) :
                      lab_dom_string(auto_height ? "auto" : "manual"));
    lab_dom_set(volume, "bounds", bounds);
    lab_dom_set(volume, "exponent", lab_dom_numeric(exponent));
    lab_dom_set(volume, "dirty", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
    lab_dom_set(volume, "started", lab_dom_scalar(LAB_VALUE_BOOLEAN, !reset));
    lab_dom_set(camera, "scale", lab_dom_numeric(segments ? 1 : scale));
    lab_plot_controls_sync();
    return true;
}

/* Extend the slider only for wheel/pinch values outside its track; never move the track under a held thumb. */
void lab_surface_zoom_sync(view_jsref view)
{
    double exponent = numeric(lab_dom_get(view, "volume"), "exponent");
    int slider = lab_dom_id("plotZoom");
    int segments = lab_dom_equal(lab_dom_read(lab_dom_get(view, "chart"), LAB_DOM_DATASET, "segments"), lab_dom_string("true"));
    lab_dom_write(slider, LAB_DOM_ATTRIBUTE, "title",
                  lab_dom_string(segments ? "Zoom the input range and redraw the 3-D curve"
                                          : "Zoom the coordinate volume equally on all axes"));
    double minimum = numeric(slider, "min"), maximum = numeric(slider, "max");
    if (exponent < minimum)
        lab_dom_write(slider, LAB_DOM_ATTRIBUTE, "min", lab_dom_format(__builtin_floor(exponent) - 6, "", ""));
    if (exponent > maximum)
        lab_dom_write(slider, LAB_DOM_ATTRIBUTE, "max", lab_dom_format(__builtin_ceil(exponent) + 6, "", ""));
    lab_dom_write(slider, LAB_DOM_VALUE, "", lab_dom_format(exponent, "", ""));
    int label;
    if (exponent < 20 && exponent > -10) {
        double percent = lab_surface_exp2(exponent) * 100;
        double rounded = __builtin_floor(percent * (percent < 100 ? 10 : 1) + 0.5) / (percent < 100 ? 10 : 1);
        label = lab_dom_format(rounded, "", "%");
    } else
        label = lab_dom_join(lab_dom_string("2^"), lab_dom_precision(exponent, 7), "×");
    lab_dom_write(slider, LAB_DOM_ATTRIBUTE, "aria-valuetext", label);
    lab_dom_write(lab_dom_id("plotZoomValue"), LAB_DOM_TEXT, "", label);
}

/* Preview labels are approximate only; deep views wait for native tick labels. */
browser_jsref lab_surface_tick_label(browser_jsref volume, browser_jsref tick)
{
    if (!lab_dom_truth(lab_dom_get(volume, "dirty")))
        return lab_dom_get(tick, "label");
    unsigned axis = (unsigned)numeric(tick, "axis");
    int bounds = lab_dom_get(volume, "bounds");
    double lo = lab_dom_to_number(lab_dom_item(bounds, axis * 2)),
           hi = lab_dom_to_number(lab_dom_item(bounds, axis * 2 + 1));
    if (!(hi > lo) || !isfinite(hi - lo))
        return lab_dom_string("");
    double fraction = numeric(tick, "fraction"), value = lo * (1 - fraction) + hi * fraction;
    return lab_dom_precision(value, lab_plot_bound_digits(value, hi - lo));
}
