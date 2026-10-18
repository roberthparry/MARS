/**
 * @file lab_events.h
 * @brief Application API: worksheet event decisions and clear-control projection in C/WebAssembly.
 *
 * Direct mode tables select input-refresh, save and goal-seek actions. Browser
 * listeners deliver events and execute asynchronous services under native request
 * ownership; C retains no event objects and does not parse expression text.
 * Input refresh plans decode these actions into ordered host storage, timer and
 * request services. Reuse validity is projected only after its timer is cancelled.
 *
 * Used by the freestanding browser modules and the versioned JavaScript host ABI.
 * This is not an installed MARS library interface; calls run synchronously on the browser thread.
 * Boolean arguments cross the WebAssembly i32 boundary as canonical 0 or 1, never scoped handles.
 */
#ifndef LAB_WASM_EVENTS_H
#define LAB_WASM_EVENTS_H

#include <stdbool.h>

#include "lab_handle.h"
#include "lab_mode.h"

/** @brief Additional reset work after scalar controls have been cleared. */
typedef enum {
    LAB_EVENT_CLEAR_NONE       = 0, /**< No additional reset. */
    LAB_EVENT_CLEAR_INTEGRATOR = 1, /**< Reset integrator rows. */
    LAB_EVENT_CLEAR_CALENDAR   = 2, /**< Reset the calendar profile. */
} lab_event_clear_action_t;

/** @brief Reevaluation policy after changing precision. */
typedef enum {
    LAB_EVENT_PRECISION_NONE     = 0, /**< No mathematical reevaluation. */
    LAB_EVENT_PRECISION_GOAL     = 1, /**< Repeat goal seeking. */
    LAB_EVENT_PRECISION_REUSE    = 2, /**< Reuse Expression input. */
    LAB_EVENT_PRECISION_EVALUATE = 3, /**< Evaluate the current worksheet. */
} lab_event_precision_action_t;

/**
 * @brief Select worksheet input actions without mutating browser state.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * @param bound True when native editor metadata reports bindings; controls source replacement for modes 1, 3 and 4.
 * @param unchanged True when Expression input matches the retained native source; ignored in other modes.
 * @return Bitmask: calendar editor 1, source update 2, binding refresh 4, save 8, history 16,
 * reuse bindings 32 and new edit 64. Invalid modes return zero.
 */
unsigned lab_events_input(lab_mode_t mode, bool bound, bool unchanged);

/**
 * @brief Return the ordered worksheet refresh plan without executing host services.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * Other values produce an empty plan.
 * @param bound True when native editor metadata reports bindings.
 * @param unchanged True when the input matches the retained native source.
 * @param context Scoped record containing the editor node and defaultDatetimeText.
 * @post lab_dom_return supplies ordered calls with real browser values, never retained handles.
 * The host cancels evaluation and binding requests before constructing this plan, then executes its services
 * outside the DOM scope. State setters remain host adapters, preserving the property descriptors of native
 * storage views. Equation saves omit text to preserve their live default after binding refresh.
 */
void lab_events_refresh(lab_mode_t mode, bool bound, bool unchanged, browser_handle context);

/**
 * @brief Mark reused bindings as current after the ordered timer-cancellation service has finished.
 * @param editor Scoped editor node whose bindingRefreshValid dataset field becomes "true".
 * @post Unrelated editor data remains unchanged.
 */
void lab_events_refresh_mark(browser_handle editor);

/**
 * @brief Clear scalar controls and select additional structured reset work.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * @param config Scoped bootstrap defaults containing DEFAULT_EQUATION_VARIABLE, DEFAULT_INTEGRATOR_INTERVAL_CAP,
 * DEFAULT_DATETIME_TEXT and DEFAULT_ALMANAC_TEXT; only the selected mode's fields are read.
 * @return Additional reset action: none 0, integrator rows 1, or calendar profile 2.
 * Invalid modes return zero without changing controls.
 */
lab_event_clear_action_t lab_events_clear(lab_mode_t mode, browser_handle config);

/**
 * @brief Decide whether page inactivity should flush saved editor state.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * @return True for Expression or equation; false for every other mode.
 */
bool lab_events_flush(lab_mode_t mode);

/**
 * @brief Select the reevaluation action after a precision change.
 * @param mode Expression 0, equation 1, differential equation 2, matrix 3, integrator 4, DateTime 5 or almanac 6.
 * @param goal_source True when a remembered goal source exists; examined only in Expression mode.
 * @param goal_target True when a remembered goal target exists; examined only in Expression mode.
 * @return No action 0 for calendar or invalid modes, goal seek 1 when both Expression goal flags are set,
 * Expression input reuse 2 otherwise, or ordinary evaluation 3 for modes 1 through 4.
 */
lab_event_precision_action_t lab_events_precision(lab_mode_t mode, bool goal_source, bool goal_target);

#endif
