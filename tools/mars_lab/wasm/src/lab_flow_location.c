/**
 * @file lab_flow_location.c
 * @brief Calendar location request sequencing, ownership and deferred projection.
 *
 * Moves application decisions out of the browser adapters while preserving town
 * generation snapshots, author edits, request cancellation and finally cleanup.
 * Fixed continuation switches describe suspension points, not growing dispatch
 * catalogues. Existing native location APIs own actual field and town policy.
 * Browser services retain Intl/Date conversion, request resources and promises.
 */
#include "lab_dom.h"
#include "lab_flow.h"
#include "lab_flow_location.h"

static unsigned lab_flow_location_stage(int frame)
{
    return (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
}

static int lab_flow_location_null(void)
{
    int plan = lab_flow_done(0);
    lab_dom_set(plan, "value", 0);
    return plan;
}

static int lab_flow_location_flag(int value)
{
    return lab_dom_numeric(lab_dom_truth(value));
}

static int lab_flow_location_jump(int frame, unsigned next)
{
    return lab_flow_effects(frame, next, lab_dom_object(5));
}

static int lab_flow_location_forms(int frame)
{
    unsigned action = (unsigned)lab_dom_number(lab_dom_get(frame, "action"));
    if (!lab_flow_location_stage(frame)) {
        int option = lab_dom_get(frame, "option"), value = lab_dom_get(frame, "value");
        if (action == 2 && (!lab_dom_truth(option) || !lab_dom_truth(value)))
            return lab_flow_done(lab_dom_scalar(1, 0));
        int payload = lab_dom_object(5);
        lab_dom_set(payload, "action", lab_dom_string(action ? "town" : "time"));
        lab_dom_set(payload, action ? "value" : "text",
                    lab_dom_text(lab_dom_truth(value) ? value : lab_dom_string("")));
        if (action == 2) {
            int candidates = lab_dom_object(4);
            lab_dom_push(candidates, lab_dom_get(option, "value"));
            lab_dom_set(payload, "candidates", candidates);
        }
        return lab_flow_call(frame, 1, "locForms", "response", 1, 1, payload);
    }
    int response = lab_dom_get(frame, "response");
    if (action == 2) {
        int index = lab_dom_get(response, "match_index");
        return lab_flow_done(lab_dom_scalar(1, lab_dom_type(index) == 2 && lab_dom_number(index) == 0));
    }
    return lab_flow_done(lab_dom_get(response, action ? "town" : "text"));
}

static int lab_flow_location_restore(int frame)
{
    int select = lab_dom_get(frame, "select"), snapshot = lab_dom_get(frame, "snapshot");
    int plan = lab_dom_get(frame, "plan");
    switch (lab_flow_location_stage(frame)) {
        case 0:
            return lab_flow_call(frame, 1, "locCurrent", "current", 0, 1, lab_dom_get(frame, "isCurrent"));
        case 1: {
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_location_null();
            if (!lab_dom_truth(select))
                return lab_flow_done(lab_dom_scalar(1, 0));
            int options = lab_dom_object(5);
            lab_dom_set(options, "selectDefault", lab_dom_scalar(1, 0));
            lab_dom_set(options, "isCurrent", lab_dom_get(frame, "isCurrent"));
            return lab_flow_call(frame, 2, "locPopulate", "preparation", 0, 3, select,
                                 lab_dom_get(frame, "jurisdiction"), options);
        }
        case 2:
            return lab_flow_call(frame, 3, "native", "snapshot", 0, 2,
                                 lab_dom_string("lab_location_snapshot"), select);
        case 3:
            return lab_flow_call(frame, 4, "locAwait", 0, 1, 1, lab_dom_get(frame, "preparation"));
        case 4:
            return lab_flow_call(frame, 5, "locCurrent", "current", 0, 1, lab_dom_get(frame, "isCurrent"));
        case 5:
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_location_null();
            return lab_flow_call(frame, 6, "native", "unchanged", 0, 3,
                                 lab_dom_string("lab_location_unchanged"), select, snapshot);
        case 6:
            if (!lab_dom_truth(lab_dom_get(frame, "unchanged")))
                return lab_flow_location_null();
            return lab_flow_call(frame, 7, "native", "plan", 0, 6, lab_dom_string("lab_location_restore"),
                                 lab_dom_numeric(lab_dom_truth(lab_dom_get(frame, "responded"))), select, snapshot,
                                 lab_dom_get(frame, "selection"), lab_dom_get(frame, "response"));
        case 7:
            if (lab_dom_truth(lab_dom_get(plan, "request"))) {
                lab_dom_set(frame, "responded", lab_dom_scalar(1, 1));
                return lab_flow_call(frame, 4, "locForms", "response", 1, 1, lab_dom_get(plan, "request"));
            }
            if (lab_dom_truth(lab_dom_get(plan, "controls")))
                return lab_flow_call(frame, 8, "locApplyTown", 0, 0, 1, lab_dom_get(plan, "controls"));
            /* Fall through: coordinate fallback does not apply named-town controls. */
        default:
            return lab_flow_done(lab_dom_get(plan, "restored"));
    }
}

static int lab_flow_location_populate(int frame)
{
    int select = lab_dom_get(frame, "select");
    switch (lab_flow_location_stage(frame)) {
        case 0:
            if (!lab_dom_truth(select))
                return lab_flow_done(lab_dom_object(4));
            return lab_flow_call(frame, 1, "locCurrent", "current", 0, 1, lab_dom_get(frame, "isCurrent"));
        case 1:
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_done(lab_dom_object(4));
            return lab_flow_call(frame, 2, "locTowns", "towns", 0, 1, lab_dom_get(frame, "jurisdiction"));
        case 2:
            return lab_flow_call(frame, 3, "native", 0, 0, 4, lab_dom_string("lab_location_populate"), select,
                                 lab_dom_get(frame, "towns"), lab_flow_location_flag(lab_dom_get(frame, "selectDefault")));
        default:
            return lab_flow_done(lab_dom_get(frame, "towns"));
    }
}

static int lab_flow_location_sync(int frame)
{
    unsigned stage = lab_flow_location_stage(frame);
    if (stage == 0 || stage == 2)
        return lab_flow_call(frame, stage + 1, "locElements", "elements", 0, 1,
                             lab_dom_string(stage ? "almanac" : "datetime"));
    if (stage == 1 || stage == 3) {
        int elements = lab_dom_get(frame, "elements"), options = lab_dom_object(5);
        lab_dom_set(options, "selectDefault", lab_dom_get(frame, "selectDefault"));
        return lab_flow_call(frame, stage + 1, "locPopulate", 0, 1, 3, lab_dom_get(elements, "town"),
                             lab_dom_get(lab_dom_get(elements, "jurisdiction"), "value"), options);
    }
    return lab_flow_done(0);
}

static int lab_flow_location_history(int frame)
{
    switch (lab_flow_location_stage(frame)) {
        case 0:
            return lab_flow_call(frame, 1, "locApplyState", "state", 0, 2, lab_dom_get(frame, "mode"),
                                 lab_dom_get(frame, "source"));
        case 1:
            return lab_flow_call(frame, 2, "locElements", "elements", 0, 1, lab_dom_get(frame, "mode"));
        case 2: {
            int elements = lab_dom_get(frame, "elements");
            return lab_flow_call(frame, 3, "locRestore", 0, 1, 6, lab_dom_get(elements, "town"),
                                 lab_dom_get(lab_dom_get(elements, "jurisdiction"), "value"),
                                 lab_dom_get(lab_dom_get(frame, "state"), "town"),
                                 lab_dom_get(lab_dom_get(elements, "latitude"), "value"),
                                 lab_dom_get(lab_dom_get(elements, "longitude"), "value"),
                                 lab_dom_get(frame, "isCurrent"));
        }
        default:
            return lab_flow_done(0);
    }
}

enum {
    LOC_TOTAL_START, LOC_TOTAL_CONTEXT, LOC_TOTAL_PREPARE, LOC_TOTAL_RESTORE, LOC_TOTAL_CURRENT,
    LOC_TOTAL_APPLY, LOC_TOTAL_SAVE, LOC_TOTAL_EVALUATE, LOC_TOTAL_FINISH, LOC_TOTAL_DONE,
    LOC_TOTAL_ERROR_CURRENT, LOC_TOTAL_ERROR_STATUS, LOC_TOTAL_ERROR_RENDER,
    LOC_TOTAL_RETHROW_FINISH, LOC_TOTAL_RETHROW
};

static int lab_flow_location_totality(int frame)
{
    int request = lab_dom_get(frame, "request"), state = lab_dom_get(frame, "state");
    switch (lab_flow_location_stage(frame)) {
        case LOC_TOTAL_START:
            if (!lab_dom_truth(lab_dom_get(frame, "button")))
                return lab_flow_done(0);
            return lab_flow_call(frame, LOC_TOTAL_CONTEXT, "locBegin", "request", 0, 2,
                                 lab_dom_string("almanacLocation"), lab_dom_string("almanac"));
        case LOC_TOTAL_CONTEXT:
            if (!lab_dom_truth(request))
                return lab_flow_done(0);
            lab_dom_set(frame, "failed", lab_dom_numeric(LOC_TOTAL_ERROR_CURRENT));
            return lab_flow_call(frame, LOC_TOTAL_PREPARE, "locContext", "context", 0, 0);
        case LOC_TOTAL_PREPARE:
            return lab_flow_call(frame, LOC_TOTAL_RESTORE, "native", "state", 0, 3,
                                 lab_dom_string("lab_location_totality_prepare"), lab_dom_get(frame, "button"),
                                 lab_dom_get(frame, "context"));
        case LOC_TOTAL_RESTORE:
            return lab_flow_call(frame, LOC_TOTAL_CURRENT, "locRestoreTotality", "restored", 1, 2, state, request);
        case LOC_TOTAL_CURRENT:
            if (!lab_dom_type(lab_dom_get(frame, "restored")))
                return lab_flow_location_jump(frame, LOC_TOTAL_FINISH);
            return lab_flow_call(frame, LOC_TOTAL_APPLY, "requestCurrent", "current", 0, 1, request);
        case LOC_TOTAL_APPLY:
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_location_jump(frame, LOC_TOTAL_FINISH);
            return lab_flow_call(frame, LOC_TOTAL_SAVE, "native", 0, 0, 3,
                                 lab_dom_string("lab_location_totality_finish"), state,
                                 lab_flow_location_flag(lab_dom_get(frame, "restored")));
        case LOC_TOTAL_SAVE:
            return lab_flow_call(frame, LOC_TOTAL_EVALUATE, "locSaveAlmanac", 0, 0, 0);
        case LOC_TOTAL_EVALUATE:
            return lab_flow_call(frame, LOC_TOTAL_FINISH, "locEvaluateCurrent", 0, 1, 0);
        case LOC_TOTAL_ERROR_CURRENT:
            lab_dom_set(frame, "failed", lab_dom_numeric(LOC_TOTAL_RETHROW_FINISH));
            return lab_flow_call(frame, LOC_TOTAL_ERROR_STATUS, "requestCurrent", "current", 0, 1, request);
        case LOC_TOTAL_ERROR_STATUS:
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_location_jump(frame, LOC_TOTAL_FINISH);
            return lab_flow_call(frame, LOC_TOTAL_ERROR_RENDER, "locStatus", 0, 0, 1, lab_dom_string("Error"));
        case LOC_TOTAL_ERROR_RENDER:
            return lab_flow_call(frame, LOC_TOTAL_FINISH, "locError", 0, 0, 1,
                                 lab_dom_get(lab_dom_get(frame, "exception"), "message"));
        case LOC_TOTAL_FINISH:
            lab_dom_delete(frame, "failed");
            return lab_flow_call(frame, LOC_TOTAL_DONE, "locFinish", 0, 0, 1, request);
        case LOC_TOTAL_RETHROW_FINISH:
            return lab_flow_call(frame, LOC_TOTAL_RETHROW, "locFinish", 0, 0, 1, request);
        case LOC_TOTAL_RETHROW:
            return lab_flow_call(frame, LOC_TOTAL_DONE, "rethrow", 0, 0, 1, lab_dom_get(frame, "exception"));
        default:
            return lab_flow_done(0);
    }
}

enum {
    LOC_REFRESH_ELEMENTS, LOC_REFRESH_CONTROLS, LOC_REFRESH_BEGIN, LOC_REFRESH_READ, LOC_REFRESH_STATE,
    LOC_REFRESH_POPULATE, LOC_REFRESH_CURRENT, LOC_REFRESH_COORDINATES, LOC_REFRESH_DEFAULT,
    LOC_REFRESH_SECOND_CURRENT, LOC_REFRESH_APPLY, LOC_REFRESH_STATUS, LOC_REFRESH_POST,
    LOC_REFRESH_RESPONSE_CURRENT, LOC_REFRESH_RESPONSE_CONTEXT, LOC_REFRESH_RESPONSE,
    LOC_REFRESH_ACCEPT, LOC_REFRESH_FINISH, LOC_REFRESH_DONE
};

static int lab_flow_location_refresh(int frame)
{
    int mode = lab_dom_get(frame, "mode"), elements = lab_dom_get(frame, "elements");
    int request = lab_dom_get(frame, "request"), state = lab_dom_get(frame, "state");
    int update = lab_dom_truth(lab_dom_get(frame, "updateCoordinates"));
    switch (lab_flow_location_stage(frame)) {
        case LOC_REFRESH_ELEMENTS:
            return lab_flow_call(frame, LOC_REFRESH_CONTROLS, "locElements", "elements", 0, 1, mode);
        case LOC_REFRESH_CONTROLS:
            return lab_flow_call(frame, LOC_REFRESH_BEGIN, "locControls", "controls", 0, 2, mode,
                                 lab_dom_scalar(1, update));
        case LOC_REFRESH_BEGIN:
            if (!lab_dom_truth(lab_dom_get(elements, "jurisdiction")))
                return lab_flow_done(0);
            return lab_flow_call(frame, LOC_REFRESH_READ, "locBegin", "request", 0, 2,
                                 lab_dom_join(mode, lab_dom_string("Location"), ""), mode);
        case LOC_REFRESH_READ:
            if (!lab_dom_truth(request))
                return lab_flow_done(0);
            return lab_flow_call(frame, LOC_REFRESH_STATE, "locRead", "read", 0, 1, mode);
        case LOC_REFRESH_STATE:
            return lab_flow_call(frame, LOC_REFRESH_POPULATE, "locState", "state", 0, 3, mode,
                                 lab_dom_get(frame, "read"), lab_dom_numeric(1));
        case LOC_REFRESH_POPULATE:
            lab_dom_set(frame, "jurisdiction", lab_dom_get(state, "jurisdiction"));
            lab_dom_set(frame, "date", lab_dom_get(state, "date"));
            lab_dom_set(frame, "failed", lab_dom_numeric(LOC_REFRESH_FINISH));
            if (update) {
                int options = lab_dom_object(5);
                lab_dom_set(options, "selectDefault", lab_dom_scalar(1, 0));
                return lab_flow_call(frame, LOC_REFRESH_CURRENT, "locPopulate", 0, 1, 3,
                                     lab_dom_get(elements, "town"), lab_dom_get(frame, "jurisdiction"), options);
            }
            return lab_flow_call(frame, LOC_REFRESH_STATUS, "locApplyTown", 0, 0, 1,
                                 lab_dom_get(frame, "controls"));
        case LOC_REFRESH_CURRENT:
            return lab_flow_call(frame, LOC_REFRESH_COORDINATES, "requestCurrent", "current", 0, 1, request);
        case LOC_REFRESH_COORDINATES:
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_location_jump(frame, LOC_REFRESH_FINISH);
            return lab_flow_call(frame, LOC_REFRESH_DEFAULT, "locCoordinates", "matched", 0, 3,
                                 lab_dom_get(elements, "town"),
                                 lab_dom_get(lab_dom_get(elements, "latitude"), "value"),
                                 lab_dom_get(lab_dom_get(elements, "longitude"), "value"));
        case LOC_REFRESH_DEFAULT:
            if (!lab_dom_truth(lab_dom_get(frame, "matched"))) {
                int options = lab_dom_object(5);
                lab_dom_set(options, "selectDefault", lab_dom_scalar(1, 1));
                return lab_flow_call(frame, LOC_REFRESH_SECOND_CURRENT, "locPopulate", 0, 1, 3,
                                     lab_dom_get(elements, "town"), lab_dom_get(frame, "jurisdiction"), options);
            }
            /* Fall through: preserve the second freshness check even without default population. */
        case LOC_REFRESH_SECOND_CURRENT:
            return lab_flow_call(frame, LOC_REFRESH_APPLY, "requestCurrent", "current", 0, 1, request);
        case LOC_REFRESH_APPLY:
            if (!lab_dom_truth(lab_dom_get(frame, "current")))
                return lab_flow_location_jump(frame, LOC_REFRESH_FINISH);
            return lab_flow_call(frame, LOC_REFRESH_STATUS, "locApplyTown", "townApplied", 0, 1,
                                 lab_dom_get(frame, "controls"));
        case LOC_REFRESH_STATUS:
            if (lab_dom_equal(mode, lab_dom_string("almanac")))
                return lab_flow_call(frame, LOC_REFRESH_POST, "locStatus", 0, 0, 1,
                                     lab_dom_string("Updating almanac location..."));
            /* Fall through: DateTime refresh does not alter status here. */
        case LOC_REFRESH_POST: {
            int payload = lab_dom_object(5);
            lab_dom_set(payload, "jurisdiction", lab_dom_get(frame, "jurisdiction"));
            lab_dom_set(payload, "date", lab_dom_get(frame, "date"));
            return lab_flow_call(frame, LOC_REFRESH_RESPONSE_CURRENT, "post", "result", 1, 2, request, payload);
        }
        case LOC_REFRESH_RESPONSE_CURRENT:
            return lab_flow_call(frame, LOC_REFRESH_RESPONSE_CONTEXT, "requestCurrent", "current", 0, 1, request);
        case LOC_REFRESH_RESPONSE_CONTEXT: {
            int result = lab_dom_get(frame, "result");
            if (!lab_dom_truth(lab_dom_get(frame, "current")) ||
                !lab_dom_truth(lab_dom_get(lab_dom_get(result, "response"), "ok")) ||
                !lab_dom_truth(lab_dom_get(lab_dom_get(result, "data"), "ok")))
                return lab_flow_location_jump(frame, LOC_REFRESH_FINISH);
            return lab_flow_call(frame, LOC_REFRESH_RESPONSE, "locContext", "context", 0, 0);
        }
        case LOC_REFRESH_RESPONSE:
            return lab_flow_call(frame, LOC_REFRESH_ACCEPT, "locResponse", 0, 0, 5, mode,
                                 lab_dom_get(lab_dom_get(frame, "result"), "data"), lab_dom_get(frame, "context"),
                                 lab_dom_scalar(1, update), lab_dom_get(frame, "townApplied"));
        case LOC_REFRESH_ACCEPT:
            return lab_flow_call(frame, LOC_REFRESH_FINISH, "locAcceptContext", 0, 0, 1,
                                 lab_dom_get(frame, "context"));
        case LOC_REFRESH_FINISH:
            lab_dom_delete(frame, "failed");
            return lab_flow_call(frame, LOC_REFRESH_DONE, "locFinish", 0, 0, 1, request);
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_location_auto(int frame)
{
    int mode = lab_dom_get(frame, "mode"), options = lab_dom_get(frame, "options");
    int almanac = lab_dom_equal(mode, lab_dom_string("almanac"));
    switch (lab_flow_location_stage(frame)) {
        case 0:
            return lab_flow_call(frame, 1, "currentMode", "currentMode", 0, 0);
        case 1:
            if (!lab_dom_equal(mode, lab_dom_get(frame, "currentMode")))
                return lab_flow_done(0);
            if (almanac)
                return lab_flow_call(frame, 2, "locClearAlmanac", 0, 0, 0);
            /* Fall through: only Almanac invalidates its cached worksheet. */
        case 2:
            if (lab_dom_truth(lab_dom_get(options, "refreshJurisdiction"))) {
                int refresh = lab_dom_object(5);
                lab_dom_set(refresh, "updateCoordinates", lab_dom_get(options, "refreshCoordinates"));
                return lab_flow_call(frame, 3, almanac ? "locRefreshAlmanac" : "locRefreshDatetime", 0, 1, 1, refresh);
            }
            /* Fall through: persistence follows the optional location refresh. */
        case 3:
            return lab_flow_call(frame, 4, almanac ? "locSaveAlmanac" : "locSaveDatetime", 0, 0, 0);
        case 4: {
            int evaluate = lab_dom_object(5);
            lab_dom_set(evaluate, "skipHistoryUpdate", lab_dom_scalar(1, 1));
            return lab_flow_call(frame, 5, almanac ? "locEvaluateAlmanac" : "locEvaluateDatetime", 0, 1, 1, evaluate);
        }
        case 5:
            return lab_flow_call(frame, 6, "updateHistoryButtons", 0, 0, 0);
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_location_time(int frame)
{
    int snapshot = lab_dom_get(frame, "snapshot");
    switch (lab_flow_location_stage(frame)) {
        case 0:
            return lab_flow_call(frame, 1, "locTimeSnapshot", "snapshot", 0, 0);
        case 1:
            lab_dom_set(frame, "failed", lab_dom_numeric(7));
            return lab_flow_call(frame, 2, "locFormatTime", "formatted", 1, 1, lab_dom_get(snapshot, "authored"));
        case 7:
            lab_dom_set(frame, "caught", lab_dom_scalar(1, 1));
            /* Fall through: recovery checks the same request and authored-value ownership. */
        case 2:
            return lab_flow_call(frame, 3, "requestContext", "context", 0, 0);
        case 3:
            if (!lab_dom_equal(lab_dom_get(snapshot, "context"), lab_dom_get(frame, "context")))
                return lab_flow_done(0);
            return lab_flow_call(frame, 4, "locLatestMain", "main", 0, 0);
        case 4:
            if (!lab_dom_equal(lab_dom_get(snapshot, "main"), lab_dom_get(frame, "main")) ||
                !lab_dom_equal(lab_dom_get(snapshot, "authored"),
                               lab_dom_get(lab_dom_get(snapshot, "element"), "value")))
                return lab_flow_done(0);
            if (lab_dom_truth(lab_dom_get(frame, "caught")))
                return lab_flow_call(frame, 6, "locStatus", 0, 0, 1, lab_dom_get(frame, "error"));
            if (lab_dom_equal(lab_dom_get(frame, "formatted"), lab_dom_get(snapshot, "authored")))
                return lab_flow_done(0);
            return lab_flow_call(frame, 6, "locTimeApply", 0, 0, 2, lab_dom_get(snapshot, "element"),
                                 lab_dom_get(frame, "formatted"));
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_location_control(int frame)
{
    int mode = lab_dom_get(frame, "mode");
    unsigned actions = (unsigned)lab_dom_number(lab_dom_get(frame, "actions"));
    switch (lab_flow_location_stage(frame)) {
        case 0:
            return lab_flow_call(frame, 1, "locElements", "elements", 0, 1, mode);
        case 1:
            if (actions & 4)
                return lab_flow_call(frame, 2, "locControls", "controls", 0, 1, mode);
            return lab_flow_location_jump(frame, 3);
        case 2:
            return lab_flow_call(frame, 3, "locApplyTown", 0, 0, 1, lab_dom_get(frame, "controls"));
        case 3:
            if (actions & 8) {
                int elements = lab_dom_get(frame, "elements");
                return lab_flow_call(frame, 4, "locClearCustom", 0, 0, 4, lab_dom_get(elements, "town"),
                                     lab_dom_get(elements, "latitude"), lab_dom_get(elements, "longitude"),
                                     lab_dom_get(elements, "elevation"));
            }
            /* Fall through: inactive-mode events may project controls but never auto-evaluate. */
        case 4:
            return lab_flow_call(frame, 5, "currentMode", "currentMode", 0, 0);
        case 5: {
            if (!lab_dom_equal(mode, lab_dom_get(frame, "currentMode")))
                return lab_flow_done(0);
            int options = lab_dom_object(5);
            lab_dom_set(options, "refreshJurisdiction", lab_dom_scalar(1, (actions & 16) != 0));
            lab_dom_set(options, "refreshCoordinates", lab_dom_scalar(1, (actions & 32) != 0));
            return lab_flow_call(frame, 6,
                                 lab_dom_equal(mode, lab_dom_string("datetime")) ? "locTriggerDatetime" : "locTriggerAlmanac",
                                 0, 0, 1, options);
        }
        default:
            return lab_flow_done(0);
    }
}

/* Keep these synchronous effects separate from asynchronous request continuations. */
void lab_flow_location_local(int text, int sections, int body, int card)
{
    int frame = lab_dom_object(5), plan = lab_dom_object(5), calls = lab_dom_object(4);
    text = lab_dom_get(text, "value");
    text = lab_dom_clean(lab_dom_text(lab_dom_truth(text) ? text : lab_dom_string("")), 0);
    if (lab_dom_truth(body)) {
        int render = lab_flow_call(frame, 1, "locRenderLocal", 0, 0, 3, body, sections, text);
        lab_dom_push(calls, lab_dom_item(lab_dom_get(render, "calls"), 0));
    }
    if (lab_dom_truth(card)) {
        int visibility = lab_flow_call(frame, 1, lab_dom_truth(text) ? "locLocalVisible" : "locLocalHidden",
                                       0, 0, 1, card);
        lab_dom_push(calls, lab_dom_item(lab_dom_get(visibility, "calls"), 0));
    }
    lab_dom_set(plan, "calls", calls);
    lab_dom_return(plan);
}

/* Read current mode only after rendering, leaving empty-card hiding independent of mode. */
void lab_flow_location_visibility(int card, int mode)
{
    lab_dom_class(card, "hidden", !lab_dom_equal(mode, lab_dom_string("datetime")));
}

/* Callable-method detection belongs to the browser; preference and fallbacks are native policy. */
void lab_flow_location_select(int select, int capabilities)
{
    int frame = lab_dom_object(5), plan = lab_dom_object(5);
    if (lab_dom_truth(select)) {
        if (lab_dom_truth(lab_dom_get(capabilities, "rebuild")))
            plan = lab_flow_call(frame, 1, "locSelectRebuild", 0, 0, 1, select);
        else if (lab_dom_truth(lab_dom_get(capabilities, "sync")))
            plan = lab_flow_call(frame, 1, "locSelectSync", 0, 0, 1, select);
    }
    lab_dom_return(plan);
}

/* Keep unchanged moves inert without making the host interpret movement policy. */
void lab_flow_location_picker(unsigned changed)
{
    int frame = lab_dom_object(5), plan = lab_dom_object(5);
    if (changed)
        plan = lab_flow_call(frame, 1, "locRenderPicker", 0, 0, 0);
    lab_dom_return(plan);
}

/* Dispatch a fixed group of independent workflows, with all persistent values in the browser frame. */
int lab_flow_location(unsigned kind, int frame, int view)
{
    (void)view;
    static int (*const handlers[])(int) = {
        [0] = lab_flow_location_forms,
        [1] = lab_flow_location_restore,
        [2] = lab_flow_location_populate,
        [3] = lab_flow_location_sync,
        [4] = lab_flow_location_history,
        [5] = lab_flow_location_totality,
        [6] = lab_flow_location_refresh,
        [7] = lab_flow_location_auto,
        [8] = lab_flow_location_time,
        [9] = lab_flow_location_control
    };
    return kind < sizeof handlers / sizeof *handlers ? handlers[kind](frame) : lab_flow_done(0);
}
