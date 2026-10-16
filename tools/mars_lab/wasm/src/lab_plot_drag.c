/**
 * @file lab_plot_drag.c
 * @brief Mouse, touch and pen curve panning and surface rotation with coalesced live redraws.
 *
 * Surface dragging supplies pointer anchors to the retained quaternion camera.
 * Shift-drag requests screen-plane roll. Angle sliders explicitly set orientation.
 * Shift+wheel adjusts elevation and Alt+wheel adjusts azimuth in the same direction.
 * Surface cameras remain local on release; curve-range gestures refine natively.
 * Sliders and degree readouts update together. Only one frame or native curve
 * request runs at a time; further movements replace the pending position.
 * Revision ownership prevents superseded interactions from adopting stale results.
 * Cancellation restores the original camera. Superseding requests and mode changes stop the gesture.
 * Browser-owned state retains real DOM objects, never transient bridge handles.
 * A second touch promotes dragging to pinch zoom; further pointers are ignored.
 */
#include "lab_dom.h"
#include "lab_workspace.h"
#include "lab_plot_pointer.h"
#include "lab_plot_pinch.h"
#include "lab_plot_drag.h"

static const char *const controls[] = {"#plotAzimuth", "#plotElevation", "#plotRoll"};
static const char *const angles[] = {"azimuth", "elevation", "roll"};

static double read_number(int object, const char *key)
{
    return lab_dom_to_number(lab_dom_get(object, key));
}

static void set_angle(unsigned axis, double value)
{
    int control = lab_dom_query(0, controls[axis]);
    lab_dom_write(control, 5, "", lab_dom_format(value, "", ""));
    int output = lab_dom_query(lab_dom_closest(control, ".plot-angle"), "output");
    lab_dom_write(output, 0, "", lab_dom_format(value, "", "°"));
}

static int current(int state)
{
    if (lab_dom_truth(lab_dom_get(state, "pan")))
        return lab_workspace_mode() == (unsigned)read_number(state, "mode") &&
               lab_dom_node_contains(lab_dom_id("plotRendered"), lab_dom_get(state, "surface")) &&
               lab_dom_equal(lab_dom_get(state, "variable"), lab_dom_read(lab_dom_id("plotFirst"), 5, ""));
    return lab_workspace_mode() == (unsigned)read_number(state, "mode") &&
           lab_dom_equal(lab_dom_get(state, "surface"), lab_dom_query(0, "#plotRendered [data-surface]")) &&
           lab_dom_equal(lab_dom_get(state, "variable"), lab_dom_read(lab_dom_id("plotFirst"), 5, ""));
}

static void queue(int state, int calls)
{
    /* Keep a stable SVG during ordinary panning; resample once the pointer is released. */
    if (lab_dom_truth(lab_dom_get(state, "pan")) && lab_dom_truth(lab_dom_get(state, "active")) &&
        !lab_dom_truth(lab_dom_get(state, "pinch")))
        return;
    if (lab_dom_truth(lab_dom_get(state, "busy")) || !lab_dom_truth(lab_dom_get(state, "dirty")))
        return;
    lab_dom_set(state, "busy", lab_dom_scalar(1, 1));
    lab_dom_set(state, "dirty", lab_dom_scalar(1, 0));
    lab_dom_set(state, "redrawn", lab_dom_scalar(1, 1));
    if (lab_dom_truth(lab_dom_get(state, "pan"))) {
        lab_dom_set(state, "panRequestX", lab_dom_get(state, "panX"));
        lab_dom_set(state, "panRequestY", lab_dom_get(state, "panY"));
    }
    int call = lab_dom_object(5), args = lab_dom_object(4);
    lab_dom_push(args, state);
    lab_dom_push(args, lab_dom_get(state, "variable"));
    int preview = lab_dom_truth(lab_dom_get(state, "active")) || lab_dom_truth(lab_dom_get(state, "wheel"));
    lab_dom_push(args, lab_dom_scalar(1, preview));
    lab_dom_push(args, lab_dom_scalar(1, !preview &&
        lab_dom_equal(lab_dom_read(lab_dom_get(state, "surface"), 3, "plotQuality"), lab_dom_string("preview"))));
    lab_dom_set(call, "service", lab_dom_string("redrawPlotDrag"));
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
}

static void stop(int state)
{
    lab_dom_set(state, "active", lab_dom_scalar(1, 0));
    lab_dom_set(state, "wheel", lab_dom_scalar(1, 0));
    lab_dom_set(state, "dirty", lab_dom_scalar(1, 0));
    lab_dom_set(state, "pan", lab_dom_scalar(1, 0));
    lab_dom_set(state, "panPlane", lab_dom_scalar(1, 0));
    lab_dom_set(state, "pinch", lab_dom_scalar(1, 0));
    lab_dom_set(state, "slider", lab_dom_scalar(1, 0));
    lab_dom_delete(state, "secondId");
    lab_dom_delete(state, "surface");
    lab_dom_delete(state, "variable");
    lab_dom_delete(state, "pointerId");
    lab_dom_class(lab_dom_id("plotRendered"), "plot-dragging", 0);
}

static void finish(int state, int calls)
{
    lab_dom_set(state, "active", lab_dom_scalar(1, 0));
    lab_dom_set(state, "wheel", lab_dom_scalar(1, 0));
    lab_dom_class(lab_dom_id("plotRendered"), "plot-dragging", 0);
    if (lab_dom_truth(lab_dom_get(state, "redrawn")))
        lab_dom_set(state, "dirty", lab_dom_scalar(1, 1));
    queue(state, calls);
    if (!lab_dom_truth(lab_dom_get(state, "busy")))
        stop(state);
}

static void cancel(int state, int calls)
{
    if (!current(state)) {
        stop(state);
        return;
    }
    if (lab_dom_truth(lab_dom_get(state, "pinch"))) {
        lab_plot_pinch_cancel(state);
        if (!lab_dom_truth(lab_dom_get(state, "pan")))
            for (unsigned axis = 0; axis < 3; ++axis)
                set_angle(axis, read_number(state, angles[axis]));
    } else if (lab_dom_truth(lab_dom_get(state, "pan"))) {
        lab_plot_pan_cancel(state);
        lab_dom_set(state, "panX", lab_dom_numeric(0));
        lab_dom_set(state, "panY", lab_dom_numeric(0));
        lab_plot_pan_preview(state);
    } else
        for (unsigned axis = 0; axis < 3; ++axis)
            set_angle(axis, read_number(state, angles[axis]));
    lab_dom_set(state, "rotationCancelled", lab_dom_scalar(1, 1));
    lab_dom_set(state, "dirty", lab_dom_get(state, "redrawn"));
    finish(state, calls);
}

static void interrupt(int state, int calls)
{
    int call = lab_dom_object(5), args = lab_dom_object(4);
    double revision = lab_dom_truth(lab_dom_get(state, "revision")) ? read_number(state, "revision") : 0;
    lab_dom_set(state, "revision", lab_dom_numeric(revision + 1));
    lab_dom_set(state, "busy", lab_dom_scalar(1, 0));
    lab_dom_push(args, state);
    lab_dom_set(call, "service", lab_dom_string("interruptPlotDrag"));
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
}

static void pause_sampling(int calls)
{
    int call = lab_dom_object(5);
    lab_dom_set(call, "service", lab_dom_string("pauseSurfaceSampling"));
    lab_dom_set(call, "args", lab_dom_object(4));
    lab_dom_push(calls, call);
}

/* Sliders use the same retained camera and revision ownership as direct rotation. */
void lab_plot_slider(unsigned action, int event, int calls)
{
    int rendered = lab_dom_id("plotRendered"), state = lab_dom_get(rendered, "marsPlotDrag");
    int surface = lab_dom_query(rendered, "[data-surface]");
    if (!state || !surface || action > 2 || (lab_workspace_mode() > 2 && lab_workspace_mode() != 4))
        return;
    if (action == 2) {
        if (lab_dom_truth(lab_dom_get(state, "active")) && lab_dom_truth(lab_dom_get(state, "slider")))
            finish(state, calls);
        return;
    }
    if (!lab_dom_truth(lab_dom_get(state, "active"))) {
        pause_sampling(calls);
        if (lab_dom_truth(lab_dom_get(state, "busy"))) {
            if (!current(state) || lab_dom_truth(lab_dom_get(state, "pan")) ||
                lab_dom_truth(lab_dom_get(state, "pinch")))
                return;
            interrupt(state, calls);
        }
        lab_plot_wheel_stop();
        lab_dom_set(state, "surface", surface);
        lab_dom_set(state, "variable", lab_dom_read(lab_dom_id("plotFirst"), 5, ""));
        lab_dom_set(state, "mode", lab_dom_numeric(lab_workspace_mode()));
        lab_dom_set(state, "active", lab_dom_scalar(1, 1));
        lab_dom_set(state, "slider", lab_dom_scalar(1, 1));
        lab_dom_set(state, "trackball", lab_dom_scalar(1, 0));
        lab_dom_set(state, "pan", lab_dom_scalar(1, 0));
        lab_dom_set(state, "wheel", lab_dom_scalar(1, 0));
        lab_dom_set(state, "dirty", lab_dom_scalar(1, 0));
        lab_dom_set(state, "redrawn", lab_dom_scalar(1, lab_dom_has(surface, "data-local-renderer")));
        lab_dom_set(state, "pointerId", lab_dom_get(event, "pointerId"));
        for (unsigned axis = 0; axis < 3; ++axis)
            lab_dom_set(state, angles[axis], lab_dom_read(lab_dom_query(0, controls[axis]), 5, ""));
    }
    if (action == 1 && lab_dom_truth(lab_dom_get(state, "slider"))) {
        lab_dom_set(state, "dirty", lab_dom_scalar(1, 1));
        queue(state, calls);
    }
}

static int start(int event, int state, int calls)
{
    lab_plot_wheel_stop();
    int rendered = lab_dom_id("plotRendered");
    int target = lab_dom_get(event, "target"), panel = lab_dom_closest(target, ".complex-panel");
    int root = panel ? panel : rendered, surface = lab_dom_query(root, "[data-surface]");
    int pan = !surface;
    if (pan)
        surface = lab_dom_query(root, "[data-plot-chart][data-x-min]:not([data-surface])");
    int resume = !pan && current(state) && !lab_dom_truth(lab_dom_get(state, "pinch")) &&
                 lab_dom_has(surface, "data-local-renderer");
    if ((lab_dom_truth(lab_dom_get(state, "busy")) && !resume) || lab_dom_truth(lab_dom_get(state, "active")) ||
        lab_dom_truth(lab_dom_get(state, "wheel")) ||
        (lab_workspace_mode() > 2 && lab_workspace_mode() != 4) || !surface ||
        read_number(event, "button") != 0 ||
        !lab_dom_truth(lab_dom_get(event, "isPrimary")) ||
        lab_dom_truth(lab_dom_get(event, "ctrlKey")) || lab_dom_truth(lab_dom_get(event, "metaKey")) ||
        lab_dom_truth(lab_dom_get(event, "altKey")) || !lab_dom_closest(target, "svg") ||
        !lab_dom_node_contains(rendered, target))
        return 0;
    if (lab_dom_truth(lab_dom_get(state, "busy")))
        interrupt(state, calls);
    if (pan && !lab_plot_pan_start(state, surface, event))
        return 0;
    lab_dom_set(state, "pan", lab_dom_scalar(1, pan));
    lab_dom_set(state, "slider", lab_dom_scalar(1, 0));
    lab_dom_set(state, "trackball", lab_dom_scalar(1, !pan));
    lab_dom_delete(state, "rotationStart");
    lab_dom_delete(state, "rotationCancelled");
    lab_dom_set(state, "panX", lab_dom_numeric(0));
    lab_dom_set(state, "panY", lab_dom_numeric(0));
    lab_dom_set(state, "panDrawnX", lab_dom_numeric(0));
    lab_dom_set(state, "panDrawnY", lab_dom_numeric(0));
    lab_dom_set(state, "mode", lab_dom_numeric(lab_workspace_mode()));
    lab_dom_set(state, "surface", surface);
    lab_dom_set(state, "variable", lab_dom_read(lab_dom_id("plotFirst"), 5, ""));
    lab_dom_set(state, "touch", lab_dom_scalar(1,
        lab_dom_equal(lab_dom_get(event, "pointerType"), lab_dom_string("touch"))));
    lab_dom_set(state, "touchX", lab_dom_get(event, "clientX"));
    lab_dom_set(state, "touchY", lab_dom_get(event, "clientY"));
    lab_dom_set(state, "active", lab_dom_scalar(1, 1));
    lab_dom_set(state, "pointerId", lab_dom_get(event, "pointerId"));
    lab_dom_set(state, "changed", lab_dom_scalar(1, 0));
    lab_dom_set(state, "dirty", lab_dom_scalar(1, 0));
    lab_dom_set(state, "redrawn", lab_dom_scalar(1, resume));
    lab_dom_set(state, "rollOnly", lab_dom_get(event, "shiftKey"));
    lab_dom_set(state, "x", lab_dom_get(event, "clientX"));
    lab_dom_set(state, "y", lab_dom_get(event, "clientY"));
    for (unsigned axis = 0; axis < 3; ++axis)
        lab_dom_set(state, angles[axis], lab_dom_read(lab_dom_query(0, controls[axis]), 5, ""));
    lab_dom_class(rendered, "plot-dragging", 1);
    if (!pan)
        pause_sampling(calls);
    return 1;
}

static double wrapped(double value)
{
    /* Pointer coordinates are finite; reduce turns without depending on libm in freestanding WASM. */
    value -= __builtin_floor(value / 360) * 360;
    return value == 0 ? 0 : value;
}

static double limited(unsigned axis, double value)
{
    return axis == 1 ? (value < -90 ? -90 : value > 90 ? 90 : value) : wrapped(value);
}

static void schedule_wheel_end(int state, int calls)
{
    double revision = lab_dom_truth(lab_dom_get(state, "wheelRevision")) ? read_number(state, "wheelRevision") : 0;
    lab_dom_set(state, "wheelRevision", lab_dom_numeric(revision + 1));
    int call = lab_dom_object(5), args = lab_dom_object(4);
    lab_dom_push(args, state);
    lab_dom_push(args, lab_dom_get(state, "wheelRevision"));
    lab_dom_push(args, lab_dom_numeric(300));
    lab_dom_set(call, "service", lab_dom_string("schedulePlotWheelEnd"));
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
}

/* Cancel deferred range refinement when another viewport interaction takes over. */
void lab_plot_wheel_stop(void)
{
    int state = lab_dom_get(lab_dom_id("plotRendered"), "marsPlotZoom");
    if (state)
        stop(state);
}

/* Wheel movement replaces queued bounds rather than starting concurrent requests. */
void lab_plot_wheel_zoom(int chart, int calls)
{
    int rendered = lab_dom_id("plotRendered"), state = lab_dom_get(rendered, "marsPlotZoom");
    if (!state) {
        state = lab_dom_object(5);
        lab_dom_set(rendered, "marsPlotZoom", state);
    }
    lab_dom_set(state, "rangeWheel", lab_dom_scalar(1, 1));
    lab_dom_set(state, "pan", lab_dom_scalar(1, !lab_dom_has(chart, "data-surface")));
    lab_dom_set(state, "surface", chart);
    lab_dom_set(state, "panProjection", lab_dom_read(lab_dom_closest(chart, ".complex-panel"), 3, "complexPanel"));
    lab_dom_set(state, "variable", lab_dom_read(lab_dom_id("plotFirst"), 5, ""));
    lab_dom_set(state, "mode", lab_dom_numeric(lab_workspace_mode()));
    lab_dom_set(state, "wheel", lab_dom_scalar(1, 1));
    lab_dom_set(state, "dirty", lab_dom_scalar(1, 1));
    queue(state, calls);
    schedule_wheel_end(state, calls);
}

static int wheel(int event, int state, int calls)
{
    int rendered = lab_dom_id("plotRendered"), surface = lab_dom_query(rendered, "[data-surface]");
    int target = lab_dom_get(event, "target");
    int panel = lab_dom_closest(target, ".complex-panel");
    if (panel && !lab_dom_node_contains(panel, surface))
        return 0;
    int shift = lab_dom_truth(lab_dom_get(event, "shiftKey"));
    int alt = lab_dom_truth(lab_dom_get(event, "altKey"));
    double delta = read_number(event, "deltaY");
    if ((lab_workspace_mode() > 2 && lab_workspace_mode() != 4) || !surface || shift == alt ||
        lab_dom_truth(lab_dom_get(event, "ctrlKey")) ||
        lab_dom_truth(lab_dom_get(event, "metaKey")) || lab_dom_truth(lab_dom_get(state, "active")) ||
        !__builtin_isfinite(delta) || delta == 0 || !lab_dom_closest(target, "svg") ||
        !lab_dom_node_contains(rendered, target))
        return 0;
    if ((lab_dom_truth(lab_dom_get(state, "busy")) || lab_dom_truth(lab_dom_get(state, "wheel"))) && !current(state))
        return 0;
    unsigned axis = shift ? 1 : 0;
    double initial = lab_dom_to_number(lab_dom_read(lab_dom_query(0, controls[axis]), 5, ""));
    /* Use a bounded angular step independent of pixel, line or page wheel units. */
    double value = limited(axis, initial + (delta > 0 ? 5 : -5));
    if (value == initial)
        return 1;
    if (!lab_dom_truth(lab_dom_get(state, "wheel"))) {
        for (unsigned i = 0; i < 3; ++i)
            lab_dom_set(state, angles[i], lab_dom_read(lab_dom_query(0, controls[i]), 5, ""));
        lab_dom_set(state, "redrawn", lab_dom_scalar(1, 0));
    }
    lab_dom_set(state, "surface", surface);
    lab_dom_set(state, "variable", lab_dom_read(lab_dom_id("plotFirst"), 5, ""));
    lab_dom_set(state, "mode", lab_dom_numeric(lab_workspace_mode()));
    lab_dom_set(state, "wheel", lab_dom_scalar(1, 1));
    lab_dom_class(rendered, "plot-dragging", 1);
    lab_dom_set(state, "trackball", lab_dom_scalar(1, 0));
    set_angle(axis, value);
    lab_dom_set(state, "dirty", lab_dom_scalar(1, 1));
    queue(state, calls);
    schedule_wheel_end(state, calls);
    return 1;
}

static void move(int event, int state)
{
    if (lab_dom_truth(lab_dom_get(state, "slider")))
        return;
    if (lab_dom_truth(lab_dom_get(state, "pinch"))) {
        if (lab_plot_pinch_move(state, event)) {
            lab_dom_set(state, "changed", lab_dom_scalar(1, 1));
            lab_dom_set(state, "dirty", lab_dom_scalar(1, 1));
        }
        return;
    }
    lab_dom_set(state, "touchX", lab_dom_get(event, "clientX"));
    lab_dom_set(state, "touchY", lab_dom_get(event, "clientY"));
    double dx = read_number(event, "clientX") - read_number(state, "x");
    double dy = read_number(event, "clientY") - read_number(state, "y");
    if (!lab_dom_truth(lab_dom_get(state, "changed")) && dx * dx + dy * dy < 9)
        return;
    if (lab_dom_truth(lab_dom_get(state, "pan"))) {
        if (lab_plot_pan_move(state, dx, dy)) {
            lab_dom_set(state, "panX", lab_dom_numeric(dx));
            lab_dom_set(state, "panY", lab_dom_numeric(dy));
            lab_plot_pan_preview(state);
            lab_dom_set(state, "changed", lab_dom_scalar(1, 1));
            lab_dom_set(state, "dirty", lab_dom_scalar(1, 1));
        }
        return;
    }
    lab_dom_set(state, "changed", lab_dom_scalar(1, 1));
    lab_dom_set(state, "dirty", lab_dom_scalar(1, 1));
}

/* Update controls immediately and coalesce moves while the native renderer is busy. */
void lab_plot_drag_dispatch(unsigned action, int event, int state)
{
    int plan = lab_dom_object(5), calls = lab_dom_object(4);
    lab_dom_set(plan, "calls", calls);
    lab_dom_return(plan);
    if (!state || action > 7)
        return;
    if (action == 1 || action == 2 || action == 7) {
        if (!lab_dom_truth(lab_dom_get(state, "active")) ||
            (!lab_dom_equal(lab_dom_get(event, "pointerId"), lab_dom_get(state, "pointerId")) &&
             !(lab_dom_truth(lab_dom_get(state, "pinch")) &&
               lab_dom_equal(lab_dom_get(event, "pointerId"), lab_dom_get(state, "secondId")))))
            return;
        if (action == 7)
            action = 3;
    }
    if (action == 6) {
        if (!lab_dom_truth(lab_dom_get(state, "wheel")) ||
            event != read_number(state, "wheelRevision"))
            return;
        if (current(state))
            finish(state, calls);
        else
            stop(state);
        return;
    }
    if (action == 5) {
        if (wheel(event, state, calls))
            lab_dom_set(plan, "prevent", lab_dom_scalar(1, 1));
        return;
    }
    if (!action) {
        if (lab_dom_truth(lab_dom_get(state, "active"))) {
            if (current(state) && lab_plot_pinch_start(state, event))
                lab_dom_set(plan, "prevent", lab_dom_scalar(1, 1));
        } else if (start(event, state, calls))
            lab_dom_set(plan, "prevent", lab_dom_scalar(1, 1));
        return;
    }
    if (!lab_dom_truth(lab_dom_get(state, "active")) &&
        !(action >= 3 && lab_dom_truth(lab_dom_get(state, "wheel"))))
        return;
    if (action == 4 && !lab_dom_equal(lab_dom_get(event, "key"), lab_dom_string("Escape")))
        return;
    if (action >= 3 || !current(state) || (action == 1 && !((unsigned)read_number(event, "buttons") & 1))) {
        cancel(state, calls);
        if (action == 4)
            lab_dom_set(plan, "prevent", lab_dom_scalar(1, 1));
        return;
    }
    if (action == 2 && read_number(event, "button") != 0)
        return;
    if (!lab_dom_truth(lab_dom_get(state, "slider")))
        lab_dom_set(plan, "prevent", lab_dom_scalar(1, 1));
    move(event, state);
    if (action == 2) {
        finish(state, calls);
        return;
    }
    queue(state, calls);
}

/* A superseded request must never adopt a replacement result or restart an obsolete gesture. */
void lab_plot_drag_complete(int state, int accepted)
{
    int plan = lab_dom_object(5), calls = lab_dom_object(4);
    lab_dom_set(plan, "calls", calls);
    lab_dom_return(plan);
    if (!state)
        return;
    lab_dom_set(state, "busy", lab_dom_scalar(1, 0));
    int pan = lab_dom_truth(lab_dom_get(state, "pan"));
    int root = lab_dom_id("plotRendered"), projection = lab_dom_get(state, "panProjection");
    if (pan && lab_dom_length(projection))
        root = lab_dom_query(root, lab_dom_equal(projection, lab_dom_string("complex_plane"))
                                      ? "[data-complex-panel=\"complex_plane\"]"
                                      : lab_dom_equal(projection, lab_dom_string("amplitude"))
                                          ? "[data-complex-panel=\"amplitude\"]"
                                          : "[data-complex-panel=\"components\"]");
    int surface = lab_dom_query(root, pan ? "[data-plot-chart][data-x-min]:not([data-surface])" : "[data-surface]");
    if (!accepted || lab_workspace_mode() != (unsigned)read_number(state, "mode") ||
        !surface || !lab_dom_equal(lab_dom_get(state, "variable"),
            lab_dom_read(lab_dom_id("plotFirst"), 5, ""))) {
        stop(state);
        return;
    }
    lab_dom_set(state, "surface", surface);
    if (pan && !lab_dom_truth(lab_dom_get(state, "pinch")) && !lab_dom_truth(lab_dom_get(state, "rangeWheel"))) {
        lab_dom_set(state, "panDrawnX", lab_dom_get(state, "panRequestX"));
        lab_dom_set(state, "panDrawnY", lab_dom_get(state, "panRequestY"));
        lab_plot_pan_preview(state);
    }
    queue(state, calls);
    if (!lab_dom_truth(lab_dom_get(state, "active")) && !lab_dom_truth(lab_dom_get(state, "wheel")) &&
        !lab_dom_truth(lab_dom_get(state, "busy")))
        stop(state);
}

/* Window subscriptions retain releases outside the plot; blur cancels releases outside the browser. */
void lab_plot_drag_install(int document, int window)
{
    int rendered = lab_dom_id("plotRendered");
    if (!rendered)
        return;
    int state = lab_dom_get(rendered, "marsPlotDrag");
    if (!state) {
        state = lab_dom_object(5);
        lab_dom_set(rendered, "marsPlotDrag", state);
    }
    lab_dom_subscribe(rendered, "pointerdown", "lab_plot_drag_dispatch", 0, state, 0);
    lab_dom_subscribe(rendered, "wheel", "lab_plot_drag_dispatch", 5, state, 0);
    lab_dom_subscribe(window, "pointermove", "lab_plot_drag_dispatch", 1, state, 0);
    lab_dom_subscribe(window, "pointerup", "lab_plot_drag_dispatch", 2, state, 0);
    lab_dom_subscribe(window, "pointercancel", "lab_plot_drag_dispatch", 7, state, 0);
    lab_dom_subscribe(window, "blur", "lab_plot_drag_dispatch", 3, state, 0);
    lab_dom_subscribe(window, "pagehide", "lab_plot_drag_dispatch", 3, state, 0);
    lab_dom_subscribe(document, "visibilitychange", "lab_plot_drag_dispatch", 3, state, 0);
    lab_dom_subscribe(document, "keydown", "lab_plot_drag_dispatch", 4, state, 0);
}
