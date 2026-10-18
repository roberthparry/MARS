/**
 * @file lab_flow_binding_edit.c
 * @brief Native binding projection and delayed edited-source refresh workflows.
 *
 * Coordinates native MARS metadata requests without interpreting mathematical
 * source. Exact editor snapshots and request ownership suppress stale results;
 * whitespace-insensitive matching is used only for the edited-body workflow.
 * Browser values are stored in request-local frames across asynchronous yields.
 */
#include "lab_dom.h"
#include "lab_flow.h"
#include "lab_flow_binding.h"

static unsigned lab_flow_binding_edit_stage(frame_jsref frame)
{
    return (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
}

static int lab_flow_binding_edit_success(frame_jsref frame)
{
    int result = lab_dom_get(frame, "result");
    return lab_dom_truth(lab_dom_get(lab_dom_get(result, "response"), "ok")) &&
           lab_dom_truth(lab_dom_get(lab_dom_get(result, "data"), "ok"));
}

static int lab_flow_binding_edit_matches(frame_jsref frame, int trimmed)
{
    int text = lab_dom_get(lab_dom_get(frame, "after"), "text");
    return lab_dom_equal(trimmed ? lab_dom_clean(text, LAB_DOM_CLEAN_TRIM) : text,
                         lab_dom_get(frame, trimmed ? "editedBody" : "editorSnapshot"));
}

enum {
    flow_binding_edit_projection_stage_status,
    flow_binding_edit_projection_stage_fetch,
    flow_binding_edit_projection_stage_validate,
    flow_binding_edit_projection_stage_owner,
    flow_binding_edit_projection_stage_capture,
    flow_binding_edit_projection_stage_bindings,
    flow_binding_edit_projection_stage_body_select,
    flow_binding_edit_projection_stage_body,
    flow_binding_edit_projection_stage_assemble,
    flow_binding_edit_projection_stage_body_owner,
    flow_binding_edit_projection_stage_body_capture,
    flow_binding_edit_projection_stage_body_accept,
    flow_binding_edit_projection_stage_history,
    flow_binding_edit_projection_stage_save,
    flow_binding_edit_projection_stage_ready,
    flow_binding_edit_projection_stage_done,
    flow_binding_edit_projection_stage_error_owner,
    flow_binding_edit_projection_stage_error_report,
    flow_binding_edit_projection_stage_failed
};

struct lab_flow_binding_edit_projection_context {
    frame_jsref frame;
    browser_jsref data;
};


static browser_jsref lab_flow_binding_edit_projection_status(struct lab_flow_binding_edit_projection_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_edit_projection_stage_fetch, "setStatus", 0, 0, 1,
                         lab_dom_string("Updating bindings..."));
}

static browser_jsref lab_flow_binding_edit_projection_fetch(struct lab_flow_binding_edit_projection_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(flow_binding_edit_projection_stage_error_owner));
    return lab_flow_call(ctx->frame, flow_binding_edit_projection_stage_validate, "bindingFetch", "result", 1, 3,
                         lab_dom_get(ctx->frame, "updated"), lab_dom_string(""), lab_dom_get(ctx->frame, "request"));
}

static browser_jsref lab_flow_binding_edit_projection_validate(struct lab_flow_binding_edit_projection_context *ctx)
{
    if (!lab_flow_binding_edit_success(ctx->frame)) {
        int error = lab_dom_get(ctx->data, "error");
        return lab_flow_call(ctx->frame, flow_binding_edit_projection_stage_failed, "fail", 0, 0, 1,
                             lab_dom_truth(error) ? error : lab_dom_string("MARS could not update the bindings"));
    }
    return lab_flow_call(ctx->frame, flow_binding_edit_projection_stage_owner, "bindingPrepare", 0, 1, 1,
                         lab_dom_get(ctx->frame, "updated"));
}

static browser_jsref lab_flow_binding_edit_projection_body_owner(struct lab_flow_binding_edit_projection_context *ctx)
{
    return lab_flow_call(ctx->frame,
                         lab_flow_binding_edit_stage(ctx->frame) == flow_binding_edit_projection_stage_owner
                             ? flow_binding_edit_projection_stage_capture
                             : flow_binding_edit_projection_stage_body_capture,
                         "requestCurrent", "current", 0, 1, lab_dom_get(ctx->frame, "request"));
}

static browser_jsref lab_flow_binding_edit_projection_body_capture(struct lab_flow_binding_edit_projection_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_done(lab_dom_scalar(LAB_VALUE_BOOLEAN, 0));
    return lab_flow_call(ctx->frame,
                         lab_flow_binding_edit_stage(ctx->frame) == flow_binding_edit_projection_stage_capture
                             ? flow_binding_edit_projection_stage_bindings
                             : flow_binding_edit_projection_stage_body_accept,
                         "bindingCapture", "after", 0, 0);
}

static browser_jsref lab_flow_binding_edit_projection_bindings(struct lab_flow_binding_edit_projection_context *ctx)
{
    if (!lab_flow_binding_edit_matches(ctx->frame, 0))
        return lab_flow_done(lab_dom_scalar(LAB_VALUE_BOOLEAN, 0));
    int bindings = lab_dom_get(ctx->data, "binding_values");
    if (lab_dom_type(bindings) != LAB_VALUE_ARRAY)
        bindings = lab_dom_object(LAB_VALUE_ARRAY);
    return lab_flow_call(ctx->frame, flow_binding_edit_projection_stage_body_select, "bindingAuthored", "bindings", 0,
                         2, bindings, lab_dom_get(ctx->frame, "updated"));
}

static browser_jsref lab_flow_binding_edit_projection_body_select(struct lab_flow_binding_edit_projection_context *ctx)
{
    if (lab_dom_get(ctx->frame, "editorBodyText"))
        return lab_flow_call(ctx->frame, flow_binding_edit_projection_stage_body, "bindingPrepare", 0, 1, 1,
                             lab_dom_get(ctx->frame, "editorBodyText"));
    return lab_flow_call(ctx->frame, flow_binding_edit_projection_stage_history, "bindingSetEditor", 0, 0, 4,
                         lab_dom_get(ctx->frame, "updated"), lab_dom_get(ctx->frame, "bindings"), 0,
                         lab_dom_get(ctx->data, "evaluation_ready"));
}

static browser_jsref lab_flow_binding_edit_projection_body(struct lab_flow_binding_edit_projection_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_edit_projection_stage_assemble, "bindingBody", "editorBody", 0, 1,
                         lab_dom_get(ctx->frame, "editorBodyText"));
}

static browser_jsref lab_flow_binding_edit_projection_assemble(struct lab_flow_binding_edit_projection_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_edit_projection_stage_body_owner, "expressionWithBindings",
                         "assembled", 1, 2, lab_dom_get(ctx->frame, "editorBody"), lab_dom_get(ctx->frame, "bindings"));
}

static browser_jsref lab_flow_binding_edit_projection_body_accept(struct lab_flow_binding_edit_projection_context *ctx)
{
    if (!lab_flow_binding_edit_matches(ctx->frame, 0))
        return lab_flow_done(lab_dom_scalar(LAB_VALUE_BOOLEAN, 0));
    int assembled = lab_dom_get(ctx->frame, "assembled");
    if (!lab_dom_truth(assembled))
        assembled = lab_dom_get(ctx->frame, "editorBody");
    return lab_flow_call(ctx->frame, flow_binding_edit_projection_stage_history, "bindingSetEditor", 0, 0, 4, assembled,
                         lab_dom_get(ctx->frame, "bindings"), lab_dom_get(ctx->frame, "editorBody"),
                         lab_dom_get(ctx->data, "evaluation_ready"));
}

static browser_jsref lab_flow_binding_edit_projection_history(struct lab_flow_binding_edit_projection_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_edit_projection_stage_save, "updateHistoryButtons", 0, 0, 0);
}

static browser_jsref lab_flow_binding_edit_projection_save(struct lab_flow_binding_edit_projection_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_edit_projection_stage_ready, "bindingSaveMode", 0, 0, 0);
}

static browser_jsref lab_flow_binding_edit_projection_ready(struct lab_flow_binding_edit_projection_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_edit_projection_stage_done, "setStatus", 0, 0, 1,
                         lab_dom_string("Ready"));
}

static browser_jsref lab_flow_binding_edit_projection_done(struct lab_flow_binding_edit_projection_context *ctx)
{
    (void)ctx;
    return lab_flow_done(lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
}

static browser_jsref lab_flow_binding_edit_projection_error_owner(struct lab_flow_binding_edit_projection_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_edit_projection_stage_error_report, "requestCurrent", "current", 0, 1,
                         lab_dom_get(ctx->frame, "request"));
}

static browser_jsref lab_flow_binding_edit_projection_error_report(struct lab_flow_binding_edit_projection_context *ctx)
{
    if (lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_call(ctx->frame, flow_binding_edit_projection_stage_failed, "setStatus", 0, 0, 1,
                             lab_dom_get(ctx->frame, "error"));
    return lab_flow_done(lab_dom_scalar(LAB_VALUE_BOOLEAN, 0));
}

static browser_jsref lab_flow_binding_edit_projection_otherwise(struct lab_flow_binding_edit_projection_context *ctx)
{
    (void)ctx;
    return lab_flow_done(lab_dom_scalar(LAB_VALUE_BOOLEAN, 0));
}

static browser_jsref lab_flow_binding_edit_projection(frame_jsref frame)
{
    int data = lab_dom_get(lab_dom_get(frame, "result"), "data");
    struct lab_flow_binding_edit_projection_context ctx = {
        .frame = frame,
        .data = data,
    };
    static browser_jsref (*const handlers[])(struct lab_flow_binding_edit_projection_context *) = {
        [flow_binding_edit_projection_stage_status]       = lab_flow_binding_edit_projection_status,
        [flow_binding_edit_projection_stage_fetch]        = lab_flow_binding_edit_projection_fetch,
        [flow_binding_edit_projection_stage_validate]     = lab_flow_binding_edit_projection_validate,
        [flow_binding_edit_projection_stage_owner]        = lab_flow_binding_edit_projection_body_owner,
        [flow_binding_edit_projection_stage_body_owner]   = lab_flow_binding_edit_projection_body_owner,
        [flow_binding_edit_projection_stage_capture]      = lab_flow_binding_edit_projection_body_capture,
        [flow_binding_edit_projection_stage_body_capture] = lab_flow_binding_edit_projection_body_capture,
        [flow_binding_edit_projection_stage_bindings]     = lab_flow_binding_edit_projection_bindings,
        [flow_binding_edit_projection_stage_body_select]  = lab_flow_binding_edit_projection_body_select,
        [flow_binding_edit_projection_stage_body]         = lab_flow_binding_edit_projection_body,
        [flow_binding_edit_projection_stage_assemble]     = lab_flow_binding_edit_projection_assemble,
        [flow_binding_edit_projection_stage_body_accept]  = lab_flow_binding_edit_projection_body_accept,
        [flow_binding_edit_projection_stage_history]      = lab_flow_binding_edit_projection_history,
        [flow_binding_edit_projection_stage_save]         = lab_flow_binding_edit_projection_save,
        [flow_binding_edit_projection_stage_ready]        = lab_flow_binding_edit_projection_ready,
        [flow_binding_edit_projection_stage_done]         = lab_flow_binding_edit_projection_done,
        [flow_binding_edit_projection_stage_error_owner]  = lab_flow_binding_edit_projection_error_owner,
        [flow_binding_edit_projection_stage_error_report] = lab_flow_binding_edit_projection_error_report,
    };
    unsigned selected = lab_flow_binding_edit_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_binding_edit_projection_otherwise(&ctx);
}

enum {
    flow_binding_edit_apply_stage_inspect,
    flow_binding_edit_apply_stage_authored,
    flow_binding_edit_apply_stage_assemble,
    flow_binding_edit_apply_stage_owner,
    flow_binding_edit_apply_stage_capture,
    flow_binding_edit_apply_stage_apply,
    flow_binding_edit_apply_stage_done
};

static browser_jsref lab_flow_binding_edit_apply_inspect(frame_jsref frame)
{
    return lab_flow_call(frame, flow_binding_edit_apply_stage_authored, "bindingInspectText", "inspected", 0, 1,
                         lab_dom_get(frame, "editedBody"));
}

static browser_jsref lab_flow_binding_edit_apply_authored(frame_jsref frame)
{
    int inspected = lab_dom_get(frame, "inspected");
    int source = lab_dom_truth(lab_dom_get(inspected, "parts")) ? lab_dom_get(frame, "editedBody")
                                                                : lab_dom_get(frame, "sourceExpression");
    return lab_flow_call(frame, flow_binding_edit_apply_stage_assemble, "bindingAuthored", "bindings", 0, 2,
                         lab_dom_get(lab_dom_get(frame, "data"), "binding_values"), source);
}

static browser_jsref lab_flow_binding_edit_apply_assemble(frame_jsref frame)
{
    return lab_flow_call(frame, flow_binding_edit_apply_stage_owner, "expressionWithBindings", "assembled", 1, 2,
                         lab_dom_get(lab_dom_get(frame, "inspected"), "body"), lab_dom_get(frame, "bindings"));
}

static browser_jsref lab_flow_binding_edit_apply_owner(frame_jsref frame)
{
    return lab_flow_call(frame, flow_binding_edit_apply_stage_capture, "requestCurrent", "current", 0, 1,
                         lab_dom_get(frame, "request"));
}

static browser_jsref lab_flow_binding_edit_apply_capture(frame_jsref frame)
{
    if (!lab_dom_truth(lab_dom_get(frame, "current")))
        return lab_flow_done(0);
    return lab_flow_call(frame, flow_binding_edit_apply_stage_apply, "bindingCapture", "after", 0, 0);
}

static browser_jsref lab_flow_binding_edit_apply_apply(frame_jsref frame)
{
    if (!lab_flow_binding_edit_matches(frame, 1))
        return lab_flow_done(0);
    int body = lab_dom_get(lab_dom_get(frame, "inspected"), "body"), assembled = lab_dom_get(frame, "assembled");
    if (!lab_dom_truth(assembled))
        assembled = body;
    return lab_flow_call(frame, flow_binding_edit_apply_stage_done, "bindingProjectEdited", 0, 0, 4, assembled, body,
                         lab_dom_get(frame, "bindings"), lab_dom_get(frame, "data"));
}

static browser_jsref lab_flow_binding_edit_apply_otherwise(frame_jsref frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static browser_jsref lab_flow_binding_edit_apply(frame_jsref frame)
{
    static browser_jsref (*const handlers[])(browser_jsref) = {
        [flow_binding_edit_apply_stage_inspect]  = lab_flow_binding_edit_apply_inspect,
        [flow_binding_edit_apply_stage_authored] = lab_flow_binding_edit_apply_authored,
        [flow_binding_edit_apply_stage_assemble] = lab_flow_binding_edit_apply_assemble,
        [flow_binding_edit_apply_stage_owner]    = lab_flow_binding_edit_apply_owner,
        [flow_binding_edit_apply_stage_capture]  = lab_flow_binding_edit_apply_capture,
        [flow_binding_edit_apply_stage_apply]    = lab_flow_binding_edit_apply_apply,
    };
    unsigned selected = lab_flow_binding_edit_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](frame)
               : lab_flow_binding_edit_apply_otherwise(frame);
}

enum {
    flow_binding_edit_refresh_stage_fetch,
    flow_binding_edit_refresh_stage_owner,
    flow_binding_edit_refresh_stage_capture,
    flow_binding_edit_refresh_stage_validate,
    flow_binding_edit_refresh_stage_prepare_source,
    flow_binding_edit_refresh_stage_recheck,
    flow_binding_edit_refresh_stage_recapture,
    flow_binding_edit_refresh_stage_apply,
    flow_binding_edit_refresh_stage_history,
    flow_binding_edit_refresh_stage_done,
    flow_binding_edit_refresh_stage_error_owner,
    flow_binding_edit_refresh_stage_error_capture,
    flow_binding_edit_refresh_stage_error_mark
};

struct lab_flow_binding_edit_refresh_context {
    frame_jsref frame;
    unsigned int stage;
};


static browser_jsref lab_flow_binding_edit_refresh_fetch(struct lab_flow_binding_edit_refresh_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(flow_binding_edit_refresh_stage_error_owner));
    return lab_flow_call(ctx->frame, flow_binding_edit_refresh_stage_owner, "bindingFetch", "result", 1, 3,
                         lab_dom_get(ctx->frame, "editedBody"), lab_dom_get(ctx->frame, "sourceExpression"),
                         lab_dom_get(ctx->frame, "request"));
}

static browser_jsref lab_flow_binding_edit_refresh_error_owner(struct lab_flow_binding_edit_refresh_context *ctx)
{
    return lab_flow_call(ctx->frame,
                         ctx->stage == flow_binding_edit_refresh_stage_owner ? flow_binding_edit_refresh_stage_capture
                         : ctx->stage == flow_binding_edit_refresh_stage_recheck
                             ? flow_binding_edit_refresh_stage_recapture
                             : flow_binding_edit_refresh_stage_error_capture,
                         "requestCurrent", "current", 0, 1, lab_dom_get(ctx->frame, "request"));
}

static browser_jsref lab_flow_binding_edit_refresh_error_capture(struct lab_flow_binding_edit_refresh_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_done(0);
    return lab_flow_call(
        ctx->frame,
        ctx->stage == flow_binding_edit_refresh_stage_capture     ? flow_binding_edit_refresh_stage_validate
        : ctx->stage == flow_binding_edit_refresh_stage_recapture ? flow_binding_edit_refresh_stage_apply
                                                                  : flow_binding_edit_refresh_stage_error_mark,
        "bindingCapture", "after", 0, 0);
}

static browser_jsref lab_flow_binding_edit_refresh_validate(struct lab_flow_binding_edit_refresh_context *ctx)
{
    if (!lab_flow_binding_edit_matches(ctx->frame, 1))
        return lab_flow_done(0);
    if (!lab_flow_binding_edit_success(ctx->frame))
        return lab_flow_call(ctx->frame, flow_binding_edit_refresh_stage_history, "bindingMarkInvalid", 0, 0, 0);
    return lab_flow_call(ctx->frame, flow_binding_edit_refresh_stage_prepare_source, "bindingPrepare", 0, 1, 1,
                         lab_dom_get(ctx->frame, "editedBody"));
}

static browser_jsref lab_flow_binding_edit_refresh_prepare_source(struct lab_flow_binding_edit_refresh_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_edit_refresh_stage_recheck, "bindingPrepare", 0, 1, 1,
                         lab_dom_get(ctx->frame, "sourceExpression"));
}

static browser_jsref lab_flow_binding_edit_refresh_apply(struct lab_flow_binding_edit_refresh_context *ctx)
{
    if (!lab_flow_binding_edit_matches(ctx->frame, 1))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, flow_binding_edit_refresh_stage_history, "bindingApplyEdited", 0, 1, 4,
                         lab_dom_get(ctx->frame, "editedBody"), lab_dom_get(ctx->frame, "sourceExpression"),
                         lab_dom_get(lab_dom_get(ctx->frame, "result"), "data"), lab_dom_get(ctx->frame, "request"));
}

static browser_jsref lab_flow_binding_edit_refresh_error_mark(struct lab_flow_binding_edit_refresh_context *ctx)
{
    if (!lab_flow_binding_edit_matches(ctx->frame, 1))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, flow_binding_edit_refresh_stage_history, "bindingMarkInvalid", 0, 0, 0);
}

static browser_jsref lab_flow_binding_edit_refresh_history(struct lab_flow_binding_edit_refresh_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_edit_refresh_stage_done, "updateHistoryButtons", 0, 0, 0);
}

static browser_jsref lab_flow_binding_edit_refresh_otherwise(struct lab_flow_binding_edit_refresh_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static browser_jsref lab_flow_binding_edit_refresh(frame_jsref frame)
{
    unsigned stage = lab_flow_binding_edit_stage(frame);
    struct lab_flow_binding_edit_refresh_context ctx = {
        .frame = frame,
        .stage = stage,
    };
    static browser_jsref (*const handlers[])(struct lab_flow_binding_edit_refresh_context *) = {
        [flow_binding_edit_refresh_stage_fetch]          = lab_flow_binding_edit_refresh_fetch,
        [flow_binding_edit_refresh_stage_owner]          = lab_flow_binding_edit_refresh_error_owner,
        [flow_binding_edit_refresh_stage_recheck]        = lab_flow_binding_edit_refresh_error_owner,
        [flow_binding_edit_refresh_stage_error_owner]    = lab_flow_binding_edit_refresh_error_owner,
        [flow_binding_edit_refresh_stage_capture]        = lab_flow_binding_edit_refresh_error_capture,
        [flow_binding_edit_refresh_stage_recapture]      = lab_flow_binding_edit_refresh_error_capture,
        [flow_binding_edit_refresh_stage_error_capture]  = lab_flow_binding_edit_refresh_error_capture,
        [flow_binding_edit_refresh_stage_validate]       = lab_flow_binding_edit_refresh_validate,
        [flow_binding_edit_refresh_stage_prepare_source] = lab_flow_binding_edit_refresh_prepare_source,
        [flow_binding_edit_refresh_stage_apply]          = lab_flow_binding_edit_refresh_apply,
        [flow_binding_edit_refresh_stage_error_mark]     = lab_flow_binding_edit_refresh_error_mark,
        [flow_binding_edit_refresh_stage_history]        = lab_flow_binding_edit_refresh_history,
    };
    unsigned selected = stage;
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_binding_edit_refresh_otherwise(&ctx);
}

/* Keep editor workflows separately readable while sharing the binding flow range. */
browser_jsref lab_flow_binding_edit(lab_flow_binding_edit_kind_t kind, frame_jsref frame)
{
    static browser_jsref (*const handlers[])(browser_jsref) = {
        [LAB_FLOW_BINDING_EDIT_PROJECT] = lab_flow_binding_edit_projection,
        [LAB_FLOW_BINDING_EDIT_APPLY]   = lab_flow_binding_edit_apply,
        [LAB_FLOW_BINDING_EDIT_REFRESH] = lab_flow_binding_edit_refresh,
    };
    return (unsigned)kind < sizeof handlers / sizeof *handlers ? handlers[kind](frame) : lab_flow_done(0);
}
