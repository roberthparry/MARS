/**
 * @file lab_flow_request.c
 * @brief Native sequencing of Lab requests, guards and browser publication.
 *
 * The browser retains Promises, abort controllers and response objects. This module
 * decides when to request, validate, publish or discard their results, using
 * request-local continuation frames. Text remains opaque: mathematical preparation
 * is performed by the native server, never by this client module.
 */
#include "lab_mode.h"
#include "lab_dom.h"
#include "lab_flow.h"
#include "lab_flow_request.h"
#define MARS_LAB_FLOW_REQUEST_INTERNAL_ACCESS
#include "lab_flow_request_internal.h"

/* Shared request-local stage access for continuation handlers. */
unsigned lab_flow_request_stage(frame_jsref frame)
{
    return (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
}

/* Shared request-local truth access for continuation handlers. */
bool lab_flow_request_truth(frame_jsref frame, const char *key)
{
    return lab_dom_truth(lab_dom_get(frame, key));
}

/* Shared request-local fallback access for continuation handlers. */
browser_jsref lab_flow_request_fallback(browser_jsref value, const char *fallback)
{
    return lab_dom_truth(value) ? value : lab_dom_string(fallback);
}

/* Shared request-local current access for continuation handlers. */
browser_jsref lab_flow_request_current(frame_jsref frame, unsigned next)
{
    return lab_flow_call(frame, next, "requestCurrent", "current", 0, 1, lab_dom_get(frame, "request"));
}

static browser_jsref lab_flow_request_finish(frame_jsref frame, unsigned next)
{
    lab_dom_delete(frame, "failed");
    return lab_flow_call(frame, next, "requestFinish", 0, 0, 1, lab_dom_get(frame, "request"));
}

/* Shared request-local response access for continuation handlers. */
browser_jsref lab_flow_request_response(frame_jsref frame)
{
    return lab_dom_get(lab_dom_get(frame, "result"), "response");
}

/* Shared request-local data access for continuation handlers. */
browser_jsref lab_flow_request_data(frame_jsref frame)
{
    return lab_dom_get(lab_dom_get(frame, "result"), "data");
}

static int lab_flow_request_ok(frame_jsref frame)
{
    return lab_dom_truth(lab_dom_get(lab_flow_request_response(frame), "ok")) &&
           lab_dom_truth(lab_dom_get(lab_flow_request_data(frame), "ok"));
}

/* Direct stage dispatch preserves the request-local protocol across browser awaits. */
enum {
    flow_request_function_stage_source,
    flow_request_function_stage_clear,
    flow_request_function_stage_begin,
    flow_request_function_stage_start,
    flow_request_function_stage_payload,
    flow_request_function_stage_post,
    flow_request_function_stage_outcome,
    flow_request_function_stage_complete,
    flow_request_function_stage_final_current,
    flow_request_function_stage_final,
    flow_request_function_stage_finish,
    flow_request_function_stage_done,
    flow_request_function_stage_error_current,
    flow_request_function_stage_error,
    flow_request_function_stage_error_final
};

struct lab_flow_request_function_context {
    frame_jsref frame;
    browser_jsref request;
};

static browser_jsref lab_flow_request_function_source(struct lab_flow_request_function_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_function_stage_clear, "native", "source", 0, 1,
                         lab_dom_string("lab_function_source"));
}

static browser_jsref lab_flow_request_function_clear(struct lab_flow_request_function_context *ctx)
{
    if (!lab_dom_get(ctx->frame, "source"))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, flow_request_function_stage_begin, "clearFunctionRun", 0, 0, 0);
}

static browser_jsref lab_flow_request_function_begin(struct lab_flow_request_function_context *ctx)
{
    int options = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(options, "input", lab_dom_scalar(LAB_VALUE_BOOLEAN, lab_flow_request_truth(ctx->frame, "source")));
    lab_dom_set(options, "button", lab_dom_id("functionRun"));
    lab_dom_set(options, "disableButton", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
    return lab_flow_call(ctx->frame, flow_request_function_stage_start, "requestBeginCurrent", "request", 0, 2,
                         lab_dom_string("function"), options);
}

static browser_jsref lab_flow_request_function_start(struct lab_flow_request_function_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_function_stage_payload, "native", 0, 0, 2,
                         lab_dom_string("lab_function_start"), lab_dom_numeric(lab_dom_truth(ctx->request)));
}

static browser_jsref lab_flow_request_function_payload(struct lab_flow_request_function_context *ctx)
{
    if (!ctx->request)
        return lab_flow_done(0);
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(flow_request_function_stage_error_current));
    int inputs = lab_dom_object(LAB_VALUE_ARRAY);
    lab_dom_push(inputs, lab_dom_get(ctx->frame, "source"));
    return lab_flow_call(ctx->frame, flow_request_function_stage_post, "buildPayload", "payload", 0, 3,
                         lab_dom_numeric(5), inputs, ctx->request);
}

static browser_jsref lab_flow_request_function_post(struct lab_flow_request_function_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_function_stage_outcome, "post", "result", 1, 3, ctx->request,
                         lab_dom_get(ctx->frame, "payload"), lab_dom_string("/function-run"));
}

static browser_jsref lab_flow_request_function_outcome(struct lab_flow_request_function_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_function_stage_complete, "requestOutcome", "outcome", 0, 3,
                         ctx->request, lab_flow_request_response(ctx->frame), lab_flow_request_data(ctx->frame));
}

static browser_jsref lab_flow_request_function_complete(struct lab_flow_request_function_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_function_stage_final_current, "functionComplete", 0, 0, 3,
                         ctx->request, lab_dom_get(ctx->frame, "outcome"), lab_flow_request_data(ctx->frame));
}

static browser_jsref lab_flow_request_function_error_current(struct lab_flow_request_function_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(flow_request_function_stage_error_final));
    return lab_flow_request_current(ctx->frame, flow_request_function_stage_error);
}

static browser_jsref lab_flow_request_function_error(struct lab_flow_request_function_context *ctx)
{
    if (!lab_flow_request_truth(ctx->frame, "current"))
        return lab_flow_request_current(ctx->frame, flow_request_function_stage_final);
    return lab_flow_call(ctx->frame, flow_request_function_stage_final_current, "functionException", 0, 0, 2,
                         ctx->request, lab_dom_get(ctx->frame, "exception"));
}

static browser_jsref lab_flow_request_function_final_current(struct lab_flow_request_function_context *ctx)
{
    lab_dom_delete(ctx->frame, "failed");
    return lab_flow_request_current(ctx->frame, flow_request_function_stage_final);
}

static browser_jsref lab_flow_request_function_final(struct lab_flow_request_function_context *ctx)
{
    if (lab_flow_request_truth(ctx->frame, "current"))
        lab_dom_disabled(lab_dom_id("functionRun"), 0);
    return lab_flow_request_finish(ctx->frame, flow_request_function_stage_done);
}

static browser_jsref lab_flow_request_function_finish(struct lab_flow_request_function_context *ctx)
{
    return lab_flow_request_finish(ctx->frame, flow_request_function_stage_done);
}

static browser_jsref lab_flow_request_function_error_final(struct lab_flow_request_function_context *ctx)
{
    lab_dom_set(ctx->frame, "propagate", lab_dom_get(ctx->frame, "exception"));
    lab_dom_delete(ctx->frame, "failed");
    return lab_flow_request_current(ctx->frame, flow_request_function_stage_final);
}

static browser_jsref lab_flow_request_function_done(struct lab_flow_request_function_context *ctx)
{
    if (lab_dom_get(ctx->frame, "propagate"))
        return lab_flow_call(ctx->frame, flow_request_function_stage_done + 100, "rethrow", 0, 0, 1,
                             lab_dom_get(ctx->frame, "propagate"));
    return lab_flow_done(0);
}

static browser_jsref lab_flow_request_function_otherwise(struct lab_flow_request_function_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static browser_jsref lab_flow_request_function(frame_jsref frame)
{
    int request = lab_dom_get(frame, "request");
    struct lab_flow_request_function_context ctx = {
        .frame = frame,
        .request = request,
    };
    static browser_jsref (*const handlers[])(struct lab_flow_request_function_context *) = {
        [flow_request_function_stage_source]        = lab_flow_request_function_source,
        [flow_request_function_stage_clear]         = lab_flow_request_function_clear,
        [flow_request_function_stage_begin]         = lab_flow_request_function_begin,
        [flow_request_function_stage_start]         = lab_flow_request_function_start,
        [flow_request_function_stage_payload]       = lab_flow_request_function_payload,
        [flow_request_function_stage_post]          = lab_flow_request_function_post,
        [flow_request_function_stage_outcome]       = lab_flow_request_function_outcome,
        [flow_request_function_stage_complete]      = lab_flow_request_function_complete,
        [flow_request_function_stage_error_current] = lab_flow_request_function_error_current,
        [flow_request_function_stage_error]         = lab_flow_request_function_error,
        [flow_request_function_stage_final_current] = lab_flow_request_function_final_current,
        [flow_request_function_stage_final]         = lab_flow_request_function_final,
        [flow_request_function_stage_finish]        = lab_flow_request_function_finish,
        [flow_request_function_stage_error_final]   = lab_flow_request_function_error_final,
        [flow_request_function_stage_done]          = lab_flow_request_function_done,
    };
    unsigned selected = lab_flow_request_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_request_function_otherwise(&ctx);
}

enum {
    flow_request_integrator_stage_snapshot,
    flow_request_integrator_stage_prepare,
    flow_request_integrator_stage_guard,
    flow_request_integrator_stage_validate,
    flow_request_integrator_stage_cancel,
    flow_request_integrator_stage_abort,
    flow_request_integrator_stage_current,
    flow_request_integrator_stage_text,
    flow_request_integrator_stage_check_text,
    flow_request_integrator_stage_rows,
    flow_request_integrator_stage_check_rows,
    flow_request_integrator_stage_render,
    flow_request_integrator_stage_bounds,
    flow_request_integrator_stage_check,
    flow_request_integrator_stage_build,
    flow_request_integrator_stage_post,
    flow_request_integrator_stage_done
};

struct lab_flow_request_integrator_context {
    frame_jsref frame;
    browser_jsref request;
    snapshot_jsref snapshot;
};

static browser_jsref lab_flow_request_integrator_snapshot(struct lab_flow_request_integrator_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_integrator_stage_prepare, "integratorSnapshot", "snapshot", 0, 0);
}

static browser_jsref lab_flow_request_integrator_prepare(struct lab_flow_request_integrator_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_integrator_stage_guard, "refreshIntegratorForms", "preparedRows", 1,
                         2, lab_dom_get(ctx->snapshot, "rows"), lab_dom_get(ctx->snapshot, "text"));
}

static browser_jsref lab_flow_request_integrator_guard(struct lab_flow_request_integrator_context *ctx)
{
    if (!lab_flow_request_truth(ctx->frame, "preparedRows"))
        return ctx->request ? lab_flow_request_current(ctx->frame, flow_request_integrator_stage_cancel)
                            : lab_flow_call(ctx->frame, flow_request_integrator_stage_done, "abort", 0, 0, 1,
                                            lab_dom_string("Obsolete integrator form preparation"));
    return lab_flow_call(ctx->frame, flow_request_integrator_stage_validate, "requestContext", "latestContext", 0, 0);
}

static browser_jsref lab_flow_request_integrator_validate(struct lab_flow_request_integrator_context *ctx)
{
    if (!lab_dom_equal(lab_dom_get(ctx->snapshot, "context"), lab_dom_get(ctx->frame, "latestContext"))) {
        if (ctx->request)
            return lab_flow_request_current(ctx->frame, flow_request_integrator_stage_cancel);
        return lab_flow_call(ctx->frame, flow_request_integrator_stage_done, "abort", 0, 0, 1,
                             lab_dom_string("Obsolete integrator form preparation"));
    }
    if (ctx->request)
        return lab_flow_request_current(ctx->frame, flow_request_integrator_stage_current);
    return lab_flow_call(ctx->frame, flow_request_integrator_stage_check_text, "integratorText", "latestText", 0, 0);
}

static browser_jsref lab_flow_request_integrator_current(struct lab_flow_request_integrator_context *ctx)
{
    if (!lab_flow_request_truth(ctx->frame, "current"))
        return lab_flow_request_current(ctx->frame, flow_request_integrator_stage_cancel);
    return lab_flow_call(ctx->frame, flow_request_integrator_stage_check_text, "integratorText", "latestText", 0, 0);
}

static browser_jsref lab_flow_request_integrator_check_text(struct lab_flow_request_integrator_context *ctx)
{
    if (!lab_dom_equal(lab_dom_get(ctx->snapshot, "text"), lab_dom_get(ctx->frame, "latestText")))
        return ctx->request ? lab_flow_request_current(ctx->frame, flow_request_integrator_stage_cancel)
                            : lab_flow_call(ctx->frame, flow_request_integrator_stage_done, "abort", 0, 0, 1,
                                            lab_dom_string("Obsolete integrator form preparation"));
    return lab_flow_call(ctx->frame, flow_request_integrator_stage_check_rows, "integratorRowsText", "latestRowsText",
                         0, 0);
}

static browser_jsref lab_flow_request_integrator_check_rows(struct lab_flow_request_integrator_context *ctx)
{
    if (!lab_dom_equal(lab_dom_get(ctx->snapshot, "rowsText"), lab_dom_get(ctx->frame, "latestRowsText")))
        return ctx->request ? lab_flow_request_current(ctx->frame, flow_request_integrator_stage_cancel)
                            : lab_flow_call(ctx->frame, flow_request_integrator_stage_done, "abort", 0, 0, 1,
                                            lab_dom_string("Obsolete integrator form preparation"));
    return lab_flow_call(ctx->frame, flow_request_integrator_stage_render, "activeIntegratorRows", "active", 0, 2,
                         lab_dom_get(ctx->frame, "preparedRows"), lab_dom_get(ctx->snapshot, "text"));
}

static browser_jsref lab_flow_request_integrator_cancel(struct lab_flow_request_integrator_context *ctx)
{
    if (lab_flow_request_truth(ctx->frame, "current"))
        return lab_flow_call(ctx->frame, flow_request_integrator_stage_abort, "requestCancel", 0, 0, 1,
                             lab_dom_string("evaluate"));
    /* A stale preparation still rejects even when its request has already ended. */
    return lab_flow_call(ctx->frame, flow_request_integrator_stage_done, "abort", 0, 0, 1,
                         lab_dom_string("Obsolete integrator form preparation"));
}

static browser_jsref lab_flow_request_integrator_abort(struct lab_flow_request_integrator_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_integrator_stage_done, "abort", 0, 0, 1,
                         lab_dom_string("Obsolete integrator form preparation"));
}

static browser_jsref lab_flow_request_integrator_render(struct lab_flow_request_integrator_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_integrator_stage_bounds, "renderIntegratorRows", 0, 0, 1,
                         lab_dom_get(lab_dom_get(ctx->frame, "active"), "rows"));
}

static browser_jsref lab_flow_request_integrator_bounds(struct lab_flow_request_integrator_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_integrator_stage_check, "native", "boundsError", 0, 2,
                         lab_dom_string("lab_payload_bounds_error"),
                         lab_dom_get(lab_dom_get(ctx->frame, "active"), "bounds"));
}

static browser_jsref lab_flow_request_integrator_check(struct lab_flow_request_integrator_context *ctx)
{
    if (lab_flow_request_truth(ctx->frame, "boundsError"))
        return lab_flow_call(ctx->frame, flow_request_integrator_stage_done, "fail", 0, 0, 1,
                             lab_dom_get(ctx->frame, "boundsError"));
    return lab_flow_call(ctx->frame, flow_request_integrator_stage_build, "saveWorksheetState", 0, 0, 1,
                         lab_dom_string("integrator"));
}

static browser_jsref lab_flow_request_integrator_build(struct lab_flow_request_integrator_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_integrator_stage_post, "integratorPayload", "payload", 0, 3,
                         lab_dom_get(ctx->snapshot, "text"), lab_dom_get(lab_dom_get(ctx->frame, "active"), "bounds"),
                         ctx->request);
}

static browser_jsref lab_flow_request_integrator_post(struct lab_flow_request_integrator_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_integrator_stage_done, "post", "value", 1, 3, ctx->request,
                         lab_dom_get(ctx->frame, "payload"), lab_dom_string("/integrator-eval"));
}

static browser_jsref lab_flow_request_integrator_otherwise(struct lab_flow_request_integrator_context *ctx)
{
    return lab_flow_done(lab_dom_get(ctx->frame, "value"));
}

static browser_jsref lab_flow_request_integrator(frame_jsref frame)
{
    int request = lab_dom_get(frame, "request"), snapshot = lab_dom_get(frame, "snapshot");
    struct lab_flow_request_integrator_context ctx = {
        .frame = frame,
        .request = request,
        .snapshot = snapshot,
    };
    static browser_jsref (*const handlers[])(struct lab_flow_request_integrator_context *) = {
        [flow_request_integrator_stage_snapshot]   = lab_flow_request_integrator_snapshot,
        [flow_request_integrator_stage_prepare]    = lab_flow_request_integrator_prepare,
        [flow_request_integrator_stage_guard]      = lab_flow_request_integrator_guard,
        [flow_request_integrator_stage_validate]   = lab_flow_request_integrator_validate,
        [flow_request_integrator_stage_current]    = lab_flow_request_integrator_current,
        [flow_request_integrator_stage_check_text] = lab_flow_request_integrator_check_text,
        [flow_request_integrator_stage_check_rows] = lab_flow_request_integrator_check_rows,
        [flow_request_integrator_stage_cancel]     = lab_flow_request_integrator_cancel,
        [flow_request_integrator_stage_abort]      = lab_flow_request_integrator_abort,
        [flow_request_integrator_stage_render]     = lab_flow_request_integrator_render,
        [flow_request_integrator_stage_bounds]     = lab_flow_request_integrator_bounds,
        [flow_request_integrator_stage_check]      = lab_flow_request_integrator_check,
        [flow_request_integrator_stage_build]      = lab_flow_request_integrator_build,
        [flow_request_integrator_stage_post]       = lab_flow_request_integrator_post,
    };
    unsigned selected = lab_flow_request_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_request_integrator_otherwise(&ctx);
}

enum {
    flow_request_land_stage_start,
    flow_request_land_stage_parent,
    flow_request_land_stage_begin,
    flow_request_land_stage_payload,
    flow_request_land_stage_post,
    flow_request_land_stage_current,
    flow_request_land_stage_accept,
    flow_request_land_stage_bind,
    flow_request_land_stage_finish,
    flow_request_land_stage_done,
    flow_request_land_stage_error_current,
    flow_request_land_stage_error
};

struct lab_flow_request_land_context {
    frame_jsref frame;
    browser_jsref request;
};

static browser_jsref lab_flow_request_land_start(struct lab_flow_request_land_context *ctx)
{
    int cells = lab_dom_all(lab_dom_id("rendered"), "[data-almanac-land-totality]");
    if (!lab_dom_count(cells) || !lab_flow_request_truth(ctx->frame, "data"))
        return lab_flow_done(0);
    lab_dom_set(ctx->frame, "cells", cells);
    return lab_flow_call(ctx->frame, flow_request_land_stage_begin, "requestLatestMain", "parent", 0, 0);
}

static browser_jsref lab_flow_request_land_begin(struct lab_flow_request_land_context *ctx)
{
    int options = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(options, "parent", lab_dom_get(ctx->frame, "parent"));
    return lab_flow_call(ctx->frame, flow_request_land_stage_payload, "requestBegin", "request", 0, 3,
                         lab_dom_string("land"), lab_dom_string("almanac"), options);
}

static browser_jsref lab_flow_request_land_payload(struct lab_flow_request_land_context *ctx)
{
    if (!ctx->request)
        return lab_flow_done(0);
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(flow_request_land_stage_error_current));
    return lab_flow_call(ctx->frame, flow_request_land_stage_post, "landPayload", "payload", 0, 2,
                         lab_dom_get(ctx->frame, "data"), lab_dom_get(ctx->frame, "cells"));
}

static browser_jsref lab_flow_request_land_post(struct lab_flow_request_land_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_land_stage_current, "post", "result", 1, 2, ctx->request,
                         lab_dom_get(ctx->frame, "payload"));
}

static browser_jsref lab_flow_request_land_current(struct lab_flow_request_land_context *ctx)
{
    return lab_flow_request_current(ctx->frame, flow_request_land_stage_accept);
}

static browser_jsref lab_flow_request_land_accept(struct lab_flow_request_land_context *ctx)
{
    if (!lab_flow_request_truth(ctx->frame, "current"))
        return lab_flow_request_finish(ctx->frame, flow_request_land_stage_done);
    if (!lab_flow_request_ok(ctx->frame))
        return lab_flow_call(ctx->frame, flow_request_land_stage_done, "fail", 0, 0, 1,
                             lab_flow_request_fallback(lab_dom_get(lab_flow_request_data(ctx->frame), "error"),
                                                       "Nearest land totality search failed"));
    return lab_flow_call(ctx->frame, flow_request_land_stage_bind, "native", 0, 0, 4,
                         lab_dom_string("lab_payload_land_apply"), lab_dom_get(ctx->frame, "cells"),
                         lab_flow_request_data(ctx->frame), lab_dom_numeric(0));
}

static browser_jsref lab_flow_request_land_bind(struct lab_flow_request_land_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_land_stage_finish, "bindAlmanacTotalityActions", 0, 0, 1,
                         lab_dom_id("rendered"));
}

static browser_jsref lab_flow_request_land_error_current(struct lab_flow_request_land_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(flow_request_land_stage_finish));
    return lab_flow_request_current(ctx->frame, flow_request_land_stage_error);
}

static browser_jsref lab_flow_request_land_error(struct lab_flow_request_land_context *ctx)
{
    if (!lab_flow_request_truth(ctx->frame, "current"))
        return lab_flow_request_finish(ctx->frame, flow_request_land_stage_done);
    return lab_flow_call(ctx->frame, flow_request_land_stage_finish, "native", 0, 0, 4,
                         lab_dom_string("lab_payload_land_apply"), lab_dom_get(ctx->frame, "cells"), 0,
                         lab_dom_numeric(lab_dom_equal(lab_dom_get(lab_dom_get(ctx->frame, "exception"), "name"),
                                                       lab_dom_string("AbortError"))
                                             ? 1
                                             : 2));
}

static browser_jsref lab_flow_request_land_finish(struct lab_flow_request_land_context *ctx)
{
    return lab_flow_request_finish(ctx->frame, flow_request_land_stage_done);
}

static browser_jsref lab_flow_request_land_otherwise(struct lab_flow_request_land_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static browser_jsref lab_flow_request_land(frame_jsref frame)
{
    int request = lab_dom_get(frame, "request");
    struct lab_flow_request_land_context ctx = {
        .frame = frame,
        .request = request,
    };
    static browser_jsref (*const handlers[])(struct lab_flow_request_land_context *) = {
        [flow_request_land_stage_start]         = lab_flow_request_land_start,
        [flow_request_land_stage_begin]         = lab_flow_request_land_begin,
        [flow_request_land_stage_payload]       = lab_flow_request_land_payload,
        [flow_request_land_stage_post]          = lab_flow_request_land_post,
        [flow_request_land_stage_current]       = lab_flow_request_land_current,
        [flow_request_land_stage_accept]        = lab_flow_request_land_accept,
        [flow_request_land_stage_bind]          = lab_flow_request_land_bind,
        [flow_request_land_stage_error_current] = lab_flow_request_land_error_current,
        [flow_request_land_stage_error]         = lab_flow_request_land_error,
        [flow_request_land_stage_finish]        = lab_flow_request_land_finish,
    };
    unsigned selected = lab_flow_request_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_flow_request_land_otherwise(&ctx);
}

enum {
    flow_request_holidays_stage_mode,
    flow_request_holidays_stage_begin,
    flow_request_holidays_stage_status,
    flow_request_holidays_stage_fetch,
    flow_request_holidays_stage_current,
    flow_request_holidays_stage_accept,
    flow_request_holidays_stage_ready,
    flow_request_holidays_stage_finish,
    flow_request_holidays_stage_done,
    flow_request_holidays_stage_error_current,
    flow_request_holidays_stage_error
};

struct lab_flow_request_holidays_context {
    frame_jsref frame;
    browser_jsref request;
};

static browser_jsref lab_flow_request_holidays_mode(struct lab_flow_request_holidays_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_holidays_stage_begin, "currentMode", "mode", 0, 0);
}

static browser_jsref lab_flow_request_holidays_begin(struct lab_flow_request_holidays_context *ctx)
{
    if (!lab_dom_equal(lab_dom_get(ctx->frame, "mode"), lab_dom_string("datetime")))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, flow_request_holidays_stage_status, "requestBegin", "request", 0, 2,
                         lab_dom_string("holidays"), lab_dom_string("datetime"));
}

static browser_jsref lab_flow_request_holidays_status(struct lab_flow_request_holidays_context *ctx)
{
    if (!ctx->request)
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, flow_request_holidays_stage_fetch, "setStatus", 0, 0, 1,
                         lab_dom_string("Refreshing holidays..."));
}

static browser_jsref lab_flow_request_holidays_fetch(struct lab_flow_request_holidays_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(flow_request_holidays_stage_error_current));
    int fetch = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(fetch, "kind", lab_dom_string("datetime"));
    return lab_flow_call(ctx->frame, flow_request_holidays_stage_current, "fetch", "result", 1, 2, fetch,
                         ctx->request);
}

static browser_jsref lab_flow_request_holidays_current(struct lab_flow_request_holidays_context *ctx)
{
    return lab_flow_request_current(ctx->frame, flow_request_holidays_stage_accept);
}

static browser_jsref lab_flow_request_holidays_accept(struct lab_flow_request_holidays_context *ctx)
{
    if (!lab_flow_request_truth(ctx->frame, "current"))
        return lab_flow_request_finish(ctx->frame, flow_request_holidays_stage_done);
    if (!lab_flow_request_ok(ctx->frame))
        return lab_flow_call(ctx->frame, flow_request_holidays_stage_finish, "setStatus", 0, 0, 1,
                             lab_dom_string("Error"));
    return lab_flow_call(ctx->frame, flow_request_holidays_stage_ready, "setDatetimeLocalText", 0, 0, 2,
                         lab_flow_request_fallback(lab_dom_get(lab_flow_request_data(ctx->frame), "local"), ""),
                         lab_dom_truth(lab_dom_get(lab_flow_request_data(ctx->frame), "local_sections"))
                             ? lab_dom_get(lab_flow_request_data(ctx->frame), "local_sections")
                             : lab_dom_object(LAB_VALUE_ARRAY));
}

static browser_jsref lab_flow_request_holidays_ready(struct lab_flow_request_holidays_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_holidays_stage_finish, "setStatus", 0, 0, 1, lab_dom_string("Ready"));
}

static browser_jsref lab_flow_request_holidays_error_current(struct lab_flow_request_holidays_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(flow_request_holidays_stage_finish));
    return lab_flow_request_current(ctx->frame, flow_request_holidays_stage_error);
}

static browser_jsref lab_flow_request_holidays_error(struct lab_flow_request_holidays_context *ctx)
{
    if (lab_flow_request_truth(ctx->frame, "current"))
        return lab_flow_call(ctx->frame, flow_request_holidays_stage_finish, "setStatus", 0, 0, 1,
                             lab_dom_string("Error"));
    return lab_flow_request_finish(ctx->frame, flow_request_holidays_stage_done);
}

static browser_jsref lab_flow_request_holidays_finish(struct lab_flow_request_holidays_context *ctx)
{
    return lab_flow_request_finish(ctx->frame, flow_request_holidays_stage_done);
}

static browser_jsref lab_flow_request_holidays_otherwise(struct lab_flow_request_holidays_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static browser_jsref lab_flow_request_holidays(frame_jsref frame)
{
    int request = lab_dom_get(frame, "request");
    struct lab_flow_request_holidays_context ctx = {
        .frame = frame,
        .request = request,
    };
    static browser_jsref (*const handlers[])(struct lab_flow_request_holidays_context *) = {
        [flow_request_holidays_stage_mode]          = lab_flow_request_holidays_mode,
        [flow_request_holidays_stage_begin]         = lab_flow_request_holidays_begin,
        [flow_request_holidays_stage_status]        = lab_flow_request_holidays_status,
        [flow_request_holidays_stage_fetch]         = lab_flow_request_holidays_fetch,
        [flow_request_holidays_stage_current]       = lab_flow_request_holidays_current,
        [flow_request_holidays_stage_accept]        = lab_flow_request_holidays_accept,
        [flow_request_holidays_stage_ready]         = lab_flow_request_holidays_ready,
        [flow_request_holidays_stage_error_current] = lab_flow_request_holidays_error_current,
        [flow_request_holidays_stage_error]         = lab_flow_request_holidays_error,
        [flow_request_holidays_stage_finish]        = lab_flow_request_holidays_finish,
    };
    unsigned selected = lab_flow_request_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_request_holidays_otherwise(&ctx);
}

enum {
    flow_request_mobile_stage_start,
    flow_request_mobile_stage_response,
    flow_request_mobile_stage_data,
    flow_request_mobile_stage_done
};

static browser_jsref lab_flow_request_mobile_start(frame_jsref frame)
{
    if (!lab_dom_id("mobileAccess") || !lab_dom_id("mobileUrl") || !lab_dom_id("mobileQr"))
        return lab_flow_done(0);
    lab_dom_set(frame, "failed", lab_dom_numeric(flow_request_mobile_stage_done));
    return lab_flow_call(frame, flow_request_mobile_stage_response, "mobileFetch", "response", 1, 0);
}

static browser_jsref lab_flow_request_mobile_response(frame_jsref frame)
{
    if (!lab_dom_truth(lab_dom_get(lab_dom_get(frame, "response"), "ok")))
        return lab_flow_done(0);
    return lab_flow_call(frame, flow_request_mobile_stage_data, "responseData", "data", 1, 1,
                         lab_dom_get(frame, "response"));
}

static browser_jsref lab_flow_request_mobile_data(frame_jsref frame)
{
    return lab_flow_call(frame, flow_request_mobile_stage_done, "native", 0, 0, 2, lab_dom_string("lab_payload_mobile"),
                         lab_dom_get(frame, "data"));
}

static browser_jsref lab_flow_request_mobile_otherwise(frame_jsref frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static browser_jsref lab_flow_request_mobile(frame_jsref frame)
{
    static browser_jsref (*const handlers[])(browser_jsref) = {
        [flow_request_mobile_stage_start]    = lab_flow_request_mobile_start,
        [flow_request_mobile_stage_response] = lab_flow_request_mobile_response,
        [flow_request_mobile_stage_data]     = lab_flow_request_mobile_data,
    };
    unsigned selected = lab_flow_request_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](frame)
               : lab_flow_request_mobile_otherwise(frame);
}

enum {
    flow_request_forms_stage_fetch,
    flow_request_forms_stage_decode,
    flow_request_forms_stage_validate,
    flow_request_forms_stage_done
};

static browser_jsref lab_flow_request_forms_fetch(frame_jsref frame)
{
    return lab_flow_call(frame, flow_request_forms_stage_decode, "rawPost", "response", 1, 3, lab_dom_string("/forms"),
                         lab_dom_get(frame, "payload"), lab_dom_get(lab_dom_get(frame, "options"), "signal"));
}

static browser_jsref lab_flow_request_forms_decode(frame_jsref frame)
{
    return lab_flow_call(frame, flow_request_forms_stage_validate, "responseData", "data", 1, 1,
                         lab_dom_get(frame, "response"));
}

static browser_jsref lab_flow_request_forms_validate(frame_jsref frame)
{
    int response = lab_dom_get(frame, "response"), data = lab_dom_get(frame, "data");
    if (lab_dom_truth(lab_dom_get(response, "ok")) && lab_dom_truth(lab_dom_get(data, "ok")))
        return lab_flow_done(data);
    int error = lab_dom_get(data, "error");
    if (!lab_dom_truth(error))
        error = lab_dom_join(lab_dom_string("Form preparation failed ("), lab_dom_text(lab_dom_get(response, "status")),
                             ")");
    return lab_flow_call(frame, flow_request_forms_stage_done, "fail", 0, 0, 1, error);
}

static browser_jsref lab_flow_request_forms_otherwise(frame_jsref frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static browser_jsref lab_flow_request_forms(frame_jsref frame)
{
    static browser_jsref (*const handlers[])(browser_jsref) = {
        [flow_request_forms_stage_fetch]    = lab_flow_request_forms_fetch,
        [flow_request_forms_stage_decode]   = lab_flow_request_forms_decode,
        [flow_request_forms_stage_validate] = lab_flow_request_forms_validate,
    };
    unsigned selected = lab_flow_request_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](frame)
               : lab_flow_request_forms_otherwise(frame);
}

enum {
    flow_request_presentation_stage_context,
    flow_request_presentation_stage_current,
    flow_request_presentation_stage_fetch,
    flow_request_presentation_stage_decode,
    flow_request_presentation_stage_check_current,
    flow_request_presentation_stage_validate,
    flow_request_presentation_stage_accept,
    flow_request_presentation_stage_done
};

struct lab_flow_request_presentation_context {
    frame_jsref frame;
    browser_jsref request;
};


static browser_jsref lab_flow_request_presentation_context(struct lab_flow_request_presentation_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_presentation_stage_current, "requestContext", "context", 0, 0);
}

static browser_jsref lab_flow_request_presentation_check_current(struct lab_flow_request_presentation_context *ctx)
{
    return lab_flow_call(ctx->frame,
                         lab_flow_request_stage(ctx->frame) == flow_request_presentation_stage_current
                             ? flow_request_presentation_stage_fetch
                             : flow_request_presentation_stage_validate,
                         ctx->request ? "requestCurrent" : "requestContext", "current", 0, ctx->request ? 1 : 0,
                         ctx->request);
}

static browser_jsref lab_flow_request_presentation_validate(struct lab_flow_request_presentation_context *ctx)
{
    {
        int live = ctx->request ? lab_flow_request_truth(ctx->frame, "current")
                                : lab_dom_equal(lab_dom_get(ctx->frame, "context"), lab_dom_get(ctx->frame, "current"));
        if (!live)
            return lab_flow_call(ctx->frame, flow_request_presentation_stage_done, "abort", 0, 0, 1,
                                 lab_dom_string("Obsolete presentation request"));
        if (lab_flow_request_stage(ctx->frame) == flow_request_presentation_stage_fetch)
            return lab_flow_call(ctx->frame, flow_request_presentation_stage_decode, "rawPost", "response", 1, 3,
                                 lab_dom_string("/presentation"), lab_dom_get(ctx->frame, "payload"),
                                 lab_dom_get(lab_dom_get(ctx->request, "controller"), "signal"));
        int data = lab_dom_get(ctx->frame, "data"), response = lab_dom_get(ctx->frame, "response");
        if (!lab_dom_truth(lab_dom_get(response, "ok")) || !lab_dom_truth(lab_dom_get(data, "ok")))
            return lab_flow_call(
                ctx->frame, flow_request_presentation_stage_done, "fail", 0, 0, 1,
                lab_flow_request_fallback(lab_dom_get(data, "error"), "Native presentation request failed"));
        return lab_flow_call(ctx->frame, flow_request_presentation_stage_done, "installPresentation", 0, 0, 1, data);
    }
}

static browser_jsref lab_flow_request_presentation_decode(struct lab_flow_request_presentation_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_presentation_stage_check_current, "responseData", "data", 1, 1,
                         lab_dom_get(ctx->frame, "response"));
}

static browser_jsref lab_flow_request_presentation_otherwise(struct lab_flow_request_presentation_context *ctx)
{
    return lab_flow_done(lab_dom_get(ctx->frame, "data"));
}

static browser_jsref lab_flow_request_presentation(frame_jsref frame)
{
    int request = lab_dom_get(frame, "request");
    struct lab_flow_request_presentation_context ctx = {
        .frame = frame,
        .request = request,
    };
    static browser_jsref (*const handlers[])(struct lab_flow_request_presentation_context *) = {
        [flow_request_presentation_stage_context]       = lab_flow_request_presentation_context,
        [flow_request_presentation_stage_current]       = lab_flow_request_presentation_check_current,
        [flow_request_presentation_stage_check_current] = lab_flow_request_presentation_check_current,
        [flow_request_presentation_stage_fetch]         = lab_flow_request_presentation_validate,
        [flow_request_presentation_stage_validate]      = lab_flow_request_presentation_validate,
        [flow_request_presentation_stage_decode]        = lab_flow_request_presentation_decode,
    };
    unsigned selected = lab_flow_request_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_request_presentation_otherwise(&ctx);
}

enum {
    flow_request_editor_stage_cache,
    flow_request_editor_stage_resources,
    flow_request_editor_stage_context,
    flow_request_editor_stage_select,
    flow_request_editor_stage_done
};

struct lab_flow_request_editor_context {
    frame_jsref frame;
    view_jsref view;
};

static browser_jsref lab_flow_request_editor_cache(struct lab_flow_request_editor_context *ctx)
{
    int source = lab_dom_clean(lab_dom_text(lab_flow_request_fallback(lab_dom_get(ctx->frame, "text"), "")), LAB_DOM_CLEAN_TRIM);
    lab_dom_set(ctx->frame, "source", source);
    int editors = lab_dom_get(ctx->view, "editors");
    if (lab_dom_member(editors, source))
        return lab_flow_done(lab_dom_map_get(editors, source));
    return lab_flow_call(ctx->frame, flow_request_editor_stage_context, "requestContext", "context", 0, 0);
}

static browser_jsref lab_flow_request_editor_context(struct lab_flow_request_editor_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_editor_stage_select, "editorRequests", "requests", 0, 0);
}

static browser_jsref lab_flow_request_editor_select(struct lab_flow_request_editor_context *ctx)
{
    int source = lab_dom_get(ctx->frame, "source"), requests = lab_dom_get(ctx->frame, "requests");
    int pending = lab_dom_map_get(requests, source);
    if (pending && lab_dom_equal(lab_dom_get(pending, "context"), lab_dom_get(ctx->frame, "context")))
        return lab_flow_call(ctx->frame, flow_request_editor_stage_done, "awaitValue", "value", 1, 1,
                             lab_dom_get(pending, "promise"));
    pending = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(pending, "context", lab_dom_get(ctx->frame, "context"));
    lab_dom_set(pending, "promise", 0);
    int payload = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(payload, "action", lab_dom_string("editor"));
    lab_dom_set(payload, "operation", lab_dom_string("analyse"));
    lab_dom_set(payload, "text", source);
    return lab_flow_call(ctx->frame, flow_request_editor_stage_done, "editorPromise", "value", 1, 3, source, pending,
                         payload);
}

static browser_jsref lab_flow_request_editor_otherwise(struct lab_flow_request_editor_context *ctx)
{
    return lab_flow_done(lab_dom_get(ctx->frame, "value"));
}

static browser_jsref lab_flow_request_editor(frame_jsref frame, view_jsref view)
{
    struct lab_flow_request_editor_context ctx = {
        .frame = frame,
        .view = view,
    };
    static browser_jsref (*const handlers[])(struct lab_flow_request_editor_context *) = {
        [flow_request_editor_stage_cache]   = lab_flow_request_editor_cache,
        [flow_request_editor_stage_context] = lab_flow_request_editor_context,
        [flow_request_editor_stage_select]  = lab_flow_request_editor_select,
    };
    unsigned selected = lab_flow_request_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_request_editor_otherwise(&ctx);
}

static browser_jsref lab_flow_request_presentation_dispatch(frame_jsref frame, view_jsref view)
{
    return lab_flow_request_truth(frame, "editor") ? lab_flow_request_editor(frame, view)
                                                   : lab_flow_request_presentation(frame);
}

enum {
    flow_request_solver_stage_prepare,
    flow_request_solver_stage_begin,
    flow_request_solver_stage_install,
    flow_request_solver_stage_post,
    flow_request_solver_stage_publish,
    flow_request_solver_stage_cleanup,
    flow_request_solver_stage_finish,
    flow_request_solver_stage_done,
    flow_request_solver_stage_error
};

struct lab_flow_request_solver_context {
    frame_jsref frame;
    browser_jsref pending;
    browser_jsref request;
};

static browser_jsref lab_flow_request_solver_prepare(struct lab_flow_request_solver_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_solver_stage_begin, "solverPrepare", "pending", 0, 0);
}

static browser_jsref lab_flow_request_solver_begin(struct lab_flow_request_solver_context *ctx)
{
    if (!ctx->pending)
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, flow_request_solver_stage_install, "requestBegin", "request", 0, 3,
                         lab_dom_get(ctx->pending, "operation"), lab_dom_get(ctx->pending, "mode"),
                         lab_dom_get(ctx->pending, "options"));
}

static browser_jsref lab_flow_request_solver_install(struct lab_flow_request_solver_context *ctx)
{
    if (!ctx->request)
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, flow_request_solver_stage_post, "solverPending", 0, 0, 2, ctx->pending,
                         ctx->request);
}

static browser_jsref lab_flow_request_solver_post(struct lab_flow_request_solver_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(flow_request_solver_stage_error));
    return lab_flow_call(ctx->frame, flow_request_solver_stage_publish, "post", "result", 1, 3, ctx->request,
                         lab_dom_get(ctx->pending, "payload"), lab_dom_get(ctx->pending, "endpoint"));
}

static browser_jsref lab_flow_request_solver_publish(struct lab_flow_request_solver_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_solver_stage_cleanup, "solverPublish", 0, 0, 4, ctx->pending,
                         lab_flow_request_response(ctx->frame), lab_flow_request_data(ctx->frame), lab_dom_numeric(0));
}

static browser_jsref lab_flow_request_solver_error(struct lab_flow_request_solver_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(flow_request_solver_stage_cleanup));
    return lab_flow_call(ctx->frame, flow_request_solver_stage_cleanup, "solverPublish", 0, 0, 4, ctx->pending, 0, 0,
                         lab_dom_numeric(1));
}

static browser_jsref lab_flow_request_solver_cleanup(struct lab_flow_request_solver_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(flow_request_solver_stage_finish));
    return lab_flow_call(ctx->frame, flow_request_solver_stage_finish, "solverRelease", 0, 0, 1, ctx->pending);
}

static browser_jsref lab_flow_request_solver_finish(struct lab_flow_request_solver_context *ctx)
{
    return lab_flow_request_finish(ctx->frame, flow_request_solver_stage_done);
}

static browser_jsref lab_flow_request_solver_otherwise(struct lab_flow_request_solver_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static browser_jsref lab_flow_request_solver(frame_jsref frame)
{
    int pending = lab_dom_get(frame, "pending"), request = lab_dom_get(frame, "request");
    struct lab_flow_request_solver_context ctx = {
        .frame = frame,
        .pending = pending,
        .request = request,
    };
    static browser_jsref (*const handlers[])(struct lab_flow_request_solver_context *) = {
        [flow_request_solver_stage_prepare] = lab_flow_request_solver_prepare,
        [flow_request_solver_stage_begin]   = lab_flow_request_solver_begin,
        [flow_request_solver_stage_install] = lab_flow_request_solver_install,
        [flow_request_solver_stage_post]    = lab_flow_request_solver_post,
        [flow_request_solver_stage_publish] = lab_flow_request_solver_publish,
        [flow_request_solver_stage_error]   = lab_flow_request_solver_error,
        [flow_request_solver_stage_cleanup] = lab_flow_request_solver_cleanup,
        [flow_request_solver_stage_finish]  = lab_flow_request_solver_finish,
    };
    unsigned selected = lab_flow_request_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_request_solver_otherwise(&ctx);
}

enum {
    flow_request_fetch_stage_build,
    flow_request_fetch_stage_save,
    flow_request_fetch_stage_post,
    flow_request_fetch_stage_done
};

/** @brief Fetch-adapter choices, distinct from the seven worksheet mode identifiers. */
typedef enum {
    LAB_FLOW_FETCH_MATRIX                = 0,                /**< Matrix evaluation. */
    LAB_FLOW_FETCH_EQUATION              = 1,              /**< Equation evaluation. */
    LAB_FLOW_FETCH_DIFFERENTIAL_EQUATION = 2, /**< Differential-equation evaluation. */
    LAB_FLOW_FETCH_DATETIME              = 3,              /**< Date/time evaluation. */
    LAB_FLOW_FETCH_ALMANAC               = 4,               /**< Almanac evaluation. */
} lab_flow_fetch_kind_t;

static const char *const lab_flow_request_fetch_endpoints[] = {"/matrix-eval", "/equation-eval", "/diffequation-eval",
                                                               "/datetime-eval", "/almanac-eval"};

static const char *const lab_flow_request_fetch_modes[] = {"matrix", "equation", "diffequation", "datetime", "almanac"};

struct lab_flow_request_fetch_context {
    frame_jsref frame;
    lab_flow_fetch_kind_t kind;
};

static browser_jsref lab_flow_request_fetch_build(struct lab_flow_request_fetch_context *ctx)
{
    if (ctx->kind == LAB_FLOW_FETCH_EQUATION || ctx->kind == LAB_FLOW_FETCH_DIFFERENTIAL_EQUATION)
        return lab_flow_call(ctx->frame, flow_request_fetch_stage_save, "saveWorksheetState", 0, 0, 1,
                             lab_dom_string(lab_flow_request_fetch_modes[ctx->kind]));
    return lab_flow_call(ctx->frame, flow_request_fetch_stage_save, "fetchPayload", "payload", 0, 1, ctx->frame);
}

static browser_jsref lab_flow_request_fetch_save(struct lab_flow_request_fetch_context *ctx)
{
    if (ctx->kind == LAB_FLOW_FETCH_EQUATION || ctx->kind == LAB_FLOW_FETCH_DIFFERENTIAL_EQUATION)
        return lab_flow_call(ctx->frame, flow_request_fetch_stage_post, "fetchPayload", "payload", 0, 1, ctx->frame);
    if (ctx->kind == LAB_FLOW_FETCH_MATRIX && !lab_dom_truth(lab_dom_get(lab_dom_get(ctx->frame, "payload"), "transient")))
        return lab_flow_call(ctx->frame, flow_request_fetch_stage_post, "saveWorksheetState", 0, 0, 1,
                             lab_dom_string("matrix"));
    if (ctx->kind >= LAB_FLOW_FETCH_DATETIME)
        return lab_flow_call(ctx->frame, flow_request_fetch_stage_post,
                             ctx->kind == LAB_FLOW_FETCH_DATETIME ? "saveLastDatetimeState" : "saveLastAlmanacState", 0, 0, 0);
    return lab_flow_call(ctx->frame, flow_request_fetch_stage_done, "post", "value", 1, 3,
                         lab_dom_get(ctx->frame, "request"), lab_dom_get(ctx->frame, "payload"),
                         lab_dom_string(lab_flow_request_fetch_endpoints[ctx->kind]));
}

static browser_jsref lab_flow_request_fetch_post(struct lab_flow_request_fetch_context *ctx)
{
    return lab_flow_call(ctx->frame, flow_request_fetch_stage_done, "post", "value", 1, 3,
                         lab_dom_get(ctx->frame, "request"), lab_dom_get(ctx->frame, "payload"),
                         lab_dom_string(lab_flow_request_fetch_endpoints[ctx->kind]));
}

static browser_jsref lab_flow_request_fetch_otherwise(struct lab_flow_request_fetch_context *ctx)
{
    return lab_flow_done(lab_dom_get(ctx->frame, "value"));
}

static browser_jsref lab_flow_request_fetch(frame_jsref frame)
{
    lab_flow_fetch_kind_t kind = (lab_flow_fetch_kind_t)(unsigned)lab_dom_number(lab_dom_get(frame, "mode"));

    if ((unsigned)kind >= sizeof lab_flow_request_fetch_endpoints / sizeof *lab_flow_request_fetch_endpoints)
        return lab_flow_done(0);
    struct lab_flow_request_fetch_context ctx = {
        .frame = frame,
        .kind = kind,
    };
    static browser_jsref (*const handlers[])(struct lab_flow_request_fetch_context *) = {
        [flow_request_fetch_stage_build] = lab_flow_request_fetch_build,
        [flow_request_fetch_stage_save]  = lab_flow_request_fetch_save,
        [flow_request_fetch_stage_post]  = lab_flow_request_fetch_post,
    };
    unsigned selected = lab_flow_request_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_flow_request_fetch_otherwise(&ctx);
}

/* Direct workflow indexing; stages remain private to each independent protocol. */
browser_jsref lab_flow_request(lab_flow_request_kind_t kind, frame_jsref frame, view_jsref view)
{
    if (kind == LAB_FLOW_REQUEST_PRESENTATION)
        return lab_flow_request_presentation_dispatch(frame, view);
    static browser_jsref (*const workflows[])(browser_jsref) = {
        [LAB_FLOW_REQUEST_FUNCTION]     = lab_flow_request_function,
        [LAB_FLOW_REQUEST_CALCULUS]     = lab_flow_request_calculus,
        [LAB_FLOW_REQUEST_INTEGRATOR]   = lab_flow_request_integrator,
        [LAB_FLOW_REQUEST_LAND]         = lab_flow_request_land,
        [LAB_FLOW_REQUEST_HOLIDAYS]     = lab_flow_request_holidays,
        [LAB_FLOW_REQUEST_MOBILE]       = lab_flow_request_mobile,
        [LAB_FLOW_REQUEST_FORMS]        = lab_flow_request_forms,
        [LAB_FLOW_REQUEST_PRESENTATION] = lab_flow_request_presentation,
        [LAB_FLOW_REQUEST_SOLVER]       = lab_flow_request_solver,
        [LAB_FLOW_REQUEST_FETCH]        = lab_flow_request_fetch,
    };
    return (unsigned)kind < sizeof workflows / sizeof *workflows ? workflows[kind](frame) : lab_flow_done(0);
}
