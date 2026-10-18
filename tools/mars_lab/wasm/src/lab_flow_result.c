/**
 * @file lab_flow_result.c
 * @brief Native result actions, clipboard policy and initial worksheet sequencing.
 *
 * Transfers native mathematical text without parsing it, guards asynchronous editor
 * reconstruction against intervening edits, and retains plain results when optional
 * layout requests fail. Clipboard access, timers and promises remain browser
 * capabilities. All continuation state belongs to individual browser frames.
 */
#include "lab_dom.h"
#include "lab_flow.h"
#include "lab_flow_result.h"

static int lab_flow_result_same_editor(frame_jsref frame)
{
    int before = lab_dom_get(frame, "before"), current = lab_dom_get(frame, "current");
    return lab_dom_equal(lab_dom_get(before, "mode"), lab_dom_get(current, "mode")) &&
           lab_dom_equal(lab_dom_get(before, "context"), lab_dom_get(current, "context")) &&
           lab_dom_equal(lab_dom_get(before, "editor"), lab_dom_get(current, "editor"));
}

struct lab_flow_result_transfer_context {
    frame_jsref frame;
    jsref text;
    jsref bindings;
};

/**
 * @brief Clear goal-seeking state during result-to-input transfer.
 * @param ctx Transfer state whose frame resumes at stage 11 after the goal is cleared.
 * @return Scoped plan calling the goal-clear service.
 */
static jsref lab_flow_result_transfer_result_clear_goal(struct lab_flow_result_transfer_context *ctx);

/**
 * @brief Save the editor after transferring a result into it.
 * @param ctx Transfer state whose frame resumes at stage 22 after the editor save.
 * @return Scoped plan calling the editor-save service.
 */
static jsref lab_flow_result_transfer_result_save_editor(struct lab_flow_result_transfer_context *ctx);

static jsref lab_flow_result_transfer_result_input(struct lab_flow_result_transfer_context *ctx)
{
    return lab_flow_call(ctx->frame, 1, "resultInput", "text", 0, 0);
}

static jsref lab_flow_result_transfer_result_snapshot(struct lab_flow_result_transfer_context *ctx)
{
    if (!lab_dom_truth(ctx->text))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, 2, "resultSnapshot", "before", 0, 0);
}

static jsref lab_flow_result_transfer_result_prepare(struct lab_flow_result_transfer_context *ctx)
{
    return lab_flow_call(ctx->frame, 3, "resultPrepare", 0, 1, 1, ctx->text);
}

static jsref lab_flow_result_transfer_expression_text(struct lab_flow_result_transfer_context *ctx)
{
    return lab_flow_call(ctx->frame, 4, "expressionText", "source", 0, 0);
}

static jsref lab_flow_result_transfer_result_prepare_4(struct lab_flow_result_transfer_context *ctx)
{
    return lab_flow_call(ctx->frame, 5, "resultPrepare", 0, 1, 1, lab_dom_get(ctx->frame, "source"));
}

static jsref lab_flow_result_transfer_result_snapshot_5(struct lab_flow_result_transfer_context *ctx)
{
    return lab_flow_call(ctx->frame, 6, "resultSnapshot", "current", 0, 0);
}

static jsref lab_flow_result_transfer_history_state(struct lab_flow_result_transfer_context *ctx)
{
    if (!lab_flow_result_same_editor(ctx->frame))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, 7, "historyState", "history", 0, 1, 0);
}

static jsref lab_flow_result_transfer_history_state_7(struct lab_flow_result_transfer_context *ctx)
{
    return lab_flow_call(ctx->frame, 8, "historyState", "nextHistory", 0, 1, ctx->text);
}

static jsref lab_flow_result_transfer_result_history_equal(struct lab_flow_result_transfer_context *ctx)
{
    return lab_flow_call(ctx->frame, 9, "resultHistoryEqual", "sameHistory", 0, 2, lab_dom_get(ctx->frame, "history"),
                         lab_dom_get(ctx->frame, "nextHistory"));
}

static jsref lab_flow_result_transfer_push_history(struct lab_flow_result_transfer_context *ctx)
{
    if (lab_dom_truth(lab_dom_get(lab_dom_get(ctx->frame, "history"), "text")) &&
        !lab_dom_truth(lab_dom_get(ctx->frame, "sameHistory")))
        return lab_flow_call(ctx->frame, 10, "pushHistory", 0, 0, 1, lab_dom_get(ctx->frame, "history"));
    /* Continue: identical history is not pushed. */
    return lab_flow_result_transfer_result_clear_goal(ctx);
}

static jsref lab_flow_result_transfer_result_clear_goal(struct lab_flow_result_transfer_context *ctx)
{
    return lab_flow_call(ctx->frame, 11, "resultClearGoal", 0, 0, 0);
}

static jsref lab_flow_result_transfer_result_source_bindings(struct lab_flow_result_transfer_context *ctx)
{
    int mode = lab_dom_get(lab_dom_get(ctx->frame, "current"), "mode");
    if (lab_dom_equal(mode, lab_dom_string("matrix")))
        return lab_flow_call(ctx->frame, 12, "resultSourceBindings", "sourceRecord", 0, 0);
    if (lab_dom_equal(mode, lab_dom_string("equation")) || lab_dom_equal(mode, lab_dom_string("diffequation")))
        return lab_flow_call(ctx->frame, 21, "setExpressionEditor", 0, 0, 1, ctx->text);
    return lab_flow_call(ctx->frame, 20, "resultApplyBinding", "applied", 1, 2, ctx->text, ctx->text);
}

static jsref lab_flow_result_transfer_result_bindings(struct lab_flow_result_transfer_context *ctx)
{
    int source = lab_dom_get(lab_dom_get(ctx->frame, "sourceRecord"), "bindings");
    lab_dom_set(ctx->frame, "bindings", lab_dom_truth(source) ? source : lab_dom_object(LAB_VALUE_ARRAY));
    return lab_flow_call(ctx->frame, 13, "resultBindings", "resultBindings", 0, 0);
}

static jsref lab_flow_result_transfer_expression_with_bindings(struct lab_flow_result_transfer_context *ctx)
{
    if (lab_dom_count(lab_dom_get(ctx->frame, "resultBindings")))
        return lab_flow_call(ctx->frame, 14, "expressionWithBindings", "boundSource", 1, 2, ctx->text, ctx->bindings);
    return lab_flow_call(ctx->frame, 16, "expressionWithBindings", "expression", 1, 2, ctx->text, ctx->bindings);
}

static jsref lab_flow_result_transfer_result_authored_bindings(struct lab_flow_result_transfer_context *ctx)
{
    return lab_flow_call(ctx->frame, 15, "resultAuthoredBindings", "bindings", 0, 2,
                         lab_dom_get(ctx->frame, "resultBindings"), lab_dom_get(ctx->frame, "boundSource"));
}

static jsref lab_flow_result_transfer_expression_with_bindings_15(struct lab_flow_result_transfer_context *ctx)
{
    return lab_flow_call(ctx->frame, 16, "expressionWithBindings", "expression", 1, 2, ctx->text, ctx->bindings);
}

static jsref lab_flow_result_transfer_result_snapshot_16(struct lab_flow_result_transfer_context *ctx)
{
    return lab_flow_call(ctx->frame, 17, "resultSnapshot", "current", 0, 0);
}

static jsref lab_flow_result_transfer_set_expression_editor(struct lab_flow_result_transfer_context *ctx)
{
    if (!lab_flow_result_same_editor(ctx->frame))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, 18, "setExpressionEditor", 0, 0, 3, lab_dom_get(ctx->frame, "expression"),
                         ctx->bindings, ctx->text);
}

static jsref lab_flow_result_transfer_result_matrix_controls(struct lab_flow_result_transfer_context *ctx)
{
    return lab_flow_call(ctx->frame, 21, "resultMatrixControls", 0, 0, 0);
}

static jsref lab_flow_result_transfer_require_applied(struct lab_flow_result_transfer_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "applied")))
        return lab_flow_done(0);
    /* Continue: only a committed transfer is persisted. */
    return lab_flow_result_transfer_result_save_editor(ctx);
}

static jsref lab_flow_result_transfer_result_save_editor(struct lab_flow_result_transfer_context *ctx)
{
    return lab_flow_call(ctx->frame, 22, "resultSaveEditor", 0, 0, 0);
}

static jsref lab_flow_result_transfer_update_history_buttons(struct lab_flow_result_transfer_context *ctx)
{
    return lab_flow_call(ctx->frame, 23, "updateHistoryButtons", 0, 0, 0);
}

static jsref lab_flow_result_transfer_set_status(struct lab_flow_result_transfer_context *ctx)
{
    lab_dom_effect(lab_dom_id("expr"), LAB_DOM_EFFECT_FOCUS);
    return lab_flow_call(ctx->frame, 24, "setStatus", 0, 0, 1, lab_dom_string("Result sent to input"));
}

static jsref lab_flow_result_transfer_otherwise(struct lab_flow_result_transfer_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static jsref lab_flow_result_transfer(frame_jsref frame)
{
    int text = lab_dom_get(frame, "text"), bindings = lab_dom_get(frame, "bindings");
    unsigned stage = (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
    struct lab_flow_result_transfer_context ctx = {
        .frame = frame,
        .text = text,
        .bindings = bindings,
    };
    static jsref (*const handlers[])(struct lab_flow_result_transfer_context *) = {
        [0]  = lab_flow_result_transfer_result_input,
        [1]  = lab_flow_result_transfer_result_snapshot,
        [2]  = lab_flow_result_transfer_result_prepare,
        [3]  = lab_flow_result_transfer_expression_text,
        [4]  = lab_flow_result_transfer_result_prepare_4,
        [5]  = lab_flow_result_transfer_result_snapshot_5,
        [6]  = lab_flow_result_transfer_history_state,
        [7]  = lab_flow_result_transfer_history_state_7,
        [8]  = lab_flow_result_transfer_result_history_equal,
        [9]  = lab_flow_result_transfer_push_history,
        [10] = lab_flow_result_transfer_result_clear_goal,
        [11] = lab_flow_result_transfer_result_source_bindings,
        [12] = lab_flow_result_transfer_result_bindings,
        [13] = lab_flow_result_transfer_expression_with_bindings,
        [14] = lab_flow_result_transfer_result_authored_bindings,
        [15] = lab_flow_result_transfer_expression_with_bindings_15,
        [16] = lab_flow_result_transfer_result_snapshot_16,
        [17] = lab_flow_result_transfer_set_expression_editor,
        [18] = lab_flow_result_transfer_result_matrix_controls,
        [20] = lab_flow_result_transfer_require_applied,
        [21] = lab_flow_result_transfer_result_save_editor,
        [22] = lab_flow_result_transfer_update_history_buttons,
        [23] = lab_flow_result_transfer_set_status,
    };
    unsigned selected = stage;
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_result_transfer_otherwise(&ctx);
}

struct lab_flow_result_pretty_context {
    frame_jsref frame;
    view_jsref view;
    jsref element;
};

static jsref lab_flow_result_pretty_native(struct lab_flow_result_pretty_context *ctx)
{
    return lab_flow_call(ctx->frame, 1, "native", "request", 0, 6, lab_dom_string("lab_result_pretty_begin"),
                         lab_dom_get(ctx->view, "caches"), ctx->element, lab_dom_get(ctx->frame, "button"),
                         lab_dom_get(ctx->frame, "text"), lab_dom_get(ctx->frame, "pretty"));
}

static jsref lab_flow_result_pretty_result_presentation(struct lab_flow_result_pretty_context *ctx)
{
    int payload = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(payload, "action", lab_dom_string("matrix"));
    lab_dom_set(payload, "text", lab_dom_get(ctx->frame, "text"));
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(3));
    return lab_flow_call(ctx->frame, 2, "resultPresentation", "data", 1, 1, payload);
}

static jsref lab_flow_result_pretty_native_2(struct lab_flow_result_pretty_context *ctx)
{
    lab_dom_delete(ctx->frame, "failed");
    return lab_flow_call(ctx->frame, 3, "native", 0, 0, 4, lab_dom_string("lab_result_pretty_finish"), ctx->element,
                         lab_dom_get(ctx->frame, "request"), lab_dom_get(ctx->frame, "data"));
}

static jsref lab_flow_result_pretty_otherwise(struct lab_flow_result_pretty_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static jsref lab_flow_result_pretty(frame_jsref frame, view_jsref view)
{
    int element = lab_dom_get(frame, "element");
    struct lab_flow_result_pretty_context ctx = {
        .frame = frame,
        .view = view,
        .element = element,
    };
    static jsref (*const handlers[])(struct lab_flow_result_pretty_context *) = {
        [0] = lab_flow_result_pretty_native,
        [1] = lab_flow_result_pretty_result_presentation,
        [2] = lab_flow_result_pretty_native_2,
    };
    unsigned selected = (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_flow_result_pretty_otherwise(&ctx);
}

/**
 * @brief Finish result-digit adjustment without a return value.
 * @param frame Digit-adjustment continuation frame; unused at this terminal stage.
 * @return Scoped completion plan with no value.
 */
static jsref lab_flow_result_digits_otherwise(frame_jsref frame);

static jsref lab_flow_result_digits_result_last_TeX(frame_jsref frame)
{
    return lab_flow_call(frame, 1, "resultLastTeX", "TeX", 0, 0);
}

static jsref lab_flow_result_digits_native(frame_jsref frame)
{
    return lab_flow_call(frame, 2, "native", "request", 0, 2, lab_dom_string("lab_result_digits_begin"),
                         lab_dom_get(frame, "TeX"));
}

static jsref lab_flow_result_digits_set_status(frame_jsref frame)
{
    if (!lab_dom_truth(lab_dom_get(frame, "request")))
        return lab_flow_done(0);
    return lab_flow_call(frame, 3, "setStatus", 0, 0, 1, lab_dom_string("Rendering full TeX..."));
}

static jsref lab_flow_result_digits_result_render_TeX(frame_jsref frame)
{
    lab_dom_set(frame, "failed", lab_dom_numeric(4));
    return lab_flow_call(frame, 4, "resultRenderTeX", "data", 1, 1, lab_dom_get(lab_dom_get(frame, "request"), "TeX"));
}

static jsref lab_flow_result_digits_native_4(frame_jsref frame)
{
    lab_dom_delete(frame, "failed");
    return lab_flow_call(frame, 5, "native", "accepted", 0, 4, lab_dom_string("lab_result_digits_finish"),
                         lab_dom_get(frame, "request"), lab_dom_get(frame, "data"),
                         lab_dom_get(frame, "error") ? lab_dom_get(frame, "error") : lab_dom_string(""));
}

static jsref lab_flow_result_digits_set_status_5(frame_jsref frame)
{
    if (lab_dom_truth(lab_dom_get(frame, "accepted")))
        return lab_flow_call(frame, 6, "setStatus", 0, 0, 1, lab_dom_string("Ready"));
    /* Continue: obsolete render results have no visible completion. */
    return lab_flow_result_digits_otherwise(frame);
}

static jsref lab_flow_result_digits_otherwise(frame_jsref frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static jsref lab_flow_result_digits(frame_jsref frame)
{
    static jsref (*const handlers[])(jsref) = {
        [0] = lab_flow_result_digits_result_last_TeX,
        [1] = lab_flow_result_digits_native,
        [2] = lab_flow_result_digits_set_status,
        [3] = lab_flow_result_digits_result_render_TeX,
        [4] = lab_flow_result_digits_native_4,
        [5] = lab_flow_result_digits_set_status_5,
    };
    unsigned selected = (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](frame)
               : lab_flow_result_digits_otherwise(frame);
}

struct lab_flow_result_copy_context {
    unsigned int input;
    frame_jsref frame;
    jsref button;
};

/**
 * @brief Copy non-empty result text with clipboard-failure recovery.
 * @param ctx Copy state whose frame holds text and receives the clipboard-failure continuation.
 * @return Scoped clipboard-write plan, or a completion plan when text is empty.
 */
static jsref lab_flow_result_copy_result_clipboard(struct lab_flow_result_copy_context *ctx);

static jsref lab_flow_result_copy_commit_bindings(struct lab_flow_result_copy_context *ctx)
{
    if (ctx->input) {
        lab_dom_set(ctx->frame, "failed", lab_dom_numeric(8));
        return lab_flow_call(ctx->frame, 1, "commitBindings", 0, 1, 0);
    }
    return lab_flow_call(ctx->frame, 3, "resultCopyText", "text", 0, 1, ctx->button);
}

static jsref lab_flow_result_copy_current_mode(struct lab_flow_result_copy_context *ctx)
{
    return lab_flow_call(ctx->frame, 2, "currentMode", "mode", 0, 0);
}

static jsref lab_flow_result_copy_result_datetime_summary(struct lab_flow_result_copy_context *ctx)
{
    if (lab_dom_equal(lab_dom_get(ctx->frame, "mode"), lab_dom_string("datetime")))
        return lab_flow_call(ctx->frame, 3, "resultDatetimeSummary", "text", 0, 0);
    lab_dom_set(ctx->frame, "text", lab_dom_clean(lab_dom_read(lab_dom_id("expr"), LAB_DOM_VALUE, ""), LAB_DOM_CLEAN_TRIM));
    /* Continue: expression clipboard source is the authored editor, not a result. */
    return lab_flow_result_copy_result_clipboard(ctx);
}

static jsref lab_flow_result_copy_result_clipboard(struct lab_flow_result_copy_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "text")))
        return lab_flow_done(0);
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(8));
    return lab_flow_call(ctx->frame, 4, "resultClipboard", 0, 1, 1, lab_dom_get(ctx->frame, "text"));
}

static jsref lab_flow_result_copy_result_flash(struct lab_flow_result_copy_context *ctx)
{
    return lab_flow_call(ctx->frame, 5, "resultFlash", 0, 0, 2, ctx->button, lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
}

static jsref lab_flow_result_copy_set_status(struct lab_flow_result_copy_context *ctx)
{
    return lab_flow_call(ctx->frame, 6, "setStatus", 0, 0, 1, lab_dom_string(ctx->input ? "Copied input" : "Copied"));
}

static jsref lab_flow_result_copy_result_status_later(struct lab_flow_result_copy_context *ctx)
{
    return lab_flow_call(ctx->frame, 10, "resultStatusLater", 0, 0, 2, lab_dom_string("Ready"), lab_dom_numeric(1000));
}

static jsref lab_flow_result_copy_result_flash_7(struct lab_flow_result_copy_context *ctx)
{
    return lab_flow_call(ctx->frame, 9, "resultFlash", 0, 0, 2, ctx->button, lab_dom_scalar(LAB_VALUE_BOOLEAN, 0));
}

static jsref lab_flow_result_copy_set_status_8(struct lab_flow_result_copy_context *ctx)
{
    return lab_flow_call(ctx->frame, 10, "setStatus", 0, 0, 1, lab_dom_get(ctx->frame, "error"));
}

static jsref lab_flow_result_copy_otherwise(struct lab_flow_result_copy_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static jsref lab_flow_result_copy(unsigned input, frame_jsref frame)
{
    int button = lab_dom_get(frame, "button");
    struct lab_flow_result_copy_context ctx = {
        .input = input,
        .frame = frame,
        .button = button,
    };
    static jsref (*const handlers[])(struct lab_flow_result_copy_context *) = {
        [0] = lab_flow_result_copy_commit_bindings,
        [1] = lab_flow_result_copy_current_mode,
        [2] = lab_flow_result_copy_result_datetime_summary,
        [3] = lab_flow_result_copy_result_clipboard,
        [4] = lab_flow_result_copy_result_flash,
        [5] = lab_flow_result_copy_set_status,
        [6] = lab_flow_result_copy_result_status_later,
        [8] = lab_flow_result_copy_result_flash_7,
        [9] = lab_flow_result_copy_set_status_8,
    };
    unsigned selected = (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_flow_result_copy_otherwise(&ctx);
}

static jsref lab_flow_result_TeX_result_fetch(frame_jsref frame)
{
    int payload = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(payload, "tex", lab_dom_get(frame, "TeX"));
    return lab_flow_call(frame, 1, "resultFetch", "response", 1, 2, lab_dom_string("/render_TeX"), payload);
}

static jsref lab_flow_result_TeX_result_decode(frame_jsref frame)
{
    return lab_flow_call(frame, 2, "resultDecode", "data", 1, 1, lab_dom_get(frame, "response"));
}

static jsref lab_flow_result_TeX_fail(frame_jsref frame)
{
    int data = lab_dom_get(frame, "data"), error = lab_dom_get(data, "error");
    if (lab_dom_truth(lab_dom_get(lab_dom_get(frame, "response"), "ok")) && lab_dom_truth(lab_dom_get(data, "ok")))
        return lab_flow_done(data);
    return lab_flow_call(frame, 3, "fail", 0, 0, 1,
                         lab_dom_truth(error) ? error : lab_dom_string("Could not render TeX"));
}

static jsref lab_flow_result_TeX_otherwise(frame_jsref frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static jsref lab_flow_result_TeX(frame_jsref frame)
{
    static jsref (*const handlers[])(jsref) = {
        [0] = lab_flow_result_TeX_result_fetch,
        [1] = lab_flow_result_TeX_result_decode,
        [2] = lab_flow_result_TeX_fail,
    };
    unsigned selected = (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](frame)
                                                                               : lab_flow_result_TeX_otherwise(frame);
}

struct lab_flow_result_initialise_context {
    frame_jsref frame;
};

/**
 * @brief Finish result-card initialisation without a return value.
 * @param ctx Result-initialisation state; unused at this terminal stage.
 * @return Scoped completion plan with no value.
 */
static jsref lab_flow_result_initialise_otherwise(struct lab_flow_result_initialise_context *ctx);

static jsref lab_flow_result_initialise_result_startup_snapshot(struct lab_flow_result_initialise_context *ctx)
{
    return lab_flow_call(ctx->frame, 1, "resultStartupSnapshot", "before", 0, 0);
}

static jsref lab_flow_result_initialise_result_restore_bounds(struct lab_flow_result_initialise_context *ctx)
{
    lab_dom_set(ctx->frame, "failed", lab_dom_numeric(8));
    return lab_flow_call(ctx->frame, 2, "resultRestoreBounds", "restored", 1, 0);
}

static jsref lab_flow_result_initialise_result_startup_snapshot_2(struct lab_flow_result_initialise_context *ctx)
{
    if (!lab_dom_truth(lab_dom_get(ctx->frame, "restored")))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, 3, "resultStartupSnapshot", "current", 0, 0);
}

static jsref lab_flow_result_initialise_result_load_state(struct lab_flow_result_initialise_context *ctx)
{
    int before = lab_dom_get(ctx->frame, "before"), current = lab_dom_get(ctx->frame, "current");
    if (!lab_dom_equal(lab_dom_get(before, "context"), lab_dom_get(current, "context")) ||
        !lab_dom_equal(lab_dom_get(before, "main"), lab_dom_get(current, "main")))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, 4, "resultLoadState", 0, 1, 0);
}

static jsref lab_flow_result_initialise_result_startup_snapshot_4(struct lab_flow_result_initialise_context *ctx)
{
    return lab_flow_call(ctx->frame, 5, "resultStartupSnapshot", "current", 0, 0);
}

static jsref lab_flow_result_initialise_result_initial_evaluation(struct lab_flow_result_initialise_context *ctx)
{
    if (!lab_dom_equal(lab_dom_get(lab_dom_get(ctx->frame, "before"), "main"),
                       lab_dom_get(lab_dom_get(ctx->frame, "current"), "main")))
        return lab_flow_done(0);
    return lab_flow_call(ctx->frame, 10, "resultInitialEvaluation", 0, 1, 0);
}

static jsref lab_flow_result_initialise_result_startup_snapshot_6(struct lab_flow_result_initialise_context *ctx)
{
    return lab_flow_call(ctx->frame, 9, "resultStartupSnapshot", "current", 0, 0);
}

static jsref lab_flow_result_initialise_set_status(struct lab_flow_result_initialise_context *ctx)
{
    {
        int before = lab_dom_get(ctx->frame, "before"), current = lab_dom_get(ctx->frame, "current");
        if (lab_dom_equal(lab_dom_get(before, "context"), lab_dom_get(current, "context")) &&
            lab_dom_equal(lab_dom_get(before, "main"), lab_dom_get(current, "main")))
            return lab_flow_call(ctx->frame, 10, "setStatus", 0, 0, 1, lab_dom_get(ctx->frame, "error"));
        /* Continue: stale startup errors cannot replace the active request's status. */
    }
    return lab_flow_result_initialise_otherwise(ctx);
}

static jsref lab_flow_result_initialise_otherwise(struct lab_flow_result_initialise_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

static jsref lab_flow_result_initialise(frame_jsref frame)
{
    unsigned stage = (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
    struct lab_flow_result_initialise_context ctx = {
        .frame = frame,
    };
    static jsref (*const handlers[])(struct lab_flow_result_initialise_context *) = {
        [0] = lab_flow_result_initialise_result_startup_snapshot,
        [1] = lab_flow_result_initialise_result_restore_bounds,
        [2] = lab_flow_result_initialise_result_startup_snapshot_2,
        [3] = lab_flow_result_initialise_result_load_state,
        [4] = lab_flow_result_initialise_result_startup_snapshot_4,
        [5] = lab_flow_result_initialise_result_initial_evaluation,
        [8] = lab_flow_result_initialise_result_startup_snapshot_6,
        [9] = lab_flow_result_initialise_set_status,
    };
    unsigned selected = stage;
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](&ctx)
               : lab_flow_result_initialise_otherwise(&ctx);
}

/* Dispatch a bounded action number, leaving mathematical strings opaque throughout. */
struct lab_flow_result_context {
    lab_flow_result_kind_t kind;
    frame_jsref frame;
    view_jsref view;
};


static jsref lab_flow_result_dispatch_transfer(struct lab_flow_result_context *ctx)
{
    return lab_flow_result_transfer(ctx->frame);
}

static jsref lab_flow_result_dispatch_pretty(struct lab_flow_result_context *ctx)
{
    return lab_flow_result_pretty(ctx->frame, ctx->view);
}

static jsref lab_flow_result_dispatch_digits(struct lab_flow_result_context *ctx)
{
    return lab_flow_result_digits(ctx->frame);
}

static jsref lab_flow_result_dispatch_copy(struct lab_flow_result_context *ctx)
{
    return lab_flow_result_copy(ctx->kind == LAB_FLOW_RESULT_COPY_INPUT, ctx->frame);
}

static jsref lab_flow_result_dispatch_TeX(struct lab_flow_result_context *ctx)
{
    return lab_flow_result_TeX(ctx->frame);
}

static jsref lab_flow_result_dispatch_initialise(struct lab_flow_result_context *ctx)
{
    return lab_flow_result_initialise(ctx->frame);
}

static jsref lab_flow_result_otherwise(struct lab_flow_result_context *ctx)
{
    (void)ctx;
    return lab_flow_done(0);
}

/* Select a result action through the bounded native handler catalogue. */
jsref lab_flow_result(lab_flow_result_kind_t kind, frame_jsref frame, view_jsref view)
{
    struct lab_flow_result_context ctx = {
        .kind = kind,
        .frame = frame,
        .view = view,
    };
    static jsref (*const handlers[])(struct lab_flow_result_context *) = {
        [LAB_FLOW_RESULT_TRANSFER]      = lab_flow_result_dispatch_transfer,
        [LAB_FLOW_RESULT_PRETTY_MATRIX] = lab_flow_result_dispatch_pretty,
        [LAB_FLOW_RESULT_EXPAND_DIGITS] = lab_flow_result_dispatch_digits,
        [LAB_FLOW_RESULT_COPY_INPUT]    = lab_flow_result_dispatch_copy,
        [LAB_FLOW_RESULT_COPY_CARD]     = lab_flow_result_dispatch_copy,
        [LAB_FLOW_RESULT_RENDER_LATEX]  = lab_flow_result_dispatch_TeX,
        [LAB_FLOW_RESULT_INITIALISE]    = lab_flow_result_dispatch_initialise,
    };
    unsigned selected = (unsigned)kind;
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](&ctx)
                                                                               : lab_flow_result_otherwise(&ctx);
}
