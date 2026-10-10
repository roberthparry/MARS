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

/** @brief Advance a transport continuation without retaining scoped handles.
 * @param kind POST zero, ordinary execution one, UI execution two; other indices are inert.
 * @param frame Borrowed mutable request-local frame containing arguments and browser results.
 * @param view Borrowed capability view, unused by this closure-local workflow.
 * @return Scoped continuation plan. The browser reads frame.result after successful completion.
 */
int lab_flow_transport(unsigned kind, int frame, int view);

/** @brief Select synchronous browser effects when beginning or cancelling a request.
 * @param action Native operation index; evaluate is zero. Use an invalid index when cancelling.
 * @param channel Native request channel.
 * @param main_channel Native main request channel.
 * @return Bit zero selects the default Evaluate button; bit one invalidates solver rendering.
 */
unsigned lab_transport_policy(unsigned action, unsigned channel, unsigned main_channel);

#endif
