/**
 * @file lab_plot_pointer.h
 * @brief Browser curve inspection and viewport zoom for native curves and surfaces.
 *
 * Consumes renderer-owned viewport metadata, never mathematical source. Event
 * registration and card controls use this UI interface; native sampling still
 * owns every newly requested plot.
 * Boolean parameters use the wasm32 i32 ABI; browser callers must supply exactly 0 or 1.
 */
#ifndef MARS_LAB_PLOT_POINTER_H
#define MARS_LAB_PLOT_POINTER_H

#include <stdbool.h>

#include "lab_handle.h"

/** @brief Pointer inspection events; non-zero ABI values always clear inspection. */
typedef enum {
    LAB_PLOT_POINTER_INSPECT = 0, /**< Inspect the current pointer position. */
    LAB_PLOT_POINTER_CLEAR = 1    /**< Remove hover overlays and curve focus. */
} lab_plot_pointer_action_t;

/** @brief Register passive hover and leave handlers on the result region. */
void lab_plot_pointer_install(void);

/**
 * @brief Retain native curve labels for custom tooltips and accessibility without duplicate SVG tooltips.
 * @param rendered Scoped browser handle to the rendered region containing the curve SVGs.
 */
void lab_plot_pointer_prepare(jsref rendered);

/**
 * @brief Project a hover guide, point marker and solution-curve focus, or clear them.
 * @param action LAB_PLOT_POINTER_INSPECT inspects movement; every other value, including unknown codes,
 * clears hover overlays and curve focus.
 * @param event Scoped browser event handle supplying target, clientX/clientY in CSS pixels and buttons.
 * @param context Reserved subscription-context handle; ignored and may be zero.
 * @details Publishes an empty event plan through lab_dom_return; DOM changes occur synchronously.
 */
void lab_plot_pointer_dispatch(lab_plot_pointer_action_t action, event_jsref event, jsref context);

/**
 * @brief Choose bound display precision relative to its interval span.
 * @param value Coordinate to display, in the plotted axis units.
 * @param span Positive interval width in the same units as value.
 * @return Significant decimal digits, 1..17; 17 for non-finite coordinates or non-positive/non-finite spans.
 */
unsigned lab_plot_bound_digits(double value, double span);

/**
 * @brief Display an interval-aware bound while retaining its unrounded value for sampling.
 * @param control Scoped browser handle to the destination range input.
 * @param value Coordinate in the plotted axis units, also stored as the input's precise sampling value.
 * @param span Positive interval width in the same units, used to select displayed significant digits.
 */
void lab_plot_write_bound(jsref control, double value, double span);

/**
 * @brief Format all six surface range controls, retaining precise bounds for sampling.
 * @param bounds Scoped browser array handle containing first-axis low/high, second-axis low/high and
 * height low/high coordinates, in each axis's native units.
 */
void lab_plot_volume_bounds(jsref bounds);

/**
 * @brief Identify the Argand projection, whose viewport is independent of the sampling variable.
 * @param chart Scoped browser handle to a chart within a complex projection panel.
 * @return True when the enclosing panel is complex_plane; false otherwise.
 */
bool lab_plot_is_plane(chart_jsref chart);

/**
 * @brief Identify component and amplitude views whose automatic height should survive input zoom.
 * @param chart Scoped browser handle to the chart whose enclosing projection panel is inspected.
 * @return True for components or amplitude when plotScale is not manual; false otherwise.
 */
bool lab_plot_auto_height(chart_jsref chart);

/**
 * @brief Store real/imaginary viewport bounds without changing the shared sampling interval.
 * @param bounds Readable native array of four coordinates: real minimum/maximum, imaginary minimum/maximum.
 * The coordinates are copied into browser state as text; the pointer is not retained.
 */
void lab_plot_plane_bounds(const double bounds[4]);

/**
 * @brief Capture a curve's initial bounds, screen scale and controls for a primary-pointer pan.
 * @param state Scoped handle to the mutable browser-owned gesture record receiving geometry and snapshots.
 * @param chart Scoped handle to the SVG chart with native range and viewport metadata.
 * @param event Scoped pointer-event handle supplying clientX/clientY in CSS pixels.
 * @return True after capture; false for unusable geometry or a pointer outside the plotting rectangle.
 */
bool lab_plot_pan_start(gesture_jsref state, chart_jsref chart, event_jsref event);

/**
 * @brief Translate initial curve bounds by pointer displacement without changing their spans.
 * @param state Scoped handle to the mutable gesture record initialised by lab_plot_pan_start.
 * @param dx Horizontal displacement from the gesture start in CSS pixels, positive rightwards.
 * @param dy Vertical displacement from the gesture start in CSS pixels, positive downwards.
 * @return True after updating bounds; false if translated bounds are non-finite or collapse.
 */
bool lab_plot_pan_move(gesture_jsref state, double dx, double dy);

/**
 * @brief Restore the controls or independent Argand viewport captured before a cancelled pan.
 * @param state Scoped handle to the browser gesture record containing the original pan snapshots.
 */
void lab_plot_pan_cancel(gesture_jsref state);

/**
 * @brief Translate rendered curve geometry within native clipping windows while resampling is pending.
 * @param state Scoped gesture-record handle containing the chart, initial scale and requested/drawn pixel offsets.
 */
void lab_plot_pan_preview(gesture_jsref state);

/**
 * @brief Route range zoom to a curve redraw or a retained surface-zoom service.
 * @param card Scoped browser handle to the result card containing the plot or complex projection grid.
 * @param event Scoped event handle; wheel zoom uses its target and clientX/clientY in CSS pixels as the anchor.
 * @param calls Scoped mutable browser array receiving deferred redraw or surface-zoom service descriptors.
 * @param reset True restores the saved viewport; false applies a zoom step.
 * @param direction Positive zooms in; zero or negative zooms out. Ignored for reset.
 * @param wheel True selects pointer-anchored, coalesced wheel zoom; false uses a centred toolbar action.
 * @return True when handled as a plot action, including ignored out-of-bounds gestures; false without an eligible chart.
 */
bool lab_plot_pointer_zoom(jsref card, event_jsref event, jsref calls, bool reset, double direction,
                           bool wheel);

#endif
