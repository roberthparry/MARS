/**
 * @file lab_plot_pointer.h
 * @brief Browser curve inspection and viewport zoom for native curves and surfaces.
 *
 * Consumes renderer-owned viewport metadata, never mathematical source. Event
 * registration and card controls use this UI interface; native sampling still
 * owns every newly requested plot.
 */
#ifndef MARS_LAB_PLOT_POINTER_H
#define MARS_LAB_PLOT_POINTER_H

/** @brief Register passive hover and leave handlers on the result region. */
void lab_plot_pointer_install(void);

/** @brief Retain native curve labels for custom tooltips and accessibility without duplicate SVG tooltips. */
void lab_plot_pointer_prepare(int rendered);

/** @brief Project a hover guide and focus a solution curve; non-zero action clears both. */
void lab_plot_pointer_dispatch(unsigned action, int event, int context);

/** @brief Choose 1–17 display digits from the interval span; invalid spans retain full precision. */
unsigned lab_plot_bound_digits(double value, double span);

/** @brief Capture a curve's initial bounds, screen scale and controls for a primary-pointer pan. */
int lab_plot_pan_start(int state, int chart, int event);

/** @brief Translate initial curve bounds by screen displacement; return zero for unrepresentable ranges. */
int lab_plot_pan_move(int state, double dx, double dy);

/** @brief Restore the controls captured at the start of a cancelled curve pan. */
void lab_plot_pan_cancel(int state);

/** @brief Translate already-rendered curve geometry within native clipping windows while resampling is pending. */
void lab_plot_pan_preview(int state);

/** @brief Zoom curve or surface ranges and emit a native redraw; return zero for non-plot cards. */
int lab_plot_pointer_zoom(int card, int event, int calls, int reset, double direction, int wheel);

#endif
