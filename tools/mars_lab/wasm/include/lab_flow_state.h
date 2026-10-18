/**
 * @file lab_flow_state.h
 * @brief Native continuations for saved worksheets, history and workspace transitions.
 *
 * Used by the shared flow dispatcher for global kinds 16 through 25. Frames and
 * callback values belong to the browser; no borrowed handle survives a call.
 * Storage, clocks and asynchronous browser capabilities are supplied by the host.
 */
#ifndef LAB_FLOW_STATE_H
#define LAB_FLOW_STATE_H

#include "lab_handle.h"

/** @brief Closed workflow selectors for lab_flow_state; numeric values are browser ABI. */
typedef enum {
    LAB_FLOW_STATE_EDITORS    = 0, /**< Restore saved editors. */
    LAB_FLOW_STATE_CONTROLS   = 1, /**< Restore saved controls. */
    LAB_FLOW_STATE_SAVED      = 2, /**< Apply saved state. */
    LAB_FLOW_STATE_RECOVERY   = 3, /**< Recover authored input. */
    LAB_FLOW_STATE_LOAD       = 4, /**< Load initial state. */
    LAB_FLOW_STATE_HISTORY    = 5, /**< Restore history. */
    LAB_FLOW_STATE_NAVIGATION = 6, /**< Navigate history. */
    LAB_FLOW_STATE_CAPTURE    = 7, /**< Capture state. */
    LAB_FLOW_STATE_SELECTION  = 8, /**< Select a worksheet. */
    LAB_FLOW_STATE_PRECISION  = 9, /**< Apply a precision change. */
} lab_flow_state_kind_t;

/**
 * @brief Advance a saved-state or workspace continuation.
 * @param kind Local workflow: editors 0, controls 1, saved state 2, recovery 3,
 * loading 4, history restoration 5, navigation 6, capture 7, selection 8, precision 9.
 * @param frame Scoped browser handle to the mutable continuation frame storing stage, inputs and service results.
 * @param view Scoped browser handle to the workspace capability view used for current state and snapshots.
 * @return Scoped browser handle to the next continuation plan; unsupported kinds produce an inert completion.
 */
jsref lab_flow_state(lab_flow_state_kind_t kind, frame_jsref frame, view_jsref view);

#endif
