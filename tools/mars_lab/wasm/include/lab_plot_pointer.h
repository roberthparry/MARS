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

/** @brief Display an interval-aware bound while retaining its unrounded value for sampling. */
void lab_plot_write_bound(int control, double value, double span);

/** @brief Format the six surface range controls from a browser-owned array of coordinate bounds. */
void lab_plot_volume_bounds(int bounds);

/** @brief Identify the Argand projection, whose viewport is independent of the sampling variable. */
int lab_plot_is_plane(int chart);

/** @brief Identify component and amplitude views whose automatic height should survive input zoom. */
int lab_plot_auto_height(int chart);

/** @brief Store four real/imaginary viewport bounds without changing the shared sampling interval. */
void lab_plot_plane_bounds(const double bounds[4]);

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
