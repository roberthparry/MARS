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
int lab_flow_call(int frame, unsigned next, const char *service, const char *save, int wait, unsigned count, ...)
{
    int plan = lab_dom_object(5), calls = lab_dom_object(4), call = lab_dom_object(5), args = lab_dom_object(4);
    va_list values;
    va_start(values, count);
    for (unsigned i = 0; i < count; ++i)
        lab_dom_push(args, va_arg(values, int));
    va_end(values);
    lab_dom_set(frame, "stage", lab_dom_numeric(next));
    lab_dom_set(call, "service", lab_dom_string(service));
    lab_dom_set(call, "args", args);
    lab_dom_push(calls, call);
    lab_dom_set(plan, "calls", calls);
    lab_dom_set(plan, "wait", lab_dom_scalar(1, wait));
    if (save)
        lab_dom_set(plan, "save", lab_dom_string(save));
    return plan;
}

/* Existing planners keep their metadata; only their ordered effects are executed. */
int lab_flow_effects(int frame, unsigned next, int plan)
{
    return lab_flow_call(frame, next, "effects", 0, lab_dom_truth(lab_dom_get(plan, "wait")), 1, plan);
}

/* Absence and an explicit false result remain distinct to callers. */
int lab_flow_done(int value)
{
    int plan = lab_dom_object(5);
    lab_dom_set(plan, "done", lab_dom_scalar(1, 1));
    if (value)
        lab_dom_set(plan, "value", value);
    return plan;
}

/* Request operations and mode ownership are policy, rather than browser branches. */
void lab_flow_begin(unsigned kind, int frame)
{
    static const char *const operations[] = {"evaluate", "goal", "weather", "solverRender"};
    static const char *const modes[] = {"expression", "equation", "diffequation", "matrix",
                                        "integrator", "datetime", "almanac"};
    if (kind > 5) {
        lab_dom_return(0);
        return;
    }
    int plan = lab_dom_object(5);
    if (kind < 4) {
        double selected = kind == 0 ? lab_dom_number(lab_dom_get(frame, "mode")) : kind == 1 ? 0 : kind == 2 ? 5 : 2;
        if (!(selected >= 0 && selected <= 6) || selected != (unsigned)selected) {
            lab_dom_return(0);
            return;
        }
        unsigned mode = (unsigned)selected;
        int options = lab_dom_object(5);
        lab_dom_set(options, "parent", lab_dom_get(frame, "parent"));
        lab_dom_set(plan, "operation", lab_dom_string(operations[kind]));
        lab_dom_set(plan, "mode", lab_dom_string(modes[mode]));
        lab_dom_set(plan, "options", options);
        lab_dom_set(plan, "ui", lab_dom_scalar(1, kind != 3));
    }
    lab_dom_return(plan);
}

/* A fixed dispatch table keeps independent workflows in their owning C modules. */
void lab_flow_step(unsigned kind, int frame, int view)
{
    static int (*const handlers[])(int, int) = {lab_flow_evaluation, lab_flow_goal,    lab_flow_weather,
                                                lab_flow_solver,     lab_flow_startup, lab_flow_install};
    double stage = lab_dom_number(lab_dom_get(frame, "stage"));
    double mode = lab_dom_number(lab_dom_get(frame, "mode"));
    if (!(stage >= 0 && stage <= 65535) || stage != (unsigned)stage ||
        ((kind == 0 || kind == 5) && (!(mode >= 0 && mode <= 6) || mode != (unsigned)mode))) {
        lab_dom_return(lab_flow_done(0));
        return;
    }
    int plan;
    if (kind < sizeof handlers / sizeof *handlers)
        plan = handlers[kind](frame, view);
    else if (kind < 16)
        plan = lab_flow_request(kind - 6, frame, view);
    else if (kind < 26)
        plan = lab_flow_state(kind - 16, frame, view);
    else if (kind < 40)
        plan = lab_flow_location(kind - 26, frame, view);
    else if (kind < 56)
        plan = lab_flow_binding(kind - 40, frame, view);
    else if (kind < 64)
        plan = lab_flow_result(kind - 56, frame, view);
    else if (kind == 64)
        plan = lab_bootstrap_step(frame, view);
    else if (kind < 70)
        plan = lab_flow_transport(kind - 65, frame, view);
    else if (kind >= 72 && kind < 76)
        plan = lab_flow_state_forms(kind - 72, frame, view);
    else
        plan = lab_flow_done(0);
    lab_dom_return(plan);
}
