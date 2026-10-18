/**
 * @file lab_surface_volume.h
 * @brief Exact native range recipes and normalised browser zoom state.
 *
 * Native MARS evaluates opaque coordinate expressions at adaptive precision.
 * WebAssembly stores only a dimensionless binary zoom exponent and display-space
 * transforms; it never subtracts rounded absolute bounds for sampling.
 * Explicit height grids in Auto retain their sampled height during horizontal
 * zoom and adopt the new native height on refinement. Space curves zoom only the
 * input interval, preserving output-axis controls and initial box proportions whilst
 * adopting fresh output ranges. Failed or superseded previews can restore accepted geometry.
 * Other policies and implicit meshes retain strict three-axis volume zoom.
 * Boolean parameters use the wasm32 i32 ABI; browser callers must supply exactly 0 or 1.
 */
#ifndef MARS_LAB_SURFACE_VOLUME_H
#define MARS_LAB_SURFACE_VOLUME_H

#include <stdbool.h>

#include "lab_handle.h"

/** @brief Surface range-zoom operations; numeric values are fixed by the browser ABI. */
typedef enum {
    LAB_SURFACE_ZOOM_FACTOR = 0,   /**< Multiply the current zoom by a relative factor. */
    LAB_SURFACE_ZOOM_EXPONENT = 1, /**< Set the base-two zoom exponent from the slider. */
    LAB_SURFACE_ZOOM_WHEEL = 2,    /**< Derive an exponent change from a wheel event. */
    LAB_SURFACE_ZOOM_PINCH = 3,    /**< Apply or cancel a two-pointer gesture snapshot. */
    LAB_SURFACE_ZOOM_ROLLBACK = 4  /**< Restore the last accepted sample without resampling. */
} lab_surface_zoom_kind_t;

/**
 * @brief Initialise exact range metadata and preserve its baseline through owned refinements.
 * @param view Scoped mutable prepared-view record; receives volume state derived from its chart and units.
 * @param saved Scoped previous view record with volume/refining state, or zero when there is no previous view.
 * @param same True when the new chart has the same plot identity as the saved view.
 * @details Segment refinements replace sampled bounds, not box proportions, camera orientation or the baseline
 * exponent recipe. The segment baseline comes only from its chart, never shared controls that a sibling
 * plot may have changed; reset retains it. Browser objects own retained state, not the scoped handle integers.
 */
void lab_surface_volume_init(view_jsref view, browser_jsref saved, bool same);

/**
 * @brief Change the displayed volume from a gesture or restore the last accepted sample.
 * @param view Scoped mutable view record containing chart, volume and camera state.
 * @param kind Operation from lab_surface_zoom_kind_t. Unknown codes keep the current exponent before applying
 * reset and the ordinary dirty-volume projection; they do not select a gesture branch.
 * @param value Dimensionless multiplier for kind 0, base-two zoom exponent for kind 1, or non-zero to restore
 * accepted controls during kind 4; ignored for wheel and pinch.
 * @param event Scoped WheelEvent for kind 2, or mutable pinch record for kind 3 with pinchDistance, touchX,
 * touchY, secondX and secondY in CSS pixels, plus optional pinchCancelled; unused for other kinds.
 * @param reset True resets the exponent and original controls after processing the gesture; ignored for rollback.
 * @return True when applied (including pinch cancellation), false for rejected geometry/resource limits or
 * a rollback with no outstanding preview.
 * @details Marks ordinary zoom changes dirty for resampling. Segments modify only input-axis controls;
 * reset restores their original chart interval without changing output-axis controls. Selecting Auto input
 * range separately requests native range selection. Pinch cancellation restores starting volume, camera scale,
 * controls and dirty state; invalid pinch distances leave state unchanged. Rollback restores sampled bounds,
 * exponent and camera scale without resampling, leaving baseline, curve proportions and orientation unchanged.
 */
bool lab_surface_zoom(view_jsref view, lab_surface_zoom_kind_t kind, double value, event_jsref event, bool reset);

/**
 * @brief Update the zoom slider and accessible readout from the binary exponent.
 * @param view Scoped prepared view record; volume.exponent controls the readout and chart type selects its hint.
 */
void lab_surface_zoom_sync(view_jsref view);

/**
 * @brief Borrow a scoped preview label, retaining native text for accepted samples.
 * @param volume Scoped volume record containing dirty state and six display bounds.
 * @param tick Scoped tick record with axis 0..2, unit-interval fraction and native label string.
 * @return Scoped browser string handle: the original label for a clean volume, an approximate preview otherwise,
 * or an empty string for invalid bounds. The handle is valid only within the current browser bridge scope.
 */
browser_jsref lab_surface_tick_label(browser_jsref volume, browser_jsref tick);

#endif
