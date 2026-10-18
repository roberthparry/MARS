/**
 * @file lab_evaluation.c
 * @brief Evaluation preparation and result lifecycle policy for the Lab browser.
 *
 * Directly indexed mode policies choose binding preparation, input validation,
 * history updates, failure recovery and asynchronous result installation.
 * Named preparation records and ordered recovery plans keep flag interpretation
 * and diagnostic precedence here. Native lab_flow continuations select await
 * boundaries and ownership checks; the browser executes their capabilities.
 * Request outcomes come from lab_requests.c; native
 * MARS remains responsible for mathematics and its presentation. This module
 * retains no request, browser handle or authored text across calls. Returned plans
 * are browser-owned values; no persistent native storage is allocated.
 */
#include <string.h>

#include "lab_mode.h"
#include "lab_dom.h"
#include "lab_evaluation.h"

enum {
    lab_eval_bindings = 1,
    lab_eval_expression = 2,
    lab_eval_input = 4,
    lab_eval_trim = 8,
    lab_eval_datetime = 16,
    lab_eval_almanac = 32,
    lab_eval_history = 64,
    lab_eval_bound_input = lab_eval_bindings | lab_eval_input,
    lab_eval_plain_input = lab_eval_trim | lab_eval_input
};

enum {
    lab_eval_install = 1,
    lab_eval_error = 2,
    lab_eval_clear = 4,
    lab_eval_clear_error = 8,
    lab_eval_clear_local = 16,
    lab_eval_clear_scalar = 32,
    lab_eval_integrator = 64,
    lab_eval_await = 128,
    lab_eval_weather = 256,
    lab_eval_replace = lab_eval_install | lab_eval_clear | lab_eval_clear_error,
    lab_eval_failure = lab_eval_error | lab_eval_clear
};

static const unsigned lab_evaluation_preparation[LAB_MODE_COUNT] = {
    [LAB_MODE_EXPRESSION]            = lab_eval_bound_input | lab_eval_expression,
    [LAB_MODE_EQUATION]              = lab_eval_plain_input | lab_eval_bindings,
    [LAB_MODE_DIFFERENTIAL_EQUATION] = lab_eval_plain_input,
    [LAB_MODE_MATRIX]                = lab_eval_bound_input,
    [LAB_MODE_INTEGRATOR]            = lab_eval_bound_input,
    [LAB_MODE_DATETIME]              = lab_eval_datetime,
    [LAB_MODE_ALMANAC]               = lab_eval_almanac,
};

/* Stale outcomes and non-expression partial results deliberately have no actions. */
static const unsigned lab_evaluation_policy[LAB_REQUEST_OUTCOME_EXCEPTION + 1][LAB_MODE_COUNT] = {
    [LAB_REQUEST_OUTCOME_FAILURE][LAB_MODE_EXPRESSION]              = lab_eval_failure,
    [LAB_REQUEST_OUTCOME_FAILURE][LAB_MODE_EQUATION]                = lab_eval_failure,
    [LAB_REQUEST_OUTCOME_FAILURE][LAB_MODE_DIFFERENTIAL_EQUATION]   = lab_eval_failure,
    [LAB_REQUEST_OUTCOME_FAILURE][LAB_MODE_MATRIX]                  = lab_eval_failure | lab_eval_clear_scalar,
    [LAB_REQUEST_OUTCOME_FAILURE][LAB_MODE_INTEGRATOR]              = lab_eval_failure | lab_eval_integrator,
    [LAB_REQUEST_OUTCOME_FAILURE][LAB_MODE_DATETIME]                = lab_eval_failure | lab_eval_clear_local,
    [LAB_REQUEST_OUTCOME_FAILURE][LAB_MODE_ALMANAC]                 = lab_eval_failure,
    [LAB_REQUEST_OUTCOME_SUCCESS][LAB_MODE_EXPRESSION]              = lab_eval_install,
    [LAB_REQUEST_OUTCOME_SUCCESS][LAB_MODE_EQUATION]                = lab_eval_replace,
    [LAB_REQUEST_OUTCOME_SUCCESS][LAB_MODE_DIFFERENTIAL_EQUATION]   = lab_eval_replace | lab_eval_await,
    [LAB_REQUEST_OUTCOME_SUCCESS][LAB_MODE_MATRIX]                  = lab_eval_replace,
    [LAB_REQUEST_OUTCOME_SUCCESS][LAB_MODE_INTEGRATOR]              = lab_eval_replace,
    [LAB_REQUEST_OUTCOME_SUCCESS][LAB_MODE_DATETIME]                = lab_eval_replace | lab_eval_weather,
    [LAB_REQUEST_OUTCOME_SUCCESS][LAB_MODE_ALMANAC]                 = lab_eval_replace,
    [LAB_REQUEST_OUTCOME_PARTIAL][LAB_MODE_EXPRESSION]              = lab_eval_install | lab_eval_error,
    [LAB_REQUEST_OUTCOME_EXCEPTION][LAB_MODE_EXPRESSION]            = lab_eval_failure,
    [LAB_REQUEST_OUTCOME_EXCEPTION][LAB_MODE_EQUATION]              = lab_eval_failure,
    [LAB_REQUEST_OUTCOME_EXCEPTION][LAB_MODE_DIFFERENTIAL_EQUATION] = lab_eval_failure,
    [LAB_REQUEST_OUTCOME_EXCEPTION][LAB_MODE_MATRIX]                = lab_eval_failure,
    [LAB_REQUEST_OUTCOME_EXCEPTION][LAB_MODE_INTEGRATOR]            = lab_eval_failure,
    [LAB_REQUEST_OUTCOME_EXCEPTION][LAB_MODE_DATETIME]              = lab_eval_failure | lab_eval_clear_local,
    [LAB_REQUEST_OUTCOME_EXCEPTION][LAB_MODE_ALMANAC]               = lab_eval_failure,
};

static const struct {
    const char *working, *failure;
} lab_evaluation_labels[LAB_MODE_COUNT] = {
    [LAB_MODE_EXPRESSION]            = {"Evaluating...", "Evaluation failed"},
    [LAB_MODE_EQUATION]              = {"Solving equation...", "Equation solving failed"},
    [LAB_MODE_DIFFERENTIAL_EQUATION] = {"Solving differential equation...", "Differential-equation solving failed"},
    [LAB_MODE_MATRIX]                = {"Evaluating matrix...", "Matrix evaluation failed"},
    [LAB_MODE_INTEGRATOR]            = {"Integrating...", "Integration failed"},
    [LAB_MODE_DATETIME]              = {"Calculating dates...", "Datetime calculation failed"},
    [LAB_MODE_ALMANAC]               = {"Working the almanac...", "Almanac calculation failed"},
};

/* Preparation precedes the guarded result phase, preserving setup-error history semantics. */
unsigned lab_evaluation_prepare(lab_mode_t mode, bool skip_history)
{
    if ((unsigned)mode >= LAB_MODE_COUNT)
        return 0;
    return lab_evaluation_preparation[mode] | (skip_history ? 0u : lab_eval_history);
}

/* Return the mode-specific action mask for the accepted request outcome. */
unsigned lab_evaluation_actions(lab_mode_t mode, lab_request_outcome_t outcome)
{
    if ((unsigned)mode >= LAB_MODE_COUNT || outcome == LAB_REQUEST_OUTCOME_STALE ||
        (unsigned)outcome > LAB_REQUEST_OUTCOME_EXCEPTION)
        return 0;
    return lab_evaluation_policy[outcome][mode];
}

/* Borrow the mode's immutable working, failure or completion label selected by field. */
const char *lab_evaluation_text(lab_mode_t mode, lab_evaluation_label_t field)
{
    static const char *const statuses[] = {
        [LAB_EVALUATION_LABEL_READY]        = "Ready",
        [LAB_EVALUATION_LABEL_ERROR]        = "Error",
        [LAB_EVALUATION_LABEL_LOCAL_SERIES] = "Local series",
        [LAB_EVALUATION_LABEL_NOT_SOLVED]   = "Not solved",
    };
    if ((unsigned)mode >= LAB_MODE_COUNT || (unsigned)field > LAB_EVALUATION_LABEL_NOT_SOLVED)
        return "";
    if (field < LAB_EVALUATION_LABEL_READY)
        return field == LAB_EVALUATION_LABEL_WORKING ? lab_evaluation_labels[mode].working
                                                     : lab_evaluation_labels[mode].failure;
    return statuses[field];
}

/* Only short static UI labels are scanned; authored or mathematical text never enters this module. */
unsigned lab_evaluation_text_length(lab_mode_t mode, lab_evaluation_label_t field)
{
    return (unsigned)strlen(lab_evaluation_text(mode, field));
}

/* Differential-equation statuses describe solver completion without interpreting its result. */
lab_evaluation_label_t lab_evaluation_status(lab_mode_t mode, lab_request_outcome_t outcome, bool series,
                                             bool solved)
{
    if (!lab_evaluation_actions(mode, outcome))
        return LAB_EVALUATION_LABEL_EMPTY;
    if (outcome != LAB_REQUEST_OUTCOME_SUCCESS)
        return LAB_EVALUATION_LABEL_ERROR;
    if (mode != LAB_MODE_DIFFERENTIAL_EQUATION)
        return LAB_EVALUATION_LABEL_READY;
    if (series)
        return LAB_EVALUATION_LABEL_LOCAL_SERIES;
    return solved ? LAB_EVALUATION_LABEL_READY : LAB_EVALUATION_LABEL_NOT_SOLVED;
}

/* Select the diagnostic source while preserving each mode's error-precedence policy. */
lab_evaluation_error_source_t lab_evaluation_error_source(lab_mode_t mode, bool has_error, bool generic,
                                                          bool has_raw)
{
    if ((unsigned)mode >= LAB_MODE_COUNT)
        return LAB_EVALUATION_ERROR_FALLBACK;
    if (mode != LAB_MODE_INTEGRATOR)
        return has_error ? LAB_EVALUATION_ERROR_ORIGINAL : LAB_EVALUATION_ERROR_FALLBACK;
    if (has_error && !generic)
        return LAB_EVALUATION_ERROR_TRIMMED;
    if (has_raw)
        return LAB_EVALUATION_ERROR_RAW;
    return has_error ? LAB_EVALUATION_ERROR_TRIMMED : LAB_EVALUATION_ERROR_FALLBACK;
}

/* Expose a label through the scoped bridge without duplicating UTF-8 buffer access in the adapter. */
void lab_evaluation_label(lab_mode_t mode, lab_evaluation_label_t field)
{
    lab_dom_return(lab_dom_string(lab_evaluation_text(mode, field)));
}

/* Name the preparation capabilities in C; the host only performs the required asynchronous work. */
void lab_evaluation_prepare_plan(lab_mode_t mode, bool skip_history)
{
    unsigned flags = lab_evaluation_prepare(mode, skip_history);
    if (!flags) {
        lab_dom_return(0);
        return;
    }
    static const char *const names[] = {"bindings", "expression", "input", "trim", "datetime", "almanac", "history"};
    int plan = lab_dom_object(LAB_VALUE_OBJECT);
    for (unsigned i = 0; i < sizeof names / sizeof *names; ++i)
        lab_dom_set(plan, names[i], lab_dom_scalar(LAB_VALUE_BOOLEAN, (flags & (1u << i)) != 0));
    lab_dom_set(plan, "label", lab_dom_string(lab_evaluation_text(mode, LAB_EVALUATION_LABEL_WORKING)));
    lab_dom_return(plan);
}

static void lab_evaluation_emit(browser_handle calls, const char *service, unsigned count, browser_handle first,
                                browser_handle second)
{
    int call = lab_dom_object(LAB_VALUE_OBJECT), args = lab_dom_object(LAB_VALUE_ARRAY);
    if (count)
        lab_dom_push(args, first);
    if (count > 1)
        lab_dom_push(args, second);
    lab_dom_set(call, "service", lab_dom_string(service));
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
}

/* Return ordered recovery services and outcome metadata; no side effects occur inside this scope. */
void lab_evaluation_response(lab_mode_t mode, lab_request_outcome_t outcome, browser_handle data,
                             browser_handle context, browser_handle error)
{
    unsigned flags = lab_evaluation_actions(mode, outcome);
    if (!flags) {
        lab_dom_return(0);
        return;
    }
    int plan = lab_dom_object(LAB_VALUE_OBJECT), calls = lab_dom_object(LAB_VALUE_ARRAY);
    lab_dom_set(plan, "calls", calls);
    lab_dom_set(plan, "install", lab_dom_scalar(LAB_VALUE_BOOLEAN, (flags & lab_eval_install) != 0));
    lab_dom_set(plan, "awaitInstall", lab_dom_scalar(LAB_VALUE_BOOLEAN, (flags & lab_eval_await) != 0));
    lab_dom_set(plan, "weather", lab_dom_scalar(LAB_VALUE_BOOLEAN, (flags & lab_eval_weather) != 0));
    int status = lab_dom_get(data, "status");
    lab_evaluation_label_t label = lab_evaluation_status(mode, outcome, lab_dom_equal(status, lab_dom_string("series")),
                                           lab_dom_equal(status, lab_dom_string("solved")));
    lab_dom_set(plan, "status", lab_dom_string(lab_evaluation_text(mode, label)));
    if (flags & lab_eval_clear_scalar)
        lab_evaluation_emit(calls, "clearMatrixScalar", 0, 0, 0);
    if (flags & lab_eval_error) {
        int fallback = lab_dom_string(lab_evaluation_text(mode, LAB_EVALUATION_LABEL_FAILURE));
        int original = lab_dom_get(data, "error");
        int trimmed = lab_dom_clean(original, LAB_DOM_CLEAN_TRIM),
            raw = lab_dom_clean(lab_dom_get(data, "raw_error"), LAB_DOM_CLEAN_TRIM);
        unsigned has_error = mode == LAB_MODE_INTEGRATOR ? lab_dom_length(trimmed) : lab_dom_truth(original);
        lab_evaluation_error_source_t source = lab_evaluation_error_source(
            mode, has_error, lab_dom_equal(trimmed, fallback), lab_dom_length(raw));
        const int messages[] = {
            [LAB_EVALUATION_ERROR_FALLBACK] = fallback,
            [LAB_EVALUATION_ERROR_ORIGINAL] = original,
            [LAB_EVALUATION_ERROR_TRIMMED]  = trimmed,
            [LAB_EVALUATION_ERROR_RAW]      = raw,
        };
        int message = lab_dom_type(error) ? error : messages[source];
        lab_evaluation_emit(calls, "setRenderedError", 1, message, 0);
        lab_evaluation_emit(calls, "resetRenderedDigits", 0, 0, 0);
    }
    if (flags & lab_eval_clear_local)
        lab_evaluation_emit(calls, "setDatetimeLocalText", 1, lab_dom_string(""), 0);
    if (flags & lab_eval_clear) {
        int options = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(options, "keepBindings", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
        lab_evaluation_emit(calls, "clearResultDetails", 1, options, 0);
    }
    if (flags & lab_eval_clear_error)
        lab_evaluation_emit(calls, "clearRenderedError", 0, 0, 0);
    if (flags & lab_eval_integrator) {
        lab_evaluation_emit(calls, "applyIntegratorBindingState", 2, data, lab_dom_get(context, "text"));
        lab_evaluation_emit(calls, "applyIntegratorResultBound", 1, data, 0);
        lab_evaluation_emit(calls, "saveWorksheetState", 1, lab_dom_string("integrator"), 0);
    }
    lab_dom_return(plan);
}
