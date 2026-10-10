/**
 * @file lab_flow_binding_edit.c
 * @brief Native binding projection and delayed edited-source refresh workflows.
 *
 * Coordinates native MARS metadata requests without interpreting mathematical
 * source. Exact editor snapshots and request ownership suppress stale results;
 * whitespace-insensitive matching is used only for the edited-body workflow.
 * Browser values are stored in request-local frames across asynchronous yields.
 */
#include "lab_dom.h"
#include "lab_flow.h"
#include "lab_flow_binding.h"

static unsigned lab_flow_binding_edit_stage(int frame)
{
    return (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
}

static int lab_flow_binding_edit_success(int frame)
{
    int result = lab_dom_get(frame, "result");
    return lab_dom_truth(lab_dom_get(lab_dom_get(result, "response"), "ok")) &&
           lab_dom_truth(lab_dom_get(lab_dom_get(result, "data"), "ok"));
}

static int lab_flow_binding_edit_matches(int frame, int trimmed)
{
    int text = lab_dom_get(lab_dom_get(frame, "after"), "text");
    return lab_dom_equal(trimmed ? lab_dom_clean(text, 0) : text,
                         lab_dom_get(frame, trimmed ? "editedBody" : "editorSnapshot"));
}

static int lab_flow_binding_edit_projection(int frame)
{
    enum {
        STATUS,
        FETCH,
        VALIDATE,
        OWNER,
        CAPTURE,
        BINDINGS,
        BODY_SELECT,
        BODY,
        ASSEMBLE,
        BODY_OWNER,
        BODY_CAPTURE,
        BODY_ACCEPT,
        HISTORY,
        SAVE,
        READY,
        DONE,
        ERROR_OWNER,
        ERROR_REPORT,
        FAILED
    };
    int data = lab_dom_get(lab_dom_get(frame, "result"), "data");
    switch (lab_flow_binding_edit_stage(frame)) {
        case STATUS:
            return lab_flow_call(frame, FETCH, "setStatus", 0, 0, 1, lab_dom_string("Updating bindings..."));
        case FETCH:
            lab_dom_set(frame, "failed", lab_dom_numeric(ERROR_OWNER));
            return lab_flow_call(frame, VALIDATE, "bindingFetch", "result", 1, 3, lab_dom_get(frame, "updated"),
                                 lab_dom_string(""), lab_dom_get(frame, "request"));
        case VALIDATE:
            if (!lab_flow_binding_edit_success(frame)) {
                int error = lab_dom_get(data, "error");
                return lab_flow_call(frame, FAILED, "fail", 0, 0, 1,
                                     lab_dom_truth(error) ? error
                                                          : lab_dom_string("MARS could not update the bindings"));
            }
            return lab_flow_call(frame, OWNER, "bindingPrepare", 0, 1, 1, lab_dom_get(frame, "updated"));
        case OWNER:
        case BODY_OWNER:
            return lab_flow_call(frame, lab_flow_binding_edit_stage(frame) == OWNER ? CAPTURE : BODY_CAPTURE,
                                 "requestCurrent", "current", 0, 1, lab_dom_get(frame, "request"));
        case CAPTURE:
        case BODY_CAPTURE:
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_done(lab_dom_scalar(1, 0));
            return lab_flow_call(frame, lab_flow_binding_edit_stage(frame) == CAPTURE ? BINDINGS : BODY_ACCEPT,
                                 "bindingCapture", "after", 0, 0);
        case BINDINGS: {
            if (!lab_flow_binding_edit_matches(frame, 0))
                return lab_flow_done(lab_dom_scalar(1, 0));
            int bindings = lab_dom_get(data, "binding_values");
            if (lab_dom_type(bindings) != 4)
                bindings = lab_dom_object(4);
            return lab_flow_call(frame, BODY_SELECT, "bindingAuthored", "bindings", 0, 2, bindings,
                                 lab_dom_get(frame, "updated"));
        }
        case BODY_SELECT:
            if (lab_dom_get(frame, "editorBodyText"))
                return lab_flow_call(frame, BODY, "bindingPrepare", 0, 1, 1, lab_dom_get(frame, "editorBodyText"));
            return lab_flow_call(frame, HISTORY, "bindingSetEditor", 0, 0, 4, lab_dom_get(frame, "updated"),
                                 lab_dom_get(frame, "bindings"), 0, lab_dom_get(data, "evaluation_ready"));
        case BODY:
            return lab_flow_call(frame, ASSEMBLE, "bindingBody", "editorBody", 0, 1,
                                 lab_dom_get(frame, "editorBodyText"));
        case ASSEMBLE:
            return lab_flow_call(frame, BODY_OWNER, "expressionWithBindings", "assembled", 1, 2,
                                 lab_dom_get(frame, "editorBody"), lab_dom_get(frame, "bindings"));
        case BODY_ACCEPT: {
            if (!lab_flow_binding_edit_matches(frame, 0))
                return lab_flow_done(lab_dom_scalar(1, 0));
            int assembled = lab_dom_get(frame, "assembled");
            if (!lab_dom_truth(assembled))
                assembled = lab_dom_get(frame, "editorBody");
            return lab_flow_call(frame, HISTORY, "bindingSetEditor", 0, 0, 4, assembled, lab_dom_get(frame, "bindings"),
                                 lab_dom_get(frame, "editorBody"), lab_dom_get(data, "evaluation_ready"));
        }
        case HISTORY:
            return lab_flow_call(frame, SAVE, "updateHistoryButtons", 0, 0, 0);
        case SAVE:
            return lab_flow_call(frame, READY, "bindingSaveMode", 0, 0, 0);
        case READY:
            return lab_flow_call(frame, DONE, "setStatus", 0, 0, 1, lab_dom_string("Ready"));
        case DONE:
            return lab_flow_done(lab_dom_scalar(1, 1));
        case ERROR_OWNER:
            return lab_flow_call(frame, ERROR_REPORT, "requestCurrent", "current", 0, 1, lab_dom_get(frame, "request"));
        case ERROR_REPORT:
            if (lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_call(frame, FAILED, "setStatus", 0, 0, 1, lab_dom_get(frame, "error"));
            return lab_flow_done(lab_dom_scalar(1, 0));
        default:
            return lab_flow_done(lab_dom_scalar(1, 0));
    }
}

static int lab_flow_binding_edit_apply(int frame)
{
    enum { INSPECT, AUTHORED, ASSEMBLE, OWNER, CAPTURE, APPLY, DONE };
    switch (lab_flow_binding_edit_stage(frame)) {
        case INSPECT:
            return lab_flow_call(frame, AUTHORED, "bindingInspectText", "inspected", 0, 1,
                                 lab_dom_get(frame, "editedBody"));
        case AUTHORED: {
            int inspected = lab_dom_get(frame, "inspected");
            int source = lab_dom_truth(lab_dom_get(inspected, "parts")) ? lab_dom_get(frame, "editedBody")
                                                                        : lab_dom_get(frame, "sourceExpression");
            return lab_flow_call(frame, ASSEMBLE, "bindingAuthored", "bindings", 0, 2,
                                 lab_dom_get(lab_dom_get(frame, "data"), "binding_values"), source);
        }
        case ASSEMBLE:
            return lab_flow_call(frame, OWNER, "expressionWithBindings", "assembled", 1, 2,
                                 lab_dom_get(lab_dom_get(frame, "inspected"), "body"), lab_dom_get(frame, "bindings"));
        case OWNER:
            return lab_flow_call(frame, CAPTURE, "requestCurrent", "current", 0, 1, lab_dom_get(frame, "request"));
        case CAPTURE:
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_done(0);
            return lab_flow_call(frame, APPLY, "bindingCapture", "after", 0, 0);
        case APPLY: {
            if (!lab_flow_binding_edit_matches(frame, 1))
                return lab_flow_done(0);
            int body = lab_dom_get(lab_dom_get(frame, "inspected"), "body"),
                assembled = lab_dom_get(frame, "assembled");
            if (!lab_dom_truth(assembled))
                assembled = body;
            return lab_flow_call(frame, DONE, "bindingProjectEdited", 0, 0, 4, assembled, body,
                                 lab_dom_get(frame, "bindings"), lab_dom_get(frame, "data"));
        }
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_binding_edit_refresh(int frame)
{
    enum {
        FETCH,
        OWNER,
        CAPTURE,
        VALIDATE,
        PREPARE_SOURCE,
        RECHECK,
        RECAPTURE,
        APPLY,
        HISTORY,
        DONE,
        ERROR_OWNER,
        ERROR_CAPTURE,
        ERROR_MARK
    };
    unsigned stage = lab_flow_binding_edit_stage(frame);
    switch (stage) {
        case FETCH:
            lab_dom_set(frame, "failed", lab_dom_numeric(ERROR_OWNER));
            return lab_flow_call(frame, OWNER, "bindingFetch", "result", 1, 3, lab_dom_get(frame, "editedBody"),
                                 lab_dom_get(frame, "sourceExpression"), lab_dom_get(frame, "request"));
        case OWNER:
        case RECHECK:
        case ERROR_OWNER:
            return lab_flow_call(frame,
                                 stage == OWNER     ? CAPTURE
                                 : stage == RECHECK ? RECAPTURE
                                                    : ERROR_CAPTURE,
                                 "requestCurrent", "current", 0, 1, lab_dom_get(frame, "request"));
        case CAPTURE:
        case RECAPTURE:
        case ERROR_CAPTURE:
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_done(0);
            return lab_flow_call(frame,
                                 stage == CAPTURE     ? VALIDATE
                                 : stage == RECAPTURE ? APPLY
                                                      : ERROR_MARK,
                                 "bindingCapture", "after", 0, 0);
        case VALIDATE:
            if (!lab_flow_binding_edit_matches(frame, 1))
                return lab_flow_done(0);
            if (!lab_flow_binding_edit_success(frame))
                return lab_flow_call(frame, HISTORY, "bindingMarkInvalid", 0, 0, 0);
            return lab_flow_call(frame, PREPARE_SOURCE, "bindingPrepare", 0, 1, 1, lab_dom_get(frame, "editedBody"));
        case PREPARE_SOURCE:
            return lab_flow_call(frame, RECHECK, "bindingPrepare", 0, 1, 1, lab_dom_get(frame, "sourceExpression"));
        case APPLY:
            if (!lab_flow_binding_edit_matches(frame, 1))
                return lab_flow_done(0);
            return lab_flow_call(frame, HISTORY, "bindingApplyEdited", 0, 1, 4, lab_dom_get(frame, "editedBody"),
                                 lab_dom_get(frame, "sourceExpression"),
                                 lab_dom_get(lab_dom_get(frame, "result"), "data"), lab_dom_get(frame, "request"));
        case ERROR_MARK:
            if (!lab_flow_binding_edit_matches(frame, 1))
                return lab_flow_done(0);
            return lab_flow_call(frame, HISTORY, "bindingMarkInvalid", 0, 0, 0);
        case HISTORY:
            return lab_flow_call(frame, DONE, "updateHistoryButtons", 0, 0, 0);
        default:
            return lab_flow_done(0);
    }
}

/* Keep editor workflows separately readable while sharing the binding flow range. */
int lab_flow_binding_edit(unsigned kind, int frame)
{
    static int (*const handlers[])(int) = {
        [0] = lab_flow_binding_edit_projection,
        [1] = lab_flow_binding_edit_apply,
        [2] = lab_flow_binding_edit_refresh,
    };
    return kind < sizeof handlers / sizeof *handlers ? handlers[kind](frame) : lab_flow_done(0);
}
