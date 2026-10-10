/**
 * @file lab_flow_binding.c
 * @brief Binding request, freshness and control-event continuations for the Lab.
 *
 * Moves asynchronous binding decisions into C. Frames contain ordinary browser
 * values between yields, not retained handles. Each small switch is a fixed
 * continuation graph, rather than a catalogue lookup; workflows themselves use
 * direct indexed dispatch. Source and binding values remain opaque mathematics.
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

static int lab_flow_binding_sync(int frame)
{
    enum { PLAN, SELECT, PREPARED, ASSEMBLED, DONE };
    switch (lab_flow_binding_stage(frame)) {
        case PLAN:
            return lab_flow_call(frame, SELECT, "native", "plan", 0, 2, lab_dom_string("lab_binding_sync_request"),
                                 frame);
        case SELECT: {
            int plan = lab_dom_get(frame, "plan");
            if (lab_dom_truth(lab_dom_get(plan, "invalid")))
                return lab_flow_call(frame, PREPARED, "bindingInvalid", 0, 0, 0);
            int request = lab_dom_get(plan, "request");
            if (lab_dom_truth(request))
                return lab_flow_call(frame, ASSEMBLED, "bindingPresentation", "data", 1, 1, request);
            int source = lab_dom_get(plan, "source");
            if (lab_dom_truth(source))
                return lab_flow_call(frame, PREPARED, "bindingPrepare", 0, 1, 1, source);
            return lab_flow_done(source);
        }
        case PREPARED:
            return lab_flow_done(lab_dom_get(lab_dom_get(frame, "plan"), "source"));
        case ASSEMBLED:
            return lab_flow_call(frame, DONE, "bindingExpression", "expression", 0, 1, lab_dom_get(frame, "data"));
        case DONE:
            return lab_flow_done(lab_dom_get(frame, "expression"));
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_binding_visible(int frame)
{
    enum { CHECK, WAIT, RECHECK, COMMIT, DONE };
    switch (lab_flow_binding_stage(frame)) {
        case CHECK:
            return lab_flow_call(frame, WAIT, "bindingCurrent", "current", 0, 1, lab_dom_get(frame, "isCurrent"));
        case WAIT:
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_binding_false();
            return lab_flow_call(frame, RECHECK, "bindingPending", 0, 1, 0);
        case RECHECK:
            return lab_flow_call(frame, COMMIT, "bindingCurrent", "current", 0, 1, lab_dom_get(frame, "isCurrent"));
        case COMMIT:
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_binding_false();
            return lab_flow_call(frame, DONE, "bindingCommitVisible", "result", 1, 1, lab_dom_get(frame, "isCurrent"));
        case DONE:
            return lab_flow_done(lab_dom_get(frame, "result"));
        default:
            return lab_flow_binding_false();
    }
}

static int lab_flow_binding_commit(int frame)
{
    enum {
        CHECK,
        CAPTURE,
        SNAPSHOT,
        PAYLOAD,
        SEND,
        CALLBACK,
        OWNER,
        CAPTURE_AFTER,
        SNAPSHOT_CURRENT,
        ACCEPT,
        EXTRACTED,
        EFFECTS,
        DONE,
        EMPTY
    };
    int before = lab_dom_get(frame, "before");
    switch (lab_flow_binding_stage(frame)) {
        case CHECK:
            return lab_flow_call(frame, CAPTURE, "bindingCurrent", "current", 0, 1, lab_dom_get(frame, "isCurrent"));
        case CAPTURE:
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_binding_false();
            return lab_flow_call(frame, SNAPSHOT, "bindingCapture", "before", 0, 0);
        case SNAPSHOT:
            return lab_flow_call(frame, PAYLOAD, "native", "snapshot", 0, 2, lab_dom_string("lab_binding_snapshot"),
                                 lab_dom_get(frame, "inputs"));
        case PAYLOAD:
            return lab_flow_call(frame, SEND, "native", "payload", 0, 4, lab_dom_string("lab_binding_commit_request"),
                                 lab_dom_get(frame, "modeId"), lab_dom_get(frame, "source"),
                                 lab_dom_get(frame, "snapshot"));
        case SEND:
            if (!lab_dom_truth(lab_dom_get(frame, "payload")))
                return lab_flow_call(frame, EMPTY, "bindingPrepare", 0, 1, 1, lab_dom_get(frame, "source"));
            return lab_flow_call(frame, CALLBACK, "bindingPresentation", "data", 1, 2, lab_dom_get(frame, "payload"),
                                 lab_dom_get(frame, "request"));
        case CALLBACK:
            return lab_flow_call(frame, OWNER, "bindingCurrent", "current", 0, 1, lab_dom_get(frame, "isCurrent"));
        case OWNER:
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_binding_false();
            return lab_flow_call(frame, CAPTURE_AFTER, "requestCurrent", "current", 0, 1,
                                 lab_dom_get(frame, "request"));
        case CAPTURE_AFTER:
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_binding_false();
            return lab_flow_call(frame, SNAPSHOT_CURRENT, "bindingCapture", "after", 0, 0);
        case SNAPSHOT_CURRENT:
            if (!lab_dom_equal(lab_dom_get(before, "text"), lab_dom_get(lab_dom_get(frame, "after"), "text")) ||
                !lab_dom_equal(lab_dom_get(frame, "source"), lab_dom_get(lab_dom_get(frame, "after"), "source")))
                return lab_flow_binding_false();
            return lab_flow_call(frame, ACCEPT, "native", "current", 0, 2,
                                 lab_dom_string("lab_binding_snapshot_current"), lab_dom_get(frame, "snapshot"));
        case ACCEPT: {
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_binding_false();
            return lab_flow_call(frame, EXTRACTED, "bindingExpression", "updated", 0, 1, lab_dom_get(frame, "data"));
        }
        case EXTRACTED: {
            int context = lab_dom_object(5);
            lab_dom_set(context, "editor", lab_dom_get(before, "editor"));
            lab_dom_set(context, "snapshot", lab_dom_get(frame, "snapshot"));
            lab_dom_set(context, "isCurrent", lab_dom_get(frame, "isCurrent"));
            return lab_flow_call(frame, EFFECTS, "native", "plan", 0, 6, lab_dom_string("lab_binding_commit_plan"),
                                 lab_dom_get(frame, "modeId"), lab_dom_numeric(0), lab_dom_get(frame, "source"),
                                 lab_dom_get(frame, "updated"), context);
        }
        case EFFECTS:
            return lab_flow_call(frame, DONE, "bindingEffects", 0, 0, 1, lab_dom_get(frame, "plan"));
        case DONE:
            return lab_flow_done(lab_dom_get(lab_dom_get(frame, "plan"), "result"));
        default:
            return lab_flow_binding_false();
    }
}

static int lab_flow_binding_toggle(int frame)
{
    enum { CAPTURE, TOGGLE, SEND, CAPTURE_AFTER, ACCEPT, EFFECTS, DONE };
    int before = lab_dom_get(frame, "before");
    switch (lab_flow_binding_stage(frame)) {
        case CAPTURE:
            return lab_flow_call(frame, TOGGLE, "bindingCapture", "before", 0, 0);
        case TOGGLE:
            return lab_flow_call(frame, SEND, "native", "toggle", 0, 2, lab_dom_string("lab_binding_toggle"),
                                 lab_dom_get(frame, "binding"));
        case SEND: {
            int toggle = lab_dom_get(frame, "toggle"), source = lab_dom_get(before, "source");
            if (!lab_dom_truth(source) || !lab_dom_truth(lab_dom_get(toggle, "name")))
                return lab_flow_done(0);
            return lab_flow_call(frame, CAPTURE_AFTER, "bindingReplaceKind", "updated", 1, 3, source,
                                 lab_dom_get(toggle, "name"), lab_dom_get(toggle, "nextKind"));
        }
        case CAPTURE_AFTER:
            return lab_flow_call(frame, ACCEPT, "bindingCapture", "after", 0, 0);
        case ACCEPT: {
            if (!lab_flow_binding_same(frame, 1))
                return lab_flow_done(0);
            int context = lab_dom_object(5);
            lab_dom_set(context, "editor", lab_dom_get(before, "editor"));
            return lab_flow_call(frame, EFFECTS, "native", "plan", 0, 6, lab_dom_string("lab_binding_commit_plan"),
                                 lab_dom_get(before, "modeId"), lab_dom_numeric(1), lab_dom_get(before, "source"),
                                 lab_dom_get(frame, "updated"), context);
        }
        case EFFECTS: {
            int plan = lab_dom_get(frame, "plan");
            return lab_flow_call(frame, DONE, "bindingEffects", 0, lab_dom_truth(lab_dom_get(plan, "wait")), 1, plan);
        }
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_binding_refresh(int frame)
{
    enum { CHECK, CAPTURE, PREPARE, RECHECK, CAPTURE_AFTER, ACCEPT, DONE, ERROR_CHECK, ERROR_CAPTURE, ERROR_REPORT };
    int before = lab_dom_get(frame, "before");
    switch (lab_flow_binding_stage(frame)) {
        case CHECK:
            return lab_flow_call(frame, CAPTURE, "bindingCurrent", "current", 0, 1, lab_dom_get(frame, "isCurrent"));
        case CAPTURE:
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_done(0);
            return lab_flow_call(frame, PREPARE, "bindingCapture", "before", 0, 0);
        case PREPARE:
            if (lab_dom_number(lab_dom_get(before, "modeId")) == 0)
                return lab_flow_call(frame, DONE, "bindingScheduleRefresh", 0, 0, 0);
            lab_dom_set(frame, "failed", lab_dom_numeric(ERROR_CHECK));
            return lab_flow_call(frame, RECHECK, "bindingPrepare", 0, 1, 1, lab_dom_get(before, "source"));
        case RECHECK:
            lab_dom_delete(frame, "failed");
            return lab_flow_call(frame, CAPTURE_AFTER, "bindingCurrent", "current", 0, 1,
                                 lab_dom_get(frame, "isCurrent"));
        case CAPTURE_AFTER:
        case ERROR_CAPTURE:
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_done(0);
            return lab_flow_call(frame, lab_flow_binding_stage(frame) == CAPTURE_AFTER ? ACCEPT : ERROR_REPORT,
                                 "bindingCapture", "after", 0, 0);
        case ACCEPT:
            if (!lab_flow_binding_same(frame, 1))
                return lab_flow_done(0);
            return lab_flow_call(frame, DONE, "bindingProjectVisible", 0, 0, 1, lab_dom_get(before, "source"));
        case ERROR_CHECK:
            return lab_flow_call(frame, ERROR_CAPTURE, "bindingCurrent", "current", 0, 1,
                                 lab_dom_get(frame, "isCurrent"));
        case ERROR_REPORT:
            if (lab_flow_binding_same(frame, 0))
                return lab_flow_call(frame, DONE, "setStatus", 0, 0, 1, lab_dom_get(frame, "error"));
            return lab_flow_done(0);
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_binding_queue(int frame)
{
    enum { MODE, COMMIT, DONE };
    switch (lab_flow_binding_stage(frame)) {
        case MODE:
            return lab_flow_call(frame, COMMIT, "currentMode", "currentMode", 0, 0);
        case COMMIT:
            if (!lab_dom_equal(lab_dom_get(frame, "mode"), lab_dom_get(frame, "currentMode")) ||
                !lab_dom_truth(lab_dom_get(lab_dom_get(frame, "input"), "isConnected")))
                return lab_flow_binding_false();
            return lab_flow_call(frame, DONE, "bindingCommitInput", "result", 1, 1, lab_dom_get(frame, "input"));
        case DONE:
            return lab_flow_done(lab_dom_get(frame, "result"));
        default:
            return lab_flow_binding_false();
    }
}

static int lab_flow_binding_goal(int frame)
{
    enum { PREPARE, STARTS, PREPARE_RESULT, DONE };
    switch (lab_flow_binding_stage(frame)) {
        case PREPARE:
            return lab_flow_call(frame, STARTS, "bindingPrepare", "editor", 1, 1, lab_dom_get(frame, "source"));
        case STARTS:
            return lab_flow_call(frame, PREPARE_RESULT, "native", "prepared", 0, 3,
                                 lab_dom_string("lab_binding_goal_prepare"), lab_dom_get(frame, "editor"),
                                 lab_dom_get(frame, "providedStart"));
        case PREPARE_RESULT:
            return lab_flow_call(frame, DONE, "bindingPrepare", 0, 1, 1,
                                 lab_dom_get(lab_dom_get(frame, "prepared"), "expression"));
        case DONE:
            return lab_flow_done(lab_dom_get(frame, "prepared"));
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_binding_row(int frame)
{
    enum { COMMIT, PREPARE, RENDER, REFRESH, HISTORY, MODE, SAVE, DONE, ERROR };
    int item = lab_dom_get(frame, "item");
    switch (lab_flow_binding_stage(frame)) {
        case COMMIT:
            lab_dom_set(frame, "failed", lab_dom_numeric(ERROR));
            return lab_flow_call(frame, PREPARE, "commitBindings", 0, 1, 0);
        case PREPARE:
            if (!lab_dom_truth(lab_dom_get(item, "isConnected")))
                return lab_flow_done(0);
            return lab_flow_call(frame, RENDER, "bindingPrepareRows", "prepared", 1, 2, lab_dom_get(frame, "index"),
                                 lab_dom_get(frame, "operation"));
        case RENDER:
            if (!lab_dom_truth(lab_dom_get(frame, "prepared")) || !lab_dom_truth(lab_dom_get(item, "isConnected")))
                return lab_flow_done(0);
            return lab_flow_call(frame, REFRESH, "bindingRenderRows", 0, 0, 1, lab_dom_get(frame, "prepared"));
        case REFRESH:
            return lab_flow_call(frame, HISTORY, "bindingRefresh", 0, 0, 0);
        case HISTORY:
            return lab_flow_call(frame, MODE, "updateHistoryButtons", 0, 0, 0);
        case MODE:
            return lab_flow_call(frame, SAVE, "currentMode", "mode", 0, 0);
        case SAVE:
            if (lab_dom_equal(lab_dom_get(frame, "mode"), lab_dom_string("integrator")))
                return lab_flow_call(frame, DONE, "saveWorksheetState", 0, 0, 1, lab_dom_string("integrator"));
            return lab_flow_done(0);
        case ERROR:
            if (lab_dom_truth(lab_dom_get(item, "isConnected")))
                return lab_flow_call(frame, DONE, "bindingRowError", 0, 0, 2, item, lab_dom_get(frame, "exception"));
            return lab_flow_done(0);
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_binding_input_current(int frame)
{
    int input = lab_dom_get(frame, "input");
    return lab_dom_truth(lab_dom_get(input, "isConnected")) &&
           lab_dom_equal(lab_dom_read(input, 5, ""), lab_dom_get(frame, "authored"));
}

static int lab_flow_binding_prepare_input(int frame)
{
    enum { PREPARE, UPDATE, MODE, REFRESH, HISTORY, SAVE, DONE, ERROR };
    switch (lab_flow_binding_stage(frame)) {
        case PREPARE:
            lab_dom_set(frame, "authored", lab_dom_read(lab_dom_get(frame, "input"), 5, ""));
            lab_dom_set(frame, "failed", lab_dom_numeric(ERROR));
            return lab_flow_call(frame, UPDATE, "bindingRefreshForms", "prepared", 1, 0);
        case UPDATE:
            if (!lab_dom_truth(lab_dom_get(frame, "prepared")) || !lab_flow_binding_input_current(frame))
                return lab_flow_done(0);
            return lab_flow_call(frame, MODE, "bindingUpdateForms", 0, 0, 1, lab_dom_get(frame, "prepared"));
        case MODE:
            return lab_flow_call(frame, REFRESH, "currentMode", "mode", 0, 0);
        case REFRESH:
            if (!lab_dom_equal(lab_dom_get(frame, "mode"), lab_dom_string("integrator")))
                return lab_flow_done(0);
            return lab_flow_call(frame, HISTORY, "bindingRefresh", 0, 0, 0);
        case HISTORY:
            return lab_flow_call(frame, SAVE, "updateHistoryButtons", 0, 0, 0);
        case SAVE:
            return lab_flow_call(frame, DONE, "saveWorksheetState", 0, 0, 1, lab_dom_string("integrator"));
        case ERROR:
            if (lab_flow_binding_input_current(frame))
                return lab_flow_call(frame, DONE, "bindingValidity", 0, 0, 2, lab_dom_get(frame, "input"),
                                     lab_dom_get(frame, "exception"));
            return lab_flow_done(0);
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_binding_action(int frame)
{
    enum {
        START,
        CLEAR_COMMIT,
        CLEAR_FOCUS,
        EVALUATE,
        COPY_OK,
        COPY_STATUS,
        COPY_RESET,
        DONE,
        COPY_FAILED,
        COPY_ERROR
    };
    int card = lab_dom_get(frame, "card");
    switch (lab_flow_binding_stage(frame)) {
        case START: {
            unsigned operation = (unsigned)lab_dom_number(lab_dom_get(frame, "operation"));
            if (operation == 0)
                return lab_flow_call(frame, EVALUATE, "bindingCommitInput", 0, 1, 1, lab_dom_get(card, "input"));
            if (operation == 1)
                return lab_flow_call(frame, CLEAR_COMMIT, "updateHistoryButtons", 0, 0, 0);
            if (operation != 2)
                return lab_flow_done(0);
            lab_dom_set(frame, "failed", lab_dom_numeric(COPY_FAILED));
            return lab_flow_call(frame, COPY_OK, "bindingClipboard", 0, 1, 1,
                                 lab_dom_read(lab_dom_get(card, "input"), 5, ""));
        }
        case CLEAR_COMMIT:
            return lab_flow_call(frame, CLEAR_FOCUS, "bindingQueue", 0, 1, 1, lab_dom_get(card, "input"));
        case CLEAR_FOCUS:
            return lab_flow_call(frame, DONE, "bindingRefocus", 0, 0, 1, card);
        case EVALUATE:
            return lab_flow_call(frame, DONE, "bindingEvaluate", "result", 1, 0);
        case COPY_OK:
            return lab_flow_call(frame, COPY_STATUS, "bindingFlash", 0, 0, 2, lab_dom_get(card, "copy"),
                                 lab_dom_scalar(1, 1));
        case COPY_STATUS:
            return lab_flow_call(frame, COPY_RESET, "setStatus", 0, 0, 1, lab_dom_get(card, "message"));
        case COPY_RESET:
            return lab_flow_call(frame, DONE, "bindingReadyTimer", 0, 0, 0);
        case COPY_FAILED:
            return lab_flow_call(frame, COPY_ERROR, "bindingFlash", 0, 0, 2, lab_dom_get(card, "copy"),
                                 lab_dom_scalar(1, 0));
        case COPY_ERROR:
            return lab_flow_call(frame, DONE, "setStatus", 0, 0, 1, lab_dom_get(frame, "error"));
        default:
            return lab_flow_done(lab_dom_get(frame, "result"));
    }
}

static int lab_flow_binding_deferred(int frame)
{
    enum { PREPARE, CAPTURE, ACCEPT, DONE, ERROR_CAPTURE, ERROR_REPORT };
    switch (lab_flow_binding_stage(frame)) {
        case PREPARE:
            lab_dom_set(frame, "failed", lab_dom_numeric(ERROR_CAPTURE));
            return lab_flow_call(frame, CAPTURE, "bindingPrepare", 0, 1, 1, lab_dom_get(frame, "fullText"));
        case CAPTURE:
        case ERROR_CAPTURE:
            return lab_flow_call(frame, lab_flow_binding_stage(frame) == CAPTURE ? ACCEPT : ERROR_REPORT,
                                 "bindingCapture", "after", 0, 0);
        case ACCEPT:
        case ERROR_REPORT: {
            int after = lab_dom_get(frame, "after"), full = lab_dom_get(frame, "fullText");
            int source = lab_dom_clean(lab_dom_truth(full) ? lab_dom_text(full) : lab_dom_string(""), 0);
            if (!lab_dom_equal(lab_dom_get(frame, "mode"), lab_dom_get(after, "mode")) ||
                !lab_dom_equal(lab_dom_get(after, "source"), source))
                return lab_flow_done(0);
            if (lab_flow_binding_stage(frame) == ERROR_REPORT)
                return lab_flow_call(frame, DONE, "setStatus", 0, 0, 1, lab_dom_get(frame, "error"));
            return lab_flow_call(frame, DONE, "bindingSetEditor", 0, 0, 4, full,
                                 lab_dom_get(frame, "evaluatedBindings"), lab_dom_get(frame, "editorBodyText"),
                                 lab_dom_get(frame, "evaluationReady"));
        }
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_binding_begin_commit(int frame)
{
    enum { CHECK, CAPTURE, RUN, DONE };
    switch (lab_flow_binding_stage(frame)) {
        case CHECK:
            return lab_flow_call(frame, CAPTURE, "bindingCurrent", "current", 0, 1, lab_dom_get(frame, "isCurrent"));
        case CAPTURE:
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_binding_false();
            return lab_flow_call(frame, RUN, "bindingCapture", "before", 0, 0);
        case RUN:
            return lab_flow_call(frame, DONE, "bindingRunCommit", "result", 1, 1, frame);
        case DONE:
            return lab_flow_done(lab_dom_get(frame, "result"));
        default:
            return lab_flow_binding_false();
    }
}

static int lab_flow_binding_kind_request(int frame)
{
    enum { REQUEST, EXTRACT, DONE };
    switch (lab_flow_binding_stage(frame)) {
        case REQUEST: {
            int request = lab_dom_object(5);
            lab_dom_set(request, "action", lab_dom_string("editor"));
            lab_dom_set(request, "operation", lab_dom_string("kind"));
            lab_dom_set(request, "text", lab_dom_get(frame, "source"));
            lab_dom_set(request, "name", lab_dom_get(frame, "name"));
            lab_dom_set(request, "kind", lab_dom_get(frame, "nextKind"));
            return lab_flow_call(frame, EXTRACT, "bindingPresentation", "data", 1, 1, request);
        }
        case EXTRACT:
            return lab_flow_call(frame, DONE, "bindingExpression", "expression", 0, 1, lab_dom_get(frame, "data"));
        case DONE:
            return lab_flow_done(lab_dom_get(frame, "expression"));
        default:
            return lab_flow_done(0);
    }
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
