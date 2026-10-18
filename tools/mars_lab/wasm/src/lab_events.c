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
#include "lab_mode.h"
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
unsigned lab_events_input(lab_mode_t mode, bool bound, bool unchanged)
{
    static const unsigned actions[] = {
        [LAB_MODE_EXPRESSION]            = lab_events_save,
        [LAB_MODE_EQUATION]              = lab_events_bindings | lab_events_save | lab_events_history,
        [LAB_MODE_DIFFERENTIAL_EQUATION] = lab_events_source | lab_events_history,
        [LAB_MODE_MATRIX]                = lab_events_bindings | lab_events_history,
        [LAB_MODE_INTEGRATOR]            = lab_events_bindings | lab_events_history,
        [LAB_MODE_DATETIME]              = lab_events_calendar_editor | lab_events_history,
        [LAB_MODE_ALMANAC]               = lab_events_history,
    };
    /** @brief Additional source refreshes needed when a mode has no bound editor. */
    static const unsigned unbound_actions[LAB_MODE_COUNT] = {
        [LAB_MODE_EQUATION]   = lab_events_source,
        [LAB_MODE_MATRIX]     = lab_events_source,
        [LAB_MODE_INTEGRATOR] = lab_events_source,
    };
    if ((unsigned)mode > LAB_MODE_ALMANAC)
        return 0;
    unsigned result = actions[mode];
    if (mode == LAB_MODE_EXPRESSION)
        return result | (unchanged ? lab_events_match | lab_events_history : lab_events_edit);
    if (!bound)
        result |= unbound_actions[mode];
    return result;
}

static void lab_events_emit(jsref calls, const char *service, unsigned count, jsref first,
                            jsref second)
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

/* Decode input policy into ordered adapters without mutating getter-backed host views. */
void lab_events_refresh(lab_mode_t mode, bool bound, bool unchanged, jsref context)
{
    int plan = lab_dom_object(LAB_VALUE_OBJECT), calls = lab_dom_object(LAB_VALUE_ARRAY);
    lab_dom_set(plan, "calls", calls);
    lab_dom_return(plan);
    unsigned actions = lab_events_input(mode, bound, unchanged);
    if (!actions)
        return;
    static const char *const modes[] = {"expression", "equation", "diffequation", "matrix",
                                        "integrator", "datetime", "almanac"};
    int name = lab_dom_string(modes[mode]), editor = lab_dom_get(context, "editor");
    int text = lab_dom_clean(lab_dom_read(editor, LAB_DOM_VALUE, ""), LAB_DOM_CLEAN_TRIM);
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
        int save = lab_dom_object(LAB_VALUE_OBJECT), options = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(options, "debounce", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
        lab_dom_set(save, "options", options);
        /* Omit equation text so its adapter uses the live save-function default after binding refresh. */
        if (mode == LAB_MODE_EXPRESSION)
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
void lab_events_refresh_mark(jsref editor)
{
    lab_dom_write(editor, LAB_DOM_DATASET, "bindingRefreshValid", lab_dom_string("true"));
}

static void lab_events_value(const char *selector, jsref text)
{
    lab_dom_write(lab_dom_query(0, selector), LAB_DOM_VALUE, "", text);
}

/* Reset scalar controls; structured row/calendar reset is an explicit caller action. */
lab_event_clear_action_t lab_events_clear(lab_mode_t mode, jsref config)
{
    /** @brief Scalar reset targets and optional configured defaults. */
    static const struct {
        const char *selector, *initial;
    } controls[LAB_MODE_COUNT] = {
        [LAB_MODE_EQUATION]   = {"#equationVariable", "DEFAULT_EQUATION_VARIABLE"},
        [LAB_MODE_MATRIX]     = {"#matrixOperand", 0},
        [LAB_MODE_INTEGRATOR] = {"#integratorIntervalCap", "DEFAULT_INTEGRATOR_INTERVAL_CAP"},
        [LAB_MODE_DATETIME]   = {"#expr", "DEFAULT_DATETIME_TEXT"},
        [LAB_MODE_ALMANAC]    = {"#expr", "DEFAULT_ALMANAC_TEXT"},
    };
    /** @brief Structured follow-up work after scalar controls have been reset. */
    static const lab_event_clear_action_t actions[LAB_MODE_COUNT] = {
        [LAB_MODE_INTEGRATOR] = LAB_EVENT_CLEAR_INTEGRATOR,
        [LAB_MODE_DATETIME]   = LAB_EVENT_CLEAR_CALENDAR,
        [LAB_MODE_ALMANAC]    = LAB_EVENT_CLEAR_CALENDAR,
    };
    if ((unsigned)mode > LAB_MODE_ALMANAC)
        return LAB_EVENT_CLEAR_NONE;
    int empty = lab_dom_string("");
    lab_events_value("#expr", empty);
    if (controls[mode].selector) {
        int value = controls[mode].initial ? lab_dom_get(config, controls[mode].initial) : empty;
        lab_events_value(controls[mode].selector, value);
    }
    /* Reset the operand before changing the operation selector. */
    if (mode == LAB_MODE_MATRIX)
        lab_events_value("#matrixOperation", lab_dom_string("eval"));
    return actions[mode];
}

/* Only expression/equation flush editor saves when the page becomes inactive. */
bool lab_events_flush(lab_mode_t mode)
{
    return (unsigned)mode < LAB_MODE_DIFFERENTIAL_EQUATION;
}

/* Mathematical precision changes can rerun goal seek, reuse expression input or reevaluate another mode. */
lab_event_precision_action_t lab_events_precision(lab_mode_t mode, bool goal_source, bool goal_target)
{
    if (mode == LAB_MODE_EXPRESSION)
        return goal_source && goal_target ? LAB_EVENT_PRECISION_GOAL : LAB_EVENT_PRECISION_REUSE;
    return (unsigned)mode < LAB_MODE_DATETIME ? LAB_EVENT_PRECISION_EVALUATE : LAB_EVENT_PRECISION_NONE;
}
