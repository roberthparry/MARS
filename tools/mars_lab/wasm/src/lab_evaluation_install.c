/**
 * @file lab_evaluation_install.c
 * @brief Ordered editor, card and persistence completion policy for native evaluations.
 *
 * Uses directly indexed mode handlers to prepare browser service plans. Initial
 * projection and final persistence are separate so getter-backed editor state is
 * read after the editor service, and differential equations can await native SVG
 * rendering between phases. Expressions, bindings and markup remain opaque native
 * values. Each binding/calendar record is visited once with per-record temporary
 * handle release; only browser-owned records cross the synchronous call boundary.
 */
#include <stdarg.h>

#include "lab_dom.h"
#include "lab_editor.h"
#include "lab_binding.h"
#include "lab_binding_editor.h"
#include "lab_evaluation_install.h"

typedef struct {
    int calls, data, context, cards, view;
    unsigned mode, outcome;
} lab_evaluation_install_t;

typedef void (*lab_evaluation_install_handler_t)(const lab_evaluation_install_t *state);

static void lab_evaluation_install_emit(int calls, const char *service, unsigned count, ...)
{
    int call = lab_dom_object(5), args = lab_dom_object(4);
    va_list values;
    va_start(values, count);
    for (unsigned i = 0; i < count; ++i)
        lab_dom_push(args, va_arg(values, int));
    va_end(values);
    lab_dom_set(call, "service", lab_dom_string(service));
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
}

static int lab_evaluation_install_or(int value, int fallback)
{
    return lab_dom_truth(value) ? value : fallback;
}

static int lab_evaluation_install_metadata(const lab_evaluation_install_t *state, int text)
{
    return lab_editor_lookup(text, lab_dom_get(state->view, "editors"));
}

static int lab_evaluation_install_bindings(const lab_evaluation_install_t *state, int text, int visible)
{
    return lab_binding_authored_values(lab_dom_get(state->data, "binding_values"),
                                       lab_dom_get(lab_evaluation_install_metadata(state, text), "bindings"), visible);
}

static int lab_evaluation_install_variables(int bindings)
{
    int variables = lab_dom_object(4);
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < lab_binding_array_count(bindings); ++i) {
        int name = lab_binding_variable_name(lab_dom_item(bindings, i));
        if (name)
            lab_dom_push(variables, name);
        lab_dom_release(mark);
    }
    return variables;
}

static void lab_evaluation_install_present(const lab_evaluation_install_t *state)
{
    lab_evaluation_install_emit(state->calls, "present", 2, lab_dom_numeric(state->mode), state->data);
}

static void lab_evaluation_install_render(const lab_evaluation_install_t *state, int expandable)
{
    lab_evaluation_install_emit(state->calls, "render", 3, lab_dom_numeric(state->mode), state->data, expandable);
}

static void lab_evaluation_install_clear(const lab_evaluation_install_t *state)
{
    lab_evaluation_install_emit(state->calls, "variables", 2, lab_dom_object(4), lab_dom_scalar(1, 0));
}

static void lab_evaluation_install_expression_begin(const lab_evaluation_install_t *state)
{
    int bindings = lab_dom_get(state->data, "binding_values");
    if (state->outcome != 3)
        lab_evaluation_install_emit(state->calls, "setRenderedResult", 1, state->data);
    if (state->outcome != 3 && lab_dom_truth(lab_dom_get(state->data, "expression"))) {
        int text =
            lab_evaluation_install_or(lab_dom_get(state->context, "editorText"), lab_dom_get(state->context, "text"));
        lab_evaluation_install_emit(
            state->calls, "setExpressionEditor", 4, text,
            lab_evaluation_install_bindings(state, text, lab_dom_get(state->context, "enteredBindings")),
            lab_evaluation_install_or(lab_dom_get(state->context, "editorBodyText"), 0),
            lab_dom_get(state->data, "evaluation_ready"));
    } else if (lab_dom_truth(bindings)) {
        lab_evaluation_install_emit(state->calls, "renderVariableValues", 1, bindings);
    }
}

static void lab_evaluation_install_expression_end(const lab_evaluation_install_t *state)
{
    lab_evaluation_install_emit(state->calls, "installEvaluationTextCards", 1, state->cards);
    lab_evaluation_install_present(state);
    lab_evaluation_install_emit(state->calls, "source", 2, lab_dom_string("lastInput"),
                                lab_dom_get(state->context, "text"));
    if (state->outcome != 3) {
        int text = lab_dom_get(state->context, "editorText");
        if (!lab_dom_truth(text))
            text = lab_dom_get(state->view, "fullText");
        if (!lab_dom_truth(text))
            text = lab_dom_clean(lab_dom_read(lab_dom_id("expr"), 5, ""), 0);
        lab_evaluation_install_emit(state->calls, "saveWorksheetState", 2, lab_dom_string("expression"), text);
    }
    lab_evaluation_install_emit(state->calls, "derivative", 1, lab_dom_get(state->cards, "derivative"));
    lab_evaluation_install_emit(state->calls, "variables", 2,
                                lab_evaluation_install_variables(lab_dom_get(state->data, "binding_values")),
                                lab_dom_get(state->cards, "differentiable"));
}

static void lab_evaluation_install_matrix_begin(const lab_evaluation_install_t *state)
{
    lab_evaluation_install_emit(state->calls, "displayMatrixResult", 1, state->data);
    lab_evaluation_install_emit(state->calls, "scalar", 1, lab_dom_get(state->cards, "scalar"));
    lab_evaluation_install_present(state);
    if (lab_binding_array_count(lab_dom_get(state->data, "binding_values"))) {
        int text = lab_dom_get(state->context, "text");
        int body = lab_dom_get(lab_evaluation_install_metadata(state, text), "body");
        if (!lab_dom_type(body))
            body = lab_dom_clean(text, 0);
        lab_evaluation_install_emit(state->calls, "setExpressionEditor", 3, body,
                                    lab_evaluation_install_bindings(state, text, 0), body);
    } else {
        lab_evaluation_install_emit(state->calls, "clearVariableValues", 0);
    }
}

static void lab_evaluation_install_matrix_end(const lab_evaluation_install_t *state)
{
    lab_evaluation_install_emit(
        state->calls, "modeSource", 2, lab_dom_string("matrix"),
        lab_evaluation_install_or(lab_editor_current_value(state->view), lab_dom_get(state->context, "text")));
    lab_evaluation_install_emit(state->calls, "saveWorksheetState", 1, lab_dom_string("matrix"));
    int variables = lab_evaluation_install_variables(lab_dom_get(state->data, "binding_values"));
    lab_evaluation_install_emit(state->calls, "variables", 2, variables,
                                lab_dom_scalar(1, lab_dom_count(variables) > 0));
}

static void lab_evaluation_install_equation_begin(const lab_evaluation_install_t *state)
{
    lab_evaluation_install_render(state, lab_dom_get(state->cards, "expandable"));
    lab_evaluation_install_emit(state->calls, "installEvaluationTextCards", 1, state->cards);
    int bindings = lab_dom_get(state->data, "binding_values");
    if (lab_dom_type(bindings) == 4)
        lab_evaluation_install_emit(state->calls, "renderVariableValues", 1, bindings);
    else
        lab_evaluation_install_emit(state->calls, "clearVariableValues", 0);
}

static void lab_evaluation_install_solver_end(const lab_evaluation_install_t *state)
{
    const char *mode = state->mode == 1 ? "equation" : "diffequation";
    if (state->mode == 2) {
        lab_evaluation_install_emit(state->calls, "setValueText", 1, lab_dom_get(state->cards, "value"));
        lab_evaluation_install_emit(state->calls, "setValueCardVisible", 1, lab_dom_scalar(1, 1));
        lab_evaluation_install_emit(state->calls, "clearVariableValues", 0);
    }
    lab_evaluation_install_emit(state->calls, "modeSource", 2, lab_dom_string(mode),
                                lab_dom_get(state->context, "text"));
    lab_evaluation_install_emit(state->calls, "saveWorksheetState", 1, lab_dom_string(mode));
    lab_evaluation_install_clear(state);
}

static void lab_evaluation_install_diffequation_begin(const lab_evaluation_install_t *state)
{
    lab_evaluation_install_render(state, lab_dom_scalar(1, 0));
    lab_evaluation_install_emit(state->calls, "scheduleRenderedTeXFit", 0);
    lab_evaluation_install_emit(state->calls, "solverTextCards", 1, state->cards);
    lab_evaluation_install_present(state);
}

static void lab_evaluation_install_integrator_begin(const lab_evaluation_install_t *state)
{
    lab_evaluation_install_render(state, lab_dom_scalar(1, 0));
    lab_evaluation_install_emit(state->calls, "installEvaluationTextCards", 1, state->cards);
    lab_evaluation_install_emit(state->calls, "applyIntegratorBindingState", 2, state->data,
                                lab_dom_get(state->context, "text"));
    lab_evaluation_install_emit(state->calls, "applyIntegratorResultBound", 1, state->data);
}

static void lab_evaluation_install_integrator_end(const lab_evaluation_install_t *state)
{
    lab_evaluation_install_emit(state->calls, "saveWorksheetState", 1, lab_dom_string("integrator"));
    lab_evaluation_install_clear(state);
}

static void lab_evaluation_install_datetime_begin(const lab_evaluation_install_t *state)
{
    int calendar = lab_dom_get(state->cards, "calendar");
    unsigned mark = lab_dom_mark();
    for (unsigned i = 0; i < lab_dom_count(calendar); ++i) {
        lab_evaluation_install_emit(state->calls, "calendarCard", 1, lab_dom_item(calendar, i));
        lab_dom_release(mark);
    }
    lab_evaluation_install_present(state);
    lab_evaluation_install_emit(
        state->calls, "setDatetimeLocalText", 2,
        lab_evaluation_install_or(lab_dom_get(state->data, "local"), lab_dom_string("")),
        lab_evaluation_install_or(lab_dom_get(state->data, "local_sections"), lab_dom_object(4)));
    lab_evaluation_install_emit(state->calls, "applyCalendarEvaluationFields", 2, lab_dom_string("datetime"),
                                state->data);
    lab_evaluation_install_emit(state->calls, "setResultInputText", 1, lab_dom_string(""));
}

static void lab_evaluation_install_calendar_end(const lab_evaluation_install_t *state)
{
    lab_evaluation_install_emit(state->calls, "clearVariableValues", 0);
    lab_evaluation_install_clear(state);
    lab_evaluation_install_emit(state->calls, state->mode == 5 ? "saveLastDatetimeState" : "saveLastAlmanacState", 0);
}

static void lab_evaluation_install_almanac_begin(const lab_evaluation_install_t *state)
{
    lab_evaluation_install_emit(state->calls, "almanacRender", 1, state->data);
    lab_evaluation_install_emit(state->calls, "almanacAccept", 1, state->data);
    lab_evaluation_install_emit(state->calls, "refreshAlmanacLandTotality", 1, state->data);
    lab_evaluation_install_present(state);
    lab_evaluation_install_emit(state->calls, "installEvaluationTextCards", 1, state->cards);
    lab_evaluation_install_emit(state->calls, "applyCalendarEvaluationFields", 2, lab_dom_string("almanac"),
                                state->data);
}

/* Return a pure two-phase plan; all state mutation stays ordered outside the scoped bridge. */
void lab_evaluation_install(unsigned mode, unsigned phase, unsigned outcome, int data, int context, int cards, int view)
{
    static const lab_evaluation_install_handler_t handlers[7][2] = {
        [0] = {lab_evaluation_install_expression_begin, lab_evaluation_install_expression_end},
        [1] = {lab_evaluation_install_equation_begin, lab_evaluation_install_solver_end},
        [2] = {lab_evaluation_install_diffequation_begin, lab_evaluation_install_solver_end},
        [3] = {lab_evaluation_install_matrix_begin, lab_evaluation_install_matrix_end},
        [4] = {lab_evaluation_install_integrator_begin, lab_evaluation_install_integrator_end},
        [5] = {lab_evaluation_install_datetime_begin, lab_evaluation_install_calendar_end},
        [6] = {lab_evaluation_install_almanac_begin, lab_evaluation_install_calendar_end}};
    if (mode > 6 || phase > 1 || (outcome != 2 && !(mode == 0 && outcome == 3))) {
        lab_dom_return(0);
        return;
    }
    int result = lab_dom_object(5), calls = lab_dom_object(4);
    lab_evaluation_install_t state = {calls, data, context, cards, view, mode, outcome};
    lab_dom_set(result, "calls", calls);
    handlers[mode][phase](&state);
    lab_dom_return(result);
}
