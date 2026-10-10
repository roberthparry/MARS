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

/**
 * @brief Advance a saved-state or workspace continuation.
 * @param kind Local workflow: editors 0, controls 1, saved state 2, recovery 3,
 * loading 4, history restoration 5, navigation 6, capture 7, selection 8, precision 9.
 * @param frame Borrowed mutable browser-owned continuation frame.
 * @param view Borrowed browser capability view; no ownership is transferred.
 * @return Scoped continuation plan for the shared host interpreter.
 */
int lab_flow_state(unsigned kind, int frame, int view);

#endif
