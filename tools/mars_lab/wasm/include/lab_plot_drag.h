/**
 * @file lab_plot_drag.h
 * @brief Browser-facing interface for curve panning, two-finger range zoom and surface-camera gestures.
 *
 * The C controller owns gesture state, angle limits and redraw decisions. Browser
 * subscriptions retain ordinary objects, never scoped DOM handles. Callers outside
 * worksheet event installation should use the ordinary plot controls instead.
 * Boolean parameters use the wasm32 i32 ABI; browser callers must supply exactly 0 or 1.
 */
#ifndef LAB_WASM_PLOT_DRAG_H
#define LAB_WASM_PLOT_DRAG_H

#include <stdbool.h>

#include "lab_handle.h"

/** @brief Camera-slider gesture phase; values are fixed by the browser ABI. */
typedef enum {
    LAB_PLOT_SLIDER_BEGIN = 0,  /**< Capture the camera at pointer-down. */
    LAB_PLOT_SLIDER_INPUT = 1,  /**< Preview the current angle controls. */
    LAB_PLOT_SLIDER_COMMIT = 2  /**< Finish the held slider gesture. */
} lab_plot_slider_action_t;

/** @brief Plot gesture event selectors; values are fixed by the browser ABI. */
typedef enum {
    LAB_PLOT_DRAG_START = 0,          /**< Begin a pointer gesture or add its second touch. */
    LAB_PLOT_DRAG_MOVE = 1,           /**< Move a tracked pointer. */
    LAB_PLOT_DRAG_RELEASE = 2,        /**< Release a tracked pointer and finish the gesture. */
    LAB_PLOT_DRAG_CANCEL = 3,         /**< Cancel on focus, page or visibility loss. */
    LAB_PLOT_DRAG_ESCAPE = 4,         /**< Cancel only when the keyboard event is Escape. */
    LAB_PLOT_DRAG_CAMERA_WHEEL = 5,   /**< Change an angle using a modified wheel gesture. */
    LAB_PLOT_DRAG_WHEEL_IDLE = 6,     /**< Finish the current wheel revision after inactivity. */
    LAB_PLOT_DRAG_POINTER_CANCEL = 7 /**< Cancel only if the cancelled pointer belongs to the gesture. */
} lab_plot_drag_action_t;

/** @brief Install idempotent mouse, touch, pen and wheel subscriptions on the plot and browser window.
 * @param document Scoped browser Document for cancellation events.
 * @param window Scoped browser Window for movement, release and loss of focus.
 */
void lab_plot_drag_install(jsref document, jsref window);

/**
 * @brief Coalesce wheel-range previews and request full detail after wheel inactivity.
 * @param chart Scoped browser handle to the chart whose already-edited bounds require resampling.
 * @param calls Scoped mutable browser array receiving redraw and wheel-inactivity service descriptors.
 */
void lab_plot_wheel_zoom(chart_jsref chart, jsref calls);

/** @brief Stop queued wheel refinement when another viewport interaction takes over. */
void lab_plot_wheel_stop(void);

/**
 * @brief Share retained camera previews and gesture completion with the angle sliders.
 * @param action Slider phase from lab_plot_slider_action_t; other values, including negatives, do nothing.
 * @param event Scoped browser event handle supplying pointerId when beginning a gesture; unused for commit.
 * @param calls Scoped mutable browser array receiving deferred preview, cancellation and redraw services.
 */
void lab_plot_slider(lab_plot_slider_action_t action, event_jsref event, jsref calls);

/** @brief Process a gesture and publish deferred host calls after releasing scoped handles.
 * @param action Gesture event from lab_plot_drag_action_t; other values, including negatives, do nothing.
 * @param event Scoped browser event, or the scheduled revision number for wheel inactivity.
 * This tagged operand remains int because LAB_PLOT_DRAG_WHEEL_IDLE supplies a number, not a handle.
 * @param state Scoped handle to the mutable browser-owned gesture record retained by the event subscriptions.
 * @details Publishes calls and an optional prevent flag through lab_dom_return; handles expire on return.
 */
void lab_plot_drag_dispatch(lab_plot_drag_action_t action, int event, gesture_jsref state);

/** @brief Complete one redraw, retaining only the newest pending camera position.
 * @param state Scoped handle to the mutable browser-owned gesture record for the completed redraw.
 * @param accepted True only if the completed request still owns the main request token.
 * Publishes at most one deferred redraw; failures or superseded requests stop the gesture.
 */
void lab_plot_drag_complete(gesture_jsref state, bool accepted);

#endif
