/**
 * @file lab_flow_request_calculus.c
 * @brief Indexed calculus and plot request continuations for the Lab runtime.
 *
 * Keeps expression, matrix and scalar request preparation, publication and recovery
 * in a dedicated workflow. Stage handlers borrow a stack-local context for one
 * synchronous dispatch; only the browser-owned frame survives an asynchronous wait.
 * Mathematical text and native plot results remain opaque to this controller.
 */
#include "lab_dom.h"
#include "lab_flow.h"
#include "lab_plot_slices.h"
#define MARS_LAB_FLOW_REQUEST_INTERNAL_ACCESS
#include "lab_flow_request_internal.h"

enum {
    flow_request_calculus_stage_start,
    flow_request_calculus_stage_capture,
    flow_request_calculus_stage_commit,
    flow_request_calculus_stage_current,
    flow_request_calculus_stage_source,
    flow_request_calculus_stage_assemble,
    flow_request_calculus_stage_assembled,
    flow_request_calculus_stage_input,
    flow_request_calculus_stage_show,
    flow_request_calculus_stage_title,
    flow_request_calculus_stage_status,
    flow_request_calculus_stage_fetch,
    flow_request_calculus_stage_prepared_current,
    flow_request_calculus_stage_matrix_fetch,
    flow_request_calculus_stage_outcome,
    flow_request_calculus_stage_accept,
    flow_request_calculus_stage_clear_error,
    flow_request_calculus_stage_display,
    flow_request_calculus_stage_scalar,
    flow_request_calculus_stage_post_current,
    flow_request_calculus_stage_variables,
    flow_request_calculus_stage_differentiable,
    flow_request_calculus_stage_buttons,
    flow_request_calculus_stage_commit_state,
    flow_request_calculus_stage_ready,
    flow_request_calculus_stage_plot_commit,
    flow_request_calculus_stage_done,
    flow_request_calculus_stage_error_current,
    flow_request_calculus_stage_error_clear,
    flow_request_calculus_stage_error_variables,
    flow_request_calculus_stage_error_differentiable,
    flow_request_calculus_stage_error_buttons,
    flow_request_calculus_stage_error_render,
    flow_request_calculus_stage_error_digits,
    flow_request_calculus_stage_error_status,
    flow_request_calculus_stage_input_accept,
    flow_request_calculus_stage_return = 999
};

struct lab_flow_request_calculus_context {
    int frame;
    int request;
    int action;
    int wrt;
    int matrix;
    int scalar;
    int integral;
    int plot;
    int data;
    int result;
};

static int lab_flow_request_calculus_start(struct lab_flow_request_calculus_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_capture, "calculusInitial", "initial", 0, 0);
}

static int lab_flow_request_calculus_capture(struct lab_flow_request_calculus_context *ctx)
{
    int initial = lab_dom_get(ctx->frame, "initial"), mode = lab_dom_get(initial, "mode");
    ctx->matrix = lab_dom_equal(mode, lab_dom_string("matrix"));
    ctx->scalar = ctx->matrix && lab_dom_truth(lab_dom_get(initial, "scalar"));
    lab_dom_set(ctx->frame, "matrix", lab_dom_scalar(1, ctx->matrix));
    lab_dom_set(ctx->frame, "scalar", lab_dom_scalar(1, ctx->scalar));
    lab_dom_set(ctx->frame, "solverPlot",
                lab_dom_scalar(1, ctx->plot && (lab_dom_equal(mode, lab_dom_string("equation")) ||
                                                lab_dom_equal(mode, lab_dom_string("diffequation")))));
    if (lab_flow_request_truth(ctx->frame, "automatic")) {
        lab_dom_set(ctx->frame, "wrt", lab_dom_read(lab_dom_id("plotFirst"), 5, ""));
        return lab_flow_call(ctx->frame, flow_request_calculus_stage_current, "calculusCapture", "snapshot", 0, 0);
    }
    int options = lab_dom_object(5);
    lab_dom_set(options, "button", lab_dom_get(ctx->frame, "actionButton"));
    lab_dom_set(options, "variable", lab_dom_scalar(1, lab_dom_truth(ctx->wrt)));
    lab_dom_set(options, "scalar", lab_dom_scalar(1, ctx->scalar));
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_return, "runRequestFlow", "value", 1, 6,
                         lab_dom_numeric(7), ctx->frame, lab_dom_numeric(flow_request_calculus_stage_commit),
                         ctx->action, mode, options);
}

static int lab_flow_request_calculus_commit(struct lab_flow_request_calculus_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_current, "calculusCapture", "snapshot", 0, 0);
}

static int lab_flow_request_calculus_current(struct lab_flow_request_calculus_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(flow_request_calculus_stage_error_current));
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_source, "commitBindings", 0, 1, 0);
}

static int lab_flow_request_calculus_source(struct lab_flow_request_calculus_context *ctx)
{
    return lab_flow_request_current(ctx->frame, flow_request_calculus_stage_assemble);
}

static int lab_flow_request_calculus_assemble(struct lab_flow_request_calculus_context *ctx)
{
    if (!lab_flow_request_truth(ctx->frame, "current"))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_assembled,
                         ctx->scalar ? "matrixScalar" : "expressionText", "source", 0, 0);
}

static int lab_flow_request_calculus_assembled(struct lab_flow_request_calculus_context *ctx)
{
    if (lab_flow_request_truth(ctx->frame, "solverPlot"))
        return lab_flow_request_current(ctx->frame, flow_request_calculus_stage_input_accept);
    if (!ctx->matrix)
        return lab_flow_call(ctx->frame, flow_request_calculus_stage_input, "expressionWithVisibleBindings", "source",
                             1, 2, lab_dom_get(ctx->frame, "source"),
                             lab_dom_get(lab_dom_get(ctx->frame, "snapshot"), "bindings"));
    return lab_flow_request_current(ctx->frame, flow_request_calculus_stage_input_accept);
}

static int lab_flow_request_calculus_input(struct lab_flow_request_calculus_context *ctx)
{
    return lab_flow_request_current(ctx->frame, flow_request_calculus_stage_input_accept);
}

static int lab_flow_request_calculus_input_accept(struct lab_flow_request_calculus_context *ctx)
{
    if (!lab_flow_request_truth(ctx->frame, "current"))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_show, "requestInput", "accepted", 0, 3, ctx->request,
                         lab_dom_get(ctx->frame, "source"), ctx->wrt);
}

static int lab_flow_request_calculus_show(struct lab_flow_request_calculus_context *ctx)
{
    if (!lab_flow_request_truth(ctx->frame, "accepted"))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_title, "showResults", 0, 0, 0);
}

static int lab_flow_request_calculus_title(struct lab_flow_request_calculus_context *ctx)
{
    if (ctx->plot)
        return lab_flow_request_current(ctx->frame, flow_request_calculus_stage_fetch);
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_status, "calculusTitle", 0, 0, 1,
                         lab_dom_join(lab_dom_join(lab_dom_text(ctx->wrt), lab_dom_string(" "), ""),
                                      lab_dom_text(ctx->action), " RESULT"));
}

static int lab_flow_request_calculus_status(struct lab_flow_request_calculus_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_fetch, "setStatus", 0, 0, 1,
                         lab_dom_join(lab_dom_string(ctx->plot       ? "Plotting against "
                                                     : ctx->integral ? "Integrating with respect to "
                                                                     : "Differentiating d/d"),
                                      lab_dom_text(ctx->wrt), "..."));
}

static int lab_flow_request_calculus_fetch(struct lab_flow_request_calculus_context *ctx)
{
    if (ctx->matrix && !ctx->scalar) {
        int payload = lab_dom_object(5);
        lab_dom_set(payload, "action", lab_dom_string("editor"));
        lab_dom_set(payload, "operation", lab_dom_string("calculus"));
        lab_dom_set(payload, "calculus", ctx->action);
        lab_dom_set(payload, "text", lab_dom_get(ctx->frame, "source"));
        lab_dom_set(payload, "name", ctx->wrt);
        return lab_flow_call(ctx->frame, flow_request_calculus_stage_prepared_current, "requestPresentation",
                             "prepared", 1, 2, payload, ctx->request);
    }
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_outcome, "fetchCalculus", "result", 1, 5,
                         lab_dom_get(ctx->frame, "source"), ctx->wrt,
                         lab_dom_string(ctx->plot       ? "plot"
                                        : ctx->integral ? "integral"
                                                        : ""),
                         ctx->request, lab_dom_get(ctx->frame, "preview"));
}

static int lab_flow_request_calculus_prepared_current(struct lab_flow_request_calculus_context *ctx)
{
    return lab_flow_request_current(ctx->frame, flow_request_calculus_stage_matrix_fetch);
}

static int lab_flow_request_calculus_matrix_fetch(struct lab_flow_request_calculus_context *ctx)
{
    if (!lab_flow_request_truth(ctx->frame, "current"))
        return lab_flow_done(0);
    int options = lab_dom_object(5);
    lab_dom_set(options, "matrixText",
                lab_dom_get(lab_dom_get(lab_dom_get(ctx->frame, "prepared"), "editor"), "expression"));
    lab_dom_set(options, "operation", lab_dom_string("eval"));
    lab_dom_set(options, "operand", lab_dom_string(""));
    lab_dom_set(options, "skipSave", lab_dom_scalar(1, 1));
    lab_dom_set(options, "request", ctx->request);
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_outcome, "fetchMatrixOptions", "result", 1, 1,
                         options);
}

static int lab_flow_request_calculus_outcome(struct lab_flow_request_calculus_context *ctx)
{
    ctx->result = lab_dom_key_get(lab_dom_get(lab_dom_get(ctx->data, "presentation"), "calculus"), ctx->action);
    lab_dom_set(ctx->frame, "calculus", ctx->result);
    int available = ctx->matrix && !ctx->scalar ? lab_dom_scalar(1, 1)
                    : ctx->plot ? lab_dom_get(ctx->result, "svg")
                                : lab_flow_request_fallback(lab_dom_get(ctx->result, "expression"), "");
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_accept, "requestOutcome", "outcome", 0, 4,
                         ctx->request, lab_flow_request_response(ctx->frame), ctx->data, available);
}

static int lab_flow_request_calculus_accept(struct lab_flow_request_calculus_context *ctx)
{
    unsigned outcome = (unsigned)lab_dom_number(lab_dom_get(ctx->frame, "outcome"));
    if (!outcome)
        return lab_flow_done(0);
    if (ctx->plot && (lab_dom_truth(lab_dom_get(ctx->data, "plot_slices")) || outcome != 1))
        lab_plot_slices_apply(lab_dom_get(ctx->data, "plot_slices"), lab_dom_get(ctx->request, "sliceInput"));
    if (outcome == 1) {
        int error = lab_dom_get(ctx->data, "error");
        if (!lab_dom_truth(error))
            error = lab_dom_get(ctx->data, "raw");
        if (!lab_dom_truth(error))
            error = lab_dom_join(lab_dom_join(lab_dom_string("No "), lab_dom_text(ctx->action), " for "),
                                 lab_dom_text(ctx->wrt), "");
        if (ctx->plot && lab_flow_request_truth(ctx->frame, "live") &&
            !lab_dom_truth(lab_dom_get(ctx->data, "plot_slices"))) {
            lab_dom_write(lab_dom_id("plotFeedback"), 0, "", error);
            lab_dom_class(lab_dom_id("plotFeedback"), "hidden", 0);
            return lab_flow_done(0);
        }
        return lab_flow_call(ctx->frame,
                             ctx->plot ? flow_request_calculus_stage_plot_commit : flow_request_calculus_stage_done,
                             ctx->plot ? "plotError" : "fail", 0, 0, 1, error);
    }
    if (ctx->plot)
        return lab_flow_call(ctx->frame, flow_request_calculus_stage_plot_commit, "displayPlot", 0, 0, 1, ctx->result);
    int options = lab_dom_object(5);
    lab_dom_set(options, "keepBindings", lab_dom_scalar(1, 1));
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_clear_error, "clearResultDetails", 0, 0, 1, options);
}

static int lab_flow_request_calculus_clear_error(struct lab_flow_request_calculus_context *ctx)
{
    lab_dom_class(lab_dom_id("plotFeedback"), "hidden", 1);
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_display, "clearRenderedError", 0, 0, 0);
}

static int lab_flow_request_calculus_display(struct lab_flow_request_calculus_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_scalar,
                         ctx->matrix && !ctx->scalar ? "displayMatrixResult" : "displayCalculusResult", 0, 0, 1,
                         ctx->matrix && !ctx->scalar ? ctx->data : ctx->result);
}

static int lab_flow_request_calculus_scalar(struct lab_flow_request_calculus_context *ctx)
{
    if (ctx->matrix && !ctx->scalar)
        return lab_flow_call(ctx->frame, flow_request_calculus_stage_post_current, "valueTitle", 0, 0, 1,
                             lab_dom_string(lab_dom_truth(lab_dom_get(ctx->data, "value")) ? "Value" : "Summary"));
    if (ctx->scalar)
        return lab_flow_call(ctx->frame, flow_request_calculus_stage_post_current, "setMatrixPrettyResult", 0, 1, 2,
                             lab_dom_get(ctx->result, "expression"), lab_dom_string(""));
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_post_current, "derivative", 0, 0, 1,
                         lab_dom_equal(ctx->action, lab_dom_string("derivative"))
                             ? lab_dom_get(ctx->result, "expression")
                             : lab_dom_string(""));
}

static int lab_flow_request_calculus_post_current(struct lab_flow_request_calculus_context *ctx)
{
    return lab_flow_request_current(ctx->frame, flow_request_calculus_stage_variables);
}

static int lab_flow_request_calculus_variables(struct lab_flow_request_calculus_context *ctx)
{
    if (!lab_flow_request_truth(ctx->frame, "current"))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_differentiable,
                         ctx->matrix ? "calculusVariables" : "calculusBindingVariables", 0, 0, 1,
                         ctx->matrix ? lab_dom_get(lab_dom_get(ctx->frame, "snapshot"), "variables")
                                     : lab_dom_get(ctx->data, "binding_values"));
}

static int lab_flow_request_calculus_differentiable(struct lab_flow_request_calculus_context *ctx)
{
    int enabled =
        lab_flow_request_truth(ctx->frame, "solverPlot") ? lab_dom_scalar(1, 0)
        : ctx->matrix
            ? lab_dom_get(lab_dom_get(ctx->frame, "snapshot"), "differentiable")
            : lab_dom_scalar(1, !lab_dom_equal(lab_dom_clean(lab_dom_text(lab_flow_request_fallback(
                                                                 lab_dom_get(ctx->data, "differentiable"), "yes")),
                                                             1),
                                               lab_dom_string("no")));
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_buttons, "calculusDifferentiable", 0, 0, 1, enabled);
}

static int lab_flow_request_calculus_buttons(struct lab_flow_request_calculus_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_commit_state, "calculusButtons", 0, 0, 0);
}

static int lab_flow_request_calculus_commit_state(struct lab_flow_request_calculus_context *ctx)
{
    if (ctx->matrix && !ctx->scalar)
        return lab_flow_call(ctx->frame, flow_request_calculus_stage_ready, "commitModeState", 0, 0, 0);
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_done, "setStatus", 0, 0, 1, lab_dom_string("Ready"));
}

static int lab_flow_request_calculus_ready(struct lab_flow_request_calculus_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_done, "setStatus", 0, 0, 1, lab_dom_string("Ready"));
}

static int lab_flow_request_calculus_plot_commit(struct lab_flow_request_calculus_context *ctx)
{
    if (lab_flow_request_truth(ctx->frame, "preview"))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_done, "commitModeState", 0, 0, 0);
}

static int lab_flow_request_calculus_error_current(struct lab_flow_request_calculus_context *ctx)
{
    return lab_flow_request_current(ctx->frame, flow_request_calculus_stage_error_clear);
}

static int lab_flow_request_calculus_error_clear(struct lab_flow_request_calculus_context *ctx)
{
    if (!lab_flow_request_truth(ctx->frame, "current"))
        return lab_flow_done(0);
    if (lab_flow_request_truth(ctx->frame, "live")) {
        lab_dom_write(lab_dom_id("plotFeedback"), 0, "", lab_dom_get(ctx->frame, "error"));
        lab_dom_class(lab_dom_id("plotFeedback"), "hidden", 0);
        return lab_flow_done(0);
    }
    if (ctx->plot)
        return lab_flow_call(ctx->frame, flow_request_calculus_stage_plot_commit, "plotError", 0, 0, 1,
                             lab_dom_get(ctx->frame, "error"));
    int options = lab_dom_object(5);
    lab_dom_set(options, "keepBindings", lab_dom_scalar(1, 1));
    return lab_flow_call(ctx->frame,
                         ctx->matrix ? flow_request_calculus_stage_error_variables
                                     : flow_request_calculus_stage_error_render,
                         "clearResultDetails", 0, 0, 1, options);
}

static int lab_flow_request_calculus_error_variables(struct lab_flow_request_calculus_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_error_differentiable, "calculusVariables", 0, 0, 1,
                         lab_dom_get(lab_dom_get(ctx->frame, "snapshot"), "variables"));
}

static int lab_flow_request_calculus_error_differentiable(struct lab_flow_request_calculus_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_error_buttons, "calculusDifferentiable", 0, 0, 1,
                         lab_dom_get(lab_dom_get(ctx->frame, "snapshot"), "differentiable"));
}

static int lab_flow_request_calculus_error_buttons(struct lab_flow_request_calculus_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_error_render, "calculusButtons", 0, 0, 0);
}

static int lab_flow_request_calculus_error_render(struct lab_flow_request_calculus_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_error_digits, "setRenderedError", 0, 0, 1,
                         lab_dom_get(ctx->frame, "error"));
}

static int lab_flow_request_calculus_error_digits(struct lab_flow_request_calculus_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_error_status, "resetRenderedDigits", 0, 0, 0);
}

static int lab_flow_request_calculus_error_status(struct lab_flow_request_calculus_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_calculus_stage_done, "setStatus", 0, 0, 1, lab_dom_string("Error"));
}

static int lab_flow_request_calculus_return(struct lab_flow_request_calculus_context *ctx)
{
    return lab_flow_done(lab_dom_get(ctx->frame, "value"));
}

static int lab_flow_request_calculus_otherwise(struct lab_flow_request_calculus_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

/* Advance calculus and plot publication without retaining scoped handles. */
int lab_flow_request_calculus(int frame)
{
    int request = lab_dom_get(frame, "request"), action = lab_dom_get(frame, "action"), wrt = lab_dom_get(frame, "wrt");
    int matrix = lab_flow_request_truth(frame, "matrix"), scalar = lab_flow_request_truth(frame, "scalar");
    int integral = lab_dom_equal(action, lab_dom_string("integral"));
    int plot = lab_dom_equal(action, lab_dom_string("plot"));
    int data = lab_flow_request_data(frame), result = lab_dom_get(frame, "calculus");
    struct lab_flow_request_calculus_context ctx = {
        .frame = frame,
        .request = request,
        .action = action,
        .wrt = wrt,
        .matrix = matrix,
        .scalar = scalar,
        .integral = integral,
        .plot = plot,
        .data = data,
        .result = result,
    };
    static int (*const handlers[])(struct lab_flow_request_calculus_context *) = {
        [flow_request_calculus_stage_start]                = lab_flow_request_calculus_start,
        [flow_request_calculus_stage_capture]              = lab_flow_request_calculus_capture,
        [flow_request_calculus_stage_commit]               = lab_flow_request_calculus_commit,
        [flow_request_calculus_stage_current]              = lab_flow_request_calculus_current,
        [flow_request_calculus_stage_source]               = lab_flow_request_calculus_source,
        [flow_request_calculus_stage_assemble]             = lab_flow_request_calculus_assemble,
        [flow_request_calculus_stage_assembled]            = lab_flow_request_calculus_assembled,
        [flow_request_calculus_stage_input]                = lab_flow_request_calculus_input,
        [flow_request_calculus_stage_input_accept]         = lab_flow_request_calculus_input_accept,
        [flow_request_calculus_stage_show]                 = lab_flow_request_calculus_show,
        [flow_request_calculus_stage_title]                = lab_flow_request_calculus_title,
        [flow_request_calculus_stage_status]               = lab_flow_request_calculus_status,
        [flow_request_calculus_stage_fetch]                = lab_flow_request_calculus_fetch,
        [flow_request_calculus_stage_prepared_current]     = lab_flow_request_calculus_prepared_current,
        [flow_request_calculus_stage_matrix_fetch]         = lab_flow_request_calculus_matrix_fetch,
        [flow_request_calculus_stage_outcome]              = lab_flow_request_calculus_outcome,
        [flow_request_calculus_stage_accept]               = lab_flow_request_calculus_accept,
        [flow_request_calculus_stage_clear_error]          = lab_flow_request_calculus_clear_error,
        [flow_request_calculus_stage_display]              = lab_flow_request_calculus_display,
        [flow_request_calculus_stage_scalar]               = lab_flow_request_calculus_scalar,
        [flow_request_calculus_stage_post_current]         = lab_flow_request_calculus_post_current,
        [flow_request_calculus_stage_variables]            = lab_flow_request_calculus_variables,
        [flow_request_calculus_stage_differentiable]       = lab_flow_request_calculus_differentiable,
        [flow_request_calculus_stage_buttons]              = lab_flow_request_calculus_buttons,
        [flow_request_calculus_stage_commit_state]         = lab_flow_request_calculus_commit_state,
        [flow_request_calculus_stage_ready]                = lab_flow_request_calculus_ready,
        [flow_request_calculus_stage_plot_commit]          = lab_flow_request_calculus_plot_commit,
        [flow_request_calculus_stage_error_current]        = lab_flow_request_calculus_error_current,
        [flow_request_calculus_stage_error_clear]          = lab_flow_request_calculus_error_clear,
        [flow_request_calculus_stage_error_variables]      = lab_flow_request_calculus_error_variables,
        [flow_request_calculus_stage_error_differentiable] = lab_flow_request_calculus_error_differentiable,
        [flow_request_calculus_stage_error_buttons]        = lab_flow_request_calculus_error_buttons,
        [flow_request_calculus_stage_error_render]         = lab_flow_request_calculus_error_render,
        [flow_request_calculus_stage_error_digits]         = lab_flow_request_calculus_error_digits,
        [flow_request_calculus_stage_error_status]         = lab_flow_request_calculus_error_status,
    };
    unsigned selected = lab_flow_request_stage(frame);
    if (selected == flow_request_calculus_stage_return)
        return lab_flow_request_calculus_return(&ctx);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_request_calculus_otherwise(&ctx);
}
