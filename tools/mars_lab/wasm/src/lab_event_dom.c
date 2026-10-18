/**
 * @file lab_event_dom.c
 * @brief Native worksheet event registration and dispatch for the MARS Lab browser.
 *
 * Owns control subscriptions, keyboard and calendar policy, card zoom and event
 * cancellation. Indexed handlers return ordered service calls for browser promises;
 * no host service runs until the scoped C call has ended. DOM nodes are borrowed
 * within a call; the host retains actual targets for installed listeners. Native
 * mathematical results are never parsed or rewritten here.
 */
#include <math.h>
#include <stdbool.h>

#include "lab_mode.h"
#include "lab_dom.h"
#include "lab_layout.h"
#include "lab_profile.h"
#include "lab_view.h"
#include "lab_workspace.h"
#include "lab_workspace_dom.h"
#include "lab_plot_drag.h"
#include "lab_plot_controls.h"
#include "lab_plot_pointer.h"
#include "lab_precision.h"
#include "lab_event_dom.h"

static void lab_event_emit(jsref calls, const char *service, jsref first, jsref second, unsigned count)
{
    int call = lab_dom_object(LAB_VALUE_OBJECT), args = lab_dom_object(LAB_VALUE_ARRAY);
    lab_dom_set(call, "service", lab_dom_string(service));
    if (count)
        lab_dom_push(args, first);
    if (count > 1)
        lab_dom_push(args, second);
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
}

static bool lab_event_key(event_jsref event, const char *key)
{
    return lab_dom_equal(lab_dom_get(event, "key"), lab_dom_string(key));
}

static bool lab_event_modifier(event_jsref event)
{
    return lab_dom_truth(lab_dom_get(event, "ctrlKey")) || lab_dom_truth(lab_dom_get(event, "metaKey"));
}

static void lab_event_prevent(jsref plan, bool stop)
{
    lab_dom_set(plan, "prevent", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
    if (stop)
        lab_dom_set(plan, "stop", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
}

static void lab_event_run(lab_event_action_t action, event_jsref event, jsref plan, jsref calls)
{
    (void)action;
    (void)event;
    (void)plan;
    if (lab_workspace_mode() <= LAB_MODE_DIFFERENTIAL_EQUATION || lab_workspace_mode() == LAB_MODE_INTEGRATOR) {
        lab_plot_controls_reset();
        lab_event_emit(calls, "resetPlotPreview", 0, 0, 0);
    }
    if (lab_workspace_mode() == LAB_MODE_EXPRESSION) {
        lab_event_emit(calls, "clearForwardHistory", 0, 0, 0);
        lab_event_emit(calls, "clearGoalSeekRequest", 0, 0, 0);
        lab_event_emit(calls, "hideTargetEntry", 0, 0, 0);
    }
    lab_event_emit(calls, "evaluateCurrentMode", 0, 0, 0);
}

static void lab_event_history(lab_event_action_t action, event_jsref event, jsref plan, jsref calls)
{
    (void)event;
    (void)plan;
    lab_event_emit(calls, "navigateHistoryFromEvent", lab_dom_numeric(action - LAB_EVENT_HISTORY_BACK), 0, 1);
}

static void lab_event_editor_key(lab_event_action_t action, event_jsref event, jsref plan, jsref calls)
{
    (void)action;
    if (lab_event_modifier(event) && lab_event_key(event, "Enter")) {
        lab_event_prevent(plan, 0);
        if (lab_workspace_mode() <= LAB_MODE_DIFFERENTIAL_EQUATION || lab_workspace_mode() == LAB_MODE_INTEGRATOR) {
            lab_plot_controls_reset();
            lab_event_emit(calls, "resetPlotPreview", 0, 0, 0);
        }
        const char *service = lab_workspace_mode() == LAB_MODE_EXPRESSION
                                  ? "evaluateFromKeyboard" : "evaluateCurrentMode";
        lab_event_emit(calls, service, 0, 0, 0);
    }
}

static void lab_event_tab(lab_event_action_t action, event_jsref event, jsref plan, jsref calls)
{
    (void)action;
    (void)plan;
    lab_event_emit(calls, "selectWorksheetMode",
                   lab_dom_read(lab_dom_get(event, "currentTarget"), LAB_DOM_DATASET, "mode"), 0, 1);
}

static void lab_event_control(lab_event_action_t action, event_jsref event, jsref plan, jsref calls)
{
    (void)event;
    (void)plan;
    lab_event_emit(calls, "normaliseWorksheetControl", lab_dom_numeric(action - LAB_EVENT_MATRIX_CONTROL), 0, 1);
}

static void lab_event_goal_key(lab_event_action_t action, event_jsref event, jsref plan, jsref calls)
{
    (void)action;
    if (lab_event_key(event, "Enter")) {
        lab_event_prevent(plan, 0);
        lab_event_emit(calls, "startGoalSeekFromEvent", 0, 0, 0);
    } else if (lab_event_key(event, "Escape")) {
        lab_event_prevent(plan, 0);
        lab_workspace_dom_target(0);
        lab_workspace_dom_status(lab_dom_string("Ready"));
        lab_dom_effect(lab_dom_query(0, "#expr"), LAB_DOM_EFFECT_FOCUS);
    }
}

static void lab_event_precision(lab_event_action_t action, event_jsref event, jsref plan, jsref calls)
{
    (void)plan;
    int slider = lab_dom_get(event, "currentTarget");
    if (!lab_dom_truth(slider) || lab_dom_truth(lab_dom_get(slider, "disabled")))
        return;
    unsigned bits = lab_workspace_precision_slider_bits(lab_dom_to_number(lab_dom_get(slider, "value")));
    lab_workspace_dom_precision_preview(bits);
    if (action == LAB_EVENT_PRECISION_CHANGE && bits != lab_workspace_requested_precision(lab_workspace_mode()))
        lab_event_emit(calls, "setWorksheetPrecision", lab_dom_numeric(bits), 0, 1);
}

static void lab_event_precision_track(lab_event_action_t action, event_jsref event, jsref plan, jsref calls)
{
    (void)action;
    int slider = lab_dom_get(event, "currentTarget");
    if (!lab_dom_truth(slider) || lab_dom_truth(lab_dom_get(slider, "disabled")) ||
        lab_dom_number(lab_dom_get(event, "button")) != 0)
        return;
    double width = lab_dom_measure(slider, LAB_DOM_MEASURE_RECT_WIDTH);
    if (width <= 16)
        return;
    double position = lab_dom_to_number(lab_dom_read(slider, LAB_DOM_VALUE, ""));
    double maximum = lab_dom_to_number(lab_dom_read(slider, LAB_DOM_ATTRIBUTE, "max"));
    if (!(maximum > 0))
        return;
    /* The shared range theme has a 16-pixel thumb including its border. */
    double centre = lab_dom_measure(slider, LAB_DOM_MEASURE_RECT_LEFT) + 8 + (width - 16) * position / maximum;
    double offset = lab_dom_number(lab_dom_get(event, "clientX")) - centre;
    if (offset >= -8 && offset <= 8)
        return; /* Preserve the browser's native thumb dragging. */
    lab_event_prevent(plan, 0);
    unsigned bits = lab_workspace_precision_slider_bits(position + (offset > 0 ? 1 : -1));
    lab_workspace_dom_precision_slider(bits);
    lab_dom_effect(slider, LAB_DOM_EFFECT_FOCUS);
    if (bits != lab_workspace_requested_precision(lab_workspace_mode()))
        lab_event_emit(calls, "setWorksheetPrecision", lab_dom_numeric(bits), 0, 1);
}

/* Select the result text through the bounded dispatcher's three digit actions. */
static void lab_event_digits(lab_event_action_t action, event_jsref event, jsref plan, jsref calls)
{
    (void)plan;
    static const char *const selectors[] = {
        [LAB_EVENT_EXPRESSION_DIGITS] = "#parsed",
        [LAB_EVENT_FUNCTION_DIGITS]   = "#functionStyle",
        [LAB_EVENT_VALUE_DIGITS]      = "#value",
    };
    lab_event_emit(calls, "toggleTextDigits", lab_dom_query(0, selectors[action]),
                   lab_dom_get(event, "currentTarget"), 2);
}

static void lab_event_copy(lab_event_action_t action, event_jsref event, jsref plan, jsref calls)
{
    (void)action;
    (void)plan;
    lab_event_emit(calls, "copyResultFromEvent", lab_dom_get(event, "currentTarget"), 0, 1);
}

static void lab_event_zoom(lab_event_action_t action, event_jsref event, jsref plan, jsref calls)
{
    (void)calls;
    double delta = 0;
    if (action == LAB_EVENT_ZOOM_WHEEL) {
        int area = lab_dom_closest(lab_dom_get(event, "target"), ".rendered");
        int card = lab_dom_closest(lab_dom_get(event, "currentTarget"), ".result-card");
        int plot = lab_dom_query(card, ".rendered svg [data-series]");
        /* Plots reserve Ctrl+wheel inside the image; other result cards retain Ctrl/Command zoom. */
        if (plot ? (!area || !lab_dom_truth(lab_dom_get(event, "ctrlKey"))) : !lab_event_modifier(event))
            return;
        delta = lab_dom_to_number(lab_dom_get(event, "deltaY"));
        if (!(delta < 0 || delta > 0))
            return;
    }
    lab_event_prevent(plan, action == LAB_EVENT_ZOOM_CLICK);
    int target = lab_dom_get(event, "currentTarget"), card = lab_dom_closest(target, ".result-card");
    if (!card)
        return;
    int reset = action == LAB_EVENT_ZOOM_CLICK && lab_dom_has(target, "data-zoom-reset");
    double direction = action == LAB_EVENT_ZOOM_WHEEL ? (delta < 0 ? 1 : -1)
                                    : lab_dom_to_number(lab_dom_read(target, LAB_DOM_DATASET, "zoomStep"));
    if (!direction)
        direction = 1;
    if (lab_plot_pointer_zoom(card, event, calls, reset, direction, action == LAB_EVENT_ZOOM_WHEEL))
        return;
    lab_layout_set_zoom(card, reset ? lab_view_zoom_index(NAN) : direction,
                        reset ? LAB_LAYOUT_ZOOM_ABSOLUTE : LAB_LAYOUT_ZOOM_STEP);
    if (action == LAB_EVENT_ZOOM_CLICK) {
        double percent = lab_view_zoom(lab_view_card_zoom(lab_dom_card_id(card))) * 100;
        lab_workspace_dom_status(lab_dom_format((unsigned)(percent + 0.5), "Zoom ", "%"));
    }
}

static void lab_event_visibility(lab_event_action_t action, event_jsref event, jsref plan, jsref calls)
{
    (void)action;
    (void)plan;
    if (lab_dom_equal(lab_dom_get(lab_dom_get(event, "currentTarget"), "visibilityState"), lab_dom_string("hidden")))
        lab_event_emit(calls, "flushWorksheetState", 0, 0, 0);
}

static void lab_event_resize(lab_event_action_t action, event_jsref event, jsref plan, jsref calls)
{
    (void)action;
    (void)event;
    (void)plan;
    int cards = lab_dom_all(0, ".result-card");
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < lab_dom_count(cards); ++i) {
        lab_layout_zoom(lab_dom_item(cards, i));
        lab_dom_release(mark);
    }
    lab_event_emit(calls, "scheduleWorkspacePanelFit", 0, 0, 0);
}

static void lab_event_expand(lab_event_action_t action, event_jsref event, jsref plan, jsref calls)
{
    (void)action;
    (void)plan;
    (void)calls;
    lab_layout_expand(lab_dom_closest(lab_dom_get(event, "currentTarget"), ".complex-panel, .result-card"),
                      LAB_LAYOUT_EXPANSION_TOGGLE);
}

static void lab_event_rotate(lab_event_action_t action, event_jsref event, jsref plan, jsref calls)
{
    (void)action;
    (void)plan;
    (void)calls;
    int control = lab_dom_get(event, "currentTarget");
    int output = lab_dom_query(lab_dom_closest(control, ".plot-angle"), "output");
    lab_dom_write(output, LAB_DOM_TEXT, "",
                  lab_dom_join(lab_dom_read(control, LAB_DOM_VALUE, ""), lab_dom_string("°"), ""));
}

static void lab_event_calendar(unsigned action, event_jsref event, jsref plan, jsref calls)
{
    lab_mode_t mode = (lab_mode_t)(action >> 8);
    unsigned field = action & 255;
    if ((mode != LAB_MODE_DATETIME && mode != LAB_MODE_ALMANAC) || field >= lab_profile_count(mode))
        return;
    int control = lab_dom_get(event, "currentTarget");
    unsigned flags = lab_profile_event(mode, field);
    int key = lab_dom_equal(lab_dom_get(event, "type"), lab_dom_string("keydown"));
    if (key) {
        if (!(flags & 1))
            return;
        int shell = lab_dom_closest(control, ".mars-date-shell");
        int button = shell ? lab_dom_query(shell, "[data-date-target]") : 0;
        if (!button)
            return;
        if (lab_event_key(event, "ArrowDown") || lab_event_key(event, "Enter")) {
            lab_event_prevent(plan, 0);
            lab_event_emit(calls, "openMarsDatePicker", control, button, 2);
        } else if (lab_event_key(event, "Escape")) {
            lab_event_emit(calls, "closeMarsDatePicker", 0, 0, 0);
        }
        return;
    }
    if (flags & 2) {
        int text = lab_dom_read(control, LAB_DOM_VALUE, "");
        if (lab_dom_length(text))
            lab_dom_write(lab_dom_query(0, "#datetimeYear"), LAB_DOM_VALUE, "", lab_dom_slice(text, 0, 4));
        lab_dom_write(lab_dom_query(0, "#datetimeJdn"), LAB_DOM_VALUE, "", lab_dom_string(""));
    }
    lab_event_emit(calls, "applyCalendarControlEvent",
                   lab_dom_string(mode == LAB_MODE_DATETIME ? "datetime" : "almanac"),
                   lab_dom_numeric(flags), 2);
}

/* Return deferred service calls after native synchronous policy has completed. */
void lab_events_dispatch(unsigned action, event_jsref event)
{
    /** @brief Worksheet action implementation receiving its event and deferred service plan. */
    typedef void (*lab_event_handler_t)(lab_event_action_t action, event_jsref event,
                                       jsref plan, jsref calls);
    static const lab_event_handler_t handlers[LAB_EVENT_COUNT] = {
        [LAB_EVENT_RUN] = lab_event_run,
        [LAB_EVENT_HISTORY_BACK] = lab_event_history,
        [LAB_EVENT_HISTORY_FORWARD] = lab_event_history,
        [LAB_EVENT_EDITOR_KEY] = lab_event_editor_key,
        [LAB_EVENT_TAB] = lab_event_tab,
        [LAB_EVENT_MATRIX_CONTROL] = lab_event_control,
        [LAB_EVENT_EQUATION_CONTROL] = lab_event_control,
        [LAB_EVENT_INTEGRATOR_CONTROL] = lab_event_control,
        [LAB_EVENT_GOAL_KEY] = lab_event_goal_key,
        [LAB_EVENT_PRECISION_CHANGE] = lab_event_precision,
        [LAB_EVENT_PRECISION_INPUT] = lab_event_precision,
        [LAB_EVENT_EXPRESSION_DIGITS] = lab_event_digits,
        [LAB_EVENT_FUNCTION_DIGITS] = lab_event_digits,
        [LAB_EVENT_VALUE_DIGITS] = lab_event_digits,
        [LAB_EVENT_RESULT_COPY] = lab_event_copy,
        [LAB_EVENT_ZOOM_CLICK] = lab_event_zoom,
        [LAB_EVENT_ZOOM_WHEEL] = lab_event_zoom,
        [LAB_EVENT_VISIBILITY] = lab_event_visibility,
        [LAB_EVENT_RESIZE] = lab_event_resize,
        [LAB_EVENT_EXPAND] = lab_event_expand,
        [LAB_EVENT_ROTATION_CHANGE] = lab_event_rotate,
        [LAB_EVENT_ROTATION_INPUT] = lab_event_rotate,
        [LAB_EVENT_PRECISION_TRACK] = lab_event_precision_track,
    };
    static const char *const services[LAB_EVENT_COUNT] = {
        [LAB_EVENT_EDITOR_INPUT] = "refreshWorksheetFromInput",
        [LAB_EVENT_CLEAR] = "clearWorksheetFromEvent",
        [LAB_EVENT_HELP] = "toggleHelp",
        [LAB_EVENT_GOAL] = "startGoalSeekFromEvent",
        [LAB_EVENT_ALMANAC_TIME] = "formatAlmanacTimeFromEvent",
        [LAB_EVENT_RENDERED_DIGITS] = "toggleRenderedDigits",
        [LAB_EVENT_FUNCTION_RUN] = "runFunctionCard",
        [LAB_EVENT_RESULT_INPUT] = "sendResultExpressionToInput",
        [LAB_EVENT_INPUT_COPY] = "copyInputFromEvent",
        [LAB_EVENT_PAGE_HIDE] = "flushWorksheetState",
        [LAB_EVENT_PANEL_FIT] = "scheduleWorkspacePanelFit",
        [LAB_EVENT_EDITOR_RESIZE] = "scheduleEditorResizeGrip",
    };
    int plan = lab_dom_object(LAB_VALUE_OBJECT), calls = lab_dom_object(LAB_VALUE_ARRAY);
    lab_dom_set(plan, "calls", calls);
    /* Calendar selectors carry a mode and a runtime profile-field index, not one enum value. */
    if (action >= 256)
        lab_event_calendar(action, event, plan, calls);
    else if (action < LAB_EVENT_COUNT) {
        if (handlers[action])
            handlers[action]((lab_event_action_t)action, event, plan, calls);
        else if (services[action])
            lab_event_emit(calls, services[action], 0, 0, 0);
    }
    lab_dom_return(plan);
}

/* Visit each configured control once; browser listeners retain nodes, never transient handles. */
void lab_events_install(jsref document, jsref window)
{
    static const struct {
        const char *selector, *type;
        lab_event_action_t action;
    } controls[] = {{"#run", "click", LAB_EVENT_RUN},
                    {"#back", "click", LAB_EVENT_HISTORY_BACK},
                    {"#forward", "click", LAB_EVENT_HISTORY_FORWARD},
                    {"#expr", "keydown", LAB_EVENT_EDITOR_KEY},
                    {"#expr", "input", LAB_EVENT_EDITOR_INPUT},
                    {"#clear", "click", LAB_EVENT_CLEAR},
                    {"#help", "click", LAB_EVENT_HELP},
                    {"#helpClose", "click", LAB_EVENT_HELP},
                    {"#goalSeek", "click", LAB_EVENT_GOAL},
                    {".mode-tab", "click", LAB_EVENT_TAB},
                    {"#matrixOperation", "change", LAB_EVENT_MATRIX_CONTROL},
                    {"#equationVariable", "change", LAB_EVENT_EQUATION_CONTROL},
                    {"#integratorIntervalCap", "change", LAB_EVENT_INTEGRATOR_CONTROL},
                    {"#almanacTime", "input", LAB_EVENT_ALMANAC_TIME},
                    {"#goalTarget", "keydown", LAB_EVENT_GOAL_KEY},
                    {"#precisionSlider", "change", LAB_EVENT_PRECISION_CHANGE},
                    {"#precisionSlider", "input", LAB_EVENT_PRECISION_INPUT},
                    {"#precisionSlider", "pointerdown", LAB_EVENT_PRECISION_TRACK},
                    {"#renderedMore", "click", LAB_EVENT_RENDERED_DIGITS},
                    {"#parsedMore", "click", LAB_EVENT_EXPRESSION_DIGITS},
                    {"#functionMore", "click", LAB_EVENT_FUNCTION_DIGITS},
                    {"#functionRun", "click", LAB_EVENT_FUNCTION_RUN},
                    {"#valueMore", "click", LAB_EVENT_VALUE_DIGITS},
                    {"#resultUseInput", "click", LAB_EVENT_RESULT_INPUT},
                    {"#inputCopy", "click", LAB_EVENT_INPUT_COPY},
                    {"[data-copy-target]", "click", LAB_EVENT_RESULT_COPY},
                    {"[data-zoom-step], [data-zoom-reset]", "click", LAB_EVENT_ZOOM_CLICK},
                    {".result-card", "wheel", LAB_EVENT_ZOOM_WHEEL},
                    {"textarea", "input", LAB_EVENT_EDITOR_RESIZE},
                    {"[data-expand-card]", "click", LAB_EVENT_EXPAND},
                    {"#plotAzimuth, #plotElevation, #plotRoll", "change", LAB_EVENT_ROTATION_CHANGE},
                    {"#plotAzimuth, #plotElevation, #plotRoll", "input", LAB_EVENT_ROTATION_INPUT}};
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < sizeof controls / sizeof *controls; ++i) {
        int nodes = lab_dom_all(0, controls[i].selector);
        unsigned node_mark = lab_dom_mark();
        for (unsigned n = 0; n < lab_dom_count(nodes); ++n) {
            lab_dom_listen(lab_dom_item(nodes, n), controls[i].type, controls[i].action, 0);
            lab_dom_release(node_mark);
        }
        lab_dom_release(mark);
    }
    for (lab_mode_t mode = LAB_MODE_DATETIME; mode <= LAB_MODE_ALMANAC; ++mode) {
        for (unsigned field = 0; field < lab_profile_count(mode); ++field) {
            const char *id = lab_profile_text(mode, field, LAB_PROFILE_ELEMENT);
            if (*id) {
                int control = lab_dom_id(id);
                if (lab_profile_event(mode, field) & 1)
                    lab_dom_listen(control, "keydown", (mode << 8) | field, 0);
                lab_dom_listen(control, "change", (mode << 8) | field, 0);
            }
            lab_dom_release(mark);
        }
    }
    lab_dom_listen(window, "pagehide", LAB_EVENT_PAGE_HIDE, 0);
    lab_dom_listen(document, "visibilitychange", LAB_EVENT_VISIBILITY, 0);
    lab_dom_listen(window, "resize", LAB_EVENT_RESIZE, 0);
    lab_dom_listen(window, "scroll", LAB_EVENT_PANEL_FIT, 1);
    lab_plot_drag_install(document, window);
    lab_plot_pointer_install();
    lab_precision_install(document);
    int cards = lab_dom_all(0, ".result-card");
    for (unsigned i = 0; i < lab_dom_count(cards); ++i)
        lab_layout_zoom(lab_dom_item(cards, i));
    lab_layout_expand(0, LAB_LAYOUT_EXPANSION_RENDER);
}
