/**
 * @file lab_plot_pinch.h
 * @brief Two-touch viewport zoom support for the plot gesture controller.
 *
 * Uses native range metadata and browser coordinates without interpreting expressions.
 * The shared drag controller owns pointer lifetimes and asynchronous redraws.
 */
#ifndef LAB_WASM_PLOT_PINCH_H
#define LAB_WASM_PLOT_PINCH_H

/** @brief Promote an active touch drag to a two-pointer range gesture. */
int lab_plot_pinch_start(int state, int event);

/** @brief Update one tracked touch and the requested viewport; return whether bounds changed. */
int lab_plot_pinch_move(int state, int event);

/** @brief Restore range controls and exact values captured before the gesture. */
void lab_plot_pinch_cancel(int state);

#endif
