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

static unsigned lab_flow_location_stage(frame_handle frame)
{
    return (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
}

static browser_handle lab_flow_location_null(void)
{
    int plan = lab_flow_done(0);
    lab_dom_set(plan, "value", 0);
    return plan;
}

static browser_handle lab_flow_location_flag(browser_handle value)
{
    return lab_dom_numeric(lab_dom_truth(value));
}

static browser_handle lab_flow_location_jump(frame_handle frame, unsigned next)
{
    return lab_flow_effects(frame, next, lab_dom_object(LAB_VALUE_OBJECT));
}

static browser_handle lab_flow_location_forms(frame_handle frame)
{
    lab_flow_location_form_action_t action =
        (lab_flow_location_form_action_t)(unsigned)lab_dom_number(lab_dom_get(frame, "action"));
    if (!lab_flow_location_stage(frame)) {
        int option = lab_dom_get(frame, "option"), value = lab_dom_get(frame, "value");
        if (action == LAB_FLOW_LOCATION_MATCH_TOWN && (!lab_dom_truth(option) || !lab_dom_truth(value)))
            return lab_flow_done(lab_dom_scalar(LAB_VALUE_BOOLEAN, 0));
        int payload = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(payload, "action", lab_dom_string(action != LAB_FLOW_LOCATION_FORMAT_TIME ? "town" : "time"));
        lab_dom_set(payload, action != LAB_FLOW_LOCATION_FORMAT_TIME ? "value" : "text",
                    lab_dom_text(lab_dom_truth(value) ? value : lab_dom_string("")));
        if (action == LAB_FLOW_LOCATION_MATCH_TOWN) {
            int candidates = lab_dom_object(LAB_VALUE_ARRAY);
            lab_dom_push(candidates, lab_dom_get(option, "value"));
            lab_dom_set(payload, "candidates", candidates);
        }
        return lab_flow_call(frame, 1, "locForms", "response", 1, 1, payload);
    }
    int response = lab_dom_get(frame, "response");
    if (action == LAB_FLOW_LOCATION_MATCH_TOWN) {
        int index = lab_dom_get(response, "match_index");
        return lab_flow_done(lab_dom_scalar(LAB_VALUE_BOOLEAN,
                                            lab_dom_type(index) == LAB_VALUE_NUMBER && lab_dom_number(index) == 0));
    }
    return lab_flow_done(lab_dom_get(response, action != LAB_FLOW_LOCATION_FORMAT_TIME ? "town" : "text"));
}

struct lab_flow_location_restore_context {
    frame_handle frame;
    node_handle select;
    snapshot_handle snapshot;
    browser_handle plan;
};

/**
 * @brief Complete location restoration with its recorded outcome.
 * @param ctx Restore state whose plan contains the restored result returned to the caller.
 * @return Scoped completion plan carrying the plan's restored value.
 */
static browser_handle lab_flow_location_restore_otherwise(struct lab_flow_location_restore_context *ctx);

static browser_handle lab_flow_location_restore_loc_current(struct lab_flow_location_restore_context *ctx)
{
    return lab_flow_call(ctx->frame, 1, "locCurrent", "current", 0, 1, lab_dom_get(ctx->frame, "isCurrent"));
}

static browser_handle lab_flow_location_restore_loc_populate(struct lab_flow_location_restore_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_location_null();
    if (!lab_dom_truth(ctx->select))
        return lab_flow_done(lab_dom_scalar(LAB_VALUE_BOOLEAN, 0));
    int options = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(options, "selectDefault", lab_dom_scalar(LAB_VALUE_BOOLEAN, 0));
    lab_dom_set(options, "isCurrent", lab_dom_get(ctx->frame, "isCurrent"));
    return lab_flow_call(ctx->frame, 2, "locPopulate", "preparation", 0, 3, ctx->select,
                         lab_dom_get(ctx->frame, "jurisdiction"), options);
}

static browser_handle lab_flow_location_restore_native(struct lab_flow_location_restore_context *ctx)
{
    return lab_flow_call(ctx->frame, 3, "native", "snapshot", 0, 2, lab_dom_string("lab_location_snapshot"),
                         ctx->select);
}

static browser_handle lab_flow_location_restore_loc_await(struct lab_flow_location_restore_context *ctx)
{
    return lab_flow_call(ctx->frame, 4, "locAwait", 0, 1, 1, lab_dom_get(ctx->frame, "preparation"));
}

static browser_handle lab_flow_location_restore_loc_current_4(struct lab_flow_location_restore_context *ctx)
{
    return lab_flow_call(ctx->frame, 5, "locCurrent", "current", 0, 1, lab_dom_get(ctx->frame, "isCurrent"));
}

static browser_handle lab_flow_location_restore_native_5(struct lab_flow_location_restore_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_location_null();
    return lab_flow_call(ctx->frame, 6, "native", "unchanged", 0, 3, lab_dom_string("lab_location_unchanged"),
                         ctx->select, ctx->snapshot);
}

static browser_handle lab_flow_location_restore_native_6(struct lab_flow_location_restore_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "unchanged")))
        return lab_flow_location_null();
    return lab_flow_call(ctx->frame, 7, "native", "plan", 0, 6, lab_dom_string("lab_location_restore"),
                         lab_dom_numeric(lab_dom_truth(lab_dom_get(ctx->frame, "responded"))), ctx->select,
                         ctx->snapshot, lab_dom_get(ctx->frame, "selection"), lab_dom_get(ctx->frame, "response"));
}

static browser_handle lab_flow_location_restore_loc_forms(struct lab_flow_location_restore_context *ctx)
{
    if (lab_dom_truth(lab_dom_get(ctx->plan, "request"))) {
        lab_dom_set(ctx->frame, "responded", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
        return lab_flow_call(ctx->frame, 4, "locForms", "response", 1, 1, lab_dom_get(ctx->plan, "request"));
    }
    if (lab_dom_truth(lab_dom_get(ctx->plan, "controls")))
        return lab_flow_call(ctx->frame, 8, "locApplyTown", 0, 0, 1, lab_dom_get(ctx->plan, "controls"));
    /* Continue: coordinate fallback does not apply named-town controls. */
    return lab_flow_location_restore_otherwise(ctx);
}

static browser_handle lab_flow_location_restore_otherwise(struct lab_flow_location_restore_context *ctx)
{
    return lab_flow_done(lab_dom_get(ctx->plan, "restored"));
}

static browser_handle lab_flow_location_restore(frame_handle frame)
{
    int select = lab_dom_get(frame, "select"), snapshot = lab_dom_get(frame, "snapshot");
    int plan = lab_dom_get(frame, "plan");
    struct lab_flow_location_restore_context ctx = {
        .frame = frame,
        .select = select,
        .snapshot = snapshot,
        .plan = plan,
    };
    static browser_handle (*const handlers[])(struct lab_flow_location_restore_context *) = {
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
    frame_handle frame;
    node_handle select;
};

static browser_handle lab_flow_location_populate_loc_current(struct lab_flow_location_populate_context *ctx)
{
    if (!lab_dom_truth(ctx->select))
        return lab_flow_done(lab_dom_object(LAB_VALUE_ARRAY));
    return lab_flow_call(ctx->frame, 1, "locCurrent", "current", 0, 1, lab_dom_get(ctx->frame, "isCurrent"));
}

static browser_handle lab_flow_location_populate_loc_towns(struct lab_flow_location_populate_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_done(lab_dom_object(LAB_VALUE_ARRAY));
    return lab_flow_call(ctx->frame, 2, "locTowns", "towns", 0, 1, lab_dom_get(ctx->frame, "jurisdiction"));
}

static browser_handle lab_flow_location_populate_native(struct lab_flow_location_populate_context *ctx)
{
    return lab_flow_call(ctx->frame, 3, "native", 0, 0, 4, lab_dom_string("lab_location_populate"), ctx->select,
                         lab_dom_get(ctx->frame, "towns"),
                         lab_flow_location_flag(lab_dom_get(ctx->frame, "selectDefault")));
}

static browser_handle lab_flow_location_populate_otherwise(struct lab_flow_location_populate_context *ctx)
{
    return lab_flow_done(lab_dom_get(ctx->frame, "towns"));
}

static browser_handle lab_flow_location_populate(frame_handle frame)
{
    int select = lab_dom_get(frame, "select");
    struct lab_flow_location_populate_context ctx = {
        .frame = frame,
        .select = select,
    };
    static browser_handle (*const handlers[])(struct lab_flow_location_populate_context *) = {
        [0] = lab_flow_location_populate_loc_current,
        [1] = lab_flow_location_populate_loc_towns,
        [2] = lab_flow_location_populate_native,
    };
    unsigned selected = lab_flow_location_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_location_populate_otherwise(&ctx);
}

static browser_handle lab_flow_location_sync(frame_handle frame)
{
    unsigned stage = lab_flow_location_stage(frame);
    if (stage == 0 || stage == 2)
        return lab_flow_call(frame, stage + 1, "locElements", "elements", 0, 1,
                             lab_dom_string(stage ? "almanac" : "datetime"));
    if (stage == 1 || stage == 3) {
        int elements = lab_dom_get(frame, "elements"), options = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(options, "selectDefault", lab_dom_get(frame, "selectDefault"));
        return lab_flow_call(frame, stage + 1, "locPopulate", 0, 1, 3, lab_dom_get(elements, "town"),
                             lab_dom_get(lab_dom_get(elements, "jurisdiction"), "value"), options);
    }
    return lab_flow_done(0);
}

static browser_handle lab_flow_location_history_loc_apply_state(frame_handle frame)
{
    return lab_flow_call(frame, 1, "locApplyState", "state", 0, 2, lab_dom_get(frame, "mode"),
                         lab_dom_get(frame, "source"));
}

static browser_handle lab_flow_location_history_loc_elements(frame_handle frame)
{
    return lab_flow_call(frame, 2, "locElements", "elements", 0, 1, lab_dom_get(frame, "mode"));
}

static browser_handle lab_flow_location_history_loc_restore(frame_handle frame)
{
    int elements = lab_dom_get(frame, "elements");
    return lab_flow_call(frame, 3, "locRestore", 0, 1, 6, lab_dom_get(elements, "town"),
                         lab_dom_get(lab_dom_get(elements, "jurisdiction"), "value"),
                         lab_dom_get(lab_dom_get(frame, "state"), "town"),
                         lab_dom_get(lab_dom_get(elements, "latitude"), "value"),
                         lab_dom_get(lab_dom_get(elements, "longitude"), "value"), lab_dom_get(frame, "isCurrent"));
}

static browser_handle lab_flow_location_history_otherwise(frame_handle frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static browser_handle lab_flow_location_history(frame_handle frame)
{
    static browser_handle (*const handlers[])(browser_handle) = {
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
    frame_handle frame;
    browser_handle request;
    browser_handle state;
};

static browser_handle lab_flow_location_totality_loc_total_start(struct lab_flow_location_totality_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "button")))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, LOC_TOTAL_CONTEXT, "locBegin", "request", 0, 2, lab_dom_string("almanacLocation"),
                         lab_dom_string("almanac"));
}

static browser_handle lab_flow_location_totality_loc_total_context(struct lab_flow_location_totality_context *ctx)
{
    if (!lab_dom_truth(ctx->request))
        return lab_flow_done(0);
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(LOC_TOTAL_ERROR_CURRENT));
    return lab_flow_call(ctx->frame, LOC_TOTAL_PREPARE, "locContext", "context", 0, 0);
}

static browser_handle lab_flow_location_totality_loc_total_prepare(struct lab_flow_location_totality_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_TOTAL_RESTORE, "native", "state", 0, 3,
                         lab_dom_string("lab_location_totality_prepare"), lab_dom_get(ctx->frame, "button"),
                         lab_dom_get(ctx->frame, "context"));
}

static browser_handle lab_flow_location_totality_loc_total_restore(struct lab_flow_location_totality_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_TOTAL_CURRENT, "locRestoreTotality", "restored", 1, 2, ctx->state,
                         ctx->request);
}

static browser_handle lab_flow_location_totality_loc_total_current(struct lab_flow_location_totality_context *ctx)
{
    if (!lab_dom_type(lab_dom_get(ctx->frame, "restored")))
        return lab_flow_location_jump(ctx->frame, LOC_TOTAL_FINISH);
    return lab_flow_call(ctx->frame, LOC_TOTAL_APPLY, "requestCurrent", "current", 0, 1, ctx->request);
}

static browser_handle lab_flow_location_totality_loc_total_apply(struct lab_flow_location_totality_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_location_jump(ctx->frame, LOC_TOTAL_FINISH);
    return lab_flow_call(ctx->frame, LOC_TOTAL_SAVE, "native", 0, 0, 3, lab_dom_string("lab_location_totality_finish"),
                         ctx->state, lab_flow_location_flag(lab_dom_get(ctx->frame, "restored")));
}

static browser_handle lab_flow_location_totality_loc_total_save(struct lab_flow_location_totality_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_TOTAL_EVALUATE, "locSaveAlmanac", 0, 0, 0);
}

static browser_handle lab_flow_location_totality_loc_total_evaluate(struct lab_flow_location_totality_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_TOTAL_FINISH, "locEvaluateCurrent", 0, 1, 0);
}

static browser_handle lab_flow_location_totality_loc_total_error_current(struct lab_flow_location_totality_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(LOC_TOTAL_RETHROW_FINISH));
    return lab_flow_call(ctx->frame, LOC_TOTAL_ERROR_STATUS, "requestCurrent", "current", 0, 1, ctx->request);
}

static browser_handle lab_flow_location_totality_loc_total_error_status(struct lab_flow_location_totality_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_location_jump(ctx->frame, LOC_TOTAL_FINISH);
    return lab_flow_call(ctx->frame, LOC_TOTAL_ERROR_RENDER, "locStatus", 0, 0, 1, lab_dom_string("Error"));
}

static browser_handle lab_flow_location_totality_loc_total_error_render(struct lab_flow_location_totality_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_TOTAL_FINISH, "locError", 0, 0, 1,
                         lab_dom_get(lab_dom_get(ctx->frame, "exception"), "message"));
}

static browser_handle lab_flow_location_totality_loc_total_finish(struct lab_flow_location_totality_context *ctx)
{
    lab_dom_delete(ctx->frame, "failed");
    return lab_flow_call(ctx->frame, LOC_TOTAL_DONE, "locFinish", 0, 0, 1, ctx->request);
}

static browser_handle lab_flow_location_totality_loc_total_rethrow_finish(struct lab_flow_location_totality_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_TOTAL_RETHROW, "locFinish", 0, 0, 1, ctx->request);
}

static browser_handle lab_flow_location_totality_loc_total_rethrow(struct lab_flow_location_totality_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_TOTAL_DONE, "rethrow", 0, 0, 1, lab_dom_get(ctx->frame, "exception"));
}

static browser_handle lab_flow_location_totality_otherwise(struct lab_flow_location_totality_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static browser_handle lab_flow_location_totality(frame_handle frame)
{
    int request = lab_dom_get(frame, "request"), state = lab_dom_get(frame, "state");
    struct lab_flow_location_totality_context ctx = {
        .frame = frame,
        .request = request,
        .state = state,
    };
    static browser_handle (*const handlers[])(struct lab_flow_location_totality_context *) = {
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
    frame_handle frame;
    browser_handle mode;
    browser_handle elements;
    browser_handle request;
    browser_handle state;
    int update;
};

/**
 * @brief Recheck location-request ownership before applying refreshed data.
 * @param ctx Refresh state borrowing the request handle and the frame that receives the current flag.
 * @return Scoped ownership-check plan resuming at LOC_REFRESH_APPLY.
 */
static browser_handle lab_flow_location_refresh_loc_refresh_second_current(struct lab_flow_location_refresh_context *ctx);

/**
 * @brief Post the captured jurisdiction and date for location refresh.
 * @param ctx Refresh state borrowing the request and a frame containing jurisdiction and date.
 * @return Scoped post plan awaiting result before the response-ownership check.
 */
static browser_handle lab_flow_location_refresh_loc_refresh_post(struct lab_flow_location_refresh_context *ctx);

static browser_handle lab_flow_location_refresh_loc_refresh_elements(struct lab_flow_location_refresh_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_REFRESH_CONTROLS, "locElements", "elements", 0, 1, ctx->mode);
}

static browser_handle lab_flow_location_refresh_loc_refresh_controls(struct lab_flow_location_refresh_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_REFRESH_BEGIN, "locControls", "controls", 0, 2, ctx->mode,
                         lab_dom_scalar(LAB_VALUE_BOOLEAN, ctx->update));
}

static browser_handle lab_flow_location_refresh_loc_refresh_begin(struct lab_flow_location_refresh_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->elements, "jurisdiction")))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, LOC_REFRESH_READ, "locBegin", "request", 0, 2,
                         lab_dom_join(ctx->mode, lab_dom_string("Location"), ""), ctx->mode);
}

static browser_handle lab_flow_location_refresh_loc_refresh_read(struct lab_flow_location_refresh_context *ctx)
{
    if (!lab_dom_truth(ctx->request))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, LOC_REFRESH_STATE, "locRead", "read", 0, 1, ctx->mode);
}

static browser_handle lab_flow_location_refresh_loc_refresh_state(struct lab_flow_location_refresh_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_REFRESH_POPULATE, "locState", "state", 0, 3, ctx->mode,
                         lab_dom_get(ctx->frame, "read"), lab_dom_numeric(1));
}

static browser_handle lab_flow_location_refresh_loc_refresh_populate(struct lab_flow_location_refresh_context *ctx)
{
    lab_dom_set(ctx->frame, "jurisdiction", lab_dom_get(ctx->state, "jurisdiction"));
    lab_dom_set(ctx->frame, "date", lab_dom_get(ctx->state, "date"));
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(LOC_REFRESH_FINISH));
    if (ctx->update) {
        int options = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(options, "selectDefault", lab_dom_scalar(LAB_VALUE_BOOLEAN, 0));
        return lab_flow_call(ctx->frame, LOC_REFRESH_CURRENT, "locPopulate", 0, 1, 3,
                             lab_dom_get(ctx->elements, "town"), lab_dom_get(ctx->frame, "jurisdiction"), options);
    }
    return lab_flow_call(ctx->frame, LOC_REFRESH_STATUS, "locApplyTown", 0, 0, 1, lab_dom_get(ctx->frame, "controls"));
}

static browser_handle lab_flow_location_refresh_loc_refresh_current(struct lab_flow_location_refresh_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_REFRESH_COORDINATES, "requestCurrent", "current", 0, 1, ctx->request);
}

static browser_handle lab_flow_location_refresh_loc_refresh_coordinates(struct lab_flow_location_refresh_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_location_jump(ctx->frame, LOC_REFRESH_FINISH);
    return lab_flow_call(ctx->frame, LOC_REFRESH_DEFAULT, "locCoordinates", "matched", 0, 3,
                         lab_dom_get(ctx->elements, "town"),
                         lab_dom_get(lab_dom_get(ctx->elements, "latitude"), "value"),
                         lab_dom_get(lab_dom_get(ctx->elements, "longitude"), "value"));
}

static browser_handle lab_flow_location_refresh_loc_refresh_default(struct lab_flow_location_refresh_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "matched"))) {
        int options = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(options, "selectDefault", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
        return lab_flow_call(ctx->frame, LOC_REFRESH_SECOND_CURRENT, "locPopulate", 0, 1, 3,
                             lab_dom_get(ctx->elements, "town"), lab_dom_get(ctx->frame, "jurisdiction"), options);
    }
    /* Continue: preserve the second freshness check even without default population. */
    return lab_flow_location_refresh_loc_refresh_second_current(ctx);
}

static browser_handle lab_flow_location_refresh_loc_refresh_second_current(struct lab_flow_location_refresh_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_REFRESH_APPLY, "requestCurrent", "current", 0, 1, ctx->request);
}

static browser_handle lab_flow_location_refresh_loc_refresh_apply(struct lab_flow_location_refresh_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_location_jump(ctx->frame, LOC_REFRESH_FINISH);
    return lab_flow_call(ctx->frame, LOC_REFRESH_STATUS, "locApplyTown", "townApplied", 0, 1,
                         lab_dom_get(ctx->frame, "controls"));
}

static browser_handle lab_flow_location_refresh_loc_refresh_status(struct lab_flow_location_refresh_context *ctx)
{
    if (lab_dom_equal(ctx->mode, lab_dom_string("almanac")))
        return lab_flow_call(ctx->frame, LOC_REFRESH_POST, "locStatus", 0, 0, 1,
                             lab_dom_string("Updating almanac location..."));
    /* Continue: DateTime refresh does not alter status here. */
    return lab_flow_location_refresh_loc_refresh_post(ctx);
}

static browser_handle lab_flow_location_refresh_loc_refresh_post(struct lab_flow_location_refresh_context *ctx)
{
    int payload = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(payload, "jurisdiction", lab_dom_get(ctx->frame, "jurisdiction"));
    lab_dom_set(payload, "date", lab_dom_get(ctx->frame, "date"));
    return lab_flow_call(ctx->frame, LOC_REFRESH_RESPONSE_CURRENT, "post", "result", 1, 2, ctx->request, payload);
}

static browser_handle lab_flow_location_refresh_loc_refresh_response_current(struct lab_flow_location_refresh_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_REFRESH_RESPONSE_CONTEXT, "requestCurrent", "current", 0, 1, ctx->request);
}

static browser_handle lab_flow_location_refresh_loc_refresh_response_context(struct lab_flow_location_refresh_context *ctx)
{
    int result = lab_dom_get(ctx->frame, "result");
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")) ||
        !lab_dom_truth(lab_dom_get(lab_dom_get(result, "response"), "ok")) ||
        !lab_dom_truth(lab_dom_get(lab_dom_get(result, "data"), "ok")))
        return lab_flow_location_jump(ctx->frame, LOC_REFRESH_FINISH);
    return lab_flow_call(ctx->frame, LOC_REFRESH_RESPONSE, "locContext", "context", 0, 0);
}

static browser_handle lab_flow_location_refresh_loc_refresh_response(struct lab_flow_location_refresh_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_REFRESH_ACCEPT, "locResponse", 0, 0, 5, ctx->mode,
                         lab_dom_get(lab_dom_get(ctx->frame, "result"), "data"), lab_dom_get(ctx->frame, "context"),
                         lab_dom_scalar(LAB_VALUE_BOOLEAN, ctx->update), lab_dom_get(ctx->frame, "townApplied"));
}

static browser_handle lab_flow_location_refresh_loc_refresh_accept(struct lab_flow_location_refresh_context *ctx)
{
    return lab_flow_call(ctx->frame, LOC_REFRESH_FINISH, "locAcceptContext", 0, 0, 1,
                         lab_dom_get(ctx->frame, "context"));
}

static browser_handle lab_flow_location_refresh_loc_refresh_finish(struct lab_flow_location_refresh_context *ctx)
{
    lab_dom_delete(ctx->frame, "failed");
    return lab_flow_call(ctx->frame, LOC_REFRESH_DONE, "locFinish", 0, 0, 1, ctx->request);
}

static browser_handle lab_flow_location_refresh_otherwise(struct lab_flow_location_refresh_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static browser_handle lab_flow_location_refresh(frame_handle frame)
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
    static browser_handle (*const handlers[])(struct lab_flow_location_refresh_context *) = {
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
    frame_handle frame;
    browser_handle mode;
    browser_handle options;
    int almanac;
};

/**
 * @brief Optionally refresh the automatic location before saving its controls.
 * @param ctx Automatic-location state containing calendar selection and jurisdiction/coordinate refresh options.
 * @return Scoped refresh plan, or the calendar-save plan when refresh is disabled.
 */
static browser_handle lab_flow_location_auto_refresh(struct lab_flow_location_auto_context *ctx);

/**
 * @brief Save the selected calendar's automatic-location settings.
 * @param ctx Automatic-location state whose almanac flag selects the calendar and whose frame advances to stage 4.
 * @return Scoped plan calling the selected calendar-save service.
 */
static browser_handle lab_flow_location_auto_save(struct lab_flow_location_auto_context *ctx);

static browser_handle lab_flow_location_auto_current_mode(struct lab_flow_location_auto_context *ctx)
{
    return lab_flow_call(ctx->frame, 1, "currentMode", "currentMode", 0, 0);
}

static browser_handle lab_flow_location_auto_loc_clear_almanac(struct lab_flow_location_auto_context *ctx)
{
    if (!lab_dom_equal(ctx->mode, lab_dom_get(ctx->frame, "currentMode")))
        return lab_flow_done(0);
    if (ctx->almanac)
        return lab_flow_call(ctx->frame, 2, "locClearAlmanac", 0, 0, 0);
    /* Continue: only Almanac invalidates its cached worksheet. */
    return lab_flow_location_auto_refresh(ctx);
}

static browser_handle lab_flow_location_auto_refresh(struct lab_flow_location_auto_context *ctx)
{
    if (lab_dom_truth(lab_dom_get(ctx->options, "refreshJurisdiction"))) {
        int refresh = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(refresh, "updateCoordinates", lab_dom_get(ctx->options, "refreshCoordinates"));
        return lab_flow_call(ctx->frame, 3, ctx->almanac ? "locRefreshAlmanac" : "locRefreshDatetime", 0, 1, 1,
                             refresh);
    }
    /* Continue: persistence follows the optional location refresh. */
    return lab_flow_location_auto_save(ctx);
}

static browser_handle lab_flow_location_auto_save(struct lab_flow_location_auto_context *ctx)
{
    return lab_flow_call(ctx->frame, 4, ctx->almanac ? "locSaveAlmanac" : "locSaveDatetime", 0, 0, 0);
}

static browser_handle lab_flow_location_auto_evaluate(struct lab_flow_location_auto_context *ctx)
{
    int evaluate = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(evaluate, "skipHistoryUpdate", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
    return lab_flow_call(ctx->frame, 5, ctx->almanac ? "locEvaluateAlmanac" : "locEvaluateDatetime", 0, 1, 1, evaluate);
}

static browser_handle lab_flow_location_auto_update_history_buttons(struct lab_flow_location_auto_context *ctx)
{
    return lab_flow_call(ctx->frame, 6, "updateHistoryButtons", 0, 0, 0);
}

static browser_handle lab_flow_location_auto_otherwise(struct lab_flow_location_auto_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static browser_handle lab_flow_location_auto(frame_handle frame)
{
    int mode = lab_dom_get(frame, "mode"), options = lab_dom_get(frame, "options");
    int almanac = lab_dom_equal(mode, lab_dom_string("almanac"));
    struct lab_flow_location_auto_context ctx = {
        .frame = frame,
        .mode = mode,
        .options = options,
        .almanac = almanac,
    };
    static browser_handle (*const handlers[])(struct lab_flow_location_auto_context *) = {
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
    frame_handle frame;
    snapshot_handle snapshot;
};

/**
 * @brief Capture request ownership context for a calendar time change.
 * @param ctx Time-change state whose frame receives context and resumes at stage 3.
 * @return Scoped request-context capture plan.
 */
static browser_handle lab_flow_location_time_request_context(struct lab_flow_location_time_context *ctx);

static browser_handle lab_flow_location_time_loc_time_snapshot(struct lab_flow_location_time_context *ctx)
{
    return lab_flow_call(ctx->frame, 1, "locTimeSnapshot", "snapshot", 0, 0);
}

static browser_handle lab_flow_location_time_loc_format_time(struct lab_flow_location_time_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(7));
    return lab_flow_call(ctx->frame, 2, "locFormatTime", "formatted", 1, 1, lab_dom_get(ctx->snapshot, "authored"));
}

static browser_handle lab_flow_location_time_caught(struct lab_flow_location_time_context *ctx)
{
    lab_dom_set(ctx->frame, "caught", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
    /* Continue: recovery checks the same request and authored-value ownership. */
    return lab_flow_location_time_request_context(ctx);
}

static browser_handle lab_flow_location_time_request_context(struct lab_flow_location_time_context *ctx)
{
    return lab_flow_call(ctx->frame, 3, "requestContext", "context", 0, 0);
}

static browser_handle lab_flow_location_time_loc_latest_main(struct lab_flow_location_time_context *ctx)
{
    if (!lab_dom_equal(lab_dom_get(ctx->snapshot, "context"), lab_dom_get(ctx->frame, "context")))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, 4, "locLatestMain", "main", 0, 0);
}

static browser_handle lab_flow_location_time_loc_status(struct lab_flow_location_time_context *ctx)
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

static browser_handle lab_flow_location_time_otherwise(struct lab_flow_location_time_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static browser_handle lab_flow_location_time(frame_handle frame)
{
    int snapshot = lab_dom_get(frame, "snapshot");
    struct lab_flow_location_time_context ctx = {
        .frame = frame,
        .snapshot = snapshot,
    };
    static browser_handle (*const handlers[])(struct lab_flow_location_time_context *) = {
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
    frame_handle frame;
    browser_handle mode;
    unsigned int actions;
};

/**
 * @brief Read the active mode before finishing a location-control change.
 * @param ctx Location-control state whose frame receives currentMode and resumes at stage 5.
 * @return Scoped plan querying the current worksheet mode.
 */
static browser_handle lab_flow_location_control_current_mode(struct lab_flow_location_control_context *ctx);

static browser_handle lab_flow_location_control_loc_elements(struct lab_flow_location_control_context *ctx)
{
    return lab_flow_call(ctx->frame, 1, "locElements", "elements", 0, 1, ctx->mode);
}

static browser_handle lab_flow_location_control_loc_controls(struct lab_flow_location_control_context *ctx)
{
    if (ctx->actions & 4)
        return lab_flow_call(ctx->frame, 2, "locControls", "controls", 0, 1, ctx->mode);
    return lab_flow_location_jump(ctx->frame, 3);
}

static browser_handle lab_flow_location_control_loc_apply_town(struct lab_flow_location_control_context *ctx)
{
    return lab_flow_call(ctx->frame, 3, "locApplyTown", 0, 0, 1, lab_dom_get(ctx->frame, "controls"));
}

static browser_handle lab_flow_location_control_loc_clear_custom(struct lab_flow_location_control_context *ctx)
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

static browser_handle lab_flow_location_control_current_mode(struct lab_flow_location_control_context *ctx)
{
    return lab_flow_call(ctx->frame, 5, "currentMode", "currentMode", 0, 0);
}

static browser_handle lab_flow_location_control_complete(struct lab_flow_location_control_context *ctx)
{
    if (!lab_dom_equal(ctx->mode, lab_dom_get(ctx->frame, "currentMode")))
        return lab_flow_done(0);
    int options = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(options, "refreshJurisdiction", lab_dom_scalar(LAB_VALUE_BOOLEAN, (ctx->actions & 16) != 0));
    lab_dom_set(options, "refreshCoordinates", lab_dom_scalar(LAB_VALUE_BOOLEAN, (ctx->actions & 32) != 0));
    return lab_flow_call(ctx->frame, 6,
                         lab_dom_equal(ctx->mode, lab_dom_string("datetime")) ? "locTriggerDatetime"
                                                                              : "locTriggerAlmanac",
                         0, 0, 1, options);
}

static browser_handle lab_flow_location_control_otherwise(struct lab_flow_location_control_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static browser_handle lab_flow_location_control(frame_handle frame)
{
    int mode = lab_dom_get(frame, "mode");
    unsigned actions = (unsigned)lab_dom_number(lab_dom_get(frame, "actions"));
    struct lab_flow_location_control_context ctx = {
        .frame = frame,
        .mode = mode,
        .actions = actions,
    };
    static browser_handle (*const handlers[])(struct lab_flow_location_control_context *) = {
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
void lab_flow_location_local(browser_handle text, browser_handle sections, browser_handle body, browser_handle card)
{
    int frame = lab_dom_object(LAB_VALUE_OBJECT), plan = lab_dom_object(LAB_VALUE_OBJECT),
        calls = lab_dom_object(LAB_VALUE_ARRAY);
    text = lab_dom_get(text, "value");
    text = lab_dom_clean(lab_dom_text(lab_dom_truth(text) ? text : lab_dom_string("")), LAB_DOM_CLEAN_TRIM);
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
void lab_flow_location_visibility(browser_handle card, browser_handle mode)
{
    lab_dom_class(card, "hidden", !lab_dom_equal(mode, lab_dom_string("datetime")));
}

/* Callable-method detection belongs to the browser; preference and fallbacks are native policy. */
void lab_flow_location_select(node_handle select, browser_handle capabilities)
{
    int frame = lab_dom_object(LAB_VALUE_OBJECT), plan = lab_dom_object(LAB_VALUE_OBJECT);
    if (lab_dom_truth(select)) {
        if (lab_dom_truth(lab_dom_get(capabilities, "rebuild")))
            plan = lab_flow_call(frame, 1, "locSelectRebuild", 0, 0, 1, select);
        else if (lab_dom_truth(lab_dom_get(capabilities, "sync")))
            plan = lab_flow_call(frame, 1, "locSelectSync", 0, 0, 1, select);
    }
    lab_dom_return(plan);
}

/* Keep unchanged moves inert without making the host interpret movement policy. */
void lab_flow_location_picker(bool changed)
{
    int frame = lab_dom_object(LAB_VALUE_OBJECT), plan = lab_dom_object(LAB_VALUE_OBJECT);
    if (changed)
        plan = lab_flow_call(frame, 1, "locRenderPicker", 0, 0, 0);
    lab_dom_return(plan);
}

/* Dispatch a fixed group of independent workflows, with all persistent values in the browser frame. */
browser_handle lab_flow_location(lab_flow_location_kind_t kind, frame_handle frame, view_handle view)
{
    (void)view;
    static browser_handle (*const handlers[])(browser_handle) = {
        [LAB_FLOW_LOCATION_FORMS]         = lab_flow_location_forms,
        [LAB_FLOW_LOCATION_RESTORE]       = lab_flow_location_restore,
        [LAB_FLOW_LOCATION_POPULATE]      = lab_flow_location_populate,
        [LAB_FLOW_LOCATION_SYNC]          = lab_flow_location_sync,
        [LAB_FLOW_LOCATION_HISTORY]       = lab_flow_location_history,
        [LAB_FLOW_LOCATION_TOTALITY]      = lab_flow_location_totality,
        [LAB_FLOW_LOCATION_REFRESH]       = lab_flow_location_refresh,
        [LAB_FLOW_LOCATION_AUTO_EVALUATE] = lab_flow_location_auto,
        [LAB_FLOW_LOCATION_TIME]          = lab_flow_location_time,
        [LAB_FLOW_LOCATION_CONTROL]       = lab_flow_location_control,
    };
    return (unsigned)kind < sizeof handlers / sizeof *handlers ? handlers[kind](frame) : lab_flow_done(0);
}
