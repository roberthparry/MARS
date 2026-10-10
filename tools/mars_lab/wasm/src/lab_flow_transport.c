/**
 * @file lab_flow_transport.c
 * @brief POST publication, request execution and UI recovery for the Lab browser.
 *
 * Keeps application sequencing in C while the browser retains fetch, decoding,
 * callbacks, controllers and timers. Staleness is checked before transport and
 * on both sides of response decoding. Cleanup executes for callback and recovery
 * failures, including non-UI requests. Error strings are read only when a current
 * UI request is actually presented; arbitrary thrown values retain their identity.
 */
#include "lab_dom.h"
#include "lab_flow.h"
#include "lab_flow_transport.h"

/* Synchronous resource adapters apply the effects selected by native request policy. */
unsigned lab_transport_policy(unsigned action, unsigned channel, unsigned main_channel)
{
    return (action == 0 ? 1u : 0u) | (channel == main_channel ? 2u : 0u);
}

static unsigned lab_flow_transport_stage(int frame)
{
    return (unsigned)lab_dom_number(lab_dom_get(frame, "stage"));
}

static int lab_flow_transport_post(int frame)
{
    enum { ASSERT_START, ENDPOINT, FETCH, ASSERT_RESPONSE, DECODE, ASSERT_DATA, ACCEPT, SYNTAX, RESULT, DONE };
    int request = lab_dom_get(frame, "request");
    switch (lab_flow_transport_stage(frame)) {
        case ASSERT_START:
            return lab_flow_call(frame, ENDPOINT, "assertCurrent", 0, 0, 1, request);
        case ENDPOINT:
            if (lab_dom_truth(request))
                return lab_flow_call(frame, FETCH, "endpoint", "endpoint", 0, 1, request);
            lab_dom_set(frame, "endpoint", lab_dom_get(frame, "fallback"));
            lab_dom_set(frame, "stage", lab_dom_numeric(FETCH));
            return lab_flow_transport_post(frame);
        case FETCH:
            if (!lab_dom_truth(lab_dom_get(frame, "endpoint")))
                return lab_flow_call(frame, DONE, "invalidEndpoint", 0, 0, 0);
            return lab_flow_call(frame, ASSERT_RESPONSE, "fetch", "response", 1, 2,
                                 lab_dom_get(frame, "endpoint"), frame);
        case ASSERT_RESPONSE:
            return lab_flow_call(frame, DECODE, "assertCurrent", 0, 0, 1, request);
        case DECODE:
            return lab_flow_call(frame, ASSERT_DATA, "decode", "data", 1, 1, lab_dom_get(frame, "response"));
        case ASSERT_DATA:
            return lab_flow_call(frame, ACCEPT, "assertCurrent", 0, 0, 1, request);
        case ACCEPT: {
            if (lab_dom_truth(lab_dom_get(lab_dom_get(frame, "response"), "ok"))) {
                int ok = lab_dom_get(lab_dom_get(frame, "data"), "ok");
                /* Only the actual boolean false rejects publication: absence, zero and empty text do not. */
                if (lab_dom_type(ok) != 1 || lab_dom_truth(ok))
                    return lab_flow_call(frame, SYNTAX, "presentation", 0, 0, 1, frame);
            }
            return lab_flow_call(frame, DONE, "result", "result", 0, 1, frame);
        }
        case SYNTAX:
            return lab_flow_call(frame, RESULT, "syntax", 0, 0, 1, frame);
        case RESULT:
            return lab_flow_call(frame, DONE, "result", "result", 0, 1, frame);
        default:
            return lab_flow_done(0);
    }
}

static int lab_flow_transport_run(int frame, int ui)
{
    enum {
        BEGIN,
        CALLBACK,
        FINISH,
        DONE,
        OUTER_CURRENT,
        OUTER_DECIDE,
        UI_CURRENT,
        UI_DECIDE,
        UI_DIGITS,
        UI_CLEAR,
        UI_STATUS,
        RETHROW
    };
    int request = lab_dom_get(frame, "request");
    switch (lab_flow_transport_stage(frame)) {
        case BEGIN:
            return lab_flow_call(frame, CALLBACK, "begin", "request", 0, 1, frame);
        case CALLBACK:
            if (!lab_dom_truth(request))
                return lab_flow_done(0);
            lab_dom_set(frame, "failed", lab_dom_numeric(ui ? UI_CURRENT : OUTER_CURRENT));
            return lab_flow_call(frame, FINISH, "callback", "result", 1, 1, frame);
        case FINISH:
            /* A finaliser failure replaces any earlier callback/recovery failure, exactly like finally. */
            lab_dom_delete(frame, "failed");
            return lab_flow_call(frame, lab_dom_truth(lab_dom_get(frame, "rethrow")) ? RETHROW : DONE,
                                 "finish", 0, 0, 1, request);
        case OUTER_CURRENT:
            /* If the ownership check itself throws, its exception still passes through the finaliser. */
            lab_dom_set(frame, "rethrow", lab_dom_scalar(1, 1));
            lab_dom_set(frame, "failed", lab_dom_numeric(FINISH));
            return lab_flow_call(frame, OUTER_DECIDE, "current", "current", 0, 1, request);
        case OUTER_DECIDE:
            lab_dom_set(frame, "rethrow", lab_dom_scalar(1, lab_dom_truth(lab_dom_get(frame, "current"))));
            lab_dom_set(frame, "stage", lab_dom_numeric(FINISH));
            return lab_flow_transport_run(frame, ui);
        case UI_CURRENT:
            /* UI recovery has the ordinary runner's enclosing catch and finally. */
            lab_dom_set(frame, "failed", lab_dom_numeric(OUTER_CURRENT));
            return lab_flow_call(frame, UI_DECIDE, "current", "current", 0, 1, request);
        case UI_DECIDE:
            if (!lab_dom_truth(lab_dom_get(frame, "current"))) {
                lab_dom_set(frame, "stage", lab_dom_numeric(FINISH));
                return lab_flow_transport_run(frame, ui);
            }
            return lab_flow_call(frame, UI_DIGITS, "renderError", 0, 0, 1, lab_dom_get(frame, "error"));
        case UI_DIGITS:
            return lab_flow_call(frame, UI_CLEAR, "resetDigits", 0, 0, 0);
        case UI_CLEAR: {
            int options = lab_dom_object(5);
            lab_dom_set(options, "keepBindings", lab_dom_scalar(1, 1));
            return lab_flow_call(frame, UI_STATUS, "clearResults", 0, 0, 1, options);
        }
        case UI_STATUS:
            return lab_flow_call(frame, FINISH, "status", 0, 0, 1, lab_dom_string("Error"));
        case RETHROW:
            return lab_flow_call(frame, DONE, "rethrow", 0, 0, 1, frame);
        default:
            return lab_flow_done(0);
    }
}

/* The browser owns arbitrary result values; native plans never coerce null, undefined or thrown primitives. */
int lab_flow_transport(unsigned kind, int frame, int view)
{
    (void)view;
    if (kind == 0)
        return lab_flow_transport_post(frame);
    if (kind == 1 || kind == 2)
        return lab_flow_transport_run(frame, kind == 2);
    return lab_flow_done(0);
}
