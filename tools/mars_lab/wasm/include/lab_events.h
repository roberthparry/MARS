/**
 * @file lab_events.h
 * @brief Private interface: worksheet event decisions and clear-control projection in C/WebAssembly.
 *
 * Direct mode tables select input-refresh, save and goal-seek actions. Browser
 * listeners deliver events and execute asynchronous services under native request
 * ownership; C retains no event objects and does not parse expression text.
 * Input refresh plans decode these actions into ordered host storage, timer and
 * request services. Reuse validity is projected only after its timer is cancelled.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 */
#ifndef LAB_WASM_EVENTS_H
#define LAB_WASM_EVENTS_H

/** @brief Select worksheet input actions. @param mode Mode index. @param bound Native metadata indicates bindings.
 * @param unchanged Input matches the retained native source.
 * @return Calendar editor 1, source update 2, refresh bindings 4, save 8, history 16, reuse 32, new edit 64. */
unsigned lab_events_input(unsigned mode, int bound, int unchanged);

/** @brief Return the ordered worksheet refresh plan without executing host services.
 * @param mode Worksheet mode index, 0..6; other values produce an empty plan.
 * @param bound Non-zero when native editor metadata reports bindings.
 * @param unchanged Non-zero when the input matches the retained native source.
 * @param context Scoped record containing the editor node and defaultDatetimeText.
 * @return No C value; lab_dom_return supplies ordered calls with real browser values, never retained handles.
 * The host cancels evaluation and binding requests before constructing this plan, then executes its services
 * outside the DOM scope. State setters remain host adapters, preserving the property descriptors of native
 * storage views. Equation saves omit text to preserve their live default after binding refresh.
 */
void lab_events_refresh(unsigned mode, int bound, int unchanged, int context);

/** @brief Mark reused bindings as current after the ordered timer-cancellation service has finished.
 * @param editor Scoped editor node whose bindingRefreshValid dataset field becomes "true".
 * @return No value. Unrelated editor data remains unchanged.
 */
void lab_events_refresh_mark(int editor);

/** @brief Clear scalar mode controls. @param mode Mode index. @param config Bootstrap defaults.
 * @return Additional reset action: none 0, integral rows 1, calendar profile 2. */
unsigned lab_events_clear(unsigned mode, int config);

/** @brief Decide whether page inactivity flushes editor state. @param mode Mode index. @return Save flag. */
int lab_events_flush(unsigned mode);

/** @brief Choose precision reevaluation. @param mode Mode index. @param goal_source Goal source present.
 * @param goal_target Goal target present. @return No action 0, goal seek 1, expression reuse 2, ordinary evaluation 3.
 */
unsigned lab_events_precision(unsigned mode, int goal_source, int goal_target);

#endif
