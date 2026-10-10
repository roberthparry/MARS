/**
 * @file lab_events.c
 * @brief Worksheet event decisions and clear-control projection in C/WebAssembly.
 *
 * Direct mode tables select input-refresh, save and goal-seek actions. Browser
 * listeners deliver events and execute asynchronous services under native request
 * ownership; C retains no event objects and does not parse expression text.
 * Input refresh plans decode these actions into ordered host storage, timer and
 * request services. Reuse validity is projected only after its timer is cancelled.
 */
#include "lab_dom.h"
#include "lab_events.h"

enum {
    lab_events_calendar_editor = 1,
    lab_events_source = 2,
    lab_events_bindings = 4,
    lab_events_save = 8,
    lab_events_history = 16,
    lab_events_match = 32,
    lab_events_edit = 64
};

/* Input action masks preserve per-mode refresh and source-reuse semantics. */
unsigned lab_events_input(unsigned mode, int bound, int unchanged)
{
    static const unsigned actions[] = {[0] = lab_events_save,
                                       [1] = lab_events_bindings | lab_events_save | lab_events_history,
                                       [2] = lab_events_source | lab_events_history,
                                       [3] = lab_events_bindings | lab_events_history,
                                       [4] = lab_events_bindings | lab_events_history,
                                       [5] = lab_events_calendar_editor | lab_events_history,
                                       [6] = lab_events_history};
    if (mode > 6)
        return 0;
    unsigned result = actions[mode];
    if (!mode)
        return result | (unchanged ? lab_events_match | lab_events_history : lab_events_edit);
    if (!bound && (mode == 1 || mode == 3 || mode == 4))
        result |= lab_events_source;
    return result;
}

static void lab_events_emit(int calls, const char *service, unsigned count, int first, int second)
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

/* Decode input policy into ordered adapters without mutating getter-backed host views. */
void lab_events_refresh(unsigned mode, int bound, int unchanged, int context)
{
    int plan = lab_dom_object(5), calls = lab_dom_object(4);
    lab_dom_set(plan, "calls", calls);
    lab_dom_return(plan);
    unsigned actions = lab_events_input(mode, bound, unchanged);
    if (!actions)
        return;
    static const char *const modes[] = {"expression", "equation", "diffequation", "matrix",
                                        "integrator", "datetime", "almanac"};
    int name = lab_dom_string(modes[mode]), editor = lab_dom_get(context, "editor");
    int text = lab_dom_clean(lab_dom_read(editor, 5, ""), 0);
    if (actions & lab_events_calendar_editor)
        lab_events_emit(calls, "setModeEditor", 2, name,
                        lab_dom_length(text) ? text : lab_dom_get(context, "defaultDatetimeText"));
    if (actions & lab_events_source) {
        lab_events_emit(calls, "setSource", 2, lab_dom_string("fullText"), text);
        lab_events_emit(calls, "setSource", 2, lab_dom_string("displayText"), text);
    }
    if (actions & lab_events_bindings)
        lab_events_emit(calls, "refreshVariableValuesFromEditor", 0, 0, 0);
    if (actions & lab_events_save) {
        int save = lab_dom_object(5), options = lab_dom_object(5);
        lab_dom_set(options, "debounce", lab_dom_scalar(1, 1));
        lab_dom_set(save, "options", options);
        /* Omit equation text so its adapter uses the live save-function default after binding refresh. */
        if (!mode)
            lab_dom_set(save, "text", text);
        lab_events_emit(calls, "saveState", 2, name, save);
    }
    if (actions & lab_events_match) {
        lab_events_emit(calls, "cancelBindingRefresh", 0, 0, 0);
        lab_events_emit(calls, "markBindingRefresh", 1, editor, 0);
    }
    if (actions & lab_events_edit) {
        lab_events_emit(calls, "clearGoalSeekRequest", 0, 0, 0);
        lab_events_emit(calls, "setSource", 2, lab_dom_string("lastInput"), lab_dom_string(""));
        lab_events_emit(calls, "scheduleEditedExpressionBindingRefresh", 0, 0, 0);
    }
    if (actions & lab_events_history)
        lab_events_emit(calls, "updateHistoryButtons", 0, 0, 0);
}

/* Mark reused binding metadata only when the host has cancelled its pending refresh timer. */
void lab_events_refresh_mark(int editor)
{
    lab_dom_write(editor, 3, "bindingRefreshValid", lab_dom_string("true"));
}

static void lab_events_value(const char *selector, int text)
{
    lab_dom_write(lab_dom_query(0, selector), 5, "", text);
}

/* Reset scalar controls; structured row/calendar reset is an explicit caller action. */
unsigned lab_events_clear(unsigned mode, int config)
{
    if (mode > 6)
        return 0;
    int empty = lab_dom_string("");
    lab_events_value("#expr", empty);
    if (mode == 1)
        lab_events_value("#equationVariable", lab_dom_get(config, "DEFAULT_EQUATION_VARIABLE"));
    else if (mode == 3) {
        lab_events_value("#matrixOperand", empty);
        lab_events_value("#matrixOperation", lab_dom_string("eval"));
    } else if (mode == 4) {
        lab_events_value("#integratorIntervalCap", lab_dom_get(config, "DEFAULT_INTEGRATOR_INTERVAL_CAP"));
        return 1;
    } else if (mode >= 5) {
        lab_events_value("#expr", lab_dom_get(config, mode == 5 ? "DEFAULT_DATETIME_TEXT" : "DEFAULT_ALMANAC_TEXT"));
        return 2;
    }
    return 0;
}

/* Only expression/equation flush editor saves when the page becomes inactive. */
int lab_events_flush(unsigned mode)
{
    return mode < 2;
}

/* Mathematical precision changes can rerun goal seek, reuse expression input or reevaluate another mode. */
unsigned lab_events_precision(unsigned mode, int goal_source, int goal_target)
{
    return !mode ? (goal_source && goal_target ? 1u : 2u) : mode < 5 ? 3u : 0u;
}
