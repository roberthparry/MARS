/**
 * @file lab_event_dom.h
 * @brief Application API: native worksheet event registration and dispatch for the MARS Lab browser.
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

#include "lab_handle.h"

/** @brief Ordinary worksheet events; packed calendar selectors are not values of this enum. */
typedef enum {
    LAB_EVENT_RUN                =  0, /**< Evaluate the worksheet. */
    LAB_EVENT_HISTORY_BACK       =  1, /**< Navigate back. */
    LAB_EVENT_HISTORY_FORWARD    =  2, /**< Navigate forward. */
    LAB_EVENT_EDITOR_KEY         =  3, /**< Handle editor keys. */
    LAB_EVENT_EDITOR_INPUT       =  4, /**< Refresh edited input. */
    LAB_EVENT_CLEAR              =  5, /**< Clear the worksheet. */
    LAB_EVENT_HELP               =  6, /**< Toggle help. */
    LAB_EVENT_GOAL               =  7, /**< Start goal seeking. */
    LAB_EVENT_TAB                =  8, /**< Select a mode tab. */
    LAB_EVENT_MATRIX_CONTROL     =  9, /**< Normalise the matrix operation. */
    LAB_EVENT_EQUATION_CONTROL   = 10, /**< Normalise the equation variable. */
    LAB_EVENT_INTEGRATOR_CONTROL = 11, /**< Normalise the integrator interval cap. */
    LAB_EVENT_ALMANAC_TIME       = 12, /**< Format Almanac time. */
    LAB_EVENT_GOAL_KEY           = 13, /**< Handle goal-entry keys. */
    LAB_EVENT_PRECISION_CHANGE   = 14, /**< Commit slider precision. */
    LAB_EVENT_PRECISION_INPUT    = 15, /**< Preview slider precision. */
    LAB_EVENT_RENDERED_DIGITS    = 16, /**< Toggle rendered digits. */
    LAB_EVENT_EXPRESSION_DIGITS  = 17, /**< Toggle expression digits. */
    LAB_EVENT_FUNCTION_DIGITS    = 18, /**< Toggle Function digits. */
    LAB_EVENT_FUNCTION_RUN       = 19, /**< Run the Function card. */
    LAB_EVENT_VALUE_DIGITS       = 20, /**< Toggle Value digits. */
    LAB_EVENT_RESULT_INPUT       = 21, /**< Reuse result as input. */
    LAB_EVENT_INPUT_COPY         = 22, /**< Copy input. */
    LAB_EVENT_RESULT_COPY        = 23, /**< Copy a result. */
    LAB_EVENT_ZOOM_CLICK         = 24, /**< Zoom using card controls. */
    LAB_EVENT_ZOOM_WHEEL         = 25, /**< Zoom using the wheel. */
    LAB_EVENT_PAGE_HIDE          = 26, /**< Flush state on page hide. */
    LAB_EVENT_VISIBILITY         = 27, /**< Flush hidden-page state. */
    LAB_EVENT_RESIZE             = 28, /**< Update resized layouts. */
    LAB_EVENT_PANEL_FIT          = 29, /**< Schedule panel fitting. */
    LAB_EVENT_EDITOR_RESIZE      = 30, /**< Schedule editor resizing. */
    LAB_EVENT_EXPAND             = 31, /**< Toggle card expansion. */
    LAB_EVENT_ROTATION_CHANGE    = 32, /**< Commit rotation controls. */
    LAB_EVENT_RESERVED           = 33, /**< Reserved inactive selector. */
    LAB_EVENT_ROTATION_INPUT     = 34, /**< Preview rotation controls. */
    LAB_EVENT_PRECISION_TRACK    = 35, /**< Step precision along its track. */
    LAB_EVENT_COUNT              = 36, /**< Number of ordinary selectors. */
} lab_event_action_t;

/**
 * @brief Publish ordered browser services after synchronous native event decisions.
 * @param action Selector: evaluate 0, history back/forward 1/2, editor key 3, editor input 4, clear 5, help 6,
 * goal 7, mode tab 8, matrix/equation/integrator controls 9/10/11, almanac time 12, goal key 13,
 * precision change/input 14/15, rendered/expression/function digits 16/17/18, function run 19,
 * value digits 20, result-to-input 21, input/result copy 22/23, zoom click/wheel 24/25, pagehide 26,
 * visibility 27, resize 28, panel fit 29, editor resize 30, card expansion 31, rotation change/input 32/34,
 * or precision track pointer 35. Selector 33 and unassigned selectors below 256 have no action.
 * The unsigned boundary accepts either lab_event_action_t or packed calendar data.
 * Calendar selectors pack mode 5 or 6 in the high byte and a lab_profile field index in the low byte.
 * @param event Scoped browser Event with currentTarget, target, type and relevant key, modifier or pointer data.
 * @post Publishes a record containing calls and optional prevent/stop flags through lab_dom_return.
 */
void lab_events_dispatch(unsigned action, event_jsref event);

/**
 * @brief Install worksheet control listeners and project initial card controls.
 * @param document Scoped browser Document used for visibility events and plot/precision gesture listeners.
 * @param window Scoped browser Window used for resize, scroll, pagehide and plot gesture listeners.
 * @details Repeated subscriptions do not duplicate listeners. The browser retains targets, not integer handles.
 */
void lab_events_install(browser_jsref document, browser_jsref window);

#endif
