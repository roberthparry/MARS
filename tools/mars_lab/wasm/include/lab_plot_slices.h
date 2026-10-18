/**
 * @file lab_plot_slices.h
 * @brief Browser projection of native plot-local parameter slice records.
 *
 * Fields carry opaque exact mathematical text. The native worker selects ranges
 * and interpolates values; WebAssembly only manages controls and request records.
 */
#ifndef LAB_WASM_PLOT_SLICES_H
#define LAB_WASM_PLOT_SLICES_H

#include "lab_handle.h"
#include "lab_plot_controls.h"

/** @brief Effect of a slice-control event on the current plot. */
typedef enum {
    LAB_PLOT_SLICE_UNHANDLED = 0, /**< Not a slice edit; continue normal plot-control handling. */
    LAB_PLOT_SLICE_SAMPLE = 1,    /**< Values or automatic limits changed; request new samples. */
    LAB_PLOT_SLICE_CONTROL = 2    /**< Only the visible slice controls changed; no sampling is needed. */
} lab_plot_slice_edit_t;

/**
 * @brief Serialise the current plot-local slice controls.
 * @return Scoped browser string handle with one tab-separated name, lower limit, upper limit,
 *         exact value and slider position per line; an empty string when there are no slices.
 */
browser_jsref lab_plot_slices_payload(void);

/**
 * @brief Apply native slice metadata only while the controls still match the request snapshot.
 * @param records Scoped browser string handle with one native name, lower limit, upper limit, exact value
 *                and slider position per tab-separated line; a falsey value clears the slice controls.
 * @param expected Scoped browser string handle holding the payload captured before the request;
 *                 a mismatch with the current controls discards the reply.
 */
void lab_plot_slices_apply(browser_jsref records, browser_jsref expected);

/** @brief Clear plot-local slices when the authored source changes. */
void lab_plot_slices_clear(void);

/** @brief Reset every slice to automatic limits for a new evaluation. */
void lab_plot_slices_auto(void);

/**
 * @brief Handle a slice choice, range mode, exact-value edit or slider movement.
 * @param action Plot-control event; only change and input actions modify slice values.
 * @param target Scoped browser handle of the event's target element, used to locate its slice row.
 * @return Whether the event is unrelated, requires fresh samples or only changes controls.
 */
lab_plot_slice_edit_t lab_plot_slices_edit(lab_plot_control_action_t action, browser_jsref target);

#endif
