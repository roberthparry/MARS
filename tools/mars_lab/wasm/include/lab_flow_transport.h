/**
 * @file lab_flow_transport.h
 * @brief Native request execution and response-publication continuations.
 *
 * Private browser workflow interface. The transport adapter retains controllers,
 * timers, response objects and callbacks; native continuations select execution,
 * stale-response checks, metadata installation and error recovery. Callback values
 * and thrown exceptions remain uncoerced browser values in the request-local frame.
 */
#ifndef LAB_FLOW_TRANSPORT_H
#define LAB_FLOW_TRANSPORT_H

#include "lab_requests.h"

/** @brief Closed workflow selectors for lab_flow_transport; numeric values are browser ABI. */
typedef enum {
    LAB_FLOW_TRANSPORT_POST   = 0, /**< Publish a POST response. */
    LAB_FLOW_TRANSPORT_RUN    = 1, /**< Execute a request. */
    LAB_FLOW_TRANSPORT_UI_RUN = 2, /**< Execute a request with UI recovery. */
} lab_flow_transport_kind_t;

/** @brief Advance a transport continuation without retaining scoped handles.
 * @param kind POST zero, ordinary execution one, UI execution two; other indices are inert.
 * @param frame Scoped browser handle to the mutable request-local frame containing stage, arguments and service
 * results.
 * @param view Reserved scoped browser capability-view handle; unused by this request-local workflow.
 * @return Scoped browser handle to a continuation plan. The host reads frame.result after successful completion.
 */
int lab_flow_transport(lab_flow_transport_kind_t kind, int frame, int view);

/** @brief Select synchronous browser effects when beginning or cancelling a request.
 * @param action Native request operation; LAB_REQUEST_EVALUATE selects Evaluate, all others suppress that button effect.
 * Use an invalid operation index when cancelling rather than beginning a request.
 * @param channel Native numeric identifier of the request channel being started or cancelled, not a browser handle.
 * @param main_channel Numeric identifier of the main request channel; equality with channel invalidates solver output.
 * @return Bit zero selects the default Evaluate button; bit one invalidates solver rendering.
 */
unsigned lab_transport_policy(lab_request_operation_t action, lab_request_channel_t channel,
                               lab_request_channel_t main_channel);

#endif
