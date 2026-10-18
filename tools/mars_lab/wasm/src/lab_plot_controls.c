/**
 * @file lab_plot_controls.c
 * @brief Grouped plot ranges, native variable radio choices and dimensional visibility.
 *
 * Names and labels come from the native binding catalogue; no formula parsing or
 * numerical range selection occurs here. Browser-owned controls retain selections.
 * Blank range text is sent unchanged to the native worker's automatic-range policy.
 */
#include "lab_dom.h"
#include "lab_layout.h"
#include "lab_workspace.h"
#include "lab_requests.h"
#include "lab_plot_controls.h"
#include "lab_plot_drag.h"
#include "lab_plot_slices.h"

static int value(const char *id)
{
    return lab_dom_read(lab_dom_id(id), 5, "");
}

static const char *const controls[] = {"plotFirst", "plotSecond", "plotKind", "plotScale", "plotInputRange"};
static const char *const input_bounds[] = {"plotLower", "plotUpper", "plotSecondLower", "plotSecondUpper"};

static void input_range_auto(void)
{
    for (unsigned i = 0; i < sizeof input_bounds / sizeof *input_bounds; ++i) {
        int control = lab_dom_id(input_bounds[i]);
        lab_dom_write(control, 5, "", lab_dom_string(""));
        lab_dom_delete(control, "marsZoomValue");
        lab_dom_delete(control, "marsZoomText");
    }
    static const char *const cached[] = {"marsZoomOriginal", "marsZoomChart", "marsZoomBounds",
                                        "marsZoomAnchor", "marsZoomTitle"};
    for (unsigned i = 0; i < sizeof cached / sizeof *cached; ++i)
        lab_dom_delete(lab_dom_id("plotRange"), cached[i]);
    lab_dom_write(lab_dom_id("plotInputRange"), 5, "", lab_dom_string("auto"));
}

/* Explicit evaluation starts a new view; internal redraws and precision changes do not. */
void lab_plot_controls_reset(void)
{
    input_range_auto();
    static const char *const limits[] = {"plotLower", "plotUpper", "plotSecondLower", "plotSecondUpper",
                                         "plotYLower", "plotYUpper"};
    static const char *const cached[] = {"marsZoomOriginal", "marsZoomChart", "marsZoomBounds",
                                         "marsZoomAnchor", "marsZoomTitle"};
    static const char *const camera[] = {"plotAzimuth", "plotElevation", "plotRoll", "plotZoom"};
    static const char *const outputs[] = {"plotAzimuthValue", "plotElevationValue", "plotRollValue", "plotZoomValue"};
    static const char *const labels[] = {"315°", "30°", "0°", "100%"};
    for (unsigned i = 0; i < sizeof limits / sizeof *limits; ++i) {
        int control = lab_dom_id(limits[i]);
        lab_dom_write(control, 5, "", lab_dom_string(""));
        lab_dom_delete(control, "marsZoomValue");
        lab_dom_delete(control, "marsZoomText");
    }
    for (unsigned i = 0; i < sizeof cached / sizeof *cached; ++i)
        lab_dom_delete(lab_dom_id("plotRange"), cached[i]);
    for (unsigned i = 0; i < sizeof camera / sizeof *camera; ++i) {
        int control = lab_dom_id(camera[i]);
        lab_dom_write(control, 5, "", lab_dom_get(control, "defaultValue"));
        lab_dom_write(lab_dom_id(outputs[i]), 0, "", lab_dom_string(labels[i]));
    }
    lab_dom_write(lab_dom_id("plotKind"), 5, "", lab_dom_string("auto"));
    lab_dom_write(lab_dom_id("plotScale"), 5, "", lab_dom_string("auto"));
    lab_dom_delete(lab_dom_id("plotRendered"), "marsComplexPlaneBounds");
    int expanded = lab_dom_query(lab_dom_id("plotRendered"), ".complex-expanded");
    if (expanded)
        lab_layout_expand(expanded, 1);
    lab_dom_delete(lab_dom_id("plotRendered"), "marsComplexExpanded");
    lab_plot_slices_clear();
    lab_plot_controls_sync();
}

static int control_index(int name)
{
    /* Five fixed UI groups; mathematical symbol lookup remains with native binding metadata. */
    for (unsigned i = 0; i < sizeof controls / sizeof *controls; ++i)
        if (lab_dom_equal(name, lab_dom_string(controls[i])))
            return (int)i;
    return -1;
}

/* Index authored binding values once; plotting names come from the native catalogue. */
static int curve_available(int first)
{
    int fixed = lab_dom_object(5);
    int inputs = lab_dom_all(lab_dom_id("variableValues"), ".binding-value-input");
    for (unsigned i = 0; i < lab_dom_count(inputs); ++i) {
        int input = lab_dom_item(inputs, i), text = lab_dom_clean(lab_dom_read(input, 5, ""), 0);
        lab_dom_key_set(fixed, lab_dom_read(input, 3, "bindingName"),
                        lab_dom_scalar(1, lab_dom_length(text) && !lab_dom_equal(text, lab_dom_string("?"))));
    }
    int names = lab_dom_get(lab_dom_id("plotRange"), "marsPlotNames");
    for (unsigned i = 0; i < lab_dom_count(names); ++i) {
        int name = lab_dom_item(names, i);
        if (!lab_dom_equal(name, first) && !lab_dom_truth(lab_dom_key_get(fixed, name)))
            return 0;
    }
    return 1;
}

/* Project a bounded UI catalogue, not a symbol lookup or mathematical rewrite. */
void lab_plot_controls_sync(void)
{
    /* Saved and gesture-authored bounds are authoritative even without a saved selector. */
    for (unsigned i = 0; i < sizeof input_bounds / sizeof *input_bounds; ++i)
        if (lab_dom_length(lab_dom_clean(value(input_bounds[i]), 0)))
            lab_dom_write(lab_dom_id("plotInputRange"), 5, "", lab_dom_string("manual"));
    int solver = lab_workspace_mode() == 1 || lab_workspace_mode() == 2 || lab_workspace_mode() == 4;
    int complex = !!lab_dom_query(0, "#plotRendered .complex-grid");
    lab_dom_class(lab_dom_id("plotKindGroup"), "hidden", solver || complex);
    int first = value("plotFirst"), second = value("plotSecond");
    int curve_allowed = solver || curve_available(first);
    int curve_option = lab_dom_query(0, "#plotKindChoices input[value='curve']");
    lab_dom_class(lab_dom_closest(curve_option, "label"), "hidden", !curve_allowed);
    if (!curve_allowed && lab_dom_equal(value("plotKind"), lab_dom_string("curve")))
        lab_dom_write(lab_dom_id("plotKind"), 5, "", lab_dom_string("auto"));
    if (lab_dom_length(first) && lab_dom_equal(first, second)) {
        second = lab_dom_string("");
        lab_dom_write(lab_dom_id("plotSecond"), 5, "", second);
    }
    int choices = lab_dom_all(lab_dom_id("plotRange"), ".plot-choices input");
    for (unsigned i = 0; i < lab_dom_count(choices); ++i) {
        unsigned mark = lab_dom_mark();
        int radio = lab_dom_item(choices, i), name = lab_dom_read(radio, 5, "");
        int index = control_index(lab_dom_get(radio, "name"));
        if (index >= 0) {
            lab_dom_checked(radio, lab_dom_equal(name, value(controls[index])));
            lab_dom_disabled(radio, index == 1 && lab_dom_equal(name, first));
        }
        lab_dom_release(mark);
    }
    int range = lab_dom_id("plotRange");
    double dimensions = lab_dom_to_number(lab_dom_get(range, "marsPlotVariables"));
    int pde = (lab_workspace_mode() == 2 && dimensions == 2) || (lab_workspace_mode() == 1 && dimensions == 3);
    int kind = solver ? lab_dom_string(pde ? "surface" : "curve") : value("plotKind");
    if (solver && lab_dom_query(0, "#plotRendered [data-series]"))
        kind = lab_dom_string(lab_dom_query(0, "#plotRendered [data-surface]") ? "surface" : "curve");
    int curve = lab_dom_equal(kind, lab_dom_string("curve"));
    int surface = lab_dom_equal(kind, lab_dom_string("surface"));
    if (lab_dom_query(0, "#plotRendered [data-series]"))
        lab_dom_set(range, "marsPlotSurface", lab_dom_scalar(1, !!lab_dom_query(0, "#plotRendered [data-surface]")));
    if (!curve && !surface)
        surface = lab_dom_truth(lab_dom_get(range, "marsPlotSurface")) || lab_dom_length(second);
    int multiple = lab_dom_to_number(lab_dom_get(range, "marsPlotVariables")) > 1;
    int automatic = !lab_dom_equal(value("plotInputRange"), lab_dom_string("manual"));
    lab_dom_class(lab_dom_closest(lab_dom_id("plotFirstGroup"), ".plot-ranges"), "hidden", 0);
    /* A single-variable group has only a range heading and bounds; retain actual variable choices. */
    lab_dom_class(lab_dom_id("plotFirstGroup"), "hidden", automatic && !multiple);
    lab_dom_class(lab_dom_query(lab_dom_id("plotFirstGroup"), ".plot-bound-pair"), "hidden", automatic);
    lab_dom_class(lab_dom_query(lab_dom_id("plotSecondGroup"), ".plot-bound-pair"), "hidden", automatic);
    lab_dom_class(lab_dom_id("plotSecondGroup"), "hidden", complex || curve || (!surface && !multiple));
    lab_dom_class(lab_dom_id("plotCamera"), "hidden", !surface);
    lab_dom_class(lab_dom_id("plotVertical"), "hidden", complex);
    lab_dom_write(lab_dom_id("plotVerticalTitle"), 0, "",
                  lab_dom_string(surface ? "Height range" : "Vertical range"));
    lab_dom_class(lab_dom_id("plotManual"), "hidden", !lab_dom_equal(value("plotScale"), lab_dom_string("manual")));
}

static void choice(int group, const char *name, int key, int label)
{
    int wrapper = lab_dom_create("label"), radio = lab_dom_create("input"), text = lab_dom_create("span");
    lab_dom_write(radio, 2, "type", lab_dom_string("radio"));
    lab_dom_write(radio, 2, "name", lab_dom_string(name));
    lab_dom_write(radio, 5, "", key);
    lab_dom_write(text, 0, "", label);
    lab_dom_append(wrapper, radio);
    lab_dom_append(wrapper, text);
    lab_dom_append(group, wrapper);
}

/* Build accessible native radio groups, preserving selections only while their variables exist. */
void lab_plot_controls_variables(int names, int labels)
{
    lab_dom_set(lab_dom_id("plotRange"), "marsPlotNames", names);
    int groups[] = {lab_dom_id("plotFirstChoices"), lab_dom_id("plotSecondChoices")};
    int index = lab_dom_object(5);
    for (unsigned axis = 0; axis < 2; ++axis)
        lab_dom_write(groups[axis], 0, "", lab_dom_string(""));
    choice(groups[1], "plotSecond", lab_dom_string(""), lab_dom_string("Auto"));
    for (unsigned i = 0; i < lab_dom_count(names); ++i) {
        unsigned mark = lab_dom_mark();
        int name = lab_dom_item(names, i), label = lab_dom_item(labels, i);
        lab_dom_key_set(index, name, lab_dom_scalar(1, 1));
        choice(groups[0], "plotFirst", name,
               lab_dom_count(names) == 1 ? lab_dom_join(label, lab_dom_string(" range"), "") : label);
        choice(groups[1], "plotSecond", name, label);
        lab_dom_release(mark);
    }
    if (!lab_dom_truth(lab_dom_key_get(index, value("plotFirst"))))
        lab_dom_write(lab_dom_id("plotFirst"), 5, "", lab_dom_count(names) ? lab_dom_item(names, 0) : lab_dom_string(""));
    if (!lab_dom_truth(lab_dom_key_get(index, value("plotSecond"))))
        lab_dom_write(lab_dom_id("plotSecond"), 5, "", lab_dom_string(""));
    lab_dom_set(lab_dom_id("plotRange"), "marsPlotVariables", lab_dom_numeric(lab_dom_count(names)));
    lab_dom_subscribe(lab_dom_id("plotCard"), "change", "lab_plot_controls_dispatch", 0, 0, 0);
    lab_dom_subscribe(lab_dom_id("plotCard"), "input", "lab_plot_controls_dispatch", 1, 0, 0);
    lab_dom_subscribe(lab_dom_id("plotCard"), "click", "lab_plot_controls_dispatch", 3, 0, 0);
    lab_dom_subscribe(lab_dom_id("plotCard"), "pointerdown", "lab_plot_controls_dispatch", 4, 0, 0);
    lab_plot_controls_sync();
}

/* Automatic plotting and live redraws share the card's selected first-axis value. */
void lab_plot_controls_prepare(int variable)
{
    lab_dom_write(lab_dom_id("plotFirst"), 5, "", variable);
    lab_plot_controls_sync();
}

static int current_source(void)
{
    int length = lab_workspace_source_get(0);
    return length > 0 ? lab_dom_utf8_string(lab_workspace_output(), (unsigned)length) : lab_dom_string("");
}

static void emit(int calls, const char *service, int first, int second, unsigned count)
{
    int call = lab_dom_object(5), args = lab_dom_object(4);
    lab_dom_push(args, first);
    if (count == 2)
        lab_dom_push(args, second);
    lab_dom_set(call, "service", lab_dom_string(service));
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
}

/* Coalesce edits; never replay an old worksheet or interrupt a newer explicit request. */
void lab_plot_controls_dispatch(unsigned action, int event, int context)
{
    int plan = lab_dom_object(5), calls = lab_dom_object(4);
    lab_dom_set(plan, "calls", calls);
    lab_dom_return(plan);
    if (action == 2) {
        if (lab_dom_to_number(lab_dom_get(context, "mode")) != lab_workspace_mode() ||
            lab_dom_to_number(lab_dom_get(context, "request")) != lab_request_latest_main() ||
            !lab_dom_equal(lab_dom_get(context, "source"), current_source()) ||
            !lab_dom_query(0, ".plot-card #plotRange"))
            return;
        if (lab_dom_equal(value("plotScale"), lab_dom_string("manual")) &&
            (!lab_dom_length(lab_dom_clean(value("plotYLower"), 0)) ||
             !lab_dom_length(lab_dom_clean(value("plotYUpper"), 0))))
            return;
        emit(calls, "takePlot", value("plotFirst"), lab_dom_get(context, "slicePreview"), 2);
        return;
    }
    int target = lab_dom_get(event, "target");
    /* The camera may be docked outside plotRange; slice controls retain their own subscription. */
    if (lab_dom_equal(lab_dom_get(event, "currentTarget"), lab_dom_id("plotCard")) &&
        !lab_dom_closest(target, "#plotRange, #plotCamera"))
        return;
    int slice_edit = lab_plot_slices_edit(action, target);
    if (slice_edit == 2)
        return;
    if (slice_edit == 1) {
        int state = lab_dom_object(5);
        lab_dom_set(state, "mode", lab_dom_numeric(lab_workspace_mode()));
        lab_dom_set(state, "request", lab_dom_numeric(lab_request_latest_main()));
        lab_dom_set(state, "source", current_source());
        lab_dom_set(state, "slicePreview", lab_dom_scalar(1, action == 1));
        emit(calls, "schedulePlotSlice", state, lab_dom_numeric(action == 1 ? 40 : 0), 2);
        return;
    }
    if (lab_dom_closest(target, "#plotZoom")) {
        if (action == 0 || action == 1 || action == 4)
            emit(calls, "setSurfaceZoom", lab_dom_read(target, 5, ""), lab_dom_numeric(action), 2);
        return;
    }
    if (lab_dom_closest(target, ".plot-angle") && lab_dom_closest(target, "input")) {
        if (action == 0 || action == 1 || (action == 4 &&
            lab_dom_to_number(lab_dom_get(event, "button")) == 0 &&
            lab_dom_truth(lab_dom_get(event, "isPrimary"))))
            lab_plot_slider(action == 4 ? 0 : action == 1 ? 1 : 2, event, calls);
        return;
    }
    if (action == 4)
        return;
    if (action == 3 && ((!lab_dom_equal(lab_dom_get(target, "name"), lab_dom_string("plotScale")) &&
                        !lab_dom_equal(lab_dom_get(target, "name"), lab_dom_string("plotInputRange"))) ||
                       !lab_dom_equal(lab_dom_read(target, 5, ""), lab_dom_string("auto"))))
        return;
    if (!lab_dom_closest(target, "input") || (lab_workspace_mode() > 2 && lab_workspace_mode() != 4))
        return;
    /* Authored bounds take precedence over the exact coordinate behind a rounded zoom label. */
    lab_plot_wheel_stop();
    lab_dom_delete(target, "marsZoomText");
    lab_dom_delete(target, "marsZoomValue");
    /* A range or axis edit starts a new viewport; pending wheel bounds no longer apply. */
    lab_dom_delete(lab_dom_id("plotRange"), "marsZoomChart");
    for (unsigned i = 0; i < sizeof input_bounds / sizeof *input_bounds; ++i)
        if (lab_dom_equal(target, lab_dom_id(input_bounds[i])))
            lab_dom_write(lab_dom_id("plotInputRange"), 5, "", lab_dom_string("manual"));
    if (lab_dom_closest(target, ".plot-choices")) {
        int index = control_index(lab_dom_get(target, "name"));
        if (index >= 0)
            lab_dom_write(lab_dom_id(controls[index]), 5, "", lab_dom_read(target, 5, ""));
        if (index == 3 && lab_dom_equal(value("plotScale"), lab_dom_string("auto"))) {
            lab_dom_delete(lab_dom_id("plotRange"), "marsZoomOriginal");
            static const char *const bounds[] = {"plotYLower", "plotYUpper"};
            for (unsigned i = 0; i < sizeof bounds / sizeof *bounds; ++i) {
                int control = lab_dom_id(bounds[i]);
                lab_dom_write(control, 5, "", lab_dom_string(""));
                lab_dom_delete(control, "marsZoomText");
                lab_dom_delete(control, "marsZoomValue");
            }
        }
        if (index == 4 && lab_dom_equal(value("plotInputRange"), lab_dom_string("auto")))
            input_range_auto();
    }
    lab_plot_controls_sync();
    int state = lab_dom_object(5);
    lab_dom_set(state, "mode", lab_dom_numeric(lab_workspace_mode()));
    lab_dom_set(state, "request", lab_dom_numeric(lab_request_latest_main()));
    lab_dom_set(state, "source", current_source());
    emit(calls, "schedulePlotControls", state, lab_dom_numeric(action == 1 ? 350 : 100), 2);
}
