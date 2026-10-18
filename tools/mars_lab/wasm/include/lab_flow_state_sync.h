/**
 * @file lab_flow_state_sync.h
 * @brief Synchronous native plans for worksheet persistence and workspace projection.
 *
 * Preserves synchronous browser API contracts while C selects the operations and
 * their order. Browser-owned frames carry values between calls; local storage,
 * clocks, timers and DOM capabilities remain host resources. No mathematics is
 * interpreted here and no borrowed handle is retained by C.
 */
#ifndef LAB_FLOW_STATE_SYNC_H
#define LAB_FLOW_STATE_SYNC_H

#include "lab_handle.h"

/** @brief Structured control choices carried in a normalisation frame's index property. */
typedef enum {
    LAB_STATE_CONTROL_MATRIX_OPERATION  = 0,  /**< Matrix operation selector. */
    LAB_STATE_CONTROL_EQUATION_VARIABLE = 1, /**< Equation solution variable. */
    LAB_STATE_CONTROL_INTERVAL_CAP      = 2       /**< Integrator interval limit. */
} lab_state_control_kind_t;

/** @brief Closed workflow selectors for lab_state_sync; numeric values are browser ABI. */
typedef enum {
    LAB_STATE_SYNC_SAVE              = 0, /**< Prepare a worksheet save. */
    LAB_STATE_SYNC_PUBLISH           = 1, /**< Publish a captured save. */
    LAB_STATE_SYNC_CALENDAR          = 2, /**< Save calendar state. */
    LAB_STATE_SYNC_SAVE_MODE         = 3, /**< Save the selected mode. */
    LAB_STATE_SYNC_EDITOR            = 4, /**< Restore the active editor. */
    LAB_STATE_SYNC_RESULT            = 5, /**< Restore result presentation. */
    LAB_STATE_SYNC_SELECT            = 6, /**< Select the native mode. */
    LAB_STATE_SYNC_APPLY             = 7, /**< Apply mode-specific controls. */
    LAB_STATE_SYNC_KEYBOARD          = 8, /**< Handle workspace keyboard actions. */
    LAB_STATE_SYNC_HISTORY           = 9, /**< Capture a history record. */
    LAB_STATE_SYNC_PUSH              = 10, /**< Push a history snapshot. */
    LAB_STATE_SYNC_CONTROL           = 11, /**< Normalise a structured control. */
    LAB_STATE_SYNC_FLUSH             = 12, /**< Flush pending edits. */
    LAB_STATE_SYNC_LOCATION          = 13, /**< Complete a location update. */
    LAB_STATE_SYNC_INTEGRATOR_RESULT = 14, /**< Project integrator result rows. */
    LAB_STATE_SYNC_CAPTURE_SAVE      = 15, /**< Capture text for saving. */
    LAB_STATE_SYNC_NEEDS_BOUNDS      = 16, /**< Check integrator bounds availability. */
} lab_state_sync_kind_t;

/**
 * @brief Advance one synchronous persistence or workspace operation.
 * @param kind Save worksheet 0, publish save 1, save calendar 2, save mode 3,
 * restore editor 4, restore result 5, select mode 6, apply mode 7, keyboard 8,
 * history snapshot 9, history push 10, normalise control 11, flush 12,
 * location completion 13, integrator result projection 14, capture save 15,
 * integrator bounds availability 16.
 * @param frame Scoped browser handle to the mutable continuation frame storing stage, inputs and service results.
 * @details Publishes a continuation plan through lab_dom_return; unsupported kinds publish an inert completion.
 */
void lab_state_sync(lab_state_sync_kind_t kind, frame_jsref frame);

#endif
