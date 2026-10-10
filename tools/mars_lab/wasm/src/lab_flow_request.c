/**
 * @file lab_flow_request.c
 * @brief Native sequencing of Lab requests, guards and browser publication.
 *
 * The browser retains Promises, abort controllers and response objects. This module
 * decides when to request, validate, publish or discard their results, using
 * request-local continuation frames. Text remains opaque: mathematical preparation
 * is performed by the native server, never by this client module.
 */
#include "lab_dom.h"
#include "lab_flow.h"
#include "lab_flow_request.h"

static unsigned lab_flow_request_stage(int frame)
{
    return (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
}

static int lab_flow_request_truth(int frame, const char *key)
{
    return lab_dom_truth(lab_dom_get(frame, key));
}

static int lab_flow_request_fallback(int value, const char *fallback)
{
    return lab_dom_truth(value) ? value : lab_dom_string(fallback);
}

static int lab_flow_request_current(int frame, unsigned next)
{
    return lab_flow_call(frame, next, "requestCurrent", "current", 0, 1, lab_dom_get(frame, "request"));
}

static int lab_flow_request_finish(int frame, unsigned next)
{
    lab_dom_delete(frame, "failed");
    return lab_flow_call(frame, next, "requestFinish", 0, 0, 1, lab_dom_get(frame, "request"));
}

static int lab_flow_request_response(int frame)
{
    return lab_dom_get(lab_dom_get(frame, "result"), "response");
}

static int lab_flow_request_data(int frame)
{
    return lab_dom_get(lab_dom_get(frame, "result"), "data");
}

static int lab_flow_request_ok(int frame)
{
    return lab_dom_truth(lab_dom_get(lab_flow_request_response(frame), "ok")) &&
           lab_dom_truth(lab_dom_get(lab_flow_request_data(frame), "ok"));
}

/* Distinct continuation cases describe an ordered protocol, not key dispatch. */
static int lab_flow_request_function(int frame)
{
    enum {
        SOURCE,
        CLEAR,
        BEGIN,
        START,
        PAYLOAD,
        POST,
        OUTCOME,
        COMPLETE,
        FINAL_CURRENT,
        FINAL,
        FINISH,
        DONE,
        ERROR_CURRENT,
        ERROR,
        ERROR_FINAL
    };
    int request = lab_dom_get(frame, "request");
    switch (lab_flow_request_stage(frame)) {
        case SOURCE:
            return lab_flow_call(frame, CLEAR, "native", "source", 0, 1, lab_dom_string("lab_function_source"));
        case CLEAR:
            if (!lab_dom_get(frame, "source"))
                return lab_flow_done(0);
            return lab_flow_call(frame, BEGIN, "clearFunctionRun", 0, 0, 0);
        case BEGIN: {
            int options = lab_dom_object(5);
            lab_dom_set(options, "input", lab_dom_scalar(1, lab_flow_request_truth(frame, "source")));
            lab_dom_set(options, "button", lab_dom_id("functionRun"));
            lab_dom_set(options, "disableButton", lab_dom_scalar(1, 1));
            return lab_flow_call(frame, START, "requestBeginCurrent", "request", 0, 2, lab_dom_string("function"),
                                 options);
        }
        case START:
            return lab_flow_call(frame, PAYLOAD, "native", 0, 0, 2, lab_dom_string("lab_function_start"),
                                 lab_dom_numeric(lab_dom_truth(request)));
        case PAYLOAD: {
            if (!request)
                return lab_flow_done(0);
            lab_dom_set(frame, "failed", lab_dom_numeric(ERROR_CURRENT));
            int inputs = lab_dom_object(4);
            lab_dom_push(inputs, lab_dom_get(frame, "source"));
            return lab_flow_call(frame, POST, "buildPayload", "payload", 0, 3, lab_dom_numeric(5), inputs, request);
        }
        case POST:
            return lab_flow_call(frame, OUTCOME, "post", "result", 1, 3, request, lab_dom_get(frame, "payload"),
                                 lab_dom_string("/function-run"));
        case OUTCOME:
            return lab_flow_call(frame, COMPLETE, "requestOutcome", "outcome", 0, 3, request,
                                 lab_flow_request_response(frame), lab_flow_request_data(frame));
        case COMPLETE:
            return lab_flow_call(frame, FINAL_CURRENT, "functionComplete", 0, 0, 3, request,
                                 lab_dom_get(frame, "outcome"), lab_flow_request_data(frame));
        case ERROR_CURRENT:
            lab_dom_set(frame, "failed", lab_dom_numeric(ERROR_FINAL));
            return lab_flow_request_current(frame, ERROR);
        case ERROR:
            if (!lab_flow_request_truth(frame, "current"))
                return lab_flow_request_current(frame, FINAL);
            return lab_flow_call(frame, FINAL_CURRENT, "functionException", 0, 0, 2, request,
                                 lab_dom_get(frame, "exception"));
        case FINAL_CURRENT:
            lab_dom_delete(frame, "failed");
            return lab_flow_request_current(frame, FINAL);
        case FINAL:
            if (lab_flow_request_truth(frame, "current"))
                lab_dom_disabled(lab_dom_id("functionRun"), 0);
            return lab_flow_request_finish(frame, DONE);
        case FINISH:
            return lab_flow_request_finish(frame, DONE);
        case ERROR_FINAL:
            lab_dom_set(frame, "propagate", lab_dom_get(frame, "exception"));
            lab_dom_delete(frame, "failed");
            return lab_flow_request_current(frame, FINAL);
        case DONE:
            if (lab_dom_get(frame, "propagate"))
                return lab_flow_call(frame, DONE + 100, "rethrow", 0, 0, 1, lab_dom_get(frame, "propagate"));
            return lab_flow_done(0);
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_request_integrator(int frame)
{
    enum {
        SNAPSHOT,
        PREPARE,
        GUARD,
        VALIDATE,
        CANCEL,
        ABORT,
        CURRENT,
        TEXT,
        CHECK_TEXT,
        ROWS,
        CHECK_ROWS,
        RENDER,
        BOUNDS,
        CHECK,
        BUILD,
        POST,
        DONE
    };
    int request = lab_dom_get(frame, "request"), snapshot = lab_dom_get(frame, "snapshot");
    switch (lab_flow_request_stage(frame)) {
        case SNAPSHOT:
            return lab_flow_call(frame, PREPARE, "integratorSnapshot", "snapshot", 0, 0);
        case PREPARE:
            return lab_flow_call(frame, GUARD, "refreshIntegratorForms", "preparedRows", 1, 2,
                                 lab_dom_get(snapshot, "rows"), lab_dom_get(snapshot, "text"));
        case GUARD:
            if (!lab_flow_request_truth(frame, "preparedRows"))
                return request ? lab_flow_request_current(frame, CANCEL)
                               : lab_flow_call(frame, DONE, "abort", 0, 0, 1,
                                               lab_dom_string("Obsolete integrator form preparation"));
            return lab_flow_call(frame, VALIDATE, "requestContext", "latestContext", 0, 0);
        case VALIDATE: {
            if (!lab_dom_equal(lab_dom_get(snapshot, "context"), lab_dom_get(frame, "latestContext"))) {
                if (request)
                    return lab_flow_request_current(frame, CANCEL);
                return lab_flow_call(frame, DONE, "abort", 0, 0, 1,
                                     lab_dom_string("Obsolete integrator form preparation"));
            }
            if (request)
                return lab_flow_request_current(frame, CURRENT);
            return lab_flow_call(frame, CHECK_TEXT, "integratorText", "latestText", 0, 0);
        }
        case CURRENT:
            if (!lab_flow_request_truth(frame, "current"))
                return lab_flow_request_current(frame, CANCEL);
            return lab_flow_call(frame, CHECK_TEXT, "integratorText", "latestText", 0, 0);
        case CHECK_TEXT:
            if (!lab_dom_equal(lab_dom_get(snapshot, "text"), lab_dom_get(frame, "latestText")))
                return request ? lab_flow_request_current(frame, CANCEL)
                               : lab_flow_call(frame, DONE, "abort", 0, 0, 1,
                                               lab_dom_string("Obsolete integrator form preparation"));
            return lab_flow_call(frame, CHECK_ROWS, "integratorRowsText", "latestRowsText", 0, 0);
        case CHECK_ROWS:
            if (!lab_dom_equal(lab_dom_get(snapshot, "rowsText"), lab_dom_get(frame, "latestRowsText")))
                return request ? lab_flow_request_current(frame, CANCEL)
                               : lab_flow_call(frame, DONE, "abort", 0, 0, 1,
                                               lab_dom_string("Obsolete integrator form preparation"));
            return lab_flow_call(frame, RENDER, "activeIntegratorRows", "active", 0, 2,
                                 lab_dom_get(frame, "preparedRows"), lab_dom_get(snapshot, "text"));
        case CANCEL:
            if (lab_flow_request_truth(frame, "current"))
                return lab_flow_call(frame, ABORT, "requestCancel", 0, 0, 1, lab_dom_string("evaluate"));
            /* A stale preparation still rejects even when its request has already ended. */
            return lab_flow_call(frame, DONE, "abort", 0, 0, 1, lab_dom_string("Obsolete integrator form preparation"));
        case ABORT:
            return lab_flow_call(frame, DONE, "abort", 0, 0, 1, lab_dom_string("Obsolete integrator form preparation"));
        case RENDER:
            return lab_flow_call(frame, BOUNDS, "renderIntegratorRows", 0, 0, 1,
                                 lab_dom_get(lab_dom_get(frame, "active"), "rows"));
        case BOUNDS:
            return lab_flow_call(frame, CHECK, "native", "boundsError", 0, 2,
                                 lab_dom_string("lab_payload_bounds_error"),
                                 lab_dom_get(lab_dom_get(frame, "active"), "bounds"));
        case CHECK:
            if (lab_flow_request_truth(frame, "boundsError"))
                return lab_flow_call(frame, DONE, "fail", 0, 0, 1, lab_dom_get(frame, "boundsError"));
            return lab_flow_call(frame, BUILD, "saveWorksheetState", 0, 0, 1, lab_dom_string("integrator"));
        case BUILD:
            return lab_flow_call(frame, POST, "integratorPayload", "payload", 0, 3, lab_dom_get(snapshot, "text"),
                                 lab_dom_get(lab_dom_get(frame, "active"), "bounds"), request);
        case POST:
            return lab_flow_call(frame, DONE, "post", "value", 1, 3, request, lab_dom_get(frame, "payload"),
                                 lab_dom_string("/integrator-eval"));
        default:
            return lab_flow_done(lab_dom_get(frame, "value"));
    }
}

static int lab_flow_request_land(int frame)
{
    enum { START, PARENT, BEGIN, PAYLOAD, POST, CURRENT, ACCEPT, BIND, FINISH, DONE, ERROR_CURRENT, ERROR };
    int request = lab_dom_get(frame, "request");
    switch (lab_flow_request_stage(frame)) {
        case START: {
            int cells = lab_dom_all(lab_dom_id("rendered"), "[data-almanac-land-totality]");
            if (!lab_dom_count(cells) || !lab_flow_request_truth(frame, "data"))
                return lab_flow_done(0);
            lab_dom_set(frame, "cells", cells);
            return lab_flow_call(frame, BEGIN, "requestLatestMain", "parent", 0, 0);
        }
        case BEGIN: {
            int options = lab_dom_object(5);
            lab_dom_set(options, "parent", lab_dom_get(frame, "parent"));
            return lab_flow_call(frame, PAYLOAD, "requestBegin", "request", 0, 3, lab_dom_string("land"),
                                 lab_dom_string("almanac"), options);
        }
        case PAYLOAD:
            if (!request)
                return lab_flow_done(0);
            lab_dom_set(frame, "failed", lab_dom_numeric(ERROR_CURRENT));
            return lab_flow_call(frame, POST, "landPayload", "payload", 0, 2, lab_dom_get(frame, "data"),
                                 lab_dom_get(frame, "cells"));
        case POST:
            return lab_flow_call(frame, CURRENT, "post", "result", 1, 2, request, lab_dom_get(frame, "payload"));
        case CURRENT:
            return lab_flow_request_current(frame, ACCEPT);
        case ACCEPT:
            if (!lab_flow_request_truth(frame, "current"))
                return lab_flow_request_finish(frame, DONE);
            if (!lab_flow_request_ok(frame))
                return lab_flow_call(frame, DONE, "fail", 0, 0, 1,
                                     lab_flow_request_fallback(lab_dom_get(lab_flow_request_data(frame), "error"),
                                                               "Nearest land totality search failed"));
            return lab_flow_call(frame, BIND, "native", 0, 0, 4, lab_dom_string("lab_payload_land_apply"),
                                 lab_dom_get(frame, "cells"), lab_flow_request_data(frame), lab_dom_numeric(0));
        case BIND:
            return lab_flow_call(frame, FINISH, "bindAlmanacTotalityActions", 0, 0, 1, lab_dom_id("rendered"));
        case ERROR_CURRENT:
            lab_dom_set(frame, "failed", lab_dom_numeric(FINISH));
            return lab_flow_request_current(frame, ERROR);
        case ERROR:
            if (!lab_flow_request_truth(frame, "current"))
                return lab_flow_request_finish(frame, DONE);
            return lab_flow_call(frame, FINISH, "native", 0, 0, 4, lab_dom_string("lab_payload_land_apply"),
                                 lab_dom_get(frame, "cells"), 0,
                                 lab_dom_numeric(lab_dom_equal(lab_dom_get(lab_dom_get(frame, "exception"), "name"),
                                                               lab_dom_string("AbortError"))
                                                     ? 1
                                                     : 2));
        case FINISH:
            return lab_flow_request_finish(frame, DONE);
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_request_holidays(int frame)
{
    enum { MODE, BEGIN, STATUS, FETCH, CURRENT, ACCEPT, READY, FINISH, DONE, ERROR_CURRENT, ERROR };
    int request = lab_dom_get(frame, "request");
    switch (lab_flow_request_stage(frame)) {
        case MODE:
            return lab_flow_call(frame, BEGIN, "currentMode", "mode", 0, 0);
        case BEGIN:
            if (!lab_dom_equal(lab_dom_get(frame, "mode"), lab_dom_string("datetime")))
                return lab_flow_done(0);
            return lab_flow_call(frame, STATUS, "requestBegin", "request", 0, 2, lab_dom_string("holidays"),
                                 lab_dom_string("datetime"));
        case STATUS:
            if (!request)
                return lab_flow_done(0);
            return lab_flow_call(frame, FETCH, "setStatus", 0, 0, 1, lab_dom_string("Refreshing holidays..."));
        case FETCH:
            lab_dom_set(frame, "failed", lab_dom_numeric(ERROR_CURRENT));
            return lab_flow_call(frame, CURRENT, "fetchDatetime", "result", 1, 2, 0, request);
        case CURRENT:
            return lab_flow_request_current(frame, ACCEPT);
        case ACCEPT:
            if (!lab_flow_request_truth(frame, "current"))
                return lab_flow_request_finish(frame, DONE);
            if (!lab_flow_request_ok(frame))
                return lab_flow_call(frame, FINISH, "setStatus", 0, 0, 1, lab_dom_string("Error"));
            return lab_flow_call(frame, READY, "setDatetimeLocalText", 0, 0, 2,
                                 lab_flow_request_fallback(lab_dom_get(lab_flow_request_data(frame), "local"), ""),
                                 lab_dom_truth(lab_dom_get(lab_flow_request_data(frame), "local_sections"))
                                     ? lab_dom_get(lab_flow_request_data(frame), "local_sections")
                                     : lab_dom_object(4));
        case READY:
            return lab_flow_call(frame, FINISH, "setStatus", 0, 0, 1, lab_dom_string("Ready"));
        case ERROR_CURRENT:
            lab_dom_set(frame, "failed", lab_dom_numeric(FINISH));
            return lab_flow_request_current(frame, ERROR);
        case ERROR:
            if (lab_flow_request_truth(frame, "current"))
                return lab_flow_call(frame, FINISH, "setStatus", 0, 0, 1, lab_dom_string("Error"));
            return lab_flow_request_finish(frame, DONE);
        case FINISH:
            return lab_flow_request_finish(frame, DONE);
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_request_mobile(int frame)
{
    enum { START, RESPONSE, DATA, DONE };
    switch (lab_flow_request_stage(frame)) {
        case START:
            if (!lab_dom_id("mobileAccess") || !lab_dom_id("mobileUrl") || !lab_dom_id("mobileQr"))
                return lab_flow_done(0);
            lab_dom_set(frame, "failed", lab_dom_numeric(DONE));
            return lab_flow_call(frame, RESPONSE, "mobileFetch", "response", 1, 0);
        case RESPONSE:
            if (!lab_dom_truth(lab_dom_get(lab_dom_get(frame, "response"), "ok")))
                return lab_flow_done(0);
            return lab_flow_call(frame, DATA, "responseData", "data", 1, 1, lab_dom_get(frame, "response"));
        case DATA:
            return lab_flow_call(frame, DONE, "native", 0, 0, 2, lab_dom_string("lab_payload_mobile"),
                                 lab_dom_get(frame, "data"));
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_request_forms(int frame)
{
    enum { FETCH, DECODE, VALIDATE, DONE };
    switch (lab_flow_request_stage(frame)) {
        case FETCH:
            return lab_flow_call(frame, DECODE, "rawPost", "response", 1, 3, lab_dom_string("/forms"),
                                 lab_dom_get(frame, "payload"), lab_dom_get(lab_dom_get(frame, "options"), "signal"));
        case DECODE:
            return lab_flow_call(frame, VALIDATE, "responseData", "data", 1, 1, lab_dom_get(frame, "response"));
        case VALIDATE: {
            int response = lab_dom_get(frame, "response"), data = lab_dom_get(frame, "data");
            if (lab_dom_truth(lab_dom_get(response, "ok")) && lab_dom_truth(lab_dom_get(data, "ok")))
                return lab_flow_done(data);
            int error = lab_dom_get(data, "error");
            if (!lab_dom_truth(error))
                error = lab_dom_join(lab_dom_string("Form preparation failed ("),
                                     lab_dom_text(lab_dom_get(response, "status")), ")");
            return lab_flow_call(frame, DONE, "fail", 0, 0, 1, error);
        }
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_request_presentation(int frame)
{
    enum { CONTEXT, CURRENT, FETCH, DECODE, CHECK_CURRENT, VALIDATE, ACCEPT, DONE };
    int request = lab_dom_get(frame, "request");
    switch (lab_flow_request_stage(frame)) {
        case CONTEXT:
            return lab_flow_call(frame, CURRENT, "requestContext", "context", 0, 0);
        case CURRENT:
        case CHECK_CURRENT:
            return lab_flow_call(frame, lab_flow_request_stage(frame) == CURRENT ? FETCH : VALIDATE,
                                 request ? "requestCurrent" : "requestContext", "current", 0, request ? 1 : 0, request);
        case FETCH:
        case VALIDATE: {
            int live = request ? lab_flow_request_truth(frame, "current")
                               : lab_dom_equal(lab_dom_get(frame, "context"), lab_dom_get(frame, "current"));
            if (!live)
                return lab_flow_call(frame, DONE, "abort", 0, 0, 1, lab_dom_string("Obsolete presentation request"));
            if (lab_flow_request_stage(frame) == FETCH)
                return lab_flow_call(frame, DECODE, "rawPost", "response", 1, 3, lab_dom_string("/presentation"),
                                     lab_dom_get(frame, "payload"),
                                     lab_dom_get(lab_dom_get(request, "controller"), "signal"));
            int data = lab_dom_get(frame, "data"), response = lab_dom_get(frame, "response");
            if (!lab_dom_truth(lab_dom_get(response, "ok")) || !lab_dom_truth(lab_dom_get(data, "ok")))
                return lab_flow_call(
                    frame, DONE, "fail", 0, 0, 1,
                    lab_flow_request_fallback(lab_dom_get(data, "error"), "Native presentation request failed"));
            return lab_flow_call(frame, DONE, "installPresentation", 0, 0, 1, data);
        }
        case DECODE:
            return lab_flow_call(frame, CHECK_CURRENT, "responseData", "data", 1, 1, lab_dom_get(frame, "response"));
        default:
            return lab_flow_done(lab_dom_get(frame, "data"));
    }
}

static int lab_flow_request_editor(int frame, int view)
{
    enum { CACHE, RESOURCES, CONTEXT, SELECT, DONE };
    switch (lab_flow_request_stage(frame)) {
        case CACHE: {
            int source = lab_dom_clean(lab_dom_text(lab_flow_request_fallback(lab_dom_get(frame, "text"), "")), 0);
            lab_dom_set(frame, "source", source);
            int editors = lab_dom_get(view, "editors");
            if (lab_dom_member(editors, source))
                return lab_flow_done(lab_dom_map_get(editors, source));
            return lab_flow_call(frame, CONTEXT, "requestContext", "context", 0, 0);
        }
        case CONTEXT:
            return lab_flow_call(frame, SELECT, "editorRequests", "requests", 0, 0);
        case SELECT: {
            int source = lab_dom_get(frame, "source"), requests = lab_dom_get(frame, "requests");
            int pending = lab_dom_map_get(requests, source);
            if (pending && lab_dom_equal(lab_dom_get(pending, "context"), lab_dom_get(frame, "context")))
                return lab_flow_call(frame, DONE, "awaitValue", "value", 1, 1, lab_dom_get(pending, "promise"));
            pending = lab_dom_object(5);
            lab_dom_set(pending, "context", lab_dom_get(frame, "context"));
            lab_dom_set(pending, "promise", 0);
            int payload = lab_dom_object(5);
            lab_dom_set(payload, "action", lab_dom_string("editor"));
            lab_dom_set(payload, "operation", lab_dom_string("analyse"));
            lab_dom_set(payload, "text", source);
            return lab_flow_call(frame, DONE, "editorPromise", "value", 1, 3, source, pending, payload);
        }
        default:
            return lab_flow_done(lab_dom_get(frame, "value"));
    }
}

static int lab_flow_request_presentation_dispatch(int frame, int view)
{
    return lab_flow_request_truth(frame, "editor") ? lab_flow_request_editor(frame, view)
                                                   : lab_flow_request_presentation(frame);
}

static int lab_flow_request_calculus(int frame)
{
    enum {
        START,
        CAPTURE,
        COMMIT,
        CURRENT,
        SOURCE,
        ASSEMBLE,
        ASSEMBLED,
        INPUT,
        SHOW,
        TITLE,
        STATUS,
        FETCH,
        PREPARED_CURRENT,
        MATRIX_FETCH,
        OUTCOME,
        ACCEPT,
        CLEAR_ERROR,
        DISPLAY,
        SCALAR,
        POST_CURRENT,
        VARIABLES,
        DIFFERENTIABLE,
        BUTTONS,
        COMMIT_STATE,
        READY,
        DONE,
        ERROR_CURRENT,
        ERROR_CLEAR,
        ERROR_VARIABLES,
        ERROR_DIFFERENTIABLE,
        ERROR_BUTTONS,
        ERROR_RENDER,
        ERROR_DIGITS,
        ERROR_STATUS,
        INPUT_ACCEPT,
        RETURN = 999
    };
    int request = lab_dom_get(frame, "request"), action = lab_dom_get(frame, "action"), wrt = lab_dom_get(frame, "wrt");
    int matrix = lab_flow_request_truth(frame, "matrix"), scalar = lab_flow_request_truth(frame, "scalar");
    int integral = lab_dom_equal(action, lab_dom_string("integral"));
    int data = lab_flow_request_data(frame), result = lab_dom_get(frame, "calculus");
    switch (lab_flow_request_stage(frame)) {
        case START:
            return lab_flow_call(frame, CAPTURE, "calculusInitial", "initial", 0, 0);
        case CAPTURE: {
            int initial = lab_dom_get(frame, "initial"), mode = lab_dom_get(initial, "mode");
            matrix = lab_dom_equal(mode, lab_dom_string("matrix"));
            scalar = matrix && lab_dom_truth(lab_dom_get(initial, "scalar"));
            lab_dom_set(frame, "matrix", lab_dom_scalar(1, matrix));
            lab_dom_set(frame, "scalar", lab_dom_scalar(1, scalar));
            int options = lab_dom_object(5);
            lab_dom_set(options, "button", lab_dom_get(frame, "actionButton"));
            lab_dom_set(options, "variable", lab_dom_scalar(1, lab_dom_truth(wrt)));
            lab_dom_set(options, "scalar", lab_dom_scalar(1, scalar));
            return lab_flow_call(frame, RETURN, "runRequestFlow", "value", 1, 6, lab_dom_numeric(7), frame,
                                 lab_dom_numeric(COMMIT), action, mode, options);
        }
        case COMMIT:
            return lab_flow_call(frame, CURRENT, "calculusCapture", "snapshot", 0, 0);
        case CURRENT:
            lab_dom_set(frame, "failed", lab_dom_numeric(ERROR_CURRENT));
            return lab_flow_call(frame, SOURCE, "commitBindings", 0, 1, 0);
        case SOURCE:
            return lab_flow_request_current(frame, ASSEMBLE);
        case ASSEMBLE:
            if (!lab_flow_request_truth(frame, "current"))
                return lab_flow_done(0);
            return lab_flow_call(frame, ASSEMBLED, scalar ? "matrixScalar" : "expressionText", "source", 0, 0);
        case ASSEMBLED:
            if (!matrix)
                return lab_flow_call(frame, INPUT, "expressionWithVisibleBindings", "source", 1, 2,
                                     lab_dom_get(frame, "source"),
                                     lab_dom_get(lab_dom_get(frame, "snapshot"), "bindings"));
            return lab_flow_request_current(frame, INPUT_ACCEPT);
        case INPUT:
            return lab_flow_request_current(frame, INPUT_ACCEPT);
        case INPUT_ACCEPT:
            if (!lab_flow_request_truth(frame, "current"))
                return lab_flow_done(0);
            return lab_flow_call(frame, SHOW, "requestInput", "accepted", 0, 3, request, lab_dom_get(frame, "source"),
                                 wrt);
        case SHOW:
            if (!lab_flow_request_truth(frame, "accepted"))
                return lab_flow_done(0);
            return lab_flow_call(frame, TITLE, "showResults", 0, 0, 0);
        case TITLE:
            return lab_flow_call(frame, STATUS, "calculusTitle", 0, 0, 1,
                                 lab_dom_join(lab_dom_join(lab_dom_text(wrt), lab_dom_string(" "), ""),
                                              lab_dom_text(action), " RESULT"));
        case STATUS:
            return lab_flow_call(
                frame, FETCH, "setStatus", 0, 0, 1,
                lab_dom_join(lab_dom_string(integral ? "Integrating with respect to " : "Differentiating d/d"),
                             lab_dom_text(wrt), "..."));
        case FETCH: {
            if (matrix && !scalar) {
                int payload = lab_dom_object(5);
                lab_dom_set(payload, "action", lab_dom_string("editor"));
                lab_dom_set(payload, "operation", lab_dom_string("calculus"));
                lab_dom_set(payload, "calculus", action);
                lab_dom_set(payload, "text", lab_dom_get(frame, "source"));
                lab_dom_set(payload, "name", wrt);
                return lab_flow_call(frame, PREPARED_CURRENT, "requestPresentation", "prepared", 1, 2, payload,
                                     request);
            }
            return lab_flow_call(frame, OUTCOME, "fetchCalculus", "result", 1, 4, lab_dom_get(frame, "source"), wrt,
                                 lab_dom_string(integral ? "integral" : ""), request);
        }
        case PREPARED_CURRENT:
            return lab_flow_request_current(frame, MATRIX_FETCH);
        case MATRIX_FETCH: {
            if (!lab_flow_request_truth(frame, "current"))
                return lab_flow_done(0);
            int options = lab_dom_object(5);
            lab_dom_set(options, "matrixText",
                        lab_dom_get(lab_dom_get(lab_dom_get(frame, "prepared"), "editor"), "expression"));
            lab_dom_set(options, "operation", lab_dom_string("eval"));
            lab_dom_set(options, "operand", lab_dom_string(""));
            lab_dom_set(options, "skipSave", lab_dom_scalar(1, 1));
            lab_dom_set(options, "request", request);
            return lab_flow_call(frame, OUTCOME, "fetchMatrixOptions", "result", 1, 1, options);
        }
        case OUTCOME: {
            result = lab_dom_key_get(lab_dom_get(lab_dom_get(data, "presentation"), "calculus"), action);
            lab_dom_set(frame, "calculus", result);
            int available = matrix && !scalar ? lab_dom_scalar(1, 1)
                                              : lab_flow_request_fallback(lab_dom_get(result, "expression"), "");
            return lab_flow_call(frame, ACCEPT, "requestOutcome", "outcome", 0, 4, request,
                                 lab_flow_request_response(frame), data, available);
        }
        case ACCEPT: {
            unsigned outcome = (unsigned)lab_dom_number(lab_dom_get(frame, "outcome"));
            if (!outcome)
                return lab_flow_done(0);
            if (outcome == 1) {
                int error = lab_dom_get(data, "error");
                if (!lab_dom_truth(error))
                    error = lab_dom_get(data, "raw");
                if (!lab_dom_truth(error))
                    error = lab_dom_join(lab_dom_join(lab_dom_string("No "), lab_dom_text(action), " for "),
                                         lab_dom_text(wrt), "");
                return lab_flow_call(frame, DONE, "fail", 0, 0, 1, error);
            }
            int options = lab_dom_object(5);
            lab_dom_set(options, "keepBindings", lab_dom_scalar(1, 1));
            return lab_flow_call(frame, CLEAR_ERROR, "clearResultDetails", 0, 0, 1, options);
        }
        case CLEAR_ERROR:
            return lab_flow_call(frame, DISPLAY, "clearRenderedError", 0, 0, 0);
        case DISPLAY:
            return lab_flow_call(frame, SCALAR, matrix && !scalar ? "displayMatrixResult" : "displayCalculusResult", 0,
                                 0, 1, matrix && !scalar ? data : result);
        case SCALAR:
            if (matrix && !scalar)
                return lab_flow_call(frame, POST_CURRENT, "valueTitle", 0, 0, 1,
                                     lab_dom_string(lab_dom_truth(lab_dom_get(data, "value")) ? "Value" : "Summary"));
            if (scalar)
                return lab_flow_call(frame, POST_CURRENT, "setMatrixPrettyResult", 0, 1, 2,
                                     lab_dom_get(result, "expression"), lab_dom_string(""));
            return lab_flow_call(frame, POST_CURRENT, "derivative", 0, 0, 1,
                                 lab_dom_equal(action, lab_dom_string("derivative")) ? lab_dom_get(result, "expression")
                                                                                     : lab_dom_string(""));
        case POST_CURRENT:
            return lab_flow_request_current(frame, VARIABLES);
        case VARIABLES:
            if (!lab_flow_request_truth(frame, "current"))
                return lab_flow_done(0);
            return lab_flow_call(frame, DIFFERENTIABLE, matrix ? "calculusVariables" : "calculusBindingVariables", 0, 0,
                                 1,
                                 matrix ? lab_dom_get(lab_dom_get(frame, "snapshot"), "variables")
                                        : lab_dom_get(data, "binding_values"));
        case DIFFERENTIABLE: {
            int enabled =
                matrix
                    ? lab_dom_get(lab_dom_get(frame, "snapshot"), "differentiable")
                    : lab_dom_scalar(1, !lab_dom_equal(lab_dom_clean(lab_dom_text(lab_flow_request_fallback(
                                                                         lab_dom_get(data, "differentiable"), "yes")),
                                                                     1),
                                                       lab_dom_string("no")));
            return lab_flow_call(frame, BUTTONS, "calculusDifferentiable", 0, 0, 1, enabled);
        }
        case BUTTONS:
            return lab_flow_call(frame, COMMIT_STATE, "calculusButtons", 0, 0, 0);
        case COMMIT_STATE:
            if (matrix && !scalar)
                return lab_flow_call(frame, READY, "commitModeState", 0, 0, 0);
            return lab_flow_call(frame, DONE, "setStatus", 0, 0, 1, lab_dom_string("Ready"));
        case READY:
            return lab_flow_call(frame, DONE, "setStatus", 0, 0, 1, lab_dom_string("Ready"));
        case ERROR_CURRENT:
            return lab_flow_request_current(frame, ERROR_CLEAR);
        case ERROR_CLEAR: {
            if (!lab_flow_request_truth(frame, "current"))
                return lab_flow_done(0);
            int options = lab_dom_object(5);
            lab_dom_set(options, "keepBindings", lab_dom_scalar(1, 1));
            return lab_flow_call(frame, matrix ? ERROR_VARIABLES : ERROR_RENDER, "clearResultDetails", 0, 0, 1,
                                 options);
        }
        case ERROR_VARIABLES:
            return lab_flow_call(frame, ERROR_DIFFERENTIABLE, "calculusVariables", 0, 0, 1,
                                 lab_dom_get(lab_dom_get(frame, "snapshot"), "variables"));
        case ERROR_DIFFERENTIABLE:
            return lab_flow_call(frame, ERROR_BUTTONS, "calculusDifferentiable", 0, 0, 1,
                                 lab_dom_get(lab_dom_get(frame, "snapshot"), "differentiable"));
        case ERROR_BUTTONS:
            return lab_flow_call(frame, ERROR_RENDER, "calculusButtons", 0, 0, 0);
        case ERROR_RENDER:
            return lab_flow_call(frame, ERROR_DIGITS, "setRenderedError", 0, 0, 1, lab_dom_get(frame, "error"));
        case ERROR_DIGITS:
            return lab_flow_call(frame, ERROR_STATUS, "resetRenderedDigits", 0, 0, 0);
        case ERROR_STATUS:
            return lab_flow_call(frame, DONE, "setStatus", 0, 0, 1, lab_dom_string("Error"));
        case RETURN:
            return lab_flow_done(lab_dom_get(frame, "value"));
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_request_solver(int frame)
{
    enum { PREPARE, BEGIN, INSTALL, POST, PUBLISH, CLEANUP, FINISH, DONE, ERROR };
    int pending = lab_dom_get(frame, "pending"), request = lab_dom_get(frame, "request");
    switch (lab_flow_request_stage(frame)) {
        case PREPARE:
            return lab_flow_call(frame, BEGIN, "solverPrepare", "pending", 0, 0);
        case BEGIN:
            if (!pending)
                return lab_flow_done(0);
            return lab_flow_call(frame, INSTALL, "requestBegin", "request", 0, 3, lab_dom_get(pending, "operation"),
                                 lab_dom_get(pending, "mode"), lab_dom_get(pending, "options"));
        case INSTALL:
            if (!request)
                return lab_flow_done(0);
            return lab_flow_call(frame, POST, "solverPending", 0, 0, 2, pending, request);
        case POST:
            lab_dom_set(frame, "failed", lab_dom_numeric(ERROR));
            return lab_flow_call(frame, PUBLISH, "post", "result", 1, 3, request, lab_dom_get(pending, "payload"),
                                 lab_dom_get(pending, "endpoint"));
        case PUBLISH:
            return lab_flow_call(frame, CLEANUP, "solverPublish", 0, 0, 4, pending, lab_flow_request_response(frame),
                                 lab_flow_request_data(frame), lab_dom_numeric(0));
        case ERROR:
            lab_dom_set(frame, "failed", lab_dom_numeric(CLEANUP));
            return lab_flow_call(frame, CLEANUP, "solverPublish", 0, 0, 4, pending, 0, 0, lab_dom_numeric(1));
        case CLEANUP:
            lab_dom_set(frame, "failed", lab_dom_numeric(FINISH));
            return lab_flow_call(frame, FINISH, "solverRelease", 0, 0, 1, pending);
        case FINISH:
            return lab_flow_request_finish(frame, DONE);
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_request_fetch(int frame)
{
    enum { BUILD, SAVE, POST, DONE };
    unsigned mode = (unsigned)lab_dom_number(lab_dom_get(frame, "mode"));
    static const char *const endpoints[] = {"/matrix-eval", "/equation-eval", "/diffequation-eval", "/datetime-eval",
                                            "/almanac-eval"};
    static const char *const modes[] = {"matrix", "equation", "diffequation", "datetime", "almanac"};
    if (mode >= sizeof endpoints / sizeof *endpoints)
        return lab_flow_done(0);
    switch (lab_flow_request_stage(frame)) {
        case BUILD:
            if (mode == 1 || mode == 2)
                return lab_flow_call(frame, SAVE, "saveWorksheetState", 0, 0, 1, lab_dom_string(modes[mode]));
            return lab_flow_call(frame, SAVE, "fetchPayload", "payload", 0, 1, frame);
        case SAVE:
            if (mode == 1 || mode == 2)
                return lab_flow_call(frame, POST, "fetchPayload", "payload", 0, 1, frame);
            if (mode == 0 && !lab_dom_truth(lab_dom_get(lab_dom_get(frame, "payload"), "transient")))
                return lab_flow_call(frame, POST, "saveWorksheetState", 0, 0, 1, lab_dom_string("matrix"));
            if (mode >= 3)
                return lab_flow_call(frame, POST, mode == 3 ? "saveLastDatetimeState" : "saveLastAlmanacState", 0, 0,
                                     0);
            return lab_flow_call(frame, DONE, "post", "value", 1, 3, lab_dom_get(frame, "request"),
                                 lab_dom_get(frame, "payload"), lab_dom_string(endpoints[mode]));
        case POST:
            return lab_flow_call(frame, DONE, "post", "value", 1, 3, lab_dom_get(frame, "request"),
                                 lab_dom_get(frame, "payload"), lab_dom_string(endpoints[mode]));
        default:
            return lab_flow_done(lab_dom_get(frame, "value"));
    }
}

/* Direct workflow indexing; stages remain private to each independent protocol. */
int lab_flow_request(unsigned kind, int frame, int view)
{
    if (kind == 7)
        return lab_flow_request_presentation_dispatch(frame, view);
    static int (*const workflows[])(int) = {
        [0] = lab_flow_request_function, [1] = lab_flow_request_calculus,     [2] = lab_flow_request_integrator,
        [3] = lab_flow_request_land,     [4] = lab_flow_request_holidays,     [5] = lab_flow_request_mobile,
        [6] = lab_flow_request_forms,    [7] = lab_flow_request_presentation, [8] = lab_flow_request_solver,
        [9] = lab_flow_request_fetch,
    };
    return kind < sizeof workflows / sizeof *workflows ? workflows[kind](frame) : lab_flow_done(0);
}
