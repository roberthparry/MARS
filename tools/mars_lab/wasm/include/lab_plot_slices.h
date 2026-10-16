/**
 * @file lab_plot_slices.h
 * @brief Browser projection of native plot-local parameter slice records.
 *
 * Fields carry opaque exact mathematical text. The native worker selects ranges
 * and interpolates values; WebAssembly only manages controls and request records.
 */
#ifndef LAB_WASM_PLOT_SLICES_H
#define LAB_WASM_PLOT_SLICES_H

/** @brief Return the current tab-separated slice controls as a scoped string. */
int lab_plot_slices_payload(void);

/** @brief Apply native rows only if controls still match the request snapshot. */
void lab_plot_slices_apply(int rows, int expected);

/** @brief Clear plot-local slices when the authored source changes. */
void lab_plot_slices_clear(void);

/** @brief Reset every slice to automatic limits for a new evaluation. */
void lab_plot_slices_auto(void);

/** @brief Return 1 for a sampling request, 2 for a control-only change, or 0 for an unrelated event. */
int lab_plot_slices_edit(unsigned action, int target);

#endif
