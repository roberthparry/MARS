/**
 * @file lab_plot_controls.h
 * @brief Native controller for grouped plot settings and variable radio choices.
 *
 * The workspace supplies native variable names and display labels. This interface
 * projects those opaque names without interpreting mathematical source in the browser.
 */
#ifndef LAB_WASM_PLOT_CONTROLS_H
#define LAB_WASM_PLOT_CONTROLS_H

/** @brief Rebuild variable choices from native metadata. @param names Names. @param labels Display labels. */
void lab_plot_controls_variables(int names, int labels);

/** @brief Synchronise choices and dimensional controls with the selected type and rendered plot. */
void lab_plot_controls_sync(void);

/** @brief Restore plot ranges, camera and slice controls for an explicit Evaluate action. */
void lab_plot_controls_reset(void);

/** @brief Select the first axis for a direct plot action. @param variable Opaque native variable name. */
void lab_plot_controls_prepare(int variable);

/**
 * @brief Handle plot setting changes and delayed redraws.
 * @param action Change 0, input 1, timer 2, Auto click 3.
 * @param event Browser event.
 * @param context Timer snapshot.
 */
void lab_plot_controls_dispatch(unsigned action, int event, int context);

#endif
