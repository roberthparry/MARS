/**
 * @file lab_flow_location.c
 * @brief Calendar location request sequencing, ownership and deferred projection.
 *
 * Moves application decisions out of the browser adapters while preserving town
 * generation snapshots, author edits, request cancellation and finally cleanup.
 * Directly indexed continuation handlers describe each suspension point. Existing
 * native location APIs own field and town policy. Browser services retain
 * Intl/Date conversion, request resources and promises.
 */
#include "lab_dom.h"
#include "lab_flow.h"
#include "lab_flow_location.h"

static unsigned lab_flow_location_stage(int frame)
{
    return (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
}

static int lab_flow_location_null(void)
{
    int plan = lab_flow_done(0);
    lab_dom_set(plan, "value", 0);
    return plan;
}

static int lab_flow_location_flag(int value)
{
    return lab_dom_numeric(lab_dom_truth(value));
}

static int lab_flow_location_jump(int frame, unsigned next)
{
    return lab_flow_effects(frame, next, lab_dom_object(5));
}

static int lab_flow_location_forms(int frame)
{
    unsigned action = (unsigned)lab_dom_number(lab_dom_get(frame, "action"));
    if (!lab_flow_location_stage(frame)) {
        int option = lab_dom_get(frame, "option"), value = lab_dom_get(frame, "value");
        if (action == 2 && (!lab_dom_truth(option) || !lab_dom_truth(value)))
            return lab_flow_done(lab_dom_scalar(1, 0));
        int payload = lab_dom_object(5);
        lab_dom_set(payload, "action", lab_dom_string(action ? "town" : "time"));
        lab_dom_set(payload, action ? "value" : "text",
                    lab_dom_text(lab_dom_truth(value) ? value : lab_dom_string("")));
        if (action == 2) {
            int candidates = lab_dom_object(4);
            lab_dom_push(candidates, lab_dom_get(option, "value"));
            lab_dom_set(payload, "candidates", candidates);
        }
        return lab_flow_call(frame, 1, "locForms", "response", 1, 1, payload);
    }
    int response = lab_dom_get(frame, "response");
    if (action == 2) {
        int index = lab_dom_get(response, "match_index");
        return lab_flow_done(lab_dom_scalar(1, lab_dom_type(index) == 2 && lab_dom_number(index) == 0));
    }
    return lab_flow_done(lab_dom_get(response, action ? "town" : "text"));
}

struct lab_flow_location_restore_context {
    int frame;
    int select;
    int snapshot;
    int plan;
};

static int lab_flow_location_restore_otherwise(struct lab_flow_location_restore_context *ctx);

static int lab_flow_location_restore_loc_current(struct lab_flow_location_restore_context *ctx)
{
    return lab_flow_call(ctx->frame, 1, "locCurrent", "current", 0, 1, lab_dom_get(ctx->frame, "isCurrent"));
}

static int lab_flow_location_restore_loc_populate(struct lab_flow_location_restore_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_location_null();
    if (!lab_dom_truth(ctx->select))
        return lab_flow_done(lab_dom_scalar(1, 0));
    int options = lab_dom_object(5);
    lab_dom_set(options, "selectDefault", lab_dom_scalar(1, 0));
    lab_dom_set(options, "isCurrent", lab_dom_get(ctx->frame, "isCurrent"));
    return lab_flow_call(ctx->frame, 2, "locPopulate", "preparation", 0, 3, ctx->select,
                         lab_dom_get(ctx->frame, "jurisdiction"), options);
}

static int lab_flow_location_restore_native(struct lab_flow_location_restore_context *ctx)
{
    return lab_flow_call(ctx->frame, 3, "native", "snapshot", 0, 2, lab_dom_string("lab_location_snapshot"),
                         ctx->select);
}

static int lab_flow_location_restore_loc_await(struct lab_flow_location_restore_context *ctx)
{
    return lab_flow_call(ctx->frame, 4, "locAwait", 0, 1, 1, lab_dom_get(ctx->frame, "preparation"));
}

static int lab_flow_location_restore_loc_current_4(struct lab_flow_location_restore_context *ctx)
{
    return lab_flow_call(ctx->frame, 5, "locCurrent", "current", 0, 1, lab_dom_get(ctx->frame, "isCurrent"));
}

static int lab_flow_location_restore_native_5(struct lab_flow_location_restore_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_location_null();
    return lab_flow_call(ctx->frame, 6, "native", "unchanged", 0, 3, lab_dom_string("lab_location_unchanged"),
                         ctx->select, ctx->snapshot);
}

static int lab_flow_location_restore_native_6(struct lab_flow_location_restore_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "unchanged")))
        return lab_flow_location_null();
    return lab_flow_call(ctx->frame, 7, "native", "plan", 0, 6, lab_dom_string("lab_location_restore"),
                         lab_dom_numeric(lab_dom_truth(lab_dom_get(ctx->frame, "responded"))), ctx->select,
                         ctx->snapshot, lab_dom_get(ctx->frame, "selection"), lab_dom_get(ctx->frame, "response"));
}

static int lab_flow_location_restore_loc_forms(struct lab_flow_location_restore_context *ctx)
{
    if (lab_dom_truth(lab_dom_get(ctx->plan, "request"))) {
        lab_dom_set(ctx->frame, "responded", lab_dom_scalar(1, 1));
        return lab_flow_call(ctx->frame, 4, "locForms", "response", 1, 1, lab_dom_get(ctx->plan, "request"));
    }
    if (lab_dom_truth(lab_dom_get(ctx->plan, "controls")))
        return lab_flow_call(ctx->frame, 8, "locApplyTown", 0, 0, 1, lab_dom_get(ctx->plan, "controls"));
    /* Continue: coordinate fallback does not apply named-town controls. */
    return lab_flow_location_restore_otherwise(ctx);
}

static int lab_flow_location_restore_otherwise(struct lab_flow_location_restore_context *ctx)
{
    return lab_flow_done(lab_dom_get(ctx->plan, "restored"));
}

static int lab_flow_location_restore(int frame)
{
    int select = lab_dom_get(frame, "select"), snapshot = lab_dom_get(frame, "snapshot");
    int plan = lab_dom_get(frame, "plan");
    struct lab_flow_location_restore_context ctx = {
        .frame = frame,
        .select = select,
        .snapshot = snapshot,
        .plan = plan,
    };
    static int (*const handlers[])(struct lab_flow_location_restore_context *) = {
        [0] = lab_flow_location_restore_loc_current,
        [1] = lab_flow_location_restore_loc_populate,
        [2] = lab_flow_location_restore_native,
        [3] = lab_flow_location_restore_loc_await,
        [4] = lab_flow_location_restore_loc_current_4,
        [5] = lab_flow_location_restore_native_5,
        [6] = lab_flow_location_restore_native_6,
        [7] = lab_flow_location_restore_loc_forms,
    };
    unsigned selected = lab_flow_location_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_location_restore_otherwise(&ctx);
}

struct lab_flow_location_populate_context {
    int frame;
    int select;
};

static int lab_flow_location_populate_loc_current(struct lab_flow_location_populate_context *ctx)
{
    if (!lab_dom_truth(ctx->select))
        return lab_flow_done(lab_dom_object(4));
    return lab_flow_call(ctx->frame, 1, "locCurrent", "current", 0, 1, lab_dom_get(ctx->frame, "isCurrent"));
}

static int lab_flow_location_populate_loc_towns(struct lab_flow_location_populate_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_done(lab_dom_object(4));
    return lab_flow_call(ctx->frame, 2, "locTowns", "towns", 0, 1, lab_dom_get(ctx->frame, "jurisdiction"));
}

static int lab_flow_location_populate_native(struct lab_flow_location_populate_context *ctx)
{
    return lab_flow_call(ctx->frame, 3, "native", 0, 0, 4, lab_dom_string("lab_location_populate"), ctx->select,
                         lab_dom_get(ctx->frame, "towns"),
                         lab_flow_location_flag(lab_dom_get(ctx->frame, "selectDefault")));
}

static int lab_flow_location_populate_otherwise(struct lab_flow_location_populate_context *ctx)
{
    return lab_flow_done(lab_dom_get(ctx->frame, "towns"));
}

static int lab_flow_location_populate(int frame)
{
    int select = lab_dom_get(frame, "select");
    struct lab_flow_location_populate_context ctx = {
        .frame = frame,
        .select = select,
    };
    static int (*const handlers[])(struct lab_flow_location_populate_context *) = {
        [0] = lab_flow_location_populate_loc_current,
        [1] = lab_flow_location_populate_loc_towns,
        [2] = lab_flow_location_populate_native,
    };
    unsigned selected = lab_flow_location_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_location_populate_otherwise(&ctx);
}

static int lab_flow_location_sync(int frame)
{
    unsigned stage = lab_flow_location_stage(frame);
    if (stage == 0 || stage == 2)
        return lab_flow_call(frame, stage + 1, "locElements", "elements", 0, 1,
                             lab_dom_string(stage ? "almanac" : "datetime"));
    if (stage == 1 || stage == 3) {
        int elements = lab_dom_get(frame, "elements"), options = lab_dom_object(5);
        lab_dom_set(options, "selectDefault", lab_dom_get(frame, "selectDefault"));
        return lab_flow_call(frame, stage + 1, "locPopulate", 0, 1, 3, lab_dom_get(elements, "town"),
                             lab_dom_get(lab_dom_get(elements, "jurisdiction"), "value"), options);
    }
    return lab_flow_done(0);
}

static int lab_flow_location_history_loc_apply_state(int frame)
{
    return lab_flow_call(frame, 1, "locApplyState", "state", 0, 2, lab_dom_get(frame, "mode"),
                         lab_dom_get(frame, "source"));
}

static int lab_flow_location_history_loc_elements(int frame)
{
    return lab_flow_call(frame, 2, "locElements", "elements", 0, 1, lab_dom_get(frame, "mode"));
}

static int lab_flow_location_history_loc_restore(int frame)
{
    int elements = lab_dom_get(frame, "elements");
    return lab_flow_call(frame, 3, "locRestore", 0, 1, 6, lab_dom_get(elements, "town"),
                         lab_dom_get(lab_dom_get(elements, "jurisdiction"), "value"),
                         lab_dom_get(lab_dom_get(frame, "state"), "town"),
                         lab_dom_get(lab_dom_get(elements, "latitude"), "value"),
                         lab_dom_get(lab_dom_get(elements, "longitude"), "value"), lab_dom_get(frame, "isCurrent"));
}

static int lab_flow_location_history_otherwise(int frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static int lab_flow_location_history(int frame)
{
    static int (*const handlers[])(int) = {
        [0] = lab_flow_location_history_loc_apply_state,
        [1] = lab_flow_location_history_loc_elements,
        [2] = lab_flow_location_history_loc_restore,
    };
    unsigned selected = lab_flow_location_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](frame)
               : lab_flow_location_history_otherwise(frame);
}

enum {
    LOC_TOTAL_START,
    LOC_TOTAL_CONTEXT,
    LOC_TOTAL_PREPARE,
    LOC_TOTAL_RESTORE,
    LOC_TOTAL_CURRENT,
    LOC_TOTAL_APPLY,
    LOC_TOTAL_SAVE,
    LOC_TOTAL_EVALUATE,
    LOC_TOTAL_FINISH,
    LOC_TOTAL_DONE,
    LOC_TOTAL_ERROR_CURRENT,
    LOC_TOTAL_ERROR_STATUS,
    LOC_TOTAL_ERROR_RENDER,
    LOC_TOTAL_RETHROW_FINISH,
    LOC_TOTAL_RETHROW
};

struct lab_flow_location_totality_context {
    int frame;
    int request;
    int state;
};

static int lab_flow_location_totality_loc_total_start(struct lab_flow_location_totality_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "button")))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, LOC_TOTAL_CONTEXT, "locBegin", "request", 0, 2, lab_dom_string("almanacLocation"),
                         lab_dom_string("almanac"));
}

static int lab_flow_location_totality_loc_total_context(struct lab_flow_location_totality_context *ctx)
{
    if (!lab_dom_truth(ctx->request))
        return lab_flow_done(0);
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(LOC_TOTAL_ERROR_CURRENT));
    return lab_flow_call(ctx->frame, LOC_TOTAL_PREPARE, "locContext", "context", 0, 0);
}

static int lab_flow_location_totality_loc_total_prepare(struct lab_flow_location_totality_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_TOTAL_RESTORE, "native", "state", 0, 3,
                         lab_dom_string("lab_location_totality_prepare"), lab_dom_get(ctx->frame, "button"),
                         lab_dom_get(ctx->frame, "context"));
}

static int lab_flow_location_totality_loc_total_restore(struct lab_flow_location_totality_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_TOTAL_CURRENT, "locRestoreTotality", "restored", 1, 2, ctx->state,
                         ctx->request);
}

static int lab_flow_location_totality_loc_total_current(struct lab_flow_location_totality_context *ctx)
{
    if (!lab_dom_type(lab_dom_get(ctx->frame, "restored")))
        return lab_flow_location_jump(ctx->frame, LOC_TOTAL_FINISH);
    return lab_flow_call(ctx->frame, LOC_TOTAL_APPLY, "requestCurrent", "current", 0, 1, ctx->request);
}

static int lab_flow_location_totality_loc_total_apply(struct lab_flow_location_totality_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_location_jump(ctx->frame, LOC_TOTAL_FINISH);
    return lab_flow_call(ctx->frame, LOC_TOTAL_SAVE, "native", 0, 0, 3, lab_dom_string("lab_location_totality_finish"),
                         ctx->state, lab_flow_location_flag(lab_dom_get(ctx->frame, "restored")));
}

static int lab_flow_location_totality_loc_total_save(struct lab_flow_location_totality_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_TOTAL_EVALUATE, "locSaveAlmanac", 0, 0, 0);
}

static int lab_flow_location_totality_loc_total_evaluate(struct lab_flow_location_totality_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_TOTAL_FINISH, "locEvaluateCurrent", 0, 1, 0);
}

static int lab_flow_location_totality_loc_total_error_current(struct lab_flow_location_totality_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(LOC_TOTAL_RETHROW_FINISH));
    return lab_flow_call(ctx->frame, LOC_TOTAL_ERROR_STATUS, "requestCurrent", "current", 0, 1, ctx->request);
}

static int lab_flow_location_totality_loc_total_error_status(struct lab_flow_location_totality_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_location_jump(ctx->frame, LOC_TOTAL_FINISH);
    return lab_flow_call(ctx->frame, LOC_TOTAL_ERROR_RENDER, "locStatus", 0, 0, 1, lab_dom_string("Error"));
}

static int lab_flow_location_totality_loc_total_error_render(struct lab_flow_location_totality_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_TOTAL_FINISH, "locError", 0, 0, 1,
                         lab_dom_get(lab_dom_get(ctx->frame, "exception"), "message"));
}

static int lab_flow_location_totality_loc_total_finish(struct lab_flow_location_totality_context *ctx)
{
    lab_dom_delete(ctx->frame, "failed");
    return lab_flow_call(ctx->frame, LOC_TOTAL_DONE, "locFinish", 0, 0, 1, ctx->request);
}

static int lab_flow_location_totality_loc_total_rethrow_finish(struct lab_flow_location_totality_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_TOTAL_RETHROW, "locFinish", 0, 0, 1, ctx->request);
}

static int lab_flow_location_totality_loc_total_rethrow(struct lab_flow_location_totality_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_TOTAL_DONE, "rethrow", 0, 0, 1, lab_dom_get(ctx->frame, "exception"));
}

static int lab_flow_location_totality_otherwise(struct lab_flow_location_totality_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static int lab_flow_location_totality(int frame)
{
    int request = lab_dom_get(frame, "request"), state = lab_dom_get(frame, "state");
    struct lab_flow_location_totality_context ctx = {
        .frame = frame,
        .request = request,
        .state = state,
    };
    static int (*const handlers[])(struct lab_flow_location_totality_context *) = {
        [LOC_TOTAL_START]          = lab_flow_location_totality_loc_total_start,
        [LOC_TOTAL_CONTEXT]        = lab_flow_location_totality_loc_total_context,
        [LOC_TOTAL_PREPARE]        = lab_flow_location_totality_loc_total_prepare,
        [LOC_TOTAL_RESTORE]        = lab_flow_location_totality_loc_total_restore,
        [LOC_TOTAL_CURRENT]        = lab_flow_location_totality_loc_total_current,
        [LOC_TOTAL_APPLY]          = lab_flow_location_totality_loc_total_apply,
        [LOC_TOTAL_SAVE]           = lab_flow_location_totality_loc_total_save,
        [LOC_TOTAL_EVALUATE]       = lab_flow_location_totality_loc_total_evaluate,
        [LOC_TOTAL_ERROR_CURRENT]  = lab_flow_location_totality_loc_total_error_current,
        [LOC_TOTAL_ERROR_STATUS]   = lab_flow_location_totality_loc_total_error_status,
        [LOC_TOTAL_ERROR_RENDER]   = lab_flow_location_totality_loc_total_error_render,
        [LOC_TOTAL_FINISH]         = lab_flow_location_totality_loc_total_finish,
        [LOC_TOTAL_RETHROW_FINISH] = lab_flow_location_totality_loc_total_rethrow_finish,
        [LOC_TOTAL_RETHROW]        = lab_flow_location_totality_loc_total_rethrow,
    };
    unsigned selected = lab_flow_location_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_location_totality_otherwise(&ctx);
}

enum {
    LOC_REFRESH_ELEMENTS,
    LOC_REFRESH_CONTROLS,
    LOC_REFRESH_BEGIN,
    LOC_REFRESH_READ,
    LOC_REFRESH_STATE,
    LOC_REFRESH_POPULATE,
    LOC_REFRESH_CURRENT,
    LOC_REFRESH_COORDINATES,
    LOC_REFRESH_DEFAULT,
    LOC_REFRESH_SECOND_CURRENT,
    LOC_REFRESH_APPLY,
    LOC_REFRESH_STATUS,
    LOC_REFRESH_POST,
    LOC_REFRESH_RESPONSE_CURRENT,
    LOC_REFRESH_RESPONSE_CONTEXT,
    LOC_REFRESH_RESPONSE,
    LOC_REFRESH_ACCEPT,
    LOC_REFRESH_FINISH,
    LOC_REFRESH_DONE
};

struct lab_flow_location_refresh_context {
    int frame;
    int mode;
    int elements;
    int request;
    int state;
    int update;
};

static int lab_flow_location_refresh_loc_refresh_second_current(struct lab_flow_location_refresh_context *ctx);
static int lab_flow_location_refresh_loc_refresh_post(struct lab_flow_location_refresh_context *ctx);

static int lab_flow_location_refresh_loc_refresh_elements(struct lab_flow_location_refresh_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_REFRESH_CONTROLS, "locElements", "elements", 0, 1, ctx->mode);
}

static int lab_flow_location_refresh_loc_refresh_controls(struct lab_flow_location_refresh_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_REFRESH_BEGIN, "locControls", "controls", 0, 2, ctx->mode,
                         lab_dom_scalar(1, ctx->update));
}

static int lab_flow_location_refresh_loc_refresh_begin(struct lab_flow_location_refresh_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->elements, "jurisdiction")))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, LOC_REFRESH_READ, "locBegin", "request", 0, 2,
                         lab_dom_join(ctx->mode, lab_dom_string("Location"), ""), ctx->mode);
}

static int lab_flow_location_refresh_loc_refresh_read(struct lab_flow_location_refresh_context *ctx)
{
    if (!lab_dom_truth(ctx->request))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, LOC_REFRESH_STATE, "locRead", "read", 0, 1, ctx->mode);
}

static int lab_flow_location_refresh_loc_refresh_state(struct lab_flow_location_refresh_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_REFRESH_POPULATE, "locState", "state", 0, 3, ctx->mode,
                         lab_dom_get(ctx->frame, "read"), lab_dom_numeric(1));
}

static int lab_flow_location_refresh_loc_refresh_populate(struct lab_flow_location_refresh_context *ctx)
{
    lab_dom_set(ctx->frame, "jurisdiction", lab_dom_get(ctx->state, "jurisdiction"));
    lab_dom_set(ctx->frame, "date", lab_dom_get(ctx->state, "date"));
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(LOC_REFRESH_FINISH));
    if (ctx->update) {
        int options = lab_dom_object(5);
        lab_dom_set(options, "selectDefault", lab_dom_scalar(1, 0));
        return lab_flow_call(ctx->frame, LOC_REFRESH_CURRENT, "locPopulate", 0, 1, 3,
                             lab_dom_get(ctx->elements, "town"), lab_dom_get(ctx->frame, "jurisdiction"), options);
    }
    return lab_flow_call(ctx->frame, LOC_REFRESH_STATUS, "locApplyTown", 0, 0, 1, lab_dom_get(ctx->frame, "controls"));
}

static int lab_flow_location_refresh_loc_refresh_current(struct lab_flow_location_refresh_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_REFRESH_COORDINATES, "requestCurrent", "current", 0, 1, ctx->request);
}

static int lab_flow_location_refresh_loc_refresh_coordinates(struct lab_flow_location_refresh_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_location_jump(ctx->frame, LOC_REFRESH_FINISH);
    return lab_flow_call(ctx->frame, LOC_REFRESH_DEFAULT, "locCoordinates", "matched", 0, 3,
                         lab_dom_get(ctx->elements, "town"),
                         lab_dom_get(lab_dom_get(ctx->elements, "latitude"), "value"),
                         lab_dom_get(lab_dom_get(ctx->elements, "longitude"), "value"));
}

static int lab_flow_location_refresh_loc_refresh_default(struct lab_flow_location_refresh_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "matched"))) {
        int options = lab_dom_object(5);
        lab_dom_set(options, "selectDefault", lab_dom_scalar(1, 1));
        return lab_flow_call(ctx->frame, LOC_REFRESH_SECOND_CURRENT, "locPopulate", 0, 1, 3,
                             lab_dom_get(ctx->elements, "town"), lab_dom_get(ctx->frame, "jurisdiction"), options);
    }
    /* Continue: preserve the second freshness check even without default population. */
    return lab_flow_location_refresh_loc_refresh_second_current(ctx);
}

static int lab_flow_location_refresh_loc_refresh_second_current(struct lab_flow_location_refresh_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_REFRESH_APPLY, "requestCurrent", "current", 0, 1, ctx->request);
}

static int lab_flow_location_refresh_loc_refresh_apply(struct lab_flow_location_refresh_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_location_jump(ctx->frame, LOC_REFRESH_FINISH);
    return lab_flow_call(ctx->frame, LOC_REFRESH_STATUS, "locApplyTown", "townApplied", 0, 1,
                         lab_dom_get(ctx->frame, "controls"));
}

static int lab_flow_location_refresh_loc_refresh_status(struct lab_flow_location_refresh_context *ctx)
{
    if (lab_dom_equal(ctx->mode, lab_dom_string("almanac")))
        return lab_flow_call(ctx->frame, LOC_REFRESH_POST, "locStatus", 0, 0, 1,
                             lab_dom_string("Updating almanac location..."));
    /* Continue: DateTime refresh does not alter status here. */
    return lab_flow_location_refresh_loc_refresh_post(ctx);
}

static int lab_flow_location_refresh_loc_refresh_post(struct lab_flow_location_refresh_context *ctx)
{
    int payload = lab_dom_object(5);
    lab_dom_set(payload, "jurisdiction", lab_dom_get(ctx->frame, "jurisdiction"));
    lab_dom_set(payload, "date", lab_dom_get(ctx->frame, "date"));
    return lab_flow_call(ctx->frame, LOC_REFRESH_RESPONSE_CURRENT, "post", "result", 1, 2, ctx->request, payload);
}

static int lab_flow_location_refresh_loc_refresh_response_current(struct lab_flow_location_refresh_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_REFRESH_RESPONSE_CONTEXT, "requestCurrent", "current", 0, 1, ctx->request);
}

static int lab_flow_location_refresh_loc_refresh_response_context(struct lab_flow_location_refresh_context *ctx)
{
    int result = lab_dom_get(ctx->frame, "result");
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")) ||
        !lab_dom_truth(lab_dom_get(lab_dom_get(result, "response"), "ok")) ||
        !lab_dom_truth(lab_dom_get(lab_dom_get(result, "data"), "ok")))
        return lab_flow_location_jump(ctx->frame, LOC_REFRESH_FINISH);
    return lab_flow_call(ctx->frame, LOC_REFRESH_RESPONSE, "locContext", "context", 0, 0);
}

static int lab_flow_location_refresh_loc_refresh_response(struct lab_flow_location_refresh_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_REFRESH_ACCEPT, "locResponse", 0, 0, 5, ctx->mode,
                         lab_dom_get(lab_dom_get(ctx->frame, "result"), "data"), lab_dom_get(ctx->frame, "context"),
                         lab_dom_scalar(1, ctx->update), lab_dom_get(ctx->frame, "townApplied"));
}

static int lab_flow_location_refresh_loc_refresh_accept(struct lab_flow_location_refresh_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_REFRESH_FINISH, "locAcceptContext", 0, 0, 1,
                         lab_dom_get(ctx->frame, "context"));
}

static int lab_flow_location_refresh_loc_refresh_finish(struct lab_flow_location_refresh_context *ctx)
{
    lab_dom_delete(ctx->frame, "failed");
    return lab_flow_call(ctx->frame, LOC_REFRESH_DONE, "locFinish", 0, 0, 1, ctx->request);
}

static int lab_flow_location_refresh_otherwise(struct lab_flow_location_refresh_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static int lab_flow_location_refresh(int frame)
{
    int mode = lab_dom_get(frame, "mode"), elements = lab_dom_get(frame, "elements");
    int request = lab_dom_get(frame, "request"), state = lab_dom_get(frame, "state");
    int update = lab_dom_truth(lab_dom_get(frame, "updateCoordinates"));
    struct lab_flow_location_refresh_context ctx = {
        .frame = frame,
        .mode = mode,
        .elements = elements,
        .request = request,
        .state = state,
        .update = update,
    };
    static int (*const handlers[])(struct lab_flow_location_refresh_context *) = {
        [LOC_REFRESH_ELEMENTS]         = lab_flow_location_refresh_loc_refresh_elements,
        [LOC_REFRESH_CONTROLS]         = lab_flow_location_refresh_loc_refresh_controls,
        [LOC_REFRESH_BEGIN]            = lab_flow_location_refresh_loc_refresh_begin,
        [LOC_REFRESH_READ]             = lab_flow_location_refresh_loc_refresh_read,
        [LOC_REFRESH_STATE]            = lab_flow_location_refresh_loc_refresh_state,
        [LOC_REFRESH_POPULATE]         = lab_flow_location_refresh_loc_refresh_populate,
        [LOC_REFRESH_CURRENT]          = lab_flow_location_refresh_loc_refresh_current,
        [LOC_REFRESH_COORDINATES]      = lab_flow_location_refresh_loc_refresh_coordinates,
        [LOC_REFRESH_DEFAULT]          = lab_flow_location_refresh_loc_refresh_default,
        [LOC_REFRESH_SECOND_CURRENT]   = lab_flow_location_refresh_loc_refresh_second_current,
        [LOC_REFRESH_APPLY]            = lab_flow_location_refresh_loc_refresh_apply,
        [LOC_REFRESH_STATUS]           = lab_flow_location_refresh_loc_refresh_status,
        [LOC_REFRESH_POST]             = lab_flow_location_refresh_loc_refresh_post,
        [LOC_REFRESH_RESPONSE_CURRENT] = lab_flow_location_refresh_loc_refresh_response_current,
        [LOC_REFRESH_RESPONSE_CONTEXT] = lab_flow_location_refresh_loc_refresh_response_context,
        [LOC_REFRESH_RESPONSE]         = lab_flow_location_refresh_loc_refresh_response,
        [LOC_REFRESH_ACCEPT]           = lab_flow_location_refresh_loc_refresh_accept,
        [LOC_REFRESH_FINISH]           = lab_flow_location_refresh_loc_refresh_finish,
    };
    unsigned selected = lab_flow_location_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_location_refresh_otherwise(&ctx);
}

struct lab_flow_location_auto_context {
    int frame;
    int mode;
    int options;
    int almanac;
};

static int lab_flow_location_auto_refresh(struct lab_flow_location_auto_context *ctx);
static int lab_flow_location_auto_save(struct lab_flow_location_auto_context *ctx);

static int lab_flow_location_auto_current_mode(struct lab_flow_location_auto_context *ctx)
{
    return lab_flow_call(ctx->frame, 1, "currentMode", "currentMode", 0, 0);
}

static int lab_flow_location_auto_loc_clear_almanac(struct lab_flow_location_auto_context *ctx)
{
    if (!lab_dom_equal(ctx->mode, lab_dom_get(ctx->frame, "currentMode")))
        return lab_flow_done(0);
    if (ctx->almanac)
        return lab_flow_call(ctx->frame, 2, "locClearAlmanac", 0, 0, 0);
    /* Continue: only Almanac invalidates its cached worksheet. */
    return lab_flow_location_auto_refresh(ctx);
}

static int lab_flow_location_auto_refresh(struct lab_flow_location_auto_context *ctx)
{
    if (lab_dom_truth(lab_dom_get(ctx->options, "refreshJurisdiction"))) {
        int refresh = lab_dom_object(5);
        lab_dom_set(refresh, "updateCoordinates", lab_dom_get(ctx->options, "refreshCoordinates"));
        return lab_flow_call(ctx->frame, 3, ctx->almanac ? "locRefreshAlmanac" : "locRefreshDatetime", 0, 1, 1,
                             refresh);
    }
    /* Continue: persistence follows the optional location refresh. */
    return lab_flow_location_auto_save(ctx);
}

static int lab_flow_location_auto_save(struct lab_flow_location_auto_context *ctx)
{
    return lab_flow_call(ctx->frame, 4, ctx->almanac ? "locSaveAlmanac" : "locSaveDatetime", 0, 0, 0);
}

static int lab_flow_location_auto_evaluate(struct lab_flow_location_auto_context *ctx)
{
    int evaluate = lab_dom_object(5);
    lab_dom_set(evaluate, "skipHistoryUpdate", lab_dom_scalar(1, 1));
    return lab_flow_call(ctx->frame, 5, ctx->almanac ? "locEvaluateAlmanac" : "locEvaluateDatetime", 0, 1, 1, evaluate);
}

static int lab_flow_location_auto_update_history_buttons(struct lab_flow_location_auto_context *ctx)
{
    return lab_flow_call(ctx->frame, 6, "updateHistoryButtons", 0, 0, 0);
}

static int lab_flow_location_auto_otherwise(struct lab_flow_location_auto_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static int lab_flow_location_auto(int frame)
{
    int mode = lab_dom_get(frame, "mode"), options = lab_dom_get(frame, "options");
    int almanac = lab_dom_equal(mode, lab_dom_string("almanac"));
    struct lab_flow_location_auto_context ctx = {
        .frame = frame,
        .mode = mode,
        .options = options,
        .almanac = almanac,
    };
    static int (*const handlers[])(struct lab_flow_location_auto_context *) = {
        [0] = lab_flow_location_auto_current_mode,
        [1] = lab_flow_location_auto_loc_clear_almanac,
        [2] = lab_flow_location_auto_refresh,
        [3] = lab_flow_location_auto_save,
        [4] = lab_flow_location_auto_evaluate,
        [5] = lab_flow_location_auto_update_history_buttons,
    };
    unsigned selected = lab_flow_location_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_flow_location_auto_otherwise(&ctx);
}

struct lab_flow_location_time_context {
    int frame;
    int snapshot;
};

static int lab_flow_location_time_request_context(struct lab_flow_location_time_context *ctx);

static int lab_flow_location_time_loc_time_snapshot(struct lab_flow_location_time_context *ctx)
{
    return lab_flow_call(ctx->frame, 1, "locTimeSnapshot", "snapshot", 0, 0);
}

static int lab_flow_location_time_loc_format_time(struct lab_flow_location_time_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(7));
    return lab_flow_call(ctx->frame, 2, "locFormatTime", "formatted", 1, 1, lab_dom_get(ctx->snapshot, "authored"));
}

static int lab_flow_location_time_caught(struct lab_flow_location_time_context *ctx)
{
    lab_dom_set(ctx->frame, "caught", lab_dom_scalar(1, 1));
    /* Continue: recovery checks the same request and authored-value ownership. */
    return lab_flow_location_time_request_context(ctx);
}

static int lab_flow_location_time_request_context(struct lab_flow_location_time_context *ctx)
{
    return lab_flow_call(ctx->frame, 3, "requestContext", "context", 0, 0);
}

static int lab_flow_location_time_loc_latest_main(struct lab_flow_location_time_context *ctx)
{
    if (!lab_dom_equal(lab_dom_get(ctx->snapshot, "context"), lab_dom_get(ctx->frame, "context")))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, 4, "locLatestMain", "main", 0, 0);
}

static int lab_flow_location_time_loc_status(struct lab_flow_location_time_context *ctx)
{
    if (!lab_dom_equal(lab_dom_get(ctx->snapshot, "main"), lab_dom_get(ctx->frame, "main")) ||
        !lab_dom_equal(lab_dom_get(ctx->snapshot, "authored"),
                       lab_dom_get(lab_dom_get(ctx->snapshot, "element"), "value")))
        return lab_flow_done(0);
    if (lab_dom_truth(lab_dom_get(ctx->frame, "caught")))
        return lab_flow_call(ctx->frame, 6, "locStatus", 0, 0, 1, lab_dom_get(ctx->frame, "error"));
    if (lab_dom_equal(lab_dom_get(ctx->frame, "formatted"), lab_dom_get(ctx->snapshot, "authored")))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, 6, "locTimeApply", 0, 0, 2, lab_dom_get(ctx->snapshot, "element"),
                         lab_dom_get(ctx->frame, "formatted"));
}

static int lab_flow_location_time_otherwise(struct lab_flow_location_time_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static int lab_flow_location_time(int frame)
{
    int snapshot = lab_dom_get(frame, "snapshot");
    struct lab_flow_location_time_context ctx = {
        .frame = frame,
        .snapshot = snapshot,
    };
    static int (*const handlers[])(struct lab_flow_location_time_context *) = {
        [0] = lab_flow_location_time_loc_time_snapshot,
        [1] = lab_flow_location_time_loc_format_time,
        [7] = lab_flow_location_time_caught,
        [2] = lab_flow_location_time_request_context,
        [3] = lab_flow_location_time_loc_latest_main,
        [4] = lab_flow_location_time_loc_status,
    };
    unsigned selected = lab_flow_location_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_flow_location_time_otherwise(&ctx);
}

struct lab_flow_location_control_context {
    int frame;
    int mode;
    unsigned int actions;
};

static int lab_flow_location_control_current_mode(struct lab_flow_location_control_context *ctx);

static int lab_flow_location_control_loc_elements(struct lab_flow_location_control_context *ctx)
{
    return lab_flow_call(ctx->frame, 1, "locElements", "elements", 0, 1, ctx->mode);
}

static int lab_flow_location_control_loc_controls(struct lab_flow_location_control_context *ctx)
{
    if (ctx->actions & 4)
        return lab_flow_call(ctx->frame, 2, "locControls", "controls", 0, 1, ctx->mode);
    return lab_flow_location_jump(ctx->frame, 3);
}

static int lab_flow_location_control_loc_apply_town(struct lab_flow_location_control_context *ctx)
{
    return lab_flow_call(ctx->frame, 3, "locApplyTown", 0, 0, 1, lab_dom_get(ctx->frame, "controls"));
}

static int lab_flow_location_control_loc_clear_custom(struct lab_flow_location_control_context *ctx)
{
    if (ctx->actions & 8) {
        int elements = lab_dom_get(ctx->frame, "elements");
        return lab_flow_call(ctx->frame, 4, "locClearCustom", 0, 0, 4, lab_dom_get(elements, "town"),
                             lab_dom_get(elements, "latitude"), lab_dom_get(elements, "longitude"),
                             lab_dom_get(elements, "elevation"));
    }
    /* Continue: inactive-mode events may project controls but never auto-evaluate. */
    return lab_flow_location_control_current_mode(ctx);
}

static int lab_flow_location_control_current_mode(struct lab_flow_location_control_context *ctx)
{
    return lab_flow_call(ctx->frame, 5, "currentMode", "currentMode", 0, 0);
}

static int lab_flow_location_control_complete(struct lab_flow_location_control_context *ctx)
{
    if (!lab_dom_equal(ctx->mode, lab_dom_get(ctx->frame, "currentMode")))
        return lab_flow_done(0);
    int options = lab_dom_object(5);
    lab_dom_set(options, "refreshJurisdiction", lab_dom_scalar(1, (ctx->actions & 16) != 0));
    lab_dom_set(options, "refreshCoordinates", lab_dom_scalar(1, (ctx->actions & 32) != 0));
    return lab_flow_call(ctx->frame, 6,
                         lab_dom_equal(ctx->mode, lab_dom_string("datetime")) ? "locTriggerDatetime"
                                                                              : "locTriggerAlmanac",
                         0, 0, 1, options);
}

static int lab_flow_location_control_otherwise(struct lab_flow_location_control_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static int lab_flow_location_control(int frame)
{
    int mode = lab_dom_get(frame, "mode");
    unsigned actions = (unsigned)lab_dom_number(lab_dom_get(frame, "actions"));
    struct lab_flow_location_control_context ctx = {
        .frame = frame,
        .mode = mode,
        .actions = actions,
    };
    static int (*const handlers[])(struct lab_flow_location_control_context *) = {
        [0] = lab_flow_location_control_loc_elements,
        [1] = lab_flow_location_control_loc_controls,
        [2] = lab_flow_location_control_loc_apply_town,
        [3] = lab_flow_location_control_loc_clear_custom,
        [4] = lab_flow_location_control_current_mode,
        [5] = lab_flow_location_control_complete,
    };
    unsigned selected = lab_flow_location_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_location_control_otherwise(&ctx);
}

/* Keep these synchronous effects separate from asynchronous request continuations. */
void lab_flow_location_local(int text, int sections, int body, int card)
{
    int frame = lab_dom_object(5), plan = lab_dom_object(5), calls = lab_dom_object(4);
    text = lab_dom_get(text, "value");
    text = lab_dom_clean(lab_dom_text(lab_dom_truth(text) ? text : lab_dom_string("")), 0);
    if (lab_dom_truth(body)) {
        int render = lab_flow_call(frame, 1, "locRenderLocal", 0, 0, 3, body, sections, text);
        lab_dom_push(calls, lab_dom_item(lab_dom_get(render, "calls"), 0));
    }
    if (lab_dom_truth(card)) {
        int visibility =
            lab_flow_call(frame, 1, lab_dom_truth(text) ? "locLocalVisible" : "locLocalHidden", 0, 0, 1, card);
        lab_dom_push(calls, lab_dom_item(lab_dom_get(visibility, "calls"), 0));
    }
    lab_dom_set(plan, "calls", calls);
    lab_dom_return(plan);
}

/* Read current mode only after rendering, leaving empty-card hiding independent of mode. */
void lab_flow_location_visibility(int card, int mode)
{
    lab_dom_class(card, "hidden", !lab_dom_equal(mode, lab_dom_string("datetime")));
}

/* Callable-method detection belongs to the browser; preference and fallbacks are native policy. */
void lab_flow_location_select(int select, int capabilities)
{
    int frame = lab_dom_object(5), plan = lab_dom_object(5);
    if (lab_dom_truth(select)) {
        if (lab_dom_truth(lab_dom_get(capabilities, "rebuild")))
            plan = lab_flow_call(frame, 1, "locSelectRebuild", 0, 0, 1, select);
        else if (lab_dom_truth(lab_dom_get(capabilities, "sync")))
            plan = lab_flow_call(frame, 1, "locSelectSync", 0, 0, 1, select);
    }
    lab_dom_return(plan);
}

/* Keep unchanged moves inert without making the host interpret movement policy. */
void lab_flow_location_picker(unsigned changed)
{
    int frame = lab_dom_object(5), plan = lab_dom_object(5);
    if (changed)
        plan = lab_flow_call(frame, 1, "locRenderPicker", 0, 0, 0);
    lab_dom_return(plan);
}

/* Dispatch a fixed group of independent workflows, with all persistent values in the browser frame. */
int lab_flow_location(unsigned kind, int frame, int view)
{
    (void)view;
    static int (*const handlers[])(int) = {[0] = lab_flow_location_forms,    [1] = lab_flow_location_restore,
                                           [2] = lab_flow_location_populate, [3] = lab_flow_location_sync,
                                           [4] = lab_flow_location_history,  [5] = lab_flow_location_totality,
                                           [6] = lab_flow_location_refresh,  [7] = lab_flow_location_auto,
                                           [8] = lab_flow_location_time,     [9] = lab_flow_location_control};
    return kind < sizeof handlers / sizeof *handlers ? handlers[kind](frame) : lab_flow_done(0);
}
