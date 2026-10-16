/**
 * @file lab_surface_volume.c
 * @brief C-owned coordinate-volume zoom without an absolute double-precision boundary.
 *
 * Each range is an opaque exact native baseline plus a binary scale exponent.
 * The worker evaluates centre +/- half-span * 2^-exponent with number_t and
 * adaptive precision. Recipes never accumulate or nest across gestures. The
 * browser's numbers are only approximate labels and relative GPU transforms.
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
    int refining = lab_dom_truth(lab_dom_get(saved, "refining"));
    if (!volume || (!same && !refining)) {
        volume = lab_dom_object(5);
        lab_dom_set(volume, "base", read_bounds(chart, keys));
        lab_dom_set(volume, "bounds", read_bounds(chart, keys));
        lab_dom_set(volume, "exact", initial_exact(chart));
        lab_dom_set(volume, "units", lab_dom_get(view, "units"));
        lab_dom_set(volume, "labelWidth", lab_dom_read(chart, 3, "labelWidth"));
        lab_dom_set(volume, "original", control_values());
        lab_dom_set(volume, "exponent", lab_dom_numeric(0));
        lab_dom_set(volume, "sampleExponent", lab_dom_numeric(0));
    }
    lab_dom_set(volume, "sampled", read_bounds(chart, keys));
    if (refining) {
        lab_dom_set(volume, "dirty", lab_dom_scalar(1, 0));
        lab_dom_set(volume, "sampleExponent", lab_dom_get(volume, "exponent"));
        for (unsigned i = 0; i < 6 && lab_dom_truth(lab_dom_get(volume, "started")); ++i) {
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
        int start = lab_dom_get(event, "surfaceExponent");
        if (!start) { start = lab_dom_numeric(exponent); lab_dom_set(event, "surfaceExponent", start); }
        double dx = numeric(event, "touchX") - numeric(event, "secondX");
        double dy = numeric(event, "touchY") - numeric(event, "secondY");
        exponent = lab_dom_to_number(start);
        if (!lab_dom_truth(lab_dom_get(event, "pinchCancelled")))
            exponent += lab_surface_log2(__builtin_sqrt(dx*dx + dy*dy) / numeric(event, "pinchDistance"));
    }
    if (reset) exponent = 0;
    /* Native plotting has a 65,536-bit resource ceiling, not an f64 coordinate limit. */
    if (!__builtin_isfinite(exponent) || exponent > 32000 || exponent < -1024) {
        int feedback = lab_dom_id("plotFeedback");
        lab_dom_write(feedback, 0, "", lab_dom_string("Zoom exceeds the native plotting resource limit."));
        lab_dom_class(feedback, "hidden", 0);
        return 0;
    }
    if (!lab_dom_truth(lab_dom_get(volume, "started")) && !reset)
        lab_dom_set(volume, "original", control_values());
    int exact = lab_dom_get(volume, "exact"), base = lab_dom_get(volume, "base"), bounds = lab_dom_object(4);
    double scale = lab_surface_exp2(exponent);
    for (unsigned i = 0; i < 6; ++i) {
        unsigned lower = (i / 2) * 2;
        double lo = lab_dom_to_number(lab_dom_item(base, lower)), hi = lab_dom_to_number(lab_dom_item(base, lower + 1));
        double half = (hi - lo) / (2 * scale), centre = lo/2 + hi/2;
        double approximate = centre + (i % 2 ? half : -half);
        lab_dom_push(bounds, lab_dom_numeric(approximate));
        int control = lab_dom_id(controls[i]);
        if (reset) {
            lab_dom_write(control, 5, "", lab_dom_item(lab_dom_get(volume, "original"), i));
            lab_dom_delete(control, "marsZoomText"); lab_dom_delete(control, "marsZoomValue");
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
    lab_dom_write(lab_dom_id("plotScale"), 5, "", reset ?
                  lab_dom_item(lab_dom_get(volume, "original"), 6) : lab_dom_string("manual"));
    lab_dom_set(volume, "bounds", bounds);
    lab_dom_set(volume, "exponent", lab_dom_numeric(exponent));
    lab_dom_set(volume, "dirty", lab_dom_scalar(1, 1));
    lab_dom_set(volume, "started", lab_dom_scalar(1, !reset));
    lab_dom_set(camera, "scale", lab_dom_numeric(scale));
    lab_plot_controls_sync();
    return 1;
}

/* The slider follows the current exponent, allowing repeated microscopic zoom. */
void lab_surface_zoom_sync(int view)
{
    double exponent = numeric(lab_dom_get(view, "volume"), "exponent");
    int slider = lab_dom_id("plotZoom");
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
