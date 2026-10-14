/**
 * @file lab_plot_drag.h
 * @brief Browser-facing interface for native surface-camera drag and wheel gestures.
 *
 * The C controller owns gesture state, angle limits and redraw decisions. Browser
 * subscriptions retain ordinary objects, never scoped DOM handles. Callers outside
 * worksheet event installation should use the ordinary plot controls instead.
 */
#ifndef LAB_WASM_PLOT_DRAG_H
#define LAB_WASM_PLOT_DRAG_H

/** @brief Install idempotent mouse, touch, pen and wheel subscriptions on the plot and browser window.
 * @param document Scoped browser Document for cancellation events.
 * @param window Scoped browser Window for movement, release and loss of focus.
 */
void lab_plot_drag_install(int document, int window);

/** @brief Process a gesture and publish deferred host calls after releasing scoped handles.
 * @param action Start 0, move 1, release 2, cancel 3, Escape 4, camera wheel 5, wheel inactivity 6, pointer cancel 7.
 * @param event Scoped browser event, or the scheduled revision number for wheel inactivity.
 * @param state Browser-owned gesture record retained by the event subscriptions.
 */
void lab_plot_drag_dispatch(unsigned action, int event, int state);

/** @brief Complete one redraw, retaining only the newest pending camera position.
 * @param state Browser-owned gesture record.
 * @param accepted Non-zero only if the completed request still owns the main request token.
 * Publishes at most one deferred redraw; failures or superseded requests stop the gesture.
 */
void lab_plot_drag_complete(int state, int accepted);

#endif
