/**
 * @file lab_flow_transport.c
 * @brief POST publication, request execution and UI recovery for the Lab browser.
 *
 * Keeps application sequencing in C while the browser retains fetch, decoding,
 * callbacks, controllers and timers. Staleness is checked before transport and
 * on both sides of response decoding. Cleanup executes for callback and recovery
 * failures, including non-UI requests. Error strings are read only when a current
 * UI request is actually presented; arbitrary thrown values retain their identity.
 */
#include "lab_dom.h"
#include "lab_flow.h"
#include "lab_flow_transport.h"

/* Synchronous resource adapters apply the effects selected by native request policy. */
unsigned lab_transport_policy(lab_request_operation_t action, lab_request_channel_t channel,
                              lab_request_channel_t main_channel)
{
    return (action == LAB_REQUEST_EVALUATE ? 1u : 0u) | (channel == main_channel ? 2u : 0u);
}

static unsigned lab_flow_transport_stage(frame_handle frame)
{
    return (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
}

enum {
    flow_transport_post_stage_assert_start,
    flow_transport_post_stage_endpoint,
    flow_transport_post_stage_fetch,
    flow_transport_post_stage_assert_response,
    flow_transport_post_stage_decode,
    flow_transport_post_stage_assert_data,
    flow_transport_post_stage_accept,
    flow_transport_post_stage_syntax,
    flow_transport_post_stage_result,
    flow_transport_post_stage_done
};

struct lab_flow_transport_post_context {
    frame_handle frame;
    browser_handle request;
};

/**
 * @brief Advance request-owned posting, response decoding and acceptance.
 * @param frame Borrowed browser frame retaining the stage, request, payload and transport results.
 * @return Scoped operation or completion plan for the selected posting stage.
 */
static browser_handle lab_flow_transport_post(frame_handle frame);

static browser_handle lab_flow_transport_post_assert_start(struct lab_flow_transport_post_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_transport_post_stage_endpoint, "assertCurrent", 0, 0, 1, ctx->request);
}

static browser_handle lab_flow_transport_post_endpoint(struct lab_flow_transport_post_context *ctx)
{
    if (lab_dom_truth(ctx->request))
        return lab_flow_call(ctx->frame, flow_transport_post_stage_fetch, "endpoint", "endpoint", 0, 1, ctx->request);
    lab_dom_set(ctx->frame, "endpoint", lab_dom_get(ctx->frame, "fallback"));
    lab_dom_set(ctx->frame, "stage", lab_dom_numeric(flow_transport_post_stage_fetch));
    return lab_flow_transport_post(ctx->frame);
}

static browser_handle lab_flow_transport_post_fetch(struct lab_flow_transport_post_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "endpoint")))
        return lab_flow_call(ctx->frame, flow_transport_post_stage_done, "invalidEndpoint", 0, 0, 0);
    return lab_flow_call(ctx->frame, flow_transport_post_stage_assert_response, "fetch", "response", 1, 2,
                         lab_dom_get(ctx->frame, "endpoint"), ctx->frame);
}

static browser_handle lab_flow_transport_post_assert_response(struct lab_flow_transport_post_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_transport_post_stage_decode, "assertCurrent", 0, 0, 1, ctx->request);
}

static browser_handle lab_flow_transport_post_decode(struct lab_flow_transport_post_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_transport_post_stage_assert_data, "decode", "data", 1, 1,
                         lab_dom_get(ctx->frame, "response"));
}

static browser_handle lab_flow_transport_post_assert_data(struct lab_flow_transport_post_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_transport_post_stage_accept, "assertCurrent", 0, 0, 1, ctx->request);
}

static browser_handle lab_flow_transport_post_accept(struct lab_flow_transport_post_context *ctx)
{
    if (lab_dom_truth(lab_dom_get(lab_dom_get(ctx->frame, "response"), "ok"))) {
        int ok = lab_dom_get(lab_dom_get(ctx->frame, "data"), "ok");
        /* Only the actual boolean false rejects publication: absence, zero and empty text do not. */
        if (lab_dom_type(ok) != LAB_VALUE_BOOLEAN || lab_dom_truth(ok))
            return lab_flow_call(ctx->frame, flow_transport_post_stage_syntax, "presentation", 0, 0, 1, ctx->frame);
    }
    return lab_flow_call(ctx->frame, flow_transport_post_stage_done, "result", "result", 0, 1, ctx->frame);
}

static browser_handle lab_flow_transport_post_syntax(struct lab_flow_transport_post_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_transport_post_stage_result, "syntax", 0, 0, 1, ctx->frame);
}

static browser_handle lab_flow_transport_post_result(struct lab_flow_transport_post_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_transport_post_stage_done, "result", "result", 0, 1, ctx->frame);
}

static browser_handle lab_flow_transport_post_otherwise(struct lab_flow_transport_post_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static browser_handle lab_flow_transport_post(frame_handle frame)
{
    int request = lab_dom_get(frame, "request");
    struct lab_flow_transport_post_context ctx = {
        .frame = frame,
        .request = request,
    };
    static browser_handle (*const handlers[])(struct lab_flow_transport_post_context *) = {
        [flow_transport_post_stage_assert_start]    = lab_flow_transport_post_assert_start,
        [flow_transport_post_stage_endpoint]        = lab_flow_transport_post_endpoint,
        [flow_transport_post_stage_fetch]           = lab_flow_transport_post_fetch,
        [flow_transport_post_stage_assert_response] = lab_flow_transport_post_assert_response,
        [flow_transport_post_stage_decode]          = lab_flow_transport_post_decode,
        [flow_transport_post_stage_assert_data]     = lab_flow_transport_post_assert_data,
        [flow_transport_post_stage_accept]          = lab_flow_transport_post_accept,
        [flow_transport_post_stage_syntax]          = lab_flow_transport_post_syntax,
        [flow_transport_post_stage_result]          = lab_flow_transport_post_result,
    };
    unsigned selected = lab_flow_transport_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_transport_post_otherwise(&ctx);
}

enum {
    flow_transport_run_stage_begin,
    flow_transport_run_stage_callback,
    flow_transport_run_stage_finish,
    flow_transport_run_stage_done,
    flow_transport_run_stage_outer_current,
    flow_transport_run_stage_outer_decide,
    flow_transport_run_stage_ui_current,
    flow_transport_run_stage_ui_decide,
    flow_transport_run_stage_ui_digits,
    flow_transport_run_stage_ui_clear,
    flow_transport_run_stage_ui_status,
    flow_transport_run_stage_rethrow
};

struct lab_flow_transport_run_context {
    frame_handle frame;
    int ui;
    browser_handle request;
};

/**
 * @brief Advance callback execution, ownership-aware recovery and finalisation.
 * @param frame Borrowed browser frame retaining the stage, request, callback result and exception state.
 * @param ui Non-zero to render current-request errors in the UI before finalisation; zero for outer recovery only.
 * @return Scoped operation or completion plan for the selected runner stage.
 */
static browser_handle lab_flow_transport_run(frame_handle frame, int ui);

static browser_handle lab_flow_transport_run_begin(struct lab_flow_transport_run_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_transport_run_stage_callback, "begin", "request", 0, 1, ctx->frame);
}

static browser_handle lab_flow_transport_run_callback(struct lab_flow_transport_run_context *ctx)
{
    if (!lab_dom_truth(ctx->request))
        return lab_flow_done(0);
    lab_dom_set(
        ctx->frame, "failed",
        lab_dom_numeric(ctx->ui ? flow_transport_run_stage_ui_current : flow_transport_run_stage_outer_current));
    return lab_flow_call(ctx->frame, flow_transport_run_stage_finish, "callback", "result", 1, 1, ctx->frame);
}

static browser_handle lab_flow_transport_run_finish(struct lab_flow_transport_run_context *ctx)
{
    /* A finaliser failure replaces any earlier callback/recovery failure, exactly like finally. */
    lab_dom_delete(ctx->frame, "failed");
    return lab_flow_call(ctx->frame,
                         lab_dom_truth(lab_dom_get(ctx->frame, "rethrow")) ? flow_transport_run_stage_rethrow
                                                                           : flow_transport_run_stage_done,
                         "finish", 0, 0, 1, ctx->request);
}

static browser_handle lab_flow_transport_run_outer_current(struct lab_flow_transport_run_context *ctx)
{
    /* If the ownership check itself throws, its exception still passes through the finaliser. */
    lab_dom_set(ctx->frame, "rethrow", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(flow_transport_run_stage_finish));
    return lab_flow_call(ctx->frame, flow_transport_run_stage_outer_decide, "current", "current", 0, 1, ctx->request);
}

static browser_handle lab_flow_transport_run_outer_decide(struct lab_flow_transport_run_context *ctx)
{
    lab_dom_set(ctx->frame, "rethrow", lab_dom_scalar(LAB_VALUE_BOOLEAN, lab_dom_truth(lab_dom_get(ctx->frame, "current"))));
    lab_dom_set(ctx->frame, "stage", lab_dom_numeric(flow_transport_run_stage_finish));
    return lab_flow_transport_run(ctx->frame, ctx->ui);
}

static browser_handle lab_flow_transport_run_ui_current(struct lab_flow_transport_run_context *ctx)
{
    /* UI recovery has the ordinary runner's enclosing catch and finally. */
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(flow_transport_run_stage_outer_current));
    return lab_flow_call(ctx->frame, flow_transport_run_stage_ui_decide, "current", "current", 0, 1, ctx->request);
}

static browser_handle lab_flow_transport_run_ui_decide(struct lab_flow_transport_run_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current"))) {
        lab_dom_set(ctx->frame, "stage", lab_dom_numeric(flow_transport_run_stage_finish));
        return lab_flow_transport_run(ctx->frame, ctx->ui);
    }
    return lab_flow_call(ctx->frame, flow_transport_run_stage_ui_digits, "renderError", 0, 0, 1,
                         lab_dom_get(ctx->frame, "error"));
}

static browser_handle lab_flow_transport_run_ui_digits(struct lab_flow_transport_run_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_transport_run_stage_ui_clear, "resetDigits", 0, 0, 0);
}

static browser_handle lab_flow_transport_run_ui_clear(struct lab_flow_transport_run_context *ctx)
{
    int options = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(options, "keepBindings", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
    return lab_flow_call(ctx->frame, flow_transport_run_stage_ui_status, "clearResults", 0, 0, 1, options);
}

static browser_handle lab_flow_transport_run_ui_status(struct lab_flow_transport_run_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_transport_run_stage_finish, "status", 0, 0, 1, lab_dom_string("Error"));
}

static browser_handle lab_flow_transport_run_rethrow(struct lab_flow_transport_run_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_transport_run_stage_done, "rethrow", 0, 0, 1, ctx->frame);
}

static browser_handle lab_flow_transport_run_otherwise(struct lab_flow_transport_run_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static browser_handle lab_flow_transport_run(frame_handle frame, int ui)
{
    int request = lab_dom_get(frame, "request");
    struct lab_flow_transport_run_context ctx = {
        .frame = frame,
        .ui = ui,
        .request = request,
    };
    static browser_handle (*const handlers[])(struct lab_flow_transport_run_context *) = {
        [flow_transport_run_stage_begin]         = lab_flow_transport_run_begin,
        [flow_transport_run_stage_callback]      = lab_flow_transport_run_callback,
        [flow_transport_run_stage_finish]        = lab_flow_transport_run_finish,
        [flow_transport_run_stage_outer_current] = lab_flow_transport_run_outer_current,
        [flow_transport_run_stage_outer_decide]  = lab_flow_transport_run_outer_decide,
        [flow_transport_run_stage_ui_current]    = lab_flow_transport_run_ui_current,
        [flow_transport_run_stage_ui_decide]     = lab_flow_transport_run_ui_decide,
        [flow_transport_run_stage_ui_digits]     = lab_flow_transport_run_ui_digits,
        [flow_transport_run_stage_ui_clear]      = lab_flow_transport_run_ui_clear,
        [flow_transport_run_stage_ui_status]     = lab_flow_transport_run_ui_status,
        [flow_transport_run_stage_rethrow]       = lab_flow_transport_run_rethrow,
    };
    unsigned selected = lab_flow_transport_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_flow_transport_run_otherwise(&ctx);
}

/* The browser owns arbitrary result values; native plans never coerce null, undefined or thrown primitives. */
browser_handle lab_flow_transport(lab_flow_transport_kind_t kind, frame_handle frame, view_handle view)
{
    (void)view;
    if (kind == LAB_FLOW_TRANSPORT_POST)
        return lab_flow_transport_post(frame);
    if (kind == LAB_FLOW_TRANSPORT_RUN || kind == LAB_FLOW_TRANSPORT_UI_RUN)
        return lab_flow_transport_run(frame, kind == LAB_FLOW_TRANSPORT_UI_RUN);
    return lab_flow_done(0);
}
