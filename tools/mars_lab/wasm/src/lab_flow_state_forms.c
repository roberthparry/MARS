/**
 * @file lab_flow_state_forms.c
 * @brief Guarded clearing and integrator forms workflows in native continuation plans.
 *
 * Preserves cancellation before clear-request acquisition, history before reset,
 * and the capture guard before clear completion. Forms workflows own revision
 * acceptance and asynchronous guard ordering. Browser services perform expression
 * reads outside native handle scopes, network calls and final metadata/DOM writes.
 */
#include "lab_dom.h"
#include "lab_rows.h"
#include "lab_flow.h"
#include "lab_flow_state_forms.h"

static unsigned lab_flow_state_forms_stage(frame_jsref frame)
{
    return (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
}

static jsref lab_flow_state_forms_next(frame_jsref frame, unsigned next, const char *service)
{
    return lab_flow_call(frame, next, service, 0, 0, 0);
}

static jsref lab_flow_state_forms_null(void)
{
    int plan = lab_flow_done(0);
    lab_dom_set(plan, "value", 0);
    return plan;
}

static jsref lab_flow_state_forms_guard(frame_jsref frame, unsigned next)
{
    return lab_flow_call(frame, next, "stateGuard", "current", 0, 1, lab_dom_get(frame, "isCurrent"));
}

static int lab_flow_state_forms_revision(frame_jsref frame)
{
    unsigned revision = lab_rows_revision_next();
    lab_dom_set(frame, "generation", lab_dom_numeric(revision));
    return revision != 0;
}

static jsref lab_flow_state_forms_exhausted(frame_jsref frame)
{
    return lab_flow_call(frame, 100, "fail", 0, 0, 1,
                         lab_dom_string("Integrator form revision limit reached; reload the Lab"));
}

static jsref lab_flow_state_forms_accept(frame_jsref frame)
{
    return lab_rows_revision_accept(
        (unsigned)lab_dom_number(lab_dom_get(frame, "generation")),
        lab_dom_equal(lab_dom_get(frame, "expression"), lab_dom_get(frame, "currentExpression")));
}

static jsref lab_flow_state_forms_payload(frame_jsref frame, jsref rows, jsref expression,
                                jsref fallback)
{
    int payload = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(payload, "action", lab_dom_string("integrator"));
    if (rows)
        lab_dom_set(payload, "rows", lab_dom_get(frame, "rows"));
    else {
        int text = lab_dom_get(frame, "text");
        lab_dom_set(payload, "text", lab_dom_text(lab_dom_truth(text) ? text : fallback));
    }
    if (expression)
        lab_dom_set(payload, "expression", lab_dom_get(frame, "expression"));
    return payload;
}

/**
 * @brief Discard forward history during worksheet clearing.
 * @param frame Borrowed clear-workflow frame advanced past history removal to stage 6.
 * @return Scoped plan calling the forward-history clear service.
 */
static jsref lab_flow_state_clear_state_clear_forward(frame_jsref frame);

/**
 * @brief Capture the cleared worksheet under the owning request.
 * @param frame Borrowed clear-workflow frame holding request and receiving the captured flag.
 * @return Scoped capture plan awaited before clearing the retained source.
 */
static jsref lab_flow_state_clear_state_capture_request(frame_jsref frame);

static jsref lab_flow_state_clear_state_clear_prelude(frame_jsref frame)
{
    return lab_flow_state_forms_next(frame, 1, "stateClearPrelude");
}

static jsref lab_flow_state_clear_state_clear_owned(frame_jsref frame)
{
    return lab_flow_call(frame, 2, "stateClearOwned", 0, 1, 0);
}

static jsref lab_flow_state_clear_complete(frame_jsref frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static jsref lab_flow_state_clear_state_current_history(frame_jsref frame)
{
    return lab_flow_call(frame, 4, "stateCurrentHistory", "history", 0, 0);
}

static jsref lab_flow_state_clear_push_history(frame_jsref frame)
{
    if (lab_dom_truth(lab_dom_get(lab_dom_get(frame, "history"), "text")))
        return lab_flow_call(frame, 5, "pushHistory", 0, 0, 1, lab_dom_get(frame, "history"));
    /* Continue for an empty worksheet. */
    return lab_flow_state_clear_state_clear_forward(frame);
}

static jsref lab_flow_state_clear_state_clear_forward(frame_jsref frame)
{
    return lab_flow_state_forms_next(frame, 6, "stateClearForward");
}

static jsref lab_flow_state_clear_state_clear_projection(frame_jsref frame)
{
    return lab_flow_call(frame, 7, "stateClearProjection", "reset", 0, 0);
}

static jsref lab_flow_state_clear_state_blank_bounds(frame_jsref frame)
{
    {
        unsigned reset = (unsigned)lab_dom_number(lab_dom_get(frame, "reset"));
        if (reset == 1)
            return lab_flow_state_forms_next(frame, 8, "stateBlankBounds");
        if (reset == 2)
            return lab_flow_state_forms_next(frame, 8, "stateResetCalendar");
        /* Continue after the mode-specific reset, if any. */
    }
    return lab_flow_state_clear_state_capture_request(frame);
}

static jsref lab_flow_state_clear_state_capture_request(frame_jsref frame)
{
    return lab_flow_call(frame, 9, "stateCaptureRequest", "captured", 1, 1, lab_dom_get(frame, "request"));
}

static jsref lab_flow_state_clear_state_clear_source(frame_jsref frame)
{
    if (!lab_dom_truth(lab_dom_get(frame, "captured")))
        return lab_flow_done(0);
    return lab_flow_state_forms_next(frame, 10, "stateClearSource");
}

static jsref lab_flow_state_clear_state_hide_target(frame_jsref frame)
{
    return lab_flow_state_forms_next(frame, 11, "stateHideTarget");
}

static jsref lab_flow_state_clear_state_clear_result(frame_jsref frame)
{
    return lab_flow_state_forms_next(frame, 12, "stateClearResult");
}

static jsref lab_flow_state_clear_state_save_result(frame_jsref frame)
{
    return lab_flow_state_forms_next(frame, 13, "stateSaveResult");
}

static jsref lab_flow_state_clear_commit_mode_state(frame_jsref frame)
{
    return lab_flow_state_forms_next(frame, 14, "commitModeState");
}

static jsref lab_flow_state_clear_state_history_buttons(frame_jsref frame)
{
    return lab_flow_state_forms_next(frame, 15, "stateHistoryButtons");
}

static jsref lab_flow_state_clear_state_status(frame_jsref frame)
{
    return lab_flow_call(frame, 16, "stateStatus", 0, 0, 1, lab_dom_string("Ready"));
}

static jsref lab_flow_state_clear_state_focus_editor(frame_jsref frame)
{
    return lab_flow_state_forms_next(frame, 17, "stateFocusEditor");
}

static jsref lab_flow_state_clear_otherwise(frame_jsref frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static jsref lab_flow_state_clear(frame_jsref frame)
{
    static jsref (*const handlers[])(jsref) = {
        [0]  = lab_flow_state_clear_state_clear_prelude,
        [1]  = lab_flow_state_clear_state_clear_owned,
        [2]  = lab_flow_state_clear_complete,
        [3]  = lab_flow_state_clear_state_current_history,
        [4]  = lab_flow_state_clear_push_history,
        [5]  = lab_flow_state_clear_state_clear_forward,
        [6]  = lab_flow_state_clear_state_clear_projection,
        [7]  = lab_flow_state_clear_state_blank_bounds,
        [8]  = lab_flow_state_clear_state_capture_request,
        [9]  = lab_flow_state_clear_state_clear_source,
        [10] = lab_flow_state_clear_state_hide_target,
        [11] = lab_flow_state_clear_state_clear_result,
        [12] = lab_flow_state_clear_state_save_result,
        [13] = lab_flow_state_clear_commit_mode_state,
        [14] = lab_flow_state_clear_state_history_buttons,
        [15] = lab_flow_state_clear_state_status,
        [16] = lab_flow_state_clear_state_focus_editor,
    };
    unsigned selected = lab_flow_state_forms_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected] ? handlers[selected](frame)
                                                                               : lab_flow_state_clear_otherwise(frame);
}

static jsref lab_flow_state_forms_refresh_state_forms_request(frame_jsref frame)
{
    if (!lab_flow_state_forms_revision(frame))
        return lab_flow_state_forms_exhausted(frame);
    return lab_flow_call(frame, 1, "stateFormsRequest", "result", 1, 1, lab_flow_state_forms_payload(frame, 1, 1, 0));
}

static jsref lab_flow_state_forms_refresh_state_forms_expression(frame_jsref frame)
{
    return lab_flow_call(frame, 2, "stateFormsExpression", "currentExpression", 0, 0);
}

static jsref lab_flow_state_forms_refresh_state_forms_metadata(frame_jsref frame)
{
    if (!lab_flow_state_forms_accept(frame))
        return lab_flow_state_forms_null();
    return lab_flow_call(frame, 3, "stateFormsMetadata", 0, 0, 2, lab_dom_get(frame, "expression"),
                         lab_dom_get(frame, "result"));
}

static jsref lab_flow_state_forms_refresh_state_forms_rows(frame_jsref frame)
{
    return lab_flow_call(frame, 4, "stateFormsRows", "completion", 0, 1, lab_dom_get(frame, "result"));
}

static jsref lab_flow_state_forms_refresh_complete(frame_jsref frame)
{
    int completion = lab_dom_get(frame, "completion");
    lab_dom_set(completion, "done", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
    return completion;
}

static jsref lab_flow_state_forms_refresh_otherwise(frame_jsref frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static jsref lab_flow_state_forms_refresh(frame_jsref frame)
{
    static jsref (*const handlers[])(jsref) = {
        [0] = lab_flow_state_forms_refresh_state_forms_request,
        [1] = lab_flow_state_forms_refresh_state_forms_expression,
        [2] = lab_flow_state_forms_refresh_state_forms_metadata,
        [3] = lab_flow_state_forms_refresh_state_forms_rows,
        [4] = lab_flow_state_forms_refresh_complete,
    };
    unsigned selected = lab_flow_state_forms_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](frame)
               : lab_flow_state_forms_refresh_otherwise(frame);
}

static jsref lab_flow_state_forms_parse_state_forms_request(frame_jsref frame)
{
    return lab_flow_call(frame, 1, "stateFormsRequest", "result", 1, 1,
                         lab_flow_state_forms_payload(frame, 0, 0, lab_dom_string("")));
}

static jsref lab_flow_state_forms_parse_state_forms_rows(frame_jsref frame)
{
    return lab_flow_call(frame, 2, "stateFormsRows", "completion", 0, 1, lab_dom_get(frame, "result"));
}

static jsref lab_flow_state_forms_parse_complete(frame_jsref frame)
{
    int completion = lab_dom_get(frame, "completion");
    lab_dom_set(completion, "done", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
    return completion;
}

static jsref lab_flow_state_forms_parse_otherwise(frame_jsref frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static jsref lab_flow_state_forms_parse(frame_jsref frame)
{
    static jsref (*const handlers[])(jsref) = {
        [0] = lab_flow_state_forms_parse_state_forms_request,
        [1] = lab_flow_state_forms_parse_state_forms_rows,
        [2] = lab_flow_state_forms_parse_complete,
    };
    unsigned selected = lab_flow_state_forms_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](frame)
               : lab_flow_state_forms_parse_otherwise(frame);
}

static jsref lab_flow_state_forms_restore_guard(frame_jsref frame)
{
    return lab_flow_state_forms_guard(frame, 1);
}

static jsref lab_flow_state_forms_restore_state_forms_expression(frame_jsref frame)
{
    if (!lab_dom_truth(lab_dom_get(frame, "current")))
        return lab_flow_done(lab_dom_scalar(LAB_VALUE_BOOLEAN, 0));
    if (!lab_flow_state_forms_revision(frame))
        return lab_flow_state_forms_exhausted(frame);
    return lab_flow_call(frame, 2, "stateFormsExpression", "expression", 0, 0);
}

static jsref lab_flow_state_forms_restore_state_forms_request(frame_jsref frame)
{
    return lab_flow_call(frame, 3, "stateFormsRequest", "result", 1, 1,
                         lab_flow_state_forms_payload(frame, 0, 1, lab_dom_get(frame, "defaultBounds")));
}

static jsref lab_flow_state_forms_restore_recheck(frame_jsref frame)
{
    return lab_flow_state_forms_guard(frame, 4);
}

static jsref lab_flow_state_forms_restore_state_forms_expression_4(frame_jsref frame)
{
    if (!lab_dom_truth(lab_dom_get(frame, "current")))
        return lab_flow_done(lab_dom_scalar(LAB_VALUE_BOOLEAN, 0));
    return lab_flow_call(frame, 5, "stateFormsExpression", "currentExpression", 0, 0);
}

static jsref lab_flow_state_forms_restore_state_forms_metadata(frame_jsref frame)
{
    if (!lab_flow_state_forms_accept(frame))
        return lab_flow_done(lab_dom_scalar(LAB_VALUE_BOOLEAN, 0));
    return lab_flow_call(frame, 6, "stateFormsMetadata", 0, 0, 2, lab_dom_get(frame, "expression"),
                         lab_dom_get(frame, "result"));
}

static jsref lab_flow_state_forms_restore_state_forms_rows(frame_jsref frame)
{
    return lab_flow_call(frame, 7, "stateFormsRows", "rows", 0, 1, lab_dom_get(frame, "result"));
}

static jsref lab_flow_state_forms_restore_state_forms_render(frame_jsref frame)
{
    return lab_flow_call(frame, 8, "stateFormsRender", 0, 0, 1, lab_dom_get(frame, "rows"));
}

static jsref lab_flow_state_forms_restore_complete(frame_jsref frame)
{
    (void)frame;
    return lab_flow_done(lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
}

static jsref lab_flow_state_forms_restore_otherwise(frame_jsref frame)
{
    (void)frame;
    return lab_flow_done(0);
}

static jsref lab_flow_state_forms_restore(frame_jsref frame)
{
    static jsref (*const handlers[])(jsref) = {
        [0] = lab_flow_state_forms_restore_guard,
        [1] = lab_flow_state_forms_restore_state_forms_expression,
        [2] = lab_flow_state_forms_restore_state_forms_request,
        [3] = lab_flow_state_forms_restore_recheck,
        [4] = lab_flow_state_forms_restore_state_forms_expression_4,
        [5] = lab_flow_state_forms_restore_state_forms_metadata,
        [6] = lab_flow_state_forms_restore_state_forms_rows,
        [7] = lab_flow_state_forms_restore_state_forms_render,
        [8] = lab_flow_state_forms_restore_complete,
    };
    unsigned selected = lab_flow_state_forms_stage(frame);
    return selected < sizeof handlers / sizeof *handlers && handlers[selected]
               ? handlers[selected](frame)
               : lab_flow_state_forms_restore_otherwise(frame);
}

/* Four distinct workflows share revision policy without retaining browser handles. */
jsref lab_flow_state_forms(lab_flow_state_forms_kind_t kind, frame_jsref frame, view_jsref view)
{
    (void)view;
    static jsref (*const handlers[])(jsref) = {
        [LAB_FLOW_STATE_FORMS_CLEAR]          = lab_flow_state_clear,
        [LAB_FLOW_STATE_FORMS_REFRESH]        = lab_flow_state_forms_refresh,
        [LAB_FLOW_STATE_FORMS_PARSE_BOUNDS]   = lab_flow_state_forms_parse,
        [LAB_FLOW_STATE_FORMS_RESTORE_BOUNDS] = lab_flow_state_forms_restore,
    };
    return (unsigned)kind < sizeof handlers / sizeof *handlers ? handlers[kind](frame) : lab_flow_done(0);
}
