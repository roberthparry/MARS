/**
 * @file lab_plot_controls.h
 * @brief Native controller for grouped plot settings and variable radio choices.
 *
 * The workspace supplies native variable names and display labels. This interface
 * projects those opaque names without interpreting mathematical source in the browser.
 */
#ifndef LAB_WASM_PLOT_CONTROLS_H
#define LAB_WASM_PLOT_CONTROLS_H

#include "lab_handle.h"

/** @brief Plot-control events; numeric values form part of the browser/WebAssembly ABI. */
typedef enum {
    LAB_PLOT_CONTROL_CHANGE = 0,       /**< Committed control value (browser change event). */
    LAB_PLOT_CONTROL_INPUT = 1,        /**< Live control movement or text entry (browser input event). */
    LAB_PLOT_CONTROL_TIMER = 2,        /**< Delayed redraw using a retained request snapshot. */
    LAB_PLOT_CONTROL_AUTO_CLICK = 3,   /**< Click on an Auto range option, including an already selected option. */
    LAB_PLOT_CONTROL_POINTER_DOWN = 4 /**< Start of a pointer gesture on a camera control. */
} lab_plot_control_action_t;

/**
 * @brief Rebuild variable choices from native metadata.
 * @param names Scoped browser array handle containing opaque native variable names.
 * @param labels Scoped browser array handle containing corresponding display labels.
 */
void lab_plot_controls_variables(jsref names, jsref labels);

/** @brief Synchronise choices and dimensional controls with the selected type and rendered plot. */
void lab_plot_controls_sync(void);

/** @brief Restore plot ranges, camera and slice controls for an explicit Evaluate action. */
void lab_plot_controls_reset(void);

/**
 * @brief Select the first axis for a direct plot action and synchronise its controls.
 * @param variable Scoped browser string handle containing the opaque native variable name.
 */
void lab_plot_controls_prepare(jsref variable);

/**
 * @brief Handle plot setting changes and delayed redraws.
 * @param action Browser control event or delayed-redraw action; ABI values are defined above.
 * @param event Scoped browser event handle for control actions; unused for a timer action.
 * @param context Scoped timer snapshot handle containing mode, request, source and optional slicePreview;
 *                unused for browser events. Stale timer snapshots are ignored.
 */
void lab_plot_controls_dispatch(lab_plot_control_action_t action, event_jsref event, jsref context);

#endif
