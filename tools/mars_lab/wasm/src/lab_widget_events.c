/**
 * @file lab_widget_events.c
 * @brief Date-picker and tooltip listener policy for MARS Lab WebAssembly.
 *
 * Owns opener toggles, calendar navigation and commit decisions, pointer boundary
 * checks and focus dismissal. Native date arithmetic remains in lab_forms; browser
 * services provide clock readings and form-control conversion. The event bridge
 * retains browser values, not scoped handles, and invokes services after dispatch.
 * No date grammar or mathematical expression is parsed by this module.
 */
#include "lab_dom.h"
#include "lab_forms.h"
#include "lab_widget_events.h"

static void lab_widget_events_emit(jsref calls, const char *service, unsigned count, jsref first,
                                jsref second, jsref third)
{
    int call = lab_dom_object(LAB_VALUE_OBJECT), args = lab_dom_object(LAB_VALUE_ARRAY);
    const int values[] = {first, second, third};
    for (unsigned i = 0; i < count; ++i)
        lab_dom_push(args, values[i]);
    lab_dom_set(call, "service", lab_dom_string(service));
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
}

static int lab_widget_events_visible(void)
{
    int picker = lab_dom_id("marsDatePicker");
    return picker && !lab_dom_has_class(picker, "hidden");
}

static void lab_widget_events_close(jsref calls, int focus)
{
    int options = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(options, "restoreFocus", lab_dom_scalar(LAB_VALUE_BOOLEAN, focus));
    lab_widget_events_emit(calls, "closeMarsDatePicker", 1, options, 0, 0);
}

/* Toggle the picker attached to the authored target. */
static void lab_widget_events_open(lab_widget_event_action_t action, event_jsref event, jsref state,
                                jsref plan, jsref calls)
{
    (void)action;
    (void)plan;
    int button = lab_dom_get(event, "currentTarget");
    int id = lab_dom_clean(lab_dom_read(button, LAB_DOM_DATASET, "dateTarget"), LAB_DOM_CLEAN_TRIM);
    /* A DOM ID lookup accepts the authored identifier as data, not as a CSS selector. */
    int input = lab_dom_value_id(id);
    if (!input)
        return;
    if (lab_dom_equal(lab_dom_get(state, "input"), input) && lab_widget_events_visible())
        lab_widget_events_close(calls, 1);
    else
        lab_widget_events_emit(calls, "openMarsDatePicker", 2, input, button, 0);
}

/* Commit an enabled day before restoring focus. */
static void lab_widget_events_grid(lab_widget_event_action_t action, event_jsref event, jsref state,
                                jsref plan, jsref calls)
{
    (void)action;
    (void)plan;
    int grid = lab_dom_id("marsDatePickerGrid");
    int button = lab_dom_closest(lab_dom_get(event, "target"), "button[data-iso-date]");
    if (!button || lab_dom_is_disabled(button) || !lab_dom_node_contains(grid, button))
        return;
    lab_widget_events_emit(calls, "commitMarsDateValue", 2, lab_dom_get(state, "input"),
                           lab_dom_read(button, LAB_DOM_DATASET, "isoDate"), 0);
    lab_widget_events_close(calls, 1);
}

/* Select the navigation service and direction before applying year modifiers. */
static void lab_widget_events_navigation(lab_widget_event_action_t action, event_jsref event, jsref state,
                                jsref plan, jsref calls)
{
    (void)state;
    (void)plan;
    static const struct {
        const char *service;
        int direction, years;
    } navigation[] = {
        [LAB_WIDGET_EVENT_PREVIOUS_MONTH] = {"shiftMarsDatePickerMonth", -1, 0},
        [LAB_WIDGET_EVENT_NEXT_MONTH]     = {"shiftMarsDatePickerMonth",  1, 0},
        [LAB_WIDGET_EVENT_PREVIOUS_YEAR]  = {"shiftMarsDatePickerYear",  -1, 1},
        [LAB_WIDGET_EVENT_NEXT_YEAR]      = {"shiftMarsDatePickerYear",   1, 1},
    };
    /* Only the four navigation entries in the bounded dispatcher reach this table. */
    int direction = navigation[action].direction;
    if (navigation[action].years)
        direction *= lab_forms_year_step(lab_dom_truth(lab_dom_get(event, "ctrlKey")),
                                         lab_dom_truth(lab_dom_get(event, "shiftKey")));
    lab_widget_events_emit(calls, navigation[action].service, 1,
                           lab_dom_numeric(direction), 0, 0);
}

/* Validate year keystrokes before reading and committing the edited date fields. */
static void lab_widget_events_date(lab_widget_event_action_t action, event_jsref event, jsref state,
                                jsref plan, jsref calls)
{
    if (action == LAB_WIDGET_EVENT_YEAR_KEY) {
        if (!lab_dom_equal(lab_dom_get(event, "key"), lab_dom_string("Enter")))
            return;
        lab_dom_set(plan, "prevent", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
    }
    int year = action == LAB_WIDGET_EVENT_MONTH ? lab_dom_get(state, "year")
                                               : lab_dom_read(lab_dom_id("marsDatePickerYear"), LAB_DOM_VALUE, "");
    int month = action == LAB_WIDGET_EVENT_MONTH ? lab_dom_read(lab_dom_id("marsDatePickerMonth"), LAB_DOM_VALUE, "")
                                                : lab_dom_get(state, "month");
    int options = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(options, "commit", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
    lab_widget_events_emit(calls, "setMarsDatePickerMonthYear", 3, year, month, options);
}

/* Dismiss a visible picker only when the pointer is outside its controls. */
static void lab_widget_events_outside(lab_widget_event_action_t action, event_jsref event, jsref state,
                                jsref plan, jsref calls)
{
    (void)action;
    (void)state;
    (void)plan;
    int target = lab_dom_get(event, "target"), picker = lab_dom_id("marsDatePicker");
    if (!lab_widget_events_visible() || lab_dom_node_contains(picker, target) ||
        lab_dom_closest(target, ".mars-date-shell"))
        return;
    lab_widget_events_close(calls, 0);
}

/* Validate pointer boundaries and Escape before emitting the selected tooltip service. */
static void lab_widget_events_tooltip(lab_widget_event_action_t action, event_jsref event, jsref state,
                                jsref plan, jsref calls)
{
    (void)state;
    (void)plan;
    static const struct {
        const char *service;
        unsigned count;
        int pointer, escape;
    } tooltips[] = {
        [LAB_WIDGET_EVENT_TOOLTIP_OVER]   = {"showButtonTooltip", 1, 1, 0},
        [LAB_WIDGET_EVENT_TOOLTIP_OUT]    = {"hideButtonTooltip", 0, 1, 0},
        [LAB_WIDGET_EVENT_TOOLTIP_FOCUS]  = {"showButtonTooltip", 1, 0, 0},
        [LAB_WIDGET_EVENT_TOOLTIP_HIDE]   = {"hideButtonTooltip", 0, 0, 0},
        [LAB_WIDGET_EVENT_TOOLTIP_ESCAPE] = {"hideButtonTooltip", 0, 0, 1},
    };
    int button = lab_dom_closest(lab_dom_get(event, "target"), "button");
    /* Only tooltip entries in the bounded dispatcher reach this table. */
    if (tooltips[action].pointer) {
        if (!button || lab_dom_node_contains(button, lab_dom_get(event, "relatedTarget")))
            return;
    } else if (tooltips[action].escape &&
               !lab_dom_equal(lab_dom_get(event, "key"), lab_dom_string("Escape"))) {
        return;
    }
    lab_widget_events_emit(calls, tooltips[action].service, tooltips[action].count, button, 0, 0);
}

/* Only the worksheet Location flag opens the detailed artwork viewer. */
static void lab_widget_events_flag(lab_widget_event_action_t action, event_jsref event, jsref state,
                                   jsref plan, jsref calls)
{
    (void)action;
    (void)state;
    int button = lab_dom_closest(lab_dom_get(event, "target"), ".almanac-summary .almanac-location-flag");
    if (!button) return;
    lab_dom_set(plan, "prevent", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
    lab_widget_events_emit(calls, "openFlagViewer", 1, button, 0, 0);
}

/* Alt affects artwork only; losing window focus must not leave the override latched. */
static void lab_widget_events_flag_modifier(lab_widget_event_action_t action, event_jsref event, jsref state,
                                           jsref plan, jsref calls)
{
    (void)state;
    (void)plan;
    if (action != LAB_WIDGET_EVENT_FLAG_RESET && !lab_dom_equal(lab_dom_get(event, "key"), lab_dom_string("Alt")))
        return;
    int held = action == LAB_WIDGET_EVENT_FLAG_RESET ? lab_dom_scalar(LAB_VALUE_BOOLEAN, 0) :
                                                     lab_dom_get(event, "altKey");
    lab_widget_events_emit(calls, "jurisdictionFlagModifier", 1, held, 0, 0);
}

/* Commit today's value only when an input is attached, then restore focus. */
static void lab_widget_events_today(lab_widget_event_action_t action, event_jsref event, jsref state,
                                jsref plan, jsref calls)
{
    (void)action;
    (void)event;
    (void)plan;
    int input = lab_dom_get(state, "input");
    if (input) {
        lab_widget_events_emit(calls, "commitMarsTodayValue", 1, input, 0, 0);
        lab_widget_events_close(calls, 1);
    }
}

/* Explicit close actions restore focus to the opener. */
static void lab_widget_events_dismiss(lab_widget_event_action_t action, event_jsref event, jsref state,
                                jsref plan, jsref calls)
{
    (void)action;
    (void)event;
    (void)state;
    (void)plan;
    lab_widget_events_close(calls, 1);
}

/* Reposition only a visible picker, preserving the conditional state read. */
static void lab_widget_events_reposition(lab_widget_event_action_t action, event_jsref event, jsref state,
                                jsref plan, jsref calls)
{
    (void)action;
    (void)event;
    (void)plan;
    if (lab_widget_events_visible())
        lab_widget_events_emit(calls, "placeMarsDatePicker", 1, lab_dom_get(state, "shell"), 0, 0);
}

/* Return ordered services; browser effects cannot re-enter this scoped dispatch. */
void lab_widget_events_dispatch(lab_widget_event_action_t action, event_jsref event, jsref state)
{
    /** @brief Widget action implementation using the current event and deferred service plan. */
    typedef void (*lab_widget_event_handler_t)(lab_widget_event_action_t action, event_jsref event,
                                               jsref state, jsref plan, jsref calls);
    static const lab_widget_event_handler_t handlers[LAB_WIDGET_EVENT_FLAG_RESET + 1] = {
        [LAB_WIDGET_EVENT_OPEN]           = lab_widget_events_open,
        [LAB_WIDGET_EVENT_GRID_DAY]       = lab_widget_events_grid,
        [LAB_WIDGET_EVENT_PREVIOUS_MONTH] = lab_widget_events_navigation,
        [LAB_WIDGET_EVENT_NEXT_MONTH]     = lab_widget_events_navigation,
        [LAB_WIDGET_EVENT_PREVIOUS_YEAR]  = lab_widget_events_navigation,
        [LAB_WIDGET_EVENT_NEXT_YEAR]      = lab_widget_events_navigation,
        [LAB_WIDGET_EVENT_MONTH]          = lab_widget_events_date,
        [LAB_WIDGET_EVENT_YEAR]           = lab_widget_events_date,
        [LAB_WIDGET_EVENT_YEAR_KEY]       = lab_widget_events_date,
        [LAB_WIDGET_EVENT_TODAY]          = lab_widget_events_today,
        [LAB_WIDGET_EVENT_CLOSE]          = lab_widget_events_dismiss,
        [LAB_WIDGET_EVENT_OUTSIDE]        = lab_widget_events_outside,
        [LAB_WIDGET_EVENT_REPOSITION]     = lab_widget_events_reposition,
        [LAB_WIDGET_EVENT_TOOLTIP_OVER]   = lab_widget_events_tooltip,
        [LAB_WIDGET_EVENT_TOOLTIP_OUT]    = lab_widget_events_tooltip,
        [LAB_WIDGET_EVENT_TOOLTIP_FOCUS]  = lab_widget_events_tooltip,
        [LAB_WIDGET_EVENT_TOOLTIP_HIDE]   = lab_widget_events_tooltip,
        [LAB_WIDGET_EVENT_TOOLTIP_ESCAPE] = lab_widget_events_tooltip,
        [LAB_WIDGET_EVENT_FLAG_OPEN]      = lab_widget_events_flag,
        [LAB_WIDGET_EVENT_FLAG_MODIFIER]  = lab_widget_events_flag_modifier,
        [LAB_WIDGET_EVENT_FLAG_RESET]     = lab_widget_events_flag_modifier,
    };
    static const char *const services[LAB_WIDGET_EVENT_FLAG_RESET + 1] = {
        [LAB_WIDGET_EVENT_OFFSET_TOUCHED] = "markDatetimeOffsetTouched",
    };
    int plan = lab_dom_object(LAB_VALUE_OBJECT), calls = lab_dom_object(LAB_VALUE_ARRAY);
    lab_dom_set(plan, "calls", calls);
    if ((unsigned)action < sizeof handlers / sizeof *handlers) {
        if (handlers[action])
            handlers[action](action, event, state, plan, calls);
        else if (services[action])
            lab_widget_events_emit(calls, services[action], 0, 0, 0, 0);
    }
    lab_dom_return(plan);
}

/* Subscribe fixed controls and document/window lifecycle once per picker context. */
void lab_widget_events_install(jsref document, jsref window, jsref state)
{
    static const struct {
        const char *selector, *type;
        lab_widget_event_action_t action;
    } controls[] = {
        {"[data-date-target]", "click", LAB_WIDGET_EVENT_OPEN},
        {"#marsDatePickerGrid", "click", LAB_WIDGET_EVENT_GRID_DAY},
        {"#marsDatePickerPrev", "click", LAB_WIDGET_EVENT_PREVIOUS_MONTH},
        {"#marsDatePickerNext", "click", LAB_WIDGET_EVENT_NEXT_MONTH},
        {"#marsDatePickerYearDown", "click", LAB_WIDGET_EVENT_PREVIOUS_YEAR},
        {"#marsDatePickerYearUp", "click", LAB_WIDGET_EVENT_NEXT_YEAR},
        {"#marsDatePickerMonth", "change", LAB_WIDGET_EVENT_MONTH},
        {"#marsDatePickerYear", "change", LAB_WIDGET_EVENT_YEAR},
        {"#marsDatePickerYear", "keydown", LAB_WIDGET_EVENT_YEAR_KEY},
        {"#marsDatePickerToday", "click", LAB_WIDGET_EVENT_TODAY},
        {"#marsDatePickerClose", "click", LAB_WIDGET_EVENT_CLOSE},
        {"#datetimeGmtOffset", "input", LAB_WIDGET_EVENT_OFFSET_TOUCHED},
    };
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < sizeof controls / sizeof *controls; ++i) {
        int nodes = lab_dom_all(0, controls[i].selector);
        unsigned node_mark = lab_dom_mark();
        for (unsigned n = 0; n < lab_dom_count(nodes); ++n) {
            lab_dom_subscribe(lab_dom_item(nodes, n), controls[i].type, "lab_widget_events_dispatch",
                              controls[i].action, state, 0);
            lab_dom_release(node_mark);
        }
        lab_dom_release(mark);
    }
    static const struct {
        const char *type;
        lab_widget_event_action_t action;
    } document_events[] = {
        {"click", LAB_WIDGET_EVENT_FLAG_OPEN},
        {"keydown", LAB_WIDGET_EVENT_FLAG_MODIFIER},
        {"keyup", LAB_WIDGET_EVENT_FLAG_MODIFIER},
        {"pointerover", LAB_WIDGET_EVENT_TOOLTIP_OVER},
        {"pointerout", LAB_WIDGET_EVENT_TOOLTIP_OUT},
        {"focusin", LAB_WIDGET_EVENT_TOOLTIP_FOCUS},
        {"focusout", LAB_WIDGET_EVENT_TOOLTIP_HIDE},
        {"click", LAB_WIDGET_EVENT_TOOLTIP_HIDE},
        {"keydown", LAB_WIDGET_EVENT_TOOLTIP_ESCAPE},
        {"click", LAB_WIDGET_EVENT_OUTSIDE},
    };
    for (unsigned i = 0; i < sizeof document_events / sizeof *document_events; ++i)
        lab_dom_subscribe(document, document_events[i].type, "lab_widget_events_dispatch", document_events[i].action,
                          state, 0);
    lab_dom_subscribe(window, "scroll", "lab_widget_events_dispatch", LAB_WIDGET_EVENT_TOOLTIP_HIDE, state, 1);
    lab_dom_subscribe(window, "blur", "lab_widget_events_dispatch", LAB_WIDGET_EVENT_FLAG_RESET, state, 0);
    lab_dom_subscribe(window, "resize", "lab_widget_events_dispatch", LAB_WIDGET_EVENT_TOOLTIP_HIDE, state, 0);
    lab_dom_subscribe(window, "resize", "lab_widget_events_dispatch", LAB_WIDGET_EVENT_REPOSITION, state, 0);
}
