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
#include "../include/lab_forms.h"
#include "lab_widget_events.h"

static void lab_widget_events_emit(int calls, const char *service, unsigned count, int first, int second, int third)
{
    int call = lab_dom_object(5), args = lab_dom_object(4);
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

static void lab_widget_events_close(int calls, int focus)
{
    int options = lab_dom_object(5);
    lab_dom_set(options, "restoreFocus", lab_dom_scalar(1, focus));
    lab_widget_events_emit(calls, "closeMarsDatePicker", 1, options, 0, 0);
}

static void lab_widget_events_open(int event, int state, int calls)
{
    int button = lab_dom_get(event, "currentTarget");
    int id = lab_dom_clean(lab_dom_read(button, 3, "dateTarget"), 0);
    /* A DOM ID lookup accepts the authored identifier as data, not as a CSS selector. */
    int input = lab_dom_value_id(id);
    if (!input)
        return;
    if (lab_dom_equal(lab_dom_get(state, "input"), input) && lab_widget_events_visible())
        lab_widget_events_close(calls, 1);
    else
        lab_widget_events_emit(calls, "openMarsDatePicker", 2, input, button, 0);
}

static void lab_widget_events_grid(int event, int state, int calls)
{
    int grid = lab_dom_id("marsDatePickerGrid");
    int button = lab_dom_closest(lab_dom_get(event, "target"), "button[data-iso-date]");
    if (!button || lab_dom_is_disabled(button) || !lab_dom_node_contains(grid, button))
        return;
    lab_widget_events_emit(calls, "commitMarsDateValue", 2, lab_dom_get(state, "input"),
                           lab_dom_read(button, 3, "isoDate"), 0);
    lab_widget_events_close(calls, 1);
}

static void lab_widget_events_navigation(unsigned action, int event, int calls)
{
    int direction = (action == 2 || action == 4) ? -1 : 1;
    int years = action >= 4;
    if (years)
        direction *= lab_forms_year_step(lab_dom_truth(lab_dom_get(event, "ctrlKey")),
                                         lab_dom_truth(lab_dom_get(event, "shiftKey")));
    lab_widget_events_emit(calls, years ? "shiftMarsDatePickerYear" : "shiftMarsDatePickerMonth", 1,
                           lab_dom_numeric(direction), 0, 0);
}

static void lab_widget_events_date(unsigned action, int event, int state, int plan, int calls)
{
    if (action == 8) {
        if (!lab_dom_equal(lab_dom_get(event, "key"), lab_dom_string("Enter")))
            return;
        lab_dom_set(plan, "prevent", lab_dom_scalar(1, 1));
    }
    int year = action == 6 ? lab_dom_get(state, "year") : lab_dom_read(lab_dom_id("marsDatePickerYear"), 5, "");
    int month = action == 6 ? lab_dom_read(lab_dom_id("marsDatePickerMonth"), 5, "") : lab_dom_get(state, "month");
    int options = lab_dom_object(5);
    lab_dom_set(options, "commit", lab_dom_scalar(1, 1));
    lab_widget_events_emit(calls, "setMarsDatePickerMonthYear", 3, year, month, options);
}

static void lab_widget_events_outside(int event, int calls)
{
    int target = lab_dom_get(event, "target"), picker = lab_dom_id("marsDatePicker");
    if (!lab_widget_events_visible() || lab_dom_node_contains(picker, target) ||
        lab_dom_closest(target, ".mars-date-shell"))
        return;
    lab_widget_events_close(calls, 0);
}

static void lab_widget_events_tooltip(unsigned action, int event, int calls)
{
    int button = lab_dom_closest(lab_dom_get(event, "target"), "button");
    if (action == 13 || action == 14) {
        if (!button || lab_dom_node_contains(button, lab_dom_get(event, "relatedTarget")))
            return;
    } else if (action == 17 && !lab_dom_equal(lab_dom_get(event, "key"), lab_dom_string("Escape"))) {
        return;
    }
    if (action == 13 || action == 15)
        lab_widget_events_emit(calls, "showButtonTooltip", 1, button, 0, 0);
    else
        lab_widget_events_emit(calls, "hideButtonTooltip", 0, 0, 0, 0);
}

/* Return ordered services; browser effects cannot re-enter this scoped dispatch. */
void lab_widget_events_dispatch(unsigned action, int event, int state)
{
    int plan = lab_dom_object(5), calls = lab_dom_object(4);
    lab_dom_set(plan, "calls", calls);
    if (action == 0)
        lab_widget_events_open(event, state, calls);
    else if (action == 1)
        lab_widget_events_grid(event, state, calls);
    else if (action >= 2 && action <= 5)
        lab_widget_events_navigation(action, event, calls);
    else if (action >= 6 && action <= 8)
        lab_widget_events_date(action, event, state, plan, calls);
    else if (action == 9) {
        int input = lab_dom_get(state, "input");
        if (input) {
            lab_widget_events_emit(calls, "commitMarsTodayValue", 1, input, 0, 0);
            lab_widget_events_close(calls, 1);
        }
    } else if (action == 10)
        lab_widget_events_close(calls, 1);
    else if (action == 11)
        lab_widget_events_outside(event, calls);
    else if (action == 12 && lab_widget_events_visible())
        lab_widget_events_emit(calls, "placeMarsDatePicker", 1, lab_dom_get(state, "shell"), 0, 0);
    else if (action >= 13 && action <= 17)
        lab_widget_events_tooltip(action, event, calls);
    else if (action == 18)
        lab_widget_events_emit(calls, "markDatetimeOffsetTouched", 0, 0, 0, 0);
    lab_dom_return(plan);
}

/* Subscribe fixed controls and document/window lifecycle once per picker context. */
void lab_widget_events_install(int document, int window, int state)
{
    static const struct {
        const char *selector, *type;
        unsigned action;
    } controls[] = {{"[data-date-target]", "click", 0},      {"#marsDatePickerGrid", "click", 1},
                    {"#marsDatePickerPrev", "click", 2},     {"#marsDatePickerNext", "click", 3},
                    {"#marsDatePickerYearDown", "click", 4}, {"#marsDatePickerYearUp", "click", 5},
                    {"#marsDatePickerMonth", "change", 6},   {"#marsDatePickerYear", "change", 7},
                    {"#marsDatePickerYear", "keydown", 8},   {"#marsDatePickerToday", "click", 9},
                    {"#marsDatePickerClose", "click", 10},   {"#datetimeGmtOffset", "input", 18}};
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
        unsigned action;
    } document_events[] = {{"pointerover", 13}, {"pointerout", 14}, {"focusin", 15}, {"focusout", 16},
                           {"click", 16},       {"keydown", 17},    {"click", 11}};
    for (unsigned i = 0; i < sizeof document_events / sizeof *document_events; ++i)
        lab_dom_subscribe(document, document_events[i].type, "lab_widget_events_dispatch", document_events[i].action,
                          state, 0);
    lab_dom_subscribe(window, "scroll", "lab_widget_events_dispatch", 16, state, 1);
    lab_dom_subscribe(window, "resize", "lab_widget_events_dispatch", 16, state, 0);
    lab_dom_subscribe(window, "resize", "lab_widget_events_dispatch", 12, state, 0);
}
