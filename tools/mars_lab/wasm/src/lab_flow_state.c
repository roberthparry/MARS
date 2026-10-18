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
#include "lab_events.h"
#include "lab_persist.h"
#include "lab_flow.h"
#include "lab_flow_state.h"

static unsigned lab_flow_state_stage(frame_jsref frame)
{
    return (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
}

static browser_jsref lab_flow_state_or(browser_jsref value, browser_jsref fallback)
{
    return lab_dom_truth(value) ? value : fallback;
}

static browser_jsref lab_flow_state_next(frame_jsref frame, unsigned next, const char *service)
{
    return lab_flow_call(frame, next, service, 0, 0, 0);
}

static browser_jsref lab_flow_state_guard(frame_jsref frame, unsigned next)
{
    return lab_flow_call(frame, next, "stateGuard", "current", 0, 1, lab_dom_get(frame, "isCurrent"));
}

static browser_jsref lab_flow_state_false(void)
{
    return lab_flow_done(lab_dom_scalar(LAB_VALUE_BOOLEAN, 0));
}

struct lab_flow_state_editors_context {
    frame_jsref frame;
    view_jsref view;
    unsigned int index;
    lab_persist_origin_t origin;
    browser_jsref text;
    unsigned int flags;
};

/**
 * @brief Advance restoration of saved text for the five editor modes.
 * @param frame Borrowed restoration frame retaining stage, editor index, storage reader, local flag and prepared text.
 * @param view Borrowed view handle carried through the shared workflow signature; not inspected here.
 * @return Scoped operation plan for the current editor, or a completion plan after restoration.
 */
static browser_jsref lab_flow_state_editors(frame_jsref frame, view_jsref view);

/**
 * @brief Read the current editor's saved text from the selected storage source.
 * @param ctx Restoration state with editor index, local/server selection and the frame's storage reader.
 * @return Scoped storage-read plan, or a completion plan once all five editors have been visited.
 */
static browser_jsref lab_flow_state_editors_state_read(struct lab_flow_state_editors_context *ctx);

/**
 * @brief Canonicalise restored text when required before projecting it into an editor.
 * @param ctx Restoration state carrying text, canonicalisation flags and the frame receiving canonical text.
 * @return Scoped canonicalisation or editor-projection plan.
 */
static browser_jsref lab_flow_state_editors_state_canonical(struct lab_flow_state_editors_context *ctx);

/**
 * @brief Install the restored canonical text in its indexed editor.
 * @param ctx Restoration state with the editor index and the frame's canonical text handle.
 * @return Scoped editor-projection plan resuming at stage 5.
 */
static browser_jsref lab_flow_state_editors_state_editor(struct lab_flow_state_editors_context *ctx);

/**
 * @brief Restore expression source text and its saved timestamp.
 * @param ctx Restoration state carrying source text and a frame containing the optional timestamp.
 * @return Scoped expression-state plan using zero for an absent timestamp.
 */
static browser_jsref lab_flow_state_editors_state_expression(struct lab_flow_state_editors_context *ctx);

/**
 * @brief Restore integrator source when flagged, otherwise move to the next editor.
 * @param ctx Restoration state with source text, integrator flag and current editor index.
 * @return Scoped integrator-source plan or the next editor's read/completion plan.
 */
static browser_jsref lab_flow_state_editors_state_integrator_source(struct lab_flow_state_editors_context *ctx);

static browser_jsref lab_flow_state_editors_advance(struct lab_flow_state_editors_context *ctx)
{
    lab_dom_set(ctx->frame, "index", lab_dom_numeric(ctx->index + 1));
    lab_dom_set(ctx->frame, "stage", lab_dom_numeric(1));
    return lab_flow_state_editors(ctx->frame, ctx->view);
}

static browser_jsref lab_flow_state_editors_begin(struct lab_flow_state_editors_context *ctx)
{
    ctx->index = 0;
    lab_dom_set(ctx->frame, "index", lab_dom_numeric(ctx->index));
    /* Continue to the first lazy storage read. */
    return lab_flow_state_editors_state_read(ctx);
}

static browser_jsref lab_flow_state_editors_state_read(struct lab_flow_state_editors_context *ctx)
{
    if (ctx->index >= 5)
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, 2, "stateRead", "raw", 0, 2, lab_dom_get(ctx->frame, "read"),
                         lab_dom_string(lab_persist_text(ctx->index, 0, ctx->origin)));
}

static browser_jsref lab_flow_state_editors_state_prepare(struct lab_flow_state_editors_context *ctx)
{
    ctx->text = lab_dom_text(lab_flow_state_or(lab_dom_get(ctx->frame, "raw"), lab_dom_string("")));
    if (ctx->origin == LAB_PERSIST_SERVER)
        ctx->text = lab_dom_clean(ctx->text, LAB_DOM_CLEAN_TRIM);
    ctx->flags = lab_persist_restore(ctx->index, lab_dom_truth(ctx->text),
                                     lab_dom_contains(ctx->text, lab_dom_string("...")), ctx->origin);
    lab_dom_set(ctx->frame, "text", ctx->text);
    lab_dom_set(ctx->frame, "flags", lab_dom_numeric(ctx->flags));
    if (!(ctx->flags & 1))
        return lab_flow_state_editors_advance(ctx);
    if (ctx->flags & 2)
        return lab_flow_call(ctx->frame, 3, "statePrepare", 0, 1, 1, ctx->text);
    /* Continue when no metadata preparation is needed. */
    return lab_flow_state_editors_state_canonical(ctx);
}

static browser_jsref lab_flow_state_editors_state_canonical(struct lab_flow_state_editors_context *ctx)
{
    if (ctx->flags & 4)
        return lab_flow_call(ctx->frame, 4, "stateCanonical", "canonical", 0, 1, ctx->text);
    lab_dom_set(ctx->frame, "canonical", ctx->text);
    /* Continue to the editor projection. */
    return lab_flow_state_editors_state_editor(ctx);
}

static browser_jsref lab_flow_state_editors_state_editor(struct lab_flow_state_editors_context *ctx)
{
    return lab_flow_call(ctx->frame, 5, "stateEditor", 0, 0, 2, lab_dom_numeric(ctx->index),
                         lab_dom_get(ctx->frame, "canonical"));
}

static browser_jsref lab_flow_state_editors_state_read_5(struct lab_flow_state_editors_context *ctx)
{
    if (ctx->flags & 8)
        return lab_flow_call(ctx->frame, 6, "stateRead", "timestamp", 0, 2, lab_dom_get(ctx->frame, "read"),
                             lab_dom_string(lab_persist_text(ctx->index, 1, ctx->origin)));
    return lab_flow_state_editors_state_integrator_source(ctx);
}

static browser_jsref lab_flow_state_editors_state_now(struct lab_flow_state_editors_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "timestamp")) && ctx->origin != LAB_PERSIST_SERVER)
        return lab_flow_call(ctx->frame, 7, "stateNow", "timestamp", 0, 0);
    /* Continue: Number conversion retains NaN for malformed saved timestamps. */
    return lab_flow_state_editors_state_expression(ctx);
}

static browser_jsref lab_flow_state_editors_state_expression(struct lab_flow_state_editors_context *ctx)
{
    return lab_flow_call(ctx->frame, 8, "stateExpression", 0, 0, 2, ctx->text,
                         lab_dom_numeric(lab_dom_to_number(
                             lab_flow_state_or(lab_dom_get(ctx->frame, "timestamp"), lab_dom_numeric(0)))));
}

static browser_jsref lab_flow_state_editors_state_integrator_source(struct lab_flow_state_editors_context *ctx)
{
    if (ctx->flags & 16)
        return lab_flow_call(ctx->frame, 9, "stateIntegratorSource", 0, 0, 1, ctx->text);
    return lab_flow_state_editors_advance(ctx);
}

static browser_jsref lab_flow_state_editors_next(struct lab_flow_state_editors_context *ctx)
{
    return lab_flow_state_editors_advance(ctx);
}

static browser_jsref lab_flow_state_editors_otherwise(struct lab_flow_state_editors_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static browser_jsref lab_flow_state_editors(frame_jsref frame, view_jsref view)
{
    (void)view;
    unsigned stage = lab_flow_state_stage(frame), index = (unsigned)lab_dom_number(lab_dom_get(frame, "index"));
    lab_persist_origin_t origin = (lab_persist_origin_t)lab_dom_truth(lab_dom_get(frame, "local"));
    int text = lab_dom_get(frame, "text");
    unsigned flags = (unsigned)lab_dom_number(lab_dom_get(frame, "flags"));
    struct lab_flow_state_editors_context ctx = {
        .frame = frame,
        .view = view,
        .index = index,
        .origin = origin,
        .text = text,
        .flags = flags,
    };
    static browser_jsref (*const handlers[])(struct lab_flow_state_editors_context *) = {
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
    frame_jsref frame;
    view_jsref view;
    unsigned int index;
    browser_jsref local;
};

/**
 * @brief Advance restoration of saved controls and integrator bounds.
 * @param frame Borrowed restoration frame retaining stage, field index, storage reader and local/server selection.
 * @param view Borrowed view handle carried through the shared workflow signature; not inspected here.
 * @return Scoped control-restoration operation or completion plan.
 */
static browser_jsref lab_flow_state_controls(frame_jsref frame, view_jsref view);

/**
 * @brief Increment the saved-control index and resume its storage read.
 * @param ctx Control-restoration state whose frame index and stage are updated before redispatch.
 * @return Scoped read plan for the next field, or a completion plan when all fields are restored.
 */
static browser_jsref lab_flow_state_controls_next(struct lab_flow_state_controls_context *ctx);

static browser_jsref lab_flow_state_controls_native(struct lab_flow_state_controls_context *ctx)
{
    lab_dom_set(ctx->frame, "index", lab_dom_numeric(0));
    return lab_flow_call(ctx->frame, 1, "native", "fields", 0, 2, lab_dom_string("lab_storage_fields"), ctx->local);
}

static browser_jsref lab_flow_state_controls_state_read(struct lab_flow_state_controls_context *ctx)
{
    int fields = lab_dom_get(ctx->frame, "fields");
    if (ctx->index >= lab_dom_count(fields))
        return lab_flow_done(0);
    int field = lab_dom_item(fields, ctx->index);
    lab_dom_set(ctx->frame, "field", field);
    return lab_flow_call(ctx->frame, 2, "stateRead", "raw", 0, 2, lab_dom_get(ctx->frame, "read"),
                         lab_dom_get(field, "key"));
}

static browser_jsref lab_flow_state_controls_state_control(struct lab_flow_state_controls_context *ctx)
{
    return lab_flow_call(ctx->frame, 3, "stateControl", "bounds", 0, 3,
                         lab_dom_get(lab_dom_get(ctx->frame, "field"), "field"), lab_dom_get(ctx->frame, "raw"),
                         ctx->local);
}

static browser_jsref lab_flow_state_controls_state_bounds(struct lab_flow_state_controls_context *ctx)
{
    if (lab_dom_truth(lab_dom_get(ctx->frame, "bounds")))
        return lab_flow_call(ctx->frame, 4, "stateBounds", 0, 1, 1, lab_dom_get(ctx->frame, "bounds"));
    /* Continue only after the bounds await when present. */
    return lab_flow_state_controls_next(ctx);
}

static browser_jsref lab_flow_state_controls_next(struct lab_flow_state_controls_context *ctx)
{
    lab_dom_set(ctx->frame, "index", lab_dom_numeric(ctx->index + 1));
    lab_dom_set(ctx->frame, "stage", lab_dom_numeric(1));
    return lab_flow_state_controls(ctx->frame, ctx->view);
}

static browser_jsref lab_flow_state_controls_otherwise(struct lab_flow_state_controls_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static browser_jsref lab_flow_state_controls(frame_jsref frame, view_jsref view)
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
    static browser_jsref (*const handlers[])(struct lab_flow_state_controls_context *) = {
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
    frame_jsref frame;
    browser_jsref data;
    browser_jsref mode;
};

/**
 * @brief Apply the mode selected from server-saved worksheet state.
 * @param ctx Saved-state context carrying the selected mode and the frame advanced after application.
 * @return Scoped mode-application plan awaited before stage 11.
 */
static browser_jsref lab_flow_state_saved_state_apply_mode(struct lab_flow_state_saved_context *ctx);

static browser_jsref lab_flow_state_saved_state_server_editors(struct lab_flow_state_saved_context *ctx)
{
    return lab_flow_call(ctx->frame, 1, "stateServerEditors", 0, 1, 1, ctx->data);
}

static browser_jsref lab_flow_state_saved_state_server_controls(struct lab_flow_state_saved_context *ctx)
{
    return lab_flow_call(ctx->frame, 2, "stateServerControls", 0, 1, 1, ctx->data);
}

static browser_jsref lab_flow_state_saved_state_calendar(struct lab_flow_state_saved_context *ctx)
{
    return lab_flow_call(ctx->frame, 3, "stateCalendar", 0, 0, 4, lab_dom_string("datetime"), ctx->data,
                         lab_dom_numeric(0), lab_dom_string("datetime_"));
}

static browser_jsref lab_flow_state_saved_state_calendar_3(struct lab_flow_state_saved_context *ctx)
{
    return lab_flow_call(ctx->frame, 4, "stateCalendar", 0, 0, 4, lab_dom_string("almanac"), ctx->data,
                         lab_dom_numeric(0), lab_dom_string("almanac_"));
}

static browser_jsref lab_flow_state_saved_native(struct lab_flow_state_saved_context *ctx)
{
    return lab_flow_call(ctx->frame, 5, "native", 0, 0, 2, lab_dom_string("lab_storage_precisions"), ctx->data);
}

static browser_jsref lab_flow_state_saved_state_sync_towns(struct lab_flow_state_saved_context *ctx)
{
    return lab_flow_call(ctx->frame, 6, "stateSyncTowns", 0, 1, 0);
}

static browser_jsref lab_flow_state_saved_state_town(struct lab_flow_state_saved_context *ctx)
{
    return lab_flow_call(ctx->frame, 7, "stateTown", 0, 1, 2, lab_dom_numeric(0),
                         lab_dom_get(ctx->data, "datetime_town"));
}

static browser_jsref lab_flow_state_saved_state_town_7(struct lab_flow_state_saved_context *ctx)
{
    return lab_flow_call(ctx->frame, 8, "stateTown", 0, 1, 2, lab_dom_numeric(1),
                         lab_dom_get(ctx->data, "almanac_town"));
}

static browser_jsref lab_flow_state_saved_state_mode(struct lab_flow_state_saved_context *ctx)
{
    return lab_flow_call(ctx->frame, 9, "stateMode", "selected", 0, 1, lab_dom_get(ctx->data, "lab_mode"));
}

static browser_jsref lab_flow_state_saved_state_prepare_mode(struct lab_flow_state_saved_context *ctx)
{
    if (!lab_dom_equal(ctx->mode, lab_dom_string("datetime")) && !lab_dom_equal(ctx->mode, lab_dom_string("almanac")))
        return lab_flow_call(ctx->frame, 10, "statePrepareMode", 0, 1, 1, ctx->mode);
    /* Continue for calendars without editor preparation. */
    return lab_flow_state_saved_state_apply_mode(ctx);
}

static browser_jsref lab_flow_state_saved_state_apply_mode(struct lab_flow_state_saved_context *ctx)
{
    return lab_flow_call(ctx->frame, 11, "stateApplyMode", 0, 1, 1, ctx->mode);
}

static browser_jsref lab_flow_state_saved_otherwise(struct lab_flow_state_saved_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static browser_jsref lab_flow_state_saved(frame_jsref frame, view_jsref view)
{
    (void)view;
    int data = lab_dom_get(frame, "data"), mode = lab_dom_get(frame, "selected");
    struct lab_flow_state_saved_context ctx = {
        .frame = frame,
        .data = data,
        .mode = mode,
    };
    static browser_jsref (*const handlers[])(struct lab_flow_state_saved_context *) = {
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
    frame_jsref frame;
    view_jsref view;
    unsigned int index;
    browser_jsref recovery;
    browser_jsref text;
};

/**
 * @brief Advance local recovery of expression and equation editors.
 * @param frame Borrowed recovery frame retaining stage, editor index, server data, recovery record and text.
 * @param view Borrowed view handle carried through the shared workflow signature; not inspected here.
 * @return Scoped recovery operation plan, or a completion plan after both editors.
 */
static browser_jsref lab_flow_state_recovery(frame_jsref frame, view_jsref view);

/**
 * @brief Select a local recovery record for the current editor.
 * @param ctx Recovery state with expression/equation index and server data held in the frame.
 * @return Scoped recovery-selection plan, or a completion plan once both editors have been visited.
 */
static browser_jsref lab_flow_state_recovery_state_recovery(struct lab_flow_state_recovery_context *ctx);

/**
 * @brief Read the current clock for a recovered worksheet patch.
 * @param ctx Recovery state whose frame receives now before constructing the recovered patch.
 * @return Scoped clock-read plan resuming at stage 5.
 */
static browser_jsref lab_flow_state_recovery_state_now(struct lab_flow_state_recovery_context *ctx);

/**
 * @brief Install recovered text in the indexed editor.
 * @param ctx Recovery state carrying the expression/equation index, recovered text and continuation frame.
 * @return Scoped editor-projection plan resuming at stage 8.
 */
static browser_jsref lab_flow_state_recovery_state_editor(struct lab_flow_state_recovery_context *ctx);

/**
 * @brief Publish the recovered worksheet patch.
 * @param ctx Recovery state whose frame holds the patch prepared from local text and timestamps.
 * @return Scoped patch-save plan resuming at stage 11.
 */
static browser_jsref lab_flow_state_recovery_state_save(struct lab_flow_state_recovery_context *ctx);

static browser_jsref lab_flow_state_recovery_advance(struct lab_flow_state_recovery_context *ctx)
{
    lab_dom_set(ctx->frame, "index", lab_dom_numeric(ctx->index + 1));
    lab_dom_set(ctx->frame, "stage", lab_dom_numeric(1));
    return lab_flow_state_recovery(ctx->frame, ctx->view);
}

static browser_jsref lab_flow_state_recovery_begin(struct lab_flow_state_recovery_context *ctx)
{
    ctx->index = 0;
    lab_dom_set(ctx->frame, "index", lab_dom_numeric(0));
    /* Continue to the first local copy. */
    return lab_flow_state_recovery_state_recovery(ctx);
}

static browser_jsref lab_flow_state_recovery_state_recovery(struct lab_flow_state_recovery_context *ctx)
{
    if (ctx->index >= 2)
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, 2, "stateRecovery", "recovery", 0, 2, lab_dom_numeric(ctx->index),
                         lab_dom_get(ctx->frame, "data"));
}

static browser_jsref lab_flow_state_recovery_state_prepare(struct lab_flow_state_recovery_context *ctx)
{
    if (!ctx->recovery)
        return lab_flow_state_recovery_advance(ctx);
    return lab_flow_call(ctx->frame, 3, "statePrepare", 0, 1, 1, lab_dom_get(ctx->recovery, "text"));
}

static browser_jsref lab_flow_state_recovery_state_canonical(struct lab_flow_state_recovery_context *ctx)
{
    if (lab_persist_canonical(ctx->index))
        return lab_flow_call(ctx->frame, 4, "stateCanonical", "text", 0, 1, lab_dom_get(ctx->recovery, "text"));
    ctx->text = lab_dom_get(ctx->recovery, "text");
    lab_dom_set(ctx->frame, "text", ctx->text);
    /* Continue after metadata preparation, before reading the clock. */
    return lab_flow_state_recovery_state_now(ctx);
}

static browser_jsref lab_flow_state_recovery_state_now(struct lab_flow_state_recovery_context *ctx)
{
    return lab_flow_call(ctx->frame, 5, "stateNow", "now", 0, 0);
}

static browser_jsref lab_flow_state_recovery_state_recovered(struct lab_flow_state_recovery_context *ctx)
{
    return lab_flow_call(ctx->frame, 6, "stateRecovered", "patch", 0, 4, lab_dom_numeric(ctx->index), ctx->recovery,
                         ctx->text, lab_dom_get(ctx->frame, "now"));
}

static browser_jsref lab_flow_state_recovery_state_timestamp(struct lab_flow_state_recovery_context *ctx)
{
    if (!ctx->index)
        return lab_flow_call(ctx->frame, 7, "stateTimestamp", 0, 0, 1,
                             lab_dom_get(lab_dom_get(ctx->frame, "patch"), "expression_updated_at"));
    /* Continue for equation recovery. */
    return lab_flow_state_recovery_state_editor(ctx);
}

static browser_jsref lab_flow_state_recovery_state_editor(struct lab_flow_state_recovery_context *ctx)
{
    return lab_flow_call(ctx->frame, 8, "stateEditor", 0, 0, 2, lab_dom_numeric(ctx->index), ctx->text);
}

static browser_jsref lab_flow_state_recovery_state_mode_index(struct lab_flow_state_recovery_context *ctx)
{
    return lab_flow_call(ctx->frame, 9, "stateModeIndex", "visible", 0, 0);
}

static browser_jsref lab_flow_state_recovery_restore_visible(struct lab_flow_state_recovery_context *ctx)
{
    if ((unsigned)lab_dom_number(lab_dom_get(ctx->frame, "visible")) == ctx->index)
        return lab_flow_call(ctx->frame, 10, ctx->index ? "stateRestoreEditor" : "stateSetExpression", 0,
                             ctx->index != 0, 1, ctx->index ? lab_dom_string("equation") : ctx->text);
    /* Continue if this recovered worksheet is not visible. */
    return lab_flow_state_recovery_state_save(ctx);
}

static browser_jsref lab_flow_state_recovery_state_save(struct lab_flow_state_recovery_context *ctx)
{
    return lab_flow_call(ctx->frame, 11, "stateSave", 0, 0, 1, lab_dom_get(ctx->frame, "patch"));
}

static browser_jsref lab_flow_state_recovery_next(struct lab_flow_state_recovery_context *ctx)
{
    return lab_flow_state_recovery_advance(ctx);
}

static browser_jsref lab_flow_state_recovery_otherwise(struct lab_flow_state_recovery_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static browser_jsref lab_flow_state_recovery(frame_jsref frame, view_jsref view)
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
    static browser_jsref (*const handlers[])(struct lab_flow_state_recovery_context *) = {
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
    frame_jsref frame;
    browser_jsref data;
    browser_jsref mode;
};

/**
 * @brief Load the locally saved almanac state during storage fallback.
 * @param ctx Load state whose frame receives almanac from the last-almanac-state storage key.
 * @return Scoped local-calendar read plan resuming at stage 15.
 */
static browser_jsref lab_flow_state_load_state_local_calendar_8(struct lab_flow_state_load_context *ctx);

/**
 * @brief Read the locally saved worksheet mode during storage fallback.
 * @param ctx Load state whose frame receives savedMode before selecting a valid mode.
 * @return Scoped local-mode read plan resuming at stage 17.
 */
static browser_jsref lab_flow_state_load_state_local_mode(struct lab_flow_state_load_context *ctx);

/**
 * @brief Apply the mode selected by the local-storage fallback.
 * @param ctx Load state carrying the selected mode and the frame advanced to final completion.
 * @return Scoped mode-application plan awaited before stage 30.
 */
static browser_jsref lab_flow_state_load_state_apply_mode(struct lab_flow_state_load_context *ctx);

static browser_jsref lab_flow_state_load_state_fetch(struct lab_flow_state_load_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(10));
    return lab_flow_call(ctx->frame, 1, "stateFetch", "response", 1, 0);
}

static browser_jsref lab_flow_state_load_state_decode(struct lab_flow_state_load_context *ctx)
{
    return lab_flow_call(ctx->frame, 2, "stateDecode", "data", 1, 1, lab_dom_get(ctx->frame, "response"));
}

static browser_jsref lab_flow_state_load_state_apply_saved(struct lab_flow_state_load_context *ctx)
{
    return lab_flow_call(ctx->frame, 3, "stateApplySaved", 0, 1, 1,
                         lab_flow_state_or(ctx->data, lab_dom_object(LAB_VALUE_OBJECT)));
}

static browser_jsref lab_flow_state_load_state_recover(struct lab_flow_state_load_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(30));
    return lab_flow_call(ctx->frame, 30, "stateRecover", 0, 1, 1, ctx->data);
}

static browser_jsref lab_flow_state_load_state_local_editors(struct lab_flow_state_load_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(30));
    return lab_flow_call(ctx->frame, 11, "stateLocalEditors", 0, 1, 0);
}

static browser_jsref lab_flow_state_load_state_local_controls(struct lab_flow_state_load_context *ctx)
{
    return lab_flow_call(ctx->frame, 12, "stateLocalControls", 0, 1, 0);
}

static browser_jsref lab_flow_state_load_state_local_calendar(struct lab_flow_state_load_context *ctx)
{
    return lab_flow_call(ctx->frame, 13, "stateLocalCalendar", "datetime", 0, 1,
                         lab_dom_string("mars.exprLab.lastDatetimeState"));
}

static browser_jsref lab_flow_state_load_state_calendar(struct lab_flow_state_load_context *ctx)
{
    if (lab_dom_truth(lab_dom_get(ctx->frame, "datetime")))
        return lab_flow_call(ctx->frame, 14, "stateCalendar", 0, 0, 2, lab_dom_string("datetime"),
                             lab_dom_get(ctx->frame, "datetime"));
    /* Continue for an absent local calendar. */
    return lab_flow_state_load_state_local_calendar_8(ctx);
}

static browser_jsref lab_flow_state_load_state_local_calendar_8(struct lab_flow_state_load_context *ctx)
{
    return lab_flow_call(ctx->frame, 15, "stateLocalCalendar", "almanac", 0, 1,
                         lab_dom_string("mars.exprLab.lastAlmanacState"));
}

static browser_jsref lab_flow_state_load_state_calendar_9(struct lab_flow_state_load_context *ctx)
{
    if (lab_dom_truth(lab_dom_get(ctx->frame, "almanac")))
        return lab_flow_call(ctx->frame, 16, "stateCalendar", 0, 0, 2, lab_dom_string("almanac"),
                             lab_dom_get(ctx->frame, "almanac"));
    /* Continue for an absent local almanac. */
    return lab_flow_state_load_state_local_mode(ctx);
}

static browser_jsref lab_flow_state_load_state_local_mode(struct lab_flow_state_load_context *ctx)
{
    return lab_flow_call(ctx->frame, 17, "stateLocalMode", "savedMode", 0, 0);
}

static browser_jsref lab_flow_state_load_state_sync_towns(struct lab_flow_state_load_context *ctx)
{
    return lab_flow_call(ctx->frame, 18, "stateSyncTowns", 0, 1, 0);
}

static browser_jsref lab_flow_state_load_state_town(struct lab_flow_state_load_context *ctx)
{
    return lab_flow_call(
        ctx->frame, 19, "stateTown", 0, 1, 2, lab_dom_numeric(0),
        lab_flow_state_or(lab_dom_get(lab_dom_get(ctx->frame, "datetime"), "town"), lab_dom_string("")));
}

static browser_jsref lab_flow_state_load_state_town_13(struct lab_flow_state_load_context *ctx)
{
    return lab_flow_call(
        ctx->frame, 20, "stateTown", 0, 1, 2, lab_dom_numeric(1),
        lab_flow_state_or(lab_dom_get(lab_dom_get(ctx->frame, "almanac"), "town"), lab_dom_string("")));
}

static browser_jsref lab_flow_state_load_state_mode(struct lab_flow_state_load_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "savedMode")))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, 21, "stateMode", "selected", 0, 1, lab_dom_get(ctx->frame, "savedMode"));
}

static browser_jsref lab_flow_state_load_state_prepare_mode(struct lab_flow_state_load_context *ctx)
{
    if (!lab_dom_equal(ctx->mode, lab_dom_string("datetime")) && !lab_dom_equal(ctx->mode, lab_dom_string("almanac")))
        return lab_flow_call(ctx->frame, 22, "statePrepareMode", 0, 1, 1, ctx->mode);
    /* Continue for calendars. */
    return lab_flow_state_load_state_apply_mode(ctx);
}

static browser_jsref lab_flow_state_load_state_apply_mode(struct lab_flow_state_load_context *ctx)
{
    return lab_flow_call(ctx->frame, 30, "stateApplyMode", 0, 1, 1, ctx->mode);
}

static browser_jsref lab_flow_state_load_otherwise(struct lab_flow_state_load_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static browser_jsref lab_flow_state_load(frame_jsref frame, view_jsref view)
{
    (void)view;
    int data = lab_dom_get(frame, "data"), mode = lab_dom_get(frame, "selected");
    struct lab_flow_state_load_context ctx = {
        .frame = frame,
        .data = data,
        .mode = mode,
    };
    static browser_jsref (*const handlers[])(struct lab_flow_state_load_context *) = {
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
    frame_jsref frame;
    browser_jsref state;
    browser_jsref plan;
};

/**
 * @brief Recheck history-restoration ownership after editor preparation.
 * @param ctx History state whose frame holds isCurrent and receives the ownership result in current.
 * @return Scoped guard plan resuming at stage 4 before applying history state.
 */
static browser_jsref lab_flow_state_history_prepared_guard(struct lab_flow_state_history_context *ctx);

/**
 * @brief Recheck history-restoration ownership before final presentation.
 * @param ctx History state whose frame holds isCurrent and receives the ownership result in current.
 * @return Scoped guard plan resuming at stage 10 before final history projection.
 */
static browser_jsref lab_flow_state_history_final_guard(struct lab_flow_state_history_context *ctx);

static browser_jsref lab_flow_state_history_begin(struct lab_flow_state_history_context *ctx)
{
    if (!lab_dom_truth(ctx->state))
        return lab_flow_state_false();
    return lab_flow_state_guard(ctx->frame, 1);
}

static browser_jsref lab_flow_state_history_state_history_plan(struct lab_flow_state_history_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_state_false();
    return lab_flow_call(ctx->frame, 2, "stateHistoryPlan", "plan", 0, 2, lab_dom_numeric(0), ctx->state);
}

static browser_jsref lab_flow_state_history_state_prepare(struct lab_flow_state_history_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->plan, "calendar")))
        return lab_flow_call(ctx->frame, 3, "statePrepare", 0, 1, 1, lab_dom_get(ctx->plan, "text"));
    /* Continue to the same ownership check for calendars. */
    return lab_flow_state_history_prepared_guard(ctx);
}

static browser_jsref lab_flow_state_history_prepared_guard(struct lab_flow_state_history_context *ctx)
{
    return lab_flow_state_guard(ctx->frame, 4);
}

static browser_jsref lab_flow_state_history_state_history_plan_4(struct lab_flow_state_history_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_state_false();
    return lab_flow_call(ctx->frame, 5, "stateHistoryPlan", 0, 0, 2, lab_dom_numeric(1), ctx->state);
}

static browser_jsref lab_flow_state_history_state_bounds(struct lab_flow_state_history_context *ctx)
{
    if (lab_dom_truth(lab_dom_get(ctx->plan, "integrator")))
        return lab_flow_call(ctx->frame, 6, "stateBounds", 0, 1, 2, lab_dom_get(ctx->plan, "bounds"),
                             lab_dom_get(ctx->frame, "isCurrent"));
    if (lab_dom_truth(lab_dom_get(ctx->plan, "calendar")))
        return lab_flow_call(ctx->frame, 9, "stateCalendarHistory", 0, 1, 3, lab_dom_get(ctx->plan, "mode"),
                             lab_dom_get(ctx->plan, "calendarState"), lab_dom_get(ctx->frame, "isCurrent"));
    return lab_flow_state_history_final_guard(ctx);
}

static browser_jsref lab_flow_state_history_bounds_guard(struct lab_flow_state_history_context *ctx)
{
    return lab_flow_state_guard(ctx->frame, 7);
}

static browser_jsref lab_flow_state_history_state_history_plan_7(struct lab_flow_state_history_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_state_false();
    return lab_flow_call(ctx->frame, 9, "stateHistoryPlan", 0, 0, 2, lab_dom_numeric(2), ctx->state);
}

static browser_jsref lab_flow_state_history_final_guard(struct lab_flow_state_history_context *ctx)
{
    return lab_flow_state_guard(ctx->frame, 10);
}

static browser_jsref lab_flow_state_history_state_history_plan_9(struct lab_flow_state_history_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_state_false();
    if (lab_dom_truth(lab_dom_get(ctx->plan, "calendar")))
        return lab_flow_call(ctx->frame, 11, "stateHistoryPlan", 0, 0, 2, lab_dom_numeric(3), ctx->state);
    return lab_flow_call(ctx->frame, 13, "stateUpdated", 0, 0, 1, lab_dom_get(ctx->plan, "text"));
}

static browser_jsref lab_flow_state_history_state_clear_source(struct lab_flow_state_history_context *ctx)
{
    return lab_flow_state_next(ctx->frame, 12, "stateClearSource");
}

static browser_jsref lab_flow_state_history_state_clear_bindings(struct lab_flow_state_history_context *ctx)
{
    return lab_flow_state_next(ctx->frame, 13, "stateClearBindings");
}

static browser_jsref lab_flow_state_history_otherwise(struct lab_flow_state_history_context *ctx)
{
    (void)ctx;
    return lab_flow_done(lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
}

static browser_jsref lab_flow_state_history(frame_jsref frame, view_jsref view)
{
    (void)view;
    int state = lab_dom_get(frame, "state"), plan = lab_dom_get(frame, "plan");
    struct lab_flow_state_history_context ctx = {
        .frame = frame,
        .state = state,
        .plan = plan,
    };
    static browser_jsref (*const handlers[])(struct lab_flow_state_history_context *) = {
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
    frame_jsref frame;
    browser_jsref request;
};

static browser_jsref lab_flow_state_navigation_state_commit_bindings(struct lab_flow_state_navigation_context *ctx)
{
    return lab_flow_call(ctx->frame, 1, "stateCommitBindings", 0, 1, 0);
}

static browser_jsref lab_flow_state_navigation_request_current(struct lab_flow_state_navigation_context *ctx)
{
    return lab_flow_call(ctx->frame, 2, "requestCurrent", "current", 0, 1, ctx->request);
}

static browser_jsref lab_flow_state_navigation_state_navigate(struct lab_flow_state_navigation_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, 3, "stateNavigate", "restored", 0, 1, lab_dom_get(ctx->frame, "direction"));
}

static browser_jsref lab_flow_state_navigation_state_history_buttons(struct lab_flow_state_navigation_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "restored")))
        return lab_flow_state_next(ctx->frame, 8, "stateHistoryButtons");
    return lab_flow_state_next(ctx->frame, 4, "stateCancelBindings");
}

static browser_jsref lab_flow_state_navigation_state_restore_history(struct lab_flow_state_navigation_context *ctx)
{
    return lab_flow_call(ctx->frame, 5, "stateRestoreHistory", 0, 1, 2, lab_dom_get(ctx->frame, "restored"),
                         ctx->request);
}

static browser_jsref lab_flow_state_navigation_request_current_5(struct lab_flow_state_navigation_context *ctx)
{
    return lab_flow_call(ctx->frame, 6, "requestCurrent", "current", 0, 1, ctx->request);
}

static browser_jsref lab_flow_state_navigation_state_evaluate_history(struct lab_flow_state_navigation_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, 8, "stateEvaluateHistory", 0, 1, 0);
}

static browser_jsref lab_flow_state_navigation_otherwise(struct lab_flow_state_navigation_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static browser_jsref lab_flow_state_navigation(frame_jsref frame, view_jsref view)
{
    (void)view;
    int request = lab_dom_get(frame, "request");
    struct lab_flow_state_navigation_context ctx = {
        .frame = frame,
        .request = request,
    };
    static browser_jsref (*const handlers[])(struct lab_flow_state_navigation_context *) = {
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
    frame_jsref frame;
    unsigned int index;
};

static browser_jsref lab_flow_state_capture_state_capture_guard(struct lab_flow_state_capture_context *ctx)
{
    return lab_flow_call(ctx->frame, 1, "stateCaptureGuard", "isCurrent", 0, 1, lab_dom_get(ctx->frame, "isCurrent"));
}

static browser_jsref lab_flow_state_capture_guard(struct lab_flow_state_capture_context *ctx)
{
    return lab_flow_state_guard(ctx->frame, 2);
}

static browser_jsref lab_flow_state_capture_state_commit_bindings(struct lab_flow_state_capture_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_state_false();
    return lab_flow_call(ctx->frame, 3, "stateCommitBindings", 0, 1, 1, lab_dom_get(ctx->frame, "isCurrent"));
}

static browser_jsref lab_flow_state_capture_binding_guard(struct lab_flow_state_capture_context *ctx)
{
    return lab_flow_state_guard(ctx->frame, 4);
}

static browser_jsref lab_flow_state_capture_state_capture_editor(struct lab_flow_state_capture_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "current")))
        return lab_flow_state_false();
    return lab_flow_call(ctx->frame, 5, "stateCaptureEditor", "index", 0, 0);
}

static browser_jsref lab_flow_state_capture_history_guard(struct lab_flow_state_capture_context *ctx)
{
    return lab_flow_call(ctx->frame, 6, ctx->index < 5 ? "stateSaveEditor" : "stateSaveCalendar", 0, 0, 1,
                         lab_dom_numeric(ctx->index));
}

static browser_jsref lab_flow_state_capture_otherwise(struct lab_flow_state_capture_context *ctx)
{
    (void)ctx;
    return lab_flow_done(lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
}

static browser_jsref lab_flow_state_capture(frame_jsref frame, view_jsref view)
{
    (void)view;
    unsigned index = (unsigned)lab_dom_number(lab_dom_get(frame, "index"));
    struct lab_flow_state_capture_context ctx = {
        .frame = frame,
        .index = index,
    };
    static browser_jsref (*const handlers[])(struct lab_flow_state_capture_context *) = {
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

/**
 * @brief Read the selected mode for mode-specific final projection.
 * @param frame Borrowed mode-selection frame receiving index after editor, result and bounds restoration.
 * @return Scoped mode-index query plan resuming at stage 16.
 */
static browser_jsref lab_flow_state_selection_state_mode_index(frame_jsref frame);

/**
 * @brief Complete mode selection without further UI changes.
 * @param frame Mode-selection continuation frame; unused when finishing or discarding stale failures.
 * @return Scoped completion plan with no value.
 */
static browser_jsref lab_flow_state_selection_otherwise(frame_jsref frame);

static browser_jsref lab_flow_state_selection_state_capture_request(frame_jsref frame)
{
    return lab_flow_call(frame, 1, "stateCaptureRequest", "captured", 1, 1, lab_dom_get(frame, "request"));
}

static browser_jsref lab_flow_state_selection_state_save_result(frame_jsref frame)
{
    if (!lab_dom_truth(lab_dom_get(frame, "captured")))
        return lab_flow_done(0);
    return lab_flow_state_next(frame, 2, "stateSaveResult");
}

static browser_jsref lab_flow_state_selection_state_set_mode(frame_jsref frame)
{
    return lab_flow_call(frame, 3, "stateSetMode", "changed", 0, 1, lab_dom_get(frame, "mode"));
}

static browser_jsref lab_flow_state_selection_state_selection_guard(frame_jsref frame)
{
    if (!lab_dom_truth(lab_dom_get(frame, "changed")))
        return lab_flow_done(0);
    return lab_flow_call(frame, 4, "stateSelectionGuard", "isCurrent", 0, 0);
}

static browser_jsref lab_flow_state_selection_state_save_mode(frame_jsref frame)
{
    lab_dom_set(frame, "failed", lab_dom_numeric(30));
    return lab_flow_state_next(frame, 5, "stateSaveMode");
}

static browser_jsref lab_flow_state_selection_state_hide_target(frame_jsref frame)
{
    return lab_flow_state_next(frame, 6, "stateHideTarget");
}

static browser_jsref lab_flow_state_selection_state_restore_current_editor(frame_jsref frame)
{
    return lab_flow_call(frame, 7, "stateRestoreCurrentEditor", 0, 1, 0);
}

static browser_jsref lab_flow_state_selection_editor_guard(frame_jsref frame)
{
    return lab_flow_state_guard(frame, 8);
}

static browser_jsref lab_flow_state_selection_state_sync_ui(frame_jsref frame)
{
    if (!lab_dom_truth(lab_dom_get(frame, "current")))
        return lab_flow_done(0);
    return lab_flow_state_next(frame, 9, "stateSyncUI");
}

static browser_jsref lab_flow_state_selection_state_restore_result(frame_jsref frame)
{
    return lab_flow_call(frame, 10, "stateRestoreResult", 0, 1, 0);
}

static browser_jsref lab_flow_state_selection_result_guard(frame_jsref frame)
{
    return lab_flow_state_guard(frame, 11);
}

static browser_jsref lab_flow_state_selection_state_needs_bounds(frame_jsref frame)
{
    if (!lab_dom_truth(lab_dom_get(frame, "current")))
        return lab_flow_done(0);
    return lab_flow_call(frame, 12, "stateNeedsBounds", "needsBounds", 0, 0);
}

static browser_jsref lab_flow_state_selection_state_reset_bounds(frame_jsref frame)
{
    if (lab_dom_truth(lab_dom_get(frame, "needsBounds")))
        return lab_flow_call(frame, 13, "stateResetBounds", "reset", 1, 0);
    return lab_flow_state_selection_state_mode_index(frame);
}

static browser_jsref lab_flow_state_selection_bounds_guard(frame_jsref frame)
{
    if (!lab_dom_truth(lab_dom_get(frame, "reset")))
        return lab_flow_done(0);
    return lab_flow_state_guard(frame, 14);
}

static browser_jsref lab_flow_state_selection_finish_guard(frame_jsref frame)
{
    if (!lab_dom_truth(lab_dom_get(frame, "current")))
        return lab_flow_done(0);
    /* Continue to mode-specific final projection. */
    return lab_flow_state_selection_state_mode_index(frame);
}

static browser_jsref lab_flow_state_selection_state_mode_index(frame_jsref frame)
{
    return lab_flow_call(frame, 16, "stateModeIndex", "index", 0, 0);
}

static browser_jsref lab_flow_state_selection_finish(frame_jsref frame)
{
    lab_mode_t index = (lab_mode_t)(unsigned)lab_dom_number(lab_dom_get(frame, "index"));
    static const char *const finishers[] = {"stateFocusEditor",  "stateFocusEditor",      "stateFocusEditor",
                                            "stateFocusEditor",  "stateFinishIntegrator", "stateFinishDatetime",
                                            "stateFinishAlmanac"};
    return (unsigned)index < LAB_MODE_COUNT ? lab_flow_state_next(frame, 40, finishers[index]) : lab_flow_done(0);
}

static browser_jsref lab_flow_state_selection_error_guard(frame_jsref frame)
{
    return lab_flow_state_guard(frame, 31);
}

static browser_jsref lab_flow_state_selection_state_status(frame_jsref frame)
{
    if (lab_dom_truth(lab_dom_get(frame, "current")))
        return lab_flow_call(frame, 40, "stateStatus", 0, 0, 1, lab_dom_get(frame, "error"));
    /* Continue: stale failures cannot change the visible status. */
    return lab_flow_state_selection_otherwise(frame);
}

static browser_jsref lab_flow_state_selection_otherwise(frame_jsref frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static browser_jsref lab_flow_state_selection(frame_jsref frame, view_jsref view)
{
    (void)view;
    static browser_jsref (*const handlers[])(browser_jsref) = {
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

/**
 * @brief Remove precision-recovery routing and refresh history buttons.
 * @param frame Borrowed precision-change frame whose failed handler is removed before final UI refresh.
 * @return Scoped history-button refresh plan resuming at stage 6.
 */
static browser_jsref lab_flow_state_precision_state_history_buttons(frame_jsref frame);

static browser_jsref lab_flow_state_precision_state_precision_step(frame_jsref frame)
{
    return lab_flow_call(frame, 1, "statePrecisionStep", 0, 0, 2, lab_dom_get(frame, "direction"),
                         lab_dom_get(frame, "bits"));
}

static browser_jsref lab_flow_state_precision_state_save_precision(frame_jsref frame)
{
    return lab_flow_state_next(frame, 2, "stateSavePrecision");
}

static browser_jsref lab_flow_state_precision_state_status(frame_jsref frame)
{
    return lab_flow_call(frame, 3, "stateStatus", 0, 0, 1, lab_dom_string("Precision changed"));
}

static browser_jsref lab_flow_state_precision_state_precision_plan(frame_jsref frame)
{
    lab_dom_set(frame, "failed", lab_dom_numeric(10));
    return lab_flow_call(frame, 4, "statePrecisionPlan", "plan", 0, 0);
}

static browser_jsref lab_flow_state_precision_complete(frame_jsref frame)
{
    {
        int plan = lab_dom_get(frame, "plan");
        lab_event_precision_action_t action = (lab_event_precision_action_t)(unsigned)lab_dom_number(lab_dom_get(plan, "action"));
        static const char *const services[] = {
            [LAB_EVENT_PRECISION_NONE]     = "stateHistoryButtons",
            [LAB_EVENT_PRECISION_GOAL]     = "statePrecisionGoal",
            [LAB_EVENT_PRECISION_REUSE]    = "statePrecisionExpression",
            [LAB_EVENT_PRECISION_EVALUATE] = "stateEvaluate",
        };
        if ((unsigned)action > LAB_EVENT_PRECISION_EVALUATE)
            action = LAB_EVENT_PRECISION_NONE;
        if (action != LAB_EVENT_PRECISION_NONE)
            return lab_flow_call(frame, 5, services[action], 0, 1, 1, plan);
        /* Continue to finally for a no-op precision refresh. */
    }
    return lab_flow_state_precision_state_history_buttons(frame);
}

static browser_jsref lab_flow_state_precision_state_history_buttons(frame_jsref frame)
{
    lab_dom_delete(frame, "failed");
    return lab_flow_state_next(frame, 6, "stateHistoryButtons");
}

static browser_jsref lab_flow_state_precision_state_history_buttons_6(frame_jsref frame)
{
    return lab_flow_state_next(frame, 11, "stateHistoryButtons");
}

static browser_jsref lab_flow_state_precision_rethrow(frame_jsref frame)
{
    return lab_flow_call(frame, 6, "rethrow", 0, 0, 1, lab_dom_get(frame, "exception"));
}

static browser_jsref lab_flow_state_precision_otherwise(frame_jsref frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static browser_jsref lab_flow_state_precision(frame_jsref frame, view_jsref view)
{
    (void)view;
    static browser_jsref (*const handlers[])(browser_jsref) = {
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
browser_jsref lab_flow_state(lab_flow_state_kind_t kind, frame_jsref frame, view_jsref view)
{
    static browser_jsref (*const handlers[])(browser_jsref, browser_jsref) = {
        [LAB_FLOW_STATE_EDITORS]    = lab_flow_state_editors,
        [LAB_FLOW_STATE_CONTROLS]   = lab_flow_state_controls,
        [LAB_FLOW_STATE_SAVED]      = lab_flow_state_saved,
        [LAB_FLOW_STATE_RECOVERY]   = lab_flow_state_recovery,
        [LAB_FLOW_STATE_LOAD]       = lab_flow_state_load,
        [LAB_FLOW_STATE_HISTORY]    = lab_flow_state_history,
        [LAB_FLOW_STATE_NAVIGATION] = lab_flow_state_navigation,
        [LAB_FLOW_STATE_CAPTURE]    = lab_flow_state_capture,
        [LAB_FLOW_STATE_SELECTION]  = lab_flow_state_selection,
        [LAB_FLOW_STATE_PRECISION]  = lab_flow_state_precision,
    };
    return (unsigned)kind < sizeof handlers / sizeof *handlers ? handlers[kind](frame, view) : lab_flow_done(0);
}
