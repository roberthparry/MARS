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

static int lab_flow_result_same_editor(int frame)
{
    int before = lab_dom_get(frame, "before"), current = lab_dom_get(frame, "current");
    return lab_dom_equal(lab_dom_get(before, "mode"), lab_dom_get(current, "mode")) &&
           lab_dom_equal(lab_dom_get(before, "context"), lab_dom_get(current, "context")) &&
           lab_dom_equal(lab_dom_get(before, "editor"), lab_dom_get(current, "editor"));
}

static int lab_flow_result_transfer(int frame)
{
    int text = lab_dom_get(frame, "text"), bindings = lab_dom_get(frame, "bindings");
    unsigned stage = (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
    switch (stage) {
        case 0:
            return lab_flow_call(frame, 1, "resultInput", "text", 0, 0);
        case 1:
            if (!lab_dom_truth(text))
                return lab_flow_done(0);
            return lab_flow_call(frame, 2, "resultSnapshot", "before", 0, 0);
        case 2:
            return lab_flow_call(frame, 3, "resultPrepare", 0, 1, 1, text);
        case 3:
            return lab_flow_call(frame, 4, "expressionText", "source", 0, 0);
        case 4:
            return lab_flow_call(frame, 5, "resultPrepare", 0, 1, 1, lab_dom_get(frame, "source"));
        case 5:
            return lab_flow_call(frame, 6, "resultSnapshot", "current", 0, 0);
        case 6:
            if (!lab_flow_result_same_editor(frame))
                return lab_flow_done(0);
            return lab_flow_call(frame, 7, "historyState", "history", 0, 1, 0);
        case 7:
            return lab_flow_call(frame, 8, "historyState", "nextHistory", 0, 1, text);
        case 8:
            return lab_flow_call(frame, 9, "resultHistoryEqual", "sameHistory", 0, 2, lab_dom_get(frame, "history"),
                                 lab_dom_get(frame, "nextHistory"));
        case 9:
            if (lab_dom_truth(lab_dom_get(lab_dom_get(frame, "history"), "text")) &&
                !lab_dom_truth(lab_dom_get(frame, "sameHistory")))
                return lab_flow_call(frame, 10, "pushHistory", 0, 0, 1, lab_dom_get(frame, "history"));
            /* Fall through: identical history is not pushed. */
        case 10:
            return lab_flow_call(frame, 11, "resultClearGoal", 0, 0, 0);
        case 11: {
            int mode = lab_dom_get(lab_dom_get(frame, "current"), "mode");
            if (lab_dom_equal(mode, lab_dom_string("matrix")))
                return lab_flow_call(frame, 12, "resultSourceBindings", "sourceRecord", 0, 0);
            if (lab_dom_equal(mode, lab_dom_string("equation")) || lab_dom_equal(mode, lab_dom_string("diffequation")))
                return lab_flow_call(frame, 21, "setExpressionEditor", 0, 0, 1, text);
            return lab_flow_call(frame, 20, "resultApplyBinding", "applied", 1, 2, text, text);
        }
        case 12: {
            int source = lab_dom_get(lab_dom_get(frame, "sourceRecord"), "bindings");
            lab_dom_set(frame, "bindings", lab_dom_truth(source) ? source : lab_dom_object(4));
            return lab_flow_call(frame, 13, "resultBindings", "resultBindings", 0, 0);
        }
        case 13:
            if (lab_dom_count(lab_dom_get(frame, "resultBindings")))
                return lab_flow_call(frame, 14, "expressionWithBindings", "boundSource", 1, 2, text, bindings);
            return lab_flow_call(frame, 16, "expressionWithBindings", "expression", 1, 2, text, bindings);
        case 14:
            return lab_flow_call(frame, 15, "resultAuthoredBindings", "bindings", 0, 2,
                                 lab_dom_get(frame, "resultBindings"), lab_dom_get(frame, "boundSource"));
        case 15:
            return lab_flow_call(frame, 16, "expressionWithBindings", "expression", 1, 2, text, bindings);
        case 16:
            return lab_flow_call(frame, 17, "resultSnapshot", "current", 0, 0);
        case 17:
            if (!lab_flow_result_same_editor(frame))
                return lab_flow_done(0);
            return lab_flow_call(frame, 18, "setExpressionEditor", 0, 0, 3, lab_dom_get(frame, "expression"), bindings,
                                 text);
        case 18:
            return lab_flow_call(frame, 21, "resultMatrixControls", 0, 0, 0);
        case 20:
            if (!lab_dom_truth(lab_dom_get(frame, "applied")))
                return lab_flow_done(0);
            /* Fall through: only a committed transfer is persisted. */
        case 21:
            return lab_flow_call(frame, 22, "resultSaveEditor", 0, 0, 0);
        case 22:
            return lab_flow_call(frame, 23, "updateHistoryButtons", 0, 0, 0);
        case 23:
            lab_dom_effect(lab_dom_id("expr"), 0);
            return lab_flow_call(frame, 24, "setStatus", 0, 0, 1, lab_dom_string("Result sent to input"));
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_result_pretty(int frame, int view)
{
    int element = lab_dom_get(frame, "element");
    switch ((unsigned)lab_dom_number(lab_dom_get(frame, "stage"))) {
        case 0:
            return lab_flow_call(frame, 1, "native", "request", 0, 6, lab_dom_string("lab_result_pretty_begin"),
                                 lab_dom_get(view, "caches"), element, lab_dom_get(frame, "button"),
                                 lab_dom_get(frame, "text"), lab_dom_get(frame, "pretty"));
        case 1: {
            int payload = lab_dom_object(5);
            lab_dom_set(payload, "action", lab_dom_string("matrix"));
            lab_dom_set(payload, "text", lab_dom_get(frame, "text"));
            lab_dom_set(frame, "failed", lab_dom_numeric(3));
            return lab_flow_call(frame, 2, "resultPresentation", "data", 1, 1, payload);
        }
        case 2:
            lab_dom_delete(frame, "failed");
            return lab_flow_call(frame, 3, "native", 0, 0, 4, lab_dom_string("lab_result_pretty_finish"), element,
                                 lab_dom_get(frame, "request"), lab_dom_get(frame, "data"));
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_result_digits(int frame)
{
    switch ((unsigned)lab_dom_number(lab_dom_get(frame, "stage"))) {
        case 0:
            return lab_flow_call(frame, 1, "resultLastTeX", "TeX", 0, 0);
        case 1:
            return lab_flow_call(frame, 2, "native", "request", 0, 2, lab_dom_string("lab_result_digits_begin"),
                                 lab_dom_get(frame, "TeX"));
        case 2:
            if (!lab_dom_truth(lab_dom_get(frame, "request")))
                return lab_flow_done(0);
            return lab_flow_call(frame, 3, "setStatus", 0, 0, 1, lab_dom_string("Rendering full TeX..."));
        case 3:
            lab_dom_set(frame, "failed", lab_dom_numeric(4));
            return lab_flow_call(frame, 4, "resultRenderTeX", "data", 1, 1,
                                 lab_dom_get(lab_dom_get(frame, "request"), "TeX"));
        case 4:
            lab_dom_delete(frame, "failed");
            return lab_flow_call(frame, 5, "native", "accepted", 0, 4, lab_dom_string("lab_result_digits_finish"),
                                 lab_dom_get(frame, "request"), lab_dom_get(frame, "data"),
                                 lab_dom_get(frame, "error") ? lab_dom_get(frame, "error") : lab_dom_string(""));
        case 5:
            if (lab_dom_truth(lab_dom_get(frame, "accepted")))
                return lab_flow_call(frame, 6, "setStatus", 0, 0, 1, lab_dom_string("Ready"));
            /* Fall through: obsolete render results have no visible completion. */
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_result_copy(unsigned input, int frame)
{
    int button = lab_dom_get(frame, "button");
    switch ((unsigned)lab_dom_number(lab_dom_get(frame, "stage"))) {
        case 0:
            if (input) {
                lab_dom_set(frame, "failed", lab_dom_numeric(8));
                return lab_flow_call(frame, 1, "commitBindings", 0, 1, 0);
            }
            return lab_flow_call(frame, 3, "resultCopyText", "text", 0, 1, button);
        case 1:
            return lab_flow_call(frame, 2, "currentMode", "mode", 0, 0);
        case 2:
            if (lab_dom_equal(lab_dom_get(frame, "mode"), lab_dom_string("datetime")))
                return lab_flow_call(frame, 3, "resultDatetimeSummary", "text", 0, 0);
            lab_dom_set(frame, "text", lab_dom_clean(lab_dom_read(lab_dom_id("expr"), 5, ""), 0));
            /* Fall through: expression clipboard source is the authored editor, not a result. */
        case 3:
            if (!lab_dom_truth(lab_dom_get(frame, "text")))
                return lab_flow_done(0);
            lab_dom_set(frame, "failed", lab_dom_numeric(8));
            return lab_flow_call(frame, 4, "resultClipboard", 0, 1, 1, lab_dom_get(frame, "text"));
        case 4:
            return lab_flow_call(frame, 5, "resultFlash", 0, 0, 2, button, lab_dom_scalar(1, 1));
        case 5:
            return lab_flow_call(frame, 6, "setStatus", 0, 0, 1, lab_dom_string(input ? "Copied input" : "Copied"));
        case 6:
            return lab_flow_call(frame, 10, "resultStatusLater", 0, 0, 2, lab_dom_string("Ready"),
                                 lab_dom_numeric(1000));
        case 8:
            return lab_flow_call(frame, 9, "resultFlash", 0, 0, 2, button, lab_dom_scalar(1, 0));
        case 9:
            return lab_flow_call(frame, 10, "setStatus", 0, 0, 1, lab_dom_get(frame, "error"));
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_result_TeX(int frame)
{
    switch ((unsigned)lab_dom_number(lab_dom_get(frame, "stage"))) {
        case 0: {
            int payload = lab_dom_object(5);
            lab_dom_set(payload, "tex", lab_dom_get(frame, "TeX"));
            return lab_flow_call(frame, 1, "resultFetch", "response", 1, 2, lab_dom_string("/render_TeX"), payload);
        }
        case 1:
            return lab_flow_call(frame, 2, "resultDecode", "data", 1, 1, lab_dom_get(frame, "response"));
        case 2: {
            int data = lab_dom_get(frame, "data"), error = lab_dom_get(data, "error");
            if (lab_dom_truth(lab_dom_get(lab_dom_get(frame, "response"), "ok")) &&
                lab_dom_truth(lab_dom_get(data, "ok")))
                return lab_flow_done(data);
            return lab_flow_call(frame, 3, "fail", 0, 0, 1,
                                 lab_dom_truth(error) ? error : lab_dom_string("Could not render TeX"));
        }
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_result_initialise(int frame)
{
    unsigned stage = (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
    switch (stage) {
        case 0:
            return lab_flow_call(frame, 1, "resultStartupSnapshot", "before", 0, 0);
        case 1:
            lab_dom_set(frame, "failed", lab_dom_numeric(8));
            return lab_flow_call(frame, 2, "resultRestoreBounds", "restored", 1, 0);
        case 2:
            if (!lab_dom_truth(lab_dom_get(frame, "restored")))
                return lab_flow_done(0);
            return lab_flow_call(frame, 3, "resultStartupSnapshot", "current", 0, 0);
        case 3: {
            int before = lab_dom_get(frame, "before"), current = lab_dom_get(frame, "current");
            if (!lab_dom_equal(lab_dom_get(before, "context"), lab_dom_get(current, "context")) ||
                !lab_dom_equal(lab_dom_get(before, "main"), lab_dom_get(current, "main")))
                return lab_flow_done(0);
            return lab_flow_call(frame, 4, "resultLoadState", 0, 1, 0);
        }
        case 4:
            return lab_flow_call(frame, 5, "resultStartupSnapshot", "current", 0, 0);
        case 5:
            if (!lab_dom_equal(lab_dom_get(lab_dom_get(frame, "before"), "main"),
                               lab_dom_get(lab_dom_get(frame, "current"), "main")))
                return lab_flow_done(0);
            return lab_flow_call(frame, 10, "resultInitialEvaluation", 0, 1, 0);
        case 8:
            return lab_flow_call(frame, 9, "resultStartupSnapshot", "current", 0, 0);
        case 9: {
            int before = lab_dom_get(frame, "before"), current = lab_dom_get(frame, "current");
            if (lab_dom_equal(lab_dom_get(before, "context"), lab_dom_get(current, "context")) &&
                lab_dom_equal(lab_dom_get(before, "main"), lab_dom_get(current, "main")))
                return lab_flow_call(frame, 10, "setStatus", 0, 0, 1, lab_dom_get(frame, "error"));
            /* Fall through: stale startup errors cannot replace the active request's status. */
        }
        default:
            return lab_flow_done(0);
    }
}

/* Dispatch a bounded action number, leaving mathematical strings opaque throughout. */
int lab_flow_result(unsigned kind, int frame, int view)
{
    switch (kind) {
        case 0:
            return lab_flow_result_transfer(frame);
        case 1:
            return lab_flow_result_pretty(frame, view);
        case 2:
            return lab_flow_result_digits(frame);
        case 3:
        case 4:
            return lab_flow_result_copy(kind == 3, frame);
        case 5:
            return lab_flow_result_TeX(frame);
        case 6:
            return lab_flow_result_initialise(frame);
        default:
            return lab_flow_done(0);
    }
}
