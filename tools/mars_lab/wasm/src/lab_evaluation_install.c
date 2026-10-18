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

#include "lab_mode.h"
#include "lab_dom.h"
#include "lab_editor.h"
#include "lab_binding.h"
#include "lab_binding_editor.h"
#include "lab_evaluation_install.h"

typedef struct {
    jsref calls, data, context, cards;
    view_jsref view;
    lab_mode_t mode;
    lab_request_outcome_t outcome;
} lab_evaluation_install_t;

/**
 * @brief Append one mode's evaluation-installation phase to the ordered browser-service calls.
 * @param state Borrowed mode/outcome and scoped payload, input context, cards and view handles;
 *   calls identifies the output array to which the handler appends services without executing them.
 */
typedef void (*lab_evaluation_install_handler_t)(const lab_evaluation_install_t *state);

static void lab_evaluation_install_emit(jsref calls, const char *service, unsigned count, ...)
{
    int call = lab_dom_object(LAB_VALUE_OBJECT), args = lab_dom_object(LAB_VALUE_ARRAY);
    va_list values;
    va_start(values, count);
    for (unsigned i = 0; i < count; ++i)
        lab_dom_push(args, va_arg(values, jsref));
    va_end(values);
    lab_dom_set(call, "service", lab_dom_string(service));
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
}

static jsref lab_evaluation_install_or(jsref value, jsref fallback)
{
    return lab_dom_truth(value) ? value : fallback;
}

static jsref lab_evaluation_install_metadata(const lab_evaluation_install_t *state, jsref text)
{
    return lab_editor_lookup(text, lab_dom_get(state->view, "editors"));
}

static jsref lab_evaluation_install_bindings(const lab_evaluation_install_t *state, jsref text,
                                jsref visible)
{
    return lab_binding_authored_values(lab_dom_get(state->data, "binding_values"),
                                       lab_dom_get(lab_evaluation_install_metadata(state, text), "bindings"), visible);
}

static jsref lab_evaluation_install_variables(jsref bindings)
{
    int variables = lab_dom_object(LAB_VALUE_ARRAY);
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
    lab_evaluation_install_emit(state->calls, "variables", 2, lab_dom_object(LAB_VALUE_ARRAY),
                                lab_dom_scalar(LAB_VALUE_BOOLEAN, 0));
}

static void lab_evaluation_install_expression_begin(const lab_evaluation_install_t *state)
{
    int bindings = lab_dom_get(state->data, "binding_values");
    if (state->outcome != LAB_REQUEST_OUTCOME_PARTIAL)
        lab_evaluation_install_emit(state->calls, "setRenderedResult", 1, state->data);
    if (state->outcome != LAB_REQUEST_OUTCOME_PARTIAL && lab_dom_truth(lab_dom_get(state->data, "expression"))) {
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
    if (state->outcome != LAB_REQUEST_OUTCOME_PARTIAL) {
        int text = lab_dom_get(state->context, "editorText");
        if (!lab_dom_truth(text))
            text = lab_dom_get(state->view, "fullText");
        if (!lab_dom_truth(text))
            text = lab_dom_clean(lab_dom_read(lab_dom_id("expr"), LAB_DOM_VALUE, ""), LAB_DOM_CLEAN_TRIM);
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
            body = lab_dom_clean(text, LAB_DOM_CLEAN_TRIM);
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
                                lab_dom_scalar(LAB_VALUE_BOOLEAN, lab_dom_count(variables) > 0));
}

static void lab_evaluation_install_equation_begin(const lab_evaluation_install_t *state)
{
    lab_evaluation_install_render(state, lab_dom_get(state->cards, "expandable"));
    lab_evaluation_install_emit(state->calls, "installEvaluationTextCards", 1, state->cards);
    int bindings = lab_dom_get(state->data, "binding_values");
    if (lab_dom_type(bindings) == LAB_VALUE_ARRAY)
        lab_evaluation_install_emit(state->calls, "renderVariableValues", 1, bindings);
    else
        lab_evaluation_install_emit(state->calls, "clearVariableValues", 0);
}

static void lab_evaluation_install_solver_end(const lab_evaluation_install_t *state)
{
    const char *mode = state->mode == LAB_MODE_EQUATION ? "equation" : "diffequation";
    if (state->mode == LAB_MODE_DIFFERENTIAL_EQUATION)
        lab_evaluation_install_emit(state->calls, "setValueText", 1, lab_dom_get(state->cards, "value"));
    /* Plotting hides this card; every completed solver evaluation must restore it. */
    lab_evaluation_install_emit(state->calls, "setValueCardVisible", 1, lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
    if (state->mode == LAB_MODE_DIFFERENTIAL_EQUATION)
        lab_evaluation_install_emit(state->calls, "clearVariableValues", 0);
    lab_evaluation_install_emit(state->calls, "modeSource", 2, lab_dom_string(mode),
                                lab_dom_get(state->context, "text"));
    lab_evaluation_install_emit(state->calls, "saveWorksheetState", 1, lab_dom_string(mode));
    lab_evaluation_install_emit(state->calls, "variables", 2,
                                lab_evaluation_install_variables(lab_dom_get(state->data, "binding_values")),
                                lab_dom_scalar(LAB_VALUE_BOOLEAN, 0));
}

static void lab_evaluation_install_diffequation_begin(const lab_evaluation_install_t *state)
{
    lab_evaluation_install_render(state, lab_dom_scalar(LAB_VALUE_BOOLEAN, 0));
    lab_evaluation_install_emit(state->calls, "scheduleRenderedTeXFit", 0);
    lab_evaluation_install_emit(state->calls, "solverTextCards", 1, state->cards);
    lab_evaluation_install_present(state);
}

static void lab_evaluation_install_integrator_begin(const lab_evaluation_install_t *state)
{
    lab_evaluation_install_render(state, lab_dom_scalar(LAB_VALUE_BOOLEAN, 0));
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
        lab_evaluation_install_or(lab_dom_get(state->data, "local_sections"), lab_dom_object(LAB_VALUE_ARRAY)));
    lab_evaluation_install_emit(state->calls, "applyCalendarEvaluationFields", 2, lab_dom_string("datetime"),
                                state->data);
    lab_evaluation_install_emit(state->calls, "setResultInputText", 1, lab_dom_string(""));
}

static void lab_evaluation_install_calendar_end(const lab_evaluation_install_t *state)
{
    lab_evaluation_install_emit(state->calls, "clearVariableValues", 0);
    lab_evaluation_install_clear(state);
    const char *service = state->mode == LAB_MODE_DATETIME ? "saveLastDatetimeState" : "saveLastAlmanacState";
    lab_evaluation_install_emit(state->calls, service, 0);
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
void lab_evaluation_install(lab_mode_t mode, lab_evaluation_install_phase_t phase, lab_request_outcome_t outcome,
                            jsref data, jsref context, jsref cards, view_jsref view)
{
    static const lab_evaluation_install_handler_t handlers[][LAB_MODE_COUNT] = {
        [LAB_EVALUATION_INSTALL_PROJECT][LAB_MODE_EXPRESSION]             = lab_evaluation_install_expression_begin,
        [LAB_EVALUATION_INSTALL_PROJECT][LAB_MODE_EQUATION]               = lab_evaluation_install_equation_begin,
        [LAB_EVALUATION_INSTALL_PROJECT][LAB_MODE_DIFFERENTIAL_EQUATION]  = lab_evaluation_install_diffequation_begin,
        [LAB_EVALUATION_INSTALL_PROJECT][LAB_MODE_MATRIX]                 = lab_evaluation_install_matrix_begin,
        [LAB_EVALUATION_INSTALL_PROJECT][LAB_MODE_INTEGRATOR]             = lab_evaluation_install_integrator_begin,
        [LAB_EVALUATION_INSTALL_PROJECT][LAB_MODE_DATETIME]               = lab_evaluation_install_datetime_begin,
        [LAB_EVALUATION_INSTALL_PROJECT][LAB_MODE_ALMANAC]                = lab_evaluation_install_almanac_begin,
        [LAB_EVALUATION_INSTALL_COMPLETE][LAB_MODE_EXPRESSION]            = lab_evaluation_install_expression_end,
        [LAB_EVALUATION_INSTALL_COMPLETE][LAB_MODE_EQUATION]              = lab_evaluation_install_solver_end,
        [LAB_EVALUATION_INSTALL_COMPLETE][LAB_MODE_DIFFERENTIAL_EQUATION] = lab_evaluation_install_solver_end,
        [LAB_EVALUATION_INSTALL_COMPLETE][LAB_MODE_MATRIX]                = lab_evaluation_install_matrix_end,
        [LAB_EVALUATION_INSTALL_COMPLETE][LAB_MODE_INTEGRATOR]            = lab_evaluation_install_integrator_end,
        [LAB_EVALUATION_INSTALL_COMPLETE][LAB_MODE_DATETIME]              = lab_evaluation_install_calendar_end,
        [LAB_EVALUATION_INSTALL_COMPLETE][LAB_MODE_ALMANAC]               = lab_evaluation_install_calendar_end,
    };
    if ((unsigned)mode > LAB_MODE_ALMANAC || (unsigned)phase > LAB_EVALUATION_INSTALL_COMPLETE ||
        (outcome != LAB_REQUEST_OUTCOME_SUCCESS &&
         !(mode == LAB_MODE_EXPRESSION && outcome == LAB_REQUEST_OUTCOME_PARTIAL))) {
        lab_dom_return(0);
        return;
    }
    int result = lab_dom_object(LAB_VALUE_OBJECT), calls = lab_dom_object(LAB_VALUE_ARRAY);
    lab_evaluation_install_t state = {calls, data, context, cards, view, mode, outcome};
    lab_dom_set(result, "calls", calls);
    handlers[phase][mode](&state);
    lab_dom_return(result);
}
