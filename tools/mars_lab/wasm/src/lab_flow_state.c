/**
 * @file lab_flow_state.c
 * @brief Saved-state and workspace workflows interpreted as native continuations.
 *
 * Owns restoration order, local fallback boundaries, history ownership guards and
 * workspace transition decisions. Browser services perform storage, clock, DOM
 * and asynchronous operations. Frames retain ordinary browser values between
 * stages; mathematical text is opaque and existing native policies remain authoritative.
 */
#include "lab_dom.h"
#include "lab_persist.h"
#include "lab_flow.h"
#include "lab_flow_state.h"

static unsigned lab_flow_state_stage(int frame)
{
    return (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
}

static int lab_flow_state_or(int value, int fallback)
{
    return lab_dom_truth(value) ? value : fallback;
}

static int lab_flow_state_next(int frame, unsigned next, const char *service)
{
    return lab_flow_call(frame, next, service, 0, 0, 0);
}

static int lab_flow_state_guard(int frame, unsigned next)
{
    return lab_flow_call(frame, next, "stateGuard", "current", 0, 1, lab_dom_get(frame, "isCurrent"));
}

static int lab_flow_state_false(void)
{
    return lab_flow_done(lab_dom_scalar(1, 0));
}

struct lab_flow_state_editors_context {
    int frame;
    int view;
    unsigned int index;
    int local;
    int text;
    unsigned int flags;
};

static int lab_flow_state_editors(int frame, int view);
static int lab_flow_state_editors_state_read(struct lab_flow_state_editors_context *ctx);
static int lab_flow_state_editors_state_canonical(struct lab_flow_state_editors_context *ctx);
static int lab_flow_state_editors_state_editor(struct lab_flow_state_editors_context *ctx);
static int lab_flow_state_editors_state_expression(struct lab_flow_state_editors_context *ctx);
static int lab_flow_state_editors_state_integrator_source(struct lab_flow_state_editors_context *ctx);

static int lab_flow_state_editors_advance(struct lab_flow_state_editors_context *ctx)
{
    lab_dom_set(ctx->frame, "index", lab_dom_numeric(ctx->index + 1));
    lab_dom_set(ctx->frame, "stage", lab_dom_numeric(1));
    return lab_flow_state_editors(ctx->frame, ctx->view);
}

static int lab_flow_state_editors_begin(struct lab_flow_state_editors_context *ctx)
{
    ctx->index = 0;
    lab_dom_set(ctx->frame, "index", lab_dom_numeric(ctx->index));
    /* Continue to the first lazy storage read. */
    return lab_flow_state_editors_state_read(ctx);
}

static int lab_flow_state_editors_state_read(struct lab_flow_state_editors_context *ctx)
{
    if (ctx->index >= 5)
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, 2, "stateRead", "raw", 0, 2, lab_dom_get(ctx->frame, "read"),
                         lab_dom_string(lab_persist_text(ctx->index, 0, ctx->local)));
}

static int lab_flow_state_editors_state_prepare(struct lab_flow_state_editors_context *ctx)
{
    ctx->text = lab_dom_text(lab_flow_state_or(lab_dom_get(ctx->frame, "raw"), lab_dom_string("")));
    if (!ctx->local)
        ctx->text = lab_dom_clean(ctx->text, 0);
    ctx->flags = lab_persist_restore(ctx->index, lab_dom_truth(ctx->text),
                                     lab_dom_contains(ctx->text, lab_dom_string("...")), ctx->local);
    lab_dom_set(ctx->frame, "text", ctx->text);
    lab_dom_set(ctx->frame, "flags", lab_dom_numeric(ctx->flags));
    if (!(ctx->flags & 1))
        return lab_flow_state_editors_advance(ctx);
    if (ctx->flags & 2)
        return lab_flow_call(ctx->frame, 3, "statePrepare", 0, 1, 1, ctx->text);
    /* Continue when no metadata preparation is needed. */
    return lab_flow_state_editors_state_canonical(ctx);
}

static int lab_flow_state_editors_state_canonical(struct lab_flow_state_editors_context *ctx)
{
    if (ctx->flags & 4)
        return lab_flow_call(ctx->frame, 4, "stateCanonical", "canonical", 0, 1, ctx->text);
    lab_dom_set(ctx->frame, "canonical", ctx->text);
    /* Continue to the editor projection. */
    return lab_flow_state_editors_state_editor(ctx);
}

static int lab_flow_state_editors_state_editor(struct lab_flow_state_editors_context *ctx)
{
    return lab_flow_call(ctx->frame, 5, "stateEditor", 0, 0, 2, lab_dom_numeric(ctx->index),
                         lab_dom_get(ctx->frame, "canonical"));
}

static int lab_flow_state_editors_state_read_5(struct lab_flow_state_editors_context *ctx)
{
    if (ctx->flags & 8)
        return lab_flow_call(ctx->frame, 6, "stateRead", "timestamp", 0, 2, lab_dom_get(ctx->frame, "read"),
                             lab_dom_string(lab_persist_text(ctx->index, 1, ctx->local)));
    return lab_flow_state_editors_state_integrator_source(ctx);
}

static int lab_flow_state_editors_state_now(struct lab_flow_state_editors_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "timestamp")) && ctx->local)
        return lab_flow_call(ctx->frame, 7, "stateNow", "timestamp", 0, 0);
    /* Continue: Number conversion retains NaN for malformed saved timestamps. */
    return lab_flow_state_editors_state_expression(ctx);
}

static int lab_flow_state_editors_state_expression(struct lab_flow_state_editors_context *ctx)
{
    return lab_flow_call(ctx->frame, 8, "stateExpression", 0, 0, 2, ctx->text,
                         lab_dom_numeric(lab_dom_to_number(
                             lab_flow_state_or(lab_dom_get(ctx->frame, "timestamp"), lab_dom_numeric(0)))));
}

static int lab_flow_state_editors_state_integrator_source(struct lab_flow_state_editors_context *ctx)
{
    if (ctx->flags & 16)
        return lab_flow_call(ctx->frame, 9, "stateIntegratorSource", 0, 0, 1, ctx->text);
    return lab_flow_state_editors_advance(ctx);
}

static int lab_flow_state_editors_next(struct lab_flow_state_editors_context *ctx)
{
    return lab_flow_state_editors_advance(ctx);
}

static int lab_flow_state_editors_otherwise(struct lab_flow_state_editors_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static int lab_flow_state_editors(int frame, int view)
{
    (void)view;
    unsigned stage = lab_flow_state_stage(frame), index = (unsigned)lab_dom_number(lab_dom_get(frame, "index"));
    int local = lab_dom_truth(lab_dom_get(frame, "local")), text = lab_dom_get(frame, "text");
    unsigned flags = (unsigned)lab_dom_number(lab_dom_get(frame, "flags"));
    struct lab_flow_state_editors_context ctx = {
        .frame = frame,
        .view = view,
        .index = index,
        .local = local,
        .text = text,
        .flags = flags,
    };
    static int (*const handlers[])(struct lab_flow_state_editors_context *) = {
        [0] = lab_flow_state_editors_begin,
        [1] = lab_flow_state_editors_state_read,
        [2] = lab_flow_state_editors_state_prepare,
        [3] = lab_flow_state_editors_state_canonical,
        [4] = lab_flow_state_editors_state_editor,
        [5] = lab_flow_state_editors_state_read_5,
        [6] = lab_flow_state_editors_state_now,
        [7] = lab_flow_state_editors_state_expression,
        [8] = lab_flow_state_editors_state_integrator_source,
        [9] = lab_flow_state_editors_next,
    };
    unsigned selected = stage;
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_flow_state_editors_otherwise(&ctx);
}

struct lab_flow_state_controls_context {
    int frame;
    int view;
    unsigned int index;
    int local;
};

static int lab_flow_state_controls(int frame, int view);
static int lab_flow_state_controls_next(struct lab_flow_state_controls_context *ctx);

static int lab_flow_state_controls_native(struct lab_flow_state_controls_context *ctx)
{
    lab_dom_set(ctx->frame, "index", lab_dom_numeric(0));
    return lab_flow_call(ctx->frame, 1, "native", "fields", 0, 2, lab_dom_string("lab_storage_fields"), ctx->local);
}

static int lab_flow_state_controls_state_read(struct lab_flow_state_controls_context *ctx)
{
    int fields = lab_dom_get(ctx->frame, "fields");
    if (ctx->index >= lab_dom_count(fields))
        return lab_flow_done(0);
    int field = lab_dom_item(fields, ctx->index);
    lab_dom_set(ctx->frame, "field", field);
    return lab_flow_call(ctx->frame, 2, "stateRead", "raw", 0, 2, lab_dom_get(ctx->frame, "read"),
                         lab_dom_get(field, "key"));
}

static int lab_flow_state_controls_state_control(struct lab_flow_state_controls_context *ctx)
{
    return lab_flow_call(ctx->frame, 3, "stateControl", "bounds", 0, 3,
                         lab_dom_get(lab_dom_get(ctx->frame, "field"), "field"), lab_dom_get(ctx->frame, "raw"),
                         ctx->local);
}

static int lab_flow_state_controls_state_bounds(struct lab_flow_state_controls_context *ctx)
{
    if (lab_dom_truth(lab_dom_get(ctx->frame, "bounds")))
        return lab_flow_call(ctx->frame, 4, "stateBounds", 0, 1, 1, lab_dom_get(ctx->frame, "bounds"));
    /* Continue only after the bounds await when present. */
    return lab_flow_state_controls_next(ctx);
}

static int lab_flow_state_controls_next(struct lab_flow_state_controls_context *ctx)
{
    lab_dom_set(ctx->frame, "index", lab_dom_numeric(ctx->index + 1));
    lab_dom_set(ctx->frame, "stage", lab_dom_numeric(1));
    return lab_flow_state_controls(ctx->frame, ctx->view);
}

static int lab_flow_state_controls_otherwise(struct lab_flow_state_controls_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static int lab_flow_state_controls(int frame, int view)
{
    (void)view;
    unsigned index = (unsigned)lab_dom_number(lab_dom_get(frame, "index"));
    int local = lab_dom_numeric(lab_dom_truth(lab_dom_get(frame, "local")));
    struct lab_flow_state_controls_context ctx = {
        .frame = frame,
        .view = view,
        .index = index,
        .local = local,
    };
    static int (*const handlers[])(struct lab_flow_state_controls_context *) = {
        [0] = lab_flow_state_controls_native,
        [1] = lab_flow_state_controls_state_read,
        [2] = lab_flow_state_controls_state_control,
        [3] = lab_flow_state_controls_state_bounds,
        [4] = lab_flow_state_controls_next,
    };
    unsigned selected = lab_flow_state_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_state_controls_otherwise(&ctx);
}

struct lab_flow_state_saved_context {
    int frame;
    int data;
    int mode;
};

static int lab_flow_state_saved_state_apply_mode(struct lab_flow_state_saved_context *ctx);

static int lab_flow_state_saved_state_server_editors(struct lab_flow_state_saved_context *ctx)
{
    return lab_flow_call(ctx->frame, 1, "stateServerEditors", 0, 1, 1, ctx->data);
}

static int lab_flow_state_saved_state_server_controls(struct lab_flow_state_saved_context *ctx)
{
    return lab_flow_call(ctx->frame, 2, "stateServerControls", 0, 1, 1, ctx->data);
}

static int lab_flow_state_saved_state_calendar(struct lab_flow_state_saved_context *ctx)
{
    return lab_flow_call(ctx->frame, 3, "stateCalendar", 0, 0, 4, lab_dom_string("datetime"), ctx->data,
                         lab_dom_numeric(0), lab_dom_string("datetime_"));
}

static int lab_flow_state_saved_state_calendar_3(struct lab_flow_state_saved_context *ctx)
{
    return lab_flow_call(ctx->frame, 4, "stateCalendar", 0, 0, 4, lab_dom_string("almanac"), ctx->data,
                         lab_dom_numeric(0), lab_dom_string("almanac_"));
}

static int lab_flow_state_saved_native(struct lab_flow_state_saved_context *ctx)
{
    return lab_flow_call(ctx->frame, 5, "native", 0, 0, 2, lab_dom_string("lab_storage_precisions"), ctx->data);
}

static int lab_flow_state_saved_state_sync_towns(struct lab_flow_state_saved_context *ctx)
{
    return lab_flow_call(ctx->frame, 6, "stateSyncTowns", 0, 1, 0);
}

static int lab_flow_state_saved_state_town(struct lab_flow_state_saved_context *ctx)
{
    return lab_flow_call(ctx->frame, 7, "stateTown", 0, 1, 2, lab_dom_numeric(0),
                         lab_dom_get(ctx->data, "datetime_town"));
}

static int lab_flow_state_saved_state_town_7(struct lab_flow_state_saved_context *ctx)
{
    return lab_flow_call(ctx->frame, 8, "stateTown", 0, 1, 2, lab_dom_numeric(1),
                         lab_dom_get(ctx->data, "almanac_town"));
}

static int lab_flow_state_saved_state_mode(struct lab_flow_state_saved_context *ctx)
{
    return lab_flow_call(ctx->frame, 9, "stateMode", "selected", 0, 1, lab_dom_get(ctx->data, "lab_mode"));
}

static int lab_flow_state_saved_state_prepare_mode(struct lab_flow_state_saved_context *ctx)
{
    if (!lab_dom_equal(ctx->mode, lab_dom_string("datetime")) && !lab_dom_equal(ctx->mode, lab_dom_string("almanac")))
        return lab_flow_call(ctx->frame, 10, "statePrepareMode", 0, 1, 1, ctx->mode);
    /* Continue for calendars without editor preparation. */
    return lab_flow_state_saved_state_apply_mode(ctx);
}

static int lab_flow_state_saved_state_apply_mode(struct lab_flow_state_saved_context *ctx)
{
    return lab_flow_call(ctx->frame, 11, "stateApplyMode", 0, 1, 1, ctx->mode);
}

static int lab_flow_state_saved_otherwise(struct lab_flow_state_saved_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static int lab_flow_state_saved(int frame, int view)
{
    (void)view;
    int data = lab_dom_get(frame, "data"), mode = lab_dom_get(frame, "selected");
    struct lab_flow_state_saved_context ctx = {
        .frame = frame,
        .data = data,
        .mode = mode,
    };
    static int (*const handlers[])(struct lab_flow_state_saved_context *) = {
        [0]  = lab_flow_state_saved_state_server_editors,
        [1]  = lab_flow_state_saved_state_server_controls,
        [2]  = lab_flow_state_saved_state_calendar,
        [3]  = lab_flow_state_saved_state_calendar_3,
        [4]  = lab_flow_state_saved_native,
        [5]  = lab_flow_state_saved_state_sync_towns,
        [6]  = lab_flow_state_saved_state_town,
        [7]  = lab_flow_state_saved_state_town_7,
        [8]  = lab_flow_state_saved_state_mode,
        [9]  = lab_flow_state_saved_state_prepare_mode,
        [10] = lab_flow_state_saved_state_apply_mode,
    };
    unsigned selected = lab_flow_state_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_flow_state_saved_otherwise(&ctx);
}

struct lab_flow_state_recovery_context {
    int frame;
    int view;
    unsigned int index;
    int recovery;
    int text;
};

static int lab_flow_state_recovery(int frame, int view);
static int lab_flow_state_recovery_state_recovery(struct lab_flow_state_recovery_context *ctx);
static int lab_flow_state_recovery_state_now(struct lab_flow_state_recovery_context *ctx);
static int lab_flow_state_recovery_state_editor(struct lab_flow_state_recovery_context *ctx);
static int lab_flow_state_recovery_state_save(struct lab_flow_state_recovery_context *ctx);

static int lab_flow_state_recovery_advance(struct lab_flow_state_recovery_context *ctx)
{
    lab_dom_set(ctx->frame, "index", lab_dom_numeric(ctx->index + 1));
    lab_dom_set(ctx->frame, "stage", lab_dom_numeric(1));
    return lab_flow_state_recovery(ctx->frame, ctx->view);
}

static int lab_flow_state_recovery_begin(struct lab_flow_state_recovery_context *ctx)
{
    ctx->index = 0;
    lab_dom_set(ctx->frame, "index", lab_dom_numeric(0));
    /* Continue to the first local copy. */
    return lab_flow_state_recovery_state_recovery(ctx);
}

static int lab_flow_state_recovery_state_recovery(struct lab_flow_state_recovery_context *ctx)
{
    if (ctx->index >= 2)
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, 2, "stateRecovery", "recovery", 0, 2, lab_dom_numeric(ctx->index),
                         lab_dom_get(ctx->frame, "data"));
}

static int lab_flow_state_recovery_state_prepare(struct lab_flow_state_recovery_context *ctx)
{
    if (!ctx->recovery)
        return lab_flow_state_recovery_advance(ctx);
    return lab_flow_call(ctx->frame, 3, "statePrepare", 0, 1, 1, lab_dom_get(ctx->recovery, "text"));
}

static int lab_flow_state_recovery_state_canonical(struct lab_flow_state_recovery_context *ctx)
{
    if (lab_persist_canonical(ctx->index))
        return lab_flow_call(ctx->frame, 4, "stateCanonical", "text", 0, 1, lab_dom_get(ctx->recovery, "text"));
    ctx->text = lab_dom_get(ctx->recovery, "text");
    lab_dom_set(ctx->frame, "text", ctx->text);
    /* Continue after metadata preparation, before reading the clock. */
    return lab_flow_state_recovery_state_now(ctx);
}

static int lab_flow_state_recovery_state_now(struct lab_flow_state_recovery_context *ctx)
{
    return lab_flow_call(ctx->frame, 5, "stateNow", "now", 0, 0);
}

static int lab_flow_state_recovery_state_recovered(struct lab_flow_state_recovery_context *ctx)
{
    return lab_flow_call(ctx->frame, 6, "stateRecovered", "patch", 0, 4, lab_dom_numeric(ctx->index), ctx->recovery,
                         ctx->text, lab_dom_get(ctx->frame, "now"));
}

static int lab_flow_state_recovery_state_timestamp(struct lab_flow_state_recovery_context *ctx)
{
    if (!ctx->index)
        return lab_flow_call(ctx->frame, 7, "stateTimestamp", 0, 0, 1,
                             lab_dom_get(lab_dom_get(ctx->frame, "patch"), "expression_updated_at"));
    /* Continue for equation recovery. */
    return lab_flow_state_recovery_state_editor(ctx);
}

static int lab_flow_state_recovery_state_editor(struct lab_flow_state_recovery_context *ctx)
{
    return lab_flow_call(ctx->frame, 8, "stateEditor", 0, 0, 2, lab_dom_numeric(ctx->index), ctx->text);
}

static int lab_flow_state_recovery_state_mode_index(struct lab_flow_state_recovery_context *ctx)
{
    return lab_flow_call(ctx->frame, 9, "stateModeIndex", "visible", 0, 0);
}

static int lab_flow_state_recovery_restore_visible(struct lab_flow_state_recovery_context *ctx)
{
    if ((unsigned)lab_dom_number(lab_dom_get(ctx->frame, "visible")) == ctx->index)
        return lab_flow_call(ctx->frame, 10, ctx->index ? "stateRestoreEditor" : "stateSetExpression", 0,
                             ctx->index != 0, 1, ctx->index ? lab_dom_string("equation") : ctx->text);
    /* Continue if this recovered worksheet is not visible. */
    return lab_flow_state_recovery_state_save(ctx);
}

static int lab_flow_state_recovery_state_save(struct lab_flow_state_recovery_context *ctx)
{
    return lab_flow_call(ctx->frame, 11, "stateSave", 0, 0, 1, lab_dom_get(ctx->frame, "patch"));
}

static int lab_flow_state_recovery_next(struct lab_flow_state_recovery_context *ctx)
{
    return lab_flow_state_recovery_advance(ctx);
}

static int lab_flow_state_recovery_otherwise(struct lab_flow_state_recovery_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static int lab_flow_state_recovery(int frame, int view)
{
    (void)view;
    unsigned index = (unsigned)lab_dom_number(lab_dom_get(frame, "index"));
    int recovery = lab_dom_get(frame, "recovery"), text = lab_dom_get(frame, "text");
    struct lab_flow_state_recovery_context ctx = {
        .frame = frame,
        .view = view,
        .index = index,
        .recovery = recovery,
        .text = text,
    };
    static int (*const handlers[])(struct lab_flow_state_recovery_context *) = {
        [0]  = lab_flow_state_recovery_begin,
        [1]  = lab_flow_state_recovery_state_recovery,
        [2]  = lab_flow_state_recovery_state_prepare,
        [3]  = lab_flow_state_recovery_state_canonical,
        [4]  = lab_flow_state_recovery_state_now,
        [5]  = lab_flow_state_recovery_state_recovered,
        [6]  = lab_flow_state_recovery_state_timestamp,
        [7]  = lab_flow_state_recovery_state_editor,
        [8]  = lab_flow_state_recovery_state_mode_index,
        [9]  = lab_flow_state_recovery_restore_visible,
        [10] = lab_flow_state_recovery_state_save,
        [11] = lab_flow_state_recovery_next,
    };
    unsigned selected = lab_flow_state_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_state_recovery_otherwise(&ctx);
}

struct lab_flow_state_load_context {
    int frame;
    int data;
    int mode;
};

static int lab_flow_state_load_state_local_calendar_8(struct lab_flow_state_load_context *ctx);
static int lab_flow_state_load_state_local_mode(struct lab_flow_state_load_context *ctx);
static int lab_flow_state_load_state_apply_mode(struct lab_flow_state_load_context *ctx);

static int lab_flow_state_load_state_fetch(struct lab_flow_state_load_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(10));
    return lab_flow_call(ctx->frame, 1, "stateFetch", "response", 1, 0);
}

static int lab_flow_state_load_state_decode(struct lab_flow_state_load_context *ctx)
{
    return lab_flow_call(ctx->frame, 2, "stateDecode", "data", 1, 1, lab_dom_get(ctx->frame, "response"));
}

static int lab_flow_state_load_state_apply_saved(struct lab_flow_state_load_context *ctx)
{
    return lab_flow_call(ctx->frame, 3, "stateApplySaved", 0, 1, 1, lab_flow_state_or(ctx->data, lab_dom_object(5)));
}

static int lab_flow_state_load_state_recover(struct lab_flow_state_load_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(30));
    return lab_flow_call(ctx->frame, 30, "stateRecover", 0, 1, 1, ctx->data);
}

static int lab_flow_state_load_state_local_editors(struct lab_flow_state_load_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(30));
    return lab_flow_call(ctx->frame, 11, "stateLocalEditors", 0, 1, 0);
}

static int lab_flow_state_load_state_local_controls(struct lab_flow_state_load_context *ctx)
{
    return lab_flow_call(ctx->frame, 12, "stateLocalControls", 0, 1, 0);
}

static int lab_flow_state_load_state_local_calendar(struct lab_flow_state_load_context *ctx)
{
    return lab_flow_call(ctx->frame, 13, "stateLocalCalendar", "datetime", 0, 1,
                         lab_dom_string("mars.exprLab.lastDatetimeState"));
}

static int lab_flow_state_load_state_calendar(struct lab_flow_state_load_context *ctx)
{
    if (lab_dom_truth(lab_dom_get(ctx->frame, "datetime")))
        return lab_flow_call(ctx->frame, 14, "stateCalendar", 0, 0, 2, lab_dom_string("datetime"),
                             lab_dom_get(ctx->frame, "datetime"));
    /* Continue for an absent local calendar. */
    return lab_flow_state_load_state_local_calendar_8(ctx);
}

static int lab_flow_state_load_state_local_calendar_8(struct lab_flow_state_load_context *ctx)
{
    return lab_flow_call(ctx->frame, 15, "stateLocalCalendar", "almanac", 0, 1,
                         lab_dom_string("mars.exprLab.lastAlmanacState"));
}

static int lab_flow_state_load_state_calendar_9(struct lab_flow_state_load_context *ctx)
{
    if (lab_dom_truth(lab_dom_get(ctx->frame, "almanac")))
        return lab_flow_call(ctx->frame, 16, "stateCalendar", 0, 0, 2, lab_dom_string("almanac"),
                             lab_dom_get(ctx->frame, "almanac"));
    /* Continue for an absent local almanac. */
    return lab_flow_state_load_state_local_mode(ctx);
}

static int lab_flow_state_load_state_local_mode(struct lab_flow_state_load_context *ctx)
{
    return lab_flow_call(ctx->frame, 17, "stateLocalMode", "savedMode", 0, 0);
}

static int lab_flow_state_load_state_sync_towns(struct lab_flow_state_load_context *ctx)
{
    return lab_flow_call(ctx->frame, 18, "stateSyncTowns", 0, 1, 0);
}

static int lab_flow_state_load_state_town(struct lab_flow_state_load_context *ctx)
{
    return lab_flow_call(
        ctx->frame, 19, "stateTown", 0, 1, 2, lab_dom_numeric(0),
        lab_flow_state_or(lab_dom_get(lab_dom_get(ctx->frame, "datetime"), "town"), lab_dom_string("")));
}

static int lab_flow_state_load_state_town_13(struct lab_flow_state_load_context *ctx)
{
    return lab_flow_call(
        ctx->frame, 20, "stateTown", 0, 1, 2, lab_dom_numeric(1),
        lab_flow_state_or(lab_dom_get(lab_dom_get(ctx->frame, "almanac"), "town"), lab_dom_string("")));
}

static int lab_flow_state_load_state_mode(struct lab_flow_state_load_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "savedMode")))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, 21, "stateMode", "selected", 0, 1, lab_dom_get(ctx->frame, "savedMode"));
}

static int lab_flow_state_load_state_prepare_mode(struct lab_flow_state_load_context *ctx)
{
    if (!lab_dom_equal(ctx->mode, lab_dom_string("datetime")) && !lab_dom_equal(ctx->mode, lab_dom_string("almanac")))
        return lab_flow_call(ctx->frame, 22, "statePrepareMode", 0, 1, 1, ctx->mode);
    /* Continue for calendars. */
    return lab_flow_state_load_state_apply_mode(ctx);
}

static int lab_flow_state_load_state_apply_mode(struct lab_flow_state_load_context *ctx)
{
    return lab_flow_call(ctx->frame, 30, "stateApplyMode", 0, 1, 1, ctx->mode);
}

static int lab_flow_state_load_otherwise(struct lab_flow_state_load_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static int lab_flow_state_load(int frame, int view)
{
    (void)view;
    int data = lab_dom_get(frame, "data"), mode = lab_dom_get(frame, "selected");
    struct lab_flow_state_load_context ctx = {
        .frame = frame,
        .data = data,
        .mode = mode,
    };
    static int (*const handlers[])(struct lab_flow_state_load_context *) = {
        [0]  = lab_flow_state_load_state_fetch,
        [1]  = lab_flow_state_load_state_decode,
        [2]  = lab_flow_state_load_state_apply_saved,
        [3]  = lab_flow_state_load_state_recover,
        [10] = lab_flow_state_load_state_local_editors,
        [11] = lab_flow_state_load_state_local_controls,
        [12] = lab_flow_state_load_state_local_calendar,
        [13] = lab_flow_state_load_state_calendar,
        [14] = lab_flow_state_load_state_local_calendar_8,
        [15] = lab_flow_state_load_state_calendar_9,
        [16] = lab_flow_state_load_state_local_mode,
        [17] = lab_flow_state_load_state_sync_towns,
        [18] = lab_flow_state_load_state_town,
        [19] = lab_flow_state_load_state_town_13,
        [20] = lab_flow_state_load_state_mode,
        [21] = lab_flow_state_load_state_prepare_mode,
        [22] = lab_flow_state_load_state_apply_mode,
    };
    unsigned selected = lab_flow_state_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_flow_state_load_otherwise(&ctx);
}

struct lab_flow_state_history_context {
    int frame;
    int state;
    int plan;
};

static int lab_flow_state_history_prepared_guard(struct lab_flow_state_history_context *ctx);
static int lab_flow_state_history_final_guard(struct lab_flow_state_history_context *ctx);

static int lab_flow_state_history_begin(struct lab_flow_state_history_context *ctx)
{
    if (!lab_dom_truth(ctx->state))
        return lab_flow_state_false();
    return lab_flow_state_guard(ctx->frame, 1);
}

static int lab_flow_state_history_state_history_plan(struct lab_flow_state_history_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_state_false();
    return lab_flow_call(ctx->frame, 2, "stateHistoryPlan", "plan", 0, 2, lab_dom_numeric(0), ctx->state);
}

static int lab_flow_state_history_state_prepare(struct lab_flow_state_history_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->plan, "calendar")))
        return lab_flow_call(ctx->frame, 3, "statePrepare", 0, 1, 1, lab_dom_get(ctx->plan, "text"));
    /* Continue to the same ownership check for calendars. */
    return lab_flow_state_history_prepared_guard(ctx);
}

static int lab_flow_state_history_prepared_guard(struct lab_flow_state_history_context *ctx)
{
    return lab_flow_state_guard(ctx->frame, 4);
}

static int lab_flow_state_history_state_history_plan_4(struct lab_flow_state_history_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_state_false();
    return lab_flow_call(ctx->frame, 5, "stateHistoryPlan", 0, 0, 2, lab_dom_numeric(1), ctx->state);
}

static int lab_flow_state_history_state_bounds(struct lab_flow_state_history_context *ctx)
{
    if (lab_dom_truth(lab_dom_get(ctx->plan, "integrator")))
        return lab_flow_call(ctx->frame, 6, "stateBounds", 0, 1, 2, lab_dom_get(ctx->plan, "bounds"),
                             lab_dom_get(ctx->frame, "isCurrent"));
    if (lab_dom_truth(lab_dom_get(ctx->plan, "calendar")))
        return lab_flow_call(ctx->frame, 9, "stateCalendarHistory", 0, 1, 3, lab_dom_get(ctx->plan, "mode"),
                             lab_dom_get(ctx->plan, "calendarState"), lab_dom_get(ctx->frame, "isCurrent"));
    return lab_flow_state_history_final_guard(ctx);
}

static int lab_flow_state_history_bounds_guard(struct lab_flow_state_history_context *ctx)
{
    return lab_flow_state_guard(ctx->frame, 7);
}

static int lab_flow_state_history_state_history_plan_7(struct lab_flow_state_history_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_state_false();
    return lab_flow_call(ctx->frame, 9, "stateHistoryPlan", 0, 0, 2, lab_dom_numeric(2), ctx->state);
}

static int lab_flow_state_history_final_guard(struct lab_flow_state_history_context *ctx)
{
    return lab_flow_state_guard(ctx->frame, 10);
}

static int lab_flow_state_history_state_history_plan_9(struct lab_flow_state_history_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_state_false();
    if (lab_dom_truth(lab_dom_get(ctx->plan, "calendar")))
        return lab_flow_call(ctx->frame, 11, "stateHistoryPlan", 0, 0, 2, lab_dom_numeric(3), ctx->state);
    return lab_flow_call(ctx->frame, 13, "stateUpdated", 0, 0, 1, lab_dom_get(ctx->plan, "text"));
}

static int lab_flow_state_history_state_clear_source(struct lab_flow_state_history_context *ctx)
{
    return lab_flow_state_next(ctx->frame, 12, "stateClearSource");
}

static int lab_flow_state_history_state_clear_bindings(struct lab_flow_state_history_context *ctx)
{
    return lab_flow_state_next(ctx->frame, 13, "stateClearBindings");
}

static int lab_flow_state_history_otherwise(struct lab_flow_state_history_context *ctx)
{
    (void)ctx;
    return lab_flow_done(lab_dom_scalar(1, 1));
}

static int lab_flow_state_history(int frame, int view)
{
    (void)view;
    int state = lab_dom_get(frame, "state"), plan = lab_dom_get(frame, "plan");
    struct lab_flow_state_history_context ctx = {
        .frame = frame,
        .state = state,
        .plan = plan,
    };
    static int (*const handlers[])(struct lab_flow_state_history_context *) = {
        [0]  = lab_flow_state_history_begin,
        [1]  = lab_flow_state_history_state_history_plan,
        [2]  = lab_flow_state_history_state_prepare,
        [3]  = lab_flow_state_history_prepared_guard,
        [4]  = lab_flow_state_history_state_history_plan_4,
        [5]  = lab_flow_state_history_state_bounds,
        [6]  = lab_flow_state_history_bounds_guard,
        [7]  = lab_flow_state_history_state_history_plan_7,
        [9]  = lab_flow_state_history_final_guard,
        [10] = lab_flow_state_history_state_history_plan_9,
        [11] = lab_flow_state_history_state_clear_source,
        [12] = lab_flow_state_history_state_clear_bindings,
    };
    unsigned selected = lab_flow_state_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_flow_state_history_otherwise(&ctx);
}

struct lab_flow_state_navigation_context {
    int frame;
    int request;
};

static int lab_flow_state_navigation_state_commit_bindings(struct lab_flow_state_navigation_context *ctx)
{
    return lab_flow_call(ctx->frame, 1, "stateCommitBindings", 0, 1, 0);
}

static int lab_flow_state_navigation_request_current(struct lab_flow_state_navigation_context *ctx)
{
    return lab_flow_call(ctx->frame, 2, "requestCurrent", "current", 0, 1, ctx->request);
}

static int lab_flow_state_navigation_state_navigate(struct lab_flow_state_navigation_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, 3, "stateNavigate", "restored", 0, 1, lab_dom_get(ctx->frame, "direction"));
}

static int lab_flow_state_navigation_state_history_buttons(struct lab_flow_state_navigation_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "restored")))
        return lab_flow_state_next(ctx->frame, 8, "stateHistoryButtons");
    return lab_flow_state_next(ctx->frame, 4, "stateCancelBindings");
}

static int lab_flow_state_navigation_state_restore_history(struct lab_flow_state_navigation_context *ctx)
{
    return lab_flow_call(ctx->frame, 5, "stateRestoreHistory", 0, 1, 2, lab_dom_get(ctx->frame, "restored"),
                         ctx->request);
}

static int lab_flow_state_navigation_request_current_5(struct lab_flow_state_navigation_context *ctx)
{
    return lab_flow_call(ctx->frame, 6, "requestCurrent", "current", 0, 1, ctx->request);
}

static int lab_flow_state_navigation_state_evaluate_history(struct lab_flow_state_navigation_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, 8, "stateEvaluateHistory", 0, 1, 0);
}

static int lab_flow_state_navigation_otherwise(struct lab_flow_state_navigation_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static int lab_flow_state_navigation(int frame, int view)
{
    (void)view;
    int request = lab_dom_get(frame, "request");
    struct lab_flow_state_navigation_context ctx = {
        .frame = frame,
        .request = request,
    };
    static int (*const handlers[])(struct lab_flow_state_navigation_context *) = {
        [0] = lab_flow_state_navigation_state_commit_bindings,
        [1] = lab_flow_state_navigation_request_current,
        [2] = lab_flow_state_navigation_state_navigate,
        [3] = lab_flow_state_navigation_state_history_buttons,
        [4] = lab_flow_state_navigation_state_restore_history,
        [5] = lab_flow_state_navigation_request_current_5,
        [6] = lab_flow_state_navigation_state_evaluate_history,
    };
    unsigned selected = lab_flow_state_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_state_navigation_otherwise(&ctx);
}

struct lab_flow_state_capture_context {
    int frame;
    unsigned int index;
};

static int lab_flow_state_capture_state_capture_guard(struct lab_flow_state_capture_context *ctx)
{
    return lab_flow_call(ctx->frame, 1, "stateCaptureGuard", "isCurrent", 0, 1, lab_dom_get(ctx->frame, "isCurrent"));
}

static int lab_flow_state_capture_guard(struct lab_flow_state_capture_context *ctx)
{
    return lab_flow_state_guard(ctx->frame, 2);
}

static int lab_flow_state_capture_state_commit_bindings(struct lab_flow_state_capture_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_state_false();
    return lab_flow_call(ctx->frame, 3, "stateCommitBindings", 0, 1, 1, lab_dom_get(ctx->frame, "isCurrent"));
}

static int lab_flow_state_capture_binding_guard(struct lab_flow_state_capture_context *ctx)
{
    return lab_flow_state_guard(ctx->frame, 4);
}

static int lab_flow_state_capture_state_capture_editor(struct lab_flow_state_capture_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_state_false();
    return lab_flow_call(ctx->frame, 5, "stateCaptureEditor", "index", 0, 0);
}

static int lab_flow_state_capture_history_guard(struct lab_flow_state_capture_context *ctx)
{
    return lab_flow_call(ctx->frame, 6, ctx->index < 5 ? "stateSaveEditor" : "stateSaveCalendar", 0, 0, 1,
                         lab_dom_numeric(ctx->index));
}

static int lab_flow_state_capture_otherwise(struct lab_flow_state_capture_context *ctx)
{
    (void)ctx;
    return lab_flow_done(lab_dom_scalar(1, 1));
}

static int lab_flow_state_capture(int frame, int view)
{
    (void)view;
    unsigned index = (unsigned)lab_dom_number(lab_dom_get(frame, "index"));
    struct lab_flow_state_capture_context ctx = {
        .frame = frame,
        .index = index,
    };
    static int (*const handlers[])(struct lab_flow_state_capture_context *) = {
        [0] = lab_flow_state_capture_state_capture_guard,
        [1] = lab_flow_state_capture_guard,
        [2] = lab_flow_state_capture_state_commit_bindings,
        [3] = lab_flow_state_capture_binding_guard,
        [4] = lab_flow_state_capture_state_capture_editor,
        [5] = lab_flow_state_capture_history_guard,
    };
    unsigned selected = lab_flow_state_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_flow_state_capture_otherwise(&ctx);
}

static int lab_flow_state_selection_state_mode_index(int frame);
static int lab_flow_state_selection_otherwise(int frame);

static int lab_flow_state_selection_state_capture_request(int frame)
{
    return lab_flow_call(frame, 1, "stateCaptureRequest", "captured", 1, 1, lab_dom_get(frame, "request"));
}

static int lab_flow_state_selection_state_save_result(int frame)
{
    if (!lab_dom_truth(lab_dom_get(frame, "captured")))
        return lab_flow_done(0);
    return lab_flow_state_next(frame, 2, "stateSaveResult");
}

static int lab_flow_state_selection_state_set_mode(int frame)
{
    return lab_flow_call(frame, 3, "stateSetMode", "changed", 0, 1, lab_dom_get(frame, "mode"));
}

static int lab_flow_state_selection_state_selection_guard(int frame)
{
    if (!lab_dom_truth(lab_dom_get(frame, "changed")))
        return lab_flow_done(0);
    return lab_flow_call(frame, 4, "stateSelectionGuard", "isCurrent", 0, 0);
}

static int lab_flow_state_selection_state_save_mode(int frame)
{
    lab_dom_set(frame, "failed", lab_dom_numeric(30));
    return lab_flow_state_next(frame, 5, "stateSaveMode");
}

static int lab_flow_state_selection_state_hide_target(int frame)
{
    return lab_flow_state_next(frame, 6, "stateHideTarget");
}

static int lab_flow_state_selection_state_restore_current_editor(int frame)
{
    return lab_flow_call(frame, 7, "stateRestoreCurrentEditor", 0, 1, 0);
}

static int lab_flow_state_selection_editor_guard(int frame)
{
    return lab_flow_state_guard(frame, 8);
}

static int lab_flow_state_selection_state_sync_ui(int frame)
{
    if (!lab_dom_truth(lab_dom_get(frame, "current")))
        return lab_flow_done(0);
    return lab_flow_state_next(frame, 9, "stateSyncUI");
}

static int lab_flow_state_selection_state_restore_result(int frame)
{
    return lab_flow_call(frame, 10, "stateRestoreResult", 0, 1, 0);
}

static int lab_flow_state_selection_result_guard(int frame)
{
    return lab_flow_state_guard(frame, 11);
}

static int lab_flow_state_selection_state_needs_bounds(int frame)
{
    if (!lab_dom_truth(lab_dom_get(frame, "current")))
        return lab_flow_done(0);
    return lab_flow_call(frame, 12, "stateNeedsBounds", "needsBounds", 0, 0);
}

static int lab_flow_state_selection_state_reset_bounds(int frame)
{
    if (lab_dom_truth(lab_dom_get(frame, "needsBounds")))
        return lab_flow_call(frame, 13, "stateResetBounds", "reset", 1, 0);
    return lab_flow_state_selection_state_mode_index(frame);
}

static int lab_flow_state_selection_bounds_guard(int frame)
{
    if (!lab_dom_truth(lab_dom_get(frame, "reset")))
        return lab_flow_done(0);
    return lab_flow_state_guard(frame, 14);
}

static int lab_flow_state_selection_finish_guard(int frame)
{
    if (!lab_dom_truth(lab_dom_get(frame, "current")))
        return lab_flow_done(0);
    /* Continue to mode-specific final projection. */
    return lab_flow_state_selection_state_mode_index(frame);
}

static int lab_flow_state_selection_state_mode_index(int frame)
{
    return lab_flow_call(frame, 16, "stateModeIndex", "index", 0, 0);
}

static int lab_flow_state_selection_finish(int frame)
{
    unsigned index = (unsigned)lab_dom_number(lab_dom_get(frame, "index"));
    static const char *const finishers[] = {"stateFocusEditor",  "stateFocusEditor",      "stateFocusEditor",
                                            "stateFocusEditor",  "stateFinishIntegrator", "stateFinishDatetime",
                                            "stateFinishAlmanac"};
    return index < 7 ? lab_flow_state_next(frame, 40, finishers[index]) : lab_flow_done(0);
}

static int lab_flow_state_selection_error_guard(int frame)
{
    return lab_flow_state_guard(frame, 31);
}

static int lab_flow_state_selection_state_status(int frame)
{
    if (lab_dom_truth(lab_dom_get(frame, "current")))
        return lab_flow_call(frame, 40, "stateStatus", 0, 0, 1, lab_dom_get(frame, "error"));
    /* Continue: stale failures cannot change the visible status. */
    return lab_flow_state_selection_otherwise(frame);
}

static int lab_flow_state_selection_otherwise(int frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static int lab_flow_state_selection(int frame, int view)
{
    (void)view;
    static int (*const handlers[])(int) = {
        [0]  = lab_flow_state_selection_state_capture_request,
        [1]  = lab_flow_state_selection_state_save_result,
        [2]  = lab_flow_state_selection_state_set_mode,
        [3]  = lab_flow_state_selection_state_selection_guard,
        [4]  = lab_flow_state_selection_state_save_mode,
        [5]  = lab_flow_state_selection_state_hide_target,
        [6]  = lab_flow_state_selection_state_restore_current_editor,
        [7]  = lab_flow_state_selection_editor_guard,
        [8]  = lab_flow_state_selection_state_sync_ui,
        [9]  = lab_flow_state_selection_state_restore_result,
        [10] = lab_flow_state_selection_result_guard,
        [11] = lab_flow_state_selection_state_needs_bounds,
        [12] = lab_flow_state_selection_state_reset_bounds,
        [13] = lab_flow_state_selection_bounds_guard,
        [14] = lab_flow_state_selection_finish_guard,
        [15] = lab_flow_state_selection_state_mode_index,
        [16] = lab_flow_state_selection_finish,
        [30] = lab_flow_state_selection_error_guard,
        [31] = lab_flow_state_selection_state_status,
    };
    unsigned selected = lab_flow_state_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](frame)
               : lab_flow_state_selection_otherwise(frame);
}

static int lab_flow_state_precision_state_history_buttons(int frame);

static int lab_flow_state_precision_state_precision_step(int frame)
{
    return lab_flow_call(frame, 1, "statePrecisionStep", 0, 0, 2, lab_dom_get(frame, "direction"),
                         lab_dom_get(frame, "bits"));
}

static int lab_flow_state_precision_state_save_precision(int frame)
{
    return lab_flow_state_next(frame, 2, "stateSavePrecision");
}

static int lab_flow_state_precision_state_status(int frame)
{
    return lab_flow_call(frame, 3, "stateStatus", 0, 0, 1, lab_dom_string("Precision changed"));
}

static int lab_flow_state_precision_state_precision_plan(int frame)
{
    lab_dom_set(frame, "failed", lab_dom_numeric(10));
    return lab_flow_call(frame, 4, "statePrecisionPlan", "plan", 0, 0);
}

static int lab_flow_state_precision_complete(int frame)
{
    {
        int plan = lab_dom_get(frame, "plan");
        unsigned action = (unsigned)lab_dom_number(lab_dom_get(plan, "action"));
        static const char *const services[] = {"stateHistoryButtons", "statePrecisionGoal", "statePrecisionExpression",
                                               "stateEvaluate"};
        if (action > 3)
            action = 0;
        if (action)
            return lab_flow_call(frame, 5, services[action], 0, 1, 1, plan);
        /* Continue to finally for a no-op precision refresh. */
    }
    return lab_flow_state_precision_state_history_buttons(frame);
}

static int lab_flow_state_precision_state_history_buttons(int frame)
{
    lab_dom_delete(frame, "failed");
    return lab_flow_state_next(frame, 6, "stateHistoryButtons");
}

static int lab_flow_state_precision_state_history_buttons_6(int frame)
{
    return lab_flow_state_next(frame, 11, "stateHistoryButtons");
}

static int lab_flow_state_precision_rethrow(int frame)
{
    return lab_flow_call(frame, 6, "rethrow", 0, 0, 1, lab_dom_get(frame, "exception"));
}

static int lab_flow_state_precision_otherwise(int frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static int lab_flow_state_precision(int frame, int view)
{
    (void)view;
    static int (*const handlers[])(int) = {
        [0]  = lab_flow_state_precision_state_precision_step,
        [1]  = lab_flow_state_precision_state_save_precision,
        [2]  = lab_flow_state_precision_state_status,
        [3]  = lab_flow_state_precision_state_precision_plan,
        [4]  = lab_flow_state_precision_complete,
        [5]  = lab_flow_state_precision_state_history_buttons,
        [10] = lab_flow_state_precision_state_history_buttons_6,
        [11] = lab_flow_state_precision_rethrow,
    };
    unsigned selected = lab_flow_state_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](frame)
               : lab_flow_state_precision_otherwise(frame);
}

/* Fixed workflow dispatch keeps browser hosts free of restoration and transition policy. */
int lab_flow_state(unsigned kind, int frame, int view)
{
    static int (*const handlers[])(int, int) = {
        lab_flow_state_editors,   lab_flow_state_controls, lab_flow_state_saved,      lab_flow_state_recovery,
        lab_flow_state_load,      lab_flow_state_history,  lab_flow_state_navigation, lab_flow_state_capture,
        lab_flow_state_selection, lab_flow_state_precision};
    return kind < sizeof handlers / sizeof *handlers ? handlers[kind](frame, view) : lab_flow_done(0);
}
