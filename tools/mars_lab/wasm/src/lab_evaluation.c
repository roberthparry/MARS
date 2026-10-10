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
#include "lab_dom.h"
#include "lab_evaluation.h"

enum { lab_evaluation_modes = 7 };

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

static const unsigned lab_evaluation_preparation[lab_evaluation_modes] = {
    [0] = lab_eval_bound_input | lab_eval_expression,
    [1] = lab_eval_plain_input | lab_eval_bindings,
    [2] = lab_eval_plain_input,
    [3] = lab_eval_bound_input,
    [4] = lab_eval_bound_input,
    [5] = lab_eval_datetime,
    [6] = lab_eval_almanac};

static const struct {
    unsigned success, failure;
} lab_evaluation_policy[lab_evaluation_modes] = {
    [0] = {lab_eval_install, lab_eval_failure},
    [1] = {lab_eval_replace, lab_eval_failure},
    [2] = {lab_eval_replace | lab_eval_await, lab_eval_failure},
    [3] = {lab_eval_replace, lab_eval_failure | lab_eval_clear_scalar},
    [4] = {lab_eval_replace, lab_eval_failure | lab_eval_integrator},
    [5] = {lab_eval_replace | lab_eval_weather, lab_eval_failure | lab_eval_clear_local},
    [6] = {lab_eval_replace, lab_eval_failure}};

static const struct {
    const char *working, *failure;
} lab_evaluation_labels[lab_evaluation_modes] = {
    [0] = {"Evaluating...", "Evaluation failed"},
    [1] = {"Solving equation...", "Equation solving failed"},
    [2] = {"Solving differential equation...", "Differential-equation solving failed"},
    [3] = {"Evaluating matrix...", "Matrix evaluation failed"},
    [4] = {"Integrating...", "Integration failed"},
    [5] = {"Calculating dates...", "Datetime calculation failed"},
    [6] = {"Working the almanac...", "Almanac calculation failed"}};

/* Preparation precedes the guarded result phase, preserving setup-error history semantics. */
unsigned lab_evaluation_prepare(unsigned mode, unsigned skip_history)
{
    if (mode >= lab_evaluation_modes)
        return 0;
    return lab_evaluation_preparation[mode] | (skip_history ? 0u : lab_eval_history);
}

/* Outcomes are stale 0, failure 1, success 2, partial expression 3 and caught exception 4. */
unsigned lab_evaluation_actions(unsigned mode, unsigned outcome)
{
    if (mode >= lab_evaluation_modes || outcome == 0 || outcome > 4)
        return 0;
    if (outcome == 1)
        return lab_evaluation_policy[mode].failure;
    if (outcome == 2)
        return lab_evaluation_policy[mode].success;
    if (outcome == 3)
        return mode == 0 ? lab_eval_install | lab_eval_error : 0;
    return lab_eval_failure | (mode == 5 ? lab_eval_clear_local : 0u);
}

/* Borrow immutable labels: working 0, failure 1, ready 2, error 3, local series 4, unsolved 5. */
const char *lab_evaluation_text(unsigned mode, unsigned field)
{
    static const char *const statuses[] = {"Ready", "Error", "Local series", "Not solved"};
    if (mode >= lab_evaluation_modes || field > 5)
        return "";
    if (field < 2)
        return field == 0 ? lab_evaluation_labels[mode].working : lab_evaluation_labels[mode].failure;
    return statuses[field - 2];
}

/* Only short static UI labels are scanned; authored or mathematical text never enters this module. */
unsigned lab_evaluation_text_length(unsigned mode, unsigned field)
{
    const char *text = lab_evaluation_text(mode, field);
    unsigned length = 0;
    while (text[length])
        ++length;
    return length;
}

/* Differential-equation statuses describe solver completion without interpreting its result. */
unsigned lab_evaluation_status(unsigned mode, unsigned outcome, unsigned series, unsigned solved)
{
    if (!lab_evaluation_actions(mode, outcome))
        return 6;
    if (outcome != 2)
        return 3;
    return mode == 2 ? (series ? 4u : solved ? 2u : 5u) : 2u;
}

/* Select error text: fallback 0, original 1, trimmed integration diagnostic 2, trimmed raw error 3. */
unsigned lab_evaluation_error_source(unsigned mode, unsigned has_error, unsigned generic, unsigned has_raw)
{
    if (mode >= lab_evaluation_modes)
        return 0;
    if (mode != 4)
        return has_error ? 1u : 0u;
    if (has_error && !generic)
        return 2;
    return has_raw ? 3u : has_error ? 2u : 0u;
}

/* Expose a label through the scoped bridge without duplicating UTF-8 buffer access in the adapter. */
void lab_evaluation_label(unsigned mode, unsigned field)
{
    lab_dom_return(lab_dom_string(lab_evaluation_text(mode, field)));
}

/* Name the preparation capabilities in C; the host only performs the required asynchronous work. */
void lab_evaluation_prepare_plan(unsigned mode, int skip_history)
{
    unsigned flags = lab_evaluation_prepare(mode, skip_history);
    if (!flags) {
        lab_dom_return(0);
        return;
    }
    static const char *const names[] = {"bindings", "expression", "input", "trim", "datetime", "almanac", "history"};
    int plan = lab_dom_object(5);
    for (unsigned i = 0; i < sizeof names / sizeof *names; ++i)
        lab_dom_set(plan, names[i], lab_dom_scalar(1, (flags & (1u << i)) != 0));
    lab_dom_set(plan, "label", lab_dom_string(lab_evaluation_text(mode, 0)));
    lab_dom_return(plan);
}

static void lab_evaluation_emit(int calls, const char *service, unsigned count, int first, int second)
{
    int call = lab_dom_object(5), args = lab_dom_object(4);
    if (count)
        lab_dom_push(args, first);
    if (count > 1)
        lab_dom_push(args, second);
    lab_dom_set(call, "service", lab_dom_string(service));
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
}

/* Return ordered recovery services and outcome metadata; no side effects occur inside this scope. */
void lab_evaluation_response(unsigned mode, unsigned outcome, int data, int context, int error)
{
    unsigned flags = lab_evaluation_actions(mode, outcome);
    if (!flags) {
        lab_dom_return(0);
        return;
    }
    int plan = lab_dom_object(5), calls = lab_dom_object(4);
    lab_dom_set(plan, "calls", calls);
    lab_dom_set(plan, "install", lab_dom_scalar(1, (flags & lab_eval_install) != 0));
    lab_dom_set(plan, "awaitInstall", lab_dom_scalar(1, (flags & lab_eval_await) != 0));
    lab_dom_set(plan, "weather", lab_dom_scalar(1, (flags & lab_eval_weather) != 0));
    int status = lab_dom_get(data, "status");
    unsigned label = lab_evaluation_status(mode, outcome, lab_dom_equal(status, lab_dom_string("series")),
                                           lab_dom_equal(status, lab_dom_string("solved")));
    lab_dom_set(plan, "status", lab_dom_string(lab_evaluation_text(mode, label)));
    if (flags & lab_eval_clear_scalar)
        lab_evaluation_emit(calls, "clearMatrixScalar", 0, 0, 0);
    if (flags & lab_eval_error) {
        int fallback = lab_dom_string(lab_evaluation_text(mode, 1)), original = lab_dom_get(data, "error");
        int trimmed = lab_dom_clean(original, 0), raw = lab_dom_clean(lab_dom_get(data, "raw_error"), 0);
        unsigned source =
            lab_evaluation_error_source(mode, mode == 4 ? lab_dom_length(trimmed) : lab_dom_truth(original),
                                        lab_dom_equal(trimmed, fallback), lab_dom_length(raw));
        const int messages[] = {fallback, original, trimmed, raw};
        int message = lab_dom_type(error) ? error : messages[source];
        lab_evaluation_emit(calls, "setRenderedError", 1, message, 0);
        lab_evaluation_emit(calls, "resetRenderedDigits", 0, 0, 0);
    }
    if (flags & lab_eval_clear_local)
        lab_evaluation_emit(calls, "setDatetimeLocalText", 1, lab_dom_string(""), 0);
    if (flags & lab_eval_clear) {
        int options = lab_dom_object(5);
        lab_dom_set(options, "keepBindings", lab_dom_scalar(1, 1));
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
