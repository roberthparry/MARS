/**
 * @file lab_binding_projection.c
 * @brief Synchronous binding editor decisions and ordered browser update plans.
 *
 * Keeps source clearing, mode selection, deferred metadata preparation and refresh
 * policy in C without making the public browser editor functions asynchronous.
 * Browser callbacks retain actual workspace and DOM resources. Native mathematical
 * strings are copied as opaque values and no handle survives plan publication.
 */
#include <stdarg.h>

#include "lab_mode.h"
#include "lab_dom.h"
#include "lab_binding_projection.h"

static browser_jsref lab_binding_projection_plan(void)
{
    int plan = lab_dom_object(LAB_VALUE_OBJECT);
    lab_dom_set(plan, "calls", lab_dom_object(LAB_VALUE_ARRAY));
    return plan;
}

static void lab_binding_projection_emit(browser_jsref plan, const char *service, unsigned count, ...)
{
    int call = lab_dom_object(LAB_VALUE_OBJECT), args = lab_dom_object(LAB_VALUE_ARRAY);
    va_list values;
    va_start(values, count);
    for (unsigned i = 0; i < count; ++i)
        lab_dom_push(args, va_arg(values, browser_jsref));
    va_end(values);
    lab_dom_set(call, "service", lab_dom_string(service));
    lab_dom_set(call, "args", args);
    lab_dom_push(lab_dom_get(plan, "calls"), call);
}

/* Source clearing is synchronous and ordered; goal-only clearing does not touch binding inputs. */
void lab_binding_projection_clear(bool goal_only)
{
    int plan = lab_binding_projection_plan(), empty = lab_dom_string("");
    if (!goal_only) {
        lab_binding_projection_emit(plan, "bindingSource", 2, lab_dom_string("fullText"), empty);
        lab_binding_projection_emit(plan, "bindingSource", 2, lab_dom_string("displayText"), empty);
        lab_binding_projection_emit(plan, "bindingSource", 2, lab_dom_string("lastInput"), empty);
        lab_binding_projection_emit(plan, "bindingResetCache", 0);
        lab_binding_projection_emit(plan, "bindingClearFlags", 0);
        lab_binding_projection_emit(plan, "bindingClearGoal", 0);
    } else {
        lab_binding_projection_emit(plan, "bindingSource", 2, lab_dom_string("goalSource"), empty);
        lab_binding_projection_emit(plan, "bindingSource", 2, lab_dom_string("goalTarget"), empty);
    }
    lab_dom_return(plan);
}

/* Wrapped native metadata selects the full editor even outside the expression-like modes. */
void lab_binding_projection_update(lab_mode_t mode, browser_jsref updated, browser_jsref metadata)
{
    int plan = lab_binding_projection_plan();
    if ((unsigned)mode <= LAB_MODE_DIFFERENTIAL_EQUATION || lab_dom_truth(lab_dom_get(metadata, "wrapped"))) {
        lab_binding_projection_emit(plan, "bindingSetEditor", 1, updated);
    } else {
        int text = lab_dom_clean(lab_dom_truth(updated) ? lab_dom_text(updated) : lab_dom_string(""), LAB_DOM_CLEAN_TRIM);
        lab_binding_projection_emit(plan, "bindingSetText", 1, text);
        lab_binding_projection_emit(plan, "bindingClearSource", 0);
        lab_binding_projection_emit(plan, "bindingClearValues", 0);
    }
    lab_dom_return(plan);
}

/* The asynchronous metadata lookup starts before the immediate editor projection, as before. */
void lab_binding_projection_prepare(browser_jsref context, browser_jsref metadata)
{
    int plan = lab_binding_projection_plan();
    if (!lab_dom_truth(metadata))
        lab_binding_projection_emit(plan, "bindingDeferEditor", 1, context);
    lab_dom_return(plan);
}

/* Browser callbacks apply source ownership and controls before scheduling any new refresh. */
void lab_binding_projection_finish(browser_jsref selected)
{
    int plan = lab_binding_projection_plan(), bindings = lab_dom_get(selected, "bindings");
    lab_binding_projection_emit(plan, "bindingSource", 2, lab_dom_string("fullText"),
                                lab_dom_get(selected, "fullText"));
    lab_binding_projection_emit(plan, "bindingSource", 2, lab_dom_string("displayText"),
                                lab_dom_get(selected, "displayText"));
    lab_binding_projection_emit(plan, "bindingResize", 0);
    lab_binding_projection_emit(plan, "bindingRenderValues", 1, bindings);
    lab_binding_projection_emit(plan, "bindingSetVariables", 1, bindings);
    lab_binding_projection_emit(plan, "bindingRenderDerivatives", 0);
    if (lab_dom_truth(lab_dom_get(selected, "refresh")))
        lab_binding_projection_emit(plan, "bindingScheduleRefresh", 0);
    lab_dom_return(plan);
}

/* Admission runs between scoped calls; rejected requests never mark or schedule a refresh. */
void lab_binding_projection_refresh(lab_binding_refresh_phase_t phase, browser_jsref context)
{
    int plan = lab_binding_projection_plan();
    if (phase == LAB_BINDING_REFRESH_BEGIN) {
        int body = lab_dom_clean(lab_dom_get(context, "editedBody"), LAB_DOM_CLEAN_TRIM),
            options = lab_dom_object(LAB_VALUE_OBJECT);
        lab_dom_set(context, "editedBody", body);
        lab_dom_set(options, "input", lab_dom_scalar(LAB_VALUE_BOOLEAN, lab_dom_truth(body)));
        lab_binding_projection_emit(plan, "bindingCancelRefresh", 0);
        lab_binding_projection_emit(plan, "bindingBeginRefresh", 4, context, lab_dom_string("bindings"),
                                    lab_dom_string("expression"), options);
    } else if (phase == LAB_BINDING_REFRESH_SCHEDULE && lab_dom_truth(lab_dom_get(context, "request"))) {
        lab_binding_projection_emit(plan, "bindingRefreshPending", 0);
        lab_binding_projection_emit(plan, "bindingRefreshHistory", 0);
        lab_binding_projection_emit(plan, "bindingRefreshTimer", 4, lab_dom_get(context, "editedBody"),
                                    lab_dom_get(context, "sourceExpression"), lab_dom_get(context, "request"),
                                    lab_dom_numeric(300));
    }
    lab_dom_return(plan);
}
