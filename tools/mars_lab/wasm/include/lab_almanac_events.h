/**
 * @file lab_almanac_events.h
 * @brief Native almanac worksheet action registration and visibility policy.
 *
 * Installs totality and visibility actions on server-rendered worksheets and selects
 * ordered browser services for storage, rerendering and asynchronous evaluation.
 * The browser supplies a read-only live state object with visibility and worksheet
 * getters; a deferred service commits visibility changes. Native markup and copy
 * text pass through unchanged. Browser listeners retain their targets and context
 * objects, while all integer handles expire at the end of each synchronous call.
 * Request ownership and cancellation remain in the existing asynchronous adapters.
 */
#ifndef LAB_WASM_ALMANAC_EVENTS_H
#define LAB_WASM_ALMANAC_EVENTS_H

/**
 * @brief Register native almanac actions idempotently on descendants of a root.
 * @param root Scoped worksheet root, or zero to search the document for totality actions.
 * @param state Scoped live visibility/worksheet state; zero registers totality actions only.
 * @details Visibility subscriptions share a browser-owned context stored on the root.
 * Reinstallation updates its state reference without creating duplicate listeners.
 */
void lab_almanac_events_install(int root, int state);

/**
 * @brief Validate and project an exact server worksheet variant, then register its actions.
 * @param target Scoped worksheet container.
 * @param data Scoped native response with visibility and almanac_presentation fields.
 * @param state Scoped read-only live state with visibility and worksheet properties.
 * @details Returns the resolved visibility string through lab_dom_return on success,
 * or null for missing target/state or invalid native presentation. The browser commits
 * this visibility after the call. Invalid presentation preserves the previous DOM and
 * visibility; the browser supplies the existing diagnostic. State is never written.
 */
void lab_almanac_events_render(int target, int data, int state);

/**
 * @brief Return ordered deferred services for a totality or visibility action.
 * @param action Totality 0 or visibility 1; other indices return an empty plan.
 * @param event Scoped browser event; currentTarget identifies the registered action button.
 * @param context Scoped registration context containing target and state; unused for totality.
 * @details Returns a calls array through lab_dom_return, without cancelling the event.
 * Visibility is normalised before comparison. Accepted changes defer visibility assignment
 * and then saving state before either
 * rerendering the last response and refreshing totality, or evaluating without history.
 */
void lab_almanac_events_dispatch(unsigned action, int event, int context);

#endif
