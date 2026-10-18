/**
 * @file lab_plot_pinch.h
 * @brief Two-touch viewport zoom support for the plot gesture controller.
 *
 * Uses native range metadata and browser coordinates without interpreting expressions.
 * The shared drag controller owns pointer lifetimes and asynchronous redraws.
 */
#ifndef LAB_WASM_PLOT_PINCH_H
#define LAB_WASM_PLOT_PINCH_H

#include <stdbool.h>

/**
 * @brief Promote an active touch drag to a two-pointer range or surface gesture.
 * @param state Scoped handle to the mutable browser gesture record containing the first touch and chart.
 * @param event Scoped second-touch pointer-event handle; clientX/clientY are CSS pixels and pointerId is opaque.
 * @return True after capturing the second touch; false for an ineligible target, invalid geometry or separation
 * below eight CSS pixels. Pointer IDs, including false, are compared without numeric conversion.
 */
bool lab_plot_pinch_start(int state, int event);

/**
 * @brief Update a tracked touch and request a curve-range or retained surface-gesture update.
 * @param state Scoped handle to the mutable browser gesture record initialised by lab_plot_pinch_start.
 * @param event Scoped pointer-event handle for either tracked touch, with clientX/clientY in CSS pixels.
 * @return True for a usable update, including surface coordinates without changed bounds; false for separation
 * below eight CSS pixels, non-finite geometry or unrepresentable curve bounds.
 */
bool lab_plot_pinch_move(int state, int event);

/**
 * @brief Restore captured curve bounds or mark a surface gesture for retained-volume cancellation.
 * @param state Scoped handle to the mutable browser gesture record containing the initial range snapshots.
 */
void lab_plot_pinch_cancel(int state);

#endif
