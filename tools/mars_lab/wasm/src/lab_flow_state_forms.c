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

static unsigned lab_flow_state_forms_stage(int frame)
{
    return (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
}

static int lab_flow_state_forms_next(int frame, unsigned next, const char *service)
{
    return lab_flow_call(frame, next, service, 0, 0, 0);
}

static int lab_flow_state_forms_null(void)
{
    int plan = lab_flow_done(0);
    lab_dom_set(plan, "value", 0);
    return plan;
}

static int lab_flow_state_forms_guard(int frame, unsigned next)
{
    return lab_flow_call(frame, next, "stateGuard", "current", 0, 1, lab_dom_get(frame, "isCurrent"));
}

static int lab_flow_state_forms_revision(int frame)
{
    unsigned revision = lab_rows_revision_next();
    lab_dom_set(frame, "generation", lab_dom_numeric(revision));
    return revision != 0;
}

static int lab_flow_state_forms_exhausted(int frame)
{
    return lab_flow_call(frame, 100, "fail", 0, 0, 1,
                         lab_dom_string("Integrator form revision limit reached; reload the Lab"));
}

static int lab_flow_state_forms_accept(int frame)
{
    return lab_rows_revision_accept(
        (unsigned)lab_dom_number(lab_dom_get(frame, "generation")),
        lab_dom_equal(lab_dom_get(frame, "expression"), lab_dom_get(frame, "currentExpression")));
}

static int lab_flow_state_forms_payload(int frame, int rows, int expression, int fallback)
{
    int payload = lab_dom_object(5);
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

static int lab_flow_state_clear(int frame)
{
    switch (lab_flow_state_forms_stage(frame)) {
        case 0:
            return lab_flow_state_forms_next(frame, 1, "stateClearPrelude");
        case 1:
            return lab_flow_call(frame, 2, "stateClearOwned", 0, 1, 0);
        case 2:
            return lab_flow_done(0);
        case 3:
            return lab_flow_call(frame, 4, "stateCurrentHistory", "history", 0, 0);
        case 4:
            if (lab_dom_truth(lab_dom_get(lab_dom_get(frame, "history"), "text")))
                return lab_flow_call(frame, 5, "pushHistory", 0, 0, 1, lab_dom_get(frame, "history"));
            /* Fall through for an empty worksheet. */
        case 5:
            return lab_flow_state_forms_next(frame, 6, "stateClearForward");
        case 6:
            return lab_flow_call(frame, 7, "stateClearProjection", "reset", 0, 0);
        case 7: {
            unsigned reset = (unsigned)lab_dom_number(lab_dom_get(frame, "reset"));
            if (reset == 1)
                return lab_flow_state_forms_next(frame, 8, "stateBlankBounds");
            if (reset == 2)
                return lab_flow_state_forms_next(frame, 8, "stateResetCalendar");
            /* Fall through after the mode-specific reset, if any. */
        }
        case 8:
            return lab_flow_call(frame, 9, "stateCaptureRequest", "captured", 1, 1, lab_dom_get(frame, "request"));
        case 9:
            if (!lab_dom_truth(lab_dom_get(frame, "captured")))
                return lab_flow_done(0);
            return lab_flow_state_forms_next(frame, 10, "stateClearSource");
        case 10:
            return lab_flow_state_forms_next(frame, 11, "stateHideTarget");
        case 11:
            return lab_flow_state_forms_next(frame, 12, "stateClearResult");
        case 12:
            return lab_flow_state_forms_next(frame, 13, "stateSaveResult");
        case 13:
            return lab_flow_state_forms_next(frame, 14, "commitModeState");
        case 14:
            return lab_flow_state_forms_next(frame, 15, "stateHistoryButtons");
        case 15:
            return lab_flow_call(frame, 16, "stateStatus", 0, 0, 1, lab_dom_string("Ready"));
        case 16:
            return lab_flow_state_forms_next(frame, 17, "stateFocusEditor");
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_state_forms_refresh(int frame)
{
    switch (lab_flow_state_forms_stage(frame)) {
        case 0:
            if (!lab_flow_state_forms_revision(frame))
                return lab_flow_state_forms_exhausted(frame);
            return lab_flow_call(frame, 1, "stateFormsRequest", "result", 1, 1,
                                 lab_flow_state_forms_payload(frame, 1, 1, 0));
        case 1:
            return lab_flow_call(frame, 2, "stateFormsExpression", "currentExpression", 0, 0);
        case 2:
            if (!lab_flow_state_forms_accept(frame))
                return lab_flow_state_forms_null();
            return lab_flow_call(frame, 3, "stateFormsMetadata", 0, 0, 2, lab_dom_get(frame, "expression"),
                                 lab_dom_get(frame, "result"));
        case 3:
            return lab_flow_call(frame, 4, "stateFormsRows", "completion", 0, 1, lab_dom_get(frame, "result"));
        case 4: {
            int completion = lab_dom_get(frame, "completion");
            lab_dom_set(completion, "done", lab_dom_scalar(1, 1));
            return completion;
        }
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_state_forms_parse(int frame)
{
    switch (lab_flow_state_forms_stage(frame)) {
        case 0:
            return lab_flow_call(frame, 1, "stateFormsRequest", "result", 1, 1,
                                 lab_flow_state_forms_payload(frame, 0, 0, lab_dom_string("")));
        case 1:
            return lab_flow_call(frame, 2, "stateFormsRows", "completion", 0, 1, lab_dom_get(frame, "result"));
        case 2: {
            int completion = lab_dom_get(frame, "completion");
            lab_dom_set(completion, "done", lab_dom_scalar(1, 1));
            return completion;
        }
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_state_forms_restore(int frame)
{
    switch (lab_flow_state_forms_stage(frame)) {
        case 0:
            return lab_flow_state_forms_guard(frame, 1);
        case 1:
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_done(lab_dom_scalar(1, 0));
            if (!lab_flow_state_forms_revision(frame))
                return lab_flow_state_forms_exhausted(frame);
            return lab_flow_call(frame, 2, "stateFormsExpression", "expression", 0, 0);
        case 2:
            return lab_flow_call(frame, 3, "stateFormsRequest", "result", 1, 1,
                                 lab_flow_state_forms_payload(frame, 0, 1, lab_dom_get(frame, "defaultBounds")));
        case 3:
            return lab_flow_state_forms_guard(frame, 4);
        case 4:
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_done(lab_dom_scalar(1, 0));
            return lab_flow_call(frame, 5, "stateFormsExpression", "currentExpression", 0, 0);
        case 5:
            if (!lab_flow_state_forms_accept(frame))
                return lab_flow_done(lab_dom_scalar(1, 0));
            return lab_flow_call(frame, 6, "stateFormsMetadata", 0, 0, 2, lab_dom_get(frame, "expression"),
                                 lab_dom_get(frame, "result"));
        case 6:
            return lab_flow_call(frame, 7, "stateFormsRows", "rows", 0, 1, lab_dom_get(frame, "result"));
        case 7:
            return lab_flow_call(frame, 8, "stateFormsRender", 0, 0, 1, lab_dom_get(frame, "rows"));
        case 8:
            return lab_flow_done(lab_dom_scalar(1, 1));
        default:
            return lab_flow_done(0);
    }
}

/* Four distinct workflows share revision policy without retaining browser handles. */
int lab_flow_state_forms(unsigned kind, int frame, int view)
{
    (void)view;
    static int (*const handlers[])(int) = {lab_flow_state_clear, lab_flow_state_forms_refresh,
                                           lab_flow_state_forms_parse, lab_flow_state_forms_restore};
    return kind < sizeof handlers / sizeof *handlers ? handlers[kind](frame) : lab_flow_done(0);
}
