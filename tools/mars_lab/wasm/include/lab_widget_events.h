/**
 * @file lab_widget_events.h
 * @brief Calendar and tooltip event ownership for the freestanding Lab browser.
 *
 * Registers date-picker, observer-offset and tooltip events through the generic
 * browser bridge. C chooses event actions and cancellation; deferred browser
 * services supply clocks, date-control conversion and asynchronous DOM effects.
 * Scoped integer handles are never retained. The host owns the picker context
 * object and listener targets for their browser lifetime.
 */
#ifndef LAB_WASM_WIDGET_EVENTS_H
#define LAB_WASM_WIDGET_EVENTS_H

/**
 * @brief Install date-picker and tooltip subscriptions idempotently.
 * @param document Scoped browser Document.
 * @param window Scoped browser Window.
 * @param state Scoped picker context containing input, button and shell nodes and native date accessors.
 */
void lab_widget_events_install(int document, int window, int state);

/**
 * @brief Select actions for a delivered browser event.
 * @param action Registered action index, 0..18; unknown actions return an empty plan.
 * @param event Scoped browser event with currentTarget, target and keyboard/pointer fields.
 * @param state Scoped mutable picker context; no handle is retained.
 * @details Returns a browser event plan via lab_dom_return. Service arguments are
 * browser values copied out of the handle scope. Date text remains opaque, and
 * focus/commit services execute only after dispatch returns.
 */
void lab_widget_events_dispatch(unsigned action, int event, int state);

#endif
