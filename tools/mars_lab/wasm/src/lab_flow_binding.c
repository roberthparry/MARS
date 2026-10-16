/**
 * @file lab_flow_binding.c
 * @brief Binding request, freshness and control-event continuations for the Lab.
 *
 * Moves asynchronous binding decisions into C. Frames contain ordinary browser
 * values between yields, not retained handles. Workflows and their continuation
 * stages use bounds-checked, directly indexed function-pointer dispatch. Source and binding values remain opaque
 * mathematics.
 */
#include "lab_dom.h"
#include "lab_flow.h"
#include "lab_flow_binding.h"

static unsigned lab_flow_binding_stage(int frame)
{
    return (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
}

static int lab_flow_binding_false(void)
{
    return lab_flow_done(lab_dom_scalar(1, 0));
}

static int lab_flow_binding_same(int frame, int source)
{
    int before = lab_dom_get(frame, "before"), after = lab_dom_get(frame, "after");
    return lab_dom_equal(lab_dom_get(before, "mode"), lab_dom_get(after, "mode")) &&
           lab_dom_equal(lab_dom_get(before, "text"), lab_dom_get(after, "text")) &&
           (!source || lab_dom_equal(lab_dom_get(before, "source"), lab_dom_get(after, "source")));
}

enum {
    flow_binding_sync_stage_plan,
    flow_binding_sync_stage_select,
    flow_binding_sync_stage_prepared,
    flow_binding_sync_stage_assembled,
    flow_binding_sync_stage_done
};

static int lab_flow_binding_sync_plan(int frame)
{
    return lab_flow_call(frame, flow_binding_sync_stage_select, "native", "plan", 0, 2,
                         lab_dom_string("lab_binding_sync_request"), frame);
}

static int lab_flow_binding_sync_select(int frame)
{
    int plan = lab_dom_get(frame, "plan");
    if (lab_dom_truth(lab_dom_get(plan, "invalid")))
        return lab_flow_call(frame, flow_binding_sync_stage_prepared, "bindingInvalid", 0, 0, 0);
    int request = lab_dom_get(plan, "request");
    if (lab_dom_truth(request))
        return lab_flow_call(frame, flow_binding_sync_stage_assembled, "bindingPresentation", "data", 1, 1, request);
    int source = lab_dom_get(plan, "source");
    if (lab_dom_truth(source))
        return lab_flow_call(frame, flow_binding_sync_stage_prepared, "bindingPrepare", 0, 1, 1, source);
    return lab_flow_done(source);
}

static int lab_flow_binding_sync_prepared(int frame)
{
    return lab_flow_done(lab_dom_get(lab_dom_get(frame, "plan"), "source"));
}

static int lab_flow_binding_sync_assembled(int frame)
{
    return lab_flow_call(frame, flow_binding_sync_stage_done, "bindingExpression", "expression", 0, 1,
                         lab_dom_get(frame, "data"));
}

static int lab_flow_binding_sync_done(int frame)
{
    return lab_flow_done(lab_dom_get(frame, "expression"));
}

static int lab_flow_binding_sync_otherwise(int frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static int lab_flow_binding_sync(int frame)
{
    static int (*const handlers[])(int) = {
        [flow_binding_sync_stage_plan]      = lab_flow_binding_sync_plan,
        [flow_binding_sync_stage_select]    = lab_flow_binding_sync_select,
        [flow_binding_sync_stage_prepared]  = lab_flow_binding_sync_prepared,
        [flow_binding_sync_stage_assembled] = lab_flow_binding_sync_assembled,
        [flow_binding_sync_stage_done]      = lab_flow_binding_sync_done,
    };
    unsigned selected = lab_flow_binding_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](frame)
                                                                               : lab_flow_binding_sync_otherwise(frame);
}

enum {
    flow_binding_visible_stage_check,
    flow_binding_visible_stage_wait,
    flow_binding_visible_stage_recheck,
    flow_binding_visible_stage_commit,
    flow_binding_visible_stage_done
};

static int lab_flow_binding_visible_check(int frame)
{
    return lab_flow_call(frame, flow_binding_visible_stage_wait, "bindingCurrent", "current", 0, 1,
                         lab_dom_get(frame, "isCurrent"));
}

static int lab_flow_binding_visible_wait(int frame)
{
    if (!lab_dom_truth(lab_dom_get(frame, "current")))
        return lab_flow_binding_false();
    return lab_flow_call(frame, flow_binding_visible_stage_recheck, "bindingPending", 0, 1, 0);
}

static int lab_flow_binding_visible_recheck(int frame)
{
    return lab_flow_call(frame, flow_binding_visible_stage_commit, "bindingCurrent", "current", 0, 1,
                         lab_dom_get(frame, "isCurrent"));
}

static int lab_flow_binding_visible_commit(int frame)
{
    if (!lab_dom_truth(lab_dom_get(frame, "current")))
        return lab_flow_binding_false();
    return lab_flow_call(frame, flow_binding_visible_stage_done, "bindingCommitVisible", "result", 1, 1,
                         lab_dom_get(frame, "isCurrent"));
}

static int lab_flow_binding_visible_done(int frame)
{
    return lab_flow_done(lab_dom_get(frame, "result"));
}

static int lab_flow_binding_visible_otherwise(int frame)
{
    (void)frame;
    return lab_flow_binding_false();
}

static int lab_flow_binding_visible(int frame)
{
    static int (*const handlers[])(int) = {
        [flow_binding_visible_stage_check]   = lab_flow_binding_visible_check,
        [flow_binding_visible_stage_wait]    = lab_flow_binding_visible_wait,
        [flow_binding_visible_stage_recheck] = lab_flow_binding_visible_recheck,
        [flow_binding_visible_stage_commit]  = lab_flow_binding_visible_commit,
        [flow_binding_visible_stage_done]    = lab_flow_binding_visible_done,
    };
    unsigned selected = lab_flow_binding_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](frame)
               : lab_flow_binding_visible_otherwise(frame);
}

enum {
    flow_binding_commit_stage_check,
    flow_binding_commit_stage_capture,
    flow_binding_commit_stage_snapshot,
    flow_binding_commit_stage_payload,
    flow_binding_commit_stage_send,
    flow_binding_commit_stage_callback,
    flow_binding_commit_stage_owner,
    flow_binding_commit_stage_capture_after,
    flow_binding_commit_stage_snapshot_current,
    flow_binding_commit_stage_accept,
    flow_binding_commit_stage_extracted,
    flow_binding_commit_stage_effects,
    flow_binding_commit_stage_done,
    flow_binding_commit_stage_empty
};

struct lab_flow_binding_commit_context {
    int frame;
    int before;
};

static int lab_flow_binding_commit_check(struct lab_flow_binding_commit_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_commit_stage_capture, "bindingCurrent", "current", 0, 1,
                         lab_dom_get(ctx->frame, "isCurrent"));
}

static int lab_flow_binding_commit_capture(struct lab_flow_binding_commit_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_binding_false();
    return lab_flow_call(ctx->frame, flow_binding_commit_stage_snapshot, "bindingCapture", "before", 0, 0);
}

static int lab_flow_binding_commit_snapshot(struct lab_flow_binding_commit_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_commit_stage_payload, "native", "snapshot", 0, 2,
                         lab_dom_string("lab_binding_snapshot"), lab_dom_get(ctx->frame, "inputs"));
}

static int lab_flow_binding_commit_payload(struct lab_flow_binding_commit_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_commit_stage_send, "native", "payload", 0, 4,
                         lab_dom_string("lab_binding_commit_request"), lab_dom_get(ctx->frame, "modeId"),
                         lab_dom_get(ctx->frame, "source"), lab_dom_get(ctx->frame, "snapshot"));
}

static int lab_flow_binding_commit_send(struct lab_flow_binding_commit_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "payload")))
        return lab_flow_call(ctx->frame, flow_binding_commit_stage_empty, "bindingPrepare", 0, 1, 1,
                             lab_dom_get(ctx->frame, "source"));
    return lab_flow_call(ctx->frame, flow_binding_commit_stage_callback, "bindingPresentation", "data", 1, 2,
                         lab_dom_get(ctx->frame, "payload"), lab_dom_get(ctx->frame, "request"));
}

static int lab_flow_binding_commit_callback(struct lab_flow_binding_commit_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_commit_stage_owner, "bindingCurrent", "current", 0, 1,
                         lab_dom_get(ctx->frame, "isCurrent"));
}

static int lab_flow_binding_commit_owner(struct lab_flow_binding_commit_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_binding_false();
    return lab_flow_call(ctx->frame, flow_binding_commit_stage_capture_after, "requestCurrent", "current", 0, 1,
                         lab_dom_get(ctx->frame, "request"));
}

static int lab_flow_binding_commit_capture_after(struct lab_flow_binding_commit_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_binding_false();
    return lab_flow_call(ctx->frame, flow_binding_commit_stage_snapshot_current, "bindingCapture", "after", 0, 0);
}

static int lab_flow_binding_commit_snapshot_current(struct lab_flow_binding_commit_context *ctx)
{
    if (!lab_dom_equal(lab_dom_get(ctx->before, "text"), lab_dom_get(lab_dom_get(ctx->frame, "after"), "text")) ||
        !lab_dom_equal(lab_dom_get(ctx->frame, "source"), lab_dom_get(lab_dom_get(ctx->frame, "after"), "source")))
        return lab_flow_binding_false();
    return lab_flow_call(ctx->frame, flow_binding_commit_stage_accept, "native", "current", 0, 2,
                         lab_dom_string("lab_binding_snapshot_current"), lab_dom_get(ctx->frame, "snapshot"));
}

static int lab_flow_binding_commit_accept(struct lab_flow_binding_commit_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_binding_false();
    return lab_flow_call(ctx->frame, flow_binding_commit_stage_extracted, "bindingExpression", "updated", 0, 1,
                         lab_dom_get(ctx->frame, "data"));
}

static int lab_flow_binding_commit_extracted(struct lab_flow_binding_commit_context *ctx)
{
    int context = lab_dom_object(5);
    lab_dom_set(context, "editor", lab_dom_get(ctx->before, "editor"));
    lab_dom_set(context, "snapshot", lab_dom_get(ctx->frame, "snapshot"));
    lab_dom_set(context, "isCurrent", lab_dom_get(ctx->frame, "isCurrent"));
    return lab_flow_call(ctx->frame, flow_binding_commit_stage_effects, "native", "plan", 0, 6,
                         lab_dom_string("lab_binding_commit_plan"), lab_dom_get(ctx->frame, "modeId"),
                         lab_dom_numeric(0), lab_dom_get(ctx->frame, "source"), lab_dom_get(ctx->frame, "updated"),
                         context);
}

static int lab_flow_binding_commit_effects(struct lab_flow_binding_commit_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_commit_stage_done, "bindingEffects", 0, 0, 1,
                         lab_dom_get(ctx->frame, "plan"));
}

static int lab_flow_binding_commit_done(struct lab_flow_binding_commit_context *ctx)
{
    return lab_flow_done(lab_dom_get(lab_dom_get(ctx->frame, "plan"), "result"));
}

static int lab_flow_binding_commit_otherwise(struct lab_flow_binding_commit_context *ctx)
{
    (void)ctx;
    return lab_flow_binding_false();
}

static int lab_flow_binding_commit(int frame)
{
    int before = lab_dom_get(frame, "before");
    struct lab_flow_binding_commit_context ctx = {
        .frame = frame,
        .before = before,
    };
    static int (*const handlers[])(struct lab_flow_binding_commit_context *) = {
        [flow_binding_commit_stage_check]            = lab_flow_binding_commit_check,
        [flow_binding_commit_stage_capture]          = lab_flow_binding_commit_capture,
        [flow_binding_commit_stage_snapshot]         = lab_flow_binding_commit_snapshot,
        [flow_binding_commit_stage_payload]          = lab_flow_binding_commit_payload,
        [flow_binding_commit_stage_send]             = lab_flow_binding_commit_send,
        [flow_binding_commit_stage_callback]         = lab_flow_binding_commit_callback,
        [flow_binding_commit_stage_owner]            = lab_flow_binding_commit_owner,
        [flow_binding_commit_stage_capture_after]    = lab_flow_binding_commit_capture_after,
        [flow_binding_commit_stage_snapshot_current] = lab_flow_binding_commit_snapshot_current,
        [flow_binding_commit_stage_accept]           = lab_flow_binding_commit_accept,
        [flow_binding_commit_stage_extracted]        = lab_flow_binding_commit_extracted,
        [flow_binding_commit_stage_effects]          = lab_flow_binding_commit_effects,
        [flow_binding_commit_stage_done]             = lab_flow_binding_commit_done,
    };
    unsigned selected = lab_flow_binding_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_binding_commit_otherwise(&ctx);
}

enum {
    flow_binding_toggle_stage_capture,
    flow_binding_toggle_stage_toggle,
    flow_binding_toggle_stage_send,
    flow_binding_toggle_stage_capture_after,
    flow_binding_toggle_stage_accept,
    flow_binding_toggle_stage_effects,
    flow_binding_toggle_stage_done
};

struct lab_flow_binding_toggle_context {
    int frame;
    int before;
};

static int lab_flow_binding_toggle_capture(struct lab_flow_binding_toggle_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_toggle_stage_toggle, "bindingCapture", "before", 0, 0);
}

static int lab_flow_binding_toggle_toggle(struct lab_flow_binding_toggle_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_toggle_stage_send, "native", "toggle", 0, 2,
                         lab_dom_string("lab_binding_toggle"), lab_dom_get(ctx->frame, "binding"));
}

static int lab_flow_binding_toggle_send(struct lab_flow_binding_toggle_context *ctx)
{
    int toggle = lab_dom_get(ctx->frame, "toggle"), source = lab_dom_get(ctx->before, "source");
    if (!lab_dom_truth(source) || !lab_dom_truth(lab_dom_get(toggle, "name")))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, flow_binding_toggle_stage_capture_after, "bindingReplaceKind", "updated", 1, 3,
                         source, lab_dom_get(toggle, "name"), lab_dom_get(toggle, "nextKind"));
}

static int lab_flow_binding_toggle_capture_after(struct lab_flow_binding_toggle_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_toggle_stage_accept, "bindingCapture", "after", 0, 0);
}

static int lab_flow_binding_toggle_accept(struct lab_flow_binding_toggle_context *ctx)
{
    if (!lab_flow_binding_same(ctx->frame, 1))
        return lab_flow_done(0);
    int context = lab_dom_object(5);
    lab_dom_set(context, "editor", lab_dom_get(ctx->before, "editor"));
    return lab_flow_call(ctx->frame, flow_binding_toggle_stage_effects, "native", "plan", 0, 6,
                         lab_dom_string("lab_binding_commit_plan"), lab_dom_get(ctx->before, "modeId"),
                         lab_dom_numeric(1), lab_dom_get(ctx->before, "source"), lab_dom_get(ctx->frame, "updated"),
                         context);
}

static int lab_flow_binding_toggle_effects(struct lab_flow_binding_toggle_context *ctx)
{
    int plan = lab_dom_get(ctx->frame, "plan");
    return lab_flow_call(ctx->frame, flow_binding_toggle_stage_done, "bindingEffects", 0,
                         lab_dom_truth(lab_dom_get(plan, "wait")), 1, plan);
}

static int lab_flow_binding_toggle_otherwise(struct lab_flow_binding_toggle_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static int lab_flow_binding_toggle(int frame)
{
    int before = lab_dom_get(frame, "before");
    struct lab_flow_binding_toggle_context ctx = {
        .frame = frame,
        .before = before,
    };
    static int (*const handlers[])(struct lab_flow_binding_toggle_context *) = {
        [flow_binding_toggle_stage_capture]       = lab_flow_binding_toggle_capture,
        [flow_binding_toggle_stage_toggle]        = lab_flow_binding_toggle_toggle,
        [flow_binding_toggle_stage_send]          = lab_flow_binding_toggle_send,
        [flow_binding_toggle_stage_capture_after] = lab_flow_binding_toggle_capture_after,
        [flow_binding_toggle_stage_accept]        = lab_flow_binding_toggle_accept,
        [flow_binding_toggle_stage_effects]       = lab_flow_binding_toggle_effects,
    };
    unsigned selected = lab_flow_binding_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_binding_toggle_otherwise(&ctx);
}

enum {
    flow_binding_refresh_stage_check,
    flow_binding_refresh_stage_capture,
    flow_binding_refresh_stage_prepare,
    flow_binding_refresh_stage_recheck,
    flow_binding_refresh_stage_capture_after,
    flow_binding_refresh_stage_accept,
    flow_binding_refresh_stage_done,
    flow_binding_refresh_stage_error_check,
    flow_binding_refresh_stage_error_capture,
    flow_binding_refresh_stage_error_report
};

struct lab_flow_binding_refresh_context {
    int frame;
    int before;
};

static int lab_flow_binding_refresh_check(struct lab_flow_binding_refresh_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_refresh_stage_capture, "bindingCurrent", "current", 0, 1,
                         lab_dom_get(ctx->frame, "isCurrent"));
}

static int lab_flow_binding_refresh_capture(struct lab_flow_binding_refresh_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, flow_binding_refresh_stage_prepare, "bindingCapture", "before", 0, 0);
}

static int lab_flow_binding_refresh_prepare(struct lab_flow_binding_refresh_context *ctx)
{
    if (lab_dom_number(lab_dom_get(ctx->before, "modeId")) == 0)
        return lab_flow_call(ctx->frame, flow_binding_refresh_stage_done, "bindingScheduleRefresh", 0, 0, 0);
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(flow_binding_refresh_stage_error_check));
    return lab_flow_call(ctx->frame, flow_binding_refresh_stage_recheck, "bindingPrepare", 0, 1, 1,
                         lab_dom_get(ctx->before, "source"));
}

static int lab_flow_binding_refresh_recheck(struct lab_flow_binding_refresh_context *ctx)
{
    lab_dom_delete(ctx->frame, "failed");
    return lab_flow_call(ctx->frame, flow_binding_refresh_stage_capture_after, "bindingCurrent", "current", 0, 1,
                         lab_dom_get(ctx->frame, "isCurrent"));
}

static int lab_flow_binding_refresh_error_capture(struct lab_flow_binding_refresh_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame,
                         lab_flow_binding_stage(ctx->frame) == flow_binding_refresh_stage_capture_after
                             ? flow_binding_refresh_stage_accept
                             : flow_binding_refresh_stage_error_report,
                         "bindingCapture", "after", 0, 0);
}

static int lab_flow_binding_refresh_accept(struct lab_flow_binding_refresh_context *ctx)
{
    if (!lab_flow_binding_same(ctx->frame, 1))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, flow_binding_refresh_stage_done, "bindingProjectVisible", 0, 0, 1,
                         lab_dom_get(ctx->before, "source"));
}

static int lab_flow_binding_refresh_error_check(struct lab_flow_binding_refresh_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_refresh_stage_error_capture, "bindingCurrent", "current", 0, 1,
                         lab_dom_get(ctx->frame, "isCurrent"));
}

static int lab_flow_binding_refresh_error_report(struct lab_flow_binding_refresh_context *ctx)
{
    if (lab_flow_binding_same(ctx->frame, 0))
        return lab_flow_call(ctx->frame, flow_binding_refresh_stage_done, "setStatus", 0, 0, 1,
                             lab_dom_get(ctx->frame, "error"));
    return lab_flow_done(0);
}

static int lab_flow_binding_refresh_otherwise(struct lab_flow_binding_refresh_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static int lab_flow_binding_refresh(int frame)
{
    int before = lab_dom_get(frame, "before");
    struct lab_flow_binding_refresh_context ctx = {
        .frame = frame,
        .before = before,
    };
    static int (*const handlers[])(struct lab_flow_binding_refresh_context *) = {
        [flow_binding_refresh_stage_check]         = lab_flow_binding_refresh_check,
        [flow_binding_refresh_stage_capture]       = lab_flow_binding_refresh_capture,
        [flow_binding_refresh_stage_prepare]       = lab_flow_binding_refresh_prepare,
        [flow_binding_refresh_stage_recheck]       = lab_flow_binding_refresh_recheck,
        [flow_binding_refresh_stage_capture_after] = lab_flow_binding_refresh_error_capture,
        [flow_binding_refresh_stage_error_capture] = lab_flow_binding_refresh_error_capture,
        [flow_binding_refresh_stage_accept]        = lab_flow_binding_refresh_accept,
        [flow_binding_refresh_stage_error_check]   = lab_flow_binding_refresh_error_check,
        [flow_binding_refresh_stage_error_report]  = lab_flow_binding_refresh_error_report,
    };
    unsigned selected = lab_flow_binding_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_binding_refresh_otherwise(&ctx);
}

enum { flow_binding_queue_stage_mode, flow_binding_queue_stage_commit, flow_binding_queue_stage_done };

static int lab_flow_binding_queue_mode(int frame)
{
    return lab_flow_call(frame, flow_binding_queue_stage_commit, "currentMode", "currentMode", 0, 0);
}

static int lab_flow_binding_queue_commit(int frame)
{
    if (!lab_dom_equal(lab_dom_get(frame, "mode"), lab_dom_get(frame, "currentMode")) ||
        !lab_dom_truth(lab_dom_get(lab_dom_get(frame, "input"), "isConnected")))
        return lab_flow_binding_false();
    return lab_flow_call(frame, flow_binding_queue_stage_done, "bindingCommitInput", "result", 1, 1,
                         lab_dom_get(frame, "input"));
}

static int lab_flow_binding_queue_done(int frame)
{
    return lab_flow_done(lab_dom_get(frame, "result"));
}

static int lab_flow_binding_queue_otherwise(int frame)
{
    (void)frame;
    return lab_flow_binding_false();
}

static int lab_flow_binding_queue(int frame)
{
    static int (*const handlers[])(int) = {
        [flow_binding_queue_stage_mode]   = lab_flow_binding_queue_mode,
        [flow_binding_queue_stage_commit] = lab_flow_binding_queue_commit,
        [flow_binding_queue_stage_done]   = lab_flow_binding_queue_done,
    };
    unsigned selected = lab_flow_binding_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](frame)
               : lab_flow_binding_queue_otherwise(frame);
}

enum {
    flow_binding_goal_stage_prepare,
    flow_binding_goal_stage_starts,
    flow_binding_goal_stage_prepare_result,
    flow_binding_goal_stage_done
};

static int lab_flow_binding_goal_prepare(int frame)
{
    return lab_flow_call(frame, flow_binding_goal_stage_starts, "bindingPrepare", "editor", 1, 1,
                         lab_dom_get(frame, "source"));
}

static int lab_flow_binding_goal_starts(int frame)
{
    return lab_flow_call(frame, flow_binding_goal_stage_prepare_result, "native", "prepared", 0, 3,
                         lab_dom_string("lab_binding_goal_prepare"), lab_dom_get(frame, "editor"),
                         lab_dom_get(frame, "providedStart"));
}

static int lab_flow_binding_goal_prepare_result(int frame)
{
    return lab_flow_call(frame, flow_binding_goal_stage_done, "bindingPrepare", 0, 1, 1,
                         lab_dom_get(lab_dom_get(frame, "prepared"), "expression"));
}

static int lab_flow_binding_goal_done(int frame)
{
    return lab_flow_done(lab_dom_get(frame, "prepared"));
}

static int lab_flow_binding_goal_otherwise(int frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static int lab_flow_binding_goal(int frame)
{
    static int (*const handlers[])(int) = {
        [flow_binding_goal_stage_prepare]        = lab_flow_binding_goal_prepare,
        [flow_binding_goal_stage_starts]         = lab_flow_binding_goal_starts,
        [flow_binding_goal_stage_prepare_result] = lab_flow_binding_goal_prepare_result,
        [flow_binding_goal_stage_done]           = lab_flow_binding_goal_done,
    };
    unsigned selected = lab_flow_binding_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](frame)
                                                                               : lab_flow_binding_goal_otherwise(frame);
}

enum {
    flow_binding_row_stage_commit,
    flow_binding_row_stage_prepare,
    flow_binding_row_stage_render,
    flow_binding_row_stage_refresh,
    flow_binding_row_stage_history,
    flow_binding_row_stage_mode,
    flow_binding_row_stage_save,
    flow_binding_row_stage_done,
    flow_binding_row_stage_error
};

struct lab_flow_binding_row_context {
    int frame;
    int item;
};

static int lab_flow_binding_row_commit(struct lab_flow_binding_row_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(flow_binding_row_stage_error));
    return lab_flow_call(ctx->frame, flow_binding_row_stage_prepare, "commitBindings", 0, 1, 0);
}

static int lab_flow_binding_row_prepare(struct lab_flow_binding_row_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->item, "isConnected")))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, flow_binding_row_stage_render, "bindingPrepareRows", "prepared", 1, 2,
                         lab_dom_get(ctx->frame, "index"), lab_dom_get(ctx->frame, "operation"));
}

static int lab_flow_binding_row_render(struct lab_flow_binding_row_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "prepared")) || !lab_dom_truth(lab_dom_get(ctx->item, "isConnected")))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, flow_binding_row_stage_refresh, "bindingRenderRows", 0, 0, 1,
                         lab_dom_get(ctx->frame, "prepared"));
}

static int lab_flow_binding_row_refresh(struct lab_flow_binding_row_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_row_stage_history, "bindingRefresh", 0, 0, 0);
}

static int lab_flow_binding_row_history(struct lab_flow_binding_row_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_row_stage_mode, "updateHistoryButtons", 0, 0, 0);
}

static int lab_flow_binding_row_mode(struct lab_flow_binding_row_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_row_stage_save, "currentMode", "mode", 0, 0);
}

static int lab_flow_binding_row_save(struct lab_flow_binding_row_context *ctx)
{
    if (lab_dom_equal(lab_dom_get(ctx->frame, "mode"), lab_dom_string("integrator")))
        return lab_flow_call(ctx->frame, flow_binding_row_stage_done, "saveWorksheetState", 0, 0, 1,
                             lab_dom_string("integrator"));
    return lab_flow_done(0);
}

static int lab_flow_binding_row_error(struct lab_flow_binding_row_context *ctx)
{
    if (lab_dom_truth(lab_dom_get(ctx->item, "isConnected")))
        return lab_flow_call(ctx->frame, flow_binding_row_stage_done, "bindingRowError", 0, 0, 2, ctx->item,
                             lab_dom_get(ctx->frame, "exception"));
    return lab_flow_done(0);
}

static int lab_flow_binding_row_otherwise(struct lab_flow_binding_row_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static int lab_flow_binding_row(int frame)
{
    int item = lab_dom_get(frame, "item");
    struct lab_flow_binding_row_context ctx = {
        .frame = frame,
        .item = item,
    };
    static int (*const handlers[])(struct lab_flow_binding_row_context *) = {
        [flow_binding_row_stage_commit]  = lab_flow_binding_row_commit,
        [flow_binding_row_stage_prepare] = lab_flow_binding_row_prepare,
        [flow_binding_row_stage_render]  = lab_flow_binding_row_render,
        [flow_binding_row_stage_refresh] = lab_flow_binding_row_refresh,
        [flow_binding_row_stage_history] = lab_flow_binding_row_history,
        [flow_binding_row_stage_mode]    = lab_flow_binding_row_mode,
        [flow_binding_row_stage_save]    = lab_flow_binding_row_save,
        [flow_binding_row_stage_error]   = lab_flow_binding_row_error,
    };
    unsigned selected = lab_flow_binding_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_flow_binding_row_otherwise(&ctx);
}

static int lab_flow_binding_input_current(int frame)
{
    int input = lab_dom_get(frame, "input");
    return lab_dom_truth(lab_dom_get(input, "isConnected")) &&
           lab_dom_equal(lab_dom_read(input, 5, ""), lab_dom_get(frame, "authored"));
}

enum {
    flow_binding_prepare_input_stage_prepare,
    flow_binding_prepare_input_stage_update,
    flow_binding_prepare_input_stage_mode,
    flow_binding_prepare_input_stage_refresh,
    flow_binding_prepare_input_stage_history,
    flow_binding_prepare_input_stage_save,
    flow_binding_prepare_input_stage_done,
    flow_binding_prepare_input_stage_error
};

static int lab_flow_binding_prepare_input_prepare(int frame)
{
    lab_dom_set(frame, "authored", lab_dom_read(lab_dom_get(frame, "input"), 5, ""));
    lab_dom_set(frame, "failed", lab_dom_numeric(flow_binding_prepare_input_stage_error));
    return lab_flow_call(frame, flow_binding_prepare_input_stage_update, "bindingRefreshForms", "prepared", 1, 0);
}

static int lab_flow_binding_prepare_input_update(int frame)
{
    if (!lab_dom_truth(lab_dom_get(frame, "prepared")) || !lab_flow_binding_input_current(frame))
        return lab_flow_done(0);
    return lab_flow_call(frame, flow_binding_prepare_input_stage_mode, "bindingUpdateForms", 0, 0, 1,
                         lab_dom_get(frame, "prepared"));
}

static int lab_flow_binding_prepare_input_mode(int frame)
{
    return lab_flow_call(frame, flow_binding_prepare_input_stage_refresh, "currentMode", "mode", 0, 0);
}

static int lab_flow_binding_prepare_input_refresh(int frame)
{
    if (!lab_dom_equal(lab_dom_get(frame, "mode"), lab_dom_string("integrator")))
        return lab_flow_done(0);
    return lab_flow_call(frame, flow_binding_prepare_input_stage_history, "bindingRefresh", 0, 0, 0);
}

static int lab_flow_binding_prepare_input_history(int frame)
{
    return lab_flow_call(frame, flow_binding_prepare_input_stage_save, "updateHistoryButtons", 0, 0, 0);
}

static int lab_flow_binding_prepare_input_save(int frame)
{
    return lab_flow_call(frame, flow_binding_prepare_input_stage_done, "saveWorksheetState", 0, 0, 1,
                         lab_dom_string("integrator"));
}

static int lab_flow_binding_prepare_input_error(int frame)
{
    if (lab_flow_binding_input_current(frame))
        return lab_flow_call(frame, flow_binding_prepare_input_stage_done, "bindingValidity", 0, 0, 2,
                             lab_dom_get(frame, "input"), lab_dom_get(frame, "exception"));
    return lab_flow_done(0);
}

static int lab_flow_binding_prepare_input_otherwise(int frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static int lab_flow_binding_prepare_input(int frame)
{
    static int (*const handlers[])(int) = {
        [flow_binding_prepare_input_stage_prepare] = lab_flow_binding_prepare_input_prepare,
        [flow_binding_prepare_input_stage_update]  = lab_flow_binding_prepare_input_update,
        [flow_binding_prepare_input_stage_mode]    = lab_flow_binding_prepare_input_mode,
        [flow_binding_prepare_input_stage_refresh] = lab_flow_binding_prepare_input_refresh,
        [flow_binding_prepare_input_stage_history] = lab_flow_binding_prepare_input_history,
        [flow_binding_prepare_input_stage_save]    = lab_flow_binding_prepare_input_save,
        [flow_binding_prepare_input_stage_error]   = lab_flow_binding_prepare_input_error,
    };
    unsigned selected = lab_flow_binding_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](frame)
               : lab_flow_binding_prepare_input_otherwise(frame);
}

enum {
    flow_binding_action_stage_start,
    flow_binding_action_stage_clear_commit,
    flow_binding_action_stage_clear_focus,
    flow_binding_action_stage_evaluate,
    flow_binding_action_stage_copy_ok,
    flow_binding_action_stage_copy_status,
    flow_binding_action_stage_copy_reset,
    flow_binding_action_stage_done,
    flow_binding_action_stage_copy_failed,
    flow_binding_action_stage_copy_error
};

struct lab_flow_binding_action_context {
    int frame;
    int card;
};

static int lab_flow_binding_action_start(struct lab_flow_binding_action_context *ctx)
{
    unsigned operation = (unsigned)lab_dom_number(lab_dom_get(ctx->frame, "operation"));
    if (operation == 0)
        return lab_flow_call(ctx->frame, flow_binding_action_stage_evaluate, "bindingCommitInput", 0, 1, 1,
                             lab_dom_get(ctx->card, "input"));
    if (operation == 1)
        return lab_flow_call(ctx->frame, flow_binding_action_stage_clear_commit, "updateHistoryButtons", 0, 0, 0);
    if (operation != 2)
        return lab_flow_done(0);
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(flow_binding_action_stage_copy_failed));
    return lab_flow_call(ctx->frame, flow_binding_action_stage_copy_ok, "bindingClipboard", 0, 1, 1,
                         lab_dom_read(lab_dom_get(ctx->card, "input"), 5, ""));
}

static int lab_flow_binding_action_clear_commit(struct lab_flow_binding_action_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_action_stage_clear_focus, "bindingQueue", 0, 1, 1,
                         lab_dom_get(ctx->card, "input"));
}

static int lab_flow_binding_action_clear_focus(struct lab_flow_binding_action_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_action_stage_done, "bindingRefocus", 0, 0, 1, ctx->card);
}

static int lab_flow_binding_action_evaluate(struct lab_flow_binding_action_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_action_stage_done, "bindingEvaluate", "result", 1, 0);
}

static int lab_flow_binding_action_copy_ok(struct lab_flow_binding_action_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_action_stage_copy_status, "bindingFlash", 0, 0, 2,
                         lab_dom_get(ctx->card, "copy"), lab_dom_scalar(1, 1));
}

static int lab_flow_binding_action_copy_status(struct lab_flow_binding_action_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_action_stage_copy_reset, "setStatus", 0, 0, 1,
                         lab_dom_get(ctx->card, "message"));
}

static int lab_flow_binding_action_copy_reset(struct lab_flow_binding_action_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_action_stage_done, "bindingReadyTimer", 0, 0, 0);
}

static int lab_flow_binding_action_copy_failed(struct lab_flow_binding_action_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_action_stage_copy_error, "bindingFlash", 0, 0, 2,
                         lab_dom_get(ctx->card, "copy"), lab_dom_scalar(1, 0));
}

static int lab_flow_binding_action_copy_error(struct lab_flow_binding_action_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_binding_action_stage_done, "setStatus", 0, 0, 1,
                         lab_dom_get(ctx->frame, "error"));
}

static int lab_flow_binding_action_otherwise(struct lab_flow_binding_action_context *ctx)
{
    return lab_flow_done(lab_dom_get(ctx->frame, "result"));
}

static int lab_flow_binding_action(int frame)
{
    int card = lab_dom_get(frame, "card");
    struct lab_flow_binding_action_context ctx = {
        .frame = frame,
        .card = card,
    };
    static int (*const handlers[])(struct lab_flow_binding_action_context *) = {
        [flow_binding_action_stage_start]        = lab_flow_binding_action_start,
        [flow_binding_action_stage_clear_commit] = lab_flow_binding_action_clear_commit,
        [flow_binding_action_stage_clear_focus]  = lab_flow_binding_action_clear_focus,
        [flow_binding_action_stage_evaluate]     = lab_flow_binding_action_evaluate,
        [flow_binding_action_stage_copy_ok]      = lab_flow_binding_action_copy_ok,
        [flow_binding_action_stage_copy_status]  = lab_flow_binding_action_copy_status,
        [flow_binding_action_stage_copy_reset]   = lab_flow_binding_action_copy_reset,
        [flow_binding_action_stage_copy_failed]  = lab_flow_binding_action_copy_failed,
        [flow_binding_action_stage_copy_error]   = lab_flow_binding_action_copy_error,
    };
    unsigned selected = lab_flow_binding_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_binding_action_otherwise(&ctx);
}

enum {
    flow_binding_deferred_stage_prepare,
    flow_binding_deferred_stage_capture,
    flow_binding_deferred_stage_accept,
    flow_binding_deferred_stage_done,
    flow_binding_deferred_stage_error_capture,
    flow_binding_deferred_stage_error_report
};


static int lab_flow_binding_deferred_prepare(int frame)
{
    lab_dom_set(frame, "failed", lab_dom_numeric(flow_binding_deferred_stage_error_capture));
    return lab_flow_call(frame, flow_binding_deferred_stage_capture, "bindingPrepare", 0, 1, 1,
                         lab_dom_get(frame, "fullText"));
}

static int lab_flow_binding_deferred_error_capture(int frame)
{
    return lab_flow_call(frame,
                         lab_flow_binding_stage(frame) == flow_binding_deferred_stage_capture
                             ? flow_binding_deferred_stage_accept
                             : flow_binding_deferred_stage_error_report,
                         "bindingCapture", "after", 0, 0);
}

static int lab_flow_binding_deferred_error_report(int frame)
{
    {
        int after = lab_dom_get(frame, "after"), full = lab_dom_get(frame, "fullText");
        int source = lab_dom_clean(lab_dom_truth(full) ? lab_dom_text(full) : lab_dom_string(""), 0);
        if (!lab_dom_equal(lab_dom_get(frame, "mode"), lab_dom_get(after, "mode")) ||
            !lab_dom_equal(lab_dom_get(after, "source"), source))
            return lab_flow_done(0);
        if (lab_flow_binding_stage(frame) == flow_binding_deferred_stage_error_report)
            return lab_flow_call(frame, flow_binding_deferred_stage_done, "setStatus", 0, 0, 1,
                                 lab_dom_get(frame, "error"));
        return lab_flow_call(frame, flow_binding_deferred_stage_done, "bindingSetEditor", 0, 0, 4, full,
                             lab_dom_get(frame, "evaluatedBindings"), lab_dom_get(frame, "editorBodyText"),
                             lab_dom_get(frame, "evaluationReady"));
    }
}

static int lab_flow_binding_deferred_otherwise(int frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static int lab_flow_binding_deferred(int frame)
{
    static int (*const handlers[])(int) = {
        [flow_binding_deferred_stage_prepare]       = lab_flow_binding_deferred_prepare,
        [flow_binding_deferred_stage_capture]       = lab_flow_binding_deferred_error_capture,
        [flow_binding_deferred_stage_error_capture] = lab_flow_binding_deferred_error_capture,
        [flow_binding_deferred_stage_accept]        = lab_flow_binding_deferred_error_report,
        [flow_binding_deferred_stage_error_report]  = lab_flow_binding_deferred_error_report,
    };
    unsigned selected = lab_flow_binding_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](frame)
               : lab_flow_binding_deferred_otherwise(frame);
}

enum {
    flow_binding_begin_commit_stage_check,
    flow_binding_begin_commit_stage_capture,
    flow_binding_begin_commit_stage_run,
    flow_binding_begin_commit_stage_done
};

static int lab_flow_binding_begin_commit_check(int frame)
{
    return lab_flow_call(frame, flow_binding_begin_commit_stage_capture, "bindingCurrent", "current", 0, 1,
                         lab_dom_get(frame, "isCurrent"));
}

static int lab_flow_binding_begin_commit_capture(int frame)
{
    if (!lab_dom_truth(lab_dom_get(frame, "current")))
        return lab_flow_binding_false();
    return lab_flow_call(frame, flow_binding_begin_commit_stage_run, "bindingCapture", "before", 0, 0);
}

static int lab_flow_binding_begin_commit_run(int frame)
{
    return lab_flow_call(frame, flow_binding_begin_commit_stage_done, "bindingRunCommit", "result", 1, 1, frame);
}

static int lab_flow_binding_begin_commit_done(int frame)
{
    return lab_flow_done(lab_dom_get(frame, "result"));
}

static int lab_flow_binding_begin_commit_otherwise(int frame)
{
    (void)frame;
    return lab_flow_binding_false();
}

static int lab_flow_binding_begin_commit(int frame)
{
    static int (*const handlers[])(int) = {
        [flow_binding_begin_commit_stage_check]   = lab_flow_binding_begin_commit_check,
        [flow_binding_begin_commit_stage_capture] = lab_flow_binding_begin_commit_capture,
        [flow_binding_begin_commit_stage_run]     = lab_flow_binding_begin_commit_run,
        [flow_binding_begin_commit_stage_done]    = lab_flow_binding_begin_commit_done,
    };
    unsigned selected = lab_flow_binding_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](frame)
               : lab_flow_binding_begin_commit_otherwise(frame);
}

enum {
    flow_binding_kind_request_stage_request,
    flow_binding_kind_request_stage_extract,
    flow_binding_kind_request_stage_done
};

static int lab_flow_binding_kind_request_request(int frame)
{
    int request = lab_dom_object(5);
    lab_dom_set(request, "action", lab_dom_string("editor"));
    lab_dom_set(request, "operation", lab_dom_string("kind"));
    lab_dom_set(request, "text", lab_dom_get(frame, "source"));
    lab_dom_set(request, "name", lab_dom_get(frame, "name"));
    lab_dom_set(request, "kind", lab_dom_get(frame, "nextKind"));
    return lab_flow_call(frame, flow_binding_kind_request_stage_extract, "bindingPresentation", "data", 1, 1, request);
}

static int lab_flow_binding_kind_request_extract(int frame)
{
    return lab_flow_call(frame, flow_binding_kind_request_stage_done, "bindingExpression", "expression", 0, 1,
                         lab_dom_get(frame, "data"));
}

static int lab_flow_binding_kind_request_done(int frame)
{
    return lab_flow_done(lab_dom_get(frame, "expression"));
}

static int lab_flow_binding_kind_request_otherwise(int frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static int lab_flow_binding_kind_request(int frame)
{
    static int (*const handlers[])(int) = {
        [flow_binding_kind_request_stage_request] = lab_flow_binding_kind_request_request,
        [flow_binding_kind_request_stage_extract] = lab_flow_binding_kind_request_extract,
        [flow_binding_kind_request_stage_done]    = lab_flow_binding_kind_request_done,
    };
    unsigned selected = lab_flow_binding_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](frame)
               : lab_flow_binding_kind_request_otherwise(frame);
}

/* Indexed workflow selection preserves module ownership without a growing name scan. */
int lab_flow_binding(unsigned kind, int frame, int view)
{
    (void)view;
    static int (*const handlers[])(int) = {
        [0]  = lab_flow_binding_sync,
        [1]  = lab_flow_binding_visible,
        [2]  = lab_flow_binding_commit,
        [3]  = lab_flow_binding_toggle,
        [4]  = lab_flow_binding_refresh,
        [5]  = lab_flow_binding_queue,
        [6]  = lab_flow_binding_goal,
        [7]  = lab_flow_binding_row,
        [8]  = lab_flow_binding_prepare_input,
        [9]  = lab_flow_binding_action,
        [10] = lab_flow_binding_deferred,
        [11] = lab_flow_binding_begin_commit,
    };
    if (kind >= 12 && kind <= 14)
        return lab_flow_binding_edit(kind - 12, frame);
    if (kind == 15)
        return lab_flow_binding_kind_request(frame);
    return kind < sizeof handlers / sizeof *handlers ? handlers[kind](frame) : lab_flow_done(0);
}
