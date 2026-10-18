/**
 * @file lab_goal.c
 * @brief Ordered goal-seek completion and diagnostic policy for the Lab browser.
 *
 * Builds bounded browser-owned service plans for successful, rejected and failed
 * requests. Mathematical values remain opaque. Deferred services preserve getter
 * timing, history-before-editor ordering and browser ownership; no scoped handle
 * or native allocation survives a call. The adapter retains awaits and freshness checks.
 */
#include "lab_dom.h"
#include "lab_goal.h"

static void lab_goal_emit(browser_jsref calls, const char *service, unsigned count, browser_jsref first, browser_jsref second)
{
    int call = lab_dom_object(LAB_VALUE_OBJECT), args = lab_dom_object(LAB_VALUE_ARRAY);
    if (count)
        lab_dom_push(args, first);
    if (count > 1)
        lab_dom_push(args, second);
    lab_dom_set(call, "service", lab_dom_string(service));
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
}

/* Select completion services without changing browser state or inspecting mathematical text. */
void lab_goal_complete(lab_request_outcome_t outcome, browser_jsref data, browser_jsref completion, browser_jsref error)
{
    if (outcome == LAB_REQUEST_OUTCOME_STALE || (unsigned)outcome > LAB_REQUEST_OUTCOME_EXCEPTION) {
        lab_dom_return(0);
        return;
    }
    int plan = lab_dom_object(LAB_VALUE_OBJECT), calls = lab_dom_object(LAB_VALUE_ARRAY);
    unsigned success = outcome == LAB_REQUEST_OUTCOME_SUCCESS || outcome == LAB_REQUEST_OUTCOME_PARTIAL;
    lab_dom_set(plan, "calls", calls);
    lab_dom_set(plan, "result", lab_dom_scalar(LAB_VALUE_BOOLEAN, success));
    if (!success) {
        int message = outcome == LAB_REQUEST_OUTCOME_EXCEPTION ? error : lab_dom_get(data, "error");
        if (outcome != LAB_REQUEST_OUTCOME_EXCEPTION && !lab_dom_truth(message))
            message = lab_dom_string("Goal seek failed");
        lab_goal_emit(calls, "setRenderedError", 1, message, 0);
        lab_goal_emit(calls, "resetRenderedDigits", 0, 0, 0);
        int options = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(options, "keepBindings", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
        lab_goal_emit(calls, "clearResultDetails", 1, options, 0);
        lab_goal_emit(calls, "setStatus", 1, lab_dom_string("Error"), 0);
    } else {
        if (!lab_dom_truth(lab_dom_get(lab_dom_get(completion, "options"), "skipHistoryUpdate")))
            lab_goal_emit(calls, "captureHistory", 0, 0, 0);
        lab_goal_emit(calls, "setRenderedResult", 1, data, 0);
        lab_goal_emit(calls, "setEditor", 2, data, completion);
        lab_goal_emit(calls, "installCards", 1, completion, 0);
        lab_goal_emit(calls, "setLastInput", 1, completion, 0);
        lab_goal_emit(calls, "clearDerivative", 0, 0, 0);
        lab_goal_emit(calls, "setVariables", 1, data, 0);
        lab_goal_emit(calls, "setDifferentiable", 1, completion, 0);
        lab_goal_emit(calls, "renderDerivatives", 0, 0, 0);
        lab_goal_emit(calls, "setSource", 1, completion, 0);
        lab_goal_emit(calls, "setTarget", 1, completion, 0);
        lab_goal_emit(calls, "hideTargetEntry", 0, 0, 0);
        lab_goal_emit(calls, "setSuccessStatus", 1, completion, 0);
    }
    lab_dom_return(plan);
}

/* Read completion status only when its ordered service executes. */
void lab_goal_status(browser_jsref cards)
{
    const char *status = lab_dom_truth(lab_dom_get(cards, "unchanged")) ? "Goal already reached" : "Goal reached";
    lab_dom_return(lab_dom_string(status));
}
