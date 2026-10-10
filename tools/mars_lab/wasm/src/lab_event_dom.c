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
#include "lab_dom.h"
#include "lab_layout.h"
#include "lab_profile.h"
#include "lab_view.h"
#include "lab_workspace_dom.h"
#include "lab_workspace.h"
#include "lab_event_dom.h"

static void lab_event_emit(int calls, const char *service, int first, int second, unsigned count)
{
    int call = lab_dom_object(5), args = lab_dom_object(4);
    lab_dom_set(call, "service", lab_dom_string(service));
    if (count)
        lab_dom_push(args, first);
    if (count > 1)
        lab_dom_push(args, second);
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
}

static int lab_event_key(int event, const char *key)
{
    return lab_dom_equal(lab_dom_get(event, "key"), lab_dom_string(key));
}

static int lab_event_modifier(int event)
{
    return lab_dom_truth(lab_dom_get(event, "ctrlKey")) || lab_dom_truth(lab_dom_get(event, "metaKey"));
}

static void lab_event_prevent(int plan, int stop)
{
    lab_dom_set(plan, "prevent", lab_dom_scalar(1, 1));
    if (stop)
        lab_dom_set(plan, "stop", lab_dom_scalar(1, 1));
}

static void lab_event_run(unsigned action, int event, int plan, int calls)
{
    (void)action;
    (void)event;
    (void)plan;
    if (!lab_workspace_mode()) {
        lab_event_emit(calls, "clearForwardHistory", 0, 0, 0);
        lab_event_emit(calls, "clearGoalSeekRequest", 0, 0, 0);
        lab_event_emit(calls, "hideTargetEntry", 0, 0, 0);
    }
    lab_event_emit(calls, "evaluateCurrentMode", 0, 0, 0);
}

static void lab_event_history(unsigned action, int event, int plan, int calls)
{
    (void)event;
    (void)plan;
    lab_event_emit(calls, "navigateHistoryFromEvent", lab_dom_numeric(action - 1), 0, 1);
}

static void lab_event_editor_key(unsigned action, int event, int plan, int calls)
{
    (void)action;
    if (lab_event_modifier(event) && lab_event_key(event, "Enter")) {
        lab_event_prevent(plan, 0);
        lab_event_emit(calls, lab_workspace_mode() ? "evaluateCurrentMode" : "evaluateFromKeyboard", 0, 0, 0);
    }
}

static void lab_event_tab(unsigned action, int event, int plan, int calls)
{
    (void)action;
    (void)plan;
    lab_event_emit(calls, "selectWorksheetMode", lab_dom_read(lab_dom_get(event, "currentTarget"), 3, "mode"), 0, 1);
}

static void lab_event_control(unsigned action, int event, int plan, int calls)
{
    (void)event;
    (void)plan;
    lab_event_emit(calls, "normaliseWorksheetControl", lab_dom_numeric(action - 9), 0, 1);
}

static void lab_event_goal_key(unsigned action, int event, int plan, int calls)
{
    (void)action;
    if (lab_event_key(event, "Enter")) {
        lab_event_prevent(plan, 0);
        lab_event_emit(calls, "startGoalSeekFromEvent", 0, 0, 0);
    } else if (lab_event_key(event, "Escape")) {
        lab_event_prevent(plan, 0);
        lab_workspace_dom_target(0);
        lab_workspace_dom_status(lab_dom_string("Ready"));
        lab_dom_effect(lab_dom_query(0, "#expr"), 0);
    }
}

static void lab_event_precision(unsigned action, int event, int plan, int calls)
{
    (void)event;
    (void)plan;
    lab_event_emit(calls, "changeWorksheetPrecision", lab_dom_numeric(action == 14 ? 1 : -1), 0, 1);
}

static void lab_event_digits(unsigned action, int event, int plan, int calls)
{
    (void)plan;
    const char *selector = action == 17 ? "#parsed" : action == 18 ? "#functionStyle" : "#value";
    lab_event_emit(calls, "toggleTextDigits", lab_dom_query(0, selector), lab_dom_get(event, "currentTarget"), 2);
}

static void lab_event_copy(unsigned action, int event, int plan, int calls)
{
    (void)action;
    (void)plan;
    lab_event_emit(calls, "copyResultFromEvent", lab_dom_get(event, "currentTarget"), 0, 1);
}

static void lab_event_zoom(unsigned action, int event, int plan, int calls)
{
    (void)calls;
    if (action == 25 && !lab_event_modifier(event))
        return;
    lab_event_prevent(plan, action == 24);
    int target = lab_dom_get(event, "currentTarget"), card = lab_dom_closest(target, ".result-card");
    if (!card)
        return;
    int reset = action == 24 && lab_dom_has(target, "data-zoom-reset");
    double direction = action == 25 ? (lab_dom_to_number(lab_dom_get(event, "deltaY")) < 0 ? 1 : -1)
                                    : lab_dom_to_number(lab_dom_read(target, 3, "zoomStep"));
    if (!direction)
        direction = 1;
    lab_layout_set_zoom(card, reset ? lab_view_zoom_index(__builtin_nan("")) : direction, !reset);
    if (action == 24) {
        double percent = lab_view_zoom(lab_view_card_zoom(lab_dom_card_id(card))) * 100;
        lab_workspace_dom_status(lab_dom_format((unsigned)(percent + 0.5), "Zoom ", "%"));
    }
}

static void lab_event_visibility(unsigned action, int event, int plan, int calls)
{
    (void)action;
    (void)plan;
    if (lab_dom_equal(lab_dom_get(lab_dom_get(event, "currentTarget"), "visibilityState"), lab_dom_string("hidden")))
        lab_event_emit(calls, "flushWorksheetState", 0, 0, 0);
}

static void lab_event_resize(unsigned action, int event, int plan, int calls)
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

static void lab_event_expand(unsigned action, int event, int plan, int calls)
{
    (void)action;
    (void)plan;
    (void)calls;
    lab_layout_expand(lab_dom_closest(lab_dom_get(event, "currentTarget"), ".result-card"), 1);
}

static void lab_event_calendar(unsigned action, int event, int plan, int calls)
{
    unsigned mode = action >> 8, field = action & 255;
    if ((mode != 5 && mode != 6) || field >= lab_profile_count(mode))
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
        int text = lab_dom_read(control, 5, "");
        if (lab_dom_length(text))
            lab_dom_write(lab_dom_query(0, "#datetimeYear"), 5, "", lab_dom_slice(text, 0, 4));
        lab_dom_write(lab_dom_query(0, "#datetimeJdn"), 5, "", lab_dom_string(""));
    }
    lab_event_emit(calls, "applyCalendarControlEvent", lab_dom_string(mode == 5 ? "datetime" : "almanac"),
                   lab_dom_numeric(flags), 2);
}

/* Return deferred service calls after native synchronous policy has completed. */
void lab_events_dispatch(unsigned action, int event)
{
    typedef void (*lab_event_handler_t)(unsigned, int, int, int);
    static const lab_event_handler_t handlers[32] = {
        [0] = lab_event_run,
        [1] = lab_event_history,
        [2] = lab_event_history,
        [3] = lab_event_editor_key,
        [8] = lab_event_tab,
        [9] = lab_event_control,
        [10] = lab_event_control,
        [11] = lab_event_control,
        [13] = lab_event_goal_key,
        [14] = lab_event_precision,
        [15] = lab_event_precision,
        [17] = lab_event_digits,
        [18] = lab_event_digits,
        [20] = lab_event_digits,
        [23] = lab_event_copy,
        [24] = lab_event_zoom,
        [25] = lab_event_zoom,
        [27] = lab_event_visibility,
        [28] = lab_event_resize,
        [31] = lab_event_expand,
    };
    static const char *const services[32] = {
        [4] = "refreshWorksheetFromInput",
        [5] = "clearWorksheetFromEvent",
        [6] = "toggleHelp",
        [7] = "startGoalSeekFromEvent",
        [12] = "formatAlmanacTimeFromEvent",
        [16] = "toggleRenderedDigits",
        [19] = "runFunctionCard",
        [21] = "sendResultExpressionToInput",
        [22] = "copyInputFromEvent",
        [26] = "flushWorksheetState",
        [29] = "scheduleWorkspacePanelFit",
        [30] = "scheduleEditorResizeGrip",
    };
    int plan = lab_dom_object(5), calls = lab_dom_object(4);
    lab_dom_set(plan, "calls", calls);
    if (action >= 256)
        lab_event_calendar(action, event, plan, calls);
    else if (action < 32) {
        if (handlers[action])
            handlers[action](action, event, plan, calls);
        else if (services[action])
            lab_event_emit(calls, services[action], 0, 0, 0);
    }
    lab_dom_return(plan);
}

/* Visit each configured control once; browser listeners retain nodes, never transient handles. */
void lab_events_install(int document, int window)
{
    static const struct {
        const char *selector, *type;
        unsigned action;
    } controls[] = {{"#run", "click", 0},
                    {"#back", "click", 1},
                    {"#forward", "click", 2},
                    {"#expr", "keydown", 3},
                    {"#expr", "input", 4},
                    {"#clear", "click", 5},
                    {"#help", "click", 6},
                    {"#goalSeek", "click", 7},
                    {".mode-tab", "click", 8},
                    {"#matrixOperation", "change", 9},
                    {"#equationVariable", "change", 10},
                    {"#integratorIntervalCap", "change", 11},
                    {"#almanacTime", "input", 12},
                    {"#goalTarget", "keydown", 13},
                    {"#morePrecision", "click", 14},
                    {"#lessPrecision", "click", 15},
                    {"#renderedMore", "click", 16},
                    {"#parsedMore", "click", 17},
                    {"#functionMore", "click", 18},
                    {"#functionRun", "click", 19},
                    {"#valueMore", "click", 20},
                    {"#resultUseInput", "click", 21},
                    {"#inputCopy", "click", 22},
                    {"[data-copy-target]", "click", 23},
                    {"[data-zoom-step], [data-zoom-reset]", "click", 24},
                    {".result-card", "wheel", 25},
                    {"textarea", "input", 30},
                    {"[data-expand-card]", "click", 31}};
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
    for (unsigned mode = 5; mode <= 6; ++mode) {
        for (unsigned field = 0; field < lab_profile_count(mode); ++field) {
            const char *id = lab_profile_text(mode, field, 1);
            if (*id) {
                int control = lab_dom_id(id);
                if (lab_profile_event(mode, field) & 1)
                    lab_dom_listen(control, "keydown", (mode << 8) | field, 0);
                lab_dom_listen(control, "change", (mode << 8) | field, 0);
            }
            lab_dom_release(mark);
        }
    }
    lab_dom_listen(window, "pagehide", 26, 0);
    lab_dom_listen(document, "visibilitychange", 27, 0);
    lab_dom_listen(window, "resize", 28, 0);
    lab_dom_listen(window, "scroll", 29, 1);
    int cards = lab_dom_all(0, ".result-card");
    for (unsigned i = 0; i < lab_dom_count(cards); ++i)
        lab_layout_zoom(lab_dom_item(cards, i));
    lab_layout_expand(0, 0);
}
