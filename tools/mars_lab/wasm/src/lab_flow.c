/**
 * @file lab_flow.c
 * @brief Browser-owned frames and native asynchronous continuation dispatch.
 *
 * Provides small operation builders and directly indexed workflow dispatch. Each
 * call borrows only scoped handles; continuations retain ordinary browser values
 * in per-request frames. The host interprets waits and exceptions, while native
 * workflows select branches, recovery and ownership. No mathematical processing
 * or transport encoding is performed here.
 */
#include <stdarg.h>

#include "lab_mode.h"
#include "lab_dom.h"
#include "lab_bootstrap.h"
#include "lab_flow_aux.h"
#include "lab_flow_binding.h"
#include "lab_flow_location.h"
#include "lab_flow_request.h"
#include "lab_flow_result.h"
#include "lab_flow_state.h"
#include "lab_flow_state_forms.h"
#include "lab_flow_transport.h"
#include "lab_flow.h"

/* Arguments are the bounded capability signature, not a searched catalogue. */
browser_jsref lab_flow_call(frame_jsref frame, unsigned next, const char *service, const char *save, bool wait,
                             unsigned count, ...)
{
    int plan = lab_dom_object(LAB_VALUE_OBJECT), calls = lab_dom_object(LAB_VALUE_ARRAY),
        call = lab_dom_object(LAB_VALUE_OBJECT), args = lab_dom_object(LAB_VALUE_ARRAY);
    va_list values;
    va_start(values, count);
    for (unsigned i = 0; i < count; ++i)
        lab_dom_push(args, va_arg(values, browser_jsref));
    va_end(values);
    lab_dom_set(frame, "stage", lab_dom_numeric(next));
    lab_dom_set(call, "service", lab_dom_string(service));
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
    lab_dom_set(plan, "calls", calls);
    lab_dom_set(plan, "wait", lab_dom_scalar(LAB_VALUE_BOOLEAN, wait));
    if (save)
        lab_dom_set(plan, "save", lab_dom_string(save));
    return plan;
}

/* Existing planners keep their metadata; only their ordered effects are executed. */
browser_jsref lab_flow_effects(frame_jsref frame, unsigned next, browser_jsref plan)
{
    return lab_flow_call(frame, next, "effects", 0, lab_dom_truth(lab_dom_get(plan, "wait")), 1, plan);
}

/* Absence and an explicit false result remain distinct to callers. */
browser_jsref lab_flow_done(browser_jsref value)
{
    int plan = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(plan, "done", lab_dom_scalar(LAB_VALUE_BOOLEAN, 1));
    if (value)
        lab_dom_set(plan, "value", value);
    return plan;
}

/* Request operations and mode ownership are policy, rather than browser branches. */
void lab_flow_begin(lab_flow_kind_t kind, frame_jsref frame)
{
    static const struct {
        const char *operation;
        lab_mode_t mode;
    } requests[] = {
        [LAB_FLOW_KIND_EVALUATE] = {"evaluate",     LAB_MODE_EXPRESSION},
        [LAB_FLOW_KIND_GOAL]     = {"goal",         LAB_MODE_EXPRESSION},
        [LAB_FLOW_KIND_WEATHER]  = {"weather",      LAB_MODE_DATETIME},
        [LAB_FLOW_KIND_SOLVER]   = {"solverRender", LAB_MODE_DIFFERENTIAL_EQUATION},
    };
    static const char *const modes[] = {"expression", "equation", "diffequation", "matrix",
                                        "integrator", "datetime", "almanac"};
    if ((unsigned)kind > LAB_FLOW_KIND_INSTALL) {
        lab_dom_return(0);
        return;
    }
    int plan = lab_dom_object(LAB_VALUE_OBJECT);
    if ((unsigned)kind < LAB_FLOW_KIND_STARTUP) {
        double selected = kind == LAB_FLOW_KIND_EVALUATE ? lab_dom_number(lab_dom_get(frame, "mode"))
                                                        : (double)requests[kind].mode;
        if (!(selected >= (double)LAB_MODE_EXPRESSION && selected <= (double)LAB_MODE_ALMANAC) ||
            selected != (unsigned)selected) {
            lab_dom_return(0);
            return;
        }
        lab_mode_t mode = (lab_mode_t)selected;
        int options = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(options, "parent", lab_dom_get(frame, "parent"));
        lab_dom_set(plan, "operation", lab_dom_string(requests[kind].operation));
        lab_dom_set(plan, "mode", lab_dom_string(modes[mode]));
        lab_dom_set(plan, "options", options);
        lab_dom_set(plan, "ui", lab_dom_scalar(LAB_VALUE_BOOLEAN, kind != LAB_FLOW_KIND_SOLVER));
    }
    lab_dom_return(plan);
}

/* Translate the public workflow selector into the request module's local selector. */
static browser_jsref dispatch_request(lab_flow_kind_t kind, frame_jsref frame, view_jsref view)
{
    return lab_flow_request((lab_flow_request_kind_t)(kind - LAB_FLOW_KIND_REQUEST_FUNCTION), frame, view);
}

/* Translate the public workflow selector into the state module's local selector. */
static browser_jsref dispatch_state(lab_flow_kind_t kind, frame_jsref frame, view_jsref view)
{
    return lab_flow_state((lab_flow_state_kind_t)(kind - LAB_FLOW_KIND_STATE_EDITORS), frame, view);
}

/* Translate the public workflow selector into the location module's local selector. */
static browser_jsref dispatch_location(lab_flow_kind_t kind, frame_jsref frame, view_jsref view)
{
    return lab_flow_location((lab_flow_location_kind_t)(kind - LAB_FLOW_KIND_LOCATION_FORMS), frame, view);
}

/* Translate the public workflow selector into the binding module's local selector. */
static browser_jsref dispatch_binding(lab_flow_kind_t kind, frame_jsref frame, view_jsref view)
{
    return lab_flow_binding((lab_flow_binding_kind_t)(kind - LAB_FLOW_KIND_BINDING_SYNC), frame, view);
}

/* Translate the public workflow selector into the result module's local selector. */
static browser_jsref dispatch_result(lab_flow_kind_t kind, frame_jsref frame, view_jsref view)
{
    return lab_flow_result((lab_flow_result_kind_t)(kind - LAB_FLOW_KIND_RESULT_TRANSFER), frame, view);
}

/* Translate the public workflow selector into the transport module's local selector. */
static browser_jsref dispatch_transport(lab_flow_kind_t kind, frame_jsref frame, view_jsref view)
{
    return lab_flow_transport((lab_flow_transport_kind_t)(kind - LAB_FLOW_KIND_TRANSPORT_POST), frame, view);
}

/* Translate the public workflow selector into the forms module's local selector. */
static browser_jsref dispatch_forms(lab_flow_kind_t kind, frame_jsref frame, view_jsref view)
{
    return lab_flow_state_forms((lab_flow_state_forms_kind_t)(kind - LAB_FLOW_KIND_STATE_FORMS_CLEAR), frame, view);
}

/* Bootstrap has no local workflow selector. */
static browser_jsref dispatch_bootstrap(lab_flow_kind_t kind, frame_jsref frame, view_jsref view)
{
    (void)kind;
    return lab_bootstrap_step(frame, view);
}

/* Fixed dispatch tables keep independent workflows in their owning C modules; holes finish without a value. */
void lab_flow_step(lab_flow_kind_t kind, frame_jsref frame, view_jsref view)
{
    static browser_jsref (*const handlers[])(browser_jsref, browser_jsref) = {
        [LAB_FLOW_KIND_EVALUATE] = lab_flow_evaluation,
        [LAB_FLOW_KIND_GOAL]     = lab_flow_goal,
        [LAB_FLOW_KIND_WEATHER]  = lab_flow_weather,
        [LAB_FLOW_KIND_SOLVER]   = lab_flow_solver,
        [LAB_FLOW_KIND_STARTUP]  = lab_flow_startup,
        [LAB_FLOW_KIND_INSTALL]  = lab_flow_install,
    };
    double stage = lab_dom_number(lab_dom_get(frame, "stage"));
    double mode = lab_dom_number(lab_dom_get(frame, "mode"));
    int needs_mode = kind == LAB_FLOW_KIND_EVALUATE || kind == LAB_FLOW_KIND_INSTALL;
    if (!(stage >= 0 && stage <= 65535) || stage != (unsigned)stage ||
        (needs_mode && (!(mode >= (double)LAB_MODE_EXPRESSION && mode <= (double)LAB_MODE_ALMANAC) || mode != (unsigned)mode))) {
        lab_dom_return(lab_flow_done(0));
        return;
    }
    static browser_jsref (*const routes[])(lab_flow_kind_t, browser_jsref, browser_jsref) = {
        [LAB_FLOW_KIND_REQUEST_FUNCTION]           = dispatch_request,
        [LAB_FLOW_KIND_REQUEST_CALCULUS]           = dispatch_request,
        [LAB_FLOW_KIND_REQUEST_INTEGRATOR]         = dispatch_request,
        [LAB_FLOW_KIND_REQUEST_LAND]               = dispatch_request,
        [LAB_FLOW_KIND_REQUEST_HOLIDAYS]           = dispatch_request,
        [LAB_FLOW_KIND_REQUEST_MOBILE]             = dispatch_request,
        [LAB_FLOW_KIND_REQUEST_FORMS]              = dispatch_request,
        [LAB_FLOW_KIND_REQUEST_PRESENTATION]       = dispatch_request,
        [LAB_FLOW_KIND_REQUEST_SOLVER]             = dispatch_request,
        [LAB_FLOW_KIND_REQUEST_FETCH]              = dispatch_request,
        [LAB_FLOW_KIND_STATE_EDITORS]              = dispatch_state,
        [LAB_FLOW_KIND_STATE_CONTROLS]             = dispatch_state,
        [LAB_FLOW_KIND_STATE_SAVED]                = dispatch_state,
        [LAB_FLOW_KIND_STATE_RECOVERY]             = dispatch_state,
        [LAB_FLOW_KIND_STATE_LOAD]                 = dispatch_state,
        [LAB_FLOW_KIND_STATE_HISTORY]              = dispatch_state,
        [LAB_FLOW_KIND_STATE_NAVIGATION]           = dispatch_state,
        [LAB_FLOW_KIND_STATE_CAPTURE]              = dispatch_state,
        [LAB_FLOW_KIND_STATE_SELECTION]            = dispatch_state,
        [LAB_FLOW_KIND_STATE_PRECISION]            = dispatch_state,
        [LAB_FLOW_KIND_LOCATION_FORMS]             = dispatch_location,
        [LAB_FLOW_KIND_LOCATION_RESTORE]           = dispatch_location,
        [LAB_FLOW_KIND_LOCATION_POPULATE]          = dispatch_location,
        [LAB_FLOW_KIND_LOCATION_SYNC]              = dispatch_location,
        [LAB_FLOW_KIND_LOCATION_HISTORY]           = dispatch_location,
        [LAB_FLOW_KIND_LOCATION_TOTALITY]          = dispatch_location,
        [LAB_FLOW_KIND_LOCATION_REFRESH]           = dispatch_location,
        [LAB_FLOW_KIND_LOCATION_AUTO_EVALUATE]     = dispatch_location,
        [LAB_FLOW_KIND_LOCATION_TIME]              = dispatch_location,
        [LAB_FLOW_KIND_LOCATION_CONTROL]           = dispatch_location,
        [LAB_FLOW_KIND_BINDING_SYNC]               = dispatch_binding,
        [LAB_FLOW_KIND_BINDING_VISIBLE]            = dispatch_binding,
        [LAB_FLOW_KIND_BINDING_COMMIT]             = dispatch_binding,
        [LAB_FLOW_KIND_BINDING_TOGGLE]             = dispatch_binding,
        [LAB_FLOW_KIND_BINDING_REFRESH]            = dispatch_binding,
        [LAB_FLOW_KIND_BINDING_QUEUE]              = dispatch_binding,
        [LAB_FLOW_KIND_BINDING_GOAL]               = dispatch_binding,
        [LAB_FLOW_KIND_BINDING_ROW]                = dispatch_binding,
        [LAB_FLOW_KIND_BINDING_PREPARE_INPUT]      = dispatch_binding,
        [LAB_FLOW_KIND_BINDING_ACTION]             = dispatch_binding,
        [LAB_FLOW_KIND_BINDING_DEFERRED]           = dispatch_binding,
        [LAB_FLOW_KIND_BINDING_BEGIN_COMMIT]       = dispatch_binding,
        [LAB_FLOW_KIND_BINDING_EDITOR_PROJECT]     = dispatch_binding,
        [LAB_FLOW_KIND_BINDING_EDITOR_APPLY]       = dispatch_binding,
        [LAB_FLOW_KIND_BINDING_EDITOR_REFRESH]     = dispatch_binding,
        [LAB_FLOW_KIND_BINDING_KIND_REQUEST]       = dispatch_binding,
        [LAB_FLOW_KIND_RESULT_TRANSFER]            = dispatch_result,
        [LAB_FLOW_KIND_RESULT_PRETTY_MATRIX]       = dispatch_result,
        [LAB_FLOW_KIND_RESULT_EXPAND_DIGITS]       = dispatch_result,
        [LAB_FLOW_KIND_RESULT_COPY_INPUT]          = dispatch_result,
        [LAB_FLOW_KIND_RESULT_COPY_CARD]           = dispatch_result,
        [LAB_FLOW_KIND_RESULT_RENDER_LATEX]        = dispatch_result,
        [LAB_FLOW_KIND_RESULT_INITIALISE]          = dispatch_result,
        [LAB_FLOW_KIND_BOOTSTRAP]                  = dispatch_bootstrap,
        [LAB_FLOW_KIND_TRANSPORT_POST]             = dispatch_transport,
        [LAB_FLOW_KIND_TRANSPORT_RUN]              = dispatch_transport,
        [LAB_FLOW_KIND_TRANSPORT_UI_RUN]           = dispatch_transport,
        [LAB_FLOW_KIND_STATE_FORMS_CLEAR]          = dispatch_forms,
        [LAB_FLOW_KIND_STATE_FORMS_REFRESH]        = dispatch_forms,
        [LAB_FLOW_KIND_STATE_FORMS_PARSE_BOUNDS]   = dispatch_forms,
        [LAB_FLOW_KIND_STATE_FORMS_RESTORE_BOUNDS] = dispatch_forms,
    };
    unsigned selector = (unsigned)kind;
    int plan;
    if (selector < sizeof handlers / sizeof *handlers)
        plan = handlers[selector](frame, view);
    else if (selector < sizeof routes / sizeof *routes && routes[selector])
        plan = routes[selector](kind, frame, view);
    else
        plan = lab_flow_done(0);
    lab_dom_return(plan);
}
