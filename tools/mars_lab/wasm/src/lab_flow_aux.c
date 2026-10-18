/**
 * @file lab_flow_aux.c
 * @brief Weather, solver-render and initial-worksheet browser continuations.
 *
 * Keeps small asynchronous workflows in native C while the browser owns transport,
 * promises and request-local frames. Weather failures preserve their own diagnostic
 * policy; solver failures propagate to the enclosing installation fallback. Startup
 * checks both request context and worksheet mode after refreshing DateTime location.
 * Native response values remain opaque and no scoped handle survives a return.
 */
#include "lab_dom.h"
#include "lab_requests.h"
#include "lab_flow.h"
#include "lab_flow_aux.h"

enum {
    LAB_FLOW_WEATHER_START,
    LAB_FLOW_WEATHER_FETCH,
    LAB_FLOW_WEATHER_OUTCOME,
    LAB_FLOW_WEATHER_RESPONSE,
    LAB_FLOW_WEATHER_EFFECTS,
    LAB_FLOW_WEATHER_DONE,
    LAB_FLOW_WEATHER_ERROR_CURRENT,
    LAB_FLOW_WEATHER_ERROR
};

static browser_jsref lab_flow_weather_start(frame_jsref frame, view_jsref view)
{
    (void)view;
    return lab_flow_call(frame, LAB_FLOW_WEATHER_FETCH, "setStatus", 0, 0, 1, lab_dom_string("Loading weather..."));
}

static browser_jsref lab_flow_weather_fetch(frame_jsref frame, view_jsref view)
{
    (void)view;
    lab_dom_set(frame, "failed", lab_dom_numeric(LAB_FLOW_WEATHER_ERROR_CURRENT));
    return lab_flow_call(frame, LAB_FLOW_WEATHER_OUTCOME, "fetchWeather", "result", 1, 2, lab_dom_get(frame, "state"),
                         lab_dom_get(frame, "request"));
}

static browser_jsref lab_flow_weather_outcome(frame_jsref frame, view_jsref view)
{
    (void)view;
    int result = lab_dom_get(frame, "result");
    return lab_flow_call(frame, LAB_FLOW_WEATHER_RESPONSE, "requestOutcome", "outcome", 0, 3,
                         lab_dom_get(frame, "request"), lab_dom_get(result, "response"), lab_dom_get(result, "data"));
}

static browser_jsref lab_flow_weather_response(frame_jsref frame, view_jsref view)
{
    (void)view;
    return lab_flow_call(frame, LAB_FLOW_WEATHER_EFFECTS, "native", "plan", 0, 4,
                         lab_dom_string("lab_evaluation_weather_response"), lab_dom_get(frame, "outcome"),
                         lab_dom_get(frame, "overviewData"), lab_dom_get(lab_dom_get(frame, "result"), "data"));
}

static browser_jsref lab_flow_weather_effects(frame_jsref frame, view_jsref view)
{
    (void)view;
    int plan = lab_dom_get(frame, "plan");
    return lab_dom_truth(plan) ? lab_flow_effects(frame, LAB_FLOW_WEATHER_DONE, plan) : lab_flow_done(0);
}

static browser_jsref lab_flow_weather_done(frame_jsref frame, view_jsref view)
{
    (void)frame;
    (void)view;
    return lab_flow_done(0);
}

static browser_jsref lab_flow_weather_error_current(frame_jsref frame, view_jsref view)
{
    (void)view;
    return lab_flow_call(frame, LAB_FLOW_WEATHER_ERROR, "requestCurrent", "current", 0, 1,
                         lab_dom_get(frame, "request"));
}

static browser_jsref lab_flow_weather_error(frame_jsref frame, view_jsref view)
{
    (void)view;
    if (!lab_dom_truth(lab_dom_get(frame, "current")))
        return lab_flow_done(0);
    return lab_flow_call(frame, LAB_FLOW_WEATHER_EFFECTS, "native", "plan", 0, 4,
                         lab_dom_string("lab_evaluation_weather_response"), lab_dom_numeric(LAB_REQUEST_OUTCOME_EXCEPTION), 0, 0);
}

/* Advance weather publication with stale-response suppression and a single recovery boundary. */
browser_jsref lab_flow_weather(frame_jsref frame, view_jsref view)
{
    static browser_jsref (*const stages[])(browser_jsref, browser_jsref) = {
        [LAB_FLOW_WEATHER_START]         = lab_flow_weather_start,
        [LAB_FLOW_WEATHER_FETCH]         = lab_flow_weather_fetch,
        [LAB_FLOW_WEATHER_OUTCOME]       = lab_flow_weather_outcome,
        [LAB_FLOW_WEATHER_RESPONSE]      = lab_flow_weather_response,
        [LAB_FLOW_WEATHER_EFFECTS]       = lab_flow_weather_effects,
        [LAB_FLOW_WEATHER_DONE]          = lab_flow_weather_done,
        [LAB_FLOW_WEATHER_ERROR_CURRENT] = lab_flow_weather_error_current,
        [LAB_FLOW_WEATHER_ERROR]         = lab_flow_weather_error,
    };
    double stage = lab_dom_number(lab_dom_get(frame, "stage"));
    return stage >= 0 && stage < sizeof stages / sizeof *stages && stage == (unsigned)stage
               ? stages[(unsigned)stage](frame, view)
               : lab_flow_done(0);
}

enum { LAB_FLOW_SOLVER_POST, LAB_FLOW_SOLVER_OUTCOME, LAB_FLOW_SOLVER_RESPONSE, LAB_FLOW_SOLVER_DONE };

static browser_jsref lab_flow_solver_post(frame_jsref frame, view_jsref view)
{
    (void)view;
    int payload = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(payload, "tex", lab_dom_get(frame, "source"));
    return lab_flow_call(frame, LAB_FLOW_SOLVER_OUTCOME, "post", "result", 1, 3, lab_dom_get(frame, "request"), payload,
                         lab_dom_string("/render_TeX"));
}

static browser_jsref lab_flow_solver_outcome(frame_jsref frame, view_jsref view)
{
    (void)view;
    int result = lab_dom_get(frame, "result");
    return lab_flow_call(frame, LAB_FLOW_SOLVER_RESPONSE, "requestOutcome", "outcome", 0, 3,
                         lab_dom_get(frame, "request"), lab_dom_get(result, "response"), lab_dom_get(result, "data"));
}

static browser_jsref lab_flow_solver_response(frame_jsref frame, view_jsref view)
{
    (void)view;
    int data = lab_dom_get(lab_dom_get(frame, "result"), "data");
    if (lab_dom_number(lab_dom_get(frame, "outcome")) == (double)LAB_REQUEST_OUTCOME_SUCCESS)
        return lab_flow_done(data);
    int message = lab_dom_get(data, "error");
    if (!lab_dom_truth(message))
        message = lab_dom_string("Could not render solver details");
    return lab_flow_call(frame, LAB_FLOW_SOLVER_DONE, "fail", 0, 0, 1, message);
}

static browser_jsref lab_flow_solver_done(frame_jsref frame, view_jsref view)
{
    (void)frame;
    (void)view;
    return lab_flow_done(0);
}

/* Let the request runner and enclosing installer own rejection and stale-parent fallback. */
browser_jsref lab_flow_solver(frame_jsref frame, view_jsref view)
{
    static browser_jsref (*const stages[])(browser_jsref, browser_jsref) = {
        [LAB_FLOW_SOLVER_POST]     = lab_flow_solver_post,
        [LAB_FLOW_SOLVER_OUTCOME]  = lab_flow_solver_outcome,
        [LAB_FLOW_SOLVER_RESPONSE] = lab_flow_solver_response,
        [LAB_FLOW_SOLVER_DONE]     = lab_flow_solver_done,
    };
    double stage = lab_dom_number(lab_dom_get(frame, "stage"));
    return stage >= 0 && stage < sizeof stages / sizeof *stages && stage == (unsigned)stage
               ? stages[(unsigned)stage](frame, view)
               : lab_flow_done(0);
}

enum {
    LAB_FLOW_STARTUP_MODE,
    LAB_FLOW_STARTUP_MODE_CHANGED,
    LAB_FLOW_STARTUP_CONTEXT,
    LAB_FLOW_STARTUP_REFRESH,
    LAB_FLOW_STARTUP_CURRENT_CONTEXT,
    LAB_FLOW_STARTUP_CURRENT_MODE,
    LAB_FLOW_STARTUP_EVALUATE,
    LAB_FLOW_STARTUP_DONE
};

static browser_jsref lab_flow_startup_mode(frame_jsref frame, view_jsref view)
{
    (void)view;
    return lab_flow_call(frame, LAB_FLOW_STARTUP_MODE_CHANGED, "currentMode", "mode", 0, 0);
}

static browser_jsref lab_flow_startup_mode_changed(frame_jsref frame, view_jsref view)
{
    (void)view;
    return lab_flow_call(frame, LAB_FLOW_STARTUP_CONTEXT, "modeChanged", 0, 0, 1, lab_dom_get(frame, "mode"));
}

static browser_jsref lab_flow_startup_context(frame_jsref frame, view_jsref view)
{
    (void)view;
    return lab_flow_call(frame, LAB_FLOW_STARTUP_REFRESH, "requestContext", "context", 0, 0);
}

static browser_jsref lab_flow_startup_evaluate_mode(frame_jsref frame)
{
    int mode = lab_dom_get(frame, "mode");
    /* This startup-only membership check has exactly seven fixed worksheet labels, never user-sized data. */
    static const char *const modes[] = {"expression", "equation", "diffequation", "matrix",
                                        "integrator", "datetime", "almanac"};
    for (unsigned i = 0; i < sizeof modes / sizeof *modes; ++i)
        if (lab_dom_equal(mode, lab_dom_string(modes[i])))
            return lab_flow_call(frame, LAB_FLOW_STARTUP_DONE, "evaluate", "result", 1, 1, mode);
    return lab_flow_call(frame, LAB_FLOW_STARTUP_DONE, "evaluate", "result", 1, 1, lab_dom_string("expression"));
}

static browser_jsref lab_flow_startup_refresh(frame_jsref frame, view_jsref view)
{
    (void)view;
    if (lab_dom_equal(lab_dom_get(frame, "mode"), lab_dom_string("datetime")))
        return lab_flow_call(frame, LAB_FLOW_STARTUP_CURRENT_CONTEXT, "refreshLocation", 0, 1, 0);
    return lab_flow_startup_evaluate_mode(frame);
}

static browser_jsref lab_flow_startup_current_context(frame_jsref frame, view_jsref view)
{
    (void)view;
    return lab_flow_call(frame, LAB_FLOW_STARTUP_CURRENT_MODE, "requestContext", "currentContext", 0, 0);
}

static browser_jsref lab_flow_startup_current_mode(frame_jsref frame, view_jsref view)
{
    (void)view;
    if (!lab_dom_equal(lab_dom_get(frame, "context"), lab_dom_get(frame, "currentContext")))
        return lab_flow_done(0);
    return lab_flow_call(frame, LAB_FLOW_STARTUP_EVALUATE, "currentMode", "currentMode", 0, 0);
}

static browser_jsref lab_flow_startup_evaluate(frame_jsref frame, view_jsref view)
{
    (void)view;
    if (!lab_dom_equal(lab_dom_get(frame, "currentMode"), lab_dom_string("datetime")))
        return lab_flow_done(0);
    return lab_flow_startup_evaluate_mode(frame);
}

static browser_jsref lab_flow_startup_done(frame_jsref frame, view_jsref view)
{
    (void)view;
    return lab_flow_done(lab_dom_get(frame, "result"));
}

/* Preserve initial-mode identity across location refresh without introducing a request of our own. */
browser_jsref lab_flow_startup(frame_jsref frame, view_jsref view)
{
    static browser_jsref (*const stages[])(browser_jsref, browser_jsref) = {
        [LAB_FLOW_STARTUP_MODE]            = lab_flow_startup_mode,
        [LAB_FLOW_STARTUP_MODE_CHANGED]    = lab_flow_startup_mode_changed,
        [LAB_FLOW_STARTUP_CONTEXT]         = lab_flow_startup_context,
        [LAB_FLOW_STARTUP_REFRESH]         = lab_flow_startup_refresh,
        [LAB_FLOW_STARTUP_CURRENT_CONTEXT] = lab_flow_startup_current_context,
        [LAB_FLOW_STARTUP_CURRENT_MODE]    = lab_flow_startup_current_mode,
        [LAB_FLOW_STARTUP_EVALUATE]        = lab_flow_startup_evaluate,
        [LAB_FLOW_STARTUP_DONE]            = lab_flow_startup_done,
    };
    double stage = lab_dom_number(lab_dom_get(frame, "stage"));
    return stage >= 0 && stage < sizeof stages / sizeof *stages && stage == (unsigned)stage
               ? stages[(unsigned)stage](frame, view)
               : lab_flow_done(0);
}
