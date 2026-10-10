/**
 * @file lab_event_dom.h
 * @brief Private interface: native worksheet event registration and dispatch for the MARS Lab browser.
 *
 * Owns control subscriptions, keyboard and calendar policy, card zoom and event
 * cancellation. Indexed handlers return ordered service calls for browser promises;
 * no host service runs until the scoped C call has ended. DOM nodes are borrowed
 * within a call; the host retains actual targets for installed listeners. Native
 * mathematical results are never parsed or rewritten here.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 */
#ifndef LAB_WASM_EVENT_DOM_H
#define LAB_WASM_EVENT_DOM_H

/** @brief Return ordered host service calls after synchronous native event decisions.
 * @param action Indexed event action 0..31, or calendar mode in the high byte and field in the low byte.
 * @param event Scoped browser Event containing currentTarget, type and key/modifier data.
 * Publishes a record with calls and optional prevent/stop flags through lab_dom_return.
 */
void lab_events_dispatch(unsigned action, int event);

/** @brief Install worksheet control listeners and project initial card controls.
 * @param document Scoped browser Document used for visibility events.
 * @param window Scoped browser Window used for resize, scroll and pagehide events.
 * Repeated installation does not duplicate listeners. Browser capabilities retain real targets only.
 */
void lab_events_install(int document, int window);

#endif
