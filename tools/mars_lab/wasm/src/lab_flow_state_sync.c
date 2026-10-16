/**
 * @file lab_flow_state_sync.c
 * @brief Synchronous worksheet save, history and workspace projection plans.
 *
 * Keeps policy in C without turning synchronous browser APIs into promises. Each
 * plan performs one allowlisted capability; the host resumes immediately with its
 * result. Deferred saves use the existing native serial owner and read keepalive
 * only when publishing. Failed browser storage does not suppress server saves.
 */
#include "lab_dom.h"
#include "lab_events.h"
#include "lab_persist.h"
#include "lab_workspace.h"
#include "lab_flow.h"
#include "lab_flow_state_sync.h"

static unsigned lab_state_sync_stage(int frame)
{
    return (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
}

static unsigned lab_state_sync_index(int frame)
{
    return (unsigned)lab_dom_number(lab_dom_get(frame, "index"));
}

static int lab_state_sync_or(int value, int fallback)
{
    return lab_dom_truth(value) ? value : fallback;
}

static int lab_state_sync_next(int frame, unsigned next, const char *service)
{
    return lab_flow_call(frame, next, service, 0, 0, 0);
}

struct lab_state_sync_save_context {
    int frame;
    unsigned int index;
    int text;
};

static int lab_state_sync_save_state_now(struct lab_state_sync_save_context *ctx);
static int lab_state_sync_save_state_retain_text(struct lab_state_sync_save_context *ctx);
static int lab_state_sync_save_state_records(struct lab_state_sync_save_context *ctx);

static int lab_state_sync_save_state_canonical(struct lab_state_sync_save_context *ctx)
{
    if (!lab_persist_count(ctx->index))
        return lab_flow_done(0);
    ctx->text = lab_dom_clean(lab_dom_text(lab_state_sync_or(ctx->text, lab_dom_string(""))), 0);
    lab_dom_set(ctx->frame, "text", ctx->text);
    if (lab_persist_canonical(ctx->index))
        return lab_flow_call(ctx->frame, 1, "stateCanonical", "text", 0, 1, ctx->text);
    /* Continue when no prepared canonical metadata is needed. */
    return lab_state_sync_save_state_now(ctx);
}

static int lab_state_sync_save_state_now(struct lab_state_sync_save_context *ctx)
{
    return lab_flow_call(ctx->frame, 2, "stateNow", "updatedAt", 0, 0);
}

static int lab_state_sync_save_state_timestamp(struct lab_state_sync_save_context *ctx)
{
    if (!ctx->index)
        return lab_flow_call(ctx->frame, 3, "stateTimestamp", 0, 0, 1, lab_dom_get(ctx->frame, "updatedAt"));
    /* Continue for non-expression worksheets. */
    return lab_state_sync_save_state_retain_text(ctx);
}

static int lab_state_sync_save_state_retain_text(struct lab_state_sync_save_context *ctx)
{
    return lab_flow_call(ctx->frame, 4, "stateRetainText", 0, 0, 2, lab_dom_numeric(ctx->index), ctx->text);
}

static int lab_state_sync_save_state_matrix_operation(struct lab_state_sync_save_context *ctx)
{
    if (ctx->index == 3)
        return lab_flow_call(ctx->frame, 5, "stateMatrixOperation", "operation", 0, 0);
    if (ctx->index == 4)
        return lab_flow_call(ctx->frame, 6, "stateCurrentBounds", "bounds", 0, 0);
    return lab_state_sync_save_state_records(ctx);
}

static int lab_state_sync_save_state_matrix_operand(struct lab_state_sync_save_context *ctx)
{
    return lab_flow_call(ctx->frame, 7, "stateMatrixOperand", "operand", 0, 0);
}

static int lab_state_sync_save_state_interval_cap(struct lab_state_sync_save_context *ctx)
{
    return lab_flow_call(ctx->frame, 7, "stateIntervalCap", "cap", 0, 0);
}

static int lab_state_sync_save_state_records(struct lab_state_sync_save_context *ctx)
{
    int values = lab_dom_object(4);
    lab_dom_push(values, ctx->text);
    lab_dom_push(values, lab_dom_get(ctx->frame, "updatedAt"));
    lab_dom_push(values, ctx->index == 3 ? lab_dom_get(ctx->frame, "operation") : lab_dom_string(""));
    lab_dom_push(values, ctx->index == 3 ? lab_dom_get(ctx->frame, "operand") : lab_dom_string(""));
    lab_dom_push(values, ctx->index == 4 ? lab_dom_get(ctx->frame, "bounds") : lab_dom_string(""));
    lab_dom_push(values, ctx->index == 4 ? lab_dom_get(ctx->frame, "cap") : lab_dom_numeric(0));
    return lab_flow_call(ctx->frame, 8, "stateRecords", "records", 0, 2, lab_dom_numeric(ctx->index), values);
}

static int lab_state_sync_save_state_local_records(struct lab_state_sync_save_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(9));
    return lab_flow_call(ctx->frame, 9, "stateLocalRecords", 0, 0, 1,
                         lab_dom_get(lab_dom_get(ctx->frame, "records"), "local"));
}

static int lab_state_sync_save_state_cancel_save_timer(struct lab_state_sync_save_context *ctx)
{
    lab_dom_delete(ctx->frame, "failed");
    unsigned token = lab_persist_begin(ctx->index, lab_dom_truth(ctx->text));
    if (!token)
        return lab_flow_done(0);
    lab_dom_set(ctx->frame, "token", lab_dom_numeric(token));
    lab_dom_set(ctx->frame, "patch", lab_dom_get(lab_dom_get(ctx->frame, "records"), "server"));
    return lab_flow_call(ctx->frame, 10, "stateCancelSaveTimer", 0, 0, 1, lab_dom_numeric(ctx->index));
}

static int lab_state_sync_save_schedule(struct lab_state_sync_save_context *ctx)
{
    unsigned delay =
        lab_persist_delay(ctx->index, lab_dom_truth(lab_dom_get(lab_dom_get(ctx->frame, "options"), "debounce")));
    lab_dom_set(ctx->frame, "delay", lab_dom_numeric(delay));
    return lab_flow_call(ctx->frame, 11, delay ? "stateScheduleSave" : "statePublishSave", 0, 0, 1, ctx->frame);
}

static int lab_state_sync_save_otherwise(struct lab_state_sync_save_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static int lab_state_sync_save(int frame)
{
    unsigned index = lab_state_sync_index(frame);
    int text = lab_dom_get(frame, "text");
    struct lab_state_sync_save_context ctx = {
        .frame = frame,
        .index = index,
        .text = text,
    };
    static int (*const handlers[])(struct lab_state_sync_save_context *) = {
        [0]  = lab_state_sync_save_state_canonical,
        [1]  = lab_state_sync_save_state_now,
        [2]  = lab_state_sync_save_state_timestamp,
        [3]  = lab_state_sync_save_state_retain_text,
        [4]  = lab_state_sync_save_state_matrix_operation,
        [5]  = lab_state_sync_save_state_matrix_operand,
        [6]  = lab_state_sync_save_state_interval_cap,
        [7]  = lab_state_sync_save_state_records,
        [8]  = lab_state_sync_save_state_local_records,
        [9]  = lab_state_sync_save_state_cancel_save_timer,
        [10] = lab_state_sync_save_schedule,
    };
    unsigned selected = lab_state_sync_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_state_sync_save_otherwise(&ctx);
}

struct lab_state_sync_publish_context {
    int frame;
    int save;
    unsigned int index;
};

static int lab_state_sync_publish_state_forget_save_timer(struct lab_state_sync_publish_context *ctx)
{
    if (!lab_persist_take(ctx->index, (unsigned)lab_dom_number(lab_dom_get(ctx->save, "token"))))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, 1, "stateForgetSaveTimer", 0, 0, 1, lab_dom_numeric(ctx->index));
}

static int lab_state_sync_publish_state_save(struct lab_state_sync_publish_context *ctx)
{
    int options = lab_dom_object(5);
    int keepalive = !lab_dom_truth(lab_dom_get(ctx->save, "delay")) &&
                    lab_dom_truth(lab_dom_get(lab_dom_get(ctx->save, "options"), "keepalive"));
    lab_dom_set(options, "keepalive", lab_dom_scalar(1, keepalive));
    return lab_flow_call(ctx->frame, 2, "stateSave", 0, 0, 2, lab_dom_get(ctx->save, "patch"), options);
}

static int lab_state_sync_publish_otherwise(struct lab_state_sync_publish_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static int lab_state_sync_publish(int frame)
{
    int save = lab_dom_get(frame, "save");
    unsigned index = lab_state_sync_index(save);
    struct lab_state_sync_publish_context ctx = {
        .frame = frame,
        .save = save,
        .index = index,
    };
    static int (*const handlers[])(struct lab_state_sync_publish_context *) = {
        [0] = lab_state_sync_publish_state_forget_save_timer,
        [1] = lab_state_sync_publish_state_save,
    };
    unsigned selected = lab_state_sync_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_state_sync_publish_otherwise(&ctx);
}

struct lab_state_sync_calendar_context {
    int frame;
    int mode;
    int state;
    int datetime;
};

static int lab_state_sync_calendar_capture(struct lab_state_sync_calendar_context *ctx)
{
    return lab_flow_call(ctx->frame, 1, ctx->datetime ? "stateDatetime" : "stateAlmanac", "state", 0, 0);
}

static int lab_state_sync_calendar_state_default_editor(struct lab_state_sync_calendar_context *ctx)
{
    return lab_flow_call(ctx->frame, 2, "stateDefaultEditor", 0, 0, 2, ctx->mode,
                         lab_dom_numeric(ctx->datetime ? 5 : 6));
}

static int lab_state_sync_calendar_state_write_calendar(struct lab_state_sync_calendar_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(3));
    return lab_flow_call(
        ctx->frame, 3, "stateWriteCalendar", 0, 0, 2,
        lab_dom_string(ctx->datetime ? "mars.exprLab.lastDatetimeState" : "mars.exprLab.lastAlmanacState"), ctx->state);
}

static int lab_state_sync_calendar_state_calendar_patch(struct lab_state_sync_calendar_context *ctx)
{
    lab_dom_delete(ctx->frame, "failed");
    return lab_flow_call(ctx->frame, 4, "stateCalendarPatch", "patch", 0, 2, ctx->mode, ctx->state);
}

static int lab_state_sync_calendar_state_save(struct lab_state_sync_calendar_context *ctx)
{
    return lab_flow_call(ctx->frame, 5, "stateSave", 0, 0, 1, lab_dom_get(ctx->frame, "patch"));
}

static int lab_state_sync_calendar_otherwise(struct lab_state_sync_calendar_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static int lab_state_sync_calendar(int frame)
{
    int mode = lab_dom_get(frame, "mode"), state = lab_dom_get(frame, "state");
    int datetime = lab_dom_equal(mode, lab_dom_string("datetime"));
    struct lab_state_sync_calendar_context ctx = {
        .frame = frame,
        .mode = mode,
        .state = state,
        .datetime = datetime,
    };
    static int (*const handlers[])(struct lab_state_sync_calendar_context *) = {
        [0] = lab_state_sync_calendar_capture,
        [1] = lab_state_sync_calendar_state_default_editor,
        [2] = lab_state_sync_calendar_state_write_calendar,
        [3] = lab_state_sync_calendar_state_calendar_patch,
        [4] = lab_state_sync_calendar_state_save,
    };
    unsigned selected = lab_state_sync_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_state_sync_calendar_otherwise(&ctx);
}

static int lab_state_sync_mode_save_state_mode(int frame)
{
    return lab_flow_call(frame, 1, "stateMode", "selected", 0, 1, lab_dom_get(frame, "mode"));
}

static int lab_state_sync_mode_save_state_write_mode(int frame)
{
    lab_dom_set(frame, "failed", lab_dom_numeric(2));
    return lab_flow_call(frame, 2, "stateWriteMode", 0, 0, 1, lab_dom_get(frame, "selected"));
}

static int lab_state_sync_mode_save_state_save(int frame)
{
    lab_dom_delete(frame, "failed");
    int patch = lab_dom_object(5);
    lab_dom_set(patch, "lab_mode", lab_dom_get(frame, "selected"));
    return lab_flow_call(frame, 3, "stateSave", 0, 0, 1, patch);
}

static int lab_state_sync_mode_save_otherwise(int frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static int lab_state_sync_mode_save(int frame)
{
    static int (*const handlers[])(int) = {
        [0] = lab_state_sync_mode_save_state_mode,
        [1] = lab_state_sync_mode_save_state_write_mode,
        [2] = lab_state_sync_mode_save_state_save,
    };
    unsigned selected = lab_state_sync_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](frame)
               : lab_state_sync_mode_save_otherwise(frame);
}

struct lab_state_sync_editor_context {
    int frame;
    unsigned int index;
    int text;
};

static int lab_state_sync_editor_state_restored_text(struct lab_state_sync_editor_context *ctx)
{
    return lab_flow_call(ctx->frame, 1, "stateRestoredText", "text", 0, 1, lab_dom_numeric(ctx->index));
}

static int lab_state_sync_editor_state_binding_parts(struct lab_state_sync_editor_context *ctx)
{
    return lab_flow_call(ctx->frame, 2, "stateBindingParts", "bound", 0, 1, ctx->text);
}

static int lab_state_sync_editor_state_set_expression(struct lab_state_sync_editor_context *ctx)
{
    if (lab_workspace_editor_bound(ctx->index, lab_dom_truth(lab_dom_get(ctx->frame, "bound"))))
        return lab_flow_call(ctx->frame, 5, "stateSetExpression", 0, 0, 1, ctx->text);
    lab_dom_write(lab_dom_id("expr"), 5, "", ctx->text);
    return lab_state_sync_next(ctx->frame, 3, "stateClearSource");
}

static int lab_state_sync_editor_state_clear_bindings(struct lab_state_sync_editor_context *ctx)
{
    return lab_state_sync_next(ctx->frame, 5, "stateClearBindings");
}

static int lab_state_sync_editor_otherwise(struct lab_state_sync_editor_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static int lab_state_sync_editor(int frame)
{
    unsigned index = lab_state_sync_index(frame);
    int text = lab_dom_get(frame, "text");
    struct lab_state_sync_editor_context ctx = {
        .frame = frame,
        .index = index,
        .text = text,
    };
    static int (*const handlers[])(struct lab_state_sync_editor_context *) = {
        [0] = lab_state_sync_editor_state_restored_text,
        [1] = lab_state_sync_editor_state_binding_parts,
        [2] = lab_state_sync_editor_state_set_expression,
        [3] = lab_state_sync_editor_state_clear_bindings,
    };
    unsigned selected = lab_state_sync_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_state_sync_editor_otherwise(&ctx);
}

static int lab_state_sync_result_state_clear_function(int frame)
{
    return lab_state_sync_next(frame, 1, "stateClearFunction");
}

static int lab_state_sync_result_state_result_projection(int frame)
{
    return lab_flow_call(frame, 2, "stateResultProjection", "result", 0, 1, lab_dom_get(frame, "mode"));
}

static int lab_state_sync_result_state_result_values(int frame)
{
    if (!lab_dom_truth(lab_dom_get(frame, "result")))
        return lab_state_sync_next(frame, 6, "stateClearResult");
    return lab_flow_call(frame, 3, "stateResultValues", 0, 0, 1, lab_dom_get(frame, "result"));
}

static int lab_state_sync_result_state_derivative_buttons(int frame)
{
    return lab_state_sync_next(frame, 4, "stateDerivativeButtons");
}

static int lab_state_sync_result_state_rendered_fit(int frame)
{
    return lab_state_sync_next(frame, 5, "stateRenderedFit");
}

static int lab_state_sync_result_state_solver_fit(int frame)
{
    return lab_state_sync_next(frame, 6, "stateSolverFit");
}

static int lab_state_sync_result_otherwise(int frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static int lab_state_sync_result(int frame)
{
    static int (*const handlers[])(int) = {
        [0] = lab_state_sync_result_state_clear_function,
        [1] = lab_state_sync_result_state_result_projection,
        [2] = lab_state_sync_result_state_result_values,
        [3] = lab_state_sync_result_state_derivative_buttons,
        [4] = lab_state_sync_result_state_rendered_fit,
        [5] = lab_state_sync_result_state_solver_fit,
    };
    unsigned selected = lab_state_sync_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](frame)
                                                                               : lab_state_sync_result_otherwise(frame);
}

struct lab_state_sync_select_context {
    int frame;
    int changed;
};

static int lab_state_sync_select_state_sync_tabs(struct lab_state_sync_select_context *ctx);

static int lab_state_sync_select_state_notify_mode(struct lab_state_sync_select_context *ctx)
{
    ctx->changed = lab_workspace_select(lab_state_sync_index(ctx->frame));
    lab_dom_set(ctx->frame, "changed", lab_dom_scalar(1, ctx->changed));
    if (ctx->changed)
        return lab_state_sync_next(ctx->frame, 1, "stateNotifyMode");
    return lab_state_sync_select_state_sync_tabs(ctx);
}

static int lab_state_sync_select_state_reset_editor_size(struct lab_state_sync_select_context *ctx)
{
    return lab_state_sync_next(ctx->frame, 2, "stateResetEditorSize");
}

static int lab_state_sync_select_state_sync_tabs(struct lab_state_sync_select_context *ctx)
{
    return lab_state_sync_next(ctx->frame, 3, "stateSyncTabs");
}

static int lab_state_sync_select_otherwise(struct lab_state_sync_select_context *ctx)
{
    return lab_flow_done(
        lab_dom_scalar(1, ctx->changed || lab_dom_truth(lab_dom_get(lab_dom_get(ctx->frame, "options"), "force"))));
}

static int lab_state_sync_select(int frame)
{
    int changed = lab_dom_truth(lab_dom_get(frame, "changed"));
    struct lab_state_sync_select_context ctx = {
        .frame = frame,
        .changed = changed,
    };
    static int (*const handlers[])(struct lab_state_sync_select_context *) = {
        [0] = lab_state_sync_select_state_notify_mode,
        [1] = lab_state_sync_select_state_reset_editor_size,
        [2] = lab_state_sync_select_state_sync_tabs,
    };
    unsigned selected = lab_state_sync_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_state_sync_select_otherwise(&ctx);
}

struct lab_state_sync_apply_context {
    int frame;
    unsigned int index;
};

static int lab_state_sync_apply_state_mode_index_4(struct lab_state_sync_apply_context *ctx);
static int lab_state_sync_apply_state_mode_index_7(struct lab_state_sync_apply_context *ctx);
static int lab_state_sync_apply_state_sync_ui(struct lab_state_sync_apply_context *ctx);
static int lab_state_sync_apply_state_mode_index_14(struct lab_state_sync_apply_context *ctx);
static int lab_state_sync_apply_otherwise(struct lab_state_sync_apply_context *ctx);

static int lab_state_sync_apply_state_force_mode(struct lab_state_sync_apply_context *ctx)
{
    return lab_flow_call(ctx->frame, 1, "stateForceMode", 0, 0, 1, lab_dom_get(ctx->frame, "mode"));
}

static int lab_state_sync_apply_state_restore_current_editor(struct lab_state_sync_apply_context *ctx)
{
    return lab_state_sync_next(ctx->frame, 2, "stateRestoreCurrentEditor");
}

static int lab_state_sync_apply_state_mode_index(struct lab_state_sync_apply_context *ctx)
{
    return lab_flow_call(ctx->frame, 3, "stateModeIndex", "index", 0, 0);
}

static int lab_state_sync_apply_state_render_active_rows(struct lab_state_sync_apply_context *ctx)
{
    if (ctx->index == 4)
        return lab_state_sync_next(ctx->frame, 4, "stateRenderActiveRows");
    /* Continue to the next mode-dependent operation. */
    return lab_state_sync_apply_state_mode_index_4(ctx);
}

static int lab_state_sync_apply_state_mode_index_4(struct lab_state_sync_apply_context *ctx)
{
    return lab_flow_call(ctx->frame, 5, "stateModeIndex", "index", 0, 0);
}

static int lab_state_sync_apply_state_datetime_defaults(struct lab_state_sync_apply_context *ctx)
{
    if (ctx->index == 5)
        return lab_state_sync_next(ctx->frame, 6, "stateDatetimeDefaults");
    return lab_state_sync_apply_state_mode_index_7(ctx);
}

static int lab_state_sync_apply_state_refresh_location(struct lab_state_sync_apply_context *ctx)
{
    return lab_state_sync_next(ctx->frame, 7, "stateRefreshLocation");
}

static int lab_state_sync_apply_state_mode_index_7(struct lab_state_sync_apply_context *ctx)
{
    return lab_flow_call(ctx->frame, 8, "stateModeIndex", "index", 0, 0);
}

static int lab_state_sync_apply_state_almanac_defaults(struct lab_state_sync_apply_context *ctx)
{
    if (ctx->index == 6)
        return lab_state_sync_next(ctx->frame, 9, "stateAlmanacDefaults");
    return lab_state_sync_apply_state_sync_ui(ctx);
}

static int lab_state_sync_apply_state_save_almanac(struct lab_state_sync_apply_context *ctx)
{
    return lab_state_sync_next(ctx->frame, 10, "stateSaveAlmanac");
}

static int lab_state_sync_apply_state_sync_ui(struct lab_state_sync_apply_context *ctx)
{
    return lab_state_sync_next(ctx->frame, 11, "stateSyncUI");
}

static int lab_state_sync_apply_state_mode_index_11(struct lab_state_sync_apply_context *ctx)
{
    return lab_flow_call(ctx->frame, 12, "stateModeIndex", "index", 0, 0);
}

static int lab_state_sync_apply_state_bound_count(struct lab_state_sync_apply_context *ctx)
{
    if (ctx->index == 4)
        return lab_flow_call(ctx->frame, 13, "stateBoundCount", "count", 0, 0);
    return lab_state_sync_apply_state_mode_index_14(ctx);
}

static int lab_state_sync_apply_state_reset_bounds(struct lab_state_sync_apply_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "count")))
        return lab_state_sync_next(ctx->frame, 14, "stateResetBounds");
    /* Continue: intentionally do not await the legacy default-bounds refresh. */
    return lab_state_sync_apply_state_mode_index_14(ctx);
}

static int lab_state_sync_apply_state_mode_index_14(struct lab_state_sync_apply_context *ctx)
{
    return lab_flow_call(ctx->frame, 15, "stateModeIndex", "index", 0, 0);
}

static int lab_state_sync_apply_state_normalise_cap(struct lab_state_sync_apply_context *ctx)
{
    if (ctx->index == 4)
        return lab_state_sync_next(ctx->frame, 16, "stateNormaliseCap");
    /* Continue when no integrator control is present. */
    return lab_state_sync_apply_otherwise(ctx);
}

static int lab_state_sync_apply_otherwise(struct lab_state_sync_apply_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static int lab_state_sync_apply(int frame)
{
    unsigned index = lab_state_sync_index(frame);
    struct lab_state_sync_apply_context ctx = {
        .frame = frame,
        .index = index,
    };
    static int (*const handlers[])(struct lab_state_sync_apply_context *) = {
        [0]  = lab_state_sync_apply_state_force_mode,
        [1]  = lab_state_sync_apply_state_restore_current_editor,
        [2]  = lab_state_sync_apply_state_mode_index,
        [3]  = lab_state_sync_apply_state_render_active_rows,
        [4]  = lab_state_sync_apply_state_mode_index_4,
        [5]  = lab_state_sync_apply_state_datetime_defaults,
        [6]  = lab_state_sync_apply_state_refresh_location,
        [7]  = lab_state_sync_apply_state_mode_index_7,
        [8]  = lab_state_sync_apply_state_almanac_defaults,
        [9]  = lab_state_sync_apply_state_save_almanac,
        [10] = lab_state_sync_apply_state_sync_ui,
        [11] = lab_state_sync_apply_state_mode_index_11,
        [12] = lab_state_sync_apply_state_bound_count,
        [13] = lab_state_sync_apply_state_reset_bounds,
        [14] = lab_state_sync_apply_state_mode_index_14,
        [15] = lab_state_sync_apply_state_normalise_cap,
    };
    unsigned selected = lab_state_sync_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_state_sync_apply_otherwise(&ctx);
}

static int lab_state_sync_keyboard_state_ready(int frame)
{
    return lab_flow_call(frame, 1, "stateReady", "ready", 0, 0);
}

static int lab_state_sync_keyboard_state_history_buttons(int frame)
{
    if (!lab_dom_truth(lab_dom_get(frame, "ready")))
        return lab_state_sync_next(frame, 5, "stateHistoryButtons");
    return lab_state_sync_next(frame, 2, "stateClearForward");
}

static int lab_state_sync_keyboard_state_mode_index(int frame)
{
    return lab_flow_call(frame, 3, "stateModeIndex", "index", 0, 0);
}

static int lab_state_sync_keyboard_evaluate(int frame)
{
    static const char *const services[] = {
        "stateEvaluateExpression", "stateEvaluateEquation", "stateEvaluateDiffequation", "stateEvaluateMatrix",
        "stateEvaluateIntegrator", "stateEvaluateDatetime", "stateEvaluateExpression"};
    unsigned index = lab_state_sync_index(frame);
    return lab_state_sync_next(frame, 5, services[index < 7 ? index : 0]);
}

static int lab_state_sync_keyboard_otherwise(int frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static int lab_state_sync_keyboard(int frame)
{
    static int (*const handlers[])(int) = {
        [0] = lab_state_sync_keyboard_state_ready,
        [1] = lab_state_sync_keyboard_state_history_buttons,
        [2] = lab_state_sync_keyboard_state_mode_index,
        [3] = lab_state_sync_keyboard_evaluate,
    };
    unsigned selected = lab_state_sync_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](frame)
               : lab_state_sync_keyboard_otherwise(frame);
}

struct lab_state_sync_history_context {
    int frame;
    unsigned int index;
};

static int lab_state_sync_history_capture_text(struct lab_state_sync_history_context *ctx);
static int lab_state_sync_history_state_current_bounds(struct lab_state_sync_history_context *ctx);
static int lab_state_sync_history_state_history_record(struct lab_state_sync_history_context *ctx);

static int lab_state_sync_history_capture_calendar(struct lab_state_sync_history_context *ctx)
{
    if (ctx->index == 5 || ctx->index == 6)
        return lab_flow_call(ctx->frame, 1, ctx->index == 5 ? "stateDatetime" : "stateAlmanac", "calendar", 0, 0);
    /* Continue for mathematical worksheets. */
    return lab_state_sync_history_capture_text(ctx);
}

static int lab_state_sync_history_capture_text(struct lab_state_sync_history_context *ctx)
{
    if (lab_dom_type(lab_dom_get(ctx->frame, "textOverride"))) {
        lab_dom_set(ctx->frame, "text", lab_dom_get(ctx->frame, "textOverride"));
        return lab_state_sync_history_state_current_bounds(ctx);
    }
    return lab_flow_call(ctx->frame, 2,
                         ctx->index == 5   ? "stateDatetimeSummary"
                         : ctx->index == 6 ? "stateAlmanacSummary"
                                           : "stateEditorText",
                         "text", 0, 1, lab_dom_get(ctx->frame, "calendar"));
}

static int lab_state_sync_history_state_current_bounds(struct lab_state_sync_history_context *ctx)
{
    if (ctx->index == 4)
        return lab_flow_call(ctx->frame, 3, "stateCurrentBounds", "bounds", 0, 0);
    return lab_state_sync_history_state_history_record(ctx);
}

static int lab_state_sync_history_state_history_cap(struct lab_state_sync_history_context *ctx)
{
    return lab_flow_call(ctx->frame, 4, "stateHistoryCap", "cap", 0, 0);
}

static int lab_state_sync_history_state_history_record(struct lab_state_sync_history_context *ctx)
{
    return lab_flow_call(ctx->frame, 5, "stateHistoryRecord", "record", 0, 5, lab_dom_numeric(ctx->index),
                         lab_dom_get(ctx->frame, "text"), lab_dom_get(ctx->frame, "calendar"),
                         ctx->index == 4 ? lab_dom_get(ctx->frame, "bounds") : lab_dom_string(""),
                         ctx->index == 4 ? lab_dom_get(ctx->frame, "cap") : lab_dom_string(""));
}

static int lab_state_sync_history_otherwise(struct lab_state_sync_history_context *ctx)
{
    return lab_flow_done(lab_dom_get(ctx->frame, "record"));
}

static int lab_state_sync_history(int frame)
{
    unsigned index = lab_state_sync_index(frame);
    struct lab_state_sync_history_context ctx = {
        .frame = frame,
        .index = index,
    };
    static int (*const handlers[])(struct lab_state_sync_history_context *) = {
        [0] = lab_state_sync_history_capture_calendar,
        [1] = lab_state_sync_history_capture_text,
        [2] = lab_state_sync_history_state_current_bounds,
        [3] = lab_state_sync_history_state_history_cap,
        [4] = lab_state_sync_history_state_history_record,
    };
    unsigned selected = lab_state_sync_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_state_sync_history_otherwise(&ctx);
}

struct lab_state_sync_push_context {
    int frame;
    int entry;
};

static int lab_state_sync_push_state_push_snapshot(struct lab_state_sync_push_context *ctx);

static int lab_state_sync_push_state_current_history(struct lab_state_sync_push_context *ctx)
{
    if (lab_dom_type(ctx->entry) == 3)
        return lab_flow_call(ctx->frame, 1, "stateCurrentHistory", "snapshot", 0, 1, ctx->entry);
    if (!lab_dom_truth(ctx->entry))
        return lab_flow_call(ctx->frame, 1, "stateCurrentHistory", "snapshot", 0, 0);
    lab_dom_set(ctx->frame, "snapshot", ctx->entry);
    /* Continue with the supplied opaque snapshot. */
    return lab_state_sync_push_state_push_snapshot(ctx);
}

static int lab_state_sync_push_state_push_snapshot(struct lab_state_sync_push_context *ctx)
{
    return lab_flow_call(ctx->frame, 2, "statePushSnapshot", 0, 0, 1, lab_dom_get(ctx->frame, "snapshot"));
}

static int lab_state_sync_push_state_history_buttons(struct lab_state_sync_push_context *ctx)
{
    return lab_state_sync_next(ctx->frame, 3, "stateHistoryButtons");
}

static int lab_state_sync_push_otherwise(struct lab_state_sync_push_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static int lab_state_sync_push(int frame)
{
    int entry = lab_dom_get(frame, "entry");
    struct lab_state_sync_push_context ctx = {
        .frame = frame,
        .entry = entry,
    };
    static int (*const handlers[])(struct lab_state_sync_push_context *) = {
        [0] = lab_state_sync_push_state_current_history,
        [1] = lab_state_sync_push_state_push_snapshot,
        [2] = lab_state_sync_push_state_history_buttons,
    };
    unsigned selected = lab_state_sync_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_state_sync_push_otherwise(&ctx);
}

static const char *const lab_state_sync_control_modes[] = {"matrix", "equation", "integrator"};

struct lab_state_sync_control_context {
    int frame;
    unsigned int index;
    int node;
};

static int lab_state_sync_control_state_sync_matrix(struct lab_state_sync_control_context *ctx);
static int lab_state_sync_control_current_mode(struct lab_state_sync_control_context *ctx);
static int lab_state_sync_control_otherwise(struct lab_state_sync_control_context *ctx);

static int lab_state_sync_control_validate(struct lab_state_sync_control_context *ctx)
{
    {
        if (!ctx->node)
            return lab_flow_done(0);
        int value = lab_dom_read(ctx->node, 5, "");
        if (ctx->index != 1)
            return lab_flow_call(ctx->frame, 1, ctx->index ? "stateValidCap" : "stateValidOperation", "value", 0, 1,
                                 value);
        int fallback = lab_dom_get(lab_dom_get(ctx->frame, "config"), "DEFAULT_EQUATION_VARIABLE");
        value = lab_dom_clean(lab_dom_text(lab_state_sync_or(value, fallback)), 0);
        lab_dom_set(ctx->frame, "value", lab_state_sync_or(value, fallback));
        /* Continue after native equation-variable normalisation. */
    }
    return lab_state_sync_control_state_sync_matrix(ctx);
}

static int lab_state_sync_control_state_sync_matrix(struct lab_state_sync_control_context *ctx)
{
    lab_dom_write(ctx->node, 5, "", lab_dom_get(ctx->frame, "value"));
    if (!ctx->index)
        return lab_state_sync_next(ctx->frame, 2, "stateSyncMatrix");
    /* Continue: only matrix operation changes need matrix UI synchronisation. */
    return lab_state_sync_control_current_mode(ctx);
}

static int lab_state_sync_control_current_mode(struct lab_state_sync_control_context *ctx)
{
    return lab_flow_call(ctx->frame, 3, "currentMode", "mode", 0, 0);
}

static int lab_state_sync_control_state_save_worksheet(struct lab_state_sync_control_context *ctx)
{
    if (lab_dom_equal(lab_dom_get(ctx->frame, "mode"), lab_dom_string(lab_state_sync_control_modes[ctx->index])))
        return lab_flow_call(ctx->frame, 4, "stateSaveWorksheet", 0, 0, 1,
                             lab_dom_string(lab_state_sync_control_modes[ctx->index]));
    /* Continue when changing an inactive control. */
    return lab_state_sync_control_otherwise(ctx);
}

static int lab_state_sync_control_otherwise(struct lab_state_sync_control_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static int lab_state_sync_control(int frame)
{
    unsigned index = lab_state_sync_index(frame);

    if (index >= 3)
        return lab_flow_done(0);
    int node = lab_dom_item(lab_dom_get(frame, "controls"), index);
    struct lab_state_sync_control_context ctx = {
        .frame = frame,
        .index = index,
        .node = node,
    };
    static int (*const handlers[])(struct lab_state_sync_control_context *) = {
        [0] = lab_state_sync_control_validate,
        [1] = lab_state_sync_control_state_sync_matrix,
        [2] = lab_state_sync_control_current_mode,
        [3] = lab_state_sync_control_state_save_worksheet,
    };
    unsigned selected = lab_state_sync_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_state_sync_control_otherwise(&ctx);
}

static int lab_state_sync_flush_otherwise(int frame);

static int lab_state_sync_flush_current_mode(int frame)
{
    return lab_flow_call(frame, 1, "currentMode", "mode", 0, 0);
}

static int lab_state_sync_flush_state_named_index(int frame)
{
    return lab_flow_call(frame, 2, "stateNamedIndex", "index", 0, 1, lab_dom_get(frame, "mode"));
}

static int lab_state_sync_flush_state_flush_save(int frame)
{
    if (lab_events_flush(lab_state_sync_index(frame)))
        return lab_flow_call(frame, 3, "stateFlushSave", 0, 0, 1, lab_dom_get(frame, "mode"));
    /* Continue for calendars. */
    return lab_state_sync_flush_otherwise(frame);
}

static int lab_state_sync_flush_otherwise(int frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static int lab_state_sync_flush(int frame)
{
    static int (*const handlers[])(int) = {
        [0] = lab_state_sync_flush_current_mode,
        [1] = lab_state_sync_flush_state_named_index,
        [2] = lab_state_sync_flush_state_flush_save,
    };
    unsigned selected = lab_state_sync_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](frame)
                                                                               : lab_state_sync_flush_otherwise(frame);
}

static int lab_state_sync_location_otherwise(int frame);

static int lab_state_sync_location_state_mode_index(int frame)
{
    return lab_flow_call(frame, 1, "stateModeIndex", "index", 0, 0);
}

static int lab_state_sync_location_state_save_datetime(int frame)
{
    if (lab_state_sync_index(frame) == 5)
        return lab_state_sync_next(frame, 2, "stateSaveDatetime");
    /* Continue: a later worksheet owns the visible state. */
    return lab_state_sync_location_otherwise(frame);
}

static int lab_state_sync_location_otherwise(int frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static int lab_state_sync_location(int frame)
{
    static int (*const handlers[])(int) = {
        [0] = lab_state_sync_location_state_mode_index,
        [1] = lab_state_sync_location_state_save_datetime,
    };
    unsigned selected = lab_state_sync_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](frame)
               : lab_state_sync_location_otherwise(frame);
}

struct lab_state_sync_integrator_result_context {
    int frame;
    int rows;
};

static int lab_state_sync_integrator_result_otherwise(struct lab_state_sync_integrator_result_context *ctx);

static int lab_state_sync_integrator_result_state_merge_rows(struct lab_state_sync_integrator_result_context *ctx)
{
    return lab_flow_call(ctx->frame, 1, "stateMergeRows", "rows", 0, 1, lab_dom_get(ctx->frame, "data"));
}

static int lab_state_sync_integrator_result_fail(struct lab_state_sync_integrator_result_context *ctx)
{
    if (lab_dom_type(ctx->rows) == 1 && !lab_dom_truth(ctx->rows))
        return lab_flow_call(ctx->frame, 2, "fail", 0, 0, 1,
                             lab_dom_string("Integrator result exceeds the 256-row form limit"));
    if (lab_dom_truth(ctx->rows))
        return lab_flow_call(ctx->frame, 2, "stateRenderRows", 0, 0, 1, ctx->rows);
    /* Continue for an absent row update. */
    return lab_state_sync_integrator_result_otherwise(ctx);
}

static int lab_state_sync_integrator_result_otherwise(struct lab_state_sync_integrator_result_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static int lab_state_sync_integrator_result(int frame)
{
    int rows = lab_dom_get(frame, "rows");
    struct lab_state_sync_integrator_result_context ctx = {
        .frame = frame,
        .rows = rows,
    };
    static int (*const handlers[])(struct lab_state_sync_integrator_result_context *) = {
        [0] = lab_state_sync_integrator_result_state_merge_rows,
        [1] = lab_state_sync_integrator_result_fail,
    };
    unsigned selected = lab_state_sync_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_state_sync_integrator_result_otherwise(&ctx);
}

struct lab_state_sync_capture_save_context {
    int frame;
    unsigned int index;
};

static int lab_state_sync_capture_save_state_expression_saved_text(struct lab_state_sync_capture_save_context *ctx)
{
    if (!ctx->index)
        return lab_flow_call(ctx->frame, 1, "stateExpressionSavedText", "text", 0, 0);
    return lab_flow_call(ctx->frame, 2, "stateSaveNamedEditor", 0, 0, 1, lab_dom_numeric(ctx->index));
}

static int lab_state_sync_capture_save_state_save_worksheet(struct lab_state_sync_capture_save_context *ctx)
{
    return lab_flow_call(ctx->frame, 2, "stateSaveWorksheet", 0, 0, 2, lab_dom_string("expression"),
                         lab_dom_get(ctx->frame, "text"));
}

static int lab_state_sync_capture_save_otherwise(struct lab_state_sync_capture_save_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static int lab_state_sync_capture_save(int frame)
{
    unsigned index = lab_state_sync_index(frame);
    struct lab_state_sync_capture_save_context ctx = {
        .frame = frame,
        .index = index,
    };
    static int (*const handlers[])(struct lab_state_sync_capture_save_context *) = {
        [0] = lab_state_sync_capture_save_state_expression_saved_text,
        [1] = lab_state_sync_capture_save_state_save_worksheet,
    };
    unsigned selected = lab_state_sync_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_state_sync_capture_save_otherwise(&ctx);
}

static int lab_state_sync_needs_bounds_state_mode_index(int frame)
{
    return lab_flow_call(frame, 1, "stateModeIndex", "index", 0, 0);
}

static int lab_state_sync_needs_bounds_state_bound_count(int frame)
{
    if (lab_state_sync_index(frame) != 4)
        return lab_flow_done(lab_dom_scalar(1, 0));
    return lab_flow_call(frame, 2, "stateBoundCount", "count", 0, 0);
}

static int lab_state_sync_needs_bounds_otherwise(int frame)
{
    return lab_flow_done(lab_dom_scalar(1, lab_dom_number(lab_dom_get(frame, "count")) == 0));
}

static int lab_state_sync_needs_bounds(int frame)
{
    static int (*const handlers[])(int) = {
        [0] = lab_state_sync_needs_bounds_state_mode_index,
        [1] = lab_state_sync_needs_bounds_state_bound_count,
    };
    unsigned selected = lab_state_sync_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](frame)
               : lab_state_sync_needs_bounds_otherwise(frame);
}

/* Browser callbacks retain resources, while this fixed dispatch owns synchronous decisions. */
void lab_state_sync(unsigned kind, int frame)
{
    static int (*const handlers[])(int) = {
        lab_state_sync_save,         lab_state_sync_publish,     lab_state_sync_calendar,
        lab_state_sync_mode_save,    lab_state_sync_editor,      lab_state_sync_result,
        lab_state_sync_select,       lab_state_sync_apply,       lab_state_sync_keyboard,
        lab_state_sync_history,      lab_state_sync_push,        lab_state_sync_control,
        lab_state_sync_flush,        lab_state_sync_location,    lab_state_sync_integrator_result,
        lab_state_sync_capture_save, lab_state_sync_needs_bounds};
    lab_dom_return(kind < sizeof handlers / sizeof *handlers ? handlers[kind](frame) : lab_flow_done(0));
}
