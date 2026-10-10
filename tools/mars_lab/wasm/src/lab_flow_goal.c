/**
 * @file lab_flow_goal.c
 * @brief Goal-seek browser continuation policy and ordered error recovery.
 *
 * Sequences binding commits, native goal requests and editor reconstruction,
 * checking request ownership after every asynchronous preparation. Existing goal
 * completion plans retain their deferred getter/effect ordering. The browser
 * owns promises and frames; this module retains no handles and never rewrites
 * mathematical source. Errors in recovery itself propagate to the outer UI guard.
 */
#include "lab_dom.h"
#include "lab_flow.h"
#include "lab_flow_aux.h"

enum {
    LAB_FLOW_GOAL_START,
    LAB_FLOW_GOAL_COMMIT,
    LAB_FLOW_GOAL_COMMIT_CURRENT,
    LAB_FLOW_GOAL_SOURCE,
    LAB_FLOW_GOAL_INPUT,
    LAB_FLOW_GOAL_PREPARE,
    LAB_FLOW_GOAL_PREPARE_CURRENT,
    LAB_FLOW_GOAL_EXPRESSION,
    LAB_FLOW_GOAL_PRECISION,
    LAB_FLOW_GOAL_POST,
    LAB_FLOW_GOAL_OUTCOME,
    LAB_FLOW_GOAL_RESPONSE,
    LAB_FLOW_GOAL_BODY,
    LAB_FLOW_GOAL_ASSEMBLE,
    LAB_FLOW_GOAL_ASSEMBLE_CURRENT,
    LAB_FLOW_GOAL_COMPLETE,
    LAB_FLOW_GOAL_EFFECTS,
    LAB_FLOW_GOAL_DONE,
    LAB_FLOW_GOAL_ERROR_CURRENT,
    LAB_FLOW_GOAL_ERROR
};

static int lab_flow_goal_start(int frame, int view)
{
    (void)view;
    return lab_flow_call(frame, LAB_FLOW_GOAL_COMMIT, "setStatus", 0, 0, 1, lab_dom_string("Goal seeking..."));
}

static int lab_flow_goal_commit(int frame, int view)
{
    (void)view;
    lab_dom_set(frame, "failed", lab_dom_numeric(LAB_FLOW_GOAL_ERROR_CURRENT));
    return lab_flow_call(frame, LAB_FLOW_GOAL_COMMIT_CURRENT, "commitBindings", 0, 1, 0);
}

static int lab_flow_goal_commit_current(int frame, int view)
{
    (void)view;
    return lab_flow_call(frame, LAB_FLOW_GOAL_SOURCE, "requestCurrent", "current", 0, 1, lab_dom_get(frame, "request"));
}

static int lab_flow_goal_input(int frame, int view)
{
    (void)view;
    return lab_flow_call(frame, LAB_FLOW_GOAL_PREPARE, "requestInput", "input", 0, 2, lab_dom_get(frame, "request"),
                         lab_dom_get(frame, "sourceText"));
}

static int lab_flow_goal_source(int frame, int view)
{
    if (!lab_dom_truth(lab_dom_get(frame, "current")))
        return lab_flow_done(lab_dom_scalar(1, 0));
    if (lab_dom_truth(lab_dom_get(lab_dom_get(frame, "options"), "commitBindings")))
        return lab_flow_call(frame, LAB_FLOW_GOAL_INPUT, "expressionText", "sourceText", 0, 0);
    return lab_flow_goal_input(frame, view);
}

static int lab_flow_goal_prepare(int frame, int view)
{
    (void)view;
    if (!lab_dom_truth(lab_dom_get(frame, "input")))
        return lab_flow_done(lab_dom_scalar(1, 0));
    return lab_flow_call(frame, LAB_FLOW_GOAL_PREPARE_CURRENT, "goalPrepare", "seek", 1, 2,
                         lab_dom_get(frame, "sourceText"), lab_dom_get(frame, "start"));
}

static int lab_flow_goal_prepare_current(int frame, int view)
{
    (void)view;
    return lab_flow_call(frame, LAB_FLOW_GOAL_EXPRESSION, "requestCurrent", "current", 0, 1,
                         lab_dom_get(frame, "request"));
}

static int lab_flow_goal_expression(int frame, int view)
{
    (void)view;
    if (!lab_dom_truth(lab_dom_get(frame, "current")))
        return lab_flow_done(lab_dom_scalar(1, 0));
    return lab_flow_call(frame, LAB_FLOW_GOAL_PRECISION, "goalExpression", "expression", 0, 1,
                         lab_dom_get(lab_dom_get(frame, "seek"), "expression"));
}

static int lab_flow_goal_precision(int frame, int view)
{
    (void)view;
    int payload = lab_dom_object(5);
    lab_dom_set(payload, "expression", lab_dom_get(frame, "expression"));
    lab_dom_set(payload, "target", lab_dom_get(frame, "target"));
    lab_dom_set(payload, "start", lab_dom_get(lab_dom_get(frame, "seek"), "start"));
    lab_dom_set(frame, "payload", payload);
    return lab_flow_call(frame, LAB_FLOW_GOAL_POST, "precision", "precision", 0, 0);
}

static int lab_flow_goal_post(int frame, int view)
{
    (void)view;
    int payload = lab_dom_get(frame, "payload");
    lab_dom_set(payload, "precision", lab_dom_get(frame, "precision"));
    return lab_flow_call(frame, LAB_FLOW_GOAL_OUTCOME, "post", "result", 1, 3, lab_dom_get(frame, "request"), payload,
                         lab_dom_string("/goal_seek"));
}

static int lab_flow_goal_outcome(int frame, int view)
{
    (void)view;
    int result = lab_dom_get(frame, "result");
    lab_dom_set(frame, "data", lab_dom_get(result, "data"));
    return lab_flow_call(frame, LAB_FLOW_GOAL_RESPONSE, "requestOutcome", "outcome", 0, 3,
                         lab_dom_get(frame, "request"), lab_dom_get(result, "response"), lab_dom_get(frame, "data"));
}

static int lab_flow_goal_response(int frame, int view)
{
    int outcome = lab_dom_get(frame, "outcome");
    if (!lab_dom_truth(outcome))
        return lab_flow_done(0);
    if (lab_dom_number(outcome) == 1)
        return lab_flow_call(frame, LAB_FLOW_GOAL_EFFECTS, "native", "plan", 0, 5, lab_dom_string("lab_goal_complete"),
                             outcome, lab_dom_get(frame, "data"), 0, 0);
    return lab_flow_call(frame, LAB_FLOW_GOAL_BODY, "native", "cards", 0, 5, lab_dom_string("lab_evaluation_cards"),
                         lab_dom_numeric(7), lab_dom_get(frame, "data"), lab_dom_get(frame, "sourceText"),
                         lab_dom_get(view, "caches"));
}

static int lab_flow_goal_body(int frame, int view)
{
    (void)view;
    return lab_flow_call(frame, LAB_FLOW_GOAL_ASSEMBLE, "goalEditorBody", "editorBody", 0, 1,
                         lab_dom_get(frame, "sourceText"));
}

static int lab_flow_goal_assemble(int frame, int view)
{
    (void)view;
    int bindings = lab_dom_get(lab_dom_get(frame, "data"), "binding_values");
    if (!lab_dom_truth(bindings))
        bindings = lab_dom_object(4);
    return lab_flow_call(frame, LAB_FLOW_GOAL_ASSEMBLE_CURRENT, "expressionWithBindings", "editorExpression", 1, 2,
                         lab_dom_get(frame, "editorBody"), bindings);
}

static int lab_flow_goal_assemble_current(int frame, int view)
{
    (void)view;
    if (!lab_dom_truth(lab_dom_get(frame, "editorExpression")))
        lab_dom_set(frame, "editorExpression", lab_dom_get(frame, "editorBody"));
    return lab_flow_call(frame, LAB_FLOW_GOAL_COMPLETE, "requestCurrent", "current", 0, 1,
                         lab_dom_get(frame, "request"));
}

static int lab_flow_goal_complete(int frame, int view)
{
    (void)view;
    if (!lab_dom_truth(lab_dom_get(frame, "current")))
        return lab_flow_done(lab_dom_scalar(1, 0));
    int context = lab_dom_object(5);
    /* The fixed completion record is copied once; fields remain opaque browser values. */
    static const char *const fields[] = {"cards", "editorBody", "editorExpression", "seek", "target", "options"};
    for (unsigned i = 0; i < sizeof fields / sizeof *fields; ++i)
        lab_dom_set(context, fields[i], lab_dom_get(frame, fields[i]));
    return lab_flow_call(frame, LAB_FLOW_GOAL_EFFECTS, "native", "plan", 0, 5, lab_dom_string("lab_goal_complete"),
                         lab_dom_get(frame, "outcome"), lab_dom_get(frame, "data"), context, 0);
}

static int lab_flow_goal_effects(int frame, int view)
{
    (void)view;
    return lab_flow_effects(frame, LAB_FLOW_GOAL_DONE, lab_dom_get(frame, "plan"));
}

static int lab_flow_goal_done(int frame, int view)
{
    (void)view;
    return lab_flow_done(lab_dom_get(lab_dom_get(frame, "plan"), "result"));
}

static int lab_flow_goal_error_current(int frame, int view)
{
    (void)view;
    return lab_flow_call(frame, LAB_FLOW_GOAL_ERROR, "requestCurrent", "current", 0, 1, lab_dom_get(frame, "request"));
}

static int lab_flow_goal_error(int frame, int view)
{
    (void)view;
    if (!lab_dom_truth(lab_dom_get(frame, "current")))
        return lab_flow_done(lab_dom_scalar(1, 0));
    return lab_flow_call(frame, LAB_FLOW_GOAL_EFFECTS, "native", "plan", 0, 5, lab_dom_string("lab_goal_complete"),
                         lab_dom_numeric(4), 0, 0, lab_dom_get(frame, "error"));
}

/* Advance one request-local goal continuation, without retaining handles between browser effects. */
int lab_flow_goal(int frame, int view)
{
    static int (*const stages[])(int, int) = {
        [LAB_FLOW_GOAL_START]            = lab_flow_goal_start,
        [LAB_FLOW_GOAL_COMMIT]           = lab_flow_goal_commit,
        [LAB_FLOW_GOAL_COMMIT_CURRENT]   = lab_flow_goal_commit_current,
        [LAB_FLOW_GOAL_SOURCE]           = lab_flow_goal_source,
        [LAB_FLOW_GOAL_INPUT]            = lab_flow_goal_input,
        [LAB_FLOW_GOAL_PREPARE]          = lab_flow_goal_prepare,
        [LAB_FLOW_GOAL_PREPARE_CURRENT]  = lab_flow_goal_prepare_current,
        [LAB_FLOW_GOAL_EXPRESSION]       = lab_flow_goal_expression,
        [LAB_FLOW_GOAL_PRECISION]        = lab_flow_goal_precision,
        [LAB_FLOW_GOAL_POST]             = lab_flow_goal_post,
        [LAB_FLOW_GOAL_OUTCOME]          = lab_flow_goal_outcome,
        [LAB_FLOW_GOAL_RESPONSE]         = lab_flow_goal_response,
        [LAB_FLOW_GOAL_BODY]             = lab_flow_goal_body,
        [LAB_FLOW_GOAL_ASSEMBLE]         = lab_flow_goal_assemble,
        [LAB_FLOW_GOAL_ASSEMBLE_CURRENT] = lab_flow_goal_assemble_current,
        [LAB_FLOW_GOAL_COMPLETE]         = lab_flow_goal_complete,
        [LAB_FLOW_GOAL_EFFECTS]          = lab_flow_goal_effects,
        [LAB_FLOW_GOAL_DONE]             = lab_flow_goal_done,
        [LAB_FLOW_GOAL_ERROR_CURRENT]    = lab_flow_goal_error_current,
        [LAB_FLOW_GOAL_ERROR]            = lab_flow_goal_error,
    };
    double stage = lab_dom_number(lab_dom_get(frame, "stage"));
    return stage >= 0 && stage < sizeof stages / sizeof *stages && stage == (unsigned)stage
               ? stages[(unsigned)stage](frame, view)
               : lab_flow_done(0);
}
